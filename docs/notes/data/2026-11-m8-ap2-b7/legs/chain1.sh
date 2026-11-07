#!/usr/bin/env bash
# AP-2 B7: the non-timing legs in sequence, each its own script under its own flock (each leg script checks
# pgrep inside the lock). usage: ARMB_SHA=<arm B sha256> bash chain1.sh   (the real pid has argv `bash chain1.sh`)
# PRE-FLIGHT (fail closed): every leg script, the analyzer and the runtime scripts in this directory name this
# worktree, this evidence directory and build-b7e/ only; a stale path (the B6 riders' worktree, build-b6r/,
# their evidence directory, or B6's own leg directories) refuses the chain before the first leg. The one B6
# path a leg may name is the iparm PIN (2026-11-m8-ap2-b6/iparm/iparm-b6.txt), which is read, never written.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a00603e2048c1c37b
R=$WT/docs/notes/data/2026-11-m8-ap2-b7
L=$R/legs
cd "$WT" || exit 1
stale=$(grep -nE 'agent-a79f97c30e3369bfe|build-b6|build-b5|build-b7/|2026-11-m8-ap2-b6-riders|2026-11-m8-ap2-b6/(legs|runtime|suites|replay|census|install-smoke|pch|offref|accelerate|flags)|/m8-ap2-b5/riders/(legs|runtime)' \
  $(ls "$L"/*.sh "$L"/*.py "$R"/runtime/*.sh "$R"/runtime/*.py | grep -v -e '/chain1.sh$') | grep -v '^[^:]*:[0-9]*:#')
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
[ -x "$WT/build-b7e/rel/bench/hven_sqp_corpus" ] || { echo "PREFLIGHT_NO_ARM_B_BUILD"; exit 9; }
[ -x "$WT/build-b7e/armAbin/hven_sqp_corpus" ] || { echo "PREFLIGHT_NO_ARM_A_BINARY"; exit 9; }
[ -f "$WT/build-b7e/armA/compile_commands.json" ] || { echo "PREFLIGHT_NO_ARM_A_DB"; exit 9; }
[ -f "$R/count/count-declared.txt" ] || { echo "PREFLIGHT_NO_DECLARED_COUNT"; exit 9; }
[ -f "$R/interior/interior-declared.txt" ] || { echo "PREFLIGHT_NO_DECLARED_INTERIOR"; exit 9; }
echo PREFLIGHT_OK
mkdir -p "$R/iparm" "$R/census" "$R/suites" "$R/replay" "$R/install-smoke" "$R/pch" "$R/offref" "$R/accelerate" "$R/flags" "$R/chain" "$R/count" "$R/interior"
leg() { # name, then command...
  local name=$1; shift
  pgrep -a hven_bench_cell && { echo "PGREP_BUSY before $name"; return 3; }
  flock /tmp/box-build.lock "$@" > "$R/chain/$name.out" 2>&1
  echo "LEG $name rc=$? $(date -Is)"
}
leg count         bash $L/count.sh "$R/count"
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
