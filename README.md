# ForSyDe-SystemC II

A SystemC-embedded implementation of ForSyDe (Formal System Design), a
methodology for designing heterogeneous embedded systems at a higher level
of abstraction, with transformational design refinement bridging the
abstraction gap down to implementation.

This library provides ForSyDe's modeling framework as a header-only
SystemC-embedded domain-specific language, with process constructors and
signals for several Models of Computation (MoCs): Synchronous (SY), Untimed
(UT) and its Synchronous Dataflow variant (SDF), Synchronous Adaptive
Dataflow (SADF), Distributed Discrete-Event (DDE), Discrete-Time (DT), and
Continuous-Time (CT).

## This is the successor repository

**ForSyDe-SystemC II** is where active development happens: the C++20
consolidation and redesign that follows on from the original
[ForSyDe-SystemC](https://github.com/forsyde/ForSyDe-SystemC), which
remains available for existing projects that depend on its current API and
is otherwise in maintenance mode. This repository was seeded from that
project's `v1.0` tag — the point at which its foundational build system and
defect-repair work was completed — and carries its full history forward.

If you are starting a new project, or want the actively developed line,
you are in the right place. If you have an existing model built against
`ForSyDe-SystemC`'s current API and want to keep building it exactly as
is, that repository remains the one to use.

## Installation

ForSyDe-SystemC is a header-only library: add its `src/` directory to your
compiler's include path (or copy it to a global include location), and you
are done — there is nothing to build or install separately.

You will also need:

- **SystemC** on your include and library paths, built and linked against.
  This project is developed and tested against SystemC 3.0.2.
- **A C++17 (or later) compiler.** The CI matrix covers both C++17 and
  C++20; there is nothing in the core library that is C++20-only yet.
- **[Boost](https://www.boost.org/)**, specifically `boost/numeric/ublas`,
  required unconditionally by the DDE MoC.

Everything above is all that's needed for the core library and every
example that doesn't opt into an optional backend. Four further
dependencies are each gated behind their own build macro, so you only need
the ones you actually use:

| Macro | Enables | Extra dependency |
|---|---|---|
| `FORSYDE_WITH_GDB` | GDB-based co-simulation wrapper | `libmigdb`, an `xterm` |
| `FORSYDE_WITH_FMI` | FMI 2.0 co-simulation | `libxml2` |
| `FORSYDE_WITH_MPI` | Parallel/distributed simulation | An MPI implementation (e.g. Open MPI) |
| `FORSYDE_WITH_ROS` | ROS co-simulation wrapper | A ROS distribution |
| `FORSYDE_INTROSPECTION` | Structural XML export of a model | — |

The example suite under `examples/` builds with plain `make`, using the
shared `examples/Makefile.defs`; each example directory has its own small
`Makefile` that includes it. `tests/run_examples.sh` builds and runs every
example in both the introspective and non-introspective configuration,
diffs the output against golden files, and is what CI runs on every push —
see `.github/workflows/ci.yml` for the exact matrix.

## Documentation and community

For API documentation, tutorials, and information about the broader
ForSyDe methodology (including its Haskell-embedded sibling), visit
**<https://forsyde.github.io/>**.

## License

BSD 3-Clause. See [`LICENSE`](LICENSE).
