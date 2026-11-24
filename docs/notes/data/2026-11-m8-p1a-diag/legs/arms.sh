#!/usr/bin/env bash
# usage: arms.sh CELLS OUTROOT "arm1 arm2 ..."  (arm = name=K:V:K:V or 'base')
WT=/home/ghecht/Projects/hven/.claude/worktrees/p1a-diag
BIN=/home/ghecht/Projects/hven-bench-p1a/build-p1a/lanes/hven/hven_bench_cell
for a in $3; do name=${a%%=*}; kv=${a#*=}; [ "$name" = "$a" ] && kv=""
  ARMENV="$(echo $kv | tr ':' ' ') ${EXTRA:-}" python3 $WT/.scratch/legs/corun.py $1 $2/$name $BIN ${CAP:-300}
done
