// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <memory>
#include <stdexcept>
#include <vector>

#include <Eigen/Core>

#include "hven/detail/interior/bound_set.h"
#include "hven/detail/globalization/acceptance_strategy.h"
#include "hven/detail/globalization/globalization_mechanism.h"
#include "hven/detail/globalization/recovery_chain.h"
#include "hven/detail/globalization/solver_context.h"
#include "hven/detail/interior/iterate_info.h"

namespace hven::solvers {

/// Watchdog constants (Chamberlain, Powell, Lemaréchal & Pedersen 1982;
/// values per the reference interior-point implementation in
/// Wächter & Biegler 2006): arm after this many consecutive shortened
/// iterations; allow at most this many relaxed trial iterations before reverting.
inline constexpr int kWatchdogShortenedIterTrigger = 10;
inline constexpr int kWatchdogTrialIterMax = 3;

/// @brief Opt-in continuation of the classic backtracking ladder past its
/// capped rejection.
///
/// Mechanics: continues the SAME ladder — same direction (DXSL, untouched),
/// same alpha_red_ divisor, same merit acceptance test — for up to
/// Settings::ls_extended_iters_ further EXTERNAL trials. Each external trial is
/// one run_acceptance_backtrack() call (the classic-vs-generic dispatch) on a
/// SCALED COPY of DXSL; going through the dispatcher (never calling
/// classic_line_search directly) is what makes the link compose with generic
/// strategies. This works because both the classic and generic internal loops
/// start their own alpha at 1.0 and, on exhaustion, return the value ONE MORE
/// division past the last-tested point — always the next UNTESTED rung:
///   - Seed `scale` with the alpha live at hook time (compute_step's return,
///     forwarded as `alpha`) — do NOT restart at 1.0.
///   - Scale a local copy dxsl_ext = scale*DXSL: the callee's internal 1.0
///     trial lands exactly on `scale` relative to DXSL, and its divisions land
///     on the next rungs — zero redundant re-testing.
///   - On that call's exhaustion, returned-alpha times its `scale` is the next
///     untested rung; carry it forward.
/// Each external call may itself run several internal sub-trials when
/// max_ls_iters_ > 1 (the same call-counts-as-one convention SOC uses);
/// ls_extended_iters_ bounds external calls, not raw alpha divisions.
///
/// Stateless (RecoveryChain's ownership rule); the scaled-direction scratch is
/// local to on_step_rejected.
class ExtendedBacktrackRecovery : public RecoveryChain {
  public:
    ExtendedBacktrackRecovery() = default;

    Action on_step_rejected(IterateInfo &Citer, const std::vector<IterateInfo> &iters,
                            SolverContext &ctx, AcceptanceStrategy &acceptance,
                            GlobalizationMechanism &mechanism,
                            InteriorPointSolver::LineSearchModes lsmode, double obj_scale,
                            double mu, double prim_obj, double barr_obj, Eigen::VectorXd &XSL,
                            Eigen::VectorXd &DXSL, Eigen::VectorXd &XSL2, Eigen::VectorXd &RHS,
                            Eigen::VectorXd &RHS2, double &alpha, double &alphap, double &alphad,
                            int &soc_steps, int &resolved_depth,
                            int &watchdog_activations) override;

    /// Stateless: nothing to clear.
    void reset() override {}
};

/// @brief Pure arm/trial/revert/reset watchdog state machine (Chamberlain,
/// Powell, Lemaréchal & Pedersen 1982), split from plumbing so transitions are
/// unit-testable with scripted (mu, merit) sequences.
///
/// Architectural scope note (read before touching arming semantics): the only
/// REJECTION-observing hook into the solve loop is on_step_rejected, reached
/// two ways — (1) a FULL backtrack rejection (every max_ls_iters_ trial failed),
/// or (2) a FORCE-rejection of an accepted step on an exhausted inertia ladder.
/// A backtracked-but-accepted iteration (alpha < 1, accepted, factorization
/// healthy) never reaches the hook. "Shortened iteration" here therefore means
/// "a full or forced rejection was dispatched", a CONSERVATIVE narrowing of
/// full paper semantics: it can only arm later or not at all. A genuinely
/// accepted iteration between rejections arrives via notify_step_accepted()
/// (which resets the consecutive counter — without it non-consecutive
/// rejections would sum straight across an accept and mis-arm). Known
/// observation gap: a non-finite-step (divergence-path) iteration reaches
/// NEITHER hook and is invisible to the counter.
class WatchdogState {
  public:
    /// Outcome of feeding one rejected iteration to the machine.
    enum class Outcome {
        kAccumulate,    ///< Not armed: no override — delegate to the wrapped chain.
        kArmed,         ///< Arm threshold just reached THIS call (== trial #1):
                        ///< override with a relaxed accept; caller snapshots now.
        kTrialRelax,    ///< Armed, within the window, no progress yet: relaxed accept.
        kTrialProgress, ///< Armed, this iterate beat the snapshot merit: disarm —
                        ///< delegate to the wrapped chain.
        kTrialRevert,   ///< Armed, window exhausted with no progress: revert to
                        ///< the snapshot and disarm.
    };

    void reset() {
        consecutive_shortened_ = 0;
        armed_ = false;
        trial_count_ = 0;
        have_last_mu_ = false;
        last_mu_ = 0.0;
        snapshot_mu_ = 0.0;
        snapshot_merit_ = 0.0;
    }

    bool armed() const { return armed_; }
    int consecutive_shortened() const { return consecutive_shortened_; }
    int trial_count() const { return trial_count_; }

    /// @brief Feeds one more fully-rejected iteration into the machine.
    /// @param mu Current barrier parameter; a change since the previous call
    ///   invalidates the consecutive count (different barrier subproblem).
    /// @param merit Scalar quality measure for the CURRENT iterate (the
    ///   decorator passes prim_obj + barr_obj — see WatchdogRecovery).
    Outcome record_rejected_iteration(double mu, double merit) {
        if (armed_) {
            if (mu != snapshot_mu_) {
                // Barrier parameter moved mid-watchdog: the snapshot's merit
                // reference is no longer comparable, so the whole state resets
                // (NOT a revert) and this call starts a fresh count.
                reset();
                return accumulate(mu, merit);
            }
            ++trial_count_;
            if (merit < snapshot_merit_) {
                reset();
                return Outcome::kTrialProgress;
            }
            if (trial_count_ >= kWatchdogTrialIterMax) {
                reset();
                return Outcome::kTrialRevert;
            }
            return Outcome::kTrialRelax;
        }
        return accumulate(mu, merit);
    }

    /// Resets the consecutive-shortened counter on a genuinely accepted
    /// iteration (a real accept breaks the "consecutive" run arming depends
    /// on). Does not touch armed_/trial_count_/the snapshot — an open trial
    /// window's own bookkeeping is unaffected by this call.
    void record_accepted_iteration() { consecutive_shortened_ = 0; }

  private:
    Outcome accumulate(double mu, double merit) {
        if (have_last_mu_ && mu != last_mu_)
            consecutive_shortened_ = 0;
        last_mu_ = mu;
        have_last_mu_ = true;
        ++consecutive_shortened_;
        if (consecutive_shortened_ >= kWatchdogShortenedIterTrigger) {
            armed_ = true;
            snapshot_mu_ = mu;
            snapshot_merit_ = merit;
            trial_count_ = 1; // This call is trial #1 of the window.
            return Outcome::kArmed;
        }
        return Outcome::kAccumulate;
    }

    int consecutive_shortened_ = 0;
    bool armed_ = false;
    int trial_count_ = 0;
    bool have_last_mu_ = false;
    double last_mu_ = 0.0;
    double snapshot_mu_ = 0.0;
    double snapshot_merit_ = 0.0;
};

/// @brief RecoveryChain decorator driving WatchdogState against the real
/// working set. Wraps whatever chain is configured underneath as an OUTER
/// decorator: kAccumulate/kTrialProgress forward the rejection unchanged;
/// while armed within the window (kArmed/kTrialRelax) the rejection is
/// overridden with a relaxed accept; on kTrialRevert XSL is restored to the
/// pre-watchdog snapshot (DXSL zeroed, alpha = 0, so the caller's commit is a
/// no-op). The merit proxy for "did the point improve" is prim_obj + barr_obj
/// — the same leading term every classic merit variant builds its own test
/// from — an always-available stand-in since no generic current-merit accessor
/// exists on the acceptance surface.
///
/// Holds real per-solve state (the state machine plus the XSL/bound-duals
/// snapshots) behind reset(), which also propagates to the wrapped inner chain.
class WatchdogRecovery : public RecoveryChain {
  public:
    /// @param inner Must be non-null — every construction site supplies a real
    /// chain (NoopRecovery at minimum), so the invariant is enforced once up
    /// front rather than guarded at every use.
    explicit WatchdogRecovery(std::unique_ptr<RecoveryChain> inner) : inner_(std::move(inner)) {
        if (!inner_)
            throw std::invalid_argument("WatchdogRecovery: inner recovery chain must not be null");
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

    /// Resets the shortened-iteration counter on real progress, then threads
    /// the notification through to the wrapped chain.
    void notify_step_accepted() override {
        state_.record_accepted_iteration();
        inner_->notify_step_accepted();
    }

    void reset() override {
        state_.reset();
        snapshot_xsl_.resize(0);
        snapshot_bound_duals_ = BoundDualState{};
        inner_->reset();
    }

  private:
    std::unique_ptr<RecoveryChain> inner_;
    WatchdogState state_;
    Eigen::VectorXd snapshot_xsl_;

    /// Bound multipliers belonging to snapshot_xsl_. A revert discards the
    /// whole trajectory the watchdog explored, and z is part of that
    /// trajectory (Sigma = z/d and the z-form dual infeasibility are built from
    /// it): multipliers left over from a discarded path would describe an
    /// iterate that no longer exists. Empty (stash/restore skipped) on a
    /// problem with no variable bounds.
    BoundDualState snapshot_bound_duals_;
};

/// @brief Composes SOC and extended backtracking in a fixed order: SOC first.
/// Rationale: SOC re-solves the rejected direction on the live factorization
/// (one informed back-substitution), while extended backtracking merely re-tests
/// the already-rejected direction at smaller alpha (no solve) — the more
/// information-bearing recovery gets first refusal; only if it declines (not
/// triggered, or exhausted) does the cheaper ladder-continuation get a turn.
/// Either pointer may be null (link disabled by Settings); null links are
/// skipped.
class ChainedRecovery : public RecoveryChain {
  public:
    ChainedRecovery(std::unique_ptr<RecoveryChain> soc, std::unique_ptr<RecoveryChain> extended)
        : soc_(std::move(soc)), extended_(std::move(extended)) {}

    Action on_step_rejected(IterateInfo &Citer, const std::vector<IterateInfo> &iters,
                            SolverContext &ctx, AcceptanceStrategy &acceptance,
                            GlobalizationMechanism &mechanism,
                            InteriorPointSolver::LineSearchModes lsmode, double obj_scale,
                            double mu, double prim_obj, double barr_obj, Eigen::VectorXd &XSL,
                            Eigen::VectorXd &DXSL, Eigen::VectorXd &XSL2, Eigen::VectorXd &RHS,
                            Eigen::VectorXd &RHS2, double &alpha, double &alphap, double &alphad,
                            int &soc_steps, int &resolved_depth,
                            int &watchdog_activations) override;

    /// Propagates to whichever links are present (legitimately null when a
    /// link is disabled by Settings).
    void notify_step_accepted() override {
        if (soc_)
            soc_->notify_step_accepted();
        if (extended_)
            extended_->notify_step_accepted();
    }

    void reset() override {
        if (soc_)
            soc_->reset();
        if (extended_)
            extended_->reset();
    }

  private:
    std::unique_ptr<RecoveryChain> soc_;
    std::unique_ptr<RecoveryChain> extended_;
};

} // namespace hven::solvers
