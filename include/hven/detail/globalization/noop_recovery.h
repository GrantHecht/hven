// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include "hven/detail/globalization/recovery_chain.h"

namespace hven::solvers {

/// @brief The empty recovery chain: unconditionally accepts the rejected step
/// as-is — exactly the classic give-up behavior with no recovery chain at all.
///
/// Wiring this link is purely structural: it never produces a retry/switch/
/// give-up action, so no dispatch branch downstream of the rejection hook is
/// reachable and the default solve path stays bit-identical to pre-recovery
/// behavior. The real dispatchers (SOC, extended backtracking, watchdog,
/// feasibility switch) ship as sibling links composed by the component rebuild
/// whenever their Settings fields opt in; the inertia/perturbation ladder is a
/// SEPARATE mechanism that lives inside the factorization and is deliberately
/// not part of this chain.
///
/// Stateless, per RecoveryChain's ownership rule.
class NoopRecovery : public RecoveryChain {
  public:
    NoopRecovery() = default;

    /// Pure no-op: ignores every argument (iterate info, history, context, the
    /// entire threaded working set) and accepts the step as-is.
    Action on_step_rejected(IterateInfo & /*Citer*/, const std::vector<IterateInfo> & /*iters*/,
                            SolverContext & /*ctx*/, AcceptanceStrategy & /*acceptance*/,
                            GlobalizationMechanism & /*mechanism*/,
                            InteriorPointSolver::LineSearchModes /*lsmode*/, double /*obj_scale*/,
                            double /*mu*/, double /*prim_obj*/, double /*barr_obj*/,
                            Eigen::VectorXd & /*XSL*/, Eigen::VectorXd & /*DXSL*/,
                            Eigen::VectorXd & /*XSL2*/, Eigen::VectorXd & /*RHS*/,
                            Eigen::VectorXd & /*RHS2*/, double & /*alpha*/, double & /*alphap*/,
                            double & /*alphad*/, int & /*soc_steps*/, int & /*resolved_depth*/,
                            int & /*watchdog_activations*/) override {
        return Action::kAcceptAsIs;
    }

    void reset() override {
        // No-op: NoopRecovery holds no state to reset.
    }
};

} // namespace hven::solvers
