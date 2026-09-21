#!/usr/bin/env bash
# M7 W2 M2b -- THE GATES at the artifact commit, in SIX lock segments so the
# harness lane's own cells can interleave between them.
#
#   G1  Debug: fresh configure, build, suite WITHOUT the >600 s test
#   G2  the >600 s test ALONE (Continuation.ProbeBudgetBoundsAFailingProposal)
#   G3  Release: fresh configure (the bench binaries carry a configure-time
#       git-describe, so a stamped leg re-configures), build, suite
#   G4  the U0 27-cell three-arm replay against the M6 close pin
#   G5  P-SYM, Release and Debug, classified against a494b7f5 with THIS task's
#       own changed-header list (the two headers 577719a0 edits under include/)
#   G6  install smoke, the export contract (three arguments), clang-format,
#       git diff --check
#
# Exit 1 from P-SYM is EXPECTED -- one public field on SqpOptions moves sizeof
# 400 -> 416 in every TU that lays the struct out -- and the gate is the
# CLASSIFICATION, which must report 0 unexplained.
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m2b
G=$S/gates
BEFORE=a494b7f5
CLASSIFIER=$ROOT/docs/notes/data/2026-09-m7-w2-m2b-proximity/wrappers/classify_psym.py
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
ROOT=/home/ghecht/Projects/hven; S=$ROOT/.scratch/m7w2m2b; G=$S/gates
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
ROOT=/home/ghecht/Projects/hven; S=$ROOT/.scratch/m7w2m2b; G=$S/gates
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest --test-dir "$S/gates-dbg" \
  -R "^Continuation.ProbeBudgetBoundsAFailingProposal$" --output-on-failure > "$G/suite-debug-long.log" 2>&1
rc=$?; tail -8 "$G/suite-debug-long.log"; exit $rc
INNER
run_locked G2-debug-long bash "$S/g2.sh"; rc2=$?

# --- G3: Release ----------------------------------------------------------
cat > "$S/g3.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=$ROOT/.scratch/m7w2m2b; G=$S/gates
rm -rf "$S/gates-rel"
cmake -S "$ROOT" -B "$S/gates-rel" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON \
  > "$G/configure-release.log" 2>&1 || exit 91
cmake --build "$S/gates-rel" -j "$(nproc)" > "$G/build-release.log" 2>&1 || exit 91
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest --test-dir "$S/gates-rel" -j4 --output-on-failure \
  > "$G/suite-release.log" 2>&1
rc=$?; tail -6 "$G/suite-release.log"
echo "--- THE GATE BUILD'S LIBRARY, against the one the 35 legs ran on"
sha256sum "$S/gates-rel/libhven.a" "$S/install/lib64/libhven.a" 2>/dev/null
exit $rc
INNER
run_locked G3-release bash "$S/g3.sh"; rc3=$?

# --- G4: the replay -------------------------------------------------------
cat > "$S/g4.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=$ROOT/.scratch/m7w2m2b; G=$S/gates
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
    "M6 CLOSE HEAD-$eng vs M7 W2 M2b HEAD-$eng" | tee -a "$D/comparison.txt"
  rc=${PIPESTATUS[0]}; echo "  CMP_RC=$rc"; [ $rc -eq 0 ] || worst=1
done
exit $worst
INNER
run_locked G4-replay bash "$S/g4.sh"; rc4=$?

# --- G5: P-SYM, both configs ---------------------------------------------
cat > "$S/g5.sh" <<'INNER'
set -uo pipefail
ROOT=/home/ghecht/Projects/hven; S=$ROOT/.scratch/m7w2m2b; G=$S/gates
BD=$S/psym-build; OUT=$G/psym; BEFORE=a494b7f5
CLASSIFIER=$ROOT/docs/notes/data/2026-09-m7-w2-m2b-proximity/wrappers/classify_psym.py
mkdir -p "$OUT"
HEAD_SHA=$(git -C "$ROOT" rev-parse HEAD)
BRANCH=$(git -C "$ROOT" rev-parse --abbrev-ref HEAD)
[ -z "$(git -C "$ROOT" status --porcelain)" ] || { echo "REFUSED: tree not clean"; exit 2; }
# The CLASSIFIER IS COPIED OUT OF THE TREE FIRST: the `before` arm below checks
# out a494b7f5, at which this task's artifact does not exist.
cp "$CLASSIFIER" "$S/classify_psym.py" || exit 93
CHANGED="src/drivers/sqp_options.cpp src/drivers/sqp_solver.cpp src/globalization/sqp/soc_elastic_restoration.cpp tests/drivers/test_options_dump.cpp tests/sqp/test_sqp_restoration.cpp"
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

# --- G6: smoke, export contract, format, whitespace ------------------------
cat > "$S/g6.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=$ROOT/.scratch/m7w2m2b; G=$S/gates
cd "$ROOT" || exit 90
# THE SMOKE'S WORKDIR IS KEPT so the export contract can read the SAME build's
# installed hvenTargets.cmake and the SAME build's configure log -- the script's
# own documented contract for the third argument (never a literal picked ahead
# of time).
rm -rf "$S/smoke-work"
HVEN_INSTALL_SMOKE_WORKDIR="$S/smoke-work" bash scripts/check_install_smoke.sh > "$G/install-smoke.log" 2>&1
echo "INSTALL_SMOKE_RC=$?"; tail -4 "$G/install-smoke.log"
ALIGN=$(grep -o "EIGEN_MAX_ALIGN_BYTES resolved to [0-9]*" "$S/smoke-work/hven-configure.log" | tail -1 | awk '{print $NF}')
echo "ALIGN_FROM_THIS_BUILDS_CONFIGURE_LOG=$ALIGN"
TARGETS="$S/smoke-work/hven-install/lib64/cmake/hven/hvenTargets.cmake"
ls -l "$TARGETS"
bash scripts/check_export_contract.sh "$TARGETS" linux "$ALIGN" > "$G/export-contract.log" 2>&1
echo "EXPORT_CONTRACT_RC=$?"; tail -3 "$G/export-contract.log"
echo "--- clang-format on this task's authored files (the eight of 577719a0, less the .md)"
clang-format --dry-run -Werror \
  include/hven/detail/globalization/sqp/restoration.h \
  include/hven/drivers/sqp_solver_types.h \
  src/drivers/sqp_options.cpp src/drivers/sqp_solver.cpp \
  src/globalization/sqp/soc_elastic_restoration.cpp \
  tests/drivers/test_options_dump.cpp tests/sqp/test_sqp_restoration.cpp
echo "CLANG_FORMAT_RC=$?"
git diff --check; echo "DIFF_CHECK_RC=$?"
echo "DIRTY_FILES=$(git status --porcelain | wc -l)"
INNER
run_locked G6-smoke bash "$S/g6.sh"; rc6=$?

echo "GATES_SUMMARY G1=$rc1 G2=$rc2 G3=$rc3 G4=$rc4 G5=$rc5 G6=$rc6"
echo "GATES_END $(date -Is)"
exit $(( rc1 | rc2 | rc3 | rc4 | rc6 ))
