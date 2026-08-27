// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

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
//
// M6 W0.4 FIXED THE NON-FINITE HOLE (registered out of M4 Task 5; see
// docs/notes/2026-08-22-m4-task5-close.md §F, which cited the scorer-vs-recorded
// verdict equality "over finite rows" precisely because this function could not
// be trusted off them). Every accumulation below is a max, and a max DROPS a
// NaN: std::max(a, NaN) returns a, and so does the ">= 0 applicability" clamp
// std::max(0.0, NaN) that guards each one-sided violation term -- so a NaN
// residual, gradient, multiplier or bound left all four residuals sitting at
// 0.0, a perfect score computed from garbage. The fix is the explicit sweep in
// the body, semantics deliberately IDENTICAL to bench/model_surface_kkt.h's
// (which already had it, and argues in its own comment why only a direct test
// can find this): any non-finite input scores every residual +inf. Identical
// semantics is the point -- the two checkers are compared cell by cell, so they
// must agree on poisoned rows as well as clean ones.

#include <algorithm>
#include <cmath>
#include <limits>

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
//
// NON-FINITE INPUT IS SCORED, NOT THROWN ON: if any quantity this check
// consumes is non-finite -- the returned quadruple, the model's gradient or
// either residual block at that point, a NaN bound, or a non-finite bound_tol
// -- ALL FOUR residuals come back +inf. An INFINITE BOUND is ordinary and legal
// (a variable free on one side), so bounds are tested for NaN alone; everything
// else must be finite. +inf rather than NaN, and all four rather than the one
// term the poison happened to reach, so that this reading and
// bench/model_surface_kkt.h's are the same reading on a poisoned row.
inline NlpKktResidual self_check_kkt(const NlpModel &model, const SqpSolution &sol,
                                     double bound_tol) {
    NlpKktResidual r;
    const Index n = model.n();
    const Vec &lo = model.lower();
    const Vec &up = model.upper();

    Vec grad = model.eval_grad(sol.x);
    if (model.me() > 0) {
        grad += model.eval_jac_e(sol.x).transpose() * sol.lambda_e;
    }
    if (model.mi() > 0) {
        grad += model.eval_jac_i(sol.x).transpose() * sol.lambda_i;
    }
    // Hoisted above the sweep, not moved in cost: exactly the same two calls
    // the two blocks below used to make, under exactly the same me()/mi()
    // guards -- the residual blocks are swept for poison before anything reads
    // them, which is only possible if they exist by then.
    const Vec ce = model.me() > 0 ? model.eval_ce(sol.x) : Vec(0);
    const Vec ci = model.mi() > 0 ? model.eval_ci(sol.x) : Vec(0);

    // THE NON-FINITE SWEEP. It is a direct test rather than something folded
    // into the maxima below because a max cannot do it: every accumulation
    // there is std::max(current, term) and every one-sided term is itself a
    // std::max(0.0, raw) applicability clamp, and BOTH return their finite
    // argument when handed a NaN. A poisoned point therefore used to score
    // zeros -- silently, on a row that claimed kOptimal. The clamps and the
    // at_lower/at_upper/free classification below are correct on finite data
    // and are reached only with finite data because this returns first.
    bool bounds_are_nan_free = true;
    for (Index i = 0; i < n && bounds_are_nan_free; ++i) {
        bounds_are_nan_free = !std::isnan(lo(i)) && !std::isnan(up(i));
    }
    if (!bounds_are_nan_free || !std::isfinite(bound_tol) || !sol.x.allFinite() ||
        !sol.z.allFinite() || !sol.lambda_e.allFinite() || !sol.lambda_i.allFinite() ||
        !grad.allFinite() || !ce.allFinite() || !ci.allFinite()) {
        const double infinite = std::numeric_limits<double>::infinity();
        r.stationarity = infinite;
        r.primal = infinite;
        r.dual_sign = infinite;
        r.complementarity = infinite;
        return r;
    }

    r.stationarity = (grad - sol.z).lpNorm<Eigen::Infinity>();

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
        // 0 * inf is a NaN MANUFACTURED HERE, protected only by z landing at
        // EXACTLY 0.0. A future driver returning a merely-tiny z (e.g. 1e-15
        // instead of exact 0.0) would turn that same product into +inf and
        // false-fail the whole battery at once. 14 of this battery's 27
        // problems have all-infinite bounds, so this is not a corner case.
        // THE SWEEP ABOVE DOES NOT COVER THIS and is not meant to: it screens
        // the INPUTS, and this NaN is a product of two perfectly finite ones,
        // so the skip remains the only thing keeping the term honest.
        if (std::isfinite(lo(i)) || std::isfinite(up(i))) {
            const double dist = std::min(sol.x(i) - lo(i), up(i) - sol.x(i));
            r.complementarity = std::max(r.complementarity, std::abs(sol.z(i) * dist));
        }
    }

    if (model.me() > 0) {
        r.primal = std::max(r.primal, ce.lpNorm<Eigen::Infinity>());
    }
    if (model.mi() > 0) {
        for (Index j = 0; j < model.mi(); ++j) {
            r.primal = std::max(r.primal, std::max(0.0, ci(j)));
            r.dual_sign = std::max(r.dual_sign, std::max(0.0, -sol.lambda_i(j)));
            r.complementarity = std::max(r.complementarity, std::abs(sol.lambda_i(j) * ci(j)));
        }
    }
    return r;
}

} // namespace hven::solvers::test_support
