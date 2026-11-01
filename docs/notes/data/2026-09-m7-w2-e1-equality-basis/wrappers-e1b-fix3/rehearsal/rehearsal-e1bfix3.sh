#!/usr/bin/env bash
# M7 W2 E1b FIX ROUND 3 -- THE P-SYM REHEARSAL (the fix3 runner's g5 segment, byte-identical
# copy emitted by `gates-e1bfix3.sh --preflight-only`) at HEAD = the fix3 code commit against
# BEFORE=de62a2e7, both configurations, under the box lock, one hold. The declaration
# arguments are HYPOTHESES (1/1 = A 1 + B 0, no non-debug mover, DEBUG_ONLY 0/0); a
# refusal still prints the measured shape, which is then declared.
# PROVENANCE: the fix-round-2 rehearsal (../../wrappers-e1b-fix2/rehearsal/rehearsal-e1bfix2.sh)
# re-pointed. The untracked wrappers directory is moved OUT of the tree for the hold (g5
# refuses a dirty tree) and moved back after.
set -o pipefail
ROOT=/home/ghecht/Projects/hven
export GATES_SCRATCH=$ROOT/.scratch/m7w2e1bfix3rehearsal
S=$GATES_SCRATCH
cd "$ROOT" || exit 90
BEFORE=de62a2e7; HEADSHA=$(git rev-parse HEAD)
echo "START $(date -Is) HEAD $HEADSHA BEFORE $BEFORE dirty=$(git status --porcelain | wc -l)"
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: tree not clean"; git status --porcelain; exit 2; }
CS=$(git diff --name-only "$BEFORE..$HEADSHA" -- src/ tests/ | tr '\n' ' ')
CH=$(git diff --name-only "$BEFORE..$HEADSHA" -- include/ | tr '\n' ' ')
OUTS="tests/sqp/test_sqp_equality_basis_seams.cpp"
COS="src/drivers/sqp_solver.cpp"
echo "G5 args: CS=[$CS] CH=[$CH] OUTS=[$OUTS] COS=[$COS]"
flock /tmp/box-build.lock bash -c '
  set -o pipefail
  echo "FOREGROUND_START R5-psym-e1bfix3 $(date -Is)"
  foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
  [ -z "$foreign" ] && echo "PGREP_EMPTY R5-psym-e1bfix3: no foreign cell binary under my lock" \
                    || echo "NOTE: a foreign cell binary is alive under my lock: $foreign"
  bash "$@"; rc=$?
  echo "FOREGROUND_END R5-psym-e1bfix3 $(date -Is) rc=${rc}"
  exit ${rc}
' _ "$S/g5.sh" "$BEFORE" "$CS" "$CH" 1 1/1 1/1 0/0 0 0 "" "" "" "" \
  "tests/CMakeFiles/hven_sqp_seam_tests.dir" "$OUTS" "" "$COS"
echo "LEG_RC R5-psym-e1bfix3 rc=$?"
echo "END $(date -Is) tree=$(git rev-parse --abbrev-ref HEAD) $(git rev-parse --short HEAD) dirty=$(git status --porcelain | wc -l)"
