#!/usr/bin/env bash
# run_sweep.sh — E1 step 4: run the 20 cells.
#
# DECLARED TERMS (protocol section "Instrument"):
#   * SEQUENTIAL, one process at a time. No cell ever co-runs with another.
#   * SINGLE-THREADED: MKL_NUM_THREADS=1 and OMP_NUM_THREADS=1 are exported.
#     Neither is read by PIQP -- it links no MKL and its OpenMP pragmas compile
#     out because BUILD_WITH_OPENMP defaults OFF and build_piqp.sh never turns
#     it on; `ldd` on the driver shows no libgomp and no BLAS. They are set to
#     match the standing convention, and the ldd evidence is what actually
#     carries the single-thread claim.
#   * 120 s HARD TIMEOUT per cell, applied to the whole `run` invocation
#     (dump parse included -- a conservative reading of the budget, since parse
#     is bridge overhead and not the object being measured). A timeout is a
#     RECORDED OUTCOME: the row is written with status=timeout and -1 in every
#     counter column, never dropped and never estimated.
#   * ONE RETRY ON SETUP FAILURE only, i.e. on a failure to GENERATE the cell.
#     A solve that fails is recorded as it failed and is never retried.
#   * WALL-CLOCK IS INFORMATIONAL. wall_info_s is PIQP's own solve wall; no
#     wall claim is made from this artifact and the box is not serialized.
#   * CELL GENERATION IS OUTSIDE THE PER-CELL BUDGET: it constructs the cell,
#     it does not solve it. Generation time is logged separately.
#
# Cell taxonomy (protocol): nx in {2e4, 1e5} -> N in {4000, 20000} on F7's
# 5 variables per node; active fraction in {1%, 10%, 30%}; margin class in
# {1e-2, 1e-4, 1e-6}. 18 constructed cells + 2 equality-only anchors = 20.

set -uo pipefail
W="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CSV="$W/artifact/e1_cells.csv"
LOG="$W/logs/sweep.log"
GENLOG="$W/artifact/KKT-VERIFICATION-full-size.log"
DUMPS="$W/dumps/sweep"
export MKL_NUM_THREADS=1
export OMP_NUM_THREADS=1

mkdir -p "$DUMPS" "$W/logs" "$W/artifact"
rm -f "$CSV"
: > "$LOG"
: > "$GENLOG"
"$W/bridge/piqp_e1_driver" header "$CSV"

{
    echo "# E1 full-size cell KKT verification log"
    echo "# Every cell the sweep actually ran was checked, at its OWN size, by"
    echo "# direct KKT residual evaluation against the dump as written to disk."
    echo "# The LICQ rank test is affordable only at small n and is recorded as"
    echo "# SKIPPED here; run_verification.sh carries it at N = 200 per class."
    echo "# date (UTC): $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo
} >> "$GENLOG"

echo "# E1 sweep log, started $(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$LOG"

run_one() {
    local id="$1" pre="$2" sol="$3" n="$4" mi="$5" af="$6" mc="$7" kind="$8"
    local before after rc
    before=$(wc -l < "$CSV")
    echo "--- $id ---" >> "$LOG"
    local t0 t1
    t0=$(date +%s.%N)
    if [ -n "$sol" ]; then
        timeout -k 5 120 "$W/bridge/piqp_e1_driver" run "$pre.qp" --sol "$sol" --out "$CSV" >> "$LOG" 2>&1
    else
        timeout -k 5 120 "$W/bridge/piqp_e1_driver" run "$pre.qp" --out "$CSV" >> "$LOG" 2>&1
    fi
    rc=$?
    t1=$(date +%s.%N)
    after=$(wc -l < "$CSV")
    echo "exit=$rc  elapsed_including_parse_s=$(python3 -c "print(f'{$t1-$t0:.3f}')")" >> "$LOG"
    if [ "$after" -eq "$before" ]; then
        # No row was written: the process was killed (timeout) or died before
        # the append. Record the ACTUAL outcome; never estimate one.
        local status="error_rc$rc"
        if [ $rc -eq 124 ] || [ $rc -eq 137 ]; then status="timeout"; fi
        echo "$id,$n,$((n*3/5)),$mi,$kind,$af,$mc,$status,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,1" >> "$CSV"
        echo "RECORDED OUTCOME: $status (no solver row; counters -1)" >> "$LOG"
    fi
}

idx=0
for N in 4000 20000; do
    for af in 0.01 0.10 0.30; do
        for mc in 1e-2 1e-4 1e-6; do
            idx=$((idx + 1))
            aftag=$(python3 -c "print(f\"{round(float('$af')*100):02d}\")")
            id="e1_f7_n${N}_af${aftag}_m${mc}"
            pre="$DUMPS/$id"
            seed=$((20260826 + idx))
            gt0=$(date +%s.%N)
            "$W/bridge/e1_generate" cell "$id" "$N" "$af" "$mc" "$seed" "$pre" >> "$GENLOG" 2>&1
            grc=$?
            if [ $grc -ne 0 ]; then
                echo "GENERATION FAILED for $id (rc=$grc) -- one retry, per the declared budget" >> "$GENLOG"
                "$W/bridge/e1_generate" cell "$id" "$N" "$af" "$mc" "$seed" "$pre" >> "$GENLOG" 2>&1
                grc=$?
            fi
            gt1=$(date +%s.%N)
            echo "generation_s=$(python3 -c "print(f'{$gt1-$gt0:.3f}')") (outside the per-cell solve budget)" >> "$GENLOG"
            echo >> "$GENLOG"
            if [ $grc -ne 0 ]; then
                echo "$id,$((N*5)),$((N*3)),$N,constructed,$af,$mc,generation_failed,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,1" >> "$CSV"
                echo "--- $id: GENERATION FAILED, recorded as generation_failed ---" >> "$LOG"
                continue
            fi
            run_one "$id" "$pre" "$pre.sol" "$((N*5))" "$N" "$af" "$mc" constructed
            rm -f "$pre.qp" "$pre.sol"
        done
    done
done

# The two anchors: the ORACLE's own equality-only cells, re-run.
for win in bound path; do
    id="e1_anchor_f7_n20000_${win}_neutral"
    pre="$DUMPS/$id"
    "$W/bridge/e1_generate" anchor "$id" 20000 "$win" "$pre" >> "$GENLOG" 2>&1
    run_one "$id" "$pre" "" 100000 20000 0 -1 anchor
    rm -f "$pre.qp"
done

echo "# E1 sweep finished $(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$LOG"
bash "$W/bridge/stamp_provenance.sh" >> "$LOG" 2>&1
echo "rows written: $(( $(grep -vc "^#" "$CSV") - 1 ))"
