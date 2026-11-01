#!/bin/bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a026c6bee01193dcb
P=$W/.scratch/a1/probe/a1_probe4
O=$W/.scratch/a1/out
echo FOREGROUND_START $(date -Is)
bash $W/.scratch/a1/build_probe4.sh || exit 1
flock /tmp/box-build.lock bash -c "
set -o pipefail
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 A1_F7_PATH=1 A1_F7_MAXN=1000
$P f7 walk > $O/f7-path-n1000-walk.txt 2>&1 &&
$P f7 ipm > $O/f7-path-n1000-ipm.txt 2>&1
"
rc=$?
echo FOREGROUND_END rc=$rc $(date -Is)
