// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// certificate_seam.h — a test seam on the kOptimal CERTIFICATE of the
// primal active-set walk, on the `HVEN_TESTING` convention docs/testing.md
// describes ("A fourth seam, on the certificate").
//
// WHAT IT DOES. Armed, it replaces the iterate `x` and the bound multipliers
// `z` that `QpEngine::run`'s certificate is about to read with a pair the
// fixture supplies, at the one point the certificate's BOUND HALF begins (M7
// W2 E4). Everything after that point — the bound test, the status it assigns
// and the export of the solution — is the production code, unmodified. That
// is the whole purpose: a point slightly outside its box but inside the
// structural margin is not reachable through a natural walk (the walk re-pins
// what it blocks on), so without this seam the tolerance-preserving half of
// the predicate has no discriminating fixture (Codex E4 review, Important 1).
//
// IT IS AN INJECTOR, and it is narrow: it fires only on the would-be-kOptimal
// branch, only while armed, and it counts its presentations so a fixture can
// prove it was reached. Unarmed it does nothing at all.
//
// PER-THREAD, on `QpWalkObserver`'s conventions and for its reasons.
//
// It compiles to NOTHING without `HVEN_TESTING`, and it is included from
// INSIDE `walk_trace.h`'s own `#ifdef HVEN_TESTING` block, so no production
// translation unit opens it or gains a line.

#ifdef HVEN_TESTING

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <fmt/format.h>

#include <hven/core/types.h>
#include <hven/qp/qp_types.h>

namespace hven::detail::testing {

struct QpCertificateSeam {
    /// Armed: the next certificate on this thread reads `x` and `z` below.
    static inline thread_local bool armed = false;
    /// The iterate and the bound multipliers presented to the certificate.
    /// Their sizes must equal the problem's `n`.
    static inline thread_local Vec x{};
    static inline thread_local Vec z{};
    /// Present on the FIRST read only: a later read (the incumbent's, after a reverted
    /// polish) then sees the walk's own point and is merely counted.
    static inline thread_local bool first_only = false;
    /// How many times a certificate read the presented pair on this thread.
    static inline thread_local Index presentations = 0;
    /// The effective `dual_mu` / `feas_tol` the certificate ran under, as of
    /// the last presentation, so a fixture computes its margin from the
    /// numbers the predicate actually used.
    static inline thread_local double eff_dual_mu = -1.0;
    static inline thread_local double eff_feas_tol = -1.0;

    static void reset() {
        armed = false;
        first_only = false;
        x = Vec{};
        z = Vec{};
        presentations = 0;
        eff_dual_mu = -1.0;
        eff_feas_tol = -1.0;
    }
};

/// The hook, called by the certificate's bound half immediately before it
/// reads `x` and `z`. Unarmed it returns without touching anything.
inline void qp_certificate_present(Vec &x, Vec &z, const QpOptions &eff_opts) {
    using S = QpCertificateSeam;
    if (!S::armed) {
        return;
    }
    if (S::x.size() != x.size() || S::z.size() != z.size()) {
        throw std::invalid_argument(
            fmt::format("QpCertificateSeam: presented sizes x={} z={} do not match n={}",
                        S::x.size(), S::z.size(), x.size()));
    }
    const bool present = !(S::first_only && S::presentations > 0);
    if (present) {
        x = S::x;
        z = S::z;
    }
    S::eff_dual_mu = eff_opts.dual_regularization;
    S::eff_feas_tol = eff_opts.feasibility_tol;
    ++S::presentations;
}

/// KOPT-1X R2: a REPAIR-PATH injector. Armed, it moves the iterate along one equality row's
/// normal to `floor_units` floor units of that row's residual, immediately before the walk's
/// successful rank-repair `continue` and with every walk flag (the gray hold included) left
/// as it stood. The pass after the repair then meets a gray-zone row on the production
/// statements. One-shot per arming; counts its firings.
struct QpRepairSeam {
    static inline thread_local bool armed = false;
    static inline thread_local Index row = 0;
    static inline thread_local double floor_units = 0.0;
    /// Fired at most this many times per arming.
    static inline thread_local Index max_fires = 1;
    static inline thread_local Index fires = 0;
    /// The row's residual in floor units before and after the last firing.
    static inline thread_local double ratio_before = 0.0;
    static inline thread_local double ratio_after = 0.0;
    /// Also set the gray-hold flag at the firing (a pass that held, then repaired).
    static inline thread_local bool force_hold = false;
    static inline thread_local Index minor_at_fire = -1;
    static inline thread_local bool hold_at_fire = false;
    /// `gray_hold_prev` at the first pass after the firing (-1 before it): the hold a repair
    /// `continue` must not have spent.
    static inline thread_local int prev_after_fire = -1;

    static void reset() {
        armed = false;
        row = 0;
        floor_units = 0.0;
        max_fires = 1;
        fires = 0;
        ratio_before = 0.0;
        ratio_after = 0.0;
        force_hold = false;
        minor_at_fire = -1;
        hold_at_fire = false;
        prev_after_fire = -1;
    }
};

/// Called at the top of every pass: records what the pass after the firing inherited.
inline void qp_repair_seam_note_prev(bool gray_hold_prev) {
    using S = QpRepairSeam;
    if (S::armed && S::fires > 0 && S::prev_after_fire < 0) {
        S::prev_after_fire = gray_hold_prev ? 1 : 0;
    }
}

/// The hook, called before the successful rank-repair `continue`. Unarmed it returns at once.
inline void qp_post_repair_perturb(const QpProblem &qp, Vec &x, const QpOptions &eff_opts,
                                   Index minor, bool &gray_hold) {
    using S = QpRepairSeam;
    if (!S::armed || S::fires >= S::max_fires || S::row < 0 || S::row >= qp.me()) {
        return;
    }
    const Vec a = qp.Ae.row(S::row).transpose();
    const double b = qp.be(S::row);
    const double scale =
        std::max({1.0, std::abs(b), a.cwiseAbs().sum() * x.lpNorm<Eigen::Infinity>()});
    const double floor = eff_opts.feasibility_tol * scale;
    S::ratio_before = std::abs(a.dot(x) - b) / floor;
    const double target = S::floor_units * floor;
    x += ((target - (a.dot(x) - b)) / a.squaredNorm()) * a;
    S::ratio_after = std::abs(a.dot(x) - b) / floor;
    S::minor_at_fire = minor;
    S::hold_at_fire = gray_hold;
    if (S::force_hold) {
        gray_hold = true;
    }
    ++S::fires;
}

} // namespace hven::detail::testing

#endif // HVEN_TESTING
