// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// eqp_solve.h — the equality-constrained QP solve over a fixed working set:
// minimizes the QP objective subject to the working inequalities held at
// equality (Ai_row . x = bi_row for rows in ws.active_ineq()) and the
// non-free variables pinned at their bound (qp.lower(i) or qp.upper(i), per
// ws.bound_state()). This is the numerical core every active-set iteration
// calls once its working set is fixed for the step.
//
// --- KKT assembly and rhs ---
//
// assemble_kkt (kkt_assembly.h) builds the regularized, bound-eliminated
// system
//     K = [ H_FF + delta*I   Ae_F^T   Aw_F^T ;
//           Ae_F              -mu*I    0     ;
//           Aw_F              0        -mu*I ]
// with rows [free Hessian | equality | working inequality], sizes
// [n_free | me | n_working], and rhs_shift holding the fixed-variable
// substitution terms in the SAME row layout (see kkt_assembly.h for the
// exact definition). Given the stationarity condition
//     grad(f) + Ae^T lambda_e + Ai^T lambda_i - z = 0    (z == 0 on free vars)
// restricted to the free rows, and the constraint equations Ae x = be,
// Aw x = bw (bw = bi restricted to working rows) restricted to their fixed-
// variable contributions, the solve's right-hand side is
//     rhs = [ -g_F - hess_shift + delta*x_k,F ; be - eq_shift ; bw - ineq_shift ]
// where hess_shift/eq_shift/ineq_shift are rhs_shift's three blocks (in the
// same row order as K). Solving K y = rhs for y = [x_F; lambda_e; lambda_w]
// and scattering x_F back alongside the fixed variables' bound values gives
// the full-space answer.
//
// --- THE PROXIMAL CENTRE `x_k` (M7 W2 E4) ---
//
// The `+ delta*x_k,F` term above is what makes this EQP solve for the STEP
// from the iterate it is called at. The problem solved is
//
//     min  (1/2) x' H x + g' x + (delta/2)||x_F - x_k,F||^2
//     s.t. Ae x = be,  Aw x = bw,  x_P pinned at its bound
//
// whose free-block stationarity row is
//     (H_FF + delta I) x_F + Ae_F' le + Aw_F' lw = -g_F - hess_shift + delta*x_k,F
// and, written in the step p = x - x_k (x_k,P already AT its pins, so
// hess_shift is unchanged by the substitution),
//     (H_FF + delta I) p_F + Ae_F' le + Aw_F' lw = -(g + H x_k)_F - hess_shift.
// The two forms are the same equation; the absolute one is what the code
// assembles, because K is then BIT-IDENTICAL to the unshifted assembly and only
// the free block's rhs moves. `x_k` is the caller's current iterate, and it is
// REQUIRED rather than defaulted: a solve with x_k = 0 is the ABSOLUTE centring
// this engine used before E4, which is a different system, and no call site may
// reach it by omission.
//
// WHY PROXIMAL AND NOT ABSOLUTE. Centred at 0, the regularization minimises
// (delta/2)||x_F||^2 along the face, i.e. it pulls the iterate toward the
// ORIGIN. On a FLAT face (Z'HZ = 0, the restoration LPs and every PSD-singular
// face) that is the only term with any curvature, so the answer is the
// least-norm point of the face and the ORDINARY step ASCENDS f by a positive
// fraction of (delta/2)(||x_F||^2 - ||x+_F||^2): measured on EVERY ordinary
// pass of three exact cycles, with the period's energy ledger closing to 8e-19
// (M7 W2 E3 §1a). Centred at x_k the same term is (delta/2)||p_F||^2, so the
// answer is a DESCENT direction for the face's model by construction and the
// stationarity residual it prices carries -delta*p rather than -delta*x_F --
// zero in the limit, which is what makes the multipliers at a converged point
// exact. Proximal-point regularization of a KKT system in this form is standard:
// Friedlander & Orban 2012 (*A primal-dual regularized interior-point method for
// convex quadratic programs*) and Gill & Wong 2015 (regularized SQP); papers
// only, no source read.
//
// THE PRICE, AND WHERE IT IS PAID. On a flat face the answer is
// p = -(1/delta) P_Z (H x_k + g) (P_Z the projector onto the face's null space),
// so p carries a factor 1/delta and delta*||p|| -- not ||p|| -- is the reduced
// gradient's size. Two of the WALK's tests therefore cannot be written on ||p||
// and are not: its KKT-point test is the free-block stationarity residual and
// its runaway guard is on the iterate and on the ridge scale (qp_engine.h
// section 5). This header's business is the system; that one's is the tests.
//
// --- Sign convention ---
//
// The K/rhs pairing above reproduces exactly the stationarity/constraint
// block the dense oracle solves (tests/sqp/support/dense_oracle.h), modulo the
// delta/mu regularization on the diagonal: solving [H C^T; C 0] y = [-g; d]
// yields multipliers that satisfy grad(f) + C^T lambda = 0 DIRECTLY, with no
// sign flip needed. Concretely:
//   - lambda_e is returned with the SAME sign as QpProblem's stationarity
//     multiplier for Ae x = be — no adjustment needed (verified against the
//     oracle's lambda_e in the EqualityOnly-style test).
//   - lambda_w (one entry per row of ws.active_ineq(), in that sorted
//     order) IS lambda_i for those rows directly: lambda_i >= 0 at
//     optimality (qp_problem.h's convention). A negative lambda_w entry
//     signals that row should be DROPPED from the working set.
//   - Bound multipliers z are NOT computed here (EqpResult has no z field).
//     qp_engine.h's price() prices them from the stationarity residual at a
//     non-free variable instead, i.e. z(i) = (Hx + g + Ae^T lambda_e +
//     Ai^T lambda_i)(i) for that i. Per qp_problem.h's convention, z >= 0 is
//     required at an active LOWER bound and z <= 0 at an active UPPER
//     bound; a kFixed variable's z is not sign-constrained.
//
// --- Iterative refinement ---
//
// With mu = 1e-8 (QpOptions default) the regularized solve above differs from
// the exact PROXIMAL equality-QP solution at O(1e-8) in the constraint rows.
// K0 (the system the model actually poses) equals K minus its DUAL
// regularization diagonal, K0 = K - diag(reg) with reg(k) = 0 for Hessian rows
// and -mu for constraint rows, so that residual at y is exactly
//     r = rhs - K0*y = (rhs - K*y) + diag(reg)*y
// computable WITHOUT ever forming K0. Solving K*correction = r against the
// SAME (already-factorized) regularized system and adding the correction to y
// is one step of iterative refinement, bringing accuracy from ~1e-8 to
// ~1e-10..1e-12 — load-bearing for later tolerances.
//
// THE PRIMAL `delta` IS NOT BACKED OUT, and that is the other half of the
// proximal pair (M7 W2 E4, ruling E4-1). Once the regularization is centred at
// x_k it is a TERM OF THE MODEL -- the proximal penalty -- and not an error:
// backing it out targets a system that on a flat face is inconsistent (the
// unregularized face has no minimiser along its null space at all), which is
// measurably worse (E3 §7, mode 1: the refinement step's own residual reaching
// 3.6e+41 there). Keeping it targets the proximal system, whose residual at a
// converged point is -delta*p -> 0, so the fixed point stays exact -- which is
// why the pair improves the terminal KKT residuals by orders of magnitude where
// keeping delta WITHOUT the proximal centring degrades them by four decades
// (E3 §3l, mode 3). The two halves are one change.
//
// --- THE EQP-SITE RESIDUAL CHECK (M7 W2 E4, ruling E4-4) ---
//
// A regularized solve can also simply be WRONG: on a face with cond(K) ~ 1e11
// whose free block along an O(10)-dimensional null space is delta*I, the
// factor's own answer was measured 2.57e+02 (and, on another cell, 1.34e+14)
// away from solving ITS OWN regularized system, entirely in the constraint rows,
// while a dense re-solve of the same system landed 8e-08 off those rows and
// 3.6e-14 after one refinement step (E3 §1c). That is a LINEAR-SOLVE defect,
// not a model one, and a step toward such an answer is a step toward nothing.
//
// So, before the model's refinement step: r = rhs - K*y is formed (one mat-vec)
// and split; if its CONSTRAINT-ROW part exceeds
// kEqpResidualRefuseTol*(1 + ||rhs_rows||inf) the answer is not accepted. The
// refinement is iterated against that same REGULARIZED system -- the system the
// factor is supposed to solve exactly -- for at most kEqpResidualRefineIters
// solves, each kept only while the row residual strictly falls; and if it still
// fails, the answer is REFUSED (EqpResult::residual_refused) and the caller
// routes the pass to a retreat instead of stepping (qp_engine.h section 5,
// QpCounters::eqp_residual_refusals). A NaN residual REFUSES: every test here is
// written as !(value <= target) for that reason.
//
// ORDER IS LOAD-BEARING: the accuracy loop runs BEFORE the mu-free model step,
// because after that step y no longer solves K y = rhs -- it solves the proximal
// system, whose regularized residual is mu*lambda by construction -- so a check
// placed after it would read the model's own footprint as a defect.
//
// ONE step is all this path takes, and the reason is structural: the shared
// stopping test compares the unregularized residual against the
// regularization footprint ||diag(reg)*y||inf, and after any step the residual
// equals diag(reg) times the LAST CORRECTION — the test reads "the last
// correction is larger than the iterate itself". Here the first solve is a
// genuine regularized solve, so the correction is a small fraction of the
// iterate and the test cannot fire; the cancellation that makes it fire on the
// BORDERED path (bordered_eqp.h's LOAD-BEARING BORDERS note) has no
// counterpart here, which is why that path's loop is unconditional and this
// one has none.

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

#include <Eigen/SparseCore>
#include <fmt/format.h>

#include <hven/detail/kkt/kkt_assembly.h>
#include <hven/detail/kkt/kkt_calls.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/detail/qp/working_set.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

namespace detail {

// THE SHARED REFINEMENT BUDGET AND FLOOR. Both EQP paths refine under one
// rule, so both read one pair of constants; they live in the header
// bordered_eqp.h includes (the reverse would be circular).
//
// kMaxBorderRefineSteps is the TOTAL number of refinement steps a solve may
// take INCLUDING the mandatory first one -- not a count of EXTRA steps on top
// of it. Each path's loop can therefore earn at most nine extra steps beyond
// the first, so a system that refuses to converge costs a fixed handful of
// triangular solves rather than an open-ended number.
//
// Ten total suffices with margin for the worst contraction measured (~0.018
// per step, crossing its stopping rule at total step 4 of 10); anything slower
// is barely reducing the residual at all, and the stagnation gate stops the
// loop long before this count does.
constexpr Index kMaxBorderRefineSteps = 10;

// Hard floor beneath the stopping rule, relative to max(1, ||rhs||inf): a
// converged residual flattens out at ~1e-16 of that scale and then oscillates
// in the last bit, so 1e-14 stops the loop rather than spending solves on
// rounding noise. It is a backstop, not the rule -- the
// regularization-footprint test is what normally ends the loop.
constexpr double kBorderRefineRelFloor = 1e-14;

// THE EQP-SITE RESIDUAL CHECK's budget and tolerance (M7 W2 E4, ruling E4-4;
// the header's own section states the check). Shared by both algebras, for the
// same reason the two constants above are: one rule, read in one place.
//
// kEqpResidualRefineIters is a count of EXTRA solves against the REGULARIZED
// system, on top of the one solve every pass already takes. Three, because the
// object being repaired is a linear-solve error on an already-computed
// factorization: refinement against the same factor contracts by a fixed factor
// per step, so a solve the first extra step barely moves will not be rescued by
// a tenth, and the refusal path below is the cheaper answer. The strict-decrease
// safeguard stops the loop earlier than the count does whenever the factor is
// past helping.
constexpr Index kEqpResidualRefineIters = 3;

// Tolerance on an EQP answer's residual against its OWN regularized system, per
// block, as a fraction of (1 + that block's ||rhs||inf) -- floored, per block, by
// the regularization's own footprint on the answer (solve_eqp states that half).
//
// IT IS DELIBERATELY LOOSE, and the reason is that this check is not a feasibility
// or accuracy test -- the classifier owns feasibility, with the footprint
// mu*|lambda| absorbed the way it always was, and the walk has always been robust
// to a mediocre solve -- it is a test of whether the SOLVE HAPPENED AT ALL. The two
// populations it separates were measured, and they are decades apart: answers the
// walk uses successfully sit at 1.1e-13 absolute and at worst 7.8e-03 on a
// 3-variable over-determined face (HS27, where Pardiso perturbs a pivot and the
// answer is still usable), while the failures the check exists for are 2.6e+02,
// 7.1e+02, 9.3e+10 and 1.3e+14 (E3 §1c, §3e). 1e-1 sits between them with more than
// a decade of margin below and three above, and every value in that gap refuses the
// same set on the measured corpus. A tolerance at feas_tol would refuse the usable
// population; one at 1 would let the smallest measured failure through.
constexpr double kEqpResidualRefuseTol = 1e-1;

// THE UNREGULARIZED POLISH's budget at the EQP site (M7 W2 E4 fix round 1,
// ruling E4-11 applied where every answer is produced; polish_eqp_core states
// the loop and its safety argument). Steps against the residual of the face's own
// exact KKT system -- dual mu AND the primal delta backed out -- on the factor the
// solve already paid for, each kept only while that residual strictly falls.
//
// TWO. The proximal answer's bias away from the face's exact KKT point is
// O(primal_delta * ||x* - x_k||) and one step contracts it by another factor of
// order primal_delta / lambda_min(Z'HZ), so on a face with curvature two steps
// take a 1e-08-sized bias past 1e-16; a flat face rejects the FIRST step and
// costs one back-solve. Same number, same reasoning, as
// qp_engine.h's kTerminalPolishSteps, which is the same rule at the terminal
// point.
constexpr Index kEqpPolishSteps = 2;

// THE UNREGULARIZED POLISH's STEP CAP, relative to the answer's own size: a step
// is kept only if it also moves the answer by no more than
// kEqpPolishStepCap * max(1, ||y||inf).
//
// WHY A CAP AND NOT THE STRICT-DECREASE GUARD ALONE. The guard says the step
// solves the face's exact system BETTER; it does not say the step is a POLISH. On
// a nearly dependent face that system is nearly inconsistent, and a step that
// reduces its residual there can be 1/delta-sized -- a DIFFERENT answer, not a
// cleaned one -- and the walk that steps from it is somewhere else entirely.
//
// 1e-6, i.e. TWO DECADES of headroom above the bias this function exists to
// remove: that bias is O(primal_delta * ||x* - x_k||), so O(1e-08) relative at the
// defaults. MEASURED on the 51-dump corpus, both algebras, against NC2 fix1's rows
// as the base:
//
//   cap     base-certified cells lost   recovered / converted
//   none    3 (ACOPR57 qp0, qp4; Cs_STEENBRB qp6)   ACOPP57_def qp0-qp3 on border
//   1e-4    1 (ACOPR57_def/qp4: opt/1736 -> num/2564)   + ACOPR57 qp0, Cs_STEENBRB qp6 back
//   1e-6    0                            + ACOPR57_def/qp4 back, + YORKNET_def/qp0
//
// The mechanism is one and the same read from both ends: the uncapped step that
// repairs `ACOPP57_def`'s bordered face (whose answer is 1.25e-07-stationary
// against the eliminated path's 1.02e-14) is the same kind of step that derails
// `ACOPR57_def`'s, and only the SIZE separates them. A cap tight enough to be the
// bias itself would refuse the legitimate step on a face whose null-space
// curvature is small -- there the bias is delta / lambda_min(Z'HZ) relative, which
// can reach 1e-2 -- and a cap at 1 would not be a cap. At 1e-6 the two cells E4
// converted that this cap gives up are named in the fix-round report; no cell
// certified at the base is lost on either algebra.
constexpr double kEqpPolishStepCap = 1e-6;

// Budget for the VERDICT-SITE face refinement (qp_engine.h's section 5), in
// the same class as kMaxBorderRefineSteps and counted the same way -- TOTAL
// steps kept. It lives beside that constant because it is the same kind of
// bound on the same kind of loop, not because the two loops share a rule:
// this one has no mandatory step (its starting point is a solve the walk has
// already refined once) and it is entered only at a would-be-kInfeasible dead
// end, so every step it takes is one the verdict asked for.
constexpr Index kMaxVerdictRefineSteps = 10;

} // namespace detail

struct EqpResult {
    Vec x, lambda_e, lambda_w;

    /// Refinement steps this ONE solve kept — deliberately two different
    /// meanings by producer, feeding QpCounters' two fields:
    /// solve_eqp reports EXTRA steps beyond the mandatory first (identically 0
    /// on this loop-free path; kept because QpCounters::eqp_refine_steps is
    /// the invariant it feeds), while solve_bordered_eqp reports TOTAL steps
    /// including the mandatory first (always >= 1).
    Index refine_steps = 0;

    /// THE EQP-SITE RESIDUAL CHECK's three outputs (M7 W2 E4; see the header's
    /// own section). `row_residual` is ||(rhs - K y)_rows||inf for the answer the
    /// check accepted -- the answer's own miss against the REGULARIZED system it
    /// came out of, in the constraint rows, BEFORE the model's mu-free step;
    /// `residual_refine_iters` the extra solves spent driving it down (0 on a
    /// healthy pass); `residual_refused` true iff it is still above
    /// kEqpResidualRefuseTol*(1 + ||rhs_rows||inf) -- or NaN -- after the budget,
    /// which means the answer does not solve its own system and the caller must
    /// not step toward it.
    double row_residual = 0.0;
    /// The check's own verdict quantity: the CONSTRAINT-ROW residual measured
    /// against that block's own scale. > 1 is the refusal.
    double residual_ratio = 0.0;
    Index residual_refine_iters = 0;
    bool residual_refused = false;

    /// THE UNREGULARIZED POLISH's output (M7 W2 E4 fix1, ruling E4-11): steps
    /// KEPT against the face's own exact KKT system, out of kEqpPolishSteps
    /// offered. 0 means the proximal answer stands -- a flat face, or an answer
    /// already exact -- and is the value on every face whose null space carries no
    /// curvature. Counted into QpCounters::eqp_polish_steps by the callers.
    Index polish_steps = 0;

    /// THE BORDERED ANSWER'S CERTIFICATE failed (bordered_eqp.h,
    /// refine_bordered_solve_iterated; M7 W2 BPS): the refinement ended above its
    /// own target. Set by solve_bordered_eqp on the independent working set's walk
    /// only; the caller takes the eliminated twin for the pass. Always false from
    /// solve_eqp.
    bool border_uncertified = false;
};

// THE UNREGULARIZED POLISH (M7 W2 E4 fix round 1, ruling E4-11): the loop, and
// then the whole-face entry point below solve_eqp.
// The loop itself, on an assembly and an rhs the caller already holds. `rhs0` is
// the ABSOLUTE unregularized right-hand side -- solve_eqp's rhs WITHOUT its
// proximal `+ delta*x_k,F` term -- and `y` is updated in place. Split out so that
// the rule exists once: solve_eqp calls it on the objects it already has, and
// polish_eqp_unregularized below builds them for a caller that does not.
inline Index polish_eqp_core(const KktAssembly &asm_, const Vec &rhs0, const QpOptions &opts,
                             detail::KktFactor &kkt, Index n_free, Index n_rows, Index max_steps,
                             Vec &y) {
    if (max_steps <= 0 || y.size() == 0) {
        return 0;
    }
    Vec reg = Vec::Zero(y.size());
    reg.head(n_free).setConstant(opts.primal_delta);
    reg.segment(n_free, n_rows).setConstant(-opts.dual_mu);
    const auto residual_of = [&](const Vec &yy) {
        const Vec Kyy = asm_.K.template selfadjointView<Eigen::Upper>() * yy;
        return Vec((rhs0 - Kyy) + reg.cwiseProduct(yy));
    };
    Vec res = residual_of(y);
    double res_inf = res.lpNorm<Eigen::Infinity>();
    Index kept = 0;
    for (Index step = 0; step < max_steps; ++step) {
        const Vec delta = detail::solve_vec(kkt, res);
        const double cap = detail::kEqpPolishStepCap * std::max(1.0, y.lpNorm<Eigen::Infinity>());
        if (!(delta.lpNorm<Eigen::Infinity>() <= cap)) {
            break; // not a polish: see kEqpPolishStepCap
        }
        const Vec candidate = y + delta;
        Vec cres = residual_of(candidate);
        const double cres_inf = cres.lpNorm<Eigen::Infinity>();
        if (!(cres_inf < res_inf)) {
            break; // stagnated, diverging, or NaN -- keep the better point
        }
        y = candidate;
        res = std::move(cres);
        res_inf = cres_inf;
        ++kept;
    }
    return kept;
}

// Solves the equality-constrained QP defined by `ws`'s working set (working
// inequalities held at equality, fixed/bound variables pinned) via one
// factorize + solve against `kkt`, the EQP-site residual check, then one step of
// iterative refinement against the PROXIMAL KKT residual (see header comment
// above). If `unrefined_out` is non-null, it receives the result BEFORE that
// model step -- intended for tests that need to observe the step's effect in
// isolation, and identical to the raw solve whenever the residual check spends
// nothing.
//
// `x_k` is the PROXIMAL CENTRE: the iterate this EQP is being solved from, of
// size qp.n(). See the header's PROXIMAL CENTRE section -- it is required, and
// passing a zero vector is the pre-E4 ABSOLUTE centring, a different system.
inline EqpResult solve_eqp(const QpProblem &qp, const WorkingSet &ws, const Vec &x_k,
                           detail::KktFactor &kkt, const QpOptions &opts,
                           EqpResult *unrefined_out = nullptr) {
    qp.validate();
    if (x_k.size() != qp.n()) {
        throw std::invalid_argument(
            fmt::format("solve_eqp: proximal centre has size {}, expected n = {}",
                        static_cast<long long>(x_k.size()), static_cast<long long>(qp.n())));
    }

    const Index n = qp.n();
    const Index me = qp.me();
    const std::vector<Index> &aw = ws.active_ineq();
    const Index n_working = static_cast<Index>(aw.size());

    KktAssembly asm_ = assemble_kkt(qp, ws, opts);
    const Index n_free = static_cast<Index>(asm_.free_of_full.size());
    const Index dim = n_free + me + n_working;

    // Substitution values for non-free variables -- same rule assemble_kkt
    // uses internally for x_fixed.
    Vec x_fixed = Vec::Zero(n);
    for (Index i = 0; i < n; ++i) {
        switch (ws.bound_state()[static_cast<std::size_t>(i)]) {
        case BoundState::kFree:
            break;
        case BoundState::kAtUpper:
            x_fixed(i) = qp.upper(i);
            break;
        case BoundState::kAtLower:
        case BoundState::kFixed:
            x_fixed(i) = qp.lower(i);
            break;
        }
    }

    // rhs = [-g_F - hess_shift + delta*x_k,F; be - eq_shift; bw - ineq_shift], in
    // the same row order as K (free Hessian rows | equality rows | working-ineq
    // rows). The free block's third term is THE PROXIMAL CENTRE (see the header):
    // it appears on the free rows and nowhere else, because those are the only
    // rows the model's penalty (delta/2)||x_F - x_k,F||^2 differentiates.
    Vec rhs(dim);
    for (Index k = 0; k < n_free; ++k) {
        const Index i = asm_.free_of_full[static_cast<std::size_t>(k)];
        rhs(k) = -qp.g(i) - asm_.rhs_shift(k) + opts.primal_delta * x_k(i);
    }
    for (Index r = 0; r < me; ++r) {
        rhs(n_free + r) = qp.be(r) - asm_.rhs_shift(n_free + r);
    }
    for (Index k = 0; k < n_working; ++k) {
        const Index row = aw[static_cast<std::size_t>(k)];
        rhs(n_free + me + k) = qp.bi(row) - asm_.rhs_shift(n_free + me + k);
    }

    detail::factorize_checked(kkt, asm_.K);
    Vec y = detail::solve_vec(kkt, rhs);

    auto scatter = [&](const Vec &yy) {
        EqpResult res;
        res.x = x_fixed; // fixed entries already sit at their bound value
        for (Index k = 0; k < n_free; ++k) {
            res.x(asm_.free_of_full[static_cast<std::size_t>(k)]) = yy(k);
        }
        res.lambda_e = yy.segment(n_free, me);
        res.lambda_w = yy.segment(n_free + me, n_working);
        return res;
    };

    // ---- THE EQP-SITE RESIDUAL CHECK (E4-4; see the header's own section) ----
    // The answer's miss against the system it was just solved FROM, over the
    // CONSTRAINT ROWS, against that block's own scale, as a single ratio: > 1 is a
    // refusal. Every test is written as !(v <= t) so a NaN refuses.
    const Index nc = me + n_working;
    const auto regularized_residual = [&](const Vec &yy) {
        const Vec Kyy = asm_.K.template selfadjointView<Eigen::Upper>() * yy;
        return Vec(rhs - Kyy);
    };
    const double rhs_rows_inf = nc > 0 ? rhs.segment(n_free, nc).lpNorm<Eigen::Infinity>() : 0.0;
    // THE SCALES CARRY THE REGULARIZATION'S OWN FOOTPRINT ON THIS ANSWER --
    // delta*||x_F|| on the free block, mu*||lambda|| on the rows. Those are the
    // sizes of terms that are IN the regularized equations, so a residual at or
    // beneath one of them is inside the regularization the engine already absorbs
    // everywhere else (qp_engine.h's row_tolerance, violation_is_structural), not
    // evidence that the factor failed. Without them the check refuses answers the
    // walk has always used: measured, HS10 and HS27 leave a row residual of
    // 3.1e-05 and 7.8e-03 on 3-variable faces whose multipliers are the 1/mu-sized
    // artifacts an over-determined working set legitimately produces, and refusing
    // those turns two kOptimal exits into kNumericalError.
    // THE CONSTRAINT ROWS ONLY (E4-4's own wording), and that is a MEASURED
    // choice, not a reading of convenience: the same check applied to the FREE
    // block as well refuses the first solve of every restoration cell in the
    // corpus -- `Cs_STEENBRB` qp1 at 1.95x its free-block scale, `A0_YORKNET` qp3
    // at 1.01x -- on faces whose row residual is 1.3e-08 and 6.8e-07 and which the
    // walk then solves to the Clarabel optimum. A flat face's free block IS
    // delta*I, so its stationarity rows are the ill-conditioned half by
    // construction and their residual is not evidence about the solve. The
    // non-finite answers the free block would have caught are caught where they
    // belong, on the answer itself (qp_engine.h section 5).
    const auto ratio_of = [&](const Vec &yy, const Vec &r) {
        if (nc == 0) {
            return 0.0;
        }
        const double scale =
            std::max(detail::kEqpResidualRefuseTol * (1.0 + rhs_rows_inf),
                     opts.dual_mu * yy.segment(n_free, nc).lpNorm<Eigen::Infinity>());
        return r.segment(n_free, nc).lpNorm<Eigen::Infinity>() / scale;
    };
    Vec r_reg = regularized_residual(y);
    double ratio = ratio_of(y, r_reg);
    Index res_iters = 0;
    while (!(ratio <= 1.0) && res_iters < detail::kEqpResidualRefineIters) {
        // COUNTED BEFORE THE SAFEGUARD: the solve below is spent whether or not
        // its candidate is kept (solver_counters.h's eqp_refine_iters).
        ++res_iters;
        const Vec candidate = y + detail::solve_vec(kkt, r_reg);
        Vec candidate_r = regularized_residual(candidate);
        const double candidate_ratio = ratio_of(candidate, candidate_r);
        if (!(candidate_ratio < ratio)) {
            break; // stagnated, diverging, or NaN -- keep the better answer
        }
        y = candidate;
        r_reg = std::move(candidate_r);
        ratio = candidate_ratio;
    }
    const bool refused = !(ratio <= 1.0);
    const double row_residual = nc > 0 ? r_reg.segment(n_free, nc).lpNorm<Eigen::Infinity>() : 0.0;

    if (unrefined_out != nullptr) {
        *unrefined_out = scatter(y);
    }

    // One step of iterative refinement against the PROXIMAL system, whose only
    // regularization is the DUAL one: r = rhs - K0*y = (rhs - K*y) + diag(reg)*y,
    // solved with the same (already-factorized) regularized system. The primal
    // delta stays in the target because it is a term of the model -- see the
    // header's Iterative refinement section.
    Vec reg = Vec::Zero(dim);
    reg.segment(n_free, nc).setConstant(-opts.dual_mu);

    const auto residual_of = [&](const Vec &yy) {
        const Vec Kyy = asm_.K.template selfadjointView<Eigen::Upper>() * yy;
        return Vec((rhs - Kyy) + reg.cwiseProduct(yy));
    };

    y = y + detail::solve_vec(kkt, residual_of(y));

    // ---- THE UNREGULARIZED POLISH (E4-11; polish_eqp_core, and the header's own
    // section) ----
    // The step above targets the PROXIMAL system, whose answer is the face's exact
    // KKT point only in the limit x_k -> x*; away from it the answer carries an
    // O(delta*||x* - x_k||) bias, which is a real error in the answer even though
    // delta is a real term of the model. So the face's OWN exact system is targeted
    // next, for at most kEqpPolishSteps steps, each kept ONLY while that residual
    // strictly falls -- and on a flat face, where that system has no answer along
    // the null space at all, the first step is rejected and the proximal answer is
    // what this function returns. The guard is the whole safety argument and it is
    // structural, not tuned: see polish_eqp_core.
    Vec rhs0 = rhs;
    for (Index k = 0; k < n_free; ++k) {
        rhs0(k) -= opts.primal_delta * x_k(asm_.free_of_full[static_cast<std::size_t>(k)]);
    }
    const Index polish_kept =
        polish_eqp_core(asm_, rhs0, opts, kkt, n_free, nc, detail::kEqpPolishSteps, y);

    EqpResult out = scatter(y);
    out.polish_steps = polish_kept;
    out.refine_steps = 0; // no iterated loop on this path -- see the header note
    out.row_residual = row_residual;
    out.residual_ratio = ratio;
    out.residual_refine_iters = res_iters;
    out.residual_refused = refused;
    return out;
}

// THE UNREGULARIZED POLISH (M7 W2 E4 fix round 1, ruling E4-11).
//
// Iterative refinement of an EQP answer `io` on the face `ws`, against the
// residual of the system with BOTH regularizations backed out -- the dual mu AND
// the primal delta, i.e. the face's OWN exact KKT system
//
//     H_FF x_F + Ae_F' le + Aw_F' lw = -g_F - hess_shift
//     Ae x = be,   Aw x = bw
//
// taken on the ALREADY-FACTORIZED `kkt` (no factorization, one assembly and two
// mat-vecs per step), at most `max_steps` steps, each KEPT only while the
// infinity norm of that residual STRICTLY FALLS. Returns the number of steps
// kept and rewrites `io.x`, `io.lambda_e` and `io.lambda_w` only if that number
// is nonzero; nothing else on `io` is touched. A NaN residual keeps nothing
// (every test is written as !(v < w)).
//
// WHY THE STRICT-DECREASE GUARD IS THE WHOLE SAFETY ARGUMENT, and why this
// function is therefore safe to call on ANY face. The proximal system this
// engine solves is the one the model poses (eqp_solve.h's Iterative refinement
// section: delta is a TERM, not an error), and its answer carries an
// O(delta * ||x* - x_k||) bias away from the face's exact KKT point. On a face
// with curvature along its null space the unregularized system above HAS that
// point, the refinement converges to it, and one or two steps remove the bias --
// which is what a terminal point, and an exact face re-solve, are asked for. On
// a FLAT face (Z'HZ = 0) it has NO solution along the null space at all; the
// step is then 1/delta-sized and does not reduce the null-space component of the
// residual by construction, so the guard rejects it and the proximal answer
// stands untouched. That is the same rule, and the same reason, as the refinement
// safeguard in QpEngine::refine_terminal_mu_free (E2 fix4), which this function
// is factored out of; it is deliberately NOT a curvature test, because a
// residual that falls IS the evidence and a curvature estimate would be a proxy
// for it.
//
// `opts` must be the options the ANSWER was produced under: both regularizations
// are read off it to form K0 = K - diag(reg), reg = [+primal_delta on the free
// rows; -dual_mu on the constraint rows], so the residual is computable without
// ever forming K0. A caller that has already zeroed one of the two (the mu-free
// re-solve) passes its own options and this function backs out what is left.
inline Index polish_eqp_unregularized(const QpProblem &qp, const WorkingSet &ws,
                                      detail::KktFactor &kkt, const QpOptions &opts,
                                      Index max_steps, EqpResult &io) {
    if (max_steps <= 0) {
        return 0;
    }
    const Index me = qp.me();
    const std::vector<Index> &aw = ws.active_ineq();
    const Index n_working = static_cast<Index>(aw.size());
    const KktAssembly asm_ = assemble_kkt(qp, ws, opts);
    const Index n_free = static_cast<Index>(asm_.free_of_full.size());
    const Index dim = n_free + me + n_working;
    if (dim == 0) {
        return 0;
    }

    // The ABSOLUTE unregularized rhs: solve_eqp's own rhs WITHOUT its proximal
    // `+ delta*x_k,F` term, which belongs to the penalty this function removes.
    Vec rhs(dim);
    for (Index k = 0; k < n_free; ++k) {
        rhs(k) = -qp.g(asm_.free_of_full[static_cast<std::size_t>(k)]) - asm_.rhs_shift(k);
    }
    for (Index r = 0; r < me; ++r) {
        rhs(n_free + r) = qp.be(r) - asm_.rhs_shift(n_free + r);
    }
    for (Index k = 0; k < n_working; ++k) {
        rhs(n_free + me + k) =
            qp.bi(aw[static_cast<std::size_t>(k)]) - asm_.rhs_shift(n_free + me + k);
    }

    Vec y(dim);
    for (Index k = 0; k < n_free; ++k) {
        y(k) = io.x(asm_.free_of_full[static_cast<std::size_t>(k)]);
    }
    y.segment(n_free, me) = io.lambda_e;
    y.segment(n_free + me, n_working) = io.lambda_w;

    const Index kept = polish_eqp_core(asm_, rhs, opts, kkt, n_free, me + n_working, max_steps, y);
    if (kept == 0) {
        return 0;
    }
    for (Index k = 0; k < n_free; ++k) {
        io.x(asm_.free_of_full[static_cast<std::size_t>(k)]) = y(k);
    }
    io.lambda_e = y.segment(n_free, me);
    io.lambda_w = y.segment(n_free + me, n_working);
    return kept;
}

} // namespace hven::solvers
