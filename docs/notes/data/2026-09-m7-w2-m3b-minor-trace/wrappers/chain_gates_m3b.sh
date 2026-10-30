#!/usr/bin/env bash
# M7 W2 M3b task A -- THE DETACHED GATE CHAIN: the six gates, then both sets of
# fold proofs. One process, started with `setsid` and left to run; its pid is
# recorded in `.superpowers/m7-w2-m3b-STATE.md` and nothing polls it.
#
# THE TREE MUST BE CLEAN AND MUST STAY CLEAN. `gates-m3b.sh`'s G5 checks out
# $BEFORE and back, and its G6 asserts `DIRTY_FILES=0`; the fold proofs compare
# each wrapper's worktree digest against `git show HEAD:<path>` and FAIL if they
# differ. So every transcript this chain writes goes to the SCRATCH tree, and
# the artifact directory is written only afterwards, by hand, from what it left.
#
# WALL IS INFORMATIONAL. Nothing this chain prints is a timing claim.
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m3b
W=$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace/wrappers
mkdir -p "$S/gates" "$S/logs"
LOG=$S/logs/chain.log
exec > "$LOG" 2>&1
echo "CHAIN_START $(date -Is)"
cd "$ROOT" || exit 90
echo "HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l)"
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: the tree is not clean"; exit 2; }

bash "$W/gates-m3b.sh"
gates_rc=$?
echo "GATES_M3B_RC=$gates_rc"

# The fold proofs take their own lock hold: they run `ctest --no-tests=error`
# over a filter that matches no test, which is a ctest invocation like any
# other, and the box protocol does not care that it is a cheap one.
pgrep -x hven_bench_cell > /dev/null && echo "NOTE: a foreign cell binary is alive before the fold-proof lock"
flock /tmp/box-build.lock bash -c '
  set -o pipefail
  ROOT=/home/ghecht/Projects/hven; S=$ROOT/.scratch/m7w2m3b
  W=$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace/wrappers
  echo "FOREGROUND_START fold-proofs $(date -Is)"
  foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
  [ -z "$foreign" ] && echo "PGREP_EMPTY fold-proofs: no foreign cell binary under my lock" \
                    || echo "NOTE: a foreign cell binary is alive under my lock: $foreign"
  bash "$W/fold_proofs_m3b.sh" "$S/dbg" > "$S/gates/fold-proofs.txt" 2>&1
  a=$?
  bash "$W/fold_proofs_psym_m3b.sh" > "$S/gates/fold-proofs-psym.txt" 2>&1
  b=$?
  echo "FOLD_PROOFS_RC=$a  FOLD_PROOFS_PSYM_RC=$b"
  echo "FOREGROUND_END fold-proofs $(date -Is)"
  [ $a -eq 0 ] && [ $b -eq 0 ]
'
fold_rc=$?
echo "FOLD_PROOFS_AGGREGATE_RC=$fold_rc"
tail -3 "$S/gates/fold-proofs.txt"
tail -3 "$S/gates/fold-proofs-psym.txt"

echo "CHAIN_END $(date -Is)"
echo "CHAIN_SUMMARY gates=$gates_rc fold_proofs=$fold_rc"
[ "$gates_rc" -eq 0 ] && [ "$fold_rc" -eq 0 ]
exit $?
