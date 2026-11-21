# PF-2e scratch: the diag lane binary (old rule + event log) over TRAIN's hven-sqp rows (D, K). Run under box-lane.
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf2e
BIN=$WT/.scratch/bin/diag/hven_bench_cell; OUT=$WT/.scratch/dlog/train; mkdir -p $OUT
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
cd $WT/.scratch/hb
one() { # core prob label arm
  PF2E_LOG=$OUT/$2.$4 timeout 300 taskset -c $1 $BIN --lane hven-sqp --problem $2 --options $3 --budget-iters 0 --options-dump /dev/null --assert-reference > $OUT/$2.$4.out 2>&1
}
read -r a b c d <<< "$BOX_LANE_PHYS"; CORES=($a $b $c $d)
i=0
while IFS=$'\t' read -r pop lane prob; do
  [ "$lane" = hven-sqp ] || continue
  for arm in D:default K:never-default; do
    one ${CORES[$((i % 4))]} $prob ${arm#*:} ${arm%%:*} &
    i=$((i + 1))
    if [ $((i % 4)) -eq 0 ]; then wait; fi
  done
done < $WT/docs/notes/data/2026-11-m8-dn3/roster.tsv
wait
echo TRAIN_DIAG_DONE
