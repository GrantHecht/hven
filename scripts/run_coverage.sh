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
    cmake --preset linux-clang-coverage
    cmake --build --preset linux-clang-coverage -j"$(nproc)"
    mkdir -p "$PROFDIR"
    # One raw profile per test process; %p disambiguates ctest's children.
    LLVM_PROFILE_FILE="$PWD/$PROFDIR/hven-%p.profraw" \
        ctest --test-dir "$BUILD" --output-on-failure
fi

mkdir -p "$OUT"
"$LLVM_PROFDATA" merge -sparse "$PROFDIR"/*.profraw -o "$BUILD/hven.profdata"

# Every test binary ctest registered, deduplicated; llvm-cov merges across
# objects so the report covers the union of what the suites executed.
mapfile -t BINARIES < <(grep -rhoP '(?<=add_test\(\[=+\[[^]]{0,200}\]=+\] ")[^"]+' \
    "$BUILD" --include='*_tests.cmake' 2>/dev/null | sort -u | head -50)
if [[ ${#BINARIES[@]} -eq 0 ]]; then
    # Fallback: the known suite executables.
    mapfile -t BINARIES < <(find "$BUILD/tests" -maxdepth 2 -type f -executable \
        -name '*test*' | sort -u)
fi
OBJ_ARGS=()
for b in "${BINARIES[@]}"; do [[ -x "$b" ]] && OBJ_ARGS+=(-object "$b"); done

"$LLVM_COV" report "${OBJ_ARGS[@]}" -instr-profile="$BUILD/hven.profdata" \
    -ignore-filename-regex='dep/|tests/|bench/|build' > "$OUT/summary.txt"
"$LLVM_COV" show "${OBJ_ARGS[@]}" -instr-profile="$BUILD/hven.profdata" \
    -ignore-filename-regex='dep/|tests/|bench/|build' \
    -format=html -output-dir="$OUT" >/dev/null

echo "== coverage totals (library sources; dep/, tests/, bench/ excluded) =="
tail -1 "$OUT/summary.txt"
echo "full table: $OUT/summary.txt   html: $OUT/index.html"
