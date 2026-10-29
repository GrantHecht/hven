#!/usr/bin/env bash
# M7 A3 GATES RUN (96f3158e..bb512d69): COPIED from the A2 chain chain_gates_a2.sh (NOT edited in place), re-pointed at gates-a3.sh; names only. Its header follows.
# M7 A2 GATES RUN (d5984ea2..3e90d587): COPIED from the SCL chain chain_gates_scl.sh (NOT edited in place), re-pointed at gates-a2.sh; names only. Its header follows.
# M7 W2 SCL GATES RUN (d9bfbb27..c1075641): COPIED from the RLA chain chain_gates_rla.sh (NOT edited in place), re-pointed at gates-scl.sh; names only,
# plus ONE derivation: W (the wrappers directory) is taken from this file's own location instead of being spelled out. Its header follows.
# M7 W2 SNX + SNR GATES RUN (5ecd0dea..fcde1bf9): COPIED from the SDL-4 + SDL-6 chain chain_gates_sdl4_sdl6.sh (NOT edited), re-pointed at gates-scl.sh; names only. Its header follows.
# M7 W2 ROUND W1 + CERT-1 GATES RUN (2d2f92b1..6bcffcd9): COPIED from the NC4 + R2 chain chain_gates_nc4_r2.sh (NOT edited), re-pointed at gates-scl.sh; the one change besides names: the classifier it pre-checks is classify_psym_m4_uncapped.py in this directory (the negative-control cap lifted, see the runner header (b)). The NC4 + R2 header follows.
# M7 W2 ROUND NC4 + R2 GATES RUN (79775010..21369067): COPIED from the NC3 run-2 chain chain_gates_nc3_r2.sh (NOT edited) and re-pointed at
# gates-scl.sh, GATES_SCL_RC and chain-scl.log; ROOT is derived from this file location (worktree run), exported for the fold-proof inner shell.
# The NC3 run-2 header follows.
# M7 W2 ROUND NC3 RUN 2, THE SUCCESSOR (-r2) after fix round 1 (a522f115..b7993013); COPIED from run 1 chain_gates_nc3.sh
# (NOT edited) and re-pointed at gates-scl.sh, GATES_SCL_RC and chain-scl.log. Run 1 header follows:
# M7 W2 ROUND NC3 (a522f115..054e4c78) -- THE DETACHED GATE CHAIN: the six gates, then the
# gate-assert library's own fold proofs. COPIED from the P1 chain
# (docs/notes/data/2026-10-m7-w2-p1-schur-cost/wrappers-gates/chain_gates_p1.sh, NOT edited)
# and re-pointed at gates-scl.sh, its rc variable GATES_SCL_RC and its log chain-scl.log. Nothing
# else changed. It writes only to the scratch tree it ASKS gates-scl.sh for.
# The P1 header's provenance line follows (P1's chain was copied from e2f9's, itself e2f8 / E4 run 2's)
# (docs/notes/data/2026-10-m7-w2-e2-fix9-standstill/wrappers-gates/chain_gates_e2f9.sh, NOT edited).
# The e2f9 header's provenance line follows (e2f8 / E4 run 2 chain lineage)
# (docs/notes/data/2026-09-m7-w2-e4-proximal-eqp/wrappers-gates/chain_gates_e4_r2.sh, NOT edited).
# The rest of E4 run 2's header follows for provenance:
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
ROOT=$(git -C "$(dirname "${BASH_SOURCE[0]}")" rev-parse --show-toplevel) || { echo "REFUSED: no worktree root"; exit 3; }
export ROOT
W=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd) || { echo "REFUSED: no wrappers directory"; exit 3; }
case "$W" in "$ROOT"/docs/notes/data/?*/wrappers-gates) : ;; *) echo "REFUSED: the wrappers directory is not under $ROOT/docs/notes/data: $W"; exit 3 ;; esac
M4W=$ROOT/docs/notes/data/2026-09-m7-w2-m4-anticycling-arm/wrappers
M3B=$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace/wrappers

# THIS CHAIN DOES NOT NAME THE SCRATCH TREE. It ASKS the gate runner, which
# holds the round's single definition, and refuses an answer it cannot use.
# Two files that each spell the same path out is the carried-path error class
# one level up -- they agree until someone re-points one of them, which is
# precisely how an earlier round's first chain died. Derived, not carried.
[ -r "$W/gates-a3.sh" ] || { echo "REFUSED: the gate runner is not readable: $W/gates-a3.sh"; exit 3; }
S=$(bash "$W/gates-a3.sh" --print-scratch) \
  || { echo "REFUSED: the gate runner would not print its scratch tree"; exit 3; }
case "$S" in
  "$ROOT"/.scratch/?*) : ;;
  *) echo "REFUSED: the gate runner's scratch tree is not under $ROOT/.scratch: '$S'"; exit 3 ;;
esac
export GATES_SCRATCH="$S"
mkdir -p "$S/gates" "$S/logs"
LOG=$S/logs/chain-a3.log
exec > "$LOG" 2>&1
echo "CHAIN_START $(date -Is)"
cd "$ROOT" || exit 90
echo "HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l)"
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: the tree is not clean"; exit 2; }

# THE PATHS THIS CHAIN HANDS ITS SIBLINGS, CHECKED BEFORE IT STARTS.
for pth in "$W/gates-a3.sh" "$M3B/gate_asserts_m3b.sh" "$W/classify_psym_m4_uncapped.py" \
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

bash "$W/gates-a3.sh"
gates_rc=$?
echo "GATES_A3_RC=$gates_rc"

# The fold proofs take their own lock hold: they run `ctest --no-tests=error`
# over a filter that matches no test, which is a ctest invocation like any
# other, and the box protocol does not care that it is a cheap one.
pgrep -x hven_bench_cell > /dev/null && echo "NOTE: a foreign cell binary is alive before the fold-proof lock"
flock /tmp/box-build.lock bash -c '
  set -o pipefail
  ROOT=${ROOT:?}; S=${GATES_SCRATCH:?}
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
