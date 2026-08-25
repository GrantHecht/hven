// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <vector>

#include <Eigen/Core>

#include "hven/detail/globalization/acceptance_strategy.h"
#include "hven/detail/globalization/globalization_mechanism.h"
#include "hven/detail/globalization/recovery_chain.h"
#include "hven/detail/globalization/solver_context.h"
#include "hven/detail/interior/iterate_info.h"

namespace hven::solvers {

/// kappa_soc — required per-correction reduction factor in the constraint
/// violation (Wächter & Biegler 2006, §2.4): a correction is worth continuing
/// only while it keeps cutting the violation by at least this factor.
inline constexpr double kSocViolationDecrease = 0.99;

/// Recommended value to enable SOC with (the reference implementation's
/// default: up to four corrections per rejected step). Settings' own default
/// stays 0 (off); this names the value to opt in with.
inline constexpr int kSocRecommendedMaxCorrections = 4;

/// @brief Trigger predicate (Wächter & Biegler 2006, §2.4).
///
/// Fire a second-order correction only when the FIRST trial step (backtracking
/// index 0) was the one rejected AND that trial did not reduce the constraint
/// violation relative to the current iterate. A negative theta means "no
/// infeasibility reading available" (the LANG merit variant records none;
/// -1 is IterateInfo's unset sentinel), in which case SOC conservatively does
/// not trigger.
///
/// @param current_infeasibility Must be the SAME quantity
///   theta_at_first_rejection_ records, under whichever norm convention the
///   driving acceptance strategy uses: squared L2 of the full constraint block
///   on the classic path, L1 on the generic path. Either way the caller derives
///   it from RHS.all_cons(), whose inequality block carries the identical slack
///   reset.
///
/// Known asymmetry: the augmented-Lagrangian merit variant zeroes constraint
/// entries within tolerance when it records theta_at_first_rejection_, while
/// current_infeasibility is not tolerance-zeroed. The zeroing can only SHRINK
/// the trial-side theta, making the trigger HARDER to satisfy — the asymmetry
/// suppresses SOC near feasibility and can never spuriously fire it.
inline bool soc_should_trigger(const IterateInfo &citer, double current_infeasibility) {
    if (citer.first_rejection_iter_ != 0)
        return false;
    const double trial_infeasibility = citer.theta_at_first_rejection_;
    if (trial_infeasibility < 0.0)
        return false;
    return trial_infeasibility >= current_infeasibility;
}

/// @brief Termination predicate (Wächter & Biegler 2006, §2.4): after a
/// rejected correction, keep correcting only while the cap has not been
/// reached AND the corrected trial's violation is still dropping by at least
/// kSocViolationDecrease versus the previous violation.
inline bool soc_should_continue(double trial_violation, double prev_violation,
                                int corrections_done, int max_soc) {
    if (corrections_done >= max_soc)
        return false;
    return trial_violation <= kSocViolationDecrease * prev_violation;
}

/// Outcome of a single correction attempt, returned by the correction
/// primitive to the loop driver.
struct SocCorrectionOutcome {
    /// Corrected step accepted by the re-run acceptance test; the corrected
    /// direction/length have been committed in place.
    bool accepted;

    /// Corrected first-trial constraint violation, in whichever norm
    /// drives_classic_path() selected (see soc_should_trigger()). Only
    /// meaningful when !accepted; drives the next accumulation and the
    /// termination test.
    double trial_violation;
};

/// @brief Correction loop driver — pure policy, no KKT/NLP machinery. Runs the
/// correction primitive do_correction(correction_index, prev_violation) until
/// a correction is accepted (kRetry) or the termination policy stops it
/// (kAcceptAsIs). Increments soc_steps once per attempted correction (one
/// back-substitution each). Templated on the primitive so unit tests drive it
/// with a scripted lambda. The first correction always runs once triggered;
/// subsequent ones are gated by soc_should_continue.
template <class CorrectionFn>
RecoveryChain::Action soc_run_loop(double first_trial_violation, int max_soc, int &soc_steps,
                                   CorrectionFn &&do_correction) {
    double prev_violation = first_trial_violation;
    int corrections = 0;
    while (true) {
        const SocCorrectionOutcome outcome = do_correction(corrections, prev_violation);
        ++corrections;
        ++soc_steps;
        if (outcome.accepted)
            return RecoveryChain::Action::kRetry;
        if (!soc_should_continue(outcome.trial_violation, prev_violation, corrections, max_soc))
            return RecoveryChain::Action::kAcceptAsIs;
        prev_violation = outcome.trial_violation;
    }
}

/// @brief Opt-in second-order correction recovery link (Wächter & Biegler 2006,
/// §2.4): applied after the line search rejects a step on its very first trial.
///
/// Mechanics: a correction re-solves the SAME KKT system on the LIVE
/// factorization (no refactor — one back-substitution) with the constraint
/// block of the right-hand side replaced by an accumulated corrected value;
/// the objective block is unchanged. The corrected direction is
/// fraction-to-boundary scaled and the FULL acceptance backtrack re-runs on it
/// THROUGH THE MECHANISM (run_acceptance_backtrack), so the corrected trial
/// faces the same criteria as the ordinary step — this is what lets SOC compose
/// with every acceptance strategy rather than only classic merit. An accepted
/// corrected step replaces the rejected one (kRetry); once the violation
/// stagnates or the Settings::max_soc_ cap is hit, the originally-rejected step
/// is taken (kAcceptAsIs — bit-identical to the no-SOC path).
///
/// Default is off (max_soc_ == 0): not even constructed then (a NoopRecovery
/// installs instead), so the solver stays bit-identical to its no-SOC behavior.
///
/// Interaction rules with the generic-path strategies:
///  (a) Funnel width / filter augmentation run on the ACCEPTED corrected trial
///      (the strategy's own accept-time bookkeeping), and rejected correction
///      rungs advance the filter's last-rejection reset heuristic like any
///      other line-search invocation — a corrected re-test IS just another
///      line search.
///  (b) During an active nested l1 restoration phase the chain runs in-phase;
///      the acceptance re-test measures the elastic subproblem's objective and
///      infeasibility via the restoration trial seam, while the c_soc
///      accumulation and CURRENT-side trigger measure stay in raw-slack-reset
///      space (the TRIAL-side trigger is elastic-shifted in-phase) — a mixed-
///      space trigger comparison that is a pre-existing property shared with
///      the classic link: it affects only heuristic sensitivity, never the
///      rigor of the phase-aware acceptance decision.
///  (c) Watchdog composition is unchanged: the watchdog wraps the chain and
///      delegates rejections to the inner chain, using the strategy-agnostic
///      merit proxy for its own arm/revert logic.
class SocRecovery : public RecoveryChain {
  public:
    SocRecovery() = default;

    Action on_step_rejected(IterateInfo &Citer, const std::vector<IterateInfo> &iters,
                            SolverContext &ctx, AcceptanceStrategy &acceptance,
                            GlobalizationMechanism &mechanism,
                            InteriorPointSolver::LineSearchModes lsmode, double obj_scale,
                            double mu, double prim_obj, double barr_obj, Eigen::VectorXd &XSL,
                            Eigen::VectorXd &DXSL, Eigen::VectorXd &XSL2, Eigen::VectorXd &RHS,
                            Eigen::VectorXd &RHS2, double &alpha, double &alphap, double &alphad,
                            int &soc_steps, int &resolved_depth,
                            int &watchdog_activations) override;

    /// Stateless (per RecoveryChain's ownership rule): nothing to clear.
    void reset() override {}

  private:
    /// @brief Evaluates the constraint block c(x_k + alpha*dir) into cons_out
    /// (size equal_cons_ + inequal_cons_, layout [eq | iq]) using the same
    /// request and slack-reset convention as the RHS assembly, so the value is
    /// directly comparable to RHS.all_cons(). Trial primals/slacks are written
    /// into xsl2_scratch.
    void eval_trial_constraints(SolverContext &ctx, double obj_scale, const Eigen::VectorXd &XSL,
                                const Eigen::VectorXd &dir, double alpha,
                                Eigen::VectorXd &xsl2_scratch, Eigen::VectorXd &cons_out);
};

} // namespace hven::solvers
