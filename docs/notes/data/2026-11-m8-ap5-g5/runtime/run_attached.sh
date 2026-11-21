#!/usr/bin/env bash
# M8 AP-5 G5 (../DECLARATION.md P2b): the attached-callback legs (the M7-4 residual re-measured), on the interior engine (the
# interior leg) and on the SQP engine (modes ipm and ssn), serial, alone (CLAUDE.md section 7), each analysed as the unattached
# legs are. Order per mode: attached-scalars (both arms kScalars), attached-default (arm A kAll == G4's kVectors|kDiagnostics,
# arm B kVectors|kDiagnostics named: like for like), attached-all (both arms kAll: arm B's carries the G5 views -- the new kAll's
# cost, INFORMATIONAL: its out-of-band exit (analysis rc 1) does not stop the driver; a counter-identity break (rc 2/3) does).
# attach_delta.py then reports, per mode, the cost of attaching per arm and the new kAll's cost (arm B all / arm B default).
# usage: run_attached.sh <expected arm B sha256> <expected arm A sha256>   Run as: flock /tmp/box-build.lock setsid -w bash run_attached.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/g5
EV=$WT/docs/notes/data/2026-11-m8-ap5-g5/runtime
EXPECT_B=${1:?expected arm B sha256}
EXPECT_A=${2:?expected arm A sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
GOT_A=$(sha256sum "$WT/build-g5/armAbin/hven_sqp_corpus" | cut -d' ' -f1)
GOT_B=$(sha256sum "$WT/build-g5/armBbin/hven_sqp_corpus" | cut -d' ' -f1)
[ "$GOT_A" = "$EXPECT_A" ] || { echo "ARM_A_SHA_MISMATCH got=$GOT_A"; exit 4; }
[ "$GOT_B" = "$EXPECT_B" ] || { echo "ARM_B_SHA_MISMATCH got=$GOT_B"; exit 4; }
verdict=PASS
for MODE in interior ipm ssn; do
  for ATT in attached-scalars attached-default attached-all; do
    D=$EV/$MODE-att-${ATT#attached-}
    bash "$EV/runtime_leg.sh" "$MODE" "$D" "$ATT" > "$D.log" 2>&1
    rc=$?; echo "LEG $MODE $ATT rc=$rc $(date -Is)"
    python3 "$EV/analyze_r11.py" "$D" "$MODE" > "$D/analysis.txt" 2>&1
    arc=$?; echo "ANALYSIS $MODE $ATT rc=$arc"; grep -E 'STOP CONDITION|VALIDITY|B/A|MODE ' "$D/analysis.txt"
    if [ $rc -ne 0 ]; then verdict="FAIL($MODE $ATT leg_rc=$rc)"; break 2; fi
    if [ "$ATT" = attached-all ] && [ $arc -eq 1 ]; then echo "INFORMATIONAL $MODE attached-all outside the band (reported, not a criterion)"; continue; fi
    if [ $arc -ne 0 ]; then verdict="FAIL($MODE $ATT analysis_rc=$arc)"; break 2; fi
  done
  python3 "$EV/attach_delta.py" "$MODE" "$EV/$MODE/analysis.txt" "$EV/$MODE-att-scalars/analysis.txt" "$EV/$MODE-att-default/analysis.txt" "$EV/$MODE-att-all/analysis.txt" | tee "$EV/attach-delta-$MODE.txt"
done
echo "VERDICT $verdict"
echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = PASS ]
