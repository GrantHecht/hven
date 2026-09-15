// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// ipqp_math.h -- the interior-point QP tier's per-element barrier kernels.
//
// QP-shaped RE-DERIVATIONS of interior/barrier_math.h's kernels, each naming the one
// it mirrors; header-inline. PRECONDITION: callers keep x STRICTLY inside these bounds,
// no kernel re-checks. Reused vs re-derived vs deliberately NOT shared: spec section 3.5.

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <Eigen/Core>

#include "hven/core/types.h"

namespace hven::solvers::detail {

/// @brief Bounds at or beyond this magnitude are ABSENT and carry no barrier
/// term.
///
/// Same value and meaning as qp_engine.h's `kEngineInfBound`, ssn_engine.h's
/// `kSsnInfBound`, and the +/-1e20 sentinel qp_problem.h documents; restated so this
/// header depends on the QP data convention only, not on either engine's internals.
inline constexpr double kIpqpInfBound = 1e20;

/// @brief Ipopt's bound-damping coefficient, applied on ONE-SIDED bounds only.
///
/// Must equal bound_set.h's `kKappaD` (Ipopt's kappa_d = 1e-5), restated here for the
/// same reason `kIpqpInfBound` is. The damping lives in the barrier OBJECTIVE and its
/// mu-form gradient ONLY -- never in the dual-infeasibility residual, never in sigma.
inline constexpr double kIpqpKappaD = 1.0e-5;

/// @brief True iff `l` is a bound the tier must keep a barrier term for.
inline bool ipqp_has_lower(double l) { return l > -kIpqpInfBound; }

/// @brief True iff `u` is a bound the tier must keep a barrier term for.
inline bool ipqp_has_upper(double u) { return u < kIpqpInfBound; }

/// @brief MIRRORS `bound_barrier_objective` (barrier_math.h). QP shape: dense
/// lower/upper vectors with the +/-1e20 absent sentinel, in place of a
/// BoundSet's index/value lists.
///
/// -mu * [sum ln(x_i - l_i) + sum ln(u_i - x_i)] over the finite bounds, plus Ipopt's
/// one-sided damping kappa_d * mu * distance. ACCUMULATION ORDER is part of the mirror,
/// not an implementation detail: all lower terms ascending, then all upper terms.
inline double ipqp_bound_barrier_objective(const Eigen::Ref<const Eigen::VectorXd> &x,
                                           const Eigen::Ref<const Eigen::VectorXd> &l,
                                           const Eigen::Ref<const Eigen::VectorXd> &u, double mu,
                                           Index n) {
    double psi = 0.0;
    for (Index i = 0; i < n; ++i) {
        if (ipqp_has_lower(l[i])) {
            const double d = x[i] - l[i];
            const double damp = ipqp_has_upper(u[i]) ? 0.0 : 1.0;
            psi += -mu * std::log(d) + kIpqpKappaD * mu * damp * d;
        }
    }
    for (Index i = 0; i < n; ++i) {
        if (ipqp_has_upper(u[i])) {
            const double d = u[i] - x[i];
            const double damp = ipqp_has_lower(l[i]) ? 0.0 : 1.0;
            psi += -mu * std::log(d) + kIpqpKappaD * mu * damp * d;
        }
    }
    return psi;
}

/// @brief MIRRORS `accumulate_bound_barrier_gradient` (barrier_math.h). Same
/// QP shape substitution as above.
///
/// mu-FORM primal gradient terms: gx_i += -mu/(x_i-l_i), += +mu/(u_i-x_i), plus the
/// damping derivative. This is what the CONDENSED NEWTON RHS carries; NEVER use it for
/// a residual a convergence test consumes -- see the z-form below.
inline void ipqp_accumulate_bound_barrier_gradient(const Eigen::Ref<const Eigen::VectorXd> &x,
                                                   const Eigen::Ref<const Eigen::VectorXd> &l,
                                                   const Eigen::Ref<const Eigen::VectorXd> &u,
                                                   double mu, Index n,
                                                   Eigen::Ref<Eigen::VectorXd> gx) {
    // Lowers then uppers, as above: one bound order across every kernel in this file.
    for (Index i = 0; i < n; ++i) {
        if (ipqp_has_lower(l[i])) {
            const double damp = ipqp_has_upper(u[i]) ? 0.0 : 1.0;
            gx[i] += -mu / (x[i] - l[i]) + kIpqpKappaD * mu * damp;
        }
    }
    for (Index i = 0; i < n; ++i) {
        if (ipqp_has_upper(u[i])) {
            const double damp = ipqp_has_lower(l[i]) ? 0.0 : 1.0;
            gx[i] += mu / (u[i] - x[i]) - kIpqpKappaD * mu * damp;
        }
    }
}

/// @brief MIRRORS `accumulate_bound_dual_terms` (barrier_math.h). QP shape:
/// dense per-variable multiplier vectors in place of a BoundDualState's two
/// compact arrays.
///
/// z-FORM primal gradient: gx_i += -zL_i, += +zU_i -- the DUAL INFEASIBILITY term the
/// convergence check consumes, undamped. TWO GUARDED LOOPS, never one fused subtraction
/// (rounding, and the absent-bound guard): `.superpowers/w1-t3-report.md`.
inline void ipqp_accumulate_bound_dual_terms(const Eigen::Ref<const Eigen::VectorXd> &l,
                                             const Eigen::Ref<const Eigen::VectorXd> &u,
                                             const Eigen::Ref<const Eigen::VectorXd> &zl,
                                             const Eigen::Ref<const Eigen::VectorXd> &zu, Index n,
                                             Eigen::Ref<Eigen::VectorXd> gx) {
    for (Index i = 0; i < n; ++i) {
        if (ipqp_has_lower(l[i])) {
            gx[i] += -zl[i];
        }
    }
    for (Index i = 0; i < n; ++i) {
        if (ipqp_has_upper(u[i])) {
            gx[i] += zu[i];
        }
    }
}

/// @brief MIRRORS `accumulate_bound_sigma` (barrier_math.h). Same QP shape
/// substitution as above.
///
/// Condensed bound curvature: sigma_i += zL_i/(x_i - l_i) + zU_i/(u_i - x_i) -- the Sigma
/// that eliminating the bound-multiplier rows leaves on the section 3.1 system's (1,1)
/// diagonal. ACCUMULATED onto the caller's base, so it composes with rho; grows no rows.
inline void ipqp_accumulate_bound_sigma(const Eigen::Ref<const Eigen::VectorXd> &x,
                                        const Eigen::Ref<const Eigen::VectorXd> &l,
                                        const Eigen::Ref<const Eigen::VectorXd> &u,
                                        const Eigen::Ref<const Eigen::VectorXd> &zl,
                                        const Eigen::Ref<const Eigen::VectorXd> &zu, Index n,
                                        Eigen::Ref<Eigen::VectorXd> sigma) {
    for (Index i = 0; i < n; ++i) {
        if (ipqp_has_lower(l[i])) {
            sigma[i] += zl[i] / (x[i] - l[i]);
        }
    }
    for (Index i = 0; i < n; ++i) {
        if (ipqp_has_upper(u[i])) {
            sigma[i] += zu[i] / (u[i] - x[i]);
        }
    }
}

/// @brief The SECTION 2.2 ITEM 4 FINAL READ'S bound curvature: the same
/// accumulation with the WEAKLY ACTIVE sides left out.
///
/// The item 4 read is stated on the CRITICAL CONE, so a WEAKLY ACTIVE side -- `z` AND
/// `gap` both small against `weak_scale * sqrt(mu)`, per side -- drops its curvature;
/// `weak_scale <= 0` reproduces the kernel above. See `.superpowers/w1-t4b-report.md`.
/// @param weak_scale The activity scale (a multiple of `sqrt(mu)`); `<= 0`
/// disables the rule entirely.
/// @param band_upper T4c disclosure BAND ceiling (`kIpqpTightBandFactor *
/// weak_scale`; `<= 0` disables it). A KEPT side is band-counted iff
/// `weak_scale < z <= band_upper`. See `.superpowers/w1-t4c-report.md`.
/// @param band_lower_idx / @param band_upper_idx receive the index of each
/// band-counted lower/upper side (`nullptr` to skip).
inline void ipqp_accumulate_bound_sigma_critical_cone(
    const Eigen::Ref<const Eigen::VectorXd> &x, const Eigen::Ref<const Eigen::VectorXd> &l,
    const Eigen::Ref<const Eigen::VectorXd> &u, const Eigen::Ref<const Eigen::VectorXd> &zl,
    const Eigen::Ref<const Eigen::VectorXd> &zu, Index n, double weak_scale,
    Eigen::Ref<Eigen::VectorXd> sigma, double band_upper = 0.0,
    std::vector<Index> *band_lower_idx = nullptr, std::vector<Index> *band_upper_idx = nullptr) {
    if (!(weak_scale > 0.0)) {
        ipqp_accumulate_bound_sigma(x, l, u, zl, zu, n, sigma);
        return;
    }
    for (Index i = 0; i < n; ++i) {
        if (ipqp_has_lower(l[i])) {
            const double gap = x[i] - l[i];
            if (!(gap <= weak_scale && zl[i] <= weak_scale)) {
                sigma[i] += zl[i] / gap;
                if (band_upper > 0.0 && band_lower_idx != nullptr && zl[i] > weak_scale &&
                    zl[i] <= band_upper) {
                    band_lower_idx->push_back(i);
                }
            }
        }
    }
    for (Index i = 0; i < n; ++i) {
        if (ipqp_has_upper(u[i])) {
            const double gap = u[i] - x[i];
            if (!(gap <= weak_scale && zu[i] <= weak_scale)) {
                sigma[i] += zu[i] / gap;
                if (band_upper > 0.0 && band_upper_idx != nullptr && zu[i] > weak_scale &&
                    zu[i] <= band_upper) {
                    band_upper_idx->push_back(i);
                }
            }
        }
    }
}

/// @brief T4c: the exponent test, a pure per-side function of the last two ACCEPTED
/// iterates. `kUninformative` on non-finite/non-positive input, `z_prev == 0`, an
/// underflowed ratio, or a mu ratio too close to 1. See `.superpowers/w1-t4c-report.md`.
enum class IpqpBarrierNoiseClass { kUninformative, kPriced, kSuspect };

struct IpqpBarrierNoiseVerdict {
    IpqpBarrierNoiseClass cls = IpqpBarrierNoiseClass::kUninformative;
    double exponent = std::numeric_limits<double>::quiet_NaN();
};

inline IpqpBarrierNoiseVerdict ipqp_classify_barrier_noise(double z_prev, double z_cur,
                                                           double mu_prev, double mu_cur) {
    const bool finite = std::isfinite(z_prev) && std::isfinite(z_cur) && std::isfinite(mu_prev) &&
                        std::isfinite(mu_cur);
    if (!finite || !(z_prev > 0.0) || !(z_cur > 0.0) || !(mu_prev > 0.0) || !(mu_cur > 0.0)) {
        return {};
    }
    const double z_ratio = z_cur / z_prev;
    const double mu_ratio = mu_cur / mu_prev;
    // ROUND 3: `> 0.0` rather than `>= 0.0`, so an underflowed ratio (finite
    // 0) is rejected here rather than reaching `log` and producing a finite
    // but meaningless `e` (e.g. `-0`) that would misclassify as `kPriced`.
    if (!(z_ratio > 0.0) || !(mu_ratio > 0.0) || !(mu_ratio <= 0.5)) {
        return {};
    }
    const double e = std::log(z_ratio) / std::log(mu_ratio);
    if (!std::isfinite(e)) {
        return {};
    }
    return {e >= 0.5 ? IpqpBarrierNoiseClass::kSuspect : IpqpBarrierNoiseClass::kPriced, e};
}

/// @brief Folds per-side verdicts into the flag. An UNINFORMATIVE side is ambiguous,
/// never cleared -- it fires the flag; uninformative history falls back to the band
/// count alone. See `.superpowers/w1-t4c-report.md`.
inline bool ipqp_barrier_noise_flag(bool informative, Index noise_count,
                                    bool any_side_uninformative, Index band_count) {
    return informative ? (noise_count > 0 || any_side_uninformative) : (band_count > 0);
}

/// @brief The tier's OWN slack/multiplier complementarity reduction, DELIBERATELY not
/// shared with the NLP engine (spec section 3.5). Ascending index, one pair at a time,
/// one accumulator; `avgcomp` = mean pair value, `mincomp`/`maxcomp` = extremes; 0 -> 0.
inline void ipqp_slack_complementarity(const Eigen::Ref<const Eigen::VectorXd> &s,
                                       const Eigen::Ref<const Eigen::VectorXd> &lam, Index mi,
                                       double &avgcomp, double &mincomp, double &maxcomp) {
    avgcomp = 0.0;
    mincomp = 0.0;
    maxcomp = 0.0;
    if (mi <= 0) {
        return;
    }
    double sum = s[0] * lam[0];
    double lo = sum;
    double hi = sum;
    for (Index j = 1; j < mi; ++j) {
        const double pair = s[j] * lam[j];
        sum += pair;
        lo = std::min(lo, pair);
        hi = std::max(hi, pair);
    }
    avgcomp = sum / static_cast<double>(mi);
    mincomp = lo;
    maxcomp = hi;
}

/// @brief MIRRORS `augment_bound_complementarity` (barrier_math.h). Same QP
/// shape substitution as the other bound kernels.
///
/// Folds the variable-bound complementarity pairs into aggregates already reduced over
/// the slack/multiplier pairs, IN PLACE -- the base aggregates are never re-reduced.
/// `base_count` = pairs behind `avgcomp`; union avg count-weighted; no bounds -> no-op.
inline void ipqp_augment_bound_complementarity(const Eigen::Ref<const Eigen::VectorXd> &x,
                                               const Eigen::Ref<const Eigen::VectorXd> &l,
                                               const Eigen::Ref<const Eigen::VectorXd> &u,
                                               const Eigen::Ref<const Eigen::VectorXd> &zl,
                                               const Eigen::Ref<const Eigen::VectorXd> &zu, Index n,
                                               Index base_count, double &avgcomp, double &mincomp,
                                               double &maxcomp) {
    double bsum = 0.0;
    double bmin = 0.0;
    double bmax = 0.0;
    Index bcount = 0;
    auto fold = [&](double pair) {
        bsum += pair;
        if (bcount == 0) {
            bmin = pair;
            bmax = pair;
        } else {
            bmin = std::min(bmin, pair);
            bmax = std::max(bmax, pair);
        }
        ++bcount;
    };
    // Lowers first, then uppers, ascending index -- the NLP kernel's own order,
    // kept so the two folds agree bit-for-bit on the same data.
    for (Index i = 0; i < n; ++i) {
        if (ipqp_has_lower(l[i])) {
            fold((x[i] - l[i]) * zl[i]);
        }
    }
    for (Index i = 0; i < n; ++i) {
        if (ipqp_has_upper(u[i])) {
            fold((u[i] - x[i]) * zu[i]);
        }
    }
    if (bcount == 0) {
        return;
    }

    if (base_count > 0) {
        mincomp = std::min(mincomp, bmin);
        maxcomp = std::max(maxcomp, bmax);
        avgcomp = (avgcomp * static_cast<double>(base_count) + bsum) /
                  static_cast<double>(base_count + bcount);
    } else {
        mincomp = bmin;
        maxcomp = bmax;
        avgcomp = bsum / static_cast<double>(bcount);
    }
}

} // namespace hven::solvers::detail
