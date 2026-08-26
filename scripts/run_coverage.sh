#!/usr/bin/env bash
# run_coverage.sh — build the coverage tree, run the test suites under LLVM
# source-based coverage, and produce a line/function/region report.
#
# INSTRUMENT TREE ONLY (see the linux-clang-coverage preset's description):
# nothing produced here is a quotable timing, a pin, or a baseline. Coverage
# runs are HEAVY (instrumented Debug, full suite) — run on a quiet box as a
# courtesy to concurrent measurement work, though nothing here ASSERTS wall.
#
# Usage:
#   scripts/run_coverage.sh                 # configure+build+test+report
#   scripts/run_coverage.sh --report-only   # re-report from existing profiles
#
# Output:
#   build-coverage/coverage/index.html      (llvm-cov show, HTML)
#   build-coverage/coverage/summary.txt     (llvm-cov report, per-file table)
#
# The summary's last line is the whole-library totals row. No threshold is
# enforced here: gating on a percentage is a policy decision taken explicitly
# (owner + ledger), not a default this script smuggles in.

set -euo pipefail
cd "$(dirname "$0")/.."

BUILD=build-coverage
PROFDIR="$BUILD/profiles"
OUT="$BUILD/coverage"
LLVM_PROFDATA=${LLVM_PROFDATA:-llvm-profdata}
LLVM_COV=${LLVM_COV:-llvm-cov}

if [[ "${1:-}" != "--report-only" ]]; then
    # CMAKE_ARGS: extra configure flags (CI pins HVEN_SIMD_ARCH here).
    cmake --preset linux-clang-coverage ${CMAKE_ARGS:-}
    cmake --build --preset linux-clang-coverage -j"$(nproc)"
    mkdir -p "$PROFDIR"
    # One raw profile per test process; %p disambiguates ctest's children.
    # CTEST_ARGS: extra ctest flags (CI passes --timeout etc.).
    # COVERAGE_TOLERATE_FAILURES=1: keep reporting coverage from whatever
    # ran even if cells failed -- a report-only CI lane wants the data and
    # the failure signal separately, not an aborted report. The ctest exit
    # status is preserved in the summary either way.
    CTEST_STATUS=0
    LLVM_PROFILE_FILE="$PWD/$PROFDIR/hven-%p.profraw" \
        ctest --test-dir "$BUILD" --output-on-failure ${CTEST_ARGS:-} \
        || CTEST_STATUS=$?
    if [[ $CTEST_STATUS -ne 0 && "${COVERAGE_TOLERATE_FAILURES:-0}" != "1" ]]; then
        echo "ctest exited $CTEST_STATUS (set COVERAGE_TOLERATE_FAILURES=1 to report anyway)"
        exit "$CTEST_STATUS"
    fi
    echo "ctest exit status: $CTEST_STATUS" > "$BUILD/ctest-status.txt"
fi

mkdir -p "$OUT"
"$LLVM_PROFDATA" merge -sparse "$PROFDIR"/*.profraw -o "$BUILD/hven.profdata"

# Every test binary ctest would run, from ctest's own registry -- the one
# authoritative list. llvm-cov merges across objects so the report covers
# the union of what the suites executed; A MISSED BINARY UNDER-REPORTS
# (its exclusive code reads 0%), which is exactly what the first CI run
# showed under the previous grep-based discovery.
mapfile -t BINARIES < <(ctest --test-dir "$BUILD" --show-only=json-v1 2>/dev/null \
    | python3 -c 'import json,sys; d=json.load(sys.stdin); print("\n".join(sorted({t["command"][0] for t in d.get("tests",[]) if t.get("command")})))')
if [[ ${#BINARIES[@]} -eq 0 ]]; then
    # Fallback: every executable under the test tree.
    mapfile -t BINARIES < <(find "$BUILD/tests" -maxdepth 3 -type f -executable | sort -u)
fi
OBJ_ARGS=()
for b in "${BINARIES[@]}"; do
    # ctest's registry also lists script tests (the golden rig's audit runs
    # a .sh); llvm-cov dies on a non-object file, so take only real ELF
    # executables.
    [[ -x "$b" ]] || continue
    [[ "$(head -c4 "$b" 2>/dev/null)" == $'\x7fELF' ]] || continue
    OBJ_ARGS+=(-object "$b")
done
echo "coverage report merges $(( ${#OBJ_ARGS[@]} / 2 )) ELF test binaries (of ${#BINARIES[@]} ctest entries)" >&2

"$LLVM_COV" report "${OBJ_ARGS[@]}" -instr-profile="$BUILD/hven.profdata" \
    -ignore-filename-regex='/dep/|/tests/|/bench/|/build' > "$OUT/summary.txt"
"$LLVM_COV" show "${OBJ_ARGS[@]}" -instr-profile="$BUILD/hven.profdata" \
    -ignore-filename-regex='/dep/|/tests/|/bench/|/build' \
    -format=html -output-dir="$OUT" >/dev/null
# lcov export for external ingestion (Codecov). Same object set and
# exclusions as the report above, so every consumer sees one story.
"$LLVM_COV" export "${OBJ_ARGS[@]}" -instr-profile="$BUILD/hven.profdata" \
    -ignore-filename-regex='/dep/|/tests/|/bench/|/build' \
    -format=lcov > "$OUT/coverage.lcov"

echo "== coverage totals (library sources; dep/, tests/, bench/ excluded) =="
tail -1 "$OUT/summary.txt"
echo "full table: $OUT/summary.txt   html: $OUT/index.html"
