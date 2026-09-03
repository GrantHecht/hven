hven M6 W2 T4 fix round 1 -- mutation runs (both configs).

apply-mutant.py rewrites src/drivers/sqp_driver.cpp from a pristine copy; each run is
    ninja -C build-w02/<cfg> hven_sqp_tests
    ./build-w02/<cfg>/tests/sqp/hven_sqp_tests \
        --gtest_filter='SqpDriverRestoration*:SqpDriverRadius*:SqpDriverSsnMode*:RestorationModel*'
(49 tests). The files here are the FAILED/PASSED lines of each run.

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
                           -> SURVIVES both configs. Declared: no fixture in this
                              repository can offer a candidate with finite values and a
                              non-finite Jacobian without also poisoning the ENTRY
                              bundle (the same non-screening hole is pre-existing at the
                              entry), and every construction tried exits kNumericalError
                              before restoration is ever requested. Registered for T7,
                              where an HVEN_TESTING seam is the mechanism.
