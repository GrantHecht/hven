#!/usr/bin/env bash
# AP-2 B5 riders-r2: the proof legs in sequence, each its own script under its own flock (each leg
# script checks pgrep inside the lock). usage: bash chain.sh   (the real pid has argv `bash chain.sh`)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a17ce5e51d9c90f96
R=$WT/docs/notes/data/2026-11-m8-ap2-b5/riders-r2
L=$R/legs
cd "$WT" || exit 1
mkdir -p "$R/suites" "$R/replay" "$R/pch" "$R/offref" "$WT/build-b5/out-r2"
leg() { # name, then command...
  local name=$1; shift
  pgrep -a hven_bench_cell && { echo "PGREP_BUSY before $name"; return 3; }
  flock /tmp/box-build.lock "$@" > "$WT/build-b5/out-r2/$name.out" 2>&1
  echo "LEG $name rc=$? $(date -Is)"
}
leg build-release bash $L/build.sh Release rel
leg build-debug   bash $L/build.sh Debug dbg
leg replay        bash $L/replay.sh "$R/replay"
leg ctest-release bash $L/ctest.sh rel "$R/suites/ctest-release.log"
leg ctest-debug   bash $L/ctest.sh dbg "$R/suites/ctest-debug.log"
leg pch           bash $L/misc.sh pch "$R/pch"
leg offref        bash $L/offref.sh "$R/offref"
echo CHAIN_DONE
