#!/usr/bin/env bash
# dev TRAIN (not a leg): lane A (base) and lane B (dev3, env toggles); enum A, B-global (HVEN_P1_GLOBAL=1); check.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/p1
L=$WT/.scratch/legs; O=$WT/.scratch/p1out
ASHA=${1:?}; BSHA=${2:?}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; exit 3; fi
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/build-k/tmp
[ -x $WT/build-k/bin/A/hven_bench_cell ] || bash $L/train_setup.sh A $ASHA || exit 1
bash $L/train_setup.sh dev3 $BSHA || exit 1
bash $L/train_enum.sh $O/train-A $WT/build-k/bin/A/hven_bench_cell $(sha256sum $WT/build-k/bin/A/hven_bench_cell | cut -d' ' -f1)
HVEN_P1_GLOBAL=1 bash $L/train_enum.sh $O/train-Bglobal $WT/build-k/bin/dev3/hven_bench_cell $(sha256sum $WT/build-k/bin/dev3/hven_bench_cell | cut -d' ' -f1)
bash $L/train_enum.sh $O/train-Btrig $WT/build-k/bin/dev3/hven_bench_cell $(sha256sum $WT/build-k/bin/dev3/hven_bench_cell | cut -d' ' -f1)
for b in Bglobal Btrig; do
python3 $L/train_check.py $O/train-$b $WT/docs/notes/data/2026-11-m8-dn3 $O/train-A > $O/train_check-$b.txt 2>&1; echo "check $b rc=$?"
grep -E '^(SUMMARY|MOVER)' $O/train_check-$b.txt | head -60
done
echo "FOREGROUND_END $(date -Is)"
