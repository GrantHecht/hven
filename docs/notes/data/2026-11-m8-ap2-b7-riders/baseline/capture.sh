#!/usr/bin/env bash
# AP-2 B7 riders, C238 (1): capture the interior leg's baseline for promotion. The corpus binary of a FRESH
# Release build of the riders' code commit (build-b7r/cap, ../legs/build.sh's flags, configured on a clean
# tree so its `# binary:` stamp is the commit), run TWICE with the frozen predecessor's own invocation (its
# 11-cell list, read off its `# invocation:` line), solo under the box lock, taskset -c 2,
# MKL_NUM_THREADS=1, OMP_NUM_THREADS=1; the two captures compared (compare_replay.py), and the first checked
# against the frozen file with B7's instrument and declared list (promotion-check.txt).
# Run as: flock /tmp/box-build.lock bash capture.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a00603e2048c1c37b
EV=$WT/docs/notes/data/2026-11-m8-ap2-b7-riders/baseline
BIN=$WT/build-b7r/cap/bench/hven_sqp_corpus
FROZEN=$WT/bench/baselines/2026-09-t8-ipm-leg/interior_baseline.csv
DECL=$WT/docs/notes/data/2026-11-m8-ap2-b7/interior/interior-declared.txt
CHECK=$WT/docs/notes/data/2026-11-m8-ap2-b7-riders/legs/interior_check.py
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
git -C "$WT" log --oneline -1
git -C "$WT" status --short --untracked-files=no
sha256sum "$BIN" "$FROZEN" "$DECL"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
unset HVEN_LEG_PRE_B7_INFINITE_BOUND HVEN_LEG_IPM_PRE_D1_DEFAULTS
CELLS=$(grep -m1 '^# invocation:' "$FROZEN" | sed -E 's/.*--cells ([^ ]+).*/\1/')
echo "cells: $CELLS"
worst=0
for k in 1 2; do
  taskset -c 2 "$BIN" --engine interior --cells "$CELLS" --csv "$EV/capture-$k.csv" > "$EV/capture-run-$k.txt" 2>&1
  rc=$?; echo "capture $k rc=$rc"; [ $rc -gt $worst ] && worst=$rc
done
python3 scripts/compare_replay.py "$EV/capture-1.csv" "$EV/capture-2.csv" "capture-1-vs-2" > "$EV/capture-comparison.txt" 2>&1
rc=$?; tail -1 "$EV/capture-comparison.txt"; [ $rc -gt $worst ] && worst=$rc
python3 scripts/compare_replay.py "$FROZEN" "$WT/docs/notes/data/2026-11-m8-ap2-b7/interior/interior-A.csv" "frozen-vs-b7-armA" > "$EV/promotion-check.txt" 2>&1
python3 "$CHECK" --check "$DECL" "$FROZEN" "$EV/capture-1.csv" >> "$EV/promotion-check.txt" 2>&1
rc=$?; tail -2 "$EV/promotion-check.txt"; [ $rc -gt $worst ] && worst=$rc
python3 "$CHECK" --list "$FROZEN" "$EV/capture-1.csv" > "$EV/promotion-differences.txt" 2>&1
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
