#pragma once

// tests/sqp/support/derivative_check.h — test-support only, NOT part of the
// public library surface. Central-difference verification of an NlpModel's
// analytic derivatives against its own zeroth/first-order functions -- the
// TRANSCRIPTION GUARD for every hand-derived Hock-Schittkowski problem in
// hs_problems.h: a sign error or dropped term in an analytic gradient/
// Jacobian/Hessian is exactly the kind of mistake central differences catch
// and a hand re-derivation on paper does not reliably catch.
//
// Each assert_* returns a ::testing::AssertionResult (gtest's predicate-
// assertion idiom) rather than asserting directly, so a call site chooses
// EXPECT_TRUE/ASSERT_TRUE -- and so the checker itself can be pinned by a
// mutation self-test that expects EXPECT_FALSE on a deliberately-wrong model
// (see test_nlp_model.cpp's DerivativeCheckSelfTest battery): if assert_*
// asserted directly, "the checker rejects a wrong gradient" could not be
// expressed as an in-test assertion at all.
//
// STEP SIZE. All three checks use a fixed central-difference step h = 1e-6.
// For a central difference, truncation error is O(h^2) and roundoff error is
// O(eps/h); h = 1e-6 puts truncation at ~1e-12 and roundoff (eps ~ 2.2e-16)
// at ~2.2e-10 -- both far below the tol = 1e-6 the brief specifies for every
// call site, so h is not exposed as a per-call parameter.
//
// TOLERANCE IS MIXED ABSOLUTE/RELATIVE, NOT ABSOLUTE (Task 11). The paragraph
// above is the reason -- and it is only true for derivatives of size O(1).
// Both error terms scale with the magnitude of the quantity being
// differenced: the roundoff floor of a central difference is
// eps * |value| / h, so at |value| ~ 1e4 with h = 1e-6 it is
// 2.2e-16 * 1e4 / 1e-6 = 2.2e-6 -- ABOVE the tol = 1e-6 every call site
// passes, with no error in the analytic derivative at all. Task 3 shipped an
// absolute comparison because its six problems (HS1/3/5/6/7/76) all have
// O(1)-to-O(1e3) derivatives at the checked points and none of them tripped
// it. Task 11's battery breaks that: HS38 at its published start point
// (-3,-1,-3,-1) has |grad f| ~ 1.2e4 and |hess L| ~ 1.1e4, which makes the
// FD-Hessian roundoff floor ~2.6e-6 -- a GUARANTEED false failure under an
// absolute 1e-6, measured before this change (see
// docs/notes/2026-07-29-hs-battery-results.md, carry item 1). HS15 (|grad| ~
// 2.4e3) sits within a factor of 4 of the same cliff.
//
// So every comparison below is now
//
//     |analytic - fd| <= tol * max(1, |analytic|)
//
// i.e. tol is RELATIVE for large entries and stays ABSOLUTE (the max's 1.0
// floor) for small ones, which is what keeps a wrong-but-tiny derivative from
// being excused by its own smallness. The scale is taken from the ANALYTIC
// value rather than the finite difference deliberately: the analytic value is
// the claim under test, so a mutation that inflates it inflates the tolerance
// it must beat by the same factor and still fails (a 10% error must beat a
// 1e-6 relative bound -- five orders of margin, which is exactly what
// DerivativeCheckSelfTest.WrongGradientFailsAssertGradient pins).
//
// assert_hessian finite-differences the GRADIENT OF THE LAGRANGIAN,
//
//     gradL(x) = eval_grad(x) + Je(x)^T lambda_e + Ji(x)^T lambda_i
//
// (obj_scale fixed at 1.0, matching nlp_model.h's stationarity convention)
// and compares its Jacobian (a finite-difference HESSIAN of L) against
// eval_hess(x, 1.0, lambda_e, lambda_i)'s upper triangle, symmetrized. This
// is deliberate: finite-differencing hess(f) alone would never notice a
// wrong or missing sum_i lambda_e(i)*hess(cE_i) / sum_j lambda_i(j)*
// hess(cI_j) term, which is exactly the composite eval_hess is contracted to
// return.

#include <algorithm>
#include <cmath>

#include <Eigen/Dense>
#include <gtest/gtest.h>

#include <hven/model/nlp_model.h>

namespace hven::solvers::test_support {

namespace detail {

constexpr double kFdStep = 1e-6;

// The mixed absolute/relative bound this header's TOLERANCE note specifies:
// tol scaled by the analytic value's own magnitude, with an absolute floor of
// 1.0 so that a small analytic entry is still held to `tol` outright.
inline double scaled_tol(double tol, double analytic) {
    return tol * std::max(1.0, std::abs(analytic));
}

// Symmetrizes an upper-triangle-only sparse matrix (SpMatRM's convention)
// into a dense n x n matrix.
inline Eigen::MatrixXd symmetrize_upper(const SpMatRM &upper) {
    const Eigen::MatrixXd dense = Eigen::MatrixXd(upper);
    return Eigen::MatrixXd(dense.selfadjointView<Eigen::Upper>());
}

} // namespace detail

// Central-difference check of eval_grad against eval_f.
inline ::testing::AssertionResult assert_gradient(const NlpModel &model, const Vec &x, double tol) {
    const Index n = model.n();
    if (x.size() != n) {
        return ::testing::AssertionFailure()
               << "assert_gradient: x has size " << x.size() << ", expected n=" << n;
    }
    const Vec analytic = model.eval_grad(x);
    if (analytic.size() != n) {
        return ::testing::AssertionFailure() << "assert_gradient: eval_grad returned size "
                                             << analytic.size() << ", expected n=" << n;
    }

    const double h = detail::kFdStep;
    for (Index i = 0; i < n; ++i) {
        Vec xp = x, xm = x;
        xp(i) += h;
        xm(i) -= h;
        const double fd = (model.eval_f(xp) - model.eval_f(xm)) / (2.0 * h);
        const double diff = std::abs(analytic(i) - fd);
        const double bound = detail::scaled_tol(tol, analytic(i));
        if (diff > bound) {
            return ::testing::AssertionFailure()
                   << "assert_gradient: mismatch at index " << i << ": analytic=" << analytic(i)
                   << " fd=" << fd << " |diff|=" << diff << " > tol*max(1,|analytic|)=" << bound;
        }
    }
    return ::testing::AssertionSuccess();
}

// Central-difference check of eval_jac_e/eval_jac_i against eval_ce/eval_ci.
inline ::testing::AssertionResult assert_jacobians(const NlpModel &model, const Vec &x,
                                                   double tol) {
    const Index n = model.n();
    const Index me = model.me();
    const Index mi = model.mi();
    if (x.size() != n) {
        return ::testing::AssertionFailure()
               << "assert_jacobians: x has size " << x.size() << ", expected n=" << n;
    }

    const Eigen::MatrixXd Je = Eigen::MatrixXd(model.eval_jac_e(x));
    const Eigen::MatrixXd Ji = Eigen::MatrixXd(model.eval_jac_i(x));
    if (Je.rows() != me || Je.cols() != n) {
        return ::testing::AssertionFailure() << "assert_jacobians: eval_jac_e is " << Je.rows()
                                             << "x" << Je.cols() << ", expected " << me << "x" << n;
    }
    if (Ji.rows() != mi || Ji.cols() != n) {
        return ::testing::AssertionFailure() << "assert_jacobians: eval_jac_i is " << Ji.rows()
                                             << "x" << Ji.cols() << ", expected " << mi << "x" << n;
    }

    const double h = detail::kFdStep;
    for (Index j = 0; j < n; ++j) {
        Vec xp = x, xm = x;
        xp(j) += h;
        xm(j) -= h;
        const Vec ce_p = model.eval_ce(xp);
        const Vec ce_m = model.eval_ce(xm);
        for (Index i = 0; i < me; ++i) {
            const double fd = (ce_p(i) - ce_m(i)) / (2.0 * h);
            const double diff = std::abs(Je(i, j) - fd);
            const double bound = detail::scaled_tol(tol, Je(i, j));
            if (diff > bound) {
                return ::testing::AssertionFailure()
                       << "assert_jacobians: eval_jac_e mismatch at (row=" << i << ", col=" << j
                       << "): analytic=" << Je(i, j) << " fd=" << fd << " |diff|=" << diff
                       << " > tol*max(1,|analytic|)=" << bound;
            }
        }
        const Vec ci_p = model.eval_ci(xp);
        const Vec ci_m = model.eval_ci(xm);
        for (Index i = 0; i < mi; ++i) {
            const double fd = (ci_p(i) - ci_m(i)) / (2.0 * h);
            const double diff = std::abs(Ji(i, j) - fd);
            const double bound = detail::scaled_tol(tol, Ji(i, j));
            if (diff > bound) {
                return ::testing::AssertionFailure()
                       << "assert_jacobians: eval_jac_i mismatch at (row=" << i << ", col=" << j
                       << "): analytic=" << Ji(i, j) << " fd=" << fd << " |diff|=" << diff
                       << " > tol*max(1,|analytic|)=" << bound;
            }
        }
    }
    return ::testing::AssertionSuccess();
}

// Central-difference check of eval_hess (the exact Lagrangian Hessian, at
// obj_scale = 1.0) against the gradient of the Lagrangian
// gradL(x) = eval_grad(x) + Je(x)^T lambda_e + Ji(x)^T lambda_i.
inline ::testing::AssertionResult assert_hessian(const NlpModel &model, const Vec &x,
                                                 const Vec &lambda_e, const Vec &lambda_i,
                                                 double tol) {
    const Index n = model.n();
    const Index me = model.me();
    const Index mi = model.mi();
    if (x.size() != n) {
        return ::testing::AssertionFailure()
               << "assert_hessian: x has size " << x.size() << ", expected n=" << n;
    }
    if (lambda_e.size() != me) {
        return ::testing::AssertionFailure()
               << "assert_hessian: lambda_e has size " << lambda_e.size() << ", expected me=" << me;
    }
    if (lambda_i.size() != mi) {
        return ::testing::AssertionFailure()
               << "assert_hessian: lambda_i has size " << lambda_i.size() << ", expected mi=" << mi;
    }

    auto grad_lagrangian = [&](const Vec &at) -> Vec {
        Vec g = model.eval_grad(at);
        if (me > 0) {
            g += Eigen::MatrixXd(model.eval_jac_e(at)).transpose() * lambda_e;
        }
        if (mi > 0) {
            g += Eigen::MatrixXd(model.eval_jac_i(at)).transpose() * lambda_i;
        }
        return g;
    };

    const SpMatRM H = model.eval_hess(x, 1.0, lambda_e, lambda_i);
    if (H.rows() != n || H.cols() != n) {
        return ::testing::AssertionFailure() << "assert_hessian: eval_hess returned " << H.rows()
                                             << "x" << H.cols() << ", expected " << n << "x" << n;
    }
    const Eigen::MatrixXd analytic = detail::symmetrize_upper(H);

    const double h = detail::kFdStep;
    for (Index j = 0; j < n; ++j) {
        Vec xp = x, xm = x;
        xp(j) += h;
        xm(j) -= h;
        const Vec fd_col = (grad_lagrangian(xp) - grad_lagrangian(xm)) / (2.0 * h);
        for (Index i = 0; i < n; ++i) {
            const double diff = std::abs(analytic(i, j) - fd_col(i));
            const double bound = detail::scaled_tol(tol, analytic(i, j));
            if (diff > bound) {
                return ::testing::AssertionFailure()
                       << "assert_hessian: mismatch at (row=" << i << ", col=" << j
                       << "): analytic=" << analytic(i, j) << " fd=" << fd_col(i)
                       << " |diff|=" << diff << " > tol*max(1,|analytic|)=" << bound;
            }
        }
    }
    return ::testing::AssertionSuccess();
}

} // namespace hven::solvers::test_support
