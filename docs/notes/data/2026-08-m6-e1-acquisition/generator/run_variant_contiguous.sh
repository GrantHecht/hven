#!/usr/bin/env bash
# run_variant_contiguous.sh — FOLLOW-ON variant, added AFTER the pre-registered
# 20-cell sweep at the settler's request.
#
# It addresses one fidelity concern the implementer raised about the original
# taxonomy: those cells' active sets are a UNIFORM RANDOM SUBSET of inequality
# rows, while F7's own geometry produces a CONTIGUOUS junction window. Whether
# the two layouts are the same difficulty for a barrier method was not measured
# there, and is measured here.
#
# 9 cells: N = 20000 (nx = 1e5) only, crossed with the same three active
# fractions {1%, 10%, 30%} and the same three margin classes {1e-2, 1e-4, 1e-6}.
# Same construction, same budgets, same counting rules, same driver.
#
# THIS IS NOT PART OF THE PRE-REGISTERED TAXONOMY and is written to its own
# file. The original 20-cell CSV is not touched, not extended, and not merged
# into -- it was re-run unchanged under this same build and every non-timing
# column, residuals at 17 digits included, was bit-identical.
#
# DECLARED TERMS: identical to run_sweep.sh -- sequential one process at a
# time, MKL_NUM_THREADS=1 / OMP_NUM_THREADS=1, 120 s hard timeout per cell with
# a timeout recorded as an outcome, one retry on generation failure only,
# wall-clock informational.
#
# LICQ, which is the reason this variant needed its own verification pass: a
# contiguous block sits differently against the collocation staircase than a
# scattered one, so LICQ is re-tested rather than inherited. Three tiers:
#   * exact sparse-QR rank at N = 200, all 9 classes;
#   * exact sparse-QR rank at N = 2000 for all three fractions at one margin --
#     the margin class cannot affect LICQ (it only sets the INACTIVE rows'
#     slacks, which never enter [Ae; Ai_A]), so one margin covers the axis;
#   * exact sparse-QR rank at N = 5000, 30% active (16500 rows -- a quarter of
#     the swept cells' 66000, at the most LICQ-stressing fraction): the largest
#     exact check this experiment can afford, ~171 s;
#   * the cheap LDL^T certificate at the full N = 20000, on every swept cell,
#     inside the generator itself, where it also GATES the offset shift rule.
#
# The exact test is superlinear on this staircase (11-16 s at 6600 rows, 171 s
# at 16500, past a 300 s budget at 33000) and cannot be paid at 66000. Where
# both tests run they agree on every cell, which is what makes the cheap one
# credible at the one size where it stands alone.

set -uo pipefail
W="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CSV="$W/artifact/variant_contiguous.csv"
LAYOUT="$W/artifact/variant_contiguous_layout.csv"
VLOG="$W/artifact/VARIANT-CONTIGUOUS-VERIFICATION.log"
SLOG="$W/artifact/variant_contiguous_sweep.log"
DUMPS="$W/dumps/variant"
export MKL_NUM_THREADS=1
export OMP_NUM_THREADS=1

mkdir -p "$DUMPS" "$W/artifact"
rm -f "$CSV"
: > "$VLOG"
: > "$SLOG"
"$W/bridge/piqp_e1_driver" header "$CSV"
echo "id,layout,active_offset,block_first_row,block_last_row,block_size,mi" > "$LAYOUT"

{
    echo "# E1 follow-on variant: CONTIGUOUS active-set window -- verification log"
    echo "# date (UTC): $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "# host: $(hostname)   MKL_NUM_THREADS=1 OMP_NUM_THREADS=1"
    echo "#"
    echo "# Added AFTER the pre-registered 20-cell sweep, at the settler's request,"
    echo "# to address the scattered-vs-contiguous activity-layout concern."
    echo
    echo "== tier 1: all 9 classes at N=200 (n=1000, mi=200), EXACT LICQ rank included =="
    echo
} >> "$VLOG"

tier1_ok=1
idx=0
for af in 0.01 0.10 0.30; do
    for m in 1e-2 1e-4 1e-6; do
        idx=$((idx + 1))
        aftag=$(python3 -c "print(f\"{round(float('$af')*100):02d}\")")
        id="e1blk_verify_n200_af${aftag}_m${m}"
        E1_LICQ_MAX_ROWS=7000 "$W/bridge/e1_generate" cellblk "$id" 200 "$af" "$m" $((800000 + idx)) "$DUMPS/$id" >> "$VLOG" 2>&1
        [ $? -ne 0 ] && tier1_ok=0
        rm -f "$DUMPS/$id.qp" "$DUMPS/$id.sol"
    done
done
echo "tier 1 (N=200, 9 classes): $([ $tier1_ok -eq 1 ] && echo "ALL PASS" || echo "*** FAILED ***")" >> "$VLOG"

{
    echo
    echo "== tier 2: the three fractions at N=2000 (n=10000, mi=2000), EXACT LICQ rank =="
    echo "# One margin class (1e-4) covers the margin axis for THIS test: the margin sets"
    echo "# the INACTIVE rows' slacks only, and no inactive row enters [Ae; Ai_A]."
    echo "# A sparse QR at this size costs ~40 s per cell; that is the budget this tier"
    echo "# deliberately pays, and it is why it is not run at N = 20000."
    echo
} >> "$VLOG"

tier2_ok=1
for af in 0.01 0.10 0.30; do
    aftag=$(python3 -c "print(f\"{round(float('$af')*100):02d}\")")
    id="e1blk_verify_n2000_af${aftag}_m1e-4"
    t0=$(date +%s.%N)
    E1_LICQ_MAX_ROWS=7000 "$W/bridge/e1_generate" cellblk "$id" 2000 "$af" 1e-4 $((810000 + idx)) "$DUMPS/$id" >> "$VLOG" 2>&1
    [ $? -ne 0 ] && tier2_ok=0
    t1=$(date +%s.%N)
    echo "  (exact LICQ rank check elapsed $(python3 -c "print(f'{$t1-$t0:.1f}')") s)" >> "$VLOG"
    idx=$((idx + 1))
    rm -f "$DUMPS/$id.qp" "$DUMPS/$id.sol"
done
echo "tier 2 (N=2000, 3 fractions): $([ $tier2_ok -eq 1 ] && echo "ALL PASS" || echo "*** FAILED ***")" >> "$VLOG"

{
    echo
    echo "== tier 2b: N=5000, 30% active (16500 rows) -- the largest EXACT LICQ rank check =="
    echo "# this experiment can afford (~171 s). 30% is the most LICQ-stressing fraction,"
    echo "# and 16500 rows is a quarter of the swept cells' 66000."
    echo
} >> "$VLOG"

t0=$(date +%s.%N)
E1_LICQ_MAX_ROWS=20000 "$W/bridge/e1_generate" cellblk e1blk_verify_n5000_af30_m1e-4 5000 0.30 1e-4 820001 "$DUMPS/tier2b" >> "$VLOG" 2>&1
tier2b_rc=$?
t1=$(date +%s.%N)
echo "  (elapsed $(python3 -c "print(f'{$t1-$t0:.1f}')") s)" >> "$VLOG"
rm -f "$DUMPS/tier2b.qp" "$DUMPS/tier2b.sol"
echo "tier 2b (N=5000, 30% active, 16500 rows): $([ $tier2b_rc -eq 0 ] && echo "PASS" || echo "*** FAILED ***")" >> "$VLOG"

{
    echo
    echo "== tier 3: the 9 swept cells at N=20000, full KKT verification =="
    echo "# The exact LICQ rank test is not affordable at this size and is recorded as"
    echo "# SKIPPED; the cheap LDL^T certificate runs here and also gated the offset"
    echo "# shift rule during generation."
    echo
} >> "$VLOG"

run_one() {
    local id="$1" pre="$2" sol="$3" n="$4" mi="$5" af="$6" mc="$7"
    local before after rc t0 t1
    before=$(grep -vc '^#' "$CSV")
    echo "--- $id ---" >> "$SLOG"
    t0=$(date +%s.%N)
    timeout -k 5 120 "$W/bridge/piqp_e1_driver" run "$pre.qp" --sol "$sol" --out "$CSV" >> "$SLOG" 2>&1
    rc=$?
    t1=$(date +%s.%N)
    after=$(grep -vc '^#' "$CSV")
    echo "exit=$rc  elapsed_including_parse_s=$(python3 -c "print(f'{$t1-$t0:.3f}')")" >> "$SLOG"
    if [ "$after" -eq "$before" ]; then
        local status="error_rc$rc"
        if [ $rc -eq 124 ] || [ $rc -eq 137 ]; then status="timeout"; fi
        echo "$id,$n,$((n*3/5)),$mi,constructed,$af,$mc,$status,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,1" >> "$CSV"
        echo "RECORDED OUTCOME: $status (no solver row; counters -1)" >> "$SLOG"
    fi
}

echo "# E1 contiguous-variant sweep log, started $(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$SLOG"
sidx=0
for af in 0.01 0.10 0.30; do
    for mc in 1e-2 1e-4 1e-6; do
        sidx=$((sidx + 1))
        aftag=$(python3 -c "print(f\"{round(float('$af')*100):02d}\")")
        id="e1blk_f7_n20000_af${aftag}_m${mc}"
        pre="$DUMPS/$id"
        seed=$((20260827 + sidx))
        gen_out=$("$W/bridge/e1_generate" cellblk "$id" 20000 "$af" "$mc" "$seed" "$pre" 2>&1)
        grc=$?
        if [ $grc -eq 2 ]; then
            # INFEASIBLE BY CONSTRUCTION: no offset admitted LICQ. Recorded, never faked.
            echo "$gen_out" >> "$VLOG"
            echo "$id,100000,60000,20000,constructed,$af,$mc,infeasible_by_construction,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,1" >> "$CSV"
            echo "$id,contiguous,-1,-1,-1,-1,20000" >> "$LAYOUT"
            continue
        fi
        if [ $grc -ne 0 ]; then
            echo "$gen_out" >> "$VLOG"
            echo "GENERATION FAILED for $id (rc=$grc) -- one retry, per the declared budget" >> "$VLOG"
            gen_out=$("$W/bridge/e1_generate" cellblk "$id" 20000 "$af" "$mc" "$seed" "$pre" 2>&1)
            grc=$?
        fi
        echo "$gen_out" >> "$VLOG"
        echo >> "$VLOG"
        if [ $grc -ne 0 ]; then
            echo "$id,100000,60000,20000,constructed,$af,$mc,generation_failed,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,1" >> "$CSV"
            echo "$id,contiguous,-1,-1,-1,-1,20000" >> "$LAYOUT"
            continue
        fi
        off=$(echo "$gen_out" | sed -n 's/.*offset=\([0-9-]*\).*/\1/p' | head -1)
        blk=$(echo "$gen_out" | sed -n 's/.*|A|=\([0-9]*\).*/\1/p' | head -1)
        echo "$id,contiguous,$off,$off,$((off + blk - 1)),$blk,20000" >> "$LAYOUT"
        run_one "$id" "$pre" "$pre.sol" 100000 20000 "$af" "$mc"
        rm -f "$pre.qp" "$pre.sol"
    done
done
echo "# E1 contiguous-variant sweep finished $(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$SLOG"

# Provenance: the same stamp the pre-registered CSV carries, plus this file's
# own "added after the fact" declaration.
TMP="$(mktemp)"
{
    echo "# artifact/variant_contiguous.csv"
    echo "#"
    echo "# FOLLOW-ON VARIANT, NOT PART OF THE PRE-REGISTERED TAXONOMY. Added after the"
    echo "# 20-cell sweep in artifact/e1_cells.csv had already run, at the settler's"
    echo "# request, to address the scattered-vs-contiguous activity-layout concern the"
    echo "# implementer raised. Same construction, same budgets, same counting rules,"
    echo "# same driver binary; the ONLY difference from a taxonomy cell is that the"
    echo "# active set is one contiguous block of rows instead of a uniform random"
    echo "# subset. Schema is identical to e1_cells.csv so the two are directly"
    echo "# comparable; the block offsets are in variant_contiguous_layout.csv."
    echo "#"
    sed 's/^/# /' "$W/artifact/PROVENANCE.txt"
} > "$TMP"
cat "$CSV" >> "$TMP"
mv "$TMP" "$CSV"
chmod 644 "$CSV"

echo "variant rows written: $(( $(grep -vc '^#' "$CSV") - 1 ))"
