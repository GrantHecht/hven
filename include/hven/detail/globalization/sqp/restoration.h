#pragma once

// restoration.h -- the restoration phase's l1 feasibility-problem model
// wrapper, carved verbatim out of drivers/sqp_driver.h (phase-C S3,
// restructure only). The comments below still speak from that header's point
// of view: the RESTORATION PHASE note ("the header note"), build_subproblem,
// and qp_failure_is_retryable remain in drivers/sqp_driver.h; the elastic
// tier they compare against is now detail/globalization/sqp/elastic.h's.
//
// WHERE THE DEFINITIONS LIVE (M3 phase-C T6): every member function of
// RestorationModel declared below is defined in
// src/globalization/sqp/soc_elastic_restoration.cpp, together with soc.h's and
// elastic.h's. That file's banner carries the measurement the carve rests on.
// One consequence is worth stating here, where the class is: `n()` is now the
// class's KEY FUNCTION, so the vtable, typeinfo and typeinfo-name are emitted
// in that TU alone rather than weakly in every TU that touches the class. This
// header keeps every declaration, the layout, and every word of the derivation.
//
// NOT SELF-CONTAINED BY DESIGN: `NlpEval` is defined in drivers/sqp_driver.h,
// which includes this header at the exact point the carved code stood -- after
// NlpEval's definition, before the driver class -- so the original definition
// order is preserved without a header cycle (this header never includes
// sqp_driver.h). Any other includer must have NlpEval complete first.

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

// =============================================================================
// THE RESTORATION PHASE (Task 9). See this header's RESTORATION PHASE note for
// the algorithm and every design choice; what follows is the MODEL WRAPPER the
// phase minimizes, factored out as a public class so it can be tested away
// from the driver's loop (the precedent set by qp_failure_is_retryable,
// build_soc_subproblem and build_elastic_subproblem) -- and, in particular, so
// that tests/sqp/support/derivative_check.h's finite-difference guards can be
// pointed at it like at any other NlpModel, which is the transcription check
// for the four derivative blocks below.
// =============================================================================

// The fraction of tr_init the trust region restarts at when the main loop
// resumes from restoration. See the RESTORATION PHASE note's WHAT RESUMING
// DOES for why a fraction and not either extreme; one order of magnitude is
// the conservative reading of "start again", and the growth rule earns it back.
inline constexpr double kRestoreRadiusFactor = 0.1;

// "Effectively infinite" upper bound on a slack, matching qp_problem.h's own
// stated convention (+/-1e20) rather than a true +inf, so that every bound
// arithmetic in the engine and in build_subproblem (upper - x) stays in
// ordinary floating point.
inline constexpr double kRestorationSlackBound = 1e20;

// The l1 FEASIBILITY PROBLEM of the restoration phase, as an NlpModel wrapping
// the caller's own model:
//
//     min   sum_i sigmaE_i (sp_i + sm_i) + sum_j sigmaI_j si_j
//     s.t.  cE(x) + sigmaE.*(sp - sm)  = 0
//           cI(x) - sigmaI.*si        <= 0
//           l <= x <= u,   sp, sm, si >= 0
//
// in y = (x, sp, sm, si), with n + 2*me + mi variables and the SAME me and mi
// (the slacks add COLUMNS, never rows -- the same shape choice the elastic
// tier makes, for the same reason). Minimizing it minimizes
// h(x) = ||cE(x)||_1 + sum_j max(0, cI_j(x)) EXACTLY: see the header note's
// EXACTNESS paragraph, which is the argument this class is built to satisfy.
//
// THE SIGMA SCALING. sigmaE_i = max(1, ||grad cE_i(x_entry)||inf) and
// sigmaI_j likewise, computed ONCE at construction (the wrapper's variables
// must have constant units) from the Jacobians already in hand at the entry
// point. This is Task 8's carry applied here -- scale the constructed COLUMN
// to the row it joins -- and it is why the slack columns are +/-sigma rather
// than +/-1. Because the objective coefficient carries the SAME sigma, the
// objective is still the actual violation and the multiplier certificate is
// unchanged; see the header note.
//
// max(1, .) rather than the row norm itself, for the elastic tier's reason
// verbatim: a column must never be scaled UP, or a small Jacobian row would
// shrink the column entry and inflate the slack variable, trading this
// conditioning problem for its mirror image.
//
// LIFETIME: holds a REFERENCE to the wrapped model, which must outlive it.
// The driver constructs one on the stack for the duration of one restoration.
class RestorationModel final : public NlpModel {
  public:
    // `ev` must be eval_nlp(model, x_entry) -- the entry point's evaluation,
    // already in the driver's hand, from which the sigmas and the start
    // point's slacks are both read without a single extra model call.
    RestorationModel(const NlpModel &model, const Vec &x_entry, const NlpEval &ev);

    Index n() const override;
    Index me() const override;
    Index mi() const override;

    // The original variables of an augmented point -- what the driver takes
    // back out of a restoration solve.
    Vec original_x(const Vec &y) const;
    const Vec &slack_scale_e() const;
    const Vec &slack_scale_i() const;

    double eval_f(const Vec &y) const override;

    // Constant: the objective is LINEAR, and has no x-block at all. That zero
    // x-block is what makes a stationary point of this problem a stationary
    // point of h rather than of some trade-off against f (header note).
    Vec eval_grad(const Vec &) const override;

    Vec eval_ce(const Vec &y) const override;

    Vec eval_ci(const Vec &y) const override;

    // obj_scale IS DELIBERATELY IGNORED HERE, and that is exact rather than a
    // shortcut: nlp_model.h defines this as obj_scale*hess(f) + sum lambda *
    // hess(c), and hess(f) is IDENTICALLY ZERO for a linear objective. What
    // remains is the wrapped model's own constraint Hessian -- which is
    // precisely model_.eval_hess(x, 0.0, .), the SAME call with the objective
    // switched off. The slack block contributes nothing (the slacks enter
    // every constraint linearly), so the augmented Hessian is the original's
    // entries in an n_-square frame with no new nonzeros -- the elastic tier's
    // H-extension, verbatim, and it satisfies QpProblem::validate's
    // upper-triangle rule for the same reason (no entry moves).
    SpMatRM eval_hess(const Vec &y, double, const Vec &lambda_e,
                      const Vec &lambda_i) const override;

    // [ Je(x) | diag(sigmaE) | -diag(sigmaE) | 0 ]
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &y) const override;

    // [ Ji(x) | 0 | 0 | -diag(sigmaI) ]
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &y) const override;

    const Vec &lower() const override;
    const Vec &upper() const override;
    Vec start_point() const override;

  private:
    const NlpModel &model_;
    Index nx_ = 0, me_ = 0, mi_ = 0, n_ = 0;
    Vec sigma_e_, sigma_i_;
    Vec lower_, upper_, start_;
};

} // namespace hven::solvers
