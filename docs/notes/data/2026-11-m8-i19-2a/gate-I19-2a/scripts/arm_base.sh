#!/usr/bin/env bash
# N2b: H0 with the i19-base lane binary (harness b026453, built against the H install) under the head harness's run_serial.
# usage: arm_base.sh <store> <corpora> <phase>      pilot: arm_base.sh <store> PILOT
HB=/home/ghecht/Projects/hven-bench-i19
BASEBIN=/home/ghecht/Projects/hven-bench-i19-base/build-i19-H/lanes/hven/hven_bench_cell
BF=/home/ghecht/Projects/hven/.claude/worktrees/i19-2a/docs/notes/data/2026-11-m8-i19-2a/gate-I19-2a/N2b/BINARIES-N2b.txt
cd $HB || exit 1
if [ "$2" = PILOT ]; then
  echo "PILOT(base) start $(date -Is)"
  .scratch/venv/bin/python tu/i19_run.py pilot H0 --store $1 --binary $BASEBIN --binaries $BF --tag "I19 lane H i19-base" --decoded $HB/.scratch/h3/decoded
else
  echo "ARM(base) H0 corpora=$2 phase=$3 start $(date -Is)"
  .scratch/venv/bin/python tu/i19_run.py arm H0 --store $1 --binary $BASEBIN --binaries $BF --tag "I19 lane H i19-base" \
    --hven-sha 91787d329a06ae99770879762804a1babf316cbe --hven-prefix $HB/.hven/install-91787d329a06 --corpora $2 --phase $3
fi
rc=$?; echo "end $(date -Is) rc=$rc"; exit $rc
