#!/usr/bin/env bash
# usage: probe.sh <outfile> <dump> <maxit> [ENV=VAL ...]   one P1Probe.RunDump run, serial, under the lock
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/p1
OUT=${1:?}; DUMP=${2:?}; MAXIT=${3:?}; shift 3
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; exit 3; fi
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-k/tmp GIT_CEILING_DIRECTORIES=$WT
env "$@" HVEN_P1_DUMP=$DUMP HVEN_P1_MAXIT=$MAXIT timeout 1200 $WT/build-k/rel/tests/hven_qp_walk_seam_tests --gtest_filter=P1Probe.RunDump > "$OUT" 2>&1
rc=$?; grep -a '\[ p1 \]' "$OUT" | cut -c1-260; echo "rc=$rc"
