// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// M7 W0 T3 STEP 2a -- THE ELASTIC-RUNG VEHICLE.
//
// WHY THIS EXISTS. The elastic rung (the certified fallback's rung A, and the
// walk route's own kInfeasible consumer) is not reached by ANY natural F7
// population in this repository: the M6 W2 T7 reachability sweep swept 1200 F7
// cells -- N in {10..400} x 15 window fractions x all five taxonomies x three
// arms, through the corpus's own generators -- and reports
// `elastic_activations == 0` and `elastic_from_ipqp_escape == 0` on EVERY row
// (docs/notes/data/2026-09-m6-w2-t7-corpus-sweep/reachability-sweep.csv). The
// structural reason is in that sweep's own header: F7's equality block is the
// collocation chain, whose Jacobian has full row rank at every point of the
// box, so no linearization the family can produce is inconsistent.
//
// The only cells in the tree that DO fire the rung are three HS problems --
// embargoed by M7's TRAIN rule until W1 H2 splits them -- and four constructed
// W2 fixtures, the largest of which is n = 2, me = 2. A tiny QP is exactly
// where the walk's hot chain wins, so nothing here could either confirm or
// refute the owner's robustness question ("does the walk struggle on a LARGE
// elastic QP?"). This header is the vehicle that question needs.
//
// WHAT IT IS. `F7CollocationChain` (scale_problems.h) with ONE contradictory
// equality pair appended -- the mechanism `W2T7ScaledInconsistentEqualitiesModel`
// (tests/sqp/test_sqp_solver.cpp) uses at n = 2, carried onto F7's shape at
// N = 100/200/400/800. The two extra rows are
//
//     s * (a^T x - b) = 0        and        s * (a^T x + b) = 0
//
// over the LAST node's control block (`a^T x = u_{N-1,0} + ... + u_{N-1,nc-1}`),
// with `b > 0`. Their Jacobian rows are IDENTICAL and CONSTANT in x, and their
// residuals differ by `2 s b` at every point, so the LINEARIZED equality system
// is inconsistent at every iterate -- there is no `p` with `Ae p = -cE`. That is
// the QP-level infeasibility the IPQP tier's section 6.3 detector is supposed to
// suspect, and the suspicion is what routes the major to the certified fallback
// whose rung A is the elastic ladder.
//
// WHY THE LAST NODE'S CONTROLS. They carry a box (|u| <= kControlBound) and no
// other equality, so each of the two rows is individually SATISFIABLE within the
// box for `b <= nc * kControlBound` -- the pair is contradictory without either
// row being separately unreachable, and node 0's state block (which the initial-
// condition rows already pin) is left alone, so the inconsistency is not
// confounded with a rank deficiency against an existing row.
//
// THE NLP IS GLOBALLY INFEASIBLE BY CONSTRUCTION, and that is the point: the
// solve ends kInfeasible after the ladder exhausts and restoration runs. The
// vehicle measures what rung A COSTS at each size, not whether a solution
// exists.
//
// NOT A CORPUS CELL. Deliberately: no bench cell, no CSV column, no schema
// move -- a test-support fixture beside the F7 support it is built from.

#include <cmath>
#include <stdexcept>
#include <vector>

#include <Eigen/SparseCore>
#include <fmt/format.h>

#include <hven/core/types.h>
#include <hven/model/nlp_model.h>

#include "scale_problems.h"

namespace hven::solvers::test_support {

/// @brief F7's collocation chain plus one scaled contradictory equality pair.
///
/// `me()` is the base family's `me()` + 2; every other shape, every bound and
/// the start point are the base family's, unchanged. The base model is held by
/// value and is never mutated after construction.
class F7InconsistentChain : public NlpModel {
  public:
    /// @param nodes  the chain's node count, forwarded to F7CollocationChain
    ///               (>= 3, that family's own guard).
    /// @param scale  `s` above -- the row scaling. The default matches the
    ///               three-mode W2 T7 fixture's own 1e2.
    /// @param offset `b` above -- half the gap between the two rows' targets.
    ///               Must be > 0 and <= control_dim() * kControlBound so each
    ///               row is individually reachable inside the box.
    explicit F7InconsistentChain(Index nodes, double scale = 1.0e2, double offset = 1.0)
        : base_(nodes), scale_(checked_positive(scale, "scale")),
          offset_(checked_offset(offset, base_)) {}

    /// The node count the base family was built at -- the size this cell IS.
    Index node_count() const { return base_.node_count(); }
    /// The base family, for a caller that wants its analytic surface.
    const F7CollocationChain &base() const { return base_; }
    double scale() const { return scale_; }
    double offset() const { return offset_; }

    // ---- NlpModel ----
    Index n() const override { return base_.n(); }
    Index me() const override { return base_.me() + 2; }
    Index mi() const override { return base_.mi(); }

    double eval_f(const Vec &x) const override { return base_.eval_f(x); }
    Vec eval_grad(const Vec &x) const override { return base_.eval_grad(x); }

    Vec eval_ce(const Vec &x) const override {
        Vec out(me());
        out.head(base_.me()) = base_.eval_ce(x);
        const double a = control_sum(x);
        out(base_.me()) = scale_ * (a - offset_);
        out(base_.me() + 1) = scale_ * (a + offset_);
        return out;
    }

    Vec eval_ci(const Vec &x) const override { return base_.eval_ci(x); }

    /// The two added rows are AFFINE, so they contribute nothing to the
    /// Hessian of the Lagrangian -- exactly as the base family's own equality
    /// rows do not (its `eval_hess` ignores `lambda_e` for that reason). The
    /// multiplier block is truncated to the base family's `me()` rather than
    /// passed through, so this stays true even if that ever changes.
    SpMatRM eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                      const Vec &lambda_i) const override {
        return base_.eval_hess(x, obj_scale, lambda_e.head(base_.me()), lambda_i);
    }

    /// Je with two CONSTANT rows appended. Both carry the same entries -- `s`
    /// in each of the last node's control columns -- which is what makes the
    /// pair inconsistent rather than merely redundant once the residuals differ.
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        const Eigen::SparseMatrix<double, Eigen::RowMajor> base_jac = base_.eval_jac_e(x);
        std::vector<Eigen::Triplet<double>> t;
        t.reserve(static_cast<std::size_t>(base_jac.nonZeros() + 2 * base_.control_dim()));
        for (Index r = 0; r < base_jac.rows(); ++r) {
            for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(base_jac, r); it;
                 ++it) {
                t.emplace_back(static_cast<int>(it.row()), static_cast<int>(it.col()), it.value());
            }
        }
        const Index base_col = control_base();
        for (Index j = 0; j < base_.control_dim(); ++j) {
            t.emplace_back(static_cast<int>(base_.me()), static_cast<int>(base_col + j), scale_);
            t.emplace_back(static_cast<int>(base_.me() + 1), static_cast<int>(base_col + j),
                           scale_);
        }
        Eigen::SparseMatrix<double, Eigen::RowMajor> je(me(), n());
        je.setFromTriplets(t.begin(), t.end());
        je.makeCompressed();
        return je;
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        return base_.eval_jac_i(x);
    }

    const Vec &lower() const override { return base_.lower(); }
    const Vec &upper() const override { return base_.upper(); }
    Vec start_point() const override { return base_.start_point(); }

  private:
    /// The last node's first control column.
    Index control_base() const {
        return base_.node_offset(base_.node_count() - 1) + base_.state_dim();
    }
    double control_sum(const Vec &x) const {
        return x.segment(control_base(), base_.control_dim()).sum();
    }

    static double checked_positive(double v, const char *what) {
        if (!(v > 0.0)) {
            throw std::invalid_argument(
                fmt::format("F7InconsistentChain: {} is {}, expected > 0", what, v));
        }
        return v;
    }
    /// VALIDATED AT THE BOUNDARY (CLAUDE.md section 4): an offset past the
    /// control box would make each row separately unreachable, which is a
    /// DIFFERENT infeasibility from the contradictory pair this cell is for --
    /// and it would fail silently, as a cell that fires for the wrong reason.
    static double checked_offset(double v, const F7CollocationChain &base) {
        const double cap =
            static_cast<double>(base.control_dim()) * F7CollocationChain::kControlBound;
        if (!(v > 0.0) || !(v <= cap)) {
            throw std::invalid_argument(fmt::format(
                "F7InconsistentChain: offset is {}, expected 0 < offset <= {} (control_dim {} x "
                "kControlBound {}), so that each of the two contradictory rows is individually "
                "reachable inside the control box",
                v, cap, base.control_dim(), F7CollocationChain::kControlBound));
        }
        return v;
    }

    F7CollocationChain base_;
    double scale_;
    double offset_;
};

} // namespace hven::solvers::test_support
