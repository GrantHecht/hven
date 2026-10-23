#!/bin/bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a026c6bee01193dcb
P=$W/.scratch/a1/probe/a1_probe2
O=$W/.scratch/a1/out
mkdir -p $O
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
echo FOREGROUND_START $(date -Is)
flock /tmp/box-build.lock bash -c "
set -o pipefail
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
for c in e1_f7_n4000_af01_m1e-6 e1_f7_n4000_af10_m1e-6 e1_f7_n4000_af30_m1e-6 e1_f7_n20000_af01_m1e-6 e1_f7_n20000_af10_m1e-6 e1_f7_n20000_af30_m1e-4; do
  $P e1demote \$c 100 60 > $O/demote-\$c-s100-c60.txt 2>&1 || exit 1
  $P e1demote \$c 1 60 > $O/demote-\$c-s1-c60.txt 2>&1 || exit 1
done &&
$P e1demote e1_f7_n20000_af30_m1e-6 100 200 > $O/demote-red-s100-c200.txt 2>&1 &&
$P e1demote e1_f7_n20000_af30_m1e-6 1 200 > $O/demote-red-s1-c200.txt 2>&1 &&
$P f7 walk > $O/f7-walk.txt 2>&1 &&
$P f7 ipm > $O/f7-ipm.txt 2>&1
"
rc=$?
echo FOREGROUND_END rc=$rc $(date -Is)
