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
    if (opts_.num_threads < 0) {
        throw std::invalid_argument(
            fmt::format("SymmetricFactor: num_threads must be >= 0 (0 means the backend default), "
                        "got {}",
                        opts_.num_threads));
    }
    if (opts_.pivot_perturb_exp < 0) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor: pivot_perturb_exp must be >= 0, got {}", opts_.pivot_perturb_exp));
    }
    if (opts_.max_refinement_iters < 0) {
        throw std::invalid_argument(
            fmt::format("SymmetricFactor: max_refinement_iters must be >= 0, got {}",
                        opts_.max_refinement_iters));
    }
}

SymmetricFactor::~SymmetricFactor() = default;
SymmetricFactor::SymmetricFactor(SymmetricFactor &&) noexcept = default;
SymmetricFactor &SymmetricFactor::operator=(SymmetricFactor &&) noexcept = default;

void SymmetricFactor::analyze(const SpMatRM &A) {
    validate_upper_csr(A);

    // A fresh session per analysis. The alternative -- re-running the
    // symbolic phase in the existing session -- would silently destroy the
    // factorization any outstanding handle is co-owning. Committing the new
    // session only after the backend succeeds also means a failed analysis
    // leaves this engine exactly as it was.
    auto session = std::make_shared<detail::FactorSession>(config_from(opts_), epoch());
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

std::uint64_t SymmetricFactor::session_id() const {
    return session_ ? session_->session_id() : 0;
}

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
