#!/usr/bin/env bash
# DEBUG BUILD + DEBUG SUITE, excluding the one >600 s test, which runs ALONE and
# DETACHED in its own leg (CLAUDE.md's box protocol / the brief's section 0).
set -o pipefail
R=/home/ghecht/Projects/hven
S=$R/.scratch/m7w2m1
cmake -S "$R" -B "$S/dbg" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON 2>&1 | tail -3 || exit 1
cmake --build "$S/dbg" -j "$(nproc)" 2>&1 | tail -10 || exit 1
cd "$S/dbg"
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest -j4 --output-on-failure \
  -E '^Continuation\.ProbeBudgetBoundsAFailingProposal$' 2>&1 | tail -25
exit ${PIPESTATUS[0]}
