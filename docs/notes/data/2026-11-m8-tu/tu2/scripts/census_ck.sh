#!/bin/bash
# census_ck.sh TAG BINARY : restricted census read for one candidate (waits for C0)
cd /home/ghecht/Projects/hven-bench-tu2
TAG=$1; BIN=$2
true
W=/home/ghecht/Projects/hven/.claude/worktrees/tu2
mkdir -p .scratch/census/$TAG
awk -F, '/^#/ {print; next} !h {print; h=1; next} $7=="Optimal" && $1!="f7_n10000_path_physics" {print}' .scratch/census/C0/walk_census.csv > .scratch/census/$TAG/restricted_baseline.csv
N=$(grep -v "^#" .scratch/census/$TAG/restricted_baseline.csv | tail -n +2 | wc -l)
echo "restricted cells: $N" > .scratch/logs/census-$TAG.log
~/.local/bin/box-lane -- bash .scratch/run_walk_census_sub.sh --binary $PWD/$BIN --out $PWD/.scratch/census/$TAG/run \
  --baseline $PWD/.scratch/census/$TAG/restricted_baseline.csv --compare $W/scripts/census_compare.py --expect-cells $N --t1-width 4 --t3-width 4 >> .scratch/logs/census-$TAG.log 2>&1
echo "$TAG rc=$?" >> .scratch/logs/census-$TAG.log
