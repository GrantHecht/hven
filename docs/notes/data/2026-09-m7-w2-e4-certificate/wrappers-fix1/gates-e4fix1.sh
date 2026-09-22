#!/usr/bin/env bash
# M7 W2 E4 FIX ROUND 1 -- THE GATES FOR THE CERTIFICATE SEAM (an HVEN_TESTING-
# only injector on the kOptimal certificate, plus three fixtures and the M1
# exact-zero assertion). This file is E4's own `wrappers/gates-e4.sh`, COPIED and
# re-pointed; E4's file is NOT edited -- it produced committed transcripts.
#
#   G1  Debug: fresh configure, build (one lock hold), then the suite WITHOUT
#       the >600 s test in CHUNKS, one lock hold per chunk
#   G2  the >600 s test ALONE (Continuation.ProbeBudgetBoundsAFailingProposal)
#   G3  Release: fresh configure, build (one hold), the suite in chunks
#   G4  the U0 27-cell three-arm replay against the M6 close pin, one hold per
#       engine
#   G5  P-SYM, one hold per configuration, classified against $BEFORE with a
#       DERIVED changed-header list, plus the fail-closed non-debug section check
#   G6  install smoke, the export contract, clang-format, git diff --check
#
# ---- WHAT CHANGED FROM E4's RUNNER, AND WHY ---------------------------------
#   * ROOT IS DERIVED, not carried: this round runs in a git WORKTREE, not in
#     the main checkout, so the runner asks git for the top level of the tree it
#     lives in, and every segment's prologue reads it from `GATES_ROOT`. The
#     pre-flight asserts each segment resolves ROOT to this runner's ROOT.
#   * THE MOVER SHAPE IS ZERO (the M4 pattern), and its derivation is a VALUE:
#     `derive_movers_e4fix1.py BEFORE HEAD` reads both revisions of every
#     changed src/ and include/ file, drops what a production compile skips
#     (`#ifdef HVEN_TESTING` bodies) while keeping line numbers, and reports
#     the non-debug movers, the Debug-only movers and any production-visible
#     header. The pre-flight refuses unless those equal the declared numbers.
#     It was calibrated on three past rounds whose shapes were MEASURED (M4
#     f5205770..dfcf048b: debug-only 1; M5 2ca21aa0: debug-only 1; E4
#     2a5b11f7..99334900: non-debug 1) and agrees with all three.
#   * LOCK HOLDS ARE PER LEG, NOT PER SUITE: a long solver chain shares the
#     box, so the suites run as index chunks (`ctest -I a,b`), each its own
#     hold, and the totals are SUMMED and asserted; the replay holds per
#     engine; P-SYM holds per configuration. G2 is one test and one hold.
#   * The fold proofs are M3b's `fold_proofs_m3b.sh` copied with ROOT derived
#     (its ROOT literal named the main checkout).
#
# ---- THIS ROUND'S DECLARED EXPECTATIONS -------------------------------------
# The derivation is `gates-fix1/psym-derivation-e4fix1.txt`, committed WITH this
# file and BEFORE the chain was detached. BEFORE = 53676599 (E4's head, this
# round's base). The claim: the production library is BYTE-IDENTICAL in Release
# and differs in Debug only in `qp_engine.cpp.o`'s debug sections.
#   Release   DIFFERING 0 = class A 0 + class B 0; DEBUG_ONLY 0
#   Debug     DIFFERING 0 = class A 0 + class B 0; DEBUG_ONLY 1 (qp_engine.cpp.o)
#   NON-DEBUG-MOVER set EMPTY in both; `psym_compare.sh compare` exits 0.
#   G4: 27 x 75 x 3, 0 differences.
#
# WALL IS INFORMATIONAL. Nothing this file prints is a timing claim.
# =========================================================================
set -o pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && git rev-parse --show-toplevel) || exit 90
S=$ROOT/.scratch/m7w2e4fix1gates
G=$S/gates
# THE LINE ABOVE IS THE ROUND'S ONE DEFINITION OF ITS SCRATCH TREE, AND THE ONLY
# LITERAL NAMING IT ANYWHERE IN THE CHAIN (M4 fix round 1's property, carried).
export GATES_SCRATCH="$S"
export GATES_ROOT="$ROOT"

PREFLIGHT_ONLY=0
case "${1:-}" in
  --print-scratch)  printf '%s\n' "$S"; exit 0 ;;
  --preflight-only) PREFLIGHT_ONLY=1; shift ;;
esac
W=$ROOT/docs/notes/data/2026-09-m7-w2-e4-certificate/wrappers-fix1
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace
M4W=$ROOT/docs/notes/data/2026-09-m7-w2-m4-anticycling-arm/wrappers
ASSERTS=$ART/wrappers/gate_asserts_m3b.sh
DERIVE=$W/derive_movers_e4fix1.py

# ---- THIS ROUND'S EXPECTED COUNTS -------------------------------------------
# E4 MEASURED 2733 Debug / 2734 Release at 2d77a672; nothing under tests/ src/
# include/ bench/ cmake/ CMakeLists.txt changed between that and 53676599. This
# round adds exactly THREE `TEST()` blocks (19 -> 22 in
# test_qp_degeneracy_arm.cpp) and renames none; the pre-flight re-derives the
# delta from git.
BEFORE=${BEFORE:-53676599}
EXPECT_TEST_DELTA=3
EXPECT_DEBUG_TESTS=${EXPECT_DEBUG_TESTS:-2736}
EXPECT_RELEASE_TESTS=${EXPECT_RELEASE_TESTS:-2737}
EXPECT_LONG_TESTS=1
CHUNK=400
EXPECT_REPLAY_CELLS=27
EXPECT_REPLAY_COLUMNS=75
EXPECT_REPLAY_DIFFS=0
EXPECT_REPLAY_ENGINES=3
MIN_NEGATIVE_CONTROL=1

# ---- THIS ROUND'S EXPECTED P-SYM MOVER SHAPE: ZERO --------------------------
EXPECT_PSYM_DIFFERING=${EXPECT_PSYM_DIFFERING:-0}
EXPECT_PSYM_CLASS_A=${EXPECT_PSYM_CLASS_A:-0}
EXPECT_PSYM_CLASS_B=${EXPECT_PSYM_CLASS_B:-0}
EXPECT_DEBUG_ONLY_RELEASE=${EXPECT_DEBUG_ONLY_RELEASE:-0}
EXPECT_DEBUG_ONLY_DEBUG=${EXPECT_DEBUG_ONLY_DEBUG:-1}

mkdir -p "$G" "$S/logs"
LOG=$S/logs/gates-e4fix1.log
[ "$PREFLIGHT_ONLY" -eq 1 ] || exec >"$LOG" 2>&1
echo "GATES_START $(date -Is)"
cd "$ROOT" || exit 90
echo "=== ROOT=$ROOT"
echo "=== HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l) ==="
if [ "$PREFLIGHT_ONLY" -eq 1 ]; then
  echo "=== PREFLIGHT_ONLY: the clean-tree refusal is NOT applied in this mode; the chain still applies it ==="
else
  [ -z "$(git status --porcelain)" ] || { echo "REFUSED: the tree is not clean"; exit 2; }
fi
echo "=== GATES_SCRATCH=$GATES_SCRATCH ==="
echo "=== BEFORE=$BEFORE  EXPECT_DEBUG_TESTS=$EXPECT_DEBUG_TESTS  EXPECT_RELEASE_TESTS=$EXPECT_RELEASE_TESTS ==="

HEAD_AT_START=$(git rev-parse HEAD)
echo "=== HEAD_AT_START=$HEAD_AT_START"
git merge-base --is-ancestor "$BEFORE" "$HEAD_AT_START" \
  || { echo "REFUSED: BEFORE $BEFORE is not an ancestor of HEAD"; exit 2; }
CHANGED_HEADERS=$(git diff --name-only "$BEFORE..$HEAD_AT_START" -- include/ | tr '\n' ' ')
CHANGED_SOURCES=$(git diff --name-only "$BEFORE..$HEAD_AT_START" -- src/ tests/ | tr '\n' ' ')
echo "=== CHANGED HEADERS: $CHANGED_HEADERS"
echo "=== CHANGED SOURCES: $CHANGED_SOURCES"
[ -n "$CHANGED_HEADERS" ] || { echo "REFUSED: no changed header under include/ against $BEFORE"; exit 2; }
[ -n "$CHANGED_SOURCES" ] || { echo "REFUSED: no changed source against $BEFORE"; exit 2; }

# ---- VALUE-BASED PRE-FLIGHT (1): THE MOVER SHAPE, DERIVED -------------------
DERIVED=$(python3 -B "$DERIVE" "$ROOT" "$BEFORE" "$HEAD_AT_START") \
  || { echo "REFUSED: the mover derivation did not run"; exit 2; }
echo "$DERIVED" | sed 's/^/=== DERIVED /'
dv() { printf '%s\n' "$DERIVED" | sed -n "s/^$1=//p"; }
EXPECT_LIB_MOVERS=$(dv NON_DEBUG_MOVERS)
[ "$(dv NON_DEBUG_MOVER_COUNT)" = "$EXPECT_PSYM_CLASS_A" ] \
  || { echo "REFUSED: derived non-debug movers $(dv NON_DEBUG_MOVER_COUNT) != declared class A $EXPECT_PSYM_CLASS_A"; exit 2; }
[ "$(dv DEBUG_ONLY_DEBUG_COUNT)" = "$EXPECT_DEBUG_ONLY_DEBUG" ] \
  || { echo "REFUSED: derived Debug-only movers $(dv DEBUG_ONLY_DEBUG_COUNT) != declared $EXPECT_DEBUG_ONLY_DEBUG"; exit 2; }
[ "$(dv PRODUCTION_VISIBLE_HEADER_COUNT)" = "$EXPECT_PSYM_CLASS_B" ] \
  || { echo "REFUSED: derived production-visible headers $(dv PRODUCTION_VISIBLE_HEADER_COUNT) != declared class B $EXPECT_PSYM_CLASS_B"; exit 2; }
echo "=== PREFLIGHT_VALUE_OK: movers non-debug $EXPECT_PSYM_CLASS_A, debug-only $EXPECT_DEBUG_ONLY_DEBUG, visible headers $EXPECT_PSYM_CLASS_B"

# ---- VALUE-BASED PRE-FLIGHT (2): THE TEST-COUNT DELTA, DERIVED --------------
delta=0
for f in $(git diff --name-only "$BEFORE..$HEAD_AT_START" -- 'tests/*.cpp'); do
  b=$(git show "$BEFORE:$f" 2>/dev/null | grep -cE '^TEST(_F|_P)?\(')
  a=$(git show "$HEAD_AT_START:$f" | grep -cE '^TEST(_F|_P)?\(')
  echo "=== TEST BLOCKS $f: $b -> $a"
  delta=$((delta + a - b))
done
[ "$delta" = "$EXPECT_TEST_DELTA" ] \
  || { echo "REFUSED: derived TEST() delta $delta != declared $EXPECT_TEST_DELTA"; exit 2; }
echo "=== PREFLIGHT_VALUE_OK: TEST() delta $delta == declared $EXPECT_TEST_DELTA (2733+3, 2734+3)"

# shellcheck source=../../2026-09-m7-w2-m3b-minor-trace/wrappers/gate_asserts_m3b.sh
. "$ASSERTS" || exit 90

# assert_ctest_chunks LABEL EXPECTED_TOTAL LOG... -- every chunk must carry its
# own CTest summary with 0 failed, and the chunk totals must SUM to the declared
# total. A missing chunk summary is a refusal, not a zero.
assert_ctest_chunks() {
  local label="$1" expect="$2"; shift 2
  local sum=0 rc=0 f t fl
  [ "$#" -gt 0 ] || { echo "ASSERT FAIL $label: no chunk logs"; return 1; }
  for f in "$@"; do
    t=$(ctest_total "$f") || { echo "ASSERT FAIL $label: no CTest summary in $f"; rc=1; continue; }
    fl=$(ctest_failed "$f")
    [ "$fl" = "0" ] || { echo "ASSERT FAIL $label: $f reports $fl failed"; rc=1; }
    echo "  chunk $(basename "$f"): $t tests, $fl failed"
    sum=$((sum + t))
  done
  assert_eq "$label chunks' total" "$sum" "$expect" || rc=1
  return $rc
}

assert_segment_scratch() {
  local f="$1" line probe got_r got_s got_g code hits reassign rc=0
  if [ ! -s "$f" ]; then
    echo "SEGMENT REFUSED $f: was not written, or is empty"
    return 1
  fi
  line=$(sed -n '2p' "$f")
  case "$line" in
    *'ROOT=${GATES_ROOT:?}; S=${GATES_SCRATCH:?}; G=$S/gates'*) : ;;
    *) echo "SEGMENT REFUSED $f: line 2 is not the GATES_ROOT/GATES_SCRATCH prologue: $line"
       return 1 ;;
  esac
  probe=$(GATES_ROOT="$GATES_ROOT" GATES_SCRATCH="$GATES_SCRATCH" bash -c "$line"'; printf "%s\n%s\n%s\n" "$ROOT" "$S" "$G"' 2>&1) || {
    echo "SEGMENT REFUSED $f: its prologue would not evaluate: $probe"
    return 1
  }
  got_r=$(printf '%s\n' "$probe" | sed -n '1p')
  got_s=$(printf '%s\n' "$probe" | sed -n '2p')
  got_g=$(printf '%s\n' "$probe" | sed -n '3p')
  [ "$got_r" = "$ROOT" ] || { echo "SEGMENT REFUSED $f: resolves ROOT='$got_r'; this runner's is '$ROOT'"; rc=1; }
  [ "$got_s" = "$GATES_SCRATCH" ] || { echo "SEGMENT REFUSED $f: resolves S='$got_s'"; rc=1; }
  [ "$got_g" = "$GATES_SCRATCH/gates" ] || { echo "SEGMENT REFUSED $f: resolves G='$got_g'"; rc=1; }
  code=$(sed -e 's/^[[:space:]]*#.*$//' "$f")
  hits=$(printf '%s\n' "$code" | grep -nE '\.scratch/|/home/')
  if [ -n "$hits" ]; then
    echo "SEGMENT REFUSED $f: its CODE names a path of its own:"
    echo "$hits"
    rc=1
  fi
  reassign=$(printf '%s\n' "$code" | sed '1,2d' | grep -nE '(^|[;[:space:]])(ROOT|S|G)=')
  if [ -n "$reassign" ]; then
    echo "SEGMENT REFUSED $f: it re-assigns ROOT, S or G after its prologue:"
    echo "$reassign"
    rc=1
  fi
  [ $rc -eq 0 ] && echo "SEGMENT_OK $f: resolves ROOT=$got_r  S=$got_s  G=$got_g"
  return $rc
}

run_locked() { # label, then the command
  local label="$1"; shift
  flock /tmp/box-build.lock bash -c '
    set -o pipefail
    label="$1"; shift
    echo "FOREGROUND_START ${label} $(date -Is)"
    foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
    [ -z "$foreign" ] && echo "PGREP_EMPTY ${label}: no foreign cell binary under my lock" \
                      || echo "NOTE: a foreign cell binary is alive under my lock: $foreign"
    "$@"
    rc=$?
    echo "FOREGROUND_END ${label} $(date -Is) rc=${rc}"
    exit ${rc}
  ' _ "$label" "$@"
  local rc=$?
  echo "WRAPPER_RC ${label} rc=${rc}"
  return $rc
}

emit_segments() {
# --- build: configure + build one tree (G1 Debug / G3 Release) -------------
cat > "$S/build.sh" <<'INNER'
set -o pipefail
ROOT=${GATES_ROOT:?}; S=${GATES_SCRATCH:?}; G=$S/gates
CFG="$1"; TREE="$S/$2"; TAG="$3"; shift 3
rm -rf "$TREE"
cmake -S "$ROOT" -B "$TREE" -G Ninja -DCMAKE_BUILD_TYPE="$CFG" \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON "$@" > "$G/configure-$TAG.log" 2>&1 || exit 91
cmake --build "$TREE" -j "$(nproc)" > "$G/build-$TAG.log" 2>&1 || exit 91
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest --test-dir "$TREE" -N > "$G/listing-$TAG.log" 2>&1 || exit 91
tail -1 "$G/listing-$TAG.log"
INNER

# --- suite chunk: `ctest -I a,b`, one lock hold --------------------------------
cat > "$S/chunk.sh" <<'INNER'
set -o pipefail
ROOT=${GATES_ROOT:?}; S=${GATES_SCRATCH:?}; G=$S/gates
TREE="$S/$1"; TAG="$2"; A="$3"; B="$4"; shift 4
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest --test-dir "$TREE" -j4 --no-tests=error -I "$A,$B" \
  "$@" --output-on-failure > "$G/suite-$TAG-$A.log" 2>&1
rc=$?; tail -4 "$G/suite-$TAG-$A.log"; exit $rc
INNER

# --- G2: the long test, ALONE ---------------------------------------------
cat > "$S/g2.sh" <<'INNER'
set -o pipefail
ROOT=${GATES_ROOT:?}; S=${GATES_SCRATCH:?}; G=$S/gates
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest --test-dir "$S/gates-dbg" --no-tests=error \
  -R "^Continuation.ProbeBudgetBoundsAFailingProposal$" --output-on-failure > "$G/suite-debug-long.log" 2>&1
rc=$?; tail -8 "$G/suite-debug-long.log"; exit $rc
INNER

# --- G4: one engine of the replay, one hold -----------------------------------
cat > "$S/g4.sh" <<'INNER'
set -o pipefail
ROOT=${GATES_ROOT:?}; S=${GATES_SCRATCH:?}; G=$S/gates
eng="$1"
BIN=$S/gates-rel/bench/hven_sqp_corpus
D=$G/replay; mkdir -p "$D"
CELLS="$(grep -v '^#' "$ROOT/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt" | tr -d '\n')"
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
  "$BIN" --engine "$eng" --cells "$CELLS" --csv "$D/head-$eng.csv" > "$D/run-$eng.log" 2>&1
rc=$?; echo "ran head-$eng rc=$rc"; exit $rc
INNER

# --- G4 comparison (no solver runs; no lock needed) -----------------------------
cat > "$S/g4cmp.sh" <<'INNER'
set -o pipefail
ROOT=${GATES_ROOT:?}; S=${GATES_SCRATCH:?}; G=$S/gates
D=$G/replay; : > "$D/comparison.txt"
BASE=$ROOT/docs/notes/data/2026-09-m6-close-gate/replay
worst=0
for eng in walk ssn ipm; do
  python3 -B "$ROOT/scripts/compare_replay.py" "$BASE/head-$eng.csv" "$D/head-$eng.csv" \
    "M6 CLOSE HEAD-$eng vs M7 W2 E4 FIX1 HEAD-$eng" | tee -a "$D/comparison.txt"
  rc=${PIPESTATUS[0]}; echo "  CMP_RC=$rc"; [ $rc -eq 0 ] || worst=1
done
exit $worst
INNER

# --- G5: P-SYM, ONE configuration per hold -------------------------------------
cat > "$S/g5.sh" <<'INNER'
set -uo pipefail
ROOT=${GATES_ROOT:?}; S=${GATES_SCRATCH:?}; G=$S/gates
BD=$S/psym-build; OUT=$G/psym
CFG="$1"; BEFORE="$2"; CHANGED_SOURCES="$3"; CHANGED_HEADERS="$4"; MIN_CONTROL="$5"
WANT_DIFFERING="$6"; WANT_A="$7"; WANT_B="$8"; WANT_DEBUG_ONLY="$9"; WANT_MOVERS="${10}"
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace
mkdir -p "$OUT"
HEAD_SHA=$(git -C "$ROOT" rev-parse HEAD)
BRANCH=$(git -C "$ROOT" rev-parse --abbrev-ref HEAD)
[ -z "$(git -C "$ROOT" status --porcelain)" ] || { echo "REFUSED: tree not clean"; exit 2; }
cp "$ROOT/docs/notes/data/2026-09-m7-w2-m4-anticycling-arm/wrappers/classify_psym_m4.py" \
   "$S/classify_psym.py" || exit 93
cp "$ART/wrappers/gate_asserts_m3b.sh" "$S/gate_asserts.sh" || exit 93
cp "$ART/wrappers/check_sections_fix2.sh" "$S/check_sections.sh" || exit 93
cp "$ART/wrappers/section_digest_fix2.py" "$S/section_digest_fix2.py" || exit 93
export DIGEST="$S/section_digest_fix2.py"
. "$S/gate_asserts.sh" || exit 93
rc=0
echo "=== P-SYM $CFG  after=$HEAD_SHA  before=$BEFORE"
rm -rf "$BD" "$OUT/after-$CFG" "$OUT/before-$CFG" "$OUT/after-obj-$CFG" "$OUT/before-obj-$CFG"
cmake -S "$ROOT" -B "$BD" -G Ninja -DCMAKE_BUILD_TYPE="$CFG" \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON \
  > "$OUT/configure-$CFG.log" 2>&1 || exit 91
CCACHE_DISABLE=1 cmake --build "$BD" -j "$(nproc)" > "$OUT/build-after-$CFG.log" 2>&1 || exit 91
"$ROOT/scripts/psym_compare.sh" capture "$OUT/after-$CFG" "$BD" > "$OUT/capture-after-$CFG.log" 2>&1
cap_after_rc=$?
mkdir -p "$OUT/after-obj-$CFG"
(cd "$BD" && find CMakeFiles/hven.dir -name '*.o' | LC_ALL=C sort) > "$OUT/libobjs-$CFG.txt"
while IFS= read -r o; do
    mkdir -p "$OUT/after-obj-$CFG/$(dirname "$o")"
    cp "$BD/$o" "$OUT/after-obj-$CFG/$o"
done < "$OUT/libobjs-$CFG.txt"
restore() { git -C "$ROOT" checkout -q "$BRANCH" 2>/dev/null || git -C "$ROOT" checkout -q "$HEAD_SHA"; }
trap restore EXIT
git -C "$ROOT" checkout -q --detach "$BEFORE" || exit 92
CCACHE_DISABLE=1 cmake --build "$BD" -j "$(nproc)" > "$OUT/build-before-$CFG.log" 2>&1 || { restore; exit 91; }
"$ROOT/scripts/psym_compare.sh" capture "$OUT/before-$CFG" "$BD" > "$OUT/capture-before-$CFG.log" 2>&1
cap_before_rc=$?
mkdir -p "$OUT/before-obj-$CFG"
while IFS= read -r o; do
    mkdir -p "$OUT/before-obj-$CFG/$(dirname "$o")"
    [ -f "$BD/$o" ] && cp "$BD/$o" "$OUT/before-obj-$CFG/$o"
done < "$OUT/libobjs-$CFG.txt"
git -C "$ROOT" checkout -q "$BRANCH"
trap - EXIT
assert_capture "after-$CFG" "$cap_after_rc" "$OUT/after-$CFG" || rc=1
assert_capture "before-$CFG" "$cap_before_rc" "$OUT/before-$CFG" || rc=1
assert_capture_pair "$OUT/before-$CFG" "$OUT/after-$CFG" || rc=1
echo "tree restored to $(git -C "$ROOT" rev-parse --abbrev-ref HEAD) $(git -C "$ROOT" rev-parse --short HEAD)"
[ "$(git -C "$ROOT" rev-parse HEAD)" = "$HEAD_SHA" ] || { echo "ASSERT FAIL tree not restored to $HEAD_SHA"; rc=1; }
"$ROOT/scripts/psym_compare.sh" compare "$OUT/before-$CFG" "$OUT/after-$CFG" > "$OUT/compare-$CFG.txt" 2>&1
cmp_rc=$?
echo "PSYM_COMPARE_RC_$CFG=$cmp_rc (0 is EXPECTED: zero movers declared)"
# shellcheck disable=SC2086
python3 -B "$S/classify_psym.py" --compare "$OUT/compare-$CFG.txt" --build "$BD" \
  --repo "$ROOT" --base "$BEFORE" --head "$HEAD_SHA" \
  --changed-sources $CHANGED_SOURCES --changed-headers $CHANGED_HEADERS \
  --expect-differing "$WANT_DIFFERING" \
  --min-negative-control "$MIN_CONTROL" > "$OUT/classification-$CFG.txt" 2>&1
cls_rc=$?
echo "CLASSIFY_RC_$CFG=$cls_rc"
grep -E "^(HEADER |  repo|  base|  head|  cmd|  list|P-SYM|DIFFERING|  class|  UNEXPLAINED|  NEGATIVE|REFUSED)" "$OUT/classification-$CFG.txt"
assert_psym "$CFG" "$OUT/classification-$CFG.txt" "$cmp_rc" "$cls_rc" "$MIN_CONTROL" \
  "$WANT_DIFFERING" "$WANT_A" "$WANT_B" || rc=1
bash "$S/check_sections.sh" "$CFG" "$OUT/before-obj-$CFG" "$OUT/after-obj-$CFG" \
  "$OUT/libobjs-$CFG.txt" > "$OUT/sections-$CFG.txt" 2>&1
sec_rc=$?
echo "SECTIONS_RC_$CFG=$sec_rc"
grep -E "^(SECTIONS_|NON-DEBUG-MOVER|DEBUG-ONLY|REFUSED)" "$OUT/sections-$CFG.txt" | head -40
got_movers=$(sed -n 's/^NON-DEBUG-MOVER  *\([^ ]*\).*/\1/p' "$OUT/sections-$CFG.txt" | LC_ALL=C sort | tr '\n' ' ' | sed 's/ $//')
got_refused=$(sed -n 's/^SECTIONS_'"$CFG"'_REFUSED=//p' "$OUT/sections-$CFG.txt")
echo "NON_DEBUG_MOVERS_GOT_$CFG=[$got_movers]  DECLARED=[$WANT_MOVERS]  REFUSED=$got_refused"
if [ -z "$WANT_MOVERS" ]; then want_sec_rc=0; else want_sec_rc=1; fi
if [ "$sec_rc" -ne "$want_sec_rc" ] || [ "$got_refused" != "0" ] || [ "$got_movers" != "$WANT_MOVERS" ]; then
  echo "ASSERT FAIL sections $CFG: rc=$sec_rc (want $want_sec_rc), refused='$got_refused', movers [$got_movers] != declared [$WANT_MOVERS]"
  rc=1
else
  echo "ASSERT OK   sections $CFG: the non-debug movers are EXACTLY the declared set, nothing refused"
fi
got_dbgonly=$(sed -n 's/^SECTIONS_'"$CFG"'_DEBUG_ONLY=//p' "$OUT/sections-$CFG.txt")
if [ "$got_dbgonly" != "$WANT_DEBUG_ONLY" ]; then
  echo "ASSERT FAIL sections $CFG: DEBUG_ONLY is '$got_dbgonly' and this round DECLARED '$WANT_DEBUG_ONLY'"
  rc=1
else
  echo "ASSERT OK   sections $CFG: DEBUG_ONLY=$got_dbgonly, as declared"
fi
grep -E "^DEBUG-ONLY" "$OUT/sections-$CFG.txt" | head -5
echo "G5_${CFG}_ASSERT_RC=$rc"
exit $rc
INNER

# --- G6: smoke, export contract, format, whitespace ------------------------
cat > "$S/g6.sh" <<'INNER'
set -o pipefail
ROOT=${GATES_ROOT:?}; S=${GATES_SCRATCH:?}; G=$S/gates
CHANGED="$1"; ASSERTS="$2"
cd "$ROOT" || exit 90
. "$ASSERTS" || exit 90
rc=0
rm -rf "$S/smoke-work"
HVEN_INSTALL_SMOKE_WORKDIR="$S/smoke-work" bash scripts/check_install_smoke.sh > "$G/install-smoke.log" 2>&1
sub=$?; echo "INSTALL_SMOKE_RC=$sub"; tail -4 "$G/install-smoke.log"; rc=$(accumulate_rc "$rc" "$sub")
ALIGN=$(grep -o "EIGEN_MAX_ALIGN_BYTES resolved to [0-9]*" "$S/smoke-work/hven-configure.log" | tail -1 | awk '{print $NF}')
echo "ALIGN_FROM_THIS_BUILDS_CONFIGURE_LOG=$ALIGN"
[ -n "$ALIGN" ] || { echo "REFUSED: no EIGEN_MAX_ALIGN_BYTES in this build's configure log"; rc=1; }
TARGETS="$S/smoke-work/hven-install/lib64/cmake/hven/hvenTargets.cmake"
ls -l "$TARGETS"; sub=$?; rc=$(accumulate_rc "$rc" "$sub")
bash scripts/check_export_contract.sh "$TARGETS" linux "$ALIGN" > "$G/export-contract.log" 2>&1
sub=$?; echo "EXPORT_CONTRACT_RC=$sub"; tail -3 "$G/export-contract.log"; rc=$(accumulate_rc "$rc" "$sub")
echo "--- clang-format on this round's changed C++ files, derived from git"
# shellcheck disable=SC2086
clang-format --dry-run -Werror $CHANGED
sub=$?; echo "CLANG_FORMAT_RC=$sub"; rc=$(accumulate_rc "$rc" "$sub")
git diff --check
sub=$?; echo "DIFF_CHECK_RC=$sub"; rc=$(accumulate_rc "$rc" "$sub")
dirty=$(git status --porcelain | wc -l)
echo "DIRTY_FILES=$dirty"
[ "$dirty" -eq 0 ] || rc=1
echo "G6_ASSERT_RC=$rc"
exit "$rc"
INNER
}

emit_segments || { echo "REFUSED: a gate segment could not be written"; exit 2; }
preflight_rc=0
for seg in build chunk g2 g4 g4cmp g5 g6; do
  assert_segment_scratch "$S/$seg.sh" || preflight_rc=1
done
if [ "$preflight_rc" -ne 0 ]; then
  echo "REFUSED: the pre-flight rejected a gate segment. NO LEG HAS RUN."
  exit 2
fi
echo "=== PREFLIGHT_OK: 7 segments written and asserted against GATES_ROOT/GATES_SCRATCH, before the first leg ==="
if [ "$PREFLIGHT_ONLY" -eq 1 ]; then
  echo "PREFLIGHT_ONLY: stopping before G1. No leg was run and no build tree was touched."
  exit 0
fi

# run_suite TREE TAG EXPECT [ctest args...] -- chunked, one hold per chunk.
run_suite() {
  local tree="$1" tag="$2" expect="$3"; shift 3
  local n a b rc=0 logs=()
  n=$(sed -n 's/^Total Tests: *\([0-9]*\).*/\1/p' "$G/listing-$tag.log")
  case "$n" in ''|*[!0-9]*) echo "ASSERT FAIL $tag: no test listing count"; return 1 ;; esac
  echo "=== $tag: $n registered tests, chunks of $CHUNK"
  a=1
  while [ "$a" -le "$n" ]; do
    b=$((a + CHUNK - 1)); [ "$b" -gt "$n" ] && b=$n
    run_locked "$tag-suite-$a-$b" bash "$S/chunk.sh" "$tree" "$tag" "$a" "$b" "$@" || rc=1
    logs+=("$G/suite-$tag-$a.log")
    a=$((b + 1))
  done
  assert_ctest_chunks "$tag suite" "$expect" "${logs[@]}" || rc=1
  return $rc
}

# --- G1: Debug -------------------------------------------------------------
run_locked G1-debug-build bash "$S/build.sh" Debug gates-dbg debug; rc1=$?
if [ $rc1 -eq 0 ]; then
  run_suite gates-dbg debug "$EXPECT_DEBUG_TESTS" -E "Continuation.ProbeBudgetBoundsAFailingProposal" || rc1=1
fi

# --- G2: the long test, ALONE ---------------------------------------------
run_locked G2-debug-long bash "$S/g2.sh"; rc2=$?
assert_ctest "G2 long test" "$G/suite-debug-long.log" "$EXPECT_LONG_TESTS" || rc2=1

# --- G3: Release ----------------------------------------------------------
run_locked G3-release-build bash "$S/build.sh" Release gates-rel release -DHVEN_BUILD_BENCH=ON; rc3=$?
if [ $rc3 -eq 0 ]; then
  run_suite gates-rel release "$EXPECT_RELEASE_TESTS" || rc3=1
fi

# --- G4: the replay, one hold per engine -----------------------------------
rc4=0
for eng in walk ssn ipm; do
  run_locked "G4-replay-$eng" bash "$S/g4.sh" "$eng" || rc4=1
done
bash "$S/g4cmp.sh" || rc4=1
assert_replay "$G/replay/comparison.txt" "$EXPECT_REPLAY_CELLS" "$EXPECT_REPLAY_COLUMNS" \
  "$EXPECT_REPLAY_DIFFS" "$EXPECT_REPLAY_ENGINES" || rc4=1

# --- G5: P-SYM, one hold per configuration -------------------------------------
rc5=0
rm -rf "$G/psym"
run_locked G5-psym-Release bash "$S/g5.sh" Release "$BEFORE" "$CHANGED_SOURCES" "$CHANGED_HEADERS" \
  "$MIN_NEGATIVE_CONTROL" "$EXPECT_PSYM_DIFFERING" "$EXPECT_PSYM_CLASS_A" "$EXPECT_PSYM_CLASS_B" \
  "$EXPECT_DEBUG_ONLY_RELEASE" "$EXPECT_LIB_MOVERS" || rc5=1
run_locked G5-psym-Debug bash "$S/g5.sh" Debug "$BEFORE" "$CHANGED_SOURCES" "$CHANGED_HEADERS" \
  "$MIN_NEGATIVE_CONTROL" "$EXPECT_PSYM_DIFFERING" "$EXPECT_PSYM_CLASS_A" "$EXPECT_PSYM_CLASS_B" \
  "$EXPECT_DEBUG_ONLY_DEBUG" "$EXPECT_LIB_MOVERS" || rc5=1
# THE DEBUG-ONLY OBJECT IS THE DERIVED ONE, not merely one object.
got_dbg=$(sed -n 's/^DEBUG-ONLY  *\([^ ]*\).*/\1/p' "$G/psym/sections-Debug.txt" | LC_ALL=C sort | tr '\n' ' ' | sed 's/ $//')
want_dbg=$(dv DEBUG_ONLY_DEBUG)
if [ "$got_dbg" = "$want_dbg" ]; then
  echo "ASSERT OK   sections Debug: the DEBUG-ONLY set is exactly the derived [$want_dbg]"
else
  echo "ASSERT FAIL sections Debug: DEBUG-ONLY set [$got_dbg] != derived [$want_dbg]"
  rc5=1
fi
# shellcheck disable=SC2086
assert_psym_cross_config "$G/psym/classification-Release.txt" "$G/psym/classification-Debug.txt" \
  -- $CHANGED_SOURCES || rc5=1
echo "G5_ASSERT_RC=$rc5"

# --- G6: smoke, export contract, format, whitespace ------------------------
CHANGED_CXX=$(git diff --name-only "$BEFORE..$HEAD_AT_START" -- 'include/*.h' 'src/*.cpp' 'tests/*.cpp' | tr '\n' ' ')
echo "=== CHANGED C++ FOR CLANG-FORMAT: $CHANGED_CXX"
run_locked G6-smoke bash "$S/g6.sh" "$CHANGED_CXX" "$ASSERTS"; rc6=$?

echo "GATES_SUMMARY G1=$rc1 G2=$rc2 G3=$rc3 G4=$rc4 G5=$rc5 G6=$rc6"
echo "GATES_END $(date -Is)"
gates_aggregate "$rc1" "$rc2" "$rc3" "$rc4" "$rc5" "$rc6"
exit $?
