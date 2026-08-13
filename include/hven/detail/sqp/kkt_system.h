#pragma once

// Platform dispatch: Apple builds get the Accelerate backend; the MKL Pardiso
// implementation below is byte-untouched so warm-start merges stay clean.
#ifdef USE_ACCELERATE_SPARSE
#include <hven/detail/sqp/kkt_system_accelerate.h>
#else

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <fmt/format.h>

#include <mkl_pardiso.h>
#include <mkl_types.h>

#include <hven/detail/sqp/types.h>

namespace hven::solvers {

// Direct wrapper around Intel MKL Pardiso for factoring and solving the
// symmetric indefinite KKT system K x = b (mtype = -2), with inertia
// (positive/negative eigenvalue counts) and perturbed-pivot reporting.
//
// K is supplied as the upper triangle of a symmetric matrix in row-major CSR
// (the hven::solvers::SpMatU convention). Pardiso is configured for 0-based
// indexing (iparm[34] = 1) so Eigen's outerIndexPtr()/innerIndexPtr()/
// valuePtr() feed directly into the C API with no reindexing, and Eigen's
// default int StorageIndex matches MKL_INT under the MKL_LP64 interface this
// build uses.
class KktSystem {
  public:
    explicit KktSystem(const QpOptions &opts);
    ~KktSystem();

    KktSystem(const KktSystem &) = delete;
    KktSystem &operator=(const KktSystem &) = delete;
    KktSystem(KktSystem &&other) noexcept;
    KktSystem &operator=(KktSystem &&other) noexcept;

    // Symbolic analysis (Pardiso phase 11). Stores a hash of K's sparsity
    // pattern so later factorize() calls can detect when re-analysis is
    // needed.
    void analyze(const SpMatU &K);

    // Numeric factorization (Pardiso phase 22). Re-runs analyze() first iff
    // K's sparsity pattern differs from the last analyzed matrix.
    void factorize(const SpMatU &K);

    // Triangular solve (Pardiso phase 33) against the most recent
    // factorization. Returns a freshly allocated solution vector.
    Vec solve(const Vec &rhs) const;

    // Partial triangular solves against the most recent factorization: L y =
    // rhs (phase 331), D z = y (phase 332), and Lᵀ x = z (phase 333 -- per the
    // MKL docs the backward pass solves against Lᵀ, i.e. U x = z with
    // U := Lᵀ). Composing solve_backward(solve_diagonal(solve_forward(rhs)))
    // reproduces solve(rhs) only when supports_partial_solve() is true; MKL
    // additionally requires zero refinement steps (iparm[7] == 0) during
    // phase-33x calls, which these methods enforce via scoped save/restore.
    Vec solve_forward(const Vec &rhs) const;
    Vec solve_diagonal(const Vec &rhs) const;
    Vec solve_backward(const Vec &rhs) const;

    // True iff the phase-33x partial solves of the CURRENT factorization can
    // be trusted to compose to the phase-33 full solve. Gated
    // per-factorization: composition was verified exact only when pardiso
    // perturbed no pivots -- with perturbed pivots the composed result
    // silently diverges from solve() by O(1e8) on the Task 3 singular probe
    // (no error is raised), so the gate returns false whenever
    // num_perturbed_pivots() != 0. Additional caveat: composition under
    // active matching/scaling (iparm[12] != 0 taking effect on larger
    // production matrices) has not been exercised at scale. Details:
    // docs/notes/2026-07-27-pardiso-inertia-findings.md.
    bool supports_partial_solve() const { return active_ && num_perturbed_pivots() == 0; }

    // Inertia of the most recent factorization.
    Index num_pos_eigs() const; // iparm[21]
    Index num_neg_eigs() const; // iparm[22]

    // Number of pivots Pardiso perturbed during the most recent
    // factorization.
    Index num_perturbed_pivots() const; // iparm[13]

    // True iff K has the same sparsity pattern as the last analyzed matrix.
    bool pattern_matches(const SpMatU &K) const;

  private:
    static constexpr MKL_INT kMtype = -2; // real, symmetric indefinite

    static void require_compressed(const SpMatU &K);
    static std::uint64_t hash_pattern(const SpMatU &K);

    // Runs one Pardiso phase against `K` (may be null for phase -1, which
    // does not reference the matrix). Throws std::runtime_error on any
    // nonzero Pardiso error code.
    void run_phase(MKL_INT phase, const SpMatU *K, MKL_INT nrhs, double *b, double *x) const;

    // Shared body of the three phase-33x partial solves: validates state and
    // rhs size, then runs `phase` with iparm[7] (refinement steps) forced to
    // zero for the duration of the call -- the MKL docs require zero
    // refinement steps for step-by-step solves ("otherwise PARDISO produces
    // wrong result"). The previous iparm[7] value is restored afterward so
    // full solve() keeps its default refinement behavior.
    Vec partial_solve(MKL_INT phase, const Vec &rhs, const char *what) const;

    // Releases Pardiso-owned internal memory (phase -1) if a factorization
    // is currently active. Never throws — errors are swallowed, matching the
    // no-throw destructor requirement.
    void release() noexcept;

    QpOptions opts_;
    mutable std::array<void *, 64> pt_{};
    mutable std::array<MKL_INT, 64> iparm_{};
    mutable std::vector<MKL_INT> perm_;
    MKL_INT n_ = 0;
    bool initialized_ = false; // pardisoinit() has been called at least once
    bool active_ = false;      // pt_ holds live Pardiso memory (needs phase -1)
    bool has_pattern_ = false;
    std::uint64_t pattern_hash_ = 0;
    SpMatU factored_k_; // last matrix's CSR data, kept alive for solve()
};

inline KktSystem::KktSystem(const QpOptions &opts) : opts_(opts) {
    pt_.fill(nullptr);
    iparm_.fill(0);
}

inline KktSystem::~KktSystem() { release(); }

inline KktSystem::KktSystem(KktSystem &&other) noexcept
    : opts_(other.opts_), pt_(other.pt_), iparm_(other.iparm_), perm_(std::move(other.perm_)),
      n_(other.n_), initialized_(other.initialized_), active_(other.active_),
      has_pattern_(other.has_pattern_), pattern_hash_(other.pattern_hash_),
      factored_k_(std::move(other.factored_k_)) {
    other.pt_.fill(nullptr);
    other.initialized_ = false;
    other.active_ = false;
    other.has_pattern_ = false;
    other.n_ = 0;
}

inline KktSystem &KktSystem::operator=(KktSystem &&other) noexcept {
    if (this != &other) {
        release();
        opts_ = other.opts_;
        pt_ = other.pt_;
        iparm_ = other.iparm_;
        perm_ = std::move(other.perm_);
        n_ = other.n_;
        initialized_ = other.initialized_;
        active_ = other.active_;
        has_pattern_ = other.has_pattern_;
        pattern_hash_ = other.pattern_hash_;
        factored_k_ = std::move(other.factored_k_);

        other.pt_.fill(nullptr);
        other.initialized_ = false;
        other.active_ = false;
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

inline void KktSystem::release() noexcept {
    // A pattern hash must not outlive the symbolic factorization it describes,
    // and this runs BEFORE the active_ early-return for exactly that reason:
    // if analyze() throws after the release() at its top, `active_` is already
    // false while pattern_hash_ still names the freed structure, and a later
    // same-pattern factorize() would skip analyze() and run phase 22 against
    // Pardiso-internal memory that no longer exists. Clearing the flag here
    // routes that call back through analyze(), which is the only correct
    // recovery.
    //
    // Isomorphic to the fix the Accelerate port made to its own copy (see
    // kkt_system_accelerate.h's release() and the review side-note in
    // docs/notes/2026-07-29-accelerate-audit-results.md, which flagged this
    // twin rather than touching it there). Not reachable from well-formed
    // input today -- every analyze() failure this project has observed comes
    // from a matrix the caller could not have handed to factorize() either --
    // so it ships without a fixture: reaching it needs a Pardiso phase-11
    // failure that leaves the object alive, which no test can provoke without
    // mocking the backend.
    has_pattern_ = false;
    if (!active_) {
        return;
    }
    MKL_INT maxfct = 1;
    MKL_INT mnum = 1;
    MKL_INT phase = -1;
    MKL_INT nrhs = 0;
    MKL_INT msglvl = 0;
    MKL_INT error = 0;
    MKL_INT n = n_;
    pardiso(pt_.data(), &maxfct, &mnum, &kMtype, &phase, &n, nullptr, nullptr, nullptr,
            perm_.data(), &nrhs, iparm_.data(), &msglvl, nullptr, nullptr, &error);
    // Phase -1 is not expected to fail in practice; even if it does, the
    // destructor (and any re-analysis path) must not throw, so the error is
    // intentionally discarded here.
    active_ = false;
}

inline void KktSystem::run_phase(MKL_INT phase, const SpMatU *K, MKL_INT nrhs, double *b,
                                 double *x) const {
    MKL_INT maxfct = 1;
    MKL_INT mnum = 1;
    MKL_INT msglvl = 0;
    MKL_INT error = 0;
    MKL_INT n = n_;
    const void *a = K != nullptr ? static_cast<const void *>(K->valuePtr()) : nullptr;
    const MKL_INT *ia = K != nullptr ? K->outerIndexPtr() : nullptr;
    const MKL_INT *ja = K != nullptr ? K->innerIndexPtr() : nullptr;
    pardiso(pt_.data(), &maxfct, &mnum, &kMtype, &phase, &n, a, ia, ja, perm_.data(), &nrhs,
            iparm_.data(), &msglvl, b, x, &error);
    if (error != 0) {
        throw std::runtime_error(fmt::format("pardiso phase {} failed: error {}", phase, error));
    }
}

inline void KktSystem::analyze(const SpMatU &K) {
    require_compressed(K);
    if (K.rows() != K.cols()) {
        throw std::invalid_argument(
            fmt::format("KktSystem::analyze: K must be square, got {}x{}", K.rows(), K.cols()));
    }

    // Free any previous factorization's internal memory before re-analyzing;
    // Pardiso's internal structures are tied to the sparsity pattern from the
    // last phase 11 call and must not be reused across a pattern change.
    release();

    n_ = static_cast<MKL_INT>(K.rows());
    perm_.assign(static_cast<std::size_t>(n_), 0);

    if (!initialized_) {
        pt_.fill(nullptr);
        iparm_.fill(0);
        pardisoinit(pt_.data(), &kMtype, iparm_.data());
        iparm_[34] = 1; // zero-based (C-style) CSR indexing
        initialized_ = true;
    }

    factored_k_ = K;
    run_phase(/*phase=*/11, &factored_k_, /*nrhs=*/0, nullptr, nullptr);
    active_ = true;

    pattern_hash_ = hash_pattern(K);
    has_pattern_ = true;
}

inline void KktSystem::factorize(const SpMatU &K) {
    require_compressed(K);
    if (!pattern_matches(K)) {
        analyze(K);
    }
    factored_k_ = K;
    run_phase(/*phase=*/22, &factored_k_, /*nrhs=*/0, nullptr, nullptr);
}

inline Vec KktSystem::solve(const Vec &rhs) const {
    if (!active_) {
        throw std::runtime_error("KktSystem::solve: called before a successful factorize()");
    }
    if (rhs.size() != n_) {
        throw std::invalid_argument(
            fmt::format("KktSystem::solve: rhs size {} does not match factored system size {}",
                        rhs.size(), n_));
    }
    Vec rhs_copy = rhs; // Pardiso cannot solve in place; b and x must differ.
    Vec x(n_);
    run_phase(/*phase=*/33, &factored_k_, /*nrhs=*/1, rhs_copy.data(), x.data());
    return x;
}

inline Vec KktSystem::partial_solve(MKL_INT phase, const Vec &rhs, const char *what) const {
    if (!active_) {
        throw std::runtime_error(
            fmt::format("KktSystem::{}: called before a successful factorize()", what));
    }
    if (rhs.size() != n_) {
        throw std::invalid_argument(
            fmt::format("KktSystem::{}: rhs size {} does not match factored system size {}", what,
                        rhs.size(), n_));
    }
    Vec rhs_copy = rhs;
    Vec x(n_);
    const MKL_INT saved_refinement_steps = iparm_[7];
    iparm_[7] = 0; // MKL requires zero refinement steps for phase-33x solves
    try {
        run_phase(phase, &factored_k_, /*nrhs=*/1, rhs_copy.data(), x.data());
    } catch (...) {
        iparm_[7] = saved_refinement_steps;
        throw;
    }
    iparm_[7] = saved_refinement_steps;
    return x;
}

inline Vec KktSystem::solve_forward(const Vec &rhs) const {
    return partial_solve(/*phase=*/331, rhs, "solve_forward");
}

inline Vec KktSystem::solve_diagonal(const Vec &rhs) const {
    return partial_solve(/*phase=*/332, rhs, "solve_diagonal");
}

inline Vec KktSystem::solve_backward(const Vec &rhs) const {
    return partial_solve(/*phase=*/333, rhs, "solve_backward");
}

inline Index KktSystem::num_pos_eigs() const { return static_cast<Index>(iparm_[21]); }

inline Index KktSystem::num_neg_eigs() const { return static_cast<Index>(iparm_[22]); }

inline Index KktSystem::num_perturbed_pivots() const { return static_cast<Index>(iparm_[13]); }

} // namespace hven::solvers

#endif // USE_ACCELERATE_SPARSE
