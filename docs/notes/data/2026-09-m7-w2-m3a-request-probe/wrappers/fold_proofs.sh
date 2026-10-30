#!/usr/bin/env bash
# M7 W2 M3a TASK A -- THE FOLD PROOFS (M2b fix round 2's file at `252201c7`,
# copied and re-pointed at this task's artifact directory and header list).
#
# Every refusal added to `gates.sh` / `gate_asserts.sh` / `classify_psym.py`
# is exercised ONCE here against a DELIBERATELY BROKEN INPUT, and must exit
# non-zero. A refusal that has never refused anything is a comment, not a gate
# -- which is precisely what the first round's runner turned out to be. The
# transcript of one run is committed as `gates-fix<N>/fold-proofs.txt`.
#
# FIX ROUND 2 closes two gaps the re-review named in this file itself.
#
#   * THE TRANSCRIPT'S RUNNER SHA MUST BE THE LANDED RUNNER'S. Fix round 1's
#     transcript recorded a `gates.sh` sha256 that no committed version has --
#     it was taken before the last edit. This version prints, for each wrapper,
#     BOTH the worktree digest and `git show HEAD:<path> | sha256sum`, so the
#     transcript proves on its own face that it ran against what landed. Run it
#     on a CLEAN tree, after the wrapper commit, and cite that commit.
#   * THE LAST TWO FOLDS MUST RUN THE RUNNER'S CODE. Fix round 1 proved "the
#     aggregate carries rc5" and "G6 propagates a failed sub-gate" by
#     re-creating the arithmetic here, which proves only that this file can add.
#     `gates.sh` now calls `gates_aggregate` and `accumulate_rc` from
#     `gate_asserts.sh`, and the folds below call those same functions.
#
# It starts no solve and builds nothing: the one leg that needs a configured
# tree (`ctest --no-tests=error`) runs CTest over a filter that matches no test,
# which is the wrong-binary incident's own shape.
#
# Usage: bash fold_proofs.sh [BUILD_DIR_FOR_THE_CTEST_AND_NINJA_LEGS]
set -o pipefail
ROOT=/home/ghecht/Projects/hven
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3a-request-probe
BUILD=${1:-$ROOT/build-debug}
TMP=$(mktemp -d "${TMPDIR:-/tmp}/m3a-fold-XXXXXX") || exit 90
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
echo "HEAD $(git -C "$ROOT" rev-parse HEAD)  dirty-files=$(git -C "$ROOT" status --porcelain | wc -l)"
echo "-- wrapper identity: WORKTREE digest vs the digest of the SAME path AT HEAD."
echo "-- They must agree, or this transcript did not exercise what landed."
for f in gates.sh gate_asserts.sh classify_psym.py fold_proofs.sh; do
    rel="docs/notes/data/2026-09-m7-w2-m3a-request-probe/wrappers/$f"
    w=$(sha256sum "$ART/wrappers/$f" | cut -d' ' -f1)
    c=$(git -C "$ROOT" show "HEAD:$rel" 2> /dev/null | sha256sum | cut -d' ' -f1)
    if [ "$w" = "$c" ]; then
        printf '%-18s sha256 %s  (worktree == HEAD)\n' "$f" "$w"
    else
        printf '%-18s sha256 %s  *** DIFFERS FROM HEAD: %s ***\n' "$f" "$w" "$c"
        fails=$((fails + 1))
    fi
done
echo

CLS="python3 -B $ART/wrappers/classify_psym.py"
HDRS="include/hven/core/solver_counters.h"

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
    $CLS --compare "$TMP/one.txt" --build "$BUILD" --changed-headers include/hven/drivers/sqp_solver_types.h

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
    assert_psym Release "$(psym_file 52 5 47 0 23)" 0 0 1 52 5 47
expect_nonzero "assert_psym: a classifier rc of 1" \
    assert_psym Release "$(psym_file 52 5 47 0 23)" 1 1 1 52 5 47
expect_nonzero "assert_psym: movers not accounted for by class A + class B" \
    assert_psym Release "$(psym_file 52 5 40 0 23)" 1 0 1 52 5 40
expect_nonzero "assert_psym: a nonzero UNEXPLAINED" \
    assert_psym Release "$(psym_file 52 5 44 3 23)" 1 0 1 52 5 44
expect_nonzero "assert_psym: a ZERO negative control" \
    assert_psym Release "$(psym_file 52 5 47 0 0)" 1 0 1 52 5 47
expect_nonzero "assert_psym: zero movers (the vacuous comparison, seen from the runner)" \
    assert_psym Release "$(psym_file 0 0 0 0 23)" 1 0 1 0 0 0
# FIX ROUND 2: the DECLARED mover shape. Each of the three counts is wrong in
# turn while the classification stays internally consistent -- the shape the
# first round's floor would have accepted without a word.
expect_nonzero "assert_psym: the right shape at the WRONG declared differing count" \
    assert_psym Release "$(psym_file 52 5 47 0 23)" 1 0 1 51 5 47
expect_nonzero "assert_psym: the declared class A does not match" \
    assert_psym Release "$(psym_file 52 5 47 0 23)" 1 0 1 52 6 47
expect_nonzero "assert_psym: the declared class B does not match (a header reached an includer)" \
    assert_psym Release "$(psym_file 52 5 47 0 23)" 1 0 1 52 5 46
expect_zero "assert_psym: the GOOD shape at the DECLARED counts is accepted" \
    assert_psym Release "$(psym_file 52 5 47 0 23)" 1 0 1 52 5 47

# ---- gate_asserts.sh: the stale/partial P-SYM capture -------------------
mkdir -p "$TMP/cap-good" "$TMP/cap-empty" "$TMP/cap-short"
printf 'a.o\nb.o\nc.o\n' > "$TMP/cap-good/.psym-manifest"
: > "$TMP/cap-empty/.psym-manifest"
printf 'a.o\nb.o\n' > "$TMP/cap-short/.psym-manifest"

expect_nonzero "assert_capture: a capture that EXITED NONZERO (the first round discarded this rc)" \
    assert_capture "after-Release" 1 "$TMP/cap-good"
expect_nonzero "assert_capture: a snapshot directory that does not exist" \
    assert_capture "after-Release" 0 "$TMP/cap-no-such-dir"
expect_nonzero "assert_capture: an EMPTY manifest (the truncate-then-refuse window)" \
    assert_capture "after-Release" 0 "$TMP/cap-empty"
expect_zero "assert_capture: a good capture is accepted" \
    assert_capture "after-Release" 0 "$TMP/cap-good"

expect_nonzero "assert_capture_pair: two arms of DIFFERENT size (one capture was partial)" \
    assert_capture_pair "$TMP/cap-short" "$TMP/cap-good"
expect_nonzero "assert_capture_pair: an arm with no manifest at all" \
    assert_capture_pair "$TMP/cap-no-such-dir" "$TMP/cap-good"
expect_zero "assert_capture_pair: two arms of the same size are accepted" \
    assert_capture_pair "$TMP/cap-good" "$TMP/cap-good"

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
#
# FIX ROUND 2: these call `gates.sh`'s OWN aggregation helpers, not a re-created
# copy of the arithmetic. `gates.sh` ends with `gates_aggregate "$rc1" ... "$rc6"`
# and G6 accumulates with `rc=$(accumulate_rc "$rc" "$sub")`, so a regression in
# either is a regression in the code these folds run.

expect_nonzero "gates_aggregate: the final aggregate carries rc5 (G1..G4,G6 green, G5 red)" \
    gates_aggregate 0 0 0 0 1 0
expect_nonzero "gates_aggregate: a failing G1 alone fails the chain" \
    gates_aggregate 1 0 0 0 0 0
expect_nonzero "gates_aggregate: an UNSET return code is a refusal, not a zero" \
    gates_aggregate 0 0 0 0 "" 0
expect_zero "gates_aggregate: six green segments aggregate to 0" \
    gates_aggregate 0 0 0 0 0 0

# THE SEQUENCING IS THIS FILE'S; THE ARITHMETIC IS THE RUNNER'S. `accumulate_rc`
# is called exactly as G6 calls it, once per sub-gate, in order.
fold_g6_accumulation() {
    local rc=0 sub
    for sub in "$@"; do
        rc=$(accumulate_rc "$rc" "$sub")
    done
    echo "G6 accumulation over [$*] -> $rc"
    return "$rc"
}
expect_nonzero "accumulate_rc: G6 carries a failed sub-gate through the later green ones" \
    fold_g6_accumulation 0 0 1 0
expect_nonzero "accumulate_rc: a failure in the LAST sub-gate is not lost either" \
    fold_g6_accumulation 0 0 0 1
expect_zero "accumulate_rc: an all-green G6 accumulates to 0" \
    fold_g6_accumulation 0 0 0 0

echo
echo "FOLD_PROOFS_TOTAL=$n  FOLD_PROOFS_FAILED=$fails"
echo "FOLD_PROOFS_END $(date -Is)"
[ "$fails" -eq 0 ] || exit 1
exit 0
