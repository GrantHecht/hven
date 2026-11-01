#!/bin/bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a026c6bee01193dcb
P=$W/.scratch/a1/probe/a1_probe
O=$W/.scratch/a1/out
mkdir -p $O
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
echo FOREGROUND_START $(date -Is)
flock /tmp/box-build.lock bash -c "
set -o pipefail
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
$P gate8 100 > $O/gate8-slack100.txt 2>&1 &&
$P gate8 1 > $O/gate8-slack1.txt 2>&1 &&
$P e1 e1_f7_n20000_af30_m1e-6 100 60 > $O/e1-red-s100-c60.txt 2>&1 &&
$P e1 e1_f7_n20000_af30_m1e-6 100 200 > $O/e1-red-s100-c200.txt 2>&1 &&
$P e1 e1_f7_n20000_af30_m1e-6 1 200 > $O/e1-red-s1-c200.txt 2>&1 &&
for c in e1_f7_n4000_af01_m1e-6 e1_f7_n4000_af10_m1e-6 e1_f7_n4000_af30_m1e-6 e1_f7_n20000_af01_m1e-6 e1_f7_n20000_af10_m1e-6 e1_f7_n20000_af30_m1e-4 e1_f7_n20000_af30_m1e-2; do
  $P e1 \$c 100 60 > $O/e1-\$c-s100-c60.txt 2>&1 || exit 1
  $P e1 \$c 1 60 > $O/e1-\$c-s1-c60.txt 2>&1 || exit 1
done &&
$P f7 walk > $O/f7-walk.txt 2>&1 &&
$P f7 ipm > $O/f7-ipm.txt 2>&1
"
rc=$?
echo FOREGROUND_END rc=$rc $(date -Is)
