#!/usr/bin/env bash
# A/A CALIBRATION LEG (r3, spec section 4 proof 4): runtime_leg.sh with AA_CALIB=1, i.e. arm A's binary as BOTH arms, same cells, same rounds.
# Run as: flock /tmp/box-build.lock bash run_aa.sh <expected-armA-sha256>     Solo and serial like run_all.sh. Evidence under runtime/aa/<mode>/.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/tr-a
H=$WT/docs/notes/data/2026-11-m8-tr-a/runtime
EV=$WT/.scratch/tra-runtime-aa
mkdir -p "$EV"
EXPECT_A=${1:?expected arm A sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
GOT_A=$(sha256sum "$WT/.scratch/build-tra/armAbin/hven_sqp_corpus" | cut -d' ' -f1)
if [ "$GOT_A" != "$EXPECT_A" ]; then echo "ARM_A_SHA_MISMATCH got=$GOT_A"; exit 4; fi
export AA_CALIB=1
verdict=PASS
for MODE in interior ipm ssn walk; do
  bash "$H/runtime_leg.sh" "$MODE" "$EV/$MODE" > "$EV/$MODE.log" 2>&1
  rc=$?; echo "LEG mode=$MODE rc=$rc $(date -Is)"
  python3 "$H/analyze_r11.py" "$EV/$MODE" "$MODE" > "$EV/$MODE/analysis.txt" 2>&1
  arc=$?; echo "ANALYSIS mode=$MODE rc=$arc"; grep -E 'STOP CONDITION|VALIDITY|B/A|MODE ' "$EV/$MODE/analysis.txt"
  mkdir -p "$H/aa/$MODE"; cp "$EV/$MODE/analysis.txt" "$H/aa/$MODE/analysis.txt"; cp "$EV/$MODE.log" "$H/aa/$MODE.log"
  if [ $rc -ne 0 ] || [ $arc -ne 0 ]; then verdict="FAIL(mode=$MODE leg_rc=$rc analysis_rc=$arc)"; break; fi
done
echo "VERDICT $verdict"
echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = PASS ]
