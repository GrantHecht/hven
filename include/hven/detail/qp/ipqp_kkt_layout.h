// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// ipqp_kkt_layout.h -- the value-only re-valuation plan for the interior-point
// QP tier's KKT matrix.
//
// WHY. The SQP side has no value-only path onto a KKT assembly (kkt_assembly.h rebuilds
// structure and values on every call, working-set shaped) and the interior engine's own
// answer is NonLinearProgram's. The mechanism is SsnEngine::sync_matrix, re-derived here.
//
// THE SYSTEM is spec section 3.1's, slack form, dim = n + 2*mi + me, UPPER TRIANGLE only
// in the row-major CSR convention SymmetricFactor requires. Nothing STRUCTURAL moves per
// iteration -- which is what licenses `kAssumeAnalyzed` on the tier's refactorize() calls.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "hven/core/types.h"

namespace hven::solvers {

/// @brief The one-time pattern layout of the interior-point QP tier's KKT
/// matrix, plus the per-iteration scatter plan that refills it without
/// touching the pattern.
///
/// Lifecycle: `sync()` on every entry. A first call, or a structure-key miss, LAYS OUT the
/// pattern and records the plan; every other call is an O(nnz) zero-fill-and-scatter. The
/// matrix is passed by reference, never owned -- its home is KktFactorization::matrix().
class IpqpKktLayout {
  public:
    /// @brief Brings `k` up to date for (H, Ae, Ai) at the given dimensions.
    /// @return true iff the pattern was (re)laid out on this call -- i.e. the
    ///         caller must re-`compute()` its factorization rather than
    ///         `refactorize()`.
    ///
    /// After the call `k` holds H's stored upper triangle, Ae'/Ai', -1 in each coupling slot
    /// and H(i,i) in each primal diagonal; the four diagonal families are the caller's to
    /// write. ONE LAYOUT SERVES ONE BUFFER -- hand it the SAME `k` for its whole life.
    ///
    /// A VALIDATION failure leaves the object and `k` untouched. A failure INSIDE the layout
    /// fails CLOSED -- `has_structure()` goes false first, every slot accessor then refuses,
    /// and the caller's recovery is to sync again. See `.superpowers/w1-t3-report.md`.
    ///
    /// @throws std::invalid_argument on a dimension disagreement, a negative
    ///         dimension, or a below-diagonal entry in H. These are validated
    ///         here rather than left to Eigen's asserts, which are compiled
    ///         out under NDEBUG.
    bool sync(const SpMatRM &H, const SpMatRM &Ae, const SpMatRM &Ai, Index n, Index me, Index mi,
              SpMatRM &k);

    /// @brief Whether a cached plan exists AND keys to (H, Ae, Ai, n, me, mi).
    ///
    /// The same predicate `sync()` uses, exposed so a caller can choose compute() over
    /// refactorize() before spending the scatter, and so the guard is testable on its own.
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
    // `diag_pos()` is a dim-length table of each row's DIAGONAL position in `k.valuePtr()`,
    // in row order; the four bases below partition it by block, in KKTVector's order. The
    // four slot accessors are the same map, O(1), spelled shorter. Neither touches pattern.

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

    // Every accessor below refuses with std::logic_error when no plan has been laid out,
    // BEFORE it range-checks its index; only the pair makes the result safe as an index.

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
    /// A fifth family, off the diagonal and so not in `diag_pos()`. `sync()` writes -1 there;
    /// a caller that equilibrates (spec section 4.3) overwrites it through this accessor.
    std::size_t slack_coupling_slot(Index j) const;

    /// @brief H's own diagonal contribution, H(i,i), captured by the last
    /// `sync()`; 0 where H stores no diagonal entry. Length n.
    ///
    /// The inertia ladder rewrites the primal diagonal once per rung and needs H's own
    /// contribution back each time, so a rung is an ASSIGNMENT, never a read-modify-write,
    /// and never compounds the previous rung's rho. A snapshot: refreshed only by `sync()`.
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
    /// Private: `sync()` is the entry point, and calling this against a `k` the cached plan
    /// was not laid out for is exactly the corruption the structure key exists to prevent.
    void scatter(const SpMatRM &H, const SpMatRM &Ae, const SpMatRM &Ai, SpMatRM &k);

    /// The emission order both the layout and the scatter walk share, defined
    /// in the .cpp for the one-step-commit reason: `.superpowers/w1-t3-report.md:403-421`.
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

    /// The laid-out matrix's stored-entry count. Part of the reuse guard, so the key is
    /// checked against the buffer actually presented and not only against the problem.
    Index nnz_ = 0;

    std::uint64_t structure_key_ = 0;
    bool has_structure_ = false;
};

} // namespace hven::solvers
