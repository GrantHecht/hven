#!/usr/bin/env bash
# M7 W2 M5 -- THE GATES FOR THE DEGENERACY ARMS (arm A, arm b1, the diagnostic
# half). This file is M7 W2 M4 FIX ROUND 1's own `gates-m4fix1.sh`, COPIED and
# re-pointed: the base commit, the expected test counts, the scratch tree and
# the log name. M4's files are NOT edited -- they produced committed
# transcripts, and a gate's evidence must stay reproducible from the script
# that produced it. The six gate SEGMENTS below are M4 fix1's verbatim; only
# the constants above them and this header are this round's.
#
#   G1  Debug: fresh configure, build, suite WITHOUT the >600 s test
#   G2  the >600 s test ALONE (Continuation.ProbeBudgetBoundsAFailingProposal)
#   G3  Release: fresh configure, build, suite
#   G4  the U0 27-cell three-arm replay against the M6 close pin
#   G5  P-SYM, Release and Debug, classified against $BEFORE with a
#       DERIVED changed-header list, plus the fail-closed non-debug section check
#   G6  install smoke, the export contract, clang-format, git diff --check
#
# ---- THIS ROUND'S DECLARED EXPECTATIONS, AND HOW THEY WERE DERIVED --------
# The derivation is `gates/psym-derivation-m5.txt`, committed BEFORE this chain
# was detached. Its summary:
#
# BEFORE = f255bae9 -- THIS TASK'S OWN BASE, which is what the brief asks for
# ("expectations DECLARED from THIS task's base..head"). The claim the gate
# carries is therefore "the WHOLE M5 degeneracy experiment -- arm A, arm b1 and
# the diagnostic half together -- leaves the production library byte-identical
# to the tree that has no degeneracy arm in it". `f255bae9..HEAD` is exactly
# that comparison. Nothing but `docs/notes/data/**` changed between M4 fix
# round 1's head (3935eb9d) and this base, checked with
# `git diff --name-only 3935eb9d f255bae9 -- tests/ src/ include/ bench/ cmake/
# CMakeLists.txt`, which is EMPTY -- so this round's declaration is directly
# comparable with M4 fix round 1's.
#
# THE CHANGED SURFACE, from `git diff --name-only f255bae9..HEAD`:
#   include/hven/detail/qp/qp_degeneracy_arm.h   NEW; its ENTIRE body is
#                                                inside `#ifdef HVEN_TESTING`
#   include/hven/detail/qp/qp_walk_trace.h       new columns and one
#                                                `#include`, ALL inside that
#                                                file's own `#ifdef HVEN_TESTING`
#   src/qp/qp_engine.cpp                         `#ifdef HVEN_TESTING` blocks
#                                                only, ALL below that TU's one
#                                                `assert()` (line 447) -- the
#                                                EARLIEST hunk of the two M5
#                                                commits is at line 622 (arm A),
#                                                checked with `git diff -U0`
#                                                before this file was written;
#                                                the whole hunk list is in
#                                                `gates/psym-derivation-m5.txt`
#   tests/CMakeLists.txt, tests/qp/test_qp_degeneracy_arm.cpp   test target only
#
# TEST COUNTS: 2730 Debug / 2731 Release. `ctest -N` reports 2733 on this
# round's own configured tree; M2b fix round 1's derived offset (a CTest summary
# counts the one SKIPPED test but not the two DISABLED ones, and Debug
# additionally excludes the long test) gives 2733 - 2 = 2731 Release and
# 2733 - 3 = 2730 Debug. CROSS-CHECKED against M4 fix round 1's own declaration:
# it declared and met 2714 Debug / 2715 Release; no test source changed between
# its head and this round's base (the emptiness check above); M5 adds ONE new
# file with exactly 16 `TEST()` blocks and changes none in place, so
# 2714 + 16 = 2730 and 2715 + 16 = 2731. The two derivations agree.
#
# THE P-SYM MOVER SHAPE: ZERO, IN BOTH CONFIGURATIONS.
#   Release   DIFFERING 0 = class A 0 + class B 0
#   Debug     DIFFERING 0 = class A 0 + class B 0
# and therefore `psym_compare.sh compare` must exit 0, not 1.
#
# THE DECLARED DEBUG-ONLY SHAPE: Release 0, Debug 1, and the one is
# `src/qp/qp_engine.cpp.o`. WHY ONE, AND WHY THIS ROUND CANNOT HAVE MOVED IT.
# No production translation unit gains a LINE: `qp_walk_trace.h`'s entire body
# is inside its own `#ifdef HVEN_TESTING`, so neither the columns it gained nor
# the new header it includes is ever opened by a production compile, and
# `qp_engine.h` is untouched. The one object that differs is the TU whose own
# `.cpp` source lines moved. Its `assert()` is at line 447 and EVERY hunk this
# round adds is at line 622 or below, so no `__LINE__` immediate moves and the
# INSTRUCTION stream cannot. This is the shape M4 and M4 fix round 1 both
# MEASURED (42/42 byte-identical in Release; 41 byte-identical and
# `qp_engine.cpp.o` debug-only in Debug), and G5b is what TESTS it here: a
# second Debug object, any Release object, or a difference in a NON-debug
# section all FAIL.
#
# A ZERO DECLARATION IS STRONGER, NOT WEAKER, and what could make a false zero
# pass is a capture that compared nothing. The gate asserts all three guards
# against that: both capture rcs with non-empty manifests, equal object counts
# on the two arms, and a non-zero negative control -- objects that DO include a
# changed header and are identical anyway.
#
# WALL IS INFORMATIONAL. Nothing this file prints is a timing claim.
# =========================================================================
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m5gates
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
#   * and `chain_gates_m5.sh` does not repeat this path either: it ASKS for it
#     with `--print-scratch`.
export GATES_SCRATCH="$S"

# --- THE TWO NON-RUNNING MODES, ANSWERED BEFORE ANY OUTPUT IS REDIRECTED -----
# `--print-scratch` makes this file the single source of the scratch path for
# every sibling that needs it. `--preflight-only` writes and checks the six gate
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
# Derived and cross-checked in `gates/psym-derivation-m5.txt`; see the header.
#
# RE-DECLARED FOR THE ARM B1 REDO (2026-09-22). The first declaration -- 2730
# Debug / 2731 Release, for the head `ced56152` -- was DECLARED AND NEVER
# MEASURED: its chain was stopped six minutes in, inside G1, when the settler's
# amendments A1-A12 superseded the design it gated. Nothing downstream may quote
# it. These numbers are this round's own, re-derived twice from the redo's head,
# and the derivation file is rewritten rather than appended to, because a
# declaration that was never run is not a pin and re-declaring it is not a pin
# mutation. The BASE is unchanged: still this task's own base, so the claim the
# gate carries is still "the whole M5 degeneracy experiment leaves the
# production library byte-identical to the tree with no degeneracy arm in it".
BEFORE=${BEFORE:-f255bae9}
EXPECT_DEBUG_TESTS=${EXPECT_DEBUG_TESTS:-2731}
EXPECT_RELEASE_TESTS=${EXPECT_RELEASE_TESTS:-2732}
EXPECT_LONG_TESTS=1
EXPECT_REPLAY_CELLS=27
EXPECT_REPLAY_COLUMNS=75
EXPECT_REPLAY_DIFFS=0
EXPECT_REPLAY_ENGINES=3
MIN_NEGATIVE_CONTROL=1

# ---- THIS ROUND'S EXPECTED P-SYM MOVER SHAPE --------------------------------
# ZERO, in both configurations, derived over the CAPTURE'S OWN MANIFEST in
# `gates/psym-derivation.txt` before this file was written. See the header.
EXPECT_PSYM_DIFFERING=${EXPECT_PSYM_DIFFERING:-0}
EXPECT_PSYM_CLASS_A=${EXPECT_PSYM_CLASS_A:-0}
EXPECT_PSYM_CLASS_B=${EXPECT_PSYM_CLASS_B:-0}

# ---- THE DECLARED DEBUG-ONLY SHAPE, PER CONFIGURATION -----------------------
# What may differ in DEBUG SECTIONS ONLY, declared before the run. RELEASE: 0 --
# nothing may differ at all, in any section, in any of the 42 library objects.
# DEBUG: exactly 1 -- `qp_engine.cpp.o`, whose own source changed, every hook
# below that TU's one `assert()`, so no `__LINE__` immediate moves. See this
# file's header for the whole derivation. A SECOND Debug object, or any Release
# object, or a difference in a NON-debug section, all FAIL, and a shape that is
# not the declared one is re-derived and declared again, never discovered.
EXPECT_DEBUG_ONLY_RELEASE=${EXPECT_DEBUG_ONLY_RELEASE:-0}
EXPECT_DEBUG_ONLY_DEBUG=${EXPECT_DEBUG_ONLY_DEBUG:-1}

mkdir -p "$G" "$S/logs"
LOG=$S/logs/gates-m5.log
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
# ALL SIX SEGMENTS ARE WRITTEN FIRST, AND ALL SIX ARE CHECKED, BEFORE ANY LEG
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

# --- G2: the long test, ALONE ---------------------------------------------
cat > "$S/g2.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=${GATES_SCRATCH:?}; G=$S/gates
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest --test-dir "$S/gates-dbg" --no-tests=error \
  -R "^Continuation.ProbeBudgetBoundsAFailingProposal$" --output-on-failure > "$G/suite-debug-long.log" 2>&1
rc=$?; tail -8 "$G/suite-debug-long.log"; exit $rc
INNER

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

# --- G4: the replay -------------------------------------------------------
cat > "$S/g4.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=${GATES_SCRATCH:?}; G=$S/gates
BIN=$S/gates-rel/bench/hven_sqp_corpus
D=$G/replay; mkdir -p "$D"; : > "$D/comparison.txt"
CELLS="$(grep -v '^#' "$ROOT/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt" | tr -d '\n')"
worst=0
for eng in walk ssn ipm; do
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
    "$BIN" --engine "$eng" --cells "$CELLS" --csv "$D/head-$eng.csv" > "$D/run-$eng.log" 2>&1
  rc=$?; echo "ran head-$eng rc=$rc"; [ $rc -eq 0 ] || { worst=1; break; }
done
[ $worst -eq 0 ] || exit 1
BASE=$ROOT/docs/notes/data/2026-09-m6-close-gate/replay
for eng in walk ssn ipm; do
  python3 -B "$ROOT/scripts/compare_replay.py" "$BASE/head-$eng.csv" "$D/head-$eng.csv" \
    "M6 CLOSE HEAD-$eng vs M7 W2 M4 HEAD-$eng" | tee -a "$D/comparison.txt"
  rc=${PIPESTATUS[0]}; echo "  CMP_RC=$rc"; [ $rc -eq 0 ] || worst=1
done
exit $worst
INNER

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
WANT_DIFFERING="$5"; WANT_A="$6"; WANT_B="$7"
WANT_DEBUG_ONLY_RELEASE="$8"; WANT_DEBUG_ONLY_DEBUG="$9"
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
  echo "=== P-SYM $CFG  after=$HEAD_SHA  before=$BEFORE"
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
  assert_capture_pair "$OUT/before-$CFG" "$OUT/after-$CFG" || rc=1
  echo "tree restored to $(git -C "$ROOT" rev-parse --abbrev-ref HEAD) $(git -C "$ROOT" rev-parse --short HEAD)"
  "$ROOT/scripts/psym_compare.sh" compare "$OUT/before-$CFG" "$OUT/after-$CFG" > "$OUT/compare-$CFG.txt" 2>&1
  cmp_rc=$?
  echo "PSYM_COMPARE_RC_$CFG=$cmp_rc (1 is EXPECTED; the gate is the classification)"
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
  bash "$S/check_sections.sh" "$CFG" "$OUT/before-obj-$CFG" "$OUT/after-obj-$CFG" \
    "$OUT/libobjs-$CFG.txt" > "$OUT/sections-$CFG.txt" 2>&1
  sec_rc=$?
  echo "SECTIONS_RC_$CFG=$sec_rc"
  grep -E "^(SECTIONS_|NON-DEBUG-MOVER|DEBUG-ONLY|REFUSED)" "$OUT/sections-$CFG.txt" | head -40
  if [ "$sec_rc" -ne 0 ]; then
    echo "ASSERT FAIL sections $CFG: the fail-closed non-debug comparison refused (rc=$sec_rc)"
    rc=1
  else
    echo "ASSERT OK   sections $CFG: every library object is BYTE-IDENTICAL or DEBUG-ONLY, on both arms"
  fi
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
# shellcheck disable=SC2086
assert_psym_cross_config "$OUT/classification-Release.txt" "$OUT/classification-Debug.txt" \
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
for seg in g1 g2 g3 g4 g5 g6; do
  assert_segment_scratch "$S/$seg.sh" || preflight_rc=1
done
if [ "$preflight_rc" -ne 0 ]; then
  echo "REFUSED: the pre-flight rejected a gate segment. NO LEG HAS RUN."
  exit 2
fi
echo "=== PREFLIGHT_OK: 6 segments written and asserted against GATES_SCRATCH, before the first leg ==="
if [ "$PREFLIGHT_ONLY" -eq 1 ]; then
  echo "PREFLIGHT_ONLY: stopping before G1. No leg was run and no build tree was touched."
  exit 0
fi

# --- G1: Debug -------------------------------------------------------------
run_locked G1-debug bash "$S/g1.sh"; rc1=$?
assert_ctest "G1 Debug suite" "$G/suite-debug.log" "$EXPECT_DEBUG_TESTS" || rc1=1

# --- G2: the long test, ALONE ---------------------------------------------
run_locked G2-debug-long bash "$S/g2.sh"; rc2=$?
assert_ctest "G2 long test" "$G/suite-debug-long.log" "$EXPECT_LONG_TESTS" || rc2=1

# --- G3: Release ----------------------------------------------------------
run_locked G3-release bash "$S/g3.sh"; rc3=$?
assert_ctest "G3 Release suite" "$G/suite-release.log" "$EXPECT_RELEASE_TESTS" || rc3=1

# --- G4: the replay -------------------------------------------------------
run_locked G4-replay bash "$S/g4.sh"; rc4=$?
assert_replay "$G/replay/comparison.txt" "$EXPECT_REPLAY_CELLS" "$EXPECT_REPLAY_COLUMNS" \
  "$EXPECT_REPLAY_DIFFS" "$EXPECT_REPLAY_ENGINES" || rc4=1

# --- G5: P-SYM, both configs ---------------------------------------------
run_locked G5-psym bash "$S/g5.sh" "$BEFORE" "$CHANGED_SOURCES" "$CHANGED_HEADERS" \
  "$MIN_NEGATIVE_CONTROL" "$EXPECT_PSYM_DIFFERING" "$EXPECT_PSYM_CLASS_A" \
  "$EXPECT_PSYM_CLASS_B" "$EXPECT_DEBUG_ONLY_RELEASE" "$EXPECT_DEBUG_ONLY_DEBUG"; rc5=$?

# --- G6: smoke, export contract, format, whitespace ------------------------
CHANGED_CXX=$(git diff --name-only "$BEFORE..$HEAD_AT_START" -- 'include/*.h' 'src/*.cpp' 'tests/*.cpp' | tr '\n' ' ')
echo "=== CHANGED C++ FOR CLANG-FORMAT: $CHANGED_CXX"
run_locked G6-smoke bash "$S/g6.sh" "$CHANGED_CXX" "$ASSERTS"; rc6=$?

echo "GATES_SUMMARY G1=$rc1 G2=$rc2 G3=$rc3 G4=$rc4 G5=$rc5 G6=$rc6"
echo "GATES_END $(date -Is)"
# rc5 IS IN THE AGGREGATE. The first round's final line omitted it, so a P-SYM
# segment that failed outright could not fail the chain. Since fix round 2 the
# aggregation itself is `gate_asserts_m3b.sh`'s, so the fold that proves it runs
# this exact code rather than a copy of it -- and a return code that is not an
# integer (an unset variable, which used to read as 0) is its own refusal.
gates_aggregate "$rc1" "$rc2" "$rc3" "$rc4" "$rc5" "$rc6"
exit $?
