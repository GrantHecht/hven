#!/usr/bin/env bash
# M8 AP-5 G4 r3 (../DECLARATION-r3.md): copied from ../ic/ with the evidence and raw directories re-pointed (-r3, .scratch/g4r3-*); nothing else.
# M8 AP-5 G4 (../DECLARATION.md): the pass A/B instruction-count leg. Pass A (ic_leg.sh, perf stat
# instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u) for walk, ssn, ipm (the SQP's
# three QP modes: the walk, SSN and IPQP minor loops) and interior (the IPM, the ladder poll); three rounds, arms
# alternated per cell; analyze.py per mode; verdict.py applies the declared thresholds. Pass B is explanatory
# and runs only as the declaration says.
# usage: run_ic.sh <expected arm B sha256> <expected arm A sha256>
# Run as: flock /tmp/box-build.lock bash run_ic.sh ...   (serial; nothing else on the box)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a2559003d3a33fac4
H=$WT/docs/notes/data/2026-11-m8-ap5-g4/ic-r3
EV=$WT/.scratch/g4r3-ic
mkdir -p "$EV"
EXPECT_B=${1:?expected arm B sha256}
EXPECT_A=${2:?expected arm A sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
GOT_A=$(sha256sum "$WT/build-g4/armAbin/hven_sqp_corpus" | cut -d' ' -f1)
GOT_B=$(sha256sum "$WT/build-g4/armBbin/hven_sqp_corpus" | cut -d' ' -f1)
[ "$GOT_A" = "$EXPECT_A" ] || { echo "ARM_A_SHA_MISMATCH got=$GOT_A"; exit 4; }
[ "$GOT_B" = "$EXPECT_B" ] || { echo "ARM_B_SHA_MISMATCH got=$GOT_B"; exit 4; }
worst=0
for MODE in walk ssn ipm interior; do
  bash "$H/ic_leg.sh" "$MODE" A 3 "$EV/passA-$MODE" > "$EV/passA-$MODE.log" 2>&1
  rc=$?; echo "LEG mode=$MODE rc=$rc $(date -Is)"; [ $rc -gt $worst ] && worst=$rc
  python3 "$H/analyze.py" "$EV/passA-$MODE" "$MODE" 3 > "$EV/passA-$MODE/analysis.txt" 2>&1
  rc=$?; echo "ANALYSIS mode=$MODE rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  mkdir -p "$H/passA-$MODE"; cp "$EV/passA-$MODE/analysis.txt" "$H/passA-$MODE/analysis.txt"; cp "$EV/passA-$MODE.log" "$H/passA-$MODE.log"
  grep -E 'counter check|CORPUS instructions|worst per-cell|cells outside' "$EV/passA-$MODE/analysis.txt"
done
python3 "$H/verdict.py" "$H" | tee "$H/VERDICT.txt"
rc=$?; [ $rc -gt $worst ] && worst=$rc
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
