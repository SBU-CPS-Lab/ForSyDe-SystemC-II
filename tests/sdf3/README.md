# tests/sdf3 -- a second view over the IR, checked against the real tool

`ForSyDe::sdf3::flatten` (`src/forsyde/sdf3.hpp`) turns an `ir::model`
into a flat SDF3 "sdf" application graph. Unlike the XML backend, this
is not a rewrite of an existing view -- SDF3's format has no hierarchy
concept at all, so producing one means eliminating every composite
boundary and making every port the source of at most one channel, which
is real graph work `xml.hpp` never had to do.

## What had to be added to reach it

Two virtual hooks on `ForSyDe::process` (`abssemantics.hpp`), the same
shape `forsyde_kind()` and `bindInfo()` already are, because `ir::build`
only ever has the one polymorphic `ForSyDe::process*` and a compile-time
trait cannot answer a question asked through a base pointer:

- **`rates()`** -- one static production/consumption rate per port.
  Overridden only by the SDF `comb`/`zip`/`unzip` families
  (`sdf_process_constructors.hpp`), which are the only classes that
  declare theirs. Everything else returns nothing, which is correct: a
  rate is meaningless for a process whose firing consumes or produces a
  data-dependent number of tokens.
- **`initial_tokens()`** -- tokens a process places on its own output
  before its first ordinary firing. `SDF::delay` (1) and `SDF::delayn`
  (its declared depth) are the only overrides; both write to their
  output from `init()`, which is how ForSyDe models a unit delay -- as
  an actor with a head start, not as a token already sitting on a
  channel the way classical SDF puts it. SDF3 needs the latter, so
  `flatten()` reads this and marks the channel leaving such an actor
  instead.

Ports with no declared rate on an otherwise-SDF leaf default to 1
(`detail::rate_of`) -- checked, not assumed: `source`, `sink`,
`constant`, `vsource`, `file_source`, `file_sink`, `delay` and `delayn`
are the eight SDF classes that never override `rates()`, and reading
each one's `exec()`/`prod()` confirms every one of them moves exactly
one token through each port it has, every firing.

## What flattening actually does

`detail::flatten_into` walks the hierarchy once, turning every leaf
into an actor (qualified by its instance path, `top__mulacc1__add1`)
and resolving every channel's endpoints down through however many
composite boundaries they cross (`detail::resolve`, following a
composite's own port to whatever it is bound to *inside* that
composite, recursively).

`detail::insert_fanouts` is the other half, and the one this project's
own examples immediately needed: `readers(...)` lets one ForSyDe output
be read by more than one downstream signal, which is ordinary,
legal fan-out -- `toysdfMN`'s own `compAvg` does it, one averager output
both leaving the composite and closing its own feedback loop. SDF3's
port model has no such thing: a port is one end of exactly one channel.
Not a limitation to route around -- the standard dataflow-tool answer is
an explicit broadcast actor, one input and N outputs, all at the
source's own rate, and that is what a fan-out becomes here. It is a
post-pass over the whole flattened graph rather than something decided
during the walk, because the two channels a fan-out produces are
typically discovered at *different* levels of the hierarchy -- one
inside the composite that owns the port, the other only after resolving
down through the boundary from its parent -- so there is no single
point during the recursive walk where both are in hand at once.

## What it refuses

Any leaf that is not `SDF` (checked as `n.pc_moc != "SDF"`), and any SDF
leaf with a port `rate_of` cannot determine, both throw
`std::runtime_error` naming the exact process and port -- never a
partial or plausible-looking graph. SADF is deliberately out of scope:
its rates are scenario-indexed, and SDF3 has a separate `sadf` XSD for
that, which is a different sub-phase, not a corner case of this one.

## The model this test builds, and why

A composite (`loopy`) whose one `SDF::combMN` both leaves the composite
*and* feeds an `SDF::delayn` that closes its own feedback loop --
exactly `toysdfMN`'s `compAvg` shape, read directly, not reconstructed
from a description -- plus a plain `SDF::comb` with different input and
output rates, and an `SDF::source`/`SDF::sink` pair for the
default-rate-1 path. A second, deliberately non-SDF composite checks
that `flatten()` actually refuses rather than silently succeeding.

One real modelling mistake was made and caught while building this,
worth recording because it is exactly the kind of thing a static
analysis tool exists to catch: the loop's second input was first
declared at rate 2 against `SDF::delayn`'s output, which is *always*
exactly one token per firing in steady state -- `n` only controls how
many extra tokens it prepends before the first one, not its per-firing
rate. `sdf3analysis-sdf --algo consistency` on the result said "Graph is
not consistent", which is what led to the fix, not a golden diff.

`gen/top.sdf3.xml` also lands in `tests/golden_ir/` and is diffed on
every run exactly like the introspection XML -- the harness's IR check
globs `gen/*.xml` without caring which backend wrote a given file, so
this exporter's output is pinned byte for byte the same way, for free.

## Verified against the real tool

The exported `gen/top.sdf3.xml` was handed to the actual `sdf3analysis-sdf`
binary, not just checked against the XSD:

```
sdf3analysis-sdf --graph gen/top.sdf3.xml --algo consistency
  -> Graph is consistent.
sdf3analysis-sdf --graph gen/top.sdf3.xml --algo deadlock
  -> Graph is deadlock free.
sdf3analysis-sdf --graph gen/top.sdf3.xml --algo repetition_vector
  -> src=1  upsample=1  lp1__averager1=2  lp1__state=2  snk=2  fanout=2
```

`upsample` firing once and producing 2 tokens against everything
downstream of it firing twice is exactly what its own declared rates
(in 1, out 2) say should happen -- checkable by hand, not only by the
tool. Re-run this by pointing `sdf3analysis-sdf` (built from
`~/Downloads/sdf3`, `build/release/Linux/bin/`) at this test's own
`gen/top.sdf3.xml` after running `./run.x`; the "failed to load
external entity" warnings about the XSD are the tool trying to fetch it
over the network and are not a sign the file was rejected -- the actual
verdict is the "Graph is ..." line after them.

## The other half of 3d: dot

Unlike SDF3, the dot view needed no new library code at all.
[f2dot](https://forsyde.ict.kth.se/trac/wiki/ForSyDe/f2dot) already
reads ForSyDe's own introspection XML directly -- `process_network`,
`leaf_process`, `composite_process`/`component_name`, `port` with
`bound_process`/`bound_port`, `signal` with `source`/`target` -- the
same tags and the same per-network-one-file convention `xml.hpp` has
always emitted, unchanged by 3a's rewrite of the backend into a view
over the IR. Run (Python 3, `pygraphviz`, GraphViz):

```
python3 f2dot -m forsyde path/to/gen/top.xml -o /some/output/dir
```

Confirmed directly against this tree's own golden IR: `f2dot3`
(a local Python 3 port) parses `tests/golden_ir/sy_mulacc__top.ir`'s
live equivalent and produces a `.dot` file GraphViz renders without
complaint. There is nothing in this repository to add for that side of
3d beyond recording that it already works, which this file is doing.
