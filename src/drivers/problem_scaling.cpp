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
// are (drivers/aggregate_eval_seam.cpp).
//
// THE `!(x > 0.0)` IDIOM APPEARS HERE FOR THE SAME REASON IT APPEARS IN
// drivers/sqp_options.cpp, and that file's banner carries the full argument:
// under SAFER_FAST (`-ffast-math -fno-finite-math-only`, this library's one
// uniform flag regime) the negated form is the one that REJECTS NaN, because
// NaN compares false against everything. A row norm that came back NaN must
// take the "no usable data" branch and yield 1.0; it must never reach the
// clamp, where it would propagate into every value the row carries.

#include <hven/detail/drivers/problem_scaling.h>

#include <algorithm>
#include <cmath>

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
    // THE ONE-SIDED ARM, and it is the ROW arm. A factor above 1 AMPLIFIES, and
    // amplifying a constraint row is a different risk from amplifying the
    // objective: a near-degenerate row -- one whose Jacobian is small at the
    // start point because the row is nearly inactive there, not because its
    // units are wrong -- would have its residual multiplied by up to
    // factor_limit, manufacturing infeasibility out of a row that has none.
    // Equilibration's actual purpose is removing the SPREAD between rows, and
    // shrinking the large ones achieves that without ever inventing a residual.
    // This is IPOPT's rule for rows, and it is deliberately NOT the rule for the
    // objective -- see compute_problem_scaling.
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
    // lpNorm<Infinity>() on an empty vector is 0.0, which factor_for maps to the
    // identity -- the right answer for a model with no variables to speak of.
    // THE OBJECTIVE IS TWO-SIDED, the rows are not, and the asymmetry is the
    // design rather than an inconsistency. An objective gradient that is TOO
    // SMALL is a correctness problem, not a conditioning one: it certifies a KKT
    // point that is not one (HS25's zero-major kOptimal at f = 32.8 against a
    // true f* = 0), and only amplification can refuse that certificate. A
    // constraint row that is too small carries no such false verdict -- the
    // feasibility test reads its residual directly -- so it is left alone rather
    // than amplified. See factor_for's ONE-SIDED ARM.
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

} // namespace hven::solvers::detail
