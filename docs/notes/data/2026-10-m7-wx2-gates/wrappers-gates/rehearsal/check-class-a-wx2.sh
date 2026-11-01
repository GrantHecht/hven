#!/usr/bin/env bash
# M7 WX2 gates run (COPIED from check-class-a-wx.sh, NOT edited in place, and re-derived: this range's shape is different).
# The offline proof of the runner's POST-G5 assertions on THIS range's own rehearsal, before any leg. The block cut out of the runner runs from `BEGIN_POSTG5_BLOCK` to the G6 header
# and holds: the reference-dispatch and no-probe-object checks, the UNITY class A check (W2-S6), the OBJECT CENSUS (W2-S7), the class B by-name check, the class A by-source check, the
# NOISE-ONLY / IDENTICAL checks and the library NOISE-ONLY mover check. This range measured: Release 9 objects differ (2 real class A movers, 4 modified sources NOISE-ONLY, 3 unmodified
# bench objects NOISE-ONLY); Debug 20 (the same 2 movers; 6 library objects NOISE-ONLY, 2 of them modified sources; 4 interior unity objects; ...). Proofs:
#   H1 the declared slots, with the rehearsal's own compare / manifests / saved objects / census inputs, PASS (rc5 0) in both configurations;
#   H2 falsifier: get_core_count.cpp.o (NOISE-ONLY in Debug, not byte-identical) declared IDENTICAL in DEBUG is REFUSED;
#   H3 falsifier: test_sqp_solver.cpp.o (a real mover) declared NOISE-ONLY in RELEASE is REFUSED;
#   H4 falsifier: the Release IDENTICAL slot emptied (the two library sources become unaccounted class A sources) is REFUSED;
#   H5 falsifier: a modified source that is in no unity file (tests/sqp/test_sqp_solver.cpp) declared a UNITY source is REFUSED;
#   H6 falsifier: the UNITY slot emptied (six modified sources unaccounted for) is REFUSED;
#   C1 census falsifier: one object's after-arm compile command gains a token outside the declared set -> REFUSED;
#   C2 census falsifier: an unmodified, header-free, delta-free object's sha is flipped (a differing object that carries no class) -> REFUSED;
#   C3 census falsifier: a wrong C_pure_differing count is declared -> REFUSED;
#   C4 census falsifier: a Debug pure class-C object's command loses the define while its bytes still differ (nothing else explains it) -> REFUSED;
#   C5 census control: the census tool on the unmodified data, with the measured counts, passes.
# Everything is DERIVED: the runner's declared section and the block are cut out of the runner's own text, the modified sources come from git.
set -o pipefail
W=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd) || exit 3
ROOT=$(git -C "$W" rev-parse --show-toplevel) || exit 3
R=$W/gates-wx2.sh
cd "$ROOT" || exit 3
S=$(bash "$R" --print-scratch)/rehearsal
SCR=$S/gates/psym
for f in compare-Release.txt compare-Debug.txt before-Release/.psym-manifest after-Release/.psym-manifest before-Debug/.psym-manifest after-Debug/.psym-manifest \
         commands-before-Release.txt commands-after-Release.txt sha-before-Release.txt sha-after-Release.txt deps-after-Release.txt \
         commands-before-Debug.txt commands-after-Debug.txt sha-before-Debug.txt sha-after-Debug.txt deps-after-Debug.txt configure-Release.log configure-Debug.log \
         build-after-Release.log build-after-Debug.log; do
  [ -s "$SCR/$f" ] || { echo "REFUSED: rehearsal scratch lacks $SCR/$f"; exit 3; }
done
export GATES_SCRATCH=$S
. "$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace/wrappers/gate_asserts_m3b.sh" || exit 3
BEFORE_S=$(sed -n 's/^BEFORE=\${BEFORE:-\([0-9a-f]*\)}$/\1/p' "$R")
DECL=$(sed -n '/^BEFORE=\${BEFORE:-/,/^LOG=\$S/p' "$R" | sed '$d')
[ -n "$DECL" ] || { echo "D FAIL: no declared section"; exit 1; }
HEAD_AT_START=$(git rev-parse HEAD)
# the commit stamps in the REHEARSAL's after-arm compile commands are the commit the rehearsal ran at (its log line `P-SYM <cfg> after=<sha>`), not this checkout's HEAD
AFTER_STAMP_SHA=$(grep -m1 -o ' after=[0-9a-f]\{40\}' "$W/rehearsal/rehearsal-wx2-log.txt" | tr -d ' ' | cut -d= -f2)
[ -n "$AFTER_STAMP_SHA" ] || AFTER_STAMP_SHA=$(grep -m1 -o ' after=[0-9a-f]\{40\}' "$S/../rehearse2-launch.out" | tr -d ' ' | cut -d= -f2)
[ -n "$AFTER_STAMP_SHA" ] || { echo "REFUSED: no rehearsal HEAD in the rehearsal log"; exit 3; }
BEFORE=$BEFORE_S
CHANGED_SOURCES=$(git diff --name-only "$BEFORE_S..$HEAD_AT_START" -- src/ tests/ bench/ | tr '\n' ' ')
CHANGED_HEADERS=$(git diff --name-only "$BEFORE_S..$HEAD_AT_START" -- include/ | tr '\n' ' ')
MODIFIED_CXX=$(git diff --name-only --diff-filter=M "$BEFORE_S..$HEAD_AT_START" -- 'src/*.cpp' 'tests/*.cpp' 'bench/*.cpp' | tr '\n' ' ')
OBJFN=$(awk '/^obj_source\(\) \{/ {f=1} f {print} f && /^\}/ {exit}' "$R")
BLOCK=$(awk '/BEGIN_POSTG5_BLOCK/ {f=1} /^# --- G6/ {f=0} f {print}' "$R")
HELPERS=$(awk '/^assert_reference_dispatch\(\) \{/ {f=1} f {print} /^assert_no_probe_object\(\) \{/ {g=1} g && /^\}/ {exit}' "$R")
[ -n "$OBJFN" ] && [ -n "$BLOCK" ] && [ -n "$HELPERS" ] || { echo "D4 FAIL: could not cut the block / obj_source / helpers out of the runner"; exit 1; }
echo "block lines: $(printf '%s\n' "$BLOCK" | grep -c .)"
TG=$(mktemp -d); mkdir -p "$TG/psym"
for x in $(ls "$SCR"); do ln -s "$SCR/$x" "$TG/psym/$x"; done
for c in Release Debug; do cp "$S/offline-classification-$c.txt" "$TG/psym/classification-$c.txt"; done
run_block() { # label, then VAR=value overrides applied after the declarations
  ( set +e; eval "$DECL" >/dev/null 2>&1; eval "$OBJFN"; eval "$HELPERS"; G=$TG; rc5=0
    for kv in "$@"; do eval "$kv"; done
    eval "$BLOCK"
    echo "RC5=$rc5" )
}
show() { grep -E "^(ASSERT FAIL|RC5)|CENSUS_[A-Za-z]+ |CENSUS_REFUSED|CENSUS_.*_OK|IDENTICAL|NOISE-ONLY class A|class A by source|UNITY|reference dispatch|probe" "$1" | cut -c1-250; }
echo "=== H1: the declared slots"
run_block > "$TG/h1.txt" 2>&1; show "$TG/h1.txt"; h1=$(sed -n 's/^RC5=//p' "$TG/h1.txt")
echo "=== H2: falsifier, get_core_count.cpp.o declared IDENTICAL in Debug (it is NOISE-ONLY there)"
run_block 'EXPECT_CLASS_A_IDENTICAL_DEBUG="CMakeFiles/hven.dir/src/interior/utils/get_core_count.cpp.o"' > "$TG/h2.txt" 2>&1; grep -E "^(ASSERT FAIL|RC5)" "$TG/h2.txt" | cut -c1-250; h2=$(sed -n 's/^RC5=//p' "$TG/h2.txt")
echo "=== H3: falsifier, test_sqp_solver.cpp.o (a real mover) declared NOISE-ONLY in Release"
run_block 'EXPECT_CLASS_A_NOISE_ONLY_RELEASE="$EXPECT_CLASS_A_NOISE_ONLY_RELEASE tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_sqp_solver.cpp.o"' > "$TG/h3.txt" 2>&1; grep -E "^(ASSERT FAIL|RC5)" "$TG/h3.txt" | cut -c1-250; h3=$(sed -n 's/^RC5=//p' "$TG/h3.txt")
echo "=== H4: falsifier, the Release IDENTICAL slot emptied (the two library sources become unaccounted class A sources)"
run_block 'EXPECT_CLASS_A_IDENTICAL_RELEASE=""' > "$TG/h4.txt" 2>&1; grep -E "^(ASSERT FAIL|RC5)" "$TG/h4.txt" | cut -c1-250; h4=$(sed -n 's/^RC5=//p' "$TG/h4.txt")
echo "=== H5: falsifier, a modified source in no unity file declared a UNITY source"
run_block 'EXPECT_CLASS_A_UNITY_SRCS="$EXPECT_CLASS_A_UNITY_SRCS tests/sqp/test_sqp_solver.cpp"' > "$TG/h5.txt" 2>&1; grep -E "^(ASSERT FAIL|RC5)" "$TG/h5.txt" | cut -c1-250; h5=$(sed -n 's/^RC5=//p' "$TG/h5.txt")
echo "=== H6: falsifier, the UNITY slot emptied"
run_block 'EXPECT_CLASS_A_UNITY_SRCS=""' > "$TG/h6.txt" 2>&1; grep -E "^(ASSERT FAIL|RC5)" "$TG/h6.txt" | cut -c1-250; h6=$(sed -n 's/^RC5=//p' "$TG/h6.txt")
# ---- the census proofs, on doctored COPIES of the Debug census inputs (the tool is run directly) ----
CENSUS=$W/object_census_wx2.py
CD=$TG/census; mkdir -p "$CD"
for x in commands-before commands-after sha-before sha-after deps-after; do cp "$SCR/$x-Debug.txt" "$CD/$x.txt"; done
cp -r "$SCR/unity-Debug" "$CD/unity"
BSHA=$(git rev-parse "$BEFORE_S"); ASHA=$AFTER_STAMP_SHA
cen() { # cmds-after, sha-after, expect-eigen-delta, expect-c-pure-differing
  python3 -B "$CENSUS" Debug "$ROOT" "$SCR/after-Debug/.psym-manifest" "$CD/commands-before.txt" "$1" "$CD/sha-before.txt" "$2" "$CD/deps-after.txt" "$CD/unity" "$S/psym-build" \
    --changed-sources $CHANGED_SOURCES --changed-headers $CHANGED_HEADERS --before-sha "$BSHA" --after-sha "$ASHA" --expect-eigen-delta "$3" --expect-c-pure-differing "$4"; }
DECLARED_C=$( (eval "$DECL" >/dev/null 2>&1; echo "$EXPECT_CENSUS_EIGEN_DELTA $EXPECT_CENSUS_C_PURE_DIFFERING") )
DBG_E=$(echo "$DECLARED_C" | awk '{split($1,a,"/"); print a[2]}'); DBG_P=$(echo "$DECLARED_C" | awk '{split($2,a,"/"); print a[2]}')
case "$DBG_E$DBG_P" in ""|*[!0-9]*) echo "REFUSED: the declared census counts are not integers: [$DECLARED_C]"; exit 3 ;; esac
echo "=== C5: control, the census on the unmodified Debug data with the declared Debug counts (eigen_delta $DBG_E, C_pure_differing $DBG_P)"
cen "$CD/commands-after.txt" "$CD/sha-after.txt" "$DBG_E" "$DBG_P" > "$CD/c5.txt" 2>&1; c5=$?; grep -E "^CENSUS_(Debug |REFUSED|Debug_OK)" "$CD/c5.txt" | cut -c1-250; echo "C5 rc=$c5 (want 0)"
echo "=== C1: falsifier, one object's after-arm command gains -O3"
python3 -B - "$CD/commands-after.txt" "$CD/c1.txt" <<'PY'
import sys
out=[];done=False
for l in open(sys.argv[1]):
    if not done and " -c " in l and " -o " in l and ".o " in l:
        l=l.replace(" -c "," -O3 -c ",1); done=True
    out.append(l)
open(sys.argv[2],"w").write("".join(out))
PY
cen "$CD/c1.txt" "$CD/sha-after.txt" "$DBG_E" "$DBG_P" > "$CD/c1o.txt" 2>&1; c1=$?; grep -E "^CENSUS_REFUSED" "$CD/c1o.txt" | cut -c1-250; echo "C1 rc=$c1 (want 1)"
echo "=== C2: falsifier, an unmodified, delta-free, header-free object's sha is flipped (a differing object that carries no class)"
python3 -B - "$CD" "$CENSUS" "$ROOT" "$CHANGED_HEADERS" <<'PY'
import sys, os
cd, tool, root, hdrs = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4].split()
sys.path.insert(0, os.path.dirname(tool)); import object_census_wx2 as m
deps = m.parse_deps(cd + "/deps-after.txt")
changed = {os.path.normpath(os.path.join(root, h)) for h in hdrs}
B = m.parse_cmds(cd + "/commands-before.txt", ""); A = m.parse_cmds(cd + "/commands-after.txt", "")
shab = m.parse_sha(cd + "/sha-before.txt"); shaa = m.parse_sha(cd + "/sha-after.txt")
pick = None
for o in sorted(A):
    if shab[os.path.normpath(o)] != shaa[os.path.normpath(o)]: continue
    if A[o][1] != B[o][1] or A[o][2] != B[o][2]: continue            # delta-free command, same stamps
    if deps.get(os.path.normpath(o), set()) & changed: continue       # header-free
    if "CMakeFiles/hven.dir/src/" not in o: continue                  # a library object: its source is not in the range
    pick = o; break
assert pick, "no candidate object"
out = []
for l in open(cd + "/sha-after.txt").read().splitlines():
    h, p = l.split("  ", 1)
    out.append(("0" * 64 if os.path.normpath(p) == os.path.normpath(pick) else h) + "  " + p)
open(cd + "/c2.txt", "w").write("\n".join(out) + "\n"); open(cd + "/c2.pick", "w").write(pick)
PY
cen "$CD/commands-after.txt" "$CD/c2.txt" "$DBG_E" "$DBG_P" > "$CD/c2o.txt" 2>&1; c2=$?; echo "(flipped $(cat "$CD/c2.pick" 2>/dev/null))"; grep -E "^CENSUS_(UNEXPLAINED|REFUSED)" "$CD/c2o.txt" | cut -c1-250; echo "C2 rc=$c2 (want 1)"
echo "=== C3: falsifier, a wrong C_pure_differing count declared"
cen "$CD/commands-after.txt" "$CD/sha-after.txt" "$DBG_E" "$((DBG_P + 1))" > "$CD/c3.txt" 2>&1; c3=$?; grep -E "^CENSUS_REFUSED" "$CD/c3.txt" | cut -c1-250; echo "C3 rc=$c3 (want 1)"
echo "=== C4: falsifier, a pure class-C object loses the define from its AFTER command while its bytes still differ: nothing explains it"
python3 -B - "$CD" "$SCR/../../../.." <<'PY'
import sys, os, re, shlex
cd = sys.argv[1]
pick = None
for l in open(cd + "/c5.txt"):
    f = l.split()
    if len(f) >= 5 and f[0] == "CENSUS_DIFFERS" and f[3] == "C" and f[2] == "outside":
        pick = f[4]; break
assert pick, "no pure-C object in the Debug census"
out = []
for l in open(cd + "/commands-after.txt"):
    if " -o " + pick + " " in l: l = l.replace(" -DEIGEN_MAX_ALIGN_BYTES=32", "", 1)
    out.append(l)
open(cd + "/c4.txt", "w").write("".join(out)); open(cd + "/c4.pick", "w").write(pick)
PY
cen "$CD/c4.txt" "$CD/sha-after.txt" "$DBG_E" "$DBG_P" > "$CD/c4o.txt" 2>&1; c4=$?; echo "(stripped $(cat "$CD/c4.pick" 2>/dev/null))"; grep -E "^CENSUS_(UNEXPLAINED|REFUSED)" "$CD/c4o.txt" | cut -c1-250; echo "C4 rc=$c4 (want 1)"
rm -rf "$TG"
echo "H1=$h1 (want 0) H2=$h2 H3=$h3 H4=$h4 H5=$h5 H6=$h6 (want 1 each) C5=$c5 (want 0) C1=$c1 C2=$c2 C3=$c3 C4=$c4 (want 1 each)"
if [ "$h1" = 0 ] && [ "$h2" = 1 ] && [ "$h3" = 1 ] && [ "$h4" = 1 ] && [ "$h5" = 1 ] && [ "$h6" = 1 ] && [ "$c5" = 0 ] && [ "$c1" = 1 ] && [ "$c2" = 1 ] && [ "$c3" = 1 ] && [ "$c4" = 1 ]; then echo "CLASS_A_CHECK_OK"; exit 0; fi
echo "CLASS_A_CHECK_FAIL"; exit 1
