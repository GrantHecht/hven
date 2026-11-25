#!/usr/bin/env bash
# Leg N1 (DECLARATION-QPC section 6): corpus replay R vs H (27 cells x walk/ssn/ipm; base x1, head x3), compare_replay.py
# byte-exact. The replay CSV does not carry the two QPC counters (bench_corpus.cpp prints a fixed column set, and adding
# columns would break the byte-exact comparison against the base binary), so the H binary writes them to a SIDE FILE per
# mode (bench/corpus_cells.h record_qpc_side, HVEN_BENCH_QPC_SIDE; replay.sh sets it; pre-freeze review S10): a
# base-vs-head differing cell is a DECLARED MOVER only if its OWN side line (repeat B1) carries a non-zero
# converged_refinement_arms or certificate_gate_refusals; a differing cell whose side line has both 0, or has no side
# line, STOPS. The base-vs-head-repeat (B1..B3) comparisons stay byte-exact, and the three repeats' side files must agree.
# usage: legN1.sh <head worktree> <base worktree> <out dir>    Run under: setsid box-lane -- bash legN1.sh ...
set -o pipefail
HW=$(realpath "$1"); BW=$(realpath "$2"); OUT=$(realpath -m "$3"); mkdir -p "$OUT"
HERE=$(cd "$(dirname "$0")" && pwd)
export CCACHE_DISABLE=1
echo "LEG N1 start $(date -Is) head=$(git -C $HW rev-parse HEAD) base=$(git -C $BW rev-parse HEAD)"
# build the base corpus binary with the head's Release configuration
if [ ! -f "$BW/.scratch/build-rel/build.ninja" ]; then
  cmake -S "$BW" -B "$BW/.scratch/build-rel" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=OFF > "$OUT/base-configure.log" 2>&1 || { echo BASE_CONFIGURE_FAIL; exit 1; }
fi
ninja -C "$BW/.scratch/build-rel" -j${BOX_LANE_JOBS:-4} hven_sqp_corpus > "$OUT/base-build.log" 2>&1 || { echo BASE_BUILD_FAIL; exit 1; }
ninja -C "$HW/.scratch/build-rel" -j${BOX_LANE_JOBS:-4} hven_sqp_corpus > "$OUT/head-build.log" 2>&1 || { echo HEAD_BUILD_FAIL; exit 1; }
worst=0
bash "$HERE/replay.sh" "$BW" .scratch/build-rel "$OUT" A > "$OUT/replay-A.log" 2>&1; rc=$?; echo "replay A rc=$rc"; [ $rc -gt $worst ] && worst=$rc
for i in 1 2 3; do
  bash "$HERE/replay.sh" "$HW" .scratch/build-rel "$OUT" B$i > "$OUT/replay-B$i.log" 2>&1; rc=$?; echo "replay B$i rc=$rc"; [ $rc -gt $worst ] && worst=$rc
done
for M in walk ssn ipm; do for i in 1 2 3; do
  python3 "$HW/scripts/compare_replay.py" "$OUT/A-$M.csv" "$OUT/B$i-$M.csv" "N1-$M-B$i" > "$OUT/cmp-$M-B$i.txt" 2>&1; rc=$?
  echo "compare $M B$i rc=$rc: $(tail -1 $OUT/cmp-$M-B$i.txt)"; [ $rc -gt $worst ] && worst=$rc
done; done
for M in walk ssn ipm; do
  for i in 2 3; do cmp -s "$OUT/B1-$M.qpc.csv" "$OUT/B$i-$M.qpc.csv" || { echo "SIDE_FILES_DIFFER $M B$i"; worst=1; }; done
  echo "side $M lines=$(wc -l < "$OUT/B1-$M.qpc.csv" 2>/dev/null || echo 0) nonzero=$(awk -F, '$2>0||$3>0' "$OUT/B1-$M.qpc.csv" 2>/dev/null | wc -l)"
done
echo "N1 worst=$worst (mover attribution: a compare_replay difference is a declared mover iff its side line is non-zero)"
echo "LEG N1 end $(date -Is)"; exit $worst
