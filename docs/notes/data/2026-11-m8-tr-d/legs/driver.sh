#!/usr/bin/env bash
# M8 TR-D: the whole leg sequence, STOPPING at the first failure: arm A prebuild, Release build, runtime (A/A then B/A), then chain.sh.
# usage: driver.sh <code head sha>     Run as: flock /tmp/box-build.lock bash driver.sh <sha>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/tr-d
D=$WT/docs/notes/data/2026-11-m8-tr-d
HEAD_SHA=${1:?code head sha}
BASE_SHA=70b5d42be
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "DRIVER_START $(date -Is) head=$HEAD_SHA"
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench examples CMakeLists.txt cmake scripts || { echo SOURCE_MOVED; exit 2; }
bash "$D/legs/prebuild.sh" "$BASE_SHA" || { echo "STOP prebuild"; exit 1; }
bash "$D/legs/build.sh" Release rel > "$WT/.scratch/build-rel.log" 2>&1 || { echo "STOP build-rel"; tail -20 "$WT/.scratch/build-rel.log"; exit 1; }
tail -2 "$WT/.scratch/build-rel.log"
ARMA=$(sha256sum "$WT/.scratch/build-trd/armAbin/hven_sqp_corpus" | cut -d' ' -f1)
ARMB=$(sha256sum "$WT/.scratch/build-trd/rel/bench/hven_sqp_corpus" | cut -d' ' -f1)
echo "ARMA=$ARMA ARMB=$ARMB"
bash "$D/runtime/run_both.sh" "$ARMA" "$ARMB" || { echo "STOP runtime"; exit 1; }
bash "$D/legs/chain.sh" "$HEAD_SHA" "$ARMB" "$ARMA" || { echo "STOP chain"; exit 1; }
echo "DRIVER_COMPLETE $(date -Is)"
