#!/usr/bin/env bash
# WA-1 rider 2 measurement lane: syntax check of the pinned scratch commit, lane binary via train_setup.sh, TRAIN with
# HVEN_WA1_MEAS. usage: meas_lane.sh <ARM> <ROUND> <sha> <evidence dir> [meas]   Run under flock /tmp/box-build.lock setsid -w
set -uo pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/wa1
L=$WT/docs/notes/data/2026-11-m8-wa1/legs
ARM=$1; ROUND=$2; SHA=$3; EV=$4; MEAS=${5:-}
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-wa/tmp
echo "START $(date -Is) arm=$ARM sha=$SHA"
src=$(mktemp -d "$TMPDIR/syn.XXXX")
git -C "$WT" archive "$SHA" src include | tar -x -C "$src" || exit 1
/usr/bin/clang++ -DEIGEN_DONT_PARALLELIZE -DEIGEN_INITIALIZE_MATRICES_BY_ZERO -DEIGEN_MAX_ALIGN_BYTES=32 -DFMT_HEADER_ONLY -DFMT_USE_LOCALE=0 \
  -I"$src/include" -I/opt/intel/oneapi/mkl/latest/include -isystem "$WT/dep/eigen" -isystem "$WT/dep/fmt/include" -DMKL_LP64 -std=c++20 \
  -fsyntax-only "$src/src/qp/walk/walk_engine.cpp" || { echo SYNTAX_FAIL; exit 1; }
rm -rf "$src"
echo "SYNTAX_OK $(date -Is)"
bash "$L/train_setup.sh" "$ARM" "$SHA" "$ROUND" || { echo SETUP_FAIL; exit 1; }
LS=$(sha256sum "$WT/build-wa/bin/$ARM/hven_bench_cell" | cut -d' ' -f1)
echo "LANE $ARM $LS"
if [ -n "$MEAS" ]; then export HVEN_WA1_MEAS=1; fi
bash "$L/train_enum.sh" "$EV" "$WT/build-wa/bin/$ARM/hven_bench_cell" "$LS" || { echo TRAIN_FAIL; exit 1; }
echo "DONE $(date -Is)"
