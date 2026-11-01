#!/usr/bin/env bash
# THE ONE >600 s DEBUG TEST, ALONE.
set -o pipefail
S=/home/ghecht/Projects/hven/.scratch/m7w2d1
cd "$S/dbg"
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest --output-on-failure \
  -R '^Continuation\.ProbeBudgetBoundsAFailingProposal$' 2>&1 | tail -20
exit ${PIPESTATUS[0]}
