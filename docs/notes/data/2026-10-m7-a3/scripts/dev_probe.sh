#!/bin/bash
# Development probe (NOT a protocol measurement): ladder counters at several skip values, used only
# to write the unit-test pins of tests/sqp/test_ipqp_ladder_skip.cpp. One lock hold.
set -o pipefail
S=/home/ghecht/Projects/hven/.claude/worktrees/agent-afecbe0a5611f9eff/.scratch/a3
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
mkdir -p "$S/dev"
for k in 0 1 2 3 1000000; do
    "$S/build-head-rel/bench/hven_sqp_ipqp_e1_arm" --ladder --ladder-skip-after "$k" --out "$S/dev/lad-k$k.csv" || exit 1
done
for k in 0 1 2 3 1000000; do
    echo "k=$k"
    grep -v '^#' "$S/dev/lad-k$k.csv" | python3 -c "
import csv,sys
for r in csv.DictReader(sys.stdin): print('  ',r['id'],r['status'],r['escape'],'it',r['ipqp_iters'],'fac',r['ipqp_factorizations'],'elev',r['ipqp_iters_at_elevated_rho'],'retry',r['ipqp_inertia_retries'],'recl',r['ipqp_ladder_reclimbs'],'skips',r['ipqp_ladder_zero_trial_skips'])"
done
