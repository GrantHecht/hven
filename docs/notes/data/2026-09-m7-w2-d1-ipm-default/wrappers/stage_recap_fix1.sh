#!/usr/bin/env bash
# M7 W2 D1 fix1, MEDIUM-2: a CLEAN re-take of the baseline capture leg, with
# the wrapper stdout kept verbatim this time. Removes the build dir,
# re-configures from the clean tree at HEAD, builds the corpus binary, then
# runs both arms (pre-event-pinned, shipped) TWICE at the baseline's 11-cell
# scope.
#
# Two references, not one, because the COMMITTED interior_baseline.csv in the
# working tree is now the SHIPPED-default content (2a75cecd re-derived it):
#   OLD_BASE  -- the baseline's content BEFORE 2a75cecd (commit 806ca757),
#                which is what the PINNED arm must reproduce byte-identically.
#   CUR_BASE  -- the baseline's content as committed right now (the working
#                tree file), which is what the SHIPPED arm must reproduce
#                byte-identically.
set -o pipefail
R=/home/ghecht/Projects/hven
S=$R/.scratch/m7w2d1
CUR_BASE=$R/bench/baselines/2026-09-t8-ipm-leg/interior_baseline.csv
PREV_LIBSHA=a2840d3a5eaff83cc1a4a23beecce2661edba7a466cc98324e9b3540fc5d92a1
D=$S/recap-fix1
mkdir -p "$D"

echo "=== HEAD $(git -C "$R" rev-parse --short HEAD)  dirty-files=$(git -C "$R" status --porcelain | wc -l) ==="

rm -rf "$S/rel"
cmake -S "$R" -B "$S/rel" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON > "$S/logs/recap-fix1-configure.log" 2>&1
rc=$?
echo "CONFIGURE_RC=$rc"
if [ $rc -ne 0 ]; then echo "CONFIGURE FAILED"; tail -40 "$S/logs/recap-fix1-configure.log"; exit $rc; fi

cmake --build "$S/rel" --target hven_sqp_corpus -j "$(nproc)" > "$S/logs/recap-fix1-build.log" 2>&1
rc=$?
echo "BUILD_RC=$rc"
if [ $rc -ne 0 ]; then echo "BUILD FAILED"; tail -60 "$S/logs/recap-fix1-build.log"; exit $rc; fi

BIN=$S/rel/bench/hven_sqp_corpus
LIBSHA=$(sha256sum "$S/rel/libhven.a" | cut -d' ' -f1)
BINSHA=$(sha256sum "$BIN" | cut -d' ' -f1)
echo "libhven.a sha256: $LIBSHA"
echo "corpus binary sha256: $BINSHA"
if [ "$LIBSHA" = "$PREV_LIBSHA" ]; then
  echo "libhven.a sha256 MATCHES PROVENANCE.txt's record for 2a75cecd's build"
else
  echo "libhven.a sha256 DOES NOT MATCH PROVENANCE.txt's record for 2a75cecd's build -- STOP"
fi

CELLS=f7_n1000_bound_neutral,f7_n1000_bound_physics,f7_n2000_bound_neutral,f7_n2000_bound_physics,f7_n5000_bound_neutral,f7_n5000_bound_physics,f7_n10000_bound_neutral,f7_n10000_bound_physics,f7_n20000_bound_neutral,f7_n20000_bound_physics,hs071_x1_fixed
worst=0
for arm in pinned shipped; do
  for n in 1 2; do
    if [ "$arm" = pinned ]; then export HVEN_LEG_IPM_PRE_D1_DEFAULTS=1; else unset HVEN_LEG_IPM_PRE_D1_DEFAULTS; fi
    MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 "$BIN" --engine interior --cells "$CELLS" \
      --csv "$D/$arm-$n.csv" > "$S/logs/recap-fix1-$arm-$n.out" 2>&1
    rc=$?; worst=$((worst|rc)); echo "$arm capture $n rc=$rc rows=$(grep -vc '^#' "$D/$arm-$n.csv")"
  done
done
unset HVEN_LEG_IPM_PRE_D1_DEFAULTS

git -C "$R" show 806ca757:bench/baselines/2026-09-t8-ipm-leg/interior_baseline.csv > "$D/old_base_full.csv"
grep -v '^#' "$D/old_base_full.csv" | cut -d, -f1-30 > "$D/old_base_nowall.csv"
grep -v '^#' "$CUR_BASE" | cut -d, -f1-30 > "$D/cur_base_nowall.csv"

echo "=== THE OFF PROOF (pinned arms vs the PRE-EVENT baseline, commit 806ca757) ==="
for n in 1 2; do
  python3 -B "$R/scripts/compare_replay.py" "$D/old_base_full.csv" "$D/pinned-$n.csv" "PRE-EVENT baseline (806ca757) vs PRE-EVENT-PINNED capture $n"
  rc=$?; worst=$((worst|rc)); echo "  CMP_RC=$rc"
  grep -v '^#' "$D/pinned-$n.csv" | cut -d, -f1-30 > "$D/p${n}_nowall.csv"
  if cmp -s "$D/old_base_nowall.csv" "$D/p${n}_nowall.csv"; then
    echo "pinned capture $n: BYTE-IDENTICAL to the pre-event baseline outside wall_s ($(wc -l < "$D/old_base_nowall.csv") lines, header + 43 rows)"
  else
    echo "pinned capture $n: BYTE DIFFERENCE outside wall_s -- STOP"; diff "$D/old_base_nowall.csv" "$D/p${n}_nowall.csv" | head -20; worst=1
  fi
done

echo "=== THE RE-DERIVATION HOLDS (shipped arms vs the CURRENTLY COMMITTED interior_baseline.csv) ==="
for n in 1 2; do
  python3 -B "$R/scripts/compare_replay.py" "$CUR_BASE" "$D/shipped-$n.csv" "CURRENTLY COMMITTED baseline vs SHIPPED-DEFAULT capture $n"
  rc=$?; worst=$((worst|rc)); echo "  CMP_RC=$rc"
  grep -v '^#' "$D/shipped-$n.csv" | cut -d, -f1-30 > "$D/s${n}_nowall.csv"
  if cmp -s "$D/cur_base_nowall.csv" "$D/s${n}_nowall.csv"; then
    echo "shipped capture $n: BYTE-IDENTICAL to the committed baseline outside wall_s ($(wc -l < "$D/cur_base_nowall.csv") lines, header + 43 rows)"
  else
    echo "shipped capture $n: BYTE DIFFERENCE outside wall_s -- STOP, the committed baseline is then in question"; diff "$D/cur_base_nowall.csv" "$D/s${n}_nowall.csv" | head -20; worst=1
  fi
done

echo "=== DETERMINISM, each arm ==="
for arm in pinned shipped; do
  python3 -B "$R/scripts/compare_replay.py" "$D/$arm-1.csv" "$D/$arm-2.csv" "$arm capture 1 vs 2 (determinism)"
  rc=$?; worst=$((worst|rc)); echo "  CMP_RC=$rc"
done

echo "RECAP_FIX1_WORST_RC=$worst"
exit $worst
