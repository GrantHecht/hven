#!/usr/bin/env bash
# M7 W2 E1b FIX ROUND 3 -- THE REDUCED GATES on the fix3 code commit (the Codex
# re-check's one remaining item: the lift-estimate wording sweep). Comments in
# src/drivers/sqp_solver.cpp and in the PUBLIC header include/hven/drivers/sqp_solver.h;
# in tests/sqp/test_equality_basis.cpp comments and ONE test renamed
# (EqualityBasis.T4NearCapMultiColumnLiftMeetsItsForwardBound ->
#  EqualityBasis.T4NearCapMultiColumnLiftMeetsItsForwardEstimate; 1 renamed, 0 added,
# 0 removed); a comment in tests/sqp/test_sqp_equality_basis_seams.cpp (outside the
# P-SYM universe). NO numeric change, no logic change.
# SUCCESSOR r2: this file is gates-e1bfix3.sh (this directory, NOT edited after its
# first leg) COPIED; the ONLY changes are the r2 names in (j) and E1BFR3r2 (n):
#   (n) THE IN-GATE RENAME CHECK READS COUNTS. The first chain's `suite_names LOG |
#     grep -qxF NAME` under `set -o pipefail` returned 141 whenever NAME WAS present:
#     grep -q exits on its first match, the upstream `sort` takes SIGPIPE, and the
#     pipeline's status is the sort's -- so `! ... grep -q RENAME_TO` refused a run
#     that DID carry the rename (G1/G3 by-name FAIL on a correct run). The check is
#     now: RENAME_FROM counted 0 AND RENAME_TO counted 1 (grep -c reads its whole
#     input), else refuse. Offline mode `--check-names CFG LOG` runs the full by-name
#     check against the declared baseline and exits 0 (pass) or 2 (refused).
# PROVENANCE: this file is the fix-round-2 r2 runner
# (../wrappers-e1b-fix2/gates-e1bfix2-r2.sh) COPIED and re-pointed; that file is NOT
# edited. Its labelled changes (a)-(i) are CARRIED unchanged under the label E1BFR3
# (the same letters, the same code): (c) the outside-universe target hven_sqp_seam_tests,
# (d) the comment-only proof by value over src/*.cpp and include/, (e) the non-debug
# section movers declared by name per configuration, (f) G2/G4 NOT RUN checked by
# value, (g)/(i) the class B by-name check over normalized sets (empty == empty),
# (h) the suite compared BY NAME. What changed, and nothing else:
#   E1BFR3 (j) the scratch tree (.scratch/m7w2e1bfix3gates), the log name
#     (gates-e1bfix3.log) and the derivation file (psym-derivation-e1bfix3.txt);
#     in THIS SUCCESSOR (r2): .scratch/m7w2e1bfix3gates-r2, gates-e1bfix3-r2.log,
#     psym-derivation-e1bfix3-r2.txt;
#   E1BFR3 (k) every declared value, DERIVED from de62a2e7..<the fix3 code commit>
#     and, for the P-SYM/section shape, measured by a FRESH P-SYM rehearsal of this
#     file's own g5 segment on that exact pair (rehearsal/);
#   E1BFR3 (l) THE BY-NAME BASELINE is the previous round's committed transcripts
#     (../gates-e1b-fix2/suite-{debug,release}.txt) WITH THE DECLARED RENAME applied:
#     the baseline must name RENAME_FROM exactly once and RENAME_TO never; the run must
#     name RENAME_TO and never RENAME_FROM; the pre-flight proves by value that the
#     range's only TEST-macro line change over tests/*.cpp is that one -/+ pair;
#   E1BFR3 (m) THE CHANGED PUBLIC HEADER. include/hven/drivers/sqp_solver.h is
#     comment-only (the (d) proof covers include/) and is DECLARED
#     (EXPECT_COMMENT_ONLY_HEADERS; the derived changed-header set must equal it). Its
#     line shift reaches every includer's DWARF (declaration lines) and nothing else:
#     no inline code sits below the changed line. G5's DEBUG-ONLY rule therefore admits
#     a library object that is a comment-only source's own object OR whose Ninja deps
#     (`ninja -t deps`, the build's own record) name a changed header -- nothing else
#     -- and the DEBUG_ONLY count per configuration is the rehearsal's, declared.
#
#   G1  Debug: fresh configure, build, suite WITHOUT the >600 s test
#   G2  NOT RUN (E1BFR3 (f))
#   G3  Release: fresh configure, build, suite
#   G4  NOT RUN (E1BFR3 (f))
#   G5  P-SYM, Release and Debug, classified against $BEFORE with a DERIVED
#       changed-header list, plus the fail-closed non-debug section check
#   G6  install smoke, the export contract, clang-format, git diff --check
#
# ---- THIS ROUND'S DECLARED EXPECTATIONS -------------------------------------
# The derivation is `psym-derivation-e1bfix3-r2.txt` (this directory), committed WITH
# this file and BEFORE the chain was detached. BEFORE = de62a2e7 (the E1b fix-round-2
# gates record on m7); the range is the fix3 code commit plus the declaration commit
# (docs/notes/data only). Class A from git; class B, the section shape and the test
# counts measured on the final pair.
#
# WALL IS INFORMATIONAL. Nothing this file prints is a timing claim.
# =========================================================================
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2e1bfix3gates-r2
G=$S/gates
# THE LINE ABOVE IS THE ROUND'S ONE DEFINITION OF ITS SCRATCH TREE, AND THE ONLY
# LITERAL NAMING IT ANYWHERE IN THE CHAIN. That property is M4 fix round 1's,
# bought with two refused chains, and it is carried here rather than re-earned:
#
#   * every gate SEGMENT names no path at all -- each one's second line is the
#     prologue `ROOT=...; S=${GATES_SCRATCH:?}; G=$S/gates`, so an unset export
#     is a refusal rather than a silent write somewhere else (M4 fix round 1's
#     run 1 died on six inner literals naming the previous round's tree);
#   * `assert_segment_scratch` below judges a segment by the VALUE its prologue
#     RESOLVES, not by grepping its prose -- M4 fix round 1's run 2 was refused
#     by its own guard firing on a COMMENT that used the word;
#   * and `chain_gates_e1bfix3_r2.sh` does not repeat this path either: it ASKS for it
#     with `--print-scratch`.
export GATES_SCRATCH="$S"

# --- THE TWO NON-RUNNING MODES, ANSWERED BEFORE ANY OUTPUT IS REDIRECTED -----
# `--print-scratch` makes this file the single source of the scratch path for
# every sibling that needs it. `--preflight-only` writes and checks the gate
# segments and stops before the first leg, so the pre-flight can be RUN and its
# output READ -- and pasted into the commit that lands it -- without spending a
# Debug suite to find out whether it passes.
PREFLIGHT_ONLY=0
case "${1:-}" in
  --print-scratch)  printf '%s\n' "$S"; exit 0 ;;
  --preflight-only) PREFLIGHT_ONLY=1; shift ;;
esac
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace
M4W=$ROOT/docs/notes/data/2026-09-m7-w2-m4-anticycling-arm/wrappers
CLASSIFIER=$M4W/classify_psym_m4.py
ASSERTS=$ART/wrappers/gate_asserts_m3b.sh


# ---- THIS ROUND'S EXPECTED COUNTS -------------------------------------------
# `psym-derivation-e1bfix3-r2.txt` §3: the previous round's 2818/2817, by name with the
# declared rename (a rename is neither an addition nor a removal).
BEFORE=${BEFORE:-de62a2e7}
EXPECT_DEBUG_TESTS=${EXPECT_DEBUG_TESTS:-2817}
EXPECT_RELEASE_TESTS=${EXPECT_RELEASE_TESTS:-2818}
# The long test is excluded from G1 and NOT RUN this round (E1BFR3 (f)); the count
# stays in the Debug = Release - long consistency check below.
EXPECT_LONG_TESTS=1
MIN_NEGATIVE_CONTROL=1
# E1BFR3 (h): THE COUNTS BY NAME. The set of test names ctest STARTS (disabled ones
# included) must EQUAL the previous round's committed suite transcript's, per config.
NAMES_BASE_DIR=$ROOT/docs/notes/data/2026-09-m7-w2-e1-equality-basis/gates-e1b-fix2
# E1BFR3 (l): THE DECLARED RENAME -- 1 renamed, 0 added, 0 removed.
RENAME_FROM=EqualityBasis.T4NearCapMultiColumnLiftMeetsItsForwardBound
RENAME_TO=EqualityBasis.T4NearCapMultiColumnLiftMeetsItsForwardEstimate
# E1BFR3 (h): LABEL GOT_LOG BASELINE -- the started-name sets are EQUAL and non-empty.
suite_names() { sed -nE 's/^ +Start +[0-9]+: (.*[^ ]) *$/\1/p' "$1" | LC_ALL=C sort; }
# E1BFR3 (l): the baseline's names with the declared rename applied, fail-closed: the
# baseline names RENAME_FROM exactly once and RENAME_TO never, else nothing is printed
# and the comparison below fails on an empty or short set.
suite_names_renamed() {
  local nf nt
  nf=$(suite_names "$1" | grep -cxF "$RENAME_FROM"); nt=$(suite_names "$1" | grep -cxF "$RENAME_TO")
  if [ "$nf" != 1 ] || [ "$nt" != 0 ]; then
    echo "RENAME REFUSED: $1 names $RENAME_FROM $nf time(s) and $RENAME_TO $nt time(s)" >&2; return 1
  fi
  suite_names "$1" | sed "s/^${RENAME_FROM}\$/${RENAME_TO}/" | LC_ALL=C sort
}
assert_names() {
  local label="$1" got="$2" base="$3" ng nb d
  [ -s "$got" ] && [ -s "$base" ] || { echo "ASSERT FAIL $label by name: a transcript is missing ($got, $base)"; return 1; }
  suite_names_renamed "$base" > /dev/null || { echo "ASSERT FAIL $label by name: the declared rename does not apply to $base"; return 1; }
  # E1BFR3r2 (n): counts, not `grep -q` (pipefail + SIGPIPE, see the header).
  local gf gt
  gf=$(suite_names "$got" | grep -cxF "$RENAME_FROM"); gt=$(suite_names "$got" | grep -cxF "$RENAME_TO")
  if [ "$gf" != 0 ] || [ "$gt" != 1 ]; then
    echo "ASSERT FAIL $label by name: this run does not carry the rename (required: $RENAME_FROM 0 times, $RENAME_TO once; measured $gf and $gt)"; return 1
  fi
  echo "ASSERT OK   $label by name: the run carries the rename ($RENAME_FROM 0 times, $RENAME_TO once)"
  ng=$(suite_names "$got" | wc -l); nb=$(suite_names_renamed "$base" | wc -l)
  d=$(LC_ALL=C comm -3 <(suite_names_renamed "$base") <(suite_names "$got"))
  if [ "$ng" -gt 0 ] && [ -z "$d" ]; then
    echo "ASSERT OK   $label by name: $ng started names == the previous round's $nb with the declared rename ($RENAME_FROM -> $RENAME_TO)"
  else
    echo "ASSERT FAIL $label by name: $ng started vs the previous round's $nb; the difference (base '<' tab-indented '>' this run):"
    printf '%s\n' "$d" | head -20
    return 1
  fi
}

# E1BFR3r2 (n): OFFLINE, no scratch tree, no log, no leg -- the full by-name check of
# LOG against the declared baseline for CFG (debug|release). rc 0 pass, rc 2 refused.
if [ "${1:-}" = --check-names ]; then
  case "${2:-}" in debug|release) : ;; *) echo "usage: --check-names debug|release LOG"; exit 2 ;; esac
  assert_names "offline ${2}" "${3:?LOG}" "$NAMES_BASE_DIR/suite-${2}.txt" && exit 0
  exit 2
fi

# ---- THIS ROUND'S EXPECTED P-SYM MOVER SHAPE, PER CONFIGURATION ----------------
# `psym-derivation-e1bfix3-r2.txt` §2. Written RELEASE/DEBUG; g5.sh splits them.
EXPECT_PSYM_DIFFERING=${EXPECT_PSYM_DIFFERING:-1/1}
EXPECT_PSYM_CLASS_A=${EXPECT_PSYM_CLASS_A:-1/1}
EXPECT_PSYM_CLASS_B=${EXPECT_PSYM_CLASS_B:-0/0}
# THE CLASS B OBJECTS BY NAME, per configuration (library and test).
EXPECT_CLASS_B_NAMES_RELEASE=${EXPECT_CLASS_B_NAMES_RELEASE-""}
EXPECT_CLASS_B_NAMES_DEBUG=${EXPECT_CLASS_B_NAMES_DEBUG-""}
# CARRIED CHANGE (5'): the configuration-asymmetric class B, DECLARED BY NAME:
# EMPTY -- the sets are equal (derivation §2).
EXPECT_PSYM_B_DEBUG_ONLY=${EXPECT_PSYM_B_DEBUG_ONLY-""}
# The C++ sources ADDED against BEFORE (derived from git; declared count): none.
EXPECT_ADDED_CXX=${EXPECT_ADDED_CXX:-0}
EXPECT_ADDED_OUTSIDE_UNIVERSE=${EXPECT_ADDED_OUTSIDE_UNIVERSE-""}
EXPECT_PSYM_ADDED=${EXPECT_PSYM_ADDED:-0}
# E1BFR3 (c): the MODIFIED C++ source whose only object is OUTSIDE the universe --
# tests/sqp/test_sqp_equality_basis_seams.cpp of hven_sqp_seam_tests (an
# HVEN_TESTING recompile target, not a PSYM_GROUPS entry; checked by value).
OUTSIDE_TARGET=hven_sqp_seam_tests
OUTSIDE_DIR=tests/CMakeFiles/$OUTSIDE_TARGET.dir
EXPECT_CLASS_A_OUTSIDE_SRCS=${EXPECT_CLASS_A_OUTSIDE_SRCS-"tests/sqp/test_sqp_equality_basis_seams.cpp"}
EXPECT_CLASS_A_OUTSIDE_UNIVERSE=$(printf '%s' "$EXPECT_CLASS_A_OUTSIDE_SRCS" | wc -w)
# E1BFR3 (d): the MODIFIED library sources whose every changed line is a comment.
EXPECT_LIB_COMMENT_ONLY_SRCS=${EXPECT_LIB_COMMENT_ONLY_SRCS-"src/drivers/sqp_solver.cpp"}
# E1BFR3 (m): the changed headers, every changed line a comment (the (d) proof), DECLARED;
# the derived CHANGED_HEADERS set must equal this.
EXPECT_COMMENT_ONLY_HEADERS=${EXPECT_COMMENT_ONLY_HEADERS-"include/hven/drivers/sqp_solver.h"}

# ---- THE DECLARED SECTION SHAPE, PER CONFIGURATION --------------------------
# E1BFR3 (e): the NON-DEBUG section movers BY NAME, per configuration, measured by
# the rehearsal. G5 asserts each is NOISE-ONLY in its configuration's classification.
EXPECT_LIB_MOVERS_RELEASE=${EXPECT_LIB_MOVERS_RELEASE-""}
EXPECT_LIB_MOVERS_DEBUG=${EXPECT_LIB_MOVERS_DEBUG-""}
EXPECT_DEBUG_ONLY_RELEASE=${EXPECT_DEBUG_ONLY_RELEASE:-0}
EXPECT_DEBUG_ONLY_DEBUG=${EXPECT_DEBUG_ONLY_DEBUG:-1}

# ---- E1BFR3 (i): THE CLASS B BY-NAME CHECK, OVER NORMALIZED NAME SETS -----------
# norm_names WORDS... -- the set: whitespace-split, blank dropped, sorted unique, one
# space between names, no trailing space. The empty list normalizes to the empty string
# however it is spelled, so empty == empty and nothing else equals it.
norm_names() { printf '%s\n' $* | awk 'NF' | LC_ALL=C sort -u | paste -sd' ' -; }
# class_b_names_check CFG CLASSIFICATION_FILE -- 0 when the MEASURED class B set of
# the file EQUALS the declared EXPECT_CLASS_B_NAMES_<CFG>, else 2. A missing file or
# one without its class B header is a refusal whatever the declared list says.
class_b_names_check() {
  local cfg="$1" file="$2" names got want
  if [ "$cfg" = Release ]; then names="$EXPECT_CLASS_B_NAMES_RELEASE"; else names="$EXPECT_CLASS_B_NAMES_DEBUG"; fi
  if [ ! -s "$file" ] || ! grep -q '^  class B -- ' "$file"; then
    echo "ASSERT FAIL class B by name $cfg: no classification with a class B section to read ($file)"; return 2
  fi
  got=$(norm_names $(psym_set "$file" B | awk '{print $NF}'))
  want=$(norm_names $names)
  if [ "$got" = "$want" ]; then
    echo "ASSERT OK   class B by name $cfg: the measured set [$got] equals the declared set [$want]"
    return 0
  fi
  echo "ASSERT FAIL class B by name $cfg: measured [$got] != declared [$want]"; return 2
}
if [ "${1:-}" = --check-class-b-names ]; then
  # Offline: no scratch tree, no log, no leg. The psym_set it reads is the library's.
  # shellcheck source=gate_asserts_m3b.sh
  . "$ASSERTS" || exit 90
  class_b_names_check "${2:?CFG}" "${3:?CLASSIFICATION_FILE}"; exit $?
fi

mkdir -p "$G" "$S/logs"
LOG=$S/logs/gates-e1bfix3-r2.log
# THE PRE-FLIGHT SPEAKS TO THE TERMINAL. A real run's transcript is the log; a
# `--preflight-only` run exists to be READ by the person about to commit it, so
# it is not redirected and it does not overwrite the previous run's log.
[ "$PREFLIGHT_ONLY" -eq 1 ] || exec >"$LOG" 2>&1
echo "GATES_START $(date -Is)"
cd "$ROOT" || exit 90
echo "=== HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l) ==="
# THE CLEAN-TREE REFUSAL IS THE RUN'S, NOT THE PRE-FLIGHT'S. A pre-flight is run
# precisely when the repair it checks is still uncommitted, so refusing on a
# dirty tree there would make the check unrunnable exactly when it is needed.
# The real chain still refuses, and says so here so the exemption is not silent.
if [ "$PREFLIGHT_ONLY" -eq 1 ]; then
  echo "=== PREFLIGHT_ONLY: the clean-tree refusal is NOT applied in this mode; the chain still applies it ==="
else
  [ -z "$(git status --porcelain)" ] || { echo "REFUSED: the tree is not clean"; exit 2; }
fi
echo "=== GATES_SCRATCH=$GATES_SCRATCH ==="
echo "=== BEFORE=$BEFORE  EXPECT_DEBUG_TESTS=$EXPECT_DEBUG_TESTS  EXPECT_RELEASE_TESTS=$EXPECT_RELEASE_TESTS ==="

# THE ROUND'S CHANGED SURFACE, DERIVED FROM GIT rather than typed: the header
# list is what the classifier checks its own declared list against, and the
# source list is what class A is keyed on.
HEAD_AT_START=$(git rev-parse HEAD)
echo "=== HEAD_AT_START=$HEAD_AT_START"
CHANGED_HEADERS=$(git diff --name-only "$BEFORE..$HEAD_AT_START" -- include/ | tr '\n' ' ')
CHANGED_SOURCES=$(git diff --name-only "$BEFORE..$HEAD_AT_START" -- src/ tests/ | tr '\n' ' ')
echo "=== CHANGED HEADERS: $CHANGED_HEADERS"
echo "=== CHANGED SOURCES: $CHANGED_SOURCES"
[ -n "$CHANGED_HEADERS" ] || { echo "REFUSED: no changed header under include/ against $BEFORE"; exit 2; }
[ -n "$CHANGED_SOURCES" ] || { echo "REFUSED: no changed source against $BEFORE"; exit 2; }

# THE MODIFIED AND THE ADDED SOURCES, DERIVED SEPARATELY. A MODIFIED source's
# object is a class A candidate; an ADDED source's object has no before-arm
# counterpart and is asserted ONLY-AFTER in G5.
MODIFIED_CXX=$(git diff --name-only --diff-filter=M "$BEFORE..$HEAD_AT_START" -- 'src/*.cpp' 'tests/*.cpp' | tr '\n' ' ')
ADDED_CXX=$(git diff --name-only --diff-filter=A "$BEFORE..$HEAD_AT_START" -- 'src/*.cpp' 'tests/*.cpp' | tr '\n' ' ')
echo "=== MODIFIED C++ SOURCES: $MODIFIED_CXX"
echo "=== ADDED C++ SOURCES: $ADDED_CXX"
n_mod=$(printf '%s' "$MODIFIED_CXX" | wc -w)
n_add=$(printf '%s' "$ADDED_CXX" | wc -w)
# VALUE-BASED PRE-FLIGHT, before any leg: the comment-only proof, the derived class
# A count, the derived ADDED count, the outside-universe sources, the declared
# library-mover list and the G2/G4 not-run declaration, against what this round
# declares.
# E1BFR3 (d): THE COMMENT-ONLY PROOF, BY VALUE. Every declared comment-only library
# source is MODIFIED in the range; the MODIFIED src/*.cpp set is EXACTLY the declared
# list; and every added or removed line of the range's diff over src/*.cpp AND over
# include/ is a `//` comment line (blank lines admitted). Any code line refuses.
n_comment_only=$(printf '%s' "$EXPECT_LIB_COMMENT_ONLY_SRCS" | wc -w)
mod_src=$(git diff --name-only --diff-filter=M "$BEFORE..$HEAD_AT_START" -- 'src/*.cpp' | LC_ALL=C sort | tr '\n' ' ')
want_src=$(printf '%s\n' $EXPECT_LIB_COMMENT_ONLY_SRCS | LC_ALL=C sort | tr '\n' ' ')
[ "$mod_src" = "$want_src" ] \
  || { echo "REFUSED: MODIFIED src/*.cpp [$mod_src] != declared comment-only library sources [$want_src]"; exit 2; }
code_lines=$(git diff -U0 "$BEFORE..$HEAD_AT_START" -- 'src/*.cpp' include/ | grep -E '^[+-]' \
  | grep -vE '^(\+\+\+|---) ' | grep -vE '^[+-][[:space:]]*(//.*)?$')
if [ -n "$code_lines" ]; then
  echo "REFUSED: a non-comment line changed in src/*.cpp or include/ -- this round declares comment-only library code:"
  printf '%s\n' "$code_lines" | head -20
  exit 2
fi
echo "=== COMMENT_ONLY_OK: every changed line of src/*.cpp and include/ in $BEFORE..$HEAD_AT_START is a // comment (sources: $mod_src)"
# E1BFR3 (m): the changed headers are EXACTLY the declared comment-only headers.
got_hdr=$(printf '%s\n' $CHANGED_HEADERS | LC_ALL=C sort | tr '\n' ' ')
want_hdr=$(printf '%s\n' $EXPECT_COMMENT_ONLY_HEADERS | LC_ALL=C sort | tr '\n' ' ')
[ "$got_hdr" = "$want_hdr" ] \
  || { echo "REFUSED: changed headers [$got_hdr] != declared comment-only headers [$want_hdr]"; exit 2; }
echo "=== COMMENT_ONLY_HEADERS_OK: [$got_hdr] (covered by the comment-only proof above)"
# E1BFR3 (l): THE DECLARED RENAME, BY VALUE. Over tests/*.cpp the range's only changed
# TEST-macro lines are exactly one removal naming RENAME_FROM and one addition naming
# RENAME_TO (suite.name split at the dot): 1 renamed, 0 added, 0 removed.
test_lines=$(git diff -U0 "$BEFORE..$HEAD_AT_START" -- 'tests/*.cpp' | grep -E '^[+-][[:space:]]*(TEST|TEST_F|TEST_P|TYPED_TEST|TYPED_TEST_P)\(' )
want_minus="-TEST(${RENAME_FROM%%.*}, ${RENAME_FROM#*.}) {"
want_plus="+TEST(${RENAME_TO%%.*}, ${RENAME_TO#*.}) {"
if [ "$(printf '%s\n' "$test_lines" | grep -c .)" != 2 ] \
   || ! printf '%s\n' "$test_lines" | grep -qxF -- "$want_minus" \
   || ! printf '%s\n' "$test_lines" | grep -qxF -- "$want_plus"; then
  echo "REFUSED: the range's TEST-macro line changes are not exactly the declared rename:"
  printf '%s\n' "$test_lines" | head -10
  exit 2
fi
for f in "$NAMES_BASE_DIR/suite-debug.txt" "$NAMES_BASE_DIR/suite-release.txt"; do
  suite_names_renamed "$f" > /dev/null || { echo "REFUSED: the declared rename does not apply to $f"; exit 2; }
done
echo "=== RENAME_OK: $RENAME_FROM -> $RENAME_TO (1 renamed, 0 added, 0 removed; both baselines name the old name once and the new one never)"
want_a_rel=${EXPECT_PSYM_CLASS_A%/*}; want_a_dbg=${EXPECT_PSYM_CLASS_A#*/}
if [ "$((n_mod - EXPECT_CLASS_A_OUTSIDE_UNIVERSE - n_comment_only))" != "$want_a_rel" ] || [ "$want_a_rel" != "$want_a_dbg" ]; then
  echo "REFUSED: $n_mod modified .cpp (less $EXPECT_CLASS_A_OUTSIDE_UNIVERSE outside the universe, less $n_comment_only comment-only library), but class A is DECLARED $EXPECT_PSYM_CLASS_A"
  exit 2
fi
if [ "$n_add" != "$EXPECT_ADDED_CXX" ]; then
  echo "REFUSED: $n_add added .cpp derived, but ADDED is DECLARED $EXPECT_ADDED_CXX"
  exit 2
fi
# CARRIED CHANGE (4'): every declared outside source is MODIFIED, is a source of the
# outside target in tests/CMakeLists.txt, is named EXACTLY ONCE in the build
# files (so its only object is the outside one), and that target is NOT a
# P-SYM group. Value checks, not prose.
outside_block=$(awk -v t="$OUTSIDE_TARGET" '$0 ~ "add_executable\\("t"([[:space:]]|$)" {f=1} f {print} f && /\)/ {exit}' tests/CMakeLists.txt)
[ -n "$outside_block" ] || { echo "REFUSED: no add_executable($OUTSIDE_TARGET in tests/CMakeLists.txt"; exit 2; }
for f in $EXPECT_CLASS_A_OUTSIDE_SRCS; do
  printf '%s\n' $MODIFIED_CXX | grep -qxF "$f" || { echo "REFUSED: declared outside source $f is not MODIFIED against $BEFORE"; exit 2; }
  printf '%s\n' "$outside_block" | grep -qF "${f#tests/}" || { echo "REFUSED: $f is not a source of $OUTSIDE_TARGET in tests/CMakeLists.txt"; exit 2; }
  n_all=$(git grep -hF "$(basename "$f")" -- 'CMakeLists.txt' '*/CMakeLists.txt' '*.cmake' | wc -l)
  n_blk=$(printf '%s\n' "$outside_block" | grep -cF "$(basename "$f")")
  [ "$n_all" = 1 ] && [ "$n_blk" = 1 ] \
    || { echo "REFUSED: $f is named $n_all time(s) in the build files, $n_blk in $OUTSIDE_TARGET's block -- its object may be captured elsewhere"; exit 2; }
done
if sed -n '/^PSYM_GROUPS=(/,/^)/p' scripts/psym_compare.sh | grep -qF "$OUTSIDE_DIR"; then
  echo "REFUSED: $OUTSIDE_DIR IS a P-SYM group -- its objects would be captured"; exit 2
fi
sed -n '/^PSYM_GROUPS=(/,/^)/p' scripts/psym_compare.sh | grep -q 'CMakeFiles' \
  || { echo "REFUSED: could not read PSYM_GROUPS from scripts/psym_compare.sh"; exit 2; }
for f in $EXPECT_ADDED_OUTSIDE_UNIVERSE; do
  echo "REFUSED: this round declares no ADDED source outside the universe, got $f"; exit 2
done
ADDED_IN_UNIVERSE=$ADDED_CXX
n_add_u=$(printf '%s' "$ADDED_IN_UNIVERSE" | wc -w)
echo "=== MODIFIED OUTSIDE THE P-SYM UNIVERSE (declared, checked): $EXPECT_CLASS_A_OUTSIDE_SRCS -> $OUTSIDE_DIR"
echo "=== ADDED INSIDE THE P-SYM UNIVERSE (derived): $ADDED_IN_UNIVERSE"
if [ "$n_add_u" != "$EXPECT_PSYM_ADDED" ]; then
  echo "REFUSED: $n_add_u added .cpp inside the universe, but the universe's ADDED is DECLARED $EXPECT_PSYM_ADDED"
  exit 2
fi
EXPECT_LIB_ADDED=$(git diff --name-only --diff-filter=A "$BEFORE..$HEAD_AT_START" -- 'src/*.cpp' \
  | sed 's|^|CMakeFiles/hven.dir/|; s|$|.o|' | LC_ALL=C sort | tr '\n' ' ')
# E1BFR3 (e): the declared non-debug movers, SORTED as g5 sorts what it measures,
# each a library object with a source on disk.
EXPECT_LIB_MOVERS_RELEASE=$(printf '%s\n' $EXPECT_LIB_MOVERS_RELEASE | grep . | LC_ALL=C sort -u | tr '\n' ' ')
EXPECT_LIB_MOVERS_DEBUG=$(printf '%s\n' $EXPECT_LIB_MOVERS_DEBUG | grep . | LC_ALL=C sort -u | tr '\n' ' ')
echo "=== DECLARED NON-DEBUG MOVERS Release (by name, each must be NOISE-ONLY): $EXPECT_LIB_MOVERS_RELEASE"
echo "=== DECLARED NON-DEBUG MOVERS Debug   (by name, each must be NOISE-ONLY): $EXPECT_LIB_MOVERS_DEBUG"
echo "=== DECLARED ADDED LIBRARY OBJECTS (derived): $EXPECT_LIB_ADDED"
for o in $EXPECT_LIB_MOVERS_RELEASE $EXPECT_LIB_MOVERS_DEBUG; do
  case "$o" in CMakeFiles/hven.dir/src/*.o) : ;; *) echo "REFUSED: declared mover $o is not a library object"; exit 2 ;; esac
  src=${o#CMakeFiles/hven.dir/}; src=${src%.o}
  [ -f "$src" ] || { echo "REFUSED: declared mover $o has no source $src"; exit 2; }
done
# No library object may be a declared class B this round: the library moves by
# noise at most (E1BFR3 (d)).
for o in $EXPECT_CLASS_B_NAMES_RELEASE $EXPECT_CLASS_B_NAMES_DEBUG; do
  case "$o" in CMakeFiles/hven.dir/*) echo "REFUSED: library object $o declared class B; this round declares none"; exit 2 ;; esac
done
# THE DECLARED COUNTS ARE CONSISTENT WITH EACH OTHER, by value: per
# configuration DIFFERING = class A + class B; the Debug suite is the Release
# suite less the long test(s) G2 runs alone; the DEBUG_ONLY counts are integers.
for cfg in rel dbg; do
  if [ $cfg = rel ]; then d=${EXPECT_PSYM_DIFFERING%/*}; a=${EXPECT_PSYM_CLASS_A%/*}; b=${EXPECT_PSYM_CLASS_B%/*}
  else d=${EXPECT_PSYM_DIFFERING#*/}; a=${EXPECT_PSYM_CLASS_A#*/}; b=${EXPECT_PSYM_CLASS_B#*/}; fi
  [ "$d" = "$((a + b))" ] || { echo "REFUSED: $cfg DIFFERING $d != class A $a + class B $b"; exit 2; }
done
[ "$EXPECT_DEBUG_TESTS" = "$((EXPECT_RELEASE_TESTS - EXPECT_LONG_TESTS))" ] \
  || { echo "REFUSED: Debug $EXPECT_DEBUG_TESTS != Release $EXPECT_RELEASE_TESTS - long $EXPECT_LONG_TESTS"; exit 2; }
case "$EXPECT_DEBUG_ONLY_RELEASE$EXPECT_DEBUG_ONLY_DEBUG" in *[!0-9]*|"") echo "REFUSED: DEBUG_ONLY counts not integers"; exit 2 ;; esac
# E1BFR3 (f): G2 and G4 are NOT RUN, and that is checked by value rather than
# asserted: the comment-only proof above covers every library source and header, and
# the long test's own source is not in the range.
LONG_TEST_SRC=tests/sqp/test_continuation.cpp
grep -qF 'ProbeBudgetBoundsAFailingProposal' "$LONG_TEST_SRC" \
  || { echo "REFUSED: the long test is not in $LONG_TEST_SRC"; exit 2; }
if ! git diff --quiet "$BEFORE..$HEAD_AT_START" -- "$LONG_TEST_SRC"; then
  echo "REFUSED: $LONG_TEST_SRC changed -- G2 (the long test) must run"; exit 2
fi
for f in "$NAMES_BASE_DIR/suite-debug.txt" "$NAMES_BASE_DIR/suite-release.txt"; do
  [ -s "$f" ] || { echo "REFUSED: the by-name baseline $f is missing"; exit 2; }
done
echo "=== BY-NAME BASELINES: $NAMES_BASE_DIR/suite-{debug,release}.txt ($(suite_names "$NAMES_BASE_DIR/suite-debug.txt" | wc -l) / $(suite_names "$NAMES_BASE_DIR/suite-release.txt" | wc -l) started names)"
echo "=== G2 NOT RUN (declared): no library code, no header code and not $LONG_TEST_SRC changed"
echo "=== G4 NOT RUN (declared): no numeric code changed (comment-only proof above); the replay's inputs are the previous round's"
# E2FR1 (d): NO DECLARED CLASS B OBJECT MAY HAVE A SOURCE MODIFIED IN THE RANGE,
# library or test, and each list's length is the declared class B count. An object
# path maps to its source as <dir before CMakeFiles>/<path after <target>.dir/> less
# ".o", normalized (so tests/CMakeFiles/x.dir/__/src/a.cpp.o -> src/a.cpp).
obj_source() {
  local o="$1" pre post
  pre=${o%%CMakeFiles/*}; post=${o#*CMakeFiles/*.dir/}; post=${post%.o}
  realpath -m --relative-to="$ROOT" "$ROOT/$pre$post"
}
for cfg in rel dbg; do
  if [ $cfg = rel ]; then names="$EXPECT_CLASS_B_NAMES_RELEASE"; want=${EXPECT_PSYM_CLASS_B%/*}
  else names="$EXPECT_CLASS_B_NAMES_DEBUG"; want=${EXPECT_PSYM_CLASS_B#*/}; fi
  n=$(printf '%s\n' $names | grep -c .)
  [ "$n" = "$want" ] || { echo "REFUSED: $cfg class B names list has $n objects, class B is DECLARED $want"; exit 2; }
  for o in $names; do
    case "$o" in
      */Unity/*)
        # A generated unity TU has no source of its own: its constituents are the
        # target's sources under the object's source directory, and none may be
        # modified in the range.
        src=${o%%CMakeFiles/*}; src=${src%/}
        [ -d "$src" ] || { echo "REFUSED: $cfg class B unity $o: no source directory $src"; exit 2; } ;;
      *)
        src=$(obj_source "$o")
        [ -f "$src" ] || { echo "REFUSED: $cfg class B $o maps to $src, which does not exist"; exit 2; } ;;
    esac
    if git diff --quiet "$BEFORE..$HEAD_AT_START" -- "$src"; then :; else
      echo "REFUSED: $cfg class B $o has its source $src MODIFIED in $BEFORE..$HEAD_AT_START (it is class A)"; exit 2
    fi
  done
done
echo "=== CLASS_B_NAMES_OK: Release $(printf '%s\n' $EXPECT_CLASS_B_NAMES_RELEASE | grep -c .) / Debug $(printf '%s\n' $EXPECT_CLASS_B_NAMES_DEBUG | grep -c .) declared objects, none with a source modified in the range"
echo "=== PREFLIGHT_VALUE_OK: class A $n_mod - $EXPECT_CLASS_A_OUTSIDE_UNIVERSE outside == declared $EXPECT_PSYM_CLASS_A; ADDED $n_add == declared $EXPECT_ADDED_CXX ($n_add_u inside the universe == declared $EXPECT_PSYM_ADDED); comment-only library sources $n_comment_only; no library class B; G2/G4 not run (declared)"

# shellcheck source=gate_asserts_m3b.sh
. "$ASSERTS" || exit 90

# A GATE SEGMENT IS JUDGED BY THE DIRECTORY IT RESOLVES, NOT BY ITS PROSE.
#
# Every segment below is a single-quoted heredoc whose SECOND line is its whole
# prologue: `ROOT=...; S=${GATES_SCRATCH:?}; G=$S/gates`. This evaluates THAT
# LINE in a subshell, with `GATES_SCRATCH` set exactly as the segment will see
# it, and asserts that the `$S` and `$G` the segment will really write to are
# this chain's own. It is a POSITIVE assertion, so a segment that names no path
# at all and still resolves somewhere else -- an unset export, a typo in it --
# fails too. Run 1's guard could only say "I found nothing bad".
#
# The residual textual check keeps the part that is genuinely about paths: a
# literal `.scratch/` in the segment's CODE, plus any re-assignment of S or G
# after the prologue. COMMENTS ARE STRIPPED BEFORE EITHER LOOKS. Run 2 of this
# chain was refused because a comment in `g5.sh` contained the WORD `scratch`;
# a sentence about a path is not a path, and a grep for a word cannot tell them
# apart.
assert_segment_scratch() {
  local f="$1" line probe got_s got_g code hits reassign rc=0
  if [ ! -s "$f" ]; then
    echo "SEGMENT REFUSED $f: was not written, or is empty"
    return 1
  fi
  line=$(sed -n '2p' "$f")
  case "$line" in
    *'S=${GATES_SCRATCH'*) : ;;
    *) echo "SEGMENT REFUSED $f: line 2 is not the GATES_SCRATCH prologue: $line"
       return 1 ;;
  esac
  # THE VALUES THE SEGMENT WILL ACTUALLY USE, obtained by running its own line.
  probe=$(GATES_SCRATCH="$GATES_SCRATCH" bash -c "$line"'; printf "%s\n%s\n" "$S" "$G"' 2>&1) || {
    echo "SEGMENT REFUSED $f: its prologue would not evaluate: $probe"
    return 1
  }
  got_s=$(printf '%s\n' "$probe" | sed -n '1p')
  got_g=$(printf '%s\n' "$probe" | sed -n '2p')
  if [ "$got_s" != "$GATES_SCRATCH" ]; then
    echo "SEGMENT REFUSED $f: resolves S='$got_s'; this chain's is '$GATES_SCRATCH'"
    rc=1
  fi
  if [ "$got_g" != "$GATES_SCRATCH/gates" ]; then
    echo "SEGMENT REFUSED $f: resolves G='$got_g'; this chain's is '$GATES_SCRATCH/gates'"
    rc=1
  fi
  code=$(sed -e 's/^[[:space:]]*#.*$//' "$f")
  hits=$(printf '%s\n' "$code" | grep -n '\.scratch/')
  if [ -n "$hits" ]; then
    echo "SEGMENT REFUSED $f: its CODE names a .scratch/ path of its own:"
    echo "$hits"
    rc=1
  fi
  reassign=$(printf '%s\n' "$code" | sed '1,2d' | grep -nE '(^|[;[:space:]])[SG]=')
  if [ -n "$reassign" ]; then
    echo "SEGMENT REFUSED $f: it re-assigns S or G after its prologue:"
    echo "$reassign"
    rc=1
  fi
  [ $rc -eq 0 ] && echo "SEGMENT_OK $f: resolves S=$got_s  G=$got_g"
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

# ===========================================================================
# ALL SEGMENTS ARE WRITTEN FIRST (FOUR this round), AND ALL ARE CHECKED, BEFORE ANY LEG
# RUNS. Run 2 of this chain wrote and checked each segment immediately above
# its own leg, so it spent 26 minutes -- a Debug suite, a 673 s test, a Release
# suite and the replay -- before discovering that `g5.sh` would be refused. A
# pre-flight that can only be reached by paying for the legs in front of it is
# not a pre-flight. Nothing below writes to the build trees; `emit_segments`
# only writes the six `.sh` files.
# ===========================================================================
emit_segments() {
# --- G1: Debug -------------------------------------------------------------
cat > "$S/g1.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=${GATES_SCRATCH:?}; G=$S/gates
rm -rf "$S/gates-dbg"
cmake -S "$ROOT" -B "$S/gates-dbg" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON > "$G/configure-debug.log" 2>&1 || exit 91
cmake --build "$S/gates-dbg" -j "$(nproc)" > "$G/build-debug.log" 2>&1 || exit 91
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest --test-dir "$S/gates-dbg" -j4 --no-tests=error \
  -E "Continuation.ProbeBudgetBoundsAFailingProposal" --output-on-failure > "$G/suite-debug.log" 2>&1
rc=$?; tail -6 "$G/suite-debug.log"; exit $rc
INNER

# --- G2: NOT WRITTEN (E1BFR3 (f): the long test is not run this round) -------

# --- G3: Release ----------------------------------------------------------
cat > "$S/g3.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=${GATES_SCRATCH:?}; G=$S/gates
rm -rf "$S/gates-rel"
cmake -S "$ROOT" -B "$S/gates-rel" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON \
  > "$G/configure-release.log" 2>&1 || exit 91
cmake --build "$S/gates-rel" -j "$(nproc)" > "$G/build-release.log" 2>&1 || exit 91
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest --test-dir "$S/gates-rel" -j4 --no-tests=error \
  --output-on-failure > "$G/suite-release.log" 2>&1
rc=$?; tail -6 "$G/suite-release.log"
exit $rc
INNER

# --- G4: NOT WRITTEN (E1BFR3 (f): no numeric code changed; the replay is not run) --

# --- G5: P-SYM, both configs ---------------------------------------------
cat > "$S/g5.sh" <<'INNER'
set -uo pipefail
ROOT=/home/ghecht/Projects/hven; S=${GATES_SCRATCH:?}; G=$S/gates
BD=$S/psym-build; OUT=$G/psym
# The section digest lives beside the checker in the scratch copy, for the same
# reason the classifier does: the `before` arm below checks out $BEFORE, at
# which this round's wrappers do not exist.
export DIGEST="$S/section_digest_fix2.py"
BEFORE="$1"; CHANGED_SOURCES="$2"; CHANGED_HEADERS="$3"; MIN_CONTROL="$4"
WANT_DIFFERING_RD="$5"; WANT_A_RD="$6"; WANT_B_RD="$7"
WANT_DEBUG_ONLY_RELEASE="$8"; WANT_DEBUG_ONLY_DEBUG="$9"
WANT_MOVERS_RELEASE="${10}"; WANT_MOVERS_DEBUG="${11}"
ADDED_CXX="${12}"; WANT_LIB_ADDED="${13}"; OUTSIDE_DIR="${14}"; OUTSIDE_SRCS="${15}"
WANT_B_DEBUG_ONLY="${16}"; COMMENT_ONLY_SRCS="${17}"
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace
# CLEARED, NOT MERELY CREATED (fix round 2). `psym_compare.sh capture` truncates
# its manifest before it may refuse an incomplete object universe, so a snapshot
# directory that survives from an earlier attempt is a live route to a narrowed
# comparison. Nothing under $OUT outlives this segment.
rm -rf "$OUT"
mkdir -p "$OUT"
HEAD_SHA=$(git -C "$ROOT" rev-parse HEAD)
BRANCH=$(git -C "$ROOT" rev-parse --abbrev-ref HEAD)
[ -z "$(git -C "$ROOT" status --porcelain)" ] || { echo "REFUSED: tree not clean"; exit 2; }
# THE CLASSIFIER AND THE ASSERTS ARE COPIED OUT OF THE TREE FIRST: the `before`
# arm below checks out $BEFORE, at which this round's wrappers do not exist.
cp "$ROOT/docs/notes/data/2026-09-m7-w2-m4-anticycling-arm/wrappers/classify_psym_m4.py" \
   "$S/classify_psym.py" || exit 93
cp "$ART/wrappers/gate_asserts_m3b.sh" "$S/gate_asserts.sh" || exit 93
cp "$ART/wrappers/check_sections_fix2.sh" "$S/check_sections.sh" || exit 93
cp "$ART/wrappers/section_digest_fix2.py" "$S/section_digest_fix2.py" || exit 93
export DIGEST="$S/section_digest_fix2.py"
. "$S/gate_asserts.sh" || exit 93
rc=0
for CFG in Release Debug; do
  if [ "$CFG" = "Release" ]; then
    WANT_DIFFERING=${WANT_DIFFERING_RD%/*}; WANT_A=${WANT_A_RD%/*}; WANT_B=${WANT_B_RD%/*}
    WANT_MOVERS="$WANT_MOVERS_RELEASE"
  else
    WANT_DIFFERING=${WANT_DIFFERING_RD#*/}; WANT_A=${WANT_A_RD#*/}; WANT_B=${WANT_B_RD#*/}
    WANT_MOVERS="$WANT_MOVERS_DEBUG"
  fi
  echo "=== P-SYM $CFG  after=$HEAD_SHA  before=$BEFORE  declared differing=$WANT_DIFFERING A=$WANT_A B=$WANT_B"
  rm -rf "$BD"
  cmake -S "$ROOT" -B "$BD" -G Ninja -DCMAKE_BUILD_TYPE="$CFG" \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON \
    > "$OUT/configure-$CFG.log" 2>&1 || exit 91
  CCACHE_DISABLE=1 cmake --build "$BD" -j "$(nproc)" > "$OUT/build-after-$CFG.log" 2>&1 || exit 91
  rm -rf "$OUT/after-$CFG"
  "$ROOT/scripts/psym_compare.sh" capture "$OUT/after-$CFG" "$BD" > "$OUT/capture-after-$CFG.log" 2>&1
  cap_after_rc=$?
  # THE PRODUCTION LIBRARY'S OBJECTS, KEPT (fix round 1). P-SYM compares its own
  # captured digests; the fail-closed NON-DEBUG SECTION check needs the objects
  # themselves, and they only exist while this build tree does.
  rm -rf "$OUT/after-obj-$CFG"; mkdir -p "$OUT/after-obj-$CFG"
  (cd "$BD" && find CMakeFiles/hven.dir -name '*.o' | LC_ALL=C sort) > "$OUT/libobjs-$CFG.txt"
  while IFS= read -r o; do
      mkdir -p "$OUT/after-obj-$CFG/$(dirname "$o")"
      cp "$BD/$o" "$OUT/after-obj-$CFG/$o"
  done < "$OUT/libobjs-$CFG.txt"
  restore() { git -C "$ROOT" checkout -q "$BRANCH" 2>/dev/null || git -C "$ROOT" checkout -q "$HEAD_SHA"; }
  trap restore EXIT
  git -C "$ROOT" checkout -q --detach "$BEFORE" || exit 92
  # E1 CHANGE (1): re-configure at BEFORE, then remove every output the BEFORE
  # manifest no longer names, so no after-arm object survives into the capture.
  cmake -S "$ROOT" -B "$BD" > "$OUT/configure-before-$CFG.log" 2>&1 || { restore; exit 91; }
  ninja -C "$BD" -t cleandead > "$OUT/cleandead-$CFG.log" 2>&1 || { restore; exit 91; }
  echo "CLEANDEAD_$CFG: $(grep -c 'Remove' "$OUT/cleandead-$CFG.log") output(s) removed"
  CCACHE_DISABLE=1 cmake --build "$BD" -j "$(nproc)" > "$OUT/build-before-$CFG.log" 2>&1 || { restore; exit 91; }
  rm -rf "$OUT/before-$CFG"
  "$ROOT/scripts/psym_compare.sh" capture "$OUT/before-$CFG" "$BD" > "$OUT/capture-before-$CFG.log" 2>&1
  cap_before_rc=$?
  rm -rf "$OUT/before-obj-$CFG"; mkdir -p "$OUT/before-obj-$CFG"
  while IFS= read -r o; do
      mkdir -p "$OUT/before-obj-$CFG/$(dirname "$o")"
      [ -f "$BD/$o" ] && cp "$BD/$o" "$OUT/before-obj-$CFG/$o"
  done < "$OUT/libobjs-$CFG.txt"
  git -C "$ROOT" checkout -q "$BRANCH"
  trap - EXIT
  # BOTH CAPTURE RETURN CODES ENTER THE GATE (fix round 2): the first round
  # discarded them, so a capture that refused an incomplete object universe was
  # invisible and its short manifest went on to `compare`.
  assert_capture "after-$CFG" "$cap_after_rc" "$OUT/after-$CFG" || rc=1
  assert_capture "before-$CFG" "$cap_before_rc" "$OUT/before-$CFG" || rc=1
  # E1 CHANGE (2): after = before + ADDED, and P-SYM's ONLY-AFTER set must be
  # exactly the ADDED sources' objects (by basename).
  nb=$(wc -l < "$OUT/before-$CFG/.psym-manifest"); na=$(wc -l < "$OUT/after-$CFG/.psym-manifest")
  want_added=$(for f in $ADDED_CXX; do basename "$f"; done | sed 's|$|.o|' | LC_ALL=C sort | tr '\n' ' ')
  n_added=$(printf '%s' "$want_added" | wc -w)
  assert_eq "capture pair $CFG after = before + ADDED" "$na" "$((nb + n_added))" || rc=1
  # E2 CHANGE (4'): the MODIFIED sources outside the universe. The after build
  # log must show their objects COMPILED, and the after capture must name NONE
  # of that target's objects. The object path is the target directory plus the
  # source's path below its CMake source directory (the first path component).
  for f in $OUTSIDE_SRCS; do
    if grep -qF "$OUTSIDE_DIR/${f#*/}.o" "$OUT/build-after-$CFG.log"; then
      echo "ASSERT OK   outside-universe $CFG: $OUTSIDE_DIR/${f#*/}.o was BUILT in the after arm"
    else
      echo "ASSERT FAIL outside-universe $CFG: the after build did not compile $OUTSIDE_DIR/${f#*/}.o"; rc=1
    fi
  done
  n_in=$(grep -c "^$OUTSIDE_DIR/" "$OUT/after-$CFG/.psym-manifest")
  assert_eq "outside-universe $CFG: objects of $OUTSIDE_DIR in the after manifest" "$n_in" 0 || rc=1
  echo "tree restored to $(git -C "$ROOT" rev-parse --abbrev-ref HEAD) $(git -C "$ROOT" rev-parse --short HEAD)"
  "$ROOT/scripts/psym_compare.sh" compare "$OUT/before-$CFG" "$OUT/after-$CFG" > "$OUT/compare-$CFG.txt" 2>&1
  cmp_rc=$?
  echo "PSYM_COMPARE_RC_$CFG=$cmp_rc (1 is EXPECTED; the gate is the classification)"
  got_only_after=$(sed -n 's/^ONLY-AFTER  *\([^ ]*\).*/\1/p' "$OUT/compare-$CFG.txt" | xargs -r -n1 basename | LC_ALL=C sort | tr '\n' ' ')
  if [ "$got_only_after" = "$want_added" ]; then
    echo "ASSERT OK   ONLY-AFTER $CFG: [$got_only_after] == the ADDED sources' objects"
  else
    echo "ASSERT FAIL ONLY-AFTER $CFG: [$got_only_after] != ADDED [$want_added]"
    rc=1
  fi
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

  # ---- G5b: THE FAIL-CLOSED NON-DEBUG SECTION CHECK (fix round 1) ----------
  # The review's Important 1. This is the arm that carries the "nothing but
  # debug information changed" claim, and it carries it by FAILING on any
  # non-debug byte rather than by passing on the bytes three other tools
  # happened to inspect. It runs over ALL library objects in BOTH
  # configurations, not only over the Debug objects that differ.
  # E1 CHANGE (3): an ADDED library object is asserted present after and absent
  # before, and the section check runs over the objects present on both sides.
  got_lib_added=""
  while IFS= read -r o; do
    [ -f "$OUT/before-obj-$CFG/$o" ] || got_lib_added="$got_lib_added$o "
  done < "$OUT/libobjs-$CFG.txt"
  if [ "$got_lib_added" = "$WANT_LIB_ADDED" ]; then
    echo "ASSERT OK   library ADDED $CFG: [$got_lib_added] present after, absent before"
  else
    echo "ASSERT FAIL library ADDED $CFG: [$got_lib_added] != derived [$WANT_LIB_ADDED]"
    rc=1
  fi
  grep -vxF -f <(printf '%s\n' $WANT_LIB_ADDED) "$OUT/libobjs-$CFG.txt" > "$OUT/libobjs-both-$CFG.txt"
  bash "$S/check_sections.sh" "$CFG" "$OUT/before-obj-$CFG" "$OUT/after-obj-$CFG" \
    "$OUT/libobjs-both-$CFG.txt" > "$OUT/sections-$CFG.txt" 2>&1
  sec_rc=$?
  echo "SECTIONS_RC_$CFG=$sec_rc"
  grep -E "^(SECTIONS_|NON-DEBUG-MOVER|DEBUG-ONLY|REFUSED)" "$OUT/sections-$CFG.txt" | head -40
  # G5b: the NON-DEBUG-MOVER set must EQUAL the declared set, with no
  # refusal. The checker is M5's, unchanged; it exits 1 whenever a mover exists.
  got_movers=$(sed -n 's/^NON-DEBUG-MOVER  *\([^ ]*\).*/\1/p' "$OUT/sections-$CFG.txt" | LC_ALL=C sort | tr '\n' ' ')
  got_refused=$(sed -n 's/^SECTIONS_'"$CFG"'_REFUSED=//p' "$OUT/sections-$CFG.txt")
  echo "NON_DEBUG_MOVERS_GOT_$CFG=[$got_movers]  DECLARED=[$WANT_MOVERS]  REFUSED=$got_refused"
  if [ -z "$WANT_MOVERS" ]; then want_sec_rc=0; else want_sec_rc=1; fi
  if [ "$sec_rc" -ne "$want_sec_rc" ] || [ "$got_refused" != "0" ] || [ "$got_movers" != "$WANT_MOVERS" ]; then
    echo "ASSERT FAIL sections $CFG: rc=$sec_rc (want $want_sec_rc), refused='$got_refused', movers [$got_movers] != declared [$WANT_MOVERS]"
    rc=1
  else
    echo "ASSERT OK   sections $CFG: the non-debug movers are EXACTLY the declared set, nothing refused"
  fi
  # E1BFR3 (d)/(e): THE PRODUCTION LIBRARY MOVES BY NOISE AT MOST. No library object
  # is a P-SYM mover (a DIFFERS or RELOCATIONS line), and every library object the
  # section check reports as a non-debug mover is NOISE-ONLY in this configuration's
  # classification (__LINE__-class disassembly differences, nothing else).
  lib_psym_movers=$(grep -E '^(DIFFERS|RELOCATIONS)[[:space:]]+CMakeFiles/hven\.dir/' "$OUT/compare-$CFG.txt" \
    | awk '{print $2}' | LC_ALL=C sort -u | tr '\n' ' ')
  if [ -z "$lib_psym_movers" ]; then
    echo "ASSERT OK   library $CFG: no production-library object is a P-SYM mover (DIFFERS/RELOCATIONS)"
  else
    echo "ASSERT FAIL library $CFG: production-library P-SYM movers [$lib_psym_movers]"
    rc=1
  fi
  noise_lib=$(sed -n 's/^      LIB   //p' "$OUT/classification-$CFG.txt" | LC_ALL=C sort)
  for o in $got_movers; do
    if printf '%s\n' "$noise_lib" | grep -qxF "$o"; then
      echo "ASSERT OK   library $CFG: non-debug mover $o is NOISE-ONLY"
    else
      echo "ASSERT FAIL library $CFG: non-debug mover $o is NOT NOISE-ONLY in classification-$CFG.txt"
      rc=1
    fi
  done
  # And every DEBUG-ONLY library object is a comment-only source's own object OR an
  # includer of a changed (comment-only) header, by the build's own Ninja deps record
  # (E1BFR3 (m)): a header's comment shift reaches its includers' DWARF declaration
  # lines and nothing else. $BD holds the BEFORE build here; the changed headers exist
  # there too.
  for o in $(sed -n 's/^DEBUG-ONLY  *\([^ ]*\).*/\1/p' "$OUT/sections-$CFG.txt"); do
    f=${o#CMakeFiles/hven.dir/}; f=${f%.o}
    if printf '%s\n' $COMMENT_ONLY_SRCS | grep -qxF "$f"; then
      echo "ASSERT OK   library $CFG: DEBUG-ONLY $o is a comment-only source's object"
      continue
    fi
    deps=$(ninja -C "$BD" -t deps "$o" 2>/dev/null)
    hit=""
    for h in $CHANGED_HEADERS; do
      printf '%s\n' "$deps" | sed 's/^[[:space:]]*//' | grep -qxF "$ROOT/$h" && hit="$hit$h "
    done
    if [ -n "$hit" ]; then
      echo "ASSERT OK   library $CFG: DEBUG-ONLY $o includes the changed header(s) [$hit] (ninja deps)"
    else
      echo "ASSERT FAIL library $CFG: DEBUG-ONLY $o is neither a comment-only source's object nor an includer of a changed header"; rc=1
    fi
  done
  for f in $COMMENT_ONLY_SRCS; do
    o=CMakeFiles/hven.dir/$f.o
    if printf '%s\n' "$noise_lib" | grep -qxF "$o"; then st=NOISE-ONLY
    elif grep -qE "^(DIFFERS|RELOCATIONS)[[:space:]]+$o([[:space:]]|\$)" "$OUT/compare-$CFG.txt"; then st=MOVER
    else st=BYTE-IDENTICAL-OR-UNREPORTED; fi
    echo "INFO comment-only source $CFG: $o -> $st"
  done
  # AND THE DECLARED DEBUG-ONLY SHAPE, per configuration, from the runner's
  # header: Release declares ZERO objects may differ at all; Debug declares
  # exactly the ONE TU whose own .cpp source lines moved. An object beyond the
  # declared count is a shape that was not declared, and is re-derived and
  # declared again, never discovered.
  got_dbgonly=$(sed -n 's/^SECTIONS_'"$CFG"'_DEBUG_ONLY=//p' "$OUT/sections-$CFG.txt")
  want_dbgonly=$([ "$CFG" = "Debug" ] && echo "$WANT_DEBUG_ONLY_DEBUG" || echo "$WANT_DEBUG_ONLY_RELEASE")
  if [ "$got_dbgonly" != "$want_dbgonly" ]; then
    echo "ASSERT FAIL sections $CFG: DEBUG_ONLY is '$got_dbgonly' and this round DECLARED '$want_dbgonly'"
    rc=1
  else
    echo "ASSERT OK   sections $CFG: DEBUG_ONLY=$got_dbgonly, as declared"
  fi
done
# E2 CHANGE (5'): THE DECLARED DEBUG-ONLY CLASS B. Fail-closed on three counts
# before the shared assert sees anything: (a) Debug B minus Release B is EXACTLY
# the declared list and Release B minus Debug B is EMPTY; (b) every listed
# object is NOISE-ONLY in the Release classification (its Release bytes moved
# only by __LINE__-class noise); (c) the Debug copy handed to the shared assert
# differs from the original by EXACTLY the listed class B lines. With an empty
# list the shared assert runs over the original, as in every earlier round.
XDBG="$OUT/classification-Debug.txt"
if [ -n "$WANT_B_DEBUG_ONLY" ]; then
  psym_set "$OUT/classification-Release.txt" B > "$OUT/classB-Release.txt"
  psym_set "$OUT/classification-Debug.txt" B > "$OUT/classB-Debug.txt"
  got_dbg_only=$(LC_ALL=C comm -13 "$OUT/classB-Release.txt" "$OUT/classB-Debug.txt" | tr '\n' ' ')
  got_rel_only=$(LC_ALL=C comm -23 "$OUT/classB-Release.txt" "$OUT/classB-Debug.txt" | tr '\n' ' ')
  want_dbg_only=$(printf '%s\n' $WANT_B_DEBUG_ONLY | LC_ALL=C sort | tr '\n' ' ')
  if [ "$got_dbg_only" = "$want_dbg_only" ] && [ -z "$got_rel_only" ]; then
    echo "ASSERT OK   class B asymmetry: Debug-only [$got_dbg_only] == declared; Release-only []"
  else
    echo "ASSERT FAIL class B asymmetry: Debug-only [$got_dbg_only] (declared [$want_dbg_only]), Release-only [$got_rel_only]"
    rc=1
  fi
  noise_rel=$(sed -n '/^  NOISE-ONLY -- /,/^$/p' "$OUT/classification-Release.txt")
  for o in $WANT_B_DEBUG_ONLY; do
    if printf '%s\n' "$noise_rel" | grep -qE "^      (LIB|test) +$o\$"; then
      echo "ASSERT OK   class B asymmetry: $o is NOISE-ONLY in Release"
    else
      echo "ASSERT FAIL class B asymmetry: $o is NOT NOISE-ONLY in Release"; rc=1
    fi
  done
  XDBG="$OUT/classification-Debug.xcfg.txt"
  awk -v drop="$WANT_B_DEBUG_ONLY" '
    BEGIN { n = split(drop, d, " "); for (i = 1; i <= n; i++) keep[d[i]] = 1 }
    /^  class B -- / { sec = "B"; print; next }
    /^  [^ ]/        { sec = "" }
    { s = $0; sub(/^ +/, "", s)
      if (sec == "B" && /^      [^ ]/ && (s in keep)) next
      print }
  ' "$OUT/classification-Debug.txt" > "$XDBG"
  removed=$(diff "$OUT/classification-Debug.txt" "$XDBG" | sed -n 's/^< *//p' | LC_ALL=C sort | tr '\n' ' ')
  if [ "$removed" = "$want_dbg_only" ]; then
    echo "ASSERT OK   class B asymmetry: the cross-config copy removes exactly [$removed]"
  else
    echo "ASSERT FAIL class B asymmetry: the cross-config copy removed [$removed], declared [$want_dbg_only]"; rc=1
  fi
fi
# shellcheck disable=SC2086
assert_psym_cross_config "$OUT/classification-Release.txt" "$XDBG" \
  -- $CHANGED_SOURCES || rc=1
echo "G5_ASSERT_RC=$rc"
exit $rc
INNER

# --- G6: smoke, export contract, format, whitespace ------------------------
cat > "$S/g6.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=${GATES_SCRATCH:?}; G=$S/gates
CHANGED="$1"; ASSERTS="$2"
cd "$ROOT" || exit 90
# THE ACCUMULATION IS THE RUNNER'S OWN FUNCTION (fix round 2), so the fold that
# proves "a failed sub-gate survives the green ones after it" runs THIS code.
# shellcheck source=gate_asserts_m3b.sh
. "$ASSERTS" || exit 90
rc=0
# THE SMOKE'S WORKDIR IS KEPT so the export contract can read the SAME build's
# installed hvenTargets.cmake and the SAME build's configure log -- the script's
# own documented contract for the third argument (never a literal picked ahead
# of time).
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

# ===========================================================================
# THE PRE-FLIGHT. Write all six, judge all six, and stop here if any one of
# them would write somewhere other than this chain's own tree.
# ===========================================================================
emit_segments || { echo "REFUSED: a gate segment could not be written"; exit 2; }
preflight_rc=0
for seg in g1 g3 g5 g6; do
  assert_segment_scratch "$S/$seg.sh" || preflight_rc=1
done
if [ "$preflight_rc" -ne 0 ]; then
  echo "REFUSED: the pre-flight rejected a gate segment. NO LEG HAS RUN."
  exit 2
fi
for seg in g2 g4; do
  [ ! -e "$S/$seg.sh" ] || { echo "REFUSED: $S/$seg.sh exists, but this round writes no $seg segment (E1BFR3 (f))"; exit 2; }
done
echo "=== PREFLIGHT_OK: 4 segments (g1 g3 g5 g6) written and asserted against GATES_SCRATCH, g2/g4 declared not run, before the first leg ==="
if [ "$PREFLIGHT_ONLY" -eq 1 ]; then
  echo "PREFLIGHT_ONLY: stopping before G1. No leg was run and no build tree was touched."
  exit 0
fi

# --- G1: Debug -------------------------------------------------------------
run_locked G1-debug bash "$S/g1.sh"; rc1=$?
assert_ctest "G1 Debug suite" "$G/suite-debug.log" "$EXPECT_DEBUG_TESTS" || rc1=1
assert_names "G1 Debug" "$G/suite-debug.log" "$NAMES_BASE_DIR/suite-debug.txt" || rc1=1

# --- G2: NOT RUN (E1BFR3 (f)) ------------------------------------------------
echo "G2 NOT RUN (declared, E1BFR3 (f)): the long test's source and every library line it runs are unchanged but for comments"

# --- G3: Release ----------------------------------------------------------
run_locked G3-release bash "$S/g3.sh"; rc3=$?
assert_ctest "G3 Release suite" "$G/suite-release.log" "$EXPECT_RELEASE_TESTS" || rc3=1
assert_names "G3 Release" "$G/suite-release.log" "$NAMES_BASE_DIR/suite-release.txt" || rc3=1

# --- G4: NOT RUN (E1BFR3 (f)) ------------------------------------------------
echo "G4 NOT RUN (declared, E1BFR3 (f)): no numeric code changed (the pre-flight's comment-only proof)"

# --- G5: P-SYM, both configs ---------------------------------------------
run_locked G5-psym bash "$S/g5.sh" "$BEFORE" "$CHANGED_SOURCES" "$CHANGED_HEADERS" \
  "$MIN_NEGATIVE_CONTROL" "$EXPECT_PSYM_DIFFERING" "$EXPECT_PSYM_CLASS_A" \
  "$EXPECT_PSYM_CLASS_B" "$EXPECT_DEBUG_ONLY_RELEASE" "$EXPECT_DEBUG_ONLY_DEBUG" \
  "$EXPECT_LIB_MOVERS_RELEASE" "$EXPECT_LIB_MOVERS_DEBUG" "$ADDED_IN_UNIVERSE" "$EXPECT_LIB_ADDED" \
  "$OUTSIDE_DIR" "$EXPECT_CLASS_A_OUTSIDE_SRCS" "$EXPECT_PSYM_B_DEBUG_ONLY" \
  "$EXPECT_LIB_COMMENT_ONLY_SRCS"; rc5=$?

# The MEASURED class B set EQUALS the declared names, per configuration. E1BFR3 (g):
# an EMPTY declared list is met only by an empty measured set, and only when the
# classification file exists and carries its class B header. E1BFR3 (i): compared as
# normalized sets by `class_b_names_check` (the offline-exercised function).
for CFG in Release Debug; do
  class_b_names_check "$CFG" "$G/psym/classification-$CFG.txt" || rc5=1
done

# --- G6: smoke, export contract, format, whitespace ------------------------
CHANGED_CXX=$(git diff --name-only "$BEFORE..$HEAD_AT_START" -- 'include/*.h' 'src/*.cpp' 'tests/*.cpp' | tr '\n' ' ')
echo "=== CHANGED C++ FOR CLANG-FORMAT: $CHANGED_CXX"
run_locked G6-smoke bash "$S/g6.sh" "$CHANGED_CXX" "$ASSERTS"; rc6=$?

echo "GATES_SUMMARY G1=$rc1 G2=NOT-RUN(declared) G3=$rc3 G4=NOT-RUN(declared) G5=$rc5 G6=$rc6"
echo "GATES_END $(date -Is)"
# rc5 IS IN THE AGGREGATE. The first round's final line omitted it, so a P-SYM
# segment that failed outright could not fail the chain. Since fix round 2 the
# aggregation itself is `gate_asserts_m3b.sh`'s, so the fold that proves it runs
# this exact code rather than a copy of it -- and a return code that is not an
# integer (an unset variable, which used to read as 0) is its own refusal.
# E1BFR3 (f): over the FOUR gates this round runs.
gates_aggregate "$rc1" "$rc3" "$rc5" "$rc6"
exit $?
