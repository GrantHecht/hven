#!/usr/bin/env bash
# AP-2 B6 suite leg (B5's instrument).  usage: ctest.sh <build-dir-name> <log> [extra ctest args...]
# Run as: flock /tmp/box-build.lock bash ctest.sh ...   (ctest -j4 under setsid)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a79f97c30e3369bfe
DIR=$WT/build-b6/${1:?dir}
LOG=${2:?log}
shift 2
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
cd "$DIR" || exit 1
setsid ctest -j4 --output-on-failure "$@" > "$LOG" 2>&1
rc=$?
tail -5 "$LOG"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
