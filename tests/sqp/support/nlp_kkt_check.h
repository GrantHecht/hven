#pragma once

// tests/sqp/support/nlp_kkt_check.h — test-support only, NOT part of the public
// library surface. The IN-TEST KKT SELF-CHECK: given a model and the
// SqpSolution a driver returned for it, recompute the whole KKT quadruple
// FROM THE MODEL at the returned point.
//
// It deliberately does not call sqp_driver.h's evaluate_kkt. The point is to
// check the REPORTED quadruple (x, lambda_e, lambda_i, z) against
// nlp_model.h's stationarity convention
//
//     grad f + Je^T lambda_e + Ji^T lambda_i - z = 0,  lambda_i >= 0,
//     z >= 0 at an active lower bound, z <= 0 at an active upper bound, and
//     z == 0 on a free variable,
//
// rather than to re-run the driver's own measurement -- so a driver that
// mismeasured its own residual cannot certify itself. It is the NLP analogue
// of test_scale_smoke.cpp's self_check_kkt, which does the same for a QP.
//
// TASK 11 MOVED THIS OUT OF tests/test_sqp_driver.cpp, where it was a
// file-local helper, because the Hock-Schittkowski battery
// (tests/test_hs_battery.cpp) needs the SAME check on 27 problems and a
// second copy of it would be a second thing to keep correct. The code is
// unchanged from that file's version; only its home and its namespace moved.

#include <algorithm>
#include <cmath>

#include <Eigen/Dense>

#include <hven/drivers/sqp_types.h>
#include <hven/model/nlp_model.h>

namespace hven::solvers::test_support {

struct NlpKktResidual {
    double stationarity = 0.0;    // ||grad f + Je^T le + Ji^T li - z||inf
    double primal = 0.0;          // worst bound / cI / cE violation
    double dual_sign = 0.0;       // worst lambda_i < 0 or wrong-signed/nonzero-when-free z
    double complementarity = 0.0; // max |lambda_i * cI|, |z * dist-to-bound|
};

// bound_tol is the GEOMETRIC ACTIVITY TOLERANCE: a variable within it of a
// bound is treated as sitting on that bound, matching SqpOptions::feas_tol's
// double duty (see sqp_types.h). Callers pass opts.feas_tol.
inline NlpKktResidual self_check_kkt(const NlpModel &model, const SqpSolution &sol,
                                     double bound_tol) {
    NlpKktResidual r;
    const Index n = model.n();

    Vec grad = model.eval_grad(sol.x);
    if (model.me() > 0) {
        grad += model.eval_jac_e(sol.x).transpose() * sol.lambda_e;
    }
    if (model.mi() > 0) {
        grad += model.eval_jac_i(sol.x).transpose() * sol.lambda_i;
    }
    r.stationarity = (grad - sol.z).lpNorm<Eigen::Infinity>();

    const Vec &lo = model.lower();
    const Vec &up = model.upper();
    for (Index i = 0; i < n; ++i) {
        r.primal = std::max(r.primal, std::max(0.0, lo(i) - sol.x(i)));
        r.primal = std::max(r.primal, std::max(0.0, sol.x(i) - up(i)));

        const bool at_lower = (sol.x(i) - lo(i)) <= bound_tol;
        const bool at_upper = (up(i) - sol.x(i)) <= bound_tol;
        if (at_lower && !at_upper) {
            r.dual_sign = std::max(r.dual_sign, std::max(0.0, -sol.z(i)));
        } else if (at_upper && !at_lower) {
            r.dual_sign = std::max(r.dual_sign, std::max(0.0, sol.z(i)));
        } else if (!at_lower && !at_upper) {
            // A free variable must carry no bound price at all.
            r.dual_sign = std::max(r.dual_sign, std::abs(sol.z(i)));
        }
        // SKIP THE COMPLEMENTARITY TERM ON A VARIABLE WITH NO FINITE BOUND ON
        // EITHER SIDE: `dist` there is +inf (or, on a variable free on both
        // sides, an inf - inf NaN in the general case), and z is already
        // required to be exactly 0 by the dual_sign branch above -- so
        // 0 * inf is a silent NaN today, protected only by z landing at
        // EXACTLY 0.0. A future driver returning a merely-tiny z (e.g. 1e-15
        // instead of exact 0.0) would turn that same product into +inf and
        // false-fail the whole battery at once. 14 of this battery's 27
        // problems have all-infinite bounds, so this is not a corner case.
        if (std::isfinite(lo(i)) || std::isfinite(up(i))) {
            const double dist = std::min(sol.x(i) - lo(i), up(i) - sol.x(i));
            r.complementarity = std::max(r.complementarity, std::abs(sol.z(i) * dist));
        }
    }

    if (model.me() > 0) {
        r.primal = std::max(r.primal, model.eval_ce(sol.x).lpNorm<Eigen::Infinity>());
    }
    if (model.mi() > 0) {
        const Vec ci = model.eval_ci(sol.x);
        for (Index j = 0; j < model.mi(); ++j) {
            r.primal = std::max(r.primal, std::max(0.0, ci(j)));
            r.dual_sign = std::max(r.dual_sign, std::max(0.0, -sol.lambda_i(j)));
            r.complementarity = std::max(r.complementarity, std::abs(sol.lambda_i(j) * ci(j)));
        }
    }
    return r;
}

} // namespace hven::solvers::test_support
