// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// Dense symmetric-indefinite factor/solve: LAPACK dsytrf (Bunch-Kaufman,
// either triangle) to factorize, LAPACK dsytrs to solve against the cached
// factorization. The small dense primitive the sparse layer's border-stack
// consumers (a dense Schur complement bordering a fixed sparse
// factorization) layer on -- deliberately small and self-contained: no
// counters, and the one evidence surface it does carry
// (BunchKaufmanBlockEvidence) reports facts about the factored blocks
// only, never a verdict about what those facts mean.
//
// Backend routing: Intel MKL's own LAPACKE header on Linux/Windows;
// hven/detail/linear/lapacke_shim.h (a shim over Apple Accelerate's
// classic Fortran-ABI LAPACK) on Apple, since Accelerate ships no native
// LAPACKE. See dense_symmetric_factor.cpp for the platform #if.

#include <optional>
#include <vector>

#include "hven/core/types.h"

namespace hven::linear {

// Which triangle of the input matrix LAPACK reads -- dsytrf/dsytrs' `uplo`
// argument, named rather than spelled as a raw 'U'/'L' char at this
// boundary. NOT a cosmetic choice of storage: Bunch-Kaufman with 'U'
// eliminates from the last column and with 'L' from the first, so the two
// produce different pivot sequences, different rounding, and different
// block evidence on the very same symmetric matrix. A factorization
// carries the triangle it was taken with, and every later read against it
// (solve, block evidence) uses that same triangle.
enum class Triangle { kUpper, kLower };

// What try_factorize() reports back. Distinct from the sparse surface's
// FactorizeOutcome (symmetric_factor.h), which is a struct carrying a
// backend status plus evidence -- these are the two outcomes the DENSE
// LAPACK path can report without throwing, hence the name.
enum class DenseFactorizeOutcome {
    kOk,             // dsytrf completed; factorized() is now true
    kExactlySingular // dsytrf reported info > 0 (an exactly zero pivot):
                     // the factorization ran to completion but is not
                     // usable, so factorized() stays FALSE
};

// Facts read off a completed Bunch-Kaufman factorization's block-diagonal
// D: the |eigenvalue| of every diagonal block (1x1 blocks contribute
// |d_ii|; 2x2 blocks contribute both of their eigenvalues, via the
// trace/determinant/discriminant decode) and how many of those
// eigenvalues are negative.
//
// EVIDENCE, NOT VERDICT. This type answers "what did the blocks come out
// as", never "is this matrix acceptable" -- condition thresholds,
// nearly-singular fractions, and inertia gates are the consuming stack's
// policy, not this factor's facts. It follows InertiaEvidence's
// absent-never-zero discipline through the accessor that produces it:
// there is no factorization to read blocks off of unless factorized() is
// true, and block_evidence() returns std::nullopt in that case rather
// than an empty list that would read as "no blocks, no negatives".
//
// The uplo convention the walk depends on is owned by the factor that
// produced the factorization, not by this struct's reader: the block
// pairing and the off-diagonal's location differ between 'U' and 'L', and
// no LAPACK internal (neither the factored matrix nor the pivot array) is
// exported for a caller to re-walk on its own.
struct BunchKaufmanBlockEvidence {
    // One entry per 1x1 block, two per 2x2 block, in ascending block
    // order -- so its size is dim(), not the block count.
    std::vector<double> block_abs_eigs;

    // How many of those eigenvalues are strictly negative: the
    // factorization's negative-inertia count.
    Index neg_eigs = 0;
};

class DenseSymmetricFactor {
  public:
    // Factorizes a symmetric matrix A -- only the upper triangle is read
    // (LAPACK dsytrf, Bunch-Kaufman). Replaces any factorization
    // previously held by this object as soon as validation passes: dim()
    // reflects A's size from that point on regardless of whether dsytrf
    // itself succeeds, and factorized() is false until it does.
    //
    // Throws std::invalid_argument if A is not square or is 0x0. Throws
    // std::runtime_error (fmt-formatted, carrying LAPACK's info code) if
    // dsytrf itself fails -- info < 0 is an illegal-argument bug internal
    // to this class; info > 0 means A is exactly singular (a zero pivot
    // at that 1-based index) and cannot be factorized this way.
    void factorize(ConstMatRef A);

    // Same, reading the triangle the caller names. factorize(A) above is
    // exactly this call with Triangle::kUpper -- same validation, same
    // throw-on-info contract, same state transitions -- so an existing
    // single-argument caller is unaffected by this overload's existence.
    //
    // Additionally throws std::invalid_argument if `triangle` is not one
    // of the two enumerators (reachable only by casting a foreign value
    // into the enum).
    void factorize(ConstMatRef A, Triangle triangle);

    // Factorizes like factorize(A, triangle), but reports an exactly
    // singular matrix (dsytrf info > 0) as an OUTCOME instead of throwing
    // -- for consumers whose contract treats "completed but singular" as
    // a reportable state they degrade on rather than an error. On
    // kExactlySingular, factorized() stays false, so solve() and
    // block_evidence() behave exactly as they do after any other
    // unsuccessful factorize.
    //
    // Every OTHER failure still throws, identically to factorize(): a
    // non-square, empty, or wrong-triangle argument throws
    // std::invalid_argument, and info < 0 (an illegal argument value
    // internal to this class -- a bug here, not a property of A) throws
    // std::runtime_error. Only exact singularity is demoted to a return
    // value.
    [[nodiscard]] DenseFactorizeOutcome try_factorize(ConstMatRef A, Triangle triangle);

    // Solves A x = rhs for every column of RHS, writing each result into
    // the matching column of X (LAPACK dsytrs against the factorization
    // cached by the most recent successful factorize()). X must already
    // be sized to match RHS -- this method never resizes X itself (X is
    // an Eigen::Ref, not an owning matrix).
    //
    // A zero-column RHS is accepted and is a no-op (X is untouched).
    //
    // Throws std::runtime_error if called before a successful
    // factorize(). Throws std::invalid_argument if RHS's row count does
    // not equal dim(), or if X's shape does not match RHS's shape.
    void solve(ConstMatRef RHS, MatRef X) const;

    // True iff the most recent factorize() call completed successfully.
    bool factorized() const noexcept;

    // The dimension of the most recently attempted factorize() call (0 if
    // factorize() has never been called). Valid regardless of
    // factorized() -- see factorize()'s doc comment above.
    Index dim() const noexcept;

    // The triangle the most recently attempted factorize() call read
    // (Triangle::kUpper if factorize() has never been called). Valid
    // regardless of factorized(), on the same terms as dim().
    Triangle triangle() const noexcept;

    // The Bunch-Kaufman block facts of the cached factorization, walked
    // with the triangle convention it was taken under -- see
    // BunchKaufmanBlockEvidence.
    //
    // std::nullopt whenever factorized() is false: never factorized,
    // factorization failed, or factorization completed exactly singular.
    // Absent, not empty and not zero-filled -- a factorization that did
    // not complete usably has no block eigenvalues to report, and "0
    // negative eigenvalues" does not follow from it.
    //
    // Computed on demand from the cached factors and pivots (O(dim())),
    // not cached: a consumer that reads it per solve should hold onto the
    // result itself. Throws std::runtime_error only if the cached pivot
    // array is malformed in a way a successful dsytrf cannot produce (a
    // can't-happen bounds guard, never a consequence of what the caller
    // passed).
    std::optional<BunchKaufmanBlockEvidence> block_evidence() const;

  private:
    // Shared factorization core behind all three entry points above:
    // validates, resets state, runs dsytrf, and returns LAPACK's raw info
    // (0 on success). Sets factorized_ only on info == 0; the callers own
    // the policy for what a nonzero info means (throw vs. report).
    // `caller` names the entry point in any thrown message, so each entry
    // point's diagnostics name itself.
    int factorize_core(ConstMatRef A, Triangle triangle, const char *caller);

    Mat factors_;           // dsytrf output (the read triangle + Bunch-Kaufman D), column-major
    std::vector<int> ipiv_; // dsytrf/dsytrs pivot indices, LAPACKE convention
    Index dim_ = 0;
    Triangle triangle_ = Triangle::kUpper;
    bool factorized_ = false;
};

} // namespace hven::linear
