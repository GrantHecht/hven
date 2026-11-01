#!/usr/bin/env bash
# THE STAMPED LEGS, at the committed clean HEAD, on the Release build the
# previous leg configured from empty.
#
#  (1) THE OFF PROOF (b): the top-level interior-point leg under the declared
#      pre-event default pin -- it must come back byte-identical to the
#      COMMITTED 43-row baseline outside wall_s.
#  (2) THE DECLARED RE-DERIVATION: the SAME leg at the shipped default. Its CSV
#      becomes the new committed baseline.
#  (3) THE CONTROL: the U0 27-cell three-arm replay, which is SQP-driver only
#      (all three arms, `ipm` included, are QP-engine arms) and must therefore
#      still report 27 cells / 75 columns / 0 differences against the M6 close
#      pin. That is the "SQP lane untouched" evidence.
set -uo pipefail
R=/home/ghecht/Projects/hven
S=$R/.scratch/m7w2d1
BIN=$S/rel/bench/hven_sqp_corpus
BASE=$R/bench/baselines/2026-09-t8-ipm-leg/interior_baseline.csv
D=$S/legs
mkdir -p "$D" "$S/logs"
worst=0
echo "=== HEAD $(git -C "$R" rev-parse --short HEAD)  dirty-files=$(git -C "$R" status --porcelain | wc -l) ==="
echo "corpus binary sha256: $(sha256sum "$BIN" | cut -d' ' -f1)"
echo "libhven.a    sha256: $(sha256sum "$S/rel/libhven.a" 2>/dev/null | cut -d' ' -f1)"

echo
echo "=== (1) INTERIOR LEG under the PRE-EVENT DEFAULT PIN (twice, for determinism) ==="
for n in 1 2; do
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 HVEN_LEG_IPM_PRE_D1_DEFAULTS=1 taskset -c 2 \
    "$BIN" --engine interior --cells all --csv "$D/interior-pinned-$n.csv" \
    > "$S/logs/interior-pinned-$n.out" 2>&1
  rc=$?; worst=$((worst | rc)); echo "pinned capture $n rc=$rc  rows=$(grep -vc '^#' "$D/interior-pinned-$n.csv" 2>/dev/null)"
done
echo "--- the pinned stamp line the leg wrote ---"
grep -m1 'acceptance_strategy=' "$D/interior-pinned-1.csv"

echo
echo "=== (2) INTERIOR LEG at the SHIPPED DEFAULT (twice, for determinism) ==="
for n in 1 2; do
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
    "$BIN" --engine interior --cells all --csv "$D/interior-shipped-$n.csv" \
    > "$S/logs/interior-shipped-$n.out" 2>&1
  rc=$?; worst=$((worst | rc)); echo "shipped capture $n rc=$rc  rows=$(grep -vc '^#' "$D/interior-shipped-$n.csv" 2>/dev/null)"
done
echo "--- the shipped stamp line the leg wrote ---"
grep -m1 'acceptance_strategy=' "$D/interior-shipped-1.csv"

echo
echo "=== THE OFF PROOF: pinned capture vs the COMMITTED baseline ==="
for n in 1 2; do
  python3 -B "$R/scripts/compare_replay.py" "$BASE" "$D/interior-pinned-$n.csv" \
      "COMMITTED interior baseline vs W2 D1 PRE-EVENT-PINNED capture $n"
  rc=$?; worst=$((worst | rc)); echo "  CMP_RC=$rc"
done
echo "--- byte claim outside wall_s (columns 1-30 of 31) ---"
grep -v '^#' "$BASE" | cut -d, -f1-30 > "$D/base_nowall.csv"
for n in 1 2; do
  grep -v '^#' "$D/interior-pinned-$n.csv" | cut -d, -f1-30 > "$D/pinned${n}_nowall.csv"
  if cmp -s "$D/base_nowall.csv" "$D/pinned${n}_nowall.csv"; then
    echo "pinned capture $n: BYTE-IDENTICAL to the committed baseline outside wall_s ($(wc -l < "$D/base_nowall.csv") lines, header + 43 rows)"
  else
    echo "pinned capture $n: BYTE DIFFERENCE outside wall_s"; diff "$D/base_nowall.csv" "$D/pinned${n}_nowall.csv" | head -40; worst=1
  fi
done

echo
echo "=== THE DECLARED RE-DERIVATION: shipped capture vs the COMMITTED baseline (differences EXPECTED) ==="
python3 -B "$R/scripts/compare_replay.py" "$BASE" "$D/interior-shipped-1.csv" \
    "COMMITTED interior baseline vs W2 D1 SHIPPED-DEFAULT capture 1"
echo "  CMP_RC=$?  (non-zero is the event; the gate is that the two shipped captures agree)"
python3 -B "$R/scripts/compare_replay.py" "$D/interior-shipped-1.csv" "$D/interior-shipped-2.csv" \
    "SHIPPED capture 1 vs SHIPPED capture 2 (determinism)"
rc=$?; worst=$((worst | rc)); echo "  CMP_RC=$rc"
python3 -B "$R/scripts/compare_replay.py" "$D/interior-pinned-1.csv" "$D/interior-pinned-2.csv" \
    "PINNED capture 1 vs PINNED capture 2 (determinism)"
rc=$?; worst=$((worst | rc)); echo "  CMP_RC=$rc"

echo
echo "=== (3) THE CONTROL: U0 27-cell three-arm replay vs the M6 close pin ==="
CELLS="$(grep -v '^#' "$R/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt" | tr -d '\n')"
for eng in walk ssn ipm; do
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
    "$BIN" --engine "$eng" --cells "$CELLS" --csv "$D/head-$eng.csv" \
    > "$S/logs/replay-$eng.out" 2>&1
  rc=$?; worst=$((worst | rc)); echo "ran head-$eng rc=$rc"
done
for eng in walk ssn ipm; do
  python3 -B "$R/scripts/compare_replay.py" \
    "$R/docs/notes/data/2026-09-m6-close-gate/replay/head-$eng.csv" "$D/head-$eng.csv" \
    "M6 CLOSE HEAD-$eng vs M7 W2 D1 HEAD-$eng"
  rc=$?; worst=$((worst | rc)); echo "  CMP_RC=$rc"
done
echo "LEGS_WORST_RC=$worst"
exit $worst
