#!/usr/bin/env bash
# WA-1 proof chain (../DECLARATION-WA1-r3.md section 5; the r3 copy of orchestrate_r2.sh: outputs under r3/, r3 dev rows and movers): P0 checks, P1 (rel, dbg), P2 replay x3, P3 TRAIN, P4 runtime gate (walk, serial and
# solo), P5 mutations, P6 install smoke. Stops at the first failed leg.
# usage: orchestrate.sh <code sha> <corpus A sha256> <corpus B sha256> <lane A sha256> <lane B sha256> <ctest count>
#   Run under: flock /tmp/box-build.lock setsid -w bash orchestrate.sh ... > ../orchestrate.out 2>&1
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/wa1
D0=$WT/docs/notes/data/2026-11-m8-wa1
D=$D0/r3   # r3: every output under r3/
L=$D0/legs
CODE=${1:?code sha}; CA=${2:?}; CB=${3:?}; LA=${4:?}; LB=${5:?}; NTESTS=${6:?ctest count}
DEVR=$WT/.scratch/r3/dev-replay; DEVA=$WT/.scratch/r3/dev-train-A; DEVB=$WT/.scratch/r3/dev-train-B
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
PATHS="src include tests bench examples bindings CMakeLists.txt cmake scripts"
git -C "$WT" diff --quiet --ignore-submodules=all "$CODE" HEAD -- $PATHS || { echo "CODE_PATHS_DIFFER_FROM $CODE"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no --ignore-submodules=all -- $PATHS)" ] || { echo CODE_PATHS_DIRTY; exit 2; }
echo "code paths == $CODE, clean; HEAD $(git -C "$WT" rev-parse HEAD)"
chk() { [ "$(sha256sum "$1" | cut -d' ' -f1)" = "$2" ] || { echo "SHA_MISMATCH $1"; exit 4; }; }
chk "$WT/build-wa/bin/A/hven_sqp_corpus" "$CA"; chk "$WT/build-wa/bin/B/hven_sqp_corpus" "$CB"
chk "$WT/build-wa/bin/A/hven_bench_cell" "$LA"; chk "$WT/build-wa/bin/B/hven_bench_cell" "$LB"
mkdir -p "$D/suites" "$D/replay" "$D/train" "$D/mutation" "$D/install-smoke" "$D/runtime"
leg() { local name=$1; shift
  echo "LEG $name start $(date -Is)"
  "$@"; local rc=$?
  echo "LEG $name rc=$rc $(date -Is)"
  if [ $rc -ne 0 ]; then echo "STOP at $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; fi; }
suites_ok() { grep -q "100% tests passed, 0 tests failed out of $NTESTS" "$1" || { echo "P1_CRITERION_NOT_MET $1"; return 1; }; }
replay_ok() { # three B repetitions, each mode: vs arm A's dev rows (exactly the declared movers) and vs B's dev rows (0 differences)
  local bad=0
  for K in 1 2 3; do for M in walk ssn ipm; do
    python3 "$WT/scripts/compare_replay.py" "$DEVR/A-k1-$M.csv" "$D/replay/B-k$K-$M.csv" "A-vs-B-k$K-$M" > "$D/replay/cmp-A-k$K-$M.txt" 2>&1; tail -1 "$D/replay/cmp-A-k$K-$M.txt"
    grep -q "differences: $(sed -n "s/^$M //p" "$D/movers-replay.txt")\$" "$D/replay/cmp-A-k$K-$M.txt" || bad=1
    python3 "$WT/scripts/compare_replay.py" "$DEVR/B-k1-$M.csv" "$D/replay/B-k$K-$M.csv" "B-vs-Bdev-k$K-$M" > "$D/replay/cmp-Bdev-k$K-$M.txt" 2>&1; tail -1 "$D/replay/cmp-Bdev-k$K-$M.txt"
    grep -q "differences: 0$" "$D/replay/cmp-Bdev-k$K-$M.txt" || bad=1
  done; done
  [ $bad -eq 0 ] && echo REPLAY_AS_DECLARED
  return $bad
}
train_ok() { python3 "$L/train_check.py" "$D/train/B" "$WT/docs/notes/data/2026-11-m8-dn3" "$DEVA" "$DEVB" "$D/movers-train.txt" > "$D/train/train_check.txt" 2>&1; local rc=$?
  grep -E '^(SUMMARY|MOVER|  FAIL|I4)' "$D/train/train_check.txt" | head -60
  grep -q "^MOVER WORSE" "$D/train/train_check.txt" && { echo "P3_WORSE_MOVER"; return 1; }
  return $rc; }
leg P1-rel bash "$L/ctest.sh" "$WT" rel "$D/suites/ctest-rel.log"
leg P1-rel-criterion suites_ok "$D/suites/ctest-rel.log"
leg P1-dbg bash "$L/ctest.sh" "$WT" dbg "$D/suites/ctest-dbg.log"
leg P1-dbg-criterion suites_ok "$D/suites/ctest-dbg.log"
leg P2 bash "$L/replay.sh" "$D/replay" B "$WT/build-wa/bin/B/hven_sqp_corpus" 3
leg P2-criterion replay_ok
leg P3 bash "$L/train_enum.sh" "$D/train/B" "$WT/build-wa/bin/B/hven_bench_cell" "$LB"
leg P3-criterion train_ok
leg P4 bash "$D0/runtime/run_walk.sh" "$CB" "$CA" "$D/runtime"
leg P5 bash "$L/mutations.sh" "$D/mutation"
leg P6 bash "$L/install_smoke.sh" "$D/install-smoke"
echo "ALL_LEGS_PASSED"
echo "FOREGROUND_END $(date -Is)"
