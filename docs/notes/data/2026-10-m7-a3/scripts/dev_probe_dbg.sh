#!/bin/bash
# Development probe (NOT a protocol measurement): the Debug build's ladder counters at skip 1/2/3,
# to choose a unit-test pin fixture whose trajectory agrees across Debug and Release.
set -o pipefail
S=/home/ghecht/Projects/hven/.claude/worktrees/agent-afecbe0a5611f9eff/.scratch/a3
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
for k in 1 2 3 1000000; do
    "$S/build-head-dbg/bench/hven_sqp_ipqp_e1_arm" --ladder --ladder-skip-after "$k" --out "$S/dev/dbg-lad-k$k.csv" || exit 1
done
for k in 1 2 3 1000000; do
    echo "k=$k"
    grep -v '^#' "$S/dev/dbg-lad-k$k.csv" | python3 -c "
import csv,sys
for r in csv.DictReader(sys.stdin): print('  ',r['id'],r['status'],r['escape'],'it',r['ipqp_iters'],'fac',r['ipqp_factorizations'],'elev',r['ipqp_iters_at_elevated_rho'],'skips',r['ipqp_ladder_zero_trial_skips'])"
done
