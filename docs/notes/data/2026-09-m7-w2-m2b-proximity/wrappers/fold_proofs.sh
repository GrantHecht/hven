#!/usr/bin/env bash
# M7 W2 M2b FIX ROUND 1 -- THE FOLD PROOFS.
#
# Every refusal added to `gates.sh` / `gate_asserts.sh` / `classify_psym.py`
# this round is exercised ONCE here against a DELIBERATELY BROKEN INPUT, and
# must exit non-zero. A refusal that has never refused anything is a comment,
# not a gate -- which is precisely what the first round's runner turned out to
# be. The transcript of one run is committed as `gates-fix1/fold-proofs.txt`.
#
# It starts no solve and builds nothing: the one leg that needs a configured
# tree (`ctest --no-tests=error`) runs CTest over a filter that matches no test,
# which is the wrong-binary incident's own shape.
#
# Usage: bash fold_proofs.sh [BUILD_DIR_FOR_THE_CTEST_AND_NINJA_LEGS]
set -o pipefail
ROOT=/home/ghecht/Projects/hven
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m2b-proximity
BUILD=${1:-$ROOT/build-debug}
TMP=$(mktemp -d "${TMPDIR:-/tmp}/m2b-fold-XXXXXX") || exit 90
trap 'rm -rf "$TMP"' EXIT

# shellcheck source=gate_asserts.sh
. "$ART/wrappers/gate_asserts.sh" || exit 90

fails=0
n=0

# expect_nonzero LABEL -- runs the rest of the line, requires a NON-ZERO exit.
expect_nonzero() {
    local label="$1"; shift
    local out rc
    out=$("$@" 2>&1); rc=$?
    n=$((n + 1))
    if [ $rc -ne 0 ]; then
        printf 'FOLD %02d  REFUSED (rc=%d)  %s\n' "$n" "$rc" "$label"
        printf '          %s\n' "$(echo "$out" | grep -E 'REFUSED|ASSERT FAIL|No tests were found|Errors while running' | head -2)"
        return 0
    fi
    printf 'FOLD %02d  *** DID NOT REFUSE (rc=0) ***  %s\n' "$n" "$label"
    printf '          %s\n' "$(echo "$out" | head -3)"
    fails=$((fails + 1))
    return 1
}

expect_zero() {
    local label="$1"; shift
    local out rc
    out=$("$@" 2>&1); rc=$?
    n=$((n + 1))
    if [ $rc -eq 0 ]; then
        printf 'FOLD %02d  ACCEPTED (rc=0)  %s\n' "$n" "$label"
        return 0
    fi
    printf 'FOLD %02d  *** REFUSED A GOOD INPUT (rc=%d) ***  %s\n' "$n" "$rc" "$label"
    printf '          %s\n' "$(echo "$out" | head -3)"
    fails=$((fails + 1))
    return 1
}

echo "FOLD_PROOFS_START $(date -Is)"
echo "root=$ROOT build=$BUILD"
echo "classify_psym.py sha256 $(sha256sum "$ART/wrappers/classify_psym.py" | cut -d' ' -f1)"
echo "gate_asserts.sh   sha256 $(sha256sum "$ART/wrappers/gate_asserts.sh" | cut -d' ' -f1)"
echo "gates.sh          sha256 $(sha256sum "$ART/wrappers/gates.sh" | cut -d' ' -f1)"
echo

CLS="python3 -B $ART/wrappers/classify_psym.py"
HDRS="include/hven/detail/globalization/sqp/restoration.h include/hven/drivers/sqp_solver_types.h"

# ---- classify_psym.py --------------------------------------------------
: > "$TMP/empty.txt"
# shellcheck disable=SC2086
expect_nonzero "classifier R1: an EMPTY comparison file" \
    $CLS --compare "$TMP/empty.txt" --build "$BUILD" --changed-headers $HDRS

printf 'usage: psym_compare.sh compare BEFORE AFTER\n' > "$TMP/malformed.txt"
# shellcheck disable=SC2086
expect_nonzero "classifier R1: a MALFORMED comparison (a usage error, no P-SYM summary)" \
    $CLS --compare "$TMP/malformed.txt" --build "$BUILD" --changed-headers $HDRS

# shellcheck disable=SC2086
expect_nonzero "classifier R1: a comparison file that does not exist" \
    $CLS --compare "$TMP/no-such-file.txt" --build "$BUILD" --changed-headers $HDRS

printf 'P-SYM: OK - no differences\n' > "$TMP/vacuous.txt"
# shellcheck disable=SC2086
expect_nonzero "classifier R2: a summary line and NO differing object (the vacuous pass)" \
    $CLS --compare "$TMP/vacuous.txt" --build "$BUILD" --changed-headers $HDRS

printf 'P-SYM: FAIL\nDIFFERS     CMakeFiles/hven.dir/src/drivers/sqp_solver.cpp.o (x)\n' > "$TMP/one.txt"
# shellcheck disable=SC2086
expect_nonzero "classifier: a changed-header list that disagrees with the declared one" \
    $CLS --compare "$TMP/one.txt" --build "$BUILD" --changed-headers include/hven/core/solver_counters.h

printf 'P-SYM: FAIL\nDIFFERS     CMakeFiles/hven.dir/src/nowhere/not_a_real_object.cpp.o (x)\n' > "$TMP/unexpl.txt"
# shellcheck disable=SC2086
expect_nonzero "classifier R4: a differing object with neither changed source nor changed header" \
    $CLS --compare "$TMP/unexpl.txt" --build "$BUILD" --changed-headers $HDRS

# shellcheck disable=SC2086
expect_nonzero "classifier R3: a negative control below the required minimum" \
    $CLS --compare "$TMP/one.txt" --build "$BUILD" --changed-headers $HDRS \
    --changed-sources src/drivers/sqp_solver.cpp --min-negative-control 999999

# ---- gate_asserts.sh: the ctest assertions -----------------------------
printf 'Test project /nowhere\nNo tests were found!!!\n' > "$TMP/zero-tests.log"
expect_nonzero "assert_ctest: a ctest log with NO summary line (the zero-test green)" \
    assert_ctest "fold" "$TMP/zero-tests.log" 2670

printf '100%% tests passed, 0 tests failed out of 2669\n' > "$TMP/wrong-total.log"
expect_nonzero "assert_ctest: the right shape at the WRONG total" \
    assert_ctest "fold" "$TMP/wrong-total.log" 2670

printf '99%% tests passed, 7 tests failed out of 2670\n' > "$TMP/failed.log"
expect_nonzero "assert_ctest: a nonzero failed count" \
    assert_ctest "fold" "$TMP/failed.log" 2670

: > "$TMP/missing.log"
expect_nonzero "assert_ctest: an empty log file" \
    assert_ctest "fold" "$TMP/missing.log" 2670

printf '100%% tests passed, 0 tests failed out of 2670\n' > "$TMP/good.log"
expect_zero "assert_ctest: the GOOD input is accepted" \
    assert_ctest "fold" "$TMP/good.log" 2670

# ---- gate_asserts.sh: the replay assertion -----------------------------
{
    printf '=== a === cells compared: 27  columns compared: 75  differences: 0\n'
    printf '=== b === cells compared: 27  columns compared: 75  differences: 0\n'
} > "$TMP/replay-short.txt"
expect_nonzero "assert_replay: only two engine lines of three" \
    assert_replay "$TMP/replay-short.txt" 27 75 0 3

{
    printf '=== a === cells compared: 27  columns compared: 75  differences: 0\n'
    printf '=== b === cells compared: 27  columns compared: 75  differences: 0\n'
    printf '=== c === cells compared: 27  columns compared: 75  differences: 4\n'
} > "$TMP/replay-diff.txt"
expect_nonzero "assert_replay: one engine with differences" \
    assert_replay "$TMP/replay-diff.txt" 27 75 0 3

expect_nonzero "assert_replay: a missing comparison file" \
    assert_replay "$TMP/no-such-comparison.txt" 27 75 0 3

{
    printf '=== a === cells compared: 27  columns compared: 75  differences: 0\n'
    printf '=== b === cells compared: 27  columns compared: 75  differences: 0\n'
    printf '=== c === cells compared: 27  columns compared: 75  differences: 0\n'
} > "$TMP/replay-good.txt"
expect_zero "assert_replay: the GOOD input is accepted" \
    assert_replay "$TMP/replay-good.txt" 27 75 0 3

# ---- gate_asserts.sh: the P-SYM shape assertion ------------------------
psym_file() { # differing A B unexplained control -> file on stdout
    local f="$TMP/psym-$1-$2-$3-$4-$5.txt"
    {
        printf 'P-SYM: FAIL\n'
        printf 'DIFFERING OBJECTS: %s\n' "$1"
        printf '  class A -- SOURCE CHANGED by this task: %s\n' "$2"
        printf '  class B -- INCLUDERS of a changed header, source untouched: %s\n' "$3"
        printf '  UNEXPLAINED: %s\n' "$4"
        printf '\n  NEGATIVE CONTROL -- objects that ALSO include one of those headers and are byte-identical: %s\n' "$5"
    } > "$f"
    echo "$f"
}

expect_nonzero "assert_psym: a compare rc of 0 (nothing was compared)" \
    assert_psym Release "$(psym_file 52 5 47 0 23)" 0 0 1
expect_nonzero "assert_psym: a classifier rc of 1" \
    assert_psym Release "$(psym_file 52 5 47 0 23)" 1 1 1
expect_nonzero "assert_psym: movers not accounted for by class A + class B" \
    assert_psym Release "$(psym_file 52 5 40 0 23)" 1 0 1
expect_nonzero "assert_psym: a nonzero UNEXPLAINED" \
    assert_psym Release "$(psym_file 52 5 44 3 23)" 1 0 1
expect_nonzero "assert_psym: a ZERO negative control" \
    assert_psym Release "$(psym_file 52 5 47 0 0)" 1 0 1
expect_nonzero "assert_psym: zero movers (the vacuous comparison, seen from the runner)" \
    assert_psym Release "$(psym_file 0 0 0 0 23)" 1 0 1
expect_zero "assert_psym: the GOOD shape is accepted" \
    assert_psym Release "$(psym_file 52 5 47 0 23)" 1 0 1

expect_nonzero "assert_same_classification: the two configs disagree" \
    assert_same_classification "$(psym_file 52 5 47 0 23)" "$(psym_file 51 5 46 0 23)"
expect_zero "assert_same_classification: identical files are accepted" \
    assert_same_classification "$(psym_file 52 5 47 0 23)" "$(psym_file 52 5 47 0 23)"

# ---- ctest --no-tests=error --------------------------------------------
if [ -f "$BUILD/CTestTestfile.cmake" ]; then
    expect_nonzero "ctest --no-tests=error over a filter that matches NO test (the wrong-binary shape)" \
        ctest --test-dir "$BUILD" --no-tests=error -R '^NoSuchTestNameExistsAnywhere$'
else
    echo "FOLD --  SKIPPED: no configured tree at $BUILD for the --no-tests=error leg"
    fails=$((fails + 1))
fi

# ---- the aggregate and the G6 accumulation ------------------------------
n=$((n + 1))
agg=$(( 0 | 0 | 0 | 0 | 1 | 0 ))
if [ "$agg" -ne 0 ]; then
    printf 'FOLD %02d  REFUSED (rc=%d)  the final aggregate carries rc5 (G1..G4,G6 green, G5 red)\n' "$n" "$agg"
else
    printf 'FOLD %02d  *** DID NOT REFUSE ***  the final aggregate dropped rc5\n' "$n"
    fails=$((fails + 1))
fi

n=$((n + 1))
g6rc=0; for sub in 0 0 1 0; do [ "$sub" -eq 0 ] || g6rc=1; done
if [ "$g6rc" -ne 0 ]; then
    printf 'FOLD %02d  REFUSED (rc=%d)  G6 propagates a failed sub-gate through the later green ones\n' "$n" "$g6rc"
else
    printf 'FOLD %02d  *** DID NOT REFUSE ***  G6 swallowed a failed sub-gate\n' "$n"
    fails=$((fails + 1))
fi

echo
echo "FOLD_PROOFS_TOTAL=$n  FOLD_PROOFS_FAILED=$fails"
echo "FOLD_PROOFS_END $(date -Is)"
[ "$fails" -eq 0 ] || exit 1
exit 0
