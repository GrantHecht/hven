// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <Eigen/Core>

#include "hven/detail/globalization/solver_context.h"
#include "hven/detail/interior/iterate_info.h"
// InteriorPointSolver::BarrierModes requires the complete InteriorPointSolver class; see
// acceptance_strategy.h's include note for why this is a plain,
// non-circular include (interior_point_solver.h does not include this directory back).
#include "hven/drivers/interior_point_solver.h"

namespace hven::solvers {

// Forward declaration only: update_barrier takes a GlobalizationMechanism by
// reference so PROBE's predictor can drive the SAME fraction-to-boundary
// step-scaling as the main-path step. A reference parameter needs only the
// incomplete type here; the concrete implementation is visible at the
// definition site.
class GlobalizationMechanism;

/// @brief Computes the next barrier parameter mu and its contribution to the
/// objective/gradient — the free<->monotone barrier-update state machine
/// interface.
///
/// The interface defines no persistent state: mu is always passed in and
/// returned, never cached. A stateful governor holds its bookkeeping behind
/// reset().
class BarrierGovernor {
  public:
    virtual ~BarrierGovernor() = default;

    /// @brief Free-mode barrier update (Mehrotra PROBE predictor-corrector or
    /// LOQO), selected by @p barmode.
    ///
    /// Callout: PROBE is NOT a pure function of (mu_in, avgcomp, mincomp). Its
    /// predictor step needs a fresh KKT solve against the already-factored
    /// system and reuses the fraction-to-boundary scaling via @p mechanism to
    /// form the predictor point Temp before computing the new mu — hence this
    /// signature takes the live KKT solver and dims (via SolverContext), the
    /// mechanism, and explicit RHS/DXSL/Temp work vectors rather than being a
    /// free function of scalars. The predictor's alphap/alphad are computed
    /// into locals and deliberately discarded: on the classic path the
    /// main-path step overwrites them before they are recorded.
    ///
    /// @param avgcomp,mincomp Complementarity measures already computed this
    ///   iteration (once per iteration, before factorization — NOT recomputed here).
    /// @param mu_in Unused by the free-mode oracles themselves (they compute an
    ///   entirely new mu from avgcomp/mincomp, then clamp against
    ///   ctx.settings_.min_mu_/max_mu_); the load-bearing consumer is
    ///   update_barrier_monotone(), which seeds and gates off it directly.
    /// @param XSL Read by LOQO's mu rule and by the common tail (barrier
    ///   objective/dual-gradient at the resulting mu); raw blocks viewed via
    ///   KKTVector built from SolverContext's dims internally.
    /// @param current This iteration's in-progress IterateInfo whose residual
    ///   fields were just filled by the convergence check — passed explicitly
    ///   because it is NOT yet in the solver's iteration history at this point
    ///   in the loop. Monitored governors read these residuals to decide the
    ///   free<->monotone switch; free-mode oracles ignore it entirely.
    /// @param barr_obj Out: barrier objective set by the common tail.
    /// @param mu_event Out-signal (caller passes false): set true when a
    ///   governor's monotone mode begins a new barrier subproblem with a fresh
    ///   parameter — the acceptance strategy's per-subproblem reset trigger.
    ///   Free-mode oracles never set it, so the caller's reset branch stays
    ///   dead and the default path remains bit-identical.
    /// @return The new (already-clamped) mu.
    virtual double update_barrier(InteriorPointSolver::BarrierModes barmode, double mu_in,
                                  double avgcomp, double mincomp, Eigen::VectorXd &XSL,
                                  Eigen::VectorXd &RHS, Eigen::VectorXd &DXSL,
                                  Eigen::VectorXd &Temp, GlobalizationMechanism &mechanism,
                                  SolverContext &ctx, double &barr_obj, const IterateInfo &current,
                                  bool &mu_event) = 0;

    /// @brief Barrier update while a nested l1 feasibility-restoration phase is
    /// active, for governors WITHOUT their own monotone safeguard. Shared,
    /// NON-VIRTUAL: every governor uses it identically; the configured
    /// governor/barmode is bypassed for the phase duration and resumes at exit.
    ///
    /// Numerical callout: transcribes the safeguarded MONOTONE restoration
    /// schedule (Fiacco-McCormick ladder anchored at the entry parameter;
    /// advance iff barrier_subproblem_error <= tol_factor * mu, reading
    /// barr_inf_, into which elastic complementarity folds while nested-active)
    /// because a free oracle cannot drive this phase: it drives EVERY
    /// complementarity product toward its proposed mu — including the elastic
    /// bound pairs of the restoration subproblem, so any mu is self-consistent —
    /// then collapses mu to its floor within a few iterations, before the
    /// elastics shrink. The condensed elastic pivot explodes, constraint rows
    /// decouple, and the phase freezes on a wrong-basin minimizer until the
    /// iteration cap. The monotone gate keeps mu anchored until the elastics
    /// actually shrink.
    ///
    /// @param mu_in Current held monotone parameter carried across phase iterations.
    /// @param barr_obj Out: barrier objective written by the log-barrier tail at
    ///   the resulting mu (slacks/iq_lmults on XSL, dual gradient on RHS), as
    ///   the free-mode common tail does.
    /// @param mu_event Set true on a strict decrease (a new barrier subproblem
    ///   — the acceptance per-subproblem reset trigger).
    /// @return The possibly-advanced mu. Never reached off the nested path.
    double update_barrier_monotone(double mu_in, Eigen::VectorXd &XSL, Eigen::VectorXd &RHS,
                                   SolverContext &ctx, double &barr_obj, const IterateInfo &current,
                                   bool &mu_event);

    /// @brief Whether this governor supplies its OWN safeguarded (monotone)
    /// barrier schedule during a nested l1 restoration phase.
    ///
    /// Default false: the seam routes the in-phase update through
    /// update_barrier_monotone so the free-oracle failure mode above cannot
    /// occur. When true, the governor's own monitor forces the safeguarded
    /// decrease in-phase and the seam leaves it to drive its own update —
    /// overlaying a second differently-anchored monotone schedule would only
    /// perturb its established convergence. Scope note on the resulting
    /// guarantee: under a free governor the free oracle is unconditionally
    /// unreachable in-phase; under a monitored governor it is reached only
    /// inside the monitor's own guarded free mode, whose completeness depends
    /// on the oracle (LOQO consumes the elastic-augmented complementarity
    /// directly; PROBE does not).
    virtual bool provides_restoration_barrier_safeguard() const { return false; }

    /// @brief Free vs. monotone mode query; the default reports always-free.
    virtual bool in_monotone_mode() const { return false; }

    /// @brief mu-event / phase-change hook; stateful governors clear their
    /// monotone-mode bookkeeping here.
    virtual void reset() = 0;

    /// @brief Writes this governor's diagnostic state (if any) into result.
    /// Same write-only contract and last-phase-wins semantics as
    /// AcceptanceStrategy::append_diagnostics; the no-op default keeps the
    /// classic path bit-identical.
    virtual void append_diagnostics(InteriorPointSolver::SolveResult &result) const {
        (void)result;
    }
};

} // namespace hven::solvers
