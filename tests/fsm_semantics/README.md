# tests/fsm_semantics -- what the state machines actually compute

`tests/instantiate` proves the process constructors compile. Nothing
proved they compute the right thing.

Not one example in this repository uses `SY::moore`, `UT::moore`,
`UT::mooreMN`, `UT::scan` or `UT::scand`. They had no golden file, no
call site, and no reader -- so when SY's Moore machine and UT's Moore
machine drifted apart, nothing could tell. UT emitted its first output
twice:

    SY::moore   0 10 20 30 40 ...
    UT::moore   0  0 10 20 30 ...      <-- before this test existed

Jantsch settles which is right. (3.4) defines `mooreU` to emit `f(w_i)`
and move to `w_{i+1} = g(w_i, a_i)`; (4.5) defines `mooreS(g,f,w0)` as
`mooreU(1,g,f,w0)`, so the synchronous and untimed Moore machines are
the same constructor at a fixed rate and cannot legitimately differ.
UT was wrong; it is fixed, and this directory is why it stays fixed.

Every machine here is the same counter -- `g(w,a) = w+1` from `w0 = 0`
-- so the output reads as the sequence of states it passed through and
an off-by-one is visible rather than derived. Two details are
deliberate:

- **`f` is not the identity** (`f(w) = 10w`). With `f = id`, a Moore
  machine and a `scand` emit the same sequence, and this test would
  agree with an implementation that confused them. A `scand` emits the
  state; a Moore machine emits `f` of it.
- **the input is a ramp, not a constant** (`f(w,a) = 100w + a` for the
  Mealy machines). With a constant input a Mealy machine cannot be told
  from a Moore one.

Expected, and pinned by the golden file:

| constructor | book | sequence |
|---|---|---|
| `UT::scan`  | (3.2) emits `w_{i+1}`    | 1 2 3 4 ... |
| `UT::scand` | (3.8) `<w0>` + `scanU`   | 0 1 2 3 ... |
| `SY/UT::moore` | (3.4) emits `f(w_i)`  | 0 10 20 30 ... |
| `SY/UT::mealy` | (3.3) emits `f(w_i,a_i)` | 0 101 202 303 ... |

`SY::moore` and `SY::mealy` skip the read on their first evaluation
cycle so that they emit before consuming, which is what makes them
usable in a feedback loop. That is why their sequences are one longer
than the Mealy machines' over the same run.

Add a row here whenever a state-machine constructor is added or its
semantics are argued about. Being able to run the argument is worth more
than being able to win it.
