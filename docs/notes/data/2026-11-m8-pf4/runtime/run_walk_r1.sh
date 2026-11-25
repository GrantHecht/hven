#!/usr/bin/env bash
# PF-4 r1 (../DECLARATION-PF-4-r1.md): run_walk.sh calling runtime_leg_r1.sh; otherwise unchanged.
# PF-4 P4a: ../../2026-11-m8-wa1/runtime/run_walk.sh re-pointed: the corpus runtime gate, WALK mode (the Schur border batch runs
# in the walk), serial, alone, under the lock. usage: run_walk.sh <armB sha256> <armA sha256> <out-dir>
#   Run under: setsid -w flock /tmp/box-build.lock bash run_walk.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf4
EV=$WT/docs/notes/data/2026-11-m8-pf4/runtime
EXPECT_B=${1:?}; EXPECT_A=${2:?}; OUTD=${3:-$EV}   # r2: the output directory (default: this directory, run 1)
if [ -n "$(pgrep -x hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -x hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
GOT_A=$(sha256sum "$WT/.scratch/bin/A/hven_sqp_corpus" | cut -d' ' -f1); GOT_B=$(sha256sum "$WT/.scratch/bin/B/hven_sqp_corpus" | cut -d' ' -f1)
[ "$GOT_A" = "$EXPECT_A" ] || { echo "ARM_A_SHA_MISMATCH $GOT_A"; exit 4; }
[ "$GOT_B" = "$EXPECT_B" ] || { echo "ARM_B_SHA_MISMATCH $GOT_B"; exit 4; }
python3 "$EV/analyze.py" --preflight "$EV/fixture/perf-table-excerpt.txt" "$EV/fixture/expected.txt" || { echo FIXTURE_PREFLIGHT_FAIL; exit 7; }
mkdir -p "$OUTD"
bash "$EV/runtime_leg_r1.sh" walk "$OUTD/walk" > "$OUTD/walk.log" 2>&1; rc=$?
echo "LEG mode=walk rc=$rc"
python3 "$EV/analyze.py" "$OUTD/walk" walk > "$OUTD/walk/analysis.txt" 2>&1; arc=$?
echo "ANALYSIS rc=$arc"; grep -E 'STOP CONDITION|VALIDITY|B/A|MODE ' "$OUTD/walk/analysis.txt"
echo "FOREGROUND_END $(date -Is)"
[ $rc -eq 0 ] && [ $arc -eq 0 ]
