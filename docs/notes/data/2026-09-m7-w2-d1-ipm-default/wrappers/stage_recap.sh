#!/usr/bin/env bash
# The interior leg re-captured at the BASELINE'S OWN CELL SCOPE (the ten F7
# bound cells plus hs071_x1_fixed) rather than `--cells all`, so the declared
# re-derivation carries the default change and nothing else. `--cells all`
# returns 14 more cells than it did when the baseline was taken -- the F7
# corpus's `path` family dual-binds now -- and that growth is a separate drift
# that does not belong in this event's diff.
set -uo pipefail
R=/home/ghecht/Projects/hven
S=$R/.scratch/m7w2d1
BIN=$S/rel/bench/hven_sqp_corpus
BASE=$R/bench/baselines/2026-09-t8-ipm-leg/interior_baseline.csv
D=$S/recap
mkdir -p "$D"
CELLS=f7_n1000_bound_neutral,f7_n1000_bound_physics,f7_n2000_bound_neutral,f7_n2000_bound_physics,f7_n5000_bound_neutral,f7_n5000_bound_physics,f7_n10000_bound_neutral,f7_n10000_bound_physics,f7_n20000_bound_neutral,f7_n20000_bound_physics,hs071_x1_fixed
echo "=== HEAD $(git -C "$R" rev-parse --short HEAD)  dirty-files=$(git -C "$R" status --porcelain | wc -l) ==="
echo "binary sha256: $(sha256sum "$BIN" | cut -d' ' -f1)"
worst=0
for arm in pinned shipped; do
  for n in 1 2; do
    if [ "$arm" = pinned ]; then export HVEN_LEG_IPM_PRE_D1_DEFAULTS=1; else unset HVEN_LEG_IPM_PRE_D1_DEFAULTS; fi
    MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 "$BIN" --engine interior --cells "$CELLS" \
      --csv "$D/$arm-$n.csv" > "$S/logs/recap-$arm-$n.out" 2>&1
    rc=$?; worst=$((worst|rc)); echo "$arm capture $n rc=$rc rows=$(grep -vc '^#' "$D/$arm-$n.csv")"
  done
done
unset HVEN_LEG_IPM_PRE_D1_DEFAULTS
echo "=== THE OFF PROOF ==="
for n in 1 2; do
  python3 -B "$R/scripts/compare_replay.py" "$BASE" "$D/pinned-$n.csv" "COMMITTED baseline vs PRE-EVENT-PINNED capture $n"
  rc=$?; worst=$((worst|rc)); echo "  CMP_RC=$rc"
done
grep -v '^#' "$BASE" | cut -d, -f1-30 > "$D/base_nowall.csv"
for n in 1 2; do
  grep -v '^#' "$D/pinned-$n.csv" | cut -d, -f1-30 > "$D/p${n}_nowall.csv"
  if cmp -s "$D/base_nowall.csv" "$D/p${n}_nowall.csv"; then
    echo "pinned capture $n: BYTE-IDENTICAL to the committed baseline outside wall_s ($(wc -l < "$D/base_nowall.csv") lines, header + 43 rows)"
  else echo "pinned capture $n: BYTE DIFFERENCE outside wall_s"; diff "$D/base_nowall.csv" "$D/p${n}_nowall.csv" | head -20; worst=1; fi
done
echo "=== THE DECLARED RE-DERIVATION (differences EXPECTED) ==="
python3 -B "$R/scripts/compare_replay.py" "$BASE" "$D/shipped-1.csv" "COMMITTED baseline vs SHIPPED-DEFAULT capture 1"
echo "  CMP_RC=$?  (non-zero IS the event)"
for arm in pinned shipped; do
  python3 -B "$R/scripts/compare_replay.py" "$D/$arm-1.csv" "$D/$arm-2.csv" "$arm capture 1 vs 2 (determinism)"
  rc=$?; worst=$((worst|rc)); echo "  CMP_RC=$rc"
done
echo "RECAP_WORST_RC=$worst"
exit $worst
