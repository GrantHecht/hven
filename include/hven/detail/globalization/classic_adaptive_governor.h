// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <cassert>
#include <utility>

#include <Eigen/Core>

#include "hven/detail/globalization/barrier_governor.h"
#include "hven/detail/globalization/globalization_mechanism.h"
#include "hven/detail/globalization/solver_context.h"
#include "hven/detail/interior/kkt_vector.h"
#include "hven/drivers/interior_point_solver.h"

namespace hven::solvers {

/// @brief The classic free-mode PROBE/LOQO barrier-parameter update; always
/// reports in_monotone_mode() == false.
///
/// Stateless (BarrierGovernor's ownership rule): every quantity is an explicit
/// per-call parameter or reached through the SolverContext passed to the call.
class ClassicAdaptiveGovernor : public BarrierGovernor {
  public:
    ClassicAdaptiveGovernor() = default;

    /// @brief PROBE (Mehrotra predictor-corrector) / LOQO barrier update plus
    /// the common clamp/objective/dual-gradient tail.
    ///
    /// Callout — the PROBE update is NOT a pure function of its scalars: it
    /// runs a predictor KKT solve into DXSL, scales that predictor DXSL to the
    /// fraction-to-boundary via @p mechanism (the SAME entry point the
    /// main-path step uses), forms Temp = XSL + DXSL, and computes mpc_mu on
    /// it. All predictor state is consumed inside the method: DXSL is
    /// overwritten by the REAL step solve that runs immediately after this
    /// returns, and Temp is solver scratch the line search re-initialises. The
    /// only quantities that escape are the returned clamped mu, the barr_obj
    /// out-param, and the corrector dual gradient written into RHS's dual_grad
    /// block.
    ///
    /// Divergence-path note: the predictor's fraction-to-boundary step writes
    /// alphap/alphad into discarded locals (this interface surfaces no alpha
    /// out-params). On a non-finite PROBE step the recorded per-iterate
    /// alpha_p_/alpha_d_ diagnostics therefore keep their 1.0 loop-init values;
    /// print-only exposure on diverging runs — iterates, mu, and iteration
    /// counts are unaffected.
    ///
    /// Free-mode only: current is ignored and mu_event is never written (no
    /// monotone mode), so the caller's mu-event reset branch stays dead on the
    /// classic path.
    double update_barrier(InteriorPointSolver::BarrierModes barmode, double mu_in, double avgcomp,
                          double mincomp, Eigen::VectorXd &XSL, Eigen::VectorXd &RHS,
                          Eigen::VectorXd &DXSL, Eigen::VectorXd &Temp,
                          GlobalizationMechanism &mechanism, SolverContext &ctx, double &barr_obj,
                          const IterateInfo &current, bool &mu_event) override;

    /// mu-event / phase-change hook — no-op: the free-mode oracles carry no
    /// persistent state across iterations.
    void reset() override {}

  private:
    /// @brief Creates a KKTVector view over a VectorXd using the context's
    /// dimensions; shared view type, so the barrier block keeps its
    /// named-segment accessors unchanged.
    static KKTVector kkt_view(Eigen::VectorXd &v, const SolverContext &ctx) {
        return KKTVector(v, ctx.primal_vars_, ctx.slack_vars_, ctx.equal_cons_, ctx.inequal_cons_);
    }

    /// TOKEN-IDENTICAL copy of the solver's complementarity INCLUDING the ULP
    /// warning: the .sum() reduction order feeds mu (via mpc_mu) and must not
    /// be reordered; unifying it with other copies needs its own evidence.
    /// Uses ctx.stli_scratch_ (the same solver-owned buffer) and reaches the
    /// bound set/multipliers through ctx. X is the primal block the bound
    /// pairs need.
    void complementarity(Eigen::Ref<Eigen::VectorXd> X, Eigen::Ref<Eigen::VectorXd> S,
                         Eigen::Ref<Eigen::VectorXd> LI, double &avgcomp, double &mincomp,
                         double &maxcomp, const SolverContext &ctx) const;

    /// Barrier kernels — forwarders into the shared inline kernels in
    /// barrier_math.h (the two-argument corrector gradient has no shared
    /// counterpart and stays local).
    double barrier_objective(Eigen::Ref<Eigen::VectorXd> S, double mu,
                             const SolverContext &ctx) const;
    void barrier_gradient(Eigen::Ref<Eigen::VectorXd> S, Eigen::Ref<Eigen::VectorXd> LI, double mu,
                          Eigen::Ref<Eigen::VectorXd> AGS) const;
    void barrier_gradient(Eigen::Ref<Eigen::VectorXd> LI, Eigen::Ref<Eigen::VectorXd> AGS) const;

    /// The LOQO oracle is a pure function of the complementarity aggregates.
    double loqo_mu(double avgcomp, double mincomp) const;

    /// The Mehrotra oracle re-runs complementarity on the predictor point (X is
    /// the predictor's primal block, so the ratio's numerator covers the same
    /// pair set as its denominator when variable bounds are present); the
    /// reduction feeds mu — hence the ULP note above.
    double mpc_mu(Eigen::Ref<Eigen::VectorXd> X, Eigen::Ref<Eigen::VectorXd> S,
                  Eigen::Ref<Eigen::VectorXd> LI, double avgcomp, double mincomp,
                  const SolverContext &ctx) const;
};

} // namespace hven::solvers
