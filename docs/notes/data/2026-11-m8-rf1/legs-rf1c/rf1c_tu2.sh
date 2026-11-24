#!/usr/bin/env bash
# RF-1c leg U: tu2_run.sh on arms R and H, co-run on the four pinned physical cores of one box lane (R cores 1-2, H cores 3-4; counters only,
# MKL_NUM_THREADS=1; CLAUDE.md section 7), then tu2_report.py. Run under: ~/.local/bin/box-lane -- bash rf1c_tu2.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1c
R=/home/ghecht/Projects/hven/.scratch/rf1c-build-legs
D=$WT/docs/notes/data/2026-11-m8-rf1; L=$D/legs-rf1c; E=$D/gate-RF-1c/tu2
echo "FOREGROUND_START $(date -Is) lane=$BOX_LANE phys=$BOX_LANE_PHYS"
read -r p1 p2 p3 p4 _ <<< "$BOX_LANE_PHYS"
want() { grep -F "$1" $D/BINARIES.txt | tail -1 | awk '{print $NF}'; }
for t in R H; do
  [ "$(sha256sum $R/bin/tu2-$t/hven_bench_cell | cut -d' ' -f1)" = "$(want "RF1C tu2 $t ")" ] || { echo "SHA_MISMATCH tu2 $t"; exit 4; }
done
mkdir -p $E
bash $L/tu2_run.sh R $E/R $p1 $p2 > $E/run-R.out 2>&1 & a=$!
bash $L/tu2_run.sh H $E/H $p3 $p4 > $E/run-H.out 2>&1 & b=$!
wait $a; echo "tu2 R rc=$?"; wait $b; echo "tu2 H rc=$?"
python3 $L/tu2_report.py $E/H $E/R > $E/report.txt 2>&1; echo "report rc=$?"; cat $E/report.txt
echo "FOREGROUND_END $(date -Is)"
