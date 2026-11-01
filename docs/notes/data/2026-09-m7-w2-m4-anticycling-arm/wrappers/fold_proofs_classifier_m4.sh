#!/usr/bin/env bash
# FOLD PROOFS for `classify_psym_m4.py`'s DERIVED header list.
#
# A checker that is only ever run on the input it is expected to accept is not
# a checker. These folds run it on inputs whose verdict is known in advance --
# the wrong list, the right list, an unresolvable revision, an empty
# derivation -- and FAIL if any of them comes back with the wrong one.
#
# The fold that matters most is the FIRST. M7 W2 M4's G5 failed because the
# classifier it ran carried M3b's header list as a literal; the refusal that
# produced was correct, and the repair is that this copy derives the list from
# `git diff --name-only <base>..<head> -- include/`. FOLD 1 proves the
# cross-check still REFUSES a runner whose list disagrees with the derivation,
# and FOLD 2 proves the round's own real list is ACCEPTED -- that the repair
# did not turn the check off.
#
# Usage: fold_proofs_classifier_m4.sh <build-dir> [<compare-file>]
set -uo pipefail

ROOT=/home/ghecht/Projects/hven
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m4-anticycling-arm
CLS=$ART/wrappers/classify_psym_m4.py
BASE=f5205770
HEAD=196f0ef0
BUILD=${1:?usage: fold_proofs_classifier_m4.sh <build-dir> [<compare-file>]}
COMPARE=${2:-$ROOT/.scratch/m7w2m4gates/gates/psym/compare-Release.txt}

for p in "$CLS" "$COMPARE"; do
  [ -e "$p" ] || { echo "REFUSED: no such path: $p"; exit 93; }
done
[ -d "$BUILD" ] || { echo "REFUSED: no such build dir: $BUILD"; exit 93; }

TOTAL=0; FAILED=0
WORK=$(mktemp -d) || exit 93
trap 'rm -rf "$WORK"' EXIT

# fold NAME WANT_RC -- <args...>
fold() {
  local name="$1" want="$2" sep="$3"; shift 3
  [ "$sep" = "--" ] || { echo "fold: expected -- before the args"; exit 93; }
  TOTAL=$((TOTAL + 1))
  local out rc
  out=$(python3 -B "$CLS" "$@" 2>&1); rc=$?
  if [ "$rc" = "$want" ]; then
    printf 'FOLD %-2d ACCEPTED (rc=%s, wanted %s)  %s\n' "$TOTAL" "$rc" "$want" "$name"
  else
    printf 'FOLD %-2d **FAILED** (rc=%s, wanted %s)  %s\n' "$TOTAL" "$rc" "$want" "$name"
    FAILED=$((FAILED + 1))
  fi
  printf '%s\n' "$out" | grep -E '^(REFUSED|HEADER LIST ACCEPTED|  list )' | sed 's/^/          /'
  echo
}

COMMON=(--compare "$COMPARE" --build "$BUILD" --repo "$ROOT"
        --changed-sources src/qp/qp_engine.cpp tests/CMakeLists.txt
        tests/qp/test_qp_anti_cycling_arm.cpp
        --expect-differing 0 --min-negative-control 1)

# 1 -- THE ROUND'S OWN FAILURE, REPRODUCED. M3b's literal list, handed in as
#      the runner's claim, against a derivation that names two headers.
fold "the WRONG runner list (M3b's one header) is REFUSED" 2 -- \
  "${COMMON[@]}" --base "$BASE" --head "$HEAD" \
  --changed-headers include/hven/detail/qp/qp_walk_trace.h

# 2 -- AND THE RIGHT ONE IS ACCEPTED: the check was repaired, not removed.
fold "the RIGHT runner list (this round's two headers) is ACCEPTED" 0 -- \
  "${COMMON[@]}" --base "$BASE" --head "$HEAD" \
  --changed-headers include/hven/detail/qp/qp_anti_cycling_arm.h \
  include/hven/detail/qp/qp_walk_trace.h

# 3 -- a list that is a SUPERSET of the derivation is still a disagreement.
fold "a runner list with an EXTRA header is REFUSED" 2 -- \
  "${COMMON[@]}" --base "$BASE" --head "$HEAD" \
  --changed-headers include/hven/detail/qp/qp_anti_cycling_arm.h \
  include/hven/detail/qp/qp_walk_trace.h include/hven/detail/qp/qp_engine.h

# 4 -- NO runner list at all: the derivation alone governs, and it is the
#      round's real one, so the classification proceeds.
fold "no runner list: the derivation alone governs and ACCEPTS" 0 -- \
  "${COMMON[@]}" --base "$BASE" --head "$HEAD"

# 5 -- an unresolvable revision REFUSES rather than deriving an empty list.
fold "an unresolvable base REFUSES (R6)" 7 -- \
  "${COMMON[@]}" --base deadbeefdeadbeef --head "$HEAD"

# 6 -- a range that touches NO header under include/ REFUSES: with no header
#      there is no class B and no negative control, so the classification
#      could not be non-vacuous. $HEAD..$HEAD is the empty range.
fold "an EMPTY derivation REFUSES (R6), never passes vacuously" 7 -- \
  "${COMMON[@]}" --base "$HEAD" --head "$HEAD"

# 7 -- the direction of the range matters and is not silently normalised: the
#      REVERSED range still names the same two files (a diff is symmetric in
#      its name list), so this one is an ACCEPT and is here to say so plainly
#      rather than to leave the reader guessing.
fold "the reversed range names the same two headers and ACCEPTS" 0 -- \
  "${COMMON[@]}" --base "$HEAD" --head "$BASE"

# 8 -- and the pre-existing refusals still stand on top of the new one: a
#      missing comparison file is still R-missing (exit 3), not a pass.
fold "a missing comparison file still REFUSES" 3 -- \
  --compare "$WORK/not-a-file.txt" --build "$BUILD" --repo "$ROOT" \
  --base "$BASE" --head "$HEAD" --expect-differing 0

# 9 -- a MALFORMED comparison (no `P-SYM: ` summary) is R1, exit 3.
: > "$WORK/empty-compare.txt"
echo "this file carries no P-SYM summary line" >> "$WORK/empty-compare.txt"
fold "a comparison with no P-SYM summary still REFUSES (R1)" 3 -- \
  --compare "$WORK/empty-compare.txt" --build "$BUILD" --repo "$ROOT" \
  --base "$BASE" --head "$HEAD" --expect-differing 0

# 10 -- and R2: declaring a mover count this comparison does not carry fails.
fold "declaring 1 mover over a 0-mover comparison still REFUSES (R2)" 4 -- \
  "${COMMON[@]:0:10}" --expect-differing 1 --min-negative-control 1 \
  --base "$BASE" --head "$HEAD"

echo "CLASSIFIER_FOLD_TOTAL=$TOTAL  CLASSIFIER_FOLD_FAILED=$FAILED"
[ "$FAILED" -eq 0 ]
