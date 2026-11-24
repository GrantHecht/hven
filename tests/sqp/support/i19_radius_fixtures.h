// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The I19-2a radius fixtures (issue #19, the r -> 2r -> r cycle): small models on which the
// growth/shrink defect is reproduced in-repo, and the history readings the tests assert.

#include <cmath>
#include <cstddef>
#include <memory>
#include <vector>

#include <hven/hven.h>

namespace hven::test_support {

/// f = a . x + k (|x|^2 - rho^2) on the sphere |x|^2 = rho^2 (one equality row), x in R^n.
struct I19Sphere final : Nlp {
    Vec a;
    double k = 0.0;
    double rho = 1.0;
    I19Sphere(Vec a_, double k_, double rho_) : a(std::move(a_)), k(k_), rho(rho_) {}
    NlpDimensions dimensions() const override {
        const Index n = a.size();
        return {.n = n, .m = 1, .jacobian_nonzeros = n, .hessian_nonzeros = n};
    }
    void bounds(VecRef l, VecRef u, VecRef gl, VecRef gu) const override {
        l.setConstant(-kInfinity);
        u.setConstant(kInfinity);
        gl[0] = rho * rho;
        gu[0] = rho * rho;
    }
    double eval_f(ConstVecRef x) const override {
        return a.dot(x) + k * (x.squaredNorm() - rho * rho);
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override { g = a + 2.0 * k * x; }
    void eval_g(ConstVecRef x, VecRef g) const override { g[0] = x.squaredNorm(); }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        for (Index j = 0; j < a.size(); ++j) {
            r[j] = 0;
            c[j] = static_cast<int>(j);
        }
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override { v = 2.0 * x; }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        for (Index j = 0; j < a.size(); ++j) {
            r[j] = static_cast<int>(j);
            c[j] = static_cast<int>(j);
        }
    }
    void eval_hessian(ConstVecRef, double s, ConstVecRef lambda, VecRef v) const override {
        v.setConstant(2.0 * s * k + 2.0 * lambda[0]);
    }
};

/// One fixture: the model, its start point and the options it is solved under (Hessian mode
/// aside). All four are I19Sphere on a circle (n = 2) started at (rho, 0) and travelling to the
/// minimiser (0, -rho); the objective scale, the radius, SOC and the caller step limit differ.
struct I19Fixture {
    std::shared_ptr<I19Sphere> model;
    Vec x0;
    SqpOptions options;
};

inline I19Fixture i19_circle(double rho, double a1, bool soc, double cap, Index max_iter,
                             SqpHessianMode mode) {
    I19Fixture f;
    f.model = std::make_shared<I19Sphere>(Vec{{0.0, a1}}, 0.0, rho);
    f.x0 = Vec{{rho, 0.0}};
    f.options.common.verbosity = Verbosity::kSilent;
    f.options.common.max_iterations = max_iter;
    f.options.hessian_mode = mode;
    f.options.advanced.second_order_correction = soc;
    f.options.caller_step_limit = cap;
    return f;
}

/// F-CYC: the pure f-type cycle. A steep linear objective keeps every trial f-type, so the
/// funnel width stays at its initial 100; a tangential step of length D leaves h ~ D^2, and the
/// radius cycles 8 -> 16 -> 8 against the width. SOC off.
inline I19Fixture f_cyc(SqpHessianMode mode) {
    return i19_circle(100.0, 1e4, /*soc=*/false, 0.0, 200, mode);
}
/// F-H: h-type acceptances whose violation rises (the width contracts onto h) grow the radius on
/// the objective ratio; the doubled step lands above the width. SOC off. Pins G1.
inline I19Fixture f_h(SqpHessianMode mode) {
    return i19_circle(30.0, 1.0, /*soc=*/false, 0.0, 200, mode);
}
/// F-SOC: SOC-rescued acceptances grow the radius and the doubled trial is rejected at the
/// width. Default options. Pins G2.
inline I19Fixture f_soc(SqpHessianMode mode) {
    return i19_circle(100.0, 1.0, /*soc=*/true, 0.0, 200, mode);
}
/// F-CAP: caller_step_limit = 0.1 holds the step while the radius does not; cap-only steps grow
/// the radius, and a ceiling rejection then halves a radius the step never touched, re-solving
/// the identical QP. Default options otherwise. Pins G5/G6.
inline I19Fixture f_cap(SqpHessianMode mode) {
    return i19_circle(100.0, 1.0, /*soc=*/true, 0.1, 300, mode);
}

inline SqpResult i19_solve(const I19Fixture &f) {
    Problem p(f.model);
    SqpSolver s(f.options);
    return s.solve(p, f.x0);
}

/// What the tests read off a solve's depth-0 history.
struct I19RadiusReading {
    int growths = 0;            // accepted row k, radius[k+1] > radius[k]
    int grow_then_reject = 0;   // a growth followed by a rejected row
    int triplets = 0;           // r -> 2r -> r: accepted-grown, rejected, radius back to r
    int ceiling_like = 0;       // rejected rows (all causes)
    int h_increase_growths = 0; // growth on an h-type acceptance whose h rose, no SOC
    int soc_growths = 0;        // growth on a SOC-rescued acceptance
    int soc_grow_then_reject = 0;
    int cap_only_growths = 0;   // growth on a step held by the cap alone
    int unbound_rejections = 0; // rejected row whose step the radius did not hold
    int identical_resolves = 0; // rejected, then rejected again with the same unbound step
    int majors = 0;
};

inline I19RadiusReading i19_read(const SqpResult &r) {
    I19RadiusReading out;
    std::vector<const IterationRecord *> rows;
    for (const IterationRecord &h : r.history) {
        if (h.sqp && h.sqp->depth == 0) {
            rows.push_back(&h);
        }
    }
    out.majors = static_cast<int>(rows.size());
    const auto acc = [](const IterationRecord &h) {
        return h.sqp->verdict && (*h.sqp->verdict == StepVerdict::kAcceptObjectiveStep ||
                                  *h.sqp->verdict == StepVerdict::kAcceptFeasibilityStep);
    };
    const auto rej = [](const IterationRecord &h) {
        return h.sqp->verdict && *h.sqp->verdict == StepVerdict::kReject;
    };
    for (std::size_t k = 0; k < rows.size(); ++k) {
        const IterationRecord &h = *rows[k];
        if (rej(h)) {
            ++out.ceiling_like;
            if (!h.sqp->tr_binding) {
                ++out.unbound_rejections;
                if (k + 1 < rows.size() && rej(*rows[k + 1]) && !rows[k + 1]->sqp->tr_binding &&
                    rows[k + 1]->step_norm && h.step_norm &&
                    *rows[k + 1]->step_norm == *h.step_norm) {
                    ++out.identical_resolves;
                }
            }
        }
        if (k + 1 >= rows.size() || !acc(h)) {
            continue;
        }
        const double r0 = h.sqp->trust_radius;
        const double r1 = rows[k + 1]->sqp->trust_radius;
        if (!(r1 > r0)) {
            continue;
        }
        ++out.growths;
        const bool next_rej = rej(*rows[k + 1]);
        out.grow_then_reject += next_rej ? 1 : 0;
        if (next_rej && k + 2 < rows.size() && rows[k + 2]->sqp->trust_radius == r0) {
            ++out.triplets;
        }
        if (h.soc) {
            ++out.soc_growths;
            out.soc_grow_then_reject += next_rej ? 1 : 0;
        } else if (*h.sqp->verdict == StepVerdict::kAcceptFeasibilityStep && h.sqp->h_trial &&
                   *h.sqp->h_trial > h.sqp->violation_l1) {
            ++out.h_increase_growths;
        }
        if (h.sqp->cap_binding && !h.sqp->tr_binding) {
            ++out.cap_only_growths;
        }
    }
    return out;
}

} // namespace hven::test_support
