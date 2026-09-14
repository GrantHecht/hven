#!/usr/bin/env bash
# The box protocol, in one place: pgrep SEPARATE (before the lock) -> PGREP_EMPTY,
# then flock /tmp/box-build.lock, then -- INSIDE the lock -- the second pgrep,
# FOREGROUND_START, the leg itself and FOREGROUND_END, all of it written into the
# retained log this leg is named by.
#
# fix1 (astra I5): the T8.5 wrapper (.scratch/w5t85/run_leg.sh) wrote
# FOREGROUND_START BEFORE acquiring the lock and FOREGROUND_END after releasing
# it, so the markers did not bracket the leg's own exclusive window.  T8.4 fix1
# had already fixed exactly that; T8.5 copied the OLD wrapper.  Both markers are
# now inside the lock, after the in-lock pgrep.
#
# fix1 (astra I5, second half): `set -o pipefail` INSIDE the lock shell, so
# LEG_RC is the PIPELINE's status.  The T8.5 smoke leg piped its command and
# recorded LEG_RC=0 while the export invocation inside the pipeline had failed.
#
# T8.7 fix1 (astra I4), TWO defects of the T8.7 wrapper, fixed here:
#
#   (a) THE WRAPPER'S OWN EXIT STATUS WAS THE FINAL MARKER'S.  `rc=$?` read the
#       status of the `flock ... bash -c '{ ...; echo FOREGROUND_END; }'`
#       compound, whose last command is the successful `echo` -- so a FAILING
#       leg exited 0 and only the LEG_RC= line in the log said otherwise.  The
#       inner shell now carries the leg's status out explicitly and the wrapper
#       exits with it: the wrapper's exit status IS the leg's.
#
#   (b) A NESTED `bash -c` DID NOT INHERIT pipefail.  `set -o pipefail` is a
#       shell OPTION, not an exported variable, so a leg written as
#       `bash -c 'a | tee b'` ran its pipeline WITHOUT pipefail and reported the
#       tee's status: log 03-goldens-derive.log recorded LEG_RC=0 over three
#       FAILED golden tests.  The fix is the one astra's I4 names: every nested
#       shell is spelled `bash -o pipefail -c` AT THE CALL SITE -- and this
#       wrapper REFUSES a leg that spells it any other way, so the discipline is
#       mechanical rather than remembered (see the guard below).  Exporting
#       SHELLOPTS would do it too and is deliberately NOT used: it would also
#       push `nounset` into every script the legs call (the install smoke, the
#       P-SYM tool), changing what those scripts do to serve this wrapper's
#       bookkeeping.
#
#       The proof of both halves is retained: `00-wrapper-failure-proof.log` (a
#       deliberately failing nested pipeline -- non-zero LEG_RC and a non-zero
#       wrapper exit) and `00b-wrapper-guard-proof.log` (the guard refusing the
#       un-flagged spelling).
#
# T8.7b (brief section 1b, the lane's fourth rider): THE GUARD BELOW INSPECTS ONLY THIS
# WRAPPER'S OWN DIRECT ARGUMENTS, so a leg SCRIPT passed here can still spawn an
# un-flagged `bash -c` from inside itself and slip past it -- which is why every
# leg script this task writes sets `set -o pipefail` in its own first lines, and
# why that is a rule about the SCRIPTS, not only about this wrapper.
#
# W6 T1: DECLARED_SKIP_PIDS.  The pgrep pattern is a substring match on whole
# COMMAND LINES, so a shell whose command line merely CONTAINS one of those words
# matches without being a build or a test.  At T1's start exactly one such
# process existed: pid 279599, the launcher shell of the READ-ONLY sol reviewer
# reading the W6 T0 commits, whose embedded review prompt contains the word
# "ctest".  It is idle (state S), runs nothing and touches no tree.  It is named
# here BY PID with that reason rather than filtered out by a looser pattern, and
# every log prints the declaration beside the verdict, so the exclusion is
# visible to the next reader instead of silent.  Empty by default.
#
# W6 T3 (this copy): THE SAME SHAPE, A DIFFERENT PID.  At T3's start the one
# matching process is pid 353157 -- the launcher shell of the READ-ONLY sol
# reviewer reading the W6 T2 commits, whose embedded review prompt again contains
# the word "ctest" (and "hven_"), `ps -o stat` = S, idle, running nothing and
# touching no tree.  It is declared BY EXACT PID in DECLARED_SKIP_PIDS for every
# leg of this task; nothing else is excluded, and the pattern itself is unchanged.
#
# usage: run_leg.sh <logfile> <command ...>
set -uo pipefail
LOG="$1"; shift
PAT='cmake|ninja|ctest|hven_|bench_corpus'
DECLARED_SKIP_PIDS="${DECLARED_SKIP_PIDS:-}"
DECLARED_NOTE="${DECLARED_NOTE:-}"

# Every pid from this process up to init: the wrapper itself, its shell and the
# flock/bash that carries the leg all have the leg's own words in their command
# lines, and none of them is a foreign process.
ancestors() {
    local p="$$"
    while [ -n "$p" ] && [ "$p" -gt 1 ] 2>/dev/null; do
        echo "$p"
        p="$(ps -o ppid= -p "$p" 2>/dev/null | tr -d ' ')"
    done
}

box_check() {
    local skip
    skip="$(ancestors | tr '\n' '|')0"
    if [ -n "$DECLARED_SKIP_PIDS" ]; then
        skip="$skip|$DECLARED_SKIP_PIDS"
        echo "PGREP_DECLARED_SKIP $DECLARED_SKIP_PIDS -- $DECLARED_NOTE"
    fi
    if pgrep -a -f "$PAT" | grep -Ev "^($skip) " | grep -v run_leg.sh > "$LOG.boxcheck.$$" 2>/dev/null; then
        echo "PGREP_NONEMPTY"
        cat "$LOG.boxcheck.$$"
    else
        echo "PGREP_EMPTY"
    fi
    rm -f "$LOG.boxcheck.$$"
}

# THE NESTED-SHELL GUARD (T8.7 fix1, astra I4b).  A leg that starts its own
# shell must start it with pipefail on, or a pipeline inside it reports the LAST
# command's status and a failure disappears.  Spelled `bash -o pipefail -c ...`,
# it passes; spelled any other way it is REFUSED here rather than run and
# believed.
nested_shell_is_safe() {
    local has_shell=0 has_c=0 has_pipefail=0 prev=""
    for a in "$@"; do
        case "$a" in
        bash | sh | /bin/bash | /usr/bin/bash | /bin/sh) has_shell=1 ;;
        -c) has_c=1 ;;
        esac
        if [ "$prev" = "-o" ] && [ "$a" = "pipefail" ]; then has_pipefail=1; fi
        case "$a" in -*o*pipefail*) has_pipefail=1 ;; esac
        prev="$a"
    done
    [ $has_shell -eq 1 ] && [ $has_c -eq 1 ] && [ $has_pipefail -eq 0 ] && return 1
    return 0
}

{
    echo "LEG: $*"
    echo "PGREP_SEPARATE $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    box_check
} > "$LOG" 2>&1

if ! nested_shell_is_safe "$@"; then
    {
        echo "LEG REFUSED: a nested shell without pipefail."
        echo "  spell it 'bash -o pipefail -c ...' so a pipeline inside it fails the leg."
        echo "LEG_RC=2 (refused before the lock; nothing ran)"
        echo "WRAPPER_EXIT=2"
    } >> "$LOG" 2>&1
    tail -25 "$LOG"
    echo "WRAPPER_EXIT=2"
    exit 2
fi

flock /tmp/box-build.lock bash -c '
set -o pipefail
LOG="$1"; shift
PAT="$1"; shift
skip="$1"; shift
{
  if pgrep -a -f "$PAT" | grep -Ev "^($skip) " | grep -v run_leg.sh; then
      echo "PGREP_NONEMPTY (inside the lock)"
  else
      echo "PGREP_EMPTY (inside the lock)"
  fi
  echo "FOREGROUND_START $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  "$@"
  leg_rc=$?
  echo "LEG_RC=$leg_rc"
  echo "FOREGROUND_END $(date -u +%Y-%m-%dT%H:%M:%SZ)"
} >> "$LOG" 2>&1
# THE LEG S STATUS LEAVES THE LOCK.  Both markers are already written, inside
# the lock, and this exit is what the wrapper reports.
exit $leg_rc
' bash "$LOG" "$PAT" "$(ancestors | tr '\n' '|')0${DECLARED_SKIP_PIDS:+|$DECLARED_SKIP_PIDS}" "$@"
rc=$?

# T8.7b: THE WRAPPER'S OWN EXIT STATUS GOES INTO THE RETAINED LOG, not only to
# stdout. A log read later by a successor (or by a reviewer) then carries both
# halves of the truth -- the leg's status and this wrapper's -- without needing
# the terminal scrollback the leg ran in.
echo "WRAPPER_EXIT=$rc" >> "$LOG"
tail -25 "$LOG"
echo "WRAPPER_EXIT=$rc"
exit $rc
