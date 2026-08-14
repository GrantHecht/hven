#pragma once

#include <cstdint>

#include <hven/core/pattern_hash.h>
#include <hven/detail/sqp/types.h>
#include <hven/linear/symmetric_factor.h>

namespace hven::solvers::detail {

// The SQP engine's proven linear configuration.
inline hven::linear::SymmetricFactor::Options sqp_kkt_options() {
    hven::linear::SymmetricFactor::Options o;
    o.kind = hven::linear::FactorKind::kLDLT;
    // 0 = backend default, and the test-side process-wide MKL_NUM_THREADS
    // pin is what makes runs reproducible. That pair is the intended and
    // sufficient mechanism: no consumer moves the count at runtime, so no
    // per-instance thread control is exposed.
    o.num_threads = 0;
    o.pivot_perturb_exp = std::nullopt;
    o.max_refinement_iters = std::nullopt;
    // Every remaining member keeps its default, and every default is
    // don't-write / absent -- the audit's SQP table is the proof this
    // covers the whole consumed surface.
    return o;
}

// The SQP engine's lifecycle state around a sparse symmetric factor. This is
// deliberately not a compatibility adapter: it owns only the explicit
// analyze-if-needed decision that KktSystem previously made internally.
struct KktFactor {
    hven::linear::SymmetricFactor factor{sqp_kkt_options()};
    std::uint64_t analyzed_pattern = 0;
    bool analyzed = false;
};

// True iff factorize_checked() would run an analysis for K. SQP call sites
// consult this before factorize_checked() to preserve their
// symbolic_analyses counting contract.
bool needs_analysis(const KktFactor &k, const SpMatU &K);

// Analyze iff the pattern changed, then factorize. A backend failure is
// restored to KktSystem's throwing contract.
hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatU &K);

// Allocate and solve, matching KktSystem's Vec-returning call shape.
Vec solve_vec(const KktFactor &k, const Vec &rhs);

} // namespace hven::solvers::detail
