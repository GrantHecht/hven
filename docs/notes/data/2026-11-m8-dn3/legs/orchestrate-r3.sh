#!/usr/bin/env bash
# M8 DN-3 r3 (../DECLARATION-r3.md): r2's orchestrator (orchestrate-r2.sh) with the chain, E and S removed (they passed
# under r2) and one step added first: the two r2 records with a LOST event are VOIDED (moved to record/void-r2/, kept) and
# `sl/adopt_run.sh P` re-takes exactly those two, alone under the lock (r0 S-A2's remedy); then T, G, the verdict and the
# restore. Each leg under its own `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty first, stopping at the
# first failure. TMPDIR as r2. Logs: build-dn3/legs-logs-r3, copied to legs-logs-r3/ of this directory.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8
D=$WT/docs/notes/data/2026-11-m8-dn3
STAT=3199459cbc87fe3438ec28d05945ad47a21c49fab09bf85f1ee4d30d8b3ac268
REC=20200e3fd610fa64c7a08baf6b121f773747df4626f8404e00635d499b44367f
LOGS=$WT/build-dn3/legs-logs-r3
export TMPDIR=$WT/build-dn3/tmp
mkdir -p "$LOGS" "$TMPDIR" "$D/record/void-r2"
echo "TMPDIR=$TMPDIR"
for v in sd-hven-ipm-SPANHYD.N sd-hven-sqp-HS92.N; do
  if [ -f "$D/record/$v.attr.json" ]; then
    mv "$D/record/$v.runs" "$D/record/$v.attr.json" "$D/record/void-r2/" || exit 4
    echo "VOIDED $v"
  fi
done
run() { # <name> <logfile> <cmd...>
  local name=$1 log=$2; shift 2
  if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY before $name"; exit 3; fi
  echo "LEG_START $name $(date -Is)"
  flock /tmp/box-build.lock "$@" > "$log" 2>&1
  local rc=$?
  echo "LEG_END $name rc=$rc $(date -Is)"
  [ $rc -eq 0 ] || { echo "STOPPED at $name"; mkdir -p "$D/legs-logs-r3"; cp -p "$LOGS"/* "$D/legs-logs-r3/"; exit $rc; }
}
run P-retake "$LOGS/P-retake.log" bash "$D/sl/adopt_run.sh" P "$D" "$STAT" "$REC"
[ "$(grep -c RECORD_DONE "$LOGS/P-retake.log")" = 2 ] || { echo "RETAKE_COUNT_NOT_2"; cp -p "$LOGS"/* "$D/legs-logs-r3/" 2>/dev/null; exit 4; }
run T "$LOGS/T.log" bash "$D/sl/adopt_run.sh" T "$D" "$STAT" "$REC"
run G "$LOGS/G.log" bash "$D/sl/adopt_run.sh" G "$D" "$STAT" "$REC"
python3 "$D/sl/adopt_verdict.py" "$D" > "$D/results/verdict.txt" 2>&1; echo "VERDICT rc=$? $(date -Is)"
run RESTORE "$LOGS/restore.log" bash "$D/sl/hb_restore.sh"
mkdir -p "$D/legs-logs-r3"; cp -p "$LOGS"/* "$D/legs-logs-r3/" 2>/dev/null
echo "ORCHESTRATION_DONE $(date -Is)"
