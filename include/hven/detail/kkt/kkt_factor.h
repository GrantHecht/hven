// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include <Eigen/SparseCore>

#include <hven/core/pattern_hash.h>
#include <hven/detail/core/inf_norm.h>
#include <hven/detail/kkt/dense_selection.h>
#include <hven/linear/symmetric_factor.h>
#include <hven/qp/qp_types.h>

namespace hven::detail {

/// @brief The SQP engine's linear configuration for its KKT factors.
///
/// `threads` is the count SqpOptions::common.threads carries, handed down by
/// whichever engine owns the factor (M6 W5 T8.8). 0 is the value every caller
/// passed before that task and means "leave the backend's own default alone"
/// -- so (0, kNever) is byte-for-byte the configuration this factory has
/// always produced, and the process-wide MKL_NUM_THREADS pin stays the
/// reproducibility mechanism at 0. A positive
/// count is applied by SymmetricFactor at each backend call and undone
/// afterward (symmetric_factor.h); it is never written to a process global.
///
/// Accelerate stores the count and applies it to nothing -- UNOBSERVED, and
/// the SQP deliberately does NOT mirror the IPM's process-wide
/// accelerate_set_num_threads() call, which is not restored on exit.
///
/// `dense` is SqpOptions::linear.dense_factorization (M8 DN-1); kNever is the
/// configuration this factory produced before the dense arm. Neither
/// parameter has a default, so every caller states its policy: a factor that
/// stays on the platform session at kAlways (the warm-start predictor's)
/// says kNever where it is built. Under a dense policy the factor reports
/// singular factorizations (DenseSingularPolicy::kReport) and
/// factorize_checked()'s site overloads respond. Under kAuto the threshold is
/// the SQP's (kSqpDenseSelectionThreshold), set here and only here, so every
/// factor this factory configures selects its arm by its own pattern's order.
inline hven::linear::SymmetricFactor::Options sqp_kkt_options(int threads,
                                                              hven::linear::DensePolicy dense) {
    hven::linear::SymmetricFactor::Options o;
    o.decomposition = hven::linear::Decomposition::kLdlt;
    o.threads = threads;
    o.dense_factorization = dense;
    o.max_dense_dimension = dense_selection_threshold(dense, kSqpDenseSelectionThreshold);
    if (dense != hven::linear::DensePolicy::kNever) {
        o.dense_singular_policy = hven::linear::DenseSingularPolicy::kReport;
    }
    o.pivot_perturbation_exponent = std::nullopt;
    o.max_refinement_steps = std::nullopt;
    // Every remaining member keeps its default (don't-write / absent).
    return o;
}

// The SQP engine's lifecycle state around a sparse symmetric factor.
//
// INVARIANT: `factorize_checked()` must be the only thing that ever drives an
// analysis on `factor`. `analyzed`/`analyzed_pattern` mirror state that
// `SymmetricFactor` keeps privately and exposes through no getter, so the
// mirror is correct only while nothing else touches the factor's symbolic
// state behind this struct's back:
//   - Calling `factor.analyze(K)` directly leaves the mirror stale, so the
//     next `factorize_checked()` runs a redundant second symbolic analysis
//     for one logical change.
//   - Installing a `SymmetricFactor::adopt()`-built factor leaves
//     `analyzed == false` even though the adopted factor may already carry a
//     reusable symbolic, so the first `factorize_checked()` forks a new
//     session, moving `session_id()` and breaking qp_engine.h's hot-start
//     reuse condition (e) for every other holder of that handle.
/// Which session holds a KktFactor's current numerics (THE PLATFORM-SOLVE
/// RESPONSE, below).
enum class PlatformRetake : std::uint8_t {
    kNone,    ///< the platform session's own (or the dense arm's)
    kScaled,  ///< D K D on the scaled session, D = symmetric_matching_scaling(K)
    kMatched, ///< K on the weighted-matching session (Pardiso iparm[12] = 1)
};

struct KktFactor {
    /// @param threads The owning engine's thread count; 0 (what every
    ///                pre-T8.8 construction site passed) leaves the
    ///                backend's own default alone. THE SOLE WAY a thread count
    ///                reaches a walk/SSN-tier factor: `factor` is configured
    ///                once, here, through sqp_kkt_options(), and no library
    ///                code default-constructs a KktFactor any more. The
    ///                checkable form of that rule (M6 W5 T8.8 fix1 -- the
    ///                earlier wording here claimed a grep that CANNOT be
    ///                empty): `grep -rn 'KktFactor \w*;' src/` has no match, so
    ///                no local or temporary is default-constructed, and the two
    ///                matches the same grep makes over `include/` are MEMBER
    ///                DECLARATIONS -- qp_engine.h's `BorderState::kkt` and
    ///                ssn_engine.h's `SsnEngine::kkt_` -- each initialized with
    ///                its owner's count in that owner's mem-initializer. That
    ///                is what makes "every factor path" a checkable claim
    ///                rather than an enumerated one.
    /// @param dense   The owning engine's dense-arm policy (M8 DN-1), passed
    ///                beside `threads` at every construction site the same way.
    ///                No default for either: a construction site that leaves a
    ///                factor on the platform session says kNever.
    explicit KktFactor(int threads, hven::linear::DensePolicy dense)
        : factor(sqp_kkt_options(threads, dense)) {}

    hven::linear::SymmetricFactor factor;
    std::uint64_t analyzed_pattern = 0;
    bool analyzed = false;
    /// True iff the current numerics are a dense shift response:
    /// a solve then answers a shifted system, so refinement against K is
    /// kept only while its residual falls, and `dense_scale` holds the
    /// equilibration D the residual is measured in (empty otherwise).
    bool dense_shifted = false;
    Vec dense_scale;
    /// Equilibrated attempts that came out exactly singular (the factor counts
    /// them in dense_singular_count); dense_response_tally() takes them back
    /// out, so its `flagged` counts unshifted factorizations only.
    Index shifted_exact_singular = 0;

    // THE PLATFORM-SOLVE RESPONSE's state (see that section below).
    /// The session holding the current numerics; solve_vec() and kkt_inertia()
    /// read it.
    PlatformRetake numerics = PlatformRetake::kNone;
    /// The retake the NEXT factorization takes if the platform's own outcome
    /// warrants it; consumed (reset to kNone) when that factorization starts,
    /// whether it then succeeds, fails or throws. Set only on a Schur stack's
    /// K0 factor (the bordered path cannot retake in place: C was formed through
    /// K0's numerics), and it applies to the NEXT K0, whatever working rows that
    /// rebuild carries -- see THE PLATFORM-SOLVE RESPONSE, THE BORDER PATH.
    PlatformRetake requested = PlatformRetake::kNone;
    /// True iff the platform solve check applies to the current numerics: the
    /// platform session's own sparse numerics where the backend reported
    /// perturbed pivots (or reports no count at all), and kScaled numerics a
    /// retake left armed. A failed retake disarms it, which is what bounds the
    /// response.
    bool check_armed = false;
    /// The perturbed-pivot report of the current kScaled numerics (absent on
    /// a backend that reports none): the escalation to kMatched applies only
    /// where it is present and positive.
    std::optional<Index> scaled_perturbed_pivots;
    /// True from a request_platform_retake() until the next factorization
    /// starts (or clear_platform_retake_request()): a holder of numerics taken
    /// before the request (a Schur stack's K0) refactors.
    bool rebuild_pending = false;
    /// D for kScaled numerics (empty otherwise): the solve is D (D K D)^-1 D.
    Vec retake_scale;
    /// The retake sessions, built on first use. release_idle_retake_sessions()
    /// frees the ones the current numerics do not live in (the walk calls it at
    /// the end of every QP solve).
    std::unique_ptr<hven::linear::SymmetricFactor> scaled_session;
    std::unique_ptr<hven::linear::SymmetricFactor> matched_session;
    /// The pattern the scaled session was last analyzed on: D K D has K's
    /// pattern and the plain session's analysis is value-independent (no
    /// matching, no scaling), so a scaled retake re-analyzes only when K's
    /// pattern moves. The matched session's analysis IS value-dependent (the
    /// weighted matching) and is retaken with every matched retake.
    std::uint64_t scaled_analyzed_pattern = 0;
    bool scaled_analyzed = false;
    /// Cumulative retake FACTORIZATIONS run on each session, taken or not (a
    /// throw before the factorization runs is not one): none of them is in any
    /// engine's `factorizations`. QpCounters::sparse_scaled_retakes /
    /// sparse_matched_retakes are their deltas.
    Index scaled_retakes = 0;
    Index matched_retakes = 0;
};

// The analyze-or-not decision for K, carrying the pattern hash it was taken
// on so nothing downstream has to recompute it.
//
// `needed` is exactly what needs_analysis() reports. `pattern` holds
// hven::pattern_hash(K) WHEN ONE WAS COMPUTED, and is disengaged on the
// short-circuit path: a factor with `analyzed == false` needs an analysis
// whatever K's pattern is, so K is not hashed to decide that. Preserving
// that short-circuit keeps the throw site where it belongs -- pattern_hash()
// throws on an uncompressed matrix -- and threading this decision through
// the call sites keeps steady state at the floor of two O(nnz) hashes per
// SSN major (this decision plus SymmetricFactor::factorize()'s own pattern
// guard, which is hven::linear's published contract and not ours to remove).
struct AnalysisDecision {
    bool needed = false;
    std::optional<std::uint64_t> pattern;
};

/// @brief The analyze-or-not decision for K, with the hash it was taken on
/// kept for the caller to hand back to factorize_checked(). The pair
/// `analysis_decision()` + `factorize_checked(k, K, decision)` is the form
/// every call site that counts symbolic_analyses should use.
AnalysisDecision analysis_decision(const KktFactor &k, const SpMatRM &K);

/// @brief True iff factorize_checked() would run an analysis for K. SQP call
/// sites consult this before factorize_checked() to preserve their
/// symbolic_analyses counting contract. It is `analysis_decision(k, K).needed`
/// and cannot disagree with it -- one is implemented in terms of the other.
bool needs_analysis(const KktFactor &k, const SpMatRM &K);

/// @brief Analyze iff the pattern changed, then factorize.
/// @throws hven::BackendError As the three-argument overload.
hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatRM &K);

/// @brief The same, on a decision the caller has already taken -- which is what
/// keeps the pattern from being hashed twice for one factorization.
///
/// The decision MUST be the one `analysis_decision()` returned for this same
/// `k` and this same K's pattern; handing back a stale decision would analyze
/// (or skip analyzing) against the wrong pattern.
/// @throws hven::BackendError If the outcome is
/// FactorizeOutcome::Status::kBackendError; every other status is returned.
hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatRM &K,
                                                 const AnalysisDecision &decision);

/// @brief A KKT site's layout for the dense singular response: the first
/// `n_primal` rows are primal (shifted by +sigma), the rest dual (-sigma).
struct DenseShiftSite {
    Index n_primal = 0;
};

/// The response's shift on the equilibrated matrix, and the one retry's growth.
inline constexpr double kDenseEquilibratedShift = 0x1p-30;
inline constexpr double kDenseShiftRetryGrowth = 256.0;

/// @brief Symmetric Ruiz equilibration of an upper-CSR K (infinity norm): at
/// most 8 sweeps of d_i /= sqrt(r_i), r_i = max_j |K_ij| d_i d_j (rows with r_i
/// zero or non-finite keep d_i), stopping once max|1 - r_i| <= 0.05. The dense
/// detector's scale (hven::linear::detail::ruiz_symmetric_scale), so the
/// response shifts the matrix the detector judged singular.
Vec equilibration_scale(const SpMatRM &K);

/// @brief factorize_checked() with the dense singular response. Under
/// DenseSingularPolicy::kReport a flagged or exactly singular outcome is
/// refactored densely as D K D + sigma diag(+1 primal, -1 dual), D =
/// equilibration_scale(K), sigma = kDenseEquilibratedShift, accepted iff taken
/// and unflagged; else once more at kDenseShiftRetryGrowth sigma; else the
/// platform session retakes K (counted, last resort). Sets `k.dense_shifted`
/// and `k.dense_scale` (dense_response_tally() reports which response ran).
/// On a throw the marker may read true over unshifted numerics, never false
/// over shifted ones.
/// @throws hven::BackendError when no factorization succeeds.
hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatRM &K,
                                                 const AnalysisDecision &decision,
                                                 const DenseShiftSite &site);
hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatRM &K,
                                                 const DenseShiftSite &site);

/// @brief The factor's dense counts so far: factorizations on the dense arm
/// (taken or reported exactly singular, shifted refactors included), unshifted
/// factorizations the detector flagged (exact included), equilibrated
/// refactors, platform retakes. `refactors - flagged` is the retries.
struct DenseResponseTally {
    Index dense = 0;
    Index flagged = 0;
    Index refactors = 0;
    Index retakes = 0;
};
inline DenseResponseTally dense_response_tally(const KktFactor &k) {
    const auto &c = k.factor.counters();
    return DenseResponseTally{c.dense_factorize_count + c.dense_singular_count,
                              c.dense_flagged_count + c.dense_singular_count -
                                  k.shifted_exact_singular,
                              c.dense_shift_refactor_count, c.dense_retake_count};
}

/// @brief Adds a factor's tally delta to four counters when it leaves scope,
/// a throw included.
class DenseTallyScope {
  public:
    DenseTallyScope(const KktFactor &k, Index &dense, Index &flagged, Index &refactors,
                    Index &retakes)
        : k_(k), dense_(dense), flagged_(flagged), refactors_(refactors), retakes_(retakes),
          before_(dense_response_tally(k)) {}
    ~DenseTallyScope() {
        const DenseResponseTally after = dense_response_tally(k_);
        dense_ += after.dense - before_.dense;
        flagged_ += after.flagged - before_.flagged;
        refactors_ += after.refactors - before_.refactors;
        retakes_ += after.retakes - before_.retakes;
    }
    DenseTallyScope(const DenseTallyScope &) = delete;
    DenseTallyScope &operator=(const DenseTallyScope &) = delete;

  private:
    const KktFactor &k_;
    Index &dense_;
    Index &flagged_;
    Index &refactors_;
    Index &retakes_;
    DenseResponseTally before_;
};

/// @brief Allocate and solve, returning the solution by value (through the
/// session `k.numerics` names).
Vec solve_vec(const KktFactor &k, const Vec &rhs);

/// @brief The inertia evidence of `k`'s current numerics (the session
/// `k.numerics` names; D K D's inertia is K's, by Sylvester's law).
hven::linear::InertiaEvidence kkt_inertia(const KktFactor &k);

/// @brief The platform-solve retakes `k` has taken so far, and a scope that
/// adds their delta to two counters when it leaves (a throw included) --
/// DenseTallyScope's twin.
struct RetakeTally {
    Index scaled = 0;
    Index matched = 0;
};
inline RetakeTally retake_tally(const KktFactor &k) {
    return RetakeTally{k.scaled_retakes, k.matched_retakes};
}
class RetakeTallyScope {
  public:
    RetakeTallyScope(const KktFactor &k, Index &scaled, Index &matched)
        : k_(k), scaled_(scaled), matched_(matched), before_(retake_tally(k)) {}
    ~RetakeTallyScope() {
        const RetakeTally after = retake_tally(k_);
        scaled_ += after.scaled - before_.scaled;
        matched_ += after.matched - before_.matched;
    }
    RetakeTallyScope(const RetakeTallyScope &) = delete;
    RetakeTallyScope &operator=(const RetakeTallyScope &) = delete;

  private:
    const KktFactor &k_;
    Index &scaled_;
    Index &matched_;
    RetakeTally before_;
};

// THE DENSE ARM'S SOLVE GATES judge an answer y to K y = b by its normwise
// backward error (Rigal-Gaches; Higham, Accuracy and Stability of Numerical
// Algorithms, 2nd ed., Thm 7.1), inf-norms throughout:
//   eta(y) = ||b - K y|| / (||K|| ||y|| + ||b||),
// the smallest relative perturbation of K and b that y solves exactly.
// One threshold, 1e-8, for every gate (argued in
// docs/notes/2026-11-m8-dense-gate-design.md section 3).

/// ||K||inf of the symmetric matrix stored as K's upper triangle, entries
/// weighted d_i d_j when `d` is non-null (||D K D||inf). `acc` is scratch
/// (resized to K.rows()). A NaN propagates.
inline double symmetric_inf_norm(const SpMatRM &K, const Vec *d, Vec &acc) {
    const auto n = static_cast<Index>(K.rows());
    acc.setZero(n);
    for (Index i = 0; i < n; ++i) {
        for (SpMatRM::InnerIterator e(K, i); e; ++e) {
            const auto j = static_cast<Index>(e.col());
            const double w =
                d != nullptr ? std::abs(e.value()) * (*d)(i) * (*d)(j) : std::abs(e.value());
            acc(i) += w;
            if (j != i) {
                acc(j) += w;
            }
        }
    }
    return nan_sticky_inf_norm(acc);
}

/// eta from its parts: ||r|| / (k_norm ||y|| + ||b||); 0 when the residual and
/// the denominator both vanish; a NaN propagates.
inline double backward_error_of(double r_inf, double k_norm, double y_inf, double b_inf) {
    const double den = k_norm * y_inf + b_inf;
    return (r_inf == 0.0 && den == 0.0) ? 0.0 : r_inf / den;
}

// THE SHIFTED FACTOR'S VERIFICATION. A solve through shifted numerics answers
// D (D K D + sigma S)^-1 D b, not K^-1 b; every consumer that steps on or adopts
// such an answer refines it against K and gates it on eta in the shift's own
// metric, the equilibrated system (D K D) (D^-1 y) = D b. Unrefined, a
// first-attempt answer reads eta <= sigma / ||D K D||: inside the threshold, so
// what the gate refuses is a factorization that solves no nearby system.

/// Refinement budget against K on shifted numerics; each step is kept only
/// while ||D (rhs - K y)||inf strictly falls (D = KktFactor::dense_scale).
inline constexpr Index kEqpShiftRefineSteps = 10;

/// The gate: an answer from shifted numerics is used only if its eta at gate
/// time is at most this (a NaN refuses). The unshifted check's threshold: the
/// shift's own contribution (<= sigma = 2^-30 at the first attempt) sits below
/// it, the retry's (256 sigma ~ 2.4e-7) only once refinement removes it.
inline constexpr double kDenseShiftedBackwardErrorTol = 1e-8;

/// @brief kEqpShiftRefineSteps' loop on `y`, a solve of K y = rhs through `k`
/// (K upper-stored); a no-op unless `k.dense_shifted`. Returns the solves
/// spent, each counted before its keep-or-reject test.
inline Index refine_shifted_solve(const SpMatRM &K, const Vec &rhs, const KktFactor &k, Vec &y) {
    if (!k.dense_shifted) {
        return 0;
    }
    const Vec &d = k.dense_scale;
    Vec r = rhs - K.template selfadjointView<Eigen::Upper>() * y;
    double r_inf = nan_sticky_inf_norm(r.cwiseProduct(d));
    Index spent = 0;
    while (spent < kEqpShiftRefineSteps) {
        ++spent;
        const Vec candidate = y + solve_vec(k, r);
        Vec cr = rhs - K.template selfadjointView<Eigen::Upper>() * candidate;
        const double cr_inf = nan_sticky_inf_norm(cr.cwiseProduct(d));
        if (!(cr_inf < r_inf)) {
            break; // stagnated, diverging, or NaN -- keep the better answer
        }
        y = candidate;
        r = std::move(cr);
        r_inf = cr_inf;
    }
    return spent;
}

/// @brief eta of `y` on the equilibrated system: ||D (rhs - K y)||inf /
/// (||D K D||inf ||D^-1 y||inf + ||D rhs||inf), D = `k.dense_scale` (0 when
/// residual and denominator vanish; a NaN propagates). Requires
/// `k.dense_shifted`.
inline double shifted_backward_error(const SpMatRM &K, const Vec &rhs, const KktFactor &k,
                                     const Vec &y) {
    const Vec &d = k.dense_scale;
    Vec acc;
    const double r_d =
        nan_sticky_inf_norm((rhs - K.template selfadjointView<Eigen::Upper>() * y).cwiseProduct(d));
    return backward_error_of(r_d, symmetric_inf_norm(K, &d, acc),
                             nan_sticky_inf_norm(y.cwiseQuotient(d)),
                             nan_sticky_inf_norm(rhs.cwiseProduct(d)));
}

/// @brief True iff `k` holds shifted numerics and `y` fails the gate.
inline bool shifted_solve_refused(const SpMatRM &K, const Vec &rhs, const KktFactor &k,
                                  const Vec &y) {
    return k.dense_shifted &&
           !(shifted_backward_error(K, rhs, k, y) <= kDenseShiftedBackwardErrorTol);
}

// THE UNSHIFTED DENSE SOLVE CHECK. A dense-arm solve a consumer steps on is
// checked against K: only a solve whose eta misses kDenseSolveBackwardErrorTol
// is refined (keep-while-falling, kEqpShiftRefineSteps), and it is refused only
// if eta still exceeds the consumer's refusal threshold. A passing solve is used
// unchanged. Platform-session answers are not checked.
//
// THE METRIC. eta is measured on the equilibrated system (D K D)(D^-1 y) = D b,
// as the shifted gate is, whenever K is not already equilibrated: in the raw
// metric a row at scale s carries ||K|| ||y|| (and ||b||) to s-size, and a
// unit-scale row's residual up to 1e-8 s then passes -- the gate goes vacuous on
// exactly the rows an SQP constraint scale leaves at unit size. Always, not on
// a raw-eta trigger: the disparity can sit in either term of the denominator,
// so no cheap raw reading tells a vacuous pass from a real one without D, and D
// costs O(nnz) per sweep against the O(n^3) factorization it checks (design
// note section 3).

/// Refinement trigger on eta, and the refusal threshold.
inline constexpr double kDenseSolveBackwardErrorTol = 1e-8;

/// @brief The equilibration of K (equilibration_scale) into `d`, with `scratch`
/// as Ruiz's row-norm workspace; both are resized to K.rows() (no allocation
/// once sized).
void equilibration_scale_into(const SpMatRM &K, Vec &d, Vec &scratch);

/// @brief eta = ||K y - rhs||inf / (k_norm ||y||inf + ||rhs||inf), K
/// upper-stored and k_norm = ||K||inf (symmetric_inf_norm); 0 when residual and
/// denominator vanish, a NaN propagates. Leaves K y - rhs in `r` (resized to
/// rhs.size()). With `d` non-null, every norm is taken in D's metric
/// (||D r||, ||D^-1 y||, ||D rhs||; k_norm must then be ||D K D||inf); `r`
/// stays unscaled.
inline double dense_backward_error(const SpMatRM &K, double k_norm, const Vec &rhs, const Vec &y,
                                   Vec &r, const Vec *d = nullptr) {
    r.resize(rhs.size());
    r.noalias() = K.template selfadjointView<Eigen::Upper>() * y;
    r -= rhs;
    if (d != nullptr) {
        return backward_error_of(nan_sticky_inf_norm(r.cwiseProduct(*d)), k_norm,
                                 nan_sticky_inf_norm(y.cwiseQuotient(*d)),
                                 nan_sticky_inf_norm(rhs.cwiseProduct(*d)));
    }
    return backward_error_of(nan_sticky_inf_norm(r), k_norm, nan_sticky_inf_norm(y),
                             nan_sticky_inf_norm(rhs));
}

/// @brief The check on `y`, a solve of K y = rhs through `solve` (a callable
/// `void(const Vec &in, Vec &out)` against the same numerics), with eta in D's
/// metric when `d` is non-null (raw when K is already equilibrated): true iff
/// eta exceeds `refuse_tol` (or is NaN) after refinement. `y` changes only when
/// the first eta exceeds kDenseSolveBackwardErrorTol. `r` and `c` are
/// caller-held scratch (no allocation once sized).
template <typename Solve>
inline bool dense_solve_check_refused(const SpMatRM &K, const Vec *d, const Vec &rhs,
                                      double refuse_tol, Solve &&solve, Vec &y, Vec &r, Vec &c) {
    const double k_norm = symmetric_inf_norm(K, d, c);
    double eta = dense_backward_error(K, k_norm, rhs, y, r, d);
    if (eta <= kDenseSolveBackwardErrorTol) {
        return false;
    }
    for (Index step = 0; step < kEqpShiftRefineSteps; ++step) {
        solve(r, c); // K^-1 (K y - rhs)
        c = y - c;
        const double c_eta = dense_backward_error(K, k_norm, rhs, c, r, d);
        if (!(c_eta < eta)) {
            break; // stagnated, diverging, or NaN -- keep the better answer
        }
        y = c;
        eta = c_eta;
    }
    return !(eta <= refuse_tol);
}

/// @brief The raw-metric form, for a K that is already equilibrated.
template <typename Solve>
inline bool dense_solve_check_refused(const SpMatRM &K, const Vec &rhs, double refuse_tol,
                                      Solve &&solve, Vec &y, Vec &r, Vec &c) {
    return dense_solve_check_refused(K, nullptr, rhs, refuse_tol, std::forward<Solve>(solve), y, r,
                                     c);
}

/// @brief The KktFactor form at kDenseSolveBackwardErrorTol, in K's
/// equilibrated metric (`d` receives equilibration_scale(K)): true iff the
/// factorization `arm` names was taken by the dense arm unshifted
/// (`!k.dense_shifted`) and `y` fails the check. `r`, `c`, `d` are scratch.
inline bool unshifted_dense_solve_refused(const SpMatRM &K, const Vec &rhs, const KktFactor &k,
                                          hven::linear::FactorArm arm, Vec &y, Vec &r, Vec &c,
                                          Vec &d) {
    if (arm != hven::linear::FactorArm::kDense || k.dense_shifted) {
        return false;
    }
    equilibration_scale_into(K, d, r);
    return dense_solve_check_refused(
        K, &d, rhs, kDenseSolveBackwardErrorTol,
        [&](const Vec &in, Vec &out) {
            out.resize(in.size());
            k.factor.solve(in, out);
        },
        y, r, c);
}

// THE PLATFORM-SOLVE RESPONSE (M8 sparse-equil-v4). The SQP runs its sparse
// KKT factors on the platform session without matching or scaling. Pardiso then
// replaces a pivot it cannot take inside a supernode by a static perturbation
// of 1e-8 ||K|| and the solve leans on iterative refinement. Two failure
// classes follow, both measured (docs/notes/data/2026-11-m8-sparse-equil-v2/):
//   - THE SCALE CLASS: on a badly scaled K the perturbation threshold is taken
//     against an unscaled norm. A symmetric scaling fixes it: D K D with D the
//     maximum-product-matching scaling (linear/matching_scaling.h), every
//     entry <= 1 and a unit entry in every row, factored on a plain session.
//   - THE PAIRING CLASS: a saddle-point K whose diagonal pivots must pair into
//     2x2 blocks. No symmetric diagonal scaling helps (the 1x1 pivot's growth
//     delta_i mu_j / a_ij^2 is invariant under K -> D K D); a weighted-matching
//     factorization (Duff & Pralet 2005) takes the pairs as 2x2 pivots
//     (SymmetricFactor::Options::weighted_matching, Pardiso iparm[12] = 1,
//     without iparm[10]'s scaling, which costs the measured STEENBRB
//     regression).
//
// THE TRIGGER runs where the engine has finished refining its answer against K
// (solve_eqp, solve_bordered_eqp) on platform numerics that reported perturbed
// pivots -- on every platform factorization on a backend that reports no
// perturbed-pivot count (Accelerate) -- and measures the answer's normwise
// backward error in the equilibrated metric (dense_backward_error with
// D = equilibration_scale(K); the raw metric is vacuous on a badly scaled K). A
// miss of kPlatformSolveFailureTol calls for the kScaled retake and the answer
// is solved again on it.
//
// THE ESCALATION. kScaled numerics that STILL report perturbed pivots are
// judged again, and fail on a backward-error miss OR on the engine's own
// residual refusal: a perturbed pivot the scaling did not remove leaves a
// near-null direction of size ~1/(1e-8 ||K||) in the answer, and the normwise
// backward error, whose denominator carries ||K|| ||y||, is blind to exactly
// that (measured on YORKNET restoration qp26: eta 1.1e-7 while the constraint
// rows miss by 3.7e7 times the engine's tolerance). A failure calls for the
// kMatched retake. That tier is Pardiso-only, and is never judged again.
//
// THE RESPONSE IS PER FACTORIZATION, never sticky. On the eliminated path
// (solve_eqp) the factor holding K is in hand, so a failed answer is retaken IN
// PLACE on that same K (retake_in_place) and re-solved; the next factorization
// is the platform's own again. A retake that does not return kOk, or throws
// (a BackendError from the retake session's analysis, an invalid_argument from
// the scaling), leaves the platform's numerics in place with the check
// disarmed, so the response is bounded at two retakes per factorization.
//
// THE BORDER PATH cannot retake in place: the Schur complement C was formed
// through K0's numerics, so a failed bordered answer (solve_bordered_eqp's
// EqpResult::platform_retake) is REQUESTED on K0's factor
// (request_platform_retake) and the pass takes the eliminated twin. The request
// is taken by the NEXT K0 factorization -- the rebuild it forces
// (SchurComplement::needs_refactorization() reads rebuild_pending) -- whatever
// working rows that K0 then carries: it is K0's block family (the same H, the
// same constraint scales), not one matrix, that the failure diagnosed, and the
// rebuilt K0 is retaken only if its OWN platform factorization perturbs pivots
// (a clean K0 is left on the platform bit for bit). A request never outlives the
// QP solve that made it: the walk clears it wherever the border stack is reset
// and at the entry and the exit of every solve (clear_platform_retake_request).
//
// COST AND MEMORY. Retake factorizations are counted in their own fields
// (QpCounters::sparse_scaled_retakes / sparse_matched_retakes), never in
// `factorizations`, whose accounting contract is the active-set search's; their
// analyses are not in `symbolic_analyses` (KktFactor::scaled_analyzed_pattern
// says when one is paid). A retake session lives from its first use to the end
// of the QP solve (release_idle_retake_sessions), except the one the factor's
// current numerics live in, which a reused K0 may still solve through.

/// The failed-solve threshold: IPOPT's documented residual_ratio_singular
/// default; the measured post-refinement distribution is bimodal around it.
inline constexpr double kPlatformSolveFailureTol = 1e-5;

/// @brief True iff the platform solve check judges answers through `k`'s
/// current numerics: armed platform numerics, or armed kScaled numerics that
/// still report perturbed pivots.
inline bool platform_solve_check_applies(const KktFactor &k) {
    if (k.dense_shifted || !k.check_armed) {
        return false;
    }
    switch (k.numerics) {
    case PlatformRetake::kNone:
        return true;
    case PlatformRetake::kScaled:
        return k.scaled_perturbed_pivots.value_or(0) > 0;
    case PlatformRetake::kMatched:
        return false;
    }
    return false;
}

/// @brief THE VERDICT, the one place the trigger and the escalation are
/// decided (solve_eqp and solve_bordered_eqp both call it): kNone when the
/// check does not apply to `k`'s numerics; on platform numerics kScaled iff
/// `missed` (the answer's backward error missed kPlatformSolveFailureTol, or
/// was NaN); on kScaled numerics kMatched iff `missed` or `engine_refused` (the
/// engine's own residual check refused the same answer -- THE ESCALATION).
PlatformRetake platform_retake_verdict(const KktFactor &k, bool missed, bool engine_refused);

/// @brief The verdict on an answer `y` to K y = rhs (K the matrix `k` factored,
/// upper-stored): `missed` is y's normwise backward error in K's equilibrated
/// metric against kPlatformSolveFailureTol (a NaN misses). kNone when the check
/// does not apply (nothing is computed then).
PlatformRetake platform_solve_response(const KktFactor &k, const SpMatRM &K, const Vec &rhs,
                                       const Vec &y, bool engine_refused);

/// @brief Retakes `k`'s current numerics of K -- the matrix its last
/// factorization took, upper-stored and compressed -- on `tier`'s session NOW
/// (the eliminated path's response; nothing is requested). On success the
/// numerics are the tier's. On failure (not kOk, or a throw from the retake
/// session or the scaling) the platform's numerics are restored and the check
/// disarmed. Returns true iff `k`'s numerics changed (the retake took, or a
/// failed escalation fell back to the platform's), so the caller must solve
/// again. A no-op returning false for tier kNone.
/// @throws std::invalid_argument If K is not compressed.
bool retake_in_place(KktFactor &k, const SpMatRM &K, PlatformRetake tier);

/// @brief Requests `tier` (kScaled or kMatched) for the next factorization of
/// `k` and marks holders of earlier numerics for a refactor (THE BORDER PATH).
/// A no-op for kNone.
void request_platform_retake(KktFactor &k, PlatformRetake tier);

/// @brief Drops a pending request and its rebuild mark (the numerics are
/// untouched).
inline void clear_platform_retake_request(KktFactor &k) {
    k.requested = PlatformRetake::kNone;
    k.rebuild_pending = false;
}

/// @brief Frees the retake sessions `k`'s current numerics do not live in (and
/// the scaled session's analysis record with it).
void release_idle_retake_sessions(KktFactor &k);

} // namespace hven::detail
