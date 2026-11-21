#!/usr/bin/env bash
# kopt-1x P4: the P2 runtime gate of ../../2026-11-m8-ap5-g3/runtime-r4/run_all.sh, WALK mode only (the polish trigger and the gray hold run in
# the walk). Serial, alone, under the lock. usage: run_walk.sh <expected armB sha256> <expected armA sha256>
#   Run under: flock /tmp/box-build.lock setsid -w bash run_walk.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/wa1
EV=$WT/docs/notes/data/2026-11-m8-wa1/runtime
EXPECT_B=${1:?}; EXPECT_A=${2:?}; OUTD=${3:-$EV}   # r2: the output directory (default: this directory, run 1)
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
GOT_A=$(sha256sum "$WT/build-wa/bin/A/hven_sqp_corpus" | cut -d' ' -f1); GOT_B=$(sha256sum "$WT/build-wa/bin/B/hven_sqp_corpus" | cut -d' ' -f1)
[ "$GOT_A" = "$EXPECT_A" ] || { echo "ARM_A_SHA_MISMATCH $GOT_A"; exit 4; }
[ "$GOT_B" = "$EXPECT_B" ] || { echo "ARM_B_SHA_MISMATCH $GOT_B"; exit 4; }
python3 "$EV/analyze_r11.py" --preflight "$EV/fixture/perf-table-excerpt.txt" "$EV/fixture/expected.txt" || { echo FIXTURE_PREFLIGHT_FAIL; exit 7; }
mkdir -p "$OUTD"
bash "$EV/runtime_leg.sh" walk "$OUTD/walk" > "$OUTD/walk.log" 2>&1; rc=$?
echo "LEG mode=walk rc=$rc"
python3 "$EV/analyze_r11.py" "$OUTD/walk" walk > "$OUTD/walk/analysis.txt" 2>&1; arc=$?
echo "ANALYSIS rc=$arc"; grep -E 'STOP CONDITION|VALIDITY|B/A|MODE ' "$OUTD/walk/analysis.txt"
echo "FOREGROUND_END $(date -Is)"
[ $rc -eq 0 ] && [ $arc -eq 0 ]
