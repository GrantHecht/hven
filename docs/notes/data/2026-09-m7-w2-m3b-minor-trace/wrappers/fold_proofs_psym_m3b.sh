#!/usr/bin/env bash
# THE FOLD PROOFS FOR M7 W2 M3b's P-SYM ASSERTION LAYER
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
BUILD=$ROOT/.scratch/m7w2m3b/rel
TMP=$(mktemp -d "${TMPDIR:-/tmp}/m3b-psymfold-XXXXXX") || exit 90
trap 'rm -rf "$TMP"' EXIT

# shellcheck source=gate_asserts_m3b.sh
. "$ART/wrappers/gate_asserts_m3b.sh" || exit 90

RUNNER=$ART/wrappers/psym_assert_only.sh
CLASSIFIER=$ART/wrappers/classify_psym_m3b.py
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

echo "PSYM_FOLD_PROOFS_M3B_START $(date -Is)"
echo "root=$ROOT"
echo "HEAD $(git -C "$ROOT" rev-parse HEAD)  dirty-files=$(git -C "$ROOT" status --porcelain | wc -l)"
echo "-- wrapper identity: WORKTREE digest vs the digest of the SAME path AT HEAD."
echo "-- They must agree, or this transcript did not exercise what landed."
for f in gate_asserts_m3b.sh psym_assert_only.sh fold_proofs_psym_m3b.sh classify_psym_m3b.py; do
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
CHANGED_SOURCES=$(git -C "$ROOT" diff --name-only 9fee5545 -- src/ tests/ | tr '\n' ' ')
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
CMP_SRC=$DERIVE/compare-Release.txt
if [ ! -s "$CMP_SRC" ]; then
    echo "REFUSED: no comparison at $CMP_SRC"
    exit 2
fi
printf '   %s\n     sha256 %s\n' "$CMP_SRC" "$(sha256sum "$CMP_SRC" | cut -d' ' -f1)"
CHANGED_HEADERS=$(git -C "$ROOT" diff --name-only 9fee5545 -- include/ | tr '\n' ' ')
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
{
    head -4 "$CMP_SRC"
    echo "NOISE-ONLY  CMakeFiles/hven.dir/src/drivers/trace_writer.cpp.o"
    echo "RELOCATIONS CMakeFiles/hven.dir/src/drivers/trace_writer.cpp.o (the instruction stream is within the accepted noise class,"
    echo "            but one relocation target is renamed)"
    tail -n +5 "$CMP_SRC"
} > "$TMP/cmp-reloc.txt"
expect_nonzero "P3: THE REGISTERED DEFECT -- a NOISE-ONLY object with a RELOCATIONS line is now HARVESTED, so a declared 0 fails" \
    classify "$TMP/cmp-reloc.txt" 0

expect_zero "P3: and declaring that one mover accepts it -- the harvest counts it as exactly one object, not two" \
    classify "$TMP/cmp-reloc.txt" 1

# The contrast that shows the fix is load-bearing: M3a's own classifier, run on
# the same doctored comparison, harvests NOTHING and reports zero movers -- and
# its R2 then refuses for the OPPOSITE reason (it demands movers), which is not
# the same thing as catching the object. The proof is in its output: the object
# does not appear in its differing set at all.
expect_nonzero "P3: CONTRAST -- M3a's classifier on the same file harvests the RELOCATIONS object as nothing" \
    bash -c "python3 -B '$M3A/wrappers/classify_psym_fix1.py' --compare '$TMP/cmp-reloc.txt' \
             --build '$BUILD' --changed-sources $CHANGED_SOURCES \
             --changed-headers include/hven/core/solver_counters.h include/hven/core/solver_status.h \
             --min-negative-control 1 | grep -q 'trace_writer.cpp.o' && exit 1 || exit 0"

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
echo "PSYM_FOLD_PROOFS_M3B_TOTAL=$n  PSYM_FOLD_PROOFS_M3B_FAILED=$fails"
echo "PSYM_FOLD_PROOFS_M3B_END $(date -Is)"
[ "$fails" -eq 0 ] || exit 1
exit 0
