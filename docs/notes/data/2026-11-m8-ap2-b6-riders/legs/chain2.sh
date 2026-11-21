#!/usr/bin/env bash
# AP-2 B6 riders, DECLARATION-r3: the re-run on code head e3c634c5 after the failed Debug suite leg of
# chain1 (r2). Rebuilds build-b6r/rel and build-b6r/dbg incrementally at the head (legs/build.sh), checks
# fail-closed that the corpus binary is byte-identical to arm B (so the runtime gate and the census, which
# measure only that binary, carry), then every non-census leg of chain1 into r3/ subdirectories.
# usage: ARMB_SHA=<arm B sha256> bash chain2.sh   (the real pid has argv `bash chain2.sh`)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a79f97c30e3369bfe
R=$WT/docs/notes/data/2026-11-m8-ap2-b6-riders
L=$R/legs
E=$R/r3
cd "$WT" || exit 1
stale=$(grep -nE 'build-b6/|build-b5|2026-11-m8-ap2-b6/(legs|runtime|suites|replay|census|install-smoke|pch|offref|accelerate|flags)|/m8-ap2-b5/riders/(legs|runtime)' \
  $(ls "$L"/*.sh "$L"/*.py | grep -v -e '/chain1.sh$' -e '/chain2.sh$' -e '/armA_config.sh$') | grep -v '^[^:]*:[0-9]*:#')
if [ -n "$stale" ]; then echo "PREFLIGHT_STALE_PATH"; echo "$stale"; exit 9; fi
[ "$(git rev-parse --short=8 HEAD)" != "" ] || exit 9
echo "PREFLIGHT_OK head $(git log --oneline -1 | cut -c1-8)"
mkdir -p "$E/iparm" "$E/suites" "$E/replay" "$E/install-smoke" "$E/pch" "$E/offref" "$E/accelerate" "$E/flags" "$E/chain"
leg() { # name, then command...
  local name=$1; shift
  pgrep -a hven_bench_cell && { echo "PGREP_BUSY before $name"; return 3; }
  flock /tmp/box-build.lock "$@" > "$E/chain/$name.out" 2>&1
  local rc=$?
  echo "LEG $name rc=$rc $(date -Is)"
  return $rc
}
leg build-rel bash $L/build.sh Release rel || { echo BUILD_FAIL; exit 1; }
leg build-dbg bash $L/build.sh Debug dbg || { echo BUILD_FAIL; exit 1; }
GOT=$(sha256sum "$WT/build-b6r/rel/bench/hven_sqp_corpus" | cut -d' ' -f1)
echo "corpus binary sha256 $GOT"
[ "$GOT" = "${ARMB_SHA:?set ARMB_SHA}" ] || { echo "CORPUS_BINARY_MOVED"; exit 4; }
echo CORPUS_BINARY_IDENTICAL_TO_ARM_B
leg replay        bash $L/replay.sh "$E/replay"
leg iparm         env RIDERS_IPARM_EV="$E/iparm" bash $L/iparm.sh
leg ctest-release bash $L/ctest.sh rel "$E/suites/ctest-release.log"
leg ctest-debug   bash $L/ctest.sh dbg "$E/suites/ctest-debug.log"
leg examples      bash $L/examples.sh
leg smoke         bash $L/install_smoke.sh "$E/install-smoke"
leg pch           bash $L/misc.sh pch "$E/pch"
leg accelerate    bash $L/misc.sh accelerate "$E/accelerate"
leg offref        bash $L/offref.sh "$E/offref"
leg flags         bash $L/flags.sh "$E/flags"
echo CHAIN2_DONE
