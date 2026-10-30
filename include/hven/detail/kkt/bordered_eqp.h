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
#include <cmath>
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

// The regularization this bordered system carries on its diagonal that is an
// ERROR to be backed out: NOTHING on K0's first `var_count` (Hessian) rows,
// -dual_mu on every remaining K0 (equality/working) row, and each border's own
// d_i on its row. Sized n0 + border_d.size().
//
// THE PRIMAL `delta` IS DELIBERATELY ABSENT (M7 W2 E4, ruling E4-1). K0 carries
// it on every variable row as it always did, but with the regularization centred
// at the iterate it is the PROXIMAL PENALTY -- a term of the model this system
// poses, not an error in it -- exactly as on the eliminated path
// (eqp_solve.h's Iterative refinement section, which states the reasoning and the
// measurement).
//
// THE ITERATED LOOP'S STOPPING RULE READS THE SAME VECTOR, and that is the same
// decision restated, not a second one: the rule stops when the residual is beneath
// "the regularization's own footprint on this answer", i.e. beneath the error the
// solve deliberately leaves, and delta is no longer such an error. Leaving delta in
// it would stop the loop at ||delta*sol||inf, and under the proximal centring what
// is left there is the walk's OWN stationarity residual, which its KKT-point test
// (qp_engine.h section 5) reads.
//
// WHAT THAT DOES AND DOES NOT EXPLAIN, measured both ways on the 51-dump corpus:
// the two forms give the SAME 51 rows on both algebras, so the four certified
// `ACOPP57_def` cells the border path LOST in E4's first round (kOptimal in 8
// minors -> kMaxIter at 1795, terminal stationarity 1.25e-07 against the
// eliminated path's 1.02e-14 on the same face) were NOT this rule's doing -- they
// were the border path's own accuracy on that face, amplified by the proximal
// step's 1/delta, and fix round 1's unregularized polish recovered them (kOptimal
// in 2 minors at 9.7e-15, parity with the eliminated path). The delta-free form is
// kept because it is what the model says, not because it moved a row.
inline Vec bordered_regularization(Index n0, Index var_count, const std::vector<double> &border_d,
                                   const QpOptions &opts) {
    const Index m = static_cast<Index>(border_d.size());
    Vec reg = Vec::Zero(n0 + m);
    reg.segment(var_count, n0 - var_count).setConstant(-opts.dual_mu);
    for (Index i = 0; i < m; ++i) {
        reg(n0 + i) = border_d[static_cast<std::size_t>(i)];
    }
    return reg;
}

// Residual of `sol` against the bordered system AS REGULARIZED -- rhs - A*sol
// with every diagonal regularization left in place. This is what the factor was
// asked to solve, so it is the object the EQP-site residual check reads
// (eqp_solve.h's own section; E4-4) and the term the refinement residual below is
// built from.
inline Vec bordered_regularized_residual(const SpMatRM &K0, Index var_count,
                                         const std::vector<Vec> &border_v,
                                         const std::vector<double> &border_d, const Vec &rhs,
                                         const Vec &sol) {
    const Index n0 = K0.rows();
    const Index m = static_cast<Index>(border_v.size());
    (void)var_count;

    const Vec x0 = sol.head(n0);
    Vec Ax = Vec::Zero(n0 + m);
    Ax.head(n0) = K0.template selfadjointView<Eigen::Upper>() * x0;
    for (Index i = 0; i < m; ++i) {
        const double yi = sol(n0 + i);
        Ax.head(n0) += border_v[static_cast<std::size_t>(i)] * yi;
        Ax(n0 + i) = border_v[static_cast<std::size_t>(i)].dot(x0) +
                     border_d[static_cast<std::size_t>(i)] * yi;
    }
    return Vec(rhs - Ax);
}

// Residual of `sol` against the bordered system the MODEL poses -- the
// regularized one with every entry bordered_regularization reports backed out.
//
// Every diagonal entry that function reports regularizes a mathematically exact
// equation, so backing all of them out of the residual -- computable without ever
// forming that matrix, since K0_true = K0 - diag(reg) implies
// r = rhs - K0_true*sol = (rhs - K0*sol) + diag(reg)*sol -- targets the same
// solution solve_eqp's own refinement step converges to. Since E4 that is the
// PROXIMAL system on both paths: the primal delta is not in `reg` and so is not
// backed out here either.
inline Vec bordered_residual(const SpMatRM &K0, Index var_count, const std::vector<Vec> &border_v,
                             const std::vector<double> &border_d, const Vec &rhs, const Vec &sol,
                             const QpOptions &opts) {
    const Vec reg = bordered_regularization(K0.rows(), var_count, border_d, opts);
    return Vec(bordered_regularized_residual(K0, var_count, border_v, border_d, rhs, sol) +
               reg.cwiseProduct(sol));
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
// ONE EXCEPTION, M6 W2 T6b: at a DEAD END the classifier is about to certify
// kInfeasible from, the engine refines the closed face past this footprint to
// the classifier's own row-unit tolerance (qp_engine.h section 5's
// VERDICT-SITE FACE REFINEMENT) -- confined there because a wrong verdict
// costs more than out-refining the oracle on that one point.
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
//
// THE BORDERED ANSWER'S CERTIFICATE (M7 W2 BPS), two readings the loop makes
// of its own refinement (iterative refinement's a-posteriori quantities;
// Higham, Accuracy and Stability of Numerical Algorithms, ch. 12), both on the
// independent working set's walk only and from an iterate that meets its
// equality rows (solve_bordered_eqp passes `outcome` and `resolution` there and
// nowhere else, iterate_meets_equality_rows below; null, the loop is exactly the
// footprint rule above). The footprint rule's premise -- below the footprint, what is
// left is the face's own conditioning, which the eliminated path carries
// identically -- is not a theorem: K0 is assembled bound-free, so on a face
// whose curvature is definite only once its pins are applied ||K0^-1|| ~
// 1/primal_delta (above), and the bordered answer can be wrong far beyond what
// the eliminated path leaves. Measured on two ACOPR118 sub-QPs (the BPS
// fixtures), each a full-step standstill to the minor cap on a face the
// eliminated path certifies in a handful of minors:
//   * TARGET MET (`outcome`). One face's refinement DIVERGES along its soft
//     direction (contraction ~1.14 per step): the loop stops on stagnation
//     ABOVE its own target max(footprint, rel_floor), with the answer 0.83 off
//     the eliminated one and proposing ASCENDING full steps. An answer whose
//     loop did not reach its own target is uncertified, and the caller
//     (border_solve_or_fall_back) takes the eliminated twin for that pass.
//   * STEP RESOLVED (`resolution`). On the other face the loop meets its
//     target after one step with the answer still 3.3e-9 off -- exactly the
//     step it proposes, back and forth, while one more step would bring it to
//     5.7e-12, below step_tol (1e-9) and so to zero motion. The walk's
//     KKT-point test compares ||p||inf with step_tol; with e the last kept
//     correction's free-variable part (under a contracting refinement, a bound
//     on the answer's remaining error), that comparison is decided when
//     ||p_F||inf - e > step_tol or ||p_F||inf + e <= step_tol. An undecided
//     answer takes further steps past the footprint, each kept only if it CONTRACTS
//     (residual and correction both at most half of the previous; strict
//     decrease alone would accept a drift along the soft direction). This is
//     best effort, NOT a certificate: where the bordered form's own rounding
//     floor sits above step_tol the answer stays as it was (a fallback there
//     was measured, and it hands the pass to the eliminated twin's own
//     one-step row miss, which then cycles with the border). Only steps within
//     e of step_tol are undecided, so it is inert away from the zero-motion
//     scale. STEP RESOLVED classifies the refinement's answer BEFORE
//     polish_bordered_unregularized runs, and that polish can move the answer by up
//     to kEqpPolishStepCap * max(1, |sol|), so the bracket is on the refined answer,
//     not on the returned step.
struct BorderedResolution {
    const Vec *x_k = nullptr;                             // the proximal centre (the iterate)
    const std::vector<BoundState> *bound_state = nullptr; // which variables are free
    double step_tol = 0.0;                                // the walk's zero-motion scale
};

struct BorderedRefineOutcome {
    bool target_met = true;
};

inline Vec refine_bordered_solve_iterated(
    const SpMatRM &K0, Index var_count, const std::vector<Vec> &border_v,
    const std::vector<double> &border_d, const SchurComplement &schur, const Vec &rhs,
    const Vec &sol, const QpOptions &opts, Index *steps_out = nullptr,
    const BorderedResolution *resolution = nullptr, BorderedRefineOutcome *outcome = nullptr) {
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

    // STEP RESOLVED's quantities (NaN-sticky: a NaN entry makes the norm NaN, and a
    // NaN reads undecided, which the strict-decrease rule then stops).
    const auto free_inf = [&](const Vec &v) {
        double m = 0.0;
        for (Index i = 0; i < var_count; ++i) {
            if ((*resolution->bound_state)[static_cast<std::size_t>(i)] == BoundState::kFree) {
                const double a = std::abs(v(i));
                if (!(a <= m)) {
                    m = a;
                }
            }
        }
        return m;
    };
    double last_correction = resolution != nullptr ? free_inf(Vec(best - sol)) : 0.0;
    const auto step_resolved = [&]() {
        if (resolution == nullptr) {
            return true;
        }
        const double p = free_inf(Vec(best.head(var_count) - *resolution->x_k));
        const double tol = resolution->step_tol;
        return p - last_correction > tol || p + last_correction <= tol;
    };

    const Vec reg = bordered_regularization(K0.rows(), var_count, border_d, opts);
    const double rel_floor =
        detail::kBorderRefineRelFloor * std::max(1.0, rhs.lpNorm<Eigen::Infinity>());

    Vec best_residual = residual_of(best);
    double best_norm = best_residual.lpNorm<Eigen::Infinity>();
    for (Index step = 1; step < detail::kMaxBorderRefineSteps; ++step) {
        const double footprint = reg.cwiseProduct(best).lpNorm<Eigen::Infinity>();
        const bool on_target = !(best_norm > std::max(footprint, rel_floor));
        if (on_target && step_resolved()) {
            break; // the first step did its job (or NaN, which no step can mend)
        }
        const Vec correction = schur.solve(best_residual);
        const Vec candidate = best + correction;
        Vec candidate_residual = residual_of(candidate);
        const double candidate_norm = candidate_residual.lpNorm<Eigen::Infinity>();
        if (!(candidate_norm < best_norm)) {
            break; // stagnated or diverging -- keep the better iterate
        }
        const double correction_free = resolution != nullptr ? free_inf(correction) : 0.0;
        if (on_target &&
            !(candidate_norm <= 0.5 * best_norm && correction_free <= 0.5 * last_correction)) {
            break; // resolving past the footprint: contracting steps only
        }
        best = candidate;
        best_residual = std::move(candidate_residual);
        best_norm = candidate_norm;
        last_correction = correction_free;
        ++steps;
    }
    if (steps_out != nullptr) {
        *steps_out = steps;
    }
    if (outcome != nullptr) {
        const double footprint = reg.cwiseProduct(best).lpNorm<Eigen::Infinity>();
        outcome->target_met = !(best_norm > std::max(footprint, rel_floor));
    }
    return best;
}

// The value a non-free variable is held at -- the same rule assemble_kkt and
// solve_eqp use for x_fixed. kFree is not a pinned state; it returns
// qp.lower(i) so the function is total, and callers must not ask.
inline double pinned_value(const QpProblem &qp, BoundState state, Index i) {
    return state == BoundState::kAtUpper ? qp.upper(i) : qp.lower(i);
}

// The bordered system's right-hand side and border columns, rebuilt from the
// ledger -- the single source of truth for what `schur` currently holds.
//
// Split out of solve_bordered_eqp so the VERDICT-SITE face refinement
// (qp_engine.h section 5) can re-form the SAME system from the same live
// border stack without duplicating the ledger's conventions. It is a pure
// function of its arguments and its result depends on `ws` only through
// bound_state(), which nothing between an EQP solve and that verdict moves.
// THE UNREGULARIZED POLISH, the border twin (M7 W2 E4 fix round 1, ruling E4-11;
// eqp_solve.h's polish_eqp_core is the eliminated form and states the rule and its
// safety argument in full).
//
// The loop above targets the system the MODEL poses -- the PROXIMAL one since E4,
// with the primal delta deliberately left in as a term rather than backed out as an
// error. That answer is the face's exact KKT point only in the limit x_k -> x*, and
// away from it it carries an O(delta*||x* - x_k||) bias. This function takes at most
// `max_steps` further steps against the FACE'S OWN EXACT system -- the primal delta
// backed out of K0's `var_count` variable rows AND its `+delta*x_k` term taken back
// off the rhs -- each KEPT only while that residual's inf-norm STRICTLY FALLS, so a
// flat face (whose exact system has no answer along its null space) rejects the
// first step and keeps the proximal answer untouched. Every step is ALSO capped in
// size by eqp_solve.h's kEqpPolishStepCap, for the reason stated there. Returns the
// number of steps kept; `sol` is updated in place.
//
// `x_k` must be the SAME proximal centre build_bordered_system used, since what is
// removed here is exactly what it added.
inline Index polish_bordered_unregularized(const SpMatRM &K0, Index var_count,
                                           const std::vector<Vec> &border_v,
                                           const std::vector<double> &border_d,
                                           const SchurComplement &schur, const Vec &rhs,
                                           const Vec &x_k, const QpOptions &opts, Index max_steps,
                                           Vec &sol) {
    if (max_steps <= 0) {
        return 0;
    }
    const Index n0 = K0.rows();
    // The rhs of the face's own exact system: the bordered rhs with the proximal
    // term removed from the variable rows it was added to.
    Vec rhs0 = rhs;
    rhs0.head(var_count) -= opts.primal_delta * x_k.head(var_count);
    // Everything bordered_regularization reports, PLUS the primal delta on the
    // variable rows -- K0 carries it there and here it IS an error to back out.
    Vec reg = bordered_regularization(n0, var_count, border_d, opts);
    reg.head(var_count).array() += opts.primal_delta;
    const auto residual_of = [&](const Vec &ss) {
        return Vec(bordered_regularized_residual(K0, var_count, border_v, border_d, rhs0, ss) +
                   reg.cwiseProduct(ss));
    };
    Vec res = residual_of(sol);
    double res_inf = res.lpNorm<Eigen::Infinity>();
    Index kept = 0;
    for (Index step = 0; step < max_steps; ++step) {
        const Vec delta = schur.solve(res);
        const double cap = detail::kEqpPolishStepCap * std::max(1.0, sol.lpNorm<Eigen::Infinity>());
        if (!(delta.lpNorm<Eigen::Infinity>() <= cap)) {
            break; // not a polish: see eqp_solve.h's kEqpPolishStepCap
        }
        const Vec candidate = sol + delta;
        Vec cres = residual_of(candidate);
        const double cres_inf = cres.lpNorm<Eigen::Infinity>();
        if (!(cres_inf < res_inf)) {
            break; // stagnated, diverging, or NaN -- keep the better answer
        }
        sol = candidate;
        res = std::move(cres);
        res_inf = cres_inf;
        ++kept;
    }
    return kept;
}

struct BorderedSystem {
    Vec rhs;
    std::vector<Vec> border_v;
    std::vector<double> border_d;
    // One flag per k0_rows entry, true iff a kRowDelete border has pinned that
    // K0 row's dual to zero. Such a row's residual entry is the border's to
    // absorb, not that row's own reading.
    std::vector<bool> k0_row_deleted;
};

inline BorderedSystem build_bordered_system(const QpProblem &qp, const KktAssembly &k0,
                                            const std::vector<Index> &k0_rows,
                                            const std::vector<BorderLedgerEntry> &ledger,
                                            const WorkingSet &ws, const Vec &x_k,
                                            const QpOptions &opts) {
    const Index n = qp.n();
    const Index me = qp.me();
    const Index n0 = k0.K.rows();
    const Index nw0 = static_cast<Index>(k0_rows.size());
    const Index m = static_cast<Index>(ledger.size());
    const std::vector<BoundState> &bound_state = ws.bound_state();

    BorderedSystem sys;
    // rhs head: [-g | be | bi over K0's own working rows]. rhs_shift is
    // all-zero in full mode, so there is nothing to subtract.
    sys.rhs = Vec::Zero(n0 + m);
    sys.rhs.head(n) = -qp.g;
    // THE PROXIMAL CENTRE (M7 W2 E4; eqp_solve.h states the system), on EVERY
    // variable row, and the per-variable justification is the pin border:
    //
    //   * a FREE variable's row is the eliminated path's own free stationarity row,
    //     and gets the same +delta*x_k,i there.
    //   * a PINNED variable's row is one the eliminated path does not have at all --
    //     it substitutes x_i out. K0 nevertheless carries +delta on that row (it is
    //     assembled bound-free), so what the eliminated path's ELIMINATION implies
    //     for it is that the delta term is absent. Adding delta*x_k,i supplies
    //     exactly that: the pin border holds x_i at its bound, which IS x_k,i, so
    //     delta*x_i - delta*x_k,i cancels to O(dual_mu*delta*|y_i|) ~ 1e-16 and the
    //     row is delta-free, as it was before E4 (where refinement backed the same
    //     term out of the same rows). MASKING the term instead LEAVES a delta*x_i
    //     the eliminated path never has: measured, that is a 1e-16 perturbation of
    //     x through the pin (x_i = bound + mu*y_i), and it is enough to flip a
    //     weakly-active bound on `QpEngineBorder.BorderModeMatchesRefactorizeMode`
    //     -- same x to 1e-8, different `bound_state`. So: unmasked, per variable,
    //     for the reason each variable gives, and the cross-algebra battery and
    //     corpus are the check.
    sys.rhs.head(n) += opts.primal_delta * x_k;
    for (Index r = 0; r < me; ++r) {
        sys.rhs(n + r) = qp.be(r);
    }
    for (Index k = 0; k < nw0; ++k) {
        sys.rhs(n + me + k) = qp.bi(k0_rows[static_cast<std::size_t>(k)]);
    }

    sys.border_v.reserve(static_cast<std::size_t>(m));
    sys.border_d.reserve(static_cast<std::size_t>(m));
    sys.k0_row_deleted.assign(static_cast<std::size_t>(nw0), false);
    for (Index b = 0; b < m; ++b) {
        const BorderLedgerEntry &e = ledger[static_cast<std::size_t>(b)];
        switch (e.kind) {
        case BorderLedgerEntry::Kind::kVarPin:
            sys.border_v.push_back(BorderOps::pin_variable(e.target, n0));
            sys.border_d.push_back(-opts.dual_mu);
            sys.rhs(n0 + b) =
                pinned_value(qp, bound_state[static_cast<std::size_t>(e.target)], e.target);
            break;
        case BorderLedgerEntry::Kind::kRowDelete:
            sys.border_v.push_back(BorderOps::delete_k0_row(e.target, me, n, n0));
            sys.border_d.push_back(0.0);
            sys.rhs(n0 + b) = 0.0;
            sys.k0_row_deleted[static_cast<std::size_t>(e.target - me)] = true;
            break;
        case BorderLedgerEntry::Kind::kIneqRow:
            sys.border_v.push_back(BorderOps::add_ineq_row(qp, e.target, n0));
            sys.border_d.push_back(-opts.dual_mu);
            sys.rhs(n0 + b) = qp.bi(e.target);
            break;
        }
    }
    return sys;
}

// WHERE THE CERTIFICATE IS ASKED (refine_bordered_solve_iterated, both readings):
// on an iterate that meets every equality row to feas_tol in the walk's own row
// scale, max(1, |be_j|, ||Ae_j||_1 ||x||inf). There the face's rows are met, the
// exact answer is a pure stationarity correction, and an answer the factor cannot
// vouch for is what turns a resolved face into a standstill. Off the rows the step
// carries a row-restoring part and the walk is still restoring; that is exactly
// where E2 F8-1's candidate S fired (its trigger required a structurally violated
// equality row) and where it cost ACOPP57 (a 569-pass jitter at a 9.87e-6 row
// residual that the walk leaves by itself), so the certificate and S's trigger are
// disjoint by construction. Measured both ways: asked on every pass, the
// target-met fallback fired 25 times in ACOPP118 qp0's row-restoring opening
// (fix 9's fixture) and sent it to a cap; asked on the rows only, it fires there 0
// times and that walk is unchanged.
inline bool iterate_meets_equality_rows(const QpProblem &qp, const Vec &x, const QpOptions &opts) {
    const double xmag = x.lpNorm<Eigen::Infinity>();
    for (Index j = 0; j < qp.me(); ++j) {
        double row_norm1 = 0.0;
        double ax = 0.0;
        for (SpMatRM::InnerIterator it(qp.Ae, j); it; ++it) {
            row_norm1 += std::abs(it.value());
            ax += it.value() * x(it.col());
        }
        const double row_scale = std::max({1.0, std::abs(qp.be(j)), row_norm1 * xmag});
        if (!(std::abs(ax - qp.be(j)) <= opts.feas_tol * row_scale)) {
            return false;
        }
    }
    return true;
}

// Solves the EQP for `ws`'s working set through the bordered K0 described
// above. `k0` is assemble_kkt_full's output for the working set K0 was built
// from, `k0_rows` that working set's active_ineq() (sorted), `schur` the live
// border stack and `ledger` its bookkeeping, in add_border call order.
// `ws` is the CURRENT working set, which the ledger's borders bring K0 up to.
// `x_k` is the PROXIMAL CENTRE, of size qp.n() -- the same required argument, with
// the same meaning, as solve_eqp's; eqp_solve.h states the system both solve.
//
// Throws whatever SchurComplement::solve throws -- notably std::runtime_error
// on an exactly singular Schur complement, which the caller is expected to
// have ruled out via needs_refactorization() first.
inline EqpResult solve_bordered_eqp(const QpProblem &qp, const KktAssembly &k0,
                                    const std::vector<Index> &k0_rows, const SchurComplement &schur,
                                    const std::vector<BorderLedgerEntry> &ledger,
                                    const WorkingSet &ws, const Vec &x_k, const QpOptions &opts) {
    const Index n = qp.n();
    const Index me = qp.me();
    const Index n0 = k0.K.rows();
    const Index m = static_cast<Index>(ledger.size());
    const std::vector<BoundState> &bound_state = ws.bound_state();
    if (x_k.size() != n) {
        throw std::invalid_argument(
            fmt::format("solve_bordered_eqp: proximal centre has size {}, expected n = {}",
                        static_cast<long long>(x_k.size()), static_cast<long long>(n)));
    }
    const BorderedSystem sys = build_bordered_system(qp, k0, k0_rows, ledger, ws, x_k, opts);

    Vec sol = schur.solve(sys.rhs);

    // ---- THE EQP-SITE RESIDUAL CHECK (E4-4), the border twin. Same object, same
    // tolerance, same budget, same two-block ratio as eqp_solve.h's -- read off the
    // REGULARIZED bordered system, with the rows block being everything below the n
    // variable rows (K0's equality/working rows AND the live borders, each of which
    // IS a constraint row). Runs BEFORE the model's refinement, for the reason
    // stated there: afterwards the row residual is the model's own mu*lambda.
    const Index nc = n0 + m - n;
    const double rhs_rows_inf = nc > 0 ? sys.rhs.segment(n, nc).lpNorm<Eigen::Infinity>() : 0.0;
    // The constraint rows only, for the reason (and with the measurement)
    // eqp_solve.h states at its own copy of this rule.
    const auto ratio_of = [&](const Vec &ss, const Vec &r) {
        if (nc == 0) {
            return 0.0;
        }
        const double scale = std::max(detail::kEqpResidualRefuseTol * (1.0 + rhs_rows_inf),
                                      opts.dual_mu * ss.segment(n, nc).lpNorm<Eigen::Infinity>());
        return r.segment(n, nc).lpNorm<Eigen::Infinity>() / scale;
    };
    Vec r_reg = bordered_regularized_residual(k0.K, n, sys.border_v, sys.border_d, sys.rhs, sol);
    double ratio = ratio_of(sol, r_reg);
    Index res_iters = 0;
    while (!(ratio <= 1.0) && res_iters < detail::kEqpResidualRefineIters) {
        ++res_iters; // spent, kept or not -- solver_counters.h's eqp_refine_iters
        const Vec candidate = sol + schur.solve(r_reg);
        Vec candidate_r =
            bordered_regularized_residual(k0.K, n, sys.border_v, sys.border_d, sys.rhs, candidate);
        const double candidate_ratio = ratio_of(candidate, candidate_r);
        if (!(candidate_ratio < ratio)) {
            break; // stagnated, diverging, or NaN -- keep the better answer
        }
        sol = candidate;
        r_reg = std::move(candidate_r);
        ratio = candidate_ratio;
    }
    const double row_residual = nc > 0 ? r_reg.segment(n, nc).lpNorm<Eigen::Infinity>() : 0.0;

    Index refine_steps = 0;
    // THE BORDERED ANSWER'S CERTIFICATE (refine_bordered_solve_iterated) -- on the
    // independent working set's walk only, so the option-off walk, the declared
    // control arm of every E2 re-take, keeps the footprint rule unchanged.
    BorderedResolution resolution;
    resolution.x_k = &x_k;
    resolution.bound_state = &bound_state;
    resolution.step_tol = opts.feas_tol * std::max(1.0, x_k.lpNorm<Eigen::Infinity>());
    BorderedRefineOutcome outcome;
    const bool certify = opts.independent_working_set && iterate_meets_equality_rows(qp, x_k, opts);
    sol = refine_bordered_solve_iterated(k0.K, n, sys.border_v, sys.border_d, schur, sys.rhs, sol,
                                         opts, &refine_steps, certify ? &resolution : nullptr,
                                         certify ? &outcome : nullptr);
    // AND THE UNREGULARIZED POLISH (E4-11), the same rule the eliminated path takes
    // at the same point in its own solve: see polish_bordered_unregularized.
    const Index polish_kept =
        polish_bordered_unregularized(k0.K, n, sys.border_v, sys.border_d, schur, sys.rhs, x_k,
                                      opts, detail::kEqpPolishSteps, sol);

    EqpResult res;
    res.polish_steps = polish_kept;
    res.refine_steps = refine_steps;
    res.border_uncertified = !outcome.target_met;
    res.row_residual = row_residual;
    res.residual_ratio = ratio;
    res.residual_refine_iters = res_iters;
    res.residual_refused = !(ratio <= 1.0);
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
            if (!sys.k0_row_deleted[p]) {
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
