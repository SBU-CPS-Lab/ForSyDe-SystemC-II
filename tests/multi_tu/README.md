# tests/multi_tu -- multi-translation-unit regression test

Every example in `examples/` is a single `.cpp` file. That means the
example suite, however green, can never catch a whole class of defect:
anything that only shows up when two translation units both include
`forsyde.hpp` and are linked together.

That is not hypothetical. Until the D1 fix, the library could not be used
from more than one `.cpp` file at all: `DEFINE_TYPE(X)` expanded to an
explicit specialization of `get_type_name<X>()`, and an explicit
specialization of a function template is an ordinary function, not a
template -- so it is not implicitly `inline` and gets no vague linkage.
Every including translation unit emitted its own strong definition, and
linking any two of them failed with fifteen `multiple definition of ...`
errors. Nothing in the repository exercised that, so it went unnoticed.

This directory is the smallest model that does exercise it: `sub.cpp`
defines a subsystem's constructor (so the process instantiation lives in
its own object file) while `main.cpp` holds `sc_main` and the top module,
and both include the full library. It is built and run by
`tests/run_examples.sh` alongside the examples, in both the
introspective and non-introspective configuration, and diffed against a
golden file the same way.

It is deliberately *not* under `examples/`: it is a test, not a model
anyone should learn the API from.
