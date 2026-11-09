// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <cstdint>
#include <optional>

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
/// says kNever where it is built.
inline hven::linear::SymmetricFactor::Options sqp_kkt_options(int threads,
                                                              hven::linear::DensePolicy dense) {
    hven::linear::SymmetricFactor::Options o;
    o.decomposition = hven::linear::Decomposition::kLdlt;
    o.threads = threads;
    o.dense_factorization = dense;
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

/// @brief Allocate and solve, returning the solution by value.
Vec solve_vec(const KktFactor &k, const Vec &rhs);

} // namespace hven::detail
