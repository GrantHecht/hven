#!/usr/bin/env bash
# M8 AP-5 G3 (../DECLARATION-r4.md P2b): the attached-callback legs (the M7-4 residual re-measured at kAll and kScalars). The interior
# leg (the 27 cells, one process) is run with the corpus leg's read-only counting callback installed in BOTH arms, once under
# IterationPayload::kAll and once under kScalars, serial, alone (CLAUDE.md section 7). Each is analysed as the unattached legs are
# (counter identity, B/A within [0.995, 1.005]); attach_delta.py then reports the cost of attaching per arm, from the unattached
# interior leg of run_all.sh and these two: (attached / unattached) for arm A and for arm B, and the B/A attached ratios.
# usage: run_attached.sh <expected arm B sha256> <expected arm A sha256>     Run as: flock /tmp/box-build.lock bash run_attached.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-ac15f1e91e3a3b6b2
EV=$WT/docs/notes/data/2026-11-m8-ap5-g3/runtime-r4
EXPECT_B=${1:?expected arm B sha256}
EXPECT_A=${2:?expected arm A sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
GOT_A=$(sha256sum "$WT/build-g3/armAbin/hven_sqp_corpus" | cut -d' ' -f1)
GOT_B=$(sha256sum "$WT/build-g3/armBbin/hven_sqp_corpus" | cut -d' ' -f1)
[ "$GOT_A" = "$EXPECT_A" ] || { echo "ARM_A_SHA_MISMATCH got=$GOT_A"; exit 4; }
[ "$GOT_B" = "$EXPECT_B" ] || { echo "ARM_B_SHA_MISMATCH got=$GOT_B"; exit 4; }
verdict=PASS
for ATT in attached-all attached-scalars; do
  D=$EV/interior-${ATT#attached-}
  D=${D/interior-all/interior-att-all}; D=${D/interior-scalars/interior-att-scalars}
  bash "$EV/runtime_leg.sh" interior "$D" "$ATT" > "$D.log" 2>&1
  rc=$?; echo "LEG $ATT rc=$rc $(date -Is)"
  python3 "$EV/analyze_r11.py" "$D" interior > "$D/analysis.txt" 2>&1
  arc=$?; echo "ANALYSIS $ATT rc=$arc"; grep -E 'STOP CONDITION|VALIDITY|B/A|MODE ' "$D/analysis.txt"
  if [ $rc -ne 0 ] || [ $arc -ne 0 ]; then verdict="FAIL($ATT leg_rc=$rc analysis_rc=$arc)"; break; fi
done
if [ "$verdict" = PASS ]; then
  python3 "$EV/attach_delta.py" "$EV/interior/analysis.txt" "$EV/interior-att-all/analysis.txt" "$EV/interior-att-scalars/analysis.txt" | tee "$EV/attach-delta.txt"
fi
echo "VERDICT $verdict"
echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = PASS ]
