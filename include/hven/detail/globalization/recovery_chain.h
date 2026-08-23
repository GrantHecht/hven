// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <vector>

#include <Eigen/Core>

#include "hven/detail/globalization/acceptance_strategy.h"
#include "hven/detail/globalization/globalization_mechanism.h"
#include "hven/detail/globalization/solver_context.h"
#include "hven/detail/interior/iterate_info.h"
// InteriorPointSolver::LineSearchModes (forwarded to the acceptance re-test during a
// second-order correction) requires the complete InteriorPointSolver class; pulled in
// transitively via the two globalization headers above. See solver_context.h's
// one-directional include-discipline note.

namespace hven::solvers {

// Recovery-dispatch depths: which link (if any) actually resolved a given
// rejection — Soc=0, Extended=1, Watchdog=2, Unresolved=3 (classic give-up),
// Restoration=4. Only a composing/wrapping link knows its position in the
// dispatch order, so individual links (SOC, extended backtracking) never write
// it — ChainedRecovery/WatchdogRecovery and FeasibilitySwitchRecovery do. The
// solver also overwrites it directly in two feasibility-restoration branches
// (the elastic re-centering fallback and the un-evaluable-fallback entry, the
// latter so the histogram attributes that iteration to restoration rather than
// to whatever depth the chain resolved). Backs the SolveResult recovery-depth
// histogram.
inline constexpr int kRecoveryDepthSoc = 0;
inline constexpr int kRecoveryDepthExtended = 1;
inline constexpr int kRecoveryDepthWatchdog = 2;
inline constexpr int kRecoveryDepthUnresolved = 3; // classic give-up: no link resolved it.
/// Feasibility-restoration mode-switch bucket, written when an inner
/// kAcceptAsIs is converted into a switch (and directly by the solver's
/// restoration branches, see above). Reachable whenever restoration_mode_ !=
/// off; the histogram is sized to 5 to hold this bucket.
inline constexpr int kRecoveryDepthRestoration = 4;

/// @brief Ordered dispatch invoked after an AcceptanceStrategy rejects a
/// trial step.
///
/// Scope callout: this is the POST-REJECTION dispatcher. The inertia/
/// perturbation ladder (Zfac cycling + escalation) is a separate mechanism that
/// stays inside the factorization and is deliberately not wrapped here.
///
/// The base contract is stateless; concrete links that hold per-solve state
/// clear it behind reset().
class RecoveryChain {
  public:
    virtual ~RecoveryChain() = default;

    /// @brief Action a recovery chain link may take on a rejected step.
    ///
    /// Value semantics: kAcceptAsIs overrides the rejection, taking the step
    /// anyway; kRetry retries this iteration (after a correction or extended
    /// backtrack); kSwitchToFeasibility hands off to a restoration strategy
    /// (selected by restoration_mode_); kSoftFeasibilityStep takes the full
    /// fraction-to-boundary step on the current direction as a soft feasibility
    /// pre-stage, evaluated under a primal-dual-error reduction test before
    /// committing to a full restoration switch — produced only by
    /// FeasibilitySwitchRecovery, and only for a nested restoration;
    /// kGiveUp means no recovery available (the shipped default path is
    /// kAcceptAsIs; there is no give-up branch in the current loop).
    enum class Action { kAcceptAsIs, kRetry, kSwitchToFeasibility, kSoftFeasibilityStep, kGiveUp };

    /// @brief Recovery dispatch on a rejected step.
    ///
    /// @param Citer The just-rejected iterate's record; mutable — an
    ///   implementation may annotate it (e.g. stamping accepted_ when a
    ///   correction is taken) and reads its trigger signals first.
    /// @param iters Read-only iteration history a link may consult.
    /// @param ctx Access to settings (e.g. the SOC cap), the NLP for trial
    ///   evaluation, and the still-LIVE KKT factorization a correction
    ///   re-solves against (no refactor).
    ///
    /// The remaining parameters are the live per-iteration working set (the
    /// same objects compute_step just operated on), threaded so a link can
    /// build and re-test a corrected step. The interface places NO no-aliasing
    /// precondition on the five VectorXd& parameters: the production caller
    /// passes distinct buffers, but test doubles legitimately bind one shared
    /// buffer to several slots, so implementations must order their writes to
    /// be aliasing-robust.
    ///
    /// Contract: a link returning kRetry must leave the accepted corrected
    /// step in DXSL (and its length in alpha) so the caller's
    /// XSL += alpha*DXSL commit applies it; on any other Action DXSL/alpha
    /// are left as compute_step produced them. soc_steps and
    /// watchdog_activations are diagnostic accumulators; resolved_depth is an
    /// out-parameter seeded to kRecoveryDepthUnresolved by the caller (see
    /// the depth constants above for who writes it).
    virtual Action
    on_step_rejected(IterateInfo &Citer, const std::vector<IterateInfo> &iters, SolverContext &ctx,
                     AcceptanceStrategy &acceptance, GlobalizationMechanism &mechanism,
                     InteriorPointSolver::LineSearchModes lsmode, double obj_scale, double mu,
                     double prim_obj, double barr_obj, Eigen::VectorXd &XSL, Eigen::VectorXd &DXSL,
                     Eigen::VectorXd &XSL2, Eigen::VectorXd &RHS, Eigen::VectorXd &RHS2,
                     double &alpha, double &alphap, double &alphad, int &soc_steps,
                     int &resolved_depth, int &watchdog_activations) = 0;

    /// @brief Called once per genuinely ACCEPTED iteration — i.e. the rejection
    /// hook was skipped because should_dispatch_recovery was false.
    ///
    /// Implementations may reset counters tied to real progress. Must NOT
    /// touch solver state (none is even passed here) or mutate Citer/iters/
    /// ctx. Default: empty body.
    virtual void notify_step_accepted() {}

    /// @brief mu-event / phase-change hook; stateful links clear their
    /// persistent state here.
    virtual void reset() = 0;
};

/// @brief Recovery-dispatch gate: the chain is driven only when the step
/// direction was usable (good_step) AND citer.accepted_ reads false.
///
/// Callout: accepted_ reads false for two reasons — the strategy rejected the
/// trial, OR the solver force-rejects a trial the strategy DID accept when
/// this iteration's factorization exhausted the inertia-correction ladder
/// (that site runs before this gate), so a genuinely accepted step can still
/// reach the hook. The non-finite-direction path (no line search) is excluded
/// via good_step regardless. Factored into one definition so the unit test
/// that guards the gate calls the real condition.
inline bool should_dispatch_recovery(bool good_step, const IterateInfo &citer) {
    return good_step && !citer.accepted_;
}

} // namespace hven::solvers
