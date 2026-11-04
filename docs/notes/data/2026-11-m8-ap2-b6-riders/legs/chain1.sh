#!/usr/bin/env bash
# AP-2 B6 riders: the non-timing legs in sequence, each its own script under its own flock (each leg
# script checks pgrep inside the lock). usage: ARMB_SHA=<arm B sha256> bash chain1.sh   (the real pid has argv `bash chain1.sh`)
# PRE-FLIGHT (fail closed): every leg script and the analyzer in this directory names this worktree, the
# riders evidence directory and build-b6r/ only; a stale path refuses the chain before the first leg.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a79f97c30e3369bfe
R=$WT/docs/notes/data/2026-11-m8-ap2-b6-riders
L=$R/legs
cd "$WT" || exit 1
stale=$(grep -nE 'build-b6/|build-b5|2026-11-m8-ap2-b6/(legs|runtime|suites|replay|census|install-smoke|pch|offref|accelerate|flags)|/m8-ap2-b5/riders/(legs|runtime)' \
  "$L"/*.sh "$L"/*.py "$R"/runtime/*.sh | grep -v '^[^:]*:[0-9]*:#')
if [ -n "$stale" ]; then echo "PREFLIGHT_STALE_PATH"; echo "$stale"; exit 9; fi
for f in "$L"/*.sh "$R"/runtime/*.sh; do
  grep -q "WT=$WT\$" "$f" || grep -q 'WT=' "$f" || continue
  grep -q "^WT=$WT\$" "$f" || { echo "PREFLIGHT_WT_MISMATCH $f"; exit 9; }
done
[ -x "$WT/build-b6r/rel/bench/hven_sqp_corpus" ] || { echo "PREFLIGHT_NO_ARM_B_BUILD"; exit 9; }
[ -f "$WT/build-b6r/armA/compile_commands.json" ] || { echo "PREFLIGHT_NO_ARM_A_DB"; exit 9; }
echo PREFLIGHT_OK
mkdir -p "$R/iparm" "$R/census" "$R/suites" "$R/replay" "$R/install-smoke" "$R/pch" "$R/offref" "$R/accelerate" "$R/flags" "$R/chain"
leg() { # name, then command...
  local name=$1; shift
  pgrep -a hven_bench_cell && { echo "PGREP_BUSY before $name"; return 3; }
  flock /tmp/box-build.lock "$@" > "$R/chain/$name.out" 2>&1
  echo "LEG $name rc=$? $(date -Is)"
}
leg replay        bash $L/replay.sh "$R/replay"
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
