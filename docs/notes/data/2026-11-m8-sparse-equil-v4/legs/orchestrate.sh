#!/usr/bin/env bash
# sparse-equil v4 proof chain (../DECLARATION-SEQ4.md section 5; adapted from ../../2026-11-m8-kopt-1x-r2/legs/orchestrate.sh):
# P0 checks, P1 ctest (rel, dbg), P2 replay x3, P3 TRAIN (238 x D/K), P4 runtime gate (walk, serial, solo), P5 mutations,
# P6 install smoke. Stops at the first failed leg.
# usage: orchestrate.sh <code sha> <corpus A sha256> <corpus B sha256> <lane A sha256> <lane B sha256> <ctest count>
#   Run under: flock /tmp/box-build.lock setsid -w bash orchestrate.sh ... > ../orchestrate.out 2>&1
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/seq4
D=$WT/docs/notes/data/2026-11-m8-sparse-equil-v4
L=$D/legs
CODE=${1:?code sha}; CA=${2:?}; CB=${3:?}; LA=${4:?}; LB=${5:?}; NTESTS=${6:?ctest count}
DEVR=$WT/.scratch/dev4-replay; DEVA=$WT/.scratch/dev-train-A; DEVB=$WT/.scratch/dev-train-B
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
PATHS="src include tests bench examples bindings CMakeLists.txt cmake scripts"
git -C "$WT" diff --quiet --ignore-submodules=all "$CODE" HEAD -- $PATHS || { echo "CODE_PATHS_DIFFER_FROM $CODE"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no --ignore-submodules=all -- $PATHS)" ] || { echo CODE_PATHS_DIRTY; exit 2; }
echo "code paths == $CODE, clean; HEAD $(git -C "$WT" rev-parse HEAD)"
chk() { [ "$(sha256sum "$1" | cut -d' ' -f1)" = "$2" ] || { echo "SHA_MISMATCH $1"; exit 4; }; }
chk "$WT/build-k/bin/A/hven_sqp_corpus" "$CA"; chk "$WT/build-k/bin/B/hven_sqp_corpus" "$CB"
chk "$WT/build-k/bin/A/hven_bench_cell" "$LA"; chk "$WT/build-k/bin/B/hven_bench_cell" "$LB"
mkdir -p "$D/suites" "$D/replay" "$D/train" "$D/mutation" "$D/install-smoke"
leg() { local name=$1; shift
  echo "LEG $name start $(date -Is)"
  "$@"; local rc=$?
  echo "LEG $name rc=$rc $(date -Is)"
  if [ $rc -ne 0 ]; then echo "STOP at $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; fi; }
suites_ok() { grep -q "100% tests passed, 0 tests failed out of $NTESTS" "$1" || { echo "P1_CRITERION_NOT_MET $1"; return 1; }; }
replay_ok() { # three B repetitions, each mode: vs arm A dev rows and vs the B dev rows, 0 differences each
  local bad=0
  for K in 1 2 3; do for M in walk ssn ipm; do
    python3 "$WT/scripts/compare_replay.py" "$DEVR/A-k1-$M.csv" "$D/replay/B-k$K-$M.csv" "A-vs-B-k$K-$M" > "$D/replay/cmp-A-k$K-$M.txt" 2>&1; tail -1 "$D/replay/cmp-A-k$K-$M.txt"
    grep -q "differences: 0$" "$D/replay/cmp-A-k$K-$M.txt" || bad=1
    python3 "$WT/scripts/compare_replay.py" "$DEVR/B-k1-$M.csv" "$D/replay/B-k$K-$M.csv" "B-vs-Bdev-k$K-$M" > "$D/replay/cmp-Bdev-k$K-$M.txt" 2>&1; tail -1 "$D/replay/cmp-Bdev-k$K-$M.txt"
    grep -q "differences: 0$" "$D/replay/cmp-Bdev-k$K-$M.txt" || bad=1
  done; done
  [ $bad -eq 0 ] && echo REPLAY_AS_DECLARED
  return $bad
}
train_ok() { python3 "$L/train_check.py" "$D/train/B" "$WT/docs/notes/data/2026-11-m8-dn3" "$DEVA" "$DEVB" "$L/movers-train.txt" > "$D/train/train_check.txt" 2>&1; local rc=$?
  grep -E '^(SUMMARY|MOVER|  FAIL|I4)' "$D/train/train_check.txt" | head -60; return $rc; }
leg P1-rel bash "$L/ctest.sh" "$WT" rel "$D/suites/ctest-rel.log"
leg P1-rel-criterion suites_ok "$D/suites/ctest-rel.log"
leg P1-dbg bash "$L/ctest.sh" "$WT" dbg "$D/suites/ctest-dbg.log"
leg P1-dbg-criterion suites_ok "$D/suites/ctest-dbg.log"
leg P2 bash "$L/replay.sh" "$D/replay" B "$WT/build-k/bin/B/hven_sqp_corpus" 3
leg P2-criterion replay_ok
leg P3 bash "$L/train_enum.sh" "$D/train/B" "$WT/build-k/bin/B/hven_bench_cell" "$LB"
leg P3-criterion train_ok
leg P4 bash "$D/runtime/run_walk.sh" "$CB" "$CA"
leg P5 bash "$L/mutations.sh" "$D/mutation"
leg P6 bash "$L/install_smoke.sh" "$D/install-smoke"
echo "ALL_LEGS_PASSED"
echo "FOREGROUND_END $(date -Is)"
