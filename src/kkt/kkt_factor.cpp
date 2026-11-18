// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <hven/detail/kkt/kkt_factor.h>

#include <hven/core/errors.h>
#include <hven/detail/linear/dense_session.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

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
    k.matched_numerics = false;
    k.perturbed_pivots = 0;
    k.matched_rebuild_pending = false; // the numerics it described are gone
}

bool flagged(const hven::linear::FactorizeOutcome &o) {
    return o.status == hven::linear::FactorizeOutcome::Status::kOk
               ? o.inertia.numerically_singular
               : o.backend_code == hven::linear::kDenseSingular;
}

// DEVDBG: the diagnostic arms (env HVEN_SEQ_ARM = MS (default) | M | P | R; HVEN_SEQ_FORCE=1
// enters matched mode at the first perturbed factorization).
int seq_arm() {
    const char *a = std::getenv("HVEN_SEQ_ARM");
    if (!a)
        return 0;
    std::string v(a);
    return v == "M" ? 1 : v == "P" ? 2 : v == "R" ? 3 : 0;
}
// Maximum-product transversal -> symmetric scaling (Duff & Koster 2001, Duff & Pralet 2005),
// from the papers: c_ij = log(colmax_j) - log|a_ij| on the full symmetric pattern, a dense
// min-cost assignment with duals (u, v) (shortest augmenting path with potentials), then
// D_r = exp(u), D_c = exp(v)/colmax, d = sqrt(D_r D_c). Unmatched/structurally zero -> d = 1.
Vec seq_mc64_scale(const SpMatRM &K) {
    const auto n = static_cast<Index>(K.rows());
    const double kInf = 1e300;
    std::vector<double> c(static_cast<std::size_t>(n * n), kInf);
    Vec colmax = Vec::Zero(n);
    for (Index i = 0; i < n; ++i) {
        for (SpMatRM::InnerIterator e(K, i); e; ++e) {
            const auto j = static_cast<Index>(e.col());
            const double a = std::abs(e.value());
            colmax(j) = std::max(colmax(j), a);
            colmax(i) = std::max(colmax(i), a);
        }
    }
    for (Index i = 0; i < n; ++i) {
        for (SpMatRM::InnerIterator e(K, i); e; ++e) {
            const auto j = static_cast<Index>(e.col());
            const double a = std::abs(e.value());
            if (a > 0.0) {
                c[static_cast<std::size_t>(i * n + j)] = std::log(colmax(j)) - std::log(a);
                c[static_cast<std::size_t>(j * n + i)] = std::log(colmax(i)) - std::log(a);
            }
        }
    }
    // Hungarian, 1-based (u rows, v cols, p[j] row matched to col j).
    std::vector<double> u(n + 1, 0.0), v(n + 1, 0.0), minv(n + 1);
    std::vector<Index> p(n + 1, 0), way(n + 1, 0);
    std::vector<char> used(n + 1);
    const double big = 1e250;
    auto cost = [&](Index i, Index j) {
        const double x = c[static_cast<std::size_t>((i - 1) * n + (j - 1))];
        return x >= kInf ? big : x;
    };
    for (Index i = 1; i <= n; ++i) {
        p[0] = i;
        Index j0 = 0;
        std::fill(minv.begin(), minv.end(), 1e300);
        std::fill(used.begin(), used.end(), 0);
        do {
            used[j0] = 1;
            const Index i0 = p[j0];
            double delta = 1e300;
            Index j1 = 0;
            for (Index j = 1; j <= n; ++j) {
                if (!used[j]) {
                    const double cur = cost(i0, j) - u[i0] - v[j];
                    if (cur < minv[j]) {
                        minv[j] = cur;
                        way[j] = j0;
                    }
                    if (minv[j] < delta) {
                        delta = minv[j];
                        j1 = j;
                    }
                }
            }
            for (Index j = 0; j <= n; ++j) {
                if (used[j]) {
                    u[p[j]] += delta;
                    v[j] -= delta;
                } else {
                    minv[j] -= delta;
                }
            }
            j0 = j1;
        } while (p[j0] != 0);
        do {
            const Index j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while (j0);
    }
    Vec d = Vec::Ones(n);
    for (Index j = 1; j <= n; ++j) {
        const Index i = p[j];
        if (i == 0 || cost(i, j) >= big * 0.5 || !(colmax(j - 1) > 0.0))
            continue;
    }
    for (Index i = 0; i < n; ++i) {
        const double dr = std::exp(u[i + 1]);
        const double dc = colmax(i) > 0.0 ? std::exp(v[i + 1]) / colmax(i) : 1.0;
        const double di = std::sqrt(dr * dc);
        d(i) = (std::isfinite(di) && di > 0.0) ? di : 1.0;
    }
    return d;
}
SpMatRM seq_congruent(const SpMatRM &K, const Vec &d) {
    SpMatRM KD = K;
    for (Index i = 0; i < static_cast<Index>(K.rows()); ++i) {
        for (SpMatRM::InnerIterator e(KD, i); e; ++e) {
            e.valueRef() *= d(i) * d(static_cast<Index>(e.col()));
        }
    }
    return KD;
}
double seq_probe_eta(const SpMatRM &K, const std::function<Vec(const Vec &)> &solve) {
    Vec d, r;
    equilibration_scale_into(K, d, r);
    const auto n = static_cast<Index>(K.rows());
    Vec u(n);
    std::uint32_t st = 0x9E3779B9u;
    for (Index i = 0; i < n; ++i) {
        st = st * 1664525u + 1013904223u;
        u(i) = (2.0 * (static_cast<double>(st >> 8) / 16777216.0) - 1.0) * d(i);
    }
    const Vec b = K.template selfadjointView<Eigen::Upper>() * u;
    const Vec y = solve(b);
    Vec acc;
    const double kn = symmetric_inf_norm(K, &d, acc);
    return dense_backward_error(K, kn, b, y, r, &d);
}

// A platform outcome's perturbed-pivot report, recorded for the solve check; in
// matched mode a perturbed platform factorization is retaken on the matched
// session (analyzed on the current values: the matching is value-dependent). A
// retake that does not return kOk leaves the platform numerics in place.
hven::linear::FactorizeOutcome platform_response(KktFactor &k, const SpMatRM &K,
                                                 hven::linear::FactorizeOutcome outcome) {
    if (!(outcome.status == hven::linear::FactorizeOutcome::Status::kOk &&
          outcome.arm == hven::linear::FactorArm::kSparse &&
          outcome.inertia.perturbed_pivots.has_value() && *outcome.inertia.perturbed_pivots > 0)) {
        return outcome;
    }
    k.perturbed_pivots = *outcome.inertia.perturbed_pivots;
    if (!k.matched_mode && std::getenv("HVEN_SEQ_FORCE")) { // DEVDBG
        enter_matched_mode(k);
    }
    if (!k.matched_mode) {
        return outcome;
    }
    const int arm = seq_arm(); // DEVDBG
    SpMatRM KD;
    const SpMatRM *Kf = &K;
    k.matched_scale.resize(0);
    if (arm == 2 || arm == 3) {
        const Vec d = arm == 2 ? seq_mc64_scale(K) : equilibration_scale(K);
        KD = seq_congruent(K, d);
        Kf = &KD;
        k.matched_scale = d;
    }
    k.matched_session->analyze(*Kf);
    hven::linear::FactorizeOutcome retake = k.matched_session->factorize(
        *Kf, hven::linear::SymmetricFactor::PatternCheck::kAssumeAnalyzed);
    ++k.matched_refactors;
    if (std::getenv("HVEN_SEQ_DBG") &&
        retake.status == hven::linear::FactorizeOutcome::Status::kOk) { // DEVDBG
        const double eraw = seq_probe_eta(K, [&](const Vec &b) {
            Vec x(b.size());
            k.factor.solve(b, x);
            return x;
        });
        const Vec ds = k.matched_scale;
        const double earm = seq_probe_eta(K, [&](const Vec &b) {
            Vec x(b.size());
            if (ds.size() > 0) {
                k.matched_session->solve(b.cwiseProduct(ds), x);
                return Vec(x.cwiseProduct(ds));
            }
            k.matched_session->solve(b, x);
            return x;
        });
        std::fprintf(stderr,
                     "SEQDBG arm=%d n=%ld raw_pert=%ld raw_in=%ld/%ld raw_eta=%.2e | arm_pert=%ld "
                     "arm_in=%ld/%ld arm_eta=%.2e\n",
                     arm, (long)K.rows(), (long)k.perturbed_pivots, (long)outcome.inertia.positive,
                     (long)outcome.inertia.negative, eraw,
                     (long)retake.inertia.perturbed_pivots.value_or(-1),
                     (long)retake.inertia.positive, (long)retake.inertia.negative, earm);
    }
    if (retake.status != hven::linear::FactorizeOutcome::Status::kOk) {
        return outcome;
    }
    k.matched_numerics = true;
    k.perturbed_pivots = 0;
    return retake;
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
    } else {
        outcome = platform_response(k, K, std::move(outcome));
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
    } else {
        outcome = platform_response(k, K, std::move(outcome));
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
    if (k.matched_numerics) {
        if (k.matched_scale.size() > 0) { // DEVDBG
            k.matched_session->solve(rhs.cwiseProduct(k.matched_scale), x);
            x.array() *= k.matched_scale.array();
            return x;
        }
        k.matched_session->solve(rhs, x);
        return x;
    }
    k.factor.solve(rhs, x);
    return x;
}

hven::linear::InertiaEvidence kkt_inertia(const KktFactor &k) {
    return k.matched_numerics ? k.matched_session->inertia() : k.factor.inertia();
}

bool platform_solve_failed(const KktFactor &k, const SpMatRM &K, const Vec &rhs, const Vec &y) {
    if (!platform_solve_check_applies(k)) {
        return false;
    }
    Vec d;
    Vec r;
    equilibration_scale_into(K, d, r);
    const double k_norm = symmetric_inf_norm(K, &d, r);
    const double eta = dense_backward_error(K, k_norm, rhs, y, r, &d);
    if (std::getenv("HVEN_SEQ_DBG"))
        std::fprintf(stderr, "SEQDBG check n=%ld pert=%ld eta=%.2e\n", (long)K.rows(),
                     (long)k.perturbed_pivots, eta); // DEVDBG
    return !(eta <= kPlatformSolveFailureTol);
}

void enter_matched_mode(KktFactor &k) {
    if (k.matched_mode) {
        return;
    }
    hven::linear::SymmetricFactor::Options o =
        sqp_kkt_options(k.factor.threads(), hven::linear::DensePolicy::kNever);
    const int arm = seq_arm(); // DEVDBG
    o.weighted_matching = arm == 0 || arm == 1;
    o.matrix_scaling = arm == 0;
    k.matched_session = std::make_unique<hven::linear::SymmetricFactor>(o);
    k.matched_mode = true;
    k.matched_rebuild_pending = true;
}

} // namespace hven::detail
