#!/usr/bin/env bash
# RF-1 gate A+B2 r4 legs T (TRAIN, arms Cr3 = C' b1fe115d0 and ABr4 = the F-2 head) and P (the pilot's 62 cells, both arms), co-run on the
# four pinned physical cores of one box lane (counters only, MKL_NUM_THREADS=1; CLAUDE.md section 7; DECLARATION-RF-1-AB2-r4.md).
# r3_corun.sh with the candidate tag ABr4 and the r4 checks (r4_check.py) added; the C' binaries are r3's, sha-checked.
# Run under: ~/.local/bin/box-lane -- bash r4_corun.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1
D=$WT/docs/notes/data/2026-11-m8-rf1; L=$D/legs; E=$D/gate-AB2-r4; R3=$D/gate-AB2-r3
echo "FOREGROUND_START $(date -Is) lane=$BOX_LANE phys=$BOX_LANE_PHYS"
read -r p1 p2 p3 p4 _ <<< "$BOX_LANE_PHYS"
want() { grep -F "$1" $D/BINARIES.txt | tail -1 | awk '{print $NF}'; }
for t in "pilot Cr3:$WT/.scratch/bin/pilot-Cr3/hven_bench_cell" "pilot ABr4:$WT/.scratch/bin/pilot-ABr4/hven_bench_cell" \
         "lane Cr3:$WT/.scratch/bin/Cr3/hven_bench_cell" "lane ABr4:$WT/.scratch/bin/ABr4/hven_bench_cell"; do
  [ "$(sha256sum "${t#*:}" | cut -d' ' -f1)" = "$(want "${t%%:*} ")" ] || { echo "SHA_MISMATCH ${t%%:*}"; exit 4; }
done
mkdir -p $E/train $E/pilot
PIN_CORE=$p1 RPIN_CORE=$p1 bash $L/train_enum.sh $E/train/Cr3 $WT/.scratch/bin/Cr3/hven_bench_cell "$(want 'lane Cr3 ')" > $E/train/enum-Cr3.out 2>&1 & t1=$!
PIN_CORE=$p2 RPIN_CORE=$p2 bash $L/train_enum.sh $E/train/ABr4 $WT/.scratch/bin/ABr4/hven_bench_cell "$(want 'lane ABr4 ')" > $E/train/enum-ABr4.out 2>&1 & t2=$!
bash $L/pilot_run_r1.sh Cr3 $E/pilot/Cr3 $p3 > $E/pilot/run-Cr3.out 2>&1 & pc=$!
bash $L/pilot_run_r1.sh ABr4 $E/pilot/ABr4 $p4 > $E/pilot/run-ABr4.out 2>&1 & pa=$!
wait $t1; echo "train Cr3 rc=$? $(date -Is)"; wait $t2; echo "train ABr4 rc=$? $(date -Is)"
python3 $L/ab2_train.py $E/train/ABr4 $E/train/Cr3 $WT/docs/notes/data/2026-11-m8-dn3/roster.tsv > $E/train/ab2_check.txt 2>&1
echo "T rc=$?"; grep -E "^(AB2_SUMMARY|TOTALS|LOST_SOLVE)" $E/train/ab2_check.txt
python3 $L/ab2_train.py $E/train/ABr4 $R3/train/ABr3 $WT/docs/notes/data/2026-11-m8-dn3/roster.tsv > $E/train/vs_ABr3.txt 2>&1
echo "T vs ABr3:"; grep -E "^(AB2_SUMMARY|TOTALS|STATUS_MOVER)" $E/train/vs_ABr3.txt
python3 $L/r4_check.py train $E/train/ABr4 $E/train/Cr3 > $E/train/r4_check.txt 2>&1; grep -E "^(FALSE_CERT|LOST|MISSING|VERDICT|rows)" $E/train/r4_check.txt
wait $pc; echo "pilot Cr3 rc=$? $(date -Is)"; wait $pa; echo "pilot ABr4 rc=$? $(date -Is)"
python3 $L/pilot_report.py $E/pilot/Cr3 $E/pilot/ABr4 > $E/pilot/report.txt 2>&1; echo "P rc=$?"
python3 $L/pilot_report.py $R3/pilot/ABr3 $E/pilot/ABr4 > $E/pilot/report-vs-ABr3.txt 2>&1
python3 $L/r4_check.py pilot $E/pilot/ABr4 $E/pilot/Cr3 $R3/pilot/ABr3 > $E/pilot/r4_check.txt 2>&1; cat $E/pilot/r4_check.txt
grep -c "threw=1" $E/pilot/Cr3/cells/*.out $E/pilot/ABr4/cells/*.out | awk -F: '{s+=$2} END {print "pilot cells that threw:", s}'
echo "FOREGROUND_END $(date -Is)"
