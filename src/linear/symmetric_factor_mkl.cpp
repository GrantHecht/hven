// The MKL Pardiso backend for SymmetricFactor / Factorization. Compiled on
// the platforms whose sparse backend is MKL (see src/CMakeLists.txt); the
// Apple Accelerate backend implements the same header in its own TU.
//
// Everything Pardiso-shaped lives one layer down, in FactorSession (the
// concrete definition this TU gives to hven/linear/symmetric_factor.h's
// backend-neutral forward declaration -- see
// hven/detail/linear/pardiso_session.h): this TU owns the contract --
// validation, pattern-hash discipline, counters, epochs, the entitlement
// rules around shared sessions -- and none of the backend's vocabulary.

#include "hven/linear/symmetric_factor.h"

#include <optional>
#include <stdexcept>
#include <utility>

#include <fmt/format.h>

#include "hven/core/pattern_hash.h"
#include "hven/detail/linear/fault_injection.h"
#include "hven/detail/linear/pardiso_session.h"

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

// Maps SymmetricFactor::Options::Ordering onto the raw iparm[1] override
// PardisoConfig carries (std::nullopt = don't write, matching
// Ordering::kBackendDefault exactly). Written as an explicit switch, not a
// bare cast, so a future Ordering value fails loudly here rather than
// writing an unintended iparm entry.
//
// pardisoinit's own iparm[1] default has CHANGED across MKL versions
// (2 on 2025.3; 3 on 2026.0/2026.1), so kBackendDefault floats with the
// linked MKL, and kParallelNestedDissection coincides with the current
// default — a behavioral no-op on this MKL. Naming a non-default value
// is therefore how a caller PINS the ordering against version drift;
// that is the option's purpose, not an oversight.
std::optional<int> pardiso_ordering_code(SymmetricFactor::Options::Ordering ordering) {
    switch (ordering) {
    case SymmetricFactor::Options::Ordering::kBackendDefault:
        return std::nullopt;
    case SymmetricFactor::Options::Ordering::kMinimumDegree:
        return 0;
    case SymmetricFactor::Options::Ordering::kNestedDissection:
        return 2;
    case SymmetricFactor::Options::Ordering::kParallelNestedDissection:
        return 3;
    }
    throw std::invalid_argument(fmt::format("SymmetricFactor: unknown Options::Ordering value ({})",
                                            static_cast<int>(ordering)));
}

// The inverse of pardiso_ordering_code, for adopt() round-tripping a
// session's stored PardisoConfig back into an Options value.
SymmetricFactor::Options::Ordering pardiso_ordering_of(std::optional<int> code) {
    if (!code.has_value()) {
        return SymmetricFactor::Options::Ordering::kBackendDefault;
    }
    switch (*code) {
    case 0:
        return SymmetricFactor::Options::Ordering::kMinimumDegree;
    case 2:
        return SymmetricFactor::Options::Ordering::kNestedDissection;
    case 3:
        return SymmetricFactor::Options::Ordering::kParallelNestedDissection;
    default:
        throw std::invalid_argument(
            fmt::format("SymmetricFactor::adopt: unrecognized stored ordering code ({})", *code));
    }
}

// Maps SymmetricFactor::Options::PivotStrategy onto the raw iparm[20]
// override PardisoConfig carries (std::nullopt = don't write, matching
// kBackendDefault exactly). Explicit switch, not a bare cast, for the same
// reason pardiso_ordering_code is one. The four non-default codes are
// EXACTLY Intel's own documented iparm[20] values -- see PivotStrategy's
// own doc comment in symmetric_factor.h for the citation.
std::optional<int> pardiso_pivot_strategy_code(SymmetricFactor::Options::PivotStrategy strategy) {
    using PivotStrategy = SymmetricFactor::Options::PivotStrategy;
    switch (strategy) {
    case PivotStrategy::kBackendDefault:
        return std::nullopt;
    case PivotStrategy::kOneByOne:
        return 0;
    case PivotStrategy::kTwoByTwo:
        return 1;
    case PivotStrategy::kOneByOneNoAutoRefine:
        return 2;
    case PivotStrategy::kTwoByTwoNoAutoRefine:
        return 3;
    }
    throw std::invalid_argument(fmt::format(
        "SymmetricFactor: unknown Options::PivotStrategy value ({})", static_cast<int>(strategy)));
}

// The inverse of pardiso_pivot_strategy_code, for adopt() round-tripping.
SymmetricFactor::Options::PivotStrategy pardiso_pivot_strategy_of(std::optional<int> code) {
    using PivotStrategy = SymmetricFactor::Options::PivotStrategy;
    if (!code.has_value()) {
        return PivotStrategy::kBackendDefault;
    }
    switch (*code) {
    case 0:
        return PivotStrategy::kOneByOne;
    case 1:
        return PivotStrategy::kTwoByTwo;
    case 2:
        return PivotStrategy::kOneByOneNoAutoRefine;
    case 3:
        return PivotStrategy::kTwoByTwoNoAutoRefine;
    default:
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor::adopt: unrecognized stored pivot strategy code ({})", *code));
    }
}

// Maps SymmetricFactor::Options::SolveParallelism onto the raw iparm[24]
// override PardisoConfig carries (std::nullopt = don't write, matching
// kBackendDefault exactly). The three non-default codes are EXACTLY
// Intel's own documented iparm[24] values -- see SolveParallelism's own
// doc comment in symmetric_factor.h for the citation and the naming
// history (an earlier bool wrote iparm[24] = 1 for "true", backwards from
// what that code means).
std::optional<int> pardiso_solve_parallelism_code(SymmetricFactor::Options::SolveParallelism mode) {
    using SolveParallelism = SymmetricFactor::Options::SolveParallelism;
    switch (mode) {
    case SolveParallelism::kBackendDefault:
        return std::nullopt;
    case SolveParallelism::kAdaptivePartitioning:
        return 0;
    case SolveParallelism::kSequential:
        return 1;
    case SolveParallelism::kMatrixPartitionParallel:
        return 2;
    }
    throw std::invalid_argument(fmt::format(
        "SymmetricFactor: unknown Options::SolveParallelism value ({})", static_cast<int>(mode)));
}

// The inverse of pardiso_solve_parallelism_code, for adopt() round-tripping.
SymmetricFactor::Options::SolveParallelism pardiso_solve_parallelism_of(std::optional<int> code) {
    using SolveParallelism = SymmetricFactor::Options::SolveParallelism;
    if (!code.has_value()) {
        return SolveParallelism::kBackendDefault;
    }
    switch (*code) {
    case 0:
        return SolveParallelism::kAdaptivePartitioning;
    case 1:
        return SolveParallelism::kSequential;
    case 2:
        return SolveParallelism::kMatrixPartitionParallel;
    default:
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor::adopt: unrecognized stored solve parallelism code ({})", *code));
    }
}

// Maps SymmetricFactor::Options::FactorizationAlgorithm onto the raw
// iparm[23] override PardisoConfig carries (std::nullopt = don't write,
// matching kBackendDefault exactly).
std::optional<int>
pardiso_factorization_algorithm_code(SymmetricFactor::Options::FactorizationAlgorithm algorithm) {
    using FactorizationAlgorithm = SymmetricFactor::Options::FactorizationAlgorithm;
    switch (algorithm) {
    case FactorizationAlgorithm::kBackendDefault:
        return std::nullopt;
    case FactorizationAlgorithm::kClassic:
        return 0;
    case FactorizationAlgorithm::kTwoLevel:
        return 1;
    }
    throw std::invalid_argument(
        fmt::format("SymmetricFactor: unknown Options::FactorizationAlgorithm value ({})",
                    static_cast<int>(algorithm)));
}

// The inverse of pardiso_factorization_algorithm_code, for adopt()
// round-tripping.
SymmetricFactor::Options::FactorizationAlgorithm
pardiso_factorization_algorithm_of(std::optional<int> code) {
    using FactorizationAlgorithm = SymmetricFactor::Options::FactorizationAlgorithm;
    if (!code.has_value()) {
        return FactorizationAlgorithm::kBackendDefault;
    }
    switch (*code) {
    case 0:
        return FactorizationAlgorithm::kClassic;
    case 1:
        return FactorizationAlgorithm::kTwoLevel;
    default:
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor::adopt: unrecognized stored factorization algorithm code ({})",
            *code));
    }
}

// The thread-count rule, applied by the constructor to Options::num_threads
// and again by the live setter -- one function so the two cannot drift into
// accepting different values. `where` is the entry point the message names,
// which is the only thing that differs between the two callers.
void validate_num_threads(int num_threads, const char *where) {
    if (num_threads < 0) {
        throw std::invalid_argument(
            fmt::format("{}: num_threads must be >= 0 (0 means the backend default), got {}", where,
                        num_threads));
    }
}

detail::PardisoConfig config_from(const SymmetricFactor::Options &opts) {
    detail::PardisoConfig cfg;
    // Real symmetric indefinite. Only kLDLT reaches here -- the constructor
    // rejects the kinds whose backend paths have not landed.
    cfg.mtype = -2;
    cfg.num_threads = opts.num_threads;
    cfg.pivot_perturb_exp = opts.pivot_perturb_exp;
    cfg.max_refinement_iters = opts.max_refinement_iters;
    cfg.ordering = pardiso_ordering_code(opts.ordering);
    cfg.weighted_matching = opts.weighted_matching;
    cfg.matrix_scaling = opts.matrix_scaling;
    cfg.pivot_strategy = pardiso_pivot_strategy_code(opts.pivot_strategy);
    cfg.factorization_algorithm =
        pardiso_factorization_algorithm_code(opts.factorization_algorithm);
    cfg.solve_parallelism = pardiso_solve_parallelism_code(opts.solve_parallelism);
    cfg.cnr_threads = opts.cnr_threads;
    cfg.collect_factor_mflops = opts.collect_factor_mflops;
    return cfg;
}

// Validates the whole input convention in one pass: compressed, square,
// non-empty, upper triangle only, and a structurally present diagonal in
// every row.
//
// The last two are not decoration. Pardiso reads an upper-triangle CSR and
// requires every diagonal entry to exist in the pattern even where its value
// is zero; handed a matrix that violates either, it does not report a clean
// error, it computes something else. Checking costs one pass over the stored
// entries, once per symbolic analysis.
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
// The MKL column of the backend semantics table, in code: Pardiso reports the
// positive and negative counts and nothing else, so the zero class is derived
// by subtraction and flagged as derived; the perturbed-pivot counter is
// always present after a successful factorization on this backend, so it is
// reported as a value, never as an absence.
InertiaEvidence evidence_of(const detail::FactorSession &session) {
    InertiaEvidence evidence;
    evidence.state = InertiaEvidence::State::kObserved;
    evidence.n_pos = session.num_pos_eigs();
    evidence.n_neg = session.num_neg_eigs();
    evidence.n_zero = session.dim() - evidence.n_pos - evidence.n_neg;
    evidence.zero_is_derived = true;
    evidence.perturbed_pivots = session.num_perturbed_pivots();
    return evidence;
}

// Builds the factor-size evidence for a session's current factorization.
// Called only from factorize()'s success branch -- see FactorEvidence's own
// doc comment (symmetric_factor.h) for why this evidence lives on
// FactorizeOutcome rather than being recomputed per solve.
//
// The MKL column of the FactorEvidence semantics table, in code:
// factor_nonzeros is UNCONDITIONAL (session.factor_nonzeros() is always
// meaningful once has_numerics() is true -- see that accessor's own doc
// comment); factor_mflops is present only when
// Options::collect_factor_mflops requested it (session.has_factor_mflops());
// factor_size_bytes stays std::nullopt unconditionally on this backend,
// which reports an entry count and (opt-in) a cost estimate instead, never
// a byte size.
FactorEvidence factor_evidence_of(const detail::FactorSession &session) {
    FactorEvidence evidence;
    evidence.factor_nonzeros = session.factor_nonzeros();
    if (session.has_factor_mflops()) {
        evidence.factor_mflops = session.factor_mflops();
    }
    return evidence;
}

#ifdef HVEN_TESTING
// Records the thread count STORED IN THE SESSION CONFIG as the backend call
// about to run is issued -- the field FactorSession::run_phase's own thread
// scope reads, not a measurement of what MKL then ran at. See
// ThreadCountObserver (fault_injection.h) for exactly what that does and
// does not establish. A boundary read through FactorSession's ordinary
// config() accessor; compiles to nothing outside the fault-injection test
// target.
void record_config_thread_count(const detail::FactorSession &session) {
    detail::testing::ThreadCountObserver::recorded = true;
    detail::testing::ThreadCountObserver::last_config_num_threads = session.config().num_threads;
}
#endif

// Shared solve bodies: SymmetricFactor and Factorization differ in who may
// call them, not in what they do.
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

    // Pardiso needs contiguous column-major buffers and cannot solve in
    // place, so the right-hand side is always copied. The solution buffer is
    // only copied when X is a strided view into a larger matrix.
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

#ifdef HVEN_TESTING
    record_config_thread_count(session);
#endif
    session.solve(rhs_buffer.data(), target, static_cast<Index>(X.cols()));

    if (!x_contiguous) {
        X = scratch;
    }

    SolveInfo info;
    info.refinement_iters = session.refinement_iters();
    return info;
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
#ifdef HVEN_TESTING
    record_config_thread_count(session);
#endif
    session.solve(rhs_buffer.data(), x.data(), 1);

    SolveInfo info;
    info.refinement_iters = session.refinement_iters();
    return info;
}

int pardiso_phase_of(SymmetricFactor::SolvePhase phase) {
    switch (phase) {
    case SymmetricFactor::SolvePhase::kForward:
        return 331;
    case SymmetricFactor::SolvePhase::kDiagonal:
        return 332;
    case SymmetricFactor::SolvePhase::kBackward:
        return 333;
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
    // (leave the backend's own default in force) and needs no range check.
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

    // accelerate_zero_tolerance is Accelerate's own knob (it overrides
    // Accelerate's zeroTolerance directly); MKL has no zeroTolerance
    // concept for it to override. A present value here would otherwise be
    // silently ignored on this backend, exactly the situation
    // weighted_matching's own throw exists to prevent.
    if (opts_.accelerate_zero_tolerance.has_value()) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor: Options::accelerate_zero_tolerance is an Accelerate-only option and "
            "has no MKL equivalent -- requires std::nullopt on this backend, got {}",
            *opts_.accelerate_zero_tolerance));
    }

    // --- documented MKL option interactions, validated rather than left
    // for Pardiso to silently ignore ---
    //
    // Matrix scaling requires weighted matching on this backend's matrix
    // type. Intel (oneMKL Developer Reference, "pardiso iparm Parameter",
    // the iparm[10] entry) states a CAPABILITY condition for symmetric
    // indefinite matrices, distinct from the separate advisory
    // recommendation two sentences later: "The scaling can also be used
    // for symmetric indefinite matrices (mtype = -2, mtype = -4, mtype =
    // 6) when the symmetric weighted matchings are applied (iparm[12] =
    // 1)." This class (mtype = -2) is the ONLY one this session ever
    // builds (see config_from() above), so the capability condition
    // applies unconditionally here: `matrix_scaling == true` without
    // `weighted_matching == true` requests a capability Intel documents as
    // usable only alongside matching on this matrix type, and throws
    // rather than being silently accepted and doing something
    // undocumented.
    if (opts_.matrix_scaling && !opts_.weighted_matching) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor: matrix_scaling == true requires weighted_matching == true -- Intel "
            "documents scaling for symmetric indefinite matrices (mtype = -2, this class's own "
            "matrix type) as usable only when symmetric weighted matching is applied (iparm[12] = "
            "1), got matrix_scaling = true, weighted_matching = {}",
            opts_.weighted_matching));
    }

    // CNR mode requires nested-dissection ordering. Intel (oneMKL Developer
    // Reference, "pardiso iparm Parameter", the iparm[33] entry): "CNR is
    // only available for the in-core version of Intel oneMKL PARDISO and
    // the non-parallel version of the nested dissection algorithm... not
    // set iparm[1] to 3 in order to not use the parallel version of the
    // nested dissection algorithm. Otherwise Intel oneMKL PARDISO does not
    // produce numerically repeatable results even if CNR is enabled." Read
    // literally, the positive requirement names nested dissection
    // specifically (iparm[1] = 2) -- not minimum degree -- so the
    // compatible set is exactly {kNestedDissection}; every other Ordering
    // value, INCLUDING kBackendDefault (which floats with the linked MKL --
    // see Ordering's own doc comment -- and is 3, the documented-
    // incompatible value, on the MKL this was verified against) throws.
    if (opts_.cnr_threads > 0 && opts_.ordering != Options::Ordering::kNestedDissection) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor: cnr_threads > 0 (CNR mode) requires ordering == "
            "Ordering::kNestedDissection -- Intel documents CNR as reproducible only under the "
            "non-parallel nested dissection algorithm (iparm[1] = 2); every other ordering, "
            "including kBackendDefault, is documented-incompatible or unverified, got cnr_threads "
            "= {} with a non-kNestedDissection ordering",
            opts_.cnr_threads));
    }

    // The two-level factorization algorithm requires nested-dissection
    // ordering. Intel (same reference, the iparm[23] entry): "NOTE: If a
    // two-level factorization algorithm is chosen (that is, iparm[23]=1),
    // then only nested dissection algorithms are available (iparm[1]=2 or
    // iparm[1]=3)." kBackendDefault and kMinimumDegree both throw.
    if (opts_.factorization_algorithm == Options::FactorizationAlgorithm::kTwoLevel &&
        opts_.ordering != Options::Ordering::kNestedDissection &&
        opts_.ordering != Options::Ordering::kParallelNestedDissection) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor: factorization_algorithm == kTwoLevel requires ordering == "
            "kNestedDissection or kParallelNestedDissection -- Intel documents that the two-level "
            "algorithm only supports nested dissection orderings (iparm[1] = 2 or 3), got ordering "
            "= {} (Options::Ordering enum value)",
            static_cast<int>(opts_.ordering)));
    }

    // The two-level factorization algorithm is documented incompatible with
    // scaling and matching. Intel (same reference, the iparm[23] entry):
    // "Disable iparm[10] (scaling) and iparm[12]=1 (matching) when using
    // the two-level factorization algorithm."
    if (opts_.factorization_algorithm == Options::FactorizationAlgorithm::kTwoLevel &&
        (opts_.matrix_scaling || opts_.weighted_matching)) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor: factorization_algorithm == kTwoLevel requires matrix_scaling == "
            "false and weighted_matching == false -- Intel documents that scaling and matching "
            "must be disabled when using the two-level factorization algorithm, got matrix_scaling "
            "= {}, weighted_matching = {}",
            opts_.matrix_scaling, opts_.weighted_matching));
    }
}

SymmetricFactor::~SymmetricFactor() = default;
SymmetricFactor::SymmetricFactor(SymmetricFactor &&) noexcept = default;
SymmetricFactor &SymmetricFactor::operator=(SymmetricFactor &&) noexcept = default;

void SymmetricFactor::set_num_threads(int num_threads) {
    // Validated before anything moves, so a rejected count leaves this engine
    // exactly as it was -- the same shape analyze() keeps on a failed
    // symbolic phase.
    validate_num_threads(num_threads, "SymmetricFactor::set_num_threads");

    opts_.num_threads = num_threads;
    if (session_) {
        // The live session is where a backend call reads the count from, and
        // Pardiso reads it at CALL SCOPE -- the count is not in the parameter
        // array and the symbolic phase never saw it -- so moving it here
        // takes effect on the next call and invalidates nothing. `opts_`
        // above is what a LATER analyze() would build its session from, and
        // the two must not disagree.
        session_->set_num_threads(num_threads);
    }
}

int SymmetricFactor::num_threads() const noexcept {
    // Read THROUGH to the session rather than reporting `opts_`, whose copy
    // goes stale the moment a CO-OWNER of this session moves the count (the
    // setter above writes both, but it writes only ITS OWN engine's `opts_`).
    // The session's copy is the one Pardiso is handed at call scope, so it is
    // the only one that answers the question this reader is asked. `opts_`
    // remains the answer before the first analyze(), when there is no session
    // to read and `opts_` is what the next one will be built from.
    //
    // Kept in the adapter, not the session header: this is contract logic
    // about which of two copies is authoritative, not a Pardiso fact.
    return session_ ? session_->config().num_threads : opts_.num_threads;
}

void SymmetricFactor::analyze(const SpMatRM &A) {
    validate_upper_csr(A);

    // A fresh session per analysis. The alternative -- re-running the
    // symbolic phase in the existing session -- would silently destroy the
    // factorization any outstanding handle is co-owning. Committing the new
    // session only after the backend succeeds also means a failed analysis
    // leaves this engine exactly as it was.
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

#ifdef HVEN_TESTING
    // Pure observation, not injection -- see PardisoIparmObserver's own doc
    // comment (fault_injection.h) for why this is the ordering/
    // weighted_matching don't-write-by-default rule's only executable
    // coverage. Reads the accessors unconditionally: they are real,
    // non-test-gated FactorSession API (pardiso_session.h), so this branch's
    // only effect is recording their result into a testing-only struct.
    detail::testing::PardisoIparmObserver::recorded = true;
    detail::testing::PardisoIparmObserver::last_ordering_iparm = session->ordering_iparm();
    detail::testing::PardisoIparmObserver::last_weighted_matching_iparm =
        session->weighted_matching_iparm();
#endif

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
        // See hven/detail/linear/fault_injection.h for the exact scope this
        // is faithful within: valid ONLY when `session_` has never
        // previously factorized successfully. The real backend call is
        // skipped entirely rather than its result overridden, so
        // `session_`'s own has_numerics_/epoch_ (owned by the MPL-derived
        // FactorSession, never touched by this hook) are left exactly as
        // they were.
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
        // describe, and reporting the previous one's counts here would be a
        // fabrication. The session has already invalidated its numerics and
        // left the epoch where it was, so solves through this engine and
        // through any shared handle throw until a factorization succeeds.
        //
        // No test reaches this branch: this backend does not refuse a
        // symmetric indefinite factorization, it perturbs its way through one
        // (see the note in FactorSession::factorize). Correctness here rests
        // on inspection, which is why the failure handling is a single
        // unconditional statement.
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

    const int backend_phase = pardiso_phase_of(phase);

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

    SolveInfo info;
    info.refinement_iters = session_->refinement_iters();
    return info;
}

bool SymmetricFactor::supports_partial_solve() const {
    // Matching-on conjunct: composition under active backend
    // matching/scaling is unexercised at scale, and that caveat applies
    // squarely to Options::weighted_matching, which this
    // option set is the first to make reachable through hven's surface at
    // all. The predicate's design law is conservative-never-fabricated-true
    // (see its doc comment in symmetric_factor.h), so until composition
    // under matching has evidence behind it, matching being on forces this
    // predicate false. A future tuning program may relax this conjunct, but
    // only with bench/correctness evidence backing that decision -- not by
    // quietly dropping it.
    return has_usable_numerics() && session_->num_perturbed_pivots() == 0 &&
           !opts_.weighted_matching;
}

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
    const detail::PardisoConfig &cfg = session->config();

    // The adopting engine drives the emitter's session, so it inherits the
    // configuration that session was built with rather than imposing a new
    // one on a factorization that has already been computed.
    Options opts;
    opts.kind = FactorKind::kLDLT;
    opts.num_threads = cfg.num_threads;
    opts.pivot_perturb_exp = cfg.pivot_perturb_exp;
    opts.max_refinement_iters = cfg.max_refinement_iters;
    opts.ordering = pardiso_ordering_of(cfg.ordering);
    opts.weighted_matching = cfg.weighted_matching;
    opts.matrix_scaling = cfg.matrix_scaling;
    opts.pivot_strategy = pardiso_pivot_strategy_of(cfg.pivot_strategy);
    opts.factorization_algorithm = pardiso_factorization_algorithm_of(cfg.factorization_algorithm);
    opts.solve_parallelism = pardiso_solve_parallelism_of(cfg.solve_parallelism);
    opts.cnr_threads = cfg.cnr_threads;
    opts.collect_factor_mflops = cfg.collect_factor_mflops;

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
