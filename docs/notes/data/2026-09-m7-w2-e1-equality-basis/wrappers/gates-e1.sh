#!/usr/bin/env bash
# M7 W2 E1 -- THE GATES FOR THE INDEPENDENT EQUALITY BASIS (SqpOptions::
# equality_basis, on by default). This file is M7 W2 E4's own `gates-e4.sh`,
# COPIED and re-pointed: the base commit, the expected counts, the P-SYM mover
# shape, the scratch tree, the log name, and THREE segment changes (G5, below).
# E4's file is NOT edited -- it produced committed transcripts.
#
#   G1  Debug: fresh configure, build, suite WITHOUT the >600 s test
#   G2  the >600 s test ALONE (Continuation.ProbeBudgetBoundsAFailingProposal)
#   G3  Release: fresh configure, build, suite
#   G4  the U0 27-cell three-arm replay against the M6 close pin
#   G5  P-SYM, Release and Debug, classified against $BEFORE with a DERIVED
#       changed-header list, plus the fail-closed non-debug section check
#   G6  install smoke, the export contract, clang-format, git diff --check
#
# ---- THIS ROUND'S DECLARED EXPECTATIONS -------------------------------------
# The derivation is `gates/psym-derivation-e1.txt`, committed WITH this file and
# BEFORE the chain was detached. BEFORE = 53676599, the E1 task's own base.
#
# THIS ROUND CHANGES TWO PUBLIC STRUCTS (SqpOptions and SqpResult each gain a
# field), so -- unlike E4 -- class B is NOT empty: every TU that lays either
# struct out, or instantiates an inline body from the two headers below the
# inserted lines, moves. The exact shape is in the derivation, per
# configuration, and is asserted here, not discovered.
#
# THE THREE G5 SEGMENT CHANGES, all forced by ADDED sources (E1 adds
# `src/drivers/equality_basis.cpp` and two test TUs, none of which exists at
# BEFORE):
#   (1) the before arm re-configures at BEFORE and runs `ninja -t cleandead`
#       before building, so an object the after arm left behind cannot sit in
#       the before capture as a byte-identical ghost of itself;
#   (2) the capture pair is judged as after = before + ADDED, and P-SYM's
#       ONLY-AFTER set must EQUAL the ADDED set derived from
#       `git diff --diff-filter=A BEFORE..HEAD` (by object basename);
#   (3) G5b's section check runs over the library objects present on BOTH
#       sides; an ADDED library object is listed, and asserted present after
#       and absent before, instead of being REFUSED as one-sided. The declared
#       NON-DEBUG-MOVER set is the MODIFIED src/*.cpp objects (derived) plus
#       the declared class-B library objects.
#
# G4 IS THE BEHAVIOURAL GATE: 0 differences declared, predicted by the offline
# basis check (`basis-check/`). A moved cell is a DECLARED FINDING, not a
# re-derivation: it is recorded and the round STOPS for the settler.
#
# WALL IS INFORMATIONAL. Nothing this file prints is a timing claim.
# =========================================================================
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2e1gates
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
#   * and `chain_gates_e1.sh` does not repeat this path either: it ASKS for it
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
# Derived in `gates/psym-derivation-e1.txt` §3.
BEFORE=${BEFORE:-53676599}
EXPECT_DEBUG_TESTS=${EXPECT_DEBUG_TESTS:-2766}
EXPECT_RELEASE_TESTS=${EXPECT_RELEASE_TESTS:-2767}
EXPECT_LONG_TESTS=1
EXPECT_REPLAY_CELLS=27
EXPECT_REPLAY_COLUMNS=75
EXPECT_REPLAY_DIFFS=0
EXPECT_REPLAY_ENGINES=3
MIN_NEGATIVE_CONTROL=1

# ---- THIS ROUND'S EXPECTED P-SYM MOVER SHAPE, PER CONFIGURATION ----------------
# `gates/psym-derivation-e1.txt` §2. Written RELEASE/DEBUG; g5.sh splits them.
# Class A is every MODIFIED src/ and tests/ .cpp whose object is in the P-SYM
# universe (the pre-flight re-derives that count from git); class B is the
# includers the derivation lists by name.
EXPECT_PSYM_DIFFERING=${EXPECT_PSYM_DIFFERING:-53/53}
EXPECT_PSYM_CLASS_A=${EXPECT_PSYM_CLASS_A:-8/8}
EXPECT_PSYM_CLASS_B=${EXPECT_PSYM_CLASS_B:-45/45}
# The P-SYM universe's ADDED objects: one per ADDED src/ or tests/ .cpp in a
# captured target. Derived from git below; this is the declared count.
EXPECT_PSYM_ADDED=${EXPECT_PSYM_ADDED:-3}
# Class A's MODIFIED sources outside the P-SYM universe (the pre-flight's
# value check subtracts them): none. `hven_ipqp_seam_tests` recompiles
# sqp_options.cpp but its objects are not captured; the source's own library
# object is.
EXPECT_CLASS_A_OUTSIDE_UNIVERSE=0

# ---- THE DECLARED SECTION SHAPE, PER CONFIGURATION --------------------------
# The NON-DEBUG movers are the MODIFIED src/*.cpp objects (DERIVED below) plus
# these declared class-B library objects, per configuration.
EXPECT_LIB_CLASS_B_RELEASE=${EXPECT_LIB_CLASS_B_RELEASE-"CMakeFiles/hven.dir/src/qp/ipqp_engine.cpp.o CMakeFiles/hven.dir/src/warmstart/continuation.cpp.o"}
EXPECT_LIB_CLASS_B_DEBUG=${EXPECT_LIB_CLASS_B_DEBUG-"CMakeFiles/hven.dir/src/qp/ipqp_engine.cpp.o CMakeFiles/hven.dir/src/warmstart/continuation.cpp.o"}
EXPECT_DEBUG_ONLY_RELEASE=${EXPECT_DEBUG_ONLY_RELEASE:-0}
EXPECT_DEBUG_ONLY_DEBUG=${EXPECT_DEBUG_ONLY_DEBUG:-4}

mkdir -p "$G" "$S/logs"
LOG=$S/logs/gates-e1.log
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
# VALUE-BASED PRE-FLIGHT, three checks, before any leg: the derived class A
# count, the derived ADDED count, and the derived library-mover list against
# what this round declares.
want_a_rel=${EXPECT_PSYM_CLASS_A%/*}; want_a_dbg=${EXPECT_PSYM_CLASS_A#*/}
if [ "$((n_mod - EXPECT_CLASS_A_OUTSIDE_UNIVERSE))" != "$want_a_rel" ] || [ "$want_a_rel" != "$want_a_dbg" ]; then
  echo "REFUSED: $n_mod modified .cpp (less $EXPECT_CLASS_A_OUTSIDE_UNIVERSE outside the universe), but class A is DECLARED $EXPECT_PSYM_CLASS_A"
  exit 2
fi
if [ "$n_add" != "$EXPECT_PSYM_ADDED" ]; then
  echo "REFUSED: $n_add added .cpp derived, but ADDED is DECLARED $EXPECT_PSYM_ADDED"
  exit 2
fi
lib_a=$(git diff --name-only --diff-filter=M "$BEFORE..$HEAD_AT_START" -- 'src/*.cpp' \
  | sed 's|^|CMakeFiles/hven.dir/|; s|$|.o|')
EXPECT_LIB_ADDED=$(git diff --name-only --diff-filter=A "$BEFORE..$HEAD_AT_START" -- 'src/*.cpp' \
  | sed 's|^|CMakeFiles/hven.dir/|; s|$|.o|' | LC_ALL=C sort | tr '\n' ' ')
EXPECT_LIB_MOVERS_RELEASE=$(printf '%s\n' $lib_a $EXPECT_LIB_CLASS_B_RELEASE | LC_ALL=C sort -u | tr '\n' ' ')
EXPECT_LIB_MOVERS_DEBUG=$(printf '%s\n' $lib_a $EXPECT_LIB_CLASS_B_DEBUG | LC_ALL=C sort -u | tr '\n' ' ')
echo "=== DECLARED NON-DEBUG MOVERS Release (derived class A + declared class B): $EXPECT_LIB_MOVERS_RELEASE"
echo "=== DECLARED NON-DEBUG MOVERS Debug   (derived class A + declared class B): $EXPECT_LIB_MOVERS_DEBUG"
echo "=== DECLARED ADDED LIBRARY OBJECTS (derived): $EXPECT_LIB_ADDED"
for o in $EXPECT_LIB_CLASS_B_RELEASE $EXPECT_LIB_CLASS_B_DEBUG; do
  src=${o#CMakeFiles/hven.dir/}; src=${src%.o}
  if printf '%s\n' $lib_a | grep -qxF "$o"; then
    echo "REFUSED: $o is declared class B but its source $src is MODIFIED (class A)"; exit 2
  fi
  [ -f "$src" ] || { echo "REFUSED: declared class B $o has no source $src"; exit 2; }
done
echo "=== PREFLIGHT_VALUE_OK: class A $n_mod == declared $EXPECT_PSYM_CLASS_A; ADDED $n_add == declared $EXPECT_PSYM_ADDED; class B library objects all unmodified"

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
    "M6 CLOSE HEAD-$eng vs M7 W2 E1 HEAD-$eng" | tee -a "$D/comparison.txt"
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
WANT_DIFFERING_RD="$5"; WANT_A_RD="$6"; WANT_B_RD="$7"
WANT_DEBUG_ONLY_RELEASE="$8"; WANT_DEBUG_ONLY_DEBUG="$9"
WANT_MOVERS_RELEASE="${10}"; WANT_MOVERS_DEBUG="${11}"
ADDED_CXX="${12}"; WANT_LIB_ADDED="${13}"
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
  "$EXPECT_PSYM_CLASS_B" "$EXPECT_DEBUG_ONLY_RELEASE" "$EXPECT_DEBUG_ONLY_DEBUG" \
  "$EXPECT_LIB_MOVERS_RELEASE" "$EXPECT_LIB_MOVERS_DEBUG" "$ADDED_CXX" "$EXPECT_LIB_ADDED"; rc5=$?

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
