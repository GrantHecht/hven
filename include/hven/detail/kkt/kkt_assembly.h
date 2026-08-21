#pragma once

// kkt_assembly.h — builds the regularized, bound-eliminated KKT system for
// the CURRENT working set (WorkingSet, working_set.h):
//
//     K = [ H_FF + delta*I   Ae_F^T   Aw_F^T ;
//           Ae_F              -mu*I    0     ;
//           Aw_F              0        -mu*I ]
//
// where F is the set of "free" variables (WorkingSet::bound_state() ==
// BoundState::kFree) and Aw is the sub-matrix of Ai's ROWS currently in the
// working set (WorkingSet::active_ineq()). Variables that are NOT free are
// eliminated by substitution at their bound value (qp.lower(i) for
// kAtLower/kFixed, qp.upper(i) for kAtUpper) rather than appearing in K;
// their contribution moves to the right-hand side (see rhs_shift below).
//
// K is returned as the UPPER triangle only, with rows laid out as
// [free-variable Hessian rows | equality rows | working-inequality rows],
// sizes [n_free | me | n_working].
//
// THE UPPER-TRIANGLE CONVENTION IS NOT IN THE TYPE. `hven::SpMatRM` is a plain
// row-major double sparse matrix; nothing about it says which half of a
// symmetric matrix is stored. Every symmetric `SpMatRM` this layer builds or
// consumes -- the K assembled here, qp_problem.h's H, and what the KKT factor
// is handed -- holds the UPPER triangle only, and the sparse backends are
// driven on that assumption. A caller that fills both triangles, or the lower
// one, is silently wrong. (Until M3 phase-C S2b the SQP layer spelled this type
// `SpMatU` and let the `U` carry the convention; the vocabulary is now the
// library's, so the convention is recorded here, at the fill site, instead.)
//
// --- rhs_shift semantics ---
//
// Eliminating the fixed variables by substitution moves known terms to the
// right-hand side of the stationarity/constraint equations. `rhs_shift` is
// exactly those terms, one entry per row of K, in the SAME row order as K:
//
//   - Hessian rows (indices [0, n_free)): for the free variable at full
//     index i = free_of_full[k],
//         rhs_shift(k) = sum over fixed j of H_full(i, j) * x_fixed(j)
//     where H_full is the SYMMETRIZED Hessian (qp.H stores only the upper
//     triangle with row <= col, so a stored entry H(p, q) with p <= q
//     contributes to this sum through whichever of p, q is the free index,
//     using the other as the fixed column/row by symmetry).
//   - Equality rows (indices [n_free, n_free + me)), one per row r of Ae:
//         rhs_shift(n_free + r) = sum over fixed j of Ae(r, j) * x_fixed(j)
//   - Working-inequality rows (indices [n_free + me, n_free + me +
//     n_working)), one per row of Ai in ws.active_ineq() (sorted order):
//         rhs_shift(...) = sum over fixed j of Ai(row, j) * x_fixed(j)
//
// rhs_shift is defined with a "+" sign, i.e. "the value the substituted
// variables contribute to the LHS of that row's equation" (Hx + ... for
// Hessian rows, Ax for constraint rows). Task 7 assembles the actual solve
// right-hand side from g, be, bi (and the working-set's bi-Ai*x terms) and
// is expected to SUBTRACT rhs_shift from it before solving through the
// KKT factor,
// since the reduced system is K y = b - rhs_shift.
//
// --- assemble_kkt_full: the GMSW (Gill-Murray-Saunders-Wright) border form
// ---
//
// assemble_kkt_full builds the SAME kind of K, but eliminates NO variables:
// K0 spans ALL n variables (the full H + delta*I block, not just the free
// ones), all me equality rows, and the working inequality rows named by
// `ws`. free_of_full is the identity map [0, 1, ..., n-1] and rhs_shift is
// all-zeros -- there is no substitution to shift onto the right-hand side,
// because nothing was substituted out.
//
// This is deliberate: it keeps K0's sparsity pattern (and Pardiso's symbolic
// analysis of it) fixed as the working set changes. A variable that would
// have been eliminated in assemble_kkt's reduced form is instead PINNED by
// bordering K0 with a unit column e_i and diagonal -opts.dual_mu
// (SchurComplement::add_border), with the bound VALUE entering through that
// border's right-hand-side entry (not through rhs_shift, which stays zero).
// See tests/test_qp_engine_border.cpp's BorderPinEquivalence for the
// pin-by-border vs. pin-by-elimination equivalence this relies on.

#include <stdexcept>
#include <vector>

#include <Eigen/SparseCore>
#include <fmt/format.h>

#include <hven/detail/qp/qp_problem.h>
#include <hven/detail/qp/working_set.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

struct KktAssembly {
    SpMatRM K;                       // (n_free + me + n_working) square, upper triangle stored
    std::vector<Index> free_of_full; // free-index -> full-variable-index map, increasing
    Vec rhs_shift;                   // see header comment above; same row layout as K
};

namespace detail {

// Shared triplet-build core for assemble_kkt (eliminate_bounds = true) and
// assemble_kkt_full (eliminate_bounds = false). When eliminate_bounds is
// false, full_to_free is forced to the identity map (every variable is
// "free" for assembly purposes regardless of ws.bound_state()), which makes
// every Hessian/Ae/Ai column lookup below land in the "both sides free"
// branch -- so rhs_shift comes out all-zeros automatically, with no
// special-casing needed in the loops themselves.
inline KktAssembly assemble_kkt_core(const QpProblem &qp, const WorkingSet &ws,
                                     const QpOptions &opts, bool eliminate_bounds,
                                     const char *who) {
    const Index n = qp.n();
    const Index me = qp.me();

    if (static_cast<Index>(ws.bound_state().size()) != n) {
        throw std::invalid_argument(
            fmt::format("{}: WorkingSet has {} variables, expected {} (= qp.n())", who,
                        ws.bound_state().size(), n));
    }
    if (ws.mi() != qp.mi()) {
        throw std::invalid_argument(
            fmt::format("{}: WorkingSet has {} inequality rows, expected {} (= qp.mi())", who,
                        ws.mi(), qp.mi()));
    }
    // THE BOX IS CHECKED TOO (M3 final review, S-6). The two checks above
    // measure the WorkingSet against the QP; these measure the QP against
    // ITSELF, because the bound-elimination path below reads qp.lower(i) /
    // qp.upper(i) at every variable the working set pins -- indices that come
    // from ws.bound_state(), whose size is n by the first check, and NOT from
    // lower/upper's own length. A QP carrying a short box (n == 2 with
    // lower.size() == 1, variable 1 kAtLower) is an assert-only read past the
    // end in Debug and an unguarded one in Release.
    //
    // TWO SIZE COMPARISONS, NOT qp.validate(). The full validation is O(nnz)
    // in H/Ae/Ai and this function runs once per working-set change on the
    // refactorize path, so calling it here would put a pattern-sized scan
    // inside a hot loop -- and would re-validate, on every driver path, a QP
    // the driver already validated once. The two O(1) checks close the actual
    // out-of-bounds read at no measurable cost.
    //
    // REACHABILITY: every in-tree driver validates its QP upstream of this
    // call, so the throws are dead on all of them. The consumer that reaches
    // them is a direct user of this detail header, which is the same class of
    // caller the assembly's existing WorkingSet checks are written for.
    if (qp.lower.size() != n) {
        throw std::invalid_argument(fmt::format("{}: qp.lower has size {}, expected {} (= qp.n())",
                                                who, qp.lower.size(), n));
    }
    if (qp.upper.size() != n) {
        throw std::invalid_argument(fmt::format("{}: qp.upper has size {}, expected {} (= qp.n())",
                                                who, qp.upper.size(), n));
    }

    const std::vector<BoundState> &bound_state = ws.bound_state();
    const std::vector<Index> &aw = ws.active_ineq();
    const Index n_working = static_cast<Index>(aw.size());

    // Free-variable index map (full index -> free position, or -1 if not
    // free) and its inverse (free position -> full index). Both are built
    // scanning i = 0..n-1 in order, so free_of_full is strictly increasing
    // and relative order among free variables matches their full-index
    // order -- this is relied on below when placing Hessian entries.
    std::vector<Index> full_to_free(static_cast<std::size_t>(n), -1);
    std::vector<Index> free_of_full;
    free_of_full.reserve(static_cast<std::size_t>(n));
    if (eliminate_bounds) {
        for (Index i = 0; i < n; ++i) {
            if (bound_state[static_cast<std::size_t>(i)] == BoundState::kFree) {
                full_to_free[static_cast<std::size_t>(i)] = static_cast<Index>(free_of_full.size());
                free_of_full.push_back(i);
            }
        }
    } else {
        for (Index i = 0; i < n; ++i) {
            full_to_free[static_cast<std::size_t>(i)] = i;
            free_of_full.push_back(i);
        }
    }
    const Index n_free = static_cast<Index>(free_of_full.size());

    // Substitution value for every non-free variable (unused for free ones,
    // and unused entirely when eliminate_bounds is false since every
    // variable is free).
    Vec x_fixed = Vec::Zero(n);
    if (eliminate_bounds) {
        for (Index i = 0; i < n; ++i) {
            switch (bound_state[static_cast<std::size_t>(i)]) {
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
    }

    const Index dim = n_free + me + n_working;
    const Index eq_off = n_free;
    const Index ineq_off = n_free + me;

    std::vector<Eigen::Triplet<double>> trips;
    Vec rhs_shift = Vec::Zero(dim);

    // Hessian block: H_FF into K's top-left block (delta added below);
    // couplings that touch a fixed variable go into rhs_shift's Hessian
    // rows instead. qp.H stores only entries with row <= col; symmetrizing
    // means a free row (p) with fixed col (q) contributes via H(p,q), and a
    // fixed row (p) with free col (q) contributes via the SAME stored
    // value H(p,q) but attributed to free row q (since H(q,p) == H(p,q)).
    for (Index i = 0; i < n; ++i) {
        for (SpMatRM::InnerIterator it(qp.H, i); it; ++it) {
            const Index j = it.col();
            const double val = it.value();
            const Index fi = full_to_free[static_cast<std::size_t>(i)];
            const Index fj = full_to_free[static_cast<std::size_t>(j)];
            if (fi >= 0 && fj >= 0) {
                trips.emplace_back(fi, fj, val);
            } else if (fi >= 0) { // i free, j fixed
                rhs_shift(fi) += val * x_fixed(j);
            } else if (fj >= 0) { // j free, i fixed (i != j, else fi == fj)
                rhs_shift(fj) += val * x_fixed(i);
            }
            // both fixed: no free Hessian row to receive a contribution.
        }
    }
    for (Index k = 0; k < n_free; ++k) {
        trips.emplace_back(k, k, opts.primal_delta);
    }

    // Equality rows: Ae_F^T into K's top-right block; fixed columns into
    // rhs_shift's equality rows; -mu*I on the equality diagonal.
    for (Index r = 0; r < me; ++r) {
        for (SpMatRM::InnerIterator it(qp.Ae, r); it; ++it) {
            const Index j = it.col();
            const double val = it.value();
            const Index fj = full_to_free[static_cast<std::size_t>(j)];
            if (fj >= 0) {
                trips.emplace_back(fj, eq_off + r, val);
            } else {
                rhs_shift(eq_off + r) += val * x_fixed(j);
            }
        }
        trips.emplace_back(eq_off + r, eq_off + r, -opts.dual_mu);
    }

    // Working-inequality rows: same pattern as equality rows, over ws's
    // active rows in sorted order.
    for (Index k = 0; k < n_working; ++k) {
        const Index row = aw[static_cast<std::size_t>(k)];
        for (SpMatRM::InnerIterator it(qp.Ai, row); it; ++it) {
            const Index j = it.col();
            const double val = it.value();
            const Index fj = full_to_free[static_cast<std::size_t>(j)];
            if (fj >= 0) {
                trips.emplace_back(fj, ineq_off + k, val);
            } else {
                rhs_shift(ineq_off + k) += val * x_fixed(j);
            }
        }
        trips.emplace_back(ineq_off + k, ineq_off + k, -opts.dual_mu);
    }

    SpMatRM K(dim, dim);
    K.setFromTriplets(trips.begin(), trips.end());
    K.makeCompressed();

    return KktAssembly{std::move(K), std::move(free_of_full), std::move(rhs_shift)};
}

} // namespace detail

inline KktAssembly assemble_kkt(const QpProblem &qp, const WorkingSet &ws, const QpOptions &opts) {
    return detail::assemble_kkt_core(qp, ws, opts, /*eliminate_bounds=*/true, "assemble_kkt");
}

// Full-variable (GMSW border) assembly: see the header comment block above
// for the K0/border layout this produces. `ws` still supplies the working
// inequality rows (Ai's rows to include, via ws.active_ineq()) and the me
// equality rows come from qp unconditionally, exactly as in assemble_kkt;
// only bound elimination is skipped. free_of_full is the identity map and
// rhs_shift is all-zeros.
inline KktAssembly assemble_kkt_full(const QpProblem &qp, const WorkingSet &ws,
                                     const QpOptions &opts) {
    return detail::assemble_kkt_core(qp, ws, opts, /*eliminate_bounds=*/false, "assemble_kkt_full");
}

} // namespace hven::solvers
