#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <fmt/format.h>

#include <Accelerate/Accelerate.h>

#include <hven/detail/sqp/types.h>

namespace hven::solvers {

// AUDIT PORT of the Pardiso KktSystem wrapper onto Apple Accelerate's Sparse
// API (SparseFactor / SparseSolve / SparseCreateSubfactor), presenting the
// identical public interface: factor and solve the symmetric indefinite KKT
// system K x = b with inertia and pivot-integrity reporting.
//
// Every policy choice here was set per the fallback ladder in
// docs/notes/2026-07-28-accelerate-audit-checklist.md, pending measurement on
// real hardware; the measurements and per-item verdicts live in
// docs/notes/2026-07-29-accelerate-audit-results.md. In particular:
//
//  - supports_partial_solve() is hardwired false (checklist §(b) fallback,
//    zero-regression -- see the method comment);
//  - num_perturbed_pivots() maps to Accelerate's ZERO-pivot count, not to a
//    hardcoded 0 (see the method comment -- this is the audit's key contract
//    mapping);
//  - the factorization kind is SparseFactorizationLDLTTPP explicitly, not the
//    SparseFactorizationLDLT alias: SparseGetInertia is documented as
//    supported only for LDLTTPP-typed factorizations, and the alias (while
//    currently implemented as TPP) records a different type tag.
//
// K is supplied as the upper triangle of a symmetric matrix in row-major CSR
// (the hven::solvers::SpMatU convention). Accelerate consumes CSC: the CSR-upper
// index arrays, read column-wise, are exactly the lower triangle of Kᵀ = K,
// so they are fed unchanged with kind = SparseSymmetric and
// triangle = SparseLowerTriangle. The only conversion is columnStarts, which
// Accelerate types as long* against Eigen's int outer indices.
class KktSystem {
  public:
    explicit KktSystem(const QpOptions &opts);
    ~KktSystem();

    KktSystem(const KktSystem &) = delete;
    KktSystem &operator=(const KktSystem &) = delete;
    KktSystem(KktSystem &&other) noexcept;
    KktSystem &operator=(KktSystem &&other) noexcept;

    // Symbolic analysis (Accelerate symbolic SparseFactor). Stores a hash of
    // K's sparsity pattern so later factorize() calls can detect when
    // re-analysis is needed.
    void analyze(const SpMatU &K);

    // Numeric factorization against the last analyzed pattern. Re-runs
    // analyze() first iff K's sparsity pattern differs from the last analyzed
    // matrix.
    void factorize(const SpMatU &K);

    // Full solve against the most recent factorization. Returns a freshly
    // allocated solution vector.
    Vec solve(const Vec &rhs) const;

    // Partial triangular solves against the most recent factorization,
    // expressed via Accelerate subfactors: L y = rhs (SparseSubfactorL),
    // D z = y (SparseSubfactorD), and Lᵀ x = z (SparseSubfactorL with the
    // transpose attribute set). Whether this three-stage split composes to
    // solve() -- as Pardiso's phases 331/332/333 did when no pivot was
    // perturbed -- is exactly checklist §(b)'s measurement: Accelerate's
    // internal fill-reducing permutation P and equilibration scaling S may
    // make the bare L/D/Lᵀ stages non-composable. The audit probe measures
    // it; consult supports_partial_solve() before trusting a composition.
    Vec solve_forward(const Vec &rhs) const;
    Vec solve_diagonal(const Vec &rhs) const;
    Vec solve_backward(const Vec &rhs) const;

    // Checklist §(b) fallback: hardwired false. MEASURED, not merely cautious
    // -- composition of the bare L/D/Lᵀ subfactor solves diverges O(1) from
    // solve() even on clean factorizations (measured ‖diff‖ = 3.355e+00 on
    // the Task-4 saddle matrix and 7.807e+00 on an 8x8 arrow matrix with a
    // real AMD permutation, while full-solve residuals were ~2e-15;
    // tests/test_accelerate_probes.cpp,
    // AccelerateProbe.ComposedPartialsVsFullSolve / ...Arrow), because
    // Accelerate's subfactor solves operate in the internal permuted+scaled
    // basis -- unlike Pardiso's phases 331/332/333. Hardwired false is
    // therefore the measured policy, not caution; zero-regression per the
    // checklist §(b) fallback -- SchurComplement::solve() always uses the
    // two-full-solve variant and the partial-solve fast path was never
    // implemented, so a false here forecloses only an optimization that was
    // never exercised. Deliberately NOT modeled on tycho's
    // AccelerateImpl::ppivs() == 0 precedent: a 0 that means "no such concept
    // on this backend" must not back a trust gate as if it meant "verified
    // unperturbed".
    bool supports_partial_solve() const { return false; }

    // Inertia of the most recent factorization, from SparseGetInertia
    // (LDLTTPP only). Throws if the factorization is absent or the inertia
    // query itself failed -- unlike Pardiso's iparm counters there is no
    // always-present backing field to read.
    Index num_pos_eigs() const;
    Index num_neg_eigs() const;

    // The audit's key contract mapping (checklist §(d), num_perturbed_pivots
    // row): Accelerate's LDLTTPP has no pivot-perturbation mechanism at all.
    // Threshold partial pivoting with zeroTolerance declares tiny pivots
    // exactly ZERO instead of perturbing them onward, so the nearest "this
    // factorization is not a genuine LDLT of K" signal is the zero-pivot
    // count from SparseGetInertia, and that is what this returns. A 0 here
    // means "no pivot was zeroed", which backs the same trust decisions
    // (inertia trust, partial-solve gating) that Pardiso's perturbed-pivot
    // counter backs. This is deliberately NOT a hardcoded 0 that would read
    // as "verified unperturbed" while meaning "no such concept". Validated by
    // the §(a) probes: on the exactly-singular probe matrix this counter
    // reads 1 exactly where Pardiso's perturbed-pivot counter read 1, while
    // the sign counts stay honest ((1,1) + zero, vs Pardiso's fabricated
    // (2,1)); see the results note.
    Index num_perturbed_pivots() const;

    // True iff K has the same sparsity pattern as the last analyzed matrix.
    bool pattern_matches(const SpMatU &K) const;

  private:
    // Explicit TPP, not the SparseFactorizationLDLT alias: SparseGetInertia
    // is documented LDLTTPP-only, and the alias records a different type tag
    // even though it is currently implemented as TPP.
    static constexpr SparseFactorization_t kFactorization = SparseFactorizationLDLTTPP;

    static void require_compressed(const SpMatU &K);
    static std::uint64_t hash_pattern(const SpMatU &K);

    // Builds the Accelerate view of factored_k_ (see the class comment for
    // the CSR-upper == CSC-lower correspondence). Pointers refer to
    // factored_k_ and col_starts_, which this class keeps alive.
    SparseMatrixStructure structure() const;

    // Shared body of the three subfactor partial solves.
    Vec partial_solve(SparseSubfactor_t which, bool transpose, const Vec &rhs,
                      const char *what) const;

    bool inertia_ready() const;

    // Release the numeric / all factorization state. Never throw -- cleanup
    // must be safe from the destructor and from re-analysis paths.
    void release_numeric() noexcept;
    void release() noexcept;

    QpOptions opts_;
    SpMatU factored_k_;            // last matrix's CSR data, kept alive for Accelerate
    std::vector<long> col_starts_; // long-typed copy of factored_k_'s outer index
    std::vector<int> perm_;        // fill-reducing ordering RETURNED by Accelerate (§(c))
    Index n_ = 0;
    SparseOpaqueSymbolicFactorization symbolic_{};
    SparseOpaqueFactorization_Double numeric_{};
    bool have_symbolic_ = false;
    bool have_numeric_ = false;
    bool inertia_valid_ = false;
    Index pos_ = 0;
    Index neg_ = 0;
    Index zero_ = 0;
    bool has_pattern_ = false;
    std::uint64_t pattern_hash_ = 0;
};

namespace detail {

// Capture target for Accelerate's reportError callback. A NULL callback makes
// Accelerate log via os_log and halt the process with __builtin_trap() on
// parameter errors -- library code must never do that (tycho rule T5) -- and
// printing from the callback would be a diagnostic outside the thrown
// exception (rule T6). The callback therefore only records the message; the
// calling method folds it into whatever it throws. Thread-local because the
// callback carries no user context pointer -- which also means an error
// reported from an Accelerate-internal worker thread would land in that
// thread's buffer and go unseen; measured behavior is that parameter errors
// fire on the calling thread, so this is a documented residual risk, not an
// observed one.
inline thread_local std::string accelerate_last_error;

inline void accelerate_capture_error(const char *message) {
    std::string text = message != nullptr ? message : "(no message)";
    while (!text.empty() && (text.back() == '\n' || text.back() == ' ')) {
        text.pop_back(); // Accelerate's messages end in '\n'; keep exceptions one-line
    }
    accelerate_last_error = std::move(text);
}

} // namespace detail

// Eigen's default int StorageIndex must match Accelerate's int rowIndices /
// ordering arrays, which this class feeds directly without conversion.
static_assert(std::is_same_v<SpMatU::StorageIndex, int>,
              "KktSystem (Accelerate) requires SpMatU::StorageIndex == int: Accelerate's "
              "sparse structure and ordering fields are int*.");

inline KktSystem::KktSystem(const QpOptions &opts) : opts_(opts) {}

inline KktSystem::~KktSystem() { release(); }

inline KktSystem::KktSystem(KktSystem &&other) noexcept
    : opts_(other.opts_), factored_k_(std::move(other.factored_k_)),
      col_starts_(std::move(other.col_starts_)), perm_(std::move(other.perm_)), n_(other.n_),
      symbolic_(other.symbolic_), numeric_(other.numeric_), have_symbolic_(other.have_symbolic_),
      have_numeric_(other.have_numeric_), inertia_valid_(other.inertia_valid_), pos_(other.pos_),
      neg_(other.neg_), zero_(other.zero_), has_pattern_(other.has_pattern_),
      pattern_hash_(other.pattern_hash_) {
    // The opaque factorizations were copied by value; clearing the source's
    // flags is what prevents a double SparseCleanup, since all cleanup here
    // is flag-guarded.
    other.have_symbolic_ = false;
    other.have_numeric_ = false;
    other.inertia_valid_ = false;
    other.has_pattern_ = false;
    other.n_ = 0;
}

inline KktSystem &KktSystem::operator=(KktSystem &&other) noexcept {
    if (this != &other) {
        release();
        opts_ = other.opts_;
        factored_k_ = std::move(other.factored_k_);
        col_starts_ = std::move(other.col_starts_);
        perm_ = std::move(other.perm_);
        n_ = other.n_;
        symbolic_ = other.symbolic_;
        numeric_ = other.numeric_;
        have_symbolic_ = other.have_symbolic_;
        have_numeric_ = other.have_numeric_;
        inertia_valid_ = other.inertia_valid_;
        pos_ = other.pos_;
        neg_ = other.neg_;
        zero_ = other.zero_;
        has_pattern_ = other.has_pattern_;
        pattern_hash_ = other.pattern_hash_;

        other.have_symbolic_ = false;
        other.have_numeric_ = false;
        other.inertia_valid_ = false;
        other.has_pattern_ = false;
        other.n_ = 0;
    }
    return *this;
}

inline void KktSystem::require_compressed(const SpMatU &K) {
    if (!K.isCompressed()) {
        throw std::invalid_argument(
            fmt::format("KktSystem: matrix must be compressed (call makeCompressed() first)"));
    }
}

inline std::uint64_t KktSystem::hash_pattern(const SpMatU &K) {
    // FNV-1a over (rows, nnz, outer index bytes, inner index bytes).
    constexpr std::uint64_t kOffsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
    std::uint64_t h = kOffsetBasis;
    auto mix_bytes = [&h](const void *data, std::size_t len) {
        const auto *bytes = static_cast<const unsigned char *>(data);
        for (std::size_t i = 0; i < len; ++i) {
            h ^= bytes[i];
            h *= kFnvPrime;
        }
    };
    const Index rows = K.rows();
    const Index nnz = K.nonZeros();
    mix_bytes(&rows, sizeof(rows));
    mix_bytes(&nnz, sizeof(nnz));
    mix_bytes(K.outerIndexPtr(), sizeof(SpMatU::StorageIndex) * static_cast<std::size_t>(rows + 1));
    mix_bytes(K.innerIndexPtr(), sizeof(SpMatU::StorageIndex) * static_cast<std::size_t>(nnz));
    return h;
}

inline bool KktSystem::pattern_matches(const SpMatU &K) const {
    require_compressed(K);
    if (!has_pattern_ || K.rows() != n_) {
        return false;
    }
    return hash_pattern(K) == pattern_hash_;
}

inline void KktSystem::release_numeric() noexcept {
    if (have_numeric_) {
        SparseCleanup(numeric_);
        have_numeric_ = false;
    }
    inertia_valid_ = false;
}

inline void KktSystem::release() noexcept {
    release_numeric();
    if (have_symbolic_) {
        SparseCleanup(symbolic_);
        have_symbolic_ = false;
    }
    // A pattern hash must not outlive the symbolic factorization it describes:
    // if analyze() fails after this release, a stale hash could later route a
    // same-pattern factorize() past analyze() and hand the freed symbolic_ to
    // SparseFactor. (The MKL twin has the isomorphic shape; noted in the
    // audit results note rather than changed there.)
    has_pattern_ = false;
}

inline SparseMatrixStructure KktSystem::structure() const {
    SparseMatrixStructure s{};
    s.rowCount = static_cast<int>(n_);
    s.columnCount = static_cast<int>(n_);
    s.columnStarts = const_cast<long *>(col_starts_.data());
    s.rowIndices = const_cast<int *>(factored_k_.innerIndexPtr());
    s.attributes.kind = SparseSymmetric;
    s.attributes.triangle = SparseLowerTriangle;
    s.blockSize = 1;
    return s;
}

inline void KktSystem::analyze(const SpMatU &K) {
    require_compressed(K);
    if (K.rows() != K.cols()) {
        throw std::invalid_argument(
            fmt::format("KktSystem::analyze: K must be square, got {}x{}", K.rows(), K.cols()));
    }
    if (K.rows() > std::numeric_limits<int>::max()) {
        throw std::invalid_argument(
            fmt::format("KktSystem::analyze: K is {}x{} but Accelerate's sparse structure uses "
                        "int dimensions",
                        K.rows(), K.cols()));
    }

    // Discard any previous factorization state; the symbolic object is tied
    // to the sparsity pattern being replaced here.
    release();

    n_ = K.rows();
    factored_k_ = K;
    const auto *outer = factored_k_.outerIndexPtr();
    col_starts_.assign(outer, outer + n_ + 1);
    // Receives the ordering Accelerate computes (orderMethod != User + a
    // non-null order pointer means "return the computed permutation") -- the
    // audit's §(c) instrument for inspecting the default AMD ordering.
    perm_.assign(static_cast<std::size_t>(n_), 0);

    SparseSymbolicFactorOptions fopts{};
    fopts.control = SparseDefaultControl;
    fopts.orderMethod = SparseOrderDefault;
    fopts.order = perm_.data();
    fopts.ignoreRowsAndColumns = nullptr;
    fopts.malloc = std::malloc;
    fopts.free = std::free;
    fopts.reportError = &detail::accelerate_capture_error;

    detail::accelerate_last_error.clear();
    symbolic_ = SparseFactor(kFactorization, structure(), fopts);
    // Cleanup is required even for a FAILED factorization, so the flag is set
    // before the status check.
    have_symbolic_ = true;
    if (symbolic_.status != SparseStatusOK) {
        const auto status = static_cast<int>(symbolic_.status);
        const std::string message = detail::accelerate_last_error;
        release();
        throw std::runtime_error(
            fmt::format("KktSystem::analyze: Accelerate symbolic factorization failed: status {}{}",
                        status, message.empty() ? "" : fmt::format(" ({})", message)));
    }

    pattern_hash_ = hash_pattern(K);
    has_pattern_ = true;
}

inline void KktSystem::factorize(const SpMatU &K) {
    require_compressed(K);
    if (!pattern_matches(K)) {
        analyze(K);
    }
    factored_k_ = K;
    const auto *outer = factored_k_.outerIndexPtr();
    col_starts_.assign(outer, outer + n_ + 1);
    release_numeric();

    // Apple's own documented double defaults (SolveImplementation.h),
    // restated explicitly so the audit pins them: default scaling for LDLT is
    // inf-norm equilibration; pivotTolerance 0.01 is the "recommended value
    // for difficult matrices in double"; zeroTolerance is "a few orders of
    // magnitude below epsilon".
    SparseNumericFactorOptions nopts{};
    nopts.control = SparseDefaultControl;
    nopts.scalingMethod = SparseScalingDefault;
    nopts.scaling = nullptr;
    nopts.pivotTolerance = 0.01;
    nopts.zeroTolerance = 1e-4 * std::numeric_limits<double>::epsilon();

    SparseMatrix_Double A{};
    A.structure = structure();
    A.data = const_cast<double *>(factored_k_.valuePtr());

    detail::accelerate_last_error.clear();
    numeric_ = SparseFactor(symbolic_, A, nopts);
    have_numeric_ = true; // cleanup required even on failure
    if (numeric_.status != SparseStatusOK) {
        // Behavioral divergence from Pardiso, deliberately surfaced rather
        // than papered over: Pardiso silently perturbs pivots on singular
        // input (error 0, iparm[13] > 0); Accelerate reports
        // SparseMatrixIsSingular (-2) or SparseFactorizationFailed (-1)
        // instead. See the audit results note.
        const auto status = static_cast<int>(numeric_.status);
        const std::string message = detail::accelerate_last_error;
        release_numeric();
        throw std::runtime_error(
            fmt::format("KktSystem::factorize: Accelerate numeric factorization failed: "
                        "status {}{}",
                        status, message.empty() ? "" : fmt::format(" ({})", message)));
    }

    // Argument order is (positive, ZERO, negative). Zero pivots are the
    // backend's pivot-integrity signal -- see num_perturbed_pivots().
    int pos = 0;
    int zero = 0;
    int neg = 0;
    const int rc = SparseGetInertia(numeric_, &pos, &zero, &neg);
    inertia_valid_ = rc == 0;
    pos_ = inertia_valid_ ? static_cast<Index>(pos) : 0;
    zero_ = inertia_valid_ ? static_cast<Index>(zero) : 0;
    neg_ = inertia_valid_ ? static_cast<Index>(neg) : 0;
}

inline Vec KktSystem::solve(const Vec &rhs) const {
    if (!have_numeric_) {
        throw std::runtime_error("KktSystem::solve: called before a successful factorize()");
    }
    if (rhs.size() != n_) {
        throw std::invalid_argument(
            fmt::format("KktSystem::solve: rhs size {} does not match factored system size {}",
                        rhs.size(), n_));
    }
    Vec rhs_copy = rhs; // Accelerate's out-of-place overload wants non-const b
    Vec x(n_);
    DenseVector_Double b{static_cast<int>(n_), rhs_copy.data()};
    DenseVector_Double xv{static_cast<int>(n_), x.data()};
    detail::accelerate_last_error.clear();
    SparseSolve(numeric_, b, xv);
    // SparseSolve takes the factorization by value, so no status is
    // observable afterward; the captured reportError message is the only
    // failure channel this call has.
    if (!detail::accelerate_last_error.empty()) {
        throw std::runtime_error(fmt::format("KktSystem::solve: Accelerate reported: {}",
                                             detail::accelerate_last_error));
    }
    return x;
}

inline Vec KktSystem::partial_solve(SparseSubfactor_t which, bool transpose, const Vec &rhs,
                                    const char *what) const {
    if (!have_numeric_) {
        throw std::runtime_error(
            fmt::format("KktSystem::{}: called before a successful factorize()", what));
    }
    if (rhs.size() != n_) {
        throw std::invalid_argument(
            fmt::format("KktSystem::{}: rhs size {} does not match factored system size {}", what,
                        rhs.size(), n_));
    }

    // RAII so the subfactor is cleaned up on the throw path too.
    struct SubfactorGuard {
        SparseOpaqueSubfactor_Double sub;
        ~SubfactorGuard() { SparseCleanup(sub); }
    };

    detail::accelerate_last_error.clear();
    SubfactorGuard guard{SparseCreateSubfactor(which, numeric_)};
    if (!detail::accelerate_last_error.empty()) {
        // A create-time error leaves the subfactor in Apple's "safe but
        // undefined" state; solving against it would misattribute the error.
        throw std::runtime_error(fmt::format("KktSystem::{}: SparseCreateSubfactor reported: {}",
                                             what, detail::accelerate_last_error));
    }
    guard.sub.attributes.transpose = transpose;

    Vec x = rhs; // in-place subfactor solve
    DenseVector_Double xb{static_cast<int>(n_), x.data()};
    detail::accelerate_last_error.clear();
    SparseSolve(guard.sub, xb);
    if (!detail::accelerate_last_error.empty()) {
        throw std::runtime_error(fmt::format("KktSystem::{}: Accelerate reported: {}", what,
                                             detail::accelerate_last_error));
    }
    return x;
}

inline Vec KktSystem::solve_forward(const Vec &rhs) const {
    return partial_solve(SparseSubfactorL, /*transpose=*/false, rhs, "solve_forward");
}

inline Vec KktSystem::solve_diagonal(const Vec &rhs) const {
    return partial_solve(SparseSubfactorD, /*transpose=*/false, rhs, "solve_diagonal");
}

inline Vec KktSystem::solve_backward(const Vec &rhs) const {
    return partial_solve(SparseSubfactorL, /*transpose=*/true, rhs, "solve_backward");
}

// Without a valid factorization (never factorized, or SparseGetInertia
// errored) the getters do NOT throw -- the MKL twin returns values
// unconditionally, and qp_engine.h's inertia_verdict documents the short
// pos+neg sum as its "stale/absent factorization" kSuspect path; a throw here
// would replace that documented degradation with a failure mode the engine
// was never written against (review finding, adopted; results note register).
// The degraded triple is (pos 0, neg 0, counter n_): every consumer lands on
// "suspect" -- the sum comes up short AND the counter is nonzero whenever
// n_ > 0. This is deliberately MORE conservative than the MKL twin, whose
// uninitialized iparm reads as the trust-signaling (0, 0, perturbed 0).
inline bool KktSystem::inertia_ready() const { return have_numeric_ && inertia_valid_; }

inline Index KktSystem::num_pos_eigs() const { return inertia_ready() ? pos_ : 0; }

inline Index KktSystem::num_neg_eigs() const { return inertia_ready() ? neg_ : 0; }

inline Index KktSystem::num_perturbed_pivots() const { return inertia_ready() ? zero_ : n_; }

} // namespace hven::solvers
