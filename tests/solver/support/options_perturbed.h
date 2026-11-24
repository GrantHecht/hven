// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// An IpmOptions and a SqpOptions with EVERY field set to a value other than its default, for the
// options-dump fixtures (tests/solver/fixtures/options_dump/). The fixtures were captured from the
// hand-written dump before the registry replaced it (M8 AP-4 G1, base 0cecdc6d0), from these exact
// values; the dump test compares the registry's output against those bytes. The values are not
// validated (`phases` and the cross-field orderings need not hold): a dump records what it is
// handed.
//
// The dump test also asserts that no line of a perturbed dump equals the same line of the default
// dump, so a field added to a struct without a perturbation here fails that test by name.

#include <memory>

#include <hven/detail/sqp/globalization/funnel.h>
#include <hven/ipm/ipm_options.h>
#include <hven/sqp/sqp_options.h>

namespace hven::test {

inline IpmOptions ipm_all_perturbed() {
    IpmOptions o;
    o.common.optimality_tol = 2e-7;
    o.common.feasibility_tol = 3e-7;
    o.common.max_iterations = 777;
    o.common.max_factorizations = 123;
    o.common.time_limit_seconds = 4.5;
    o.common.verbosity = Verbosity::kDetail;
    o.common.threads = 5;
    o.common.start_level = StartLevel::kHot;
    o.common.infinite_bound = 1e19;
    o.common.return_best = ReturnBest::kOnAnyNonOptimal;
    o.phases = {IpmPhase::kSolve, IpmPhase::kOptimize};
    o.termination.complementarity_tol = 5e-7;
    o.termination.acceptable_iterations = 11;
    o.termination.acceptable_stationarity_tol = 2e-2;
    o.termination.acceptable_equality_tol = 2e-3;
    o.termination.acceptable_inequality_tol = 3e-3;
    o.termination.acceptable_complementarity_tol = 4e-3;
    o.termination.divergence_stationarity_tol = 1e14;
    o.termination.divergence_equality_tol = 2e14;
    o.termination.divergence_inequality_tol = 3e14;
    o.termination.divergence_complementarity_tol = 4e14;
    o.barrier_initial = 2e-3;
    o.barrier_max = 50.0;
    o.barrier_min = 1e-10;
    o.fraction_to_boundary = 0.95;
    o.bound_push = 2e-3;
    o.bound_interval_push = 2e-2;
    o.fixed_variable_treatment = IpmFixedVariableTreatment::kRelaxBounds;
    o.bound_relax_factor = 2e-8;
    o.objective_scale = 2.5;
    o.linear.ordering = IpmOrdering::kMinimumDegree;
    o.linear.pivot_strategy = IpmPivotStrategy::kOneByOne;
    o.linear.weighted_matching = false;
    o.linear.matrix_scaling = true;
    o.linear.pivot_perturbation_exponent = 7;
    o.linear.max_refinement_steps = 2;
    o.linear.solve_parallelism = IpmSolveParallelism::kSequential;
    o.linear.conditional_reproducibility = true;
    o.linear.force_analysis = true;
    o.linear.dense_factorization = linear::DensePolicy::kNever;
    o.linear.accelerate_zero_tol = 1e-9;
    o.linear.max_dense_dimension = 64;
    o.advanced.acceptance_mode = IpmAcceptanceMode::kMerit;
    o.advanced.merit_penalty_rule = IpmMeritPenaltyRule::kFlexible;
    o.advanced.barrier_governor_mode = IpmBarrierGovernorMode::kClassicAdaptive;
    o.advanced.never_monotone = true;
    o.advanced.restoration_mode = IpmRestorationMode::kProximalSwitch;
    o.advanced.inertia_mode = IpmInertiaMode::kProximalRegularization;
    o.advanced.max_second_order_corrections = 3;
    o.advanced.max_extended_backtracks = 2;
    o.advanced.watchdog = true;
    o.advanced.max_restoration_entries = 5;
    o.advanced.max_line_search_backtracks = 4;
    o.advanced.backtrack_divisor = 3.0;
    o.advanced.solve_phase_mode = IpmSolvePhaseMode::kOptimize;
    o.advanced.optimize_barrier_mode = IpmBarrierMode::kProbe;
    o.advanced.solve_barrier_mode = IpmBarrierMode::kProbe;
    o.advanced.optimize_line_search_mode = IpmLineSearchMode::kL1;
    o.advanced.solve_line_search_mode = IpmLineSearchMode::kLagrangian;
    o.advanced.step_length_rule = IpmStepLengthRule::kAllMinimum;
    o.advanced.inertia_correction_initial = 2e-5;
    o.advanced.inertia_correction_increase = 9.0;
    o.advanced.inertia_correction_decrease = 0.25;
    o.advanced.max_inertia_correction_attempts = 12;
    o.advanced.skip_unperturbed_attempts = false;
    o.advanced.negative_slack_reset = 2e-12;
    return o;
}

inline SqpOptions sqp_all_perturbed() {
    SqpOptions o;
    o.common.optimality_tol = 2e-7;
    o.common.feasibility_tol = 3e-7;
    o.common.max_iterations = 77;
    o.common.max_factorizations = 123;
    o.common.time_limit_seconds = 4.5;
    o.common.verbosity = Verbosity::kDetail;
    o.common.threads = 3;
    o.common.start_level = StartLevel::kHot;
    o.common.infinite_bound = 1e19;
    o.common.return_best = ReturnBest::kOnAnyNonOptimal;
    o.termination.acceptable_stationarity_tol = 2e-2;
    o.termination.acceptable_equality_tol = 2e-3;
    o.termination.acceptable_inequality_tol = 3e-3;
    o.termination.acceptable_complementarity_tol = 4e-3;
    o.trust_region_initial = 2.0;
    o.trust_region_max = 1e9;
    o.trust_region_min = 1e-9;
    o.caller_step_limit = 0.5;
    o.qp.primal_regularization = 1e-7;
    o.qp.dual_regularization = 2e-8;
    o.qp.feasibility_tol = 1e-8;
    o.qp.optimality_tol = 2e-9;
    o.qp.max_iterations = 99;
    o.qp.max_schur_border_dimension = 64;
    o.qp.schur_condition_max = 1e8;
    o.qp.working_set_algebra = QpWorkingSetAlgebra::kRefactorize;
    o.qp.trust_region_radius = 5.0;
    o.qp.independent_working_set = false;
    o.qp.hessian_known_convex = true;
    o.qp_mode = QpMode::kIpqp;
    o.row_scaling = true;
    o.scaling_max_gradient = 50.0;
    o.scaling_factor_max = 1e6;
    o.row_scaling_power_of_two = true;
    o.column_scaling = SqpColumnScaling::kAlways;
    o.column_scaling_threshold = 1e6;
    o.column_scaling_factor_max = 512.0;
    o.project_start_point = false;
    o.hessian_mode = SqpHessianMode::kLbfgsDamped;
    o.lbfgs_memory = 5;
    o.linear.dense_factorization = linear::DensePolicy::kAlways;
    o.linear.max_dense_dimension = 32;
    SqpAdvancedOptions &a = o.advanced;
    a.second_order_correction = false;
    a.adaptive_dual_regularization = false;
    a.make_strategy = []() -> std::unique_ptr<detail::GlobalizationStrategy> { return nullptr; };
    a.warm_full_step = false;
    a.elastic_ladder_early_exit = true;
    a.crash_basis = true;
    a.elastic_rung_engine = SqpElasticRungEngine::kTier;
    a.ipqp_reseed_after_disproval = true;
    a.ipqp.max_iterations = 33;
    a.ipqp.hard_max_iterations = 61;
    a.ipqp.max_factorizations = 77;
    a.ipqp.barrier_initial = 2e-2;
    a.ipqp.barrier_min = 1e-11;
    a.ipqp.primal_regularization_initial = 7.0;
    a.ipqp.dual_regularization_initial = 7.0;
    a.ipqp.regularization_min = 1e-9;
    a.ipqp.regularization_max = 1e5;
    a.ipqp.regularization_decrease = 0.2;
    a.ipqp.fraction_to_boundary = 0.99;
    a.ipqp.face_ratio_threshold = 2e-2;
    a.ipqp.convergence_slack = 50.0;
    a.ipqp.ruiz_equilibration = false;
    a.ipqp.warm_repair = false;
    a.ipqp.max_warm_iterations = 14;
    a.ipqp.barrier_adoption_factor = 2.0;
    a.ipqp.stall_window = 6;
    a.ipqp.max_consecutive_escapes = 4;
    a.ipqp.farkas_gate = false;
    a.ipqp.hoist_symbolic_analysis = false;
    a.ipqp.require_final_inertia = false;
    a.ipqp.barrier_initial_rule = IpqpBarrierInitialRule::kComplementarity;
    a.ipqp.barrier_residual_floor = true;
    a.ipqp.ladder_skip_after = 4;
    a.qp_dump_dir = "dump_dir";
    a.qp_dump_policy = QpDumpPolicy::kAll;
    a.ssn_proximal_carry = true;
    a.ssn_certify_from_face = true;
    a.ssn_sigma_rule = SsnSigmaRule::kResidualAlways;
    a.ssn_hint_rule = SsnHintRule::kWatchdog;
    a.ssn_infeasibility_rule = SsnInfeasibilityRule::kFarkasGated;
    a.max_consecutive_qp_failures = 4;
    a.qp_iteration_limit_retry = true;
    a.qp_failure_step = false;
    a.qp_retry_regularization_first = true;
    a.restoration_proximity = 0.5;
    a.equality_basis = false;
    a.elastic_witness_start = true;
    a.elastic_last_optimal_rung = true;
    a.restoration_resume_reduction = 0.5;
    a.restoration_stall_factorizations = 4321;
    a.max_unproductive_restoration_entries = 3;
    a.restoration_max_total_factorizations = 98765;
    a.lbfgs_update_on_reject = true;
    a.lbfgs_nonlinear_only = true;
    a.max_lbfgs_full_memory_dimension = 200;
    a.restoration_curvature = SqpRestorationCurvature::kRegularized;
    a.max_restoration_entries = 7;
    a.trust_region_growth = SqpTrustRegionGrowth::kAnyAccepted;
    a.ssn_farkas_residual_tol = 1e-5;
    a.ssn_uncertain_tol = 0.2;
    a.ssn_lm_sigma_c = 2.0;
    a.ssn_lm_sigma_exponent = 1.5;
    a.restoration_elastic_seed = false;
    a.elastic_rho0_headroom_rungs = 3;
    a.adaptive_dual_regularization_max = 1e-6;
    return o;
}

} // namespace hven::test
