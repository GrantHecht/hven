#!/usr/bin/env bash
# THE FOLD PROOFS FOR THE CORRECTED P-SYM ASSERTION LAYER
# (`gate_asserts_v2.sh` + `psym_assert_only.sh`), M7 W2 M3a fix round 1's
# settlement round.
#
# `fold_proofs_fix1.sh` beside this file proved the refusals that were in force
# when the chain ran, and is NOT edited: it produced the committed
# `gates-fix1/fold-proofs.txt`. This file proves the refusals that REPLACE the
# one the run showed to be wrong, and the ACCEPTANCE that replaces it.
#
# EVERY FOLD RUNS ON A COPY OF THE REAL CAPTURE. The two classification files
# the failed chain produced are copied into a temporary directory and broken
# there, one property at a time, so that each refusal is shown to fire on
# INPUT THAT IS OTHERWISE EXACTLY WHAT THE RUN PRODUCED -- a refusal proven on
# a hand-written toy proves only that the parser can read a toy. The originals
# are read-only inputs and are never written.
#
# It starts no solve, builds nothing, captures nothing and runs neither
# `psym_compare.sh` nor `classify_psym*.py`. Its only inputs are two text files
# and `git`.
#
# Usage: bash fold_proofs_psym_v2.sh [RELEASE_CLASSIFICATION] [DEBUG_CLASSIFICATION]
set -o pipefail
ROOT=/home/ghecht/Projects/hven
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3a-request-probe
G=$ROOT/.scratch/m7w2m3a/gates-fix1/psym
REL_SRC=${1:-$G/classification-Release.txt}
DBG_SRC=${2:-$G/classification-Debug.txt}
TMP=$(mktemp -d "${TMPDIR:-/tmp}/m3a-psymfold-XXXXXX") || exit 90
trap 'rm -rf "$TMP"' EXIT

# shellcheck source=gate_asserts_v2.sh
. "$ART/wrappers/gate_asserts_v2.sh" || exit 90

RUNNER=$ART/wrappers/psym_assert_only.sh
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

echo "PSYM_FOLD_PROOFS_V2_START $(date -Is)"
echo "root=$ROOT"
echo "HEAD $(git -C "$ROOT" rev-parse HEAD)  dirty-files=$(git -C "$ROOT" status --porcelain | wc -l)"
echo "-- wrapper identity: WORKTREE digest vs the digest of the SAME path AT HEAD."
echo "-- They must agree, or this transcript did not exercise what landed."
for f in gate_asserts_v2.sh psym_assert_only.sh fold_proofs_psym_v2.sh; do
    rel="docs/notes/data/2026-09-m7-w2-m3a-request-probe/wrappers/$f"
    w=$(sha256sum "$ART/wrappers/$f" | cut -d' ' -f1)
    c=$(git -C "$ROOT" show "HEAD:$rel" 2> /dev/null | sha256sum | cut -d' ' -f1)
    if [ "$w" = "$c" ]; then
        printf '%-24s sha256 %s  (worktree == HEAD)\n' "$f" "$w"
    else
        printf '%-24s sha256 %s  *** DIFFERS FROM HEAD: %s ***\n' "$f" "$w" "$c"
        fails=$((fails + 1))
    fi
done
echo
echo "-- the REAL captures these folds copy and break:"
printf '   %s\n     sha256 %s\n' "$REL_SRC" "$(sha256sum "$REL_SRC" | cut -d' ' -f1)"
printf '   %s\n     sha256 %s\n' "$DBG_SRC" "$(sha256sum "$DBG_SRC" | cut -d' ' -f1)"
echo

CHANGED_SOURCES=$(git -C "$ROOT" diff --name-only 11b066e8 -- src/ tests/ | tr '\n' ' ')
echo "-- the round's changed sources, from git:"
echo "   $CHANGED_SOURCES"
echo

REL=$TMP/rel.txt
DBG=$TMP/dbg.txt
cp "$REL_SRC" "$REL"
cp "$DBG_SRC" "$DBG"

# run_layer REL DBG  -- the whole assertion layer, at THIS ROUND'S CORRECTED
# per-configuration shape, with everything else held at what the run recorded.
run_layer() {
    REL_CLS="$1" DBG_CLS="$2" \
        EXPECT_DIFFERING_RELEASE="${3:-10}" EXPECT_CLASS_A_RELEASE="${4:-7}" \
        EXPECT_CLASS_B_RELEASE="${5:-3}" \
        EXPECT_DIFFERING_DEBUG="${6:-11}" EXPECT_CLASS_A_DEBUG="${7:-8}" \
        EXPECT_CLASS_B_DEBUG="${8:-3}" \
        COMPARE_RC_RELEASE=1 COMPARE_RC_DEBUG=1 \
        CLASSIFY_RC_RELEASE=0 CLASSIFY_RC_DEBUG=0 \
        MIN_NEGATIVE_CONTROL=1 CHANGED_SOURCES="$CHANGED_SOURCES" \
        RECORD_BANNER=0 BASE=11b066e8 \
        bash "$RUNNER"
}

# ---- THE ACCEPTANCE ---------------------------------------------------------
expect_zero "the REAL capture pair passes the corrected layer (per-config shapes 10/7/3 and 11/8/3)" \
    run_layer "$REL" "$DBG"

# shellcheck disable=SC2086
expect_zero "the REAL capture pair passes assert_psym_cross_config on its own" \
    assert_psym_cross_config "$REL" "$DBG" -- $CHANGED_SOURCES

# ---- WHAT THE ROUND ACTUALLY DECLARED, AND WHY G5 FAILED --------------------
expect_nonzero "the round's ONE declared shape (12/9/3 for BOTH configs) is refused -- this is G5's own failure, reproduced from the transcripts alone" \
    run_layer "$REL" "$DBG" 12 9 3 12 9 3

expect_nonzero "PER-CONFIGURATION is real: Release asserted with Debug's shape (11/8/3)" \
    run_layer "$REL" "$DBG" 11 8 3 11 8 3

expect_nonzero "PER-CONFIGURATION is real, the other way: Debug asserted with Release's shape (10/7/3)" \
    run_layer "$REL" "$DBG" 10 7 3 10 7 3

# ---- THE CROSS-CONFIGURATION CLAUSES ----------------------------------------
# (1) the class B sets must be EQUAL. One class B object is removed from the
#     DEBUG copy; every count in both files is left alone, so nothing but the
#     set comparison can fire.
grep -v '^      tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_cap_levers.cpp.o$' \
    "$DBG" > "$TMP/dbg-classB-short.txt"
# shellcheck disable=SC2086
expect_nonzero "cross-config clause 1: the two class B sets are NOT equal" \
    assert_psym_cross_config "$REL" "$TMP/dbg-classB-short.txt" -- $CHANGED_SOURCES

# (2) UNEXPLAINED must be 0 in BOTH, as a count and as a list.
sed 's|^  UNEXPLAINED: 0$|  UNEXPLAINED: 1\n      CMakeFiles/hven.dir/src/nowhere/not_a_real_object.cpp.o|' \
    "$REL" > "$TMP/rel-unexplained.txt"
sed 's|^  UNEXPLAINED: 0$|  UNEXPLAINED: 1\n      CMakeFiles/hven.dir/src/nowhere/not_a_real_object.cpp.o|' \
    "$DBG" > "$TMP/dbg-unexplained.txt"
expect_nonzero "cross-config clause 2: one UNEXPLAINED object in the RELEASE classification" \
    run_layer "$TMP/rel-unexplained.txt" "$DBG"
expect_nonzero "cross-config clause 2: one UNEXPLAINED object in the DEBUG classification" \
    run_layer "$REL" "$TMP/dbg-unexplained.txt"
# shellcheck disable=SC2086
expect_nonzero "cross-config clause 2, isolated: the cross-config rule alone refuses an unexplained object" \
    assert_psym_cross_config "$TMP/rel-unexplained.txt" "$DBG" -- $CHANGED_SOURCES

# (3)+(4) a class A difference on a source this round did NOT change. The object
#     inserted is a real object of this build that includes a changed header and
#     was byte-identical -- i.e. exactly the thing the loosened rule must NOT be
#     read as admitting.
#     Its COUNTS are kept internally consistent (DIFFERING 11 -> 12, class A
#     8 -> 9) and the fold below declares that very shape, so `assert_psym`
#     passes on it and the ONLY thing left to refuse it is the cross-config
#     rule -- which is the point.
sed -e 's|^  class A -- \(.*\): 8$|  class A -- \1: 9\n      CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o|' \
    -e 's|^DIFFERING OBJECTS: 11$|DIFFERING OBJECTS: 12|' \
    "$DBG" > "$TMP/dbg-classA-stray.txt"
# shellcheck disable=SC2086
expect_nonzero "cross-config clauses 3+4: class A differs on an object whose SOURCE this round did not change" \
    assert_psym_cross_config "$REL" "$TMP/dbg-classA-stray.txt" -- $CHANGED_SOURCES
expect_nonzero "the same stray class A object, through the whole layer, with the per-configuration shape DECLARED TO MATCH IT (12/9/3) so only the cross-config rule can refuse it" \
    run_layer "$REL" "$TMP/dbg-classA-stray.txt" 10 7 3 12 9 3

# ---- THE LAYER'S OWN INPUT REFUSALS -----------------------------------------
expect_nonzero "an EMPTY changed-source list -- the symmetric-difference clause would admit anything" \
    assert_psym_cross_config "$REL" "$DBG" --

expect_nonzero "a classification file that does not exist" \
    run_layer "$TMP/no-such-file.txt" "$DBG"

: > "$TMP/empty.txt"
expect_nonzero "an EMPTY classification file" \
    run_layer "$REL" "$TMP/empty.txt"

grep -v '^  UNEXPLAINED:' "$REL" > "$TMP/rel-no-unexpl-count.txt"
expect_nonzero "a classification missing one of its five counts (UNEXPLAINED stripped)" \
    run_layer "$TMP/rel-no-unexpl-count.txt" "$DBG"

sed 's|^\(  NEGATIVE CONTROL[^:]*\): 107$|\1: 0|' "$REL" > "$TMP/rel-no-control.txt"
expect_nonzero "a negative control of 0 -- the differing set IS everything that sees the header" \
    run_layer "$TMP/rel-no-control.txt" "$DBG"

# ---- THE RULE THAT WAS REPLACED ---------------------------------------------
# Version 1's `assert_same_classification` required the two classifications to
# be byte-identical. Run here on the UNMODIFIED REAL PAIR, it FAILS -- which is
# the whole reason this file exists. It is loaded in a subshell so that version
# 1's definitions cannot leak into the folds above.
expect_nonzero "CONTRAST: version 1's assert_same_classification refuses the REAL, CORRECT capture pair" \
    bash -c ". '$ART/wrappers/gate_asserts.sh'; assert_same_classification '$REL' '$DBG'"

echo
echo "PSYM_FOLD_PROOFS_V2_TOTAL=$n  PSYM_FOLD_PROOFS_V2_FAILED=$fails"
echo "PSYM_FOLD_PROOFS_V2_END $(date -Is)"
[ "$fails" -eq 0 ] || exit 1
exit 0
