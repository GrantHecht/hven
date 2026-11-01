#!/usr/bin/env bash
# M7 W2 M2 -- THE GATES at the S5 commit, in SIX lock segments so the harness
# lane's 1800 s tier can interleave between them.
#
#   G1  Debug: fresh configure, build, suite WITHOUT the >600 s test
#   G2  the >600 s test ALONE (Continuation.ProbeBudgetBoundsAFailingProposal)
#   G3  Release: fresh configure (the bench binaries carry a configure-time
#       git-describe, so a stamped leg re-configures), build, suite
#   G4  the U0 27-cell three-arm replay against the M6 close pin
#   G5  P-SYM, Release and Debug, classified against aeebdccc with THIS task's
#       own changed-header list
#   G6  install smoke (both front ends), export contract, clang-format,
#       git diff --check
#
# Exit 1 from P-SYM is EXPECTED -- five public fields on SqpCounters move
# sizeof 808 -> 848 in every TU that lays the struct out -- and the gate is the
# CLASSIFICATION, which must report 0 unexplained.
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m2
G=$S/gates
BEFORE=aeebdccc
mkdir -p "$G" "$S/logs"
LOG=$S/logs/gates.log
exec >"$LOG" 2>&1
echo "GATES_START $(date -Is)"
cd "$ROOT" || exit 90
echo "=== HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l) ==="
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: the tree is not clean"; exit 2; }

run_locked() { # label, then the command
  local label="$1"; shift
  flock /tmp/box-build.lock bash -c '
    set -o pipefail
    label="$1"; shift
    echo "FOREGROUND_START ${label} $(date -Is)"
    foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
    [ -z "$foreign" ] || { echo "NOTE: a foreign cell binary is alive under my lock: $foreign"; }
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
ROOT=/home/ghecht/Projects/hven; S=$ROOT/.scratch/m7w2m2; G=$S/gates
rm -rf "$S/gates-dbg"
cmake -S "$ROOT" -B "$S/gates-dbg" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON > "$G/configure-debug.log" 2>&1 || exit 91
cmake --build "$S/gates-dbg" -j "$(nproc)" > "$G/build-debug.log" 2>&1 || exit 91
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest --test-dir "$S/gates-dbg" -j4 \
  -E "Continuation.ProbeBudgetBoundsAFailingProposal" --output-on-failure > "$G/suite-debug.log" 2>&1
rc=$?; tail -6 "$G/suite-debug.log"; exit $rc
INNER
run_locked G1-debug bash "$S/g1.sh"; rc1=$?

# --- G2: the long test, ALONE ---------------------------------------------
cat > "$S/g2.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=$ROOT/.scratch/m7w2m2; G=$S/gates
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest --test-dir "$S/gates-dbg" \
  -R "^Continuation.ProbeBudgetBoundsAFailingProposal$" --output-on-failure > "$G/suite-debug-long.log" 2>&1
rc=$?; tail -8 "$G/suite-debug-long.log"; exit $rc
INNER
run_locked G2-debug-long bash "$S/g2.sh"; rc2=$?

# --- G3: Release ----------------------------------------------------------
cat > "$S/g3.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=$ROOT/.scratch/m7w2m2; G=$S/gates
rm -rf "$S/gates-rel"
cmake -S "$ROOT" -B "$S/gates-rel" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON \
  > "$G/configure-release.log" 2>&1 || exit 91
cmake --build "$S/gates-rel" -j "$(nproc)" > "$G/build-release.log" 2>&1 || exit 91
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest --test-dir "$S/gates-rel" -j4 --output-on-failure \
  > "$G/suite-release.log" 2>&1
rc=$?; tail -6 "$G/suite-release.log"
sha256sum "$S/gates-rel/libhven.a" "$S/gates-rel/bench/hven_sqp_corpus" 2>/dev/null
exit $rc
INNER
run_locked G3-release bash "$S/g3.sh"; rc3=$?

# --- G4: the replay -------------------------------------------------------
cat > "$S/g4.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=$ROOT/.scratch/m7w2m2; G=$S/gates
BIN=$S/gates-rel/bench/hven_sqp_corpus
D=$G/replay; mkdir -p "$D"
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
    "M6 CLOSE HEAD-$eng vs M7 W2 M2 HEAD-$eng" | tee -a "$D/comparison.txt"
  rc=${PIPESTATUS[0]}; echo "  CMP_RC=$rc"; [ $rc -eq 0 ] || worst=1
done
exit $worst
INNER
run_locked G4-replay bash "$S/g4.sh"; rc4=$?

# --- G5: P-SYM, both configs ---------------------------------------------
cat > "$S/g5.sh" <<'INNER'
set -uo pipefail
ROOT=/home/ghecht/Projects/hven; S=$ROOT/.scratch/m7w2m2; G=$S/gates
BD=$S/psym-build; OUT=$G/psym; BEFORE=aeebdccc
mkdir -p "$OUT"
HEAD_SHA=$(git -C "$ROOT" rev-parse HEAD)
BRANCH=$(git -C "$ROOT" rev-parse --abbrev-ref HEAD)
[ -z "$(git -C "$ROOT" status --porcelain)" ] || { echo "REFUSED: tree not clean"; exit 2; }
CHANGED="src/drivers/sqp_solver.cpp tests/drivers/test_callback.cpp tests/drivers/test_threads.cpp tests/sqp/test_trace_writer.cpp tests/sqp/test_sqp_restoration.cpp"
for CFG in Release Debug; do
  echo "=== P-SYM $CFG  after=$HEAD_SHA  before=$BEFORE"
  rm -rf "$BD"
  cmake -S "$ROOT" -B "$BD" -G Ninja -DCMAKE_BUILD_TYPE="$CFG" \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON \
    > "$OUT/configure-$CFG.log" 2>&1 || exit 91
  CCACHE_DISABLE=1 cmake --build "$BD" -j "$(nproc)" > "$OUT/build-after-$CFG.log" 2>&1 || exit 91
  "$ROOT/scripts/psym_compare.sh" capture "$OUT/after-$CFG" "$BD" > "$OUT/capture-after-$CFG.log" 2>&1
  restore() { git -C "$ROOT" checkout -q "$BRANCH" 2>/dev/null || git -C "$ROOT" checkout -q "$HEAD_SHA"; }
  trap restore EXIT
  git -C "$ROOT" checkout -q --detach "$BEFORE" || exit 92
  CCACHE_DISABLE=1 cmake --build "$BD" -j "$(nproc)" > "$OUT/build-before-$CFG.log" 2>&1 || { restore; exit 91; }
  "$ROOT/scripts/psym_compare.sh" capture "$OUT/before-$CFG" "$BD" > "$OUT/capture-before-$CFG.log" 2>&1
  git -C "$ROOT" checkout -q "$BRANCH"
  trap - EXIT
  echo "tree restored to $(git -C "$ROOT" rev-parse --abbrev-ref HEAD) $(git -C "$ROOT" rev-parse --short HEAD)"
  "$ROOT/scripts/psym_compare.sh" compare "$OUT/before-$CFG" "$OUT/after-$CFG" > "$OUT/compare-$CFG.txt" 2>&1
  echo "PSYM_COMPARE_RC=$? (1 is EXPECTED; the gate is the classification)"
  python3 -B "$S/classify_psym.py" --compare "$OUT/compare-$CFG.txt" --build "$BD" \
    --changed-sources $CHANGED > "$OUT/classification-$CFG.txt" 2>&1
  echo "CLASSIFY_RC_$CFG=$?"
  grep -E "^(P-SYM|DIFFERING|  class|  UNEXPLAINED|  NEGATIVE)" "$OUT/classification-$CFG.txt"
done
diff -q "$OUT/classification-Release.txt" "$OUT/classification-Debug.txt" \
  && echo "CLASSIFICATIONS BYTE-IDENTICAL BETWEEN CONFIGS" \
  || echo "CLASSIFICATIONS DIFFER BETWEEN CONFIGS (expected: Debug carries DWARF-only movers)"
INNER
run_locked G5-psym bash "$S/g5.sh"; rc5=$?

# --- G6: smoke, format, whitespace ---------------------------------------
cat > "$S/g6.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=$ROOT/.scratch/m7w2m2; G=$S/gates
cd "$ROOT" || exit 90
bash scripts/check_install_smoke.sh > "$G/install-smoke.log" 2>&1
echo "INSTALL_SMOKE_RC=$?"; tail -4 "$G/install-smoke.log"
bash scripts/check_export_contract.sh > "$G/export-contract.log" 2>&1
echo "EXPORT_CONTRACT_RC=$?"; tail -3 "$G/export-contract.log"
echo "--- clang-format on this task's authored files"
clang-format --dry-run -Werror \
  include/hven/core/solver_counters.h src/drivers/sqp_solver.cpp \
  tests/drivers/test_callback.cpp tests/drivers/test_threads.cpp \
  tests/sqp/test_trace_writer.cpp tests/sqp/test_sqp_restoration.cpp
echo "CLANG_FORMAT_RC=$?"
git diff --check; echo "DIFF_CHECK_RC=$?"
git status --porcelain | wc -l
INNER
run_locked G6-smoke bash "$S/g6.sh"; rc6=$?

echo "GATES_SUMMARY G1=$rc1 G2=$rc2 G3=$rc3 G4=$rc4 G5=$rc5 G6=$rc6"
echo "GATES_END $(date -Is)"
exit $(( rc1 | rc2 | rc3 | rc4 | rc6 ))
