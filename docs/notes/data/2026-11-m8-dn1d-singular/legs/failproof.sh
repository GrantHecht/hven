#!/usr/bin/env bash
# M8 DN-1d (../DECLARATION.txt P14): copied from ../../2026-11-m8-dn1c-sqp-check/legs/failproof.sh, re-pointed (worktree
# dn1d, build-dn1de/, this directory) and rewritten for DN-1d's four mutations of the code head's `git archive` export,
# each fail closed unless its site occurs exactly once:
#   policy-off   include/hven/detail/kkt/kkt_calls.h: sqp_kkt_options' `o.dense_singular_policy =
#                hven::linear::DenseSingularPolicy::kReport;` -> `...::kSparseFallback;` (the SQP's factors never report);
#   detector-off src/linear/dense_session.cpp: `flagged = !(rcond_estimate(anorm) >= kDenseConditionSingular);` ->
#                `flagged = false;` (finite factorizations never flagged; exact singularity still reported);
#   shift-off    src/kkt/kkt_calls.cpp: `for (int attempt = 0; attempt < 2 && !done; ++attempt) {` -> `attempt < 0`
#                (no equilibrated refactor: every flag goes to the platform retake);
#   equil-off    src/kkt/kkt_calls.cpp: `Vec d = equilibration_scale(K);` -> `Vec d = Vec::Ones(K.rows());` (the shift
#                and the residual measures unscaled).
# Each built (Release, legs/build.sh's flags; hven_tests, hven_qp_walk_seam_tests, hven_drivers_tests) at
# build-dn1de/fp-<name>; the head's binaries are build-dn1de/rel's. Filters: hven_tests 'DenseArm*',
# hven_qp_walk_seam_tests 'QpDenseArm.*', hven_drivers_tests 'DenseArmEngines.*'. Criterion: head passes every test;
# each mutation fails EXACTLY its declared set (WANT below, from the declaration).
# usage: failproof.sh <code-head sha> <evidence-dir>   Run as: flock /tmp/box-build.lock bash failproof.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1d
L=$WT/docs/notes/data/2026-11-m8-dn1d-singular/legs
TREES=${TREES:-$WT/build-dn1de}
HEAD_SHA=${1:?code head sha}
EV=${2:?evidence dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
export MKL_NUM_THREADS=1
run_filters() { # <build dir> <name>
  "$1/tests/hven_tests" --gtest_filter='DenseArm*' > "$EV/run-$2-hven_tests.txt" 2>&1; echo "$2 hven_tests rc=$?"
  "$1/tests/hven_qp_walk_seam_tests" --gtest_filter='QpDenseArm.*' > "$EV/run-$2-walk_seam.txt" 2>&1; echo "$2 walk_seam rc=$?"
  "$1/tests/drivers/hven_drivers_tests" --gtest_filter='DenseArmEngines.*' > "$EV/run-$2-drivers.txt" 2>&1; echo "$2 drivers rc=$?"
}
for M in policy-off detector-off shift-off equil-off; do
  S=$TREES/fp-src-$M
  rm -rf "$S" "$TREES/fp-$M"
  mkdir -p "$S"
  git archive "$HEAD_SHA" | tar -x -C "$S" || { echo ARCHIVE_FAIL; exit 1; }
  rm -rf "$S/dep/eigen" "$S/dep/fmt"
  cp -a dep/eigen "$S/dep/eigen" && cp -a dep/fmt "$S/dep/fmt" || exit 1
  case $M in
    policy-off)   F=include/hven/detail/kkt/kkt_calls.h ;;
    detector-off) F=src/linear/dense_session.cpp ;;
    shift-off)    F=src/kkt/kkt_calls.cpp ;;
    equil-off)    F=src/kkt/kkt_calls.cpp ;;
  esac
  python3 - "$S/$F" "$M" <<'PY' || { echo MUTATION_FAIL; exit 1; }
import sys
p, m = sys.argv[1], sys.argv[2]
s = open(p).read()
site, new, count = {
    'policy-off': ('o.dense_singular_policy = hven::linear::DenseSingularPolicy::kReport;',
                   'o.dense_singular_policy = hven::linear::DenseSingularPolicy::kSparseFallback;', 1),
    'detector-off': ('flagged = !(rcond_estimate(anorm) >= kDenseConditionSingular);', 'flagged = false;', 1),
    'shift-off': ('for (int attempt = 0; attempt < 2 && !done; ++attempt) {',
                  'for (int attempt = 0; attempt < 0 && !done; ++attempt) {', 1),
    'equil-off': ('Vec d = equilibration_scale(K);', 'Vec d = Vec::Ones(K.rows());', 1),
}[m]
print(m, 'sites', s.count(site), 'expected', count)
if s.count(site) != count:
    sys.exit(1)
open(p, 'w').write(s.replace(site, new))
PY
  diff <(git show "$HEAD_SHA:$F") "$S/$F" > "$EV/failproof-mutation-$M.diff"
  cat "$EV/failproof-mutation-$M.diff"
  SRC=$S BUILD_ROOT=$TREES bash "$L/build.sh" Release fp-$M hven_tests hven_qp_walk_seam_tests hven_drivers_tests > "$EV/failproof-build-$M.log" 2>&1 || { echo "FP_BUILD_FAIL $M"; tail -20 "$EV/failproof-build-$M.log"; exit 1; }
  run_filters "$TREES/fp-$M" "$M"
done
run_filters "${HEAD_TREE:-$TREES/rel}" head
python3 - "$EV" <<'PY' | tee "$EV/failproof-verdict.txt"
import re, sys
ev = sys.argv[1]
def res(name):
    out, txt = {}, ''
    for part in ('hven_tests', 'walk_seam', 'drivers'):
        t = open(f'{ev}/run-{name}-{part}.txt').read()
        txt += t
        out.update({m.group(2): m.group(1) for m in re.finditer(r'^\[\s+(OK|FAILED)\s+\] (\w+\.\w+) \(', t, re.M)})
    return out, txt
failed = lambda d: {k for k, v in d.items() if v == 'FAILED'}
h, _ = res('head')
want = {
    'policy-off': {'DenseArmEngines.SqpShiftsASingularKktOnTheDenseArm',
                   'QpDenseArm.ANearNullPivotIsShiftedToTheSparseAnswer',
                   'QpDenseArm.ARankDeficientWorkingSetAtMuZeroIsShiftedAndVerified',
                   'QpDenseArm.AnExactZeroColumnIsShiftedNotRetaken',
                   'QpDenseArm.FactorizeCheckedRetriesOnceThenRetakesOnThePlatform',
                   'QpDenseArm.FactorizeCheckedShiftsAFlaggedFactorAndReturnsTheSiteRegularization',
                   'QpDenseArm.SqpKktOptionsReportUnderADensePolicyOnly',
                   'QpDenseArm.TheAllinitcShapeMuFreeSystemIsAnsweredOnTheDenseArm',
                   'QpDenseArm.TheUnscaledMonitorStallsWhereTheEquilibratedOneConverges'},
    'detector-off': {'DenseArmEngines.SqpShiftsASingularKktOnTheDenseArm',
                     'DenseArmReport.FlagsASingularToWorkingPrecisionFactorAndKeepsItsNumerics',
                     'DenseArmReport.RefusedUnderNeverAndCarriedByHandles',
                     'DenseArmReport.TheCatastrophicMeshFactorIsFlaggedAndTheShiftedSolvePasses',
                     'DenseArmReport.TheDetectorBracketsBothThresholds',
                     'DenseArmReport.TheEquilibratedRefactorReportsPerturbedPivotsAndSolvesInASpace',
                     'QpDenseArm.ANearNullPivotIsShiftedToTheSparseAnswer',
                     'QpDenseArm.FactorizeCheckedShiftsAFlaggedFactorAndReturnsTheSiteRegularization',
                     'QpDenseArm.TheAllinitcShapeMuFreeSystemIsAnsweredOnTheDenseArm',
                     'QpDenseArm.TheUnscaledMonitorStallsWhereTheEquilibratedOneConverges'},
    'shift-off': {'DenseArmEngines.SqpShiftsASingularKktOnTheDenseArm',
                  'QpDenseArm.ANearNullPivotIsShiftedToTheSparseAnswer',
                  'QpDenseArm.ARankDeficientWorkingSetAtMuZeroIsShiftedAndVerified',
                  'QpDenseArm.AnExactZeroColumnIsShiftedNotRetaken',
                  'QpDenseArm.FactorizeCheckedRetriesOnceThenRetakesOnThePlatform',
                  'QpDenseArm.FactorizeCheckedShiftsAFlaggedFactorAndReturnsTheSiteRegularization',
                  'QpDenseArm.TheAllinitcShapeMuFreeSystemIsAnsweredOnTheDenseArm',
                  'QpDenseArm.TheUnscaledMonitorStallsWhereTheEquilibratedOneConverges'},
    'equil-off': {'DenseArmEngines.SqpShiftsASingularKktOnTheDenseArm',
                  'QpDenseArm.TheAllinitcShapeMuFreeSystemIsAnsweredOnTheDenseArm',
                  'QpDenseArm.TheUnscaledMonitorStallsWhereTheEquilibratedOneConverges'},
}
ok = len(h) > 0 and not failed(h)
print('head     :', len(h), 'tests, failed', sorted(failed(h)) or 'none')
for m, w in want.items():
    r, t = res(m)
    good = set(r) == set(h) and failed(r) == w
    print(f'{m:12s}:', len(r), 'tests, failed', sorted(failed(r)), '|', 'OK' if good else 'NOT AS DECLARED')
    ok = ok and good
print('FAILPROOF_AS_DECLARED' if ok else 'FAILPROOF_NOT_AS_DECLARED')
sys.exit(0 if ok else 1)
PY
rc=${PIPESTATUS[0]}
echo "FOREGROUND_END $(date -Is)"
exit $rc
