#!/usr/bin/env bash
# AP-2 B2 runtime wrapper: freezes arm B, runs B2-R1 (walk, ssn, ipm, interior; three
# rounds) then B2-R2 (walk, interior; one round), then the analyses.
# Run as: flock /tmp/box-build.lock bash runtime_all.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aba3deeb97313f444
EV=$WT/docs/notes/data/2026-11-m8-ap2-b2/runtime
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$WT/build-b2/armB"
cp -p "$WT/build-b2/rel/bench/hven_sqp_corpus" "$WT/build-b2/armB/hven_sqp_corpus" || exit 1
sha256sum "$WT/build-b2/armA/hven_sqp_corpus" "$WT/build-b2/armB/hven_sqp_corpus"
worst=0
for MODE in walk ssn ipm interior; do
  bash "$EV/runtime_leg.sh" "$MODE" A 3 "$EV/passA-$MODE" > "$EV/passA-$MODE.log" 2>&1
  rc=$?; echo "R1 mode=$MODE rc=$rc $(date -Is)"; [ $rc -gt $worst ] && worst=$rc
  python3 "$EV/analyze.py" "$EV/passA-$MODE" "$MODE" 3 > "$EV/passA-$MODE/analysis.txt" 2>&1
  echo "R1 analysis mode=$MODE rc=$?"; grep -iE 'corpus|counter check' "$EV/passA-$MODE/analysis.txt" | head -5
done
for MODE in walk interior; do
  bash "$EV/runtime_r2.sh" "$MODE" "$EV/r2-$MODE" > "$EV/r2-$MODE.log" 2>&1
  rc=$?; echo "R2 mode=$MODE rc=$rc $(date -Is)"
  python3 "$EV/analyze.py" "$EV/r2-$MODE" "$MODE" 1 > "$EV/r2-$MODE/analysis.txt" 2>&1
  echo "R2 analysis mode=$MODE rc=$?"; grep -iE 'corpus|counter check' "$EV/r2-$MODE/analysis.txt" | head -5
done
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
