#!/usr/bin/env bash
# M8 PF-HB: the non-timing legs in sequence, each its own script under its own flock (each leg script checks pgrep
# inside the lock). usage: ARMB_SHA=<arm B sha256> bash chain1.sh   (wait on the pid of `bash chain1.sh`)
# PRE-FLIGHT (fail closed), over EVERY script and instrument in this directory, this file included: they name this
# worktree, this evidence directory and build-pfe/ only. A path of another worktree, another build tree (build-b*,
# build-pfhb -- the dev tree) or another evidence directory refuses the chain before the first leg, except the
# read-only pins in ALLOWED. The patterns are assembled from pieces so this file's own pattern lines cannot match
# themselves.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-ab44eb20498de1642
R=$WT/docs/notes/data/2026-11-m8-pf-huge-bound
L=$R/legs
cd "$WT" || exit 1
OTHER_WT='worktrees/agent-a''[0-9a-f]{15}'
OTHER_BUILD='build-(b[0-9]|pf''hb)'
OTHER_EV='docs/notes/data/20''[0-9]{2}-[0-9]{2}-'
# The read-only pins (DECLARATION P3/P4/P5/P9/P12) and the replay cell list.
ALLOWED='2026-10-m7-wx2-gates/gates/replay|2026-11-m8-ap2-b8-''riders/iparm/iparm-b8r\.txt|2026-11-m8-ap2-b8-''riders/accelerate'
ALLOWED="${ALLOWED}|2026-08-m6-w1-acceptance/u0-replay-cells\.txt|2026-11-m8-pf-""huge-bound"
# flags_diff.py (byte copy since B3) filters an M8 B1 scratch tree out of a compile database: a string, not a path.
ALLOWED="${ALLOWED}|and '/build-b""1/' not in f\}"
SCAN=$(cd "$R" && ls legs/*.sh legs/*.py runtime/*.sh runtime/*.py)
stale=$(cd "$R" && grep -nE "${OTHER_BUILD}|${OTHER_EV}" $SCAN | grep -vE '^[^:]*:[0-9]*:[[:space:]]*#' |
        grep -vE "^[^:]*:[0-9]*:.*(${ALLOWED})")
stale2=$(cd "$R" && grep -n "" $SCAN | sed 's/agent-ab44eb20498de1642//g' | grep -E "${OTHER_WT}" |
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
grep -q "build-pfe/rel/compile_commands.json" "$L/accelerate_sweep.py" || { echo "PREFLIGHT_SWEEP_DB"; exit 9; }
[ -x "$WT/build-pfe/rel/bench/hven_sqp_corpus" ] || { echo "PREFLIGHT_NO_ARM_B_BUILD"; exit 9; }
[ -x "$WT/build-pfe/armAbin/hven_sqp_corpus" ] || { echo "PREFLIGHT_NO_ARM_A_BINARY"; exit 9; }
[ -f "$WT/build-pfe/armA-compile_commands.json" ] || { echo "PREFLIGHT_NO_ARM_A_DB"; exit 9; }
[ -f "$WT/build-pfe/dbg/build.ninja" ] || { echo "PREFLIGHT_NO_DEBUG_BUILD"; exit 9; }
[ -f "$WT/bench/baselines/2026-11-m8-b7-ipm-leg/interior_baseline.csv" ] || { echo "PREFLIGHT_NO_PROMOTED_BASELINE"; exit 9; }
[ -f "$WT/docs/notes/data/2026-11-m8-ap2-b8-riders/iparm/iparm-b8r.txt" ] || { echo "PREFLIGHT_NO_IPARM_PIN"; exit 9; }
[ -f "$WT/docs/notes/data/2026-11-m8-ap2-b8-riders/accelerate/accelerate-sweep.txt" ] || { echo "PREFLIGHT_NO_ACCEL_PIN"; exit 9; }
echo PREFLIGHT_OK
[ -n "$PREFLIGHT_ONLY" ] && exit 0   # a dry pre-flight (run before the declaration)
mkdir -p "$R/iparm" "$R/census" "$R/suites" "$R/replay" "$R/install-smoke" "$R/pch" "$R/offref" "$R/accelerate" "$R/flags" "$R/chain" "$R/interior"
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
leg census        bash $L/census.sh "$R/census" ${ARMB_SHA:?set ARMB_SHA}
echo CHAIN1_DONE
