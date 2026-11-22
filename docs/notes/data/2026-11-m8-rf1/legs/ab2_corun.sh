#!/usr/bin/env bash
# RF-1 gate A+B2 legs T (TRAIN, arm AB2) and P (the pilot's failing cells, arms C and AB2), co-run on three pinned
# physical cores of one box lane (counters only, MKL_NUM_THREADS=1; CLAUDE.md section 7; DECLARATION-RF-1-AB2.md).
# Run under: ~/.local/bin/box-lane -- bash ab2_corun.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1
D=$WT/docs/notes/data/2026-11-m8-rf1; L=$D/legs; E=$D/gate-AB2
echo "FOREGROUND_START $(date -Is) lane=$BOX_LANE phys=$BOX_LANE_PHYS"
read -r p1 p2 p3 _ <<< "$BOX_LANE_PHYS"
want() { grep -F "$1" $D/BINARIES.txt | awk '{print $NF}'; }
for t in "pilot C:$WT/.scratch/bin/pilot-C/hven_bench_cell" "pilot AB2:$WT/.scratch/bin/pilot-AB2/hven_bench_cell"; do
  [ "$(sha256sum "${t#*:}" | cut -d' ' -f1)" = "$(want "${t%%:*} ")" ] || { echo "SHA_MISMATCH ${t%%:*}"; exit 4; }
done
mkdir -p $E/train $E/pilot
PIN_CORE=$p1 RPIN_CORE=$p1 bash $L/train_enum.sh $E/train/AB2 $WT/.scratch/bin/AB2/hven_bench_cell "$(want 'lane AB2 ')" > $E/train/enum-AB2.out 2>&1 & tp=$!
bash $L/pilot_run.sh C $E/pilot/C $p2 > $E/pilot/run-C.out 2>&1 & pc=$!
bash $L/pilot_run.sh AB2 $E/pilot/AB2 $p3 > $E/pilot/run-AB2.out 2>&1 & pa=$!
wait $tp; echo "train rc=$? $(date -Is)"
python3 $L/ab2_train.py $E/train/AB2 $D/gate-C/train/C $WT/docs/notes/data/2026-11-m8-dn3/roster.tsv > $E/train/ab2_check.txt 2>&1
echo "T rc=$?"; grep -E "^(AB2_SUMMARY|TOTALS|LOST_SOLVE)" $E/train/ab2_check.txt
wait $pc; echo "pilot C rc=$? $(date -Is)"; wait $pa; echo "pilot AB2 rc=$? $(date -Is)"
python3 $L/pilot_report.py $E/pilot/C $E/pilot/AB2 > $E/pilot/report.txt 2>&1; echo "P rc=$?"
echo "FOREGROUND_END $(date -Is)"
