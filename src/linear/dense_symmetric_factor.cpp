#include "hven/linear/dense_symmetric_factor.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <fmt/format.h>

// Backend routing: HVEN_USE_ACCELERATE_LAPACK is defined project-wide (see
// cmake/hven_sparse_backend.cmake) exactly on Apple, where Accelerate ships
// no native LAPACKE and the migrated shim stands in for it. Everywhere
// else, MKL's own LAPACKE header is already on the include path via the
// hven target's PUBLIC propagation. This is a dedicated macro, not a reuse
// of USE_ACCELERATE_SPARSE (the sparse-backend selector) -- see
// hven_sparse_backend.cmake's doc comment for why the two are named
// separately even though they are set together today.
#ifdef HVEN_USE_ACCELERATE_LAPACK
#include "hven/detail/linear/lapacke_shim.h"
using hven::linear::detail::lapack_int;
using hven::linear::detail::LAPACKE_dsytrf;
using hven::linear::detail::LAPACKE_dsytrs;
#else
#include <mkl_lapacke.h>
#endif

namespace hven::linear {

namespace {

// The dsytrf/dsytrs `uplo` char for a Triangle, validated at the boundary:
// an enum class value can still be a foreign integer cast into the enum,
// and handing LAPACK an unknown uplo char would be caught (if at all) as an
// opaque negative info far from the caller that caused it.
char uplo_char(Triangle triangle, const char *caller) {
    switch (triangle) {
    case Triangle::kUpper:
        return 'U';
    case Triangle::kLower:
        return 'L';
    }
    throw std::invalid_argument(fmt::format("DenseSymmetricFactor::{}: unknown Triangle value {}",
                                            caller, static_cast<int>(triangle)));
}

} // namespace

int DenseSymmetricFactor::factorize_core(ConstMatRef A, Triangle triangle, const char *caller) {
    if (A.rows() != A.cols()) {
        throw std::invalid_argument(
            fmt::format("DenseSymmetricFactor::{}: matrix must be square, got {}x{}", caller,
                        A.rows(), A.cols()));
    }
    if (A.rows() == 0) {
        throw std::invalid_argument(fmt::format(
            "DenseSymmetricFactor::{}: matrix must be non-empty, got a 0x0 matrix", caller));
    }
    const char uplo = uplo_char(triangle, caller);

    const Index n = static_cast<Index>(A.rows());

    // dim() and factorized() must both reflect this attempt from here on,
    // even if dsytrf below fails -- a stale "still factorized at the old
    // size" state would be a plausible-looking but wrong answer. triangle()
    // moves with them for the same reason: block_evidence() and solve()
    // decode against the triangle THIS attempt used.
    factorized_ = false;
    dim_ = n;
    triangle_ = triangle;
    factors_ = A;
    ipiv_.assign(static_cast<std::size_t>(n), 0);

    const auto info =
        static_cast<int>(LAPACKE_dsytrf(LAPACK_COL_MAJOR, uplo, static_cast<lapack_int>(n),
                                        factors_.data(), static_cast<lapack_int>(n), ipiv_.data()));
    factorized_ = (info == 0);
    return info;
}

void DenseSymmetricFactor::factorize(ConstMatRef A) { factorize(A, Triangle::kUpper); }

void DenseSymmetricFactor::factorize(ConstMatRef A, Triangle triangle) {
    const int info = factorize_core(A, triangle, "factorize");
    if (info != 0) {
        throw std::runtime_error(fmt::format(
            "DenseSymmetricFactor::factorize: LAPACKE_dsytrf failed, info={} ({})", info,
            info < 0 ? "illegal argument value" : "matrix is exactly singular (zero pivot)"));
    }
}

DenseFactorizeOutcome DenseSymmetricFactor::try_factorize(ConstMatRef A, Triangle triangle) {
    const int info = factorize_core(A, triangle, "try_factorize");
    if (info < 0) {
        // A bug in this class's own call, not a property of A: still a
        // throw here, exactly as in factorize(). Only info > 0 -- exact
        // singularity, a fact ABOUT A -- is demoted to an outcome.
        throw std::runtime_error(
            fmt::format("DenseSymmetricFactor::try_factorize: LAPACKE_dsytrf failed, info={} "
                        "(illegal argument value)",
                        info));
    }
    return info > 0 ? DenseFactorizeOutcome::kExactlySingular : DenseFactorizeOutcome::kOk;
}

std::optional<BunchKaufmanBlockEvidence> DenseSymmetricFactor::block_evidence() const {
    if (!factorized_) {
        return std::nullopt;
    }

    // Walk the Bunch-Kaufman block structure (LAPACK dsytrf documentation):
    // ipiv_[k] > 0 marks a 1x1 block at k, and a negative entry marks a 2x2
    // block, which BOTH uplo conventions record as an adjacent equal
    // negative PAIR -- (k-1, k) for 'U', (k, k+1) for 'L' (1-based) -- so a
    // forward scan that treats the first negative it meets as the block's
    // top-left index is correct under either convention. What the triangle
    // does change is where D's off-diagonal entry lives: the superdiagonal
    // (k, k+1) when the upper triangle was factored, the subdiagonal
    // (k+1, k) when the lower was.
    //
    // The 2x2 trace/determinant/discriminant arithmetic below is migrated
    // VERBATIM from the SQP border stack's own walk, whose cond_estimate()
    // consumers are float-load-bearing: the expression forms and their
    // evaluation order are the contract here, not just the values they
    // nominally compute.
    const bool upper = (triangle_ == Triangle::kUpper);
    BunchKaufmanBlockEvidence evidence;
    evidence.block_abs_eigs.reserve(static_cast<std::size_t>(dim_));

    Index k = 0;
    while (k < dim_) {
        const auto uk = static_cast<std::size_t>(k);
        if (ipiv_[uk] > 0) {
            const double dii = factors_(k, k);
            evidence.block_abs_eigs.push_back(std::abs(dii));
            if (dii < 0.0) {
                ++evidence.neg_eigs;
            }
            k += 1;
        } else {
            // Bounds guard, not an expected path: a successful dsytrf never
            // reports a 0 pivot entry, and never opens a 2x2 block on the
            // last index (its partner column would not exist). Either would
            // step this walk off the end of factors_ -- and Eigen's own
            // bounds asserts are compiled out under NDEBUG, so they cannot
            // be the only thing standing between malformed pivot data and
            // an out-of-bounds read.
            if (ipiv_[uk] == 0 || k + 1 >= dim_) {
                throw std::runtime_error(fmt::format(
                    "DenseSymmetricFactor::block_evidence: malformed dsytrf pivot data at index "
                    "{} of {} (ipiv={}) -- cannot decode the Bunch-Kaufman block structure",
                    k, dim_, ipiv_[uk]));
            }
            const double a11 = factors_(k, k);
            const double a21 = upper ? factors_(k, k + 1) : factors_(k + 1, k);
            const double a22 = factors_(k + 1, k + 1);
            const double trace = a11 + a22;
            const double det = a11 * a22 - a21 * a21;
            const double disc = std::sqrt(std::max(0.0, trace * trace - 4.0 * det));
            const double e1 = (trace + disc) / 2.0;
            const double e2 = (trace - disc) / 2.0;
            evidence.block_abs_eigs.push_back(std::abs(e1));
            evidence.block_abs_eigs.push_back(std::abs(e2));
            if (e1 < 0.0) {
                ++evidence.neg_eigs;
            }
            if (e2 < 0.0) {
                ++evidence.neg_eigs;
            }
            k += 2;
        }
    }

    return evidence;
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

    if (X.cols() == 0) {
        return; // no columns to solve; avoid handing LAPACK an empty/null buffer
    }

    const auto n = static_cast<lapack_int>(dim_);
    const auto nrhs = static_cast<lapack_int>(X.cols());

    // LAPACKE_dsytrs solves in place and requires contiguous column-major
    // storage with leading dimension == n. X's own storage satisfies that
    // whenever it is not itself a strided view into a larger matrix (e.g. a
    // block of a caller-owned matrix); when it is, solve into an owned
    // contiguous scratch buffer instead and copy the result back into X
    // afterward. Correctness first -- this component is small-matrix by
    // design. Both cases fill in `target` (the buffer LAPACK writes into)
    // and share one dsytrs call below, rather than duplicating the call and
    // its error check per branch.
    const bool x_contiguous = X.outerStride() == X.rows();
    Mat scratch;
    double *target = nullptr;
    if (x_contiguous) {
        X = RHS;
        target = X.data();
    } else {
        scratch = RHS;
        target = scratch.data();
    }

    // The triangle the cached factorization was TAKEN with -- dsytrs decodes
    // the packed factors under the same convention dsytrf packed them with,
    // so this is never a free choice at solve time. Validated at factorize
    // time; re-validated here only in the sense that uplo_char() has no
    // other way to reach LAPACK.
    const char uplo = uplo_char(triangle_, "solve");

    const auto info = static_cast<int>(LAPACKE_dsytrs(LAPACK_COL_MAJOR, uplo, n, nrhs,
                                                      factors_.data(), n, ipiv_.data(), target, n));
    if (info != 0) {
        throw std::runtime_error(
            fmt::format("DenseSymmetricFactor::solve: LAPACKE_dsytrs failed, info={}", info));
    }

    if (!x_contiguous) {
        X = scratch;
    }
}

bool DenseSymmetricFactor::factorized() const noexcept { return factorized_; }

Index DenseSymmetricFactor::dim() const noexcept { return dim_; }

Triangle DenseSymmetricFactor::triangle() const noexcept { return triangle_; }

} // namespace hven::linear
