#!/bin/bash
set -o pipefail
R=/home/ghecht/Projects/hven/.scratch/w5t89r
LOG=$R/logs/L1b-smoke.log
LEG_RC=0
fold(){ local rc=$1; [ "$rc" -gt "$LEG_RC" ] && LEG_RC=$rc; return 0; }
H=$R/arm-head/build/bench/hven_sqp_corpus
mkdir -p $R/smoke
{
echo "PGREP_INLOCK $(date -u +%Y-%m-%dT%H:%M:%SZ)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(date -u +%Y-%m-%dT%H:%M:%SZ)"
E="MKL_NUM_THREADS=1 OMP_NUM_THREADS=1"
for spec in "ipm-corpus:--engine ipm --cells f7_n1000_bound_neutral --csv $R/smoke/c.csv" \
            "hs-r5:--hs --engine ipm --repeat 5 --csv $R/smoke/hs.csv" \
            "interior-1:--engine interior --cells f7_n1000_bound_neutral --csv $R/smoke/i.csv"; do
  name=${spec%%:*}; args=${spec#*:}
  s=$(date +%s.%N)
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $H $args > $R/smoke/$name.out 2>&1
  fold $?
  e=$(date +%s.%N)
  echo "SMOKE $name elapsed=$(echo "$e - $s" | bc) rc_so_far=$LEG_RC"
done
echo "FOREGROUND_END $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
