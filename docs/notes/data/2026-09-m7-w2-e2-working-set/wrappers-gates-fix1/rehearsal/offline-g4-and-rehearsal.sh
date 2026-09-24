#!/usr/bin/env bash
# M7 W2 E2 FIX ROUND 1b -- OFFLINE G4 CHECK (at HEAD, build/ Release) then the P-SYM
# REHEARSAL (the E2 fix1 chain's g5 segment, byte-identical copy) at HEAD against
# BEFORE=6d123e75, both configurations, under the box lock, each leg its own hold.
# The rehearsal's declaration arguments are HYPOTHESES; a refusal still prints the
# measured shape, which is then declared.
set -o pipefail
ROOT=/home/ghecht/Projects/hven
export GATES_SCRATCH=$ROOT/.scratch/m7w2e2fr1brehearsal
S=$GATES_SCRATCH; G=$S/gates
cd "$ROOT" || exit 90
BEFORE=6d123e75; HEADSHA=$(git rev-parse HEAD)
echo "START $(date -Is) HEAD $HEADSHA BEFORE $BEFORE dirty=$(git status --porcelain | wc -l)"
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: tree not clean"; exit 2; }
lock() {
  flock /tmp/box-build.lock bash -c '
    set -o pipefail
    label="$1"; shift
    echo "FOREGROUND_START ${label} $(date -Is)"
    foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
    [ -z "$foreign" ] && echo "PGREP_EMPTY ${label}: no foreign cell binary under my lock" \
                      || echo "NOTE: a foreign cell binary is alive under my lock: $foreign"
    "$@"; rc=$?
    echo "FOREGROUND_END ${label} $(date -Is) rc=${rc}"
    exit ${rc}
  ' _ "$@"
}
g4() {
  BIN=$ROOT/build/bench/hven_sqp_corpus
  D=$G/replay; mkdir -p "$D"; : > "$D/comparison.txt"
  CELLS="$(grep -v '^#' "$ROOT/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt" | tr -d '\n')"
  for eng in walk ssn ipm; do
    MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 "$BIN" --engine "$eng" --cells "$CELLS" --csv "$D/head-$eng.csv" > "$D/run-$eng.log" 2>&1
    echo "ran head-$eng rc=$?"
  done
  for eng in walk ssn ipm; do
    python3 -B "$ROOT/scripts/compare_replay.py" --residual-gate 1e-5 "$ROOT/docs/notes/data/2026-09-m6-close-gate/replay/head-$eng.csv" "$D/head-$eng.csv" \
      "M6 CLOSE HEAD-$eng vs E2 fix1b HEAD-$eng (GATED 1e-5)" | tee -a "$D/comparison.txt" | tail -4
    echo "  CMP_RC=${PIPESTATUS[0]}"
  done
}
export -f g4; export ROOT G
lock OFFLINE-G4 bash -c g4
echo "LEG_RC OFFLINE-G4 rc=$?"
CS=$(git diff --name-only "$BEFORE..$HEADSHA" -- src/ tests/ | tr '\n' ' ')
CH=$(git diff --name-only "$BEFORE..$HEADSHA" -- include/ | tr '\n' ' ')
M=$(git diff --name-only --diff-filter=M "$BEFORE..$HEADSHA" -- 'src/*.cpp' | sed 's|^|CMakeFiles/hven.dir/|; s|$|.o|' | LC_ALL=C sort | tr '\n' ' ')
ADD=$(git diff --name-only --diff-filter=A "$BEFORE..$HEADSHA" -- 'src/*.cpp' 'tests/*.cpp' | tr '\n' ' ')
LADD=$(git diff --name-only --diff-filter=A "$BEFORE..$HEADSHA" -- 'src/*.cpp' | sed 's|^|CMakeFiles/hven.dir/|; s|$|.o|' | LC_ALL=C sort | tr '\n' ' ')
OUTS="tests/qp/test_qp_anti_cycling_arm.cpp"
echo "G5 args: CS=[$CS] CH=[$CH] MOV=[$M] ADD=[$ADD] LADD=[$LADD] OUTS=[$OUTS]"
lock R5-psym-e2fix1b bash "$S/g5.sh" "$BEFORE" "$CS" "$CH" 1 60/60 3/3 57/57 0 1 "$M" "$M" "$ADD" "$LADD" \
  "tests/CMakeFiles/hven_qp_walk_seam_tests.dir" "$OUTS" ""
echo "LEG_RC R5-psym-e2fix1b rc=$?"
echo "END $(date -Is) tree=$(git rev-parse --abbrev-ref HEAD) $(git rev-parse --short HEAD) dirty=$(git status --porcelain | wc -l)"
