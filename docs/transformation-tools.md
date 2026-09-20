# Export backends live outside the library

**Decision.** ForSyDe-SystemC exports exactly one thing: its own
intermediate representation, ForSyDe-XML. Every other format -- dot,
SDF<sup>3</sup>, anything later -- is produced by a separate tool reading
that XML. The library does not grow a backend per format.

This is the architecture [f2dot](https://forsyde.ict.kth.se/trac/wiki/ForSyDe/f2dot)
already assumed, and it is the one to build the rest on. The library
stays small; transformers evolve on their own schedule, in whatever
language suits them (f2dot is Python); a new output format needs no
library change and no model rebuild; and the tools compose into a
pipeline outside the simulator.

## Why this is written down as a reversal

An SDF<sup>3</sup> backend was built inside the library (`src/forsyde/sdf3.hpp`,
plus supporting hooks) and then reverted. The reversal is the useful
part, so here is what it cost and what it proved.

The backend needed two things the IR did not expose in memory, and they
were added as virtual hooks on `ForSyDe::process` alongside
`forsyde_kind()` and `bindInfo()`:

- `rates()` -- per-port static production/consumption counts
- `initial_tokens()` -- tokens a process places before its first firing

That was 138 lines across four library headers, on top of the 361-line
flattener. **None of it was necessary.** Everything the flattener read
was already in the exported XML, and had been since long before:

| What flattening needs | Where it already is in ForSyDe-XML |
|---|---|
| Production/consumption rates | `<argument name="otoks" value="[2]"/>`, `<argument name="itoks" value="[1]"/>` |
| Which port a rate belongs to | `<port>` order, which is `boundInChans` then `boundOutChans` -- the same order the in-memory zip relied on |
| Initial tokens | `delay` is implicitly 1; `delayn` exports `<argument name="n" value="1"/>` |
| Hierarchy | `<composite_process component_name="...">`, one file per network |
| Fan-out | two `<signal>`s sharing `source` **and** `source_port` |

So the hooks did not unlock anything. They built a second, in-memory
path to data already on disk, and the check that would have caught it
was reading one golden file in `tests/golden_ir/` before writing any of
it.

The one argument for in-process access -- a running model analysing its
own graph, the recursive self-reflection Phase 4a is aimed at -- does
not need a flattener in the library either. It needs `ir::model`, which
is already a value in memory (3a) and stays.

## What a transformer has to get right

This is what building the flattener actually established. It is the
spec for the external tool, not lost work.

### SDF<sup>3</sup> has no hierarchy

Its "sdf" application graph is flat, so every composite boundary has to
be eliminated: walk the hierarchy, qualify each leaf by its instance
path (`top__mulacc1__add1`), and resolve every channel endpoint down
through however many composite boundaries it crosses, following a
composite's port to whatever it is bound to *inside* that composite,
recursively.

### A port is one end of exactly one channel

ForSyDe's `readers(...)` lets one output be read by several downstream
signals. This is ordinary and legal -- `toysdfMN`'s own `compAvg` does
it, one averager output both leaving the composite and closing its own
feedback loop. SDF<sup>3</sup> has no equivalent, and the real
`sdf3analysis-sdf` rejects it outright:

```
Port 'compAvg1__averager1.port_2' already connected.
```

The standard dataflow answer is an explicit broadcast actor: one input,
N outputs, all at the source's own rate. **It has to be a post-pass over
the fully flattened graph, not a decision during the walk** -- the two
channels a fan-out produces are typically discovered at different levels
of the hierarchy, one inside the composite owning the port and the other
only after resolving down through the boundary from its parent, so there
is no single point in the recursive walk where both are in hand.

### A delay is a head start, not a token on a channel

`SDF::delay` and `SDF::delayn` both write their initial value(s) from
`init()`, before the ordinary `prep()`/`exec()`/`prod()` loop. That is
ForSyDe modelling a unit delay as *an actor with a head start*.
Classical SDF -- and so SDF<sup>3</sup> -- instead puts initial tokens
*on the channel*. The transformer has to convert: read the delay's depth
and mark the channel leaving that actor.

Note `delayn`'s `n` is **not** a rate. It controls how many tokens are
prepended before the first ordinary one; the actor still produces
exactly one token per firing in steady state. Declaring a downstream
input at rate `n` is a modelling error, not a choice -- see below.

### An undeclared rate defaults to 1, and that was checked

Eight SDF classes never declare rates: `source`, `sink`, `constant`,
`vsource`, `file_source`, `file_sink`, `delay`, `delayn`. Reading each
one's `exec()`/`prod()` confirms every one moves exactly one token
through each port it has, every firing. A transformer may default to 1
for these. For anything else with no rate it can determine, it should
refuse.

### Refuse rather than emit a plausible graph

A non-SDF leaf, or an SDF port with no determinable rate, must be an
error naming the exact process and port. A wrong graph that loads is
worse than no graph. SADF is out of scope for the `sdf` schema
specifically -- its rates are scenario-indexed and SDF<sup>3</sup> has a
separate `sadf`/`fsmsadf` XSD, which is a different transformer.

## Verify against the real tool, not the schema

Schema-shape agreement proves nothing about whether the graph means what
the model meant. Point the actual binary at the output:

```
sdf3analysis-sdf --graph out.sdf3.xml --algo consistency
sdf3analysis-sdf --graph out.sdf3.xml --algo deadlock
sdf3analysis-sdf --graph out.sdf3.xml --algo repetition_vector
```

(Built from `~/Downloads/sdf3`, binaries under
`build/release/Linux/bin/`. The "failed to load external entity"
warnings are the tool trying to fetch the XSD over the network, not a
rejection -- the verdict is the `Graph is ...` line after them.)

This is not a formality. It caught a real modelling mistake in the
first test model: a loop input declared at rate 2 against a `delayn`
whose actual rate is always 1. `--algo consistency` said "Graph is not
consistent", which is exactly the class of error a static analysis tool
exists to find, and no golden diff would have shown it.

A repetition vector is also checkable by hand, which is worth doing
once: an actor declared in-1/out-2 firing once against everything
downstream firing twice is arithmetic, not trust.

## The dot side needs nothing

f2dot already reads ForSyDe-XML directly -- `process_network`,
`leaf_process`, `composite_process`/`component_name`, `port` with
`bound_process`/`bound_port`, `signal` with `source`/`target` -- the same
tags and the same one-file-per-network convention the exporter has
always emitted, unchanged by 3a's rewrite of the backend into a view
over the IR.

```
python3 f2dot -m forsyde path/to/gen/top.xml -o output/dir
```

Confirmed against this tree's current output: `f2dot3` (a local Python 3
port, `~/code/f2dot3`; the original is `~/code/f2dot`) parses it and
produces a `.dot` file GraphViz renders without complaint.

## What this means for the XML

Since the XML is now the only interface, its stability matters more than
it did, and two things about it are worth stating:

1. **The port-to-rate association is by position, not by name.**
   `otoks`/`itoks` are argument strings; a consumer maps them onto
   `<port>` elements in document order. That convention is currently
   pinned by the 50 files in `tests/golden_ir/`, so it cannot drift
   silently -- but it is a convention, not something the format states.
   If a transformer turns out to be awkward to write because of it, the
   right fix is a `rate` attribute on `<port>` in the exporter, which is
   a small change to one backend, not a new backend.

2. **Argument values are display strings** (`"[1, 1]"`), formatted by
   `operator<<`. Consumers parse them. Same caveat, same remedy.
