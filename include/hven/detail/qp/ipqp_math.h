// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// ipqp_math.h -- the interior-point QP tier's per-element barrier kernels.
//
// WHAT THIS FILE IS. The tier's algorithm is the NLP interior engine's, so its
// arithmetic already exists once, in
// include/hven/detail/interior/barrier_math.h. What does NOT carry over is the
// SHAPE the NLP kernels are written against: their variable-bound kernels walk
// a `BoundSet` (bound_set.h) -- REDUCED-SPACE indices, RELAXED bound values,
// fixed variables already eliminated, damping indicators materialized by the
// NLP's own classifier -- none of which a `QpProblem` has. QpProblem states its
// bounds as two DENSE n-vectors with a +/-1e20 absent sentinel, and the tier
// iterates in the QP's own space with nothing eliminated.
//
// So each kernel below is a QP-shaped RE-DERIVATION and NAMES the
// barrier_math.h kernel it mirrors, so that a future divergence between the
// two is visible in the diff rather than discovered numerically.
//
// WHAT IS NOT HERE, AND WHY.
//
//   * `barrier_objective` and `barrier_gradient` (barrier_math.h) are called
//     DIRECTLY by the tier and are deliberately absent from this file. Their
//     shape lines up exactly -- a slack vector, a multiplier vector, mu and a
//     count -- so re-deriving them would create a second copy of arithmetic
//     that has no reason to diverge. The section 3.5 reuse ledger's rule is
//     "where a shape lines up the tier calls the existing kernel"; this is
//     that case.
//   * `detail::max_step_to_boundary` (barrier_math.h:62) is likewise reused
//     verbatim -- the ledger lists it under Verbatim, not Specialized.
//   * The COMPLEMENTARITY REDUCTION is the opposite case: it is written here,
//     locally, and deliberately NOT shared. barrier_math.h's own banner warns
//     that the solver's complementarity reduction is kept out of that file
//     because its `.sum()` order feeds mu and is ULP-load-bearing under
//     fast-math. Sharing one reduction between the NLP engine and this tier
//     would tie the NLP engine's mu -- and therefore every pinned NLP counter
//     and baseline -- to the tier's own accumulation order. The tier pays for
//     its own loop instead.
//
// TU PLACEMENT (settler ruling Q-S2): header-inline, no .cpp. Every function
// here is a per-element hot loop the tier runs once or more per iteration, and
// CLAUDE.md section 5's criterion for staying in a header is exactly that --
// code whose speed depends on inlining at the call site. The
// declarations-only header budget binds the tier's engine header, not this
// one.
//
// INVARIANT, shared with barrier_math.h: every caller keeps x STRICTLY inside
// the bounds recorded here (the tier's interior push and its
// fraction-to-boundary rule guarantee it). None of these kernels re-checks,
// and a variable sitting exactly on a finite bound divides by zero here just
// as it would in the NLP engine.

#include <algorithm>
#include <cmath>
#include <vector>

#include <Eigen/Core>

#include "hven/core/types.h"

namespace hven::solvers::detail {

/// @brief Bounds at or beyond this magnitude are ABSENT and carry no barrier
/// term.
///
/// Identical value and meaning to qp_engine.h's `kEngineInfBound`, to
/// ssn_engine.h's `kSsnInfBound`, and to the +/-1e20 sentinel qp_problem.h
/// documents; restated rather than reached for, so this header depends on the
/// QP data convention only and not on either engine's internals -- the same
/// choice, for the same reason, that ssn_engine.h records at its own copy.
inline constexpr double kIpqpInfBound = 1e20;

/// @brief Ipopt's bound-damping coefficient, applied on ONE-SIDED bounds only.
///
/// Must equal bound_set.h's `kKappaD`, and is restated here for the same
/// reason `kIpqpInfBound` is: this header stays free of the NLP-side
/// reduced-space bound types. The two are the same constant in Ipopt's own
/// formulation (kappa_d = 1e-5), and the damping lives in the barrier
/// OBJECTIVE and its mu-form gradient ONLY -- never in the dual-infeasibility
/// residual and never in sigma, so the tier's convergence account and its
/// condensed curvature stay undamped exactly as the NLP engine's do.
inline constexpr double kIpqpKappaD = 1.0e-5;

/// @brief True iff `l` is a bound the tier must keep a barrier term for.
inline bool ipqp_has_lower(double l) { return l > -kIpqpInfBound; }

/// @brief True iff `u` is a bound the tier must keep a barrier term for.
inline bool ipqp_has_upper(double u) { return u < kIpqpInfBound; }

/// @brief MIRRORS `bound_barrier_objective` (barrier_math.h). QP shape: dense
/// lower/upper vectors with the +/-1e20 absent sentinel, in place of a
/// BoundSet's index/value lists.
///
/// -mu * [sum ln(x_i - l_i) + sum ln(u_i - x_i)] over the finite bounds, plus
/// the one-sided damping term kappa_d * mu * distance on each variable bounded
/// on that side ONLY. The damping is what keeps a variable with a single
/// finite bound from being driven arbitrarily far out in its unbounded
/// direction; it is Ipopt's `CalcBarrierTerm` term and the NLP engine carries
/// the identical one.
///
/// The damping indicator is computed inline from the pair (l_i, u_i) rather
/// than read from a materialized array: the QP shape still HAS both endpoints
/// of a variable in hand at the point of use, which is precisely the reason
/// BoundSet had to materialize its own (its two lists no longer pair them).
///
/// ACCUMULATION ORDER is part of the mirror, not an implementation detail: all
/// lower-bound terms first, in ascending variable index, then all upper-bound
/// terms. That is exactly the order the NLP kernel walks its two lists in, so
/// the two agree to the last bit on the same data rather than merely to a
/// tolerance -- which is what makes "mirrors" a testable claim.
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
/// mu-FORM primal gradient terms: gx_i += -mu/(x_i-l_i), += +mu/(u_i-x_i),
/// plus the damping derivative (+/- kappa_d * mu on single-sided entries).
/// This is the gradient of the objective above and what the CONDENSED NEWTON
/// RIGHT-HAND SIDE carries. Never use this form for a residual a convergence
/// test consumes -- see the z-form below, and the NLP kernel's own note on why
/// the two exist.
inline void ipqp_accumulate_bound_barrier_gradient(const Eigen::Ref<const Eigen::VectorXd> &x,
                                                   const Eigen::Ref<const Eigen::VectorXd> &l,
                                                   const Eigen::Ref<const Eigen::VectorXd> &u,
                                                   double mu, Index n,
                                                   Eigen::Ref<Eigen::VectorXd> gx) {
    // Lowers then uppers, as above. Here the split is not strictly required --
    // the two terms of a two-sided variable land on the same accumulator in
    // the same relative order either way -- but it keeps every kernel in this
    // file walking its bounds in one order.
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
/// z-FORM primal gradient terms: gx_i += -zL_i, += +zU_i. This is the DUAL
/// INFEASIBILITY contribution -- the residual whose norm the convergence check
/// consumes. It agrees with the mu-form above only where bound complementarity
/// holds exactly; away from the central path they differ, which is why both
/// exist. Undamped, deliberately.
///
/// TWO GUARDED LOOPS, NOT ONE FUSED SUBTRACTION, on two independent grounds.
///
/// FIRST, rounding. `gx += (zU - zL)` is not `(gx + (-zL)) + zU`: with
/// gx = 1e16, zL = 1e16, zU = 1 the mirror returns 1 and the fused form
/// returns 0, because 1 - 1e16 rounds back to -1e16. A dual-residual norm
/// built from this kernel feeds a convergence decision, so that is a
/// difference that can change an answer, not only a last digit. The mirror's
/// order -- all lowers, then all uppers, ascending index -- is reproduced
/// exactly.
///
/// SECOND, the absent-bound guard. The NLP kernel is structurally immune to a
/// stale multiplier: it walks the two index lists, so an unbounded variable is
/// unreachable. A dense loop over all n has no such immunity, and this kernel
/// is the one whose result would otherwise depend on the CALLER having zeroed
/// zl/zu at absent bounds rather than on l/u. It therefore asks l and u the
/// same question every other kernel in this file asks, and a free variable
/// contributes nothing whatever zl/zu happen to hold.
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
/// Condensed bound curvature: sigma_i += zL_i/(x_i - l_i) + zU_i/(u_i - x_i)
/// -- the Sigma that eliminating the bound-multiplier rows leaves on the
/// section 3.1 system's (1,1) diagonal. ACCUMULATED onto whatever base the
/// caller already wrote, so it composes with the proximal term rho rather than
/// replacing it, and it grows no rows.
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
/// WHY THE FINAL READ CANNOT USE THE ORDINARY `Sigma` (M6 W1 T4b fix round 1,
/// settler ruling R1). The item 4 read asks whether the converged point is a
/// second-order point of the CALLER'S QP, and second-order conditions are
/// stated on the CRITICAL CONE. At a bound whose multiplier is essentially
/// zero, the direction moving OFF that bound IS in the critical cone -- so the
/// read must VERIFY that direction, not mask it. The barrier's own curvature
/// `z / gap` masks it: at a weakly active bound both `z` and `gap` are of order
/// `sqrt(mu)`, so `Sigma` is of order ONE and can cancel a genuinely negative
/// eigenvalue. Measured before this function existed: `H = diag(2, -1)` on a
/// symmetric box of half-width `s`, `x = 0` (a SADDLE), certified `kOptimal`
/// for every `s` with `2 mu / s^2 > 1`. That is a `kOptimal`-at-a-non-minimizer,
/// the exact wrong-answer class task 4 closed for the SSN kernel.
///
/// WHAT COUNTS AS WEAKLY ACTIVE, and why the test needs BOTH halves.
/// Complementarity gives `z * gap ~ mu` at every bound, so the three regimes
/// separate cleanly at `sqrt(mu)`:
///   * STRONGLY ACTIVE -- `z -> z* > 0`, so `gap ~ mu / z*`, far BELOW
///     `sqrt(mu)`. The multiplier test fails, `Sigma` is kept: the direction is
///     NOT in the critical cone and its curvature is genuinely constrained.
///   * INACTIVE -- `gap = O(1)`, far ABOVE `sqrt(mu)`. The gap test fails,
///     `Sigma` is kept (it is negligible anyway).
///   * WEAKLY ACTIVE -- both at `sqrt(mu)`. Both tests pass and the side's
///     contribution is dropped.
/// The AND is what makes the rule safe: a wrong verdict needs BOTH a small
/// multiplier and a small gap, and the two are inversely related through
/// complementarity, so a bound has to sit in a `factor^2` band around
/// `sqrt(mu)` to be dropped.
///
/// THE SCALE IS THE SOLVE'S OWN, NOT A NEW ABSOLUTE KNOB (the
/// `IpCrossoverOptions::activity_rel_tol` precedent, `warm_start.h:408`, which
/// builds its threshold out of the hand-off's own `mu_hat` and dual norm rather
/// than out of a fixed magnitude): `weak_scale` is a caller-supplied multiple of
/// `sqrt(mu_measured)`. Pass `0.0` to disable, which reproduces
/// `ipqp_accumulate_bound_sigma` EXACTLY -- same two loops, same order, same
/// `+=`, so a read with nothing weak is bit-identical to the read before this
/// existed.
///
/// PER SIDE, NOT PER VARIABLE: a variable pinned strongly at its lower bound
/// and weakly at its upper keeps the lower side's curvature and drops the
/// upper's, which is what the critical cone actually says.
///
/// @param weak_scale The activity scale (a multiple of `sqrt(mu)`); `<= 0`
/// disables the rule entirely.
/// @param band_upper M6 W1 T4c fix round 1's disclosure BAND ceiling (an
/// absolute value, `kIpqpTightBandFactor * weak_scale`; `<= 0` disables the
/// band). Round 1 REPLACES the original trigger (`gap <= weak_scale AND z >
/// weak_scale`), which the spec author confirmed degenerates to "any
/// strongly active side" by the `z * gap ~ mu` identity -- see
/// `kIpqpTightBandFactor`'s own doc comment and
/// `.superpowers/w1-t4c-report.md`. A KEPT side is band-counted iff
/// `weak_scale < z <= band_upper`; no `gap` test, deliberately (the identity
/// makes it redundant once `z` is bounded both ways).
/// @param band_lower_idx / @param band_upper_idx receive the index of each
/// band-counted lower/upper side (`nullptr` to skip), so the caller can run
/// the exponent-test refinement on exactly those sides rather than
/// re-deriving the band.
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

/// @brief The tier's OWN slack/multiplier complementarity reduction.
///
/// DELIBERATELY NOT SHARED with the NLP engine (section 3.5's "explicitly not
/// shared" row, and barrier_math.h's own banner): that reduction's `.sum()`
/// order feeds the NLP engine's mu and is ULP-load-bearing under fast-math, so
/// every pinned NLP counter and every frozen baseline is downstream of the
/// exact expression it is written as. A shared kernel would put those pins at
/// the mercy of this tier's accumulation order. The cost of NOT sharing is one
/// loop; the cost of sharing is a re-derivation event on the NLP engine.
///
/// The order is stated rather than left to Eigen: ascending index, one pair at
/// a time, into a single accumulator. `avgcomp` is the mean pair value (the
/// tier's mu when the pairs are s_j * lambda_j), `mincomp`/`maxcomp` the
/// extremes. A zero count leaves all three at 0.
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
/// Folds the variable-bound complementarity pairs (x_i-l_i)*zL_i and
/// (u_i-x_i)*zU_i into aggregates already reduced over the slack/multiplier
/// pairs, in place. `base_count` is how many pairs went into `avgcomp` (their
/// sum reconstructs as avgcomp*base_count). The base aggregates are NOT
/// re-reduced, so whatever reduction produced them survives untouched --
/// exactly the property the NLP kernel preserves, and the reason this one is a
/// fold rather than a second full pass. Union min/max are min-of-mins and
/// max-of-maxes; the union average is count-weighted. A problem with no
/// finite variable bounds leaves all three untouched.
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
