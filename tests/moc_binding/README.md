# tests/moc_binding -- which MoCs may be bound to which (D13)

Every ForSyDe signal and port now carries its model of computation as a
compile-time tag, and `moc_traits.hpp` turns those tags into a rule about
which bindings are legal. This directory pins the rule and proves the
legal cases still bind.

## The rule

A signal's **carrier** is what it physically holds:

| carrier | token | MoCs |
|---|---|---|
| untimed | `T` | UT, SDF, SADF |
| synchronous | `abst_ext<T>` | SY, DT |
| timed | `ttn_event<T>` | DDE |
| continuous | `sub_signal` | CT |

Crossing a carrier has always been impossible: those are different C++
types, so the compiler rejected it long before any of this existed.
Measured, not assumed -- binding an SY signal to an SDF port has never
compiled.

What was unchecked is the **inside** of a carrier, where the token types
coincide but the MoCs do not. UT, SDF and SADF all carry a bare `T`; SY
and DT both carry `abst_ext<T>`. Every one of those combinations bound
silently, which is D13.

Within a carrier the MoCs differ in their **firing rule** -- how a
process decides how many tokens to take per evaluation cycle. SDF fixes
the rates at construction, SADF indexes them by a scenario token from a
detector, UT lets the actor decide from its own state. A producer that
meets the stricter contract already meets the looser one, so

    SDF -> SADF -> UT

is not quite the whole story, because SDF and SADF are *mutually*
compatible rather than ordered: an SADF process is an SDF process within
any one scenario. That is why SADF re-exports several of SDF's
constructors instead of repeating them, and why SADF models call
`SDF::make_unzip` and `SDF::make_zip` directly on SADF signals. Both
widen to UT, which assumes least of its producers.

`UT -> SDF` and `UT -> SADF` are genuine *narrowings*: a UT actor picks
its rate per firing from its own state, which can break the static
schedule an SDF consumer is built on, and nothing at the binding site
can know whether it will. That wants a named refinement checking the
rate at each firing.

SY and DT share a carrier but neither refines the other: an absent event
means "no value this tick" in SY and "a tick elapsed" in DT, which is
what lets a DT process consume a state-dependent number of them to
measure time. Reading one as the other is a change of meaning, so they
are *incomparable* and rejected outright.

## What is pinned

`main.cpp` asserts the lattice directly, elaborates every legal binding,
and prints the full 7x7 verdict matrix, which the golden file fixes. The
rejected cases cannot be exercised from a program that has to compile --
that is the point of them -- so they are pinned by the `narrows_to` and
`incomparable` assertions instead.

## Two levels of check

**Always on.** A binding between MoCs that share a carrier but not a
meaning is rejected outright, as is one crossing carriers (which the
token types catch first anyway). SY and DT are the case that matters:
both carry `abst_ext<T>` one per tick, but an absent event is "no value
this tick" in one and "a tick elapsed" in the other, so reading either
as the other changes what the model says.

**Under `FORSYDE_STRICT_MOC`**, which only this test defines: narrowing
is reported too. That is deliberately not the default. Within the
untimed carrier the MoCs are meant to interoperate, and a real narrowing
-- `UT -> SDF` -- is a question about a model rather than a broken
binding. Keeping the relation available means a future
`refine<UT,SDF>(rate)` process and the analysis tools have something to
act on, without the library refusing to compile models that are correct.

Two of the shared components got clearer types along the way.
`SDF::delayn`, `source`, `sink` and `combMN` -- the four SADF re-exports
-- now declare carrier-U ports (`UT_in`, `UT_out`) rather than SDF ones,
because that is what they are: generic over the untimed carrier, not
specific to SDF. Both SDF and SADF reach them by widening.
