#!/usr/bin/env bash
# sparse-equil r2 proof chain (../DECLARATION-r2.md section 5): P0 checks, P3 TRAIN, P4 runtime gate (walk),
# P5 mutations. Stops at the first failed leg. usage: orchestrate.sh <code sha> <count> <corpus A sha256> <corpus B sha256> <lane A sha256> <lane B sha256>
#   Run under: flock /tmp/box-build.lock setsid -w bash orchestrate.sh ... > ../orchestrate.out 2>&1
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/sparse-equil
D=$WT/docs/notes/data/2026-11-m8-sparse-equil
L=$D/legs
CODE=${1:?code sha}; COUNT=${2:?ctest count}; CA=${3:?}; CB=${4:?}; LA=${5:?}; LB=${6:?}
DEVR=$WT/.scratch/dev-replay
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
PATHS="src include tests bench examples bindings CMakeLists.txt cmake scripts"
git -C "$WT" diff --quiet "$CODE" HEAD -- $PATHS || { echo "CODE_PATHS_DIFFER_FROM $CODE"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no -- $PATHS)" ] || { echo CODE_PATHS_DIRTY; exit 2; }
echo "code paths == $CODE, clean; HEAD $(git -C "$WT" rev-parse HEAD)"
chk() { [ "$(sha256sum "$1" | cut -d' ' -f1)" = "$2" ] || { echo "SHA_MISMATCH $1"; exit 4; }; }
chk "$WT/build-se/bin/A/hven_sqp_corpus" "$CA"; chk "$WT/build-se/bin/B/hven_sqp_corpus" "$CB"
chk "$WT/build-se/bin/A/hven_bench_cell" "$LA"; chk "$WT/build-se/bin/B/hven_bench_cell" "$LB"
mkdir -p "$D/train" "$D/mutation"
leg() { local name=$1; shift
  echo "LEG $name start $(date -Is)"
  "$@"; local rc=$?
  echo "LEG $name rc=$rc $(date -Is)"
  if [ $rc -ne 0 ]; then echo "STOP at $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; fi; }
suites_ok() { grep -q "100% tests passed, 0 tests failed out of $COUNT" "$1" || { echo "P1_CRITERION_NOT_MET $1"; return 1; }; }
replay_ok() { python3 "$L/replay_check.py" "$DEVR" "$D/replay" "$L/movers-replay.txt"; }
train_ok() { python3 "$L/train_check.py" "$WT/.scratch/r2-train/B" "$WT/.scratch/r2-train/A" "$WT/docs/notes/data/2026-11-m8-dn3/roster.tsv" > "$D/train/train_check.txt" 2>&1; local rc=$?
  grep -E '^(SUMMARY|MOVER|  FAIL|I4)' "$D/train/train_check.txt" | head -60; return $rc; }
leg P3A bash "$L/train_enum.sh" "$WT/.scratch/r2-train/A" "$WT/build-se/bin/A/hven_bench_cell" "$LA"
leg P3B bash "$L/train_enum.sh" "$WT/.scratch/r2-train/B" "$WT/build-se/bin/B/hven_bench_cell" "$LB"
leg P3-criterion train_ok
leg P4 bash "$D/runtime/run_walk.sh" "$CB" "$CA"
leg P5 bash "$L/mutations.sh" "$D/mutation"
echo "ALL_LEGS_PASSED"
echo "FOREGROUND_END $(date -Is)"
