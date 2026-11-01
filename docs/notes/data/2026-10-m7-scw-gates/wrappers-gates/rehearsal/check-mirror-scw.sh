#!/usr/bin/env bash
# M7 SCW gates run: the offline proof of the ONE structural addition to the runner, the class B MIRROR admission (EXPECT_PSYM_B_RELEASE_ONLY,
# a Release-only class B whose Debug object is a non-mover). The rehearsal's g5 segment predates the addition (the rehearsal measured the
# shape that made it necessary), so the added block is proven HERE, on the rehearsal's own classifications, before any leg:
#   M0 the CARRIED rule alone (class B sets EQUAL) REFUSES the measured pair -- the addition is necessary;
#   M1 the block, with the declared list, PASSES, and the shared cross-config assert then PASSES over the Release copy;
#   M2 falsifier: a declared list missing one of the two objects is REFUSED;
#   M3 falsifier: a declared object that is not a Release-only class B is REFUSED;
#   M4 falsifier: the mirror block extracted from the runner is non-empty (the markers exist exactly once).
# Everything is DERIVED: the block is cut out of the runner's g5 text, the declared list is read from the runner, the changed sources from git.
set -o pipefail
W=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd) || exit 3
ROOT=$(git -C "$W" rev-parse --show-toplevel) || exit 3
R=$W/gates-scw.sh
REH=$W/rehearsal
cd "$ROOT" || exit 3
BEFORE=$(sed -n 's/^BEFORE=\${BEFORE:-\([0-9a-f]*\)}$/\1/p' "$R")
CHANGED_SOURCES=$(git diff --name-only "$BEFORE..HEAD" -- src/ tests/ bench/ | tr '\n' ' ')
DECL=$(sed -n 's/^EXPECT_PSYM_B_RELEASE_ONLY=\${EXPECT_PSYM_B_RELEASE_ONLY-"\(.*\)"}$/\1/p' "$R")
echo "declared Release-only class B: [$DECL]"
BLOCK=$(awk '/^cat > "\$S\/g5.sh" <<.INNER.$/ {f=1; next} f && /^INNER$/ {exit} f {print}' "$R" | awk '/^# BEGIN_MIRROR_BLOCK$/ {b++; f=1; next} /^# END_MIRROR_BLOCK$/ {f=0} f {print}')
nb=$(awk '/^# BEGIN_MIRROR_BLOCK$/ {n++} END {print n+0}' "$R")
echo "M4: BEGIN_MIRROR_BLOCK markers in the runner: $nb (want 1); block lines: $(printf '%s\n' "$BLOCK" | grep -c .)"
[ "$nb" = 1 ] && [ -n "$BLOCK" ] || { echo "M4 FAIL"; exit 1; }
TMP=$(mktemp -d)
. "$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace/wrappers/gate_asserts_m3b.sh" || exit 3
cp "$REH/rehearsal-scw-classification-Release.txt" "$TMP/classification-Release.txt"
cp "$REH/rehearsal-scw-classification-Debug.txt" "$TMP/classification-Debug.txt"
echo "=== M0: the carried rule alone (no mirror admission)"
# shellcheck disable=SC2086
assert_psym_cross_config "$TMP/classification-Release.txt" "$TMP/classification-Debug.txt" -- $CHANGED_SOURCES > "$TMP/m0.txt" 2>&1; m0=$?
grep -E "^ASSERT (OK|FAIL) P-SYM cross-config: class B" "$TMP/m0.txt"; echo "M0 rc=$m0 (want 1: refused)"
run_block() { # declared-list
  ( OUT=$TMP; XDBG=$TMP/classification-Debug.txt; XREL=$OUT/classification-Release.txt; WANT_B_RELEASE_ONLY="$1"; rc=0
    eval "$BLOCK"
    echo "BLOCK_RC=$rc"
    # shellcheck disable=SC2086
    assert_psym_cross_config "$XREL" "$XDBG" -- $CHANGED_SOURCES || rc=1
    echo "FINAL_RC=$rc" )
}
echo "=== M1: the declared list"
run_block "$DECL" > "$TMP/m1.txt" 2>&1; grep -E "^(ASSERT|BLOCK_RC|FINAL_RC)" "$TMP/m1.txt"
m1=$(sed -n 's/^FINAL_RC=//p' "$TMP/m1.txt")
echo "M1 FINAL_RC=$m1 (want 0)"
echo "=== M2: falsifier, one of the two objects missing from the declared list"
one=${DECL%% *}
run_block "$one" > "$TMP/m2.txt" 2>&1; grep -E "^(ASSERT FAIL|FINAL_RC)" "$TMP/m2.txt"
m2=$(sed -n 's/^FINAL_RC=//p' "$TMP/m2.txt"); echo "M2 FINAL_RC=$m2 (want 1)"
echo "=== M3: falsifier, a declared object that is not a Release-only class B (a class A library object)"
run_block "$DECL CMakeFiles/hven.dir/src/drivers/sqp_solver.cpp.o" > "$TMP/m3.txt" 2>&1; grep -E "^(ASSERT FAIL|FINAL_RC)" "$TMP/m3.txt"
m3=$(sed -n 's/^FINAL_RC=//p' "$TMP/m3.txt"); echo "M3 FINAL_RC=$m3 (want 1)"
rm -rf "$TMP"
if [ "$m0" = 1 ] && [ "$m1" = 0 ] && [ "$m2" = 1 ] && [ "$m3" = 1 ]; then echo "MIRROR_CHECK_OK"; exit 0; fi
echo "MIRROR_CHECK_FAIL"; exit 1
