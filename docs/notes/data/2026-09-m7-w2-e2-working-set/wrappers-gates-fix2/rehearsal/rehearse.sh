#!/usr/bin/env bash
# M7 W2 E2 FIX ROUND 2 -- the P-SYM REHEARSAL (the fix2 runner's g5 segment, byte-identical
# to fix1's) at HEAD d1edc391 against BEFORE=763cda00, both configurations, one lock hold.
# The declaration arguments are HYPOTHESES; a refusal still prints the measured shape.
set -o pipefail
ROOT=/home/ghecht/Projects/hven
export GATES_SCRATCH=$ROOT/.scratch/m7w2e2fix2s/rehearsal
S=$GATES_SCRATCH
cd "$ROOT" || exit 90
BEFORE=763cda00; HEADSHA=$(git rev-parse HEAD)
echo "START $(date -Is) HEAD $HEADSHA BEFORE $BEFORE dirty=$(git status --porcelain | wc -l)"
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: tree not clean"; exit 2; }
CS=$(git diff --name-only "$BEFORE..$HEADSHA" -- src/ tests/ | tr '\n' ' ')
CH=$(git diff --name-only "$BEFORE..$HEADSHA" -- include/ | tr '\n' ' ')
M=$(git diff --name-only --diff-filter=M "$BEFORE..$HEADSHA" -- 'src/*.cpp' | sed 's|^|CMakeFiles/hven.dir/|; s|$|.o|' | LC_ALL=C sort | tr '\n' ' ')
ADD=$(git diff --name-only --diff-filter=A "$BEFORE..$HEADSHA" -- 'src/*.cpp' 'tests/*.cpp' | tr '\n' ' ')
LADD=$(git diff --name-only --diff-filter=A "$BEFORE..$HEADSHA" -- 'src/*.cpp' | sed 's|^|CMakeFiles/hven.dir/|; s|$|.o|' | LC_ALL=C sort | tr '\n' ' ')
OUTS="tests/qp/test_qp_anti_cycling_arm.cpp"
echo "G5 args: CS=[$CS] CH=[$CH] MOV=[$M] ADD=[$ADD] LADD=[$LADD] OUTS=[$OUTS]"
flock /tmp/box-build.lock bash -c '
  set -o pipefail
  echo "FOREGROUND_START R5-psym-e2fix2 $(date -Is)"
  foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
  [ -z "$foreign" ] && echo "PGREP_EMPTY R5-psym-e2fix2: no foreign cell binary under my lock" \
                    || echo "NOTE: a foreign cell binary is alive under my lock: $foreign"
  "$@"; rc=$?
  echo "FOREGROUND_END R5-psym-e2fix2 $(date -Is) rc=${rc}"
  exit ${rc}
' _ bash "$S/g5.sh" "$BEFORE" "$CS" "$CH" 1 39/39 3/3 36/36 0 6 "$M" "$M" "$ADD" "$LADD" \
  "tests/CMakeFiles/hven_qp_walk_seam_tests.dir" "$OUTS" ""
echo "LEG_RC R5-psym-e2fix2 rc=$?"
echo "END $(date -Is) tree=$(git rev-parse --abbrev-ref HEAD) $(git rev-parse --short HEAD) dirty=$(git status --porcelain | wc -l)"
