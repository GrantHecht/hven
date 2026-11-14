set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a7a2ce81cf8988de5
D=$WT/docs/notes/data/2026-11-m8-walk-cap
OUT=$D/legs/rebase-92e7c5d83
BIN=$WT/build-wc/bin-rb/hven_sqp_corpus
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; exit 3; fi
echo PGREP_EMPTY; echo "START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/build-wc/tmp
cd $WT; mkdir -p $OUT
sha256sum $BIN
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
for MODE in walk ssn ipm; do
  $BIN --engine $MODE --cells "$CELLS" --csv $OUT/$MODE.csv > $OUT/run-$MODE.txt 2>&1; echo "run $MODE rc=$?"
  python3 scripts/compare_replay.py $D/replay/k1-$MODE.csv $OUT/$MODE.csv "rebase-vs-leg-k1-$MODE" > $OUT/comparison-$MODE.txt 2>&1; echo "compare rc=$?"; head -1 $OUT/comparison-$MODE.txt
done
echo "END $(date -Is)"
