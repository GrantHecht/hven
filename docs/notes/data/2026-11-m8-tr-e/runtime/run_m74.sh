#!/usr/bin/env bash
# M8 TR-E (../DECLARATION.md, M7-4 leg). Run as: flock /tmp/box-build.lock bash run_m74.sh
# Freezes the Release hven_sqp_corpus of the TR-E code head at build-tre/armbin/, then the interior leg TWICE, serial, solo:
#   aa   : neither arm attached (the A/A control: the noise floor of this instrument on this box)
#   att  : arm A no callback, arm B the counting callback at kScalars (the M7-4 attach cost = B/A - 1)
# each analysed by analyze_r11.py (counter identity first; THROTTLE == 0 or VOID). Prints the attach cost and the verdict.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/tr-e
EV=$WT/docs/notes/data/2026-11-m8-tr-e/runtime
EXPECT=${1:?expected hven_sqp_corpus sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$WT/.scratch/build-tre/armbin"
cp -p "$WT/.scratch/build-tre/rel/bench/hven_sqp_corpus" "$WT/.scratch/build-tre/armbin/hven_sqp_corpus" || exit 1
GOT=$(sha256sum "$WT/.scratch/build-tre/armbin/hven_sqp_corpus" | cut -d' ' -f1)
echo "hven_sqp_corpus sha256 $GOT"
[ "$GOT" = "$EXPECT" ] || { echo "SHA_MISMATCH expected=$EXPECT"; exit 4; }
python3 "$EV/analyze_r11.py" --preflight "$EV/fixture/perf-table-excerpt.txt" "$EV/fixture/expected.txt" || { echo "FIXTURE_PREFLIGHT_FAIL"; exit 7; }
mkdir -p "$WT/.scratch/m74"
for LEG in aa att; do
  D=$WT/.scratch/m74/$LEG
  if [ $LEG = aa ]; then export TRE_AA=1; else unset TRE_AA; fi
  bash "$EV/runtime_leg.sh" interior "$D" > "$WT/.scratch/m74/$LEG.log" 2>&1
  rc=$?; echo "LEG $LEG rc=$rc $(date -Is)"
  python3 "$EV/analyze_r11.py" "$D" interior > "$D/analysis.txt" 2>&1
  arc=$?; echo "ANALYSIS $LEG rc=$arc"; grep -E 'STOP CONDITION|VALIDITY|B/A|filtered samples' "$D/analysis.txt"
  if [ $rc -ne 0 ]; then echo "VERDICT FAIL($LEG leg_rc=$rc)"; exit 1; fi
done
echo "FOREGROUND_END $(date -Is)"
