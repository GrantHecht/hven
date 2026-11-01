// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// qp_certificate_seam.h — a test seam on the kOptimal CERTIFICATE of the
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
// INSIDE `qp_walk_trace.h`'s own `#ifdef HVEN_TESTING` block, so no production
// translation unit opens it or gains a line.

#ifdef HVEN_TESTING

#include <stdexcept>

#include <fmt/format.h>

#include <hven/core/types.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers::detail::testing {

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
    S::eff_dual_mu = eff_opts.dual_mu;
    S::eff_feas_tol = eff_opts.feas_tol;
    ++S::presentations;
}

} // namespace hven::solvers::detail::testing

#endif // HVEN_TESTING
