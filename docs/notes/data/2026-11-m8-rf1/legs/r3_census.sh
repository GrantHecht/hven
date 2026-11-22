#!/usr/bin/env bash
# RF-1 gate A+B2 leg W (r3): the walk census of one arm (Cr3 or ABr3) in one box lane, through the lane copy of the census
# runner (4-wide T1/T3, T2 solo within the lane), tiered from the frozen baseline of record; then, once both arms'
# merged CSVs exist, C vs AB2 by the comparator of record. usage: ab2_census.sh <arm>
# Run under: ~/.local/bin/box-lane -- bash ab2_census.sh <arm>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1
D=$WT/docs/notes/data/2026-11-m8-rf1; L=$D/legs; E=$D/gate-AB2-r3/census
ARM=${1:?arm}
echo "FOREGROUND_START $(date -Is) lane=$BOX_LANE phys=$BOX_LANE_PHYS arm=$ARM"
BIN=$WT/.scratch/bin/$ARM/hven_sqp_corpus
[ "$(sha256sum "$BIN" | cut -d' ' -f1)" = "$(grep -F "corpus $ARM " $D/BINARIES.txt | awk '{print $NF}')" ] || { echo SHA_MISMATCH; exit 4; }
mkdir -p $E
BASE=$WT/bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv
bash $L/run_walk_census_lane.sh --binary "$BIN" --out $E/$ARM --baseline $BASE --compare $WT/scripts/census_compare.py \
  --t1-width 4 --t3-width 4 > $E/census-$ARM.log 2>&1; echo "census $ARM rc=$?"
tail -4 $E/census-$ARM.log
if [ -s $E/Cr3/walk_census.csv ] && [ -s $E/ABr3/walk_census.csv ]; then
  python3 $WT/scripts/census_compare.py $E/Cr3/walk_census.csv $E/ABr3/walk_census.csv > $E/Cr3-vs-ABr3.txt 2>&1
  echo "compare rc=$?"; tail -3 $E/Cr3-vs-ABr3.txt
fi
echo "FOREGROUND_END $(date -Is)"
