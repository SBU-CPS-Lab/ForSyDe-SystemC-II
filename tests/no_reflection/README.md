# tests/no_reflection -- the opt-out, compiled

Reflection is on by default. `FORSYDE_NO_REFLECTION` turns it off, for a
model that wants the smaller object file and the faster rebuild and has
no use for the structural record. See `src/forsyde/config.hpp` for the
measurements the default rests on.

This directory is the only thing in the tree that compiles that
configuration, and it exists for the reason every sub-phase of Phase 2
re-learned: a code path nothing instantiates is not "probably fine", it
is unverified source text. Four separate process constructors were
broken precisely because no example named them (`SY::sunzip`,
`UT::zipsN`, `DDE::unzipX`, `MI::SY2SDF`), and `tests/instantiate`
exists because of it.

An opt-out is a whole build configuration in that category. Turning it
on compiles away the base class's pure-virtual `bindInfo()`, every
`arg_vec` push in every constructor, the recording `operator()`
overloads on every port, and the `introspective_port` /
`introspective_channel` bases themselves. Without something building it,
the first person to try it is the one who finds out -- which is exactly
what happened to `examples/mi/ir_uwb_radar` during 3b: it defines its
own process class, that class's `bindInfo()` was still guarded on
`FORSYDE_INTROSPECTION`, and so it stopped compiling the moment the base
class started requiring it in ordinary builds. That was caught only
because the example already happened to be registered as a known
failure for an unrelated reason.

## How the two rows work

The harness builds every directory twice. Here:

- **"on"** uses this directory's Makefile, which defines
  `FORSYDE_NO_REFLECTION` -- so the **"on" row is the opt-out build**.
- **"off"** overrides `CFLAGS` wholesale and so drops the macro, giving
  an ordinary build with reflection on.

The two goldens are the two sides, and the first line of each says which
is which.

The model is deliberately a spread rather than a single process -- an SY
chain, an SDF actor with a rate, a UT process and a DT one -- so that
the ports, signals, bindings and `bindInfo` of four MoCs are
instantiated without reflection, rather than one.
