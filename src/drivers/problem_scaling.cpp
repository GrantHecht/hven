// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// problem_scaling.cpp -- the factor computation declared in
// detail/drivers/problem_scaling.h.
//
// CLAUDE.md section 5 homes orchestration and options-shaped code in .cpp
// translation units regardless of how hot the surrounding loop is. This code
// runs ONCE PER SOLVE, over the start point's derivatives, and nothing in it
// depends on inlining through a template parameter -- the per-major and
// per-minor arithmetic that DOES is the applying, which stays where the values
// are (drivers/assembly_eval_seam.cpp).
//
// THE `!(x > 0.0)` IDIOM APPEARS HERE FOR THE SAME REASON IT APPEARS IN
// drivers/sqp_options.cpp, and that file's banner carries the full argument:
// under SAFER_FAST (`-ffast-math -fno-finite-math-only`, this library's one
// uniform flag regime) the negated form is the one that REJECTS NaN, because
// NaN compares false against everything. A row norm that came back NaN must
// take the "no usable data" branch and yield 1.0; it must never reach the
// clamp, where it would propagate into every value the row carries.

#include <hven/detail/drivers/problem_scaling.h>

#include <hven/detail/drivers/column_scaling.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include <fmt/format.h>

namespace hven::solvers::detail {
namespace {

// The rule of section 1.3, for one norm. Factored out because the objective and
// every constraint row take it identically -- that sameness is the design, not
// an accident of implementation, and a reader should not have to diff three
// copies to confirm it.
double factor_for(double norm, const ScalingRule &rule, bool two_sided) {
    // NaN and +-inf take this branch, as does an exactly-zero norm: none of the
    // three carries a usable scale, and the identity is the honest answer. See
    // the banner on why the predicate is written negated.
    if (!(norm > 0.0) || !std::isfinite(norm)) {
        return 1.0;
    }
    const double raw = rule.max_gradient / norm;
    // THE ONE-SIDED ARM, and it is the ROW arm: a factor above 1 AMPLIFIES, and a
    // row is never amplified. problem_scaling.h's compute_problem_scaling
    // carries the asymmetry and its reasoning; this is its enforcement.
    if (!two_sided && raw > 1.0) {
        return 1.0;
    }
    return std::clamp(raw, 1.0 / rule.factor_limit, rule.factor_limit);
}

// Inf-norm of each row of a ROW-MAJOR sparse matrix, in one pass over the
// stored entries. Structurally empty rows come back 0.0 and are handed to
// factor_for, which maps them to the identity.
Vec row_inf_norms(const SpMatRM &m) {
    Vec norms = Vec::Zero(m.rows());
    for (Index r = 0; r < m.rows(); ++r) {
        double best = 0.0;
        for (SpMatRM::InnerIterator it(m, r); it; ++it) {
            best = std::max(best, std::abs(it.value()));
        }
        norms(r) = best;
    }
    return norms;
}

double extreme_row_factor(const ProblemScaling &s, bool want_max) {
    bool seen = false;
    double best = 1.0;
    const auto fold = [&](const Vec &block) {
        for (Index i = 0; i < block.size(); ++i) {
            const double v = block(i);
            if (!seen) {
                best = v;
                seen = true;
            } else {
                best = want_max ? std::max(best, v) : std::min(best, v);
            }
        }
    };
    fold(s.eq_rows);
    fold(s.ineq_rows);
    // No rows at all -- a bound-constrained problem -- reports the identity
    // rather than an empty-range sentinel, so a reader never has to special-case
    // the report of a problem that simply has nothing to equilibrate.
    return best;
}

} // namespace

double ProblemScaling::row_max() const noexcept { return extreme_row_factor(*this, true); }

double ProblemScaling::row_min() const noexcept { return extreme_row_factor(*this, false); }

ProblemScaling compute_problem_scaling(const Vec &grad, const SpMatRM &Je, const SpMatRM &Ji,
                                       const ScalingRule &rule) {
    ProblemScaling s;
    s.active = true;
    // An empty gradient folds to 0.0, which factor_for maps to the identity --
    // the right answer for a model with no variables to speak of. THE OBJECTIVE
    // IS TWO-SIDED and the rows are not; the header's declaration carries why.
    s.obj = factor_for(grad.size() > 0 ? grad.cwiseAbs().maxCoeff() : 0.0, rule,
                       /*two_sided=*/true);

    const Vec e_norms = row_inf_norms(Je);
    s.eq_rows.resize(e_norms.size());
    for (Index i = 0; i < e_norms.size(); ++i) {
        s.eq_rows(i) = factor_for(e_norms(i), rule, /*two_sided=*/false);
    }

    const Vec i_norms = row_inf_norms(Ji);
    s.ineq_rows.resize(i_norms.size());
    for (Index i = 0; i < i_norms.size(); ++i) {
        s.ineq_rows(i) = factor_for(i_norms(i), rule, /*two_sided=*/false);
    }
    return s;
}

// ===========================================================================
// THE COLUMN LAYER (M7 W2 SCL), declared in detail/drivers/column_scaling.h. In
// this TU rather than one of its own for the reason the row layer is here: it
// runs once per solve over the start point's derivatives, and ColumnScaledModel's
// per-evaluation arithmetic is one multiply per stored entry beside the model call
// it wraps, which no inlining across this boundary would buy back.
// ===========================================================================

namespace {

// A Jacobian entry carries scale information only when it is finite and nonzero.
bool usable(double v) { return std::isfinite(v) && v != 0.0; }

// The nearest power of two to v > 0, its exponent confined to
// [-floor(log2 limit), floor(log2 limit)] so the clamp keeps the factor a power of
// two inside [1/limit, limit].
double power_of_two_factor(double v, double limit) {
    const int cap = static_cast<int>(std::floor(std::log2(limit)));
    const int e = std::clamp(static_cast<int>(std::lround(std::log2(v))), -cap, cap);
    return std::ldexp(1.0, e);
}

} // namespace

ColumnScaling compute_column_scaling(Index n, const SpMatRM &Je, const SpMatRM &Ji,
                                     const ColumnScalingRule &rule) {
    if (Je.cols() != n || Ji.cols() != n) {
        throw std::invalid_argument(fmt::format(
            "compute_column_scaling: the Jacobians have {} and {} columns, expected n = {}",
            Je.cols(), Ji.cols(), n));
    }
    const SpMatRM *blocks[2] = {&Je, &Ji};
    Vec c = Vec::Ones(n);
    Vec r[2] = {Vec::Ones(Je.rows()), Vec::Ones(Ji.rows())};
    std::vector<char> has_data(static_cast<std::size_t>(n), 0);
    for (const SpMatRM *J : blocks) {
        for (Index i = 0; i < J->outerSize(); ++i) {
            for (SpMatRM::InnerIterator it(*J, i); it; ++it) {
                if (usable(it.value())) {
                    has_data[static_cast<std::size_t>(it.col())] = 1;
                }
            }
        }
    }
    // RUIZ'S SWEEP: both norms are read off the SAME scaled matrix, then both sides
    // are divided by the square root -- the published simultaneous form. Rows and
    // columns with no usable entry keep their factor (their norm is 0).
    Vec row_norm[2];
    Vec col_norm(n);
    for (Index sweep = 0; sweep < rule.sweeps; ++sweep) {
        col_norm.setZero();
        bool equilibrated = true;
        for (int b = 0; b < 2; ++b) {
            const SpMatRM &J = *blocks[b];
            row_norm[b] = Vec::Zero(J.rows());
            for (Index i = 0; i < J.outerSize(); ++i) {
                for (SpMatRM::InnerIterator it(J, i); it; ++it) {
                    if (!usable(it.value())) {
                        continue;
                    }
                    const double a = std::abs(r[b](i) * it.value() * c(it.col()));
                    row_norm[b](i) = std::max(row_norm[b](i), a);
                    col_norm(it.col()) = std::max(col_norm(it.col()), a);
                }
            }
            for (Index i = 0; i < J.rows(); ++i) {
                if (row_norm[b](i) > 0.0 && std::abs(row_norm[b](i) - 1.0) > rule.tolerance) {
                    equilibrated = false;
                }
            }
        }
        for (Index j = 0; j < n; ++j) {
            if (col_norm(j) > 0.0 && std::abs(col_norm(j) - 1.0) > rule.tolerance) {
                equilibrated = false;
            }
        }
        if (equilibrated) {
            break;
        }
        for (int b = 0; b < 2; ++b) {
            for (Index i = 0; i < r[b].size(); ++i) {
                if (row_norm[b](i) > 0.0 && std::isfinite(row_norm[b](i))) {
                    r[b](i) /= std::sqrt(row_norm[b](i));
                }
            }
        }
        for (Index j = 0; j < n; ++j) {
            if (col_norm(j) > 0.0 && std::isfinite(col_norm(j))) {
                c(j) /= std::sqrt(col_norm(j));
            }
        }
    }

    ColumnScaling out;
    Vec d = Vec::Ones(n);
    bool seen = false;
    bool moved = false;
    for (Index j = 0; j < n; ++j) {
        if (!has_data[static_cast<std::size_t>(j)] || !(c(j) > 0.0) || !std::isfinite(c(j))) {
            continue;
        }
        d(j) = power_of_two_factor(c(j), rule.factor_limit);
        moved = moved || d(j) != 1.0;
        out.col_min = seen ? std::min(out.col_min, d(j)) : d(j);
        out.col_max = seen ? std::max(out.col_max, d(j)) : d(j);
        seen = true;
    }
    out.spread = out.col_max / out.col_min;
    if (moved && out.spread >= rule.threshold) {
        out.active = true;
        out.cols = std::move(d);
    }
    return out;
}

ColumnScaledModel::ColumnScaledModel(const NlpModel &model, Vec d)
    : model_(model), d_(std::move(d)) {
    if (d_.size() != model_.n()) {
        throw std::invalid_argument(fmt::format(
            "ColumnScaledModel: {} factors for a model of {} variables", d_.size(), model_.n()));
    }
    for (Index j = 0; j < d_.size(); ++j) {
        if (!(d_(j) > 0.0) || !std::isfinite(d_(j))) {
            throw std::invalid_argument(fmt::format(
                "ColumnScaledModel: factor {} is {}; every factor must be finite and > 0", j,
                d_(j)));
        }
    }
    // inf / d is inf, so an absent bound stays absent. A finite bound whose quotient
    // overflows becomes infinite (the banner's OVERFLOW note).
    lower_ = model_.lower().cwiseQuotient(d_);
    upper_ = model_.upper().cwiseQuotient(d_);
}

Vec ColumnScaledModel::to_caller(const Vec &x_scaled) const { return d_.cwiseProduct(x_scaled); }

Vec ColumnScaledModel::to_scaled(const Vec &x) const { return x.cwiseQuotient(d_); }

void ColumnScaledModel::scale_columns(SpMatRM &jac) const {
    for (Index i = 0; i < jac.outerSize(); ++i) {
        for (SpMatRM::InnerIterator it(jac, i); it; ++it) {
            it.valueRef() *= d_(it.col());
        }
    }
}

void ColumnScaledModel::scale_symmetric(SpMatRM &hess) const {
    for (Index i = 0; i < hess.outerSize(); ++i) {
        for (SpMatRM::InnerIterator it(hess, i); it; ++it) {
            it.valueRef() *= d_(it.row()) * d_(it.col());
        }
    }
}

double ColumnScaledModel::eval_f(const Vec &x) const { return model_.eval_f(to_caller(x)); }

HVEN_SUPPRESS_DEPRECATED_BEGIN
Vec ColumnScaledModel::eval_grad(const Vec &x) const {
    Vec g;
    eval_grad_in_place(x, g);
    return g;
}

Vec ColumnScaledModel::eval_ce(const Vec &x) const {
    Vec c;
    eval_ce_in_place(x, c);
    return c;
}

Vec ColumnScaledModel::eval_ci(const Vec &x) const {
    Vec c;
    eval_ci_in_place(x, c);
    return c;
}

SpMatRM ColumnScaledModel::eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                                     const Vec &lambda_i) const {
    SpMatRM h;
    eval_hess_in_place(x, obj_scale, lambda_e, lambda_i, h);
    return h;
}

SpMatRM ColumnScaledModel::eval_jac_e(const Vec &x) const {
    SpMatRM j;
    eval_jac_e_in_place(x, j);
    return j;
}

SpMatRM ColumnScaledModel::eval_jac_i(const Vec &x) const {
    SpMatRM j;
    eval_jac_i_in_place(x, j);
    return j;
}
HVEN_SUPPRESS_DEPRECATED_END

void ColumnScaledModel::eval_grad_in_place(const Vec &x, Vec &out) const {
    model_.eval_grad_in_place(to_caller(x), out);
    out.array() *= d_.array();
}

void ColumnScaledModel::eval_ce_in_place(const Vec &x, Vec &out) const {
    model_.eval_ce_in_place(to_caller(x), out);
}

void ColumnScaledModel::eval_ci_in_place(const Vec &x, Vec &out) const {
    model_.eval_ci_in_place(to_caller(x), out);
}

void ColumnScaledModel::eval_jac_e_in_place(const Vec &x, SpMatRM &out) const {
    model_.eval_jac_e_in_place(to_caller(x), out);
    scale_columns(out);
}

void ColumnScaledModel::eval_jac_i_in_place(const Vec &x, SpMatRM &out) const {
    model_.eval_jac_i_in_place(to_caller(x), out);
    scale_columns(out);
}

void ColumnScaledModel::eval_hess_in_place(const Vec &x, double obj_scale, const Vec &lambda_e,
                                           const Vec &lambda_i, SpMatRM &out) const {
    model_.eval_hess_in_place(to_caller(x), obj_scale, lambda_e, lambda_i, out);
    scale_symmetric(out);
}

Vec ColumnScaledModel::start_point() const { return to_scaled(model_.start_point()); }

void ColumnScaledModel::eval_values(const Vec &x, double &f, Vec &cE, Vec &cI) const {
    model_.eval_values(to_caller(x), f, cE, cI);
}

} // namespace hven::solvers::detail
