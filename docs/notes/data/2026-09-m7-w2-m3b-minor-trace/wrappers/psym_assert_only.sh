#!/usr/bin/env bash
# THE P-SYM ASSERTION LAYER, ALONE -- no configure, no build, no capture, no
# `psym_compare.sh`, no `classify_psym*.py`. It reads two classification files
# that ALREADY EXIST and runs `gate_asserts_m3b.sh`'s P-SYM refusals over them.
#
# WHAT IT IS FOR, AND WHAT IT IS NOT. M7 W2 M3a fix round 1's G5 FAILED: the
# round declared one mover shape for both configurations and the run produced
# two different ones. The settler's ruling is that the FAILED gate stays FAILED
# and is NOT re-run to green behind a declaration written afterwards. This
# script exists so the CORRECTED RULE can be exercised over the SAME captures
# without re-running anything that could change them -- and every transcript it
# writes says, in its own first lines, that its expectation was written AFTER
# the capture was seen and is therefore a RECORD, not a pre-registered pass.
#
# M7 W2 M3b IS that future round and this is its copy: it declares its
# PER-CONFIGURATION shapes -- both of them ZERO -- in `gates/psym-derivation.txt`
# BEFORE its chain is detached, so it runs this script with `RECORD_BANNER=0`
# and uses it only as the fold proofs' way of driving the layer without a
# build. M3a's own copy is not edited. A round that declares before the run has
# its `gates*.sh` source
# `gate_asserts_m3b.sh` and calls `assert_psym` twice with the two shapes and
# `assert_psym_cross_config` once, and this script is then only a way to
# re-check a landed transcript without a build.
#
# USAGE (every setting is an environment variable, and every one of them is
# PRINTED into the transcript):
#
#   REL_CLS   DBG_CLS                         the two classification files
#   EXPECT_DIFFERING_RELEASE  EXPECT_CLASS_A_RELEASE  EXPECT_CLASS_B_RELEASE
#   EXPECT_DIFFERING_DEBUG    EXPECT_CLASS_A_DEBUG    EXPECT_CLASS_B_DEBUG
#   COMPARE_RC_RELEASE COMPARE_RC_DEBUG        as the run recorded them
#   CLASSIFY_RC_RELEASE CLASSIFY_RC_DEBUG      likewise
#   MIN_NEGATIVE_CONTROL                       default 1
#   CHANGED_SOURCES                            space-separated, normally
#                                              `git diff --name-only <base> -- src/ tests/`
#   RECORD_BANNER                              `1` (default) prints the
#                                              after-the-fact banner; a future
#                                              round that declared its shapes
#                                              BEFORE the run sets it to 0
#   BASE                                       the commit the captures compare
#                                              against, for the banner
set -o pipefail
ROOT=/home/ghecht/Projects/hven
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace

REL_CLS=${REL_CLS:?REL_CLS is required}
DBG_CLS=${DBG_CLS:?DBG_CLS is required}
EXPECT_DIFFERING_RELEASE=${EXPECT_DIFFERING_RELEASE:?required}
EXPECT_CLASS_A_RELEASE=${EXPECT_CLASS_A_RELEASE:?required}
EXPECT_CLASS_B_RELEASE=${EXPECT_CLASS_B_RELEASE:?required}
EXPECT_DIFFERING_DEBUG=${EXPECT_DIFFERING_DEBUG:?required}
EXPECT_CLASS_A_DEBUG=${EXPECT_CLASS_A_DEBUG:?required}
EXPECT_CLASS_B_DEBUG=${EXPECT_CLASS_B_DEBUG:?required}
COMPARE_RC_RELEASE=${COMPARE_RC_RELEASE:-1}
COMPARE_RC_DEBUG=${COMPARE_RC_DEBUG:-1}
CLASSIFY_RC_RELEASE=${CLASSIFY_RC_RELEASE:-0}
CLASSIFY_RC_DEBUG=${CLASSIFY_RC_DEBUG:-0}
MIN_NEGATIVE_CONTROL=${MIN_NEGATIVE_CONTROL:-1}
CHANGED_SOURCES=${CHANGED_SOURCES:?CHANGED_SOURCES is required}
RECORD_BANNER=${RECORD_BANNER:-1}
BASE=${BASE:-}

# shellcheck source=gate_asserts_m3b.sh
. "$ART/wrappers/gate_asserts_m3b.sh" || exit 90

if [ "$RECORD_BANNER" = "1" ]; then
    cat <<'BANNER'
============================================================================
THIS IS A RECORD, NOT A PRE-REGISTERED PASS.
The per-configuration expected mover shape asserted below was WRITTEN AFTER
THE CAPTURE WAS SEEN. It is not evidence that the round predicted its own
result; the round's ONE declared shape (DIFFERING 12 = class A 9 + class B 3,
the same in both configurations) was WRONG, its G5 FAILED, and that failure
stands as recorded in `gates-fix1/gates.txt` and in this directory's README.
What is shown here is the CORRECTED RULE -- per-configuration shapes, equal
class B sets, UNEXPLAINED 0 in both, and a class A symmetric difference
confined to sources this round changed -- run over the SAME classification
files the failed chain produced. Nothing was rebuilt, re-captured or
re-compared to produce it.
============================================================================
BANNER
fi

echo "PSYM_ASSERT_ONLY_START $(date -Is)"
echo "root=$ROOT"
echo "HEAD $(git -C "$ROOT" rev-parse --short HEAD)  dirty-files=$(git -C "$ROOT" status --porcelain | wc -l)"
[ -n "$BASE" ] && echo "captures compare this HEAD against BASE=$BASE"
echo
echo "-- INPUTS (read-only; this script writes nothing but its own stdout)"
for f in "$REL_CLS" "$DBG_CLS"; do
    printf '   %s\n     sha256 %s\n' "$f" "$(sha256sum "$f" 2>/dev/null | cut -d' ' -f1)"
done
printf '   gate_asserts_m3b.sh sha256 %s\n' \
    "$(sha256sum "$ART/wrappers/gate_asserts_m3b.sh" | cut -d' ' -f1)"
echo
echo "-- THE DECLARED PER-CONFIGURATION SHAPE"
echo "   Release  DIFFERING $EXPECT_DIFFERING_RELEASE = class A $EXPECT_CLASS_A_RELEASE + class B $EXPECT_CLASS_B_RELEASE"
echo "   Debug    DIFFERING $EXPECT_DIFFERING_DEBUG = class A $EXPECT_CLASS_A_DEBUG + class B $EXPECT_CLASS_B_DEBUG"
echo "   compare rc Release=$COMPARE_RC_RELEASE Debug=$COMPARE_RC_DEBUG; classify rc Release=$CLASSIFY_RC_RELEASE Debug=$CLASSIFY_RC_DEBUG"
echo "   min negative control = $MIN_NEGATIVE_CONTROL"
echo "   changed sources: $CHANGED_SOURCES"
echo

rc=0
echo "-- PER-CONFIGURATION"
assert_psym Release "$REL_CLS" "$COMPARE_RC_RELEASE" "$CLASSIFY_RC_RELEASE" \
    "$MIN_NEGATIVE_CONTROL" "$EXPECT_DIFFERING_RELEASE" "$EXPECT_CLASS_A_RELEASE" \
    "$EXPECT_CLASS_B_RELEASE" || rc=1
assert_psym Debug "$DBG_CLS" "$COMPARE_RC_DEBUG" "$CLASSIFY_RC_DEBUG" \
    "$MIN_NEGATIVE_CONTROL" "$EXPECT_DIFFERING_DEBUG" "$EXPECT_CLASS_A_DEBUG" \
    "$EXPECT_CLASS_B_DEBUG" || rc=1
echo
echo "-- CROSS-CONFIGURATION"
# shellcheck disable=SC2086
assert_psym_cross_config "$REL_CLS" "$DBG_CLS" -- $CHANGED_SOURCES || rc=1

echo
echo "PSYM_ASSERT_ONLY_RC=$rc"
echo "PSYM_ASSERT_ONLY_END $(date -Is)"
exit $rc
