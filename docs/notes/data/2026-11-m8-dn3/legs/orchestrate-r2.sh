#!/usr/bin/env bash
# M8 DN-3 r2 (../DECLARATION-r2.md): the declared order after r1's failed chain -- legs/chain-r2.sh (P1-P4, P6), then
# sl/adopt_run.sh E, S, P, T, G, sl/adopt_verdict.py, sl/hb_restore.sh -- each leg under its own
# `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty before each, stopping at the first failure. TMPDIR is
# exported to build-dn3/tmp (the home volume) for every leg. P5 is NOT re-run: r1's P5 (runtime/run_all.log, VERDICT
# PASS) stands for r2 (the same code head and arm binaries; the gate writes nothing under /tmp).
# usage: orchestrate-r2.sh [all|chain|adopt]   (run in the foreground of the caller's turn; logs under build-dn3/legs-logs-r2,
# copied into legs-logs-r2/ of this directory at the end)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8
D=$WT/docs/notes/data/2026-11-m8-dn3
ARMB=3cb0aafba25eb7c310ef85863834819fc5d66459db342fa695c9e41ec0ba0c3f
HEAD=96ac1a27d085853ccc84326dfb36783fb63dab2e
STAT=3199459cbc87fe3438ec28d05945ad47a21c49fab09bf85f1ee4d30d8b3ac268
REC=20200e3fd610fa64c7a08baf6b121f773747df4626f8404e00635d499b44367f
LOGS=$WT/build-dn3/legs-logs-r2
export TMPDIR=$WT/build-dn3/tmp
mkdir -p "$LOGS" "$D/chain-r2" "$TMPDIR"
echo "TMPDIR=$TMPDIR"
run() { # <name> <logfile> <cmd...>
  local name=$1 log=$2; shift 2
  if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY before $name"; exit 3; fi
  echo "LEG_START $name $(date -Is)"
  flock /tmp/box-build.lock "$@" > "$log" 2>&1
  local rc=$?
  echo "LEG_END $name rc=$rc $(date -Is)"
  [ $rc -eq 0 ] || { echo "STOPPED at $name"; mkdir -p "$D/legs-logs-r2"; cp -p "$LOGS"/* "$D/legs-logs-r2/"; exit $rc; }
}
case "${1:-all}" in
all|chain) run CHAIN "$D/chain-r2/chain.out" bash "$D/legs/chain-r2.sh" "$HEAD" "$ARMB" ;;&
all|adopt)
  run E "$LOGS/E.log" bash "$D/sl/adopt_run.sh" E "$D" "$STAT" "$REC"
  run S "$LOGS/S.log" bash "$D/sl/adopt_run.sh" S "$D" "$STAT" "$REC"
  run P "$LOGS/P.log" bash "$D/sl/adopt_run.sh" P "$D" "$STAT" "$REC"
  run T "$LOGS/T.log" bash "$D/sl/adopt_run.sh" T "$D" "$STAT" "$REC"
  run G "$LOGS/G.log" bash "$D/sl/adopt_run.sh" G "$D" "$STAT" "$REC"
  python3 "$D/sl/adopt_verdict.py" "$D" > "$D/results/verdict.txt" 2>&1; echo "VERDICT rc=$? $(date -Is)"
  run RESTORE "$LOGS/restore.log" bash "$D/sl/hb_restore.sh"
  ;;
esac
mkdir -p "$D/legs-logs-r2"; cp -p "$LOGS"/* "$D/legs-logs-r2/" 2>/dev/null
echo "ORCHESTRATION_DONE $(date -Is)"
