#!/usr/bin/env bash
set -o pipefail
ROOT=/home/ghecht/Projects/hven
export GATES_SCRATCH=$ROOT/.scratch/m7w2e1brehearsal
cd $ROOT
BEFORE=01d214bf; HEADSHA=$(git rev-parse HEAD)
CS=$(git diff --name-only "$BEFORE..$HEADSHA" -- src/ tests/ | tr '\n' ' ')
CH=$(git diff --name-only "$BEFORE..$HEADSHA" -- include/ | tr '\n' ' ')
M="CMakeFiles/hven.dir/src/drivers/equality_basis.cpp.o CMakeFiles/hven.dir/src/drivers/sqp_solver.cpp.o "
flock /tmp/box-build.lock bash -c '
  echo "LOCK_TAKEN $(date -Is) HEAD $(git -C /home/ghecht/Projects/hven rev-parse --short HEAD)"
  foreign=$(pgrep -x hven_bench_cell | tr "\n" " "); echo "FOREIGN_CELLS=[$foreign]"
  bash "$GATES_SCRATCH/g5.sh" "$@"
  echo "G5_REHEARSAL_RC=$? $(date -Is)"
' _ "$BEFORE" "$CS" "$CH" 1 1/1 7/7 0/0 0 0 "$M" "$M" "" "" \
  "tests/CMakeFiles/hven_sqp_seam_tests.dir" "tests/sqp/test_sqp_equality_basis_seams.cpp"
