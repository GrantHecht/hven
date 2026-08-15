#include <hven/detail/sqp/kkt_calls.h>

#include <stdexcept>

#include <fmt/format.h>

namespace hven::solvers::detail {

AnalysisDecision analysis_decision(const KktFactor &k, const SpMatU &K) {
    // Short-circuits on `!k.analyzed` exactly as needs_analysis() always has:
    // nothing has been analyzed, so the answer is `true` without looking at
    // K, and no hash is computed to be carried.
    if (!k.analyzed) {
        return AnalysisDecision{true, std::nullopt};
    }
    const std::uint64_t pattern = hven::pattern_hash(K);
    return AnalysisDecision{pattern != k.analyzed_pattern, pattern};
}

bool needs_analysis(const KktFactor &k, const SpMatU &K) { return analysis_decision(k, K).needed; }

hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatU &K,
                                                 const AnalysisDecision &decision) {
    if (decision.needed) {
        k.factor.analyze(K);
        // The record reuses the hash the decision was taken on. Only the
        // short-circuit path (nothing analyzed yet, so K was never hashed)
        // has none to reuse, and there this is the pattern's FIRST hash, not
        // a second one -- the count is the same either way.
        k.analyzed_pattern = decision.pattern ? *decision.pattern : hven::pattern_hash(K);
        k.analyzed = true;
    }

    hven::linear::FactorizeOutcome outcome = k.factor.factorize(K);
    if (outcome.status == hven::linear::FactorizeOutcome::Status::kBackendError) {
        throw std::runtime_error(
            fmt::format("KktFactor::factorize_checked: factorization failed, backend error {}",
                        outcome.backend_code));
    }
    return outcome;
}

hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatU &K) {
    return factorize_checked(k, K, analysis_decision(k, K));
}

Vec solve_vec(const KktFactor &k, const Vec &rhs) {
    Vec x(rhs.size());
    k.factor.solve(rhs, x);
    return x;
}

} // namespace hven::solvers::detail
