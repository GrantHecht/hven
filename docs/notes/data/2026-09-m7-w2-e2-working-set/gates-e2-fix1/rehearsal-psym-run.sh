#!/usr/bin/env bash
# M7 W2 E2 GATES FIX1 -- the P-SYM REHEARSAL at the style commit: the first
# declaration's g5 segment (extracted byte-for-byte from wrappers-gates/gates-e2.sh),
# run against BEFORE=0747050c under ONE hold of the box lock, with the
# HYPOTHESIS 70/70 = A 13/13 + B 57/57 passed as the declaration. A refusal
# still prints the measured counts; the classification is then re-run offline
# with the measured count. Detached under setsid by its launcher. The first
# chain's own compare files (.scratch/m7w2e2gates/gates/psym/) are NOT reused:
# this is a fresh measurement on the new head.
set -o pipefail
ROOT=/home/ghecht/Projects/hven
export GATES_SCRATCH=$ROOT/.scratch/m7w2e2fix1rehearsal
S=$GATES_SCRATCH
cd "$ROOT" || exit 90
BEFORE=0747050c; HEADSHA=$(git rev-parse HEAD)
echo "REHEARSAL_START $(date -Is) HEAD $HEADSHA BEFORE $BEFORE dirty=$(git status --porcelain | wc -l)"
CS=$(git diff --name-only "$BEFORE..$HEADSHA" -- src/ tests/ | tr '\n' ' ')
CH=$(git diff --name-only "$BEFORE..$HEADSHA" -- include/ | tr '\n' ' ')
M=$(git diff --name-only --diff-filter=M "$BEFORE..$HEADSHA" -- 'src/*.cpp' | sed 's|^|CMakeFiles/hven.dir/|; s|$|.o|' | LC_ALL=C sort | tr '\n' ' ')
LIBB="CMakeFiles/hven.dir/src/core/ledger.cpp.o CMakeFiles/hven.dir/src/drivers/sqp_solver.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/soc_elastic_restoration.cpp.o CMakeFiles/hven.dir/src/qp/ipqp_engine.cpp.o CMakeFiles/hven.dir/src/qp/ssn_engine.cpp.o CMakeFiles/hven.dir/src/warmstart/continuation.cpp.o"
MOV=$(printf '%s\n' $M $LIBB | LC_ALL=C sort -u | tr '\n' ' ')
ADD=$(git diff --name-only --diff-filter=A "$BEFORE..$HEADSHA" -- 'src/*.cpp' 'tests/*.cpp' | tr '\n' ' ')
LADD=$(git diff --name-only --diff-filter=A "$BEFORE..$HEADSHA" -- 'src/*.cpp' | sed 's|^|CMakeFiles/hven.dir/|; s|$|.o|' | LC_ALL=C sort | tr '\n' ' ')
OUTS=$(git diff --name-only --diff-filter=M "$BEFORE..$HEADSHA" -- 'tests/qp/*.cpp' | tr '\n' ' ')
echo "G5 args: CS=[$CS] CH=[$CH] MOV=[$MOV] ADD=[$ADD] LADD=[$LADD] OUTS=[$OUTS]"
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
' _ R5-psym-fix1 "$S/g5.sh" "$BEFORE" "$CS" "$CH" 1 70/70 13/13 57/57 0 4 "$MOV" "$MOV" "$ADD" "$LADD" \
  "tests/CMakeFiles/hven_qp_walk_seam_tests.dir" "$OUTS" ""
echo "LEG_RC R5-psym-fix1 rc=$?"
echo "REHEARSAL_END $(date -Is) tree=$(git rev-parse --abbrev-ref HEAD) $(git rev-parse --short HEAD) dirty=$(git status --porcelain | wc -l)"
