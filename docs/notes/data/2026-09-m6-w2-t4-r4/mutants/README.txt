hven M6 W2 T4 fix rounds 1-2 -- mutation runs (both configs).

apply-mutant.py rewrites src/drivers/sqp_driver.cpp from a pristine copy; each run is
    ninja -C build-w02/<cfg> hven_sqp_tests
    ./build-w02/<cfg>/tests/sqp/hven_sqp_tests \
        --gtest_filter='SqpDriverRestoration*:SqpDriverRadius*:SqpDriverSsnMode*:RestorationModel*'
(49 tests through fix round 1; 50 from fix round 2's
AJacobianPoisonedCandidateIsRefusedNotTaken onward). The files here are the FAILED/PASSED
lines of each run.

  M1_qp_mode_gate          the candidate block gated on qp_mode == kWalk (the scope
                           decision owner ruling Q-O3 overturned)
                           -> DIES both configs: TheSeedIsNotGatedOnTheQpMode,
                              TheExhaustedLadderSiteSeedsFromItsOwnStep,
                              TheGuardAndThePhaseRunOnTheCallerScale
  M2_no_reclassification   ++evals_full / --evals_values removed at the trial site
                           -> DIES both configs: TheSeedsEvaluationIsCountedExactlyOnce
  M3_fresh_full_query      refresh_derivatives replaced by a fresh seam.eval_nlp
                           -> DIES both configs: TheSeedsEvaluationIsCountedExactlyOnce
  M4_no_zero_step_check    the p_elastic != 0 conjunct removed
                           -> DIES both configs: AZeroElasticStepIsNotACandidate
  M5_no_candidate_anywhere every site's candidate suppressed (the F-2 mutation)
                           -> DIES both configs: the six "taken" pins; the three
                              "refused" pins and every pre-existing test stay green
  M6_engine_scale_guard    the guard reverted to the ENGINE-scale h on both sides
                           -> DIES both configs: TheGuardAndThePhaseRunOnTheCallerScale
  M7_no_jacobian_screen    jacobian_values_finite() dropped from both arms
                           -> NO LONGER A SURVIVOR (fix round 2): the round 1 declaration
                              (no fixture can offer a finite-values/non-finite-Jacobian
                              candidate) was refuted by an ORDINAL-KEYED poison --
                              JacobianPoisonedAtOrdinalModel poisons the k-th DISTINCT
                              point eval_jac_e is queried at, not a spatial predicate, so
                              the candidate's OWN Jacobian (not its values bundle) goes
                              non-finite without touching the entry. DIES both configs:
                              AJacobianPoisonedCandidateIsRefusedNotTaken (k = 20 both
                              configs on InfeasibleCircleLineModel(2.0, 2.0),
                              ws_algebra = kRefactorize).

  M8_nan_h_discriminator   the arm discriminator taken on the MEASURED h instead of on the
                           OFFER's shape (the pre-fix routing this file's A1 change replaced)
                           -> DIES both configs: ANonFiniteEvaluationAtTheCandidateIsRefused
                              (evals_full reads 4 against the pin's 3 -- the NaN-h trial goes
                              down the unmeasured arm and spends a fresh full query there).
                              ADDED IN W2 T5's FIX ROUND 1 (registered item Z-6) and run against
                              THAT tree: pass its ref (8f4f062) as apply-mutant.py's optional
                              second argument, since the default a14ad96 predates T5's test file.
