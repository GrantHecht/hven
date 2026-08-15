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
//     rhs = [ -g_F - hess_shift ; be - eq_shift ; bw - ineq_shift ]
// where hess_shift/eq_shift/ineq_shift are rhs_shift's three blocks (in the
// same row order as K). Solving K y = rhs for y = [x_F; lambda_e; lambda_w]
// and scattering x_F back alongside the fixed variables' bound values gives
// the full-space answer.
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
//     signals that row should be DROPPED from the working set (Task 9).
//   - Bound multipliers z are NOT computed here (EqpResult has no z field).
//     Task 9 must price them from the stationarity residual at a non-free
//     variable, i.e. z(i) = (Hx + g + Ae^T lambda_e + Ai^T lambda_i)(i) for
//     that i. Per qp_problem.h's convention, z >= 0 is required at an
//     active LOWER bound and z <= 0 at an active UPPER bound; a kFixed
//     variable's z is not sign-constrained.
//
// --- Iterative refinement ---
//
// With delta = mu = 1e-8 (QpOptions defaults) the regularized solve above
// differs from the exact (unregularized) equality-QP solution at O(1e-8).
// K0 (the unregularized system) equals K with its regularization diagonal
// removed, i.e. K0 = K - diag(reg) with reg(k) = delta for Hessian rows and
// -mu for constraint rows, so the unregularized residual at y is exactly
//     r = rhs - K0*y = (rhs - K*y) + diag(reg)*y
// computable WITHOUT ever forming K0. Solving K*correction = r against the
// SAME (already-factorized) regularized system and adding the correction to
// y is one step of iterative refinement; per the task brief this brings
// accuracy from ~1e-8 to ~1e-10..1e-12 and is load-bearing for later
// tolerances.
//
// ONE step is all this path takes, and the alternative was measured and
// REMOVED rather than left unexplored. A flag-gated iterated loop
// (QpOptions::eqp_refine) ran here for one phase, stepping under exactly the
// rule bordered_eqp.h's refine_bordered_solve_iterated uses. It never fired:
// 0 extra steps across 27 Hock-Schittkowski problems x 2 tolerance regimes x 2
// algebra modes plus 13 deliberately adversarial QP-level probes, measured
// independently on MKL/Pardiso and on Apple Accelerate, with bit-identical
// answers in every cell.
//
// The reason is structural, not a property of those fixtures, and it is worth
// stating here because it is what licenses the single step. The shared
// stopping test compares the unregularized residual against the
// regularization footprint ||diag(reg)*y||inf, and the residual after any step
// equals diag(reg) times the LAST CORRECTION -- so the test reads "the last
// correction is larger than the iterate itself". On THIS path the first solve
// is a genuine regularized solve, so the correction is a small fraction of the
// iterate and the test cannot fire. The cancellation that makes it fire on the
// BORDERED path (bordered_eqp.h's LOAD-BEARING BORDERS note) has no
// counterpart here, which is why that path's loop is unconditional and this
// one has none. The shared constants detail::kMaxBorderRefineSteps /
// detail::kBorderRefineRelFloor still live in THIS header, below: the bordered
// path includes it, and they stayed put so the move is not conflated with the
// deletion.
//
// docs/notes/2026-07-29-eqp-refinement-ab.md (DISPOSITION section) and
// docs/notes/2026-07-29-accelerate-audit-results.md (Phase C) carry the
// measurement and the ruling.

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include <Eigen/SparseCore>

#include <hven/detail/kkt/kkt_assembly.h>
#include <hven/detail/kkt/kkt_calls.h>
#include <hven/detail/sqp/qp_problem.h>
#include <hven/detail/sqp/working_set.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

namespace detail {

// THE SHARED REFINEMENT BUDGET AND FLOOR. Both EQP paths refine under one
// rule, so both read one pair of constants; they live here, in the header
// bordered_eqp.h includes, rather than the other way round (bordered_eqp.h
// includes this file, so the reverse would be circular). The names keep the
// "Border" they were introduced with (Task 11b, where the bordered path was
// the only iterated one) so that the Task-11b analysis, the tests, and the
// notes that cite them by name all still refer to the same symbols.
//
// kMaxBorderRefineSteps is the TOTAL number of refinement steps a solve may
// take INCLUDING the mandatory first one -- not a count of EXTRA steps on top
// of it. The first step is the one every solve has always taken; each path's
// loop (`for (step = 1; step < kMaxBorderRefineSteps; ...)`) can therefore
// earn at most kMaxBorderRefineSteps - 1 == 9 EXTRA steps beyond it, so a
// system that refuses to converge costs a fixed handful of triangular solves
// rather than an open-ended number.
//
// Ten total is enough with a decent margin for the worst contraction this
// project has measured: the Task 11b reproduction contracts by ~0.018 per
// step and crosses its stopping rule at total step 4 of 10. A system whose
// contraction is so slow that ten total steps do not suffice is one where
// refinement is barely reducing the residual at all, and the stagnation gate
// stops the loop long before this count does.
constexpr Index kMaxBorderRefineSteps = 10;

// Hard floor beneath the stopping rule, relative to max(1, ||rhs||inf): a
// converged residual flattens out at ~1e-16 of that scale and then oscillates
// in the last bit, so 1e-14 stops the loop rather than spending solves on
// rounding noise. It is a backstop, not the rule -- the
// regularization-footprint test is what normally ends the loop.
constexpr double kBorderRefineRelFloor = 1e-14;

} // namespace detail

struct EqpResult {
    Vec x, lambda_e, lambda_w;

    // Refinement steps this ONE solve kept. Its meaning depends on which
    // function produced the result, and the difference is deliberate -- see
    // QpCounters (types.h), whose two fields these feed:
    //   solve_eqp           EXTRA steps beyond the mandatory first. That path
    //                       has no iterated loop (see the header note), so
    //                       this is identically 0 and is left set that way
    //                       rather than removed: QpCounters::eqp_refine_steps
    //                       is the invariant it feeds.
    //   solve_bordered_eqp  TOTAL steps including the mandatory first (so
    //                       always >= 1).
    Index refine_steps = 0;
};

// Solves the equality-constrained QP defined by `ws`'s working set (working
// inequalities held at equality, fixed/bound variables pinned) via one
// factorize + solve against `kkt`, then one step of iterative refinement
// against the unregularized KKT residual (see header comment above). If
// `unrefined_out` is non-null, it receives the PRE-refinement result --
// intended for tests that need to observe the refinement step's effect in
// isolation.
inline EqpResult solve_eqp(const QpProblem &qp, const WorkingSet &ws, detail::KktFactor &kkt,
                           const QpOptions &opts, EqpResult *unrefined_out = nullptr) {
    qp.validate();

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

    // rhs = [-g_F - hess_shift; be - eq_shift; bw - ineq_shift], in the same
    // row order as K (free Hessian rows | equality rows | working-ineq rows).
    Vec rhs(dim);
    for (Index k = 0; k < n_free; ++k) {
        rhs(k) = -qp.g(asm_.free_of_full[static_cast<std::size_t>(k)]) - asm_.rhs_shift(k);
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

    if (unrefined_out != nullptr) {
        *unrefined_out = scatter(y);
    }

    // One step of iterative refinement against the UNREGULARIZED system:
    // r = rhs - K0*y = (rhs - K*y) + diag(reg)*y, solved with the same
    // (already-factorized) regularized system.
    Vec reg = Vec::Zero(dim);
    reg.head(n_free).setConstant(opts.primal_delta);
    reg.segment(n_free, me + n_working).setConstant(-opts.dual_mu);

    const auto residual_of = [&](const Vec &yy) {
        const Vec Kyy = asm_.K.template selfadjointView<Eigen::Upper>() * yy;
        return Vec((rhs - Kyy) + reg.cwiseProduct(yy));
    };

    y = y + detail::solve_vec(kkt, residual_of(y));

    EqpResult out = scatter(y);
    out.refine_steps = 0; // no iterated loop on this path -- see the header note
    return out;
}

} // namespace hven::solvers
