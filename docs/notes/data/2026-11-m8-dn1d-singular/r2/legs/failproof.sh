#!/usr/bin/env bash
# M8 DN-1d r2 (../../DECLARATION-r2.txt P14): from ../../legs/failproof.sh (r1), re-pointed (build-dn1de2/, r2/) and
# extended to DN-1d r2's TEN mutations of the code head's `git archive` export, each fail closed unless its site occurs
# exactly the stated number of times:
#   policy-off       include/hven/detail/kkt/kkt_calls.h: sqp_kkt_options sets kSparseFallback (the SQP never reports);
#   detector-off     src/linear/dense_session.cpp: finite factorizations never flagged;
#   shift-off        src/kkt/kkt_calls.cpp: no equilibrated refactor (every flag goes to the platform retake);
#   equil-off        src/kkt/kkt_calls.cpp: D = 1 (the shift and the residual measures unscaled);
#   border-rho-off   include/hven/detail/kkt/bordered_eqp.h: solve_bordered_eqp's shifted-K0 gate never refuses;
#   rho-gate-off     include/hven/detail/kkt/kkt_calls.h: shifted_solve_refused() never refuses (eliminated EQP, SSN,
#                    eliminated verdict refinement);
#   refine-off       include/hven/detail/kkt/kkt_calls.h: refine_shifted_solve takes no step;
#   terminal-off     src/qp/qp_engine.cpp: the mu-free terminal refinement and the terminal polish adopt a refused
#                    shifted answer (2 sites);
#   ipqp-map-off     src/interior/kkt_factorization.cpp (MKL branch): kDenseSingular no longer maps to NumericalIssue;
#   retake-drop-off  src/linear/symmetric_factor_mkl.cpp: a failed retake fork keeps the dense numerics.
# Each built (Release, legs/build.sh's flags; the five test executables below) at build-dn1de2/fp-<name>; the head's
# binaries are build-dn1de2/rel's. Filters: hven_tests 'DenseArm*', hven_qp_walk_seam_tests 'QpDenseArm.*',
# hven_drivers_tests 'DenseArmEngines.*', hven_fault_injection_tests 'DenseArmFallbackFault.*', hven_ipqp_seam_tests
# the two IpqpSeamTest dense reads. Criterion: head passes every test; each mutation fails EXACTLY its declared set
# (WANT below, from the declaration).
# usage: failproof.sh <code-head sha> <evidence-dir>   Run as: flock /tmp/box-build.lock bash failproof.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1d
L=$WT/docs/notes/data/2026-11-m8-dn1d-singular/r2/legs
TREES=${TREES:-$WT/build-dn1de2}
HEAD_SHA=${1:?code head sha}
EV=${2:?evidence dir}
MUTS=${MUTS:-policy-off detector-off shift-off equil-off border-rho-off rho-gate-off refine-off terminal-off ipqp-map-off retake-drop-off}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
export MKL_NUM_THREADS=1
IPQP_FILTER='IpqpSeamTest.AFlaggedTerminalReadIsPerturbedAndEscapesNumerical:IpqpSeamTest.AnExactlySingularDenseFactorIsANumericReadNotASymbolicFailure'
run_filters() { # <build dir> <name>
  "$1/tests/hven_tests" --gtest_filter='DenseArm*' > "$EV/run-$2-hven_tests.txt" 2>&1; echo "$2 hven_tests rc=$?"
  "$1/tests/hven_qp_walk_seam_tests" --gtest_filter='QpDenseArm.*' > "$EV/run-$2-walk_seam.txt" 2>&1; echo "$2 walk_seam rc=$?"
  "$1/tests/drivers/hven_drivers_tests" --gtest_filter='DenseArmEngines.*' > "$EV/run-$2-drivers.txt" 2>&1; echo "$2 drivers rc=$?"
  "$1/tests/hven_fault_injection_tests" --gtest_filter='DenseArmFallbackFault.*' > "$EV/run-$2-fault.txt" 2>&1; echo "$2 fault rc=$?"
  "$1/tests/hven_ipqp_seam_tests" --gtest_filter="$IPQP_FILTER" > "$EV/run-$2-ipqp.txt" 2>&1; echo "$2 ipqp rc=$?"
}
for M in $MUTS; do
  S=$TREES/fp-src-$M
  rm -rf "$S" "$TREES/fp-$M"
  mkdir -p "$S"
  git archive "$HEAD_SHA" | tar -x -C "$S" || { echo ARCHIVE_FAIL; exit 1; }
  rm -rf "$S/dep/eigen" "$S/dep/fmt"
  cp -a dep/eigen "$S/dep/eigen" && cp -a dep/fmt "$S/dep/fmt" || exit 1
  case $M in
    policy-off)      F=include/hven/detail/kkt/kkt_calls.h ;;
    detector-off)    F=src/linear/dense_session.cpp ;;
    shift-off)       F=src/kkt/kkt_calls.cpp ;;
    equil-off)       F=src/kkt/kkt_calls.cpp ;;
    border-rho-off)  F=include/hven/detail/kkt/bordered_eqp.h ;;
    rho-gate-off)    F=include/hven/detail/kkt/kkt_calls.h ;;
    refine-off)      F=include/hven/detail/kkt/kkt_calls.h ;;
    terminal-off)    F=src/qp/qp_engine.cpp ;;
    ipqp-map-off)    F=src/interior/kkt_factorization.cpp ;;
    retake-drop-off) F=src/linear/symmetric_factor_mkl.cpp ;;
    *) echo "UNKNOWN_MUTATION $M"; exit 1 ;;
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
    'border-rho-off': ('        k0_shift_scale != nullptr &&\n',
                       '        false && k0_shift_scale != nullptr &&\n', 1),
    'rho-gate-off': ('return k.dense_shifted && !(shifted_residual_ratio(K, rhs, k, y) <= kEqpShiftedResidualTol);',
                     'return false && k.dense_shifted &&\n           !(shifted_residual_ratio(K, rhs, k, y) <= kEqpShiftedResidualTol);', 1),
    'refine-off': ('while (spent < kEqpShiftRefineSteps) {', 'while (false && spent < kEqpShiftRefineSteps) {', 1),
    'terminal-off': ('local.dense_shifted && r.residual_refused', 'false && r.residual_refused', 2),
    'ipqp-map-off': ('        return Eigen::Success;\n    }\n'
                     '    // The dense arm\'s exactly singular report (kReport) is a numeric outcome,\n'
                     '    // never a symbolic failure, on either backend.\n'
                     '    if (backend_code == hven::linear::kDenseSingular) {\n',
                     '        return Eigen::Success;\n    }\n'
                     '    // The dense arm\'s exactly singular report (kReport) is a numeric outcome,\n'
                     '    // never a symbolic failure, on either backend.\n'
                     '    if (false && backend_code == hven::linear::kDenseSingular) {\n', 1),
    'retake-drop-off': ('            dense_->drop_numerics();\n            ++counters_.factorize_count;\n',
                        '            ++counters_.factorize_count;\n', 1),
}[m]
print(m, 'sites', s.count(site), 'expected', count)
if s.count(site) != count:
    sys.exit(1)
open(p, 'w').write(s.replace(site, new))
PY
  diff <(git show "$HEAD_SHA:$F") "$S/$F" > "$EV/failproof-mutation-$M.diff"
  cat "$EV/failproof-mutation-$M.diff"
  SRC=$S BUILD_ROOT=$TREES bash "$L/build.sh" Release fp-$M hven_tests hven_qp_walk_seam_tests hven_drivers_tests \
    hven_fault_injection_tests hven_ipqp_seam_tests > "$EV/failproof-build-$M.log" 2>&1 || { echo "FP_BUILD_FAIL $M"; tail -20 "$EV/failproof-build-$M.log"; exit 1; }
  run_filters "$TREES/fp-$M" "$M"
done
run_filters "${HEAD_TREE:-$TREES/rel}" head
MUTS="$MUTS" python3 - "$EV" <<'PY' | tee "$EV/failproof-verdict.txt"
import os, re, sys
ev = sys.argv[1]
def res(name):
    out = {}
    for part in ('hven_tests', 'walk_seam', 'drivers', 'fault', 'ipqp'):
        t = open(f'{ev}/run-{name}-{part}.txt').read()
        out.update({m.group(2): m.group(1) for m in re.finditer(r'^\[\s+(OK|FAILED)\s+\] (\w+\.\w+) \(', t, re.M)})
    return out
failed = lambda d: {k for k, v in d.items() if v == 'FAILED'}
h = res('head')
want = {
    # (from the dev measurement before the declaration; the declaration lists the same sets)
    'policy-off': {'DenseArmEngines.SqpShiftsASingularKktOnTheDenseArm',
                   'DenseArmEngines.TheSsnRefusesAShiftedNewtonStepThatMissesK',
                   'QpDenseArm.ANearNullPivotIsShiftedToTheSparseAnswer',
                   'QpDenseArm.ANearlyInconsistentFaceIsRefusedByRhoWithItsRowsMet',
                   'QpDenseArm.ARankDeficientWorkingSetAtMuZeroIsShiftedAndVerified',
                   'QpDenseArm.AShiftedK0BorderAnswerMeetingRhoIsUsed',
                   'QpDenseArm.AShiftedK0BorderAnswerMissingRhoFallsToTheEliminatedPath',
                   'QpDenseArm.AnExactZeroColumnIsShiftedNotRetaken',
                   'QpDenseArm.FactorizeCheckedRetriesOnceThenRetakesOnThePlatform',
                   'QpDenseArm.FactorizeCheckedShiftsAFlaggedFactor',
                   'QpDenseArm.SqpKktOptionsReportUnderADensePolicyOnly',
                   'QpDenseArm.TheAllinitcShapeMuFreeSystemIsAnsweredOnTheDenseArm',
                   'QpDenseArm.TheMuFreeTerminalRefinementRejectsARefusedShiftedAnswer',
                   'QpDenseArm.TheRetryAnswersAShiftThatIsItselfFlagged',
                   'QpDenseArm.TheTerminalPolishRejectsARefusedShiftedAnswer',
                   'QpDenseArm.TheUnscaledMonitorStallsWhereTheEquilibratedOneConverges'},
    'detector-off': {'DenseArmEngines.SqpShiftsASingularKktOnTheDenseArm',
                     'DenseArmFallbackFault.AFailedRetakeAfterAFlaggedFactorLeavesNothingSolvable',
                     'DenseArmReport.FlagsASingularToWorkingPrecisionFactorAndKeepsItsNumerics',
                     'DenseArmReport.RefusedUnderNeverAndCarriedByHandles',
                     'DenseArmReport.TheCatastrophicMeshFactorIsFlaggedAndTheShiftedSolvePasses',
                     'DenseArmReport.TheDetectorBracketsBothThresholds',
                     'DenseArmReport.TheEquilibratedRefactorReportsPerturbedPivotsAndSolvesInASpace',
                     'QpDenseArm.ANearNullPivotIsShiftedToTheSparseAnswer',
                     'QpDenseArm.FactorizeCheckedShiftsAFlaggedFactor',
                     'QpDenseArm.TheAllinitcShapeMuFreeSystemIsAnsweredOnTheDenseArm',
                     'QpDenseArm.TheRetryAnswersAShiftThatIsItselfFlagged',
                     'QpDenseArm.TheUnscaledMonitorStallsWhereTheEquilibratedOneConverges'},
    'shift-off': {'DenseArmEngines.SqpShiftsASingularKktOnTheDenseArm',
                  'DenseArmEngines.TheSsnRefusesAShiftedNewtonStepThatMissesK',
                  'QpDenseArm.ANearNullPivotIsShiftedToTheSparseAnswer',
                  'QpDenseArm.ANearlyInconsistentFaceIsRefusedByRhoWithItsRowsMet',
                  'QpDenseArm.ARankDeficientWorkingSetAtMuZeroIsShiftedAndVerified',
                  'QpDenseArm.AShiftedK0BorderAnswerMeetingRhoIsUsed',
                  'QpDenseArm.AShiftedK0BorderAnswerMissingRhoFallsToTheEliminatedPath',
                  'QpDenseArm.AnExactZeroColumnIsShiftedNotRetaken',
                  'QpDenseArm.FactorizeCheckedRetriesOnceThenRetakesOnThePlatform',
                  'QpDenseArm.FactorizeCheckedShiftsAFlaggedFactor',
                  'QpDenseArm.TheAllinitcShapeMuFreeSystemIsAnsweredOnTheDenseArm',
                  'QpDenseArm.TheMuFreeTerminalRefinementRejectsARefusedShiftedAnswer',
                  'QpDenseArm.TheRetryAnswersAShiftThatIsItselfFlagged',
                  'QpDenseArm.TheTerminalPolishRejectsARefusedShiftedAnswer',
                  'QpDenseArm.TheUnscaledMonitorStallsWhereTheEquilibratedOneConverges'},
    'equil-off': {'DenseArmEngines.SqpShiftsASingularKktOnTheDenseArm',
                  'DenseArmEngines.TheSsnRefusesAShiftedNewtonStepThatMissesK',
                  'QpDenseArm.TheAllinitcShapeMuFreeSystemIsAnsweredOnTheDenseArm',
                  'QpDenseArm.TheUnscaledMonitorStallsWhereTheEquilibratedOneConverges'},
    'border-rho-off': {'QpDenseArm.AShiftedK0BorderAnswerMissingRhoFallsToTheEliminatedPath'},
    'rho-gate-off': {'DenseArmEngines.TheSsnRefusesAShiftedNewtonStepThatMissesK',
                     'QpDenseArm.ANearlyInconsistentFaceIsRefusedByRhoWithItsRowsMet',
                     'QpDenseArm.ARankDeficientWorkingSetAtMuZeroIsShiftedAndVerified',
                     'QpDenseArm.AShiftedK0BorderAnswerMissingRhoFallsToTheEliminatedPath',
                     'QpDenseArm.TheMuFreeTerminalRefinementRejectsARefusedShiftedAnswer',
                     'QpDenseArm.TheTerminalPolishRejectsARefusedShiftedAnswer'},
    'refine-off': {'QpDenseArm.TheUnscaledMonitorStallsWhereTheEquilibratedOneConverges'},
    'terminal-off': {'QpDenseArm.TheMuFreeTerminalRefinementRejectsARefusedShiftedAnswer'},
    'ipqp-map-off': {'IpqpSeamTest.AnExactlySingularDenseFactorIsANumericReadNotASymbolicFailure'},
    'retake-drop-off': {'DenseArmFallbackFault.AFailedRetakeAfterAFlaggedFactorLeavesNothingSolvable'},
}
ok = len(h) > 0 and not failed(h)
print('head           :', len(h), 'tests, failed', sorted(failed(h)) or 'none')
for m in os.environ['MUTS'].split():
    r = res(m)
    w = want.get(m)
    good = w is not None and len(w) >= 1 and set(r) == set(h) and failed(r) == w
    print(f'{m:15s}:', len(r), 'tests, failed', sorted(failed(r)), '|', 'OK' if good else 'NOT AS DECLARED')
    ok = ok and good
print('FAILPROOF_AS_DECLARED' if ok else 'FAILPROOF_NOT_AS_DECLARED')
sys.exit(0 if ok else 1)
PY
rc=${PIPESTATUS[0]}
echo "FOREGROUND_END $(date -Is)"
exit $rc
