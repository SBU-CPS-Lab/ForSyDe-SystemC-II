# tests/instantiate -- template instantiation coverage

A class template that nothing ever names is never compiled. Its member
function bodies are parsed for syntax, but nothing in them is checked
against real types, and nothing in them is checked against reality at
all. In a header-only library where the only exercise the code gets is
`examples/`, that means every constructor no example happens to use is
effectively unverified source text.

That is not hypothetical either. `UT::zipsN` was used by no example, by
no other library header, and by no `make_*` helper -- so it had never
been instantiated since it was written, and it carried two defects that
a single instantiation would have made obvious:

- its constructor named its *output* port `"iport1"`, so the
  introspection XML reported an output port called `iport1`;
- its `bindInfo()` resized `boundOutChans` to 1 and then assigned the
  output port to `boundInChans[0]` -- clobbering the first *input* port
  and leaving the output entry null, so the emitted XML lost an input
  and gained an empty `<port/>`.

`main.cpp` names every process constructor in the SY, SDF and UT
combinatorial families once, with concrete types, under
`FORSYDE_INTROSPECTION` -- which is what forces `bindInfo()` to be
instantiated too. It elaborates a module full of unconnected processes
and returns without calling `sc_start()`: building it is the point, and
running a simulation over deliberately unbound ports would only deadlock.
Its golden output is therefore just SystemC's banner.

This is a coverage floor, not a functional test: it proves each of these
templates *compiles* with its `bindInfo()` and its abstract-semantics
overrides in place. Whether each one then does the right thing is what
the examples and their golden files are for. Extend it whenever a
constructor is added, and especially whenever one has no example using
it -- that is exactly the case this directory exists to cover.
