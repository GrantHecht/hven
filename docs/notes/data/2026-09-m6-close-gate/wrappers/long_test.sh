#!/usr/bin/env bash
# The >600 s Debug test, ALONE, on the box lock, under nohup, with its pids
# recorded.  Continuation.ProbeBudgetBoundsAFailingProposal is the one cell the
# Debug suite leg EXCLUDES (-E), so between that leg and this one the Debug
# inventory is covered whole.
set -uo pipefail
R=/home/ghecht/Projects/hven
F=$R/.scratch/m6-close
exec bash "$F/run_leg.sh" "$F/logs/04-long-test.log" \
    env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 \
    ctest --test-dir "$F/build-debug" \
    -R '^Continuation\.ProbeBudgetBoundsAFailingProposal$' --output-on-failure
