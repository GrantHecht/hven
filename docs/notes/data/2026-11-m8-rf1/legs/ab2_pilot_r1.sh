#!/usr/bin/env bash
# RF-1 gate A+B2 leg P, re-run under DECLARATION-RF-1-AB2-r1.md: arms C and AB2 co-run on two pinned physical cores of one
# box lane (counters only, MKL_NUM_THREADS=1), through pilot_run_r1.sh; then pilot_report.py.
# Run under: ~/.local/bin/box-lane -- bash ab2_pilot_r1.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1
D=$WT/docs/notes/data/2026-11-m8-rf1; L=$D/legs; E=$D/gate-AB2/pilot-r1
echo "FOREGROUND_START $(date -Is) lane=$BOX_LANE phys=$BOX_LANE_PHYS"
read -r p1 p2 _ <<< "$BOX_LANE_PHYS"
want() { grep -F "$1" $D/BINARIES.txt | awk '{print $NF}'; }
for t in "pilot C:$WT/.scratch/bin/pilot-C/hven_bench_cell" "pilot AB2:$WT/.scratch/bin/pilot-AB2/hven_bench_cell"; do
  [ "$(sha256sum "${t#*:}" | cut -d' ' -f1)" = "$(want "${t%%:*} ")" ] || { echo "SHA_MISMATCH ${t%%:*}"; exit 4; }
done
mkdir -p $E
bash $L/pilot_run_r1.sh C $E/C $p1 > $E/run-C.out 2>&1 & pc=$!
bash $L/pilot_run_r1.sh AB2 $E/AB2 $p2 > $E/run-AB2.out 2>&1 & pa=$!
wait $pc; echo "pilot C rc=$? $(date -Is)"; wait $pa; echo "pilot AB2 rc=$? $(date -Is)"
python3 $L/pilot_report.py $E/C $E/AB2 > $E/report.txt 2>&1; echo "P rc=$?"
grep -c "threw=1" $E/C/cells/*.out $E/AB2/cells/*.out | awk -F: '{s+=$2} END {print "cells that threw:", s}'
echo "FOREGROUND_END $(date -Is)"
