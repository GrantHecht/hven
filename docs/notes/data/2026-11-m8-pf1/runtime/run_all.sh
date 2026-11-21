#!/usr/bin/env bash
# M8 PF-1 (PF-HB's instrument, re-pointed) runtime wrapper (the B8 riders' copy of the B2-runtime-r5 instrument; arm A = bdd797e3) (DECLARATION.txt in this directory).
# Run as: flock /tmp/box-build.lock bash run_all.sh <expected-armB-sha256>
# Freezes arm B, checks its sha256 against the declaration's (fail closed), runs the
# fixture parser pre-flight, then the four measured modes (interior, ipm, ssn, walk), each
# analysed as soon as it ends. STOPS at the first mode that breaks counter identity or
# reads outside the band; nothing is re-run.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aa2c5c3a798557d0e
EV=$WT/docs/notes/data/2026-11-m8-pf1/runtime
EXPECT_B=${1:?expected arm B sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$WT/build-pf1e/armBbin"
cp -p "$WT/build-pf1e/rel/bench/hven_sqp_corpus" "$WT/build-pf1e/armBbin/hven_sqp_corpus" || exit 1
GOT_B=$(sha256sum "$WT/build-pf1e/armBbin/hven_sqp_corpus" | cut -d' ' -f1)
sha256sum "$WT/build-pf1e/armAbin/hven_sqp_corpus" "$WT/build-pf1e/armBbin/hven_sqp_corpus"
GOT_A=$(sha256sum "$WT/build-pf1e/armAbin/hven_sqp_corpus" | cut -d' ' -f1)
if [ "$GOT_A" != cb07803891f17091a8bbe1de280d947c99d59a16e05bfbefd49d54d1a014166b ]; then echo "ARM_A_SHA_MISMATCH got=$GOT_A"; exit 4; fi
if [ "$GOT_B" != "$EXPECT_B" ]; then echo "ARM_B_SHA_MISMATCH got=$GOT_B expected=$EXPECT_B"; exit 4; fi
python3 "$EV/analyze_r5.py" --preflight "$EV/fixture/perf-table-excerpt.txt" "$EV/fixture/expected.txt" || { echo "FIXTURE_PREFLIGHT_FAIL"; exit 7; }
verdict=PASS
for MODE in interior ipm ssn walk; do
  bash "$EV/runtime_leg.sh" "$MODE" "$EV/$MODE" > "$EV/$MODE.log" 2>&1
  rc=$?; echo "LEG mode=$MODE rc=$rc $(date -Is)"
  python3 "$EV/analyze_r5.py" "$EV/$MODE" "$MODE" > "$EV/$MODE/analysis.txt" 2>&1
  arc=$?; echo "ANALYSIS mode=$MODE rc=$arc"; grep -E 'STOP CONDITION|VALIDITY|B/A|MODE ' "$EV/$MODE/analysis.txt"
  if [ $rc -ne 0 ] || [ $arc -ne 0 ]; then verdict="FAIL(mode=$MODE leg_rc=$rc analysis_rc=$arc)"; break; fi
done
echo "VERDICT $verdict"
echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = PASS ]
