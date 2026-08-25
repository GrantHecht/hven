// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

// Three tiny pure kernels the barrier machinery evaluates everywhere: the
// slack reset that completes a raw inequality residual, the log-barrier
// objective, and its dual gradient -- each a one-line forwarder target for the
// components that share them, so the arithmetic exists once. The four bound
// kernels at the bottom are the same idea for barrier terms on PRIMAL VARIABLE
// BOUNDS; they walk a BoundSet (reduced-space indices), so a problem with no
// variable bounds gives them no trip count and every caller guards on an empty
// set anyway. Two of the gradient accumulators look interchangeable and are
// NOT -- see the mu-form / z-form notes below.
//
// Callout: the solver's complementarity reduction is deliberately NOT here.
// Its .sum() feeds mu, so any change to how that sum is formed can move
// iterates by a ULP under fast-math; unifying it needs its own evidence.

#pragma once

#include <algorithm>
#include <cmath>

#include <Eigen/Core>

#include "hven/detail/interior/bound_set.h"
#include "hven/detail/interior/typedefs/eigen_types.h"

namespace hven::solvers::detail {

/// @brief Completes the raw inequality residual g(x) into g(x) + s: a slack
/// below neg_slack_reset is clamped to that floor for the purpose of the sum
/// (S[i] itself is left untouched on this branch); a negative residual
/// instead zeroes the residual and resets S[i] to max(|fx|, neg_slack_reset).
inline void apply_reset_slacks(Eigen::Ref<Eigen::VectorXd> S, Eigen::Ref<Eigen::VectorXd> FXI,
                               int slack_vars, double neg_slack_reset) {
    for (int i = 0; i < slack_vars; i++) {
        double fxi = FXI[i];
        double si = S[i];
        if (si < neg_slack_reset) {
            si = neg_slack_reset;
        }

        if (fxi < 0.0) {
            FXI[i] = 0.0;
            S[i] = std::max(std::abs(fxi), neg_slack_reset);
        } else {
            FXI[i] += si;
        }
    }
}

/// @brief Fraction-to-boundary step for one block of strictly-positive
/// quantities: the largest alpha in (0, 1] with SLI + alpha*dSLI >=
/// (1-bfrac)*SLI componentwise. count is passed explicitly because the blocks
/// served have different lengths (slacks/multipliers, bound distances, bound
/// multipliers, restoration re-centring).
inline double max_step_to_boundary(Eigen::Ref<Eigen::VectorXd> SLI,
                                   Eigen::Ref<Eigen::VectorXd> dSLI, double bfrac, int count) {
    double alpha = 1.0;
    for (int i = 0; i < count; i++) {
        if (dSLI[i] < -bfrac * SLI[i]) {
            double an = -bfrac * SLI[i] / dSLI[i];
            if (an < alpha)
                alpha = an;
        }
    }
    return alpha;
}

/// @brief Log-barrier objective -mu * sum(log s_i) over the first inequal_cons slacks.
inline double barrier_objective(Eigen::Ref<Eigen::VectorXd> S, double mu, int inequal_cons) {
    double psi = 0;
    for (int i = 0; i < inequal_cons; i++) {
        psi += -mu * std::log(S[i]);
    }
    return psi;
}

/// @brief Dual gradient of the log-barrier objective: lambda_i - mu/s_i,
/// written into the KKT vector's dual-gradient block.
inline void barrier_gradient(Eigen::Ref<Eigen::VectorXd> S, Eigen::Ref<Eigen::VectorXd> LI,
                             double mu, Eigen::Ref<Eigen::VectorXd> AGS) {
    AGS = LI - mu * (S.cwiseInverse());
}

// Primal variable-bound barrier kernels: for the bounds recorded in b
// (reduced-space indices; a two-sided variable appears in both lists) the
// barrier objective is phi_mu(x) = f(x) - mu*sum ln(x_i-l_i) -
// mu*sum ln(u_i-x_i). Invariant: every caller keeps x STRICTLY inside the
// recorded bounds (the solve-entry interior push and the fraction-to-boundary
// rule guarantee it); none of these kernels re-checks.

/// @brief Bound-set log-barrier objective: -mu * [sum ln(x_i-l_i) +
/// sum ln(u_i-x_i)], plus the one-sided damping term kappa_d * mu *
/// sum(distance) over entries whose variable is bounded on that side only.
///
/// The damping is Ipopt's (CalcBarrierTerm adds kappa_d * mu * slack.dot(dampind)
/// per side). Without it a variable with only one finite bound has nothing to
/// push back against in its unbounded direction and a barrier subproblem can
/// drive it arbitrarily far out. It belongs to the barrier OBJECTIVE and its
/// mu-form gradient only -- see kKappaD's note in bound_set.h for the seams it
/// must stay out of.
inline double bound_barrier_objective(ConstEigenRef<Eigen::VectorXd> x, const BoundSet &b,
                                      double mu) {
    const int nl = static_cast<int>(b.lower_idx_.size());
    const int nu = static_cast<int>(b.upper_idx_.size());
    double psi = 0.0;
    for (int k = 0; k < nl; k++) {
        const double d = x[b.lower_idx_[k]] - b.lower_val_[k];
        psi += -mu * std::log(d) + kKappaD * mu * b.lower_damp_[k] * d;
    }
    for (int k = 0; k < nu; k++) {
        const double d = b.upper_val_[k] - x[b.upper_idx_[k]];
        psi += -mu * std::log(d) + kKappaD * mu * b.upper_damp_[k] * d;
    }
    return psi;
}

/// @brief mu-FORM primal gradient terms: gx_i += -mu/(x_i-l_i), +=
/// +mu/(u_i-x_i), plus the damping derivative (+/-kappa_d*mu on single-sided
/// entries). This is the gradient of the barrier objective above and what the
/// CONDENSED NEWTON RIGHT-HAND SIDE carries: eliminating the bound-multiplier
/// rows turns the primal RHS into grad phi_mu + J'lambda (the -z_L+z_U cancels
/// against the substituted multiplier steps). Ipopt keeps the same split,
/// damping and all. Never use this form for a residual a convergence test
/// consumes.
inline void accumulate_bound_barrier_gradient(ConstEigenRef<Eigen::VectorXd> x, const BoundSet &b,
                                              double mu, EigenRef<Eigen::VectorXd> gx) {
    const int nl = static_cast<int>(b.lower_idx_.size());
    const int nu = static_cast<int>(b.upper_idx_.size());
    for (int k = 0; k < nl; k++) {
        const int i = b.lower_idx_[k];
        gx[i] += -mu / (x[i] - b.lower_val_[k]) + kKappaD * mu * b.lower_damp_[k];
    }
    for (int k = 0; k < nu; k++) {
        const int i = b.upper_idx_[k];
        gx[i] += mu / (b.upper_val_[k] - x[i]) - kKappaD * mu * b.upper_damp_[k];
    }
}

/// @brief z-FORM primal gradient terms: gx_i += -zL_i, += +zU_i. This is the
/// DUAL INFEASIBILITY contribution -- the residual whose norm the convergence
/// check consumes. Agrees with the mu-form only where bound complementarity
/// holds exactly (z_L = mu/(x-l), z_U = mu/(u-x)); away from the central path
/// they differ, which is the whole reason both exist. Never use this form for
/// a Newton right-hand side.
inline void accumulate_bound_dual_terms(const BoundSet &b, const BoundDualState &z,
                                        EigenRef<Eigen::VectorXd> gx) {
    const int nl = static_cast<int>(b.lower_idx_.size());
    const int nu = static_cast<int>(b.upper_idx_.size());
    for (int k = 0; k < nl; k++) {
        gx[b.lower_idx_[k]] += -z.z_lower_[k];
    }
    for (int k = 0; k < nu; k++) {
        gx[b.upper_idx_[k]] += z.z_upper_[k];
    }
}

/// @brief Folds the variable-bound complementarity pairs (x_i-l_i)*zL_i and
/// (u_i-x_i)*zU_i into aggregates already reduced over the inequality
/// slack/multiplier pairs, in place. base_count is how many pairs were reduced
/// into avgcomp (their sum reconstructs as avgcomp*base_count). The base
/// aggregates are NOT re-reduced, so the exact Eigen reduction that produced
/// them -- whose ordering feeds mu and is ULP-load-bearing under fast-math --
/// survives untouched; union min/max are min-of-mins/max-of-maxes, the union
/// average count-weighted. A problem with no bounded variables never reaches
/// this (callers guard on non-empty sets).
inline void augment_bound_complementarity(ConstEigenRef<Eigen::VectorXd> x, const BoundSet &b,
                                          const BoundDualState &z, int base_count, double &avgcomp,
                                          double &mincomp, double &maxcomp) {
    const int nl = static_cast<int>(b.lower_idx_.size());
    const int nu = static_cast<int>(b.upper_idx_.size());
    if (nl + nu == 0)
        return;

    double bsum = 0.0;
    double bmin = 0.0;
    double bmax = 0.0;
    bool first = true;
    auto fold = [&](double pair) {
        bsum += pair;
        if (first) {
            bmin = pair;
            bmax = pair;
            first = false;
        } else {
            bmin = std::min(bmin, pair);
            bmax = std::max(bmax, pair);
        }
    };
    for (int k = 0; k < nl; k++) {
        fold((x[b.lower_idx_[k]] - b.lower_val_[k]) * z.z_lower_[k]);
    }
    for (int k = 0; k < nu; k++) {
        fold((b.upper_val_[k] - x[b.upper_idx_[k]]) * z.z_upper_[k]);
    }

    const int bcount = nl + nu;
    if (base_count > 0) {
        mincomp = std::min(mincomp, bmin);
        maxcomp = std::max(maxcomp, bmax);
        avgcomp = (avgcomp * double(base_count) + bsum) / double(base_count + bcount);
    } else {
        mincomp = bmin;
        maxcomp = bmax;
        avgcomp = bsum / double(bcount);
    }
}

// Condensed bound curvature: sigma_i += zL_i/(x_i - l_i) and += zU_i/(u_i - x_i).
//
// The Sigma that eliminating the bound-multiplier rows leaves on the Hessian
// (1,1) diagonal. Accumulated onto whatever base the solver already writes to
// the primal-diagonal slots, so it composes with the restoration and proximal
// bases instead of replacing them, and it does not grow the KKT system.
inline void accumulate_bound_sigma(ConstEigenRef<Eigen::VectorXd> x, const BoundSet &b,
                                   const BoundDualState &z, EigenRef<Eigen::VectorXd> sigma) {
    const int nl = static_cast<int>(b.lower_idx_.size());
    const int nu = static_cast<int>(b.upper_idx_.size());
    for (int k = 0; k < nl; k++) {
        const int i = b.lower_idx_[k];
        sigma[i] += z.z_lower_[k] / (x[i] - b.lower_val_[k]);
    }
    for (int k = 0; k < nu; k++) {
        const int i = b.upper_idx_[k];
        sigma[i] += z.z_upper_[k] / (b.upper_val_[k] - x[i]);
    }
}

} // namespace hven::solvers::detail
