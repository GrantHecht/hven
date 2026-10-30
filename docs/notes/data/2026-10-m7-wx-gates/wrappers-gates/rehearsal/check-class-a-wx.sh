#!/usr/bin/env bash
# M7 WX gates run (NEW script, written for this declaration; it replaces A3's check-dbgonly-a3.sh, whose subject -- a Debug-only class B object -- this range does not have).
# The offline proof of the runner's final class A assertions on THIS range's own rehearsal, before any leg. This range measured a shape the carried
# assertions had no slot for (Release: 7 MODIFIED sources are NOISE-ONLY and ONE, src/qp/qp_engine.cpp, is BYTE-IDENTICAL; Debug: the same 7 NOISE-ONLY and qp_engine DIFFERS),
# so the runner gained the EXPECT_CLASS_A_IDENTICAL_{RELEASE,DEBUG} slots and this script proves them, exactly as the SCW round proved its mirror block offline:
#   H1 the block, with the declared slots and the rehearsal's own compare / manifests / saved library objects, PASSES (rc5 0) in both configurations;
#   H2 falsifier: qp_engine.cpp.o declared IDENTICAL in DEBUG (where it DIFFERS in the compare) is REFUSED;
#   H3 falsifier: sqp_solver.cpp.o (a real class A mover) declared IDENTICAL in RELEASE is REFUSED;
#   H4 falsifier: the IDENTICAL slot emptied, so qp_engine.cpp.o is an unexplained missing class A source in Release, is REFUSED;
#   H5 falsifier: a declared NOISE-ONLY object that is not NOISE-ONLY in the compare (bench_corpus, a real DIFFERS) is REFUSED.
# Everything is DERIVED: the runner's declared section and the block are cut out of the runner's own text, the modified sources come from git.
set -o pipefail
W=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd) || exit 3
ROOT=$(git -C "$W" rev-parse --show-toplevel) || exit 3
R=$W/gates-wx.sh
REH=$W/rehearsal
cd "$ROOT" || exit 3
SCR=$(bash "$R" --print-scratch)/rehearsal/gates/psym
for f in compare-Release.txt compare-Debug.txt before-Release/.psym-manifest after-Release/.psym-manifest before-Debug/.psym-manifest after-Debug/.psym-manifest; do
  [ -s "$SCR/$f" ] || { echo "REFUSED: rehearsal scratch lacks $SCR/$f"; exit 3; }
done
S=$ROOT/.scratch/gates-wx   # the runner's own scratch name; used only by the sed-extracted declarations below
export GATES_SCRATCH=$S
. "$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace/wrappers/gate_asserts_m3b.sh" || exit 3
# the runner's declared section (BEFORE= .. the line before LOG=), eval'ed verbatim
DECL=$(sed -n '/^BEFORE=\${BEFORE:-/,/^LOG=\$S/p' "$R" | sed '$d')
[ -n "$DECL" ] || { echo "D FAIL: no declared section"; exit 1; }
HEAD_AT_START=$(git rev-parse HEAD)
MODIFIED_CXX=$(git diff --name-only --diff-filter=M "$(sed -n 's/^BEFORE=\${BEFORE:-\([0-9a-f]*\)}$/\1/p' "$R")..$HEAD_AT_START" -- 'src/*.cpp' 'tests/*.cpp' 'bench/*.cpp' | tr '\n' ' ')
OBJFN=$(awk '/^obj_source\(\) \{/ {f=1} f {print} f && /^\}/ {exit}' "$R")
BLOCK=$(awk '/^# E2FR2 \(d\): the MEASURED class B set EQUALS/ {f=1} /^# --- G6/ {f=0} f {print}' "$R")
[ -n "$OBJFN" ] && [ -n "$BLOCK" ] || { echo "D4 FAIL: could not cut the block / obj_source out of the runner"; exit 1; }
echo "block lines: $(printf '%s\n' "$BLOCK" | grep -c .)"
TG=$(mktemp -d); mkdir -p "$TG/psym"
for x in compare-Release.txt compare-Debug.txt before-Release after-Release before-Debug after-Debug before-obj-Release after-obj-Release before-obj-Debug after-obj-Debug; do ln -s "$SCR/$x" "$TG/psym/$x"; done
for c in Release Debug; do cp "$REH/rehearsal-wx-classification-$c.txt" "$TG/psym/classification-$c.txt"; done
run_block() { # label, then VAR=value overrides applied after the declarations
  ( set +e; eval "$DECL" >/dev/null 2>&1; eval "$OBJFN"; G=$TG; rc5=0
    for kv in "$@"; do eval "$kv"; done
    eval "$BLOCK"
    echo "RC5=$rc5" )
}
echo "=== H1: the declared slots"
run_block > "$TG/h1.txt" 2>&1; grep -E "^(ASSERT FAIL|RC5)|IDENTICAL|NOISE-ONLY class A|class A by source" "$TG/h1.txt" | cut -c1-260
h1=$(sed -n 's/^RC5=//p' "$TG/h1.txt")
echo "=== H2: falsifier, qp_engine.cpp.o declared IDENTICAL in Debug (it DIFFERS there)"
run_block 'EXPECT_CLASS_A_IDENTICAL_DEBUG="CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o"' > "$TG/h2.txt" 2>&1; grep -E "^(ASSERT FAIL|RC5)" "$TG/h2.txt" | cut -c1-260
h2=$(sed -n 's/^RC5=//p' "$TG/h2.txt")
echo "=== H3: falsifier, sqp_solver.cpp.o (a real mover) declared IDENTICAL in Release"
run_block 'EXPECT_CLASS_A_IDENTICAL_RELEASE="CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o CMakeFiles/hven.dir/src/drivers/sqp_solver.cpp.o"' > "$TG/h3.txt" 2>&1; grep -E "^(ASSERT FAIL|RC5)" "$TG/h3.txt" | cut -c1-260
h3=$(sed -n 's/^RC5=//p' "$TG/h3.txt")
echo "=== H4: falsifier, the IDENTICAL slot emptied (qp_engine.cpp.o becomes an unaccounted class A source in Release)"
run_block 'EXPECT_CLASS_A_IDENTICAL_RELEASE=""' > "$TG/h4.txt" 2>&1; grep -E "^(ASSERT FAIL|RC5)" "$TG/h4.txt" | cut -c1-260
h4=$(sed -n 's/^RC5=//p' "$TG/h4.txt")
echo "=== H5: falsifier, bench_corpus.cpp.o (a real DIFFERS) declared NOISE-ONLY in Release"
run_block 'EXPECT_CLASS_A_NOISE_ONLY_RELEASE="$EXPECT_CLASS_A_NOISE_ONLY_RELEASE bench/CMakeFiles/hven_sqp_corpus.dir/bench_corpus.cpp.o"' > "$TG/h5.txt" 2>&1; grep -E "^(ASSERT FAIL|RC5)" "$TG/h5.txt" | cut -c1-260
h5=$(sed -n 's/^RC5=//p' "$TG/h5.txt")
rm -rf "$TG"
echo "H1=$h1 (want 0) H2=$h2 H3=$h3 H4=$h4 H5=$h5 (want 1 each)"
if [ "$h1" = 0 ] && [ "$h2" = 1 ] && [ "$h3" = 1 ] && [ "$h4" = 1 ] && [ "$h5" = 1 ]; then echo "CLASS_A_CHECK_OK"; exit 0; fi
echo "CLASS_A_CHECK_FAIL"; exit 1
