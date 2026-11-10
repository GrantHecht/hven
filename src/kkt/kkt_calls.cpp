// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <hven/detail/kkt/kkt_calls.h>

#include <hven/core/errors.h>

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

bool flagged(const hven::linear::FactorizeOutcome &o) {
    return o.status == hven::linear::FactorizeOutcome::Status::kOk
               ? o.inertia.numerically_singular
               : o.backend_code == hven::linear::kDenseSingular;
}

} // namespace

hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatRM &K,
                                                 const AnalysisDecision &decision) {
    analyze_if_needed(k, K, decision);
    k.dense_shifted = false;
    k.dense_scale.resize(0);
    hven::linear::FactorizeOutcome outcome = k.factor.factorize(K);
    if (flagged(outcome)) {
        // No site to shift on: the platform session retakes K.
        outcome = k.factor.retake_on_platform(
            K, hven::linear::SymmetricFactor::PatternCheck::kAssumeAnalyzed);
    }
    if (outcome.status == hven::linear::FactorizeOutcome::Status::kBackendError) {
        throw_failed(outcome);
    }
    return outcome;
}

Vec equilibration_scale(const SpMatRM &K) {
    const auto n = static_cast<Index>(K.rows());
    Vec d = Vec::Ones(n);
    Vec r(n);
    constexpr int kMaxSweeps = 8;
    constexpr double kStop = 0.05;
    for (int sweep = 0; sweep < kMaxSweeps; ++sweep) {
        r.setZero();
        for (Index i = 0; i < n; ++i) {
            for (SpMatRM::InnerIterator e(K, i); e; ++e) {
                const auto j = static_cast<Index>(e.col());
                const double w = std::abs(e.value()) * d(i) * d(j);
                r(i) = std::max(r(i), w);
                r(j) = std::max(r(j), w);
            }
        }
        double dev = 0.0;
        for (Index i = 0; i < n; ++i) {
            if (r(i) > 0.0 && std::isfinite(r(i))) {
                dev = std::max(dev, std::abs(1.0 - r(i)));
                d(i) /= std::sqrt(r(i));
            }
        }
        if (dev <= kStop) {
            break;
        }
    }
    return d;
}

CheckedFactorization factorize_checked(KktFactor &k, const SpMatRM &K,
                                       const AnalysisDecision &decision,
                                       const DenseShiftSite &site) {
    using Check = hven::linear::SymmetricFactor::PatternCheck;
    analyze_if_needed(k, K, decision);
    k.dense_shifted = false;
    k.dense_scale.resize(0);
    CheckedFactorization out;
    out.delta = site.delta;
    out.mu = site.mu;
    out.outcome = k.factor.factorize(K);
    if (flagged(out.outcome)) {
        // The pattern was verified by the call above; the responses factor the
        // same K.
        Vec d = equilibration_scale(K);
        double sigma = kDenseEquilibratedShift;
        bool done = false;
        for (int attempt = 0; attempt < 2 && !done; ++attempt) {
            if (attempt > 0) {
                sigma *= kDenseShiftRetryGrowth;
            }
            out.outcome = k.factor.factorize_equilibrated(K, site.n_primal, d.data(), sigma,
                                                          Check::kAssumeAnalyzed);
            if (out.outcome.status != hven::linear::FactorizeOutcome::Status::kOk) {
                ++k.shifted_exact_singular;
            }
            done = !flagged(out.outcome) &&
                   out.outcome.status == hven::linear::FactorizeOutcome::Status::kOk;
        }
        if (done) {
            out.shifted = true;
            k.dense_shifted = true;
            k.dense_scale = std::move(d);
        } else {
            out.outcome = k.factor.retake_on_platform(K, Check::kAssumeAnalyzed);
            out.retaken = true;
        }
    }
    if (out.outcome.status == hven::linear::FactorizeOutcome::Status::kBackendError) {
        throw_failed(out.outcome);
    }
    return out;
}

CheckedFactorization factorize_checked(KktFactor &k, const SpMatRM &K, const DenseShiftSite &site) {
    return factorize_checked(k, K, analysis_decision(k, K), site);
}

hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatRM &K) {
    return factorize_checked(k, K, analysis_decision(k, K));
}

Vec solve_vec(const KktFactor &k, const Vec &rhs) {
    Vec x(rhs.size());
    k.factor.solve(rhs, x);
    return x;
}

} // namespace hven::detail
