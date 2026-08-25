// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <cassert>
#include <utility>
#include <vector>

#include <Eigen/Core>

#include "hven/detail/globalization/acceptance_strategy.h"
#include "hven/detail/globalization/progress_measures.h"
#include "hven/detail/globalization/solver_context.h"
#include "hven/detail/interior/iterate_info.h"
#include "hven/detail/interior/kkt_vector.h"
#include "hven/drivers/interior_point_solver.h"

namespace hven::solvers {

/// @brief The classic backtracking merit line search (the one fused-path
/// strategy): LANG / L1 / augmented-Lagrangian merit variants behind a single
/// dispatcher.
///
/// Lifetime callout: holds its SolverContext BY VALUE. That cannot dangle —
/// the solver owns this strategy (and therefore outlives it), every context
/// member refers to a stable solver member, and the context is rebuilt at the
/// start of every solve invocation, so the captured NLP raw pointer never goes
/// stale.
class ClassicMeritAcceptance : public AcceptanceStrategy {
  public:
    explicit ClassicMeritAcceptance(const SolverContext &ctx) : ctx_(ctx) {}

    /// The classic strategy is the one (and only) fused-path driver.
    bool drives_classic_path() const override { return true; }

    /// @brief Never driven on the classic path: the classic acceptance test is
    /// fused inside classic_line_search's backtracking loop. Reaching this is
    /// a wiring bug, so it throws rather than fabricate an answer.
    /// @throws std::logic_error Always.
    bool is_iterate_acceptable(const ProgressMeasures &current, const ProgressMeasures &trial,
                               const ProgressMeasures &predicted_reduction,
                               double objective_multiplier, double step_length) override;

    /// @brief Restoration-exit test, driven by the solver while the classic
    /// strategy runs in feasibility mode: relative infeasibility reduction,
    /// shaped after Ipopt's IpRestoConvCheck with a single-tolerance floor.
    bool is_infeasibility_sufficiently_reduced(const ProgressMeasures &reference,
                                               const ProgressMeasures &trial) const override;

    /// mu-event / phase-change hook — no-op: the classic merit test carries no
    /// persistent state across iterations.
    void reset() override {}

    /// Classic fused entry point: the dispatcher over the three merit variants.
    double classic_line_search(InteriorPointSolver::LineSearchModes lsmode, double obj_scale,
                               double mu, double prim_obj, double barr_obj, Eigen::VectorXd &XSL,
                               Eigen::VectorXd &DXSL, Eigen::VectorXd &XSL2, Eigen::VectorXd &RHS,
                               Eigen::VectorXd &RHS2, IterateInfo &Citer,
                               const std::vector<IterateInfo> &iters) override;

  private:
    SolverContext ctx_;

    /// @brief Creates a KKTVector view over a VectorXd using the context's
    /// dimensions; the view type is shared with the solver and the sibling
    /// components, so the merit bodies keep their named-segment accessors
    /// unchanged.
    KKTVector kkt_view(Eigen::VectorXd &v) {
        return KKTVector(v, ctx_.primal_vars_, ctx_.slack_vars_, ctx_.equal_cons_,
                         ctx_.inequal_cons_);
    }

    /// Merit penalty triple (l1/l2/l-inf), used only by the merit bodies below.
    struct PenaltyTerms {
        double l1_, l2_, linf_;
    };

    /// The three merit variants behind the classic dispatcher.
    double ls_lang(double obj_scale, double mu, double prim_obj, double barr_obj, KKTVector &xsl,
                   KKTVector &dxsl, KKTVector &xsl2, KKTVector &rhs, KKTVector &rhs2,
                   IterateInfo &citer);
    double ls_l1(double obj_scale, double mu, double prim_obj, double barr_obj, KKTVector &xsl,
                 KKTVector &dxsl, KKTVector &xsl2, KKTVector &rhs, KKTVector &rhs2,
                 IterateInfo &citer);
    double ls_auglang(double obj_scale, double mu, double prim_obj, double barr_obj, KKTVector &xsl,
                      KKTVector &dxsl, KKTVector &xsl2, KKTVector &rhs, KKTVector &rhs2,
                      IterateInfo &citer);

    /// Trial-point evaluation for the merit tests.
    void eval_trial_point_occ(double obj_scale, double mu, double alpha, KKTVector &xsl,
                              KKTVector &dxsl, KKTVector &xsl2, KKTVector &rhs2, double &ptest,
                              double &btest);

    /// Penalty triple for the current iterate / trial point.
    PenaltyTerms compute_penalties(KKTVector &xsl, KKTVector &rhs) const;

    /// Secondary (safeguard) acceptance test applied after the primary merit test.
    bool secondary_accept(double ptest, double prim_obj, const PenaltyTerms &test,
                          const PenaltyTerms &init) const;

    /// Barrier/eval helpers: apply_reset_slacks / barrier_objective /
    /// barrier_gradient forward to the shared inline kernels in barrier_math.h;
    /// eval_rhs stays a local body (it issues the first-order right-hand-side
    /// request through the aggregate contract and has no shared counterpart).
    void eval_rhs(double obj_scale, const Eigen::Ref<const Eigen::VectorXd> &XSL, double &val,
                  Eigen::Ref<Eigen::VectorXd> GX, Eigen::Ref<Eigen::VectorXd> AGXS_FX);
    void apply_reset_slacks(Eigen::Ref<Eigen::VectorXd> S, Eigen::Ref<Eigen::VectorXd> FXI) const;
    double barrier_objective(Eigen::Ref<Eigen::VectorXd> S, double mu) const;
    void barrier_gradient(Eigen::Ref<Eigen::VectorXd> S, Eigen::Ref<Eigen::VectorXd> LI, double mu,
                          Eigen::Ref<Eigen::VectorXd> AGS) const;

    /// Nested-restoration trial-path scratch (dead unless a nested restoration
    /// strategy is active): backs the elastic residual shift added to a
    /// trial's raw constraint residuals so the merit sees the restoration
    /// subproblem's infeasibility, without per-backtrack heap allocation.
    /// mutable so the const-facing trial helpers may fill them.
    mutable Eigen::VectorXd resto_eq_shift_scratch_;
    mutable Eigen::VectorXd resto_iq_shift_scratch_;
};

} // namespace hven::solvers
