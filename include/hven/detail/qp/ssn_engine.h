// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// ssn_engine.h -- THE SEMISMOOTH-NEWTON QP KERNEL.
//
// A Newton method on a Fischer-Burmeister (FB) reformulation of the QP's own
// KKT conditions, solving the SAME subproblem qp_engine.h's primal active-set
// WALK solves (qp_problem.h's QpProblem, same sign convention, same
// regularization knobs), by a different mechanism: every step changes the
// WHOLE implied active set at once, where the walk changes one working-set
// member per minor iteration. The walk's cost at scale is in the NUMBER of
// minor iterations, so a method whose iteration count does not grow with |W*|
// is the only kind of change that can move that figure.
//
// SCOPE -- READ THIS BEFORE JUDGING A DIVERGENCE. THIS ENGINE HAS TWO MODES:
//
//   SsnSafeguards::kBare -- the LOCAL METHOD. Full, undamped Newton steps;
//     no line search, no merit function, no proximal term, no uncertain
//     set, no dual projection, no inertia gate. Locally superlinearly
//     convergent UNDER BD-REGULARITY AT THE SOLUTION, which a WEAKLY ACTIVE
//     row (lambda* = 0 AND c* = 0) can violate. GLOBALLY nothing at all --
//     it can cycle, and it can certify a SADDLE POINT as kOptimal. A
//     POSITIVE CONTROL, not a product surface; every safeguard is scored
//     against it.
//
//   SsnSafeguards::kFull -- the production iteration, and the default.
//     Section 7 below is its specification; 7b is the contract that makes
//     the saddle claim above a claim about kBare ALONE: under kFull no
//     kOptimal is issued anywhere -- cold start, warm hand-off, or a seed
//     placed exactly on the saddle -- without an inertia verdict read AT THE
//     CERTIFIED POINT.
//
// DRIVER WIRING: sqp_driver.h dispatches its MAIN subproblem call on
// SqpOptions::qp_mode, constructing an SsnEngine (kFull) under QpMode::kSsn.
// The SOC re-solve, elastic ladder rungs and restoration sub-solve stay on
// the WALK unconditionally regardless of qp_mode (each is a rescue path
// hot-started off the immediately preceding walk solve). With
// `qp_mode = kWalk` every existing test, pin and battery remains
// byte-identical.
//
// The only convergence claims either mode makes are on the ANALYTIC FIXTURES,
// all of them at most six variables. The corpus gates score the safeguarded
// engine; nothing here should be quoted against them.
//
// 1. THE RESIDUAL.
//
// The QP (qp_problem.h) is
//
//     min  g^T x + 1/2 x^T H x   s.t.  Ae x = be,  Ai x <= bi,  l <= x <= u
//
// with the project's KKT sign convention
//
//     grad(f) + Ae^T lambda_e + Ai^T lambda_i - z = 0,   lambda_i >= 0
//
// where grad(f) = Hx + g and z is the SIGNED bound multiplier (>= 0 at an
// active lower bound, <= 0 at an active upper bound).
//
// BOUNDS ARE ROWS -- each finite bound becomes a one-sided inequality row
// with its own NON-NEGATIVE multiplier, exactly like a row of Ai (lower:
// l_j - x_j <= 0, gradient -e_j; upper: x_j - u_j <= 0, gradient +e_j) --
// and the signed z the rest of the project speaks is recovered as
// z_j = lambda^lo_j - lambda^up_j. A bound at or beyond detail::kSsnInfBound
// in magnitude is ABSENT and contributes no row at all.
//
// Writing s for the SLACK of an inequality-shaped row (s = bi_k - (Ai x)_k
// for a row of Ai, s = x_j - l_j / u_j - x_j for a bound row), the KKT
// conditions per row are s >= 0, lambda >= 0, s * lambda = 0, which the
// Fischer-Burmeister function phi(a, b) = a + b - sqrt(a^2 + b^2) encodes
// exactly: phi(a, b) = 0 <=> a >= 0, b >= 0, a*b = 0. The whole KKT system
// becomes ONE nonlinear equation F(w) = 0 in
// w = (x, lambda_e, lambda_i, lambda_bound):
//
//     F_x  = H x + g + Ae^T lambda_e + Ai^T lambda_i + B^T lambda_b   (n rows)
//     F_e  = Ae x - be                                               (me rows)
//     F_i  = phi(s_k, lambda_i_k)          per row of Ai             (mi rows)
//     F_b  = phi(s_r, lambda_b_r)          per finite bound          (mb rows)
//
// where B is the bound-row matrix (one +/-1 per row). Equalities enter
// LINEARLY -- no FB pair, nothing to select -- so on a QP with no
// inequalities and no finite bounds F is AFFINE and one Newton step is the
// exact solve of the regularized system.
//
// "ONE STEP" IS A STATEMENT ABOUT F, NOT A PROMISE OF ONE FACTORIZATION,
// and the gap is delta/mu: the step solves K = [H + delta I, Ae^T; Ae,
// -mu I], not the unregularized KKT matrix, so it lands
// O(delta*||x*|| + mu*||lambda*||) away from the true solution, and that
// residual SCALES WITH THE ITERATE -- an equality-heavy subproblem
// warm-started far from its solution is NOT a one-factorization solve.
//
// 2. THE GENERALIZED JACOBIAN, AND WHY THE ASSEMBLED MATRIX IS SYMMETRIC.
//
// phi is not differentiable at the origin but IS semismooth everywhere, with
//
//     rho    = sqrt(s^2 + lambda^2)
//     alpha  = d phi / d s      = 1 - s / rho        (in [0, 2])
//     beta   = d phi / d lambda = 1 - lambda / rho   (in [0, 2])
//
// away from s = lambda = 0, where any (1 - xi, 1 - eta) with xi^2 + eta^2 <= 1
// is an element of the C-subdifferential and select_branch selects
// xi = eta = 1/sqrt(2), i.e. alpha = beta = detail::kSsnDegenerateFbDeriv.
//
// alpha AND beta CAN NEVER BOTH BE SMALL: alpha + beta = 2 - (s+lambda)/rho
// >= 2 - sqrt(2) ~ 0.586 for every (s, lambda). alpha = 0 means exactly
// "lambda = 0 and s > 0" (a strictly INACTIVE row) and beta = 0 means
// exactly "s = 0 and lambda > 0" (a strictly ACTIVE one), so the partition
//
//     row k is ACTIVE  <=>  alpha_k > beta_k  <=>  lambda_k > s_k
//
// is read off the Jacobian itself rather than maintained as separate state
// -- it IS the primal-dual active set of PDAS, whose equivalence to
// semismooth Newton is the design's whole point.
//
// Since s_k = b_k - a_k^T x the FB row is
// -alpha_k (a_k^T dx) + beta_k d lambda_k = -phi_k, which is NOT symmetric
// against the stationarity block's a_k column. Multiplying through by
// (-1 / alpha_k) gives
//
//     a_k^T dx - (beta_k / alpha_k) d lambda_k = phi_k / alpha_k
//
// whose off-diagonal is EXACTLY the constraint gradient a_k, so the assembled
// matrix is the project's ordinary symmetric KKT shape with a PER-ROW
// negative diagonal in place of the walk's uniform -mu:
//
//     K = [ H + delta I   Ae^T    Ai^T    B^T   ]
//         [ Ae           -mu I     0       0    ]
//         [ Ai             0     -D_i      0    ]
//         [ B              0       0     -D_b   ]
//
//     D_k = beta_k / alpha_k + mu   (mu = QpOptions::dual_mu, delta =
//                                    QpOptions::primal_delta -- the SAME two
//                                    regularizers kkt_assembly.h applies)
//
// stored as the UPPER TRIANGLE (SpMatRM) and factored by the SAME sparse
// factor (MKL Pardiso / Apple Accelerate) the walk uses.
//
// D IS LARGE WHERE THE ROW IS INACTIVE AND ~mu WHERE IT IS ACTIVE, which is
// why the division is by alpha and not by beta: alpha -> 0 sends the diagonal
// to +infinity (a decoupled, diagonally dominant row -- numerically benign),
// while beta -> 0 sends it to mu (the ordinary active-constraint KKT row the
// walk already factors every minor).
//
// THE alpha FLOOR. alpha is EXACTLY zero on a strictly inactive row, so the
// division carries alpha_f = max(alpha, detail::kSsnAlphaFloor). It
// introduces no active/inactive decision; detail::kSsnAlphaFloor derives why.
//
// 3. THE FIXED SPARSITY PATTERN -- THE LOAD-BEARING PROPERTY.
//
// K's PATTERN depends on (H, Ae, Ai)'s patterns and on WHICH BOUNDS ARE
// FINITE, and on nothing else -- not the active set, the iterate, or which
// branch any FB row is in (a branch change moves the VALUE of one diagonal
// entry and nothing else). Concretely:
//
//   * every row of K at index >= n holds EXACTLY ONE stored entry, its own
//     diagonal (all the constraint coupling lives in the upper triangle, in
//     the x-rows), so that entry's position in the compressed value array is
//     K.outerIndexPtr()[row] with no bookkeeping;
//   * the diagonal slots are built with a nonzero PLACEHOLDER so they exist
//     structurally regardless of what value a branch later wants there
//     (an exact-zero triplet is not a slot one may rely on);
//   * consequently ONE Newton iteration costs one numeric factorization and
//     ZERO symbolic analyses, and a SECOND QP of identical structure costs
//     zero symbolic analyses too -- the analysis decision sees the same
//     hash and factorize_checked() skips the symbolic analysis (kkt_calls.h).
//
// SsnResult::symbolic_analyses reports this directly, counted at the call
// site before factorize(), because factorize() decides internally and does
// not report back.
//
// 4. THE ACTIVITY HINT = ONE PDAS STEP.
//
// SsnStart::activity_hint, when supplied, replaces the FB branch selection ON
// THE FIRST NEWTON STEP ONLY:
//
//     hinted ACTIVE    (alpha, beta) = (1, 0), row residual s_k
//                      => a_k^T (x + dx) = b_k   -- drive the slack to zero
//     hinted INACTIVE  (alpha, beta) = (alpha_floor, 1), row residual lambda_k
//                      => d lambda_k = -lambda_k -- drive the multiplier to zero
//
// which is EXACTLY one primal-dual active-set (equality-constrained KKT)
// solve on the hinted set, up to delta/mu and the O(1e-12) floor coupling.
// Every step after the first uses the FB branch. This is what makes a
// correctly hinted, correctly seeded QP converge in ONE iteration, and it
// is the seam the driver's warm-start activity hands through.
//
// "DRIVE THE SLACK TO ZERO" IS EXACT ONLY AT dual_mu = 0: beta = 0 leaves the
// hinted-active row's diagonal at -mu, so the landing slack is
// s_k^+ = -mu * d lambda_k -- exactly zero when mu = 0, O(mu * |d lambda|)
// under the shipped default. Never use an exactly-zero slack as a premise.
//
// 5. THE TOLERANCE, AND WHAT IT CERTIFIES.
//
// Convergence is ||F(w)||_inf <= SsnOptions::fb_tol, measured on the
// UNSCALED residual of section 1 (never on the row-scaled linear system's
// right-hand side). For s, lambda >= 0 one has the two-sided bound
//
//     (2 - sqrt(2)) * min(s, lambda)  <=  phi(s, lambda)  <=  min(s, lambda)
//
// (both tight), so ||F||_inf <= fb_tol gives
//
//     ||grad L||_inf <= fb_tol,  ||Ae x - be||_inf <= fb_tol,
//     min(s_k, lambda_k) <= fb_tol / (2 - sqrt(2)) = 1.7071 * fb_tol
//
// per row -- AND THAT LAST LINE CARRIES ITS HYPOTHESIS WITH IT: the
// two-sided bound is derived for s, lambda >= 0, which an inexact exit point
// need not satisfy (|phi| <= fb_tol permits either component to be negative
// by O(fb_tol)). The honest reading: every row with a NON-NEGATIVE pair
// satisfies min(s_k, lambda_k) <= 1.7071 * fb_tol, and any row that does not
// has both components within O(fb_tol) of the non-negative orthant. A caller
// needing a strictly feasible point must project.
//
// THE DEFAULT: fb_tol = kkt_tol, which buys stationarity and equality
// feasibility at exactly kkt_tol and complementarity within
// detail::kSsnComplementarityFactor (~1.71) of it.
// ssn_fb_tol_from_kkt_tol() is that derivation in code.
//
// 6. THE PER-SOLVE SEAM: TRUST REGION AND REGULARIZERS.
//
// solve() has an overload taking qp_types.h's SolveOverrides -- THE WALK'S
// OWN struct, not a parallel one, because the funnel driver already builds
// one per subproblem and per SOC re-solve. Sentinels and resolution are
// qp_types.h's rules unchanged.
//
// THE TRUST REGION IS A BOX, AND THAT IS THE WHOLE DESIGN. lo_eff =
// max(lower, x0 - Delta), up_eff = min(upper, x0 + Delta), about this solve's
// own start point, resolved once. Because it is a box it changes only the
// bound rows' VALUES -- never which rows exist, once the radius is finite at
// all -- so K's sparsity pattern is INVARIANT ACROSS RADIUS CHANGES (a
// shrink-retry loop re-solves at zero pattern cost and zero symbolic
// analyses; detail::SsnBoundRow states the key's exclusion at the field), and
// Delta = +inf reproduces the no-trust-region solve BIT FOR BIT.
//
// TR PINS ARE NOT BOUNDS, AND THE EXPORT KEEPS THEM APART:
//
//   tr_active[j]     true iff a TR-tight row is implied active at j
//   bound_state[j]   REAL bounds only -- a TR-pinned variable reads kFree
//   z(j)             REAL bounds only -- a TR-pinned row's dual is dropped
//   start.z          priced against REAL bounds; TR rows seed at zero
//
// This is qp_problem.h's QpSolution::tr_active contract verbatim: a driver
// detects a binding radius by reading tr_active, never z or bound_state.
//
// AND THE RADIUS IS A SOFT CONSTRAINT: ON AN ESCAPED EXIT THE RETURNED x
// MAY LIE FAR OUTSIDE IT -- the TR is bound ROWS and an FB row holds only at
// a root. A certifying exit respects the box to O(1.71 * fb_tol) and is the
// ONLY exit whose x may be used as a step. SsnResult::tr_violation carries
// the full contract.
//
// THE REGULARIZERS RESOLVE TOO, BUT THE DRIVER'S ADAPTIVE-mu SCHEDULE BUYS
// NOTHING HERE and is switched off deliberately: delta and mu perturb ONLY
// the Jacobian and never the residual, which residual() computes
// unregularized. The iteration is therefore modified Newton on the EXACT F --
// fixed points exact, unregularized KKT points -- so (delta, mu) cost
// ITERATIONS rather than ACCURACY, the opposite trade from the walk.
//
// 7. THE SAFEGUARDED ITERATION.
//
// ONE ATTEMPT, in order. Everything marked [G] is skipped entirely under
// SsnSafeguards::kBare, which is what keeps bare mode the bare local method
// rather than a re-implementation of it.
//
//   1. Evaluate F at the iterate. TWO norms come out of the one walk: ||F||inf,
//      the CERTIFICATE of section 5, and 1/2||F||_2^2, the LINE SEARCH's
//      merit. Different on purpose -- detail::kSsnArmijoSigma says why.
//   2. Converged? ||F||inf <= fb_tol. [G] A CERTIFYING EXIT IS NOT FREE: it
//      runs the SECOND-ORDER VERIFICATION first (section 7b), which is the one
//      place the safeguarded iteration pays a factorization the bare one does
//      not.
//   3. [G] DIVERGENCE TELEMETRY, and it runs BEFORE the budget test. An
//      infeasible QP that also runs out of budget must report the DIAGNOSIS,
//      because kBudget is a value a caller answers by raising the budget, which
//      on an infeasible QP is exactly the wrong move. Criteria and their
//      derivation: detail::kSsnStallWindow.
//   4. Out of budget? hard_budget attempts -> kBudget.
//   5. [G] SOFT BUDGET: crossing it arms the proximal term. That is the whole of
//      "soft_budget warns", and the warning is SsnCounters::ssn_prox_updates
//      leaving zero rather than anything printed.
//   6. CLASSIFY every FB row. [G] adds the third set: see select_branch for the
//      band, the hysteresis, the tie policy and why the damped element is still
//      a generalized Jacobian element.
//   7. Refresh the FB diagonals, build the right-hand side, factorize.
//   8. [G] INERTIA GATE. K's inertia is (n, me+mi+mb) IFF the primal block is
//      positive definite -- an identity, not a hope, PROVIDED delta+sigma > 0
//      -- so the gate is provably inert on any convex subproblem under that
//      hypothesis. kWrong escalates the proximal ladder and retries the SAME
//      iterate; kSuspect does NOT act, matching qp_engine.h.
//   9. [G] LINE SEARCH: Armijo on the merit, backtracking by
//      detail::kSsnBacktrackFactor to the floor detail::kSsnMinStep, over
//      trial points that carry the DUAL PROJECTION. Iteration 0 is exempt when
//      a HINT governed it -- see the exemption's own block at the call site.
//  10. Accepted -> apply. Not accepted -> the escape ladder: the infeasibility
//      DIAGNOSIS outranks the repair, then a proximal rung, then
//      kNoContraction.
//
// FOUR OF THESE STEPS HAVE AN OPT-IN ALTERNATIVE, each shipping at the value
// that reproduces the list above BIT FOR BIT; none is a default. Each field
// carries its own contract:
//
//   step 2/7b -- SsnOptions::defer_certification
//   step 5/10 -- SsnOptions::sigma_rule
//   step 9    -- SsnOptions::hint_rule + watchdog_q
//   step 3    -- SsnOptions::infeasibility_rule
//
// WHAT THE PROXIMAL TERM IS, PRECISELY. sigma is added to the SAME two slots
// primal_delta and dual_mu already occupy, anchored at the CURRENT iterate,
// so it perturbs only the Jacobian and section 6's property survives
// verbatim. It is NOT FBstab's outer proximal loop with a lagging centre --
// see detail::kSsnProxInit for the ladder and SsnStart::prox_center_x for the
// anchor ruling.
//
// ATTEMPTS ARE NOT STEPS. An attempt can pay its factorization and then
// take no step (a rejected line search, a wrong inertia, or the second-order
// verification of 7b), so bare mode's "factorizations == iters" is now
// iters <= factorizations, with equality UNREACHABLE under kFull (7b's
// verification adds one) and holding under kBare exactly when nothing was
// rejected. SsnResult::factorizations documents it at the field.
//
// 7b. THE CERTIFYING EXIT, AND WHY IT COSTS A FACTORIZATION.
//
// NO kOptimal IS ISSUED UNDER kFull WITHOUT AN INERTIA VERDICT READ AT THE
// POINT BEING CERTIFIED. A solve seeded at (or within fb_tol of) a stationary
// point that only ran the convergence test would return kOptimal having gated
// nothing -- including at a SADDLE, reachable by exactly the warm hand-off
// the driver builds.
//
// So the convergence test opens a VERIFICATION ATTEMPT instead of exiting:
// classify the rows at the converged point, refresh K's diagonals, factorize,
// read the inertia -- and only then certify. It takes no step, forms no
// right-hand side and runs no triangular solve; it pays ONE numeric
// factorization on the cached pattern and moves no counter but
// SsnResult::factorizations.
//
//   verdict kOk       -> QpStatus::kOptimal. The primal block is positive
//                        definite at the certified point, which on the active
//                        face is the QP's own second-order condition.
//   verdict kSuspect  -> QpStatus::kOptimal, and this is the ONE residual hole
//                        (see the Accelerate note below). kSuspect does not
//                        act, matching qp_engine.h's ruled policy exactly.
//   verdict kWrong    -> SsnEscape::kIndefinite. NOT an escalation: sigma
//                        perturbs the Jacobian only, so at a point where F is
//                        already zero the step stays zero at every rung and a
//                        ladder climb could only spend factorizations on its
//                        way to the same answer.
//
// THE VERDICT IS READ AT THE CALLER'S OWN REGULARIZATION. If the proximal
// ladder is armed when the solve converges, sigma is dropped to 0 for the
// verification and restored for reporting: In(K) = (n, m) iff
// H + (delta+sigma) I + A^T D^{-1} A is positive definite, so at a large
// sigma the test is about H + sigma I and says nothing about the QP the
// caller handed in. An earlier attempt's verdict is not reused for the same
// reason: D is a function of the ITERATE.
//
// THE COST IS ONE FACTORIZATION PER CERTIFIED SOLVE -- a real product cost: a
// one-iteration warm hand-off goes from one factorization to two, and the
// zero-step hand-off from zero to one. THE GATE IS INERT ON A CONVEX
// SUBPROBLEM ONLY WHILE delta+sigma > 0: SolveOverrides::primal_delta = 0.0
// with dual_mu = 0.0 is legal and non-sentinel, and under it an inactive FB
// row's diagonal is exactly 0, so the identity's hypothesis fails. With the
// hypothesis held the verdict is kOk and the verification can never turn a
// good answer into an escape.
//
// ACCELERATE COROLLARY. The gate reads the factorization evidence's
// perturbed-pivot count, which means PERTURBED pivots on MKL and ZERO
// pivots on Accelerate, and the verdict tests it FIRST. An Accelerate
// factorization of an indefinite K that reports a zero pivot therefore
// lands on kSuspect, which does not act -- so the verification CERTIFIES
// THE SADDLE there, in the cold case as well as the seeded one. "Degrades
// toward not firing" is the safe direction for a REPAIR trigger and the
// UNSAFE one for a certificate.
//
// 8. REFERENCES (public mathematics, clean-room).
//
// The PDAS/semismooth-Newton equivalence is Hintermueller, Ito & Kunisch,
// "The primal-dual active set strategy as a semismooth Newton method", SIAM J.
// Optim. 13(3), 2003 -- which is also where the M-matrix hypothesis under which
// PLAIN PDAS converges comes from. The FB residual formulation of a QP's KKT
// system follows Liao-McPherson & Kolmanovsky, "FBstab: A proximally stabilized
// semismooth algorithm for convex quadratic programming", Automatica 113, 2020.
// The three-set partition scaffolds on Curtis, Han & Robinson, "A globally
// convergent primal-dual active-set framework for large-scale convex quadratic
// optimization", Comput. Optim. Appl. 60(2), 2015; the n <= 2 converges /
// n = 3 cycles boundary for the min-map form is Ben Gharbia & Gilbert,
// "Nonconvergence of the plain Newton-min algorithm for linear complementarity
// problems with a P-matrix", Math. Program. 134, 2012. The merit 1/2||Phi||^2
// and its Armijo form are De Luca, Facchinei & Kanzow, "A semismooth equation
// approach to the solution of nonlinear complementarity problems", Math.
// Program. 75, 1996. NO PAPER'S NUMERICAL EXAMPLE IS REPRODUCED HERE -- the
// cycling fixtures were found by search over the failing family, since none
// of the published instances is an instance for THIS (FB, not min-map)
// iteration. Published mathematics only -- no third-party source was read.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/SparseCore>
#include <fmt/format.h>

#include <hven/core/pattern_hash.h>
#include <hven/detail/kkt/kkt_calls.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/drivers/sqp_types.h>
#include <hven/linear/symmetric_factor.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

namespace detail {

// Bounds at or beyond this magnitude are ABSENT and contribute no FB row.
// Identical value and meaning to qp_engine.h's kEngineInfBound and to the
// +/-1e20 sentinel nlp_model.h/qp_problem.h document; restated rather than
// reached for so this header depends on the QP data types only, not on the
// walk's internals.
inline constexpr double kSsnInfBound = 1e20;

// Floor applied to alpha = d phi / d s before it is divided by (banner
// banner section 2). alpha_f appears in BOTH the FB diagonal and the right-hand
// side, so it CANCELS in the recovered multiplier step:
//
//     d lambda_k ~ -(r_k / alpha_f) / (beta_k / alpha_f + mu)
//                = -r_k / (beta_k + mu * alpha_f)  ->  -r_k / beta_k
//
// and what it DOES perturb is the dx-coupling of rows whose true coupling is
// numerically zero. 1e-12 keeps mu * alpha_f (1e-8 * 1e-12 = 1e-20)
// negligible against the beta ~ 1 that always accompanies a floored alpha,
// while the resulting diagonal (beta / alpha_f, at most 2e12) stays far from
// any overflow or conditioning cliff.
inline constexpr double kSsnAlphaFloor = 1e-12;

// Pure division guard for rho = sqrt(s^2 + lambda^2): below this the pair is
// the FB function's one non-differentiable point and the C-subdifferential
// selection below is used instead. Deliberately at the far bottom of the
// double range -- this is NOT a "treat as degenerate" tolerance (which would
// be a branch, and would need a justification of its own); it exists only so
// that 0/0 cannot be formed.
inline constexpr double kSsnRhoFloor = 1e-300;

// The C-subdifferential element selected at s = lambda = 0: xi = eta =
// 1/sqrt(2), giving alpha = beta = 1 - 1/sqrt(2). The symmetric choice, and
// the one that keeps alpha strictly above the floor so the degenerate row
// still couples to dx.
inline const double kSsnDegenerateFbDeriv = 1.0 - 1.0 / std::sqrt(2.0);

// 1 / (2 - sqrt(2)) ~ 1.7071: the factor by which a satisfied FB residual
// bounds the per-row complementarity min(s, lambda). See banner section 5.
inline const double kSsnComplementarityFactor = 1.0 / (2.0 - std::sqrt(2.0));

// phi(a, b) = a + b - sqrt(a^2 + b^2), evaluated in the CANCELLATION-FREE
// form 2ab / (a + b + rho) whenever a + b > 0. The two are algebraically
// identical -- multiply by (a+b+rho)/(a+b+rho) and use (a+b)^2 - rho^2 = 2ab
// -- but not numerically: the naive form has an absolute error floor of
// ~ulp(rho)/2, because fl(sqrt(fl(s*s))) == s exactly in IEEE double and
// fl(s + lambda) == s for any |lambda| < ulp(s)/2, so a FAR-SLACK row
// (s = 1e6, lambda = 1e-14, true phi ~1e-14) evaluates to EXACTLY 0. That
// under-reports rather than adding noise, so it cannot stall the iteration,
// but it puts an additive ulp(s_row)/2 slop on the exit CERTIFICATE and the
// merit 1/2||F||^2 inherits the same quantization.
//
// The stable form removes that floor for one extra multiply, and is used
// only when a + b > 0 -- the same-sign regime where the cancellation lives.
// When a + b <= 0 the naive expression sums two non-positive terms and
// cannot cancel, and it also covers a = b = 0 (where the stable form's
// denominator vanishes).
inline double ssn_fb(double a, double b) {
    const double rho = std::sqrt(a * a + b * b);
    const double sum = a + b;
    return sum > 0.0 ? 2.0 * a * b / (sum + rho) : sum - rho;
}

// THE SAFEGUARD CONSTANTS.
//
// Every value below is an IMPLEMENTATION CHOICE, not a paper constant, and is
// stated as such with its own argument -- the same standard sqp_types.h's
// kWarmResidualGrowthMax / kWarmFullStepWindow are held to.

// ---- THE UNCERTAIN BAND (the three-set partition's only tolerance) --------
//
// THE MARGIN IS MEASURED ON THE FB DERIVATIVE PAIR ITSELF, which is what
// makes it dimensionless and scale-free, so no second tolerance is needed.
// Writing (s, lambda) = rho (cos theta, sin theta),
// alpha - beta = (lambda - s) / rho = sqrt(2) * sin(theta - pi/4): 0 exactly
// on the kink ray s = lambda (where the classification is undecidable), 1 at
// either PURE state. The partition rule alpha > beta is the SIGN of this
// quantity; the uncertain set is the band around its zero.
//
// kSsnUncertainEnter = 0.1 -- a row enters the uncertain set when its pair is
// within 0.1 of the kink in this measure, i.e. within
// arcsin(0.1/sqrt(2)) = 4.05 degrees of the kink ray. The largest swept value
// that costs no iterations on the benign set; 0 reproduces the binary
// partition exactly.
//
// kSsnUncertainLeaveRatio = 3 -- the HYSTERESIS. A row LEAVES the uncertain set
// only once its margin reaches 3x the entering threshold, so a row whose margin
// hovers in [0.1, 0.3] keeps whatever class it already had and cannot chatter.
// The smallest ratio that is a band at all is > 1; 3 is large enough that a
// row must move materially to be re-decided and small enough that a genuinely
// decided row is never trapped.
inline constexpr double kSsnUncertainEnter = 0.1;
inline constexpr double kSsnUncertainLeaveRatio = 3.0;

// ---- THE LINE SEARCH -----------------------------------------------------
//
// Armijo on the FB merit psi(w) = 1/2 ||F(w)||_2^2, accepted when
//
//     psi(w + t d) <= (1 - 2 * kSsnArmijoSigma * t) * psi(w),
//
// which is the standard sufficient-decrease form for a semismooth Newton
// direction on an FB reformulation (De Luca-Facchinei-Kanzow's merit; the
// exact Newton step of the UNregularized system gives
// grad psi^T d = -||F||^2, so the condition above is Armijo with that
// derivative substituted). 1e-4 is the textbook Armijo constant, loose enough
// that a full Newton step in the local regime always passes it -- which is
// what keeps the safeguarded trajectory equal to the bare one on every benign
// fixture.
//
// **THE MERIT IS THE 2-NORM SQUARE, THE CERTIFICATE IS THE INF-NORM**, on
// purpose: fb_tol certifies per-row quantities and must stay the inf-norm of
// banner section 5, while a merit function has to be smooth where phi is and
// max_k |F_k| is not.
inline constexpr double kSsnArmijoSigma = 1e-4;

// Halving. A coarser factor (0.1) throws away the intermediate step lengths a
// nearly-accepted Newton step needs; a finer one (0.8) pays more residual
// evaluations for the same reduction.
inline constexpr double kSsnBacktrackFactor = 0.5;

// The step FLOOR. Below this the backtracking schedule is declared exhausted
// and the escape ladder takes over (proximal escalation, then
// SsnEscape::kNoContraction). 1e-4 with a halving factor bounds the schedule
// at 14 trial points -- 14 O(nnz) residual evaluations against the ONE
// factorization they protect.
inline constexpr double kSsnMinStep = 1e-4;

// ---- THE LM-REGULARIZED LADDER ----
//
// NAMING: *proximal* properly means FBstab's outer loop with a LAGGING
// centre -- the thing this ladder is NOT. sigma is Wachter-Biegler-style
// regularization escalation on a CURRENT-iterate anchor, not a
// proximal-point method. The `ssn_prox_*` identifiers are shipped surface and
// are not renamed to match.
//
// The CURRENT-iterate anchor makes sigma an additive increment to the two
// regularizers the engine already applies: K's primal block gets
// H + (delta + sigma) I and every dual diagonal gets -(... + mu + sigma).
// The residual is untouched, so banner section 6's property survives intact:
// sigma costs ITERATIONS, never ACCURACY, and the iteration's fixed points
// remain exact, unregularized KKT points. A LAGGING centre would make the
// exit certificate two-level; SsnStart::prox_center_x carries that ruling.
//
// THE LADDER IS PER-SOLVE AND MONOTONE -- sigma never decreases inside one
// solve, except for the second-order verification's drop to the caller's own
// regularization (banner section 7b), which is not a rung. Two decades per
// rung and a 1e6 ceiling give SEVEN rungs from 1e-6 (1e-6, 1e-4, 1e-2, 1,
// 1e2, 1e4, 1e6), enough to dominate any Hessian the engine can be handed at a
// sane scaling and bounded by the step budget in any case.
//
// **THE CEILING TEST NEEDS A RELATIVE SLACK.** Repeated multiplication by 100
// does not reproduce 1e6 in binary floating point -- the sequence ends at
// 999999.9999999998, strictly below the cap -- so an exact `>= kSsnProxMax`
// guard grants an extra rung that raises sigma by 2.3e-10 relative and buys a
// full numeric factorization plus a full backtracking schedule for it.
// kSsnProxCapSlack closes the guard against that drift: 1e-9 is seven orders
// above the accumulated relative error (~2e-16) and eleven orders below one
// rung (a factor 100), so it can neither miss the ceiling nor merge two
// genuine rungs.
inline constexpr double kSsnProxInit = 1e-6;
inline constexpr double kSsnProxGrowth = 100.0;
inline constexpr double kSsnProxMax = 1e6;
inline constexpr double kSsnProxCapSlack = 1e-9;
// The ladder's DOCUMENTED length, asserted by the test suite rather than read
// by the iteration -- the rungs come from Init/Growth/Max above. It exists so
// the documented count and the delivered count cannot drift apart silently.
inline constexpr Index kSsnProxRungs = 7;

// THE INFEASIBILITY TELEMETRY.
//
// An infeasible convex QP has NO KKT point, so F has no root and the
// iteration cannot converge. Instead ||F||inf FLATTENS onto a positive floor
// (the least-squares distance between the contradictory rows) while the
// multipliers of those rows GROW without bound, because
// phi(s, lambda) -> s as lambda -> +infinity on a row with s < 0. BOTH
// HALVES ARE REQUIRED: growth alone is ordinary early behaviour, and a stall
// alone is a hard-but-feasible problem or a budget too small (kBudget).
//
// THREE PROPERTIES THE STALL TEST MUST HAVE:
//
//   (i) THE WINDOW ADVANCES ON ACCEPTED STEPS, NEVER ON ATTEMPTS. A
//       proximal retry re-evaluates the IDENTICAL residual at the IDENTICAL
//       iterate, so counting attempts would let one rough patch fill the
//       window with copies of one point.
//   (ii) THE IMPROVEMENT DEMAND IS OVER THE WHOLE WINDOW, NOT PER STEP. A
//       proximally damped iteration legitimately crawls, which a per-step
//       demand reads as a stall on a perfectly FEASIBLE subproblem; over a
//       window the same crawl improves measurably and re-arms, while the
//       exactly-flat residual an infeasible QP produces still fills it.
//   (iii) THE GROWTH CONJUNCT DIFFERS ON THE TWO ROUTES, because they see
//       different evidence. Measuring both against the START POINT (floored
//       at 1) would make the test VACUOUS for a cold start on any feasible
//       QP whose true multipliers exceed 1e4.
//         - THE STANDING ROUTE re-arms its reference with the window, so
//           "the duals grew 1e4x" means the divergence happened WHILE
//           nothing improved.
//         - THE EXHAUSTION ROUTE cannot use a windowed reference at all:
//           the divergence and the last progress are the SAME accepted
//           step, so any "growth since the last progress" is 1. It keeps
//           the start-point reference and adds kSsnDualStepGrowth: the
//           multiplier norm must have multiplied by an ORDER OF MAGNITUDE
//           across the most recently accepted step. A trajectory converging
//           to large multipliers has settled (per-step ratio -> 1) by the
//           time its line search dies.
//
// A window in which the PROXIMAL LADDER escalated cannot declare a stall at
// all -- it is discarded and a fresh one starts. Slow progress under a sigma
// that just changed is the safeguard's doing, not the problem's.
//
// kSsnStallWindow = 5 ACCEPTED STEPS over which ||F||inf must improve on the
// window's reference by kSsnStallImproveFactor. Five is an order of magnitude
// above the local regime this method targets, so no healthy trajectory can
// reach it. The 1% improvement demand per WINDOW is deliberately feeble, and
// RELATIVE so that an exactly-flat residual cannot be mistaken for
// improvement by rounding noise.
//
// kSsnDualGrowthFactor = 1e4 against the multiplier norm at the reference
// point the route selects (floored at 1, so a zero-multiplier reference is
// measured absolutely). Four orders is far above any legitimate dual growth
// between two points that made no progress on each other, and far below the
// 1e150 scale at which IEEE double arithmetic itself breaks down.
//
// kSsnDualStepGrowth = 10, the EXHAUSTION route's second conjunct, on the
// growth across ONE accepted step: an order of magnitude in a single step is
// not a trajectory settling onto its multipliers, it is one whose multipliers
// have no limit to settle onto.
inline constexpr Index kSsnStallWindow = 5;
inline constexpr double kSsnStallImproveFactor = 0.99;
inline constexpr double kSsnDualGrowthFactor = 1e4;
inline constexpr double kSsnDualStepGrowth = 10.0;

// THE RESEARCH LEVERS' OWN CONSTANTS.
//
// Every value here is reachable ONLY through a non-default SsnOptions field, so
// none of them can move a shipped trajectory. They are stated to the same
// standard as the constants above nonetheless, because a lever measured on an
// unjustified constant measures the constant rather than the lever.

// THE LEVENBERG-MARQUARDT SIZING (SsnOptions::sigma_rule).
//
// sigma_k = kSsnLmSigmaC * min(r_k, r_k^2), r_k = ||F_k||inf / f_scale, with
// f_scale = max(1, ||F_0||inf) the residual at THIS solve's own start point.
//
// WHY min(r, r^2) AND NOT r^2. Yamashita & Fukushima (Computing Suppl. 15,
// 2001) prove quadratic local convergence for mu_LM = ||F||^2 under a local
// error bound; Fan & Yuan (Computing 74, 2005) sharpen the exponent to any
// delta in [1, 2]. r^2 is the right size CLOSE to the solution and far too
// small FAR from it, where r is what the same theory wants; min(r, r^2) joins
// the two regimes at the point where they agree.
//
// WHY THE NORMALIZATION IS THE START RESIDUAL. This file's shift sits inside
// a KKT block whose primal diagonal is H's, so the exponent and the constant
// are TUNABLE rather than derived; dividing by the start residual makes the
// lever's first sigma independent of the problem's absolute scaling. Floored
// at 1 so an already-converged start point cannot manufacture a large r.
//
// c = 1 -- the neutral constant: with the floor and the cap below, c only
// selects WHERE between them sigma sits.
inline constexpr double kSsnLmSigmaC = 1.0;

// THE WATCHDOG (SsnOptions::hint_rule + watchdog_q).
//
// q = 1 relaxed step is the DEFAULT, and is the value at which the watchdog
// reproduces the shipped iteration-0 exemption exactly on a hint that works:
// step 0 is taken unsearched, and if the residual has not come down by step 1
// the iteration returns to its best stored point and takes a monotone step
// from there. Other values are reachable through SsnOptions::watchdog_q.
inline constexpr Index kSsnWatchdogQ = 1;

// THE FARKAS RESIDUAL TEST (SsnOptions::infeasibility_rule).
//
// The system {Ae x = be, a_k^T x <= b_k} is infeasible IFF there is
// (y_e free, y >= 0) with Ae^T y_e + sum_k y_k a_k = 0 and
// be^T y_e + sum_k y_k b_k < 0 (Farkas). This file tests that pair on the
// NORMALIZED DUAL INCREMENT projected onto the sign cone -- one matvec plus
// O(m), and no factorization.
//
// kSsnFarkasResidualTol = 1e-6 on the RELATIVE residual
// ||A^T y||inf / max(1, ||(|A|^T |y|)||inf). Relative because the test must
// survive bad scaling; 1e-6 because the direction tested is an FB dual
// increment rather than an exact recession direction, so demanding more would
// test the iteration's convergence rather than the certificate's existence.
//
// BOTH QUANTITIES ARE RELATIVE ONLY ABOVE 1. The `max(1.0, .)` in each
// denominator is an ABSOLUTE FLOOR: below 1 the test is on the ABSOLUTE
// residual, which is what stops a near-zero denominator from manufacturing a
// certificate out of rounding noise. Same reading for the gap.
//
// kSsnFarkasGapTol = 1e-8 on the RELATIVE Farkas objective
// <b, y> / max(1, sum_k |b_k y_k|), which must be at most -kSsnFarkasGapTol.
// A strictly negative <b, y> is the certificate's own second half; the
// tolerance exists only so a rounding-noise negative cannot fire it, eight
// orders below the O(1) values a genuine contradiction produces.
inline constexpr double kSsnFarkasResidualTol = 1e-6;
inline constexpr double kSsnFarkasGapTol = 1e-8;

// The three-set partition of CHR 2015's scaffold. kUncertain is unreachable
// with SsnSafeguards::kBare, which is what makes bare mode reproduce the bare
// binary partition exactly.
enum class SsnRowClass : std::uint8_t {
    kInactive = 0,
    kActive = 1,
    kUncertain = 2,
};

// One bound of one variable, recast as an inequality row
//
//     c = sign * x(var) - rhs <= 0,   slack s = rhs - sign * x(var)
//
// so sign = -1, rhs = -lower(var) is the lower bound and sign = +1,
// rhs = upper(var) the upper one. Held in a flat, ascending-by-variable list
// (both rows of a two-sided variable adjacent, lower first) which IS the
// bound block's row order in K.
struct SsnBoundRow {
    Index var = 0;
    double sign = 1.0;
    double rhs = 0.0;
    // True iff this row's bound is the TRUST REGION's, strictly tighter than
    // whatever real bound (if any) the QP declares on this side. An EXPORT
    // attribute, not a structural one: a TR row occupies the same slot in K as
    // the real row it displaced, so this field must NEVER enter the structure
    // key (structure_hash / bound_rows_match_cached) -- if it did, two radii
    // would look like two structures and the pattern cache would rebuild on
    // every major of a shrink-retry loop.
    bool from_tr = false;
};

// The two norms of F one iteration needs. Separate fields rather than two
// passes because they are accumulated in the same walk over the same blocks.
struct SsnNorms {
    double inf_norm = 0.0; // ||F||inf -- the CERTIFICATE (banner section 5)
    double merit = 0.0;    // 1/2 ||F||_2^2 -- the LINE SEARCH's function
};

// What one factorization's reported inertia says about the system factorized.
//
// **THIS IS qp_engine.h's detail::InertiaVerdict, RESTATED RATHER THAN
// INCLUDED**, for the same reason kSsnInfBound is restated: this header
// depends on the QP data types and on the KKT factor helper, never on the
// walk's internals. The verdict rules are qp_engine.h's exactly
// (docs/retarget-design-sqp.md SS4.1): non-kObserved evidence routes to
// kSuspect explicitly, then perturbed pivots first -- a factorization whose
// pivots were perturbed reports an inertia that looks exactly like a genuine
// one, so its counts carry no information AT ALL, including when they happen
// to match -- then the short-sum rule.
enum class SsnInertia {
    kOk,      // trustworthy AND equal to the expectation
    kSuspect, // untrustworthy: perturbed pivots, or counts that do not sum to n
    kWrong,   // trustworthy AND different from the expectation
};

inline SsnInertia ssn_inertia_verdict(const hven::linear::InertiaEvidence &e, Index expected_pos,
                                      Index expected_neg) {
    if (e.state != hven::linear::InertiaEvidence::State::kObserved) {
        return SsnInertia::kSuspect;
    }
    if (e.perturbed_pivots.has_value() && *e.perturbed_pivots != 0) {
        return SsnInertia::kSuspect;
    }
    const Index pos = static_cast<Index>(e.n_pos);
    const Index neg = static_cast<Index>(e.n_neg);
    if (pos + neg != expected_pos + expected_neg) {
        return SsnInertia::kSuspect;
    }
    return (pos == expected_pos && neg == expected_neg) ? SsnInertia::kOk : SsnInertia::kWrong;
}

} // namespace detail

// Why a solve stopped somewhere other than a converged point. kNone is the
// converged case.
//
// ALL FIVE ARE REACHABLE:
//
//   kBudget            hard_budget attempts were spent. Says nothing about the
//                      problem -- only that this budget was too small.
//   kSingular          the factorization threw, the step came back non-finite,
//                      or the inertia was TRUSTWORTHY AND WRONG at the top of
//                      the ladder. escape_detail names which.
//   kIndefinite        THE SECOND-ORDER VERIFICATION's verdict: the residual
//                      satisfies fb_tol -- so the point IS first-order KKT --
//                      but the inertia of K there is trustworthy and NOT
//                      (n, me+mi+mb), i.e. the primal block is not positive
//                      definite. The point is a saddle or a maximizer of the
//                      QP, which no residual-based test can see.
//   kNoContraction     THE LINE SEARCH's verdict: the Armijo schedule ran down
//                      to detail::kSsnMinStep without the merit accepting, at
//                      the top of the ladder. The direction is not a descent
//                      direction for the merit and damping did not make it one.
//   kInfeasibleSuspect THE DIVERGENCE TELEMETRY's verdict: ||F||inf stalled
//                      (detail::kSsnStallWindow) WHILE the multiplier norm grew
//                      by kSsnDualGrowthFactor. SUSPECT because it is a
//                      diagnosis from behaviour, never a Farkas certificate.
//
// **NONE OF THEM CERTIFIES ANYTHING.** An escaped SsnResult reports where the
// solve STOPPED. The driver routes an escaped subproblem back to the walk;
// that routing is the only correct consumption of any value below.
//
// **BRANCH ON escape_reason, NEVER ON status.** kInfeasibleSuspect reports
// QpStatus::kInfeasible, which the walk issues as a CERTIFICATE, and
// kIndefinite/kNoContraction/kSingular all report QpStatus::kNumericalError.
// A driver switching on status alone would promote a suspicion to a
// certificate at the driver layer.
enum class SsnEscape {
    kNone = 0,
    kBudget = 1,
    kSingular = 2,
    kNoContraction = 3,
    kInfeasibleSuspect = 4,
    kIndefinite = 5,
};

// Which rows a caller believes are active, used for the FIRST Newton step only
// (banner section 4). The two halves are INDEPENDENT: supply either, both, or
// neither. An empty half means "no hint for these rows" and those rows take
// the ordinary FB branch on step one like every later step; a non-empty half
// must be exactly sized or solve() throws.
struct SsnActivityHint {
    std::vector<bool> ineq;         // size mi, or empty
    std::vector<BoundState> bounds; // size n, or empty

    bool empty() const { return ineq.empty() && bounds.empty(); }
};

// The point a solve starts from. Every vector may be left EMPTY, which means
// "zero of the right size" -- so a default-constructed SsnStart is the cold
// start from the origin with no multipliers and no hint.
struct SsnStart {
    Vec x;        // n
    Vec lambda_e; // me
    Vec lambda_i; // mi, >= 0 expected but not required (the FB residual is
                  // defined for any sign; a negative seed simply reads as a
                  // large complementarity residual)
    // SIGNED bound multiplier, qp_problem.h's QpSolution::z convention: >= 0
    // at an active lower bound, <= 0 at an active upper bound. Split into the
    // two non-negative bound-row multipliers on ingest (lambda^lo = max(z, 0)
    // against a finite lower bound, lambda^up = max(-z, 0) against a finite
    // upper one) and recombined into SsnResult::z on exit. Added beyond the
    // bare method's own field list: without it a warm start cannot carry
    // bound activity at all.
    Vec z; // n

    // **IGNORED, AND STRUCTURALLY SO.** For a QP the slack of a row is a
    // FUNCTION of x (s = bi - Ai x), so a separately seeded slack block can
    // only agree with x or contradict it; residual() always derives it. The
    // field exists because the proximally stabilized formulation carries an
    // independent slack/dual block a caller may want to hand back in.
    // Validated for size when non-empty; never read otherwise.
    Vec slacks; // mi, or empty

    // **STILL IGNORED, AND THIS IS A RULING RATHER THAN AN OMISSION.** The
    // proximal term does not read them: it anchors at the CURRENT ITERATE
    // instead of at a lagging centre.
    //
    // WHY THAT ANCHOR. With the centre at the current point the proximal term
    // perturbs only the JACOBIAN, so banner section 6's property survives --
    // the iteration stays modified Newton on the EXACT F, its fixed points
    // stay exact unregularized KKT points, and ONE residual per attempt serves
    // both the certificate and the merit. A LAGGING centre changes the
    // residual (F_sigma = F + sigma(w - wbar), the FB rows carrying a shifted
    // slack), which splits the exit certificate into an inner and an outer one
    // and forces two residual evaluations per attempt. The one thing it
    // uniquely buys -- the divergence-of-the-proximal-step infeasibility
    // certificate -- this engine does not claim to produce anyway
    // (SsnEscape::kInfeasibleSuspect is behavioural).
    //
    // The fields stay, still size-checked when non-empty, so the interface is
    // the right shape if a cell ever needs a lagging centre. A caller wanting
    // a warm proximal sequence today uses SsnOptions::prox_sigma_init.
    Vec prox_center_x;      // n, or empty
    Vec prox_center_lambda; // me + mi, or empty

    SsnActivityHint activity_hint;
};

// WHICH ITERATION A SOLVE RUNS. Two modes, and the bare one exists to be a
// POSITIVE CONTROL rather than a product surface.
//
// **kBare IS THE BARE LOCAL METHOD, BIT FOR BIT** -- full undamped steps, the
// binary alpha > beta partition with no uncertain set and no hysteresis, no
// line search, no dual projection, no proximal term and no inertia gate. The
// two modes are the same function, so the reproduction claim is checked on
// every suite run rather than argued.
//
// **A RUNTIME SWITCH, DELIBERATELY.** A compile-time switch could only be a
// macro (un-co-testable in one TU) or a template parameter on SsnEngine
// (propagating into every driver signature and into SqpCounters). A runtime
// enum costs one predictable branch per solve on a path that has already paid
// a sparse factorization, and keeps both modes in ONE binary so a single test
// can compare them directly.
enum class SsnSafeguards {
    kBare = 0, ///< The bare local method.
    kFull = 1, ///< The production iteration (default).
};

// Per-solve knobs. The regularizers delta/mu are NOT here: they come from the
// QpOptions the SsnEngine was constructed with, exactly as QpEngine takes
// its own, so the two kernels are regularized identically by construction.
struct SsnOptions {
    // Which iteration to run. kFull is the shipped default; kBare is the
    // positive control (see SsnSafeguards).
    SsnSafeguards safeguards = SsnSafeguards::kFull;

    // THE ESCALATION THRESHOLD, and it is a threshold on ATTEMPTS rather than
    // on accepted steps -- an attempt that ends in a rejected step is exactly
    // the kind of trouble the escalation exists for, so it must count.
    //
    // On reaching it a solve that is still running unregularized turns the
    // proximal term ON at detail::kSsnProxInit. That is the whole of the
    // "soft_budget warns" contract, and the warning is OBSERVABLE rather than
    // printed: SsnCounters::ssn_prox_updates leaves zero. No separate warn
    // counter, deliberately -- one would widen SqpCounters for a fact
    // ssn_prox_updates already carries.
    //
    // 12 is above every benign fixture's attempt count, so the escalation is
    // provably inert on all of them.
    Index soft_budget = 12;

    // THE HARD CAP, on ATTEMPTS rather than on accepted steps
    // (SsnResult::factorizations states the invariant). A solve that reaches
    // it stops with QpStatus::kMaxIter and SsnEscape::kBudget. Must be >= 0;
    // 0 means "test the start point and take no step".
    //
    // **THE SECOND-ORDER VERIFICATION IS NOT AN ATTEMPT AND IS NOT CAPPED BY
    // THIS FIELD** (banner section 7b). Under kFull a start point that is
    // already converged still pays the ONE verification factorization at
    // hard_budget = 0, because the alternative is certifying a point nothing
    // ever looked at. It takes no step, so the field's contract is intact.
    Index hard_budget = 25;

    // ||F||_inf convergence threshold. Default = SqpOptions::kkt_tol's own
    // default, per the derivation in banner section 5; use
    // ssn_fb_tol_from_kkt_tol() to track a non-default kkt_tol. Must be > 0.
    double fb_tol = 1e-6;

    // THE PROXIMAL TERM'S STARTING VALUE, and **0.0 IS THE RULED DEFAULT**:
    // the proximal term is a REPAIR, not a policy. It costs iterations
    // wherever it is on, it repairs nothing on a converging solve, and every
    // case that needs it is one the escalation ladder ARMS it on. A caller
    // CONTINUING a proximal sequence (the re-solve after an escape) can start
    // it warm here; must be >= 0.
    double prox_sigma_init = 0.0;

    // THE UNCERTAIN BAND's entering threshold, on |alpha - beta| -- see
    // detail::kSsnUncertainEnter for the geometry and the hysteresis ratio
    // that derives the LEAVING threshold from it.
    //
    // 0.0 DISABLES THE UNCERTAIN SET without disabling anything else, which is
    // what makes "the uncertain set, ablated" a one-field experiment rather
    // than a rebuild. Must be in [0, 1): at 1 a strictly active row
    // (|alpha - beta| = 1) would itself be uncertain and the partition would
    // carry no information at all.
    double uncertain_tol = detail::kSsnUncertainEnter;

    // THE FOUR RESEARCH LEVERS.
    //
    // Every field below is OPT-IN and ships at the value that reproduces the
    // shipped iteration BIT FOR BIT; none of them is a default.

    // DEFER THE CERTIFYING EXIT'S INERTIA EVIDENCE (SsnOptions::defer_certification).
    //
    // false (default) -- banner section 7b verbatim: the convergence test opens a
    // VERIFICATION ATTEMPT that factorizes K at the converged point and reads
    // its inertia before kOptimal is issued.
    //
    // true -- the verification attempt is built (rows classified at the
    // converged point, sigma dropped to the caller's own regularization, K's
    // diagonals refreshed) but NOT FACTORIZED. The solve returns kOptimal with
    // `SsnResult::certification_deferred` set, and the caller owes the engine
    // exactly one of finish_deferred_certification() or
    // discard_deferred_certification() before the next solve.
    //
    // **WHY A CALLER WOULD WANT THAT** is Gould's lemma (Gould, Math. Prog. 32,
    // 1985): the KKT matrix of the face EQP has inertia (n_f, m_f, 0) IFF the
    // reduced Hessian on that face is positive definite, so a caller about to
    // re-solve the identified face EXACTLY -- QpEngine::refine_on_face, which
    // gates on that very verdict -- already buys the second-order evidence.
    // THE TWO CERTIFICATES ARE NOT THE SAME STATEMENT: this engine's
    // verification tests (n, me+mi+mb) on the FULL augmented block at the
    // caller's regularization; the face route tests positive definiteness of
    // the reduced Hessian ON THE IDENTIFIED FACE. They can disagree.
    //
    // **NOTHING IS WEAKENED WHEN THE FACE SOLVE IS REFUSED.** A caller whose
    // refinement is refused calls finish_deferred_certification() and gets
    // exactly the shipped verdict at exactly the shipped cost, because the
    // matrix it factorizes is the one this loop already built.
    bool defer_certification = false;

    // HOW sigma IS SIZED (sqp_types.h's SsnSigmaRule).
    //
    // kLadder (default) is banner section 7's failure-reactive ladder verbatim. The
    // two residual rules replace the CLIMB with a size read off the residual:
    //
    //     sigma_k = max(ladder_k, clamp(c * min(r_k, r_k^2),
    //                                   kSsnProxInit, kSsnProxMax))
    //
    // with r_k = ||F_k||inf / max(1, ||F_0||inf) and c = detail::kSsnLmSigmaC.
    //
    // THE LADDER IS RETAINED AS A MONOTONE FLOOR, NOT REPLACED: every
    // escalation trigger still climbs a rung, so a solve that cannot be
    // repaired still reaches the ceiling and escapes with the same reason.
    // Between triggers sigma is sized from the evidence rather than the
    // history, and a residual that FALLS lowers sigma back toward the floor --
    // which the monotone ladder structurally cannot do.
    //
    // kResidualArmed is INERT until the ladder arms, so it is provably inert
    // on every fixture whose ladder never arms. kResidualAlways sizes sigma
    // from attempt 0, carries at least kSsnProxInit everywhere, and is inert
    // NOWHERE.
    SsnSigmaRule sigma_rule = SsnSigmaRule::kLadder;

    // WHAT PROTECTS THE HINTED FIRST STEP (sqp_types.h's SsnHintRule).
    //
    // kIterationZeroFree (default) is banner section 7 step 9's exemption verbatim.
    // kWatchdog replaces it with the published rule the exemption is an
    // unsafeguarded special case of: up to `watchdog_q` relaxed (unsearched)
    // steps from the hinted start, a stored BEST point, and -- if the merit has
    // not achieved Armijo decrease against the watchdog's own reference by then
    // -- a RETURN TO THAT BEST POINT followed by a monotone step.
    //
    // The two agree exactly whenever the hint was right (the relaxed step lands
    // at the solution and the convergence test fires before the watchdog can
    // judge it) and differ exactly where the exemption has no answer: a wrong
    // hint whose relaxed step is accepted and bad. `watchdog_q` must be >= 1.
    SsnHintRule hint_rule = SsnHintRule::kIterationZeroFree;
    Index watchdog_q = detail::kSsnWatchdogQ;

    // WHAT TURNS A SUSPICION INTO AN EXIT (sqp_types.h's SsnInfeasibilityRule).
    //
    // kSymptoms (default) is the stall/growth conjunct pair of the
    // kSsnStallWindow block verbatim: the symptoms ARE the exit test.
    //
    // kFarkasGated keeps every symptom conjunct as the ARMING condition and
    // adds a CERTIFICATE as the firing condition -- the normalized dual
    // increment, projected onto the sign cone, tested as an approximate Farkas
    // direction (detail::kSsnFarkasResidualTol / kSsnFarkasGapTol). The
    // increment is measured against the STANDING route's window reference on
    // that route and against the PREVIOUS accepted step on the EXHAUSTION
    // route, mirroring exactly the reference each route's growth conjunct
    // already uses and for the same reason.
    //
    // It can only make the engine MORE reluctant to report kInfeasible: an
    // armed check that finds no Farkas direction falls through to the ordinary
    // budget/no-contraction routes.
    SsnInfeasibilityRule infeasibility_rule = SsnInfeasibilityRule::kSymptoms;
};

// The tolerance derivation of banner section 5, in code: an FB residual at
// `kkt_tol` buys stationarity and equality feasibility at exactly kkt_tol and
// per-row complementarity within detail::kSsnComplementarityFactor of it.
// Throws std::invalid_argument on a non-positive kkt_tol.
inline double ssn_fb_tol_from_kkt_tol(double kkt_tol) {
    if (!(kkt_tol > 0.0)) {
        throw std::invalid_argument(
            fmt::format("ssn_fb_tol_from_kkt_tol: kkt_tol must be > 0, got {}", kkt_tol));
    }
    return kkt_tol;
}

struct SsnResult {
    QpStatus status = QpStatus::kOptimal;
    SsnEscape escape_reason = SsnEscape::kNone;

    // True iff this solve reached a certifying exit under
    // SsnOptions::defer_certification and therefore returned kOptimal
    // WITHOUT the second-order verification having been factorized. The
    // status is provisional until the caller calls
    // SsnEngine::finish_deferred_certification() (which may withdraw it) or
    // SsnEngine::discard_deferred_certification() (which drops the pending
    // evidence unread, legitimate only when the caller is discarding the
    // exit anyway). ALWAYS false when the option is off, on an escape, and
    // under kBare -- so a consumer that never sets the option never sees it
    // set.
    bool certification_deferred = false;

    // THE LEVER INSTRUMENTS live here rather than on SsnCounters DELIBERATELY:
    // they measure an opt-in arm, not the product, and SqpCounters::ssn is
    // serialized into the corpus CSV whose schema is a pinned artifact.
    //
    // `watchdog_returns` -- times the q-step watchdog exhausted its relaxed
    // window without Armijo decrease and RETURNED TO THE BEST STORED POINT.
    // Structurally 0 under SsnHintRule::kIterationZeroFree.
    //
    // `farkas_fired` / `farkas_refusals` -- infeasibility exits the Farkas
    // certificate CONFIRMED, and armed symptom pairs it REFUSED (i.e. the
    // shipped rule would have declared kInfeasible and this one did not).
    // Both structurally 0 under SsnInfeasibilityRule::kSymptoms.
    Index watchdog_returns = 0;
    Index farkas_fired = 0;
    Index farkas_refusals = 0;

    Vec x, lambda_e, lambda_i;
    // Signed bound multiplier, recombined from the two bound-row multipliers
    // (z_j = lambda^lo_j - lambda^up_j). Same convention as QpSolution::z.
    Vec z;

    // Newton steps ACCEPTED -- identical to counters.ssn_iters, which exists
    // separately only so that SqpCounters::ssn can aggregate the whole struct
    // without special-casing one field.
    Index iters = 0;
    // Numeric factorizations paid, one per ATTEMPT.
    //
    // **THE SAFEGUARDS SEPARATED THESE TWO**, and the gap is their cost read
    // directly: an attempt can pay its factorization and take NO step (the
    // schedule was exhausted, or the inertia came back wrong), so
    //
    //     iters <= factorizations <= min(attempts, hard_budget) + 1,
    //
    // where the +1 is the CERTIFYING EXIT's second-order verification (banner
    // banner section 7b) -- which is why equality on the left is unreachable under
    // kFull. **UNDER SsnOptions::defer_certification THE +1 IS NOT PAID BY
    // THIS SOLVE**: it moves to finish_deferred_certification(), which adds it
    // to THIS field when the caller runs it, or is never paid at all when the
    // caller's face solve supersedes it -- so a deferring solve can read
    // iters == factorizations at a certifying exit. Under kBare equality is
    // exact.
    Index factorizations = 0;
    // Pardiso phase-11 symbolic analyses paid. 1 for the first solve of a new
    // structure on this engine, 0 for every later solve of the same structure
    // -- banner section 3.
    Index symbolic_analyses = 0;
    // TRIPLET REBUILDS of K's sparsity pattern paid by this solve: 1 when the
    // structure differs from the one this engine currently holds, 0 when it
    // was reused and only VALUES were refreshed. The sibling of
    // symbolic_analyses one level down -- that field counts what PARDISO
    // re-derives, this one what THIS FILE re-derives.
    Index pattern_rebuilds = 0;

    // ||F(w)||_inf at the returned point, on the UNSCALED residual.
    double fb_residual = 0.0;

    // THE IMPLIED ACTIVE SET AT THE RETURNED POINT, in the two shapes
    // QpSolution reports it (qp_problem.h): one flag per row of Ai, one
    // BoundState per variable. Derived from the FINAL iterate by the engine's
    // own partition rule -- row active iff lambda > s, equivalently
    // alpha > beta (banner section 2) -- so it is the same partition the last
    // Jacobian would have selected, not a separately maintained working set.
    //
    // WRITE-ONLY, LIKE THE COUNTERS: nothing here reads either vector back,
    // and computing them cannot move a trajectory. They exist for the activity
    // export (warm-start hand-off and stable-face refinement).
    //
    // ALWAYS POPULATED, INCLUDING ON AN ESCAPE and on a solve that took zero
    // steps: the derivation reads the iterate, not the loop's history. On an
    // escaped solve it describes where the solve STOPPED and certifies
    // nothing.
    //
    // A variable whose lower AND upper bound rows are both implied active
    // reports kFixed; one with neither reports kFree. A variable with no finite
    // bound at all always reports kFree, since it has no row to be active.
    std::vector<bool> ineq_active;       // mi
    std::vector<BoundState> bound_state; // n

    // TRUST-REGION ACTIVITY, size n, qp_problem.h's QpSolution::tr_active
    // contract verbatim. True at index i iff the variable is held by a
    // TR-tight effective bound rather than a real one. Such a variable reports
    // kFree in bound_state above -- a REAL-BOUND-ONLY view -- and z(i) is 0,
    // because TR duals are internal to this solve and are never exposed.
    //
    // SAME STATIONARITY CAVEAT AS THE WALK'S: at a TR-pinned index the
    // reported quantities do NOT satisfy stationarity, since the multiplier
    // that balanced that row was dropped on the way out. A kFree entry there
    // is evidence that no REAL bound constrains the coordinate, NOT evidence
    // that the point is stationary in it. **Read tr_active, never z or
    // bound_state, to detect a binding radius.**
    //
    // All-false whenever the solve ran without a trust region (the default
    // SolveOverrides), and all-false when a finite radius never bound.
    std::vector<bool> tr_active; // n

    // **HOW FAR x LIES OUTSIDE THE TRUST REGION, AND IT CAN BE FAR.**
    // max_j max(0, x_j - up_eff_j, lo_eff_j - x_j) over the
    // variables whose effective bound came from the RADIUS -- 0.0 when no
    // radius was supplied, and 0.0 when the radius held.
    //
    // THE CONTRACT, stated here because the driver's funnel is the consumer
    // and its ratio test presumes ||d||inf <= Delta:
    //
    //   * THE TRUST REGION IS A SOFT CONSTRAINT IN THIS KERNEL. It enters as FB
    //     bound ROWS, and an FB row is satisfied only at a root -- so no
    //     intermediate iterate is confined to the box, the line search does not
    //     confine it either (the TR rows are four more terms in the same merit,
    //     not a step-length cap), and a solve that stops early can stop
    //     anywhere. MEASURED on cycling_qp_3var from x0 = 0 at
    //     tr_radius = 0.01: hard_budget 1..5 return ||x||inf between 1.33 and
    //     1.60, i.e. 133x to 160x the radius.
    //   * ON A CERTIFYING EXIT the violation is bounded by the tolerance and
    //     nothing else: |phi| <= fb_tol permits a slack negative by O(fb_tol)
    //     (banner section 5's hypothesis caveat, applied to the TR rows), so
    //     tr_violation <= kSsnComplementarityFactor * fb_tol ~ 1.71 fb_tol.
    //     That is the only exit at which this kernel's x may be used as a
    //     trust-region step.
    //   * ON ANY ESCAPE the value is UNBOUNDED and is reported rather than
    //     repaired. It is deliberately NOT clamped: clamping would break the
    //     export's one invariant -- that x, fb_residual, ineq_active,
    //     bound_state, tr_active and the uncertain flags all describe ONE
    //     point -- and hand the funnel a point whose residual it had never
    //     been measured at. The driver routes every escape to the walk; this
    //     field is what lets it ASSERT that it did.
    double tr_violation = 0.0;

    // THE UNCERTAIN SET AT THE RETURNED POINT, the third leg of the CHR
    // partition that ineq_active/bound_state cannot express -- those two
    // report the BINARY reading (lambda > s), because the driver's hint ingest
    // and QpSolution's own contract are binary.
    //
    // ineq_uncertain[k] is true iff row k of Ai was in the uncertain set when
    // the LAST generalized Jacobian was assembled; bound_uncertain[j] is true
    // iff EITHER of variable j's bound rows was -- the pessimistic reading,
    // since the flag exists to warn a consumer off trusting the binary one.
    //
    // **THE LAST ASSEMBLY**, which under kFull is the RETURNED ITERATE on every
    // certifying exit (the second-order verification classifies there, banner
    // banner section 7b) and is one iterate back on an escape. The distinction
    // matters because the classification carries HYSTERESIS: it is a function
    // of the whole trajectory and cannot be recomputed from the final point.
    // On a solve that assembled no Jacobian at all both vectors are all-false
    // -- the honest reading of "no classification was ever made", not a claim
    // that every row was decided. Always sized (mi / n).
    //
    // ALWAYS ALL-FALSE UNDER SsnSafeguards::kBare, which has no uncertain set.
    std::vector<bool> ineq_uncertain;  // mi
    std::vector<bool> bound_uncertain; // n

    // The proximal sigma this solve FINISHED at (0.0 whenever the ladder never
    // armed, which is every benign fixture). Write-only, like the counters, and
    // the one number a caller needs to restart a proximal sequence through
    // SsnOptions::prox_sigma_init.
    double prox_sigma = 0.0;

    // Why the solve stopped, in words, whenever there are words to add: the
    // linear solver's own message on kSingular, the stall/growth figures on
    // kInfeasibleSuspect, the exhausted schedule on kNoContraction. Carried
    // out rather than printed, per CLAUDE.md banner section 4's rule that a
    // diagnostic must fold into what the caller receives. Empty on kNone and
    // on kBudget, which say everything they have to say in the enum.
    std::string escape_detail;

    SsnCounters counters;
};

// The engine.
//
// Holds ONE KktFactor across solves, which is what makes banner section 3's
// "symbolic analysis once per structure, reused across QPs of identical
// structure" property observable: construct one SsnEngine and solve a
// sequence of same-shaped QPs through it.
//
// NOT THREAD-SAFE and not copyable, for the same reason the sparse factor is
// not: it owns live Pardiso/Accelerate internal state.
class SsnEngine {
  public:
    explicit SsnEngine(const QpOptions &opts) : opts_(opts) {}

    SsnEngine(const SsnEngine &) = delete;
    SsnEngine &operator=(const SsnEngine &) = delete;

    const QpOptions &options() const { return opts_; }

    // Solve `qp` from `start`. `out` must be non-null and is fully overwritten.
    //
    // Throws std::invalid_argument for a null `out`, a malformed QpProblem
    // (qp.validate()'s own diagnostics), a wrongly sized start/hint vector, or
    // an out-of-range SsnOptions field -- i.e. for caller errors, which are
    // distinguished from SOLVER outcomes: a solve that cannot make progress
    // reports a status and an SsnEscape and does not throw.
    //
    // This overload forwards a default-constructed SolveOverrides, which
    // resolves to every opts_ value unchanged, so it is BYTE-IDENTICAL to
    // solving with no override support at all -- the same guarantee
    // QpEngine's plain overloads carry (qp_types.h).
    void solve(const QpProblem &qp, const SsnStart &start, const SsnOptions &sopts, SsnResult *out);

    // THE PER-SOLVE SEAM. Takes the WALK'S OWN SolveOverrides (qp_types.h)
    // rather than a parallel type, because the funnel driver already builds
    // one per subproblem and per SOC re-solve. Every field's sentinel and
    // resolution rule is qp_types.h's, unchanged: tr_radius +inf means "no
    // radius", primal_delta/dual_mu negative means "use the engine's".
    //
    // TRUST REGION. lo_eff = max(lower, x0 - Delta), up_eff = min(upper,
    // x0 + Delta), about THIS solve's own start point, resolved once here and
    // used for every bound row thereafter -- see build_bound_rows for the
    // pattern-invariance property that makes a shrink-retry loop free, and
    // export_activity/recombine_bound_multipliers for the TR-pin/real-bound
    // separation that keeps radius artefacts out of z and bound_state.
    //
    // REGULARIZERS. primal_delta/dual_mu are honoured too -- ignoring a field
    // the caller deliberately set is the silent-drop antipattern. **BUT THE
    // DRIVER'S ADAPTIVE-mu SCHEDULE BUYS NOTHING HERE**: delta and mu perturb
    // only the JACOBIAN (for_each_entry's emissions and the FB diagonal) and
    // never the residual, so the iteration is modified Newton on the EXACT F
    // and its fixed points are exact, unregularized KKT points. They cost
    // iterations, not accuracy -- the opposite trade from the walk, whose
    // returned solution carries the mu|lambda| footprint that motivated the
    // schedule.
    void solve(const QpProblem &qp, const SsnStart &start, const SsnOptions &sopts,
               const SolveOverrides &overrides, SsnResult *out);

    // THE DEFERRED CERTIFICATION'S TWO CLOSING MOVES. Both throw if called
    // when nothing is pending -- a caller that closes a deferral twice, or
    // closes one it never opened, has lost track of which point it is
    // certifying.
    //
    // finish_deferred_certification() PAYS THE FACTORIZATION THE SOLVE DID
    // NOT. It factorizes the matrix the solve left in place -- classified at
    // the converged point, at the caller's own regularization -- so its
    // verdict is EXACTLY the one banner section 7b's in-loop verification
    // would have returned, at the same cost. On kWrong it rewrites `out` into
    // the SsnEscape::kIndefinite escape the in-loop route issues, census
    // included; on a thrown factorization into SsnEscape::kSingular the same
    // way. `out` MUST be the SsnResult the deferring solve wrote.
    //
    // Returns true iff the certificate stands.
    bool finish_deferred_certification(SsnResult *out);

    // DROPS THE PENDING EVIDENCE UNREAD. Legitimate on exactly one path: the
    // caller is discarding the exit anyway, so no certificate is being claimed
    // and no step taken from that point. Calling it while a certificate IS
    // being claimed is the wrong-answer class banner section 7b closes --
    // which is why it is a separate, named call rather than a default.
    void discard_deferred_certification();

    // Whether this engine owes a caller one of the two calls above.
    bool has_deferred_certification() const { return deferred_pending_; }

  private:
    // -----------------------------------------------------------------------
    // Validation
    // -----------------------------------------------------------------------
    // qp_types.h's SolveOverrides precondition, applied unchanged: tr_radius
    // must be the +inf sentinel or >= 0 -- never negative (a negative Delta
    // would silently cross lo_eff and up_eff) and never NaN;
    // primal_delta/dual_mu keep the negative-means-sentinel convention but
    // reject NaN, which no downstream arithmetic can absorb.
    static void validate_overrides(const SolveOverrides &o);

    static void validate_options(const SsnOptions &s);

    void validate_start(const QpProblem &qp, const SsnStart &start) const;

    static void check_size(const Vec &v, Index want, const char *what);

    static Vec seed_vector(const Vec &v, Index want, const char *);

    // -----------------------------------------------------------------------
    // Bound rows
    // -----------------------------------------------------------------------
    // Builds the bound-row list from the EFFECTIVE bounds
    //
    //     lo_eff = max(lower, x0 - Delta),   up_eff = min(upper, x0 + Delta)
    //
    // -- qp_types.h's SolveOverrides::tr_radius contract for the walk, applied
    // here unchanged, including "about the SOLVE'S OWN start point x0" and
    // "computed once, at the top of solve()". A row is marked from_tr iff the
    // trust region is STRICTLY tighter than the real bound on that side (a tie
    // reads as the real bound, since the reported activity is then genuine).
    //
    // WITH A FINITE RADIUS EVERY VARIABLE GETS BOTH ROWS, whatever the radius
    // is, so the row list -- and therefore K's pattern -- is INVARIANT across
    // radius changes. That is what makes a shrink-retry loop free: only bound
    // VALUES move, and values are not structure. Delta = +inf reproduces the
    // real-bound list exactly, bit for bit (max(lower, -inf) is lower).
    void build_bound_rows(const QpProblem &qp, const Vec &x0, double tr_radius);

    // A kFixed hint marks BOTH of a variable's rows active, which is right for
    // a genuinely fixed variable (l == u) and is a DOCUMENTED DEGRADED MODE
    // when l < u.
    //
    // HOW IT ARISES: export_activity reports kFixed whenever both of a
    // variable's bound rows come out implied-active, which a noisy or ESCAPED
    // iterate can produce on an l < u variable -- and that export is exactly
    // what the driver re-ingests as a warm-start hint. The first step then
    // solves the contradictory pair {x_j = l_j, x_j = u_j}, which the dual-mu
    // block regularizes into the midpoint rather than a singular
    // factorization.
    //
    // **THE CHOICE IS DOCUMENTED RECOVERY, NOT REJECTION.** Rejecting would
    // make a warm-start hand-off THROW because the previous solve happened to
    // stop somewhere noisy, and push every caller into sanitising a hint the
    // kernel itself produced. A hint is ADVICE about the first step, not a
    // constraint: every later step re-derives the partition from the FB
    // branch, so a contradictory hint costs iterations and nothing else.
    // Masking kFixed at EXPORT was rejected too -- it would suppress a real
    // signal and cannot help a caller who builds the hint elsewhere.
    static bool bound_hint_active(const std::vector<BoundState> &hint,
                                  const detail::SsnBoundRow &br);

    // Splits the signed z into the two non-negative bound-row multipliers.
    //
    // **A SEEDED z THAT PRICES A BOUND THIS QP DOES NOT HAVE IS A CALLER ERROR
    // AND THROWS.** z(j) > 0 prices variable j's LOWER bound and z(j) < 0 its
    // UPPER one; if that side is absent (at or beyond kSsnInfBound, so there
    // is no row for the multiplier to live on) the mass has nowhere to go.
    // A real hazard rather than a tidiness point: warm_start.h records exactly
    // this shape (a z priced against a 1e20 bound) poisoning a downstream
    // estimate, and an ingest handing one model's z to a differently bounded
    // model would produce it.
    //
    // THE TEST IS EXACT ZERO, not a tolerance: an absent side has no
    // multiplier at all, so the only correct value there is 0, and any nonzero
    // -- however small -- means the caller believes in a bound this QP does
    // not have.
    //
    // **THE ABSENT-SIDE TEST READS THE REAL BOUND, NOT THE EFFECTIVE ONE**, and
    // a TR row is seeded at zero. A caller's z prices the QP's OWN bounds; the
    // trust region is this solve's private construction, so a finite radius
    // must not turn "you priced a bound that does not exist" into silence
    // (every variable has finite effective bounds under a finite radius, which
    // would make the check vacuous). TR duals are internal, exactly as
    // qp_problem.h's tr_active contract requires on the way out.
    Vec split_bound_multipliers(const QpProblem &qp, const Vec &z) const;

    // **TR ROWS ARE EXCLUDED**, which is qp_problem.h's contract verbatim: "TR
    // duals are internal to the ratio test/drop rule and are never exposed",
    // and z(i) is forced to 0 at a TR-pinned index. A caller reading z back
    // therefore sees only multipliers of the QP's own bounds, whatever radius
    // the solve ran under -- the property that lets a warm-start export survive
    // a shrink-retry loop without accumulating radius-dependent junk.
    Vec recombine_bound_multipliers(const Vec &lb, Index n) const;

    // THE FIXED PATTERN, AND ITS REUSE ACROSS SOLVES.
    //
    // THE FB DIAGONALS GET A NONZERO PLACEHOLDER (-1.0) rather than their
    // eventual value, so their slots exist regardless of what any later branch
    // wants there (beta/alpha_f + mu is legitimately EXACTLY 0 on a strictly
    // active row when the caller zeroes dual_mu), and the diagonal refresh
    // addresses those slots positionally (K.outerIndexPtr()[row]) rather than
    // by search. Defensive rather than strictly required by the vendored Eigen
    // -- setFromTriplets preserves explicit zeros today, but prune()/assignment
    // paths drop them elsewhere -- and the cost of not depending on it is one
    // literal.
    //
    // sync_matrix() decides between two paths and reports which:
    //
    //   REBUILD (returns true) -- the structure differs from the cached one:
    //     emit every entry as a triplet, setFromTriplets, then record each
    //     emitted entry's position in the compressed value array (one binary
    //     search per entry, over its own row) so the reuse path can address
    //     them positionally.
    //   REFRESH (returns false) -- same structure: zero the value array and
    //     re-emit, accumulating into the recorded positions. O(nnz), no sort,
    //     no allocation, and Pardiso's cached symbolic analysis survives
    //     because K's index arrays were never touched.
    //
    // THE TWO PATHS SHARE ONE EMISSION ORDER BY CONSTRUCTION -- for_each_entry()
    // is the single walk both drive -- so they cannot drift apart.
    //
    // ACCUMULATION, NOT ASSIGNMENT, on the refresh path: setFromTriplets SUMS
    // duplicates, and for_each_entry emits duplicates deliberately (H's diagonal and
    // the separate primal_delta entry land on the same slot). Zeroing first and
    // then += reproduces that summation exactly; a straight = would silently
    // drop primal_delta wherever H has a stored diagonal.
    //
    // THE STRUCTURE KEY is a COMPOSITE of two conjuncts:
    //
    //   1. structure_hash -- hven's combined pattern key (feed_pattern,
    //      docs/pattern-hash.md), the same instrument the KKT factor helper's
    //      pattern compare uses to decide whether to skip the symbolic
    //      analysis (kkt_calls.h).
    //   2. the bound-row (var, sign) list, compared EXACTLY against the copy
    //      cached when the current pattern was built (bound_rows_match_cached
    //      / structure_bound_key_) -- not hashed, so no collision exposure at
    //      all on this half.
    //
    // A COLLISION IN CONJUNCT 1 IS GUARDED, NOT MERELY ASSERTED AWAY: the
    // refresh path bounds-checks every write and requires that the emission
    // consumed the map EXACTLY (t == value_pos_.size()), converting a
    // structure-mismatched refresh into a thrown std::runtime_error rather
    // than undefined behaviour. A collision between two structures with the
    // SAME entry count remains undetected -- a 64-bit FNV coincidence, and the
    // honest residual exposure; the guard is unreachable by any fixture, so it
    // is a knowingly-unkillable defensive line like the placeholder above.
    template <typename Emit>
    void for_each_entry(const QpProblem &qp, Index n, Index me, Index mi, Index mb,
                        Emit emit) const;

    // CONJUNCT 1 of the composite structure key: hven's combined pattern key
    // over the dimensions and the three input patterns. Fed **through
    // feed_pattern, NOT off the raw index arrays**: qp.H/Ae/Ai are
    // CALLER-SUPPLIED and QpProblem imposes no compression requirement, and
    // feed_pattern's contract is that either storage state produces the
    // compressed digest -- same O(nnz), no compressed copy, exact in both
    // states. Every ingredient goes through Fnv1a::feed_index (64-bit widened,
    // LSB-first), so the digest depends on neither host byte order nor
    // SpMatRM::StorageIndex's width. The bound-row list is DELIBERATELY not
    // here: it is conjunct 2 (bound_rows_match_cached), compared exactly
    // rather than hashed. The digest's VALUE is only ever compared against
    // another computation of this same function on this same engine.
    std::uint64_t structure_hash(const QpProblem &qp, Index n, Index me, Index mi, Index mb) const;

    // CONJUNCT 2: the bound-row (var, sign) list, compared EXACTLY against
    // the copy cached when the current pattern was built.
    //
    // **br.var IS THE LOAD-BEARING HALF, AND mb ALONE DOES NOT COVER IT.**
    // The case mb misses is a DIFFERENT ASSIGNMENT of the same NUMBER of bound
    // rows to variables -- two rows both on variable 0 versus one row each on
    // variables 0 and 1 -- which puts the bound block's off-diagonal entries
    // in different columns of K. Reusing a pattern across that would write
    // B's values into A's slots.
    //
    // sign participates too, CONSERVATIVELY rather than necessarily: a sign
    // flip (a variable trading its lower bound for its upper) moves no slot,
    // only a value, and the refresh path re-emits br.sign. It is kept because
    // an over-conservative key costs one avoidable rebuild in that one case,
    // and because rebuild COUNTS are pinned currency.
    //
    // rhs and from_tr are EXCLUDED: both are value attributes of a row whose
    // slot they do not move (see SsnBoundRow), and comparing either would
    // rebuild the pattern on bound VALUE moves -- from_tr on every radius
    // change of a shrink-retry loop, which is exactly the tax this cache
    // exists to remove. The cache is a (var, sign) pair list, not a
    // SsnBoundRow copy, so neither CAN be compared by accident.
    bool bound_rows_match_cached() const;

    // Brings k_ up to date for `qp`. Returns true iff the pattern was rebuilt.
    bool sync_matrix(const QpProblem &qp, Index n, Index me, Index mi, Index mb);

    // -----------------------------------------------------------------------
    // The activity export
    // -----------------------------------------------------------------------
    //
    // The engine's own partition rule, applied to the returned iterate: a row
    // is active iff its multiplier exceeds its slack (equivalently
    // alpha > beta, banner section 2). Written, never read.
    void export_activity(SsnResult *out, const Vec &slack_i, const Vec &slack_b,
                         const Vec &lambda_b, Index n, Index mi) const;

    // The uncertain export. `klass` is null when no Jacobian was ever
    // assembled, which is the honest "no classification was made" case and is
    // reported as all-false rather than as all-decided.
    void export_uncertain(SsnResult *out, const std::vector<detail::SsnRowClass> *klass, Index n,
                          Index mi) const;

    // -----------------------------------------------------------------------
    // The globalization helpers
    // -----------------------------------------------------------------------

    // ||(lambda_e, lambda_i, lambda_b)||_inf -- the divergence telemetry's
    // growth measure. The EQUALITY multipliers are included even though they
    // are sign-free: an inconsistent equality block diverges exactly the same
    // way an inconsistent inequality block does, and excluding them would make
    // the test blind to the commonest infeasibility an SQP linearization
    // produces.
    static double dual_norm(const Vec &le, const Vec &li, const Vec &lb);

    // w + step * dw, with THE DUAL PROJECTION applied to the two non-negative
    // multiplier blocks.
    //
    // **THE PROJECTION IS THE WRONG-HINT MITIGATION**, and the only cheap one
    // available: the landing configuration (s, lambda) = (0, lambda < 0) is a
    // DIFFERENTIABLE point of phi -- subdifferential the singleton {(1, 2)} --
    // so no derivative re-selection can help. A wrongly hinted active row
    // lands with a NEGATIVE multiplier and is confined to the line
    // s+ + 2 lambda+ = 2 s0 - (2 s0/|lambda| + mu) d lambda, on which a
    // non-degenerate row's root does not lie; clipping lambda to 0 moves the
    // pair onto the kink, where the symmetric subdifferential element applies
    // and the confinement dissolves.
    //
    // It is a PROJECTION ONTO A CONVEX SET THAT CONTAINS EVERY SOLUTION
    // (lambda >= 0 is a KKT condition), so it cannot move the iterate away
    // from the solution set, and it is inert at any point that already
    // satisfies it. lambda_e is untouched: equality multipliers are free-sign
    // and clipping them would be a wrong answer, not a safeguard.
    static void trial_point(const Vec &x, const Vec &le, const Vec &li, const Vec &lb,
                            const Vec &dw, double step, bool project, Index n, Index me, Index mi,
                            Index mb, Vec &t_x, Vec &t_le, Vec &t_li, Vec &t_lb);

    // Sets sigma and re-emits K's VALUES for it. The proximal increment lands
    // on the same two slots primal_delta and dual_mu already occupy, so the
    // pattern is untouched and sync_matrix takes its REFRESH path -- O(nnz), no
    // sort, no allocation, and Pardiso's cached symbolic analysis survives. The
    // return value is ignored deliberately: a refresh is not a pattern rebuild
    // and must not be counted as one.
    void set_prox_sigma(double sigma, const QpProblem &qp, Index n, Index me, Index mi, Index mb);

    // sigma = max(the ladder's monotone state, the residual-driven size).
    // Under the shipped kLadder rule `lm_sigma_` is identically 0, so this is
    // `set_prox_sigma(ladder_sigma_, ...)` and `prox_sigma_ == ladder_sigma_`
    // at every point the shipped iteration can observe.
    void apply_sigma(const QpProblem &qp, Index n, Index me, Index mi, Index mb);

    // One rung up the ladder. Returns false at the ceiling, which is the
    // caller's signal to escape rather than to keep paying factorizations.
    //
    // **THE CEILING TEST CARRIES A RELATIVE SLACK**, because the rungs are
    // computed by repeated multiplication and do not land on the cap exactly.
    // detail::kSsnProxCapSlack carries the derivation.
    bool escalate_prox(const QpProblem &qp, Index n, Index me, Index mi, Index mb);

    // -----------------------------------------------------------------------
    // THE FARKAS RESIDUAL TEST, matvec-only
    // -----------------------------------------------------------------------
    //
    // The QP's constraint system is {Ae x = be} together with the
    // inequality-shaped rows a_k^T x <= b_k of banner section 1 -- the rows of
    // Ai and every finite bound row alike. Farkas' lemma: that system is
    // INFEASIBLE if and only if there is (y_e free, y >= 0) with
    //
    //     Ae^T y_e + sum_k y_k a_k = 0    and    <be, y_e> + sum_k y_k b_k < 0.
    //
    // `dle`/`dli`/`dlb` is the DUAL INCREMENT to be tested. It is projected
    // onto the sign cone (the equality block passes through; the two
    // non-negative blocks are clipped at 0), normalized by its own inf-norm,
    // and the two Farkas quantities are evaluated: the RESIDUAL
    // ||Ae^T y_e + Ai^T y_i + B^T y_b||inf and the OBJECTIVE <b, y>, each
    // reported RELATIVE to the same combination taken in absolute value -- the
    // cancellation-free scale of the sum, so a badly scaled row cannot make a
    // non-certificate look like one or vice versa.
    //
    // One matvec over Ae/Ai plus O(mb) plus O(n): no factorization, no solve,
    // and no allocation beyond two n-vectors.
    //
    // Returns true iff both tolerances are met, i.e. iff the increment IS an
    // approximate Farkas direction. `resid_out`/`gap_out` carry the two
    // relative quantities for the caller's diagnostic message.
    bool farkas_certificate(const QpProblem &qp, const Vec &dle, const Vec &dli, const Vec &dlb,
                            Index mb, double *resid_out, double *gap_out) const;

    // -----------------------------------------------------------------------
    // Residual
    // -----------------------------------------------------------------------

    // Fills every scratch block and returns both norms of F: the inf-norm that
    // certifies (banner section 5) and the merit 1/2||F||_2^2 the line search
    // decreases. Both come out of the same walk over the same blocks.
    detail::SsnNorms residual(const QpProblem &qp, const Vec &x, const Vec &le, const Vec &li,
                              const Vec &lb, Vec &resid_x, Vec &resid_e, Vec &slack_i, Vec &slack_b,
                              Vec &phi_i, Vec &phi_b) const;

    // BRANCH SELECTION (banner sections 2 and 4).
    //
    // Writes alpha/beta/row_resid/klass at slot `slot`. `hinted` says a hint
    // governs THIS row (first step, and the relevant half of the hint was
    // supplied); `hint_active` is that hint's verdict. `guarded` selects the
    // three-set classification and the uncertain damping; under kBare this
    // function IS the bare local method, statement for statement.
    //
    // THE THREE-SET PARTITION (CHR 2015's scaffold). The FB pair's own margin
    // |alpha - beta| measures how far the row is from the kink ray s = lambda,
    // where its classification is undecidable (detail::kSsnUncertainEnter
    // derives the geometry). The classification is read with HYSTERESIS --
    // enter the uncertain set at `tau`, leave it only at
    // kSsnUncertainLeaveRatio * tau -- so a row whose margin hovers inside the
    // band keeps whatever class it had and cannot chatter.
    //
    // THE TIE POLICY IS A CONSEQUENCE OF THE BAND, NOT A SEPARATE RULE: an
    // exact tie (alpha == beta) has margin 0, inside every band with tau > 0,
    // so a tie NEVER decides anything -- a previously decided row becomes
    // uncertain and a previously uncertain row stays uncertain. (kBare's
    // binary rule, and the activity export, break an exact tie toward INACTIVE
    // via a strict >, since QpSolution's contract is binary.)
    //
    // WHAT AN UNCERTAIN ROW GETS: BOTH BRANCHES DAMPED. The FB pair is
    // replaced wholesale by the SYMMETRIC element
    // (alpha, beta) = (1 - 1/sqrt(2), 1 - 1/sqrt(2)) -- the same
    // detail::kSsnDegenerateFbDeriv the kink itself uses: the row's diagonal
    // becomes -(1 + mu + sigma) instead of racing toward -mu (a hard equality)
    // or toward -2e12 (a decoupled row), and its coupling to dx keeps a
    // moderate 0.293 weight instead of 1 or ~0.
    //
    // AND IT IS STILL A GENERALIZED JACOBIAN ELEMENT, which is what keeps this
    // a semismooth Newton method rather than a heuristic: with
    // (xi, eta) = (1 - alpha, 1 - beta), the C-subdifferential of phi at the
    // kink is exactly {(1 - xi, 1 - eta) : xi^2 + eta^2 <= 1}, the symmetric
    // element sits ON that unit circle, and the row it is applied to is within
    // O(tau) of the kink by construction -- an inexact generalized Jacobian
    // with a bounded, stated error, not a branch.
    //
    // WHY THE HINT OVERRIDES THE CLASSIFICATION. A hinted row takes its hint,
    // uncertain or not: an exact-solution seed can put an active row exactly
    // at the kink (s, lambda) = (0, 0), so a classification that outranked the
    // hint would damp precisely the row the hint exists to snap onto its face.
    // The hint governs step 0 only; every step after it is classified.
    static void select_branch(double s, double lam, double phi, bool hinted, bool hint_active,
                              bool guarded, double tau, std::vector<double> &alpha,
                              std::vector<double> &beta, std::vector<double> &row_resid,
                              std::vector<detail::SsnRowClass> &klass, Index slot);

    QpOptions opts_;
    detail::KktFactor kkt_;
    SpMatRM k_;
    Index dim_ = 0;
    std::vector<detail::SsnBoundRow> bound_rows_;
    std::vector<double> real_lower_, real_upper_;
    // The pattern cache: the structure key k_ was built for -- both conjuncts
    // of it (the combined pattern digest, and the bound-row (var, sign) list
    // the digest deliberately omits) -- and the position in k_.valuePtr() of
    // each entry for_each_entry() emits, in emission order.
    std::vector<std::size_t> value_pos_;
    std::uint64_t structure_key_ = 0;
    std::vector<std::pair<Index, double>> structure_bound_key_;
    bool has_structure_ = false;
    // The regularizers THIS solve resolved to (SolveOverrides, or opts_ when
    // the override is at its sentinel). Members rather than parameters because
    // for_each_entry is driven from two places and both must see the same pair.
    //
    // base_* is what the CALLER asked for and never moves inside a solve;
    // eff_* is base_* + prox_sigma_ and is what for_each_entry emits. Keeping
    // both is what lets the proximal ladder step without losing the caller's
    // own regularization (a ladder that overwrote eff_* would silently reset
    // primal_delta to sigma).
    double base_delta_ = 0.0;
    double base_mu_ = 0.0;
    double eff_delta_ = 0.0;
    double eff_mu_ = 0.0;
    // The proximal-point regularizer. Per-solve, monotone non-decreasing,
    // 0.0 whenever the escalation ladder never armed.
    double prox_sigma_ = 0.0;
    // `ladder_sigma_` is the LADDER's own monotone state and `lm_sigma_` the
    // residual-driven size; `prox_sigma_` is their max.
    // Under the shipped SsnSigmaRule::kLadder `lm_sigma_` never leaves 0, so
    // `ladder_sigma_ == prox_sigma_` identically and the pair is invisible.
    double ladder_sigma_ = 0.0;
    double lm_sigma_ = 0.0;

    // The deferred-certification pending-verdict state. `deferred_pending_`
    // is the whole of the contract: while it is true, k_ holds the matrix a
    // certifying exit declined to factorize and kkt_ must not be touched. The
    // other five fields are only what the withdrawal message needs, saved
    // because the loop that knew them has exited.
    bool deferred_pending_ = false;
    Index deferred_pos_ = 0;
    Index deferred_neg_ = 0;
    Index deferred_attempt_ = 0;
    double deferred_fb_residual_ = 0.0;
    double deferred_fb_tol_ = 0.0;
};

} // namespace hven::solvers
