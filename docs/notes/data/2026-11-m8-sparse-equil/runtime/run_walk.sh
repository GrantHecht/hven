#!/usr/bin/env bash
# kopt-1x P4: the P2 runtime gate of ../../2026-11-m8-ap5-g3/runtime-r4/run_all.sh, WALK mode only (the polish trigger and the gray hold run in
# the walk). Serial, alone, under the lock. usage: run_walk.sh <expected armB sha256> <expected armA sha256>
#   Run under: flock /tmp/box-build.lock setsid -w bash run_walk.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/sparse-equil
EV=$WT/docs/notes/data/2026-11-m8-sparse-equil/runtime
EXPECT_B=${1:?}; EXPECT_A=${2:?}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
GOT_A=$(sha256sum "$WT/build-se/bin/A/hven_sqp_corpus" | cut -d' ' -f1); GOT_B=$(sha256sum "$WT/build-se/bin/B/hven_sqp_corpus" | cut -d' ' -f1)
[ "$GOT_A" = "$EXPECT_A" ] || { echo "ARM_A_SHA_MISMATCH $GOT_A"; exit 4; }
[ "$GOT_B" = "$EXPECT_B" ] || { echo "ARM_B_SHA_MISMATCH $GOT_B"; exit 4; }
python3 "$EV/analyze_r11.py" --preflight "$EV/fixture/perf-table-excerpt.txt" "$EV/fixture/expected.txt" || { echo FIXTURE_PREFLIGHT_FAIL; exit 7; }
bash "$EV/runtime_leg.sh" walk "$EV/walk" > "$EV/walk.log" 2>&1; rc=$?
echo "LEG mode=walk rc=$rc"
python3 "$EV/analyze_r11.py" "$EV/walk" walk > "$EV/walk/analysis.txt" 2>&1; arc=$?
echo "ANALYSIS rc=$arc"; grep -E 'STOP CONDITION|VALIDITY|B/A|MODE ' "$EV/walk/analysis.txt"
echo "FOREGROUND_END $(date -Is)"
[ $rc -eq 0 ] && [ $arc -eq 0 ]
