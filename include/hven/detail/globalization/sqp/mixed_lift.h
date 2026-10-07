// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <hven/core/types.h>
#include <hven/detail/globalization/sqp/lbfgs_damped.h>
#include <hven/detail/qp/qp_problem.h>

namespace hven::solvers::detail {

// The MIXED LIFT of a quasi-Newton Hessian into a QP the engines already solve (design
// m7-qn-design.md section 2; rulings C30 BLOCKING-1/BLOCKING-2, AMEND-6/8/9).
//
// Given a QpProblem in p (n variables, its H IGNORED and replaced) and
//     B = sigma I + Up diag(theta_p) Up^T - V V^T      (MixedLiftData, n_struct rows),
// the lifted QP lives in (p, u', w') -- n + r+ + r- variables, in that order:
//
//   objective   g^T p + 1/2 sigma ||p_s||^2 + 1/2 sum_k theta_k (kappa_k u'_k + c_k)^2
//               + 1/2 ||D w' + a||^2 - (D w' + a)^T V^T p_s,        D = diag(kappa_v)
//   rows        kappa_k u'_k - Up_k^T p_s = -c_k        (r+ equality rows, APPENDED to Ae)
//   bounds      the input's on p; u', w' free (+-inf)
//
// where p_s is the first n_struct entries of p (the variables B acts on; trailing columns, an
// elastic slack block, get zero rows in Up/V and zero curvature), p_ref = clamp(0, lower, upper),
// c = Up^T p_ref and a = V^T p_ref. The auxiliaries are SHIFTED and SCALED:
//
//   u'_k = Up_k^T (p - p_ref) / kappa_k       w'_j = V_j^T (p - p_ref) / kappa_j.
//
// This is a pure reparametrisation of u = Up^T p (equality-constrained) and w (free,
// minimised at w = V^T p), so eliminating the auxiliaries returns EXACTLY
// g^T p + 1/2 p^T B p (the dense-B oracle's objective). The lifted Hessian
//
//   [ sigma I    0          -kappa_w V ]
//   [ 0          kappa^2 Th  0         ]      (Th = diag(theta_p) > 0)
//   [ -kappa_w V^T  0        kappa_w^2 ]
//
// is positive definite on the structural block iff sigma I - V V^T > 0, which is B > 0 on
// span(V) -- B's own positive-definiteness; with it the walk's inertia gate reads kOk and the
// repair / saddle machinery is dormant unconditionally (BLOCKING-2). Upper triangle only.
//
// WHY THE SCALES (BLOCKING-1). Every engine boxes EVERY variable, auxiliaries included, about
// its start: |x_i - centre_i| <= Delta. For a step with ||p - p_ref||_inf <= Delta the shifted
// auxiliary satisfies |u'_k| <= ||Up_k||_1 Delta / kappa_k <= Delta/2 when kappa_k >= 2
// ||Up_k||_1 (likewise w'_j with kappa_j >= 2 ||V_j||_1), so the window never binds an
// auxiliary: tr_active is false on every lifted index. The shift is what lets the SAME lifted
// problem start from any engine's centre: the walk (cold: clamp(0); warm: the clamped seed),
// the interior tier (always clamp(0)) and SSN (its start, or its box_center) all centre the
// auxiliaries on the value consistent with p there -- cold p_ref gives auxiliary 0, and a warm
// start is lifted by lift_primal/lift_seed to the matching auxiliary values. (SSN cold with
// 0 outside [lower, upper] centres on the unclamped origin and is outside this contract; every
// driver-produced subproblem has 0 inside its box.)
//
// THE PATTERN IS STABLE (AMEND-8): Up and V are stored structurally dense over the n_struct
// leading columns, so the lifted pattern depends only on (n, me, mi, n_struct, r+, r-), never on
// the values of the current B.
//
// ORDER: the lift is the LAST transform before the engine call -- after elastic augmentation
// and after the equality-basis reduction. Un-lift drops the auxiliaries and the r+ lift-row
// multipliers.

class MixedLift {
  public:
    /// @param qp       the QP in p (validated); its H is ignored.
    /// @param n_struct leading variables B acts on, 0 <= n_struct <= qp.n(). Up and V must have
    ///                 n_struct rows (when they have columns).
    /// @param data     sigma > 0 finite, theta_plus > 0 finite, kappa_i finite and
    ///                 >= 2 ||column_i||_1 (a tolerance of 1e-12 relative).
    /// @throws std::invalid_argument on any violation.
    MixedLift(const QpProblem &qp, Index n_struct, const MixedLiftData &data);

    const QpProblem &problem() const { return lifted_; }

    Index n_orig() const { return n_orig_; }
    Index n_lifted() const { return lifted_.n(); }
    Index num_plus() const { return r_plus_; }
    Index num_minus() const { return r_minus_; }
    Index orig_eq_rows() const { return me_orig_; }
    /// clamp(0, lower, upper) of the input QP: the reference the auxiliaries are measured from.
    const Vec &reference() const { return p_ref_; }

    /// Auxiliary values consistent with p (clamped into [lower, upper] first, as the walk's
    /// start does): the lifted point (p_c, u', w'). p has n_orig entries.
    Vec lift_primal(const Vec &p) const;

    /// The lifted warm seed: x lifted (when sized), bound_state / tr_active extended with
    /// kFree / false, lambda_e and z extended with zeros, the rest carried. A field that is
    /// empty stays empty (the engine reads it as absent); a field of any other wrong size throws.
    QpSolution lift_seed(const QpSolution &seed) const;

    /// Drop auxiliaries and lift-row multipliers from a lifted solution; x, z, bound_state,
    /// tr_active, lambda_e and the Farkas certificate (y_e, box_part) return to the unlifted
    /// sizes. A bound-blocker certificate whose index fell on an auxiliary (never, under
    /// BLOCKING-1) is reported absent. Throws on a wrongly sized lifted solution.
    QpSolution unlift(const QpSolution &lifted) const;

    /// The component un-lifts, for engines whose results are not QpSolution (interior tier,
    /// SSN): the leading n_orig entries / me_orig entries.
    Vec unlift_primal(const Vec &x_lifted) const;
    Vec unlift_equality_multipliers(const Vec &lambda_e_lifted) const;

  private:
    QpProblem lifted_;
    Vec p_ref_;
    Eigen::MatrixXd u_plus_, v_;
    Vec kappa_plus_, kappa_v_;
    Index n_orig_ = 0, n_struct_ = 0, me_orig_ = 0, r_plus_ = 0, r_minus_ = 0;
};

} // namespace hven::solvers::detail
