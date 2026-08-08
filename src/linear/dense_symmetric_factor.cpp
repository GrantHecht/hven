#include "hven/linear/dense_symmetric_factor.h"

#include <stdexcept>

#include <fmt/format.h>

// Backend routing follows the same pattern as the sibling SQP engine's own
// dense Bunch-Kaufman consumer: USE_ACCELERATE_SPARSE is defined
// project-wide (see cmake/hven_sparse_backend.cmake) exactly on Apple,
// where Accelerate ships no native LAPACKE and the migrated shim stands in
// for it. Everywhere else, MKL's own LAPACKE header is already on the
// include path via the hven target's PUBLIC propagation.
#ifdef USE_ACCELERATE_SPARSE
#include "hven/detail/linear/lapacke_shim.h"
using hven::linear::detail::lapack_int;
using hven::linear::detail::LAPACKE_dsytrf;
using hven::linear::detail::LAPACKE_dsytrs;
#else
#include <mkl_lapacke.h>
#endif

namespace hven::linear {

void DenseSymmetricFactor::factorize(ConstMatRef A) {
    if (A.rows() != A.cols()) {
        throw std::invalid_argument(
            fmt::format("DenseSymmetricFactor::factorize: matrix must be square, got {}x{}",
                        A.rows(), A.cols()));
    }
    if (A.rows() == 0) {
        throw std::invalid_argument(
            "DenseSymmetricFactor::factorize: matrix must be non-empty, got a 0x0 matrix");
    }

    const Index n = static_cast<Index>(A.rows());

    // dim() and factorized() must both reflect this attempt from here on,
    // even if dsytrf below fails -- a stale "still factorized at the old
    // size" state would be a plausible-looking but wrong answer.
    factorized_ = false;
    dim_ = n;
    factors_ = A;
    ipiv_.assign(static_cast<std::size_t>(n), 0);

    const auto info =
        static_cast<int>(LAPACKE_dsytrf(LAPACK_COL_MAJOR, 'U', static_cast<lapack_int>(n),
                                        factors_.data(), static_cast<lapack_int>(n), ipiv_.data()));
    if (info != 0) {
        throw std::runtime_error(fmt::format(
            "DenseSymmetricFactor::factorize: LAPACKE_dsytrf failed, info={} ({})", info,
            info < 0 ? "illegal argument value" : "matrix is exactly singular (zero pivot)"));
    }

    factorized_ = true;
}

void DenseSymmetricFactor::solve(ConstMatRef RHS, MatRef X) const {
    if (!factorized_) {
        throw std::runtime_error(
            "DenseSymmetricFactor::solve: called before a successful factorize()");
    }
    if (RHS.rows() != dim_) {
        throw std::invalid_argument(
            fmt::format("DenseSymmetricFactor::solve: RHS has {} rows, factorization dim is {}",
                        RHS.rows(), dim_));
    }
    if (X.rows() != RHS.rows() || X.cols() != RHS.cols()) {
        throw std::invalid_argument(fmt::format(
            "DenseSymmetricFactor::solve: X is {}x{} but RHS is {}x{} -- shapes must match",
            X.rows(), X.cols(), RHS.rows(), RHS.cols()));
    }

    const auto n = static_cast<lapack_int>(dim_);
    const auto nrhs = static_cast<lapack_int>(X.cols());

    // LAPACKE_dsytrs solves in place and requires contiguous column-major
    // storage with leading dimension == n. X's own storage satisfies that
    // whenever it is not itself a strided view into a larger matrix (e.g.
    // a block of a caller-owned matrix); when it is, solve into an owned
    // contiguous scratch buffer and copy the result back into X.
    // Correctness first -- this component is small-matrix by design.
    if (X.outerStride() == X.rows()) {
        X = RHS;
        const auto info = static_cast<int>(LAPACKE_dsytrs(
            LAPACK_COL_MAJOR, 'U', n, nrhs, factors_.data(), n, ipiv_.data(), X.data(), n));
        if (info != 0) {
            throw std::runtime_error(
                fmt::format("DenseSymmetricFactor::solve: LAPACKE_dsytrs failed, info={}", info));
        }
    } else {
        Mat scratch = RHS;
        const auto info = static_cast<int>(LAPACKE_dsytrs(
            LAPACK_COL_MAJOR, 'U', n, nrhs, factors_.data(), n, ipiv_.data(), scratch.data(), n));
        if (info != 0) {
            throw std::runtime_error(
                fmt::format("DenseSymmetricFactor::solve: LAPACKE_dsytrs failed, info={}", info));
        }
        X = scratch;
    }
}

bool DenseSymmetricFactor::factorized() const noexcept { return factorized_; }

Index DenseSymmetricFactor::dim() const noexcept { return dim_; }

} // namespace hven::linear
