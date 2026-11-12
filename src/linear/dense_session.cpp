// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include "hven/detail/linear/dense_session.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include <fmt/format.h>

#include "hven/core/errors.h"
#include "hven/detail/linear/block_decode.h"
#include "hven/detail/linear/session_id.h"
#include "hven/detail/linear/thread_scope.h"

// The same platform routing as dense_symmetric_factor.cpp: MKL's LAPACKE, or
// the Accelerate shim's identically named caller-workspace entry points.
#ifdef HVEN_USE_ACCELERATE_LAPACK
#include "hven/detail/linear/lapacke_shim.h"
using hven::linear::detail::lapack_int;
using hven::linear::detail::LAPACKE_dsycon_rook_work;
using hven::linear::detail::LAPACKE_dsytrf_rook_work;
using hven::linear::detail::LAPACKE_dsytrf_work;
using hven::linear::detail::LAPACKE_dsytrs_rook_work;
using hven::linear::detail::LAPACKE_dsytrs_work;
#else
#include <mkl_lapack.h>
#include <mkl_lapacke.h>
namespace {
// MKL's LAPACKE has no dsycon_rook; the Fortran entry, in LAPACKE's shape.
lapack_int LAPACKE_dsycon_rook_work(int /*matrix_layout*/, char uplo, lapack_int n, const double *a,
                                    lapack_int lda, const lapack_int *ipiv, double anorm,
                                    double *rcond, double *work, lapack_int *iwork) {
    MKL_INT info = 0;
    const MKL_INT nn = n;
    const MKL_INT ld = lda;
    dsycon_rook(&uplo, &nn, a, &ld, ipiv, &anorm, rcond, work, iwork, &info);
    return static_cast<lapack_int>(info);
}
} // namespace
#endif

// ipiv_ is a std::vector<int> handed to LAPACK as lapack_int*.
static_assert(std::is_same_v<lapack_int, int>, "the dense arm requires an LP64 LAPACK interface");

namespace hven::linear::detail {

void validate_dense_dimension(Index n) {
    if (n < 1) {
        throw std::invalid_argument(
            fmt::format("SymmetricFactor::analyze: dense arm dimension must be >= 1, got {}", n));
    }
    constexpr auto kLapackMax = static_cast<Index>(std::numeric_limits<lapack_int>::max());
    if (n > kLapackMax) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor::analyze: the dense arm cannot take n = {}: it exceeds LAPACK's "
            "integer ({}); it would also need 8*n^2 bytes",
            n, kLapackMax));
    }
    constexpr auto kIndexMax = static_cast<std::uint64_t>(std::numeric_limits<Index>::max());
    constexpr auto kSizeMax = static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max());
    constexpr std::uint64_t kBytesMax = kIndexMax < kSizeMax ? kIndexMax : kSizeMax;
    const auto un = static_cast<std::uint64_t>(n);
    if (un > kBytesMax / un / sizeof(double)) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor::analyze: the dense arm cannot take n = {}: 8*n^2 bytes (about "
            "{:.3g}) overflows the addressable size",
            n, 8.0 * static_cast<double>(n) * static_cast<double>(n)));
    }
}

DenseInertiaCount dense_block_walk(const double *a, Index n, const int *ipiv) {
    DenseInertiaCount out;
    Index k = 0;
    while (k < n) {
        if (ipiv[k] > 0) {
            const double d = a[k * n + k];
            if (!std::isfinite(d) || d == 0.0) {
                out.singular = true;
                return out;
            }
            if (d > 0.0) {
                ++out.positive;
            } else {
                ++out.negative;
            }
            k += 1;
        } else {
            // A successful dsytrf never reports a 0 pivot entry nor opens a 2x2
            // block on the last index; either would read past the factors.
            // A 2x2 block is marked by equal negative entries on both of its
            // indices; anything else is not dsytrf's output either.
            if (ipiv[k] == 0 || k + 1 >= n || ipiv[k + 1] != ipiv[k]) {
                throw std::logic_error(fmt::format(
                    "SymmetricFactor: malformed dsytrf pivot data at index {} of {} (ipiv={}, "
                    "next={})",
                    k, n, ipiv[k], k + 1 < n ? ipiv[k + 1] : 0));
            }
            // uplo 'U': the block's off-diagonal entry is D(k, k+1).
            Block2x2 block;
            bool det_zero = false;
            if (!decode_2x2(a[k * n + k], a[(k + 1) * n + k], a[(k + 1) * n + (k + 1)], block,
                            det_zero) ||
                det_zero) {
                out.singular = true;
                return out;
            }
            out.negative += block.neg;
            out.positive += 2 - block.neg;
            k += 2;
        }
    }
    return out;
}

DenseRookCount dense_rook_walk(const double *a, Index n, const int *ipiv) {
    DenseRookCount out;
    DenseInertiaCount &c = out.counts;
    double dmin = std::numeric_limits<double>::infinity();
    double dmax = 0.0;
    Index k = 0;
    while (k < n) {
        if (ipiv[k] > 0) {
            const double d = a[k * n + k];
            if (!std::isfinite(d) || d == 0.0) {
                c.singular = true;
                return out;
            }
            if (d > 0.0) {
                ++c.positive;
            } else {
                ++c.negative;
            }
            dmin = std::min(dmin, std::abs(d));
            dmax = std::max(dmax, std::abs(d));
            k += 1;
        } else {
            // dsytrf_rook marks a 2x2 block by two negative entries (each its own
            // interchange); a zero entry or a block on the last index is not its
            // output.
            if (ipiv[k] == 0 || k + 1 >= n || ipiv[k + 1] >= 0) {
                throw std::logic_error(fmt::format(
                    "SymmetricFactor: malformed dsytrf_rook pivot data at index {} of {} (ipiv={}, "
                    "next={})",
                    k, n, ipiv[k], k + 1 < n ? ipiv[k + 1] : 0));
            }
            Block2x2 block;
            bool det_zero = false;
            if (!decode_2x2(a[k * n + k], a[(k + 1) * n + k], a[(k + 1) * n + (k + 1)], block,
                            det_zero) ||
                det_zero) {
                c.singular = true;
                return out;
            }
            c.negative += block.neg;
            c.positive += 2 - block.neg;
            dmin = std::min({dmin, block.abs_first, block.abs_second});
            dmax = std::max({dmax, block.abs_first, block.abs_second});
            k += 2;
        }
    }
    out.d_ratio = dmax > 0.0 ? dmin / dmax : 0.0;
    return out;
}

DenseSession::DenseSession(const SymmetricFactor::Options &opts, std::uint64_t initial_epoch)
    : opts_(opts), epoch_(initial_epoch), session_id_(next_session_id()) {}

void DenseSession::analyze(const SpMatRM &A) {
    const auto n = static_cast<Index>(A.rows());
    validate_dense_dimension(n);
    const auto nn = static_cast<std::size_t>(n) * static_cast<std::size_t>(n);

    // Zeroed here, so an allocation the OS cannot back fails now, not mid-solve.
    std::vector<double> a(nn, 0.0);
    std::vector<int> ipiv(static_cast<std::size_t>(n), 0);

    double query = 0.0;
    {
        const MklThreadScope threads(kDenseArmThreads);
        const auto info = static_cast<int>(
            report()
                ? LAPACKE_dsytrf_rook_work(LAPACK_COL_MAJOR, 'U', static_cast<lapack_int>(n),
                                           a.data(), static_cast<lapack_int>(n), ipiv.data(),
                                           &query, -1)
                : LAPACKE_dsytrf_work(LAPACK_COL_MAJOR, 'U', static_cast<lapack_int>(n), a.data(),
                                      static_cast<lapack_int>(n), ipiv.data(), &query, -1));
        if (info != 0) {
            throw hven::BackendError(fmt::format(
                "SymmetricFactor::analyze: dense workspace query failed, info={}", info));
        }
    }
    const double query_max = static_cast<double>(std::numeric_limits<lapack_int>::max());
    const auto lwork =
        std::max<std::size_t>(1, static_cast<std::size_t>(std::min(query, query_max)));
    std::vector<double> work(lwork, 0.0);

    const auto *outer = A.outerIndexPtr();
    const auto *inner = A.innerIndexPtr();
    std::vector<Index> scatter(static_cast<std::size_t>(A.nonZeros()));
    for (Index i = 0; i < n; ++i) {
        for (auto k = static_cast<Index>(outer[i]); k < static_cast<Index>(outer[i + 1]); ++k) {
            const auto j = static_cast<Index>(inner[k]);
            scatter[static_cast<std::size_t>(k)] = j * n + i; // (i, j), j >= i, column-major
        }
    }

    std::vector<double> colsum;
    std::vector<double> con_work;
    std::vector<int> con_iwork;
    if (report()) {
        colsum.assign(static_cast<std::size_t>(n), 0.0);
        con_work.assign(2 * static_cast<std::size_t>(n), 0.0);
        con_iwork.assign(static_cast<std::size_t>(n), 0);
    }

    n_ = n;
    a_ = std::move(a);
    ipiv_ = std::move(ipiv);
    work_ = std::move(work);
    scatter_ = std::move(scatter);
    colsum_ = std::move(colsum);
    con_work_ = std::move(con_work);
    con_iwork_ = std::move(con_iwork);
    scale_.clear();
    has_numerics_ = false;
    positive_ = 0;
    negative_ = 0;
    numerically_singular_ = false;
    shifted_pivots_ = 0;
}

void DenseSession::drop_numerics() noexcept {
    has_numerics_ = false;
    scale_.clear();
    positive_ = 0;
    negative_ = 0;
    numerically_singular_ = false;
    shifted_pivots_ = 0;
}

DenseFactorizeResult DenseSession::factorize(const SpMatRM &A, std::uint64_t next_epoch,
                                             const DenseShift *shift) {
    // The cheap bound guard that stands whatever the pattern check was: the
    // scatter below indexes a_ by the analyzed pattern.
    if (static_cast<Index>(A.rows()) != n_ || static_cast<Index>(A.cols()) != n_ ||
        static_cast<std::size_t>(A.nonZeros()) != scatter_.size()) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor::factorize: a {}x{} matrix with {} stored entries is not the analyzed "
            "{}x{} pattern with {}",
            A.rows(), A.cols(), A.nonZeros(), n_, n_, scatter_.size()));
    }
    // From here the buffer is overwritten: no numerics until this succeeds.
    drop_numerics();

    double *a = a_.data();
    for (Index j = 0; j < n_; ++j) {
        std::fill(a + j * n_, a + j * n_ + j + 1, 0.0);
    }
    const double *values = A.valuePtr();
    const std::size_t nnz = scatter_.size();
    for (std::size_t k = 0; k < nnz; ++k) {
        a[scatter_[k]] = values[k];
    }
    double shifted_norm = 0.0;
    if (shift != nullptr) {
        const double *d = shift->scale;
        for (Index j = 0; j < n_; ++j) {
            for (Index i = 0; i <= j; ++i) {
                a[j * n_ + i] *= d[i] * d[j];
            }
            a[j * n_ + j] += j < shift->n_primal ? shift->sigma : -shift->sigma;
        }
        if (report()) {
            shifted_norm = dense_one_norm();
        }
    }

    int info = 0;
    {
        const MklThreadScope threads(kDenseArmThreads);
        info = static_cast<int>(
            report() ? LAPACKE_dsytrf_rook_work(LAPACK_COL_MAJOR, 'U', static_cast<lapack_int>(n_),
                                                a, static_cast<lapack_int>(n_), ipiv_.data(),
                                                work_.data(), static_cast<lapack_int>(work_.size()))
                     : LAPACKE_dsytrf_work(LAPACK_COL_MAJOR, 'U', static_cast<lapack_int>(n_), a,
                                           static_cast<lapack_int>(n_), ipiv_.data(), work_.data(),
                                           static_cast<lapack_int>(work_.size())));
    }
    if (info < 0) {
        throw hven::BackendError(fmt::format(
            "SymmetricFactor::factorize: dense dsytrf failed, info={} (illegal argument)", info));
    }
    if (info > 0) {
        return DenseFactorizeResult::kSingular;
    }
    bool flagged = false;
    DenseInertiaCount counts;
    if (report()) {
        const DenseRookCount rook = dense_rook_walk(a, n_, ipiv_.data());
        counts = rook.counts;
        if (counts.singular) {
            return DenseFactorizeResult::kSingular;
        }
        // The screen, then the estimate; NaN flags.
        if (rook.d_ratio < kDenseSingularScreen) {
            const double anorm = shift != nullptr ? shifted_norm : csr_one_norm(A);
            flagged = !(rcond_estimate(anorm) >= kDenseConditionSingular);
        }
    } else {
        counts = dense_block_walk(a, n_, ipiv_.data());
        if (counts.singular) {
            return DenseFactorizeResult::kSingular;
        }
    }
    positive_ = counts.positive;
    negative_ = counts.negative;
    numerically_singular_ = flagged;
    shifted_pivots_ = shift != nullptr ? n_ : 0;
    if (shift != nullptr) {
        scale_.assign(shift->scale, shift->scale + n_);
    }
    // Never back, never the same: a co-owner may have advanced this shared
    // session past the epoch the calling engine knows (see the header).
    epoch_ = std::max(epoch_ + 1, next_epoch);
    has_numerics_ = true;
    return flagged ? DenseFactorizeResult::kNumericallySingular : DenseFactorizeResult::kTaken;
}

double DenseSession::csr_one_norm(const SpMatRM &A) {
    // ||A||_1 of the symmetric matrix from its stored upper triangle.
    std::fill(colsum_.begin(), colsum_.end(), 0.0);
    const auto *outer = A.outerIndexPtr();
    const auto *inner = A.innerIndexPtr();
    const double *values = A.valuePtr();
    for (Index i = 0; i < n_; ++i) {
        for (auto k = static_cast<Index>(outer[i]); k < static_cast<Index>(outer[i + 1]); ++k) {
            const auto j = static_cast<Index>(inner[k]);
            const double v = std::abs(values[k]);
            colsum_[static_cast<std::size_t>(j)] += v;
            if (j != i) {
                colsum_[static_cast<std::size_t>(i)] += v;
            }
        }
    }
    return *std::max_element(colsum_.begin(), colsum_.end());
}

double DenseSession::dense_one_norm() {
    std::fill(colsum_.begin(), colsum_.end(), 0.0);
    const double *a = a_.data();
    for (Index j = 0; j < n_; ++j) {
        for (Index i = 0; i <= j; ++i) {
            const double v = std::abs(a[j * n_ + i]);
            colsum_[static_cast<std::size_t>(j)] += v;
            if (i != j) {
                colsum_[static_cast<std::size_t>(i)] += v;
            }
        }
    }
    return *std::max_element(colsum_.begin(), colsum_.end());
}

double DenseSession::rcond_estimate(double anorm) {
    double rcond = 0.0;
    int info = 0;
    {
        const MklThreadScope threads(kDenseArmThreads);
        info = static_cast<int>(
            LAPACKE_dsycon_rook_work(LAPACK_COL_MAJOR, 'U', static_cast<lapack_int>(n_), a_.data(),
                                     static_cast<lapack_int>(n_), ipiv_.data(), anorm, &rcond,
                                     con_work_.data(), con_iwork_.data()));
    }
    ++condition_estimates_;
    if (info != 0) {
        throw hven::BackendError(fmt::format(
            "SymmetricFactor::factorize: dense dsycon_rook failed, info={} (illegal argument)",
            info));
    }
    return rcond;
}

void DenseSession::solve(double *b, Index nrhs, Index ldb) const {
    const auto apply_scale = [&] {
        for (Index c = 0; c < nrhs; ++c) {
            for (Index i = 0; i < n_; ++i) {
                b[c * ldb + i] *= scale_[static_cast<std::size_t>(i)];
            }
        }
    };
    if (!scale_.empty()) {
        apply_scale();
    }
    const MklThreadScope threads(kDenseArmThreads);
    const auto info = static_cast<int>(
        report() ? LAPACKE_dsytrs_rook_work(LAPACK_COL_MAJOR, 'U', static_cast<lapack_int>(n_),
                                            static_cast<lapack_int>(nrhs), a_.data(),
                                            static_cast<lapack_int>(n_), ipiv_.data(), b,
                                            static_cast<lapack_int>(ldb))
                 : LAPACKE_dsytrs_work(LAPACK_COL_MAJOR, 'U', static_cast<lapack_int>(n_),
                                       static_cast<lapack_int>(nrhs), a_.data(),
                                       static_cast<lapack_int>(n_), ipiv_.data(), b,
                                       static_cast<lapack_int>(ldb)));
    if (info != 0) {
        throw hven::BackendError(
            fmt::format("SymmetricFactor::solve: dense dsytrs failed, info={}", info));
    }
    if (!scale_.empty()) {
        apply_scale();
    }
}

InertiaEvidence dense_evidence_of(const DenseSession &session) {
    InertiaEvidence evidence;
    evidence.state = InertiaEvidence::State::kObserved;
    evidence.positive = session.positive();
    evidence.negative = session.negative();
    // Counted: the walk found no zero pivot, no zero 2x2 determinant and no
    // non-finite value (any of them leaves the session without numerics).
    evidence.zero = 0;
    evidence.zero_is_derived = false;
    // Present only on a shifted factorization (the pivot factorization itself
    // perturbs nothing), so the shifted inertia reads suspect.
    if (session.shifted_pivots() > 0) {
        evidence.perturbed_pivots = session.shifted_pivots();
    }
    evidence.numerically_singular = session.numerically_singular();
    return evidence;
}

FactorEvidence dense_factor_evidence_of(const DenseSession &session) {
    FactorEvidence evidence;
    evidence.dense_factor_entries = session.factor_entries();
    return evidence;
}

SolveInfo run_dense_solve(const DenseSession &session, ConstMatRef RHS, MatRef X,
                          const char *what) {
    if (RHS.rows() != session.dim()) {
        throw std::invalid_argument(
            fmt::format("SymmetricFactor::{}: right-hand side has {} rows, the factorization's "
                        "dimension is {}",
                        what, RHS.rows(), session.dim()));
    }
    if (X.rows() != RHS.rows() || X.cols() != RHS.cols()) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor::{}: solution is {}x{} but the right-hand side is {}x{} -- "
            "shapes must match",
            what, X.rows(), X.cols(), RHS.rows(), RHS.cols()));
    }
    if (X.cols() == 0) {
        return SolveInfo{};
    }
    // dsytrs solves in place with any leading dimension >= n, so a strided X
    // needs no scratch.
    const Index ldb = X.cols() == 1 ? X.rows() : static_cast<Index>(X.outerStride());
    if (ldb < X.rows()) {
        throw std::invalid_argument(
            fmt::format("SymmetricFactor::{}: solution outer stride {} is below its {} rows", what,
                        ldb, X.rows()));
    }
    // Both reach LAPACK as lapack_int; neither may narrow.
    constexpr auto kLapackMax = static_cast<Index>(std::numeric_limits<lapack_int>::max());
    if (static_cast<Index>(X.cols()) > kLapackMax || ldb > kLapackMax) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor::{}: {} right-hand sides with leading dimension {} exceed LAPACK's "
            "integer ({})",
            what, X.cols(), ldb, kLapackMax));
    }
    X = RHS;
    session.solve(X.data(), static_cast<Index>(X.cols()), ldb);
    return SolveInfo{};
}

SolveInfo run_dense_solve(const DenseSession &session, ConstVecRef rhs, VecRef x,
                          const char *what) {
    if (rhs.size() != session.dim()) {
        throw std::invalid_argument(
            fmt::format("SymmetricFactor::{}: right-hand side has {} entries, the factorization's "
                        "dimension is {}",
                        what, rhs.size(), session.dim()));
    }
    if (x.size() != rhs.size()) {
        throw std::invalid_argument(
            fmt::format("SymmetricFactor::{}: solution has {} entries but the right-hand side has "
                        "{} -- sizes must match",
                        what, x.size(), rhs.size()));
    }
    x = rhs;
    session.solve(x.data(), 1, static_cast<Index>(x.size()));
    return SolveInfo{};
}

void validate_dense_options(const SymmetricFactor::Options &opts) {
    switch (opts.dense_factorization) {
    case DensePolicy::kNever:
    case DensePolicy::kAlways:
        if (opts.max_dense_dimension.has_value()) {
            throw std::invalid_argument(fmt::format(
                "SymmetricFactor: max_dense_dimension ({}) is meaningful only under "
                "DensePolicy::kAuto, got dense_factorization = {}",
                *opts.max_dense_dimension,
                opts.dense_factorization == DensePolicy::kNever ? "kNever" : "kAlways"));
        }
        break;
    case DensePolicy::kAuto:
        if (!opts.max_dense_dimension.has_value() || *opts.max_dense_dimension < 1) {
            throw std::invalid_argument(
                "SymmetricFactor: DensePolicy::kAuto requires max_dense_dimension >= 1 (the "
                "factor layer has no library rule)");
        }
        break;
    default:
        throw std::invalid_argument(fmt::format("SymmetricFactor: unknown DensePolicy value ({})",
                                                static_cast<int>(opts.dense_factorization)));
    }
    switch (opts.dense_singular_policy) {
    case DenseSingularPolicy::kSparseFallback:
        break;
    case DenseSingularPolicy::kReport:
        if (opts.dense_factorization == DensePolicy::kNever) {
            throw std::invalid_argument(
                "SymmetricFactor: DenseSingularPolicy::kReport configures the dense arm and is "
                "refused under DensePolicy::kNever");
        }
        break;
    default:
        throw std::invalid_argument(
            fmt::format("SymmetricFactor: unknown DenseSingularPolicy value ({})",
                        static_cast<int>(opts.dense_singular_policy)));
    }
    if (opts.conditional_reproducibility_threads > 0 &&
        opts.dense_factorization != DensePolicy::kNever) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor: conditional_reproducibility_threads > 0 requires DensePolicy::kNever "
            "-- CNR is a per-session Pardiso guarantee the dense arm's LAPACK cannot keep, got "
            "conditional_reproducibility_threads = {}",
            opts.conditional_reproducibility_threads));
    }
}

bool dense_selected(const SymmetricFactor::Options &opts, Index n) noexcept {
    switch (opts.dense_factorization) {
    case DensePolicy::kAlways:
        return true;
    case DensePolicy::kAuto:
        return opts.max_dense_dimension.has_value() && n <= *opts.max_dense_dimension;
    default:
        return false;
    }
}

} // namespace hven::linear::detail
