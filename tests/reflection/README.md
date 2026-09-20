# tests/reflection -- something that actually subscribes

`ForSyDe::reflection` (src/forsyde/reflection.hpp) is the runtime half of
the reflection service: a process reports what it just did, and whatever
wants to know subscribes. This directory is the thing that subscribes.

## Why it has to exist

Self-reporting used to work by handing a process a `FILE**` at
construction, guarded by `FORSYDE_SELF_REPORTING`. That macro was
**commented out in every Makefile in this repository**, so the reporting
path was compiled by nothing at all. It is the same hazard
`tests/instantiate` and `tests/no_reflection` exist to close, and the one
that had already produced four broken process constructors elsewhere in
the library.

Moving reporting onto a service does not fix that by itself. If nothing
subscribes, the path is exactly as unexercised as it was before -- so
this test is not an optional extra on top of 3c, it is the half that
keeps 3c from rotting the same way.

## What it checks

A two-scenario SADF model (a detector alternating the kernel's scenario)
with an observer installed at `start_of_simulation`:

- a `kernelMN` and a `detectorMN` both report;
- every firing carries a **fully qualified** kind (`SADF::kernelMN`),
  names its instance, and quotes the scenario it fired under together
  with the rates that scenario selects;
- firings arrive in non-decreasing simulated time;
- `short_kind` strips the MoC, and `as_report_line` renders exactly the
  format the self-report pipe has always carried -- that format is read
  by things outside this repository, and changing the representation is
  not what this sub-phase is for.

## The opt-out

Under `FORSYDE_NO_REFLECTION` the vocabulary keeps its shape and goes
inert: `observe()` accepts a subscriber, `observed()` is a compile-time
false, `report()` does nothing. That is deliberate -- a process reports
through the same three lines either way, so turning reflection off is a
decision about whether anything *listens*, not a different way of
writing a model.

`tests/no_reflection` is what compiles that side, since its "on" row is
the opt-out build. This directory's `#else` branch documents the same
expectation for anyone who builds it that way directly.
