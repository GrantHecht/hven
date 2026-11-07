#!/usr/bin/env bash
# M8 PF-1 (DECLARATION P16): THE FALSIFIER. The code head's `git archive` export at build-pf1e/fp-src with the rule
# disabled by ONE mutation -- detail::kMaxDualRegDecreases 2 -> 0 (fail closed unless exactly one line changes) --
# built (Release, legs/build.sh's flags, hven_qp_walk_seam_tests only) at build-pf1e/fp, then
# QpReleaseReadmission.F7PathCorruptedQp1LeavesTheCappedEndpoint run on it. Criterion: the test FAILS (gtest exit
# != 0) and prints dual_regularization_decreases=0 -- the fixture does not leave the capped endpoint without the rule.
# usage: failproof.sh <code-head sha> <evidence-dir>   Run as: flock /tmp/box-build.lock bash failproof.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aa2c5c3a798557d0e
L=$WT/docs/notes/data/2026-11-m8-pf1/legs
HEAD_SHA=${1:?code head sha}
EV=${2:?evidence dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
rm -rf "$WT/build-pf1e/fp-src" "$WT/build-pf1e/fp"
mkdir -p "$WT/build-pf1e/fp-src"
git archive "$HEAD_SHA" | tar -x -C "$WT/build-pf1e/fp-src" || { echo ARCHIVE_FAIL; exit 1; }
rm -rf "$WT/build-pf1e/fp-src/dep/eigen" "$WT/build-pf1e/fp-src/dep/fmt"
cp -a dep/eigen "$WT/build-pf1e/fp-src/dep/eigen" && cp -a dep/fmt "$WT/build-pf1e/fp-src/dep/fmt" || exit 1
H=$WT/build-pf1e/fp-src/include/hven/detail/qp/qp_engine.h
n=$(grep -c '^constexpr Index kMaxDualRegDecreases = 2;$' "$H")
[ "$n" = 1 ] || { echo "MUTATION_SITE_COUNT $n"; exit 1; }
sed -i 's/^constexpr Index kMaxDualRegDecreases = 2;$/constexpr Index kMaxDualRegDecreases = 0;/' "$H"
diff <(git show "$HEAD_SHA:include/hven/detail/qp/qp_engine.h") "$H" | tee "$EV/failproof-mutation.diff"
[ "$(diff <(git show "$HEAD_SHA:include/hven/detail/qp/qp_engine.h") "$H" | grep -c '^[<>]')" = 2 ] || { echo MUTATION_NOT_ONE_LINE; exit 1; }
SRC=$WT/build-pf1e/fp-src bash "$L/build.sh" Release fp hven_qp_walk_seam_tests > "$EV/failproof-build.log" 2>&1 || { echo FP_BUILD_FAIL; tail -20 "$EV/failproof-build.log"; exit 1; }
export MKL_NUM_THREADS=1
"$WT/build-pf1e/fp/tests/hven_qp_walk_seam_tests" \
  --gtest_filter=QpReleaseReadmission.F7PathCorruptedQp1LeavesTheCappedEndpoint > "$EV/failproof-run.txt" 2>&1
trc=$?
echo "mutant test rc=$trc"
grep -E '^\[ pf1 \]|FAILED|PASSED|OK \]' "$EV/failproof-run.txt"
rc=0
if [ "$trc" -ne 0 ] && grep -q 'dual_regularization_decreases=0 ' "$EV/failproof-run.txt"; then
  echo FAILPROOF_AS_DECLARED
else
  echo FAILPROOF_NOT_AS_DECLARED; rc=1
fi
echo "FOREGROUND_END $(date -Is)"
exit $rc
