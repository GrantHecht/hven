// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <Eigen/Core>

#include <cmath>

#include "hven/detail/globalization/progress_measures.h"
#include "hven/detail/globalization/proximal_restoration.h"
#include "hven/detail/globalization/restoration.h"
#include "hven/detail/globalization/solver_context.h"

namespace hven::solvers {

/// Penalty parameter rho of the elastic objective; Ipopt option
/// "resto_penalty_parameter" default 1e3.
inline constexpr double kRestoPenaltyParameter = 1.0e3;

/// Threshold above which the re-entry bound-multiplier update resets all slack
/// multipliers to 1 (Ipopt option "bound_mult_reset_threshold" default 1e3).
/// Exposed here so the solver's exit step and the constant live at the same
/// literature default.
inline constexpr double kBoundMultResetThreshold = 1.0e3;

// kRestoProximityWeight, kNearFeasibleGuardFactor, and
// kRestoFailureFeasibilityFactor are shared with the proximal switch
// (proximal_restoration.h) — included above; not redefined here.

/// Result aggregate of the closed-form elastic slack initialization for one
/// constraint row.
struct ElasticSlackInit {
    double n;  ///< Positive slack, root of n² − 2 a n − b = 0.
    double p;  ///< Paired slack, p = c + n.
    double zn; ///< Bound multiplier resto_mu / n.
    double zp; ///< Bound multiplier resto_mu / p.
};

/// @brief Closed-form elastic slack initialization for one constraint row;
/// exposed as a free function so it is directly unit-testable at a chosen
/// resto_mu independent of the entry max-rule. Positive root:
/// k = resto_mu/(2rho), a = k - c/2, b = c*k, n = a + sqrt(a^2 + b),
/// p = c + n. The c = 0 edge gives a > 0, b = 0, n = p = 2a (no division by
/// zero); n,p > 0 for mixed-sign c.
inline ElasticSlackInit l1_elastic_slack_init(double c, double resto_mu, double rho) {
    const double k = resto_mu / (2.0 * rho);
    const double a = k - 0.5 * c;
    const double b = c * k;
    const double n = a + std::sqrt(a * a + b);
    const double p = c + n;
    return ElasticSlackInit{n, p, resto_mu / n, resto_mu / p};
}

/// @brief Condensed l1 elastic feasibility restoration: solves the elastic
/// feasibility reformulation as an in-place phase reusing the outer barrier
/// algorithm's KKT system rather than a separate nested solver instance.
///
/// Elastic reformulation, per constraint ROW with residual value c (equality:
/// c = h_j(x); inequality: c = g_j(x) + s_j):
///
///     row:       c + n - p = 0,   n, p >= 0
///     objective: rho*sum(n+p) + (eta(mu)/2)*|D_R (x - x_R)|^2
///
/// Rows are the entire reach of the relaxation: native primal variable bounds
/// are NOT rows and get no elastic pair — their barrier terms, condensed sigma
/// contribution and both fraction-to-boundary legs run in-phase exactly as out
/// of phase, so a bounded variable stays strictly interior on the phase's last
/// iterate as on its first. Deliberate and reference-faithful: a phase that let
/// a variable reach its bound would take the log of a non-positive number and
/// end the solve, restoration or not.
///
/// Ingredients (Ipopt-transcribed):
///  - rho = kRestoPenaltyParameter ("resto_penalty_parameter").
///  - eta(mu) = kRestoProximityWeight*sqrt(mu), recomputed LIVE on every
///    evaluation — unlike the proximal switch, which freezes zeta at entry.
///  - Reference scaling D_R = diag(1/max(1,|x_R_i|)); dr2_ caches d_i^2.
///  - Closed-form slack init at entry with
///    resto_mu = max(mu_outer, |h|_inf, |g+s|_inf), also the phase's starting
///    barrier parameter (entry_mu()).
///  - Per-iteration block condensation eliminates each (n,p,z_n,z_p) group
///    analytically: pivot = n/z_n + p/z_p (POSITIVE; the seam negates it into
///    the KKT (y,y) diagonal); condensed row RHS
///    r~ = (c+n-p) + mu/z_n - (n/z_n)(rho+y) - mu/z_p + (p/z_p)(rho-y);
///    step recovery dn/dz formulas per channel; n,p join the primal
///    fraction-to-boundary cap, z_n,z_p the dual cap under the standard tau rule.
///
/// Disclosed deviations from a literal nested solve:
///  (a) Condensed in-place: same per-step algebra by block-elimination
///      equivalence, but iteration trajectories differ from the enlarged-system
///      reference and there is no inner/outer iteration split. CONDITIONING
///      asymmetry: elimination concentrates the elastic barrier curvature into
///      one constraint-row pivot growing like 1/mu where the enlarged system
///      carries it in bounded diagonal blocks — the condensed KKT is more
///      sensitive to a barrier parameter running ahead of elastic
///      complementarity, which is why the phase feeds the elastic pairs to the
///      barrier oracle and runs the monotone safeguard.
///  (b) Constraint multipliers reset to ZERO on exit (and start at zero on
///      entry): the shipped-default behavior of the reference re-entry path;
///      its dormant least-squares branch is not implemented (no knob exposes it).
///  (c) Single-measure floors: one econ_tol_ stands in for the scaled/unscaled
///      tolerance pair in both the entry guard and stall classification;
///      boundary behavior can differ where two tolerances would diverge.
///  (d) No separate restoration iteration budget: the outer iteration limit
///      bounds this in-place phase; the shared max_feas_rest_ ENTRY budget and
///      entry_permitted() gating match the proximal switch.
///  (e) Second-level re-centering fallback (recenter_elastics): on in-phase
///      line-search failure the separable elastic subproblem is re-solved in
///      closed form holding x,s fixed. Two adaptations, disclosed: (i) z is
///      RE-CENTERED alongside n,p (z_n=mu/n, z_p=mu/p) because the condensation
///      pivots/recovery assume the fresh n*z=mu pairing — stale z would leave
///      the condensed KKT inconsistent (the enlarged-system reference leaves z
///      untouched only because there z are real variables the next Newton step
///      updates); (ii) the fallback is BOUNDED TO ONE SHOT per consecutive-
///      failure run (a second exhaustion falls through to accept-as-is; the
///      budget re-arms on any accepted step and at each entry) — this phase has
///      no inner solver guaranteeing forward progress, so an unbounded re-center
///      risks a no-progress loop.
///
/// Ownership: no SolverContext reference or NLP handle is retained across
/// calls — only the entry snapshot, live elastic state, recovered steps,
/// cached pivots, and per-phase counters below.
class NestedL1Restoration final : public RestorationStrategy {
  public:
    /// Guard-only entry: records the reference point and proximal center for
    /// the entry-permission counters/budget; the full elastic initialization
    /// runs in enter_nested().
    void enter_restoration(const ProgressMeasures &reference,
                           const Eigen::Ref<const Eigen::VectorXd> &primals, double mu) override;

    void exit_restoration() override { active_ = false; }

    bool is_active() const override { return active_; }

    void reset() override;

    /// The frozen-zeta proximal trio is not used by the nested mode (it has its
    /// own live-mu nested_* pieces); reaching these marks a wiring bug — they throw.
    double proximal_objective(const Eigen::Ref<const Eigen::VectorXd> &primals) const override;
    void add_proximal_gradient(const Eigen::Ref<const Eigen::VectorXd> &primals,
                               Eigen::Ref<Eigen::VectorXd> grad_out) const override;
    const Eigen::VectorXd &proximal_diagonal() const override;

    // entry_permitted() (virtual, shared default body) and append_diagnostics()
    // (non-virtual) are both inherited unoverridden from RestorationStrategy —
    // both read/write only entries_/iterations_in_mode_, which this class
    // shares with the base (see restoration.h).

    const ProgressMeasures &reference() const override { return reference_; }

    void note_iteration() override { ++iterations_in_mode_; }

    // --- Nested restoration surface (contract in restoration.h) ---

    bool is_nested() const override { return true; }

    void enter_nested(const ProgressMeasures &reference,
                      const Eigen::Ref<const Eigen::VectorXd> &primals,
                      const Eigen::Ref<const Eigen::VectorXd> &eq_residuals,
                      const Eigen::Ref<const Eigen::VectorXd> &iq_residuals,
                      double outer_mu) override;

    double entry_mu() const override { return resto_mu_; }

    const Eigen::VectorXd &e_pivots() const override { return e_pivots_; }
    const Eigen::VectorXd &i_pivots() const override { return i_pivots_; }

    void nested_complementarity(double &sum, double &min_comp, double &max_comp,
                                int &count) const override;

    void condensed_residuals(double mu, const Eigen::Ref<const Eigen::VectorXd> &eq_residuals,
                             const Eigen::Ref<const Eigen::VectorXd> &iq_residuals,
                             const Eigen::Ref<const Eigen::VectorXd> &eq_lmults,
                             const Eigen::Ref<const Eigen::VectorXd> &iq_lmults,
                             Eigen::Ref<Eigen::VectorXd> eq_rtilde_out,
                             Eigen::Ref<Eigen::VectorXd> iq_rtilde_out) const override;

    double nested_objective(double mu,
                            const Eigen::Ref<const Eigen::VectorXd> &primals) const override;
    void add_nested_gradient(double mu, const Eigen::Ref<const Eigen::VectorXd> &primals,
                             Eigen::Ref<Eigen::VectorXd> grad_out) const override;
    void nested_primal_diagonal(double mu, Eigen::Ref<Eigen::VectorXd> diag_out) const override;

    void recover_elastic_steps(double mu, const Eigen::Ref<const Eigen::VectorXd> &eq_lmults,
                               const Eigen::Ref<const Eigen::VectorXd> &iq_lmults,
                               const Eigen::Ref<const Eigen::VectorXd> &eq_dy,
                               const Eigen::Ref<const Eigen::VectorXd> &iq_dy) override;

    void recenter_elastics(double mu, const Eigen::Ref<const Eigen::VectorXd> &eq_residuals,
                           const Eigen::Ref<const Eigen::VectorXd> &iq_residuals) override;

    double primal_boundary_alpha(double tau) const override;
    double dual_boundary_alpha(double tau) const override;
    void apply_elastic_step(double alpha_primal, double alpha_dual) override;

    double trial_objective(double mu, double alpha,
                           const Eigen::Ref<const Eigen::VectorXd> &trial_primals) const override;
    void trial_residual_shift(double alpha, Eigen::Ref<Eigen::VectorXd> eq_shift_out,
                              Eigen::Ref<Eigen::VectorXd> iq_shift_out) const override;

    /// Test/diagnostic observers.
    const Eigen::VectorXd &dr2() const { return dr2_; }
    const Eigen::VectorXd &snapshot() const { return x_r_; }
    double resto_mu() const { return resto_mu_; }
    int entries() const { return entries_; }
    int iterations_in_mode() const { return iterations_in_mode_; }
    int recenter_calls() const { return recenter_calls_; }

    /// Live elastic state readers: equality channel (ec_*) and inequality channel (ic_*),
    /// slacks/multipliers (n/p/zn/zp) and recovered steps (dn/dp/dzn/dzp).
    const Eigen::VectorXd &ec_n() const { return n_e_; }
    const Eigen::VectorXd &ec_p() const { return p_e_; }
    const Eigen::VectorXd &ec_zn() const { return z_ne_; }
    const Eigen::VectorXd &ec_zp() const { return z_pe_; }
    const Eigen::VectorXd &ic_n() const { return n_i_; }
    const Eigen::VectorXd &ic_p() const { return p_i_; }
    const Eigen::VectorXd &ic_zn() const { return z_ni_; }
    const Eigen::VectorXd &ic_zp() const { return z_pi_; }

    const Eigen::VectorXd &ec_dn() const { return dn_e_; }
    const Eigen::VectorXd &ec_dp() const { return dp_e_; }
    const Eigen::VectorXd &ec_dzn() const { return dzn_e_; }
    const Eigen::VectorXd &ec_dzp() const { return dzp_e_; }
    const Eigen::VectorXd &ic_dn() const { return dn_i_; }
    const Eigen::VectorXd &ic_dp() const { return dp_i_; }
    const Eigen::VectorXd &ic_dzn() const { return dzn_i_; }
    const Eigen::VectorXd &ic_dzp() const { return dzp_i_; }

  private:
    /// Initializes one channel's elastic state from residual values at the
    /// given barrier parameter (the closed-form positive-root solve). Single
    /// home of the quadratic solve — used both by the entry init (at resto_mu_)
    /// and the second-level re-center (at live mu); never duplicated.
    void init_channel(const Eigen::Ref<const Eigen::VectorXd> &residuals, double mu,
                      Eigen::VectorXd &n, Eigen::VectorXd &p, Eigen::VectorXd &zn,
                      Eigen::VectorXd &zp) const;

    /// Recomputes a channel's pivot vector from its live elastic state.
    static void update_pivots(const Eigen::VectorXd &n, const Eigen::VectorXd &p,
                              const Eigen::VectorXd &zn, const Eigen::VectorXd &zp,
                              Eigen::VectorXd &pivots);

    bool active_ = false;
    ProgressMeasures reference_;

    /// Entry snapshot (proximal center and cached D_R^2) and entry barrier.
    Eigen::VectorXd x_r_;
    Eigen::VectorXd dr2_;
    double resto_mu_ = 0.0;

    /// Live elastic state — equality channel (EC) and inequality channel (IC).
    Eigen::VectorXd n_e_, p_e_, z_ne_, z_pe_;
    Eigen::VectorXd n_i_, p_i_, z_ni_, z_pi_;

    /// Last recovered steps per channel (set by recover_elastic_steps).
    Eigen::VectorXd dn_e_, dp_e_, dzn_e_, dzp_e_;
    Eigen::VectorXd dn_i_, dp_i_, dzn_i_, dzp_i_;

    /// Cached pivot vectors (recomputed at entry and after apply_elastic_step).
    Eigen::VectorXd e_pivots_, i_pivots_;

    /// Count of second-level re-center invocations (test/diagnostic observer
    /// of the fallback; not folded into SolveResult).
    int recenter_calls_ = 0;
};

} // namespace hven::solvers
