#!/usr/bin/env bash
# PF-4 r3 (../../DECLARATION-PF-4-r3.md): run_walk_r1.sh calling runtime_leg_r3.sh and analyze_r3.py; otherwise unchanged.
# PF-4 r2 (../../DECLARATION-PF-4-r2.md): copy of ../../runtime/run_walk_r1.sh re-pointed to the pf4-r2 worktree and r2/ (paths only); older lines are historical.
# PF-4 r1 (../DECLARATION-PF-4-r1.md): run_walk.sh calling runtime_leg_r3.sh; otherwise unchanged.
# PF-4 P4a: ../../2026-11-m8-wa1/runtime/run_walk.sh re-pointed: the corpus runtime gate, WALK mode (the Schur border batch runs
# in the walk), serial, alone, under the lock. usage: run_walk.sh <armB sha256> <armA sha256> <out-dir>
#   Run under (r2): ~/.local/bin/box-lane -- bash run_walk.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf4-r2
EV=$WT/docs/notes/data/2026-11-m8-pf4/r2/runtime
EXPECT_B=${1:?}; EXPECT_A=${2:?}; OUTD=${3:-$EV}   # r2: the output directory (default: this directory, run 1)
[ -n "${BOX_LANE:-}" ] || { echo "NOT_IN_BOX_LANE (r2: every leg runs inside ~/.local/bin/box-lane)"; exit 3; }; echo "BOX_LANE=$BOX_LANE phys=[$BOX_LANE_PHYS] jobs=$BOX_LANE_JOBS"
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
GOT_A=$(sha256sum "$WT/.scratch/bin/A/hven_sqp_corpus" | cut -d' ' -f1); GOT_B=$(sha256sum "$WT/.scratch/bin/B/hven_sqp_corpus" | cut -d' ' -f1)
[ "$GOT_A" = "$EXPECT_A" ] || { echo "ARM_A_SHA_MISMATCH $GOT_A"; exit 4; }
[ "$GOT_B" = "$EXPECT_B" ] || { echo "ARM_B_SHA_MISMATCH $GOT_B"; exit 4; }
python3 "$EV/analyze_r3.py" --preflight "$EV/fixture/perf-table-excerpt.txt" "$EV/fixture/expected.txt" || { echo FIXTURE_PREFLIGHT_FAIL; exit 7; }
mkdir -p "$OUTD"
bash "$EV/runtime_leg_r3.sh" walk "$OUTD/walk" > "$OUTD/walk.log" 2>&1; rc=$?
echo "LEG mode=walk rc=$rc"
python3 "$EV/analyze_r3.py" "$OUTD/walk" walk > "$OUTD/walk/analysis.txt" 2>&1; arc=$?
echo "ANALYSIS rc=$arc"; grep -E 'STOP CONDITION|VALIDITY|B/A|MODE ' "$OUTD/walk/analysis.txt"
echo "FOREGROUND_END $(date -Is)"
[ $rc -eq 0 ] && [ $arc -eq 0 ]
