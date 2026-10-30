#!/usr/bin/env bash
# THE BOX PROTOCOL, in one place.  usage: run_leg.sh <logfile> <command ...>
#   1. pgrep -x on process names (hven_* binaries incl. hven_bench_cell, ctest, ninja, cmake) -> PGREP_SEPARATE; the caller ALSO ran
#      `pgrep -a hven_bench_cell` as its own command before the chain;
#   2. flock /tmp/box-build.lock;
#   3. INSIDE the lock: pgrep again; NONEMPTY refuses the leg (exit 75) -- PGREP_EMPTY is printed only after
#      the check found nothing; then FOREGROUND_START, the leg, LEG_RC, FOREGROUND_END, all in the retained log;
#   4. the wrapper's exit status IS the leg's (the status leaves the lock explicitly).
# `set -o pipefail` is on in the wrapper and in the inner shell; a nested `bash -c` without pipefail is REFUSED.
# No `timeout` wrapper anywhere.  Nothing is ever signalled.  Ancestors of this process (whose command lines
# carry the leg's own words) are excluded by pid, nothing else is.
set -uo pipefail
LOG="$1"; shift
PAT="${PGREP_PATTERN:-hven_.*|ctest|ninja|cmake|ipopt|snopt}"   # process NAMES (pgrep -x), not command lines: an idle shell merely mentioning ctest is not a process
ancestors() { local p="$$"; while [ -n "$p" ] && [ "$p" -gt 1 ] 2>/dev/null; do echo "$p"; p="$(ps -o ppid= -p "$p" 2>/dev/null | tr -d ' ')"; done; }
SKIP="$(ancestors | tr '\n' '|')0"
nested_ok() { local sh=0 c=0 pf=0 prev=""; for a in "$@"; do case "$a" in bash|sh|/bin/bash|/usr/bin/bash) sh=1;; -c) c=1;; esac
  [ "$prev" = "-o" ] && [ "$a" = "pipefail" ] && pf=1; prev="$a"; done; [ $sh -eq 1 ] && [ $c -eq 1 ] && [ $pf -eq 0 ] && return 1; return 0; }
mkdir -p "$(dirname "$LOG")"
{ echo "LEG: $*"; echo "PGREP_SEPARATE $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  if pgrep -x -a "$PAT" | grep -Ev "^($SKIP) " | grep -v run_leg.sh; then echo PGREP_NONEMPTY_BEFORE_LOCK; else echo PGREP_EMPTY_BEFORE_LOCK; fi; } > "$LOG" 2>&1
if ! nested_ok "$@"; then echo "LEG REFUSED: nested shell without pipefail; LEG_RC=2" >> "$LOG"; echo "WRAPPER_EXIT=2" | tee -a "$LOG"; exit 2; fi
flock /tmp/box-build.lock bash -c '
set -o pipefail
LOG="$1"; PAT="$2"; SKIP="$3"; shift 3
{
  if pgrep -x -a "$PAT" | grep -Ev "^($SKIP) " | grep -v run_leg.sh; then
      echo "PGREP_NONEMPTY (inside the lock) -- leg refused"; echo "LEG_RC=75"; exit 75
  fi
  echo "PGREP_EMPTY (inside the lock)"
  echo "FOREGROUND_START $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  "$@"; leg_rc=$?
  echo "LEG_RC=$leg_rc"
  echo "FOREGROUND_END $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  exit $leg_rc
} >> "$LOG" 2>&1
' bash "$LOG" "$PAT" "$SKIP" "$@"
rc=$?
echo "WRAPPER_EXIT=$rc" >> "$LOG"
echo "WRAPPER_EXIT=$rc  (log $LOG)"
exit $rc
