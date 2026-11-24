#!/usr/bin/env bash
# usage: arm.sh <store> <R|H> <arm> <corpora> <phase> <logfile>   -- runs `i19_run.py arm` (each leg takes its own box-lane acquisition) in the foreground.
HB=/home/ghecht/Projects/hven-bench-i19
B=/home/ghecht/Projects/hven/.claude/worktrees/i19-2a/docs/notes/data/2026-11-m8-i19-2a/BINARIES.txt
STORE=$1; T=$2; ARM=$3; CORP=$4; PH=$5
cd $HB || exit 1
if [ $T = R ]; then SHA=afb65b8040a59f2064cff467f705210b5cb9a4f3; PFX=$HB/.hven/install-afb65b8040a5
else SHA=91787d329a06ae99770879762804a1babf316cbe; PFX=$HB/.hven/install-91787d329a06; fi
echo "ARM $ARM lane=$T corpora=$CORP phase=$PH start $(date -Is) harness=$(git rev-parse HEAD)"
.scratch/venv/bin/python tu/i19_run.py arm $ARM --store $STORE --binary build-i19-$T/lanes/hven/hven_bench_cell --binaries $B --tag "I19 lane $T" \
  --hven-sha $SHA --hven-prefix $PFX --corpora $CORP --phase $PH --grid .hven/hven_sqp_i19_grid-91787d329a06 --sens-cells $(dirname $B)/cells/sens_p.tsv
rc=$?
echo "ARM $ARM lane=$T end $(date -Is) rc=$rc"; exit $rc
