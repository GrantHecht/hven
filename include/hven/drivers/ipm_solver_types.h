// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief What an interior-point solve returns (`IpmResult`). The options are in
///        drivers/ipm_options.h, which this header includes.

#include <array>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <Eigen/Core>

#include "hven/core/types.h"
#include "hven/drivers/common_options.h"
#include "hven/drivers/ipm_options.h"
#include "hven/drivers/ipm_phase_report.h"
#include "hven/drivers/solve_result.h"
#include "hven/linear/symmetric_factor.h"

namespace hven::solvers {

/// @brief The four scalars the interior-point convergence test gates on, with
///        IterateInfo's definitions, so each is directly comparable against its
///        matching IpmOptions tolerance.
///
/// `kkt` and `barrier` carry IpmOptions::objective_scale; `equality` and
/// `inequality` carry no scale, and nothing here is unscaled on the way out. On
/// a restoration-active exit they describe the restoration subproblem, not the
/// NLP. Last phase wins. NaN means UNMEASURED, never "zero residual".
struct IpmConvergence {
    /// inf-norm dual infeasibility (z-form) at the reported iterate.
    double kkt = std::numeric_limits<double>::quiet_NaN();
    /// Complementarity (barrier) error at the reported iterate.
    double barrier = std::numeric_limits<double>::quiet_NaN();
    /// inf-norm equality-constraint residual; 0 when there are no equalities.
    double equality = std::numeric_limits<double>::quiet_NaN();
    /// inf-norm inequality-constraint residual; 0 when there are no inequalities.
    double inequality = std::numeric_limits<double>::quiet_NaN();
};

/// @brief Where one interior-point call spent its wall-clock seconds.
///
/// INFORMATIONAL, NEVER ASSERTED -- counters are this project's currency of
/// correctness, not timings. Measured on std::chrono::steady_clock.
struct IpmTiming {
    /// Total wall-clock time of the call.
    double total = 0;
    /// Setup/preprocessing time.
    double pre = 0;
    /// NLP evaluation callback time.
    double func = 0;
    /// KKT assembly/factorization/solve time.
    double kkt = 0;
    /// Printing time.
    double print = 0;
    /// Solver-initialization time (measured before the main timer starts).
    double solver_init = 0;

    /// Total minus all categorized components (excluding `solver_init`, which
    /// is measured before the main timer starts): callback time, step
    /// application, convergence checks and the like.
    double misc() const { return total - pre - kkt - func - print; }
};

/// @brief The NLP evaluation errors absorbed during one call.
///
/// A trial evaluation that throws is converted into a rejected trial instead of
/// unwinding the solve; this records how often that happened and the most
/// recent message.
struct EvaluationErrors {
    /// Number of absorbed evaluation errors.
    Index count = 0;
    /// The most recent message; empty when `count` is 0.
    std::string last_message;
};

/// @brief What one interior-point solve produced.
///
/// The base carries the answer in DECLARED space and CALLER units; everything
/// added here is this engine's own -- its measurements, its timings, its
/// per-phase account and its factorization snapshot.
///
/// PER CALL, all of it. This is a value returned by `solve()`, not an
/// accumulator a caller reads later.
struct IpmResult : SolveResult {

    // The warm-start payload's own two counters: both 0 on every solve that was
    // handed no payload, and 0 or 1 otherwise. A caller who set
    // `common.start_level` in one place and attached a payload in another has no
    // other way to find out which of the two won.

    /// @brief 1 when this call was handed a payload and IGNORED IT ENTIRELY
    ///        because `IpmOptions::common.start_level` was kCold; 0 otherwise.
    ///
    /// The payload's block lengths and its declaration stamp were still checked
    /// -- a foreign payload is refused at every rung -- and then nothing of it
    /// was applied.
    Index payload_ignored = 0;

    /// @brief 1 when this call was handed a payload carrying the
    ///        `"hven.ipm.polish.v1"` extension and did NOT consume it;
    ///        0 otherwise.
    ///
    /// Two ways to earn it, one reason. At a ceiling of kSeeded only the
    /// multipliers are applied; and on the MULTIPLIERS-ONLY seed form (an empty
    /// `primal_`) there is no point to apply at any ceiling. Either way this
    /// solve starts at `x0`, while the extension's (z_lower, z_upper) pair and
    /// inequality values are stated at the EXPORTER's point. 0 at a kCold
    /// ceiling, where `payload_ignored` is the answer.
    Index polish_ignored = 0;

    /// @brief Which IpmOptions::fixed_variable_treatment this call actually
    ///        ran under.
    ///
    /// Always equals the option's value at the time the call ran --
    /// configure_variable_treatment runs the requested treatment or throws,
    /// never substitutes. Overwritten unconditionally at the start of every
    /// call. Recorded here because it decides whether the internal fixing rows
    /// below exist.
    FixedVariableTreatment fixed_variable_treatment = FixedVariableTreatment::kMakeParameter;

    // --- The treatment's own rows, reported rather than dropped ---
    //
    // Under MakeConstraint a bound-fixed variable stays in the problem and is
    // held by an INTERNAL equality row `x - value = 0` appended after the
    // declared rows. Those rows are the treatment's, not the declaration's, so
    // they are taken OFF the base's ce/lambda_e -- whose contract is the
    // DECLARED problem -- and reported here instead. Empty under every other
    // treatment, and empty under MakeConstraint on a problem with no
    // bound-fixed variables.
    //
    // The fixing row's multiplier is also what prices that variable's box in
    // declared space: the base's z carries -lambda_fix at that coordinate, the
    // sign the stationarity convention grad f + J'lambda - z = 0 forces.
    /// @brief Residuals of the internal fixing rows, one per bound-fixed
    ///        variable, in the order of
    ///        NonLinearProgram::fixed_variable_indices().
    Vec internal_fixed_ce;
    /// @brief Multipliers of those same rows, same order.
    Vec internal_fixed_lambda_e;

    /// @brief The engine's own terminal measurements: what its convergence test
    ///        gated on, in its space. NOT the base's shared diagnostics.
    IpmConvergence convergence;

    /// @brief Where the call spent its wall-clock seconds.
    IpmTiming timing;

    // --- Factorization stats ---
    /// @brief Memory reported by the last factorization.
    Index factor_mem = 0;
    /// @brief Flops reported by the last factorization.
    Index factor_flops = 0;

    /// Number of second-order correction back-substitutions performed across
    /// the whole solve (one per correction attempt; each costs a single
    /// constraint evaluation and one back-substitution on the live
    /// factorization). Always 0 when SOC is off
    /// (advanced.max_second_order_corrections == 0).
    Index soc_steps_taken = 0;

    /// Number of times the watchdog armed across the whole solve (Chamberlain,
    /// Powell, Lemarechal & Pedersen 1982). Always 0 when the watchdog is off
    /// (advanced.watchdog == false).
    Index watchdog_activations = 0;

    /// Per-rejection recovery-chain outcome depth, indexed by the
    /// kRecoveryDepth* constants in globalization/recovery_chain.h: [0] SOC,
    /// [1] extended backtracking, [2] watchdog, [3] unresolved (the only
    /// bucket that increments when all three are off), [4] restoration
    /// (increments only when advanced.restoration_mode != kOff). Counts
    /// REJECTIONS, not interventions.
    std::array<Index, 5> recovery_depth_histogram{};

    // The last_* diagnostics below describe the LAST PHASE of the call. Each is
    // nullopt when the selected strategy does not report it, or when nothing ran
    // in that phase to report.

    /// Final funnel width (tau) reported by FunnelAcceptance.
    std::optional<double> last_funnel_width;

    /// Final filter size (stored (theta, phi) pairs); nullopt unless the
    /// acceptance strategy is the filter.
    std::optional<Index> last_filter_size;

    /// Total filter-reset-heuristic clears; nullopt unless the acceptance
    /// strategy is the filter. Under advanced.barrier_governor == kMonitored it
    /// counts per barrier subproblem: resets since the last mu-event.
    std::optional<Index> last_filter_resets;

    /// Number of free -> monotone handoffs, reported by MonitoredBarrierGovernor;
    /// nullopt unless the governor is kMonitored.
    std::optional<Index> last_monotone_switches;

    /// Number of iterations spent in monotone mode; nullopt unless the governor
    /// is kMonitored.
    std::optional<Index> last_monotone_iters;

    /// Number of times feasibility restoration was entered; nullopt when no
    /// restoration strategy is constructed. Write-only diagnostics.
    std::optional<Index> last_feas_rest_entries;

    /// Number of iterations spent in restoration mode; nullopt when no
    /// restoration strategy is constructed. Write-only diagnostics.
    std::optional<Index> last_feas_rest_iters;

    /// Proximal primal-dual regularization shifts at the LAST FACTORIZED
    /// ITERATION of the last phase: the persistent primal base shift rho_k added
    /// to the Hessian diagonal. nullopt unless advanced.inertia_mode ==
    /// kProximalRegularization, and when a phase converged before its first
    /// factorization.
    std::optional<double> last_prox_reg_primal;
    /// The barrier-scaled dual shift delta_c subtracted from the constraint-row
    /// diagonals (0.0 when suppressed inside a nested l1 phase), on the same
    /// nullopt rules.
    std::optional<double> last_prox_reg_dual;

    /// The last non-Success status observed from the KKT factorization within
    /// the CURRENT phase (per phase, never an earlier phase's). Purely
    /// observational: it feeds no control-flow decision.
    Eigen::ComputationInfo last_kkt_info = Eigen::Success;

    // --- The per-phase account ---
    /// @brief One entry per phase in `IpmOptions::phases`, in that order.
    ///
    /// The base's `status` is the LAST RAN phase's status and the base's
    /// `iterations` is the sum over the phases that ran.
    std::vector<IpmPhaseReport> phases;

    // --- The factorization SNAPSHOT ---
    //
    // Taken as this result was built, so they describe the solve that produced
    // them and cannot go stale behind a caller's back.
    /// @brief How many times the producing solver had laid and analyzed the KKT
    ///        sparsity pattern over its LIFETIME, as of this result.
    Index kkt_analyses_total = 0;
    /// @brief How many of those happened during THIS call.
    Index kkt_analyses_this_call = 0;
    /// @brief The KKT factor's linear-layer call counters as of this result,
    ///        including the pattern-guard count.
    hven::linear::SymmetricFactor::Counters kkt_factor_counters;
    /// @brief The NLP evaluation errors absorbed during this call.
    EvaluationErrors evaluation_errors;
};

} // namespace hven::solvers
