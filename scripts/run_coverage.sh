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

# ---------------------------------------------------------------------------
# THE INSTRUMENT-TREE EXCLUSION LIST (taken at M6 W6 T0, 2026-09-13)
#
# The disposition the M5 ledger registered and left open --
# docs/notes/2026-08-m5-ledger.md:440-461: three cells failed under the first
# CI coverage run, "REGISTERED to characterize (likely disposition: a small
# instrument-tree exclusion list or pin-relaxation ... decided at the next
# window, NEVER by silently editing the pins)". It is taken here, in this
# script, where an exclusion costs coverage and nothing else: no pin, no test
# and no engine source is touched by anything below.
#
# WHAT IT IS FOR. A byte-strict float pin that holds under the uniform flag
# regime can fail in this tree: the coverage flags change codegen (the preset's
# own description says so, CMakePresets.json's linux-clang-coverage entry), and
# a last-digit move on a bit-exact compare is the L-1 divergence family. Such a
# cell can be dropped from THIS run, by name, with its reason written down.
#
# TWO CONDITIONS, BOTH REQUIRED, before a cell is named in the regex below:
#   (i)  it is OBSERVED to fail under the instrument tree for that reason --
#        not merely suspected of being able to; and
#   (ii) the paths it covers are covered by other cells that pass here.
# Condition (ii) is not a formality. A FAILING gtest still writes its .profraw,
# so COVERAGE_TOLERATE_FAILURES keeps a failing cell's coverage while -E DROPS
# it. A cell that fails instrumented but covers paths nothing else reaches is
# therefore TOLERATED, not excluded -- and is named here as such, so the next
# reader knows the difference was considered.
#
# THE LIST IS EMPTY, and that is the finding, not an omission. The three cells
# M5 saw fail (ledger :447-449; CI run 32924315809, a GitHub ubuntu-latest
# runner) --
#     B1Gate.EqualityOnlyWarmSolvesAreBitIdenticalAcrossTheRepair
#     CorpusTask6bPhaseB.TheShippedKSsnConfigurationIsUnmovedByTheFourLevers
#     SsnEngineLocal.WeaklyActiveRowFinishesUncertain
# -- all three survive W5's renames under those exact names, all three RAN in
# the W6 T0 instrumented read at 7da77b5d, and all three PASSED; so did every
# other cell (2544 registered, 2542 executed, 0 failed, 2 disabled in the tree;
# ctest exit 0), as at the W2 read
# (docs/notes/data/2026-09-m6-w2-acceptance/coverage-areas.txt:4). None of them
# meets condition (i) on this hardware, so none is excluded: excluding a cell
# that passes would drop real coverage and buy nothing. M5's observation was on
# a foreign runner microarch and the ledger says exactly that, so the remedy for
# that machine stays what it already is -- COVERAGE_TOLERATE_FAILURES=1 in the
# CI lane, which keeps both the report and the failure signal.
# Two of the three have their own history, worth having beside the names:
#   * SsnEngineLocal.WeaklyActiveRowFinishesUncertain was made flake-immune by
#     construction in M6 W0.4 (docs/notes/2026-08-m6-ledger.md:164-176) -- its
#     per-backend exact pins are retired, so the L-1 route into it is gone.
#   * CorpusTask6bPhaseB.TheShippedKSsnConfigurationIsUnmovedByTheFourLevers
#     still has a byte-strict Debug-arm compare (tests/sqp/test_corpus_cells.cpp
#     :3866) where the Release arm already uses a 1e-5 residual gate (:3818).
#     If it ever does fail here, that gate -- W6 T1's item -- is the remedy, not
#     this list: the fix belongs at the compare, where it is visible to every
#     tree, rather than in a coverage-only exclusion.
#
# PROVISIONAL: re-derived at W6 T2's close read. To exclude a cell, add its
# exact ctest name to the regex and write its two conditions above it.
# COVERAGE_EXCLUDE_REGEX in the environment overrides this default for a
# one-off run; it does not replace the record above.
# ---------------------------------------------------------------------------
COVERAGE_EXCLUDE_REGEX=${COVERAGE_EXCLUDE_REGEX:-}

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
    EXCLUDE_ARGS=()
    if [[ -n "$COVERAGE_EXCLUDE_REGEX" ]]; then
        EXCLUDE_ARGS=(-E "$COVERAGE_EXCLUDE_REGEX")
        echo "instrument-tree exclusion in force: -E '$COVERAGE_EXCLUDE_REGEX'" >&2
    fi
    LLVM_PROFILE_FILE="$PWD/$PROFDIR/hven-%p.profraw" \
        ctest --test-dir "$BUILD" --output-on-failure \
        "${EXCLUDE_ARGS[@]}" ${CTEST_ARGS:-} \
        || CTEST_STATUS=$?
    if [[ $CTEST_STATUS -ne 0 && "${COVERAGE_TOLERATE_FAILURES:-0}" != "1" ]]; then
        echo "ctest exited $CTEST_STATUS (set COVERAGE_TOLERATE_FAILURES=1 to report anyway)"
        exit "$CTEST_STATUS"
    fi
    # Both halves of what the run was: its status, and what it was allowed to
    # skip. A report read later cannot tell an empty exclusion from a wide one
    # unless the run says so here.
    {
        echo "ctest exit status: $CTEST_STATUS"
        echo "instrument-tree exclusion regex: ${COVERAGE_EXCLUDE_REGEX:-(none -- every registered cell ran)}"
    } > "$BUILD/ctest-status.txt"
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
