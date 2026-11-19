// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <hven/detail/kkt/kkt_factor.h>

#include <hven/core/errors.h>
#include <hven/detail/linear/dense_session.h>
#include <hven/detail/linear/matching_scaling.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <fmt/format.h>

namespace hven::detail {

AnalysisDecision analysis_decision(const KktFactor &k, const SpMatRM &K) {
    // Short-circuits on `!k.analyzed` exactly as needs_analysis() always has:
    // nothing has been analyzed, so the answer is `true` without looking at
    // K, and no hash is computed to be carried.
    if (!k.analyzed) {
        return AnalysisDecision{true, std::nullopt};
    }
    const std::uint64_t pattern = hven::pattern_hash(K);
    return AnalysisDecision{pattern != k.analyzed_pattern, pattern};
}

bool needs_analysis(const KktFactor &k, const SpMatRM &K) { return analysis_decision(k, K).needed; }

namespace {

void analyze_if_needed(KktFactor &k, const SpMatRM &K, const AnalysisDecision &decision) {
    if (decision.needed) {
        k.factor.analyze(K);
        // The record reuses the hash the decision was taken on. Only the
        // short-circuit path (nothing analyzed yet, so K was never hashed)
        // has none to reuse, and there this is the pattern's FIRST hash, not
        // a second one -- the count is the same either way.
        k.analyzed_pattern = decision.pattern ? *decision.pattern : hven::pattern_hash(K);
        k.analyzed = true;
    }
}

[[noreturn]] void throw_failed(const hven::linear::FactorizeOutcome &outcome) {
    throw hven::BackendError(
        fmt::format("KktFactor::factorize_checked: factorization failed, backend error {}",
                    outcome.backend_code));
}

void clear_shift_marker(KktFactor &k) {
    k.dense_shifted = false;
    k.dense_scale.resize(0);
    // The platform-solve response's description of the old numerics goes with
    // them (the request survives: platform_response() consumes it).
    k.numerics = PlatformRetake::kNone;
    k.check_armed = false;
    k.scaled_perturbed_pivots.reset();
    k.retake_scale.resize(0);
    k.rebuild_pending = false;
}

// One retake of K on `tier`'s session (analyzed on the current values: the
// weighted matching is value-dependent, and the scaled matrix's values are
// new). Returns true iff it returned kOk, the numerics then being the tier's.
bool take_retake(KktFactor &k, const SpMatRM &K, PlatformRetake tier) {
    using Check = hven::linear::SymmetricFactor::PatternCheck;
    using Status = hven::linear::FactorizeOutcome::Status;
    if (tier == PlatformRetake::kScaled) {
        // A non-finite K is the platform's to report; the scaling refuses it.
        if (!K.coeffs().allFinite()) {
            return false;
        }
        Vec d = hven::linear::detail::symmetric_matching_scaling(K);
        SpMatRM kd = K;
        for (Index i = 0; i < kd.outerSize(); ++i) {
            for (SpMatRM::InnerIterator e(kd, i); e; ++e) {
                e.valueRef() *= d(i) * d(static_cast<Index>(e.col()));
            }
        }
        k.scaled_session->analyze(kd);
        const hven::linear::FactorizeOutcome o =
            k.scaled_session->factorize(kd, Check::kAssumeAnalyzed);
        if (o.status != Status::kOk) {
            return false;
        }
        k.numerics = PlatformRetake::kScaled;
        k.retake_scale = std::move(d);
        k.scaled_perturbed_pivots = o.inertia.perturbed_pivots;
        ++k.scaled_retakes;
        return true;
    }
    k.matched_session->analyze(K);
    const hven::linear::FactorizeOutcome o =
        k.matched_session->factorize(K, Check::kAssumeAnalyzed);
    if (o.status != Status::kOk) {
        return false;
    }
    k.numerics = PlatformRetake::kMatched;
    ++k.matched_retakes;
    return true;
}

// The platform's own sparse outcome: arms the check, and takes a pending
// request when the outcome warrants it (perturbed pivots, or no report). A
// retake that does not return kOk leaves the platform numerics in place with
// the check disarmed (the response is bounded); the request is consumed
// either way. The returned outcome is the platform's: its status is what the
// caller acts on, and the evidence a caller reads goes through kkt_inertia().
void platform_response(KktFactor &k, const SpMatRM &K,
                       const hven::linear::FactorizeOutcome &outcome) {
    const PlatformRetake request = k.requested;
    k.requested = PlatformRetake::kNone;
    if (!(outcome.status == hven::linear::FactorizeOutcome::Status::kOk &&
          outcome.arm == hven::linear::FactorArm::kSparse)) {
        return;
    }
    const std::optional<Index> &pp = outcome.inertia.perturbed_pivots;
    const bool warranted = !pp.has_value() || *pp > 0;
    if (!warranted) {
        return;
    }
    if (request == PlatformRetake::kNone) {
        k.check_armed = true;
        return;
    }
    if (!take_retake(k, K, request)) {
        k.check_armed = false;
    }
}

bool flagged(const hven::linear::FactorizeOutcome &o) {
    return o.status == hven::linear::FactorizeOutcome::Status::kOk
               ? o.inertia.numerically_singular
               : o.backend_code == hven::linear::kDenseSingular;
}

} // namespace

hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatRM &K,
                                                 const AnalysisDecision &decision) {
    // THE MARKER MAY OVER-CLAIM, NEVER UNDER-CLAIM: a throw out of analyze()
    // or factorize()'s input checks leaves the previous numerics solvable
    // (both commit only on success), so the marker describing them is cleared
    // only once factorize() has returned -- by then the old numerics are gone.
    analyze_if_needed(k, K, decision);
    hven::linear::FactorizeOutcome outcome = k.factor.factorize(K);
    clear_shift_marker(k);
    if (flagged(outcome)) {
        // No site to shift on: the platform session retakes K.
        outcome = k.factor.retake_on_platform(
            K, hven::linear::SymmetricFactor::PatternCheck::kAssumeAnalyzed);
        k.requested = PlatformRetake::kNone;
    } else {
        platform_response(k, K, outcome);
    }
    if (outcome.status == hven::linear::FactorizeOutcome::Status::kBackendError) {
        throw_failed(outcome);
    }
    return outcome;
}

Vec equilibration_scale(const SpMatRM &K) {
    // The dense detector's own scale: one definition, so the shift response
    // factors exactly the matrix the detector judged.
    const auto n = static_cast<Index>(K.rows());
    Vec d(n);
    Vec r(n);
    hven::linear::detail::ruiz_symmetric_scale(K, d.data(), r.data());
    return d;
}

void equilibration_scale_into(const SpMatRM &K, Vec &d, Vec &scratch) {
    const auto n = static_cast<Index>(K.rows());
    d.resize(n);
    scratch.resize(n);
    hven::linear::detail::ruiz_symmetric_scale(K, d.data(), scratch.data());
}

hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatRM &K,
                                                 const AnalysisDecision &decision,
                                                 const DenseShiftSite &site) {
    using Check = hven::linear::SymmetricFactor::PatternCheck;
    using Status = hven::linear::FactorizeOutcome::Status;
    // Cleared only once factorize() has returned, as in the overload above.
    analyze_if_needed(k, K, decision);
    hven::linear::FactorizeOutcome outcome = k.factor.factorize(K);
    clear_shift_marker(k);
    if (flagged(outcome)) {
        // The pattern was verified by the call above; the responses factor the
        // same K. The marker is SET before the first equilibrated attempt, so a
        // throw from an attempt or from the retake leaves it over-claiming
        // (the gate then verifies unshifted numerics against K, which costs
        // nothing in safety), never under-claiming shifted ones.
        Vec d = equilibration_scale(K);
        k.dense_shifted = true;
        k.dense_scale = d;
        double sigma = kDenseEquilibratedShift;
        bool done = false;
        for (int attempt = 0; attempt < 2 && !done; ++attempt) {
            if (attempt > 0) {
                sigma *= kDenseShiftRetryGrowth;
            }
            outcome = k.factor.factorize_equilibrated(K, site.n_primal, d.data(), sigma,
                                                      Check::kAssumeAnalyzed);
            if (outcome.status != Status::kOk) {
                ++k.shifted_exact_singular;
            }
            done = !flagged(outcome) && outcome.status == Status::kOk;
        }
        if (!done) {
            outcome = k.factor.retake_on_platform(K, Check::kAssumeAnalyzed);
            clear_shift_marker(k);
        }
        k.requested = PlatformRetake::kNone;
    } else {
        platform_response(k, K, outcome);
    }
    if (outcome.status == Status::kBackendError) {
        throw_failed(outcome);
    }
    return outcome;
}

hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatRM &K,
                                                 const DenseShiftSite &site) {
    return factorize_checked(k, K, analysis_decision(k, K), site);
}

hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatRM &K) {
    return factorize_checked(k, K, analysis_decision(k, K));
}

Vec solve_vec(const KktFactor &k, const Vec &rhs) {
    Vec x(rhs.size());
    switch (k.numerics) {
    case PlatformRetake::kNone:
        k.factor.solve(rhs, x);
        break;
    case PlatformRetake::kScaled:
        k.scaled_session->solve(rhs.cwiseProduct(k.retake_scale), x);
        x.array() *= k.retake_scale.array();
        break;
    case PlatformRetake::kMatched:
        k.matched_session->solve(rhs, x);
        break;
    }
    return x;
}

hven::linear::InertiaEvidence kkt_inertia(const KktFactor &k) {
    switch (k.numerics) {
    case PlatformRetake::kScaled:
        return k.scaled_session->inertia();
    case PlatformRetake::kMatched:
        return k.matched_session->inertia();
    case PlatformRetake::kNone:
        break;
    }
    return k.factor.inertia();
}

PlatformRetake platform_solve_response(const KktFactor &k, const SpMatRM &K, const Vec &rhs,
                                       const Vec &y, bool engine_refused) {
    if (!platform_solve_check_applies(k)) {
        return PlatformRetake::kNone;
    }
    Vec d;
    Vec r;
    equilibration_scale_into(K, d, r);
    const double k_norm = symmetric_inf_norm(K, &d, r);
    const bool missed =
        !(dense_backward_error(K, k_norm, rhs, y, r, &d) <= kPlatformSolveFailureTol);
    if (k.numerics == PlatformRetake::kNone) {
        return missed ? PlatformRetake::kScaled : PlatformRetake::kNone;
    }
    return (missed || engine_refused) ? PlatformRetake::kMatched : PlatformRetake::kNone;
}

void request_platform_retake(KktFactor &k, PlatformRetake tier) {
    if (tier == PlatformRetake::kNone) {
        return;
    }
    if (tier == PlatformRetake::kScaled && !k.scaled_session) {
        // The platform's own configuration, dense arm off: the scaled matrix
        // has K's pattern, so it would select the arm K did (sparse).
        k.scaled_session = std::make_unique<hven::linear::SymmetricFactor>(
            sqp_kkt_options(k.factor.threads(), hven::linear::DensePolicy::kNever));
    }
    if (tier == PlatformRetake::kMatched && !k.matched_session) {
        // IPARM-SURFACE: iparm[12] = 1 on this second session only, without
        // iparm[10]'s scaling (SymmetricFactor::Options::weighted_matching).
        hven::linear::SymmetricFactor::Options o =
            sqp_kkt_options(k.factor.threads(), hven::linear::DensePolicy::kNever);
        o.weighted_matching = true;
        k.matched_session = std::make_unique<hven::linear::SymmetricFactor>(o);
    }
    k.requested = tier;
    k.rebuild_pending = true;
}

} // namespace hven::detail
