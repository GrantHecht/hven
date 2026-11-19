#!/usr/bin/env bash
# WA-1 r4 (../DECLARATION-WA1-r4.md section 3): the legs r3 did not complete -- P5 mutations (M5's anchor fixed) and P6 install smoke -- on
# r3's code commit and binaries; outputs under r4/. P1-P4 stand from r3 (same code, same binaries).
# usage: orchestrate_r4.sh <code sha>   Run under: flock /tmp/box-build.lock setsid -w bash orchestrate_r4.sh ... > ../r4/orchestrate.out 2>&1
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/wa1
D0=$WT/docs/notes/data/2026-11-m8-wa1
D=$D0/r4
L=$D0/legs
CODE=${1:?code sha}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
PATHS="src include tests bench examples bindings CMakeLists.txt cmake scripts"
git -C "$WT" diff --quiet --ignore-submodules=all "$CODE" HEAD -- $PATHS || { echo "CODE_PATHS_DIFFER_FROM $CODE"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no --ignore-submodules=all -- $PATHS)" ] || { echo CODE_PATHS_DIRTY; exit 2; }
echo "code paths == $CODE, clean; HEAD $(git -C "$WT" rev-parse HEAD)"
mkdir -p "$D/mutation" "$D/install-smoke"
leg() { local name=$1; shift
  echo "LEG $name start $(date -Is)"
  "$@"; local rc=$?
  echo "LEG $name rc=$rc $(date -Is)"
  if [ $rc -ne 0 ]; then echo "STOP at $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; fi; }
leg P5 bash "$L/mutations.sh" "$D/mutation"
leg P6 bash "$L/install_smoke.sh" "$D/install-smoke"
echo "ALL_LEGS_PASSED"
echo "FOREGROUND_END $(date -Is)"
