#pragma once

#include <cstdint>
#include <optional>

#include <hven/core/pattern_hash.h>
#include <hven/linear/symmetric_factor.h>
#include <hven/qp/qp_types.h>

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

// The analyze-or-not decision for K, carrying the pattern hash it was taken
// on so nothing downstream has to recompute it.
//
// `needed` is exactly what needs_analysis() reports. `pattern` holds
// hven::pattern_hash(K) WHEN ONE WAS COMPUTED, and is disengaged on the
// short-circuit path: a factor with `analyzed == false` needs an analysis
// whatever K's pattern is, so K is not hashed to decide that. Preserving
// that short-circuit is not cosmetic -- pattern_hash() throws on an
// uncompressed matrix, so hashing unconditionally here would move a throw
// site (docs/retarget-design-sqp.md section 11, ledger row 8).
//
// WHY THIS EXISTS (M3 phase C, B2). Steady state used to pay THREE O(nnz)
// pattern hashes per SSN major -- the call site's needs_analysis(), a second
// one inside factorize_checked(), and SymmetricFactor::factorize()'s own
// pattern guard -- where the dissolved KktSystem seam paid two. B1 priced
// the extra one at 6.12-7.76 % of SSN-major wall-clock on the SSN-heavy
// families (docs/notes/data/2026-08-15-m3-b1-hash-cost/), above the plan's
// 5 % gate-blocking threshold. Threading the decision through is what puts
// steady state back at two: this one, plus factorize()'s guard -- which is
// hven::linear's own published contract (SymmetricFactor rejects a foreign
// pattern) and is therefore the floor, not something B2 removes.
struct AnalysisDecision {
    bool needed = false;
    std::optional<std::uint64_t> pattern;
};

// The analyze-or-not decision for K, with the hash it was taken on kept for
// the caller to hand back to factorize_checked(). The pair
// `analysis_decision()` + `factorize_checked(k, K, decision)` is the form
// every call site that counts symbolic_analyses should use.
AnalysisDecision analysis_decision(const KktFactor &k, const SpMatU &K);

// True iff factorize_checked() would run an analysis for K. SQP call sites
// consult this before factorize_checked() to preserve their
// symbolic_analyses counting contract. It is `analysis_decision(k, K).needed`
// and cannot disagree with it -- one is implemented in terms of the other.
bool needs_analysis(const KktFactor &k, const SpMatU &K);

// Analyze iff the pattern changed, then factorize. A backend failure is
// restored to KktSystem's throwing contract.
hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatU &K);

// The same, on a decision the caller has already taken -- which is what
// keeps the pattern from being hashed twice for one factorization. The
// decision MUST be the one `analysis_decision()` returned for this same `k`
// and this same K's pattern; handing back a stale decision would analyze (or
// skip analyzing) against the wrong pattern.
hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatU &K,
                                                 const AnalysisDecision &decision);

// Allocate and solve, matching KktSystem's Vec-returning call shape.
Vec solve_vec(const KktFactor &k, const Vec &rhs);

} // namespace hven::solvers::detail
