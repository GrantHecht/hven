// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <deque>
#include <memory>

#include <Eigen/Core>

#include "hven/detail/globalization/barrier_governor.h"
#include "hven/detail/globalization/solver_context.h"
#include "hven/detail/interior/iterate_info.h"

namespace hven::solvers {

class GlobalizationMechanism;

// Ipopt option defaults this governor transcribes (in order):
// kAdaptiveMuKktErrorRedIters -> adaptive_mu_kkterror_red_iters,
// kAdaptiveMuKktErrorRedFact -> adaptive_mu_kkterror_red_fact,
// kAdaptiveMuMonotoneInitFactor -> adaptive_mu_monotone_init_factor,
// kBarrierKappaMu -> mu_linear_decrease_factor,
// kBarrierThetaMu -> mu_superlinear_decrease_power,
// kBarrierTolFactor -> barrier_tol_factor.
inline constexpr int kAdaptiveMuKktErrorRedIters = 4;       
inline constexpr double kAdaptiveMuKktErrorRedFact = 0.9999; 
inline constexpr double kAdaptiveMuMonotoneInitFactor = 0.8; 
inline constexpr double kBarrierKappaMu = 0.2;              
inline constexpr double kBarrierThetaMu = 1.5;              
inline constexpr double kBarrierTolFactor = 10.0;           

/// @brief Free<->monotone monitored barrier-parameter governor.
///
/// In FREE mode it delegates the barrier update to a composed BarrierGovernor
/// (by default a ClassicAdaptiveGovernor running the PROBE/LOQO oracles
/// unchanged). A KKT-error monitor watches the recent iteration history; when
/// free mode stops making sufficient progress the governor hands off to a
/// MONOTONE (Fiacco–McCormick) mode that holds the barrier parameter fixed
/// until the barrier subproblem converges, then decreases it. When monotone
/// progress brings the KKT error back into the reference band the governor
/// re-enters free mode. Opt-in only: nothing constructs this type on the
/// default solve path.
///
/// Formulation (transcribed from Ipopt's adaptive/fixed mu-update pair,
/// releases/3.14.19):
///
/// (1) Monitor error — under the reference defaults the centrality/balancing
///     terms vanish, leaving a sum of squared, per-dimension-averaged part
///     norms of (dual_inf, primal_inf, compl). This engine's mapping keeps the
///     sum-of-squared-parts structure but uses the ∞-norm residual scalars its
///     IterateInfo carries:
///         monitor_error = kkt_inf_² + econ_inf_² + icon_inf_² + barr_inf_²
///     Documented deviations: each part is an ∞-norm scalar rather than a
///     squared 2-norm; no per-dimension division (three separate weights, so
///     switch timing CAN shift relative to the reference formula — harmless
///     because monitor_error and everything it is compared against use the
///     identical formula here, and the monotone fallback this monitor gates is
///     safe regardless of when it engages); barr_inf_ = max(sᵢ·λᵢ) stands in
///     for the complementarity part.
/// (2) Sufficient progress: trivially true until kAdaptiveMuKktErrorRedIters
///     reference values exist; afterwards true iff
///     curr_error ≤ kAdaptiveMuKktErrorRedFact · ref for AT LEAST ONE ref.
/// (3) Reference list: FIFO of at most kAdaptiveMuKktErrorRedIters errors,
///     updated ONLY while progress is being made (free mode or re-entry) —
///     never while remaining monotone, so during monotone mode the reference
///     band freezes at the last free-mode errors and re-entry is measured
///     against that frozen band.
/// (4) Handoff free -> monotone: μ ← clamp(kAdaptiveMuMonotoneInitFactor ·
///     avgcomp, min_mu, max_mu), no iterate restore (matching the reference
///     default); the safeguard/max_ref caps are inert under their defaults and
///     omitted.
/// (5) Re-entry monotone -> free: the sufficient-progress test is re-evaluated
///     every monotone iteration against the frozen band; passing it re-enters
///     free mode. (The reference additionally requires that the previous step
///     was not a tiny step; this engine tracks no tiny-step state and does not
///     apply that extra condition.)
/// (6) Monotone Fiacco–McCormick advance: only once the barrier subproblem has
///         converged: sub_problem_error ≤ kBarrierTolFactor · μ,
///     then μ⁺ = max(floor, min(kBarrierKappaMu·μ, μ^kBarrierThetaMu)) with
///     floor = min(bar_tol, kkt_tol)/(kBarrierTolFactor + 1) — this engine
///     maps the reference's compl_inf_tol/tol pair onto bar_tol_/kkt_tol_
///     because it carries per-part tolerances instead of one overall tol —
///     finally clamped to [min_mu, max_mu].
///     sub_problem_error = max(kkt_inf_, econ_inf_, icon_inf_, barr_inf_),
///     where at a barrier solution s·λ ≈ μ makes the gate read "complementarity
///     is within kBarrierTolFactor of μ".
///
/// mu_event semantics (the per-barrier-subproblem acceptance-reset trigger):
/// set exactly when a new monotone barrier subproblem begins with a fresh
/// parameter — the handoff AND each strict μ-advance; NOT on monotone->free
/// re-entry (that reset belongs to the free-mode oracle's per-iteration cycle,
/// which the delegated classic path does not reproduce).
class MonitoredBarrierGovernor : public BarrierGovernor {
  public:
    /// Default: composes a ClassicAdaptiveGovernor as the free-mode delegate.
    MonitoredBarrierGovernor();

    /// Injects the free-mode delegate (tests pass a recording fake).
    explicit MonitoredBarrierGovernor(std::unique_ptr<BarrierGovernor> free_delegate);
    ~MonitoredBarrierGovernor() override;

    double update_barrier(InteriorPointSolver::BarrierModes barmode, double mu_in, double avgcomp,
                          double mincomp, Eigen::VectorXd &XSL, Eigen::VectorXd &RHS,
                          Eigen::VectorXd &DXSL, Eigen::VectorXd &Temp,
                          GlobalizationMechanism &mechanism, SolverContext &ctx, double &barr_obj,
                          const IterateInfo &current, bool &mu_event) override;

    bool in_monotone_mode() const override { return monotone_mode_; }

    /// This governor supplies its own safeguarded schedule during nested
    /// restoration (its monitor forces the Fiacco-McCormick decrease), so the
    /// seam does not overlay update_barrier_monotone — see
    /// BarrierGovernor::provides_restoration_barrier_safeguard().
    ///
    /// Scope caveat, per oracle: under LOQO the free-mode update reads the
    /// elastic-augmented complementarity directly, so μ tracks the restoration
    /// problem's own barrier state; under PROBE the predictor recomputes
    /// complementarity from the original slack/multiplier pairs only, so the
    /// first free window after phase entry runs without the augmented signal —
    /// containment then rests on the monitor's error measure (which does fold
    /// the elastic complementarity) and the monotone handoff re-anchoring at
    /// the augmented average. Validated empirically on the restoration corpus
    /// cases: normal entry/iterate/exit under PROBE, outcomes within a few
    /// iterations of the no-restoration baseline.
    bool provides_restoration_barrier_safeguard() const override { return true; }

    /// Clears the reference window, mode, monotone-mu bookkeeping, and the
    /// write-only diagnostics; also resets the free-mode delegate. Phase
    /// boundaries start in free mode.
    void reset() override;

    /// Reports last_monotone_switches_/last_monotone_iters_ into the
    /// corresponding SolveResult fields.
    void append_diagnostics(InteriorPointSolver::SolveResult &result) const override;

    // ------------------------------------------------------------------------
    // Testable state machine. `decide` advances the monitor/mode state from
    // `current` (the in-progress iterate's residuals) and returns the barrier
    // decision WITHOUT touching any KKT vectors (the tolerances/bounds it
    // needs are passed as scalars). update_barrier calls it, then applies the
    // barrier tail in monotone mode or delegates in free mode. Exposed so the
    // monitor, handoff, Fiacco–McCormick, re-entry, and mu-event logic are
    // drivable in unit tests without a real KKT solve.
    // ------------------------------------------------------------------------
    struct BarrierDecision {
        double mu = 0.0;      ///< Barrier parameter to use (MEANINGFUL IFF monotone:
                               ///< decide() returns mu_in untouched on every free-mode
                               ///< path, so read d.mu only after checking d.monotone).
        bool mu_event = false; ///< A new monotone barrier subproblem began.
        bool monotone = false; ///< Resulting mode after this decision.
    };
    BarrierDecision decide(const IterateInfo &current, double mu_in, double avgcomp,
                           double bar_tol, double kkt_tol, double min_mu, double max_mu);

    /// Pure quantities (see formulation items (1), (6), (4)).
    static double monitor_error(const IterateInfo &it);
    static double barrier_subproblem_error(const IterateInfo &it);
    static double fiacco_mccormick_mu(double mu, double bar_tol, double kkt_tol, double min_mu,
                                      double max_mu);
    static double handoff_mu(double avgcomp, double min_mu, double max_mu);

    /// Reference-window operations (see formulation items (2), (3)).
    bool check_sufficient_progress(double curr_error) const;
    void remember_accepted(double curr_error);

    /// Test/diagnostic observers.
    const std::deque<double> &reference_values() const { return refs_vals_; }
    double monotone_mu() const { return monotone_mu_; }
    int last_monotone_switches() const { return last_monotone_switches_; }
    int last_monotone_iters() const { return last_monotone_iters_; }

  private:
    /// Barrier tail helpers (shared-kernel forwarders reading through ctx),
    /// applied in monotone mode to write the barrier objective and dual
    /// gradient at the fixed monotone μ; free mode delegates this to the
    /// composed governor.
    double barrier_objective(Eigen::Ref<Eigen::VectorXd> S, double mu,
                             const SolverContext &ctx) const;
    void barrier_gradient(Eigen::Ref<Eigen::VectorXd> S, Eigen::Ref<Eigen::VectorXd> LI, double mu,
                          Eigen::Ref<Eigen::VectorXd> AGS) const;

    std::unique_ptr<BarrierGovernor> free_delegate_;

    /// KKT-error reference window (FIFO, at most kAdaptiveMuKktErrorRedIters entries).
    std::deque<double> refs_vals_;
    bool monotone_mode_ = false;
    double monotone_mu_ = 0.0; ///< Current monotone barrier parameter (meaningful iff monotone_mode_).

    /// Write-only SolveResult diagnostics, bound via append_diagnostics().
    int last_monotone_switches_ = 0; ///< Free -> monotone handoffs this phase.
    int last_monotone_iters_ = 0;    ///< Iterations spent in monotone mode this phase.
};

} // namespace hven::solvers
