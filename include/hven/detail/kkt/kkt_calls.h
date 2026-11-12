// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <cstdint>
#include <optional>
#include <utility>

#include <Eigen/SparseCore>

#include <hven/core/detail/inf_norm.h>
#include <hven/core/pattern_hash.h>
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
/// factorize_checked()'s site overloads respond.
inline hven::linear::SymmetricFactor::Options sqp_kkt_options(int threads,
                                                              hven::linear::DensePolicy dense) {
    hven::linear::SymmetricFactor::Options o;
    o.decomposition = hven::linear::Decomposition::kLdlt;
    o.threads = threads;
    o.dense_factorization = dense;
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
/// zero or non-finite keep d_i), stopping once max|1 - r_i| <= 0.05.
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

/// @brief The factor's dense singular-response counts so far: unshifted
/// factorizations the detector flagged (exact included), equilibrated
/// refactors, platform retakes. `refactors - flagged` is the retries.
struct DenseResponseTally {
    Index flagged = 0;
    Index refactors = 0;
    Index retakes = 0;
};
inline DenseResponseTally dense_response_tally(const KktFactor &k) {
    const auto &c = k.factor.counters();
    return DenseResponseTally{c.dense_flagged_count + c.dense_singular_count -
                                  k.shifted_exact_singular,
                              c.dense_shift_refactor_count, c.dense_retake_count};
}

/// @brief Adds a factor's tally delta to three counters when it leaves scope,
/// a throw included.
class DenseTallyScope {
  public:
    DenseTallyScope(const KktFactor &k, Index &flagged, Index &refactors, Index &retakes)
        : k_(k), flagged_(flagged), refactors_(refactors), retakes_(retakes),
          before_(dense_response_tally(k)) {}
    ~DenseTallyScope() {
        const DenseResponseTally after = dense_response_tally(k_);
        flagged_ += after.flagged - before_.flagged;
        refactors_ += after.refactors - before_.refactors;
        retakes_ += after.retakes - before_.retakes;
    }
    DenseTallyScope(const DenseTallyScope &) = delete;
    DenseTallyScope &operator=(const DenseTallyScope &) = delete;

  private:
    const KktFactor &k_;
    Index &flagged_;
    Index &refactors_;
    Index &retakes_;
    DenseResponseTally before_;
};

/// @brief Allocate and solve, returning the solution by value.
Vec solve_vec(const KktFactor &k, const Vec &rhs);

// THE SHIFTED FACTOR'S VERIFICATION. A solve through shifted numerics answers
// D (D K D + sigma S)^-1 D b, not K^-1 b; every consumer that steps on or adopts
// such an answer refines it against K and gates it on rho below.

/// Refinement budget against K on shifted numerics; each step is kept only
/// while ||D (rhs - K y)||inf strictly falls (D = KktFactor::dense_scale).
inline constexpr Index kEqpShiftRefineSteps = 10;

/// The gate: an answer from shifted numerics is used only if its rho at gate
/// time is at most this (a NaN refuses).
inline constexpr double kEqpShiftedResidualTol = 1e-8;

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

/// @brief rho = ||D (rhs - K y)||inf / ||D rhs||inf with D = `k.dense_scale`
/// (0 when both vanish; a NaN propagates). Requires `k.dense_shifted`.
inline double shifted_residual_ratio(const SpMatRM &K, const Vec &rhs, const KktFactor &k,
                                     const Vec &y) {
    const Vec &d = k.dense_scale;
    const double r_d =
        nan_sticky_inf_norm((rhs - K.template selfadjointView<Eigen::Upper>() * y).cwiseProduct(d));
    const double b_d = nan_sticky_inf_norm(rhs.cwiseProduct(d));
    return (r_d == 0.0 && b_d == 0.0) ? 0.0 : r_d / b_d;
}

/// @brief True iff `k` holds shifted numerics and `y` fails the gate.
inline bool shifted_solve_refused(const SpMatRM &K, const Vec &rhs, const KktFactor &k,
                                  const Vec &y) {
    return k.dense_shifted && !(shifted_residual_ratio(K, rhs, k, y) <= kEqpShiftedResidualTol);
}

// THE UNSHIFTED DENSE SOLVE CHECK. A dense-arm solve a consumer steps on is
// checked against K: only a solve whose rho misses kDenseSolveResidualTol is
// refined (keep-while-falling, kEqpShiftRefineSteps), and it is refused only if
// rho still exceeds the consumer's refusal threshold (>= kDenseSolveResidualTol).
// A passing solve is used unchanged. Platform-session answers are not checked.

/// Refinement trigger on rho (and the refusal threshold where a consumer
/// passes no looser one).
inline constexpr double kDenseSolveResidualTol = 1e-8;

/// @brief rho = ||K y - rhs||inf / ||rhs||inf (K upper-stored); 0 when both
/// vanish, a NaN propagates. Leaves K y - rhs in `r` (resized to rhs.size()).
inline double dense_residual_ratio(const SpMatRM &K, const Vec &rhs, const Vec &y, Vec &r) {
    r.resize(rhs.size());
    r.noalias() = K.template selfadjointView<Eigen::Upper>() * y;
    r -= rhs;
    const double r_inf = nan_sticky_inf_norm(r);
    const double b_inf = nan_sticky_inf_norm(rhs);
    return (r_inf == 0.0 && b_inf == 0.0) ? 0.0 : r_inf / b_inf;
}

/// @brief The check on `y`, a solve of K y = rhs through `solve` (a callable
/// `void(const Vec &in, Vec &out)` against the same numerics): true iff rho
/// exceeds `refuse_tol` (or is NaN) after refinement. `y` changes only when the
/// first rho exceeds kDenseSolveResidualTol. `r` and `c` are caller-held
/// scratch (no allocation once sized).
template <typename Solve>
inline bool dense_solve_check_refused(const SpMatRM &K, const Vec &rhs, double refuse_tol,
                                      Solve &&solve, Vec &y, Vec &r, Vec &c) {
    double rho = dense_residual_ratio(K, rhs, y, r);
    if (rho <= kDenseSolveResidualTol) {
        return false;
    }
    for (Index step = 0; step < kEqpShiftRefineSteps; ++step) {
        solve(r, c); // K^-1 (K y - rhs)
        c = y - c;
        const double c_rho = dense_residual_ratio(K, rhs, c, r);
        if (!(c_rho < rho)) {
            break; // stagnated, diverging, or NaN -- keep the better answer
        }
        y = c;
        rho = c_rho;
    }
    return !(rho <= refuse_tol);
}

/// @brief The KktFactor form at kDenseSolveResidualTol: true iff the
/// factorization `arm` names was taken by the dense arm unshifted
/// (`!k.dense_shifted`) and `y` fails the check.
inline bool unshifted_dense_solve_refused(const SpMatRM &K, const Vec &rhs, const KktFactor &k,
                                          hven::linear::FactorArm arm, Vec &y, Vec &r, Vec &c) {
    if (arm != hven::linear::FactorArm::kDense || k.dense_shifted) {
        return false;
    }
    return dense_solve_check_refused(
        K, rhs, kDenseSolveResidualTol,
        [&](const Vec &in, Vec &out) {
            out.resize(in.size());
            k.factor.solve(in, out);
        },
        y, r, c);
}

} // namespace hven::detail
