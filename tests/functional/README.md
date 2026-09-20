# tests/functional -- one claim, and the diff that keeps it true

`ForSyDe::fn` (`src/forsyde/functional.hpp`) is the applicative surface:
a network written as composition on signal handles rather than as
declare-then-bind.

```cpp
fn::network net(*this);
auto s1 = fn::SY::constant(1, 4)(net);
auto s3 = (fn::SY::comb(add_one) | fn::SY::comb(times_two))(s1);
          fn::SY::sink(report)(s3);
```

The roadmap's word for this is **a thin front end that calls the
explicit one**, and "thin" is a testable claim rather than a
description. So this test builds the same four-process network twice --
once each way -- and compares the two IRs node for node, port for port,
channel for channel. If the functional surface ever drifts into being a
second implementation, that comparison says so.

It comes out identical down to the instance names, which is not a
coincidence arranged by the test: the surface names a process by its
constructor plus a serial number (`comb1`, `comb2`), which is the
convention the hand-written composite was already following.

## What else is checked here

Two things the surface adds that the explicit one has no equivalent of,
and which therefore have nowhere else to be checked:

- **Automatic naming**, above.
- **`declare()` / `into()`**, the feedback pair. An applicative surface
  cannot tie a knot without laziness, so the back edge is declared and
  then assigned. A declared signal that never gets a producer is an
  error *at the end of the description* -- which is the whole point,
  because the explicit surface's version of that mistake is a FIFO
  nobody writes and a simulation that deadlocks in silence.

## Two limitations worth knowing

**No fan-out.** An `sc_fifo` takes exactly one reader, and the explicit
surface's answer -- `readers(...)` -- is a decision made by the
*producer*. A handle that has already been returned cannot be split
after the fact, so applying two processes to one handle is an
elaboration error rather than an implicit broadcast. The feedback model
here works around it by printing from inside the accumulator's own
function instead of adding a sink. Supporting it properly means letting
a constructor be applied with a declared fan-out --
`comb(f).fanout<2>()` returning a pair of handles -- which is a real
addition rather than a fix, and has not been made.

**Composition is flat.** `a | b` applies a and then b in the network
being described. The roadmap suggested every `|` emit a composite around
its operands, making composition and hierarchy the same operation; that
is elegant, and it also means a three-stage pipeline written with two
bars produces two nested composites nobody asked for. Hierarchy stays
something a model asks for, by writing a composite.

## Why the failure paths are built the way they are

Both of the awkward-looking arrangements in `main.cpp` are there for the
same reason, and it is worth stating once: **SystemC will not let a
channel be created after elaboration**. A signal built in `sc_main`
after `sc_start()` is not a test fixture, it is a crash. So the
"declared but never written" check lives inside a composite's
constructor, which is the only place a `fn::network` exists and the only
place a signal may legally be made.
