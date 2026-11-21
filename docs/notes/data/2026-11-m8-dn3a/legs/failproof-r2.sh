#!/usr/bin/env bash
# M8 DN-3a r2 (../DECLARATION-r2.txt P4): legs/failproof.sh's structure (multi-site mutations of the code head's
# `git archive` export, every site at its declared count or fail closed) re-pointed for r2: the r2 mutation table, a
# fourth test binary (hven_ipqp_seam_tests, the IPQP dense-solve injector's pins), outputs in the evidence directory.
# Mutations:
#   thread-pin-off              dense_session.cpp: the four DenseArmThreadPin sites take MklThreadScope(opts_.threads);
#   check-never-refuses         kkt_calls.h: dense_solve_check_refused's final verdict is "pass";
#   refusal-threshold-ignored   kkt_calls.h: the verdict compares against kDenseSolveResidualTol, not refuse_tol;
#   refine-off                  kkt_calls.h: the check's refinement takes no step;
#   refine-always               kkt_calls.h: the check refines a solve that already passes (no early return);
#   dense-only-off              kkt_calls.h: unshifted_dense_solve_refused checks platform-session answers too;
#   ssn-unwired                 ssn_engine.cpp: the SSN step is not checked;
#   ssn-fold-off                sqp_solver.cpp: charge_ssn_subproblem_cost drops the refusal count;
#   ipqp-unwired                ipqp_engine.cpp: the IPQP solves are not checked;
#   ipqp-two-threshold-off      ipqp_engine.cpp: the IPQP refuses above kDenseSolveResidualTol (r1's single threshold);
#   ipqp-check-after-unscale    ipqp_engine.cpp: the check runs after the Ruiz unscale (w.sol *= dscale);
#   ipqp-response-estimate-off  ipqp_engine.cpp: a refused mu-estimate solve is used;
#   ipqp-response-affine-off    ipqp_engine.cpp: a refused affine solve is used;
#   ipqp-response-corrector-off ipqp_engine.cpp: a refused corrector solve is used;
#   ipqp-reladder-off           ipqp_engine.cpp: a refused affine/corrector solve ends the tier (r1: kNumerical at once);
#   gram-dense-clause-off       ws_independence.h: the Gram refusal ignores the dense arm's inertia;
#   gram-unwired                ws_independence.cpp: the rank repair keys on the old perturbed-pivot test (UNKILLED:
#                               a fault path no fixture reaches; the wiring is by inspection).
# Each built Release (legs/build.sh's flags; the four targets below) at <TREES>/fp2-<name>; the head's binaries are
# build-dn3a/rel's. Output: the failing names per arm in <evidence-dir>/failing-<arm>.txt.
# usage: failproof-r2.sh <code-head sha> <evidence-dir>   Run as: flock /tmp/box-build.lock bash failproof-r2.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-acfc8f7d568381148
L=$WT/docs/notes/data/2026-11-m8-dn3a/legs
TREES=${TREES:-$WT/build-dn3a}
HEADTREE=${HEADTREE:-$WT/build-dn3a/rel}
HEAD_SHA=${1:?code head sha}
EV=${2:?evidence dir}
MUTS=${MUTS:-thread-pin-off check-never-refuses refusal-threshold-ignored refine-off refine-always dense-only-off ssn-unwired ssn-fold-off ipqp-unwired ipqp-two-threshold-off ipqp-check-after-unscale ipqp-response-estimate-off ipqp-response-affine-off ipqp-response-corrector-off ipqp-reladder-off gram-dense-clause-off gram-unwired}
F_LIN='DenseArmThreads.*:DenseSolveCheck.*:DenseArm.*:DenseArmReport.*'
F_DRV='DenseArmEngines.*'
F_SQP='IpqpDenseSolveCheck.*'
F_SEAM='IpqpDenseSolveSeamTest.*'
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
export MKL_NUM_THREADS=1
run_filter() { # <build dir> <name>   (fails closed when a binary is absent)
  : > "$EV/run-$2.txt"
  for pair in "tests/hven_tests|$F_LIN" "tests/drivers/hven_drivers_tests|$F_DRV" "tests/sqp/hven_sqp_tests|$F_SQP" \
              "tests/hven_ipqp_seam_tests|$F_SEAM"; do
    b=${pair%%|*}; f=${pair#*|}
    [ -x "$1/$b" ] || { echo "BINARY_MISSING $1/$b"; exit 8; }
    "$1/$b" --gtest_filter="$f" >> "$EV/run-$2.txt" 2>&1; echo "$2 $b rc=$?"
  done
  grep -E '^\[  FAILED  \] [A-Za-z0-9/]+\.[A-Za-z0-9/]+(,| ).*\([0-9]+ ms\)$' "$EV/run-$2.txt" | awk '{print $4}' \
    | sed 's/,$//' | sort -u > "$EV/failing-$2.txt"
  echo "$2 passed=$(grep -hE '^\[  PASSED  \]' "$EV/run-$2.txt" | awk '{s+=$4} END{print s+0}') failing=$(wc -l < "$EV/failing-$2.txt"): $(tr '\n' ' ' < "$EV/failing-$2.txt")"
}
for M in $MUTS; do
  S=$TREES/fp2-src-$M
  rm -rf "$S" "$TREES/fp2-$M"
  mkdir -p "$S"
  git archive "$HEAD_SHA" | tar -x -C "$S" || { echo ARCHIVE_FAIL; exit 1; }
  rm -rf "$S/dep/eigen" "$S/dep/fmt"
  cp -a dep/eigen "$S/dep/eigen" && cp -a dep/fmt "$S/dep/fmt" || exit 1
  python3 - "$S" "$M" <<'PY' || { echo MUTATION_FAIL; exit 1; }
import sys
root, m = sys.argv[1], sys.argv[2]
KC = 'include/hven/detail/kkt/kkt_calls.h'
IP = 'src/qp/ipqp_engine.cpp'
UNSCALE = ('        if (iopts.ruiz_equilibration) {\n'
           '            w.sol.array() *= w.dscale.array();\n'
           '        }\n')
table = {
    'thread-pin-off': [('src/linear/dense_session.cpp', 'const DenseArmThreadPin threads;',
                        'const MklThreadScope threads(opts_.threads);', 4)],
    'check-never-refuses': [(KC, '    return !(rho <= refuse_tol);\n}', '    return false;\n}', 1)],
    'refusal-threshold-ignored': [(KC, '    return !(rho <= refuse_tol);\n}',
                                   '    (void)refuse_tol;\n    return !(rho <= kDenseSolveResidualTol);\n}', 1)],
    'refine-off': [(KC, 'for (Index step = 0; step < kEqpShiftRefineSteps; ++step) {',
                    'for (Index step = 0; step < 0; ++step) {', 1)],
    'refine-always': [(KC, '    double rho = dense_residual_ratio(K, rhs, y, r);\n'
                           '    if (rho <= kDenseSolveResidualTol) {\n'
                           '        return false;\n'
                           '    }\n',
                       '    double rho = dense_residual_ratio(K, rhs, y, r);\n', 1)],
    'dense-only-off': [(KC, '    if (arm != hven::linear::FactorArm::kDense || k.dense_shifted) {',
                        '    if (k.dense_shifted) {', 1)],
    'ssn-unwired': [('src/qp/ssn_engine.cpp',
                     'dw_dense_refused = detail::unshifted_dense_solve_refused(\n'
                     '                    k_, rhs, kkt_, fo.arm, dw, dense_check_r_, dense_check_c_);',
                     '(void)fo;', 1)],
    'ssn-fold-off': [('src/drivers/sqp_solver.cpp',
                      '    total.dense_shifted_solve_refusals += res.dense_shifted_solve_refusals;\n'
                      '    total.dense_solve_residual_refusals += res.dense_solve_residual_refusals;\n',
                      '    total.dense_shifted_solve_refusals += res.dense_shifted_solve_refusals;\n', 1)],
    'ipqp-unwired': [(IP, '        if (kkt_.arm() == hven::linear::FactorArm::kDense) {\n#ifdef HVEN_TESTING',
                      '        if (false) {\n#ifdef HVEN_TESTING', 1)],
    'ipqp-two-threshold-off': [(IP, 'kkt_.matrix(), w.rhs, detail::kDenseResidualRatioSingular,',
                                'kkt_.matrix(), w.rhs, detail::kDenseSolveResidualTol,', 1)],
    'ipqp-check-after-unscale': [(IP, '        kkt_.solve(w.rhs, w.sol);\n        bool refused = false;\n',
                                  '        kkt_.solve(w.rhs, w.sol);\n' + UNSCALE + '        bool refused = false;\n', 1),
                                 (IP, UNSCALE + '        return refused;', '        return refused;', 1)],
    'ipqp-response-estimate-off': [(IP, 'ok = !refused && w.sol.allFinite();', 'ok = w.sol.allFinite();', 1)],
    'ipqp-response-affine-off': [(IP, 'if (affine_refused) {', 'if ((void)affine_refused, false) {', 1)],
    'ipqp-response-corrector-off': [(IP, 'if (corrector_refused) {', 'if ((void)corrector_refused, false) {', 1)],
    'ipqp-reladder-off': [(IP, 'read = factorize_with_ladder(rho_sched, rho_dem, delta, /*solve_refused=*/true);',
                           'read = InertiaRead::kPerturbed;', 1)],
    'gram-dense-clause-off': [('include/hven/detail/qp/ws_independence.h',
                               '    if (fo.arm == hven::linear::FactorArm::kDense) {\n        return !(',
                               '    if (false) {\n        return !(', 1)],
    'gram-unwired': [('src/qp/ws_independence.cpp', '                    if (rank_gram_factor_refused(fo, mr)) {',
                      '                    if (fo.status != hven::linear::FactorizeOutcome::Status::kOk ||\n'
                      '                        (fo.inertia.perturbed_pivots.has_value() &&\n'
                      '                         *fo.inertia.perturbed_pivots != 0)) {', 1)],
}
for path, site, new, want in table[m]:
    p = f'{root}/{path}'
    s = open(p).read()
    n = s.count(site)
    if n != want:
        sys.exit(f'{m}: site in {path} occurs {n} times, expected {want}')
    s = s.replace(site, new)
    open(p, 'w').write(s)
print(f'MUTATED {m}')
PY
  SRC=$S BUILD_ROOT=$TREES bash "$L/build.sh" Release "fp2-$M" hven_tests hven_drivers_tests hven_sqp_tests hven_ipqp_seam_tests > "$EV/build-$M.txt" 2>&1
  rc=$?; echo "build $M rc=$rc"; [ $rc -ne 0 ] && { tail -20 "$EV/build-$M.txt"; exit 1; }
  run_filter "$TREES/fp2-$M" "$M"
done
run_filter "$HEADTREE" head
# The criterion (../DECLARATION-r2.txt P4): the head runs HEAD_COUNT tests and fails none; each mutation fails EXACTLY
# its declared set (WANT, measured on a dev run before the declaration).
HEAD_COUNT=57
WANT_thread_pin_off="DenseArmThreads.EveryDenseLapackCallRunsAtOneThread DenseArmThreads.ThePinHoldsAtThreadsZeroUnderAGlobalCountAboveOne"
WANT_check_never_refuses="DenseArmEngines.TheSsnRefusesAnUnshiftedDenseNewtonStepThatMissesK DenseSolveCheck.ANonFiniteSolveIsRefusedAndRhoIsTheInfNormRatio DenseSolveCheck.AnUnflaggedIllConditionedDenseSolveIsRefused DenseSolveCheck.TheRefusalThresholdIsTheConsumersAndRefinementStillRuns IpqpDenseSolveSeamTest.ARefusedAffineSolveTakesOneLadderRungAndTheSolveFinishes IpqpDenseSolveSeamTest.ARefusedCorrectorSolveTakesOneLadderRungAndTheSolveFinishes IpqpDenseSolveSeamTest.ARefusedMuEstimateSolveTakesTheEstimatesFallback IpqpDenseSolveSeamTest.RefusalsUntilTheLadderIsExhaustedEndTheTierNumerical"
WANT_refusal_threshold_ignored="DenseSolveCheck.TheRefusalThresholdIsTheConsumersAndRefinementStillRuns IpqpDenseSolveCheck.LateInteriorSolvesBetweenTheThresholdsAreNotRefused"
WANT_refine_off="DenseSolveCheck.RefinementRescuesASolveThatMissesAtFirst DenseSolveCheck.TheRefusalThresholdIsTheConsumersAndRefinementStillRuns"
WANT_refine_always="DenseSolveCheck.TheRefusalThresholdIsTheConsumersAndRefinementStillRuns"
WANT_dense_only_off="DenseArmEngines.TheSsnRefusesAnUnshiftedDenseNewtonStepThatMissesK DenseSolveCheck.OnlyUnshiftedDenseNumericsAreChecked"
WANT_ssn_unwired="DenseArmEngines.TheSsnRefusesAnUnshiftedDenseNewtonStepThatMissesK"
WANT_ssn_fold_off="DenseArmEngines.TheSsnRefusesAnUnshiftedDenseNewtonStepThatMissesK"
WANT_ipqp_unwired="IpqpDenseSolveSeamTest.ARefusedAffineSolveTakesOneLadderRungAndTheSolveFinishes IpqpDenseSolveSeamTest.ARefusedCorrectorSolveTakesOneLadderRungAndTheSolveFinishes IpqpDenseSolveSeamTest.ARefusedMuEstimateSolveTakesTheEstimatesFallback IpqpDenseSolveSeamTest.RefusalsUntilTheLadderIsExhaustedEndTheTierNumerical"
WANT_ipqp_two_threshold_off="IpqpDenseSolveCheck.LateInteriorSolvesBetweenTheThresholdsAreNotRefused"
WANT_ipqp_check_after_unscale="IpqpDenseSolveCheck.LateInteriorSolvesBetweenTheThresholdsAreNotRefused IpqpDenseSolveCheck.TheCheckReadsTheEquilibratedSystem IpqpDenseSolveSeamTest.ARefusedAffineSolveTakesOneLadderRungAndTheSolveFinishes IpqpDenseSolveSeamTest.ARefusedCorrectorSolveTakesOneLadderRungAndTheSolveFinishes IpqpDenseSolveSeamTest.ARefusedMuEstimateSolveTakesTheEstimatesFallback IpqpDenseSolveSeamTest.TheControlSolveRefusesNothing"
WANT_ipqp_response_estimate_off="IpqpDenseSolveSeamTest.ARefusedMuEstimateSolveTakesTheEstimatesFallback"
WANT_ipqp_response_affine_off="IpqpDenseSolveSeamTest.ARefusedAffineSolveTakesOneLadderRungAndTheSolveFinishes IpqpDenseSolveSeamTest.RefusalsUntilTheLadderIsExhaustedEndTheTierNumerical"
WANT_ipqp_response_corrector_off="IpqpDenseSolveSeamTest.ARefusedCorrectorSolveTakesOneLadderRungAndTheSolveFinishes"
WANT_ipqp_reladder_off="IpqpDenseSolveSeamTest.ARefusedAffineSolveTakesOneLadderRungAndTheSolveFinishes IpqpDenseSolveSeamTest.ARefusedCorrectorSolveTakesOneLadderRungAndTheSolveFinishes IpqpDenseSolveSeamTest.RefusalsUntilTheLadderIsExhaustedEndTheTierNumerical"
WANT_gram_dense_clause_off="DenseSolveCheck.TheRankGramRefusesADenseFactorThatIsNotPositiveDefinite"
WANT_gram_unwired=""
verdict=FAILPROOF_AS_DECLARED
heads=$(grep -hE '^\[  PASSED  \]' "$EV/run-head.txt" | awk '{s+=$4} END{print s+0}')
[ -s "$EV/failing-head.txt" ] && verdict=FAILPROOF_HEAD_FAILS
[ "$heads" = "$HEAD_COUNT" ] || verdict="FAILPROOF_HEAD_COUNT($heads)"
for M in $MUTS; do
  v=WANT_${M//-/_}
  got=$(tr '\n' ' ' < "$EV/failing-$M.txt" | sed 's/ $//')
  if [ "$got" != "${!v}" ]; then verdict="FAILPROOF_MISMATCH($M)"; echo "MISMATCH $M got=[$got] want=[${!v}]"; fi
done
echo "$verdict"
echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = FAILPROOF_AS_DECLARED ]
