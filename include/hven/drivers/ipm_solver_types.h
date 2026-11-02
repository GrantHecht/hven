// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief What an interior-point solve returns (`IpmResult`). The options are in
///        drivers/ipm_options.h, which this header includes.

#include <array>
#include <limits>
#include <string>
#include <vector>

#include <Eigen/Core>

#include "hven/detail/interior/eval_error_log.h"
#include "hven/detail/interior/kkt_factorization.h"
#include "hven/drivers/common_options.h"
#include "hven/drivers/ipm_options.h"
#include "hven/drivers/ipm_phase_report.h"
#include "hven/drivers/solve_result.h"

namespace hven::solvers {

/// @brief What one interior-point solve produced.
///
/// The base carries the answer in DECLARED space and CALLER units; everything
/// added here is this engine's own -- its measurements, its timings, its
/// per-phase account and its factorization snapshot.
///
/// PER CALL, all of it. This is a value returned by `solve()`, not an
/// accumulator a caller reads later: the engine holds no result between calls
/// and there is no `result()` accessor any more.
struct IpmResult : SolveResult {

    // --- The warm-start PAYLOAD's own two counters (M6 W5 T8.5) ---
    //
    // Both are 0 on every solve that was handed no payload -- the cold
    // overload, and any native-route caller -- and 0 or 1 otherwise, there
    // being one payload per call. They exist because the alternative to
    // counting a discarded payload is discarding it silently: a caller who set
    // `common.start_level` in one place and attached a payload in another has
    // no other way to find out which of the two won.

    /// @brief 1 when this call was handed a payload and IGNORED IT ENTIRELY
    ///        because `IpmOptions::common.start_level` was kCold; 0 otherwise.
    ///
    /// The payload's block lengths and its declaration stamp were still checked
    /// -- a foreign payload is refused at every rung -- and then nothing of it
    /// was applied. Apart from this field the call is the solve it would have
    /// been with no payload at all.
    int payload_ignored = 0;

    /// @brief 1 when this call was handed a payload carrying the
    ///        `"hven.ipm.polish.v1"` extension and did NOT consume it;
    ///        0 otherwise.
    ///
    /// Two ways to earn it, one reason. At a ceiling of kSeeded only the
    /// multipliers are applied; and on the MULTIPLIERS-ONLY seed form (an empty
    /// `primal_`) there is no point to apply at any ceiling. Either way this
    /// solve starts at `x0`, while the extension's (z_lower, z_upper) pair and
    /// inequality values are stated at the EXPORTER's point -- seeding barrier
    /// state from them would describe somewhere this solve is not.
    ///
    /// 0 at a kCold ceiling: nothing of the payload was read there, and
    /// `payload_ignored` is that call's answer.
    int polish_ignored = 0;

    /// @brief Which IpmOptions::fixed_variable_treatment this call actually
    ///        ran under.
    ///
    /// Always equals the option's value at the time the call ran --
    /// configure_variable_treatment runs the requested treatment or throws,
    /// never substitutes. Overwritten unconditionally at the start of every
    /// call. Recorded here because it decides lambda_e's shape.
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

    // --- The engine's OWN terminal measurements ---
    //
    // NOT the base's four shared diagnostics: these are what this engine's
    // convergence test gated on, in ITS space, and a tolerance comparison must
    // be made against the number the test actually read.
    //
    // --- Terminal KKT residuals ---
    //
    // The four scalars converge_check() gates on, with IterateInfo's
    // definitions, so each is directly comparable against its matching
    // IpmOptions tolerance. kkt_inf and barr_inf carry IpmOptions::objective_scale;
    // econ_inf and icon_inf carry no scale, and nothing here is unscaled on
    // the way out. On a restoration-active exit they describe the restoration
    // subproblem, not the NLP. Last phase wins; the per-call reset sets them
    // to NaN, and NaN means UNMEASURED, never "zero residual".
    /// @brief inf-norm dual infeasibility (z-form) at the reported iterate.
    double kkt_inf = std::numeric_limits<double>::quiet_NaN();
    /// @brief Complementarity (barrier) error at the reported iterate.
    double barr_inf = std::numeric_limits<double>::quiet_NaN();
    /// @brief inf-norm equality-constraint residual; 0 when equal_cons == 0.
    double econ_inf = std::numeric_limits<double>::quiet_NaN();
    /// @brief inf-norm inequality-constraint residual; 0 when inequal_cons == 0.
    double icon_inf = std::numeric_limits<double>::quiet_NaN();

    // --- Timing (seconds) ---
    /// @brief Total wall-clock time of the most recent call.
    ///
    /// INFORMATIONAL, NEVER ASSERTED -- counters are this project's currency
    /// of correctness, not timings. Measured on std::chrono::steady_clock.
    double total_time = 0;
    /// @brief Setup/preprocessing time.
    double pre_time = 0;
    /// @brief NLP evaluation callback time.
    double func_time = 0;
    /// @brief KKT assembly/factorization/solve time.
    double kkt_time = 0;
    /// @brief Printing time.
    double print_time = 0;
    /// @brief Solver-initialization time (measured before the main timer starts).
    double solver_init_time = 0;

    /// Derived timing — total wall-clock minus all categorized components.
    /// Excludes solver_init_time (measured before the main timer starts).
    /// Captures: callback time, step application, convergence checks, etc.
    double misc_time() const { return total_time - pre_time - kkt_time - func_time - print_time; }

    // --- Factorization stats ---
    /// @brief Memory reported by the last factorization.
    int factor_mem = 0;
    /// @brief Flops reported by the last factorization.
    int factor_flops = 0;

    /// Number of second-order correction back-substitutions performed
    /// across the whole solve (one per correction attempt; each costs a
    /// single constraint evaluation + one back-substitution on the live
    /// factorization). Always 0 when SOC is off (advanced.max_second_order_corrections == 0). Reset
    /// per solve alongside the other accumulators.
    int soc_steps_taken = 0;

    /// Number of times the watchdog armed across the whole solve
    /// (Chamberlain, Powell, Lemaréchal & Pedersen 1982; constants per
    /// Wächter & Biegler 2006's implementation, globalization/watchdog.h).
    /// Always 0 when the watchdog is off (advanced.watchdog == false). Reset per
    /// solve alongside the other accumulators.
    int watchdog_activations = 0;

    /// Per-rejection recovery-chain outcome depth, indexed by the
    /// kRecoveryDepth* constants in globalization/recovery_chain.h: [0] SOC,
    /// [1] extended backtracking, [2] watchdog, [3] unresolved (the only
    /// bucket that increments when all three are off), [4] restoration
    /// (increments only when advanced.restoration_mode != kOff). Counts REJECTIONS,
    /// not interventions. Reset per solve.
    std::array<int, 5> recovery_depth_histogram{};

    /// Final funnel width (tau) reported by FunnelAcceptance at the end of
    /// the most recent solve's LAST PHASE. Sentinel -1.0 when the selected
    /// acceptance strategy does not report it, and when no acceptance test ran
    /// in that phase. A multi-phase call reports only the last phase's value.
    /// Reset per solve, not by the per-phase AcceptanceStrategy::reset().
    double last_funnel_width = -1.0;

    /// Final filter size (number of stored (θ, φ) pairs, Filter::size())
    /// reported by FilterAcceptance::append_diagnostics()
    /// (globalization/filter_acceptance.h) at the end of the most recent
    /// solve's LAST PHASE. Sentinel -1 when the selected acceptance
    /// strategy is not filter. Same last-phase-only semantics as
    /// last_funnel_width above.
    int last_filter_size = -1;

    /// Total filter-reset-heuristic clears (FilterAcceptance::filter_resets())
    /// at the end of the most recent solve's LAST PHASE. Sentinel -1 when the
    /// acceptance strategy is not kFilter. PER-PHASE, and under
    /// advanced.barrier_governor == kMonitored per barrier SUBPROBLEM: each mu-event
    /// also clears the counter, so this reports resets since the last mu-event
    /// of the last phase.
    int last_filter_resets = -1;

    /// Number of free -> monotone handoffs during the most recent solve's
    /// LAST PHASE, reported by MonitoredBarrierGovernor. Sentinel -1 when the
    /// selected barrier_governor is not monitored. Per-phase, like
    /// last_filter_resets above.
    int last_monotone_switches = -1;

    /// Number of iterations spent in monotone mode during the most recent
    /// solve's LAST PHASE, reported by MonitoredBarrierGovernor::
    /// append_diagnostics(). Sentinel -1 when the selected
    /// advanced.barrier_governor is not kMonitored. Same per-phase semantics as
    /// last_monotone_switches.
    int last_monotone_iters = -1;

    /// Number of times feasibility restoration was entered during the most
    /// recent solve's LAST PHASE, reported by RestorationStrategy. WRITE-ONLY
    /// diagnostics: no algorithm code reads it back. Sentinel -1 when no
    /// restoration strategy is constructed. Counting is identical across both
    /// modes -- entries_ increments once per entry call, iterations_in_mode_
    /// once per note_iteration() while active.
    int last_feas_rest_entries = -1;

    /// Number of iterations spent in restoration mode during the most
    /// recent solve's LAST PHASE, reported by RestorationStrategy::
    /// append_diagnostics(). WRITE-ONLY diagnostics field. Sentinel -1
    /// when no restoration strategy is constructed. Same per-phase
    /// semantics as last_feas_rest_entries (including the nested-mode
    /// counting note above).
    int last_feas_rest_iters = -1;

    /// Proximal primal-dual regularization shifts applied at the LAST
    /// FACTORIZED ITERATION of the most recent solve's last phase.
    /// last_prox_reg_primal is the persistent primal base shift rho_k added
    /// to the Hessian diagonal there; last_prox_reg_dual is the
    /// barrier-scaled dual shift delta_c subtracted from the constraint-row
    /// diagonals (0.0 when suppressed inside a nested l1 phase). Sentinel -1.0
    /// for BOTH when advanced.inertia_mode != kProximalRegularization, and when a
    /// mode-on phase converged before its first factorization.
    double last_prox_reg_primal = -1.0;
    /// The dual half of the pair documented just above: the barrier-scaled
    /// dual shift δ_c, on the same sentinel and last-phase-wins rules.
    double last_prox_reg_dual = -1.0;

    /// Message of the most recent trial-evaluation exception absorbed by
    /// the acceptance machinery during the most recent solve call (all
    /// phases). Empty when every evaluation succeeded. A populated value
    /// means the solver rejected un-evaluable trial steps and continued —
    /// to full recovery, to a graceful ACCEPTABLE-level exit at an
    /// already-acceptable iterate, or into feasibility restoration. When
    /// none of those paths existed, the solve threw the latched message
    /// wrapped in solver context instead.
    std::string last_eval_exception;

    /// The last non-Success status observed from kkt_sol_.info() by
    /// factor_impl() within the CURRENT phase (alg_impl resets it on
    /// entry, so the value is per-PHASE and never an earlier phase's).
    /// Purely observational -- surfaced by `ipm.phase.exit`'s
    /// `last_kkt_info` key, named there rather than raw (M6 W5 T8.7b; the
    /// print_exit_stats() this note used to name is deleted) -- and it
    /// feeds no control-flow decision in factor_impl.
    Eigen::ComputationInfo last_kkt_info = Eigen::Success;
    // --- The per-phase account (M6 W5 T8.4) ---
    /// @brief One entry per phase in `IpmOptions::phases`, in that order.
    ///
    /// The base's `status` is the LAST RAN phase's status and the base's
    /// `iterations` is the sum over the phases that ran.
    std::vector<IpmPhaseReport> phases;

    // --- The factorization SNAPSHOT (M6 W5 T8.4) ---
    //
    // These answer cross-call questions that used to be asked through
    // accessors on the solver. They are SNAPSHOTS taken as this result was
    // built, so they describe the solve that produced them and cannot go stale
    // behind a caller's back.
    /// @brief How many times the producing solver had laid and analyzed the KKT
    ///        sparsity pattern over its LIFETIME, as of this result.
    Index kkt_analyses_total = 0;
    /// @brief How many of those happened during THIS call.
    Index kkt_analyses_this_call = 0;
    /// @brief The KKT factor's linear-layer call counters as of this result,
    ///        including the pattern-guard count.
    KktFactorization::Counters kkt_factor_counters;
    /// @brief The log of NLP evaluation errors absorbed during this call.
    EvalErrorLog eval_error_log;
};

} // namespace hven::solvers
