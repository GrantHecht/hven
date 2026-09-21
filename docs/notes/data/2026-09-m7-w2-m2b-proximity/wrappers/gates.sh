#!/usr/bin/env bash
# M7 W2 M2b -- THE GATES, in SIX lock segments so the harness lane's own cells
# can interleave between them.
#
#   G1  Debug: fresh configure, build, suite WITHOUT the >600 s test
#   G2  the >600 s test ALONE (Continuation.ProbeBudgetBoundsAFailingProposal)
#   G3  Release: fresh configure (the bench binaries carry a configure-time
#       git-describe, so a stamped leg re-configures), build, suite
#   G4  the U0 27-cell three-arm replay against the M6 close pin
#   G5  P-SYM, Release and Debug, classified against $BEFORE with THIS round's
#       own changed-header list
#   G6  install smoke, the export contract (three arguments), clang-format,
#       git diff --check
#
# Exit 1 from P-SYM is EXPECTED -- one public field on SqpOptions moves sizeof
# 400 -> 416 in every TU that lays the struct out -- and the gate is the
# CLASSIFICATION, which must report 0 unexplained.
#
# =========================================================================
# FIX ROUND 1 (the review's "the gate wrapper itself is unsafe"). The first
# round's runner could finish green over a suite that ran NO tests, over a
# P-SYM comparison that named no object, over a classification that differed
# between configs, and over a failed G6 sub-gate -- and `rc5` never entered its
# aggregate at all. This version ASSERTS instead of ECHOING:
#
#   * every ctest leg runs with `--no-tests=error` AND has its total and its
#     failed count checked against an expected number declared at the top of
#     this file;
#   * the replay's three engine lines are each checked for the pinned
#     cells/columns/differences shape;
#   * P-SYM asserts compare rc == 1, classify rc == 0, movers >= 1,
#     class A + class B == movers, UNEXPLAINED == 0, negative control >= 1, and
#     that the two configs' classifications are byte-identical (a mismatch
#     FAILS; the first round accepted it with an `echo`);
#   * the classifier itself refuses an empty or malformed comparison
#     (classify_psym.py's R1/R2/R3);
#   * every G6 sub-gate's rc is accumulated;
#   * the final aggregate includes rc5.
#
# The refusals live in `gate_asserts.sh` so that this runner and the fold
# proofs exercise the same code; each one is FOLD-PROVEN in
# `gates-fix1/fold-proofs.txt`.
#
# FIX ROUND 2 (the re-review's remaining Important 2). Three holes were left:
#
#   * G5's two `psym_compare.sh capture` calls IGNORED their return codes while
#     `$OUT` was merely created, not cleared -- and `capture` truncates its
#     manifest before it may refuse an incomplete object universe, so two
#     similarly PARTIAL captures could reach compare and agree. The snapshot
#     tree is now DELETED before each capture, both return codes are asserted
#     (`assert_capture`), and the two arms must enumerate the same number of
#     objects (`assert_capture_pair`).
#   * `assert_psym` required only "at least one mover" plus A + B consistency.
#     The expected MOVER SHAPE is now declared below -- derived from
#     `git diff --name-only $BEFORE` before this chain was detached -- and
#     asserted for equality.
#   * the final aggregate and G6's sub-gate accumulation were arithmetic written
#     inline here, so the folds that proved them re-created that arithmetic
#     rather than running it. Both are now `gate_asserts.sh` functions
#     (`gates_aggregate`, `accumulate_rc`) that this runner CALLS and the folds
#     exercise directly.
#
# This round's transcripts go to `gates-fix2/`; `gates/` and `gates-fix1/` are
# landed artifacts and are never edited.
# =========================================================================
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m2bfix2
G=$S/gates
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m2b-proximity
CLASSIFIER=$ART/wrappers/classify_psym.py
ASSERTS=$ART/wrappers/gate_asserts.sh

# ---- THIS ROUND'S EXPECTED COUNTS -------------------------------------------
# Read off this round's own configured build trees before the chain was
# detached, NOT copied from the previous round: fix round 1 added three
# fixtures and fix round 2 adds three more, so a runner that kept an earlier
# round's numbers would fail green-for-the-wrong-reason.
#
# DERIVED AND CROSS-CHECKED. Fix round 1's run totals were 2672 (Debug, with the
# long test excluded) and 2673 (Release, everything) -- `ctest -N` reported
# 2674 / 2675 there, two higher, because a CTest summary line counts the SKIPPED
# test but not the two DISABLED ones, and `gates-fix1/gates.txt` carries that
# derivation in full. This round adds exactly THREE TEST() blocks, all in
# tests/sqp/test_sqp_restoration.cpp (TheZeroWeightGateNeverReadsTheBundle,
# TheZeroWeightCertifiesOnAScaledSolve, and
# TheProductionCentreIsTheRequestingIterateNotTheSeed), so both numbers are +3 --
# and the runner CHECKS that rather than trusting it.
BEFORE=${BEFORE:-37dcc9fa}
EXPECT_DEBUG_TESTS=${EXPECT_DEBUG_TESTS:-2675}
EXPECT_RELEASE_TESTS=${EXPECT_RELEASE_TESTS:-2676}
EXPECT_LONG_TESTS=1
EXPECT_REPLAY_CELLS=27
EXPECT_REPLAY_COLUMNS=75
EXPECT_REPLAY_DIFFS=0
EXPECT_REPLAY_ENGINES=3
MIN_NEGATIVE_CONTROL=1

# ---- THIS ROUND'S EXPECTED P-SYM MOVER SHAPE --------------------------------
# DECLARED BEFORE THE RUN, derived from `git diff --name-only 37dcc9fa`:
#
#   src/drivers/sqp_solver.cpp                   -> hven.dir/.../sqp_solver.cpp.o
#   src/globalization/sqp/soc_elastic_restoration.cpp
#                                                -> hven.dir/.../soc_elastic_restoration.cpp.o
#   tests/sqp/test_sqp_restoration.cpp           -> hven_sqp_tests.dir/.../test_sqp_restoration.cpp.o
#
# three objects whose SOURCE changed, i.e. class A = 3. The two changed headers
# (detail/globalization/sqp/restoration.h, drivers/sqp_solver_types.h) carry
# comment corrections plus, in restoration.h, two free functions appended after
# the class -- one declaration and one template that no other TU instantiates.
# Neither emits code or debug info in a TU that does not USE it, and the three
# TUs that do are exactly the class-A three, so class B = 0. That is the shape
# fix round 1 measured against a LARGER edit of the same two headers (3 = 3 + 0),
# and it is an ASSERTION here rather than an expectation: if a header edit did
# reach an includer, this gate fails and the shape is re-derived and declared,
# never discovered.
EXPECT_PSYM_DIFFERING=${EXPECT_PSYM_DIFFERING:-3}
EXPECT_PSYM_CLASS_A=${EXPECT_PSYM_CLASS_A:-3}
EXPECT_PSYM_CLASS_B=${EXPECT_PSYM_CLASS_B:-0}

mkdir -p "$G" "$S/logs"
LOG=$S/logs/gates.log
exec >"$LOG" 2>&1
echo "GATES_START $(date -Is)"
cd "$ROOT" || exit 90
echo "=== HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l) ==="
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: the tree is not clean"; exit 2; }
echo "=== BEFORE=$BEFORE  EXPECT_DEBUG_TESTS=$EXPECT_DEBUG_TESTS  EXPECT_RELEASE_TESTS=$EXPECT_RELEASE_TESTS ==="

# THE ROUND'S CHANGED SURFACE, DERIVED FROM GIT rather than typed: the header
# list is what the classifier checks its own declared list against, and the
# source list is what class A is keyed on.
CHANGED_HEADERS=$(git diff --name-only "$BEFORE" -- include/ | tr '\n' ' ')
CHANGED_SOURCES=$(git diff --name-only "$BEFORE" -- src/ tests/ | tr '\n' ' ')
echo "=== CHANGED HEADERS: $CHANGED_HEADERS"
echo "=== CHANGED SOURCES: $CHANGED_SOURCES"
[ -n "$CHANGED_HEADERS" ] || { echo "REFUSED: no changed header under include/ against $BEFORE"; exit 2; }
[ -n "$CHANGED_SOURCES" ] || { echo "REFUSED: no changed source against $BEFORE"; exit 2; }

# shellcheck source=gate_asserts.sh
. "$ASSERTS" || exit 90

run_locked() { # label, then the command
  local label="$1"; shift
  flock /tmp/box-build.lock bash -c '
    set -o pipefail
    label="$1"; shift
    echo "FOREGROUND_START ${label} $(date -Is)"
    foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
    [ -z "$foreign" ] && echo "PGREP_EMPTY ${label}: no foreign cell binary under my lock" \
                      || echo "NOTE: a foreign cell binary is alive under my lock: $foreign"
    "$@"
    rc=$?
    echo "FOREGROUND_END ${label} $(date -Is) rc=${rc}"
    exit ${rc}
  ' _ "$label" "$@"
  local rc=$?
  echo "WRAPPER_RC ${label} rc=${rc}"
  return $rc
}

# --- G1: Debug -------------------------------------------------------------
cat > "$S/g1.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=$ROOT/.scratch/m7w2m2bfix2; G=$S/gates
rm -rf "$S/gates-dbg"
cmake -S "$ROOT" -B "$S/gates-dbg" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON > "$G/configure-debug.log" 2>&1 || exit 91
cmake --build "$S/gates-dbg" -j "$(nproc)" > "$G/build-debug.log" 2>&1 || exit 91
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest --test-dir "$S/gates-dbg" -j4 --no-tests=error \
  -E "Continuation.ProbeBudgetBoundsAFailingProposal" --output-on-failure > "$G/suite-debug.log" 2>&1
rc=$?; tail -6 "$G/suite-debug.log"; exit $rc
INNER
run_locked G1-debug bash "$S/g1.sh"; rc1=$?
assert_ctest "G1 Debug suite" "$G/suite-debug.log" "$EXPECT_DEBUG_TESTS" || rc1=1

# --- G2: the long test, ALONE ---------------------------------------------
cat > "$S/g2.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=$ROOT/.scratch/m7w2m2bfix2; G=$S/gates
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest --test-dir "$S/gates-dbg" --no-tests=error \
  -R "^Continuation.ProbeBudgetBoundsAFailingProposal$" --output-on-failure > "$G/suite-debug-long.log" 2>&1
rc=$?; tail -8 "$G/suite-debug-long.log"; exit $rc
INNER
run_locked G2-debug-long bash "$S/g2.sh"; rc2=$?
assert_ctest "G2 long test" "$G/suite-debug-long.log" "$EXPECT_LONG_TESTS" || rc2=1

# --- G3: Release ----------------------------------------------------------
cat > "$S/g3.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=$ROOT/.scratch/m7w2m2bfix2; G=$S/gates
rm -rf "$S/gates-rel"
cmake -S "$ROOT" -B "$S/gates-rel" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON \
  > "$G/configure-release.log" 2>&1 || exit 91
cmake --build "$S/gates-rel" -j "$(nproc)" > "$G/build-release.log" 2>&1 || exit 91
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest --test-dir "$S/gates-rel" -j4 --no-tests=error \
  --output-on-failure > "$G/suite-release.log" 2>&1
rc=$?; tail -6 "$G/suite-release.log"
exit $rc
INNER
run_locked G3-release bash "$S/g3.sh"; rc3=$?
assert_ctest "G3 Release suite" "$G/suite-release.log" "$EXPECT_RELEASE_TESTS" || rc3=1

# --- G4: the replay -------------------------------------------------------
cat > "$S/g4.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=$ROOT/.scratch/m7w2m2bfix2; G=$S/gates
BIN=$S/gates-rel/bench/hven_sqp_corpus
D=$G/replay; mkdir -p "$D"; : > "$D/comparison.txt"
CELLS="$(grep -v '^#' "$ROOT/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt" | tr -d '\n')"
worst=0
for eng in walk ssn ipm; do
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
    "$BIN" --engine "$eng" --cells "$CELLS" --csv "$D/head-$eng.csv" > "$D/run-$eng.log" 2>&1
  rc=$?; echo "ran head-$eng rc=$rc"; [ $rc -eq 0 ] || { worst=1; break; }
done
[ $worst -eq 0 ] || exit 1
BASE=$ROOT/docs/notes/data/2026-09-m6-close-gate/replay
for eng in walk ssn ipm; do
  python3 -B "$ROOT/scripts/compare_replay.py" "$BASE/head-$eng.csv" "$D/head-$eng.csv" \
    "M6 CLOSE HEAD-$eng vs M7 W2 M2b fix1 HEAD-$eng" | tee -a "$D/comparison.txt"
  rc=${PIPESTATUS[0]}; echo "  CMP_RC=$rc"; [ $rc -eq 0 ] || worst=1
done
exit $worst
INNER
run_locked G4-replay bash "$S/g4.sh"; rc4=$?
assert_replay "$G/replay/comparison.txt" "$EXPECT_REPLAY_CELLS" "$EXPECT_REPLAY_COLUMNS" \
  "$EXPECT_REPLAY_DIFFS" "$EXPECT_REPLAY_ENGINES" || rc4=1

# --- G5: P-SYM, both configs ---------------------------------------------
cat > "$S/g5.sh" <<'INNER'
set -uo pipefail
ROOT=/home/ghecht/Projects/hven; S=$ROOT/.scratch/m7w2m2bfix2; G=$S/gates
BD=$S/psym-build; OUT=$G/psym
BEFORE="$1"; CHANGED_SOURCES="$2"; CHANGED_HEADERS="$3"; MIN_CONTROL="$4"
WANT_DIFFERING="$5"; WANT_A="$6"; WANT_B="$7"
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m2b-proximity
# CLEARED, NOT MERELY CREATED (fix round 2). `psym_compare.sh capture` truncates
# its manifest before it may refuse an incomplete object universe, so a snapshot
# directory that survives from an earlier attempt is a live route to a narrowed
# comparison. Nothing under $OUT outlives this segment.
rm -rf "$OUT"
mkdir -p "$OUT"
HEAD_SHA=$(git -C "$ROOT" rev-parse HEAD)
BRANCH=$(git -C "$ROOT" rev-parse --abbrev-ref HEAD)
[ -z "$(git -C "$ROOT" status --porcelain)" ] || { echo "REFUSED: tree not clean"; exit 2; }
# THE CLASSIFIER AND THE ASSERTS ARE COPIED OUT OF THE TREE FIRST: the `before`
# arm below checks out $BEFORE, at which this round's wrappers do not exist.
cp "$ART/wrappers/classify_psym.py" "$S/classify_psym.py" || exit 93
cp "$ART/wrappers/gate_asserts.sh" "$S/gate_asserts.sh" || exit 93
. "$S/gate_asserts.sh" || exit 93
rc=0
for CFG in Release Debug; do
  echo "=== P-SYM $CFG  after=$HEAD_SHA  before=$BEFORE"
  rm -rf "$BD"
  cmake -S "$ROOT" -B "$BD" -G Ninja -DCMAKE_BUILD_TYPE="$CFG" \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON \
    > "$OUT/configure-$CFG.log" 2>&1 || exit 91
  CCACHE_DISABLE=1 cmake --build "$BD" -j "$(nproc)" > "$OUT/build-after-$CFG.log" 2>&1 || exit 91
  rm -rf "$OUT/after-$CFG"
  "$ROOT/scripts/psym_compare.sh" capture "$OUT/after-$CFG" "$BD" > "$OUT/capture-after-$CFG.log" 2>&1
  cap_after_rc=$?
  restore() { git -C "$ROOT" checkout -q "$BRANCH" 2>/dev/null || git -C "$ROOT" checkout -q "$HEAD_SHA"; }
  trap restore EXIT
  git -C "$ROOT" checkout -q --detach "$BEFORE" || exit 92
  CCACHE_DISABLE=1 cmake --build "$BD" -j "$(nproc)" > "$OUT/build-before-$CFG.log" 2>&1 || { restore; exit 91; }
  rm -rf "$OUT/before-$CFG"
  "$ROOT/scripts/psym_compare.sh" capture "$OUT/before-$CFG" "$BD" > "$OUT/capture-before-$CFG.log" 2>&1
  cap_before_rc=$?
  git -C "$ROOT" checkout -q "$BRANCH"
  trap - EXIT
  # BOTH CAPTURE RETURN CODES ENTER THE GATE (fix round 2): the first round
  # discarded them, so a capture that refused an incomplete object universe was
  # invisible and its short manifest went on to `compare`.
  assert_capture "after-$CFG" "$cap_after_rc" "$OUT/after-$CFG" || rc=1
  assert_capture "before-$CFG" "$cap_before_rc" "$OUT/before-$CFG" || rc=1
  assert_capture_pair "$OUT/before-$CFG" "$OUT/after-$CFG" || rc=1
  echo "tree restored to $(git -C "$ROOT" rev-parse --abbrev-ref HEAD) $(git -C "$ROOT" rev-parse --short HEAD)"
  "$ROOT/scripts/psym_compare.sh" compare "$OUT/before-$CFG" "$OUT/after-$CFG" > "$OUT/compare-$CFG.txt" 2>&1
  cmp_rc=$?
  echo "PSYM_COMPARE_RC_$CFG=$cmp_rc (1 is EXPECTED; the gate is the classification)"
  # shellcheck disable=SC2086
  python3 -B "$S/classify_psym.py" --compare "$OUT/compare-$CFG.txt" --build "$BD" \
    --changed-sources $CHANGED_SOURCES --changed-headers $CHANGED_HEADERS \
    --min-negative-control "$MIN_CONTROL" > "$OUT/classification-$CFG.txt" 2>&1
  cls_rc=$?
  echo "CLASSIFY_RC_$CFG=$cls_rc"
  grep -E "^(P-SYM|DIFFERING|  class|  UNEXPLAINED|  NEGATIVE|REFUSED)" "$OUT/classification-$CFG.txt"
  assert_psym "$CFG" "$OUT/classification-$CFG.txt" "$cmp_rc" "$cls_rc" "$MIN_CONTROL" \
    "$WANT_DIFFERING" "$WANT_A" "$WANT_B" || rc=1
done
assert_same_classification "$OUT/classification-Release.txt" "$OUT/classification-Debug.txt" || rc=1
echo "G5_ASSERT_RC=$rc"
exit $rc
INNER
run_locked G5-psym bash "$S/g5.sh" "$BEFORE" "$CHANGED_SOURCES" "$CHANGED_HEADERS" \
  "$MIN_NEGATIVE_CONTROL" "$EXPECT_PSYM_DIFFERING" "$EXPECT_PSYM_CLASS_A" \
  "$EXPECT_PSYM_CLASS_B"; rc5=$?

# --- G6: smoke, export contract, format, whitespace ------------------------
cat > "$S/g6.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=$ROOT/.scratch/m7w2m2bfix2; G=$S/gates
CHANGED="$1"; ASSERTS="$2"
cd "$ROOT" || exit 90
# THE ACCUMULATION IS THE RUNNER'S OWN FUNCTION (fix round 2), so the fold that
# proves "a failed sub-gate survives the green ones after it" runs THIS code.
# shellcheck source=gate_asserts.sh
. "$ASSERTS" || exit 90
rc=0
# THE SMOKE'S WORKDIR IS KEPT so the export contract can read the SAME build's
# installed hvenTargets.cmake and the SAME build's configure log -- the script's
# own documented contract for the third argument (never a literal picked ahead
# of time).
rm -rf "$S/smoke-work"
HVEN_INSTALL_SMOKE_WORKDIR="$S/smoke-work" bash scripts/check_install_smoke.sh > "$G/install-smoke.log" 2>&1
sub=$?; echo "INSTALL_SMOKE_RC=$sub"; tail -4 "$G/install-smoke.log"; rc=$(accumulate_rc "$rc" "$sub")
ALIGN=$(grep -o "EIGEN_MAX_ALIGN_BYTES resolved to [0-9]*" "$S/smoke-work/hven-configure.log" | tail -1 | awk '{print $NF}')
echo "ALIGN_FROM_THIS_BUILDS_CONFIGURE_LOG=$ALIGN"
[ -n "$ALIGN" ] || { echo "REFUSED: no EIGEN_MAX_ALIGN_BYTES in this build's configure log"; rc=1; }
TARGETS="$S/smoke-work/hven-install/lib64/cmake/hven/hvenTargets.cmake"
ls -l "$TARGETS"; sub=$?; rc=$(accumulate_rc "$rc" "$sub")
bash scripts/check_export_contract.sh "$TARGETS" linux "$ALIGN" > "$G/export-contract.log" 2>&1
sub=$?; echo "EXPORT_CONTRACT_RC=$sub"; tail -3 "$G/export-contract.log"; rc=$(accumulate_rc "$rc" "$sub")
echo "--- clang-format on this round's changed C++ files, derived from git"
# shellcheck disable=SC2086
clang-format --dry-run -Werror $CHANGED
sub=$?; echo "CLANG_FORMAT_RC=$sub"; rc=$(accumulate_rc "$rc" "$sub")
git diff --check
sub=$?; echo "DIFF_CHECK_RC=$sub"; rc=$(accumulate_rc "$rc" "$sub")
dirty=$(git status --porcelain | wc -l)
echo "DIRTY_FILES=$dirty"
[ "$dirty" -eq 0 ] || rc=1
echo "G6_ASSERT_RC=$rc"
exit "$rc"
INNER
CHANGED_CXX=$(git diff --name-only "$BEFORE" -- 'include/*.h' 'src/*.cpp' 'tests/*.cpp' | tr '\n' ' ')
echo "=== CHANGED C++ FOR CLANG-FORMAT: $CHANGED_CXX"
run_locked G6-smoke bash "$S/g6.sh" "$CHANGED_CXX" "$ASSERTS"; rc6=$?

echo "GATES_SUMMARY G1=$rc1 G2=$rc2 G3=$rc3 G4=$rc4 G5=$rc5 G6=$rc6"
echo "GATES_END $(date -Is)"
# rc5 IS IN THE AGGREGATE. The first round's final line omitted it, so a P-SYM
# segment that failed outright could not fail the chain. Since fix round 2 the
# aggregation itself is `gate_asserts.sh`'s, so the fold that proves it runs
# this exact code rather than a copy of it -- and a return code that is not an
# integer (an unset variable, which used to read as 0) is its own refusal.
gates_aggregate "$rc1" "$rc2" "$rc3" "$rc4" "$rc5" "$rc6"
exit $?
