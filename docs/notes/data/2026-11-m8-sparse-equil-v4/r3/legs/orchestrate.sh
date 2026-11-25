#!/usr/bin/env bash
# sparse-equil v4 r3 continuation (../../DECLARATION-SEQ4-r3.md): P0 exactly as r2's, then P4, P5, P6 only (r2 run 1's P1-P3 stand). From r2's orchestrator (../../DECLARATION-SEQ4-r2.md section 5; ../../legs/orchestrate.sh re-pointed to r2/, bin-r2/, the
# r2 dev rows, and THE CODE IDENTITY of the declaration's section 6): P0 checks, P1 ctest (rel, dbg), P2 replay x3, P3 TRAIN
# (238 x D/K), P4 runtime gate (walk, serial, solo), P5 mutations, P6 install smoke. Stops at the first failed leg.
# usage: orchestrate.sh <code sha> <corpus A sha256> <corpus B sha256> <lane A sha256> <lane B sha256> <ctest count>
#   Run under: flock /tmp/box-build.lock setsid -w bash orchestrate.sh ... > ../orchestrate.out 2>&1
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/seq4
D=$WT/docs/notes/data/2026-11-m8-sparse-equil-v4/r2
L=$D/legs
R3=$WT/docs/notes/data/2026-11-m8-sparse-equil-v4/r3
CODE=${1:?code sha}; CA=${2:?}; CB=${3:?}; LA=${4:?}; LB=${5:?}; NTESTS=${6:?ctest count}
DEVR=$WT/.scratch/r2/dev-replay; DEVA=$WT/.scratch/r2/dev-train-A; DEVB=$WT/.scratch/r2/dev-train-B
BASE=44f74446f5e5c973de3b06b5628b41fd39449cd8
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
PATHS="src include tests bench examples bindings CMakeLists.txt cmake scripts"
git -C "$WT" diff --quiet --ignore-submodules=all "$CODE" HEAD -- $PATHS || { echo "CODE_PATHS_DIFFER_FROM $CODE"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no --ignore-submodules=all -- $PATHS)" ] || { echo CODE_PATHS_DIRTY; exit 2; }
# THE CODE IDENTITY (declaration section 6): the trees declared before the code commit, and the untouched paths equal the base.
for P in src include tests; do
  WANT=$(grep -E "^TREE $P " "$D/../DECLARATION-SEQ4-r2.md" | awk '{print $3}')
  GOT=$(git -C "$WT" rev-parse "$CODE:$P")
  [ -n "$WANT" ] && [ "$WANT" = "$GOT" ] || { echo "TREE_MISMATCH $P want=$WANT got=$GOT"; exit 2; }
done
git -C "$WT" diff --quiet --ignore-submodules=all "$BASE" "$CODE" -- bench examples bindings CMakeLists.txt cmake scripts || { echo BASE_PATHS_MOVED; exit 2; }
echo "code paths == $CODE == the declared trees, clean; HEAD $(git -C "$WT" rev-parse HEAD)"
chk() { [ "$(sha256sum "$1" | cut -d' ' -f1)" = "$2" ] || { echo "SHA_MISMATCH $1"; exit 4; }; }
chk "$WT/build-k/bin-r2/A/hven_sqp_corpus" "$CA"; chk "$WT/build-k/bin-r2/B/hven_sqp_corpus" "$CB"
chk "$WT/build-k/bin-r2/A/hven_bench_cell" "$LA"; chk "$WT/build-k/bin-r2/B/hven_bench_cell" "$LB"
mkdir -p "$R3/mutation" "$R3/install-smoke"
leg() { local name=$1; shift
  echo "LEG $name start $(date -Is)"
  "$@"; local rc=$?
  echo "LEG $name rc=$rc $(date -Is)"
  if [ $rc -ne 0 ]; then echo "STOP at $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; fi; }
leg P4 bash "$R3/runtime/run_walk.sh" "$CB" "$CA"
leg P5 bash "$L/mutations.sh" "$R3/mutation"
leg P6 bash "$L/install_smoke.sh" "$R3/install-smoke"
echo "ALL_LEGS_PASSED"
echo "FOREGROUND_END $(date -Is)"
