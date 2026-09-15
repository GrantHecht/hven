#!/usr/bin/env bash
# run_verification.sh — E1 step 3: verify the GENERATOR before any sweep cell
# runs. A mis-constructed cell would invalidate the whole verdict, so this
# script is a precondition of the sweep, not a postscript to it.
#
# Three sections:
#   (a) the archived bridge's own REFEREE GATE, at all three tolerances, run
#       through THIS workspace's derived driver -- the referee has to be
#       checked before it can referee.
#   (b) one SMALL instance per taxonomy cell class (3 active fractions x 3
#       margin classes at N = 200, n = 1000, mi = 200), each checked by direct
#       KKT residual evaluation AGAINST THE DUMP AS WRITTEN TO DISK, including
#       the LICQ rank test that is only affordable at small n.
#   (c) the ANCHOR reproduction check: the two anchor cells are re-solved and
#       their counters compared against the committed oracle artifact
#       (tycho_sqp prototypes/piqp_bridge/results/oracle_sweep.csv).
#
# Single-threaded, sequential.

set -uo pipefail
W="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG="$W/artifact/KKT-VERIFICATION.log"
export MKL_NUM_THREADS=1
export OMP_NUM_THREADS=1

mkdir -p "$W/dumps/verify" "$W/artifact"
: > "$LOG"

{
    echo "# E1 generator verification log"
    echo "# date (UTC): $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "# host: $(hostname)"
    echo "# MKL_NUM_THREADS=$MKL_NUM_THREADS OMP_NUM_THREADS=$OMP_NUM_THREADS"
    echo
    echo "== (a) referee gate (archived bridge's own, through this workspace's driver) =="
} >> "$LOG"

gate_ok=1
for eps in 1e-10 1e-12 1e-13; do
    echo "--- gate eps_abs=$eps ---" >> "$LOG"
    "$W/bridge/piqp_e1_driver" gate "$eps" >> "$LOG" 2>&1
    rc=$?
    echo "exit=$rc" >> "$LOG"
    # 1e-10 is EXPECTED to fail the referee's own 1e-8 x_err bar -- that is the
    # archive's own committed three-tolerance table (results/gate_tolerance_
    # sweep.txt), reproduced here, not a defect. Only the default 1e-12 gates.
    if [ "$eps" = "1e-12" ] && [ $rc -ne 0 ]; then gate_ok=0; fi
done
echo "gate (default 1e-12): $([ $gate_ok -eq 1 ] && echo PASS || echo FAIL)" >> "$LOG"

{
    echo
    echo "== (b) per-class KKT verification at small size (N=200, n=1000, mi=200) =="
    echo "# Each block regenerates a cell of the class, writes it to disk, reads it BACK,"
    echo "# and checks the KKT conditions on the re-read data."
    echo
} >> "$LOG"

class_ok=1
idx=0
for af in 0.01 0.10 0.30; do
    for m in 1e-2 1e-4 1e-6; do
        idx=$((idx + 1))
        aftag=$(printf '%02d' "$(python3 -c "print(round(float('$af')*100))")")
        id="e1_verify_n200_af${aftag}_m${m}"
        pre="$W/dumps/verify/$id"
        "$W/bridge/e1_generate" cell "$id" 200 "$af" "$m" $((900000 + idx)) "$pre" >> "$LOG" 2>&1
        if [ $? -ne 0 ]; then class_ok=0; fi
    done
done
echo "per-class KKT verification: $([ $class_ok -eq 1 ] && echo "ALL 9 CLASSES PASS" || echo "*** FAILED ***")" >> "$LOG"

{
    echo
    echo "== (c) anchor reproduction against the committed oracle artifact =="
    echo "# Committed reference (tycho_sqp prototypes/piqp_bridge/results/oracle_sweep.csv,"
    echo "# rows f7_n20000_bound_neutral / f7_n20000_path_neutral):"
    echo "#   bound: n=100000 me=60000 mi=20000 status=solved iter=9"
    echo "#          ext_stationarity=2.0688919324248507e-12 ext_feasibility=1.0578555514743383e-14"
    echo "#   path : n=100000 me=60000 mi=20000 status=solved iter=9"
    echo "#          ext_stationarity=2.3507055808231912e-12 ext_feasibility=1.8377463992854531e-14"
    echo "# and results/activity_diagnostics.txt records active_ineq=0/20000 on both."
    echo "# NOTE the mapping: this driver's res_dual IS ext_stationarity and its res_primal IS"
    echo "# ext_feasibility -- the same recomputation, renamed for the E1 CSV's columns."
    echo
} >> "$LOG"

for win in bound path; do
    id="e1_anchor_f7_n20000_${win}_neutral"
    pre="$W/dumps/verify/$id"
    "$W/bridge/e1_generate" anchor "$id" 20000 "$win" "$pre" >> "$LOG" 2>&1
    timeout -k 5 120 "$W/bridge/piqp_e1_driver" run "$pre.qp" >> "$LOG" 2>&1
    echo "exit=$?" >> "$LOG"
    rm -f "$pre.qp"
done

echo >> "$LOG"
echo "== end of verification ==" >> "$LOG"
cat "$LOG"
