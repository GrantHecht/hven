// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// ipqp_kkt_layout.h -- the value-only re-valuation plan for the interior-point
// QP tier's KKT matrix.
//
// WHY THIS EXISTS. The SQP side has no value-only path onto a KKT assembly:
// detail/kkt/kkt_assembly.h::assemble_kkt_core builds a triplet vector and
// calls setFromTriplets() + makeCompressed() on EVERY call, structure and
// values together, and its matrix is WORKING-SET shaped (only active
// inequality rows appear) -- the wrong shape for a barrier method that carries
// every row every iteration. The interior engine's own answer
// (NonLinearProgram::analyze_sparsity + KktLocationTable) is
// NonLinearProgram's and cannot be pointed at a QpProblem.
//
// THE TEMPLATE IS SsnEngine::sync_matrix (include/hven/detail/qp/ssn_engine.h,
// src/qp/ssn_engine.cpp): cache each emitted entry's position in the value
// array, in emission order, behind a structure-key guard with an explicit
// entry-count collision check; zero-fill and re-scatter on a match, rebuild by
// triplets otherwise. This class is that mechanism re-derived for the
// interior-point tier's own (fixed, non-working-set) block pattern.
//
// THE SYSTEM. Slack form, in KKTVector's block order
// (include/hven/detail/interior/kkt_vector.h: [primals | slacks | eq_lmults |
// iq_lmults]), with s = bi - Ai x >= 0 and lambda_i >= 0. Variable bounds are
// CONDENSED into the (1,1) diagonal and add no rows, so
//
//     dim = n + mi + me + mi = n + 2*mi + me
//
// and the Newton system is
//
//     [ H + rho I + Sigma_b     0        Ae'       Ai'    ]
//     [      0            Lam S^-1        0        -I     ]
//     [     Ae                  0     -delta I      0     ]
//     [     Ai                 -I          0     -delta I ]
//
// quasi-definite for rho, delta > 0. Only the UPPER TRIANGLE is stored, in the
// row-major CSR convention hven::linear::SymmetricFactor requires (a
// structural diagonal in every row, nothing below it) -- so Ae and Ai appear
// once each, as the (x, eq) and (x, iq) transposed blocks, and the (s, iq)
// coupling -I appears once.
//
// WHAT MOVES PER ITERATION. Nothing structural: within a subproblem only the
// four diagonal families and the -I coupling carry iteration-dependent values,
// and across majors the source values change but the pattern does not. That is
// what licenses PatternCheck::kAssumeAnalyzed on the tier's refactorize()
// calls -- the tier alone writes into the buffer, from this fixed plan, so it
// can NAME the mechanism keeping the pattern fixed rather than assert it.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "hven/core/types.h"

namespace hven::solvers {

/// @brief The one-time pattern layout of the interior-point QP tier's KKT
/// matrix, plus the per-iteration scatter plan that refills it without
/// touching the pattern.
///
/// Lifecycle: `sync()` on every entry with the current (H, Ae, Ai, n, me, mi).
/// The first call, and any call whose structure key differs from the cached
/// one, LAYS OUT the pattern (one setFromTriplets + makeCompressed) and
/// records the plan; every other call is an O(nnz) zero-fill-and-scatter with
/// no allocation, no re-sort and no setFromTriplets. Either way the caller
/// then writes the four diagonal families through the offset accessors below.
///
/// The matrix is passed in by reference on every call rather than owned:
/// its home is KktFactorization::matrix(), the buffer the linear layer takes
/// by reference at analyze()/factorize().
class IpqpKktLayout {
  public:
    /// @brief Brings `k` up to date for (H, Ae, Ai) at the given dimensions.
    /// @return true iff the pattern was (re)laid out on this call -- i.e. the
    ///         caller must re-`compute()` its factorization rather than
    ///         `refactorize()`.
    ///
    /// After the call, `k` holds: H's stored (upper-triangle) values in the
    /// (1,1) block, Ae' and Ai' in the (1,3) and (1,4) blocks, -1 in each
    /// (s_k, iq_k) coupling slot, H(i,i) in each primal diagonal slot (0 where
    /// H stores no diagonal entry), and ZERO in the slack, eq-pivot and
    /// iq-pivot diagonal slots. The four diagonal families are the caller's to
    /// write; see `primal_diag_source()` for the one that is not simply
    /// overwritten.
    ///
    /// ONE LAYOUT SERVES ONE BUFFER. A plan is a map into a specific value
    /// array, so a given IpqpKktLayout must be handed the SAME `k` for its
    /// whole life. The reuse guard checks rows, cols, `nonZeros()` and
    /// `isCompressed()`, which is strictly more than SsnEngine::sync_matrix
    /// checks (that class owns its matrix outright) -- but a shape-and-count
    /// match is not a PATTERN match: two matrices of identical dimensions and
    /// identical entry counts, with different patterns, both pass, and the
    /// scatter would then write through a map built for the other one. The
    /// structure key covers the PROBLEM; nothing here can cover a swapped
    /// buffer, so the caller must not swap one.
    ///
    /// DEGENERATE CASE. `n == me == mi == 0` is accepted and lays out a 0x0
    /// matrix with empty tables: `dim() == 0`, every slot accessor refuses,
    /// and a reuse sync is a no-op. It is legal here and rejected downstream
    /// -- the linear layer will not analyze an empty matrix -- so a caller
    /// reaching it has a problem this class is not the right place to
    /// diagnose.
    ///
    /// EXCEPTION SAFETY, in two layers because the inner one is not reachable
    /// from any fixture and so cannot be kept honest by a test.
    ///
    /// A VALIDATION failure (everything under @throws below) is raised before
    /// anything is read or written: the object and `k` are untouched, and a
    /// previously laid-out plan stays valid and addressable.
    ///
    /// A failure INSIDE the layout -- `bad_alloc` from the triplet reserve or
    /// from `setFromTriplets`, or the internal-consistency throw from the
    /// position lookup -- fails CLOSED: `has_structure()` goes false first, so
    /// every slot accessor then refuses with `std::logic_error`, and the
    /// caller's recovery is to sync again. This matters because the accessors
    /// are bounds-checked against `n_`/`me_`/`mi_` and nothing else: a state
    /// with the new dimensions stored beside the old tables would pass that
    /// check and index out of range, in Release, silently, with the result
    /// used as an index into `k.valuePtr()`. The layout therefore ALSO builds
    /// every product into locals and commits them in one step at the end, so
    /// the dimensions and the tables they describe can never disagree -- but
    /// the planless-first flag is what makes the guarantee independent of that
    /// ordering staying right.
    ///
    /// @throws std::invalid_argument on a dimension disagreement, a negative
    ///         dimension, or a below-diagonal entry in H. These are validated
    ///         here rather than left to Eigen's asserts, which are compiled
    ///         out under NDEBUG.
    bool sync(const SpMatRM &H, const SpMatRM &Ae, const SpMatRM &Ai, Index n, Index me, Index mi,
              SpMatRM &k);

    /// @brief Whether a cached plan exists AND keys to (H, Ae, Ai, n, me, mi).
    ///
    /// The same predicate `sync()` uses. Exposed so a caller can decide
    /// between compute() and refactorize() before spending the scatter, and so
    /// the guard is testable on its own.
    bool matches(const SpMatRM &H, const SpMatRM &Ae, const SpMatRM &Ai, Index n, Index me,
                 Index mi) const;

    /// True once a plan has been laid out.
    bool has_structure() const { return has_structure_; }

    /// The laid-out system's dimension, n + 2*mi + me. Zero before the first
    /// `sync()`.
    Index dim() const { return dim_; }

    // ---------------------------------------------------------------------
    // Diagonal slot addressing
    // ---------------------------------------------------------------------
    //
    // `diag_pos()` is a dim-length table of each row's DIAGONAL position in
    // `k.valuePtr()`, in row order; the four bases below partition it by
    // block, in KKTVector's order. A caller writes
    //
    //     k.valuePtr()[layout.diag_pos()[layout.primal_diag_base() + i]] = ...
    //
    // or, equivalently and more briefly, through the four slot accessors.
    // Both spellings are O(1) and neither touches the pattern.

    /// Each row's diagonal position in the value array, in row order.
    /// @throws std::logic_error if no plan has been laid out.
    const std::vector<std::size_t> &diag_pos() const {
        require_structure("diag_pos");
        return diag_pos_;
    }

    /// Index into `diag_pos()` of the (1,1) primal block's first diagonal.
    Index primal_diag_base() const { return 0; }
    /// Index into `diag_pos()` of the slack block's first diagonal.
    Index slack_diag_base() const { return n_; }
    /// Index into `diag_pos()` of the equality block's first pivot.
    Index eq_pivot_base() const { return n_ + mi_; }
    /// Index into `diag_pos()` of the inequality block's first pivot.
    Index iq_pivot_base() const { return n_ + mi_ + me_; }

    // Every accessor below refuses with std::logic_error when no plan has been
    // laid out, BEFORE it range-checks its index. The two guards answer
    // different questions -- "is there a table" and "is this index inside the
    // block" -- and only the pair of them makes the returned value safe to use
    // as an index into `k.valuePtr()`.

    /// @brief Position in `k.valuePtr()` of primal diagonal `i` -- the slot
    /// carrying H(i,i) + rho + Sigma_b(i). `i` in [0, n).
    std::size_t primal_diag_slot(Index i) const { return diag_slot(primal_diag_base(), i, n_); }

    /// @brief Position of slack diagonal `j` -- the slot carrying
    /// (Lambda S^-1)_j. `j` in [0, mi).
    std::size_t slack_diag_slot(Index j) const { return diag_slot(slack_diag_base(), j, mi_); }

    /// @brief Position of equality pivot `r` -- the slot carrying -delta.
    /// `r` in [0, me).
    std::size_t eq_pivot_slot(Index r) const { return diag_slot(eq_pivot_base(), r, me_); }

    /// @brief Position of inequality pivot `j` -- the slot carrying -delta.
    /// `j` in [0, mi).
    std::size_t iq_pivot_slot(Index j) const { return diag_slot(iq_pivot_base(), j, mi_); }

    /// @brief Position of the (s_j, iq_j) coupling slot -- the -I block.
    /// `j` in [0, mi).
    ///
    /// A fifth family, off the diagonal and therefore not in `diag_pos()`.
    /// `sync()` writes -1 there; a caller that equilibrates its system (spec
    /// section 4.3's Ruiz scaling) overwrites it through this accessor rather
    /// than re-scattering.
    std::size_t slack_coupling_slot(Index j) const;

    /// @brief H's own diagonal contribution, H(i,i), captured by the last
    /// `sync()`; 0 where H stores no diagonal entry. Length n.
    ///
    /// The inertia ladder rewrites the primal diagonal several times per
    /// iteration with a new rho, and each rewrite needs H's contribution back:
    /// without this the ladder would have to re-scatter H (O(nnz)) to recover
    /// what a previous rung overwrote, or read it back out of a slot it is
    /// about to clobber. Recorded once per layout/scatter instead, so a rung
    /// is `values[primal_diag_slot(i)] = primal_diag_source()[i] + rho +
    /// sigma[i]` -- an assignment, not a read-modify-write, so a rung never
    /// compounds the previous rung's regularization.
    ///
    /// A SNAPSHOT TAKEN AT SYNC TIME, deliberately: it records what the
    /// scatter put in those slots, and it goes stale the instant the caller
    /// writes a primal diagonal -- which is the point, since what the ladder
    /// needs back is precisely the value from BEFORE its own writes. It is
    /// refreshed by every `sync()`, layout or reuse, and by nothing else.
    ///
    /// @throws std::logic_error if no plan has been laid out.
    const std::vector<double> &primal_diag_source() const {
        require_structure("primal_diag_source");
        return primal_diag_source_;
    }

  private:
    /// @brief Zero-fills `k`'s value array and re-scatters H/Ae/Ai through the
    /// cached position map, then re-establishes the coupling block's -1 and
    /// re-reads `primal_diag_source_`.
    ///
    /// Private: `sync()` is the entry point, and calling this against a
    /// `k` the cached plan was not laid out for is exactly the corruption the
    /// structure key exists to prevent.
    void scatter(const SpMatRM &H, const SpMatRM &Ae, const SpMatRM &Ai, SpMatRM &k);

    /// The emission order both the layout and the scatter walk. Defined in the
    /// .cpp, where both call sites live: ONE emission order by construction,
    /// which is what makes the cached position map meaningful.
    ///
    /// Takes its dimensions as arguments rather than reading `n_`/`me_`/`mi_`:
    /// the layout branch runs it BEFORE those members are committed, which is
    /// what lets the commit happen in one step at the end.
    template <typename Emit>
    static void for_each_entry(const SpMatRM &H, const SpMatRM &Ae, const SpMatRM &Ai, Index n,
                               Index me, Index mi, Emit emit);

    /// Refuses, with std::logic_error, an accessor called before any plan
    /// exists. `what` names the accessor in the message.
    void require_structure(const char *what) const;

    std::uint64_t structure_hash(const SpMatRM &H, const SpMatRM &Ae, const SpMatRM &Ai, Index n,
                                 Index me, Index mi) const;

    /// Bounds-checked `diag_pos_` lookup. Bounds are checked in every build
    /// configuration, not asserted: a wrong index here would write into an
    /// unrelated entry of the KKT matrix and corrupt it silently.
    std::size_t diag_slot(Index base, Index offset, Index count) const;

    Index n_ = 0;
    Index me_ = 0;
    Index mi_ = 0;
    Index dim_ = 0;

    /// Where each entry `for_each_entry` emits landed in `k.valuePtr()`, in
    /// emission order. The scatter consumes it in the same order and checks
    /// that it consumed it EXACTLY -- see the collision guard in the .cpp.
    std::vector<std::size_t> value_pos_;
    std::vector<std::size_t> diag_pos_;
    std::vector<std::size_t> coupling_pos_;
    std::vector<double> primal_diag_source_;

    /// The laid-out matrix's stored-entry count. Part of the reuse guard: a
    /// caller hands the matrix in by reference on every call, so "same
    /// dimensions, same structure key" is checked against the buffer actually
    /// presented, not only against the problem.
    Index nnz_ = 0;

    std::uint64_t structure_key_ = 0;
    bool has_structure_ = false;
};

} // namespace hven::solvers
