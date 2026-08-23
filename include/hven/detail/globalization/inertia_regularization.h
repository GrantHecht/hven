// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <algorithm>
#include <cmath>

namespace hven::solvers {

// Proximal primal-dual KKT regularization: constants and pure-logic helpers
// for the `proximal_regularization` inertia mode — an alternative to the
// classic on-demand inertia ladder.
///
/// Instead of first attempting an unperturbed factorization every iteration and
/// shifting only on wrong inertia, this mode carries a small persistent primal
/// shift AND an always-on barrier-scaled dual shift into the base matrix, then
/// lets the same ladder escalate on top when needed. (The classic ladder borrows
/// the dual shift below on demand too — at most once per phase on the
/// singularity signal — so its constants are shared by both modes; applying both
/// shifts always-on in the base matrix is what is exclusive to this mode.)
///
// Numerical caveats every reader needs:
// - The negative sign on the constraint block is the augmented-system
///   convention (W + delta_w*I / -delta_c*I): it keeps one negative eigenvalue
///   per constraint row, so the target inertia count is unchanged, while making
//   a rank-deficient constraint Jacobian factorizable. The dual shift shrinks
///   to zero with mu and never masks non-convergence — convergence residuals
///   are read from raw evaluations, never from the shifted matrix.
// - Nested-restoration suppression: while a nested l1 restoration phase is
///   active the dual shift is NOT applied — elastic pivots already regularize
///   the constraint-row diagonals at magnitude ~1/mu, and the condensed elastic
//   step-recovery algebra assumes the (y,y) diagonal equals the elastic pivot
///   EXACTLY; stacking -delta_c would make recovered elastic steps inconsistent
///   with the solved system. The primal base shift stays on throughout (it
///   touches only the Hessian diagonal), as does the dual shift under the
///   proximal mode-switch restoration (same reasoning) — no slot conflict there.
// - Orthogonality to Pardiso static pivoting: the dual shift is a problem-level
///   constraint regularization; the static tiny-pivot perturbation is a
///   factorization-internal guard. Different failure modes; neither subsumes
///   the other; this mode does not adjust the pivot exponent.

/// @brief Floor for the persistent primal base shift rho_k, and its initial
/// value rho_0: the Cipolla–Gondzio regularization floor
/// (arXiv:2205.01775, eq. (19): rho = delta = max{1/max(|A|_inf,|H|_inf), 1e-10}).
inline constexpr double kProxRegFloor = 1.0e-10;

/// @brief Prefactor of the barrier-scaled dual shift
/// delta_c = kDualRegScale * mu^kDualRegExponent. Ipopt's
/// `jacobian_regularization_value` default.
inline constexpr double kDualRegScale = 1.0e-8;

/// @brief mu exponent of the dual shift — Ipopt's
/// `jacobian_regularization_exponent` default. delta_c therefore shrinks to
/// zero as mu -> 0.
inline constexpr double kDualRegExponent = 0.25;

/// @brief Barrier-scaled dual shift magnitude delta_c(mu). Returned as a
/// non-negative magnitude; the negative sign on the constraint block is
/// imposed by the KKT assembly.
inline double dual_regularization(double mu) {
    return kDualRegScale * std::pow(mu, kDualRegExponent);
}

/// @brief One decay step of the persistent primal base shift toward its floor:
/// rho_{k+1} = max(kProxRegFloor, applied_total * decr).
///
/// @param applied_total The primal shift actually carried by the factorization
///   this iteration — rho_k alone when the base attempt sufficed, or rho_k
///   plus the last successful ladder delta when the ladder fired — so the
///   decayed total successful shift persists into the next iteration.
inline double prox_reg_decay(double applied_total, double decr) {
    return std::max(kProxRegFloor, applied_total * decr);
}

} // namespace hven::solvers
