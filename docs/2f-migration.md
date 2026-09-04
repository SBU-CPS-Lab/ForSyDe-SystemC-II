# 2f: migrating the examples off `make_*`

The library half of 2f is done and green. This is the spec for the other
half: rewriting all 44 examples against the new surface, then deleting
the ten helper headers.

It has to land in **one commit**. A clean break has no staged version,
and `forsyde.hpp` stops including the helpers in the same change.

## The rewrite rule

Every helper, without exception:

```
X::make_foo("n", args..., out..., in...)
    ->  add(new X::foo("n", args...))(out..., in...)
```

The argument order does not change. `make_*` put the outputs first and
so does the binder, which is what makes this mechanical rather than a
re-derivation.

Three consequences at each call site:

1. **The module has to own the process.** `SC_MODULE(top)` becomes
   `struct top : ForSyDe::composite`, and `add()` parks the pointer in
   the composite's `vector<unique_ptr>`. Nothing else about the module
   changes -- `SC_CTOR`, the signals, `start_of_simulation()` all stay.
   Do this for composite processes too, not only the top.

2. **The template arguments usually disappear.** `SY::comb2("m", f)`
   deduces `<int,int,int>` from `f`'s signature. Where it cannot --
   a generic lambda, or a constructor with no argument that mentions
   the token type (`fanout`, `zip`, `unzip`) -- write them out. The
   compiler says so; it does not deduce something wrong.

3. **A process bound to more than one signal on one port** keeps the
   extra bind explicit, as it is today:

   ```cpp
   auto& add1 = add(new SY::scomb2("add1", add_func));
   add1(acci, addi1, addi2);
   add1.oport1(result);          // the second reader of oport1
   ```

A composite's *own* ports are still bound by name (`m.a(sig)`).
`ForSyDe::composite` deliberately hides SystemC's positional binding,
and the static_assert says why.

## The exemplar

`examples/sy/mulacc` is migrated -- `top.hpp` and `mulacc.hpp`, a top and
a composite. Read those two files before starting; every other example is
the same shape.

## Deduction guides

SY has its full set, at the end of `sy_process_constructors.hpp` under
the comment "Deduction guides". The other MoCs have none yet, so add
them as the examples need them, following that block. The traits they
are written in terms of -- `ForSyDe::detail::arg_t`, which reads a
callable's parameter list and looks through whatever the MoC wraps the
value in -- are in `binding.hpp` and already handle `abst_ext`,
`std::vector` and `ttn_event`.

## What "done" means

Not "it builds". The check that matters here is the third one:

1. `tests/run_examples.sh` reports `new-fail=0` in both configurations.
   Run with `FORSYDE_SKIP_GDB=1` unless you want an xterm.
2. The harness log is unchanged, line for line.
3. **`git status` on `examples/**/gen` is empty.** The ~139 generated
   XML and dot files record which channel each port is bound to. A
   mis-ordered bind between two signals of the same type still runs and
   still prints plausible numbers -- this is the only check that catches
   it. If a `gen` file changes, a bind is wrong; do not reseed.
4. `tests/instantiate` and `tests/multi_tu` still build.

## Then, and only then

Delete `src/forsyde/*_helpers*.hpp` -- ten files, 3,944 lines, 137
functions -- and remove their includes from `forsyde.hpp`. Nothing in
the library refers to them, so this is a deletion rather than an
unpicking. Re-run all four checks above afterwards.
