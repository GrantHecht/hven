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
//
// Invariant: `factorize_checked()` must be the only thing that ever drives
// an analysis on `factor`. `analyzed`/`analyzed_pattern` mirror state that
// `SymmetricFactor` keeps privately and exposes through no getter, so the
// mirror is only correct as long as nothing else touches the factor's
// symbolic state behind this struct's back:
//   - Calling `factor.analyze(K)` directly (bypassing this struct) leaves
//     the mirror stale, so the next `factorize_checked()` runs a redundant
//     second symbolic analysis and moves `symbolic_analyses` a second time
//     for one logical change.
//   - Installing a `SymmetricFactor::adopt()`-built factor (e.g.
//     `KktFactor{SymmetricFactor::adopt(handle)}`) leaves `analyzed ==
//     false` even though the adopted engine may already carry a reusable
//     symbolic, so the first `factorize_checked()` forks a new session
//     (`SymmetricFactor::analyze()`), moving `session_id()` and breaking
//     §7.1's reuse condition (e) for every other holder of that handle. A
//     future adopt path must seed `analyzed`/`analyzed_pattern` from the
//     handle's own `pattern_hash()` before first use.
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
