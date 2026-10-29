#!/usr/bin/env bash
# M7 A3 gates run: COPIED from the A2 rehearse-a2.sh (NOT edited in place), re-pointed at ../gates-a3.sh; every argument DERIVED from git and the runner (no outside-universe source this range: OUTM/OUTS empty). Its header follows.
# M7 A2 gates run: COPIED from the RST rehearse-rst.sh (NOT edited in place), re-pointed at ../gates-a2.sh; every argument DERIVED from git and the runner. Its header follows.
# M7 W2 SCL gates run: COPIED from the RLA rehearse-rla.sh (NOT edited in place), re-pointed at ../gates-scl.sh; every argument is DERIVED from git and the runner. Its header follows.
# M7 W2 SNX + SNR gates run: COPIED from the SDL-4 + SDL-6 rehearse-sdl4-sdl6.sh (NOT edited), re-pointed at ../gates-scl.sh; every argument is DERIVED from git and the runner. Its header follows.
# M7 W2 NC4 + R2 gates run: COPIED from NC3 run-2 rehearse-nc3-r2.sh (NOT edited), re-pointed at ../gates-scl.sh; ROOT derived (worktree);
# ODIR is now a LIST (two outside targets, see the runner header (c)). The NC3 run-2 text follows.
# NC3 RUN 2 (successor): COPIED from run 1 rehearse-nc3.sh (NOT edited), re-pointed at ../gates-scl.sh; runs on the final pair
# a522f115/b7993013. Run 1 text follows.
# M7 W2 ROUND NC3 -- COPIED from the P1 rehearsal script (itself e2f9 / e2f8 / E4 run 2) and
# re-pointed (runner name, task directory, leg labels). NC3: four headers, five MODIFIED .cpp (four
# inside the P-SYM universe, one -- tests/qp/test_qp_unrun_dependency_test.cpp -- in the outside
# target), one ADDED .cpp (tests/qp/test_qp_saddle_exit.cpp, outside), so OUTM and OUTS are each ONE
# file. The P1 text follows for provenance.
# M7 W2 ROUND P1 -- COPIED from the e2f9 rehearsal script (itself e2f8 / E4 run 2) and re-pointed
# (runner name, task directory, scratch ask, leg labels, previous gate record, G4 label); the e2f9 prose
# below is the e2f9 round's and is kept for provenance. P1: one header, two MODIFIED .cpp both inside
# the P-SYM universe, no ADDED .cpp, nothing outside the universe; the OUTS/OUTM lists are EMPTY.
# M7 W2 ROUND P1 (the e2f9 text follows) -- COPIED from the e2f8 rehearse-e2f8.sh (itself E4 run 2's rehearse-r2.sh)
# and re-pointed at ../gates-scl.sh (path, runner name, leg label); nothing else changed. E4's
# header follows:
# M7 W2 E4 RUN 2 -- the P-SYM REHEARSAL: the runner's own g5 segment (EXTRACTED from
# ../gates-e4-r2.sh, byte for byte, not re-typed) at HEAD = the E4 code commit against BEFORE,
# both configurations, one lock hold. Copied from the previous round's rehearse.sh and re-pointed:
# the scratch tree is ASKED of the runner (--print-scratch) plus /rehearsal, so the round keeps
# ONE scratch literal; every argument is DERIVED from git and the runner. The declaration
# arguments are HYPOTHESES (class A only, class B 0); a refusal still prints the measured shape.
#
# E4: the derivations below take bench/ as well as src/ and tests/, for the reason
# ../gates-e4-r2.sh's header states at E4 (c) -- bench/bench_scale.cpp is MODIFIED in this range
# and its object is in the P-SYM capture universe, so a two-directory derivation would hand
# g5 a changed-source list that does not name a changed source.
set -o pipefail
ROOT=$(git -C "$(dirname "${BASH_SOURCE[0]}")" rev-parse --show-toplevel) || { echo "REFUSED: no worktree root"; exit 3; }
export ROOT
W=${W:-$ROOT/docs/notes/data/2026-10-m7-a3-gates/wrappers-gates}
BASE_S=$(bash "$W/gates-a3.sh" --print-scratch) || { echo "REFUSED: no scratch from the runner"; exit 3; }
case "$BASE_S" in "$ROOT"/.scratch/?*) : ;; *) echo "REFUSED: scratch '$BASE_S'"; exit 3 ;; esac
export GATES_SCRATCH=$BASE_S/rehearsal
S=$GATES_SCRATCH
mkdir -p "$S"
# THE SEGMENT, FROM THE RUNNER'S TEXT: the heredoc body between `cat > "$S/g5.sh" <<'INNER'`
# and its `INNER`.
awk '/^cat > "\$S\/g5.sh" <<.INNER.$/ {f=1; next} f && /^INNER$/ {exit} f {print}' \
  "$W/gates-a3.sh" > "$S/g5.sh"
[ -s "$S/g5.sh" ] || { echo "REFUSED: could not extract g5 from the runner"; exit 3; }
sed -i "s|^ROOT=@ROOT@;|ROOT=$ROOT;|" "$S/g5.sh" && grep -q "^ROOT=$ROOT;" "$S/g5.sh" || { echo "REFUSED: g5 ROOT placeholder not substituted"; exit 3; }
cd "$ROOT" || exit 90
BEFORE=$(sed -n 's/^BEFORE=\${BEFORE:-\([0-9a-f]*\)}$/\1/p' "$W/gates-a3.sh")
[ -n "$BEFORE" ] || { echo "REFUSED: no BEFORE in the runner"; exit 3; }
HEADSHA=$(git rev-parse HEAD)
echo "START $(date -Is) HEAD $HEADSHA BEFORE $BEFORE dirty=$(git status --porcelain | wc -l)"
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: tree not clean"; exit 2; }
CS=$(git diff --name-only "$BEFORE..$HEADSHA" -- src/ tests/ bench/ | tr '\n' ' ')
CH=$(git diff --name-only "$BEFORE..$HEADSHA" -- include/ | tr '\n' ' ')
M=$(git diff --name-only --diff-filter=M "$BEFORE..$HEADSHA" -- 'src/*.cpp' | sed 's|^|CMakeFiles/hven.dir/|; s|$|.o|' | LC_ALL=C sort | tr '\n' ' ')
OUTS=$(sed -n 's/^EXPECT_ADDED_OUTSIDE_UNIVERSE=\${EXPECT_ADDED_OUTSIDE_UNIVERSE-"\(.*\)"}$/\1/p' "$W/gates-a3.sh")
# The MODIFIED sources outside the universe (the runner's declaration) -- THREE this round --
# excluded from the class A hypothesis and handed to g5 beside the ADDED outside ones (none).
OUTM=$(sed -n 's/^EXPECT_CLASS_A_OUTSIDE_SRCS=\${EXPECT_CLASS_A_OUTSIDE_SRCS-"\(.*\)"}$/\1/p' "$W/gates-a3.sh")
NMOD=$(git diff --name-only --diff-filter=M "$BEFORE..$HEADSHA" -- 'src/*.cpp' 'tests/*.cpp' 'bench/*.cpp' | grep -vxF -f <(printf '%s\n' $OUTM) | grep -c .)
ADD=$(git diff --name-only --diff-filter=A "$BEFORE..$HEADSHA" -- 'src/*.cpp' 'tests/*.cpp' 'bench/*.cpp' | grep -vxF -f <(printf '%s\n' $OUTS) | tr '\n' ' ')
LADD=$(git diff --name-only --diff-filter=A "$BEFORE..$HEADSHA" -- 'src/*.cpp' | sed 's|^|CMakeFiles/hven.dir/|; s|$|.o|' | LC_ALL=C sort | tr '\n' ' ')
OT=$(sed -n 's/^OUTSIDE_TARGETS="\(.*\)"$/\1/p' "$W/gates-a3.sh"); [ -n "$OT" ] || { echo "REFUSED: no OUTSIDE_TARGETS in the runner"; exit 3; }
ODIR=$(for t in $OT; do printf 'tests/CMakeFiles/%s.dir ' "$t"; done); ODIR=${ODIR% }
echo "G5 args: CS=[$CS] CH=[$CH] MOV=[$M] NMOD=$NMOD ADD(in universe)=[$ADD] LADD=[$LADD] OUTS=[$OUTS] OUTM=[$OUTM] ODIR=$ODIR"
pgrep -x hven_bench_cell > /dev/null && echo "NOTE: a foreign cell binary is alive before the lock"
flock /tmp/box-build.lock bash -c '
  set -o pipefail
  echo "FOREGROUND_START R-psym-a3 $(date -Is)"
  foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
  [ -z "$foreign" ] && echo "PGREP_EMPTY R-psym-a3: no foreign cell binary under my lock" \
                    || echo "NOTE: a foreign cell binary is alive under my lock: $foreign"
  "$@"; rc=$?
  echo "FOREGROUND_END R-psym-a3 $(date -Is) rc=${rc}"
  exit ${rc}
' _ bash "$S/g5.sh" "$BEFORE" "$CS" "$CH" 1 "$NMOD/$NMOD" "$NMOD/$NMOD" 0/0 0 0 "$M" "$M" "$ADD" "$LADD" \
  "$ODIR" "$OUTM $OUTS" ""
echo "LEG_RC R-psym-a3 rc=$?"
echo "END $(date -Is) tree=$(git rev-parse --abbrev-ref HEAD) $(git rev-parse --short HEAD) dirty=$(git status --porcelain | wc -l)"
