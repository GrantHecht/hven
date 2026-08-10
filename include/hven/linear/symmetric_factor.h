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

// The backend factorization session (Pardiso's phase-driven handle on MKL
// platforms; Accelerate's symbolic/numeric factorization pair on Apple).
// Deliberately incomplete here: no backend type reaches this header, so
// consumers never need MKL's or Accelerate's headers on their include path.
// The name is backend-NEUTRAL on purpose: exactly one concrete definition of
// this type is ever compiled into a given build (src/linear/ picks the
// platform's implementation the same way DenseSymmetricFactor's backend split
// does), so both backends' session headers define a class with this exact
// name rather than a backend-qualified one that would leak here.
class FactorSession;

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

// What a factorization reports about its own size and cost. Per-backend
// semantics are STRUCTURALLY different, not merely differently named or
// scaled -- MKL Pardiso reports an entry COUNT and (opt-in) a cost
// estimate; Accelerate reports a BYTE size and no cost estimate at all --
// so no field here is ever populated with more than one backend's meaning,
// and a backend with no real observable for a field leaves it
// std::nullopt. Same evidence-honesty discipline InertiaEvidence follows:
// absent is never zero-filled into a plausible-looking number.
//
// Lives on FactorizeOutcome, alongside InertiaEvidence, because both are
// read at the SAME point in the lifecycle -- right after a successful
// numeric factorization -- and neither exists yet on a path where no
// backend factorize() has run. A solve does not refresh this evidence: it
// describes the factorization solves run against, not the solve itself.
struct FactorEvidence {
    // MKL Pardiso only: the number of nonzero entries in the LDLT factor
    // (iparm[17]). ALWAYS collected after a successful MKL factorization --
    // no Options field gates this one, because Intel documents no cost for
    // requesting it and pardisoinit's own default already requests it (see
    // this field's fuller treatment in pardiso_session.cpp). std::nullopt
    // on Accelerate, which reports a byte size instead (below), never an
    // entry count.
    std::optional<Index> factor_nonzeros;

    // MKL Pardiso only: the estimated factorization cost in Mflops
    // (iparm[18]), collected only when Options::collect_factor_mflops is
    // set -- see that option's own doc comment for why it stays opt-in
    // where factor_nonzeros does not. std::nullopt on Accelerate (no
    // equivalent estimate exists) and std::nullopt on MKL too when the
    // option is off.
    std::optional<Index> factor_mflops;

    // Accelerate only: the symbolic factorization's own reported factor
    // size, in BYTES. ALWAYS collected after a successful Accelerate
    // factorization -- Accelerate computes this value as part of the
    // symbolic factorization regardless of any hven option, so there is
    // nothing to gate. A structurally different quantity from
    // factor_nonzeros above -- a byte size, not an entry count -- so it is
    // never folded into that field under a shared name. std::nullopt on
    // MKL. Accelerate reports no factorization-cost estimate at all, so
    // there is no Accelerate counterpart to factor_mflops, not even an
    // absent one -- the frozen contract's ABSENT-never-zero rule applies to
    // a field that could exist and currently does not report, not to a
    // field the backend has no concept of whatsoever.
    std::optional<Index> factor_size_bytes;
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

    // Factor-size and factor-cost evidence from this SAME factorization --
    // see FactorEvidence's own doc comment for why it lives here rather
    // than on SolveInfo. Default-constructed (every field absent) on a
    // kBackendError outcome, exactly like inertia.
    FactorEvidence factor;
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
// detected rather than prevented, via the session id and the epoch -- see
// session_id(), epoch(), share(), and adopt() below.
//
// IDENTITY. One set of numerics is named by the TRIPLE (pattern_hash,
// session_id, epoch), and it takes all three. The pattern hash says which
// structure was factorized; the session id says which backend session
// produced the numbers; the epoch says which of that session's successive
// factorizations they came from. Two of the three are not enough: analyze()
// FORKS -- it starts a fresh session while any outstanding handle keeps the
// old one alive and solvable, and the fresh session's epoch sequence
// CONTINUES the old one's rather than restarting it -- so both branches can
// go on committing numerics and reach the same epoch on the same pattern.
// The session ids differ, which is what keeps the two branches
// distinguishable. A consumer keying a cache or a warm start on a
// factorization must key it on the triple.
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

        // Fill-in reordering for the symbolic analysis (Pardiso iparm[1] on
        // MKL; Accelerate's own ordering-method control on the other
        // backend). DON'T-WRITE-BY-DEFAULT ON MKL: at kBackendDefault, hven
        // does not touch iparm[1] at all, so pardisoinit's own value
        // survives exactly.
        //
        // Backend-neutral mapping -- both backends support every value;
        // there is no Pardiso-only throw for this option:
        //   kBackendDefault            MKL: don't write iparm[1]
        //                              Accelerate: SparseOrderDefault
        //                              (documented by Apple as AMD for
        //                              symmetric matrices)
        //   kMinimumDegree             MKL: iparm[1] = 0
        //                              Accelerate: SparseOrderAMD
        //   kNestedDissection          MKL: iparm[1] = 2 (METIS)
        //                              Accelerate: SparseOrderMetis
        //   kParallelNestedDissection  MKL: iparm[1] = 3 (its OpenMP-
        //                              parallel variant)
        //                              Accelerate: SparseOrderMTMetis,
        //                              downgraded to SparseOrderMetis at
        //                              runtime on a host that lacks it
        //                              (macOS < 26 -- passing the
        //                              unsupported value unconditionally
        //                              raises a backend parameter error)
        //
        // MIGRATION HAZARD: a caller migrating from a seam whose Accelerate
        // default was METIS must request kNestedDissection explicitly --
        // Accelerate's OWN default is AMD, so leaving this at
        // kBackendDefault silently changes which ordering method runs, not
        // merely its cost.
        enum class Ordering {
            kBackendDefault,
            kMinimumDegree,
            kNestedDissection,
            kParallelNestedDissection
        };
        Ordering ordering = Ordering::kBackendDefault;

        // Maximum weighted matching (Pardiso iparm[12]). DON'T-WRITE-BY-
        // DEFAULT: `false` does NOT write iparm[12] = 0, it leaves the entry
        // untouched; only `true` writes iparm[12] = 1. Pardiso-only: `true`
        // THROWS std::invalid_argument at construction on the Accelerate
        // backend, which has no equivalent concept. On MKL, setting this
        // true also forces supports_partial_solve() to false for the
        // lifetime of this engine's factorizations -- see that method's own
        // doc comment: composition under active matching is unexercised at
        // scale, and the predicate's design law is conservative, never a
        // fabricated true.
        bool weighted_matching = false;

        // Matrix scaling for the numeric factorization (Pardiso iparm[10],
        // Intel's own comment: "Use nonsymmetric permutation and scaling
        // MPS"). DON'T-WRITE-BY-DEFAULT: `false` does NOT write iparm[10] =
        // 0, it leaves the entry untouched; only `true` writes iparm[10] =
        // 1. Pardiso-only: `true` THROWS std::invalid_argument at
        // construction on Accelerate. This is the SAME judgment as
        // weighted_matching above, not a separate one: iparm[10]'s MPS
        // scaling is computed FROM the maximum-weighted-matching
        // permutation that option switches on, so a backend with no
        // matching concept has no scaling concept to pair it with either --
        // Accelerate's own numeric-factorization scaling (inf-norm
        // equilibration, on unconditionally, no off switch) is a different
        // mechanism entirely, not a lower-fidelity version of this one.
        bool matrix_scaling = false;

        // Pivoting strategy for symmetric indefinite matrices (Pardiso
        // iparm[20]). DON'T-WRITE-BY-DEFAULT: kBackendDefault leaves
        // iparm[20] untouched -- pardisoinit's own value survives -- the
        // same shape as `ordering` above and for the identical reason: on
        // the MKL version this was written against, pardisoinit's own
        // default for this entry already equals kTwoByTwo's value, so
        // naming kTwoByTwo explicitly is how a caller PINS the choice
        // against version drift rather than a behavioral no-op.
        //
        // The four named values are EXACTLY Pardiso's own documented
        // iparm[20] codes (Intel's oneMKL Developer Reference, "pardiso
        // iparm Parameter", the iparm[20] entry) -- no others are
        // documented, so no others are exposed here:
        //   kOneByOne              iparm[20] = 0  "Apply 1x1 diagonal
        //                          pivoting during the factorization
        //                          process."
        //   kTwoByTwo              iparm[20] = 1  "Apply 1x1 and 2x2
        //                          Bunch-Kaufman pivoting during the
        //                          factorization process." (Pardiso's own
        //                          documented default, and the value
        //                          psiopt writes today.)
        //   kOneByOneNoAutoRefine  iparm[20] = 2  Same as kOneByOne,
        //                          "except that the solve step does not
        //                          automatically make iterative
        //                          refinements when perturbed pivots are
        //                          obtained."
        //   kTwoByTwoNoAutoRefine  iparm[20] = 3  Same as kTwoByTwo, with
        //                          the identical no-auto-refine exception.
        //
        // Pardiso-only: any non-default value THROWS std::invalid_argument
        // at construction on Accelerate, which exposes no equivalent
        // pivoting-strategy selector -- its LDLT factorization's pivoting
        // scheme is fixed by the TPP factorization kind this class always
        // requests, with no per-call override.
        enum class PivotStrategy {
            kBackendDefault,
            kOneByOne,
            kTwoByTwo,
            kOneByOneNoAutoRefine,
            kTwoByTwoNoAutoRefine,
        };
        PivotStrategy pivot_strategy = PivotStrategy::kBackendDefault;

        // The two-level factorization algorithm (Pardiso iparm[23]).
        // DON'T-WRITE-BY-DEFAULT: kBackendDefault leaves iparm[23]
        // untouched. kClassic writes iparm[23] = 0 (Pardiso's own classic
        // algorithm, which coincides with pardisoinit's own default on
        // every MKL version observed -- naming it explicitly still PINS the
        // choice, the same rationale as `ordering` and `pivot_strategy`
        // above); kTwoLevel writes iparm[23] = 1. Pardiso-only: any
        // non-default value THROWS std::invalid_argument at construction on
        // Accelerate, which has no two-level factorization concept at all.
        //
        // kTwoLevel carries two REQUIRED interactions with other options,
        // both documented by Intel (oneMKL Developer Reference, "pardiso
        // iparm Parameter", the iparm[23] entry) and both VALIDATED at
        // construction (throwing std::invalid_argument naming the
        // conflict) rather than left for Pardiso to silently ignore:
        //   - "If a two-level factorization algorithm is chosen (that is,
        //     iparm[23]=1), then only nested dissection algorithms are
        //     available (iparm[1]=2 or iparm[1]=3)." -- `ordering` must be
        //     kNestedDissection or kParallelNestedDissection; kBackendDefault
        //     and kMinimumDegree both throw.
        //   - "Disable iparm[10] (scaling) and iparm[12]=1 (matching) when
        //     using the two-level factorization algorithm." -- both
        //     `matrix_scaling` and `weighted_matching` must be false.
        enum class FactorizationAlgorithm { kBackendDefault, kClassic, kTwoLevel };
        FactorizationAlgorithm factorization_algorithm = FactorizationAlgorithm::kBackendDefault;

        // Parallel forward/backward solve control (Pardiso iparm[24]).
        // DON'T-WRITE-BY-DEFAULT: kBackendDefault leaves iparm[24]
        // untouched. Every other value WRITES iparm[24] explicitly --
        // including kAdaptivePartitioning, which pins Pardiso's own
        // default strategy against version drift the same way `ordering`'s
        // non-default values do, and is the value psiopt writes today.
        //
        // The three non-default values are EXACTLY Pardiso's own documented
        // iparm[24] codes (Intel's oneMKL Developer Reference, "pardiso
        // iparm Parameter", the iparm[24] entry):
        //   kAdaptivePartitioning       iparm[24] = 0  "In the case of the
        //                               one right-hand side, the
        //                               parallelization will be performed
        //                               by partitioning the matrix.
        //                               Otherwise, the parallelization
        //                               will be over the right-hand
        //                               sides." (Pardiso's own documented
        //                               default.)
        //   kSequential                 iparm[24] = 1  "Intel oneMKL
        //                               PARDISO uses the sequential
        //                               forward and backward solve."
        //   kMatrixPartitionParallel    iparm[24] = 2  "Independent from
        //                               the number of the right-hand
        //                               sides, Intel oneMKL PARDISO uses
        //                               the parallel algorithm based on
        //                               the matrix partitioning."
        //
        // NAMING NOTE: an earlier revision of this option was a bare
        // `bool parallel_solve` that wrote iparm[24] = 1 for `true` --
        // exactly backwards, since 1 is the SEQUENTIAL code and 0 (the
        // default) is Pardiso's own parallel strategy. This enum exists
        // instead of a corrected bool specifically so the value names say
        // what they do rather than relying on a reader to remember which
        // way a boolean points.
        //
        // Pardiso-only: any non-default value THROWS std::invalid_argument
        // at construction on Accelerate, which has no per-instance thread
        // control of any kind for this option to parallelize a solve with
        // (see num_threads' own doc comment on that backend's
        // best-effort-absent treatment) -- a silently-inert non-default
        // value would defeat the point of naming the option at all, exactly
        // the situation weighted_matching's throw exists to prevent.
        enum class SolveParallelism {
            kBackendDefault,
            kAdaptivePartitioning,
            kSequential,
            kMatrixPartitionParallel,
        };
        SolveParallelism solve_parallelism = SolveParallelism::kBackendDefault;

        // Thread count for conditional numerical reproducibility (CNR)
        // mode (Pardiso iparm[33]). 0 (default) leaves iparm[33] untouched
        // -- CNR mode off, matching Pardiso's own default; a positive value
        // WRITES iparm[33] to that count and turns CNR mode on, trading
        // some performance for a bitwise-reproducible reduction order
        // across runs using that many threads.
        //
        // REQUIRED interaction with `ordering`, documented by Intel
        // (oneMKL Developer Reference, "pardiso iparm Parameter", the
        // iparm[33] entry) and VALIDATED at construction rather than left
        // for Pardiso to silently fail to reproduce: "CNR is only
        // available for the in-core version of Intel oneMKL PARDISO and
        // the non-parallel version of the nested dissection algorithm...
        // not set iparm[1] to 3 in order to not use the parallel version
        // of the nested dissection algorithm. Otherwise Intel oneMKL
        // PARDISO does not produce numerically repeatable results even if
        // CNR is enabled." Read literally -- CNR's positive requirement
        // names nested dissection specifically, not minimum degree -- a
        // positive `cnr_threads` REQUIRES `ordering ==
        // Ordering::kNestedDissection` exactly; every other Ordering value,
        // INCLUDING kBackendDefault (whose value floats with the linked
        // MKL -- see `ordering`'s own doc comment -- and is 3, the
        // documented-incompatible one, on the MKL this was verified
        // against) and kMinimumDegree (not named by the positive
        // requirement clause), THROWS std::invalid_argument naming the
        // conflict.
        //
        // Pardiso-only: a positive value THROWS std::invalid_argument at
        // construction on Accelerate, which has no CNR concept. Unlike
        // num_threads, this is not a plain thread count Accelerate could
        // best-effort-ignore: CNR is a reproducibility GUARANTEE, and
        // silently dropping it would let a caller believe it still held.
        int cnr_threads = 0;

        // Whether to collect and report the factorization's Mflop-cost
        // estimate (Pardiso iparm[18]) as FactorizeOutcome::factor
        // .factor_mflops. DON'T-WRITE-BY-DEFAULT: `false` leaves iparm[18]
        // untouched; `true` writes iparm[18] = -1, Pardiso's own request
        // code. Opt-in rather than unconditional because Intel documents a
        // real cost: "Enable report if iparm[18] < 0 on entry. This
        // increases the reordering time." (oneMKL Developer Reference,
        // "pardiso iparm Parameter", the iparm[18] entry). Contrast
        // FactorizeOutcome::factor.factor_nonzeros (iparm[17]), which
        // carries NO such cost warning and is always collected -- see that
        // field's own doc comment for why it needs no option at all.
        //
        // KNOWN VERSION-DEPENDENT CAVEAT, recorded rather than silently
        // relied on: on the MKL this was verified against (oneAPI MKL
        // 2026.1), pardisoinit's OWN sample initialization already sets
        // iparm[18] = -1 for mtype = -2 -- the identical value this option
        // would write for `true` -- even though Intel's own iparm[18]
        // table marks ">= 0" (disabled) as the default. This is the same
        // kind of pardisoinit-vs-documented-default coincidence `ordering`
        // and `factorization_algorithm` already document for their own
        // entries, carried here because it means `false` (don't write) may
        // NOT actually avoid the documented time cost on every linked MKL
        // version -- only an explicit MKL-version audit of pardisoinit's
        // own array can confirm which state a given build is actually in.
        // hven does not force-write a canceling value here: doing so would
        // break this option's don't-write-by-default shape (identical to
        // every sibling option in this struct) to chase a guarantee this
        // project cannot make ("no cost by default") given a backend whose
        // own sample initializer already disagrees with its own
        // documentation on this one entry.
        bool collect_factor_mflops = false;

        // An explicit override for Accelerate's zero-pivot threshold
        // (SparseNumericFactorOptions::zeroTolerance), bypassing the
        // pivot_perturb_exp-derived formula documented on that field
        // entirely. std::nullopt (default) uses that formula; a present
        // value is passed to Accelerate verbatim and must be > 0.
        // Accelerate-only: a present value THROWS std::invalid_argument at
        // construction on MKL, which has no zeroTolerance concept of its
        // own -- pivot_perturb_exp's MKL meaning (a perturbation EXPONENT
        // feeding iparm[9]'s relative formula) is not the same knob as this
        // one (an absolute THRESHOLD), so a value set here cannot silently
        // fold into pivot_perturb_exp's MKL behavior instead.
        std::optional<double> accelerate_zero_tolerance;
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
    // On MKL it ALSO requires Options::weighted_matching == false: composition
    // under active matching has not been exercised at scale (the caveat this
    // rule was originally carried under, now enforced rather than merely
    // documented, since weighted_matching is the option that first makes
    // matching reachable through this surface). This predicate's design law
    // is conservative-never-fabricated-true -- a future tuning program may
    // relax the conjunct, but only with evidence backing that decision.
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
    // The handle's session id and epoch are fixed at emission, so a consumer
    // that needs emission-time numerics compares handle->session_id() and
    // handle->epoch() against the live session_id() and epoch() first and
    // treats any difference as stale. Same for Factorization::inertia(),
    // which likewise reports the session's current evidence.
    //
    // Throws std::runtime_error if there is no usable factorization to
    // share.
    std::shared_ptr<const Factorization> share();

    // The live committed epoch: 0 before the first successful factorization,
    // and incremented by every successful factorize() on this session --
    // including one driven by a co-owner. A FAILED factorize does not
    // advance it, so an epoch always names numerics that actually committed.
    // Re-analyzing carries the count forward rather than restarting it, so
    // an epoch never runs backwards.
    //
    // AN EPOCH IS NOT AN IDENTITY ON ITS OWN. It is unique within one
    // session; re-analysis forks (see the IDENTITY note on this class), and
    // two branches of a fork can commit different numerics at the same epoch
    // on the same pattern. Pair it with session_id().
    std::uint64_t epoch() const;

    // The process-unique id of the backend session this engine currently
    // drives; 0 before the first analyze() and before adopting a handle.
    // Fixed for the life of the session: analyze() moves this engine onto a
    // NEW session (and a new id), adopt() moves it onto the emitter's, and
    // nothing else changes it. This is the conjunct that makes an epoch an
    // identity rather than a per-branch counter.
    //
    // Not stable across processes -- it names a session that exists, so it
    // is never written down and read back. The pattern hash is the part of
    // the triple that survives a process boundary.
    std::uint64_t session_id() const;

    // Build an engine on top of an existing shared factorization, validating
    // both structural and numeric currency and degrading rather than lying.
    // Numeric reuse takes the whole identity triple -- same session AND same
    // epoch AND, at the first factorize(), the same pattern:
    //
    //   - session and epoch both match: full reuse. Solves work immediately
    //     against the shared session, and a later factorize() reuses the
    //     symbolic with no re-analysis.
    //   - session or epoch MISMATCH (a co-owner has refactorized since the
    //     handle was emitted): symbolic-only reuse. The handle's numerics are
    //     stale, so solves through this engine are REFUSED with a
    //     std::runtime_error explaining the staleness -- but the symbolic is
    //     still good, so the first factorize() reuses it with no re-analysis
    //     and clears the refusal.
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
    std::shared_ptr<detail::FactorSession> session_;
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
// pattern_hash(), session_id() and epoch() -- the identity triple -- are
// fixed at emission. That pairing is what makes staleness detectable.
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
    Factorization(PrivateTag, std::shared_ptr<detail::FactorSession> session,
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

    // The id of the session this handle co-owns. Fixed by construction
    // rather than merely recorded: a handle co-owns exactly the session it
    // was emitted from and never rebinds, and a session's id never changes,
    // so the live value and the emission-time value are the same value.
    // Together with pattern_hash() and epoch() it is the identity triple --
    // see the IDENTITY note on SymmetricFactor.
    std::uint64_t session_id() const;

    // The emitting engine's committed epoch at emission, fixed. Meaningful
    // as an identity only alongside session_id(): a co-owner's re-analysis
    // forks the session, and the fork's epochs continue this one's.
    std::uint64_t epoch() const;

  private:
    SolveInfo solve_single(ConstVecRef rhs, VecRef x) const;

    std::shared_ptr<detail::FactorSession> session_;
    std::uint64_t pattern_hash_ = 0;
    std::uint64_t epoch_ = 0;
};

} // namespace hven::linear
