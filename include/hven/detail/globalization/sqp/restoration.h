#pragma once

// restoration.h -- the restoration phase's l1 feasibility-problem model
// wrapper, carved verbatim out of drivers/sqp_driver.h (phase-C S3,
// restructure only). The comments below still speak from that header's point
// of view: the RESTORATION PHASE note ("the header note"), build_subproblem,
// and qp_failure_is_retryable remain in drivers/sqp_driver.h; the elastic
// tier they compare against is now detail/globalization/sqp/elastic.h's.
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
    RestorationModel(const NlpModel &model, const Vec &x_entry, const NlpEval &ev)
        : model_(model), nx_(model.n()), me_(model.me()), mi_(model.mi()) {
        if (x_entry.size() != nx_) {
            throw std::invalid_argument(
                fmt::format("RestorationModel: x_entry has size {}, expected {} (= model.n())",
                            x_entry.size(), nx_));
        }
        n_ = nx_ + 2 * me_ + mi_;

        sigma_e_ = Vec::Ones(me_);
        for (Index k = 0; k < me_; ++k) {
            double row_max = 0.0;
            for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(ev.Je, k); it;
                 ++it) {
                row_max = std::max(row_max, std::abs(it.value()));
            }
            sigma_e_(k) = std::max(1.0, row_max);
        }
        sigma_i_ = Vec::Ones(mi_);
        for (Index j = 0; j < mi_; ++j) {
            double row_max = 0.0;
            for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(ev.Ji, j); it;
                 ++it) {
                row_max = std::max(row_max, std::abs(it.value()));
            }
            sigma_i_(j) = std::max(1.0, row_max);
        }

        lower_ = Vec::Zero(n_);
        upper_ = Vec::Constant(n_, kRestorationSlackBound);
        lower_.head(nx_) = model.lower();
        upper_.head(nx_) = model.upper();

        // THE START POINT IS FEASIBLE FOR THIS WRAPPER BY CONSTRUCTION, with
        // objective exactly h(x_entry): put each row's whole violation into
        // the slack that absorbs it, in that slack's own units.
        start_ = Vec::Zero(n_);
        start_.head(nx_) = x_entry;
        for (Index k = 0; k < me_; ++k) {
            const double c = ev.ce(k);
            start_(nx_ + k) = std::max(0.0, -c) / sigma_e_(k);      // sp
            start_(nx_ + me_ + k) = std::max(0.0, c) / sigma_e_(k); // sm
        }
        for (Index j = 0; j < mi_; ++j) {
            start_(nx_ + 2 * me_ + j) = std::max(0.0, ev.ci(j)) / sigma_i_(j); // si
        }
    }

    Index n() const override { return n_; }
    Index me() const override { return me_; }
    Index mi() const override { return mi_; }

    // The original variables of an augmented point -- what the driver takes
    // back out of a restoration solve.
    Vec original_x(const Vec &y) const {
        return y.size() >= nx_ ? Vec(y.head(nx_)) : Vec::Zero(nx_);
    }
    const Vec &slack_scale_e() const { return sigma_e_; }
    const Vec &slack_scale_i() const { return sigma_i_; }

    double eval_f(const Vec &y) const override {
        double s = 0.0;
        for (Index k = 0; k < me_; ++k) {
            s += sigma_e_(k) * (y(nx_ + k) + y(nx_ + me_ + k));
        }
        for (Index j = 0; j < mi_; ++j) {
            s += sigma_i_(j) * y(nx_ + 2 * me_ + j);
        }
        return s;
    }

    // Constant: the objective is LINEAR, and has no x-block at all. That zero
    // x-block is what makes a stationary point of this problem a stationary
    // point of h rather than of some trade-off against f (header note).
    Vec eval_grad(const Vec &) const override {
        Vec g = Vec::Zero(n_);
        g.segment(nx_, me_) = sigma_e_;
        g.segment(nx_ + me_, me_) = sigma_e_;
        g.segment(nx_ + 2 * me_, mi_) = sigma_i_;
        return g;
    }

    Vec eval_ce(const Vec &y) const override {
        if (me_ == 0) {
            return Vec(0);
        }
        Vec c = model_.eval_ce(original_x(y));
        for (Index k = 0; k < me_; ++k) {
            c(k) += sigma_e_(k) * (y(nx_ + k) - y(nx_ + me_ + k));
        }
        return c;
    }

    Vec eval_ci(const Vec &y) const override {
        if (mi_ == 0) {
            return Vec(0);
        }
        Vec c = model_.eval_ci(original_x(y));
        for (Index j = 0; j < mi_; ++j) {
            c(j) -= sigma_i_(j) * y(nx_ + 2 * me_ + j);
        }
        return c;
    }

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
                      const Vec &lambda_i) const override {
        const SpMatRM inner = model_.eval_hess(original_x(y), 0.0, lambda_e, lambda_i);
        std::vector<Eigen::Triplet<double>> t;
        t.reserve(static_cast<std::size_t>(inner.nonZeros()));
        for (Index i = 0; i < nx_; ++i) {
            for (SpMatRM::InnerIterator it(inner, i); it; ++it) {
                t.emplace_back(it.row(), it.col(), it.value());
            }
        }
        SpMatRM H(n_, n_);
        H.setFromTriplets(t.begin(), t.end());
        H.makeCompressed();
        return H;
    }

    // [ Je(x) | diag(sigmaE) | -diag(sigmaE) | 0 ]
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &y) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> J(me_, n_);
        if (me_ == 0) {
            return J;
        }
        const Eigen::SparseMatrix<double, Eigen::RowMajor> Je = model_.eval_jac_e(original_x(y));
        std::vector<Eigen::Triplet<double>> t;
        t.reserve(static_cast<std::size_t>(Je.nonZeros() + 2 * me_));
        for (Index k = 0; k < me_; ++k) {
            for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(Je, k); it; ++it) {
                t.emplace_back(it.row(), it.col(), it.value());
            }
            t.emplace_back(k, nx_ + k, sigma_e_(k));
            t.emplace_back(k, nx_ + me_ + k, -sigma_e_(k));
        }
        J.setFromTriplets(t.begin(), t.end());
        J.makeCompressed();
        return J;
    }

    // [ Ji(x) | 0 | 0 | -diag(sigmaI) ]
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &y) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> J(mi_, n_);
        if (mi_ == 0) {
            return J;
        }
        const Eigen::SparseMatrix<double, Eigen::RowMajor> Ji = model_.eval_jac_i(original_x(y));
        std::vector<Eigen::Triplet<double>> t;
        t.reserve(static_cast<std::size_t>(Ji.nonZeros() + mi_));
        for (Index j = 0; j < mi_; ++j) {
            for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(Ji, j); it; ++it) {
                t.emplace_back(it.row(), it.col(), it.value());
            }
            t.emplace_back(j, nx_ + 2 * me_ + j, -sigma_i_(j));
        }
        J.setFromTriplets(t.begin(), t.end());
        J.makeCompressed();
        return J;
    }

    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return start_; }

  private:
    const NlpModel &model_;
    Index nx_ = 0, me_ = 0, mi_ = 0, n_ = 0;
    Vec sigma_e_, sigma_i_;
    Vec lower_, upper_, start_;
};

} // namespace hven::solvers
