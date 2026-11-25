#!/usr/bin/env bash
# RF-1c legs T (TRAIN, arms R = origin/m8 976739bc6 and H = the RF-1c head, the 119 hven-sqp roster cells x {D, K}) and P (the pilot's 62 cells, both arms),
# co-run on the four pinned physical cores of one box lane (counters only, MKL_NUM_THREADS=1; CLAUDE.md section 7; DECLARATION-RF-1c.md).
# r4_corun.sh with the arms R / H and the reference-R checks (rf1c_check.py).
# Run under: ~/.local/bin/box-lane -- bash rf1c_corun.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1c
D=$WT/docs/notes/data/2026-11-m8-rf1; L=$D/legs-rf1c; E=$D/gate-RF-1c
echo "FOREGROUND_START $(date -Is) lane=$BOX_LANE phys=$BOX_LANE_PHYS"
read -r p1 p2 p3 p4 _ <<< "$BOX_LANE_PHYS"
want() { grep -F "$1" $D/BINARIES.txt | tail -1 | awk '{print $NF}'; }
for t in "pilot R:/home/ghecht/Projects/hven/.scratch/rf1c-build-legs/bin/pilot-R/hven_bench_cell" "pilot H:/home/ghecht/Projects/hven/.scratch/rf1c-build-legs/bin/pilot-H/hven_bench_cell" \
         "lane R:/home/ghecht/Projects/hven/.scratch/rf1c-build-legs/bin/R/hven_bench_cell" "lane H:/home/ghecht/Projects/hven/.scratch/rf1c-build-legs/bin/H/hven_bench_cell"; do
  [ "$(sha256sum "${t#*:}" | cut -d' ' -f1)" = "$(want "RF1C ${t%%:*} ")" ] || { echo "SHA_MISMATCH ${t%%:*}"; exit 4; }
done
mkdir -p $E/train $E/pilot
PIN_CORE=$p1 RPIN_CORE=$p1 bash $L/train_enum.sh $E/train/R /home/ghecht/Projects/hven/.scratch/rf1c-build-legs/bin/R/hven_bench_cell "$(want 'RF1C lane R ')" > $E/train/enum-R.out 2>&1 & t1=$!
PIN_CORE=$p2 RPIN_CORE=$p2 bash $L/train_enum.sh $E/train/H /home/ghecht/Projects/hven/.scratch/rf1c-build-legs/bin/H/hven_bench_cell "$(want 'RF1C lane H ')" > $E/train/enum-H.out 2>&1 & t2=$!
bash $L/pilot_run_r1.sh R $E/pilot/R $p3 > $E/pilot/run-R.out 2>&1 & pc=$!
bash $L/pilot_run_r1.sh H $E/pilot/H $p4 > $E/pilot/run-H.out 2>&1 & pa=$!
wait $t1; echo "train R rc=$? $(date -Is)"; wait $t2; echo "train H rc=$? $(date -Is)"
python3 $L/ab2_train.py $E/train/H $E/train/R $D/../2026-11-m8-dn3/roster.tsv > $E/train/ab2_check.txt 2>&1
echo "T rc=$?"; grep -E "^(AB2_SUMMARY|TOTALS|LOST_SOLVE)" $E/train/ab2_check.txt
python3 $L/rf1c_check.py train $E/train/H $E/train/R > $E/train/rf1c_check.txt 2>&1; grep -E "^(FALSE_CERT|LOST|MISSING|VERDICT|rows|STATUS_MOVER)" $E/train/rf1c_check.txt
wait $pc; echo "pilot R rc=$? $(date -Is)"; wait $pa; echo "pilot H rc=$? $(date -Is)"
python3 $L/pilot_report.py $E/pilot/R $E/pilot/H > $E/pilot/report.txt 2>&1; echo "P rc=$?"
python3 $L/rf1c_check.py pilot $E/pilot/H $E/pilot/R > $E/pilot/rf1c_check.txt 2>&1; cat $E/pilot/rf1c_check.txt
grep -c "threw=1" $E/pilot/R/cells/*.out $E/pilot/H/cells/*.out | awk -F: '{s+=$2} END {print "pilot cells that threw:", s}'
echo "FOREGROUND_END $(date -Is)"
