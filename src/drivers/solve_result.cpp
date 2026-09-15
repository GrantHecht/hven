// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The shared declared diagnostics, defined once (M6 W5 T8.4).
//
// A TU rather than an inline body, on solve_status.cpp's footing: this is
// reporting arithmetic taken ONCE per solve, at the exit seam, not a
// per-element hot path, and CLAUDE.md §5 places that in a .cpp.
//
// It EVALUATES NOTHING. Every model quantity arrives as an argument, from the
// evaluation the engine already holds at the point it is about to return -- see
// drivers/solve_result.h on why a fresh evaluation here would be a defect and
// not a convenience.

#include <hven/drivers/solve_result.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <fmt/format.h>

namespace hven::solvers {

namespace {

/// The block sizes this function's arithmetic assumes, checked at the boundary
/// rather than left to Eigen's asserts -- which are compiled out under NDEBUG
/// (CLAUDE.md §4).
void check_core_shapes(const Vec &x, const Vec &lambda_i, const Vec &z, const Vec &grad_lag,
                       const Vec &ci, const Vec &lower, const Vec &upper) {
    const Index n = x.size();
    if (z.size() != n || grad_lag.size() != n || lower.size() != n || upper.size() != n) {
        throw std::invalid_argument(fmt::format(
            "compute_declared_diagnostics: x has {} entries, so z, the Lagrangian gradient, lower "
            "and upper must too; got ({}, {}, {}, {})",
            n, z.size(), grad_lag.size(), lower.size(), upper.size()));
    }
    if (lambda_i.size() != ci.size()) {
        throw std::invalid_argument(
            fmt::format("compute_declared_diagnostics: lambda_i has {} entries and ci has {}; the "
                        "declared inequality block is one width",
                        lambda_i.size(), ci.size()));
    }
}

} // namespace

DeclaredDiagnostics compute_declared_diagnostics(const Vec &x, const Vec &lambda_e,
                                                 const Vec &lambda_i, const Vec &z, const Vec &grad,
                                                 const SpMatRM &Je, const SpMatRM &Ji,
                                                 const Vec &ce, const Vec &ci, const Vec &lower,
                                                 const Vec &upper,
                                                 const std::vector<Index> &excluded_coordinates) {
    // The block shapes this overload alone can check -- the ones that vanish
    // once the pieces are folded into one vector.
    const Index n = x.size();
    if (grad.size() != n) {
        throw std::invalid_argument(fmt::format(
            "compute_declared_diagnostics: x has {} entries and grad has {}", n, grad.size()));
    }
    if (lambda_e.size() != ce.size()) {
        throw std::invalid_argument(
            fmt::format("compute_declared_diagnostics: lambda_e has {} entries and ce has {}; the "
                        "declared equality block is one width",
                        lambda_e.size(), ce.size()));
    }
    if (lambda_i.size() != ci.size()) {
        throw std::invalid_argument(
            fmt::format("compute_declared_diagnostics: lambda_i has {} entries and ci has {}; the "
                        "declared inequality block is one width",
                        lambda_i.size(), ci.size()));
    }
    if (ce.size() > 0 && (Je.rows() != ce.size() || Je.cols() != n)) {
        throw std::invalid_argument(
            fmt::format("compute_declared_diagnostics: Je is {}x{}, expected {}x{}", Je.rows(),
                        Je.cols(), ce.size(), n));
    }
    if (ci.size() > 0 && (Ji.rows() != ci.size() || Ji.cols() != n)) {
        throw std::invalid_argument(
            fmt::format("compute_declared_diagnostics: Ji is {}x{}, expected {}x{}", Ji.rows(),
                        Ji.cols(), ci.size(), n));
    }

    // grad L = grad f + Je^T lambda_e + Ji^T lambda_i, WITHOUT the bound term.
    // The bound term is subtracted per coordinate downstream, where the
    // exclusion set is applied.
    Vec grad_lag = grad;
    if (ce.size() > 0) {
        grad_lag += Je.transpose() * lambda_e;
    }
    if (ci.size() > 0) {
        grad_lag += Ji.transpose() * lambda_i;
    }
    return compute_declared_diagnostics_from_grad_lag(x, lambda_i, z, grad_lag, ce, ci, lower,
                                                      upper, excluded_coordinates);
}

DeclaredDiagnostics
compute_declared_diagnostics_from_grad_lag(const Vec &x, const Vec &lambda_i, const Vec &z,
                                           const Vec &grad_lag, const Vec &ce, const Vec &ci,
                                           const Vec &lower, const Vec &upper,
                                           const std::vector<Index> &excluded_coordinates) {
    check_core_shapes(x, lambda_i, z, grad_lag, ci, lower, upper);

    const Index n = x.size();
    const Index me = ce.size();
    const Index mi = ci.size();

    // DEFAULT-CONSTRUCTED IS ALL-NaN, and every early return below leans on
    // that: an unmeasurable point reports UNMEASURED, never a zero residual.
    DeclaredDiagnostics d;

    // THE FINITENESS GATE, and the reason it is here rather than left to the
    // caller: std::max propagates a NaN unpredictably (max(NaN, 3.0) is 3.0 on
    // this platform's libc++), so a single non-finite entry could otherwise
    // produce a finite-looking inf-norm that measured nothing. Every engine
    // already refuses to build a stash at an unevaluable point; this makes the
    // promise structural rather than by convention.
    if (!x.allFinite() || !z.allFinite() || !grad_lag.allFinite() || !ce.allFinite() ||
        !ci.allFinite() || !lambda_i.allFinite()) {
        return d;
    }

    // The excluded set, as a dense mask: the sets are tiny (one entry per
    // bound-fixed variable) but the loop below is O(n), so a per-coordinate
    // linear search over the set would be the only quadratic term here.
    std::vector<bool> excluded(static_cast<std::size_t>(n), false);
    for (const Index idx : excluded_coordinates) {
        if (idx >= 0 && idx < n) {
            excluded[static_cast<std::size_t>(idx)] = true;
        }
    }

    // --- the two CONSTRAINT-BLOCK passes ----------------------------------
    // The equality block's inf-norm; 0 on a problem with no declared equality
    // rows, which is the inf-norm of an empty vector and matches both engines'
    // own econ measures.
    d.feasibility_e = me > 0 ? ce.lpNorm<Eigen::Infinity>() : 0.0;

    // The POSITIVE PART of the inequality rows (the feasible set is ci <= 0)
    // and the inequality complementarity products, over the one block they
    // both read. MAGNITUDES for the products, like the violation beside them:
    // at a feasible point every product is already non-negative, and at an
    // infeasible one a signed product would silently drop out of the inf-norm.
    double fi = 0.0;
    double cp = 0.0;
    for (Index j = 0; j < mi; ++j) {
        fi = std::max(fi, std::max(0.0, ci(j)));
        cp = std::max(cp, std::abs(lambda_i(j) * ci(j)));
    }

    // --- THE ONE COORDINATE PASS ------------------------------------------
    // ONE loop over the declared coordinates, as the plan prescribed and as
    // the T8.4 implementation did not (fix1, astra Minor 8: it ran three).
    // The three quantities it accumulates are independent maxima over the same
    // index set, so folding them changes no value -- std::max over a set of
    // finite doubles does not depend on the order it is taken in, and the
    // finiteness gate above has already refused the only inputs that could
    // make that false.
    //
    // STATIONARITY is grad L - z over the coordinates that were MEASURED. An
    // excluded coordinate is one the producing engine has no row for at all
    // (an interior-point MakeParameter elimination): its reduced gradient
    // reports 0 there, and a 0 that means "no row" must not enter an inf-norm
    // beside 0s that mean "stationary". The other two quantities read the box,
    // which the declaration carries for every coordinate, excluded or not.
    //
    // THE BOUND VIOLATION is part of feasibility because the box is part of
    // the declared problem: a point outside it is infeasible whether or not
    // any row says so.
    //
    // THE BOUND PRICES use the CANONICAL SPLIT -- max(z,0) * (x - l) and
    // max(-z,0) * (u - x) -- because a signed z = zL - zU cannot recover two
    // separate prices at a two-sided bound where both are positive. Where an
    // engine holds the two prices separately -- the interior-point barrier
    // does -- its own two-price measure stays on its own result
    // (IpmResult::barr_inf). An infinite bound contributes NOTHING to either
    // term: there is no price to pay at a bound that does not exist, and
    // 0 * inf is not a measurement.
    double st = 0.0;
    bool measured_any = false;
    for (Index i = 0; i < n; ++i) {
        if (!excluded[static_cast<std::size_t>(i)]) {
            st = std::max(st, std::abs(grad_lag(i) - z(i)));
            measured_any = true;
        }
        if (std::isfinite(lower(i))) {
            fi = std::max(fi, std::max(0.0, lower(i) - x(i)));
            cp = std::max(cp, std::abs(std::max(z(i), 0.0) * (x(i) - lower(i))));
        }
        if (std::isfinite(upper(i))) {
            fi = std::max(fi, std::max(0.0, x(i) - upper(i)));
            cp = std::max(cp, std::abs(std::max(-z(i), 0.0) * (upper(i) - x(i))));
        }
    }
    if (measured_any) {
        d.stationarity = st;
    }
    // else: every coordinate excluded -- nothing was measured, so the NaN the
    // default carries stands.
    d.feasibility_i = fi;
    d.complementarity = cp;

    return d;
}

} // namespace hven::solvers
