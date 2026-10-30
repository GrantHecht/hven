#!/usr/bin/env bash
# THE M7 CLOSE-GATE CHAIN, R2 (successor): resumes at sv1-guards with the r2 guards; see ../DECLARATION-r2.md.
# The chain STOPS at the first failing leg or assertion (a failed leg is reported, never patched: a fix gets a successor
# -r2 declaration).  usage: chain.sh [first-step]   (resume point; default the first)
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/../wrappers-close/env.sh"
mkdir -p "$LOGS"
CHAIN="$F/chain.txt"; exec >> "$CHAIN" 2>&1
CK="python3 -B $W/close_check.py"; DER="$REC/derivation"
echo "CHAIN_R2_START $(date -u +%FT%TZ) head=$(git -C "$R" rev-parse HEAD)"
# 0. pre-flight in RUN mode (clean tree, declaration committed): refuses the chain on any failure
bash "$W/preflight.sh" run --falsify > "$LOGS/preflight-run.txt" 2>&1; prc=$?
tail -3 "$LOGS/preflight-run.txt"; [ $prc -eq 0 ] || { echo "CHAIN_REFUSED preflight rc=$prc"; exit 3; }
HEAD0="$(git -C "$R" rev-parse HEAD)"

STEPS=(sv1-guards sv1-hs sv1-cutest census)
START="${1:-${STEPS[0]}}"; go=0
leg() { local name="$1"; shift; bash "$W/run_leg.sh" "$LOGS/leg-$name.log" "$@" | tail -1; return ${PIPESTATUS[0]}; }
fail() { echo "CHAIN_STOP at $STEP: $*"; echo "CHAIN_END $(date -u +%FT%TZ) STOPPED"; exit 1; }
for STEP in "${STEPS[@]}"; do
  [ "$STEP" = "$START" ] && go=1; [ $go -eq 1 ] || continue
  echo "STEP_START $STEP $(date -u +%FT%TZ)"
  case "$STEP" in
   build-release) leg $STEP bash -o pipefail "$W/legs.sh" build-release || fail "leg rc"; $CK configure "$LOGS/configure-release.log" || fail "configure assertion";;
   build-debug)   leg $STEP bash -o pipefail "$W/legs.sh" build-debug   || fail "leg rc"; $CK configure "$LOGS/configure-debug.log"   || fail "configure assertion";;
   suite-release) leg $STEP bash -o pipefail "$W/legs.sh" suite-release || fail "leg rc"; $CK suite "$LOGS/suite-release.out" "$LOGS/inventory-release.out" "$DER" release || fail "suite assertion";;
   suite-debug)   leg $STEP bash -o pipefail "$W/legs.sh" suite-debug   || fail "leg rc"; $CK suite "$LOGS/suite-debug.out" "$LOGS/inventory-debug.out" "$DER" debug || fail "suite assertion";;
   long-debug)    leg $STEP bash -o pipefail "$W/legs.sh" long-debug    || fail "leg rc"; $CK suite "$LOGS/suite-debug-long.out" /dev/null "$DER" long || fail "long-test assertion";;
   smoke)         leg $STEP bash -o pipefail "$W/legs.sh" smoke         || fail "leg rc"
                  grep -q '== hven install smoke: PASSED' "$LOGS/install-smoke.out" || fail "smoke PASSED line"
                  grep -q 'both solvers converged through one generic consumer' "$LOGS/install-smoke.out" || fail "smoke both-solvers line"
                  grep -q '== export contract OK for platform=linux, align=32' "$LOGS/export-contract.out" || fail "export contract line";;
   rig-build)     leg $STEP bash -o pipefail "$W/legs.sh" rig-build     || fail "leg rc"
                  grep -c 'golden rig' "$LOGS/configure-3seam.log"; grep -q 'interior-point old-seam arm pinned to consumed tree bfb7d30c5cc821069b7af3f9efa077f29474de5e' "$LOGS/configure-3seam.log" || fail "psiopt seam pin line"
                  grep -q 'SQP old-seam arm pinned to phase-7-close (4faa1df116da53c9dc68f36635c118f52d39d2b9)' "$LOGS/configure-3seam.log" || fail "sqp seam pin line";;
   rig-run)       leg $STEP bash -o pipefail "$W/legs.sh" rig-run       || fail "leg rc (wrapper-level: ctest did not run)"
                  $CK rig "$LOGS" "$DER" "$LOGS/inventory-release.out" || fail "rig assertion";;
   replay)        leg $STEP bash -o pipefail "$W/legs.sh" replay        || fail "leg rc"
                  D="$F/replay"; : > "$D/exact-vs-wx2.txt"; : > "$D/gated-vs-wx2.txt"; : > "$D/gated-vs-m6.txt"
                  for e in walk ssn ipm; do
                    python3 -B "$R/scripts/compare_replay.py" "$WX2/gates/replay/head-$e.csv" "$D/head-$e.csv" "WX2 RECORD HEAD-$e vs M7 CLOSE HEAD-$e (EXACT)" >> "$D/exact-vs-wx2.txt"; echo "EXACT_RC_$e=$?"
                    python3 -B "$R/scripts/compare_replay.py" --residual-gate 1e-5 "$WX2/gates/replay/head-$e.csv" "$D/head-$e.csv" "WX2 RECORD HEAD-$e vs M7 CLOSE HEAD-$e (GATED 1e-5)" >> "$D/gated-vs-wx2.txt"; echo "GATED_WX2_RC_$e=$?"
                    python3 -B "$R/scripts/compare_replay.py" --residual-gate 1e-5 "$M6/replay/head-$e.csv" "$D/head-$e.csv" "M6 CLOSE HEAD-$e vs M7 CLOSE HEAD-$e (GATED 1e-5)" >> "$D/gated-vs-m6.txt"; echo "GATED_M6_RC_$e=$?"
                  done
                  # VERDICT: the gated comparison against the latest gated record has 0 differences (counters/statuses exact, residual-class
                  # columns through the calibrated gate); the measured difference set against the M6 pin EQUALS the declared 43-set.  The EXACT
                  # arm is REPORTED (WX2 asserted 0/0/0); a difference there that is not inside the gate would already fail the gated arm.
                  $CK replay-exact "$D/gated-vs-wx2.txt" || fail "gated replay vs the WX2 record has differences"
                  $CK replay-m6 "$D/gated-vs-m6.txt" "$DER" || fail "difference set vs the M6 pin != the declared 43-set"
                  $CK replay-exact "$D/exact-vs-wx2.txt" || echo "NOTE exact arm differs from the WX2 record (reported; inside the gate by the gated verdict above)";;
   sv1-pin)       leg $STEP bash -o pipefail "$W/suitev1.sh" pin        || fail "leg rc";;
   sv1-build)     leg $STEP bash -o pipefail "$W/suitev1.sh" build      || fail "leg rc";;
   sv1-guards)    leg $STEP bash -o pipefail "$W/../wrappers-close-r2/suitev1_r2.sh" guards     || fail "leg rc (a transcription guard)";;
   sv1-hs)        leg $STEP bash -o pipefail "$W/suitev1.sh" hs;  rc=$?; [ $rc -le 1 ] || fail "run_serial rc $rc (evidence not whole)";;
   sv1-cutest)    leg $STEP bash -o pipefail "$W/suitev1.sh" cutest; rc=$?; [ $rc -le 1 ] || fail "run_serial rc $rc (evidence not whole)"
                  python3 -B "$W/check_suitev1.py" "$F/suitev1/run-hs" "$F/suitev1/run-cutest" "$BENCH" "$PIN" | tee "$LOGS/check-suitev1.txt"; [ ${PIPESTATUS[0]} -eq 0 ] || fail "suite v1 assertion";;
   census)        leg $STEP bash -o pipefail "$W/legs.sh" census        || fail "leg rc (infrastructure: the evidence is not whole)"
                  C="$F/census"; python3 -B "$R/scripts/census_compare.py" --residual-gate 1e-5 "$CENSUS_BASELINE" "$C/walk_census.csv" > "$C/compare-gated.txt"; echo "CENSUS_GATED_RC=$?"
                  python3 -B "$R/scripts/census_compare.py" "$M6/census/walk_census.csv" "$C/walk_census.csv" > "$C/compare-vs-m6-census-exact.txt"; echo "CENSUS_VS_M6_EXACT_RC=$?"
                  tail -1 "$C/compare.txt"
                  $CK census-gated "$C/compare-gated.txt" "$DER" || fail "census: an undeclared mover";;
  esac
  echo "STEP_OK $STEP $(date -u +%FT%TZ)"
  [ "$(git -C "$R" rev-parse HEAD)" = "$HEAD0" ] || fail "HEAD moved during the gate"
done
[ -z "$(git -C "$R" status --porcelain)" ] || { echo "CHAIN_NOTE tree dirty at the end: $(git -C "$R" status --porcelain | head -3)"; }
echo "CHAIN_END $(date -u +%FT%TZ) ALL_STEPS_OK"
