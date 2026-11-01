#!/usr/bin/env bash
# M7 W2 E2 gates REHEARSAL: the gate segments g1-g5 at the merged head, each leg
# under its own hold of the box lock (released per leg), placeholder P-SYM
# declaration so G5 prints the movers. Detached under setsid by its launcher.
set -o pipefail
ROOT=/home/ghecht/Projects/hven
export GATES_SCRATCH=$ROOT/.scratch/m7w2e2rehearsal
S=$GATES_SCRATCH
cd "$ROOT" || exit 90
BEFORE=0747050c; HEADSHA=$(git rev-parse HEAD)
echo "REHEARSAL_START $(date -Is) HEAD $HEADSHA BEFORE $BEFORE dirty=$(git status --porcelain | wc -l)"
leg() { # label script args...
  local label="$1"; shift
  flock /tmp/box-build.lock bash -c '
    set -o pipefail
    label="$1"; shift
    echo "FOREGROUND_START ${label} $(date -Is)"
    foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
    [ -z "$foreign" ] && echo "PGREP_EMPTY ${label}: no foreign cell binary under my lock" \
                      || echo "NOTE: a foreign cell binary is alive under my lock: $foreign"
    bash "$@"; rc=$?
    echo "FOREGROUND_END ${label} $(date -Is) rc=${rc}"
    exit ${rc}
  ' _ "$label" "$@"
  echo "LEG_RC ${label} rc=$?"
}
leg R1-debug "$S/g1.sh"
leg R2-debug-long "$S/g2.sh"
leg R3-release "$S/g3.sh"
leg R4-replay "$S/g4.sh"
CS=$(git diff --name-only "$BEFORE..$HEADSHA" -- src/ tests/ | tr '\n' ' ')
CH=$(git diff --name-only "$BEFORE..$HEADSHA" -- include/ | tr '\n' ' ')
M=$(git diff --name-only --diff-filter=M "$BEFORE..$HEADSHA" -- 'src/*.cpp' | sed 's|^|CMakeFiles/hven.dir/|; s|$|.o|' | LC_ALL=C sort | tr '\n' ' ')
ADD=$(git diff --name-only --diff-filter=A "$BEFORE..$HEADSHA" -- 'src/*.cpp' 'tests/*.cpp' | tr '\n' ' ')
LADD=$(git diff --name-only --diff-filter=A "$BEFORE..$HEADSHA" -- 'src/*.cpp' | sed 's|^|CMakeFiles/hven.dir/|; s|$|.o|' | LC_ALL=C sort | tr '\n' ' ')
OUTS=$(git diff --name-only --diff-filter=M "$BEFORE..$HEADSHA" -- 'tests/qp/*.cpp' | tr '\n' ' ')
echo "G5 placeholder args: CS=[$CS] CH=[$CH] M=[$M] ADD=[$ADD] LADD=[$LADD] OUTS=[$OUTS]"
leg R5-psym "$S/g5.sh" "$BEFORE" "$CS" "$CH" 1 1/1 12/12 0/0 0 0 "$M" "$M" "$ADD" "$LADD" \
  "tests/CMakeFiles/hven_qp_walk_seam_tests.dir" "$OUTS" ""
echo "REHEARSAL_END $(date -Is) tree=$(git rev-parse --abbrev-ref HEAD) $(git rev-parse --short HEAD)"
