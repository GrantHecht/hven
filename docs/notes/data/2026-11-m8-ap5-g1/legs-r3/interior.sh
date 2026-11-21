#!/usr/bin/env bash
# M8 AP-5 G1 (../DECLARATION.txt P4): copied from ../../2026-11-m8-dn3b/legs/interior.sh (whose provenance chain is in that
# file) and changed in two ways: arm A's CSV passes through the DECLARED COLUMN MAP (stop_reason_map.py) before the
# unmoved comparison, and the promoted baseline is G1's own re-derived file. Three comparisons, all
# scripts/compare_replay.py EXACT:
#   unmoved   -- map(arm A) vs arm B on the 27 replay cells at the defaults;
#   promoted  -- arm B under the promoted baseline's own invocation vs
#                bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv;
#   derived   -- map(bench/baselines/2026-11-m8-dn3-flip-ipm-leg/interior_baseline.csv) vs the G1 baseline (the file is
#                what the declared map makes of its frozen predecessor, the DN-3 flip's file, and nothing else).
# usage: interior.sh <evidence-dir> <expected arm B sha256>   Run as: flock /tmp/box-build.lock bash interior.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-afb4015ceb50aa1d1
EV=${1:?evidence dir}
EXPECT=${2:?expected arm B sha256}
MAP=$WT/docs/notes/data/2026-11-m8-ap5-g1/legs-r3/stop_reason_map.py
PROMOTED=$WT/bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv
FROZEN=$WT/bench/baselines/2026-11-m8-dn3-flip-ipm-leg/interior_baseline.csv
A=$WT/build-g3/armAbin/hven_sqp_corpus
B=$WT/build-g3/armBbin/hven_sqp_corpus
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
if [ ! -f "$B" ]; then mkdir -p "$(dirname "$B")"; cp -p "$WT/build-g3/rel/bench/hven_sqp_corpus" "$B" || exit 1; fi
GOT=$(sha256sum "$B" | cut -d' ' -f1)
sha256sum "$A" "$B" "$PROMOTED" "$FROZEN" "$MAP"
[ "$GOT" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
unset HVEN_LEG_PRE_B7_INFINITE_BOUND HVEN_LEG_IPM_PRE_DN3_DEFAULT
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
BASE_CELLS=$(grep -m1 '^# invocation:' "$PROMOTED" | sed -E 's/.*--cells ([^ ]+).*/\1/')
echo "baseline cells: $BASE_CELLS"
worst=0
"$A" --engine interior --cells "$CELLS" --csv "$EV/interior-A.csv" > "$EV/interior-run-A.txt" 2>&1
rc=$?; echo "arm A rc=$rc"; [ $rc -gt $worst ] && worst=$rc
"$B" --engine interior --cells "$CELLS" --csv "$EV/interior-B.csv" > "$EV/interior-run-B.txt" 2>&1
rc=$?; echo "arm B rc=$rc"; [ $rc -gt $worst ] && worst=$rc
"$B" --engine interior --cells "$BASE_CELLS" --csv "$EV/interior-B-baseline.csv" > "$EV/interior-run-B-baseline.txt" 2>&1
rc=$?; echo "arm B (baseline invocation) rc=$rc"; [ $rc -gt $worst ] && worst=$rc
python3 "$MAP" "$EV/interior-A.csv" "$EV/interior-A-mapped.csv" > "$EV/interior-map-A.txt" 2>&1
rc=$?; cat "$EV/interior-map-A.txt"; echo "map arm A rc=$rc"; [ $rc -gt $worst ] && worst=$rc
python3 "$MAP" "$FROZEN" "$EV/flip-baseline-mapped.csv" > "$EV/interior-map-flip.txt" 2>&1
rc=$?; cat "$EV/interior-map-flip.txt"; echo "map flip baseline rc=$rc"; [ $rc -gt $worst ] && worst=$rc
python3 scripts/compare_replay.py "$EV/interior-A-mapped.csv" "$EV/interior-B.csv" "interior-unmoved" > "$EV/interior-comparison-unmoved.txt" 2>&1
rc=$?; tail -1 "$EV/interior-comparison-unmoved.txt"; echo "compare unmoved rc=$rc"; [ $rc -gt $worst ] && worst=$rc
python3 scripts/compare_replay.py "$PROMOTED" "$EV/interior-B-baseline.csv" "interior-promoted" > "$EV/interior-comparison-promoted.txt" 2>&1
rc=$?; tail -1 "$EV/interior-comparison-promoted.txt"; echo "compare promoted rc=$rc"; [ $rc -gt $worst ] && worst=$rc
python3 scripts/compare_replay.py "$EV/flip-baseline-mapped.csv" "$PROMOTED" "interior-derived" > "$EV/interior-comparison-derived.txt" 2>&1
rc=$?; tail -1 "$EV/interior-comparison-derived.txt"; echo "compare derived rc=$rc"; [ $rc -gt $worst ] && worst=$rc
# The raw (unmapped) A/B difference, reported, never a criterion: it is the declared column map's footprint.
python3 scripts/compare_replay.py "$EV/interior-A.csv" "$EV/interior-B.csv" "interior-raw" > "$EV/interior-comparison-raw-REPORTED.txt" 2>&1
echo "raw (reported) rc=$? $(tail -1 "$EV/interior-comparison-raw-REPORTED.txt")"
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
