// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// restoration.h -- the restoration phase's l1 feasibility-problem model
// wrapper. The RESTORATION PHASE note ("the header note"), build_subproblem,
// and qp_failure_is_retryable live in drivers/sqp_solver.h; the elastic tier
// compared against is detail/globalization/sqp/elastic.h's. Every member
// function of RestorationModel declared here is defined in
// src/globalization/sqp/soc_elastic_restoration.cpp (with soc.h's and
// elastic.h's definitions). One consequence worth stating where the class is:
// `n()` is the class's KEY FUNCTION, so the vtable/typeinfo are emitted in that
// TU alone rather than weakly in every TU touching the class.
//
// NOT SELF-CONTAINED BY DESIGN: `NlpEval` is defined in drivers/sqp_solver.h,
// which includes this header after NlpEval's definition and before the driver
// class, so no header cycle exists (this header never includes sqp_solver.h).
// Any other includer must have NlpEval complete first.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include <Eigen/SparseCore>
#include <fmt/format.h>

#include <hven/core/types.h>
#include <hven/model/nlp_model.h>

namespace hven::solvers {

/// The fraction of tr_init the trust region restarts at when the main loop
/// resumes from restoration: one order of magnitude is the conservative reading
/// of "start again", and the growth rule earns it back (see sqp_solver.h's
/// WHAT RESUMING DOES).
inline constexpr double kRestoreRadiusFactor = 0.1;

/// "Effectively infinite" upper bound on a slack, matching qp_problem.h's own
/// stated convention (+/-1e20) rather than a true +inf, so every bound
/// arithmetic in the engine and in build_subproblem (upper - x) stays in
/// ordinary floating point.
inline constexpr double kRestorationSlackBound = 1e20;

/// @brief The l1 FEASIBILITY PROBLEM of the restoration phase, as an NlpModel
/// wrapping the caller's own model:
///
///     min   sum_i sigmaE_i (sp_i + sm_i) + sum_j sigmaI_j si_j
///     s.t.  cE(x) + sigmaE.*(sp - sm)  = 0
///           cI(x) - sigmaI.*si        <= 0
///           l <= x <= u,   sp, sm, si >= 0
///
/// in y = (x, sp, sm, si), with n + 2*me + mi variables and the SAME me and mi
/// (the slacks add COLUMNS, never rows — the same shape choice as the elastic
/// tier). Minimizing this minimizes h(x) = ||cE(x)||_1 +
/// sum_j max(0, cI_j(x)) EXACTLY (sqp_solver.h's EXACTNESS paragraph is the
/// argument this class is built to satisfy).
///
/// THE SIGMA SCALING: sigmaE_i = max(1, ||grad cE_i(x_entry)||inf), sigmaI_j
/// likewise, computed ONCE at construction (the wrapper's variables must have
/// constant units) from the Jacobians already in hand at entry — scale the
/// constructed COLUMN to the row it joins, which is why the slack columns are
/// +/-sigma rather than +/-1. Because the objective coefficient carries the
/// SAME sigma, the objective is still the actual violation and the multiplier
/// certificate is unchanged.
///
/// max(1, .) rather than the row norm itself: a column must never be scaled UP,
/// or a small Jacobian row would shrink the column entry and inflate the slack
/// variable, trading this conditioning problem for its mirror image.
///
/// THE PROXIMITY TERM (M7 W2 M2b), PRESENT ONLY AT zeta > 0. With
/// `SqpOptions::restoration_proximity = zeta > 0` the objective above gains
///
///     + (zeta/2) * ||D (x - x_k)||^2,   D = diag(d_i), d_i = min(1, 1/|x_{k,i}|)
///
/// over the ORIGINAL x-block only (every slack's coefficient is 0), with `x_k`
/// the REQUESTING ITERATE handed in beside `x_entry`. `D` is computed ONCE at
/// construction from data already in hand -- O(n), no model query -- and the
/// term is the interior-point lane's, transcribed: see
/// detail/globalization/l1_restoration.h, whose nested-l1 restoration carries
/// `rho*sum(n+p) + (eta(mu)/2)*|D_R (x - x_R)|^2`. What it buys is the one
/// piece of information the feasibility problem is missing -- that `x` should
/// stay near the point that asked for the phase.
///
/// WHY `x_k` AND NOT THE WRAPPER'S OWN CONSTRUCTION POINT. The phase's seed
/// guard may build the wrapper at an offered candidate (`x_entry != x_k`), and
/// the excursion this term prices is the CALLER'S iterate's -- the same point
/// the S5 excursion record's entry half is taken at (core/solver_counters.h).
/// (The certificate's residual is a different reading: it is evaluated at the
/// RESTORED point `x_r`, and `x_k` enters it only through this displacement.)
/// Centring on the construction point instead would make the term measure a
/// displacement
/// from a point the caller never asked about. The two coincide whenever the
/// seed guard does not fire, which is every cell measured in
/// docs/notes/data/2026-09-m7-w2-m2-restoration/. ONE CONSEQUENCE, STATED HERE
/// BECAUSE IT IS A REAL ONE: at zeta > 0 AND a fired seed guard the wrapper's
/// objective at its own start point is h(x_entry) + P(x_entry) rather than
/// h(x_entry) -- the exactness of the l1 reformulation is untouched (the term is
/// part of the objective being minimized, not of the violation being measured),
/// but the "objective at the start point IS h" identity holds only at zeta = 0
/// or an unfired guard.
///
/// AT zeta == 0 THE TERM IS BRANCHED OUT, NOT ADDED AS ZERO, in all three of
/// `eval_f`, `eval_grad` and `eval_hess`. A zero triplet would still enter the
/// Hessian's pattern and move the sub-solve's symbolic analysis, so the OFF
/// state is a CODE SHAPE and not merely an arithmetic identity -- that is what
/// makes `SqpOptions::restoration_proximity = 0` the wrapper that shipped
/// before the field existed.
///
/// LIFETIME: holds a REFERENCE to the wrapped model, which must outlive it; the
/// driver constructs one on the stack for the duration of one restoration.
class RestorationModel final : public NlpModel {
  public:
    /// @param ev Must be eval_nlp(model, x_entry) -- the entry point's
    /// evaluation, already in hand, from which both the sigmas and the start
    /// point's slacks are read without a single extra model call. The start's
    /// x-block is x_entry CLAMPED into the model's box (M7 W2 ladder, L2); only
    /// when that moves a component are the rows re-evaluated, once, at the
    /// clamped point (`start_clamps()` counts the moved components).
    ///
    /// THE zeta = 0 WRAPPER, i.e. the one that ships: it delegates to the
    /// constructor below with the centre set to `x_entry`, which that overload
    /// then never reads.
    RestorationModel(const NlpModel &model, const Vec &x_entry, const NlpEval &ev);

    /// @param x_centre The REQUESTING ITERATE `x_k` the proximity term is
    /// centred on (class note); read only at `proximity > 0`, and must have
    /// model.n() entries.
    /// @param proximity `zeta >= 0`; 0 is OFF and is the shipped value.
    RestorationModel(const NlpModel &model, const Vec &x_entry, const NlpEval &ev,
                     const Vec &x_centre, double proximity);

    Index n() const override;
    Index me() const override;
    Index mi() const override;

    /// The original variables of an augmented point -- what the driver takes
    /// back out of a restoration solve.
    Vec original_x(const Vec &y) const;
    const Vec &slack_scale_e() const;
    const Vec &slack_scale_i() const;

    /// The proximity weight `zeta` this wrapper was built with; 0 == OFF.
    double proximity() const;

    /// Components of `x_entry` strictly outside the box, clamped into it for
    /// the start point. 0 on an in-box entry, and 0 when the rows are not finite
    /// at the clamped point, where the start stays the entry (the centre, at
    /// zeta > 0, is clamped regardless).
    Index start_clamps() const;

    double eval_f(const Vec &y) const override;

    /// CONSTANT gradient AT zeta == 0, and that is the shipped state: the
    /// objective is then LINEAR with no x-block at all, and that zero x-block is
    /// what makes a stationary point of this problem a stationary point of h
    /// rather than of some trade-off against f. At zeta > 0 the x-block is
    /// `zeta D^2 (x - x_k)` and the stationarity claim changes with it -- what a
    /// stationary point of the wrapper is then stationary for is `h + P`, which
    /// is why the driver computes `||J^T lambda - z_sub||_inf` EXPLICITLY before
    /// it certifies anything, rather than reading the claim off the sub-solve's
    /// own kOptimal (SqpResult::infeasibility_certified). The argument is named
    /// at both weights because the vector depends on it at one of them.
    Vec eval_grad(const Vec &y) const override;

    Vec eval_ce(const Vec &y) const override;

    Vec eval_ci(const Vec &y) const override;

    /// obj_scale IS DELIBERATELY IGNORED AT zeta == 0, and that is exact rather
    /// than a shortcut: hess = obj_scale*hess(f) + sum lambda*hess(c), and
    /// hess(f) is IDENTICALLY ZERO for a linear objective. What remains is the
    /// wrapped model's own constraint Hessian -- model_.eval_hess(x, 0.0, .),
    /// the SAME call with the objective switched off. The slack block
    /// contributes nothing (slacks enter linearly), so the augmented Hessian is
    /// the original's entries in an n_-square frame with no new nonzeros -- the
    /// elastic tier's H-extension, satisfying QpProblem::validate's upper-
    /// triangle rule for the same reason (no entry moves).
    ///
    /// AT zeta > 0 BOTH HALVES OF THAT PARAGRAPH CHANGE (M7 W2 M2b). `hess(f)`
    /// is `zeta D^2`, a positive diagonal on the x-block, so `obj_scale` is
    /// named and APPLIED to it -- nlp_model.h's contract is
    /// `obj_scale*hess(f) + sum lambda*hess(c)`, and this term is part of
    /// `f` -- and the augmented Hessian gains up to `nx_` diagonal entries,
    /// which is a change to the sub-solve's SPARSITY PATTERN and hence to its
    /// symbolic analysis. Those entries are emitted UNCONDITIONALLY at
    /// zeta > 0, for every original coordinate, whatever `obj_scale` or the
    /// multipliers are: nlp_model.h's STRUCTURAL PATTERN INVARIANCE precondition
    /// forbids a pattern that depends on the arguments a model is called with.
    SpMatRM eval_hess(const Vec &y, double obj_scale, const Vec &lambda_e,
                      const Vec &lambda_i) const override;

    /// [ Je(x) | diag(sigmaE) | -diag(sigmaE) | 0 ]
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &y) const override;

    /// [ Ji(x) | 0 | 0 | -diag(sigmaI) ]
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &y) const override;

    const Vec &lower() const override;
    const Vec &upper() const override;
    Vec start_point() const override;

  private:
    const NlpModel &model_;
    Index nx_ = 0, me_ = 0, mi_ = 0, n_ = 0;
    Vec sigma_e_, sigma_i_;
    Vec lower_, upper_, start_;
    Index start_clamps_ = 0;
    /// THE PROXIMITY TERM'S THREE PIECES, all empty/zero at the shipped OFF.
    /// `d_` is the d_i of the class note, squared nowhere: the squares are taken
    /// where they are used, so the one stored vector is the one the class note
    /// names. `centre_` is a COPY of `x_k`, not a reference: the driver's own
    /// `st.x` is moved from on the resume arm while this wrapper is still alive.
    double zeta_ = 0.0;
    Vec centre_, d_;
};

/// @brief The infeasibility certificate's residual at a restored point,
/// `||Je^T lambda_e + Ji^T lambda_i - z||_inf`.
///
/// A subgradient of `h`, not of the Lagrangian: there is NO `grad f` term --
/// that absence is what makes it a certificate of infeasibility -- and `z` is
/// the restoration sub-problem's own bound price, UNCORRECTED. Subtracting
/// `grad P` from it would make this identically zero at every
/// wrapper-stationary point, i.e. certify everything.
///
/// `ev` must be in the units `lambda_e`, `lambda_i` and `z` price. Cost:
/// O(nnz(J)) on a bundle the caller already holds -- no model query, so no
/// eval counter moves.
double subgradient_certificate_residual_inf(const NlpEval &ev, const Vec &lambda_e,
                                            const Vec &lambda_i, const Vec &z);

/// @brief The certificate's GATE at a kOptimal feasibility sub-solve: may that
/// sub-solve's verdict be read as a certificate of infeasibility for `h`?
///
/// AT `proximity == 0` THE ANSWER IS YES WITHOUT READING ANYTHING, and that is
/// the point of this function's shape rather than an optimization. The
/// wrapper's objective is then the violation alone, so the sub-solve's own KKT
/// test -- taken in the raw frame it ran in -- IS
/// `||J^T lambda - z||_inf <= kkt_tol`. Re-deriving that number here would put
/// a scale round-trip and a fresh summation order between the shipped default
/// and its answer, and at the tolerance boundary two roundings of one quantity
/// need not agree. `make_caller_scale_bundle` is therefore NOT CALLED at the
/// shipped weight; tests/sqp/test_sqp_restoration.cpp pins that by handing this
/// a bundle it must not touch.
///
/// AT `proximity > 0` (EXPERIMENTAL) the sub-solve certified stationarity of
/// `h + P`, which is not the claim, so the residual is recomputed from the
/// bundle the callable returns. That bundle is the engine-scale one mapped back
/// to the caller's frame, which agrees with the raw frame TO ROUNDING and not
/// bit for bit; a raw re-evaluation is deliberately not taken, because it would
/// move eval counters on an exit path for an ulp-level gain on an option that
/// carries no calibrated value.
template <typename MakeCallerScaleBundle>
bool feasibility_subsolve_certifies(double proximity, double kkt_tol, const Vec &lambda_e,
                                    const Vec &lambda_i, const Vec &z,
                                    MakeCallerScaleBundle &&make_caller_scale_bundle) {
    if (proximity == 0.0) {
        return true;
    }
    const NlpEval ev = make_caller_scale_bundle();
    return subgradient_certificate_residual_inf(ev, lambda_e, lambda_i, z) <= kkt_tol;
}

} // namespace hven::solvers
