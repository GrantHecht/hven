#!/usr/bin/env bash
# usage: selftests.sh      (no box lock: pure python, no solver)
# The bench comparator self-tests, run in the NEW arm's private clone (branch tr-e). Whole discover is run and the known
# pre-existing failures of this box are listed explicitly by name; the three TR-E suites must be 100% green.
WT=/home/ghecht/Projects/hven/.claude/worktrees/tr-e
cd $WT/.scratch/bench-new || exit 1
git rev-parse HEAD
echo "== TR-E suites =="
python3 -m unittest tests.test_compare_side tests.test_trace_v1 tests.test_compare -v 2>&1 | tail -80
python3 -m unittest tests.test_compare_side tests.test_trace_v1 tests.test_compare 2>&1 | tail -3
echo "== whole discover (informational: the corun wall tests need a quiet-box binary set) =="
python3 -m unittest discover -s tests 2>&1 | grep -E "^(ERROR|FAIL):|^Ran|FAILED|^OK"
