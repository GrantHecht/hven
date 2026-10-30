#!/usr/bin/env bash
# M7 W2 E4 FIX ROUND 1 -- THE DETACHED GATE CHAIN: the six gates, then the gate
# library's fold proofs. E4's `wrappers/chain_gates_e4.sh`, copied and
# re-pointed at `gates-e4fix1.sh`; E4's file is not edited. Two changes beyond
# the re-pointing: ROOT is DERIVED from the tree this file lives in (the round
# runs in a git worktree), and the fold proofs run this round's copy
# `fold_proofs_e4fix1.sh`, whose only change is the same derivation.
#
# WHAT IS BEING GATED: an HVEN_TESTING-only certificate seam and three
# fixtures. The declared shape (zero P-SYM movers; Debug-only qp_engine.cpp.o)
# is in `gates-e4fix1.sh`'s header and `gates-fix1/psym-derivation-e4fix1.txt`.
#
# WALL IS INFORMATIONAL. Nothing this chain prints is a timing claim.
set -o pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && git rev-parse --show-toplevel) || exit 90
W=$ROOT/docs/notes/data/2026-09-m7-w2-e4-certificate/wrappers-fix1
M4W=$ROOT/docs/notes/data/2026-09-m7-w2-m4-anticycling-arm/wrappers
M3B=$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace/wrappers

[ -r "$W/gates-e4fix1.sh" ] || { echo "REFUSED: the gate runner is not readable: $W/gates-e4fix1.sh"; exit 3; }
S=$(bash "$W/gates-e4fix1.sh" --print-scratch) \
  || { echo "REFUSED: the gate runner would not print its scratch tree"; exit 3; }
case "$S" in
  "$ROOT"/.scratch/?*) : ;;
  *) echo "REFUSED: the gate runner's scratch tree is not under $ROOT/.scratch: '$S'"; exit 3 ;;
esac
export GATES_SCRATCH="$S"
export GATES_ROOT="$ROOT"
mkdir -p "$S/gates" "$S/logs"
LOG=$S/logs/chain.log
exec > "$LOG" 2>&1
echo "CHAIN_START $(date -Is)"
cd "$ROOT" || exit 90
echo "ROOT $ROOT"
echo "HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l)"
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: the tree is not clean"; exit 2; }

for pth in "$W/gates-e4fix1.sh" "$W/derive_movers_e4fix1.py" "$W/fold_proofs_e4fix1.sh" \
           "$M3B/gate_asserts_m3b.sh" "$M4W/classify_psym_m4.py" \
           "$M3B/check_sections_fix2.sh" "$M3B/section_digest_fix2.py" \
           "$ROOT/scripts/psym_compare.sh" "$ROOT/scripts/compare_replay.py" \
           "$ROOT/scripts/check_install_smoke.sh" "$ROOT/scripts/check_export_contract.sh" \
           "$ROOT/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt" \
           "$ROOT/docs/notes/data/2026-09-m6-close-gate/replay"; do
    if [ ! -e "$pth" ]; then
        echo "REFUSED: a path this chain was about to hand a sibling does not exist: $pth"
        exit 3
    fi
    echo "PATH_OK $pth"
done

bash "$W/gates-e4fix1.sh"
gates_rc=$?
echo "GATES_E4FIX1_RC=$gates_rc"

pgrep -x hven_bench_cell > /dev/null && echo "NOTE: a foreign cell binary is alive before the fold-proof lock"
flock /tmp/box-build.lock bash -c '
  set -o pipefail
  S=${GATES_SCRATCH:?}; W="$1"
  echo "FOREGROUND_START fold-proofs $(date -Is)"
  foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
  [ -z "$foreign" ] && echo "PGREP_EMPTY fold-proofs: no foreign cell binary under my lock" \
                    || echo "NOTE: a foreign cell binary is alive under my lock: $foreign"
  DBG_TREE=$S/gates-dbg
  for pth in "$W/fold_proofs_e4fix1.sh" "$DBG_TREE/CTestTestfile.cmake"; do
      if [ ! -e "$pth" ]; then
          echo "REFUSED: the fold-proof segment was about to hand a sibling a path that does not exist: $pth"
          echo "FOREGROUND_END fold-proofs $(date -Is)"
          exit 3
      fi
      echo "PATH_OK $pth"
  done
  bash "$W/fold_proofs_e4fix1.sh" "$DBG_TREE" > "$S/gates/fold-proofs.txt" 2>&1
  a=$?
  echo "FOLD_PROOFS_RC=$a"
  echo "FOREGROUND_END fold-proofs $(date -Is)"
  [ $a -eq 0 ]
' _ "$W"
fold_rc=$?
echo "FOLD_PROOFS_AGGREGATE_RC=$fold_rc"
tail -3 "$S/gates/fold-proofs.txt"

echo "CHAIN_END $(date -Is)"
echo "CHAIN_SUMMARY gates=$gates_rc fold_proofs=$fold_rc"
[ "$gates_rc" -eq 0 ] && [ "$fold_rc" -eq 0 ]
exit $?
