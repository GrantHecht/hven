#!/usr/bin/env bash
# usage: corpus.sh <tag> <algebra> <listfile> [e2 0|1] [ENV=v ...] -> corpus/<tag>-<alg>-<list>.txt
set -o pipefail
S=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/.scratch/e4
tag=$1; alg=$2; list=$3; e2=${4:-1}; shift 4 2>/dev/null || shift $#
for e in "$@"; do export "$e"; done
mkdir -p $S/corpus
s0=$(date +%s)
out=$S/corpus/$tag-$alg-$(basename $list .list).txt
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $S/tools-build-replay/dump_replay --algebra $alg --e2 $e2 $(cat $list) > $out 2> ${out%.txt}.err
rc=$?
echo "corpus $tag-$alg-$(basename $list .list) [$*] rc=$rc secs=$(( $(date +%s)-s0 ))"
exit $rc
