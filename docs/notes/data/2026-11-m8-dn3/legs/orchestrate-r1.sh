#!/usr/bin/env bash
# M8 DN-3 r1: the scratch orchestration that ran r1 (copied here as the record after the fact; it was not in r1's commit). It ran P5 (PASS) and started the chain, whose P1 failed on /tmp quota; it was then stopped (the chain too). Superseded by orchestrate-r2.sh.
# DN-3 orchestration (scratch): the declared order, each leg under its own flock, stop at the first failure.
set -o pipefail
D=/home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8/docs/notes/data/2026-11-m8-dn3
ARMB=3cb0aafba25eb7c310ef85863834819fc5d66459db342fa695c9e41ec0ba0c3f
HEAD=96ac1a27d085853ccc84326dfb36783fb63dab2e
STAT=3199459cbc87fe3438ec28d05945ad47a21c49fab09bf85f1ee4d30d8b3ac268
REC=20200e3fd610fa64c7a08baf6b121f773747df4626f8404e00635d499b44367f
LOGS=/home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8/build-dn3/legs-logs
mkdir -p "$LOGS" "$D/chain"
run() { # <name> <logfile> <cmd...>
  local name=$1 log=$2; shift 2
  if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY before $name"; exit 3; fi
  echo "LEG_START $name $(date -Is)"
  flock /tmp/box-build.lock "$@" > "$log" 2>&1
  local rc=$?
  echo "LEG_END $name rc=$rc $(date -Is)"
  [ $rc -eq 0 ] || { echo "STOPPED at $name"; exit $rc; }
}
case "${1:-all}" in
all|p5) run P5 "$D/runtime/run_all.log" bash "$D/runtime/run_all.sh" "$ARMB" ;;&
all|chain) run CHAIN "$D/chain/chain.out" bash "$D/legs/chain.sh" "$HEAD" "$ARMB" ;;&
all|adopt)
  run E "$LOGS/E.log" bash "$D/sl/adopt_run.sh" E "$D" "$STAT" "$REC"
  run S "$LOGS/S.log" bash "$D/sl/adopt_run.sh" S "$D" "$STAT" "$REC"
  run P "$LOGS/P.log" bash "$D/sl/adopt_run.sh" P "$D" "$STAT" "$REC"
  run T "$LOGS/T.log" bash "$D/sl/adopt_run.sh" T "$D" "$STAT" "$REC"
  run G "$LOGS/G.log" bash "$D/sl/adopt_run.sh" G "$D" "$STAT" "$REC"
  python3 "$D/sl/adopt_verdict.py" "$D" > "$D/results/verdict.txt" 2>&1; echo "VERDICT rc=$?"
  run RESTORE "$LOGS/restore.log" bash "$D/sl/hb_restore.sh"
  ;;
esac
echo "ORCHESTRATION_DONE $(date -Is)"
