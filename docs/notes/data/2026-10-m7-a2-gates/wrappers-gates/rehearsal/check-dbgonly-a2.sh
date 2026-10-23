#!/usr/bin/env bash
# M7 A2 gates run (NEW script, written for this declaration): the offline proof of the class B DEBUG-ONLY admission (EXPECT_PSYM_B_DEBUG_ONLY, carried
# runner code from E2/E4) on THIS range's own rehearsal classifications, before any leg. This range measured a Debug-only class B object
# (tests/drivers/.../test_solve_status.cpp.o: a Debug class B mover, BYTE-IDENTICAL in Release), so a g5 failure here would cost a 35-minute leg; the
# block is proven offline instead, exactly as the SCW round proved its mirror block:
#   D0 the CARRIED cross-config rule alone (class B sets EQUAL) REFUSES the measured pair -- the admission is necessary;
#   D1 the block, with the declared list, PASSES, and the shared cross-config assert then PASSES over the Debug copy;
#   D2 falsifier: a declared list that also names a class A library object is REFUSED;
#   D3 falsifier: a declared object that is a class B mover in BOTH configurations is REFUSED;
#   D4 the block extracted from the runner is non-empty; the EXPECT_PSYM_B_RELEASE_ONLY (mirror) slot is EMPTY, so the mirror block is not exercised
#      here, and it is byte-identical to the SCW runner's block (which SCW proved offline and ran green).
# Everything is DERIVED: the block is cut out of the runner's g5 text, the declared list is read from the runner, the changed sources from git.
set -o pipefail
W=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd) || exit 3
ROOT=$(git -C "$W" rev-parse --show-toplevel) || exit 3
R=$W/gates-a2.sh
REH=$W/rehearsal
cd "$ROOT" || exit 3
BEFORE=$(sed -n 's/^BEFORE=\${BEFORE:-\([0-9a-f]*\)}$/\1/p' "$R")
CHANGED_SOURCES=$(git diff --name-only "$BEFORE..HEAD" -- src/ tests/ bench/ | tr '\n' ' ')
DECL=$(sed -n 's/^EXPECT_PSYM_B_DEBUG_ONLY=\${EXPECT_PSYM_B_DEBUG_ONLY-"\(.*\)"}$/\1/p' "$R")
RELONLY=$(sed -n 's/^EXPECT_PSYM_B_RELEASE_ONLY=\${EXPECT_PSYM_B_RELEASE_ONLY-"\(.*\)"}$/\1/p' "$R")
echo "declared Debug-only class B: [$DECL]; declared Release-only (mirror) class B: [$RELONLY] (want empty)"
G5=$(awk '/^cat > "\$S\/g5.sh" <<.INNER.$/ {f=1; next} f && /^INNER$/ {exit} f {print}' "$R")
BLOCK=$(printf '%s\n' "$G5" | awk '/^XDBG="\$OUT\/classification-Debug.txt"$/ {f=1} /^# SCW: THE MIRROR ADMISSION/ {f=0} f {print}')
MBLOCK=$(printf '%s\n' "$G5" | awk '/^# BEGIN_MIRROR_BLOCK$/ {f=1; next} /^# END_MIRROR_BLOCK$/ {f=0} f {print}')
SCWBLOCK=$(git show 6a2a58c7:docs/notes/data/2026-10-m7-scw-gates/wrappers-gates/gates-scw.sh | awk '/^cat > "\$S\/g5.sh" <<.INNER.$/ {f=1; next} f && /^INNER$/ {exit} f {print}' | awk '/^# BEGIN_MIRROR_BLOCK$/ {f=1; next} /^# END_MIRROR_BLOCK$/ {f=0} f {print}')
echo "D4: Debug-only block lines: $(printf '%s\n' "$BLOCK" | grep -c .); mirror block lines: $(printf '%s\n' "$MBLOCK" | grep -c .)"
[ -n "$DECL" ] && [ -z "$RELONLY" ] && [ -n "$BLOCK" ] && [ -n "$MBLOCK" ] || { echo "D4 FAIL"; exit 1; }
if [ "$MBLOCK" = "$SCWBLOCK" ]; then echo "D4: the mirror block is byte-identical to SCW's (6a2a58c7)"; else echo "D4 FAIL: the mirror block differs from SCW's"; exit 1; fi
TMP=$(mktemp -d)
. "$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace/wrappers/gate_asserts_m3b.sh" || exit 3
cp "$REH/rehearsal-a2-classification-Release.txt" "$TMP/classification-Release.txt"
cp "$REH/rehearsal-a2-classification-Debug.txt" "$TMP/classification-Debug.txt"
echo "=== D0: the carried rule alone (no admission)"
# shellcheck disable=SC2086
assert_psym_cross_config "$TMP/classification-Release.txt" "$TMP/classification-Debug.txt" -- $CHANGED_SOURCES > "$TMP/d0.txt" 2>&1; d0=$?
grep -E "^ASSERT (OK|FAIL) P-SYM cross-config: class B" "$TMP/d0.txt"; echo "D0 rc=$d0 (want 1: refused)"
run_block() { # declared-list
  ( OUT=$TMP; WANT_B_DEBUG_ONLY="$1"; rc=0
    eval "$BLOCK"
    echo "BLOCK_RC=$rc"
    # shellcheck disable=SC2086
    assert_psym_cross_config "$OUT/classification-Release.txt" "$XDBG" -- $CHANGED_SOURCES || rc=1
    echo "FINAL_RC=$rc" )
}
echo "=== D1: the declared list"
run_block "$DECL" > "$TMP/d1.txt" 2>&1; grep -E "^(ASSERT|BLOCK_RC|FINAL_RC)" "$TMP/d1.txt"
d1=$(sed -n 's/^FINAL_RC=//p' "$TMP/d1.txt"); echo "D1 FINAL_RC=$d1 (want 0)"
echo "=== D2: falsifier, a class A library object also declared"
run_block "$DECL CMakeFiles/hven.dir/src/drivers/sqp_solver.cpp.o" > "$TMP/d2.txt" 2>&1; grep -E "^(ASSERT FAIL|FINAL_RC)" "$TMP/d2.txt"
d2=$(sed -n 's/^FINAL_RC=//p' "$TMP/d2.txt"); echo "D2 FINAL_RC=$d2 (want 1)"
echo "=== D3: falsifier, an object that is a class B mover in BOTH configurations declared"
run_block "tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_concurrency.cpp.o" > "$TMP/d3.txt" 2>&1; grep -E "^(ASSERT FAIL|FINAL_RC)" "$TMP/d3.txt"
d3=$(sed -n 's/^FINAL_RC=//p' "$TMP/d3.txt"); echo "D3 FINAL_RC=$d3 (want 1)"
rm -rf "$TMP"
if [ "$d0" = 1 ] && [ "$d1" = 0 ] && [ "$d2" = 1 ] && [ "$d3" = 1 ]; then echo "DBGONLY_CHECK_OK"; exit 0; fi
echo "DBGONLY_CHECK_FAIL"; exit 1
