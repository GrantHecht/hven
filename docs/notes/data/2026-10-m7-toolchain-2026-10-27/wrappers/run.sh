#!/usr/bin/env bash
ROOT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a7d1a449e03873b3a
for leg in "$@"; do
 flock /tmp/box-build.lock bash -c '
  set -o pipefail
  echo "FOREGROUND_START $2 $(date -Is)"
  foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
  [ -z "$foreign" ] && echo "PGREP_EMPTY $2" || echo "NOTE foreign cell: $foreign"
  bash '"$ROOT"'/docs/notes/data/2026-10-m7-toolchain-2026-10-27/wrappers/legs.sh '"$ROOT"' "$2"
  rc=$?; echo "FOREGROUND_END $2 $(date -Is) rc=$rc"; exit $rc' _ x "$leg"
 echo "WRAPPER_RC $leg rc=$?"
done
