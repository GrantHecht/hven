#!/usr/bin/env bash
# usage: capsearch.sh <cap-literal>   e.g. 1e-6
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
cap=$1
tag=cap${cap}
python3 - "$cap" <<'PY'
import re,sys
cap=sys.argv[1]
p='/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/include/hven/detail/qp/eqp_solve.h'
s=open(p).read()
s=re.sub(r'constexpr double kEqpPolishStepCap = [0-9eE.+-]+;', f'constexpr double kEqpPolishStepCap = {cap};', s, count=1)
open(p,'w').write(s)
PY
grep -n "kEqpPolishStepCap = " $W/include/hven/detail/qp/eqp_solve.h
$S/build.sh release all $tag-rel > /dev/null || exit 1
$S/build_tools.sh "-DNC2_COUNTERS=1" > /dev/null || exit 1
echo "=== CAP $cap : the three step-cap cells (border) ==="
for alg in border refactorize; do
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $S/tools-build-replay/dump_replay --algebra $alg --e2 1 $(cat $S/capcells.list) | sed "s|^.*/dumps/|$alg |" | cut -c1-110
done
B=$(find $W/.scratch/nc2-build-release/tests -name hven_sqp_tests -type f | head -1)
echo "=== CAP $cap : warm battery + key ==="
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $B --gtest_filter="WarmStartBattery.Corpus" > $S/logs2/$tag-wsb.log 2>&1
echo "warm-battery-rc=$? failures=$(grep -cE '^\[  FAILED  \] [A-Za-z].*\)$' $S/logs2/$tag-wsb.log)"
grep -E "^F3(n50|n1000|stress) +[a-z@+]+ +on " $S/logs2/$tag-wsb.log | awk '{k=$1" "$2; if(!(k in seen)){seen[k]=1; printf "%-10s %-10s majors=%-4s minors=%-5s brdref=%s\n",$1,$2,$5,$6,$11}}'
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $B --gtest_filter="QpEngineStructuralViolation.T7*:HsSweepCorpus.*:HsBattery.*" > $S/logs2/$tag-key.log 2>&1
echo "key-rc=$? failures=$(grep -cE '^\[  FAILED  \] [A-Za-z].*\)$' $S/logs2/$tag-key.log)"
