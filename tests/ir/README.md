# tests/ir -- the model graph, checked as a graph

Until 3a, the structure of an elaborated model was not a thing you could
hold. `XMLExport::traverse` walked SystemC's object tree and wrote what
it found straight into a rapidxml document, so discovering the structure
and rendering it were one loop. There was exactly one thing that could
be done with the graph, it had to be done during that one walk, and the
only way to check the graph was to read the XML -- which exercises the
backend and the graph at once and cannot say which of the two is wrong.

`ir::build` (src/forsyde/ir.hpp) now returns an `ir::model`, and the XML
backend is a view over it. The golden corpus in `tests/golden_ir` pins
the view: the 50 files it holds did not change by a byte when the
backend was rewritten. This test pins the other half -- the graph
itself.

## What it checks

It builds a model whose shape is known by construction (two levels of
hierarchy, a composite instance with boundary ports, leaf processes
carrying constructor arguments, signals crossing between them), dumps
the IR canonically, and then asserts the invariants a *consumer* of the
IR depends on:

- **`order` covers every node, port and channel exactly once.** A
  network keeps three typed lists plus an ordered list of which-list,
  which-index slots, because a backend reproducing a file byte for byte
  needs elaboration order while a consumer wants to iterate all the
  channels. Those two views have to describe the same graph.
- **Every channel endpoint names a node in the same network.** An edge
  cannot point at nothing.
- **Every boundary port names a node in its own network.**
- **Every composite node's `sub` resolves to a real network, no two
  composite nodes claim the same one, and every network but the top is
  claimed by exactly one.** That is what makes `model::networks` a tree
  rather than a pile.

None of these is exercised by the XML backend, which reads the fields it
needs and would happily render a graph with a dangling edge in it.

## Why the "off" golden says "unavailable"

The IR is reachable only under `FORSYDE_INTROSPECTION` today, because it
is built from `process::arg_vec`, `boundInChans` and `boundOutChans` --
members the macro compiles away entirely, along with the binding
operators that record which channel a port was bound to. In the "off"
configuration there is no structural information recorded anywhere, so
there is nothing to build an IR from.

That is the state 3b changes: reflection becomes always compiled in, and
`FORSYDE_INTROSPECTION` goes back to meaning only "also write the XML".
When it does, this test's off golden stops saying "unavailable" and
starts saying what the on golden says -- so the two goldens together are
the marker for whether 3b has landed.
