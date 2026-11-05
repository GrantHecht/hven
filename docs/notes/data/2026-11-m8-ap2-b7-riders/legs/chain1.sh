#!/usr/bin/env bash
# AP-2 B7 riders: the non-timing legs in sequence, each its own script under its own flock (each leg script
# checks pgrep inside the lock). usage: ARMB_SHA=<arm B sha256> bash chain1.sh  (the real pid: `bash chain1.sh`)
# PRE-FLIGHT (fail closed), over EVERY script and instrument in this directory, this file included: they name
# this worktree, this evidence directory and build-b7r/ only. A path of B7's own evidence (its build tree or
# its evidence directory) refuses the chain before the first leg, except the read-only pins listed in ALLOWED
# (B7's arm A sources and its three pins). The patterns are assembled from pieces so this file's own pattern
# line cannot match itself.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a00603e2048c1c37b
R=$WT/docs/notes/data/2026-11-m8-ap2-b7-riders
L=$R/legs
cd "$WT" || exit 1
B7E='build-b7''e/'
B7DIR='2026-11-m8-ap2-''b7/'
OLD='build-b''6|build-b''5|build-b7''/|agent-a79f97c30''e3369bfe|2026-11-m8-ap2-b6''-riders|m8-ap2-b5''/riders'
ALLOWED="${B7E}armBbin/hven_sqp_corpus|${B7E}rel/compile_commands.json|${B7DIR}iparm/iparm-b7.txt|${B7DIR}interior/interior-declared.txt|${B7DIR}accelerate"
SCAN=$(ls "$L"/*.sh "$L"/*.py "$R"/runtime/*.sh "$R"/runtime/*.py)
stale=$(grep -nE "${B7E}|${B7DIR}|${OLD}" $SCAN | grep -v '^[^:]*:[0-9]*:#' | grep -vE "^[^:]*:[0-9]*:.*(${ALLOWED})" )
if [ -n "$stale" ]; then echo "PREFLIGHT_STALE_PATH"; echo "$stale"; exit 9; fi
for f in "$L"/*.sh "$R"/runtime/*.sh; do
  grep -q 'WT=' "$f" || continue
  grep -q "^WT=$WT\$" "$f" || { echo "PREFLIGHT_WT_MISMATCH $f"; exit 9; }
done
# The two copied python instruments derive the root from their own location: checked here, not carried.
for p in "$L/accelerate_sweep.py" "$R/runtime/analyze_r5.py"; do
  got=$(python3 -c "import os,sys; print(os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])), '../../../../..')))" "$p")
  [ "$got" = "$WT" ] || { echo "PREFLIGHT_ROOT_DEPTH $p -> $got"; exit 9; }
done
[ -x "$WT/build-b7r/rel/bench/hven_sqp_corpus" ] || { echo "PREFLIGHT_NO_ARM_B_BUILD"; exit 9; }
[ -x "$WT/build-b7r/armAbin/hven_sqp_corpus" ] || { echo "PREFLIGHT_NO_ARM_A_BINARY"; exit 9; }
[ -f "$WT/build-b7r/armA-compile_commands.json" ] || { echo "PREFLIGHT_NO_ARM_A_DB"; exit 9; }
[ -f "$WT/build-b7r/dbg/build.ninja" ] || { echo "PREFLIGHT_NO_DEBUG_BUILD"; exit 9; }
[ -f "$WT/bench/baselines/2026-11-m8-b7-ipm-leg/interior_baseline.csv" ] || { echo "PREFLIGHT_NO_PROMOTED_BASELINE"; exit 9; }
echo PREFLIGHT_OK
mkdir -p "$R/iparm" "$R/census" "$R/suites" "$R/replay" "$R/install-smoke" "$R/pch" "$R/offref" "$R/accelerate" "$R/flags" "$R/chain" "$R/interior"
leg() { # name, then command...
  local name=$1; shift
  pgrep -a hven_bench_cell && { echo "PGREP_BUSY before $name"; return 3; }
  flock /tmp/box-build.lock "$@" > "$R/chain/$name.out" 2>&1
  echo "LEG $name rc=$? $(date -Is)"
}
leg replay        bash $L/replay.sh "$R/replay"
leg interior      bash $L/interior.sh "$R/interior" ${ARMB_SHA:?set ARMB_SHA}
leg iparm         bash $L/iparm.sh
leg ctest-release bash $L/ctest.sh rel "$R/suites/ctest-release.log"
leg ctest-debug   bash $L/ctest.sh dbg "$R/suites/ctest-debug.log"
leg examples      bash $L/examples.sh
leg smoke         bash $L/install_smoke.sh "$R/install-smoke"
leg pch           bash $L/misc.sh pch "$R/pch"
leg accelerate    bash $L/misc.sh accelerate "$R/accelerate"
leg offref        bash $L/offref.sh "$R/offref"
leg flags         bash $L/flags.sh "$R/flags"
leg census        bash $L/census.sh "$R/census" ${ARMB_SHA:?set ARMB_SHA}
echo CHAIN1_DONE
