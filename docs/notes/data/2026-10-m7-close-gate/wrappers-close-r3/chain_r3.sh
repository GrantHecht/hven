#!/usr/bin/env bash
# THE M7 CLOSE-GATE CHAIN, r3.  One detached process; every leg through the frozen run_leg.sh (the box lock).  Stops at the first failing step (never patched).
# usage: chain_r3.sh [first-step]     steps: census census-check char
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/env.sh"
mkdir -p "$LOGS3"
CHAIN="$F3/chain.txt"; exec >> "$CHAIN" 2>&1
echo "CHAIN_R3_START $(date -u +%FT%TZ) head=$(git -C "$R" rev-parse HEAD)"
bash "$W3/preflight_r3.sh" --falsify > "$LOGS3/preflight-r3.txt" 2>&1; prc=$?
tail -3 "$LOGS3/preflight-r3.txt"; [ $prc -eq 0 ] || { echo "CHAIN_REFUSED preflight rc=$prc"; exit 3; }
HEAD0="$(git -C "$R" rev-parse HEAD)"
STEPS=(census census-check char)
START="${1:-${STEPS[0]}}"; go=0
leg() { local name="$1"; shift; bash "$W/run_leg.sh" "$LOGS3/leg-$name.log" "$@" | tail -1; return ${PIPESTATUS[0]}; }
fail() { echo "CHAIN_STOP at $STEP: $*"; echo "CHAIN_END $(date -u +%FT%TZ) STOPPED"; exit 1; }
for STEP in "${STEPS[@]}"; do
  [ "$STEP" = "$START" ] && go=1; [ $go -eq 1 ] || continue
  echo "STEP_START $STEP $(date -u +%FT%TZ)"
  case "$STEP" in
   census)       leg $STEP bash -o pipefail "$W3/legs_r3.sh" census || fail "leg rc (infrastructure: the evidence is not whole)";;
   census-check) C="$F3/census"
                 python3 -B "$R/scripts/census_compare.py" --residual-gate 1e-5 "$CENSUS_BASELINE" "$C/walk_census.csv" > "$C/compare-gated.txt"; echo "CENSUS_GATED_VS_BASELINE_RC=$? (informational)"
                 python3 -B "$R/scripts/census_compare.py" --residual-gate 1e-5 "$R2_CENSUS" "$C/walk_census.csv" > "$C/compare-gated-vs-r2.txt"; echo "CENSUS_GATED_VS_R2_RC=$? (informational)"
                 python3 -B "$R/scripts/census_compare.py" "$R2_CENSUS" "$C/walk_census.csv" > "$C/compare-exact-vs-r2.txt"; echo "CENSUS_EXACT_VS_R2_RC=$? (informational)"
                 python3 -B "$W3/close_check_r3.py" census-r3 "$CENSUS_BASELINE" "$R2_CENSUS" "$C/walk_census.csv" "$W3/expected-census-movers-r3.txt" "$REC/derivation" || fail "census: the r3 verdict (prediction not reproduced / an undeclared mover)";;
   char)         leg $STEP bash -o pipefail "$W3/legs_r3.sh" char || fail "characterization leg rc (infrastructure)";;
  esac
  echo "STEP_OK $STEP $(date -u +%FT%TZ)"
  [ "$(git -C "$R" rev-parse HEAD)" = "$HEAD0" ] || fail "HEAD moved during the gate"
done
echo "CHAIN_R3_END $(date -u +%FT%TZ) ALL_STEPS_OK"
