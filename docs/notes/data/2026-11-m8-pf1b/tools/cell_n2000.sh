#!/usr/bin/env bash
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf1b
OUT=$WT/build-pf1b/cell-n2000
mkdir -p "$OUT/dumps"
export HVEN_PF1_QPDUMP="$OUT/dumps" HVEN_PF1_SUMMARY=1 HVEN_PF1B_TRACE=1
"$WT/build-pf1b/diag/bench/hven_sqp_corpus" --engine walk --cells f7_n2000_path_corrupted --csv "$OUT/row.csv" > "$OUT/stdout.txt" 2> "$OUT/stderr.txt"
echo "cell rc=$?"
grep -v '^#' "$OUT/row.csv" | cut -d, -f1,7-13
grep PF1SOLVE "$OUT/stderr.txt" | awk '{print $2,$3,$9,$10}'
