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

is a *widening*: sound, free, no process, accepted silently. The reverse
is a *narrowing* and is rejected, because a UT actor's data-dependent
rate can break the static schedule an SDF consumer is built on and
nothing at the binding site can know whether it will. That needs a named
refinement that checks the rate at each firing, not a silent coercion.

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

## Why the assertions are not on by default yet

`check_bind()`'s asserts are behind `FORSYDE_STRICT_MOC`, which this
test's Makefile defines and nothing else does. Turning them on globally
rejects four models in this repository, all for one reason: SADF
re-exports several of SDF's components -- `delayn`, `source`, `sink`,
`combMN` -- as type aliases, so their ports are typed `SDF_in` and
`SDF_out`. An SADF model calling `SADF::make_delayn` is therefore binding
an SADF signal to an SDF port, which is a narrowing, and the check is
right to say so. The aliases are what make it fire.

There is more than one defensible way out, and they differ in what they
claim about the library's structure rather than only in effort: give SADF
its own components; retype the shared ones as carrier-U (`UT_in`,
`UT_out`) so SDF and SADF both reach them by widening; or decide these
particular rate-static components are polymorphic in their MoC. That is a
design decision, so the machinery ships checked and tested, the models
keep building, and the choice stays open.
