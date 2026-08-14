// =============================================================================
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// Derived from the AccelerateSupport module of the Eigen linear algebra
// library (MPL-2.0) -- see the notice at the top of the matching header for
// exactly what is derived and what is not, and for the two Mac-verified
// downstream ports this file's call shapes follow. MPL-2.0 applies to this
// file only; the remainder of hven is Apache-2.0. See notices/eigen-mpl2.txt.
// =============================================================================

#include "hven/detail/linear/accelerate_session.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <fmt/format.h>

namespace hven::linear::detail {

// Eigen's default int StorageIndex must match Accelerate's int rowIndices /
// ordering arrays, which this class feeds directly without conversion --
// same assertion as the MKL session makes for MKL_INT.
static_assert(std::is_same_v<SpMatRM::StorageIndex, int>,
              "hven's Accelerate session requires SpMatRM::StorageIndex == int: Accelerate's "
              "sparse structure and ordering fields are int*.");

namespace {

// Capture target for Accelerate's reportError callback, exactly the
// KktSystem precedent's pattern (the SQP engine's
// kkt_system_accelerate.h): a NULL callback makes Accelerate log via os_log
// and __builtin_trap() on parameter errors, which library code must never do
// (hven's T5/T6 rules -- CLAUDE.md Sec. 4), and printing from the callback
// would be a diagnostic outside the thrown exception's message. The callback
// only records the message; the calling method folds it into whatever it
// throws. Thread-local for the same documented-residual-risk reason as the
// precedent: the callback carries no user context pointer.
thread_local std::string g_last_error;

void capture_error(const char *message) {
    std::string text = message != nullptr ? message : "(no message)";
    while (!text.empty() && (text.back() == '\n' || text.back() == ' ')) {
        text.pop_back();
    }
    g_last_error = std::move(text);
}

} // namespace

FactorSession::FactorSession(const AccelerateConfig &cfg, std::uint64_t initial_epoch)
    : cfg_(cfg), epoch_(initial_epoch) {}

FactorSession::~FactorSession() { release(); }

void FactorSession::release_numeric() noexcept {
    if (have_numeric_) {
        SparseCleanup(numeric_);
        have_numeric_ = false;
    }
}

void FactorSession::release() noexcept {
    release_numeric();
    if (have_symbolic_) {
        SparseCleanup(symbolic_);
        have_symbolic_ = false;
    }
}

SparseMatrixStructure FactorSession::structure() const {
    // A's upper-triangle CSR, read column-wise, is exactly the lower
    // triangle of A^T = A (A is symmetric): row starts become column starts,
    // column indices within a row become row indices within a column, and no
    // transpose attribute or value reordering is needed. Identical
    // correspondence to the one KktSystem::structure() uses.
    SparseMatrixStructure s{};
    s.rowCount = n_;
    s.columnCount = n_;
    s.columnStarts = const_cast<long *>(col_starts_.data());
    s.rowIndices = const_cast<int *>(matrix_.innerIndexPtr());
    s.attributes.kind = SparseSymmetric;
    s.attributes.triangle = SparseLowerTriangle;
    s.blockSize = 1;
    return s;
}

void FactorSession::analyze(const SpMatRM &A) {
    release();

    if (A.rows() > std::numeric_limits<int>::max()) {
        throw std::invalid_argument(
            fmt::format("SymmetricFactor::analyze: matrix is {}x{} but Accelerate's sparse "
                        "structure uses int dimensions",
                        A.rows(), A.cols()));
    }

    n_ = static_cast<int>(A.rows());
    matrix_ = A;
    const auto *outer = matrix_.outerIndexPtr();
    col_starts_.assign(outer, outer + n_ + 1);

    // Explicit LDLTTPP, not the SparseFactorizationLDLT alias: SparseGetInertia
    // is documented supported for LDLTTPP-typed factorizations specifically,
    // and the alias records a different type tag even though it is currently
    // implemented as TPP (both precedents make this same choice explicitly).
    SparseSymbolicFactorOptions fopts{};
    fopts.control = SparseDefaultControl;
    // Already resolved to Accelerate's own vocabulary (including the
    // kParallelNestedDissection -> SparseOrderMTMetis OS-availability
    // downgrade) by the adapter's accelerate_ordering_code() -- see
    // AccelerateConfig::ordering's own doc comment. Defaults to
    // SparseOrderDefault, the prior hardcoded value, when Options::ordering
    // is kBackendDefault.
    fopts.orderMethod = cfg_.ordering;
    // `order = nullptr`: a deliberate, noted divergence from
    // KktSystem::analyze, which passes a non-null buffer here to receive
    // Accelerate's computed fill-reducing permutation back (its own audit
    // instrument for inspecting the ordering -- see
    // kkt_system_accelerate.h's perm_ field and its §(c) comment). hven's
    // FactorSession has no consumer for that permutation, and Eigen's
    // AccelerateSupport module (the upstream both ports derive from) also
    // passes null here by default -- the write-back is documented as
    // optional. Flagged rather than silently omitted: the review record's
    // Accelerate-API precedent table lists this as a stated difference from
    // KktSystem::analyze, not an unstated one.
    fopts.order = nullptr;
    fopts.ignoreRowsAndColumns = nullptr;
    fopts.malloc = std::malloc;
    fopts.free = std::free;
    fopts.reportError = &capture_error;

    g_last_error.clear();
    symbolic_ = SparseFactor(SparseFactorizationLDLTTPP, structure(), fopts);
    // Cleanup is required even for a FAILED factorization, so the flag is set
    // before the status check.
    have_symbolic_ = true;
    if (symbolic_.status != SparseStatusOK) {
        const auto status = static_cast<int>(symbolic_.status);
        const std::string message = g_last_error;
        release();
        throw std::runtime_error(fmt::format(
            "SymmetricFactor::analyze: Accelerate symbolic factorization failed, status {}{}",
            status, message.empty() ? "" : fmt::format(" ({})", message)));
    }
}

int FactorSession::factorize(const SpMatRM &A) {
    if (!have_symbolic_) {
        throw std::runtime_error(
            "SymmetricFactor::factorize: no symbolic analysis is available in this session");
    }
    if (static_cast<int>(A.rows()) != n_ || A.nonZeros() != matrix_.nonZeros()) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor::factorize: matrix is {}x{} with {} stored entries but the analyzed "
            "structure is {}x{} with {} stored entries",
            A.rows(), A.cols(), A.nonZeros(), matrix_.rows(), matrix_.cols(), matrix_.nonZeros()));
    }

    std::copy(A.valuePtr(), A.valuePtr() + A.nonZeros(), matrix_.valuePtr());

    // A factorization attempt invalidates the previous numerics up front,
    // same invariant the MKL session guarantees -- see the identical note
    // there. release_numeric() also handles the required SparseCleanup of
    // whatever numeric factorization this session previously held.
    release_numeric();

    // Apple's own documented double defaults (SolveImplementation.h, restated
    // by the KktSystem precedent): default scaling for LDLT is inf-norm
    // equilibration; pivotTolerance 0.01 is "the recommended value for
    // difficult matrices in double" and is left at that default -- there is
    // no hven Options field asking for a different one. zeroTolerance is
    // driven by pivot_perturb_exp via zeroTolerance = 10^-k * eps -- the same
    // functional SHAPE Apple's own default already uses (1e-4 * eps is
    // exactly this formula at k = 4).
    //
    // When this implementation was written, the only real-hardware precedent
    // was k = 4 (~= 2.2e-20; kkt_system_accelerate.h:393, hardcoded, part of
    // the audited KktSystem precedent). hven's own k = 8 default makes
    // zeroTolerance ~= 2.2e-24 -- FOUR ORDERS OF MAGNITUDE TIGHTER -- and
    // therefore diverges from that precedent. "Tighter" is NOT
    // straightforwardly "safer": a smaller zeroTolerance means a
    // tiny-but-nonzero pivot is more likely to be USED by threshold partial
    // pivoting rather than caught by the zero check, which on a
    // near-singular fixture could degrade the factorization rather than
    // improve it -- "directionally conservative" is true only for evidence
    // honesty (less eager to declare a zero pivot that isn't observed), not
    // obviously conservative numerically. This is a documented engineering
    // choice, not a claimed equivalence to Pardiso's perturbation semantics.
    // The shipped k=8 default now runs green on real Apple Silicon in native
    // macOS CI; what remains UNOBSERVED is the Mac A/B of k=8 against k=4 on
    // the golden-rig fixtures, not merely "confirm no surprising effect."
    SparseNumericFactorOptions nopts{};
    nopts.control = SparseDefaultControl;
    nopts.scalingMethod = SparseScalingDefault;
    nopts.scaling = nullptr;
    nopts.pivotTolerance = 0.01;
    // zero_tolerance_override (Options::accelerate_zero_tolerance) bypasses
    // the pivot_perturb_exp-derived formula entirely when present -- an
    // explicit absolute threshold, not a further exponent to feed the
    // formula. Otherwise a present pivot_perturb_exp feeds the formula,
    // unchanged from before either optional existed. When BOTH are nullopt
    // (pivot_perturb_exp's don't-write state), the value passed is Apple's
    // OWN documented default, 1e-4 * eps -- the formula at k = 4, written
    // as the explicit product of the documented constants rather than
    // through std::pow so what reaches Accelerate is exactly the
    // documented value. "Don't write" has no literal referent on a backend
    // configured by struct (SOME value is always passed), so it means the
    // backend's documented default: a value citable from Apple's own
    // documentation, not an observation fabricated here.
    if (cfg_.zero_tolerance_override.has_value()) {
        nopts.zeroTolerance = *cfg_.zero_tolerance_override;
    } else if (cfg_.pivot_perturb_exp.has_value()) {
        nopts.zeroTolerance = std::pow(10.0, -static_cast<double>(*cfg_.pivot_perturb_exp)) *
                              std::numeric_limits<double>::epsilon();
    } else {
        nopts.zeroTolerance = 1e-4 * std::numeric_limits<double>::epsilon();
    }

    SparseMatrix_Double amat{};
    amat.structure = structure();
    amat.data = const_cast<double *>(matrix_.valuePtr());

    g_last_error.clear();
    numeric_ = SparseFactor(symbolic_, amat, nopts);
    have_numeric_ = true; // cleanup required even on failure
    if (numeric_.status != SparseStatusOK) {
        const auto status = static_cast<int>(numeric_.status);
        release_numeric();
        // Message is discarded here (unlike analyze()'s throw): factorize()
        // RETURNS a status code for the caller to fold into FactorizeOutcome,
        // matching the MKL session's contract of returning rather than
        // throwing on a numeric failure. g_last_error is cleared above and
        // read fresh by whichever call captures it next.
        return status;
    }

    ++epoch_;
    return 0;
}

void FactorSession::solve(const double *b, double *x, Index nrhs) const {
    if (!have_numeric_) {
        throw std::runtime_error("SymmetricFactor::solve: called before a successful factorize()");
    }
    // One column at a time via the DenseVector_Double overload -- the exact
    // call shape KktSystem::solve uses on real hardware. Deliberately not
    // batched through a DenseMatrix_Double call (see the header comment and
    // the review record): this keeps every Accelerate call this session
    // makes traceable to a call shape that has actually run on Apple
    // hardware, at the cost of nrhs separate SparseSolve calls instead of
    // one.
    for (Index col = 0; col < nrhs; ++col) {
        DenseVector_Double bv{n_, const_cast<double *>(b + col * n_)};
        DenseVector_Double xv{n_, x + col * n_};
        g_last_error.clear();
        SparseSolve(numeric_, bv, xv);
        if (!g_last_error.empty()) {
            throw std::runtime_error(
                fmt::format("SymmetricFactor::solve: Accelerate reported: {}", g_last_error));
        }
    }
}

void FactorSession::solve_partial(SubfactorPhase phase, const double *b, double *x) const {
    if (!have_numeric_) {
        throw std::runtime_error(
            "SymmetricFactor::solve_partial: called before a successful factorize()");
    }

    const SparseSubfactor_t which =
        (phase == SubfactorPhase::kDiagonal) ? SparseSubfactorD : SparseSubfactorL;
    const bool transpose = (phase == SubfactorPhase::kBackward);

    // RAII so the subfactor is cleaned up on the throw path too, exactly the
    // KktSystem precedent's SubfactorGuard.
    struct SubfactorGuard {
        SparseOpaqueSubfactor_Double sub;
        ~SubfactorGuard() { SparseCleanup(sub); }
    };

    g_last_error.clear();
    SubfactorGuard guard{SparseCreateSubfactor(which, numeric_)};
    if (!g_last_error.empty()) {
        throw std::runtime_error(fmt::format(
            "SymmetricFactor::solve_partial: SparseCreateSubfactor reported: {}", g_last_error));
    }
    guard.sub.attributes.transpose = transpose;

    // In-place subfactor solve: copy b into x first, exactly like the
    // MKL twin's caller (the adapter) already hands this a scratch buffer
    // that is not the caller's own rhs storage.
    std::copy(b, b + n_, x);
    DenseVector_Double xv{n_, x};
    g_last_error.clear();
    SparseSolve(guard.sub, xv);
    if (!g_last_error.empty()) {
        throw std::runtime_error(
            fmt::format("SymmetricFactor::solve_partial: Accelerate reported: {}", g_last_error));
    }
}

} // namespace hven::linear::detail
