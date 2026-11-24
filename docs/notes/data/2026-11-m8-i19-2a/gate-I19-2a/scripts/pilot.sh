#!/usr/bin/env bash
# usage: pilot.sh <store> <R|H> <arm>   PILOT-62 leg (one box-lane acquisition inside `i19_run.py pilot`)
HB=/home/ghecht/Projects/hven-bench-i19
B=/home/ghecht/Projects/hven/.claude/worktrees/i19-2a/docs/notes/data/2026-11-m8-i19-2a/BINARIES.txt
cd $HB || exit 1
echo "PILOT $3 lane=$2 start $(date -Is) harness=$(git rev-parse HEAD)"
.scratch/venv/bin/python tu/i19_run.py pilot $3 --store $1 --binary build-i19-$2/lanes/hven/hven_bench_cell --binaries $B --tag "I19 lane $2" --decoded $HB/.scratch/h3/decoded
rc=$?; echo "PILOT $3 lane=$2 end $(date -Is) rc=$rc"; exit $rc
