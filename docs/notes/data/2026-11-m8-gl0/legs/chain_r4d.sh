#!/usr/bin/env bash
# M8 GL-0 (../DECLARATION-r4d.md): the mutation leg alone, on the r4c head (no source moves since).
# usage: chain_r4d.sh <code head sha>      Run as: flock /tmp/box-build.lock setsid -w bash chain_r4d.sh <sha>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/gl0
D=$WT/docs/notes/data/2026-11-m8-gl0
L=$D/legs
HEAD_SHA=${1:?code head sha}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export GIT_CEILING_DIRECTORIES=$WT MKL_NUM_THREADS=1
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench examples CMakeLists.txt cmake scripts || { echo "SOURCE_MOVED_SINCE_CODE_HEAD"; exit 2; }
R=$WT/.scratch
O=$D/r4
mkdir -p "$O/mutation"
RAW_DIR=$R/r4d-mutation bash "$L/mutations.sh" "$HEAD_SHA" "$O/mutation"; rc=$?
echo "STEP mutations rc=$rc $(date -Is)"
[ $rc -eq 0 ] || { echo CHAIN_STOPPED_AT mutations; exit $rc; }
MUST_EXTRA="n1 n1c n2" REPORTED_EXTRA="n1b" python3 "$L/mutation_verdict.py" "$O/mutation/mutation-table.csv" "$O/mutation/verdict.txt"; rc=$?
echo "STEP mutation-verdict rc=$rc $(date -Is)"
echo "CHAIN_COMPLETE"
echo "FOREGROUND_END $(date -Is)"
exit $rc
