#!/usr/bin/env bash
# M7 W2 E2 FIX ROUND 6 -- the P-SYM REHEARSAL: the runner's own g5 segment (EXTRACTED from
# ../gates-e2fix6.sh, byte for byte, not re-typed) at HEAD = the fix-6 commit against BEFORE,
# both configurations, one lock hold. Copied from the previous round's rehearse.sh and re-pointed:
# the scratch tree is ASKED of the runner (--print-scratch) plus /rehearsal, so the round keeps
# ONE scratch literal; every argument is DERIVED from git and the runner. The declaration
# arguments are HYPOTHESES (class A only, class B 0); a refusal still prints the measured shape.
set -o pipefail
ROOT=/home/ghecht/Projects/hven
W=${W:-$ROOT/docs/notes/data/2026-09-m7-w2-e2-working-set/wrappers-gates-fix6}
BASE_S=$(bash "$W/gates-e2fix6.sh" --print-scratch) || { echo "REFUSED: no scratch from the runner"; exit 3; }
case "$BASE_S" in "$ROOT"/.scratch/?*) : ;; *) echo "REFUSED: scratch '$BASE_S'"; exit 3 ;; esac
export GATES_SCRATCH=$BASE_S/rehearsal
S=$GATES_SCRATCH
mkdir -p "$S"
# THE SEGMENT, FROM THE RUNNER'S TEXT: the heredoc body between `cat > "$S/g5.sh" <<'INNER'`
# and its `INNER`.
awk '/^cat > "\$S\/g5.sh" <<.INNER.$/ {f=1; next} f && /^INNER$/ {exit} f {print}' \
  "$W/gates-e2fix6.sh" > "$S/g5.sh"
[ -s "$S/g5.sh" ] || { echo "REFUSED: could not extract g5 from the runner"; exit 3; }
cd "$ROOT" || exit 90
BEFORE=$(sed -n 's/^BEFORE=\${BEFORE:-\([0-9a-f]*\)}$/\1/p' "$W/gates-e2fix6.sh")
[ -n "$BEFORE" ] || { echo "REFUSED: no BEFORE in the runner"; exit 3; }
HEADSHA=$(git rev-parse HEAD)
echo "START $(date -Is) HEAD $HEADSHA BEFORE $BEFORE dirty=$(git status --porcelain | wc -l)"
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: tree not clean"; exit 2; }
CS=$(git diff --name-only "$BEFORE..$HEADSHA" -- src/ tests/ | tr '\n' ' ')
CH=$(git diff --name-only "$BEFORE..$HEADSHA" -- include/ | tr '\n' ' ')
M=$(git diff --name-only --diff-filter=M "$BEFORE..$HEADSHA" -- 'src/*.cpp' | sed 's|^|CMakeFiles/hven.dir/|; s|$|.o|' | LC_ALL=C sort | tr '\n' ' ')
OUTS=$(sed -n 's/^EXPECT_ADDED_OUTSIDE_UNIVERSE=\${EXPECT_ADDED_OUTSIDE_UNIVERSE-"\(.*\)"}$/\1/p' "$W/gates-e2fix6.sh")
# FX6: the MODIFIED sources outside the universe (the runner's declaration), excluded from the
# class A hypothesis and handed to g5 beside the ADDED outside ones.
OUTM=$(sed -n 's/^EXPECT_CLASS_A_OUTSIDE_SRCS=\${EXPECT_CLASS_A_OUTSIDE_SRCS-"\(.*\)"}$/\1/p' "$W/gates-e2fix6.sh")
NMOD=$(git diff --name-only --diff-filter=M "$BEFORE..$HEADSHA" -- 'src/*.cpp' 'tests/*.cpp' | grep -vxF -f <(printf '%s\n' $OUTM) | grep -c .)
ADD=$(git diff --name-only --diff-filter=A "$BEFORE..$HEADSHA" -- 'src/*.cpp' 'tests/*.cpp' | grep -vxF "$OUTS" | tr '\n' ' ')
LADD=$(git diff --name-only --diff-filter=A "$BEFORE..$HEADSHA" -- 'src/*.cpp' | sed 's|^|CMakeFiles/hven.dir/|; s|$|.o|' | LC_ALL=C sort | tr '\n' ' ')
ODIR="tests/CMakeFiles/$(sed -n 's/^OUTSIDE_TARGET=\([A-Za-z0-9_]*\)$/\1/p' "$W/gates-e2fix6.sh").dir"
echo "G5 args: CS=[$CS] CH=[$CH] MOV=[$M] NMOD=$NMOD ADD(in universe)=[$ADD] LADD=[$LADD] OUTS=[$OUTS] OUTM=[$OUTM] ODIR=$ODIR"
pgrep -x hven_bench_cell > /dev/null && echo "NOTE: a foreign cell binary is alive before the lock"
flock /tmp/box-build.lock bash -c '
  set -o pipefail
  echo "FOREGROUND_START R-psym-e2fix6 $(date -Is)"
  foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
  [ -z "$foreign" ] && echo "PGREP_EMPTY R-psym-e2fix6: no foreign cell binary under my lock" \
                    || echo "NOTE: a foreign cell binary is alive under my lock: $foreign"
  "$@"; rc=$?
  echo "FOREGROUND_END R-psym-e2fix6 $(date -Is) rc=${rc}"
  exit ${rc}
' _ bash "$S/g5.sh" "$BEFORE" "$CS" "$CH" 1 "$NMOD/$NMOD" "$NMOD/$NMOD" 0/0 0 0 "$M" "$M" "$ADD" "$LADD" \
  "$ODIR" "$OUTM $OUTS" ""
echo "LEG_RC R-psym-e2fix6 rc=$?"
echo "END $(date -Is) tree=$(git rev-parse --abbrev-ref HEAD) $(git rev-parse --short HEAD) dirty=$(git status --porcelain | wc -l)"
