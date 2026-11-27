#!/usr/bin/env bash
# Leg M1R-r2 (DECLARATION-QPC-r2 r2-4: legM1R.sh + harness at e806548 + r2-3(a),(c)). Original header follows.
# Leg M1R (DECLARATION-QPC section 6; settler C581, pre-freeze re-review must 1): the gate's own reading at every
# would-be certified exit, at H, on TRAIN-238 member 0 and SENS-Q -- REPORTED, with the C581 STOP rule on column-local
# stationarity.
# The diagnostic print reaches H by the V-PDIV mechanism (section 5.2): a LOCAL branch made from the H commit carrying
# ONE env-gated DIAGNOSTIC-ONLY commit, `legs/m1r/m1r_diag.patch` (applied with `git am --committer-date-is-author-date`,
# so the diagnostic commit is a function of H and the patch) -- never on qpc, never pushed. It prints one `[m1r]` line per
# data-half verdict (HVEN_QPC_M1 set) and changes nothing else: the print reads the verdict's inputs after the verdict.
# Steps (one box-lane acquisition): (1) detached worktree at H, patch applied, the diagnostic sha printed; (2) the
# census recipe build of that commit (hven-bench `ens` scripts/census_build.sh: install + lane binary); (3) the SENS-Q
# grid built against that install (legs/build_grid.sh); (4) SENS-Q: the 67 e2-d4 dumps x {refactorize, border}, one
# process per cell, 900 s kill, HVEN_QPC_M1=1, stderr per cell (sensq/<algebra>/<dump>.err) and the grid CSV
# (sensq-<algebra>.csv: the independent readings incl. d_colstat); (5) TRAIN-238 member 0 (legs/m1r/run_m1r_train.py:
# the harness's cell list and per-cell argv, co-run one process per pinned physical core); (6) legs/m1r/m1r_summary.py
# -> SUMMARY.txt (verdict counts, the per-disjunct refusal histogram, column-local stationarity, the STOP-RULE line) and
# cmp_sensq.py vqp on both grid CSVs (colstat reported).
# usage: legM1R.sh <H sha> <hven-bench checkout at ens 27c02687> <work dir> <out dir>
# Run under: setsid -w ~/.local/bin/box-lane -- bash legM1R.sh ...
set -o pipefail
H=${1:?H sha}; HB=$(realpath "${2:?hven-bench}"); W=$(realpath -m "${3:?work dir}"); OUT=$(realpath -m "${4:?out dir}")
L=$(cd "$(dirname "$0")" && pwd)
REPO=$(git -C "$L" rev-parse --show-toplevel)
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 CCACHE_DISABLE=1
mkdir -p "$W" "$OUT"
echo "LEG M1R start $(date -Is) H=$H harness=$(git -C "$HB" rev-parse HEAD) lane=${BOX_LANE:-?} phys=[${BOX_LANE_PHYS:-}]"
[ "$(git -C "$HB" rev-parse HEAD)" = e80654847013a53f5d5676b03a495811e2cc2907 ] || { echo "STOP: harness not at e806548"; exit 2; }
for p in $(cat "$REPO/docs/notes/data/2026-11-m8-qpc/cells/decoded-required.txt"); do [ -f "$HB/.scratch/h3/decoded/$p/libpb.so" ] || { echo "STOP r2-3(a): decoded/$p/libpb.so missing"; exit 2; }; done
SRC=$W/src
if [ ! -d "$SRC" ]; then
  git -C "$REPO" worktree add --quiet --detach "$SRC" "$H" > /dev/null 2>&1 || exit 1
  git -C "$SRC" am --committer-date-is-author-date "$L/m1r/m1r_diag.patch" > /dev/null || { echo "STOP: the patch does not apply on H"; exit 2; }
fi
[ "$(git -C "$SRC" rev-parse HEAD~1)" = "$(git -C "$REPO" rev-parse "$H")" ] || { echo "STOP: $SRC is not H + one commit"; exit 2; }
D=$(git -C "$SRC" rev-parse HEAD)
echo "M1R diagnostic commit $D = H + legs/m1r/m1r_diag.patch (sha256 $(sha256sum "$L/m1r/m1r_diag.patch" | cut -d' ' -f1))"
git -C "$SRC" diff --stat HEAD~1 HEAD -- src include | tail -1
if [ ! -x "$W/lane/lanes/hven/hven_bench_cell" ]; then
  bash "$HB/scripts/census_build.sh" "$D" "$SRC" "$W/inst" "$W/lane" > "$W/build.log" 2>&1 || { echo "BUILD_FAIL"; tail -20 "$W/build.log"; exit 1; }
fi
LANE=$W/lane/lanes/hven/hven_bench_cell
echo "lane $(sha256sum "$LANE" | cut -d' ' -f1)"
bash "$L/build_grid.sh" "$W/inst/install" "$SRC" "$W/grid" > "$W/grid.sha" || { echo GRID_FAIL; exit 1; }
G=$W/grid/hven_qp_dump_grid
echo "grid $(cut -d' ' -f1 "$W/grid.sha")"
echo "== SENS-Q $(date -Is)"
for alg in refactorize border; do
  mkdir -p "$OUT/sensq/$alg"; rm -f "$OUT/sensq-$alg.csv"
  for f in "$SRC"/tests/qp/fixtures/e2-d4/*.txt; do
    b=$(basename "$f" .txt); [ "$b" = FORMAT ] && continue
    HVEN_QPC_M1=1 timeout 900 "$G" --file "$f" --algebra $alg --csv "$OUT/sensq-$alg.csv" > /dev/null 2> "$OUT/sensq/$alg/$b.err"
    rc=$?; [ $rc -ne 0 ] && echo "CELL $alg $b rc=$rc (124 = killed)"
  done
  echo "SENS-Q $alg rows=$(($(wc -l < "$OUT/sensq-$alg.csv") - 1))"
  python3 "$L/cmp_sensq.py" vqp "$OUT/sensq-$alg.csv" "$L/pinned_optima.tsv" > "$OUT/vqp-$alg.txt" 2>&1
  echo "V-QP(M1R build) $alg rc=$?: $(tail -1 "$OUT/vqp-$alg.txt")"
done
echo "== TRAIN-238 member 0 $(date -Is)"
rm -rf "$OUT/train"
python3 "$L/m1r/run_m1r_train.py" "$HB" "$LANE" "$OUT/train" || { echo "TRAIN_FAIL"; exit 1; }
n=$(ls "$OUT/train"/*.result 2>/dev/null | wc -l)
[ "$n" = 238 ] || { echo "STOP r2-3(c): $n TRAIN-238 cell results, not 238"; exit 2; }
bad=$(grep -L '^rc=0 ' "$OUT/train"/*.result | grep -vE '/[^/]*-hs0(25|33)-[^/]*\.result$' | head -3)
[ -z "$bad" ] || { echo "STOP r2-3(c): rc!=0 cells beyond hs025/hs033: $bad"; exit 2; }
echo "r2-3(c): 238 cell results; rc!=0: $(grep -L '^rc=0 ' "$OUT/train"/*.result | wc -l) (hs025/hs033 only)"
python3 "$L/m1r/m1r_summary.py" "$OUT" > "$OUT/SUMMARY.txt" 2>&1
grep -E "verdicts=|certified with colstat|STOP-RULE" "$OUT/SUMMARY.txt"
echo "LEG M1R end $(date -Is)"
