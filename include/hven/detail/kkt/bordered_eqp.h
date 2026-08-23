// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// bordered_eqp.h — the equality-QP solve over a working set represented as a
// FIXED, factored K0 (kkt_assembly.h's assemble_kkt_full) plus a stack of
// GMSW borders (border_ops.h / schur_complement.h), rather than as a freshly
// assembled bound-eliminated K (eqp_solve.h's solve_eqp).
//
// This is solve_eqp's border-mode twin: same inputs in problem terms, same
// EqpResult out (x in FULL space, lambda_e per equality row, lambda_w one
// entry per row of ws.active_ineq() in that sorted order), so the engine's
// pricing/drop/ratio machinery consumes it unchanged.
//
// --- The system being solved ---
//
// K0 spans [n variables | me equality rows | n_w0 initial working rows], the
// last of those named by `k0_rows` (the WorkingSet K0 was assembled from).
// Every working-set change SINCE that assembly is a border, listed in
// `ledger` in SchurComplement::add_border call order:
//
//   kVarPin(i)    v = e_i,                d = -dual_mu, rhs = the bound value
//   kRowDelete(k) v = e_{n + k},          d = 0,        rhs = 0
//   kIneqRow(j)   v = Ai.row(j)^T,        d = -dual_mu, rhs = bi(j)
//
// The head of the right-hand side is [-g | be | bi over k0_rows], i.e. the
// same rhs solve_eqp builds, minus the rhs_shift terms -- which are
// identically zero in full mode, since nothing is substituted out.
//
// TRUE rhs, not the shifted one: a homotopy-shifted working row (qp_engine.h,
// step 1) enters the EQP at qp.bi(j), never at bi(j) + shift(j). That is what
// pulls a shifted row toward feasibility, and it is exactly what solve_eqp
// does, so the two paths must agree here or the homotopy walks differently in
// the two modes.
//
// --- Multipliers ---
//
//   lambda_e  K0's equality block, read straight off the solve.
//   lambda_w  per row of ws.active_ineq(): if the row is one of K0's own
//             initial rows AND has not been deactivated by a kRowDelete
//             border, its dual is that K0 row's; otherwise the row is live
//             only through its kIneqRow border and its dual is that border's
//             y entry. A DELETED K0 row is not in ws.active_ineq() at all
//             (the delete border pins its dual to exactly zero -- see
//             border_ops.h), so it never reaches this loop.
//   z         NOT computed here, exactly as in eqp_solve.h: the engine prices
//             bound multipliers from the stationarity residual. (The pin
//             borders' own duals are that residual up to sign, but they are
//             deliberately not plumbed out -- the engine's price() is the one
//             definition of z, shared by both linear-algebra paths.)
//
// --- Pinned variables are scattered, not read back ---
//
// x's pinned entries are overwritten with their exact bound values rather
// than taken from the solve, matching solve_eqp's scatter. A pin border
// carries d = -dual_mu, so the solve itself only satisfies x(i) to
// O(dual_mu * |y_i|) of the bound -- and on the over-determined working sets
// the loop legitimately visits mid-run (every variable pinned AND a working
// row still demanding something of them) a pin's dual is a regularization
// artifact of order 1/dual_mu, so that gap is O(1) rather than O(dual_mu).
// The refinement below happens to close it on every fixture in the
// equivalence battery, but the overwrite is kept because the bound value is
// KNOWN exactly and it makes the two paths agree on pinned components by
// construction rather than by the refinement's good behavior.

#include <algorithm>
#include <utility>
#include <vector>

#include <Eigen/SparseCore>

#include <hven/detail/kkt/border_ops.h>
#include <hven/detail/kkt/kkt_assembly.h>
#include <hven/detail/kkt/schur_complement.h>
#include <hven/detail/qp/eqp_solve.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/detail/qp/working_set.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

// detail::kMaxBorderRefineSteps and detail::kBorderRefineRelFloor -- the
// budget and floor this file's refinement loop runs on -- live in eqp_solve.h
// (which this header includes). This loop is their one consumer.

// The regularization this bordered system carries on its diagonal:
// +primal_delta on K0's first `var_count` (Hessian) rows, -dual_mu on every
// remaining K0 (equality/working) row, and each border's own d_i on its row.
// Sized n0 + border_d.size().
inline Vec bordered_regularization(Index n0, Index var_count, const std::vector<double> &border_d,
                                   const QpOptions &opts) {
    const Index m = static_cast<Index>(border_d.size());
    Vec reg = Vec::Zero(n0 + m);
    reg.head(var_count).setConstant(opts.primal_delta);
    reg.segment(var_count, n0 - var_count).setConstant(-opts.dual_mu);
    for (Index i = 0; i < m; ++i) {
        reg(n0 + i) = border_d[static_cast<std::size_t>(i)];
    }
    return reg;
}

// Residual of `sol` against the fully UNREGULARIZED bordered system.
//
// Every diagonal entry bordered_regularization reports regularizes a
// mathematically exact equation, so backing all of them out of the residual --
// computable without ever forming the unregularized matrix, since
// K0_true = K0 - diag(reg) implies r = rhs - K0_true*sol = (rhs - K0*sol) +
// diag(reg)*sol -- targets the same solution solve_eqp's own refinement step
// converges to.
inline Vec bordered_residual(const SpMatRM &K0, Index var_count, const std::vector<Vec> &border_v,
                             const std::vector<double> &border_d, const Vec &rhs, const Vec &sol,
                             const QpOptions &opts) {
    const Index n0 = K0.rows();
    const Index m = static_cast<Index>(border_v.size());
    const Vec reg = bordered_regularization(n0, var_count, border_d, opts);

    const Vec x0 = sol.head(n0);
    Vec Ax = Vec::Zero(n0 + m);
    Ax.head(n0) = K0.template selfadjointView<Eigen::Upper>() * x0;
    for (Index i = 0; i < m; ++i) {
        const double yi = sol(n0 + i);
        Ax.head(n0) += border_v[static_cast<std::size_t>(i)] * yi;
        Ax(n0 + i) = border_v[static_cast<std::size_t>(i)].dot(x0) +
                     border_d[static_cast<std::size_t>(i)] * yi;
    }

    return (rhs - Ax) + reg.cwiseProduct(sol);
}

// ONE step of iterative refinement of a bordered solve against the fully
// UNREGULARIZED system, the bordered counterpart of solve_eqp's refinement
// step (eqp_solve.h). `border_v`/`border_d` must list every currently-live
// border in SchurComplement::add_border call order (matching `schur`'s
// internal state); `sol` is the pre-refinement [x0; y] solve to refine.
// Without it the two paths agree only to O(primal_delta) ~ 1e-8.
//
// solve_bordered_eqp does NOT call this directly -- see
// refine_bordered_solve_iterated for the case where one step is not enough. It
// is kept as the single-step primitive that function and the border
// equivalence tests are both written against.
inline Vec refine_bordered_solve(const SpMatRM &K0, Index var_count,
                                 const std::vector<Vec> &border_v,
                                 const std::vector<double> &border_d, const SchurComplement &schur,
                                 const Vec &rhs, const Vec &sol, const QpOptions &opts) {
    return sol + schur.solve(bordered_residual(K0, var_count, border_v, border_d, rhs, sol, opts));
}

// One mandatory refinement step, then further steps for as long as the first
// one demonstrably failed to do its job.
//
// WHY ONE STEP IS NOT ALWAYS ENOUGH. The two-solve Schur form computes K0^-1
// rhs and K0^-1 V and then CANCELS them against each other, so the accuracy
// of the bordered answer is set by how large those intermediates are
// relative to the answer. K0 is assembled from the SEED working set, and a
// bound pin is a border against every possible K0 (border_ops.h), so on a
// problem whose curvature only becomes definite ONCE the pins are applied,
// K0 is exactly singular in exact arithmetic and primal_delta alone makes it
// invertible; ||K0^-1|| ~ 1/primal_delta then leaves the raw two-solve
// answer wrong in its first digit, while refinement -- whose target system
// was well conditioned in the reproduction (cond ~1.5e2) -- converges only
// geometrically.
// The eliminated path (eqp_solve.h) never forms those intermediates and does
// not need this loop.
//
// THE STOPPING RULE IS THE REGULARIZATION FOOTPRINT, which keeps the extra
// steps confined to the error BORDERING introduces. The mandatory step's job
// is to remove the O(primal_delta) bias of the regularized solve, whose size
// on the current iterate is exactly ||diag(reg)*sol||inf:
//
//   - residual now BELOW the footprint => the step did its job; everything
//     left is the conditioning of the problem itself, which the eliminated
//     path carries identically and which is not this seam's to remove.
//   - residual still ABOVE the footprint => the bordered form lost digits
//     that the eliminated form never loses, and the extra steps recover
//     exactly those.
//
// Refining past the footprint where border and refactorize already agree
// would make border mode strictly more accurate than its own equivalence
// oracle (qp_engine.h's cross-mode batteries), so the rule stops at it.
//
// At schur.dim() == 0 the loop is skipped outright: there is no border
// stack, schur.solve degenerates to a plain K0 solve, and the computation IS
// solve_eqp's on an all-free working set -- so it gets solve_eqp's budget by
// CONSTRUCTION rather than by the rule above happening to agree. That guard
// is belt-and-braces (the footprint rule already stops every shipped dim()==0
// case after the first step); it is kept because "an unbordered solve gets
// exactly what the unbordered path gets" should not depend on a threshold
// holding.
//
// Every extra step is also safeguarded: it is kept only if it STRICTLY
// reduces the residual's inf-norm, so a stack too ill-conditioned for
// refinement to help can never end up worse than the single step this engine
// has always taken.
//
// `steps_out`, when non-null, receives the TOTAL number of steps KEPT --
// including the mandatory first one, so it is never less than 1, matching
// the accounting kMaxBorderRefineSteps itself uses. A candidate rejected by
// the strict-decrease rule is discarded and not counted. This feeds
// QpCounters::border_refine_steps.
inline Vec refine_bordered_solve_iterated(const SpMatRM &K0, Index var_count,
                                          const std::vector<Vec> &border_v,
                                          const std::vector<double> &border_d,
                                          const SchurComplement &schur, const Vec &rhs,
                                          const Vec &sol, const QpOptions &opts,
                                          Index *steps_out = nullptr) {
    const auto residual_of = [&](const Vec &s) {
        return bordered_residual(K0, var_count, border_v, border_d, rhs, s, opts);
    };
    Index steps = 1; // the mandatory step below

    // Step 1, unconditional: the step that backs the regularization out of
    // the answer, taken through the single-step primitive so that function
    // remains what the border equivalence tests are written against.
    Vec best = refine_bordered_solve(K0, var_count, border_v, border_d, schur, rhs, sol, opts);
    if (schur.dim() == 0) {
        if (steps_out != nullptr) {
            *steps_out = steps;
        }
        return best; // not bordered: solve_eqp's arithmetic, and its budget
    }

    const Vec reg = bordered_regularization(K0.rows(), var_count, border_d, opts);
    const double rel_floor =
        detail::kBorderRefineRelFloor * std::max(1.0, rhs.lpNorm<Eigen::Infinity>());

    Vec best_residual = residual_of(best);
    double best_norm = best_residual.lpNorm<Eigen::Infinity>();
    for (Index step = 1; step < detail::kMaxBorderRefineSteps; ++step) {
        const double footprint = reg.cwiseProduct(best).lpNorm<Eigen::Infinity>();
        if (!(best_norm > std::max(footprint, rel_floor))) {
            break; // the first step did its job (or NaN, which no step can mend)
        }
        const Vec candidate = best + schur.solve(best_residual);
        Vec candidate_residual = residual_of(candidate);
        const double candidate_norm = candidate_residual.lpNorm<Eigen::Infinity>();
        if (!(candidate_norm < best_norm)) {
            break; // stagnated or diverging -- keep the better iterate
        }
        best = candidate;
        best_residual = std::move(candidate_residual);
        best_norm = candidate_norm;
        ++steps;
    }
    if (steps_out != nullptr) {
        *steps_out = steps;
    }
    return best;
}

// The value a non-free variable is held at -- the same rule assemble_kkt and
// solve_eqp use for x_fixed. kFree is not a pinned state; it returns
// qp.lower(i) so the function is total, and callers must not ask.
inline double pinned_value(const QpProblem &qp, BoundState state, Index i) {
    return state == BoundState::kAtUpper ? qp.upper(i) : qp.lower(i);
}

// Solves the EQP for `ws`'s working set through the bordered K0 described
// above. `k0` is assemble_kkt_full's output for the working set K0 was built
// from, `k0_rows` that working set's active_ineq() (sorted), `schur` the live
// border stack and `ledger` its bookkeeping, in add_border call order.
// `ws` is the CURRENT working set, which the ledger's borders bring K0 up to.
//
// Throws whatever SchurComplement::solve throws -- notably std::runtime_error
// on an exactly singular Schur complement, which the caller is expected to
// have ruled out via needs_refactorization() first.
inline EqpResult solve_bordered_eqp(const QpProblem &qp, const KktAssembly &k0,
                                    const std::vector<Index> &k0_rows, const SchurComplement &schur,
                                    const std::vector<BorderLedgerEntry> &ledger,
                                    const WorkingSet &ws, const QpOptions &opts) {
    const Index n = qp.n();
    const Index me = qp.me();
    const Index n0 = k0.K.rows();
    const Index nw0 = static_cast<Index>(k0_rows.size());
    const Index m = static_cast<Index>(ledger.size());
    const std::vector<BoundState> &bound_state = ws.bound_state();

    // rhs head: [-g | be | bi over K0's own working rows]. rhs_shift is
    // all-zero in full mode, so there is nothing to subtract.
    Vec rhs = Vec::Zero(n0 + m);
    rhs.head(n) = -qp.g;
    for (Index r = 0; r < me; ++r) {
        rhs(n + r) = qp.be(r);
    }
    for (Index k = 0; k < nw0; ++k) {
        rhs(n + me + k) = qp.bi(k0_rows[static_cast<std::size_t>(k)]);
    }

    // Rebuild each live border's column and diagonal from the ledger (the
    // ledger is the single source of truth for what `schur` currently holds),
    // and fill in its rhs entry.
    std::vector<Vec> border_v;
    std::vector<double> border_d;
    border_v.reserve(static_cast<std::size_t>(m));
    border_d.reserve(static_cast<std::size_t>(m));
    std::vector<bool> k0_row_deleted(static_cast<std::size_t>(nw0), false);
    for (Index b = 0; b < m; ++b) {
        const BorderLedgerEntry &e = ledger[static_cast<std::size_t>(b)];
        switch (e.kind) {
        case BorderLedgerEntry::Kind::kVarPin:
            border_v.push_back(BorderOps::pin_variable(e.target, n0));
            border_d.push_back(-opts.dual_mu);
            rhs(n0 + b) =
                pinned_value(qp, bound_state[static_cast<std::size_t>(e.target)], e.target);
            break;
        case BorderLedgerEntry::Kind::kRowDelete:
            border_v.push_back(BorderOps::delete_k0_row(e.target, me, n, n0));
            border_d.push_back(0.0);
            rhs(n0 + b) = 0.0;
            k0_row_deleted[static_cast<std::size_t>(e.target - me)] = true;
            break;
        case BorderLedgerEntry::Kind::kIneqRow:
            border_v.push_back(BorderOps::add_ineq_row(qp, e.target, n0));
            border_d.push_back(-opts.dual_mu);
            rhs(n0 + b) = qp.bi(e.target);
            break;
        }
    }

    Vec sol = schur.solve(rhs);
    Index refine_steps = 0;
    sol = refine_bordered_solve_iterated(k0.K, n, border_v, border_d, schur, rhs, sol, opts,
                                         &refine_steps);

    EqpResult res;
    res.refine_steps = refine_steps;
    res.x = sol.head(n);
    for (Index i = 0; i < n; ++i) {
        const BoundState st = bound_state[static_cast<std::size_t>(i)];
        if (st != BoundState::kFree) {
            res.x(i) = pinned_value(qp, st, i); // see the header note on pins
        }
    }
    res.lambda_e = sol.segment(n, me);

    const std::vector<Index> &aw = ws.active_ineq();
    res.lambda_w = Vec::Zero(static_cast<Index>(aw.size()));
    for (std::size_t k = 0; k < aw.size(); ++k) {
        const Index row = aw[k];
        const auto it = std::lower_bound(k0_rows.begin(), k0_rows.end(), row);
        if (it != k0_rows.end() && *it == row) {
            const auto p = static_cast<std::size_t>(it - k0_rows.begin());
            if (!k0_row_deleted[p]) {
                res.lambda_w(static_cast<Index>(k)) = sol(n + me + static_cast<Index>(p));
                continue;
            }
        }
        for (Index b = 0; b < m; ++b) {
            const BorderLedgerEntry &e = ledger[static_cast<std::size_t>(b)];
            if (e.kind == BorderLedgerEntry::Kind::kIneqRow && e.target == row) {
                res.lambda_w(static_cast<Index>(k)) = sol(n0 + b);
                break;
            }
        }
    }
    return res;
}

} // namespace hven::solvers
