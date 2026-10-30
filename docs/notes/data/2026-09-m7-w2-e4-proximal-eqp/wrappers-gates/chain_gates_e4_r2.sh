#!/usr/bin/env bash
# M7 W2 E4 (THE PROXIMAL EQP) + FIX ROUNDS 1-4b -- THE DETACHED GATE CHAIN, RUN 2: the six
# gates, then the gate-assert library's own fold proofs. Run 1's chain (chain_gates_e4.sh,
# declaration 13013b4c) is byte-frozen and its scratch tree .scratch/m7w2e4gates is left alone,
# so its transcripts survive for the record round; this chain writes only to
# .scratch/m7w2e4gates-r2, which it ASKS the r2 runner for.
# One process, started with `setsid` and left to run; its pid is reported to the settler at
# launch and nothing polls it.
#
# COPIED from run 1's chain (chain_gates_e4.sh, this directory -- itself NC2's, re-pointed) and
# re-pointed at `gates-e4-r2.sh`, its rc variable and its log name. Nothing else changed. (`check_sections_fix2.sh` / `section_digest_fix2.py`
# below are the M3b fix-round-2 tools by name, not this round's.)
#
# G5 CHECKS THE MAIN TREE OUT TO $BEFORE while it builds its before arm (twice,
# once per configuration): nothing may edit the tree while this chain runs.
#
# WALL IS INFORMATIONAL. Nothing this chain prints is a timing claim.
set -o pipefail
ROOT=/home/ghecht/Projects/hven
W=$ROOT/docs/notes/data/2026-09-m7-w2-e4-proximal-eqp/wrappers-gates
M4W=$ROOT/docs/notes/data/2026-09-m7-w2-m4-anticycling-arm/wrappers
M3B=$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace/wrappers

# THIS CHAIN DOES NOT NAME THE SCRATCH TREE. It ASKS the gate runner, which
# holds the round's single definition, and refuses an answer it cannot use.
# Two files that each spell the same path out is the carried-path error class
# one level up -- they agree until someone re-points one of them, which is
# precisely how an earlier round's first chain died. Derived, not carried.
[ -r "$W/gates-e4-r2.sh" ] || { echo "REFUSED: the gate runner is not readable: $W/gates-e4-r2.sh"; exit 3; }
S=$(bash "$W/gates-e4-r2.sh" --print-scratch) \
  || { echo "REFUSED: the gate runner would not print its scratch tree"; exit 3; }
case "$S" in
  "$ROOT"/.scratch/?*) : ;;
  *) echo "REFUSED: the gate runner's scratch tree is not under $ROOT/.scratch: '$S'"; exit 3 ;;
esac
export GATES_SCRATCH="$S"
mkdir -p "$S/gates" "$S/logs"
LOG=$S/logs/chain-e4-r2.log
exec > "$LOG" 2>&1
echo "CHAIN_START $(date -Is)"
cd "$ROOT" || exit 90
echo "HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l)"
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: the tree is not clean"; exit 2; }

# THE PATHS THIS CHAIN HANDS ITS SIBLINGS, CHECKED BEFORE IT STARTS.
for pth in "$W/gates-e4-r2.sh" "$M3B/gate_asserts_m3b.sh" "$M4W/classify_psym_m4.py" \
           "$M3B/check_sections_fix2.sh" "$M3B/section_digest_fix2.py" \
           "$M3B/fold_proofs_m3b.sh" \
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

bash "$W/gates-e4-r2.sh"
gates_rc=$?
echo "GATES_E4_R2_RC=$gates_rc"

# The fold proofs take their own lock hold: they run `ctest --no-tests=error`
# over a filter that matches no test, which is a ctest invocation like any
# other, and the box protocol does not care that it is a cheap one.
pgrep -x hven_bench_cell > /dev/null && echo "NOTE: a foreign cell binary is alive before the fold-proof lock"
flock /tmp/box-build.lock bash -c '
  set -o pipefail
  ROOT=/home/ghecht/Projects/hven; S=${GATES_SCRATCH:?}
  M3B=$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace/wrappers
  echo "FOREGROUND_START fold-proofs $(date -Is)"
  foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
  [ -z "$foreign" ] && echo "PGREP_EMPTY fold-proofs: no foreign cell binary under my lock" \
                    || echo "NOTE: a foreign cell binary is alive under my lock: $foreign"
  DBG_TREE=$S/gates-dbg
  for pth in "$M3B/fold_proofs_m3b.sh" "$DBG_TREE/CTestTestfile.cmake"; do
      if [ ! -e "$pth" ]; then
          echo "REFUSED: the fold-proof segment was about to hand a sibling a path that does not exist: $pth"
          echo "FOREGROUND_END fold-proofs $(date -Is)"
          exit 3
      fi
      echo "PATH_OK $pth"
  done
  bash "$M3B/fold_proofs_m3b.sh" "$DBG_TREE" > "$S/gates/fold-proofs.txt" 2>&1
  a=$?
  echo "FOLD_PROOFS_RC=$a"
  echo "FOREGROUND_END fold-proofs $(date -Is)"
  [ $a -eq 0 ]
'
fold_rc=$?
echo "FOLD_PROOFS_AGGREGATE_RC=$fold_rc"
tail -3 "$S/gates/fold-proofs.txt"

echo "CHAIN_END $(date -Is)"
echo "CHAIN_SUMMARY gates=$gates_rc fold_proofs=$fold_rc"
[ "$gates_rc" -eq 0 ] && [ "$fold_rc" -eq 0 ]
exit $?
