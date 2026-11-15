#!/usr/bin/env bash
# M8 AP-5 G5 (../DECLARATION.md): copied from G3 runtime-r4, re-pointed (worktree g5, build-g5). Was: M8 AP-5 G3 (../DECLARATION-r4.md): adapted from ../../2026-11-m8-ap5-g2/ (legs-r4, runtime); see the declaration for what differs.
# Run as: flock /tmp/box-build.lock bash run_all.sh <expected-armB-sha256> <expected-armA-sha256>
# Freezes arm B, checks its sha256 against the declaration's (fail closed), runs the
# fixture parser pre-flight, then the four measured modes (interior, ipm, ssn, walk), each
# analysed as soon as it ends. STOPS at the first mode that breaks counter identity or
# reads outside the band; nothing is re-run.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/g5
EV=$WT/docs/notes/data/2026-11-m8-ap5-g5/runtime
EXPECT_B=${1:?expected arm B sha256}
EXPECT_A=${2:?expected arm A sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$WT/build-g5/armBbin"
cp -p "$WT/build-g5/rel/bench/hven_sqp_corpus" "$WT/build-g5/armBbin/hven_sqp_corpus" || exit 1
GOT_B=$(sha256sum "$WT/build-g5/armBbin/hven_sqp_corpus" | cut -d' ' -f1)
sha256sum "$WT/build-g5/armAbin/hven_sqp_corpus" "$WT/build-g5/armBbin/hven_sqp_corpus"
GOT_A=$(sha256sum "$WT/build-g5/armAbin/hven_sqp_corpus" | cut -d' ' -f1)
if [ "$GOT_A" != "$EXPECT_A" ]; then echo "ARM_A_SHA_MISMATCH got=$GOT_A"; exit 4; fi
if [ "$GOT_B" != "$EXPECT_B" ]; then echo "ARM_B_SHA_MISMATCH got=$GOT_B expected=$EXPECT_B"; exit 4; fi
[ -f "$WT/scripts/compare_replay.py" ] || { echo "COMPARE_NOT_FOUND"; exit 7; }
python3 "$EV/analyze_r11.py" --preflight "$EV/fixture/perf-table-excerpt.txt" "$EV/fixture/expected.txt" || { echo "FIXTURE_PREFLIGHT_FAIL"; exit 7; }
verdict=PASS
for MODE in interior ipm ssn walk; do
  bash "$EV/runtime_leg.sh" "$MODE" "$EV/$MODE" > "$EV/$MODE.log" 2>&1
  rc=$?; echo "LEG mode=$MODE rc=$rc $(date -Is)"
  python3 "$EV/analyze_r11.py" "$EV/$MODE" "$MODE" > "$EV/$MODE/analysis.txt" 2>&1
  arc=$?; echo "ANALYSIS mode=$MODE rc=$arc"; grep -E 'STOP CONDITION|VALIDITY|B/A|MODE ' "$EV/$MODE/analysis.txt"
  if [ $rc -ne 0 ] || [ $arc -ne 0 ]; then verdict="FAIL(mode=$MODE leg_rc=$rc analysis_rc=$arc)"; break; fi
done
echo "VERDICT $verdict"
echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = PASS ]
