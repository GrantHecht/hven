#!/usr/bin/env bash
# EVERY wrapper this gate uses, proven against a STUBBED failure.  Each stub
# makes one inner command fail while the wrapper's LATER commands SUCCEED, so a
# wrapper that reported the last command's status (a trailing tail, grep, count,
# sha print or summary echo) would exit 0 and be caught here.  Expected: every
# WRAPPER_RC below is NON-ZERO.  Run inside the box lock through run_leg.sh.
#
# NOT HERE, AND WHY (the W6 T6b lesson, .scratch/w6t7/fold_proof.sh:8-13):
# run_leg.sh's own failure-propagation proof invokes run_leg.sh, which takes
# /tmp/box-build.lock -- and this script already runs inside that lock, so a
# nested invocation DEADLOCKS against its own parent.  That proof is its own leg
# (run_leg_failure_proof.sh); only the GUARD refusal, which exits BEFORE the
# lock is taken, is proven from in here.
set -uo pipefail
R=/home/ghecht/Projects/hven
F=$R/.scratch/m6-close
P=$F/foldproof
rm -rf "$P"; mkdir -p "$P/bin" "$P/scripts"
fails=0
check() { local name="$1" rc="$2"
  if [ "$rc" -ne 0 ]; then echo "FOLD OK   $name -> WRAPPER_RC=$rc (non-zero, as required)";
  else echo "FOLD FAIL $name -> WRAPPER_RC=0 (a failure was MASKED)"; fails=$((fails+1)); fi
}

cat > "$P/bin/cmake" <<'STUB'
#!/bin/sh
echo "STUB cmake: deliberate failure" >&2
exit 1
STUB
# A ctest stub that FAILS and prints NO summary line -- the shape rig_leg.sh and
# rig_full.sh guard against, since they deliberately do not fold ctest's status.
cat > "$P/bin/ctest_nosummary" <<'STUB'
#!/bin/sh
echo "STUB ctest: deliberate failure, no summary line" >&2
exit 8
STUB
cat > "$P/bin/refuse_cmp.py" <<'STUB'
import sys
sys.stderr.write("STUB comparator: deliberate refusal\n")
sys.exit(3)
STUB
cat > "$P/bin/corpus_stub" <<'STUB'
#!/bin/sh
echo "STUB hven_sqp_corpus: deliberate failure" >&2
exit 5
STUB
# A corpus stub whose STAMP PREFLIGHT succeeds but prints a -dirty stamp: the
# census wrapper must refuse it rather than start a multi-hour sweep.
cat > "$P/bin/corpus_dirty" <<'STUB'
#!/bin/sh
echo "# hven_sqp_corpus provenance"
echo "# binary: deadbeef1234-dirty"
echo "# schema: 76"
exit 0
STUB
# A smoke script that FAILS after leaving a well-formed install and configure
# log behind, so the export-contract half (which PASSES) runs afterwards and
# could mask it.
cat > "$P/scripts/check_install_smoke.sh" <<'STUB'
#!/bin/sh
mkdir -p "$HVEN_INSTALL_SMOKE_WORKDIR/hven-install/lib64/cmake/hven"
: > "$HVEN_INSTALL_SMOKE_WORKDIR/hven-install/lib64/cmake/hven/hvenTargets.cmake"
echo "hven: EIGEN_MAX_ALIGN_BYTES resolved to 32 (stub)" > "$HVEN_INSTALL_SMOKE_WORKDIR/hven-configure.log"
echo "STUB install smoke: deliberate failure" >&2
exit 1
STUB
cat > "$P/scripts/check_export_contract.sh" <<'STUB'
#!/bin/sh
echo "STUB export contract: PASSED (it must not mask the smoke failure)"
exit 0
STUB
chmod +x "$P/bin/cmake" "$P/bin/ctest_nosummary" "$P/bin/corpus_stub" \
         "$P/bin/corpus_dirty" "$P/scripts/check_install_smoke.sh" \
         "$P/scripts/check_export_contract.sh"

echo "=== 1. run_leg.sh: the nested-shell guard REFUSES a bash -c without pipefail ==="
bash "$F/run_leg.sh" "$P/run_leg-guard.log" sh -c 'exit 7' > "$P/run_leg-guard.stdout" 2>&1
check "run_leg.sh(guard refusal)" "$?"
grep -E 'REFUSED|LEG_RC|WRAPPER_EXIT' "$P/run_leg-guard.log"

echo
echo "=== 2. configure_one.sh (cmake stub fails; the trailing tail and grep SUCCEED) ==="
PATH="$P/bin:$PATH" bash "$F/configure_one.sh" "$P/build-stub" Release > "$P/configure.log" 2>&1
check "configure_one.sh" "$?"; grep -E '_RC=' "$P/configure.log"

echo
echo "=== 3. build_one.sh (cmake stub fails; the trailing grep SUCCEEDS) ==="
PATH="$P/bin:$PATH" bash "$F/build_one.sh" "$P/build-stub" foldproof --target hven > "$P/build.log" 2>&1
check "build_one.sh" "$?"; grep -E '_RC=' "$P/build.log"

echo
echo "=== 4. suite.sh (ctest fails on a nonexistent build dir; the trailing"
echo "       inventory line would otherwise be the status) ==="
bash "$F/suite.sh" foldproof "$P/no-such-build-dir" > "$P/suite.log" 2>&1
check "suite.sh" "$?"; grep -E '_RC=' "$P/suite.log"

echo
echo "=== 5. smoke.sh (the install smoke FAILS; the export contract PASSES after"
echo "        it and must not mask it) ==="
HVEN_CLOSE_SCRIPTS="$P/scripts" bash "$F/smoke.sh" > "$P/smoke.log" 2>&1
check "smoke.sh" "$?"; grep -E '_RC=' "$P/smoke.log"

echo
echo "=== 6. replay_arm.sh (the FIRST engine's run fails; the later engines and"
echo "        the trailing summary would otherwise succeed) ==="
HVEN_CLOSE_REPLAY_BIN="$P/bin/corpus_stub" bash "$F/replay_arm.sh" foldproofarm > "$P/replayarm.log" 2>&1
check "replay_arm.sh" "$?"; grep -E '_RC=|FAILED' "$P/replayarm.log"

echo
echo "=== 7. replay_cmp.sh (the comparator REFUSES every pair; the trailing stamp"
echo "        grep SUCCEEDS) ==="
mkdir -p "$F/replay"; : > "$F/replay/head-ipm.csv"
HVEN_CLOSE_CMP="$P/bin/refuse_cmp.py" bash "$F/replay_cmp.sh" > "$P/replaycmp.log" 2>&1
check "replay_cmp.sh" "$?"; grep -E '_RC=' "$P/replaycmp.log"
rm -f "$F/replay/head-ipm.csv"

echo
echo "=== 8a. census_leg.sh (the corpus binary FAILS its stamp preflight; the"
echo "         multi-hour sweep must never start) ==="
HVEN_CLOSE_CENSUS_BIN="$P/bin/corpus_stub" HVEN_CLOSE_CENSUS_OUT="$P/census-stub" \
    bash "$F/census_leg.sh" > "$P/census1.log" 2>&1
check "census_leg.sh(preflight failure)" "$?"; grep -E '_RC=|FATAL' "$P/census1.log"

echo
echo "=== 8b. census_leg.sh (the preflight SUCCEEDS but the stamp reads -dirty --"
echo "         the gate's provenance requirement, run_walk_census.sh:49-57) ==="
HVEN_CLOSE_CENSUS_BIN="$P/bin/corpus_dirty" HVEN_CLOSE_CENSUS_OUT="$P/census-stub2" \
    bash "$F/census_leg.sh" > "$P/census2.log" 2>&1
check "census_leg.sh(-dirty stamp)" "$?"; grep -E '_RC=|FATAL|binary:' "$P/census2.log"

echo
echo "=== 9. census_serial_rerun.sh (the solve FAILS; the comparator step and the"
echo "        trailing greps would otherwise succeed) ==="
HVEN_CLOSE_CENSUS_BIN="$P/bin/corpus_stub" bash "$F/census_serial_rerun.sh" f7_stub_cell \
    > "$P/serial.log" 2>&1
check "census_serial_rerun.sh" "$?"; grep -E '_RC=' "$P/serial.log"

echo
echo "=== 10. rig_leg.sh (ctest fails AND prints no summary line -- the wrapper"
echo "         does not fold ctest's status, so this is the guard that catches it) ==="
HVEN_CLOSE_CTEST="$P/bin/ctest_nosummary" HVEN_CLOSE_RIG_BLD="$P/no-such-build-dir" \
    bash "$F/rig_leg.sh" > "$P/rigleg.log" 2>&1
check "rig_leg.sh" "$?"; grep -E '_RC=|WRAPPER FAILURE' "$P/rigleg.log"

echo
echo "=== 11. rig_full.sh (same shape) ==="
HVEN_CLOSE_CTEST="$P/bin/ctest_nosummary" HVEN_CLOSE_RIG_BLD="$P/no-such-build-dir" \
    bash "$F/rig_full.sh" > "$P/rigfull.log" 2>&1
check "rig_full.sh" "$?"; grep -E '_RC=|WRAPPER FAILURE' "$P/rigfull.log"

echo
echo "FOLD_PROOF_FAILURES=$fails"
[ "$fails" -eq 0 ] || exit 1
echo "FOLD_PROOF_WORST_RC=0  (every wrapper propagated its stubbed failure)"
exit 0
