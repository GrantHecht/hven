// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// border_ops.h — GMSW-87 (Gill-Murray-Saunders-Wright) border constructions
// over a FULL-mode K0 (assemble_kkt_full's output, kkt_assembly.h): K0's rows
// are laid out as [n variables | me equalities | n_w0 initial working rows],
// with n_w0 == the size of the WorkingSet K0 was assembled from. Each static
// method below builds the border COLUMN `v` for one of the four working-set
// change types; the caller pairs `v` with a scalar diagonal `d` and hands
// both to SchurComplement::add_border(v, d) (schur_complement.h), which
// extends the fixed, factored K0 with
//     [ K0   v ] [x0]   [rhs0]
//     [ v^T  d ] [y ] = [rhs1]
// without ever re-factorizing K0 itself.
//
// This header only builds `v` vectors (plain data, no SchurComplement/
// KKT-factor dependency) -- the diagonal `d` and the border's own rhs entry
// are the CALLER's responsibility (documented per-method below) because
// they depend on which regularization convention the caller wants (matching
// kkt_assembly.h's -mu*I on every other constraint row) and on the RHS
// value being pinned to (e.g. a bound value), neither of which this header
// has any business deciding.

#include <stdexcept>

#include <fmt/format.h>

#include <hven/detail/qp/qp_problem.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

// Bookkeeping tag for one currently-live border, kept by the engine in
// SchurComplement::add_border call order (so its position in this list
// matches the index SchurComplement::drop_border expects). `target` records
// what the border is ABOUT in problem terms -- a row index into qp.Ai for
// kIneqRow, the K0 constraint-row index k for kRowDelete, or a variable
// index for kVarPin -- so the engine can find "the border for row j" or
// "the pin for variable i" without having to search by content, and so it
// can reconstruct each border's rhs entry (which depends on `target`, e.g.
// qp.bi(target) for kIneqRow) when rebuilding the full rhs vector.
struct BorderLedgerEntry {
    enum class Kind {
        kIneqRow,   // target = row index into qp.Ai (an add_ineq_row border)
        kRowDelete, // target = k, the K0 constraint-row index being deactivated
        kVarPin,    // target = variable index i (a pin_variable border)
    };
    Kind kind;
    Index target;
};

struct BorderOps {
    // 1. Activate inequality row `j`, which is NOT one of K0's n_w0 initial
    //    working rows (if it were, it would already be part of K0 and
    //    should not be bordered in as well): v = Ai.row(j)^T, scattered over
    //    the FIRST qp.n() entries of the k0_rows-sized vector (the variable
    //    block), zero over the equality/working-row blocks -- exactly the
    //    column kkt_assembly.h's "Working-inequality rows" loop would have
    //    placed for this row had it been part of the initial working set.
    //
    //    The caller pairs this with d = -opts.dual_mu, matching every other
    //    constraint row's diagonal in K0 (kkt_assembly.h's -mu*I), and with
    //    a border rhs entry of qp.bi(j).
    static Vec add_ineq_row(const QpProblem &qp, Index j, Index k0_rows) {
        if (j < 0 || j >= qp.mi()) {
            throw std::invalid_argument(
                fmt::format("BorderOps::add_ineq_row: row {} out of range [0, {})", j, qp.mi()));
        }
        const Index n = qp.n();
        if (k0_rows < n) {
            throw std::invalid_argument(fmt::format(
                "BorderOps::add_ineq_row: k0_rows {} smaller than qp.n() {}", k0_rows, n));
        }
        Vec v = Vec::Zero(k0_rows);
        for (SpMatRM::InnerIterator it(qp.Ai, j); it; ++it) {
            v(it.col()) = it.value();
        }
        return v;
    }

    // 2. Deactivate the constraint occupying K0's constraint row `k`
    //    (0-indexed among K0's me + n_w0 constraint rows; that row lives at
    //    absolute position var_count + k in the k0_rows-sized system, per
    //    assemble_kkt_full's [n vars | me eq | n_w0 working] layout):
    //    v = e_{var_count + k}. The caller pairs this with d = 0 and a
    //    border rhs entry of 0.
    //
    //    WHY d = 0 AND rhs = 0, NOT -mu: with v = e_{var_count+k}, the
    //    border's own equation pins x0(var_count+k) -- the row's ORIGINAL
    //    dual -- to exactly zero, which makes the rest of the system reduce
    //    to exactly K0 with that row removed. A -mu diagonal would relax the
    //    pinning equation by O(mu), leaving a deactivation that is only
    //    approximate; an inactive constraint's row must contribute NOTHING,
    //    not O(mu). The zero rhs is what pins the dual to zero rather than
    //    to some nonzero target value.
    //
    //    Equalities are never deactivated this way: only rows that were
    //    part of K0's INITIAL working set (the n_w0 rows at constraint
    //    indices [me, me + n_w0)) may be deleted. `me` and `var_count` are
    //    both required so this precondition can be enforced here rather than
    //    left to the caller: passing a k in the equality block (k < me) or
    //    past K0's last constraint row (k >= k0_rows - var_count) throws
    //    std::invalid_argument.
    static Vec delete_k0_row(Index k, Index me, Index var_count, Index k0_rows) {
        if (me < 0 || var_count < 0 || k0_rows < var_count + me) {
            throw std::invalid_argument(
                fmt::format("BorderOps::delete_k0_row: inconsistent dimensions (me={}, "
                            "var_count={}, k0_rows={})",
                            me, var_count, k0_rows));
        }
        const Index n_constraint_rows = k0_rows - var_count;
        if (k < me || k >= n_constraint_rows) {
            throw std::invalid_argument(fmt::format(
                "BorderOps::delete_k0_row: k={} out of range [{}, {}) -- equality rows [0, {}) "
                "may never be deleted; only the initial working-inequality rows can be",
                k, me, n_constraint_rows, me));
        }
        Vec v = Vec::Zero(k0_rows);
        v(var_count + k) = 1.0;
        return v;
    }

    // 3. Pin variable `i` at a bound: v = e_i (unit vector at the variable
    //    block's position i). The caller pairs this with d = -opts.dual_mu
    //    (the same convention as every other constraint row) and carries
    //    the bound VALUE (qp.lower(i) or qp.upper(i)) in the border's rhs
    //    entry directly -- NOT through rhs_shift, which stays all-zero in
    //    full mode. See kkt_assembly.h's assemble_kkt_full header comment
    //    for the pin-by-border vs. pin-by-elimination equivalence this
    //    relies on.
    //
    //    This method only knows k0_rows, not where the variable block ends:
    //    unlike delete_k0_row, whose validity depends on distinguishing
    //    equality from working rows, an out-of-variable-range i is a caller
    //    bug this constructor cannot detect from i and k0_rows alone; it
    //    only guards the vector's own bounds.
    //
    //    Freeing a variable pinned this way is NOT a builder here: full
    //    mode never eliminates a variable from K0 (var_count == qp.n()
    //    always), so "freeing" a pin reduces exactly to
    //    SchurComplement::drop_border(k) for that pin's Schur-complement
    //    index -- there is no vector to build, only an existing border to
    //    remove. BorderLedgerEntry::kVarPin's `target` (the variable index)
    //    is how the engine finds that index.
    static Vec pin_variable(Index i, Index k0_rows) {
        if (i < 0 || i >= k0_rows) {
            throw std::invalid_argument(
                fmt::format("BorderOps::pin_variable: index {} out of range [0, {})", i, k0_rows));
        }
        Vec v = Vec::Zero(k0_rows);
        v(i) = 1.0;
        return v;
    }
};

} // namespace hven::solvers
