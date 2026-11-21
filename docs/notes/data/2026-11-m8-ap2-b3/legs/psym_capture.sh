#!/usr/bin/env bash
# AP-2 B3-g: P-SYM capture of build-b3/rel (Release) into <snapshot-dir>.
# usage: psym_capture.sh <snapshot-dir>   Run as: flock /tmp/box-build.lock bash psym_capture.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a3b067b098f5e9221
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
bash scripts/psym_compare.sh capture "${1:?snapshot dir}" "$WT/build-b3/rel"
rc=$?
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
