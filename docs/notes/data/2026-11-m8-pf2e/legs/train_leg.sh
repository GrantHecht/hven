#!/usr/bin/env bash
# PF-2e P2 (../DECLARATION-PF-2e.md section 4): TRAIN 238 x D/K of arms L and E co-run, one process per pinned physical
# core of the lane (the first two of $BOX_LANE_PHYS), MKL_NUM_THREADS=1, CAP 300 s, stat mode; then the read-out.
# Run under: ~/.local/bin/box-lane -- bash train_leg.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf2e
D0=$WT/docs/notes/data/2026-11-m8-pf2e; D=$D0/gate/train; L=$D0/legs
P2D=/home/ghecht/Projects/hven/.claude/worktrees/pf2d
BIN_L=$P2D/.scratch/bin/L/hven_bench_cell; BIN_E=$WT/.scratch/bin/E/hven_bench_cell
echo "FOREGROUND_START $(date -Is) lane=$BOX_LANE phys=$BOX_LANE_PHYS"
sha() { grep -F "$1" $D0/BINARIES.txt | awk '{print $NF}'; }
[ "$(sha256sum $BIN_L | cut -d' ' -f1)" = "$(sha 'lane L')" ] || { echo SHA_MISMATCH_L; exit 4; }
[ "$(sha256sum $BIN_E | cut -d' ' -f1)" = "$(sha 'lane E')" ] || { echo SHA_MISMATCH_E; exit 4; }
mkdir -p $D/L $D/E
read -r p1 p2 _ <<< "$BOX_LANE_PHYS"
PIN_CORE=$p1 RPIN_CORE=$p1 bash $L/train_enum.sh $D/L $BIN_L "$(sha 'lane L')" > $D/enum-L.out 2>&1 & a=$!
PIN_CORE=$p2 RPIN_CORE=$p2 bash $L/train_enum.sh $D/E $BIN_E "$(sha 'lane E')" > $D/enum-E.out 2>&1 & b=$!
rc=0; wait $a || rc=1; wait $b || rc=1
echo "enum rc=$rc $(date -Is)"
python3 $L/path_quality.py $D/E $D/L $WT/docs/notes/data/2026-11-m8-dn3 > $D/path-E-vs-L.txt 2>&1
P=$P2D/docs/notes/data/2026-11-m8-pf2d/gate/train
for X in F G; do python3 $L/path_quality.py $P/$X $P/L $WT/docs/notes/data/2026-11-m8-dn3 > $D/path-pf2d$X-vs-pf2dL.txt 2>&1; done
python3 $L/path_quality.py $D/L $P/L $WT/docs/notes/data/2026-11-m8-dn3 > $D/path-L-retake-vs-pf2dL.txt 2>&1
grep -E "^(SUMMARY|INSTR)" $D/path-*.txt
grep -q "status movers 0;" $D/path-E-vs-L.txt && grep -q "missing/capped 0$" $D/path-E-vs-L.txt && [ $rc -eq 0 ] \
  && echo P2_AS_DECLARED || echo P2_CRITERION_NOT_MET
echo "FOREGROUND_END $(date -Is)"
