#!/usr/bin/env bash
# AP-2 B5 riders: the non-timing legs in sequence, each its own script under its own flock (each leg
# script checks pgrep inside the lock). usage: ARMB_SHA=<arm B sha256> bash chain1.sh   (the real pid has argv `bash chain1.sh`)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a17ce5e51d9c90f96
R=$WT/docs/notes/data/2026-11-m8-ap2-b5/riders
L=$R/legs
cd "$WT" || exit 1
mkdir -p "$R/iparm" "$R/census" "$R/suites" "$R/replay" "$R/install-smoke" "$R/pch" "$R/offref" "$R/accelerate" "$R/flags" "$WT/build-b5/out-r"
leg() { # name, then command...
  local name=$1; shift
  pgrep -a hven_bench_cell && { echo "PGREP_BUSY before $name"; return 3; }
  flock /tmp/box-build.lock "$@" > "$WT/build-b5/out-r/$name.out" 2>&1
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
