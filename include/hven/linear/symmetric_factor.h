#pragma once

// Sparse symmetric-indefinite factor/solve: the analyze / factorize / solve
// lifecycle over a backend factorization session (MKL Pardiso on Linux and
// Windows; Apple Accelerate on macOS, landing separately). This is the
// workhorse of hven's linear-algebra layer -- the KKT oracle every engine
// above it drives -- and its semantics are set by the frozen interface
// contract, not by what any one backend finds convenient.
//
// Three properties shape everything below.
//
// 1. EVIDENCE, NOT VERDICTS. A factorization reports what the backend
//    actually observed (inertia counts, whether the zero class was derived
//    rather than measured, how many pivots were perturbed, how many
//    refinement steps ran). It never decides "singular" or "wrong inertia"
//    -- the drivers own those verdicts. Absent evidence is reported absent
//    (std::optional), never zero-filled into a plausible-looking number.
//
// 2. MISUSE THROWS, NUMERICS RETURN. Calling out of order, handing in a
//    matrix whose sparsity pattern is not the analyzed one, or a size
//    mismatch is a caller bug and throws with a formatted message. A
//    backend that factorizes and reports trouble is a numeric outcome and
//    comes back in FactorizeOutcome.
//
// 3. THE SYMBOLIC IS PRECIOUS. analyze() is the expensive step; factorize()
//    never re-runs it, and the counters make that observable rather than
//    merely claimed.
//
// The matrix convention throughout is the UPPER TRIANGLE of a symmetric
// matrix in compressed row-major CSR (hven::SpMatRM), with every diagonal
// entry present in the sparsity pattern even where its value is zero.
// analyze() validates all of that at the boundary.

#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>

#include "hven/core/types.h"

namespace hven::linear {

namespace detail {

// The backend session (Pardiso handle, its parameter array, and the CSR copy
// it factorizes). Deliberately incomplete here: no backend type reaches this
// header, so consumers never need MKL's headers on their include path.
class PardisoSession;

// Eigen binds a vector to a matrix Ref and a matrix to a vector Ref, so a
// pair of solve(vector) / solve(matrix) overloads spelled purely in terms of
// Ref parameters is ambiguous for BOTH call shapes. This concept splits them
// by compile-time column count instead: the single-RHS entry point is a
// template constrained on it (an exact match, so it wins for vector-shaped
// arguments), and the multi-RHS entry point stays a plain overload.
//
// Spelled over MatrixBase rather than DenseBase so the concept means what
// its name says: a matrix expression with one column. An array expression
// (Eigen::ArrayXd and friends) is excluded by this constraint but is NOT
// rejected by the class -- it binds the multi-RHS overload via Ref
// conversion and solves as a one-column right-hand side, which is benign
// (a solve has no array-vs-matrix semantic ambiguity, and mis-shaped
// inputs still hit this class's own size validation).
template <class T>
concept VectorShaped =
    std::derived_from<std::remove_cvref_t<T>, Eigen::MatrixBase<std::remove_cvref_t<T>>> &&
    (std::remove_cvref_t<T>::ColsAtCompileTime == 1);

} // namespace detail

// Which factorization the engine asks the backend for. kLDLT (symmetric
// indefinite) is the workhorse and the only kind implemented today;
// constructing with any other kind throws std::logic_error until its backend
// path lands.
//
// When kLLT and kLU do land, their inertia obligation is already settled:
// neither reports one, on either backend, so inertia() and every
// FactorizeOutcome they produce carry State::kUnavailable. Today that rule is
// unreachable rather than untrue -- no object of those kinds can be built.
enum class FactorKind { kLDLT, kLLT, kLU };

// What a factorization observed about the matrix's inertia, and how much of
// that observation is real.
//
// The counts are only meaningful when `state == kObserved`. The zero class is
// backend-dependent: MKL Pardiso reports the positive and negative counts and
// nothing else, so the zero count is DERIVED as dim - n_pos - n_neg and
// flagged with `zero_is_derived = true`; Accelerate reports all three
// natively. A backend whose inertia query ran and failed reports
// `kQueryFailed` with the counts left invalid -- distinguishable by
// construction from "there is genuinely a zero class", which is the
// distinction the drivers' regularization policy turns on.
struct InertiaEvidence {
    enum class State {
        kObserved,    // the counts below are valid
        kQueryFailed, // the query ran and failed -- counts are INVALID
        kUnavailable  // this backend / factor kind cannot report inertia
    };

    State state = State::kUnavailable;
    Index n_pos = -1;
    Index n_neg = -1;
    Index n_zero = -1;

    // True when n_zero was computed as dim - n_pos - n_neg rather than
    // reported by the backend (MKL Pardiso), so a consumer can tell a
    // measured zero class from an inferred one.
    bool zero_is_derived = false;

    // How many pivots the backend perturbed to get through the
    // factorization. Backend-qualified and OPTIONAL: present with Pardiso's
    // semantics on MKL, absent on Accelerate, which has no such counter.
    // Absence is the honest state and is never filled with a zero -- a zero
    // here means "the backend counted, and the answer was none".
    std::optional<Index> perturbed_pivots;
};

// The result of one numeric factorization. A backend error is reported here,
// not thrown: the caller decides whether a failed factorization is fatal or
// just means "regularize and try again".
struct FactorizeOutcome {
    enum class Status { kOk, kBackendError };

    Status status = Status::kBackendError;

    // The raw backend error code, 0 when status is kOk. Raw on purpose: it is
    // the only thing that survives a bug report intact.
    int backend_code = 0;

    // Evidence from this factorization. kQueryFailed here is NOT by itself a
    // factorize failure -- status carries that.
    InertiaEvidence inertia;
};

// What a solve reports about itself. Populated where the backend reports it;
// absent otherwise.
struct SolveInfo {
    // Iterative-refinement steps the backend actually performed.
    std::optional<Index> refinement_iters;
};

class Factorization;

// The sparse symmetric factor/solve engine.
//
// LIFECYCLE. analyze(A) runs the symbolic analysis and captures A's pattern
// hash as this instance's structural key. factorize(A) runs the numeric
// factorization into that existing symbolic -- it verifies the pattern hash
// and NEVER re-analyzes, so a value update on a fixed pattern costs exactly
// one numeric factorization. solve()/solve_partial() run against the current
// factorization, unlimited times, freely interleaved with other instances'
// work.
//
// SHARING. share() hands out a co-owning, read-only handle on the CURRENT
// factorization. It does not empty this engine and does not copy anything:
// the handle and this engine drive the same backend session. Staleness is
// detected rather than prevented, via the epoch -- see epoch(), share(), and
// adopt() below.
//
// THREAD SAFETY. Solves are not internally synchronized. An engine and any
// handles sharing its session are one backend session between them;
// concurrent solves across co-owners must be serialized by the caller.
class SymmetricFactor {
  public:
    // Every knob the engines actually consume. There is deliberately no raw
    // backend-parameter escape hatch: a need to reach an unlisted backend
    // parameter is a change to this surface, reviewed as such.
    struct Options {
        // Which factorization to run. Only kLDLT is implemented today.
        FactorKind kind = FactorKind::kLDLT;

        // Threads for this instance's backend calls. 0 leaves the backend's
        // own default alone. A positive value is applied at call scope and
        // undone afterward -- never by touching a process-global or
        // environment setting, which would leak into every other solver in
        // the process. Best-effort-absent on backends with no thread
        // control.
        int num_threads = 0;

        // Static pivot perturbation exponent k: a pivot too small to use is
        // replaced by one of magnitude ~10^-k relative to the matrix, and
        // counted in InertiaEvidence::perturbed_pivots.
        int pivot_perturb_exp = 8;

        // Maximum iterative-refinement steps for a full solve. Partial
        // solves always run with refinement off -- see solve_partial().
        int max_refinement_iters = 0;
    };

    // The three factors of a symmetric factorization, solved against
    // separately. Composing backward(diagonal(forward(rhs))) reproduces the
    // full solve -- but only when supports_partial_solve() is true.
    enum class SolvePhase { kForward, kDiagonal, kBackward };

    // Exact call counts, the currency tests and benchmarks assert on.
    //
    // Every counter below counts calls made THROUGH THIS ENGINE INSTANCE:
    // adopting a shared factorization starts a fresh set at zero, because
    // this instance did not do that work. All four count calls that reached
    // the backend AND RETURNED: a call rejected by validation (wrong
    // pattern, bad size, wrong order) never happened as far as the counters
    // are concerned, and neither did one that failed inside the backend and
    // threw.
    struct Counters {
        // +1 per completed symbolic analysis. factorize() never increments
        // it, which is what makes "the symbolic is reused" checkable rather
        // than merely claimed. Adopting a shared factorization does not
        // increment it either: no analysis was performed.
        Index analyze_count = 0;

        // +1 per factorize() call that reached the backend, whether the
        // numeric factorization succeeded or reported an error -- it counts
        // work done, not work that worked.
        Index factorize_count = 0;

        // +1 per solve() call that reached the backend, single- or
        // multi-RHS, regardless of the number of right-hand sides in it. A
        // multi-RHS solve with zero columns does no backend work and does
        // not increment.
        Index solve_count = 0;

        // +1 per solve_partial() call that reached the backend. Partial
        // solves are counted separately from full solves: they are a
        // different operation with a different cost, and composing three of
        // them is not one solve.
        Index partial_solve_count = 0;
    };

    explicit SymmetricFactor(Options opts);
    ~SymmetricFactor();

    // Move-only. Copying would silently produce a second driver of one
    // backend session, which is what share()/adopt() do explicitly and with
    // staleness detection.
    SymmetricFactor(const SymmetricFactor &) = delete;
    SymmetricFactor &operator=(const SymmetricFactor &) = delete;
    SymmetricFactor(SymmetricFactor &&) noexcept;
    SymmetricFactor &operator=(SymmetricFactor &&) noexcept;

    // --- lifecycle ---

    // Symbolic analysis. Captures A's pattern hash as this instance's
    // structural key, and starts a fresh backend session, so a previously
    // shared factorization is never disturbed by a re-analysis here.
    //
    // Throws std::invalid_argument unless A is compressed, square,
    // non-empty, stores only its upper triangle, and has every diagonal
    // entry present in its pattern (the backend requires the structural
    // diagonal even where the value is zero). Throws std::runtime_error if
    // the backend's symbolic phase itself fails; this instance keeps
    // whatever state it had.
    void analyze(const SpMatRM &A);

    // Numeric factorization into the EXISTING symbolic -- never re-analyzes.
    //
    // Throws std::runtime_error if no symbolic analysis is available (call
    // analyze() first, or adopt() a handle that carries one), and
    // std::invalid_argument if A's pattern hash differs from the analyzed
    // key: a different pattern is a contract violation, not a numeric
    // outcome, and the fix is an explicit analyze().
    //
    // A backend failure comes back in the returned outcome. It also
    // invalidates the current numerics: solves throw until a factorization
    // succeeds, and the epoch does not advance.
    FactorizeOutcome factorize(const SpMatRM &A);

    // --- solves ---

    // Multi-RHS solve: X's column j is the solution for RHS's column j. X
    // must already be sized to match RHS. A zero-column RHS is accepted and
    // is a no-op.
    SolveInfo solve(ConstMatRef RHS, MatRef X) const;

    // Single-RHS solve. A constrained template rather than a plain overload
    // -- see detail::VectorShaped above for why the obvious spelling cannot
    // be written in C++.
    //
    // Both overloads throw std::runtime_error when no usable factorization
    // exists (never factorized, the last factorization failed, or this
    // engine adopted a handle whose numerics are stale), and
    // std::invalid_argument on a size mismatch.
    template <class RhsT, class XT>
        requires detail::VectorShaped<RhsT> && detail::VectorShaped<XT>
    SolveInfo solve(const RhsT &rhs, XT &&x) const {
        // `x` is named, hence an lvalue here whatever XT deduced to, which
        // is what Eigen's writable Ref binds to.
        return solve_single(rhs, x);
    }

    // Solve against ONE factor of the factorization. The composition
    // backward(diagonal(forward(rhs))) equals the full solve only when
    // supports_partial_solve() is true -- when it is false the composition
    // can differ from the full solve by orders of magnitude with no error
    // raised anywhere, so a caller that skips the check gets a wrong answer
    // silently. This method does not check the predicate for you: the
    // predicate is the contract, and a caller that cannot use partial solves
    // is expected to fall back to a full solve, not to catch an exception.
    //
    // Refinement is forced off for the duration of every partial solve
    // regardless of Options::max_refinement_iters: the backend requires it,
    // and a refinement setting that leaked in here would corrupt the result.
    SolveInfo solve_partial(SolvePhase phase, ConstVecRef rhs, VecRef x) const;

    // True iff the CURRENT factorization's partial solves can be trusted to
    // compose to the full solve.
    //
    // False before the first successful factorization -- nothing composable
    // exists yet -- and false whenever this engine has no usable numerics.
    // On MKL it additionally requires that no pivot was perturbed: under
    // perturbation the composed partials diverge from the full solve
    // silently. On Accelerate there is no perturbation evidence at all, so
    // composability is unverifiable and the answer is a conservative false.
    //
    // Caveat carried from the observation this rule came from: composition
    // under active backend matching/scaling has not been exercised at scale.
    bool supports_partial_solve() const;

    // Evidence from the LAST factorization, as this engine is entitled to
    // see it: an engine that has not factorized -- including one that
    // adopted a handle whose numerics are stale -- reports kUnavailable
    // rather than describing a factorization that is not its to report.
    InertiaEvidence inertia() const;

    const Counters &counters() const;

    // --- shared handles ---

    // Package the CURRENT factorization as a co-owning, read-only handle.
    //
    // This engine is NOT emptied and keeps working: the handle and this
    // engine drive the same backend session, and the handle keeps that
    // session alive after this engine is destroyed. Nothing is copied.
    //
    // The consequence to design around: a handle's solve() reflects the
    // session's CURRENT numeric state, not a snapshot taken at emission. If
    // any co-owner refactorizes, the handle solves against the NEW numerics.
    // The handle's epoch is fixed at emission, so a consumer that needs
    // emission-time numerics compares handle->epoch() against the live
    // epoch() first and treats a mismatch as stale. Same for
    // Factorization::inertia(), which likewise reports the session's current
    // evidence.
    //
    // Throws std::runtime_error if there is no usable factorization to
    // share.
    std::shared_ptr<const Factorization> share();

    // The live committed epoch: 0 before the first successful factorization,
    // and incremented by every successful factorize() on this session --
    // including one driven by a co-owner. A FAILED factorize does not
    // advance it, so an epoch always names numerics that actually committed.
    // Re-analyzing carries the count forward rather than restarting it, so
    // an epoch value is never reused for different numerics.
    std::uint64_t epoch() const;

    // Build an engine on top of an existing shared factorization, validating
    // both structural and numeric currency and degrading rather than lying:
    //
    //   - epoch match: full reuse. Solves work immediately against the
    //     shared session, and a later factorize() reuses the symbolic with
    //     no re-analysis.
    //   - epoch MISMATCH (a co-owner has refactorized since the handle was
    //     emitted): symbolic-only reuse. The handle's numerics are stale, so
    //     solves through this engine are REFUSED with a std::runtime_error
    //     explaining the staleness -- but the symbolic is still good, so the
    //     first factorize() reuses it with no re-analysis and clears the
    //     refusal.
    //   - pattern mismatch: the structural key travels with the handle and
    //     is checked at the first factorize(), which throws
    //     std::invalid_argument for a different pattern. The recovery is an
    //     explicit analyze(), from which point the handle contributes
    //     nothing.
    //
    // The adopting engine inherits the emitting engine's Options: it is
    // driving that engine's session, and the session's configuration is
    // already baked into the factorization it is adopting.
    //
    // Throws std::invalid_argument if the handle is null.
    static SymmetricFactor adopt(std::shared_ptr<const Factorization> handle);

  private:
    SolveInfo solve_single(ConstVecRef rhs, VecRef x) const;

    // Throws unless this engine may solve against the session right now,
    // distinguishing "nothing factorized" from "adopted numerics are stale".
    void require_solvable(const char *what) const;

    // True iff this engine is entitled to the session's current numerics.
    bool has_usable_numerics() const;

    Options opts_;
    std::shared_ptr<detail::PardisoSession> session_;
    std::uint64_t pattern_hash_ = 0;
    bool has_pattern_ = false;

    // Set only by adopt() on an epoch mismatch: the symbolic is reusable but
    // the numerics belong to a factorization this engine never saw. Cleared
    // by this engine's first successful factorize().
    bool numerics_refused_ = false;
    std::uint64_t refused_epoch_ = 0;

    mutable Counters counters_;
};

// A shared, read-only view of a factorization, co-owning the backend session
// it was emitted from.
//
// It stays valid after the emitting engine is destroyed or idle; the session
// is released when the last co-owner goes away. Its solves reflect the
// session's CURRENT numeric state (see SymmetricFactor::share()), while its
// pattern_hash() and epoch() are fixed at emission -- that pairing is what
// makes staleness detectable.
//
// Thread-safety: solves on one Factorization are not internally
// synchronized; concurrent use requires external serialization.
class Factorization {
  private:
    // Construction is SymmetricFactor::share()'s privilege; the tag makes
    // the constructor unreachable elsewhere while staying usable with
    // std::make_shared.
    struct PrivateTag {
        explicit PrivateTag() = default;
    };
    friend class SymmetricFactor;

  public:
    Factorization(PrivateTag, std::shared_ptr<detail::PardisoSession> session,
                  std::uint64_t pattern_hash, std::uint64_t epoch);
    ~Factorization();

    Factorization(const Factorization &) = delete;
    Factorization &operator=(const Factorization &) = delete;

    // Multi-RHS and single-RHS solves, with the same shapes, errors, and
    // ambiguity workaround as SymmetricFactor's own.
    SolveInfo solve(ConstMatRef RHS, MatRef X) const;

    template <class RhsT, class XT>
        requires detail::VectorShaped<RhsT> && detail::VectorShaped<XT>
    SolveInfo solve(const RhsT &rhs, XT &&x) const {
        // `x` is named, hence an lvalue here whatever XT deduced to, which
        // is what Eigen's writable Ref binds to.
        return solve_single(rhs, x);
    }

    // Evidence for the session's CURRENT factorization -- live, matching
    // what solve() would use, not a snapshot taken at emission.
    InertiaEvidence inertia() const;

    // The structural key this factorization was built under, fixed at
    // emission.
    std::uint64_t pattern_hash() const;

    // The emitting engine's committed epoch at emission, fixed.
    std::uint64_t epoch() const;

  private:
    SolveInfo solve_single(ConstVecRef rhs, VecRef x) const;

    std::shared_ptr<detail::PardisoSession> session_;
    std::uint64_t pattern_hash_ = 0;
    std::uint64_t epoch_ = 0;
};

} // namespace hven::linear
