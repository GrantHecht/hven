#!/usr/bin/env bash
# M7 W2 M3b FIX ROUND 2 -- THE DETACHED GATE CHAIN: the six gates, then both sets
# of fold proofs. One process, started with `setsid` and left to run; its pid is
# recorded in `.superpowers/m7-w2-m3b-fix2-STATE.md` and nothing polls it.
#
# IT IS FIX ROUND 1's CHAIN WITH ITS ONE DEFECT REPAIRED. That round handed
# `fold_proofs_m3b.sh` the path `$S/dbg` while `gates-fix1.sh` configures
# `$S/gates-dbg`, so the one `ctest --no-tests=error` fold found no configured
# tree and the fold script fail-closed on the skip: 45/1 instead of 46/0. The
# process rule it earned is written into this file rather than left in a report:
# EVERY PATH A RUNNER HANDS A SIBLING SCRIPT IS CHECKED TO EXIST BEFORE IT IS
# HANDED OVER, and a missing one is a REFUSAL of the chain, not a skip inside
# the sibling.
#
# THE TREE MUST BE CLEAN AND MUST STAY CLEAN. `gates-fix2.sh`'s G5 checks out
# $BEFORE and back, and its G6 asserts `DIRTY_FILES=0`; the fold proofs compare
# each wrapper's worktree digest against `git show HEAD:<path>` and FAIL if they
# differ. So every transcript this chain writes goes to the SCRATCH tree, and
# the artifact directory is written only afterwards, by hand, from what it left.
#
# WALL IS INFORMATIONAL. Nothing this chain prints is a timing claim.
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m3bfix2
W=$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace/wrappers
mkdir -p "$S/gates" "$S/logs"
LOG=$S/logs/chain.log
exec > "$LOG" 2>&1
echo "CHAIN_START $(date -Is)"
cd "$ROOT" || exit 90
echo "HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l)"
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: the tree is not clean"; exit 2; }

bash "$W/gates-fix2.sh"
gates_rc=$?
echo "GATES_FIX2_RC=$gates_rc"

# The fold proofs take their own lock hold: they run `ctest --no-tests=error`
# over a filter that matches no test, which is a ctest invocation like any
# other, and the box protocol does not care that it is a cheap one.
pgrep -x hven_bench_cell > /dev/null && echo "NOTE: a foreign cell binary is alive before the fold-proof lock"
flock /tmp/box-build.lock bash -c '
  set -o pipefail
  ROOT=/home/ghecht/Projects/hven; S=$ROOT/.scratch/m7w2m3bfix2
  W=$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace/wrappers
  echo "FOREGROUND_START fold-proofs $(date -Is)"
  foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
  [ -z "$foreign" ] && echo "PGREP_EMPTY fold-proofs: no foreign cell binary under my lock" \
                    || echo "NOTE: a foreign cell binary is alive under my lock: $foreign"
  # THE PATHS THIS SEGMENT HANDS ITS SIBLINGS, CHECKED BEFORE THEY ARE HANDED
  # OVER. `gates-fix2.sh` configures the Debug tree at $S/gates-dbg, and that is
  # the tree that the `ctest --no-tests=error` fold in `fold_proofs_m3b.sh`
  # needs; fix round 1 passed $S/dbg, which never existed. A missing path is a
  # REFUSAL.
  DBG_TREE=$S/gates-dbg
  for pth in "$W/fold_proofs_m3b.sh" "$W/fold_proofs_psym_fix2.sh" \
             "$DBG_TREE/CTestTestfile.cmake"; do
      if [ ! -e "$pth" ]; then
          echo "REFUSED: the fold-proof segment was about to hand a sibling a path that does not exist: $pth"
          echo "FOREGROUND_END fold-proofs $(date -Is)"
          exit 3
      fi
      echo "PATH_OK $pth"
  done
  bash "$W/fold_proofs_m3b.sh" "$DBG_TREE" > "$S/gates/fold-proofs.txt" 2>&1
  a=$?
  bash "$W/fold_proofs_psym_fix2.sh" > "$S/gates/fold-proofs-psym.txt" 2>&1
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
