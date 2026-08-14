// The Apple Accelerate backend for SymmetricFactor / Factorization. Compiled
// on the platforms whose sparse backend is Accelerate (see
// src/CMakeLists.txt); MKL Pardiso implements the same header in its own TU
// (symmetric_factor_mkl.cpp). The two adapters are intentionally NOT shared:
// MKL's phase-driven session and Accelerate's symbolic/numeric factorization
// objects have nothing in common below the public header, so each backend
// gets its own TU and only the platform's own is ever compiled -- see the
// comment in src/CMakeLists.txt.
//
// Everything Accelerate-shaped lives one layer down, in FactorSession (the
// concrete definition this TU gives to hven/linear/symmetric_factor.h's
// backend-neutral forward declaration -- see
// hven/detail/linear/accelerate_session.h): this TU owns the contract --
// validation, pattern-hash discipline, counters, epochs, the entitlement
// rules around shared sessions -- and none of the backend's vocabulary
// EXCEPT the inertia query: evidence_of() below calls SparseGetInertia
// directly against session.native_factorization() rather than reading a
// value FactorSession cached at factorize() time. That placement is
// deliberate, not a leak -- it is what lets the test-seam convention
// (docs/testing.md) inject SparseGetInertia's failure without adding a hook
// to the MPL-derived accelerate_session.h/.cpp. This mirrors
// symmetric_factor_mkl.cpp's own structure closely; the two files are
// deliberately kept in lock-step shape so a reader who knows one can find
// their way around the other, even though neither #includes the other.
//
// This file compiles and runs against the real Accelerate framework on every
// macOS CI run. The minimal local-stub pass on Linux
// (scripts/check_accelerate_syntax_linux.sh) remains a narrower structural
// check; docs/testing.md states exactly what that pass does and does not prove.

#include "hven/linear/symmetric_factor.h"

#include <cmath>
#include <stdexcept>
#include <utility>

#include <fmt/format.h>

#include "hven/core/pattern_hash.h"
#include "hven/detail/linear/accelerate_session.h"
#include "hven/detail/linear/fault_injection.h"

namespace hven::linear {

namespace {

const char *kind_name(FactorKind kind) {
    switch (kind) {
    case FactorKind::kLDLT:
        return "kLDLT";
    case FactorKind::kLLT:
        return "kLLT";
    case FactorKind::kLU:
        return "kLU";
    }
    return "unknown";
}

// MT-METIS (multi-threaded METIS) is only declared starting in the macOS 26
// SDK. This is an SDK-version macro -- it says the enum constant
// SparseOrderMTMetis EXISTS at compile time, not that the RUNNING host
// implements it -- exactly mirroring the legacy interior-point engine's own has-MT-METIS /
// accelerate_supported_order guard, full predicate included
// (`defined(__APPLE__)` guards the SDK-version macro check itself, which is
// otherwise meaningless off Apple platforms), which this block deliberately
// follows.
#if defined(__APPLE__) && defined(__MAC_OS_X_VERSION_MAX_ALLOWED) &&                               \
    __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
#define HVEN_HAS_MTMETIS 1
#endif

#ifdef HVEN_HAS_MTMETIS
// Downgrades SparseOrderMTMetis to SparseOrderMetis at RUNTIME on a host
// that lacks it (macOS < 26): passing SparseOrderMTMetis unconditionally
// there raises SparseParameterError and a dead solver, per the legacy interior-point engine
// precedent this mirrors. Every other order passes through unchanged.
SparseOrder_t accelerate_supported_order(SparseOrder_t order) {
    if (order != SparseOrderMTMetis) {
        return order;
    }
    if (__builtin_available(macOS 26.0, *)) {
        return order;
    }
    return SparseOrderMetis;
}
#endif

// Maps SymmetricFactor::Options::Ordering onto Accelerate's own SparseOrder_t
// vocabulary -- the Accelerate half of the backend-neutral ordering mapping
// documented in symmetric_factor.h: kBackendDefault -> SparseOrderDefault
// (Apple documents this as AMD for symmetric matrices), kMinimumDegree -> SparseOrderAMD,
// kNestedDissection -> SparseOrderMetis, kParallelNestedDissection ->
// SparseOrderMTMetis with the OS-availability downgrade above. Written as an
// explicit switch, not a bare cast, so a future Ordering value fails loudly
// here rather than silently picking an unintended order method -- the same
// discipline symmetric_factor_mkl.cpp's pardiso_ordering_code uses for its
// half of the same mapping.
SparseOrder_t accelerate_ordering_code(SymmetricFactor::Options::Ordering ordering) {
    switch (ordering) {
    case SymmetricFactor::Options::Ordering::kBackendDefault:
        return SparseOrderDefault;
    case SymmetricFactor::Options::Ordering::kMinimumDegree:
        return SparseOrderAMD;
    case SymmetricFactor::Options::Ordering::kNestedDissection:
        return SparseOrderMetis;
    case SymmetricFactor::Options::Ordering::kParallelNestedDissection:
#ifdef HVEN_HAS_MTMETIS
        return accelerate_supported_order(SparseOrderMTMetis);
#else
        // This SDK does not declare SparseOrderMTMetis at all -- the same
        // state a host older than macOS 26 downgrades to at runtime above,
        // reached here at compile time instead.
        return SparseOrderMetis;
#endif
    }
    throw std::invalid_argument(fmt::format("SymmetricFactor: unknown Options::Ordering value ({})",
                                            static_cast<int>(ordering)));
}

// The inverse of accelerate_ordering_code, for adopt() round-tripping a
// session's stored AccelerateConfig back into an Options value. A session
// downgraded from SparseOrderMTMetis to SparseOrderMetis at analyze() time
// round-trips as kNestedDissection here -- an accurate report, not a lossy
// one: SparseOrderMetis is genuinely the order method that session is
// running.
SymmetricFactor::Options::Ordering accelerate_ordering_of(SparseOrder_t code) {
    if (code == SparseOrderDefault) {
        return SymmetricFactor::Options::Ordering::kBackendDefault;
    }
    if (code == SparseOrderAMD) {
        return SymmetricFactor::Options::Ordering::kMinimumDegree;
    }
    if (code == SparseOrderMetis) {
        return SymmetricFactor::Options::Ordering::kNestedDissection;
    }
#ifdef HVEN_HAS_MTMETIS
    if (code == SparseOrderMTMetis) {
        return SymmetricFactor::Options::Ordering::kParallelNestedDissection;
    }
#endif
    throw std::invalid_argument(
        fmt::format("SymmetricFactor::adopt: unrecognized stored Accelerate ordering code ({})",
                    static_cast<int>(code)));
}

// The thread-count rule, applied by the constructor to Options::num_threads
// and again by the live setter -- one function so the two cannot drift into
// accepting different values. `where` is the entry point the message names,
// which is the only thing that differs between the two callers. The rule is
// validated here even though this backend applies the count to nothing: an
// argument that is invalid on the surface is invalid on every backend, and
// silently accepting a negative one here would make the two backends
// disagree about what the API takes.
void validate_num_threads(int num_threads, const char *where) {
    if (num_threads < 0) {
        throw std::invalid_argument(
            fmt::format("{}: num_threads must be >= 0 (0 means the backend default), got {}", where,
                        num_threads));
    }
}

detail::AccelerateConfig config_from(const SymmetricFactor::Options &opts) {
    detail::AccelerateConfig cfg;
    // Only kLDLT reaches here -- the constructor rejects the kinds whose
    // backend paths have not landed. SparseFactorizationLDLTTPP is what
    // detail::FactorSession::analyze() actually asks Accelerate for.
    cfg.num_threads = opts.num_threads;
    cfg.pivot_perturb_exp = opts.pivot_perturb_exp;
    cfg.max_refinement_iters = opts.max_refinement_iters;
    cfg.ordering = accelerate_ordering_code(opts.ordering);
    // matrix_scaling, pivot_strategy, factorization_algorithm,
    // solve_parallelism, cnr_threads and collect_factor_mflops are all
    // Pardiso-only -- the constructor below throws before construction
    // succeeds on any non-default value, so AccelerateConfig carries no
    // fields for them at all, matching weighted_matching's existing
    // precedent. factor_size_bytes similarly needs no config field at all:
    // it is always collected (see FactorSession::factor_size_bytes()'s own
    // doc comment), with nothing to gate.
    cfg.zero_tolerance_override = opts.accelerate_zero_tolerance;
    return cfg;
}

// Validates the whole input convention in one pass: compressed, square,
// non-empty, upper triangle only, and a structurally present diagonal in
// every row. Identical rule to (and deliberately duplicated from, not shared
// with) symmetric_factor_mkl.cpp's own validate_upper_csr -- see
// src/CMakeLists.txt's comment on why the two backend adapters do not share
// a TU. The rule itself is backend-agnostic (hven's own upper-CSR
// convention, documented at the top of symmetric_factor.h), but Accelerate
// enforces it for the identical reason Pardiso does: fed a matrix that
// violates either, it does not report a clean error, it computes something
// else.
void validate_upper_csr(const SpMatRM &A) {
    if (!A.isCompressed()) {
        throw std::invalid_argument(
            fmt::format("SymmetricFactor::analyze: matrix must be compressed (call "
                        "A.makeCompressed() first) -- got a {}x{} matrix with {} stored entries",
                        A.rows(), A.cols(), A.nonZeros()));
    }
    if (A.rows() != A.cols()) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor::analyze: matrix must be square, got {}x{}", A.rows(), A.cols()));
    }
    if (A.rows() == 0) {
        throw std::invalid_argument(
            "SymmetricFactor::analyze: matrix must be non-empty, got a 0x0 matrix");
    }

    const auto *outer = A.outerIndexPtr();
    const auto *inner = A.innerIndexPtr();
    const Index rows = static_cast<Index>(A.rows());

    for (Index i = 0; i < rows; ++i) {
        bool has_diagonal = false;
        for (Index k = static_cast<Index>(outer[i]); k < static_cast<Index>(outer[i + 1]); ++k) {
            const Index j = static_cast<Index>(inner[k]);
            if (j < i) {
                throw std::invalid_argument(fmt::format(
                    "SymmetricFactor::analyze: matrix stores an entry at ({}, {}), below the "
                    "diagonal -- this surface takes the UPPER TRIANGLE of a symmetric matrix only",
                    i, j));
            }
            if (j == i) {
                has_diagonal = true;
            }
        }
        if (!has_diagonal) {
            throw std::invalid_argument(fmt::format(
                "SymmetricFactor::analyze: row {} has no diagonal entry in its sparsity pattern -- "
                "the backend requires a structural diagonal in every row, even where the value is "
                "zero (insert an explicit zero)",
                i));
        }
    }
}

// Builds the inertia evidence for a session's current factorization.
//
// The Accelerate column of the backend semantics table, in code: unlike
// Pardiso's cheap iparm readback, SparseGetInertia is a genuine,
// independently-fallible API call against the numeric factorization --
// called HERE, in the adapter, rather than cached inside FactorSession at
// factorize() time, precisely so the test-seam convention (docs/testing.md)
// can inject its failure without touching the MPL-derived session file. A
// query failure is reported as kQueryFailed with the counts left at their
// invalid sentinel (-1), NEVER zero-filled -- the frozen contract's
// fabrication fix. perturbed_pivots is unconditionally absent: Accelerate has
// no perturbed-pivot counter, and absence is the honest state -- NOT the SQP
// shim's superseded choice of mapping Accelerate's zero-pivot count into that
// field, which the contract's degradation rule (a reading on this backend may
// be LESS INFORMATIVE than the reference one, never DIFFERENTLY VALUED)
// forbids.
InertiaEvidence evidence_of(const detail::FactorSession &session) {
    InertiaEvidence evidence;
    if (!session.has_numerics()) {
        return evidence; // kUnavailable
    }

    int n_pos = 0;
    int n_zero = 0;
    int n_neg = 0;
    int rc;
#ifdef HVEN_TESTING
    if (detail::testing::InertiaQueryFaultInjector::active) {
        // Side-effect-free query, so overriding its return code is faithful
        // in every scenario -- see hven/detail/linear/fault_injection.h.
        rc = detail::testing::InertiaQueryFaultInjector::injected_rc;
    } else
#endif
    {
        rc = SparseGetInertia(session.native_factorization(), &n_pos, &n_zero, &n_neg);
    }

    if (rc != 0) {
        evidence.state = InertiaEvidence::State::kQueryFailed;
        return evidence; // counts stay at their -1 sentinel; never zero-filled
    }

    evidence.state = InertiaEvidence::State::kObserved;
    evidence.n_pos = n_pos;
    evidence.n_neg = n_neg;
    evidence.n_zero = n_zero;
    evidence.zero_is_derived = false; // native 3-way report, not derived by subtraction
    evidence.perturbed_pivots = std::nullopt;
    return evidence;
}

// Builds the factor-size evidence for a session's current factorization.
// Called only from factorize()'s success branch -- see FactorEvidence's own
// doc comment (symmetric_factor.h) for why this evidence lives on
// FactorizeOutcome rather than being recomputed per solve.
//
// The Accelerate column of the FactorEvidence semantics table, in code:
// factor_size_bytes is UNCONDITIONAL -- Accelerate computes it as part of
// the symbolic factorization regardless of any hven option, so there is no
// gate to check (see FactorSession::factor_size_bytes()'s own doc
// comment); factor_nonzeros/factor_mflops stay std::nullopt
// unconditionally on this backend, which reports a byte size instead,
// never an entry count, and reports no cost estimate at all.
FactorEvidence factor_evidence_of(const detail::FactorSession &session) {
    FactorEvidence evidence;
    evidence.factor_size_bytes = static_cast<Index>(session.factor_size_bytes());
    return evidence;
}

// Shared solve bodies: SymmetricFactor and Factorization differ in who may
// call them, not in what they do. Identical shape to
// symmetric_factor_mkl.cpp's own run_solve pair; only the backend call
// underneath (detail::FactorSession::solve) differs.
SolveInfo run_solve(const detail::FactorSession &session, ConstMatRef RHS, MatRef X,
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
        return SolveInfo{}; // nothing to solve; never reaches the backend
    }

    // Accelerate's DenseVector_Double solve (see FactorSession::solve) needs
    // contiguous column-major buffers and cannot solve in place, so the
    // right-hand side is always copied. The solution buffer is only copied
    // when X is a strided view into a larger matrix. Identical discipline to
    // the MKL adapter.
    Mat rhs_buffer = RHS;
    const bool x_contiguous = X.cols() == 1 || X.outerStride() == X.rows();
    Mat scratch;
    double *target = nullptr;
    if (x_contiguous) {
        target = X.data();
    } else {
        scratch.resize(X.rows(), X.cols());
        target = scratch.data();
    }

    session.solve(rhs_buffer.data(), target, static_cast<Index>(X.cols()));

    if (!x_contiguous) {
        X = scratch;
    }

    // Accelerate reports no iterative-refinement count this session can
    // honestly surface (see AccelerateConfig's doc comment) -- absent,
    // never a fabricated value.
    return SolveInfo{};
}

SolveInfo run_solve(const detail::FactorSession &session, ConstVecRef rhs, VecRef x,
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

    Vec rhs_buffer = rhs;
    session.solve(rhs_buffer.data(), x.data(), 1);

    return SolveInfo{};
}

// Maps the frozen SolvePhase enum onto the SubfactorPhase FactorSession's
// solve_partial expects. Written as an explicit switch (not a bare
// static_cast) even though the two enums' declared orders already agree, so
// a future reordering of either one fails to compile here instead of
// silently swapping which subfactor a phase solves against.
detail::SubfactorPhase accelerate_phase_of(SymmetricFactor::SolvePhase phase) {
    switch (phase) {
    case SymmetricFactor::SolvePhase::kForward:
        return detail::SubfactorPhase::kForward;
    case SymmetricFactor::SolvePhase::kDiagonal:
        return detail::SubfactorPhase::kDiagonal;
    case SymmetricFactor::SolvePhase::kBackward:
        return detail::SubfactorPhase::kBackward;
    }
    throw std::invalid_argument(fmt::format(
        "SymmetricFactor::solve_partial: unknown solve phase ({})", static_cast<int>(phase)));
}

} // namespace

// =============================================================================
// SymmetricFactor
// =============================================================================

SymmetricFactor::SymmetricFactor(Options opts) : opts_(opts) {
    if (opts_.kind != FactorKind::kLDLT) {
        throw std::logic_error(
            fmt::format("SymmetricFactor: FactorKind::{} is not yet implemented -- only kLDLT has "
                        "a backend path today",
                        kind_name(opts_.kind)));
    }
    validate_num_threads(opts_.num_threads, "SymmetricFactor");
    // A present value is validated; std::nullopt is the don't-write state
    // (on this backend: Apple's own documented defaults -- see the two
    // options' doc comments) and needs no range check. Identical rule and
    // message to the MKL adapter's, because the argument's validity is a
    // property of the surface, not of the backend behind it.
    if (opts_.pivot_perturb_exp.has_value() && *opts_.pivot_perturb_exp < 0) {
        throw std::invalid_argument(
            fmt::format("SymmetricFactor: pivot_perturb_exp must be >= 0 (or std::nullopt to "
                        "leave the backend's own default in force), got {}",
                        *opts_.pivot_perturb_exp));
    }
    if (opts_.max_refinement_iters.has_value() && *opts_.max_refinement_iters < 0) {
        throw std::invalid_argument(
            fmt::format("SymmetricFactor: max_refinement_iters must be >= 0 (or std::nullopt to "
                        "leave the backend's own default in force), got {}",
                        *opts_.max_refinement_iters));
    }
    if (opts_.cnr_threads < 0) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor: cnr_threads must be >= 0 (0 means CNR mode is off), got {}",
            opts_.cnr_threads));
    }

    // weighted_matching is a Pardiso-only concept (it configures iparm[12],
    // which has no Accelerate analogue -- Apple's own matching precedent,
    // where it exists at all, is not exposed through this surface).
    // Accelerate never silently ignores it: a non-default value throws
    // here, at construction, naming the option and the backend, rather than
    // being dropped on the floor.
    //
    // ordering, unlike weighted_matching, is NOT Pardiso-only: every
    // Options::Ordering value maps onto an Accelerate order method (see
    // accelerate_ordering_code() above and the backend-neutral mapping in
    // symmetric_factor.h's own doc comment), so it is accepted unconditionally
    // here -- the mapping is resolved once, in config_from(), at the point
    // analyze() actually needs it.
    static constexpr const char *kBackendName = "Accelerate";
    if (opts_.weighted_matching) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor: Options::weighted_matching is a Pardiso-only option and has no {} "
            "equivalent -- requires false on this backend, got true",
            kBackendName));
    }

    // matrix_scaling: same judgment as weighted_matching, and for the same
    // reason (see Options::matrix_scaling's own doc comment) -- iparm[10]'s
    // MPS scaling is computed FROM the weighted-matching permutation, so a
    // backend with no matching has no scaling to pair it with either.
    if (opts_.matrix_scaling) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor: Options::matrix_scaling is a Pardiso-only option and has no {} "
            "equivalent -- requires false on this backend, got true",
            kBackendName));
    }
    // pivot_strategy: Pardiso-only -- Accelerate's LDLTTPP factorization has
    // a fixed pivoting scheme with no per-call selector.
    if (opts_.pivot_strategy != Options::PivotStrategy::kBackendDefault) {
        throw std::invalid_argument(
            fmt::format("SymmetricFactor: Options::pivot_strategy is a Pardiso-only option and has "
                        "no {} equivalent -- requires kBackendDefault on this backend",
                        kBackendName));
    }
    // factorization_algorithm: Pardiso-only -- Accelerate has no two-level
    // factorization concept.
    if (opts_.factorization_algorithm != Options::FactorizationAlgorithm::kBackendDefault) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor: Options::factorization_algorithm is a Pardiso-only option and has no "
            "{} equivalent -- requires kBackendDefault on this backend",
            kBackendName));
    }
    // solve_parallelism: Pardiso-only -- Accelerate has no per-instance
    // thread control for this to parallelize a solve with.
    if (opts_.solve_parallelism != Options::SolveParallelism::kBackendDefault) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor: Options::solve_parallelism is a Pardiso-only option and has no {} "
            "equivalent -- requires kBackendDefault on this backend",
            kBackendName));
    }
    // cnr_threads: Pardiso-only -- Accelerate has no CNR reproducibility
    // concept, and this is a guarantee a silent no-op would misrepresent
    // (see the option's own doc comment).
    if (opts_.cnr_threads > 0) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor: Options::cnr_threads is a Pardiso-only option and has no {} "
            "equivalent -- requires 0 on this backend, got {}",
            kBackendName, opts_.cnr_threads));
    }
    // collect_factor_mflops: Pardiso-only -- Accelerate reports no
    // factorization-cost estimate under any configuration (see
    // FactorEvidence::factor_mflops's own doc comment), so `true` would
    // otherwise be silently ignored -- the same situation the other
    // Pardiso-only throws above exist to prevent. factor_nonzeros'
    // Accelerate counterpart, factor_size_bytes, needs no such throw: it is
    // always collected on this backend, with no option to silently ignore.
    if (opts_.collect_factor_mflops) {
        throw std::invalid_argument(
            fmt::format("SymmetricFactor: Options::collect_factor_mflops is a Pardiso-only option "
                        "and has no {} "
                        "equivalent -- requires false on this backend, got true",
                        kBackendName));
    }

    // accelerate_zero_tolerance is this backend's own knob (checked for
    // validity here, symmetrically with how MKL's own throw for it lives in
    // its own adapter file's constructor).
    if (opts_.accelerate_zero_tolerance.has_value()) {
        const double value = *opts_.accelerate_zero_tolerance;
        if (!(value > 0.0) || !std::isfinite(value)) {
            throw std::invalid_argument(fmt::format(
                "SymmetricFactor: accelerate_zero_tolerance must be finite and > 0, got {}",
                value));
        }
    }
}

SymmetricFactor::~SymmetricFactor() = default;
SymmetricFactor::SymmetricFactor(SymmetricFactor &&) noexcept = default;
SymmetricFactor &SymmetricFactor::operator=(SymmetricFactor &&) noexcept = default;

void SymmetricFactor::set_num_threads(int num_threads) {
    // Validated before anything moves, so a rejected count leaves this engine
    // exactly as it was -- the same rule and the same message the MKL adapter
    // applies, because the argument's validity is a property of the surface,
    // not of the backend behind it.
    validate_num_threads(num_threads, "SymmetricFactor::set_num_threads");

    // BEST-EFFORT-ABSENT, per Options::num_threads' own contract: this
    // backend has no per-instance thread control, so the count is stored and
    // applied to nothing. Storing it in BOTH places is what keeps the stored
    // value honest -- `opts_` is what a later analyze() builds a session
    // from, the session's copy is what adopt() round-trips back into Options
    // -- and the two must not disagree about a value a caller has set.
    opts_.num_threads = num_threads;
    if (session_) {
        session_->set_num_threads(num_threads);
    }
}

void SymmetricFactor::analyze(const SpMatRM &A) {
    validate_upper_csr(A);

    // A fresh session per analysis -- identical rationale to the MKL
    // adapter's analyze(): a previously shared factorization is never
    // disturbed by a re-analysis here, and a failed analysis leaves this
    // engine exactly as it was.
    auto session = std::make_shared<detail::FactorSession>(config_from(opts_), epoch());
#ifdef HVEN_TESTING
    // See AnalyzeFaultInjector's own doc comment (fault_injection.h) for why
    // the symbolic phase needs an injector at all. Raised INSTEAD of the
    // backend call and before `session_` is replaced below, so this engine is
    // left exactly as it was -- the same guarantee a real symbolic failure
    // carries.
    if (detail::testing::AnalyzeFaultInjector::active) {
        const int code = detail::testing::AnalyzeFaultInjector::injected_backend_code;
        throw std::runtime_error(fmt::format(
            "SymmetricFactor::analyze: symbolic analysis failed, backend error {}", code));
    }
#endif
    session->analyze(A);

    session_ = std::move(session);
    pattern_hash_ = hven::pattern_hash(A);
    has_pattern_ = true;
    numerics_refused_ = false;
    refused_epoch_ = 0;
    ++counters_.analyze_count;
}

FactorizeOutcome SymmetricFactor::factorize(const SpMatRM &A) {
    if (!has_pattern_ || !session_) {
        throw std::runtime_error("SymmetricFactor::factorize: called before analyze() (or before "
                                 "adopting a factorization that carries a symbolic analysis)");
    }
    if (!A.isCompressed()) {
        throw std::invalid_argument(
            fmt::format("SymmetricFactor::factorize: matrix must be compressed (call "
                        "A.makeCompressed() first) -- got a {}x{} matrix with {} stored entries",
                        A.rows(), A.cols(), A.nonZeros()));
    }

    const std::uint64_t hash = hven::pattern_hash(A);
    if (hash != pattern_hash_) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor::factorize: the matrix's sparsity pattern (hash {:#018x}) is not the "
            "analyzed one (hash {:#018x}) -- factorize() never re-analyzes; call analyze() for a "
            "new pattern",
            hash, pattern_hash_));
    }

    int backend_code;
#ifdef HVEN_TESTING
    if (detail::testing::FactorizeFaultInjector::active) {
        // See hven/detail/linear/fault_injection.h for the exact scope this is
        // faithful within -- identical to the MKL twin's: the real backend
        // call is SKIPPED rather than its result overridden, so the session's
        // own state is left exactly as it was, which is only a faithful
        // scenario on a session that has never factorized successfully.
        backend_code = detail::testing::FactorizeFaultInjector::injected_backend_code;
    } else
#endif
    {
        backend_code = session_->factorize(A);
    }
    ++counters_.factorize_count;

    FactorizeOutcome outcome;
    outcome.backend_code = backend_code;
    if (backend_code == 0) {
        outcome.status = FactorizeOutcome::Status::kOk;
        outcome.inertia = evidence_of(*session_);
        outcome.factor = factor_evidence_of(*session_);
        // This engine has now produced numerics of its own, which retires
        // any staleness inherited from an adopted handle.
        numerics_refused_ = false;
        refused_epoch_ = 0;
    } else {
        outcome.status = FactorizeOutcome::Status::kBackendError;
        // The inertia stays kUnavailable: there is no factorization to
        // describe. The session has already invalidated its numerics
        // (FactorSession::factorize calls release_numeric() up front) and
        // left the epoch where it was, so solves through this engine and
        // through any shared handle throw until a factorization succeeds.
        //
        // UNLIKE the MKL adapter's identical branch, this one is not known
        // to be unreachable by ordinary fixtures: Accelerate is documented
        // (and was measured on real hardware by the SQP engine's
        // 2026-07-29 Accelerate audit) to genuinely refuse a
        // numeric factorization on some singular/indefinite input rather
        // than perturbing through it. Native macOS CI now runs this backend,
        // but no contract test pins a particular fixture to this nonzero-status
        // branch. See docs/testing.md.
    }
    return outcome;
}

SolveInfo SymmetricFactor::solve(ConstMatRef RHS, MatRef X) const {
    require_solvable("solve");
    const SolveInfo info = run_solve(*session_, RHS, X, "solve");
    if (X.cols() > 0) {
        ++counters_.solve_count;
    }
    return info;
}

SolveInfo SymmetricFactor::solve_single(ConstVecRef rhs, VecRef x) const {
    require_solvable("solve");
    const SolveInfo info = run_solve(*session_, rhs, x, "solve");
    ++counters_.solve_count;
    return info;
}

SolveInfo SymmetricFactor::solve_partial(SolvePhase phase, ConstVecRef rhs, VecRef x) const {
    require_solvable("solve_partial");

    const detail::SubfactorPhase backend_phase = accelerate_phase_of(phase);

    if (rhs.size() != session_->dim()) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor::solve_partial: right-hand side has {} entries, the factorization's "
            "dimension is {}",
            rhs.size(), session_->dim()));
    }
    if (x.size() != rhs.size()) {
        throw std::invalid_argument(
            fmt::format("SymmetricFactor::solve_partial: solution has {} entries but the "
                        "right-hand side has {} -- sizes must match",
                        x.size(), rhs.size()));
    }

    Vec rhs_buffer = rhs;
    session_->solve_partial(backend_phase, rhs_buffer.data(), x.data());
    ++counters_.partial_solve_count;

    return SolveInfo{}; // no refinement count to report on this backend
}

// Unconditionally false on Accelerate: perturbation evidence is entirely
// absent on this backend (evidence_of() never populates perturbed_pivots),
// so composability of the partial-solve stages is unverifiable by
// construction, not merely uninvestigated. This is the frozen contract's
// conservative rung -- never a fabricated true, and NOT gated on
// has_usable_numerics() the way the MKL twin's predicate is, because there
// is no numerics-dependent case where this backend could honestly answer
// true.
bool SymmetricFactor::supports_partial_solve() const { return false; }

InertiaEvidence SymmetricFactor::inertia() const {
    if (!has_usable_numerics()) {
        return InertiaEvidence{};
    }
    return evidence_of(*session_);
}

const SymmetricFactor::Counters &SymmetricFactor::counters() const { return counters_; }

std::shared_ptr<const Factorization> SymmetricFactor::share() {
    require_solvable("share");
    return std::make_shared<const Factorization>(Factorization::PrivateTag{}, session_,
                                                 pattern_hash_, session_->epoch());
}

std::uint64_t SymmetricFactor::epoch() const { return session_ ? session_->epoch() : 0; }

std::uint64_t SymmetricFactor::session_id() const { return session_ ? session_->session_id() : 0; }

SymmetricFactor SymmetricFactor::adopt(std::shared_ptr<const Factorization> handle) {
    if (!handle) {
        throw std::invalid_argument("SymmetricFactor::adopt: handle must not be null");
    }

    const std::shared_ptr<detail::FactorSession> &session = handle->session_;
    const detail::AccelerateConfig &cfg = session->config();

    // The adopting engine drives the emitter's session, so it inherits the
    // configuration that session was built with rather than imposing a new
    // one on a factorization that has already been computed.
    Options opts;
    opts.kind = FactorKind::kLDLT;
    opts.num_threads = cfg.num_threads;
    opts.pivot_perturb_exp = cfg.pivot_perturb_exp;
    opts.max_refinement_iters = cfg.max_refinement_iters;
    opts.ordering = accelerate_ordering_of(cfg.ordering);
    opts.accelerate_zero_tolerance = cfg.zero_tolerance_override;

    SymmetricFactor adopted(opts);
    adopted.session_ = session;
    adopted.pattern_hash_ = handle->pattern_hash_;
    adopted.has_pattern_ = true;

    // The identity triple is validated before any reuse: the session and the
    // epoch here, the pattern hash at the first factorize(). An epoch
    // mismatch means a co-owner has refactorized since this handle was
    // emitted, so the symbolic is still reusable but the numerics are not
    // this handle's any more -- refuse them rather than solving against
    // numbers the adopter never agreed to.
    //
    // The session conjunct is written out although it cannot fail from here:
    // `session` IS `handle->session_`, so the two ids are the same id today.
    // It is the identity rule, not a redundancy -- the rule is "same session
    // AND same epoch", and stating it here is what makes this the one place
    // to look if a handle ever comes to name a session it does not co-own.
    if (handle->session_id() != session->session_id() || handle->epoch() != session->epoch()) {
        adopted.numerics_refused_ = true;
        adopted.refused_epoch_ = handle->epoch();
    }

    return adopted;
}

bool SymmetricFactor::has_usable_numerics() const {
    return session_ && session_->has_numerics() && !numerics_refused_;
}

void SymmetricFactor::require_solvable(const char *what) const {
    if (!session_ || !session_->has_numerics()) {
        throw std::runtime_error(
            fmt::format("SymmetricFactor::{}: called before a successful factorize()", what));
    }
    if (numerics_refused_) {
        throw std::runtime_error(fmt::format(
            "SymmetricFactor::{}: this engine adopted a factorization emitted at epoch {}, but the "
            "shared session has since been refactorized (it is at epoch {}) -- those numerics are "
            "stale and numeric reuse is refused; call factorize() to produce this engine's own",
            what, refused_epoch_, session_->epoch()));
    }
}

// =============================================================================
// Factorization
// =============================================================================

Factorization::Factorization(PrivateTag, std::shared_ptr<detail::FactorSession> session,
                             std::uint64_t pattern_hash, std::uint64_t epoch)
    : session_(std::move(session)), pattern_hash_(pattern_hash), epoch_(epoch) {}

Factorization::~Factorization() = default;

SolveInfo Factorization::solve(ConstMatRef RHS, MatRef X) const {
    if (!session_->has_numerics()) {
        throw std::runtime_error("Factorization::solve: the shared session no longer holds a "
                                 "usable factorization (a co-owner's factorize() failed)");
    }
    return run_solve(*session_, RHS, X, "solve");
}

SolveInfo Factorization::solve_single(ConstVecRef rhs, VecRef x) const {
    if (!session_->has_numerics()) {
        throw std::runtime_error("Factorization::solve: the shared session no longer holds a "
                                 "usable factorization (a co-owner's factorize() failed)");
    }
    return run_solve(*session_, rhs, x, "solve");
}

InertiaEvidence Factorization::inertia() const {
    if (!session_->has_numerics()) {
        return InertiaEvidence{};
    }
    return evidence_of(*session_);
}

std::uint64_t Factorization::pattern_hash() const { return pattern_hash_; }

// Read from the co-owned session rather than stored at emission: the handle
// never rebinds and a session's id never changes, so there is nothing here
// that could go out of step with the session, and no second copy to keep in
// step. Contrast epoch_, which is a SNAPSHOT of a value that does move.
std::uint64_t Factorization::session_id() const { return session_->session_id(); }

std::uint64_t Factorization::epoch() const { return epoch_; }

} // namespace hven::linear
