#!/usr/bin/env bash
# WX-R1 gates: one flock per leg, PGREP_EMPTY / FOREGROUND_START / FOREGROUND_END markers.
ROOT=/home/ghecht/Projects/hven/.claude/worktrees/wx-r1
for leg in "$@"; do
 flock /tmp/box-build.lock bash -c '
  set -o pipefail
  echo "FOREGROUND_START $2 $(date -Is)"
  foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
  [ -z "$foreign" ] && echo "PGREP_EMPTY $2" || echo "NOTE foreign cell: $foreign"
  bash '"$ROOT"'/docs/notes/data/2026-10-m7-wx-r1/wrappers/legs.sh '"$ROOT"' "$2"
  rc=$?; echo "FOREGROUND_END $2 $(date -Is) rc=$rc"; exit $rc' _ x "$leg"
 echo "WRAPPER_RC $leg rc=$?"
done
