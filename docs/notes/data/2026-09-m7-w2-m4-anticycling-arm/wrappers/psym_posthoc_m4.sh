#!/usr/bin/env bash
# THIS IS A POST-HOC RECORD. IT IS NOT A GATE AND IT DOES NOT MAKE G5 PASS.
# ============================================================================
# M7 W2 M4's G5 FAILED, at 2026-09-22T07:00:02-04:00, and stays failed. It
# failed against its OWN MACHINERY: the classifier the chain copied
# (`classify_psym_fix1.py`) carried M3b's changed-header list as a literal
# tuple, this round's `git diff --name-only f5205770..196f0ef0 -- include/`
# names TWO headers, and the cross-check REFUSED (rc 2). With no classification
# on disk the runner's five-count assert and two of its cross-config asserts
# had nothing to read and failed too. Nothing about the TREE was in question.
#
# The chain is NOT re-run: no file under `src/`, `include/`, `tests/`,
# `bench/` or `cmake/` has changed since `196f0ef0`, so a re-run would rebuild
# the same objects and re-take the same captures. What this script does instead
# is run the REPAIRED classifier -- `classify_psym_m4.py`, which DERIVES its
# header list from `<base>..<head>` and carries no literal -- and the same
# assertion layer, OFFLINE, over the captures the failed chain already took.
# The captures are the evidence; this is a second reading of them, taken
# afterwards, and it is labelled as one wherever it is quoted.
#
# WHAT THE SUBSTANTIVE CRITERION ALREADY SHOWED, from the raw transcripts and
# independently re-verified by hand before this script was written: zero
# `DIFFERS` lines in both configurations; all 42 library objects byte-identical
# in Release; 41 byte-identical in Debug with `src/qp/qp_engine.cpp.o`
# differing in `.debug_info`, `.debug_line`, `.debug_str` and
# `.rela.debug_line` and in NO other section; 0 non-debug movers in both.
#
# Usage: psym_posthoc_m4.sh [<out-dir>]
set -uo pipefail

ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m4gates
OUT=${1:-$S/gates/psym}
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m4-anticycling-arm
M3B=$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace
CLS=$ART/wrappers/classify_psym_m4.py
BASE=f5205770
HEAD=196f0ef0
MIN_CONTROL=1
WANT_DIFFERING=0; WANT_A=0; WANT_B=0

# THE DEP MAP COMES FROM EACH CONFIGURATION'S OWN AFTER-ARM BUILD TREE, and the
# choice is declared because it is not free. `psym-build`, the tree the
# captures were taken from, was left standing at the BEFORE arm (`f5205770`,
# Debug) when G5's last checkout was restored, so its `ninja -t deps` predates
# the new header. G1's and G3's trees were configured and built at HEAD with
# the same pinned compiler, so they carry the AFTER arm's include relation,
# which is the one the classification describes. Both readings are printed
# below and they agree on every production object; see this task's
# `gates/README.md`.
declare -A BUILDDIR=( [Release]="$S/gates-rel" [Debug]="$S/gates-dbg" )

for p in "$CLS" "$M3B/wrappers/gate_asserts_m3b.sh"; do
  [ -e "$p" ] || { echo "REFUSED: no such path: $p"; exit 93; }
done
. "$M3B/wrappers/gate_asserts_m3b.sh" || exit 93

CHANGED_HEADERS=$(git -C "$ROOT" diff --name-only "$BASE..$HEAD" -- include/ | tr '\n' ' ')
CHANGED_SOURCES=$(git -C "$ROOT" diff --name-only "$BASE..$HEAD" -- src/ tests/ | tr '\n' ' ')
echo "POSTHOC_START $(date -Is)"
echo "BASE=$BASE  HEAD=$HEAD"
echo "RUNNER-DERIVED CHANGED HEADERS: $CHANGED_HEADERS"
echo "RUNNER-DERIVED CHANGED SOURCES: $CHANGED_SOURCES"
echo

rc=0
for CFG in Release Debug; do
  BD=${BUILDDIR[$CFG]}
  CMPF=$OUT/compare-$CFG.txt
  [ -s "$CMPF" ] || { echo "REFUSED: no comparison at $CMPF"; exit 93; }
  [ -d "$BD" ] || { echo "REFUSED: no build tree at $BD"; exit 93; }
  echo "=== POST-HOC P-SYM $CFG   compare=$CMPF   deps=$BD"

  # THE RECORDED COMPARE RETURN CODE, read out of the FAILED CHAIN'S OWN LOG
  # rather than re-derived: `psym_compare.sh compare` is not re-run here, and
  # inventing its rc would be exactly the kind of carried declaration this
  # repair exists to end.
  cmp_rc=$(sed -n 's/^PSYM_COMPARE_RC_'"$CFG"'=\([0-9]*\).*/\1/p' "$S/logs/gates-m4.log" | head -1)
  if [ -z "$cmp_rc" ]; then
    echo "REFUSED: no PSYM_COMPARE_RC_$CFG line in the chain's gate log"
    exit 93
  fi
  echo "RECORDED_PSYM_COMPARE_RC_$CFG=$cmp_rc (read from logs/gates-m4.log)"

  # shellcheck disable=SC2086
  python3 -B "$CLS" --compare "$CMPF" --build "$BD" --repo "$ROOT" \
    --base "$BASE" --head "$HEAD" \
    --changed-sources $CHANGED_SOURCES --changed-headers $CHANGED_HEADERS \
    --expect-differing "$WANT_DIFFERING" \
    --min-negative-control "$MIN_CONTROL" > "$OUT/classification-$CFG-posthoc.txt" 2>&1
  cls_rc=$?
  echo "POSTHOC_CLASSIFY_RC_$CFG=$cls_rc"
  grep -E "^(HEADER |  repo|  base|  head|  cmd|  list|P-SYM|DIFFERING|  class|  UNEXPLAINED|  NEGATIVE|REFUSED)" \
    "$OUT/classification-$CFG-posthoc.txt"

  assert_psym "$CFG" "$OUT/classification-$CFG-posthoc.txt" "$cmp_rc" "$cls_rc" \
    "$MIN_CONTROL" "$WANT_DIFFERING" "$WANT_A" "$WANT_B" || rc=1

  # THE NEGATIVE CONTROL, RESTRICTED TO THE COMPARED UNIVERSE. The classifier's
  # own control is taken over every `.o` under the build tree, which includes
  # test-target objects `scripts/psym_compare.sh` deliberately does not
  # capture. That is a pre-existing looseness of the script (it is M3b's, and
  # M3b's copies are never edited); it is not repaired here, it is MEASURED
  # here, so the reader can see the control that stands inside the universe the
  # comparison actually covered.
  ctl_lib=$(ninja -C "$BD" -t deps 2>/dev/null \
    | awk '/^[^ ]/{o=$1} /qp_walk_trace\.h|qp_anti_cycling_arm\.h/{print o}' \
    | sed 's/:$//' | sort -u | grep -c '^CMakeFiles/hven\.dir/')
  noise_lib=$(grep -c '^NOISE-ONLY *CMakeFiles/hven\.dir/' "$CMPF")
  echo "CONTROL_IN_CAPTURED_UNIVERSE_$CFG=$((ctl_lib - noise_lib)) " \
       "(library includers $ctl_lib, of which NOISE-ONLY $noise_lib)"
  if [ "$((ctl_lib - noise_lib))" -lt "$MIN_CONTROL" ]; then
    echo "ASSERT FAIL post-hoc $CFG: the restricted negative control is below $MIN_CONTROL"
    rc=1
  else
    echo "ASSERT OK   post-hoc $CFG: restricted negative control = $((ctl_lib - noise_lib)) (>= $MIN_CONTROL)"
  fi
  echo
done

# shellcheck disable=SC2086
assert_psym_cross_config "$OUT/classification-Release-posthoc.txt" \
  "$OUT/classification-Debug-posthoc.txt" -- $CHANGED_SOURCES || rc=1

echo "POSTHOC_ASSERT_RC=$rc"
echo "POSTHOC_END $(date -Is)"
echo
echo "AND G5 IS STILL FAILED. This record does not change that: the gate that"
echo "ran on 2026-09-22 ran a classifier carrying the wrong list, and a gate is"
echo "judged by the run it had, not by the reading it got afterwards."
exit $rc
