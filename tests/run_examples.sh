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
# The two configurations are produced by overriding CFLAGS wholesale on
# the `make` command line (GNU make: a command-line assignment to a
# variable suppresses every in-makefile assignment to it, `+=` included)
# rather than by a real per-macro switch, because FORSYDE_INTROSPECTION
# is not independently toggleable from the two examples that also bake
# FORSYDE_COSIMULATION_WRAPPERS / FORSYDE_PARALLEL_SIM into the same
# CFLAGS += line. For those two examples specifically, the "off" pass
# also strips the wrapper/MPI macro, not introspection alone -- that is
# a real limitation of today's build system, not of this script, and it
# is the reason Phase 0 is splitting the coarse macros into independent
# CMake options.

set -u
cd "$(dirname "${BASH_SOURCE[0]}")/.."
ROOT="$(pwd)"
GOLDEN_DIR="$ROOT/tests/golden"
KNOWN_FAILURES="$ROOT/tests/known_failures.txt"
TIMEOUT=30

SEED=0
if [ "${1:-}" = "--seed" ]; then SEED=1; fi

BASE_CFLAGS="-Wall -Wno-deprecated -Wno-return-type -Wno-char-subscripts -pthread -g -O0 -std=c++17"

is_known_failure() {
    # $1 = "path config", e.g. "examples/sy/adaptivecodec on"
    grep -qxF "$1" "$KNOWN_FAILURES" 2>/dev/null
}

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

for dir in "${DIRS[@]}"; do
    name="${dir#examples/}"

    # An example whose Makefile hardcodes a toolchain that is not
    # installed here (currently: MPI's mpic++) is not a failure of the
    # code -- it is an environment gap. Skip it rather than reporting a
    # spurious regression.
    if grep -q '^CC[[:space:]]*=[[:space:]]*mpic++' "$dir/Makefile" 2>/dev/null \
       && ! command -v mpic++ >/dev/null 2>&1; then
        echo "SKIP  $name (mpic++ not installed)"
        skip=$((skip+1))
        continue
    fi

    for cfg in on off; do
        key="$name $cfg"
        golden="$GOLDEN_DIR/${name//\//_}.$cfg.out"

        ( cd "$dir" && make clean >/dev/null 2>&1 )

        if [ "$cfg" = on ]; then
            build_log=$( cd "$dir" && make 2>&1 )
        else
            build_log=$( cd "$dir" && make CFLAGS="$BASE_CFLAGS" 2>&1 )
        fi
        build_rc=$?

        if [ $build_rc -ne 0 ] || [ ! -x "$dir/run.x" ]; then
            if is_known_failure "$key"; then
                echo "FAIL  $key (known: build)"
                known=$((known+1))
            else
                echo "FAIL  $key (build) -- NEW"
                new_failures+=("$key (build)")
                new_fail=$((new_fail+1))
            fi
            continue
        fi

        run_out=$( cd "$dir" && timeout "$TIMEOUT" ./run.x 2>&1 )
        run_rc=$?

        if [ $run_rc -ne 0 ]; then
            if is_known_failure "$key"; then
                echo "FAIL  $key (known: run, exit $run_rc)"
                known=$((known+1))
            else
                echo "FAIL  $key (run, exit $run_rc) -- NEW"
                new_failures+=("$key (run, exit $run_rc)")
                new_fail=$((new_fail+1))
            fi
            continue
        fi

        if [ $SEED -eq 1 ]; then
            printf '%s' "$run_out" > "$golden"
            echo "SEED  $key"
            pass=$((pass+1))
            continue
        fi

        if [ ! -f "$golden" ]; then
            if is_known_failure "$key"; then
                echo "FAIL  $key (known: no golden)"
                known=$((known+1))
            else
                echo "FAIL  $key (no golden -- run with --seed) -- NEW"
                new_failures+=("$key (no golden)")
                new_fail=$((new_fail+1))
            fi
            continue
        fi

        if [ "$run_out" = "$(cat "$golden")" ]; then
            echo "PASS  $key"
            pass=$((pass+1))
        else
            if is_known_failure "$key"; then
                echo "FAIL  $key (known: golden mismatch)"
                known=$((known+1))
            else
                echo "FAIL  $key (output differs from golden) -- NEW"
                new_failures+=("$key (golden mismatch)")
                new_fail=$((new_fail+1))
            fi
        fi
    done
done

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
