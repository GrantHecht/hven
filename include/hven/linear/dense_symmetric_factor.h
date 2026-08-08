#pragma once

// Dense symmetric-indefinite factor/solve: LAPACK dsytrf (Bunch-Kaufman,
// upper triangle) to factorize, LAPACK dsytrs to solve against the cached
// factorization. This is the small dense factor/solve primitive the
// sparse layer's border-stack consumers (a dense Schur complement
// bordering a fixed sparse factorization) will layer on later --
// deliberately small and self-contained: no inertia surface, no counters
// (added only when a consumer reads them).
//
// Backend routing: Intel MKL's own LAPACKE header on Linux/Windows;
// hven/detail/linear/lapacke_shim.h (a shim over Apple Accelerate's
// classic Fortran-ABI LAPACK) on Apple, since Accelerate ships no native
// LAPACKE. See dense_symmetric_factor.cpp for the platform #if.

#include <vector>

#include "hven/core/types.h"

namespace hven::linear {

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

    // Solves A x = rhs for every column of RHS, writing each result into
    // the matching column of X (LAPACK dsytrs against the factorization
    // cached by the most recent successful factorize()). X must already
    // be sized to match RHS -- this method never resizes X itself (X is
    // an Eigen::Ref, not an owning matrix).
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

  private:
    Mat factors_;           // dsytrf output (upper triangle + Bunch-Kaufman D), column-major
    std::vector<int> ipiv_; // dsytrf/dsytrs pivot indices, LAPACKE convention
    Index dim_ = 0;
    bool factorized_ = false;
};

} // namespace hven::linear
