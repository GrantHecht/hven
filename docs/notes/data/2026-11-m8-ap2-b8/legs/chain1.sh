#!/usr/bin/env bash
# AP-2 B8: the non-timing legs in sequence, each its own script under its own flock (each leg script checks
# pgrep inside the lock). usage: ARMB_SHA=<arm B sha256> bash chain1.sh   (wait on the pid of `bash chain1.sh`)
# PRE-FLIGHT (fail closed), over EVERY script and instrument in this directory, this file included: they
# name this worktree, this evidence directory and build-b8/ only. A path of another worktree, another build
# tree or another evidence directory refuses the chain before the first leg, except the read-only pins in
# ALLOWED (B7 riders' iparm dump and Accelerate sweeps). The patterns are assembled from pieces so this
# file's own pattern lines cannot match themselves.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aad152e17d08967f4
R=$WT/docs/notes/data/2026-11-m8-ap2-b8
L=$R/legs
cd "$WT" || exit 1
OTHER_WT='worktrees/agent-a''[0-9a-f]{15}'
OLD_BUILD='build-b''[1-7]'
OLD_EV='2026-11-m8-ap2-''b[1-7]'
ALLOWED='2026-11-m8-ap2-''b7-riders/iparm/iparm-b7r''\.txt|2026-11-m8-ap2-''b7-riders/accelerate'
# flags_diff.py (byte copy since B3) filters an M8 B1 scratch tree out of a compile database: a string, not a path it reads.
ALLOWED="${ALLOWED}|and '/build-b""1/' not in f\}"
# Relative names (cd "$R"), so a listed file name never carries this worktree's own path. This worktree's
# name is deleted from each line before the other-worktree pattern is applied.
SCAN=$(cd "$R" && ls legs/*.sh legs/*.py runtime/*.sh runtime/*.py psym/*.py)
stale=$(cd "$R" && grep -nE "${OLD_BUILD}|${OLD_EV}" $SCAN | grep -vE '^[^:]*:[0-9]*:[[:space:]]*#' |
        grep -vE "^[^:]*:[0-9]*:.*(${ALLOWED})")
stale2=$(cd "$R" && grep -n "" $SCAN | sed 's/agent-aad152e17d08967f4//g' | grep -E "${OTHER_WT}" |
         grep -vE '^[^:]*:[0-9]*:[[:space:]]*#')
if [ -n "$stale$stale2" ]; then echo "PREFLIGHT_STALE_PATH"; echo "$stale"; echo "$stale2"; exit 9; fi
for f in "$L"/*.sh "$R"/runtime/*.sh; do
  grep -q 'WT=' "$f" || continue
  grep -q "^WT=$WT\$" "$f" || { echo "PREFLIGHT_WT_MISMATCH $f"; exit 9; }
done
# The copied python instruments derive the root from their own location: checked here, not carried.
for p in "$L/accelerate_sweep.py" "$R/runtime/analyze_r5.py"; do
  got=$(python3 -c "import os,sys; print(os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])), '../../../../..')))" "$p")
  [ "$got" = "$WT" ] || { echo "PREFLIGHT_ROOT_DEPTH $p -> $got"; exit 9; }
done
[ -x "$WT/build-b8/rel/bench/hven_sqp_corpus" ] || { echo "PREFLIGHT_NO_ARM_B_BUILD"; exit 9; }
[ -x "$WT/build-b8/armAbin/hven_sqp_corpus" ] || { echo "PREFLIGHT_NO_ARM_A_BINARY"; exit 9; }
[ -f "$WT/build-b8/armA-compile_commands.json" ] || { echo "PREFLIGHT_NO_ARM_A_DB"; exit 9; }
[ -f "$WT/build-b8/dbg/build.ninja" ] || { echo "PREFLIGHT_NO_DEBUG_BUILD"; exit 9; }
[ -d "$WT/build-b8/psym-before" ] || { echo "PREFLIGHT_NO_PSYM_BEFORE"; exit 9; }
[ -f "$R/psym/exceptions.txt" ] || { echo "PREFLIGHT_NO_PSYM_EXCEPTIONS"; exit 9; }
[ -f "$WT/bench/baselines/2026-11-m8-b7-ipm-leg/interior_baseline.csv" ] || { echo "PREFLIGHT_NO_PROMOTED_BASELINE"; exit 9; }
echo PREFLIGHT_OK
mkdir -p "$R/iparm" "$R/census" "$R/suites" "$R/replay" "$R/install-smoke" "$R/pch" "$R/offref" "$R/accelerate" "$R/flags" "$R/chain" "$R/interior" "$R/psym"
leg() { # name, then command...
  local name=$1; shift
  pgrep -a hven_bench_cell && { echo "PGREP_BUSY before $name"; return 3; }
  flock /tmp/box-build.lock "$@" > "$R/chain/$name.out" 2>&1
  echo "LEG $name rc=$? $(date -Is)"
}
leg arma          bash $L/armA.sh
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
leg psym          bash $L/psym.sh "$R/psym"
leg manifest      python3 $L/manifest_check.py
leg census        bash $L/census.sh "$R/census" ${ARMB_SHA:?set ARMB_SHA}
echo CHAIN1_DONE
