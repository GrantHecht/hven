#!/usr/bin/env bash
# PF-HB: the runtime gate alone (its own flock), then chain1 only on VERDICT PASS.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-ab44eb20498de1642
R=$WT/docs/notes/data/2026-11-m8-pf-huge-bound
B=3e1b7b1e70c099af16bd07fa7031b879e0e7103d5d6ba9f4b503e377be72d0f8
cd "$WT" || exit 1
pgrep -a hven_bench_cell && { echo "PGREP_BUSY before gate"; exit 3; }
flock /tmp/box-build.lock bash "$R/runtime/run_all.sh" "$B" > "$R/runtime/run_all.log" 2>&1
grc=$?
echo "GATE rc=$grc $(date -Is)"
grep -q "VERDICT PASS" "$R/runtime/run_all.log" || { echo "GATE_NOT_PASS -- chain1 not started"; exit 5; }
ARMB_SHA=$B bash "$R/legs/chain1.sh" > "$R/chain/chain1.log" 2>&1
echo "CHAIN rc=$? $(date -Is)"
