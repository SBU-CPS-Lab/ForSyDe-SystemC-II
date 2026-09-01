#!/usr/bin/env bash
# tests/run_examples.sh -- build and run every example, in both the
# introspective and non-introspective configuration, and diff stdout
# against a captured golden file.
#
# Usage:
#   tests/run_examples.sh              # check mode: diff against goldens, used by CI
#   tests/run_examples.sh --seed       # (re)write the goldens from the current output
#
# Exit status: 0 if every example is either passing or listed in
# tests/known_failures.txt; 1 if anything unregistered regressed.
#
# What this does NOT do yet: this drives the existing per-example
# Makefiles, which is the only build system that exists at the moment.
# The "off" configuration is produced by overriding CFLAGS wholesale on
# the `make` command line (GNU make: a command-line assignment to a
# variable suppresses every in-makefile assignment to it, `+=` included)
# rather than by a real per-macro switch, because FORSYDE_INTROSPECTION
# is not independently toggleable from the two examples that also bake
# FORSYDE_WITH_GDB / FORSYDE_WITH_FMI / FORSYDE_WITH_MPI into the same
# CFLAGS += line (see the per-example Makefiles). For those examples
# specifically, the "off" pass also strips whichever of those macros
# that example's Makefile sets, not introspection alone -- a real
# limitation of today's Make-only build system, not of this script.
#
# CXXSTD (env var, default c++17) selects the -std passed to both
# configurations, and is what the CI workflow uses to run this same
# script once per {c++17, c++20} row. It does not require a separate
# golden set: nothing in this tree is C++20-specific yet, so the two
# rows are expected to reproduce byte-identical output, and a mismatch
# would itself be the finding.
#
# CXXSTD=c++20 needs a libsystemc that was itself built as C++20:
# SystemC 3.0.x encodes the standard into an ABI-guard symbol
# (sc_api_version_3_0_2_cxx202002L), so a C++20 model linked against a
# C++17-built library fails with "undefined reference to
# sc_core::sc_api_version_..." at link time and every example reports as
# a build failure at once. That is an environment mismatch, not a
# regression. The CI workflow builds SystemC once per standard, which is
# why the c++20 row passes there; a distribution package (Debian's
# libsystemc-dev, say) is typically C++17 and will not.

set -u
cd "$(dirname "${BASH_SOURCE[0]}")/.."
ROOT="$(pwd)"
GOLDEN_DIR="$ROOT/tests/golden"
KNOWN_FAILURES="$ROOT/tests/known_failures.txt"
TIMEOUT=30
CXXSTD="${CXXSTD:-c++17}"

SEED=0
if [ "${1:-}" = "--seed" ]; then SEED=1; fi

BASE_CFLAGS="-Wall -Wno-deprecated -Wno-return-type -Wno-char-subscripts -pthread -g -O0 -std=$CXXSTD"

is_known_failure() {
    # $1 = "path config", e.g. "examples/sy/adaptivecodec on"
    grep -qxF "$1" "$KNOWN_FAILURES" 2>/dev/null
}

# How many example directories to build concurrently. This suite is
# almost entirely compilation: one example takes ~4.4s to compile and
# ~0.01s to run, because forsyde.hpp is a deep header-only template
# library that every model recompiles from scratch. 68 builds
# single-threaded is ~5 minutes of one core while the rest of the
# machine idles.
#
# The unit of parallelism is the *directory*, not the build: the "on"
# and "off" configurations of one example share a build directory (the
# per-example Makefiles all emit main.o/run.x next to the source, and
# the harness does `make clean` between them), so running them
# concurrently would have them clobbering each other's object files and
# binaries. Directories are independent of each other -- including for
# the output files some examples write next to themselves (gen/,
# output.txt, fmuTmpXXXXXX/) -- so one job per directory is both the
# largest safe grain and enough to saturate a typical machine.
#
# Output stays byte-identical to a serial run: each job writes its lines
# to its own file and the parent prints them in DIRS order once all jobs
# are done, rather than letting them interleave as they finish. Counters
# are likewise tallied from those files, since a background job cannot
# increment a variable in the parent shell.
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

pass=0 fail=0 known=0 skip=0 new_fail=0
new_failures=()

# Discover every example directory that has a *tracked* Makefile, at any
# depth under examples/. Deliberately git-tracked rather than a plain
# `find`: several example directories exist on disk but are gitignored
# (superseded/discontinued experiments, or in-flight work on another
# branch) and must not be exercised here -- a fresh clone would never
# have them, and this harness exists to describe what a fresh clone can
# build and run.
# A "Makefile" alone is not sufficient: examples/mi/cruisecontrol/software
# is a companion native (non-SystemC) binary for the GDB co-simulation
# wrapper to attach to, with its own tiny Makefile that does not include
# Makefile.defs and does not produce run.x. Filter on that inclusion,
# which is what actually marks a directory as a ForSyDe-SystemC example.
mapfile -t DIRS < <(git ls-files -- 'examples/*/Makefile' 'examples/*/*/Makefile' 'examples/*/*/*/Makefile' \
    | xargs -n1 dirname | sort -u \
    | while read -r d; do grep -q 'Makefile\.defs' "$d/Makefile" && echo "$d"; done)

# tests/multi_tu is not an example -- it is a regression test that happens
# to be shaped like one, so it rides the same build/run/diff machinery
# below rather than duplicating it. Every example in this repository is a
# single .cpp file, which means the example suite structurally cannot
# catch a defect that only appears when two translation units both
# include forsyde.hpp and are linked together; D1 was exactly that, and
# went unnoticed for years. See tests/multi_tu/README.md.
DIRS+=("tests/multi_tu")

# tests/instantiate is the same idea for a different blind spot: a class
# template nothing names is never really compiled, so every process
# constructor no example happens to use is unverified source text.
# UT::zipsN was exactly that and had two defects to show for it. See
# tests/instantiate/README.md.
DIRS+=("tests/instantiate")

echo "CXXSTD=$CXXSTD $( [ $SEED = 1 ] && echo '(seed mode)' || echo '(check mode)' )"

# One job per directory. Everything this prints goes to the job's own
# log file; counters go to its tally file, because a background job
# cannot increment a variable in the parent shell.
run_dir() {
    local dir="$1"
    local tally="$2"
    name="${dir#examples/}"

    # An example whose Makefile hardcodes a toolchain that is not
    # installed here (currently: MPI's mpic++) is not a failure of the
    # code -- it is an environment gap. Skip it rather than reporting a
    # spurious regression.
    if grep -q '^CC[[:space:]]*=[[:space:]]*mpic++' "$dir/Makefile" 2>/dev/null \
       && ! command -v mpic++ >/dev/null 2>&1; then
        echo "SKIP  $name (mpic++ not installed)"
        echo "SKIP" >> "$tally"
        return
    fi

    # mi/cruisecontrol's Makefile links -lmigdb and libxml2
    # unconditionally (EXTRA_LIBS is not tied to which macros CFLAGS
    # actually carries), so *neither* config can link without both
    # present -- confirmed directly: core's CI job, which correctly has
    # neither installed, failed "off" (the config that never touches
    # gdbwrap or the FMI wrapper at all) with "collect2: error: ld
    # returned 1 exit status" on exactly this. Skip the whole directory
    # rather than let that surface as a spurious core regression; the
    # optional-gdb and optional-fmi CI jobs install both and exercise it
    # properly. Making EXTRA_LIBS conditional on the macros actually
    # requested is real work worth doing, not a one-line fix here --
    # natural fit for the Phase 1b wrapper rework.
    if [ "$name" = "mi/cruisecontrol" ] \
       && { ! echo 'int main(){}' | g++ -x c++ - -lmigdb -o /dev/null 2>/dev/null \
            || ! pkg-config --exists libxml-2.0 2>/dev/null; }; then
        echo "SKIP  $name (libmigdb and/or libxml2 not installed -- required to link either config)"
        echo "SKIP" >> "$tally"
        return
    fi

    for cfg in on off; do
        key="$name $cfg"
        golden="$GOLDEN_DIR/${name//\//_}.$cfg.out"

        # mi/cruisecontrol's "on" config spawns a real
        # `gdb --interpreter=mi` inside a real `xterm`. In a sandboxed
        # environment where GDB cannot set a controlling terminal
        # ("Operation not permitted"), that gdb session hangs
        # indefinitely rather than erroring out -- and unlike the gdb
        # subprocess itself, the xterm window it hung inside was never
        # covered by this script's subprocess sweep, so it survives as a
        # real, visible, un-closeable window on whoever's screen this
        # runs on. Confirmed directly: three such windows accumulated
        # here across three routine harness runs. Skip this one config
        # outright when FORSYDE_SKIP_GDB is set, rather than registering
        # it as a known-fail that still actually runs -- and still opens
        # a window -- every time. Set this permanently in the
        # environment on any machine where a GDB session cannot get a
        # real controlling terminal, which a desktop dev sandbox and a
        # locked-down CI runner both are, and a normal unrestricted
        # machine is not. The "off" config is unaffected -- it never
        # touches gdbwrap -- so only "on" is skipped, not the directory.
        if [ "$name" = "mi/cruisecontrol" ] && [ "$cfg" = on ] \
           && [ -n "${FORSYDE_SKIP_GDB:-}" ]; then
            echo "SKIP  $key (FORSYDE_SKIP_GDB set -- gdbwrap cannot get a controlling terminal here)"
            echo "SKIP" >> "$tally"
            continue
        fi

        ( cd "$dir" && make clean >/dev/null 2>&1 )

        if [ "$cfg" = on ]; then
            # OPT carries "-std=c++17" in Makefile.defs; override it the
            # same way CFLAGS is overridden below, rather than leaving
            # this row silently pinned to c++17 regardless of CXXSTD.
            build_log=$( cd "$dir" && make OPT="-O0 -std=$CXXSTD" 2>&1 )
        else
            build_log=$( cd "$dir" && make CFLAGS="$BASE_CFLAGS" 2>&1 )
        fi
        build_rc=$?

        if [ $build_rc -ne 0 ] || [ ! -x "$dir/run.x" ]; then
            if is_known_failure "$key"; then
                echo "FAIL  $key (known: build)"
                echo "KNOWN" >> "$tally"
            else
                echo "FAIL  $key (build) -- NEW"
                echo "$build_log" | sed 's/^/      /'
                echo "NEWFAIL $key (build)" >> "$tally"
            fi
            continue
        fi

        # Capture through a plain file, never `$(timeout ... ./run.x)`:
        # bash's command substitution waits for its read end to see EOF,
        # which needs every process holding the write end open to close
        # it -- not just the timed-out process. A wrapper example that
        # leaks a child subprocess (confirmed here: mi/cruisecontrol's
        # gdbwrap spawns `gdb --interpreter=mi` and never kills it once
        # attaching fails) leaves that child holding the pipe open, and
        # the whole harness hangs long after `timeout` has already killed
        # run.x -- the 30s budget below never even comes into it. A file
        # has no such holder-count semantics.
        #
        # Run it in the foreground: backgrounding and waiting on the pid
        # is unnecessary once the pipe is gone, since the wait is bounded
        # by `timeout` either way (bash waits for the subshell, the
        # subshell for `timeout`, and neither waits on a leaked
        # grandchild).
        #
        # The trailing `exit $?` is not redundant. Without it the
        # subshell is itself terminated by the signal that killed run.x,
        # and bash announces that on stderr -- "line 154: 501164
        # Aborted ..." -- embedding a pid that differs on every run. Two
        # examples here exit on SIGABRT, so that put two
        # nondeterministic lines into every log, which is precisely what
        # you do not want when diffing one run against another. Exiting
        # explicitly makes the subshell terminate *normally* with status
        # 128+signal, which bash reports nothing about, and which
        # $? below reads identically.
        run_tmp=$(mktemp)
        ( cd "$dir" && timeout "$TIMEOUT" ./run.x; exit $? ) > "$run_tmp" 2>&1 < /dev/null
        run_rc=$?
        # SystemC prints its own copyright banner unconditionally on
        # first sc_start(), and one line of it embeds the exact build
        # timestamp of whichever libsystemc.so is linked -- confirmed by
        # building SystemC 3.0.2 from source and diffing its output
        # against this apt package's: identical simulation output,
        # different banner date. Every CI row builds SystemC from
        # source, so that line would never match a golden seeded here
        # without this. Nothing else in the banner varies.
        run_out=$(sed -E 's/^([[:space:]]*SystemC .*-Accellera --- ).*$/\1<build-date-elided>/' "$run_tmp")
        rm -f "$run_tmp"
        # Whatever leaked process caused the hang this fixes is still
        # leaked -- the file redirect stops it from blocking the harness,
        # it doesn't stop it from existing. Sweep the one leak diagnosed
        # so far (gdbwrap's child) rather than leaving it to accumulate
        # across a 33-example run; a real per-example process-group kill
        # is Phase-1b's job when the wrapper itself is rebuilt.
        #
        # The [=] is not a typo. `pkill -f` matches against the whole
        # command line, and pkill's own command line contains the
        # pattern it was given -- so while pkill excludes *itself*, with
        # JOBS>1 it happily matches and SIGKILLs the pkill that another
        # job is running at that moment. That surfaced as
        #   run_examples.sh: line 253: 685555 Killed  pkill -9 -f ...
        # appearing in some runs and not others, at whichever job
        # happened to overlap: nondeterministic output from the one
        # script whose whole job is to diff one run against another.
        # Bracketing the '=' makes the pattern match a real gdb argv
        # (`--interpreter=mi`) but not the literal text `[=]` sitting in
        # a sibling pkill's own argv.
        pkill -9 -f 'gdb --interpreter[=]mi' >/dev/null 2>&1

        if [ $run_rc -ne 0 ]; then
            if is_known_failure "$key"; then
                echo "FAIL  $key (known: run, exit $run_rc)"
                echo "KNOWN" >> "$tally"
            else
                echo "FAIL  $key (run, exit $run_rc) -- NEW"
                echo "NEWFAIL $key (run, exit $run_rc)" >> "$tally"
            fi
            continue
        fi

        if [ $SEED -eq 1 ]; then
            if is_known_failure "$key"; then
                # A registered failure that happens to exit 0 on this
                # particular seeding run is not evidence it's fixed --
                # sadf/mp4dec on is registered specifically *because* its
                # output is non-deterministic even though it often exits
                # 0, and seeding a golden from one lucky run would just
                # make the next ordinary run "regress" against it. Known
                # failures are never golden-tracked, regardless of what
                # this one run's exit code was.
                echo "SEED  $key (known -- not golden-tracked)"
                echo "KNOWN" >> "$tally"
            else
                printf '%s' "$run_out" > "$golden"
                echo "SEED  $key"
                echo "PASS" >> "$tally"
            fi
            continue
        fi

        if [ ! -f "$golden" ]; then
            if is_known_failure "$key"; then
                echo "FAIL  $key (known: no golden)"
                echo "KNOWN" >> "$tally"
            else
                echo "FAIL  $key (no golden -- run with --seed) -- NEW"
                echo "NEWFAIL $key (no golden)" >> "$tally"
            fi
            continue
        fi

        if [ "$run_out" = "$(cat "$golden")" ]; then
            echo "PASS  $key"
            echo "PASS" >> "$tally"
        else
            if is_known_failure "$key"; then
                echo "FAIL  $key (known: golden mismatch)"
                echo "KNOWN" >> "$tally"
            else
                echo "FAIL  $key (output differs from golden) -- NEW"
                echo "NEWFAIL $key (golden mismatch)" >> "$tally"
            fi
        fi
    done
}

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

job_idx=0
for dir in "${DIRS[@]}"; do
    job_idx=$((job_idx+1))
    slot="$(printf '%04d' "$job_idx")"
    # Bounded concurrency: block until a slot frees up. `wait -n` needs
    # bash 4.3+; fall back to waiting for all outstanding jobs if it is
    # unavailable, which is slower but still correct.
    while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do
        wait -n 2>/dev/null || wait
    done
    run_dir "$dir" "$WORK/$slot.tally" > "$WORK/$slot.log" 2>&1 &
done
wait

# Print every job's output in DIRS order (the %04d prefix makes the glob
# sort match it), so a parallel run reads exactly like a serial one.
for f in "$WORK"/*.log; do [ -f "$f" ] && cat "$f"; done

while IFS= read -r tline; do
    case "$tline" in
        PASS)     pass=$((pass+1)) ;;
        KNOWN)    known=$((known+1)) ;;
        SKIP)     skip=$((skip+1)) ;;
        NEWFAIL\ *) new_fail=$((new_fail+1)); new_failures+=("${tline#NEWFAIL }") ;;
    esac
done < <(cat "$WORK"/*.tally 2>/dev/null)


echo
echo "pass=$pass known-fail=$known skip=$skip new-fail=$new_fail"

if [ "$new_fail" -gt 0 ]; then
    echo
    echo "New, unregistered failures:"
    for f in "${new_failures[@]}"; do echo "  - $f"; done
    echo
    echo "If this is expected, add the affected \"path config\" lines to tests/known_failures.txt."
    exit 1
fi

exit 0
