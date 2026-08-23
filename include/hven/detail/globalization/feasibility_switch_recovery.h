// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <memory>
#include <stdexcept>

#include <Eigen/Core>

#include "hven/detail/globalization/recovery_chain.h"

// Test fixture (declared for the friend grant below).
class NestedLifecycleHarness;

namespace hven::solvers {

/// Soft feasibility pre-stage constant: a soft step is accepted while its trial
/// primal-dual error is at most this fraction of the current one (Ipopt's
/// soft_resto_pderror_reduction_factor, shipped default 1 - 1e-4; the
/// factor-0 "disable" branch is not transcribed — the pre-stage ships
/// unconditionally for the nested mode with no knob).
inline constexpr double kSoftRestoPdErrorReductionFactor = 1.0 - 1e-4;

/// Maximum number of successive soft pre-stage iterations before escalating to
/// the full restoration switch (Ipopt max_soft_resto_iters, shipped default 10).
inline constexpr int kMaxSoftRestoIters = 10;

/// @brief Outermost recovery link: converts a ladder-exhausted step rejection
/// into a feasibility-restoration mode switch — the last resort, tried only
/// when nothing else salvaged the step.
///
/// Behavior: delegate the whole rejection to the inner chain first. If the
/// inner chain RESOLVES it (kRetry / kSwitchToFeasibility / kGiveUp, or a
/// kAcceptAsIs whose resolved_depth was already stamped by a link that took
/// the relaxed step on purpose), the outcome passes through untouched. The
/// LOAD-BEARING discriminator is therefore the resolved_depth out-parameter,
/// not the Action alone: kAcceptAsIs is overloaded between "ladder exhausted,
/// nothing resolved it" (depth still the caller-seeded unresolved sentinel)
/// and "a link resolved this on purpose and its resolution happens to be
/// kAcceptAsIs" (depth already set). Only the unresolved case is intercepted;
/// if a real restoration episode is warranted — current point not already
/// near-feasible, per-phase entry budget not exhausted
/// (RestorationStrategy::entry_permitted), restoration not already active —
/// returns kSwitchToFeasibility; otherwise the inner kAcceptAsIs passes
/// through unchanged.
///
/// This link performs NO mutation: it does not enter restoration, snapshot
/// primals, or touch the working set — the solver's kSwitchToFeasibility case
/// is the single place actual mode entry happens. The violation tested by
/// entry_permitted is the L1 norm of the current KKT constraint block, the
/// same measure the solver builds the restoration reference from.
///
/// Soft feasibility pre-stage (nested restoration only): before committing to
/// a full switch, a nested strategy first gets soft steps — the full
/// fraction-to-boundary step on the current direction, tested under a
/// primal-dual-error reduction rule (kSoftRestoPdErrorReductionFactor). A
/// successive-iteration counter lives here; past kMaxSoftRestoIters in a row
/// the link escalates to the real switch. Deviation note (vs Ipopt, which
/// tries soft restoration the moment its line search fails): here the soft
/// pre-stage runs only once the SOC/watchdog ladder is exhausted — soft steps
/// are attempted strictly later, the conservative composition with the extra
/// machinery. The proximal-switch mode has NO pre-stage: it switches directly,
/// byte-for-byte as before.
///
/// Acceptance-notification timing callout: the single
/// notify_switch_to_feasibility stays at the FULL restoration entry in the
/// solver — issued exactly once per episode, never during the pre-stage. Soft
/// steps are ordinary optimality-phase steps; keeping the acceptance strategy
/// in the optimality phase across the pre-stage is what lets the pre-stage
/// exit be the ordinary acceptance recovering on its own, and guarantees the
/// filter augmentation is not issued twice. Disclosed consequence (a deviation
/// from Ipopt, which augments its filter at soft-stage start): during the
/// pre-stage the trigger point's (theta, phi) pair has NOT yet been added to
/// the filter, so soft steps are tested against the un-augmented optimality
/// state — a soft step accepted here could be one Ipopt would have rejected,
/// never the reverse (augmentation only shrinks the acceptable region). The
/// pre-stage cannot loop on this: the counter is cleared only by a genuine
/// optimality acceptance, so at most kMaxSoftRestoIters such steps occur
/// before escalation performs the augmentation.
///
/// Ownership: holds only the inner chain plus this link's own soft-pre-stage
/// counter (cleared by reset()); reset()/notify_step_accepted() thread through
/// to the inner chain as well.
class FeasibilitySwitchRecovery : public RecoveryChain {
  public:
    explicit FeasibilitySwitchRecovery(std::unique_ptr<RecoveryChain> inner)
        : inner_(std::move(inner)) {
        if (!inner_)
            throw std::invalid_argument(
                "FeasibilitySwitchRecovery: inner recovery chain must not be null");
    }

    Action on_step_rejected(IterateInfo &Citer, const std::vector<IterateInfo> &iters,
                            SolverContext &ctx, AcceptanceStrategy &acceptance,
                            GlobalizationMechanism &mechanism,
                            InteriorPointSolver::LineSearchModes lsmode, double obj_scale,
                            double mu, double prim_obj, double barr_obj, Eigen::VectorXd &XSL,
                            Eigen::VectorXd &DXSL, Eigen::VectorXd &XSL2, Eigen::VectorXd &RHS,
                            Eigen::VectorXd &RHS2, double &alpha, double &alphap, double &alphad,
                            int &soc_steps, int &resolved_depth,
                            int &watchdog_activations) override;

    /// A genuinely accepted regular step means the ordinary optimality-phase
    /// acceptance test recovered, so any soft pre-stage in progress has ended:
    /// clear the counter, then thread through to the inner chain.
    void notify_step_accepted() override {
        soft_counter_ = 0;
        inner_->notify_step_accepted();
    }

    /// mu-event / phase-change reset also clears the soft pre-stage counter (a
    /// restoration entry or exit resets the pre-stage), then threads through.
    void reset() override {
        soft_counter_ = 0;
        inner_->reset();
    }

  private:
    friend class ::NestedLifecycleHarness;

    std::unique_ptr<RecoveryChain> inner_;

    /// Successive soft pre-stage iterations taken (nested restoration only).
    /// Incremented per soft step; past kMaxSoftRestoIters the link escalates.
    /// Cleared by notify_step_accepted()/reset(). A mid-pre-stage accept-as-is
    /// (entry refused by guard/budget) does NOT clear it — at worst the
    /// pre-stage escalates one episode early into the full restoration
    /// backstop, which is the safe direction.
    int soft_counter_ = 0;
};

} // namespace hven::solvers
