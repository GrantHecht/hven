// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Option-validation refusal table for both engines and the shared CommonOptions block.
// One row per refusable field/condition: a value that is ill-formed in exactly one way, and the
// substring the refusal's message must carry. Companion to test_options.cpp (which pins the
// transactional and in-flight behaviour); this file closes the per-field refusal paths that the
// older cases did not reach. Each row also checks that the unmutated default is accepted, so a
// row cannot pass by refusing everything.

#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <hven/ipm/ipm_options.h>
#include <hven/sqp/sqp_options.h>

namespace {

using hven::IpmOptions;
using hven::SqpOptions;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

template <class Opts> struct Row {
    const char *label;
    std::function<void(Opts &)> mutate;
    const char *expect; // substring of the refusal message
};

template <class Opts> void run_rows(const std::vector<Row<Opts>> &rows) {
    EXPECT_NO_THROW(hven::validate(Opts{}));
    for (const auto &row : rows) {
        SCOPED_TRACE(row.label);
        Opts o;
        row.mutate(o);
        try {
            hven::validate(o);
            ADD_FAILURE() << "validate() accepted the ill-formed value";
        } catch (const std::invalid_argument &e) {
            EXPECT_NE(std::string(e.what()).find(row.expect), std::string::npos)
                << "message was: " << e.what();
        }
    }
}

TEST(ValidateRefusals, IpmEveryRefusablePath) {
    run_rows<IpmOptions>({
        {"empty phases", [](IpmOptions &o) { o.phases.clear(); }, "phases"},
        {"phase out of range",
         [](IpmOptions &o) { o.phases = {static_cast<hven::IpmPhase>(99)}; }, "phases[i]"},
        {"optimality_tol inf", [](IpmOptions &o) { o.common.optimality_tol = kInf; },
         "common.optimality_tol"},
        {"feasibility_tol zero", [](IpmOptions &o) { o.common.feasibility_tol = 0.0; },
         "common.feasibility_tol"},
        {"max_iterations beyond int",
         [](IpmOptions &o) { o.common.max_iterations = std::numeric_limits<long long>::max(); },
         "common.max_iterations"},
        {"max_factorizations negative", [](IpmOptions &o) { o.common.max_factorizations = -1; },
         "common.max_factorizations"},
        {"time limit zero", [](IpmOptions &o) { o.common.time_limit_seconds = 0.0; },
         "common.time_limit_seconds"},
        {"time limit nan", [](IpmOptions &o) { o.common.time_limit_seconds = kNaN; },
         "common.time_limit_seconds"},
        {"start_level range",
         [](IpmOptions &o) { o.common.start_level = static_cast<hven::StartLevel>(40); },
         "common.start_level"},
        {"return_best range",
         [](IpmOptions &o) { o.common.return_best = static_cast<hven::ReturnBest>(40); },
         "common.return_best"},
        {"infinite_bound nonpositive", [](IpmOptions &o) { o.common.infinite_bound = 0.0; },
         "common.infinite_bound"},
        {"complementarity_tol nonpositive",
         [](IpmOptions &o) { o.termination.complementarity_tol = -1.0; },
         "termination.complementarity_tol"},
        {"acceptable_iterations zero", [](IpmOptions &o) { o.termination.acceptable_iterations = 0; },
         "termination.acceptable_iterations"},
        {"acceptable_stationarity_tol nan",
         [](IpmOptions &o) { o.termination.acceptable_stationarity_tol = kNaN; },
         "termination.acceptable_stationarity_tol"},
        {"acceptable_inequality_tol zero",
         [](IpmOptions &o) { o.termination.acceptable_inequality_tol = 0.0; },
         "termination.acceptable_inequality_tol"},
        {"acceptable_complementarity_tol zero",
         [](IpmOptions &o) { o.termination.acceptable_complementarity_tol = 0.0; },
         "termination.acceptable_complementarity_tol"},
        {"divergence_stationarity_tol zero",
         [](IpmOptions &o) { o.termination.divergence_stationarity_tol = 0.0; },
         "termination.divergence_stationarity_tol"},
        {"divergence_equality_tol zero",
         [](IpmOptions &o) { o.termination.divergence_equality_tol = 0.0; },
         "termination.divergence_equality_tol"},
        {"divergence_inequality_tol zero",
         [](IpmOptions &o) { o.termination.divergence_inequality_tol = 0.0; },
         "termination.divergence_inequality_tol"},
        {"divergence_complementarity_tol zero",
         [](IpmOptions &o) { o.termination.divergence_complementarity_tol = 0.0; },
         "termination.divergence_complementarity_tol"},
        {"optimality above acceptable stationarity",
         [](IpmOptions &o) {
             o.termination.acceptable_stationarity_tol = o.common.optimality_tol / 10.0;
         },
         "must be <= termination.acceptable_stationarity_tol"},
        {"feasibility above acceptable inequality",
         [](IpmOptions &o) {
             o.termination.acceptable_inequality_tol = o.common.feasibility_tol / 10.0;
         },
         "termination.acceptable_inequality_tol"},
        {"complementarity (defaulted) above acceptable",
         [](IpmOptions &o) {
             o.termination.acceptable_complementarity_tol = o.common.optimality_tol / 10.0;
         },
         "the complementarity tolerance"},
        {"complementarity (explicit) above acceptable",
         [](IpmOptions &o) {
             o.termination.complementarity_tol = 1.0;
             o.termination.acceptable_complementarity_tol = 0.5;
         },
         "termination.complementarity_tol"},
        {"acceptable equality above divergence",
         [](IpmOptions &o) {
             o.termination.divergence_equality_tol = o.termination.acceptable_equality_tol / 10.0;
         },
         "termination.divergence_equality_tol"},
        {"acceptable inequality above divergence",
         [](IpmOptions &o) {
             o.termination.divergence_inequality_tol =
                 o.termination.acceptable_inequality_tol / 10.0;
         },
         "termination.divergence_inequality_tol"},
        {"acceptable complementarity above divergence",
         [](IpmOptions &o) {
             o.termination.divergence_complementarity_tol =
                 o.termination.acceptable_complementarity_tol / 10.0;
         },
         "termination.divergence_complementarity_tol"},
        {"barrier_initial nonpositive", [](IpmOptions &o) { o.barrier_initial = 0.0; },
         "barrier_initial"},
        {"barrier_min nan", [](IpmOptions &o) { o.barrier_min = kNaN; }, "barrier_min"},
        {"barrier_max inf", [](IpmOptions &o) { o.barrier_max = kInf; }, "barrier_max"},
        {"bound_push nonpositive", [](IpmOptions &o) { o.bound_push = 0.0; }, "bound_push"},
        {"bound_relax_factor negative", [](IpmOptions &o) { o.bound_relax_factor = -1e-9; },
         "bound_relax_factor"},
        {"relax-bounds with zero factor",
         [](IpmOptions &o) {
             o.fixed_variable_treatment = hven::IpmFixedVariableTreatment::kRelaxBounds;
             o.bound_relax_factor = 0.0;
         },
         "kRelaxBounds"},
        {"objective_scale nan", [](IpmOptions &o) { o.objective_scale = kNaN; },
         "objective_scale"},
        {"linear.ordering range",
         [](IpmOptions &o) { o.linear.ordering = static_cast<hven::IpmOrdering>(77); },
         "linear.ordering"},
        {"linear.pivot_strategy range",
         [](IpmOptions &o) { o.linear.pivot_strategy = static_cast<hven::IpmPivotStrategy>(77); },
         "linear.pivot_strategy"},
        {"pivot_perturbation_exponent negative",
         [](IpmOptions &o) { o.linear.pivot_perturbation_exponent = -1; },
         "linear.pivot_perturbation_exponent"},
        {"solve_parallelism range",
         [](IpmOptions &o) {
             o.linear.solve_parallelism = static_cast<hven::IpmSolveParallelism>(77);
         },
         "linear.solve_parallelism"},
        {"accelerate_zero_tol nonpositive",
         [](IpmOptions &o) { o.linear.accelerate_zero_tol = 0.0; }, "linear.accelerate_zero_tol"},
        {"dense_factorization range",
         [](IpmOptions &o) {
             o.linear.dense_factorization = static_cast<hven::linear::DensePolicy>(77);
         },
         "linear.dense_factorization"},
        {"max_dense_dimension reserved",
         [](IpmOptions &o) { o.linear.max_dense_dimension = 10; }, "reserved"},
        {"CNR with the dense arm",
         [](IpmOptions &o) {
             o.linear.dense_factorization = hven::linear::DensePolicy::kAlways;
             o.linear.conditional_reproducibility = true;
         },
         "conditional_reproducibility"},
        {"max_inertia_correction_attempts negative",
         [](IpmOptions &o) { o.advanced.max_inertia_correction_attempts = -1; },
         "advanced.max_inertia_correction_attempts"},
        {"max_line_search_backtracks negative",
         [](IpmOptions &o) { o.advanced.max_line_search_backtracks = -1; },
         "advanced.max_line_search_backtracks"},
        {"max_second_order_corrections negative",
         [](IpmOptions &o) { o.advanced.max_second_order_corrections = -1; },
         "advanced.max_second_order_corrections"},
        {"max_extended_backtracks negative",
         [](IpmOptions &o) { o.advanced.max_extended_backtracks = -1; },
         "advanced.max_extended_backtracks"},
        {"acceptance_mode range",
         [](IpmOptions &o) {
             o.advanced.acceptance_mode = static_cast<hven::IpmAcceptanceMode>(77);
         },
         "advanced.acceptance_mode"},
        {"merit_penalty_rule range",
         [](IpmOptions &o) {
             o.advanced.merit_penalty_rule = static_cast<hven::IpmMeritPenaltyRule>(77);
         },
         "advanced.merit_penalty_rule"},
        {"barrier_governor_mode range",
         [](IpmOptions &o) {
             o.advanced.barrier_governor_mode = static_cast<hven::IpmBarrierGovernorMode>(77);
         },
         "advanced.barrier_governor_mode"},
        {"restoration_mode range",
         [](IpmOptions &o) {
             o.advanced.restoration_mode = static_cast<hven::IpmRestorationMode>(77);
         },
         "advanced.restoration_mode"},
        {"inertia_mode range",
         [](IpmOptions &o) { o.advanced.inertia_mode = static_cast<hven::IpmInertiaMode>(77); },
         "advanced.inertia_mode"},
        {"solve_phase_mode range",
         [](IpmOptions &o) {
             o.advanced.solve_phase_mode = static_cast<hven::IpmSolvePhaseMode>(77);
         },
         "advanced.solve_phase_mode"},
        {"optimize_barrier_mode range",
         [](IpmOptions &o) {
             o.advanced.optimize_barrier_mode = static_cast<hven::IpmBarrierMode>(77);
         },
         "advanced.optimize_barrier_mode"},
        {"solve_barrier_mode range",
         [](IpmOptions &o) { o.advanced.solve_barrier_mode = static_cast<hven::IpmBarrierMode>(77); },
         "advanced.solve_barrier_mode"},
        {"optimize_line_search_mode range",
         [](IpmOptions &o) {
             o.advanced.optimize_line_search_mode = static_cast<hven::IpmLineSearchMode>(77);
         },
         "advanced.optimize_line_search_mode"},
        {"solve_line_search_mode range",
         [](IpmOptions &o) {
             o.advanced.solve_line_search_mode = static_cast<hven::IpmLineSearchMode>(77);
         },
         "advanced.solve_line_search_mode"},
        {"step_length_rule range",
         [](IpmOptions &o) {
             o.advanced.step_length_rule = static_cast<hven::IpmStepLengthRule>(77);
         },
         "advanced.step_length_rule"},
        {"negative_slack_reset nonpositive",
         [](IpmOptions &o) { o.advanced.negative_slack_reset = 0.0; },
         "advanced.negative_slack_reset"},
        {"inertia_correction_initial nonpositive",
         [](IpmOptions &o) { o.advanced.inertia_correction_initial = 0.0; },
         "advanced.inertia_correction_initial"},
        {"inertia_correction_increase not above one",
         [](IpmOptions &o) { o.advanced.inertia_correction_increase = 1.0; },
         "advanced.inertia_correction_increase"},
        {"inertia_correction_decrease outside (0,1)",
         [](IpmOptions &o) { o.advanced.inertia_correction_decrease = 1.0; },
         "advanced.inertia_correction_decrease"},
    });
}

TEST(ValidateRefusals, SqpEveryRefusablePath) {
    run_rows<SqpOptions>({
        {"optimality_tol zero", [](SqpOptions &o) { o.common.optimality_tol = 0.0; },
         "common.optimality_tol"},
        {"max_iterations negative", [](SqpOptions &o) { o.common.max_iterations = -1; },
         "common.max_iterations"},
        {"threads negative", [](SqpOptions &o) { o.common.threads = -1; }, "common.threads"},
        {"qp.max_iterations zero", [](SqpOptions &o) { o.qp.max_iterations = 0; },
         "qp.max_iterations"},
        {"qp.dual_regularization negative",
         [](SqpOptions &o) { o.qp.dual_regularization = -1.0; }, "qp.dual_regularization"},
        {"qp.primal_regularization inf",
         [](SqpOptions &o) { o.qp.primal_regularization = kInf; }, "qp.primal_regularization"},
        {"qp.feasibility_tol zero", [](SqpOptions &o) { o.qp.feasibility_tol = 0.0; },
         "qp.feasibility_tol"},
        {"qp.optimality_tol nan", [](SqpOptions &o) { o.qp.optimality_tol = kNaN; },
         "qp.optimality_tol"},
        {"qp.schur_condition_max below one",
         [](SqpOptions &o) { o.qp.schur_condition_max = 0.5; }, "qp.schur_condition_max"},
        {"qp.max_schur_border_dimension negative",
         [](SqpOptions &o) { o.qp.max_schur_border_dimension = -1; },
         "qp.max_schur_border_dimension"},
        {"qp.working_set_algebra range",
         [](SqpOptions &o) { o.qp.working_set_algebra = static_cast<hven::QpWorkingSetAlgebra>(77); },
         "qp.working_set_algebra"},
        {"acceptable stationarity below convergence",
         [](SqpOptions &o) {
             o.termination.acceptable_stationarity_tol = o.common.optimality_tol / 10.0;
         },
         "common.optimality_tol"},
        {"acceptable equality nonpositive",
         [](SqpOptions &o) { o.termination.acceptable_equality_tol = 0.0; },
         "termination.acceptable_equality_tol"},
        {"acceptable inequality below convergence",
         [](SqpOptions &o) {
             o.termination.acceptable_inequality_tol = o.common.feasibility_tol / 10.0;
         },
         "termination.acceptable_inequality_tol"},
        {"acceptable complementarity inf",
         [](SqpOptions &o) { o.termination.acceptable_complementarity_tol = kInf; },
         "termination.acceptable_complementarity_tol"},
        {"qp.trust_region_radius negative",
         [](SqpOptions &o) { o.qp.trust_region_radius = -1.0; }, "qp.trust_region_radius"},
        {"trust_region_initial zero", [](SqpOptions &o) { o.trust_region_initial = 0.0; },
         "trust_region_initial"},
        {"trust_region_max below initial",
         [](SqpOptions &o) {
             o.trust_region_initial = 1.0;
             o.trust_region_max = 0.5;
         },
         "trust_region_max"},
        {"trust_region_min zero", [](SqpOptions &o) { o.trust_region_min = 0.0; },
         "trust_region_min"},
        {"trust_region_min above initial",
         [](SqpOptions &o) {
             o.trust_region_initial = 1.0;
             o.trust_region_max = 2.0;
             o.trust_region_min = 1.5;
         },
         "trust_region_min"},
        {"scaling_max_gradient inf", [](SqpOptions &o) { o.scaling_max_gradient = kInf; },
         "scaling_max_gradient"},
        {"scaling_factor_max below one", [](SqpOptions &o) { o.scaling_factor_max = 0.5; },
         "scaling_factor_max"},
        {"column_scaling_factor_max below one",
         [](SqpOptions &o) { o.column_scaling_factor_max = 0.5; }, "column_scaling_factor_max"},
        {"caller_step_limit negative", [](SqpOptions &o) { o.caller_step_limit = -1.0; },
         "caller_step_limit"},
        {"qp_dump_policy range",
         [](SqpOptions &o) { o.advanced.qp_dump_policy = static_cast<hven::QpDumpPolicy>(77); },
         "advanced.qp_dump_policy"},
        {"qp_dump_policy on without a directory",
         [](SqpOptions &o) {
             o.advanced.qp_dump_policy = hven::QpDumpPolicy::kAll;
             o.advanced.qp_dump_dir.clear();
         },
         "qp_dump_dir"},
        {"column_scaling range",
         [](SqpOptions &o) { o.column_scaling = static_cast<hven::SqpColumnScaling>(77); },
         "column_scaling"},
        {"column_scaling_threshold below one",
         [](SqpOptions &o) { o.column_scaling_threshold = 0.5; }, "column_scaling_threshold"},
        {"qp_mode range", [](SqpOptions &o) { o.qp_mode = static_cast<hven::QpMode>(77); },
         "qp_mode"},
        {"elastic_rung_engine range",
         [](SqpOptions &o) {
             o.advanced.elastic_rung_engine = static_cast<hven::SqpElasticRungEngine>(77);
         },
         "advanced.elastic_rung_engine"},
        {"ssn_sigma_rule range",
         [](SqpOptions &o) { o.advanced.ssn_sigma_rule = static_cast<hven::SsnSigmaRule>(77); },
         "advanced.ssn_sigma_rule"},
        {"ssn_hint_rule range",
         [](SqpOptions &o) { o.advanced.ssn_hint_rule = static_cast<hven::SsnHintRule>(77); },
         "advanced.ssn_hint_rule"},
        {"ssn_infeasibility_rule range",
         [](SqpOptions &o) {
             o.advanced.ssn_infeasibility_rule = static_cast<hven::SsnInfeasibilityRule>(77);
         },
         "advanced.ssn_infeasibility_rule"},
        {"max_consecutive_qp_failures zero",
         [](SqpOptions &o) { o.advanced.max_consecutive_qp_failures = 0; },
         "max_consecutive_qp_failures"},
        {"restoration_proximity negative",
         [](SqpOptions &o) { o.advanced.restoration_proximity = -1.0; },
         "advanced.restoration_proximity"},
    });
}

} // namespace
