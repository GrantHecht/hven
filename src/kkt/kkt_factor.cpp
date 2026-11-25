// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// kkt_factor.cpp -- IMPLEMENTS hven/detail/kkt/kkt_factor.h (`KktFactor`, the QP-side factor).
// Not kkt_factorization.cpp, which implements the IPM/IPQP `KktFactorization`.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include <hven/core/errors.h>
#include <hven/detail/kkt/kkt_factor.h>
#include <hven/detail/linear/dense_session.h>
#include <hven/detail/linear/matching_scaling.h>

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

using Counters = hven::linear::SymmetricFactor::Counters;

// The thirteen fields of SymmetricFactor::Counters, in declaration order.
#define HVEN_KKT_FACTOR_COUNTERS(X)                                                                \
    X(analyze_count)                                                                               \
    X(factorize_count)                                                                             \
    X(solve_count)                                                                                 \
    X(partial_solve_count)                                                                         \
    X(pattern_verify_count)                                                                        \
    X(dense_factorize_count)                                                                       \
    X(dense_fallback_count)                                                                        \
    X(dense_singular_count)                                                                        \
    X(dense_flagged_count)                                                                         \
    X(dense_condition_estimate_count)                                                              \
    X(dense_equilibrated_estimate_count)                                                           \
    X(dense_shift_refactor_count)                                                                  \
    X(dense_retake_count)

#define HVEN_KKT_COUNT_ONE(f) +1
static_assert(sizeof(Counters) == (0 HVEN_KKT_FACTOR_COUNTERS(HVEN_KKT_COUNT_ONE)) * sizeof(Index),
              "SymmetricFactor::Counters gained or lost a field: update "
              "HVEN_KKT_FACTOR_COUNTERS in kkt/kkt_factor.cpp.");
#undef HVEN_KKT_COUNT_ONE

// acc += sign * c, field by field.
void accumulate(Counters &acc, const Counters &c, Index sign) noexcept {
#define HVEN_KKT_ACCUMULATE(f) acc.f += sign * c.f;
    HVEN_KKT_FACTOR_COUNTERS(HVEN_KKT_ACCUMULATE)
#undef HVEN_KKT_ACCUMULATE
}

thread_local AnalysisReuseTally *t_reuse_innermost = nullptr;

void copy_pattern(const SpMatRM &K, std::vector<SpMatRM::StorageIndex> &outer,
                  std::vector<SpMatRM::StorageIndex> &inner) {
    outer.assign(K.outerIndexPtr(), K.outerIndexPtr() + K.rows() + 1);
    inner.assign(K.innerIndexPtr(), K.innerIndexPtr() + K.nonZeros());
}

// THE KEY (kkt_factor.h, THE RETAINED ANALYSES): the hash, the order and the
// stored-entry count, then the exact pattern. K is compressed (it was hashed).
bool holds_pattern(const RetainedAnalysis &e, std::uint64_t pattern, const SpMatRM &K) {
    if (e.pattern_hash != pattern || e.outer.size() != static_cast<std::size_t>(K.rows()) + 1 ||
        e.inner.size() != static_cast<std::size_t>(K.nonZeros())) {
        return false;
    }
    return std::equal(e.outer.begin(), e.outer.end(), K.outerIndexPtr()) &&
           std::equal(e.inner.begin(), e.inner.end(), K.innerIndexPtr());
}

// Installs `incoming` (analyzed on `pattern`, whose index arrays are `outer` and
// `inner`) as k.factor. `handover` is what incoming's counters read when it
// entered (zero for a session built on this miss), so the counts it already
// contributed to departed_counters are not counted twice. The outgoing analyzed
// session becomes the most recently used retained entry and the least recently
// used one past the cap is freed. Nothrow once `k.retained` has room for one
// more entry, which the caller reserves before anything moves.
void swap_in(KktFactor &k, hven::linear::SymmetricFactor &incoming, const Counters &handover,
             std::uint64_t pattern, std::vector<SpMatRM::StorageIndex> &outer,
             std::vector<SpMatRM::StorageIndex> &inner) noexcept {
    accumulate(k.departed_counters, k.factor.counters(), +1);
    accumulate(k.departed_counters, handover, -1);
    std::swap(k.factor, incoming); // `incoming` now holds the outgoing session
    k.retained.insert(k.retained.begin(),
                      RetainedAnalysis{k.analyzed_pattern, std::move(k.analyzed_outer),
                                       std::move(k.analyzed_inner), std::move(incoming)});
    if (k.retained.size() > kRetainedAnalyses) {
        k.retained.pop_back();
    }
    k.analyzed_outer = std::move(outer);
    k.analyzed_inner = std::move(inner);
    k.analyzed_pattern = pattern;
    k.analyzed = true;
}

// A retained session swapped into k.factor by analyze_if_needed(), and the
// unanalyzed session that replaces it should its refactorization not commit
// (built before the swap, so the discard itself cannot fail).
struct SwapIn {
    std::uint64_t session_id = 0;
    std::uint64_t epoch = 0;
    hven::linear::SymmetricFactor spare;
};

// THE DISCARD (kkt_factor.h, THE RETAINED ANALYSES). Armed by a hit; on every
// exit from factorize_checked() it checks whether the swapped-in session is
// still in k.factor at the (session_id, epoch) it was retained with -- no
// factorization committed after the swap (a kBackendError outcome, or a throw
// from factorize() or a response) -- and if so replaces it by the spare, an
// unanalyzed session (session id 0, which no record carries).
class SwapGuard {
  public:
    explicit SwapGuard(KktFactor &k) : k_(k) {}
    ~SwapGuard() {
        if (in_ && k_.factor.session_id() == in_->session_id && k_.factor.epoch() == in_->epoch) {
            accumulate(k_.departed_counters, k_.factor.counters(), +1);
            std::swap(k_.factor, in_->spare);
            k_.analyzed = false;
            k_.analyzed_pattern = 0;
            k_.analyzed_outer.clear();
            k_.analyzed_inner.clear();
        }
    }
    SwapGuard(const SwapGuard &) = delete;
    SwapGuard &operator=(const SwapGuard &) = delete;

    std::optional<SwapIn> &in() { return in_; }

  private:
    KktFactor &k_;
    std::optional<SwapIn> in_;
};

// Analyzes K when the decision says so -- or swaps in a retained session
// analyzed on K's pattern (THE RETAINED ANALYSES), arming `guard`.
void analyze_if_needed(KktFactor &k, const SpMatRM &K, const AnalysisDecision &decision,
                       SwapGuard &guard) {
    if (!decision.needed) {
        return;
    }
    if (!k.analyzed && k.retained.empty()) {
        // Nothing to look up and nothing to retain: the session analyzes in
        // place, as it always has.
        k.factor.analyze(K);
        // The record reuses the hash the decision was taken on. Only the
        // short-circuit path (nothing analyzed yet, so K was never hashed)
        // has none to reuse, and there this is the pattern's FIRST hash, not
        // a second one -- the count is the same either way.
        k.analyzed_pattern = decision.pattern ? *decision.pattern : hven::pattern_hash(K);
        k.analyzed = true;
        copy_pattern(K, k.analyzed_outer, k.analyzed_inner);
        return;
    }
    const std::uint64_t pattern = decision.pattern ? *decision.pattern : hven::pattern_hash(K);
    for (auto it = k.retained.begin(); it != k.retained.end(); ++it) {
        if (!holds_pattern(*it, pattern, K)) {
            continue;
        }
        // Everything that can throw comes before anything moves.
        guard.in().emplace(SwapIn{0, 0, hven::linear::SymmetricFactor(k.options)});
        RetainedAnalysis hit = std::move(*it);
        k.retained.erase(it);
        const Counters handover = hit.factor.counters();
        if (k.analyzed) {
            swap_in(k, hit.factor, handover, pattern, hit.outer, hit.inner);
        } else {
            // A discarded session left k.factor unanalyzed: nothing to retain.
            accumulate(k.departed_counters, k.factor.counters(), +1);
            accumulate(k.departed_counters, handover, -1);
            std::swap(k.factor, hit.factor);
            k.analyzed_outer = std::move(hit.outer);
            k.analyzed_inner = std::move(hit.inner);
            k.analyzed_pattern = pattern;
            k.analyzed = true;
        }
        guard.in()->session_id = k.factor.session_id();
        guard.in()->epoch = k.factor.epoch();
        AnalysisReuseTally::record();
        return;
    }
    if (!k.analyzed) {
        k.factor.analyze(K);
        k.analyzed_pattern = pattern;
        k.analyzed = true;
        copy_pattern(K, k.analyzed_outer, k.analyzed_inner);
        return;
    }
    // A miss: K is analyzed on a fresh session, committed only once that
    // analysis has succeeded (a throw leaves `k` exactly as it was).
    hven::linear::SymmetricFactor fresh(k.options);
    fresh.analyze(K);
    std::vector<SpMatRM::StorageIndex> outer;
    std::vector<SpMatRM::StorageIndex> inner;
    copy_pattern(K, outer, inner);
    k.retained.reserve(kRetainedAnalyses + 1);
    swap_in(k, fresh, Counters{}, pattern, outer, inner);
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
    // them (a pending request was taken when this factorization started).
    k.numerics = PlatformRetake::kNone;
    k.check_armed = false;
    k.scaled_perturbed_pivots.reset();
    k.retake_scale.resize(0);
}

hven::linear::SymmetricFactor &retake_session(KktFactor &k, PlatformRetake tier) {
    std::unique_ptr<hven::linear::SymmetricFactor> &s =
        tier == PlatformRetake::kScaled ? k.scaled_session : k.matched_session;
    if (!s) {
        // The platform's own configuration, dense arm off: the scaled matrix
        // has K's pattern, so it would select the arm K did (sparse).
        hven::linear::SymmetricFactor::Options o =
            sqp_kkt_options(k.factor.threads(), hven::linear::DensePolicy::kNever);
        // IPARM-SURFACE: iparm[12] = 1 on the matched session only, without
        // iparm[10]'s scaling (SymmetricFactor::Options::weighted_matching).
        o.weighted_matching = tier == PlatformRetake::kMatched;
        s = std::make_unique<hven::linear::SymmetricFactor>(o);
    }
    return *s;
}

// One retake of K on `tier`'s session. The scaled session re-analyzes only when
// K's pattern moves (D K D has K's pattern; its analysis is value-independent);
// the matched session analyzes K's current values every time (the weighted
// matching is value-dependent). Returns true iff the factorization returned
// kOk, the numerics then being the tier's (and, for kScaled, armed). A
// BackendError (the analysis) or std::invalid_argument (the scaling, the
// session's input checks) is a failed retake: nothing about `k`'s numerics
// changes, and the scaled session's analysis record moves only once an analysis
// has succeeded.
bool take_retake(KktFactor &k, const SpMatRM &K, PlatformRetake tier) {
    using Check = hven::linear::SymmetricFactor::PatternCheck;
    using Status = hven::linear::FactorizeOutcome::Status;
    if (!K.isCompressed()) {
        // The values are read through K's storage below (coeffs()); an
        // uncompressed K would carry the free slots' garbage.
        throw std::invalid_argument("KktFactor platform retake: K must be in compressed storage");
    }
    try {
        hven::linear::SymmetricFactor &session = retake_session(k, tier);
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
            const std::uint64_t pattern = hven::pattern_hash(K);
            if (!k.scaled_analyzed || pattern != k.scaled_analyzed_pattern) {
                k.scaled_analyzed = false;
                session.analyze(kd);
                k.scaled_analyzed_pattern = pattern;
                k.scaled_analyzed = true;
            }
            ++k.scaled_retakes;
            const hven::linear::FactorizeOutcome o = session.factorize(kd, Check::kAssumeAnalyzed);
            if (o.status != Status::kOk) {
                return false;
            }
            k.numerics = PlatformRetake::kScaled;
            k.retake_scale = std::move(d);
            k.scaled_perturbed_pivots = o.inertia.perturbed_pivots;
            k.check_armed = true;
            return true;
        }
        session.analyze(K);
        ++k.matched_retakes;
        const hven::linear::FactorizeOutcome o = session.factorize(K, Check::kAssumeAnalyzed);
        if (o.status != Status::kOk) {
            return false;
        }
        k.numerics = PlatformRetake::kMatched;
        k.retake_scale.resize(0);
        k.scaled_perturbed_pivots.reset();
        return true;
    } catch (const hven::BackendError &) {
        return false;
    } catch (const std::invalid_argument &) {
        return false;
    }
}

// The platform's own sparse outcome: arms the check, and takes `request` when
// the outcome warrants it (perturbed pivots, or no report). A retake that fails
// leaves the platform numerics in place with the check disarmed (the response
// is bounded). The returned outcome is the platform's: its status is what the
// caller acts on, and the evidence a caller reads goes through kkt_inertia().
void platform_response(KktFactor &k, const SpMatRM &K,
                       const hven::linear::FactorizeOutcome &outcome, PlatformRetake request) {
    if (!(outcome.status == hven::linear::FactorizeOutcome::Status::kOk &&
          outcome.arm == hven::linear::FactorArm::kSparse)) {
        return;
    }
    const std::optional<Index> &pp = outcome.inertia.perturbed_pivots;
    const bool warranted = !pp.has_value() || *pp > 0;
    if (!warranted) {
        return;
    }
    k.check_armed = true;
    if (request == PlatformRetake::kNone) {
        return;
    }
    if (!take_retake(k, K, request)) {
        k.check_armed = false;
    }
}

// Takes the pending request off `k` as a factorization starts: a throw from the
// analysis or the factorization below cannot leave it pending for a later one.
PlatformRetake take_request(KktFactor &k) {
    const PlatformRetake request = k.requested;
    clear_platform_retake_request(k);
    return request;
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
    const PlatformRetake request = take_request(k);
    SwapGuard guard(k);
    analyze_if_needed(k, K, decision, guard);
    hven::linear::FactorizeOutcome outcome = k.factor.factorize(K);
    clear_shift_marker(k);
    if (flagged(outcome)) {
        // No site to shift on: the platform session retakes K.
        outcome = k.factor.retake_on_platform(
            K, hven::linear::SymmetricFactor::PatternCheck::kAssumeAnalyzed);
    } else {
        platform_response(k, K, outcome, request);
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
    const PlatformRetake request = take_request(k);
    SwapGuard guard(k);
    analyze_if_needed(k, K, decision, guard);
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
    } else {
        platform_response(k, K, outcome, request);
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
        if (k.retake_scale.size() != rhs.size()) {
            throw std::invalid_argument(fmt::format(
                "solve_vec: right-hand side has size {}, the scaled retake's D has size {}",
                static_cast<long long>(rhs.size()), static_cast<long long>(k.retake_scale.size())));
        }
        k.scaled_session->solve(rhs.cwiseProduct(k.retake_scale), x);
        x.array() *= k.retake_scale.array();
        break;
    case PlatformRetake::kMatched:
        k.matched_session->solve(rhs, x);
        break;
    }
    return x;
}

void solve_mat(const KktFactor &k, const Mat &rhs, Mat &x) {
    x.resize(rhs.rows(), rhs.cols());
    switch (k.numerics) {
    case PlatformRetake::kNone:
        k.factor.solve(rhs, x);
        break;
    case PlatformRetake::kScaled: {
        if (k.retake_scale.size() != rhs.rows()) {
            throw std::invalid_argument(fmt::format(
                "solve_mat: right-hand sides have {} rows, the scaled retake's D has size {}",
                static_cast<long long>(rhs.rows()), static_cast<long long>(k.retake_scale.size())));
        }
        // Column by column, the same elementwise products solve_vec forms.
        Mat scaled(rhs.rows(), rhs.cols());
        for (Index j = 0; j < rhs.cols(); ++j) {
            scaled.col(j) = rhs.col(j).cwiseProduct(k.retake_scale);
        }
        k.scaled_session->solve(scaled, x);
        for (Index j = 0; j < x.cols(); ++j) {
            x.col(j).array() *= k.retake_scale.array();
        }
        break;
    }
    case PlatformRetake::kMatched:
        k.matched_session->solve(rhs, x);
        break;
    }
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

PlatformRetake platform_retake_verdict(const KktFactor &k, bool missed, bool engine_refused) {
    if (!platform_solve_check_applies(k)) {
        return PlatformRetake::kNone;
    }
    if (k.numerics == PlatformRetake::kNone) {
        return missed ? PlatformRetake::kScaled : PlatformRetake::kNone;
    }
    return (missed || engine_refused) ? PlatformRetake::kMatched : PlatformRetake::kNone;
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
    return platform_retake_verdict(k, missed, engine_refused);
}

bool retake_in_place(KktFactor &k, const SpMatRM &K, PlatformRetake tier) {
    if (tier == PlatformRetake::kNone) {
        return false;
    }
    const PlatformRetake before = k.numerics;
    if (take_retake(k, K, tier)) {
        return true;
    }
    // The platform's numerics never left k.factor: restore them, disarmed.
    k.numerics = PlatformRetake::kNone;
    k.check_armed = false;
    k.scaled_perturbed_pivots.reset();
    k.retake_scale.resize(0);
    return before != PlatformRetake::kNone;
}

void request_platform_retake(KktFactor &k, PlatformRetake tier) {
    if (tier == PlatformRetake::kNone) {
        return;
    }
    k.requested = tier;
    k.rebuild_pending = true;
}

hven::linear::SymmetricFactor::Counters kkt_counters(const KktFactor &k) {
    Counters c = k.departed_counters;
    accumulate(c, k.factor.counters(), +1);
    return c;
}

void release_retained_analyses(KktFactor &k) noexcept { k.retained.clear(); }

AnalysisReuseTally::AnalysisReuseTally() noexcept : outer_(t_reuse_innermost) {
    t_reuse_innermost = this;
}

AnalysisReuseTally::~AnalysisReuseTally() {
    // Tallies are scoped objects, so they close in reverse order of opening.
    t_reuse_innermost = outer_;
}

void AnalysisReuseTally::record() noexcept {
    for (AnalysisReuseTally *t = t_reuse_innermost; t != nullptr; t = t->outer_) {
        ++t->reuses_;
    }
}

void release_idle_retake_sessions(KktFactor &k) {
    if (k.numerics != PlatformRetake::kScaled) {
        k.scaled_session.reset();
        k.scaled_analyzed = false;
        k.scaled_analyzed_pattern = 0;
    }
    if (k.numerics != PlatformRetake::kMatched) {
        k.matched_session.reset();
    }
}

} // namespace hven::detail
