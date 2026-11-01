#!/usr/bin/env bash
# THE FOLD PROOFS FOR M7 W2 M3b FIX ROUND 1's P-SYM ASSERTION LAYER
# (`gate_asserts_m3b.sh` version 3 + `psym_assert_only.sh` +
# `classify_psym_m3b.py`).
#
# It starts no solve and builds nothing. Its inputs are text files that already
# exist -- M3a fix round 1's COMMITTED classifications, this round's own
# derivation captures, and `git` -- and every fold that breaks something breaks
# a COPY. The originals are read-only and are never written.
#
# THREE PARTS, and the first is there because this round CHANGED a shared rule:
#
#   PART 1  THE CARRIED-OVER REFUSALS, RE-RUN AGAINST THIS ROUND'S COPY.
#           Version 3 differs from version 2 in one line of `assert_psym` (the
#           expected `compare` exit code is derived from the declared mover
#           count instead of being fixed at 1). The settler's addendum requires
#           that version 2's fold proofs be re-run against the copy, and the
#           honest way to do it is on version 2's OWN inputs: M3a fix round 1's
#           committed `psym-classification-{Release,Debug}.txt`, a real capture
#           pair WITH movers. If version 3 broke any of those refusals, this
#           part fails.
#
#   PART 2  THIS ROUND'S OWN SHAPE: a ZERO-mover capture pair, which version 2
#           could not express at all. The acceptance, and the refusals that
#           have to stand around it -- above all, that a declared 0 which meets
#           a real mover FAILS.
#
#   PART 3  THE CLASSIFIER, `classify_psym_m3b.py`: its new `--expect-differing`
#           refusal, and the REGISTERED HARVEST DEFECT this round fixed -- an
#           object whose verdict is NOISE-ONLY plus a RELOCATIONS line used to
#           escape the differing set entirely and be printed as byte-identical.
#           Part 3 runs the classifier, which reads a build tree's Ninja deps;
#           it compiles nothing.
#
# Usage: bash fold_proofs_psym_m3b.sh
set -o pipefail
ROOT=/home/ghecht/Projects/hven
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace
M3A=$ROOT/docs/notes/data/2026-09-m7-w2-m3a-request-probe
DERIVE=$ROOT/.scratch/m7w2m3b/derive
BUILD=${BUILD:-$ROOT/build}
TMP=$(mktemp -d "${TMPDIR:-/tmp}/m3b-psymfold-XXXXXX") || exit 90
trap 'rm -rf "$TMP"' EXIT

# shellcheck source=gate_asserts_m3b.sh
. "$ART/wrappers/gate_asserts_m3b.sh" || exit 90

RUNNER=$ART/wrappers/psym_assert_only.sh
CLASSIFIER=${CLASSIFIER:-$ART/wrappers/classify_psym_fix1.py}
fails=0
n=0

expect_nonzero() {
    local label="$1"; shift
    local out rc
    out=$("$@" 2>&1); rc=$?
    n=$((n + 1))
    if [ $rc -ne 0 ]; then
        printf 'FOLD %02d  REFUSED (rc=%d)  %s\n' "$n" "$rc" "$label"
        printf '          %s\n' "$(echo "$out" | grep -E 'ASSERT FAIL|REFUSED' | head -2)"
        return 0
    fi
    printf 'FOLD %02d  *** DID NOT REFUSE (rc=0) ***  %s\n' "$n" "$label"
    printf '          %s\n' "$(echo "$out" | tail -4 | head -3)"
    fails=$((fails + 1))
    return 1
}

expect_zero() {
    local label="$1"; shift
    local out rc
    out=$("$@" 2>&1); rc=$?
    n=$((n + 1))
    if [ $rc -eq 0 ]; then
        printf 'FOLD %02d  ACCEPTED (rc=0)  %s\n' "$n" "$label"
        return 0
    fi
    printf 'FOLD %02d  *** REFUSED A GOOD INPUT (rc=%d) ***  %s\n' "$n" "$rc" "$label"
    printf '          %s\n' "$(echo "$out" | grep -E 'ASSERT FAIL|REFUSED' | head -3)"
    fails=$((fails + 1))
    return 1
}

echo "PSYM_FOLD_PROOFS_FIX1_START $(date -Is)"
echo "root=$ROOT"
echo "HEAD $(git -C "$ROOT" rev-parse HEAD)  dirty-files=$(git -C "$ROOT" status --porcelain | wc -l)"
echo "-- wrapper identity: WORKTREE digest vs the digest of the SAME path AT HEAD."
echo "-- They must agree, or this transcript did not exercise what landed."
for f in gate_asserts_m3b.sh psym_assert_only.sh fold_proofs_psym_fix1.sh classify_psym_fix1.py check_sections_fix1.sh section_digest_fix1.py; do
    rel="docs/notes/data/2026-09-m7-w2-m3b-minor-trace/wrappers/$f"
    w=$(sha256sum "$ART/wrappers/$f" | cut -d' ' -f1)
    c=$(git -C "$ROOT" show "HEAD:$rel" 2> /dev/null | sha256sum | cut -d' ' -f1)
    if [ "$w" = "$c" ]; then
        printf '%-28s sha256 %s  (worktree == HEAD)\n' "$f" "$w"
    else
        printf '%-28s sha256 %s  *** DIFFERS FROM HEAD: %s ***\n' "$f" "$w" "$c"
        fails=$((fails + 1))
    fi
done
echo

################################################################################
echo "== PART 1 -- M3a FIX ROUND 1's FOLDS, RE-RUN AGAINST VERSION 3 =========="
################################################################################
# The inputs are that round's COMMITTED classifications: a real capture pair
# with movers (Release 10 = 7 + 3, Debug 11 = 8 + 3), exactly what version 2 was
# written for. Version 3 must behave identically on them.
M3A_REL_SRC=$M3A/gates-fix1/psym-classification-Release.txt
M3A_DBG_SRC=$M3A/gates-fix1/psym-classification-Debug.txt
M3A_REL=$TMP/m3a-rel.txt
M3A_DBG=$TMP/m3a-dbg.txt
cp "$M3A_REL_SRC" "$M3A_REL"
cp "$M3A_DBG_SRC" "$M3A_DBG"
printf '   %s\n     sha256 %s\n' "$M3A_REL_SRC" "$(sha256sum "$M3A_REL_SRC" | cut -d' ' -f1)"
printf '   %s\n     sha256 %s\n' "$M3A_DBG_SRC" "$(sha256sum "$M3A_DBG_SRC" | cut -d' ' -f1)"
M3A_SOURCES=$(git -C "$ROOT" diff --name-only 11b066e8 ea027804 -- src/ tests/ | tr '\n' ' ')
echo "   M3a fix1's changed sources, from git: $M3A_SOURCES"
echo

m3a_layer() {
    REL_CLS="$1" DBG_CLS="$2" \
        EXPECT_DIFFERING_RELEASE="${3:-10}" EXPECT_CLASS_A_RELEASE="${4:-7}" \
        EXPECT_CLASS_B_RELEASE="${5:-3}" \
        EXPECT_DIFFERING_DEBUG="${6:-11}" EXPECT_CLASS_A_DEBUG="${7:-8}" \
        EXPECT_CLASS_B_DEBUG="${8:-3}" \
        COMPARE_RC_RELEASE=1 COMPARE_RC_DEBUG=1 \
        CLASSIFY_RC_RELEASE=0 CLASSIFY_RC_DEBUG=0 \
        MIN_NEGATIVE_CONTROL=1 CHANGED_SOURCES="$M3A_SOURCES" \
        RECORD_BANNER=0 BASE=11b066e8 \
        bash "$RUNNER"
}

expect_zero "P1: M3a's REAL capture pair still passes under version 3 (10/7/3 and 11/8/3, compare rc 1)" \
    m3a_layer "$M3A_REL" "$M3A_DBG"

# shellcheck disable=SC2086
expect_zero "P1: M3a's pair still passes assert_psym_cross_config on its own" \
    assert_psym_cross_config "$M3A_REL" "$M3A_DBG" -- $M3A_SOURCES

expect_nonzero "P1: M3a's ONE declared shape (12/9/3 for BOTH configs) is still refused -- G5's own failure, reproduced" \
    m3a_layer "$M3A_REL" "$M3A_DBG" 12 9 3 12 9 3

expect_nonzero "P1: PER-CONFIGURATION is still real: Release asserted with Debug's shape" \
    m3a_layer "$M3A_REL" "$M3A_DBG" 11 8 3 11 8 3

expect_nonzero "P1: PER-CONFIGURATION is still real, the other way" \
    m3a_layer "$M3A_REL" "$M3A_DBG" 10 7 3 10 7 3

grep -v '^      tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_cap_levers.cpp.o$' \
    "$M3A_DBG" > "$TMP/m3a-dbg-classB-short.txt"
# shellcheck disable=SC2086
expect_nonzero "P1: cross-config clause 1 -- the two class B sets are NOT equal" \
    assert_psym_cross_config "$M3A_REL" "$TMP/m3a-dbg-classB-short.txt" -- $M3A_SOURCES

sed 's|^  UNEXPLAINED: 0$|  UNEXPLAINED: 1\n      CMakeFiles/hven.dir/src/nowhere/not_a_real_object.cpp.o|' \
    "$M3A_REL" > "$TMP/m3a-rel-unexplained.txt"
sed 's|^  UNEXPLAINED: 0$|  UNEXPLAINED: 1\n      CMakeFiles/hven.dir/src/nowhere/not_a_real_object.cpp.o|' \
    "$M3A_DBG" > "$TMP/m3a-dbg-unexplained.txt"
expect_nonzero "P1: cross-config clause 2 -- one UNEXPLAINED object in the RELEASE classification" \
    m3a_layer "$TMP/m3a-rel-unexplained.txt" "$M3A_DBG"
expect_nonzero "P1: cross-config clause 2 -- one UNEXPLAINED object in the DEBUG classification" \
    m3a_layer "$M3A_REL" "$TMP/m3a-dbg-unexplained.txt"
# shellcheck disable=SC2086
expect_nonzero "P1: cross-config clause 2, isolated" \
    assert_psym_cross_config "$TMP/m3a-rel-unexplained.txt" "$M3A_DBG" -- $M3A_SOURCES

sed -e 's|^  class A -- \(.*\): 8$|  class A -- \1: 9\n      CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o|' \
    -e 's|^DIFFERING OBJECTS: 11$|DIFFERING OBJECTS: 12|' \
    "$M3A_DBG" > "$TMP/m3a-dbg-classA-stray.txt"
# shellcheck disable=SC2086
expect_nonzero "P1: cross-config clauses 3+4 -- class A differs on an object whose SOURCE that round did not change" \
    assert_psym_cross_config "$M3A_REL" "$TMP/m3a-dbg-classA-stray.txt" -- $M3A_SOURCES
expect_nonzero "P1: the same stray class A object end to end, with the per-config shape declared to match it" \
    m3a_layer "$M3A_REL" "$TMP/m3a-dbg-classA-stray.txt" 10 7 3 12 9 3

expect_nonzero "P1: an EMPTY changed-source list" \
    assert_psym_cross_config "$M3A_REL" "$M3A_DBG" --
expect_nonzero "P1: a classification file that does not exist" \
    m3a_layer "$TMP/no-such-file.txt" "$M3A_DBG"
: > "$TMP/empty.txt"
expect_nonzero "P1: an EMPTY classification file" \
    m3a_layer "$M3A_REL" "$TMP/empty.txt"
grep -v '^  UNEXPLAINED:' "$M3A_REL" > "$TMP/m3a-rel-no-count.txt"
expect_nonzero "P1: a classification missing one of its five counts" \
    m3a_layer "$TMP/m3a-rel-no-count.txt" "$M3A_DBG"
sed 's|^\(  NEGATIVE CONTROL[^:]*\): 107$|\1: 0|' "$M3A_REL" > "$TMP/m3a-rel-no-control.txt"
expect_nonzero "P1: a negative control of 0" \
    m3a_layer "$TMP/m3a-rel-no-control.txt" "$M3A_DBG"

################################################################################
echo
echo "== PART 2 -- THIS ROUND'S ZERO-MOVER SHAPE =============================="
################################################################################
REL_SRC=$DERIVE/classification-Release.txt
DBG_SRC=$DERIVE/classification-Debug.txt
if [ ! -s "$REL_SRC" ] || [ ! -s "$DBG_SRC" ]; then
    echo "REFUSED: this round's derivation classifications are missing --"
    echo "         run wrappers/derive_psym_m3b.sh for both configurations first."
    exit 2
fi
REL=$TMP/rel.txt
DBG=$TMP/dbg.txt
cp "$REL_SRC" "$REL"
cp "$DBG_SRC" "$DBG"
printf '   %s\n     sha256 %s\n' "$REL_SRC" "$(sha256sum "$REL_SRC" | cut -d' ' -f1)"
printf '   %s\n     sha256 %s\n' "$DBG_SRC" "$(sha256sum "$DBG_SRC" | cut -d' ' -f1)"
CHANGED_SOURCES=$(git -C "$ROOT" diff --name-only 56032eed -- src/ tests/ | tr '\n' ' ')
echo "   this round's changed sources, from git: $CHANGED_SOURCES"
echo

layer() {
    REL_CLS="$1" DBG_CLS="$2" \
        EXPECT_DIFFERING_RELEASE="${3:-0}" EXPECT_CLASS_A_RELEASE="${4:-0}" \
        EXPECT_CLASS_B_RELEASE="${5:-0}" \
        EXPECT_DIFFERING_DEBUG="${6:-0}" EXPECT_CLASS_A_DEBUG="${7:-0}" \
        EXPECT_CLASS_B_DEBUG="${8:-0}" \
        COMPARE_RC_RELEASE="${9:-0}" COMPARE_RC_DEBUG="${10:-0}" \
        CLASSIFY_RC_RELEASE=0 CLASSIFY_RC_DEBUG=0 \
        MIN_NEGATIVE_CONTROL=1 CHANGED_SOURCES="$CHANGED_SOURCES" \
        RECORD_BANNER=0 BASE=9fee5545 \
        bash "$RUNNER"
}

expect_zero "P2: THIS round's REAL zero-mover pair passes (0/0/0 both configs, compare rc 0)" \
    layer "$REL" "$DBG"

# shellcheck disable=SC2086
expect_zero "P2: the zero-mover pair passes assert_psym_cross_config on its own (empty class A and class B sets)" \
    assert_psym_cross_config "$REL" "$DBG" -- $CHANGED_SOURCES

# THE VERSION-2 CONTRAST, and the reason version 3 exists: version 2 hard-coded
# `compare rc == 1`, so it REFUSES this round's correct, clean capture pair.
expect_nonzero "P2: CONTRAST -- version 2's assert_psym refuses this round's REAL, CORRECT zero-mover pair" \
    bash -c ". '$M3A/wrappers/gate_asserts_v2.sh'; assert_psym Release '$REL' 0 0 1 0 0 0"

# ...and version 3 must not have become permissive about the rc: a zero
# declaration met by a `compare` that exited 1 is a contradiction and fails.
expect_nonzero "P2: a declared 0 with compare rc 1 -- the comparison found something the declaration denies" \
    layer "$REL" "$DBG" 0 0 0 0 0 0 1 1

expect_nonzero "P2: a declared 0 with compare rc 1 in DEBUG only" \
    layer "$REL" "$DBG" 0 0 0 0 0 0 0 1

# A REAL MOVER AGAINST A ZERO DECLARATION. One class A object is inserted into
# the Release copy with its counts kept internally consistent, so `assert_psym`
# has a self-consistent file in front of it and the ONLY thing that can refuse
# it is the declared count.
sed -e 's|^DIFFERING OBJECTS: 0$|DIFFERING OBJECTS: 1|' \
    -e 's|^  class A -- \(.*\): 0$|  class A -- \1: 1\n      CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o|' \
    "$REL" > "$TMP/rel-one-mover.txt"
expect_nonzero "P2: ONE real mover in Release against a declared 0 -- the claim this task makes, falsified" \
    layer "$TMP/rel-one-mover.txt" "$DBG"

expect_nonzero "P2: the same mover in DEBUG against a declared 0" \
    layer "$REL" "$TMP/rel-one-mover.txt"

# And declaring the mover does NOT rescue it while the rc says otherwise:
# the shape now matches, so what refuses it is the rc rule alone.
expect_nonzero "P2: the mover DECLARED (1/1/0) but compare rc still 0 -- a mover with a PASS is a contradiction" \
    layer "$TMP/rel-one-mover.txt" "$DBG" 1 1 0 0 0 0 0 0

sed 's|^\(  NEGATIVE CONTROL[^:]*\): 70$|\1: 0|' "$REL" > "$TMP/rel-no-control.txt"
expect_nonzero "P2: a ZERO declaration with a ZERO negative control -- nothing was compared, and 0 movers means nothing" \
    layer "$TMP/rel-no-control.txt" "$DBG"

sed 's|^  UNEXPLAINED: 0$|  UNEXPLAINED: 1\n      CMakeFiles/hven.dir/src/nowhere/not_a_real_object.cpp.o|' \
    "$REL" > "$TMP/rel-unexplained.txt"
expect_nonzero "P2: an UNEXPLAINED object beside a declared 0" \
    layer "$TMP/rel-unexplained.txt" "$DBG"

: > "$TMP/empty2.txt"
expect_nonzero "P2: an EMPTY classification file against a zero declaration" \
    layer "$REL" "$TMP/empty2.txt"

expect_nonzero "P2: a classification file that does not exist, against a zero declaration" \
    layer "$TMP/no-such-file-2.txt" "$DBG"

################################################################################
echo
echo "== PART 3 -- THE CLASSIFIER ============================================="
################################################################################
if [ ! -d "$BUILD" ]; then
    echo "REFUSED: no build tree at $BUILD for the classifier to read deps from"
    exit 2
fi
CMP_SRC=${CMP_SRC:-$ART/gates/psym-compare-Release.txt}
if [ ! -s "$CMP_SRC" ]; then
    echo "REFUSED: no comparison at $CMP_SRC"
    exit 2
fi
printf '   %s\n     sha256 %s\n' "$CMP_SRC" "$(sha256sum "$CMP_SRC" | cut -d' ' -f1)"
CHANGED_HEADERS=$(git -C "$ROOT" diff --name-only 56032eed -- include/ | tr '\n' ' ')
echo "   this round's changed headers, from git: $CHANGED_HEADERS"
echo

classify() { # COMPARE_FILE EXPECT_DIFFERING
    # shellcheck disable=SC2086
    python3 -B "$CLASSIFIER" --compare "$1" --build "$BUILD" \
        --changed-sources $CHANGED_SOURCES --changed-headers $CHANGED_HEADERS \
        --expect-differing "$2" --min-negative-control 1
}

expect_zero "P3: the REAL comparison classifies clean against --expect-differing 0" \
    classify "$CMP_SRC" 0

expect_nonzero "P3: the same comparison against --expect-differing 1 (R2) -- a declared count that is not the measured one" \
    classify "$CMP_SRC" 1

# THE REGISTERED HARVEST DEFECT. A `NOISE-ONLY` object with a `RELOCATIONS`
# line -- the exact shape M3a fix1's Release `trace_writer.cpp.o` had -- is
# inserted into a COPY of the real comparison. Under every earlier copy of this
# classifier it would have been harvested by nothing, counted as 0 differing,
# and printed in the NEGATIVE CONTROL under "are byte-identical". Under this
# copy it is a mover, so a declaration of 0 must now FAIL.
# The object injected is `qp_engine.cpp.o`, whose SOURCE this round changed, so
# that once it is harvested it is CLASS A and the classifier's R4 has nothing to
# say about it -- the only thing left to decide the fold is whether the harvest
# saw it at all.
{
    head -4 "$CMP_SRC"
    echo "NOISE-ONLY  CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o"
    echo "RELOCATIONS CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o (the instruction stream is within the accepted noise class,"
    echo "            but one relocation target is renamed)"
    tail -n +5 "$CMP_SRC"
} > "$TMP/cmp-reloc.txt"
expect_nonzero "P3: THE REGISTERED DEFECT -- a NOISE-ONLY object with a RELOCATIONS line is now HARVESTED, so a declared 0 fails" \
    classify "$TMP/cmp-reloc.txt" 0

expect_zero "P3: and declaring that one mover accepts it -- the harvest counts it as exactly one object, not two, and classifies it" \
    classify "$TMP/cmp-reloc.txt" 1

# THE CONTRAST THAT SHOWS THE FIX IS LOAD-BEARING, stated as a two-sided
# comparison on the SAME file rather than as an absence: M3a's classifier
# harvests NO differing object from it (its own R2 says so in those words),
# while this round's harvests exactly one.
expect_zero "P3: CONTRAST -- M3a's classifier reports NO differing object on that same file; this round's reports exactly one" \
    bash -c "old=\$(python3 -B '$M3A/wrappers/classify_psym_fix1.py' --compare '$TMP/cmp-reloc.txt' \
                 --build '$BUILD' --changed-sources $CHANGED_SOURCES \
                 --changed-headers include/hven/core/solver_counters.h include/hven/core/solver_status.h \
                 --min-negative-control 1 2>&1);
             echo \"\$old\" | grep -q 'names no DIFFERS object' || { echo \"OLD DID NOT REPORT AN EMPTY HARVEST: \$old\"; exit 1; };
             new=\$(python3 -B '$CLASSIFIER' --compare '$TMP/cmp-reloc.txt' --build '$BUILD' \
                 --changed-sources $CHANGED_SOURCES --changed-headers $CHANGED_HEADERS \
                 --expect-differing 0 --min-negative-control 1 2>&1);
             echo \"\$new\" | grep -q 'names 1 differing object' || { echo \"NEW DID NOT HARVEST ONE: \$new\"; exit 1; };
             exit 0"

: > "$TMP/cmp-empty.txt"
expect_nonzero "P3: R1 -- a comparison with no 'P-SYM: ' summary line" \
    classify "$TMP/cmp-empty.txt" 0

expect_nonzero "P3: a comparison file that does not exist" \
    classify "$TMP/no-such-compare.txt" 0

expect_nonzero "P3: a changed-header list that disagrees with the classifier's own declared list" \
    bash -c "python3 -B '$CLASSIFIER' --compare '$CMP_SRC' --build '$BUILD' \
             --changed-sources $CHANGED_SOURCES --changed-headers include/hven/core/types.h \
             --expect-differing 0 --min-negative-control 1"

expect_nonzero "P3: an impossible negative-control floor -- 0 movers with a control below the floor explains nothing" \
    bash -c "python3 -B '$CLASSIFIER' --compare '$CMP_SRC' --build '$BUILD' \
             --changed-sources $CHANGED_SOURCES --changed-headers $CHANGED_HEADERS \
             --expect-differing 0 --min-negative-control 100000"

echo

################################################################################
echo
echo "== PART 4 -- THE THREE-WAY LABELLING (FIX ROUND 1) ======================"
################################################################################
# THE DEFECT: the negative control used to be the complement of the MOVER set
# alone, so an object P-SYM reported as NOISE-ONLY -- bytes differ, differences
# are __LINE__-class noise -- was printed under a heading that said
# "byte-identical". These folds prove the repair on REAL P-SYM output: the three
# production-library objects M3b's own Debug comparison reports as NOISE-ONLY.
DBG_CMP=$ART/gates/psym-compare-Debug.txt
if [ ! -s "$DBG_CMP" ]; then
    echo "REFUSED: no Debug comparison at $DBG_CMP for the labelling folds"
    exit 2
fi
NOISE_LIB="CMakeFiles/hven.dir/src/drivers/sqp_solver.cpp.o CMakeFiles/hven.dir/src/qp/ipqp_engine.cpp.o CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o"
echo "   the three NOISE-ONLY library objects this part is about:"
printf '     %s\n' $NOISE_LIB

classify_dbg() { # COMPARE_FILE
    # shellcheck disable=SC2086
    python3 -B "$CLASSIFIER" --compare "$1" --build "$BUILD" \
        --changed-sources $CHANGED_SOURCES --changed-headers $CHANGED_HEADERS \
        --expect-differing 0 --min-negative-control 1
}

# P4.1 -- the OLD classifier prints all three under "byte-identical"; the NEW
# one prints none of them there. Run side by side, on the same input, so the
# fold is a DIFFERENCE between the two scripts and not an assertion about one.
expect_zero "P4: the repaired classifier names ZERO of the three NOISE-ONLY objects in its BYTE-IDENTICAL control, and the OLD one names all three" \
    bash -c "
      old=\$(python3 -B '$ART/wrappers/classify_psym_m3b.py' --compare '$DBG_CMP' --build '$BUILD' \
             --changed-sources $CHANGED_SOURCES \
             --changed-headers include/hven/detail/qp/qp_engine.h include/hven/detail/qp/qp_walk_trace.h \
             --expect-differing 0 --min-negative-control 1 2>/dev/null |
            sed -n '/NEGATIVE CONTROL/,\$p')
      new=\$(python3 -B '$CLASSIFIER' --compare '$DBG_CMP' --build '$BUILD' \
             --changed-sources $CHANGED_SOURCES --changed-headers $CHANGED_HEADERS \
             --expect-differing 0 --min-negative-control 1 2>/dev/null |
            sed -n '/NEGATIVE CONTROL/,\$p')
      no=0; nn=0
      for o in $NOISE_LIB; do
          printf '%s' \"\$old\" | grep -qF \"\$o\" && no=\$((no+1))
          printf '%s' \"\$new\" | grep -qF \"\$o\" && nn=\$((nn+1))
      done
      echo \"      OLD control names \$no/3, NEW control names \$nn/3\"
      [ \"\$no\" -eq 3 ] && [ \"\$nn\" -eq 0 ]"

# P4.2 -- and the new one does not simply DROP them: each is printed, labelled
# NOISE-ONLY and marked as a library object.
expect_zero "P4: each of the three is reported under NOISE-ONLY, tagged LIB" \
    bash -c "
      out=\$(python3 -B '$CLASSIFIER' --compare '$DBG_CMP' --build '$BUILD' \
             --changed-sources $CHANGED_SOURCES --changed-headers $CHANGED_HEADERS \
             --expect-differing 0 --min-negative-control 1 2>/dev/null)
      k=0
      for o in $NOISE_LIB; do
          printf '%s' \"\$out\" | grep -qxF \"      LIB   \$o\" && k=\$((k+1))
      done
      echo \"      NOISE-ONLY LIB lines matched: \$k/3\"
      [ \"\$k\" -eq 3 ]"

# P4.3 -- the MOVER set is UNCHANGED by the repair. This is the fold that makes
# it a labelling fix rather than a loosening: the declared count still comes out
# of the same harvest.
expect_zero "P4: the repair does not change the MOVER count on the same input" \
    bash -c "
      a=\$(python3 -B '$ART/wrappers/classify_psym_m3b.py' --compare '$DBG_CMP' --build '$BUILD' \
            --changed-sources $CHANGED_SOURCES \
            --changed-headers include/hven/detail/qp/qp_engine.h include/hven/detail/qp/qp_walk_trace.h \
            --expect-differing 0 --min-negative-control 1 2>/dev/null | grep '^DIFFERING OBJECTS:')
      b=\$(python3 -B '$CLASSIFIER' --compare '$DBG_CMP' --build '$BUILD' \
            --changed-sources $CHANGED_SOURCES --changed-headers $CHANGED_HEADERS \
            --expect-differing 0 --min-negative-control 1 2>/dev/null | grep '^DIFFERING OBJECTS:')
      echo \"      OLD '\$a' vs NEW '\$b'\"
      [ -n \"\$a\" ] && [ \"\$a\" = \"\$b\" ]"

# P4.4 -- R5. An input in which one object is reported BOTH as DIFFERS and as
# NOISE-ONLY makes the three-way labelling unsound, and is refused rather than
# resolved by precedence.
cp "$DBG_CMP" "$TMP/cmp-both.txt"
printf 'DIFFERS  CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o\n' >> "$TMP/cmp-both.txt"
# DECLARED 1, so R2 -- which fires on the count -- is satisfied and R5 is the
# rule actually under test. A fold that trips an earlier refusal proves nothing
# about the later one.
expect_zero "P4: R5 -- one object carrying BOTH a DIFFERS and a NOISE-ONLY verdict line is REFUSED, and the refusal names R5 (declared 1, so R2 is satisfied and R5 is what fires)" \
    bash -c "
      python3 -B '$CLASSIFIER' --compare '$TMP/cmp-both.txt' --build '$BUILD' \
        --changed-sources $CHANGED_SOURCES --changed-headers $CHANGED_HEADERS \
        --expect-differing 1 --min-negative-control 1 2>&1 | grep -q 'REFUSED (R5)'"

# P4.4b -- MOVER PRECEDENCE. `NOISE-ONLY` plus `RELOCATIONS` on one object is
# the shape M3a fix1's Release `trace_writer.cpp.o` had, and it is legal P-SYM
# output: the verdict is noise, the relocations moved anyway. It must be counted
# as a MOVER and must NOT also appear under the NOISE-ONLY label, or the three
# classes stop being disjoint and one object is reported twice.
cp "$DBG_CMP" "$TMP/cmp-noise-reloc.txt"
printf 'RELOCATIONS  CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o\n' >> "$TMP/cmp-noise-reloc.txt"
expect_zero "P4: an object that is NOISE-ONLY *and* RELOCATIONS counts as ONE mover and is NOT also labelled NOISE-ONLY" \
    bash -c "
      out=\$(python3 -B '$CLASSIFIER' --compare '$TMP/cmp-noise-reloc.txt' --build '$BUILD' \
              --changed-sources $CHANGED_SOURCES --changed-headers $CHANGED_HEADERS \
              --expect-differing 1 --min-negative-control 1 2>&1)
      mv=\$(printf '%s' \"\$out\" | sed -n 's/^DIFFERING OBJECTS: //p')
      nz=\$(printf '%s' \"\$out\" | sed -n 's/.*(\\([0-9]*\\) of them in the PRODUCTION LIBRARY.*/\\1/p')
      dup=\$(printf '%s' \"\$out\" | grep -cxF '      LIB   CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o')
      echo \"      movers=\$mv  noise-only LIB objects=\$nz (was 3)  the object under both labels=\$dup\"
      [ \"\$mv\" = 1 ] && [ \"\$nz\" = 2 ] && [ \"\$dup\" = 0 ]"

# P4.5 -- and the NOISE-ONLY harvest is not vacuous: an input with no
# NOISE-ONLY line at all reports zero of them, so a non-zero count above is a
# reading of the input and not a constant.
grep -v '^NOISE-ONLY' "$DBG_CMP" > "$TMP/cmp-nonoise.txt"
expect_zero "P4: an input with no NOISE-ONLY line reports 0 NOISE-ONLY objects" \
    bash -c "
      python3 -B '$CLASSIFIER' --compare '$TMP/cmp-nonoise.txt' --build '$BUILD' \
        --changed-sources $CHANGED_SOURCES --changed-headers $CHANGED_HEADERS \
        --expect-differing 0 --min-negative-control 1 2>/dev/null |
      grep -q 'NOISE-ONLY -- objects whose BYTES DIFFER .*: 0 '"

################################################################################
echo
echo "== PART 5 -- THE FAIL-CLOSED NON-DEBUG SECTION CHECK ===================="
################################################################################
# Every fold below is taken on a REAL object of this tree, mutated one section
# at a time. A check that only ever sees identical inputs proves nothing; these
# are the inputs it has to REFUSE.
CHK=$ART/wrappers/check_sections_fix1.sh
SECOBJ=""
for cand in "$ROOT/build-debug/CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o" \
            "$ROOT/build/CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o"; do
    [ -f "$cand" ] && { SECOBJ=$cand; break; }
done
if [ -z "$SECOBJ" ]; then
    echo "REFUSED: no real object to fold the section check against"
    exit 2
fi
echo "   folding against $SECOBJ"
SF=$TMP/secfold
mkdir -p "$SF/before" "$SF/after"
cp "$SECOBJ" "$SF/before/a.o"
echo "a.o" > "$SF/list.txt"
: > "$SF/empty.txt"

# A one-byte flip inside a NAMED section, leaving every other byte alone. Both
# the debug-only and the non-debug folds are built with it, so the only thing
# that differs between them is WHICH section moved.
cat > "$SF/flip.py" <<'FLIP'
import struct, sys
path, sec = sys.argv[1], sys.argv[2]
d = bytearray(open(path, 'rb').read())
sh_off = struct.unpack_from("<Q", d, 0x28)[0]
ent = struct.unpack_from("<H", d, 0x3A)[0]
num = struct.unpack_from("<H", d, 0x3C)[0]
strx = struct.unpack_from("<H", d, 0x3E)[0]
def hdr(i):
    return struct.unpack_from("<IIQQQQIIQQ", d, sh_off + i * ent)
if num == 0:
    num = hdr(0)[5]
if strx == 0xFFFF:
    strx = hdr(0)[6]
st = hdr(strx)
names = bytes(d[st[4]:st[4] + st[5]])
for i in range(num):
    h = hdr(i)
    e = names.find(b"\0", h[0])
    if names[h[0]:e].decode() == sec and h[1] != 8 and h[5] > 0:
        d[h[4]] ^= 0xFF
        open(path, 'wb').write(d)
        sys.exit(0)
sys.exit(1)
FLIP

sec_run() { bash "$CHK" Fold "$SF/before" "$SF/after" "$1" > "$TMP/sec-out.txt" 2>&1; rc=$?
            sed -n 's/^\(SECTIONS_Fold_[A-Z_]*=.*\)/      \1/p;s/^\(NON-DEBUG-MOVER.*\)/      \1/p;s/^\(REFUSED.*\)/      \1/p' "$TMP/sec-out.txt"
            return $rc; }
sec_mutate() { cp "$SECOBJ" "$SF/after/a.o"; [ -z "$1" ] || python3 -B "$SF/flip.py" "$SF/after/a.o" "$1"; }

sec_mutate ""
expect_zero "P5: two IDENTICAL objects pass, and are counted as BYTE-IDENTICAL" \
    sec_run "$SF/list.txt"

sec_mutate ".debug_line"
expect_zero "P5: a change confined to .debug_line passes, and is labelled DEBUG-ONLY" \
    sec_run "$SF/list.txt"

for sec in .text .rodata .comment; do
    sec_mutate "$sec"
    expect_nonzero "P5: a change to $sec FAILS -- this is the class of difference the old 'DWARF only' proof could not have caught" \
        sec_run "$SF/list.txt"
done

sec_mutate ""
rm -f "$SF/after/a.o"
expect_nonzero "P5: an object present on only one side is REFUSED, never counted as agreement" \
    sec_run "$SF/list.txt"

sec_mutate ""
expect_nonzero "P5: an EMPTY object list is REFUSED -- a check that inspected nothing is not a pass" \
    sec_run "$SF/empty.txt"

sec_mutate ""
head -c 4096 "$SECOBJ" > "$SF/after/a.o"
expect_nonzero "P5: a TRUNCATED object is REFUSED by the digest rather than silently skipped" \
    sec_run "$SF/list.txt"

sec_mutate ".debug_line"
expect_nonzero "P5: a missing section digest is REFUSED (the check cannot run open-loop)" \
    bash -c "DIGEST=/nonexistent/section_digest.py bash '$CHK' Fold '$SF/before' '$SF/after' '$SF/list.txt' > /dev/null 2>&1"

# THE DIGEST'S OWN NON-VACUITY: it must report DIFFERENT digests for the two
# sides of a mutation it is supposed to see, and the same for one it is not.
sec_mutate ".rodata"
expect_zero "P5: the digest itself distinguishes the mutated .rodata line and leaves every other line alone" \
    bash -c "
      python3 -B '$ART/wrappers/section_digest_fix1.py' '$SF/before/a.o' > '$TMP/d-b.txt' &&
      python3 -B '$ART/wrappers/section_digest_fix1.py' '$SF/after/a.o'  > '$TMP/d-a.txt' &&
      nd=\$(diff '$TMP/d-b.txt' '$TMP/d-a.txt' | grep -c '^[<>]')
      echo \"      digest lines that differ: \$nd (expected 2 -- one per side)\"
      [ \"\$nd\" -eq 2 ] && grep -q '^\\.rodata' <(diff '$TMP/d-b.txt' '$TMP/d-a.txt' | sed 's/^[<>] //')"

echo "PSYM_FOLD_PROOFS_FIX1_TOTAL=$n  PSYM_FOLD_PROOFS_FIX1_FAILED=$fails"
echo "PSYM_FOLD_PROOFS_FIX1_END $(date -Is)"
[ "$fails" -eq 0 ] || exit 1
exit 0
