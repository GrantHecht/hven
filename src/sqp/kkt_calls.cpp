#include <hven/detail/sqp/kkt_calls.h>

#include <stdexcept>

#include <fmt/format.h>

namespace hven::solvers::detail {

bool needs_analysis(const KktFactor &k, const SpMatU &K) {
    return !k.analyzed || hven::pattern_hash(K) != k.analyzed_pattern;
}

hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatU &K) {
    if (needs_analysis(k, K)) {
        k.factor.analyze(K);
        k.analyzed_pattern = hven::pattern_hash(K);
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

Vec solve_vec(const KktFactor &k, const Vec &rhs) {
    Vec x(rhs.size());
    k.factor.solve(rhs, x);
    return x;
}

} // namespace hven::solvers::detail
