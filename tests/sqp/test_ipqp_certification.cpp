// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M6 W1 task 5: the IPQP tier's certification vocabulary, five-way escape census, and the
// section 6.1-6.3 ladders. Only what a LEGAL subproblem reaches on a real backend (seam-only
// states: test_ipqp_seams.cpp); every value pin carries a fallible neighbouring fixture.

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/detail/qp/ssn_engine.h>

#include "support/indefinite_fixtures.h"
#include "support/ipqp_test_support.h"

namespace hven::solvers {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

SpMatRM dense_upper(const std::vector<std::vector<double>> &a) {
    const Index n = static_cast<Index>(a.size());
    SpMatRM m(n, n);
    std::vector<Eigen::Triplet<double>> t;
    for (Index i = 0; i < n; ++i) {
        for (Index j = i; j < n; ++j) {
            if (a[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] != 0.0) {
                t.emplace_back(static_cast<int>(i), static_cast<int>(j),
                               a[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]);
            }
        }
    }
    m.setFromTriplets(t.begin(), t.end());
    m.makeCompressed();
    return m;
}

SpMatRM dense_rows(const std::vector<std::vector<double>> &a, Index cols) {
    const Index rows = static_cast<Index>(a.size());
    SpMatRM m(rows, cols);
    std::vector<Eigen::Triplet<double>> t;
    for (Index i = 0; i < rows; ++i) {
        for (Index j = 0; j < cols; ++j) {
            if (a[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] != 0.0) {
                t.emplace_back(static_cast<int>(i), static_cast<int>(j),
                               a[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]);
            }
        }
    }
    m.setFromTriplets(t.begin(), t.end());
    m.makeCompressed();
    return m;
}

Vec vec(const std::vector<double> &v) {
    Vec out(static_cast<Index>(v.size()));
    for (std::size_t i = 0; i < v.size(); ++i) {
        out(static_cast<Index>(i)) = v[i];
    }
    return out;
}

QpOptions tight_opts() {
    QpOptions o;
    o.tr_radius = kInf;
    return o;
}

/// A strictly convex, well-conditioned QP with both row blocks -- the control
/// fixture every non-vacuity partner below is taken against.
QpProblem convex_qp() {
    QpProblem qp;
    qp.H = dense_upper({{2.0, 0.0}, {0.0, 2.0}});
    qp.g = vec({-2.0, -4.0});
    qp.Ae = dense_rows({{1.0, -1.0}}, 2);
    qp.be = vec({0.5});
    qp.Ai = dense_rows({{1.0, 1.0}}, 2);
    qp.bi = vec({1.0});
    qp.lower = vec({-10.0, -10.0});
    qp.upper = vec({10.0, 10.0});
    return qp;
}

/// THE A11 PARAMETRIC INDEFINITE FAMILY at QP level: min 1/2 x0^2 - x1^2 - c x0 + 1/4 x1 on
/// [-1, 1]^2. The KKT residual vanishes at BOTH the minimizer (c, -1) and the interior saddle
/// (c, 1/8), so only a section 2.2 item 4 inertia read separates them.
QpProblem indefinite_box_qp(double c) {
    QpProblem qp;
    qp.H = dense_upper({{1.0, 0.0}, {0.0, -2.0}});
    qp.g = vec({-c, 0.25});
    qp.Ae = dense_rows({}, 2);
    qp.be = Vec(0);
    qp.Ai = dense_rows({}, 2);
    qp.bi = Vec(0);
    qp.lower = vec({-1.0, -1.0});
    qp.upper = vec({1.0, 1.0});
    return qp;
}

/// The saddle fixture task 4's `TheFinalReadCatchesASaddle...` uses: `H =
/// diag(2, -1)`, `g = 0`, a SYMMETRIC box, so the origin is an exact KKT point
/// of the barrier problem at every `mu` and the tier converges to it.
QpProblem saddle_qp() {
    QpProblem qp;
    qp.H = dense_upper({{2.0, 0.0}, {0.0, -1.0}});
    qp.g = vec({0.0, 0.0});
    qp.Ae = dense_rows({}, 2);
    qp.be = Vec(0);
    qp.Ai = dense_rows({}, 2);
    qp.bi = Vec(0);
    qp.lower = vec({-10.0, -10.0});
    qp.upper = vec({10.0, 10.0});
    return qp;
}

/// How many of the five census fields are nonzero.
Index census_entries(const IpqpCounters &c) {
    return (c.ipqp_escape_budget != 0 ? 1 : 0) + (c.ipqp_escape_stall != 0 ? 1 : 0) +
           (c.ipqp_escape_indefinite != 0 ? 1 : 0) + (c.ipqp_escape_numerical != 0 ? 1 : 0) +
           (c.ipqp_escape_infeasible_suspect != 0 ? 1 : 0);
}

} // namespace

// ---------------------------------------------------------------------------
// The status vocabulary ruling (plan section 7 note (j), carried item 2)
// ---------------------------------------------------------------------------

TEST(IpqpCertificationTest, ADowngradedCertificateReportsNumericalErrorAndNotANewQpStatus) {
    // TASK 5'S RULING, PINNED SO A CHANGE IS DELIBERATE: a downgraded-without-escape outcome
    // is `QpStatus::kNumericalError`, with NO new enumerator; the static_assert also pins the
    // enum unchanged. Plan section 7 note (j); argued in ipqp_engine.h's STATUS VOCABULARY.
    static_assert(static_cast<int>(QpStatus::kNumericalError) == 3,
                  "QpStatus::kNumericalError is the downgraded-certificate spelling; a widened "
                  "QpStatus must revisit ipqp_engine.h's STATUS VOCABULARY ruling.");

    IpqpOptions io;
    io.ipqp_require_final_inertia = false;
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(convex_qp(), nullptr, io, SolveOverrides{});

    // It CONVERGED -- this is not a failure dressed up as one.
    EXPECT_GT(r.counters.ipqp_iters, 0);
    EXPECT_LE(r.residuals.worst(), tight_opts().opt_tol * io.ipqp_converge_slack);

    // ... and reports a downgrade with NO escape.
    EXPECT_EQ(r.counters.ipqp_final_inertia_read, 3);
    EXPECT_TRUE(r.certificate_downgraded);
    EXPECT_FALSE(r.inertia_evidence_failed);
    EXPECT_EQ(r.escape_reason, IpqpEscape::kNone);
    EXPECT_EQ(r.status, QpStatus::kNumericalError);
    EXPECT_NE(r.status, QpStatus::kOptimal);

    // NO CENSUS ENTRY AND NO SECTION 6.1 CHARGE (note (j)): value 3 is a
    // downgrade, never its own escape class, and must NOT be folded into
    // `ipqp_escape_numerical`.
    EXPECT_EQ(r.counters.ipqp_escapes, 0);
    EXPECT_EQ(census_entries(r.counters), 0);
    EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(r.counters));

    // THE MUTATION PARTNER: the identical fixture with the read switched ON
    // certifies, so every assertion above is about the OPTION and not about a
    // problem the tier cannot solve.
    IpqpEngine on(tight_opts());
    const IpqpResult ok = on.solve(convex_qp(), nullptr, IpqpOptions{}, SolveOverrides{});
    EXPECT_EQ(ok.counters.ipqp_final_inertia_read, 0);
    EXPECT_FALSE(ok.certificate_downgraded);
    EXPECT_EQ(ok.status, QpStatus::kOptimal);
    EXPECT_EQ(ok.counters.ipqp_escapes, 0);
}

// ---------------------------------------------------------------------------
// The five-way escape census
// ---------------------------------------------------------------------------

TEST(IpqpCensusTest, EveryEscapeIncrementsExactlyOneCensusFieldAndIpqpEscapes) {
    // THE PARTITION CLAIM, exercised on every escape class reachable from a legal fixture.
    // `kStall` and `kInfeasibleSuspect` have their own fixtures below; `kNumerical`'s terminal
    // inertia forms are the seam file's.
    struct Case {
        const char *name;
        QpProblem qp;
        IpqpOptions io;
        IpqpEscape expect;
    };

    IpqpOptions budget_io;
    budget_io.ipqp_hard_iter_cap = 1;

    std::vector<Case> cases;
    cases.push_back({"budget", convex_qp(), budget_io, IpqpEscape::kBudget});
    cases.push_back({"indefinite", saddle_qp(), IpqpOptions{}, IpqpEscape::kIndefinite});

    for (const Case &c : cases) {
        SCOPED_TRACE(c.name);
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(c.qp, nullptr, c.io, SolveOverrides{});
        ASSERT_EQ(r.escape_reason, c.expect);
        EXPECT_EQ(r.counters.ipqp_escapes, 1);
        EXPECT_EQ(census_entries(r.counters), 1);
        EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(r.counters));
    }
}

TEST(IpqpCensusTest, ADeclinedSubproblemIsNotAnEscapeAndCensusesNothing) {
    // `ipqp_declined_pinned`'s settled text, exercised: the tier never ran, so a decline never
    // counts toward `ipqp_escapes` or the K=3 retirement threshold -- the census stays empty
    // even though the status is `kNumericalError`.
    QpProblem qp = convex_qp();
    qp.lower(1) = 3.0;
    qp.upper(1) = 3.0;

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_TRUE(r.declined_pinned);
    EXPECT_EQ(r.escape_reason, IpqpEscape::kNone);
    EXPECT_EQ(r.counters.ipqp_escapes, 0);
    EXPECT_EQ(census_entries(r.counters), 0);
    EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(r.counters));
}

// ---------------------------------------------------------------------------
// A11, engine half: the parametric indefinite family
// ---------------------------------------------------------------------------

TEST(IpqpA11Test, TheIndefiniteFamilyRidesNegativeCurvatureToABoundAndCertifiesHonestly) {
    // A11's four claims on the parametric family, RE-DERIVED AT T4b: every member now walks the
    // negative curvature to a BOUND (never the interior saddle) and reaches the section 2.2 item
    // 4 read. Swept over `c` so the pins are the mechanism's. `.superpowers/w1-t4b-report.md`.
    Index reached_read = 0;
    Index armed = 0;
    Index non_monotone = 0;
    for (const double c : {0.0, 0.25, 0.5, 0.75, 1.0}) {
        SCOPED_TRACE(c);
        IpqpEngine tier(tight_opts());
        const IpqpResult r =
            tier.solve(indefinite_box_qp(c), nullptr, IpqpOptions{}, SolveOverrides{});

        // (1) THE GATE FIRED. `rho_demanded_max` is the modification's
        // high-water mark and starts at 0, so a nonzero reading is the gate
        // having refused a factorization on its inertia.
        EXPECT_GT(r.counters.ipqp_rho_demanded_max, 0.0);
        EXPECT_GT(r.counters.ipqp_inertia_retries, 0);
        ++armed;

        // (2) THE LADDER IS ALGORITHM IC'S, NOT A MONOTONE FLOOR: the memory is free to fall.
        // `<=` is asserted on every member (a memory above the high-water mark is a bookkeeping
        // bug) and the strict inequality is COUNTED, so non-monotonicity is a measurement.
        EXPECT_LE(r.counters.ipqp_rho_demanded_last, r.counters.ipqp_rho_demanded_max);
        if (r.counters.ipqp_rho_demanded_last < r.counters.ipqp_rho_demanded_max) {
            ++non_monotone;
        }

        // (3) IT CONVERGES AND THE CERTIFICATE IS HONEST: the point is at a BOUND of the
        // negative-curvature coordinate, where `H + Sigma` really is positive definite, and NOT
        // the interior saddle at `x1 = 1/8` -- the discriminating assertion here.
        EXPECT_EQ(r.status, QpStatus::kOptimal);
        EXPECT_EQ(r.escape_reason, IpqpEscape::kNone);
        EXPECT_FALSE(r.certificate_downgraded);
        EXPECT_EQ(r.counters.ipqp_escapes, 0);
        EXPECT_EQ(census_entries(r.counters), 0);
        EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(r.counters));
        EXPECT_NEAR(r.x(0), c, 1e-5);
        EXPECT_NEAR(std::abs(r.x(1)), 1.0, 1e-6) << "landed at a bound, not in the interior";
        EXPECT_GT(std::abs(r.x(1) - 0.125), 0.5) << "and specifically NOT at the interior saddle";
        EXPECT_LE(r.residuals.worst(), tight_opts().opt_tol * IpqpOptions{}.ipqp_converge_slack);

        // (4) THE FINAL READ HAPPENED AND AGREED. `0` is also the structural value on a solve
        // that never reached the read, so the certifying exit above disambiguates and the
        // option-off partner below proves the factorization is genuinely paid.
        EXPECT_EQ(r.counters.ipqp_final_inertia_read, 0);
        ++reached_read;

        // (5) THE MODIFICATION IS ABSENT FROM THE ANSWER: `rho` is the schedule's final value
        // and `rho_mod` the modification the last step ran at, while the read is taken at
        // `rho_dem = 0` by construction -- what makes `read == 0` a claim about the caller's QP.
        EXPECT_LT(r.rho, IpqpOptions{}.ipqp_rho_init);
        EXPECT_GE(r.rho_mod, 0.0);
        EXPECT_LE(r.rho_mod, r.counters.ipqp_rho_demanded_max);
    }

    EXPECT_EQ(armed, 5);
    EXPECT_EQ(reached_read, 5) << "every member reaches the section 2.2 item 4 read";
    EXPECT_GT(non_monotone, 0) << "and at least one settles below its own peak -- the executable "
                                  "statement that the monotone floor is gone";

    // THE READ IS REALLY BEING PAID: the same fixture with `ipqp_require_final_inertia` off
    // reports `3` (NOT PERFORMED), a downgrade with no escape, and one factorization fewer.
    // Without this partner, `read == 0` would be compatible with never taking the read.
    IpqpOptions off;
    off.ipqp_require_final_inertia = false;
    IpqpEngine on_tier(tight_opts());
    IpqpEngine off_tier(tight_opts());
    const IpqpResult on =
        on_tier.solve(indefinite_box_qp(0.5), nullptr, IpqpOptions{}, SolveOverrides{});
    const IpqpResult offr = off_tier.solve(indefinite_box_qp(0.5), nullptr, off, SolveOverrides{});
    EXPECT_EQ(offr.counters.ipqp_final_inertia_read, 3);
    EXPECT_TRUE(offr.certificate_downgraded);
    EXPECT_EQ(offr.escape_reason, IpqpEscape::kNone);
    EXPECT_EQ(on.counters.ipqp_factorizations, offr.counters.ipqp_factorizations + 1)
        << "exactly one factorization is the item 4 read's whole cost";
}

TEST(IpqpA11Test, TheHSIndefiniteRowsConvergeAndReachTheRequiredFinalRead) {
    // A11'S ROW HALF (spec section 8.4), MET AS OF M6 W1 T4b and NOT at W1 head: all three HS
    // indefinite rows now converge to a local minimizer, reach the section 2.2 item 4 read, and
    // report an honest standing certificate. `.superpowers/w1-t4b-report.md`.
    RecordProperty("a11_hs_half", "MET at M6 W1 T4b: all three HS indefinite rows converge, reach "
                                  "the section 2.2 item 4 read and certify at a derived local "
                                  "minimizer");

    // THE ADMISSIBLE TRAJECTORIES, EXACT, ONE ROW PER MEASURED OUTCOME. Two entries for the
    // first row because `indefinite_equality_qp` has two local minimizers that Debug and Release
    // tie differently; a result must match ONE entirely. `.superpowers/w1-t4b-report.md`.
    struct Outcome {
        Index iters;
        Index facts;
        Vec x;
    };
    struct HsCase {
        const char *name;
        QpProblem qp;
        std::vector<Outcome> admissible;
    };
    const std::vector<HsCase> hs = {
        {"hs_indefinite_equality",
         test_support::indefinite_equality_qp(),
         // FIX ROUND 1: T10b's Debug/Release split is BUILD-SCOPED, not admitted cross-build --
         // Debug takes 31/42 to the SAME point Release reaches in 14/20, and neither build may
         // regress to the other's cost unnoticed. `.superpowers/w1-t10b-fix1-report.md` item D.
         {
#ifdef NDEBUG
             {14, 20, vec({1.0, -2.0, 2.0})},
#else
             {31, 42, vec({1.0, -2.0, 2.0})},
#endif
             {24, 36, vec({1.0, 2.0, -2.0})}}},
        // x = (1, -2, -0.5), objective -1.375: the second of that fixture's
        // two derived minimizers, x1 at LOWER and the general row active.
        {"hs_indefinite_equality_and_row",
         test_support::indefinite_equality_and_row_qp(),
         {{14, 21, vec({1.0, -2.0, -0.5})}}},
        // x = (-2, -2, -0.1), objective -4.205: one of the five vertices that
        // fixture's header enumerates, row slack. A LOCAL and not the global
        // minimizer, which is exactly what that fixture exists to accept.
        {"hs_two_negative_eigenvalue_row",
         test_support::two_negative_eigenvalue_row_qp(),
         {{27, 38, vec({-2.0, -2.0, -0.1})}}},
    };

    Index reached_read_hs = 0;
    Index descended = 0;
    for (const HsCase &c : hs) {
        SCOPED_TRACE(c.name);
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(c.qp, nullptr, IpqpOptions{}, SolveOverrides{});

        // A11 CLAIM 1 -- THE INERTIA GATE FIRES on a CONSTRAINED indefinite KKT. Both counters,
        // since either alone can pass without meaning: `inertia_retries` counts factorizations
        // the gate REFUSED, `rho_demanded_max` the level it demanded; both are 0 when convex.
        EXPECT_GT(r.counters.ipqp_inertia_retries, 0);
        EXPECT_GT(r.counters.ipqp_rho_demanded_max, 0.0);

        // A11 CLAIM 2 -- THE LADDER IS ALGORITHM IC'S, not the monotone floor's `last == max`.
        // The memory ends BELOW the peak on every one of these rows, which is what lets the
        // walk finish: a floor pinned at the first climb's overshoot is what froze them.
        EXPECT_LE(r.counters.ipqp_rho_demanded_last, r.counters.ipqp_rho_demanded_max);
        descended += r.counters.ipqp_rho_demanded_last < r.counters.ipqp_rho_demanded_max ? 1 : 0;
        EXPECT_GT(r.counters.ipqp_iters_at_elevated_rho, 0);

        // A11 CLAIMS 3 AND 4 -- THE READ HAPPENS AND ITS CERTIFICATE IS HONEST. Here it STANDS
        // (each row really converges to a local minimizer); the "a wrong read downgrades" half
        // is carried by TheFinalReadIsReachedOnAConstrainedIndefiniteKkt and the settled band.
        EXPECT_EQ(r.status, QpStatus::kOptimal);
        EXPECT_EQ(r.escape_reason, IpqpEscape::kNone);
        EXPECT_FALSE(r.certificate_downgraded);
        EXPECT_EQ(r.counters.ipqp_final_inertia_read, 0);
        EXPECT_EQ(r.counters.ipqp_escapes, 0);
        EXPECT_EQ(census_entries(r.counters), 0);
        EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(r.counters));
        EXPECT_LE(r.residuals.worst(), tight_opts().opt_tol * IpqpOptions{}.ipqp_converge_slack);
        ++reached_read_hs;

        // WHERE IT LANDED AND WHAT IT COST, matched as ONE WHOLE TRAJECTORY against the
        // admissible list, so the point and its exact iteration and factorization counts are
        // pinned together and a ladder change cannot be absorbed by a loose bound on either.
        RecordProperty(std::string(c.name) + "_iters", std::to_string(r.counters.ipqp_iters));
        RecordProperty(std::string(c.name) + "_factorizations",
                       std::to_string(r.counters.ipqp_factorizations));
#ifdef USE_ACCELERATE_SPARSE
        // The admissible list is an exact trajectory pin, so it is MKL-scoped
        // and UNOBSERVED elsewhere (CLAUDE.md section 6; T4b F5).
        RecordProperty(std::string(c.name) + "_accelerate",
                       "UNOBSERVED -- the exact trajectory is MKL-only");
#else
        Index matched = 0;
        for (const Outcome &o : c.admissible) {
            ASSERT_EQ(r.x.size(), o.x.size());
            if ((r.x - o.x).lpNorm<Eigen::Infinity>() < 1e-6 && r.counters.ipqp_iters == o.iters &&
                r.counters.ipqp_factorizations == o.facts) {
                ++matched;
            }
        }
        EXPECT_EQ(matched, 1) << "x = " << r.x.transpose() << ", iters = " << r.counters.ipqp_iters
                              << ", factorizations = " << r.counters.ipqp_factorizations
                              << " matches no admissible trajectory";
#endif
        EXPECT_LT(r.counters.ipqp_iters, IpqpOptions{}.ipqp_hard_iter_cap)
            << "and none of them is stopped by the budget any more";
    }

    // **THE FLIPPED PIN.** This line read `EXPECT_EQ(reached_read_hs, 0)` at
    // W1 head, with a comment saying the day it failed was the day A11's HS
    // half was met. That day was T4b.
    EXPECT_EQ(reached_read_hs, 3)
        << "A11 HS half: all three HS indefinite rows reach the section 2.2 item 4 read";

    // CLAIM 2's CENSUS, re-derived at T10b: the memory ends STRICTLY below the
    // peak on two of the three rows; `hs_indefinite_equality` now settles at
    // its own peak (all three did descend at the 0.1 placeholder).
    RecordProperty("a11_rows_that_descended", std::to_string(descended));
#ifndef USE_ACCELERATE_SPARSE
#ifdef NDEBUG
    EXPECT_EQ(descended, 2);
#else
    EXPECT_EQ(descended, 3) << "Debug's longer trajectory on hs0 leaves room to walk down";
#endif
#endif

    // NON-VACUITY FOR THE READ on the HS rows: with `ipqp_require_final_inertia` off the same
    // row reports `3` (NOT PERFORMED) and a downgrade, and pays exactly one factorization
    // fewer. Without it a `read == 0` from a solve that never took the read looks the same.
    IpqpOptions off;
    off.ipqp_require_final_inertia = false;
    IpqpEngine off_tier(tight_opts());
    IpqpEngine on_tier(tight_opts());
    const IpqpResult offr =
        off_tier.solve(test_support::indefinite_equality_qp(), nullptr, off, SolveOverrides{});
    const IpqpResult onr = on_tier.solve(test_support::indefinite_equality_qp(), nullptr,
                                         IpqpOptions{}, SolveOverrides{});
    EXPECT_EQ(offr.counters.ipqp_final_inertia_read, 3);
    EXPECT_TRUE(offr.certificate_downgraded);
    EXPECT_NE(offr.status, QpStatus::kOptimal);
    EXPECT_EQ(offr.counters.ipqp_iters, onr.counters.ipqp_iters)
        << "the option changes only the read, so the iteration the trajectory takes is the same";
    EXPECT_EQ(offr.counters.ipqp_factorizations, onr.counters.ipqp_factorizations - 1)
        << "exactly one factorization is the item 4 read's whole cost";
}

TEST(IpqpA11Test, TheFinalReadIsReachedOnAConstrainedIndefiniteKkt) {
    // A11's claims 3 and 4 -- the required final unregularized inertia read HAPPENS, and a wrong
    // read DOWNGRADES rather than reporting kOptimal -- on a CONSTRAINED indefinite KKT:
    // `saddle_qp()`, plus that same fixture carrying one equality row satisfied at the origin.
    QpProblem saddle_with_row = saddle_qp();
    saddle_with_row.Ae = dense_rows({{1.0, 0.0}}, 2);
    saddle_with_row.be = vec({0.0});

    Index reached_read = 0;
    for (const QpProblem &qp : {saddle_qp(), saddle_with_row}) {
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});

        // NON-VACUOUS BY CONSTRUCTION: the read is ASSERTED to have happened,
        // so a regression to a budget escape fails here rather than skipping
        // the assertion (co-review I-5).
        ASSERT_NE(r.counters.ipqp_final_inertia_read, 0);
        ++reached_read;
        EXPECT_EQ(r.counters.ipqp_final_inertia_read, 1);
        EXPECT_TRUE(r.certificate_downgraded);
        EXPECT_EQ(r.escape_reason, IpqpEscape::kIndefinite);
        EXPECT_NE(r.status, QpStatus::kOptimal);
        EXPECT_EQ(r.counters.ipqp_escape_indefinite, 1);
        EXPECT_EQ(census_entries(r.counters), 1);
        EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(r.counters));
        EXPECT_LE(r.counters.ipqp_iters, 10);
    }
    EXPECT_EQ(reached_read, 2);
}

TEST(IpqpA11Test, TheConvexTwinOfTheFamilyCertifiesAndLeavesTheLadderInert) {
    // THE NON-VACUITY PARTNER for the whole block above: flip the sign of the one negative
    // curvature and every assertion above inverts. Without this the family's pins could pass
    // on an engine that always refused to certify.
    QpProblem qp = indefinite_box_qp(0.5);
    qp.H = dense_upper({{1.0, 0.0}, {0.0, 2.0}});

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});

    EXPECT_DOUBLE_EQ(r.counters.ipqp_rho_demanded_max, 0.0);
    EXPECT_EQ(r.counters.ipqp_inertia_retries, 0);
    EXPECT_EQ(r.status, QpStatus::kOptimal);
    EXPECT_EQ(r.escape_reason, IpqpEscape::kNone);
    EXPECT_EQ(r.counters.ipqp_final_inertia_read, 0);
    EXPECT_FALSE(r.certificate_downgraded);
    EXPECT_EQ(r.counters.ipqp_escapes, 0);
}

// ---------------------------------------------------------------------------
// T4b close gates 7-10: what the separated ladder is and is not
// ---------------------------------------------------------------------------

namespace {

/// T4b close gate 7's fixture: min 1/2 (x0^2 + lam x1^2) s.t. x0 = 0.5, x in [-10, 10]^2. The
/// equality eliminates x0, so the REDUCED Hessian is the scalar `lam`, known analytically; `g1 =
/// 0` and a symmetric box make the SADDLE `x1 = 0` an exact barrier KKT point at every `mu`.
QpProblem equality_null_space_indefinite_qp(double lam) {
    QpProblem qp;
    qp.H = dense_upper({{1.0, 0.0}, {0.0, lam}});
    qp.g = vec({0.0, 0.0});
    qp.Ae = dense_rows({{1.0, 0.0}}, 2);
    qp.be = vec({0.5});
    qp.Ai = dense_rows({}, 2);
    qp.bi = Vec(0);
    qp.lower = vec({-10.0, -10.0});
    qp.upper = vec({10.0, 10.0});
    return qp;
}

/// T4b close gate 8's fixture: `H = diag(2, h)`, `g = 0`, a SYMMETRIC box of half-width `s` on
/// x1, so `x1 = 0` is an exact barrier KKT point at every `mu` with `Sigma = 2 mu / s^2`.
/// `h < 0` makes it a SADDLE whose certificate must FALL; `h > 0` a MINIMIZER that must STAND.
QpProblem weakly_active_indefinite_qp(double s, double h) {
    QpProblem qp;
    qp.H = dense_upper({{2.0, 0.0}, {0.0, h}});
    qp.g = vec({0.0, 0.0});
    qp.Ae = dense_rows({}, 2);
    qp.be = Vec(0);
    qp.Ai = dense_rows({}, 2);
    qp.bi = Vec(0);
    qp.lower = vec({-10.0, -s});
    qp.upper = vec({10.0, s});
    return qp;
}

/// THE SAME FAMILY AT A TARGET `Sigma`, which is what the gate is really about: T4b's own
/// `Sigma = 2 mu_stop / s^2` inverted. A fixed half-width drops out of the weak-active regime
/// the moment the barrier default moves; a target `Sigma` does not.
QpProblem weakly_active_indefinite_qp_at(double sigma, double h, double mu_stop) {
    return weakly_active_indefinite_qp(std::sqrt(2.0 * mu_stop / sigma), h);
}

/// This family's stopping `mu`, MEASURED rather than assumed: `x1 = 0` is an exact barrier KKT
/// point at every `mu`, so the trajectory -- and therefore `mu_stop` -- is independent of both
/// `s` and `h`, and one probe solve fixes the whole ladder. `.superpowers/w1-t4b-report.md`.
double gate8_mu_stop() {
    IpqpEngine probe(tight_opts());
    return probe
        .solve(weakly_active_indefinite_qp(1.0e-4, -1.0), nullptr, IpqpOptions{}, SolveOverrides{})
        .mu;
}

/// T4b close gate 10's fixture: min 1/2 (2 x0^2 - 0.1 x1^2) - 2 x0 - 0.01 x1 on [-100, 100]^2,
/// built to make the negative-curvature walk as long as the design allows (small additional-
/// shift threshold, bound 100 units away, shallow slope) and the section 3.2 gate silent.
QpProblem long_negative_curvature_walk_qp() {
    QpProblem qp;
    qp.H = dense_upper({{2.0, 0.0}, {0.0, -0.1}});
    qp.g = vec({-2.0, -0.01});
    qp.Ae = dense_rows({}, 2);
    qp.be = Vec(0);
    qp.Ai = dense_rows({}, 2);
    qp.bi = Vec(0);
    qp.lower = vec({-100.0, -100.0});
    qp.upper = vec({100.0, 100.0});
    return qp;
}

/// The bound curvature `Sigma_i` the section 2.2 item 4 read sees at index
/// `i`, recomputed from the RESULT rather than from inside the engine -- the
/// same `zl/(x-l) + zu/(u-x)` the tier accumulates.
double sigma_at(const IpqpResult &r, const QpProblem &qp, Index i) {
    double sig = 0.0;
    if (std::isfinite(qp.lower(i))) {
        sig += r.zl(i) / (r.x(i) - qp.lower(i));
    }
    if (std::isfinite(qp.upper(i))) {
        sig += r.zu(i) / (qp.upper(i) - r.x(i));
    }
    return sig;
}

} // namespace

TEST(IpqpLadderBandTest, TheSettledModificationSitsInsideAlgorithmICsOwnBand) {
    // T4b CLOSE GATE 7: with the reduced Hessian known in closed form, the shift the ladder
    // SETTLES at must sit in Algorithm IC's band -- theta < rho_dem_settled <= kIpqpLadderUp *
    // theta, theta = |lambda_min(reduced H)| - rho_sched. `.superpowers/w1-t4b-report.md`.
    const double lam = -2.0;
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(equality_null_space_indefinite_qp(lam), nullptr, IpqpOptions{},
                                    SolveOverrides{});

    // THE PRECONDITIONS THE ANALYTIC BAND RESTS ON, asserted rather than assumed: (a) it
    // converged, so there IS a settled value; (b) the point is the one the fixture was built
    // around; (c) NO BOUND IS ACTIVE, so the reduced Hessian is `lam` and not `lam + Sigma`.
#ifdef USE_ACCELERATE_SPARSE
    RecordProperty("t4b_gate7_accelerate", "UNOBSERVED -- the exact iteration count is MKL-only");
    ASSERT_GT(r.counters.ipqp_iters, 0);
#else
    // T10b re-derivation (30 at the 0.1 placeholder), and no longer build-invariant:
    // Debug's unvectorized arithmetic takes 30 where Release takes 26.
#ifdef NDEBUG
    ASSERT_EQ(r.counters.ipqp_iters, 26);
#else
    ASSERT_EQ(r.counters.ipqp_iters, 30);
#endif
#endif
    EXPECT_NEAR(r.x(0), 0.5, 1e-9);
    EXPECT_NEAR(r.x(1), 0.0, 1e-6);
    EXPECT_LT(sigma_at(r, equality_null_space_indefinite_qp(lam), 1), 1e-6)
        << "no active bound: Sigma must be numerically absent from the read";

    const double theta = std::abs(lam) - r.rho;
    // T10b RE-DERIVATION (declared): the settled shift is accepted at a LATER iterate than
    // `theta`'s own matrix, so the lower edge is approximate -- at the measured `ipqp_init_mu`
    // it lands 2.7 % under (2.403 at the 0.1 placeholder). One ladder rung is the real edge.
    EXPECT_GT(r.rho_mod, theta / detail::kIpqpLadderDown)
        << "a settled value a whole rung below the threshold was never demanded at all";
    EXPECT_LE(r.rho_mod, detail::kIpqpLadderUp * theta)
        << "and one above 8 x the threshold means the ladder never walked its first climb's "
           "overshoot back down";
    RecordProperty("t4b_gate7_theta", std::to_string(theta));
    RecordProperty("t4b_gate7_rho_mod", std::to_string(r.rho_mod));
    RecordProperty("t4b_gate7_rho_demanded_max", std::to_string(r.counters.ipqp_rho_demanded_max));

    // THE FIRST CLIMB IS *NOT* WHERE IT SETTLES -- the non-vacuity partner, re-derived at T10b
    // against the SETTLED value rather than against `theta` (the measured peak is 7.39, inside
    // `8 theta`, where the 0.1 placeholder's was 100): the ladder walked down a full rung.
    EXPECT_GT(r.counters.ipqp_rho_demanded_max, detail::kIpqpLadderDown * r.rho_mod)
        << "the first climb overshoots by construction; the band is a claim about where the "
           "ladder SETTLES, not about where it starts";

    // ... AND THE POINT IS A SADDLE, so the section 2.2 item 4 read must
    // downgrade. `x1 = 0` maximizes `-x1^2` on a symmetric box.
    EXPECT_EQ(r.counters.ipqp_final_inertia_read, 1);
    EXPECT_TRUE(r.certificate_downgraded);
    EXPECT_EQ(r.escape_reason, IpqpEscape::kIndefinite);
    EXPECT_NE(r.status, QpStatus::kOptimal);

    // THE HS ROWS GET THE OBSERVED-THRESHOLD FORM: with bounds active the analytic band is not
    // valid, so assert `rho_d_settled <= kIpqpLadderUp *` the measured smallest sufficient
    // shift. Both sides run equilibration off (T4b F3; `.superpowers/w1-t4b-report.md`).
    int hs_row = 0;
    Index rows_that_descended = 0;
    for (const QpProblem &qp :
         {test_support::indefinite_equality_qp(), test_support::indefinite_equality_and_row_qp(),
          test_support::two_negative_eigenvalue_row_qp()}) {
        // One suffix per row: a bare key records only the last fixture.
        const std::string row = "_hs" + std::to_string(hs_row++);
        SCOPED_TRACE(row);
        IpqpEngine hs_tier(tight_opts());
        const IpqpResult h = hs_tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
        ASSERT_GT(h.counters.ipqp_rho_demanded_max, 0.0);
        // T10b: the measured default halves these trajectories, so the memory has fewer
        // chances to walk down -- `hs0` now ends AT its peak. The one-rung descent is
        // therefore a census over the rows, not a per-row law.
        EXPECT_LE(h.counters.ipqp_rho_demanded_last, h.counters.ipqp_rho_demanded_max);
        rows_that_descended += h.counters.ipqp_rho_demanded_last <=
                                       h.counters.ipqp_rho_demanded_max / detail::kIpqpLadderDown
                                   ? 1
                                   : 0;
        RecordProperty("t4b_gate7_settled_default" + row,
                       std::to_string(h.counters.ipqp_rho_demanded_last));

        // The comparison, both sides at `dsq == 1`.
        IpqpOptions unscaled;
        unscaled.ipqp_ruiz = false;
        IpqpEngine ref_tier(tight_opts());
        const IpqpResult ref = ref_tier.solve(qp, nullptr, unscaled, SolveOverrides{});
        ASSERT_GT(ref.counters.ipqp_rho_demanded_max, 0.0)
            << "the reference solve must arm the ladder, or there is no settled value to bound";

        double observed_threshold = std::numeric_limits<double>::infinity();
        for (double trial = 128.0; trial >= 1.0 / 1024.0; trial *= 0.5) {
            IpqpOptions probe;
            probe.ipqp_ruiz = false;
            probe.ipqp_rho_init = trial;
            probe.ipqp_hard_iter_cap = 1;
            IpqpEngine probe_tier(tight_opts());
            const IpqpResult pr = probe_tier.solve(qp, nullptr, probe, SolveOverrides{});
            if (pr.counters.ipqp_inertia_retries == 0) {
                observed_threshold = trial;
            } else {
                break;
            }
        }
        ASSERT_TRUE(std::isfinite(observed_threshold))
            << "no trial shift in the sweep sufficed -- the probe is not measuring a threshold";
        RecordProperty("t4b_gate7_observed_threshold" + row, std::to_string(observed_threshold));
        RecordProperty("t4b_gate7_settled_unscaled" + row,
                       std::to_string(ref.counters.ipqp_rho_demanded_last));
        EXPECT_LE(ref.counters.ipqp_rho_demanded_last,
                  2.0 * detail::kIpqpLadderUp * observed_threshold)
            << "settled = " << ref.counters.ipqp_rho_demanded_last
            << ", observed threshold = " << observed_threshold;
    }
    RecordProperty("t4b_gate7_rows_that_descended", std::to_string(rows_that_descended));
#ifndef USE_ACCELERATE_SPARSE
#ifdef NDEBUG
    EXPECT_EQ(rows_that_descended, 2) << "3 of 3 at the 0.1 placeholder";
#else
    EXPECT_EQ(rows_that_descended, 3) << "Debug's longer hs0 trajectory still walks down";
#endif
#endif
}

TEST(IpqpCertificationTest, TheFinalReadVerifiesDirectionsOffAWeaklyActiveBoundInsteadOfMasking) {
    // T4b CLOSE GATE 8, REPAIRED IN FIX ROUND 1 (settler ruling R1): at a weakly active bound
    // the barrier's own `Sigma` masked negative curvature that is IN the critical cone, so
    // saddles certified as kOptimal. The read now drops it. `.superpowers/w1-t4b-report.md`.
    Index dropped_and_downgraded = 0;
    Index kept_and_stood = 0;
    Index skipped_ties = 0;
    // THE SWEEP IS IN `Sigma`, NOT IN `s` (T10b): the same ten members as T4b measured, now
    // CONSTRUCTED at those curvatures instead of at half-widths that only produced them at the
    // 0.1 placeholder's `mu_stop`. `.superpowers/w1-t4b-report.md`.
    const double mu_stop = gate8_mu_stop();
    for (const double target : {0.1, 0.28, 0.63, 0.77, 1.0, 1.28, 1.74, 2.5, 10.0, 250.0}) {
        SCOPED_TRACE(target);
        const QpProblem saddle = weakly_active_indefinite_qp_at(target, -1.0, mu_stop);
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(saddle, nullptr, IpqpOptions{}, SolveOverrides{});

        // The family's invariants, so the sweep is one experiment with one
        // variable moving. The iteration count is an exact trajectory value,
        // so it is MKL-scoped (T4b F5).
#ifdef USE_ACCELERATE_SPARSE
        RecordProperty("t4b_gate8_iters_accelerate",
                       "UNOBSERVED -- the exact iteration count is MKL-only");
        ASSERT_GT(r.counters.ipqp_iters, 0);
#else
        ASSERT_EQ(r.counters.ipqp_iters, 3);
#endif
        EXPECT_NEAR(r.x(1), 0.0, 1e-12);
        EXPECT_DOUBLE_EQ(r.counters.ipqp_rho_demanded_max, 0.0)
            << "the ladder never arms here -- rho_0 = 8 covers |H11| = 1 from the first "
               "factorization, which is what makes this a test of the READ";

        // THE KNIFE-EDGE MEMBER IS SKIPPED STRUCTURALLY, by the target it was BUILT at rather
        // than by where its measured `Sigma` landed (fix round 1, Claude F8): `Sigma == |H11|`
        // leaves the read's matrix near-singular -- the registered tie-flake class.
        if (target == 1.0) {
            ++skipped_ties;
            continue;
        }

        // THE REGIME IS ASSERTED BEFORE THE READ IS, and fatally: an asserted member that did
        // not land at the curvature it was built for makes a moved `mu_stop`, not the read,
        // the variable -- so the sweep stops rather than reporting the read's verdict.
        const double sigma = sigma_at(r, saddle, 1);
        ASSERT_NEAR(sigma / target, 1.0, 1e-2) << "the construction did not hold";

        // THE RULE THE READ NOW APPLIES, recomputed here from the returned point rather than
        // read out of the engine: a bound side is weakly active iff BOTH its gap and its
        // multiplier are below `kIpqpWeakActiveFactor * sqrt(mu)`. Here the two are symmetric.
        const double weak_scale = detail::kIpqpWeakActiveFactor * std::sqrt(r.mu);
        const double gap = r.x(1) - saddle.lower(1);
        const bool weak = gap <= weak_scale && r.zl(1) <= weak_scale;
        // ... AND THE BAND THAT RULE IS, IN `Sigma`: `gap = s`, `z = mu/s`, so a side is weak
        // exactly when `Sigma` lies in `[2/F^2, 2 F^2]` for the shipped factor `F`.
        constexpr double kF = detail::kIpqpWeakActiveFactor;
        EXPECT_EQ(weak, sigma >= 2.0 / (kF * kF) && sigma <= 2.0 * kF * kF);

        // THE WEAK-ACTIVE REGIME ITSELF, on the members that are in it: the
        // slack and the multiplier are both of order `sqrt(mu)`.
        if (weak) {
            EXPECT_NEAR(std::log10(gap / std::sqrt(r.mu)), 0.0, 1.2);
            EXPECT_NEAR(std::log10(r.zl(1) / std::sqrt(r.mu)), 0.0, 1.2);
            // THE REPAIR: the masking curvature is dropped, the read sees
            // `H11 = -1`, and the certificate FALLS -- at a point that is not a
            // minimizer, which is the whole content of gate 8.
            EXPECT_NE(r.counters.ipqp_final_inertia_read, 0)
                << "sigma = " << sigma << " masked a saddle";
            EXPECT_TRUE(r.certificate_downgraded);
            EXPECT_NE(r.status, QpStatus::kOptimal);
            ++dropped_and_downgraded;
        } else {
            // STRONGLY ACTIVE: the direction off the bound is NOT in the critical cone, so the
            // read keeps its curvature (`Sigma` is 250 here, not 1.28). The residual exposure
            // is an accuracy question, recorded in `.superpowers/w1-t4b-report.md`.
            EXPECT_GT(sigma, 1.0);
            EXPECT_EQ(r.counters.ipqp_final_inertia_read, 0);
            // T4c, gate-8 residual C1 (`s = 1e-5`): band-counted AND exponent-suspect, so the
            // flag fires on the DISCRIMINATING count. R3: MKL-scoped -- the margin (11.18 vs
            // floor 10) is boundary-sensitive. See `.superpowers/w1-t4c-report.md`.
            if (target == 250.0) {
#ifdef USE_ACCELERATE_SPARSE
                RecordProperty("t4c_c1_exposed_accelerate",
                               "UNOBSERVED -- band/exponent trigger on this narrow-margin member "
                               "is MKL-only");
#else
                EXPECT_TRUE(r.read_kept_tight);
                EXPECT_EQ(r.counters.ipqp_read_kept_tight_sides, 2);
                EXPECT_EQ(r.counters.ipqp_read_barrier_noise_sides, 2)
                    << "the exponent test must find this member informative and suspect, not "
                       "fall back to the band";
                // T1: e pinned AS A VALUE, not only the discretized count --
                // both sides track the barrier exactly here (z = mu / s).
                EXPECT_GE(r.read_barrier_noise_exponent_min, 0.9);
                EXPECT_LE(r.read_barrier_noise_exponent_max, 1.1);
                RecordProperty("t4c_c1_exposed_kept_tight_sides",
                               std::to_string(r.counters.ipqp_read_kept_tight_sides));
                RecordProperty("t4c_c1_exposed_barrier_noise_sides",
                               std::to_string(r.counters.ipqp_read_barrier_noise_sides));
#endif
            }
            ++kept_and_stood;
        }
    }
    // The existence forms, unconditional on every backend.
    EXPECT_GT(dropped_and_downgraded, 0) << "the repair must fire somewhere, or nothing is tested";
    EXPECT_GT(kept_and_stood, 0) << "and it must NOT fire on a strongly active bound, or the rule "
                                    "is 'drop everything'";
    EXPECT_GT(skipped_ties, 0) << "the sweep crosses the boundary";
    EXPECT_EQ(dropped_and_downgraded + kept_and_stood + skipped_ties, 10)
        << "every swept member is classified exactly once -- structural, and true on any backend";
    RecordProperty("t4b_gate8_census",
                   "dropped_and_downgraded=" + std::to_string(dropped_and_downgraded) +
                       " kept_and_stood=" + std::to_string(kept_and_stood) +
                       " skipped_ties=" + std::to_string(skipped_ties));

    // The exact split, not just its existence: a drift in WHICH members are weak moves one
    // between the branches and leaves every `> 0` form true. Eight, not nine: the `Sigma = 1`
    // target is skipped as a tie before classifying. (T4b F1; `.superpowers/w1-t4b-report.md`.)
#ifdef USE_ACCELERATE_SPARSE
    RecordProperty("t4b_gate8_census_accelerate",
                   "UNOBSERVED -- which members are weak depends on the mu the backend converges "
                   "to; the split is MKL-only");
#else
    EXPECT_EQ(dropped_and_downgraded, 8);
    EXPECT_EQ(kept_and_stood, 1);
    EXPECT_EQ(skipped_ties, 1);
#endif

    // **CODEX'S NAMED MEMBER**, asserted on its own rather than only inside the
    // loop: the `Sigma = 1.28 > |-1|` member (T4b measured it at s = 1.4e-4) was
    // the concrete case filed as CRITICAL. It downgrades.
    {
        const QpProblem qp = weakly_active_indefinite_qp_at(1.28, -1.0, mu_stop);
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
        EXPECT_GT(sigma_at(r, qp, 1), 1.0) << "the masking really is in force at this member";
        EXPECT_EQ(r.counters.ipqp_final_inertia_read, 1);
        EXPECT_TRUE(r.certificate_downgraded);
        EXPECT_EQ(r.escape_reason, IpqpEscape::kIndefinite);
        EXPECT_NE(r.status, QpStatus::kOptimal);
    }

    // THE DEFECT, PINNED AS A MUTATION PARTNER. No option restores the masked `Sigma`, so the
    // partner is built from the arithmetic: at the returned point the OLD read's `H11 + sigma`
    // must be POSITIVE while the new one is NEGATIVE. Putting the masking back fails here.
    {
        const QpProblem qp = weakly_active_indefinite_qp_at(1.28, -1.0, mu_stop);
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
        const double h11 = -1.0;
        EXPECT_GT(h11 + sigma_at(r, qp, 1), 0.0)
            << "the UN-repaired read's matrix is positive definite here -- that is the defect";
        EXPECT_LT(h11, 0.0) << "and the repaired read's is not, because Sigma is dropped";
        EXPECT_TRUE(r.certificate_downgraded) << "and the engine takes the repaired answer";
    }

    // THE PARTNER THE RULING ASKS FOR: A TRUE WEAKLY-ACTIVE MINIMIZER STILL CERTIFIES. Same
    // geometry, same weak bounds, curvature flipped, so the dropped `Sigma` leaves `H11 = +1`
    // and the read agrees -- else "the saddle downgrades" is met by downgrading everything.
    Index minimizers_certified = 0;
    for (const double target : {0.1, 0.77, 1.28, 2.5, 10.0}) {
        SCOPED_TRACE(target);
        const QpProblem qp = weakly_active_indefinite_qp_at(target, 1.0, mu_stop);
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
        const double weak_scale = detail::kIpqpWeakActiveFactor * std::sqrt(r.mu);
        ASSERT_LE(r.x(1) - qp.lower(1), weak_scale) << "the bound really is weakly active here";
        ASSERT_LE(r.zl(1), weak_scale);
        EXPECT_EQ(r.status, QpStatus::kOptimal);
        EXPECT_EQ(r.counters.ipqp_final_inertia_read, 0);
        EXPECT_FALSE(r.certificate_downgraded);
        ++minimizers_certified;
    }
    EXPECT_EQ(minimizers_certified, 5);

    RecordProperty("t4b_gate8", "REPAIRED (fix round 1, R1): the item 4 read drops the Sigma of "
                                "weakly active bound sides, so a saddle held by barrier curvature "
                                "downgrades while a true weakly-active minimizer still certifies");
}

TEST(IpqpLadderTest, AFixedPointOfTheExecutedMapIsAPointTheGateCanSee) {
    // T4b CLOSE GATE 9 -- MECHANISM 4'S INVARIANT, in the form `IpqpResult` can carry: a solve
    // that reaches a fixed point either CONVERGES or is stopped by something other than the
    // iteration budget. The per-step form is source-mutation evidence in w1-t4b-report.md.
    struct Case {
        const char *name;
        QpProblem qp;
    };
    const std::vector<Case> cases = {
        {"hs_indefinite_equality", test_support::indefinite_equality_qp()},
        {"hs_indefinite_equality_and_row", test_support::indefinite_equality_and_row_qp()},
        {"hs_two_negative_eigenvalue_row", test_support::two_negative_eigenvalue_row_qp()},
        {"indefinite_box_family_c0", indefinite_box_qp(0.0)},
        {"indefinite_box_family_c1", indefinite_box_qp(1.0)},
        {"equality_null_space", equality_null_space_indefinite_qp(-2.0)},
        {"saddle", saddle_qp()},
    };
    for (const Case &c : cases) {
        SCOPED_TRACE(c.name);
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(c.qp, nullptr, IpqpOptions{}, SolveOverrides{});
        EXPECT_NE(r.escape_reason, IpqpEscape::kBudget)
            << "a budget stop on an indefinite fixture is the signature of the freeze T4b removed";
        EXPECT_LT(r.counters.ipqp_iters, IpqpOptions{}.ipqp_hard_iter_cap);
        // Every one of them reached a CERTIFYING exit -- the read was taken,
        // whatever it then said.
        EXPECT_TRUE(r.escape_reason == IpqpEscape::kNone ||
                    r.escape_reason == IpqpEscape::kIndefinite);
        EXPECT_LE(r.residuals.worst(), tight_opts().opt_tol * IpqpOptions{}.ipqp_converge_slack);
        // T4b GATE 5's COST TABLE, re-readable at any head (M6 W1 T9 item 10).
        RecordProperty(std::string(c.name) + "_cost",
                       fmt::format("it={} fac={} reclimbs={} retries={} armed_no_adv={} "
                                   "rho_dem_max={:.6g} rho_dem_last={:.6g}",
                                   r.counters.ipqp_iters, r.counters.ipqp_factorizations,
                                   r.counters.ipqp_ladder_reclimbs, r.counters.ipqp_inertia_retries,
                                   r.counters.ipqp_iters_ladder_armed_no_advance,
                                   r.counters.ipqp_rho_demanded_max,
                                   r.counters.ipqp_rho_demanded_last));
    }

    // THE SECOND HALF OF GATE 9: the item 4 read runs at `rho_dem = 0` and `rho_sched =
    // ipqp_reg_floor` BY CONSTRUCTION, never at the ladder's memory. The saddle fixture is the
    // witness: `rho_demanded_max = 100` reads right, and the read still comes back WRONG.
    IpqpEngine saddle_tier(tight_opts());
    const IpqpResult sr = saddle_tier.solve(saddle_qp(), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_GT(sr.counters.ipqp_rho_demanded_max, 1.0);
    ASSERT_GT(sr.counters.ipqp_rho_demanded_last, 1.0);
    EXPECT_EQ(sr.counters.ipqp_final_inertia_read, 1)
        << "a read seeded from the ladder's memory would find the inertia it was looking for and "
           "certify a saddle as a minimum";
    EXPECT_EQ(sr.escape_reason, IpqpEscape::kIndefinite);
}

TEST(IpqpLadderTest, ALongArmedWalkWithNoGateAdvanceStillConvergesInsideTheBudget) {
    // T4b CLOSE GATE 10 -- THE ADVERSARIAL LONG WALK. The section 3.2 gate is SILENT for the
    // whole armed walk by construction (the schedule's residual GROWS along negative reduced
    // curvature); section 6.2 does NOT fire, so plan 2.1(a)'s window reset is NOT built.
    IpqpEngine tier(tight_opts());
    const IpqpResult r =
        tier.solve(long_negative_curvature_walk_qp(), nullptr, IpqpOptions{}, SolveOverrides{});

    // THE WALK IS REAL AND IT IS LONG. Both halves matter: armed (so the gap's
    // precondition holds at all) and silent (so it is the gap and not an
    // ordinary solve).
    EXPECT_GE(r.counters.ipqp_iters_ladder_armed_no_advance, 10)
        << "fewer than ten armed silent iterations and this fixture licenses nothing";
    EXPECT_GT(r.counters.ipqp_rho_demanded_max, 0.0);
#ifdef USE_ACCELERATE_SPARSE
    // UNOBSERVED on Apple/Accelerate (CLAUDE.md section 6: a Mac value is never guessed). The
    // structural assertions above and below hold on every backend; the exact trajectory counts
    // are MKL evidence and are pinned only there.
    RecordProperty("t4b_gate10_accelerate", "UNOBSERVED -- exact walk counts are MKL-only");
#else
    EXPECT_EQ(r.counters.ipqp_iters_ladder_armed_no_advance, 26);
    EXPECT_EQ(r.counters.ipqp_iters_at_elevated_rho, 29);
#endif

    // ... AND IT FINISHES, INSIDE THE BUDGET, AT THE ANSWER.
#ifndef USE_ACCELERATE_SPARSE
    EXPECT_EQ(r.counters.ipqp_iters, 31);
#endif
    EXPECT_LT(r.counters.ipqp_iters, IpqpOptions{}.ipqp_hard_iter_cap);
    EXPECT_EQ(r.status, QpStatus::kOptimal);
    EXPECT_EQ(r.escape_reason, IpqpEscape::kNone);
    EXPECT_EQ(r.counters.ipqp_final_inertia_read, 0);
    EXPECT_NEAR(r.x(0), 1.0, 1e-6);
    EXPECT_NEAR(r.x(1), -100.0, 1e-6);

    // SECTION 6.2 DID NOT FIRE -- the pin the phi-reset decision rests on.
    EXPECT_EQ(r.counters.ipqp_escape_stall, 0);
    EXPECT_FALSE(r.stall_evidence.fired);
    EXPECT_EQ(r.counters.ipqp_escapes, 0);

    // THE WALK IS UNINTERRUPTED, PROVED NOT INFERRED (fix round 1, CX8): aggregate counts cannot
    // establish contiguity, so a TRUNCATION SWEEP over `ipqp_hard_iter_cap = k` says per
    // iteration whether it advanced and whether it was armed. `.superpowers/w1-t4b-report.md`.
    Index longest_run = 0;
    Index run = 0;
    Index prev_prox = 0;
    Index prev_elev = 0;
    for (Index k = 1; k <= r.counters.ipqp_iters; ++k) {
        IpqpOptions capped;
        capped.ipqp_hard_iter_cap = k;
        IpqpEngine step_tier(tight_opts());
        const IpqpResult sr =
            step_tier.solve(long_negative_curvature_walk_qp(), nullptr, capped, SolveOverrides{});
        ASSERT_EQ(sr.counters.ipqp_iters, k) << "the truncated solve must take exactly k steps";
        const bool advanced = sr.counters.ipqp_prox_center_updates > prev_prox;
        const bool armed = sr.counters.ipqp_iters_at_elevated_rho > prev_elev;
        prev_prox = sr.counters.ipqp_prox_center_updates;
        prev_elev = sr.counters.ipqp_iters_at_elevated_rho;
        run = (armed && !advanced) ? run + 1 : 0;
        longest_run = std::max(longest_run, run);
    }
    EXPECT_GE(longest_run, 10) << "the gap needs a CONTIGUOUS armed run with no gate advance, and "
                                  "ten is the floor the plan sets for this fixture";
    RecordProperty("t4b_gate10_longest_uninterrupted_armed_run", std::to_string(longest_run));
#ifndef USE_ACCELERATE_SPARSE
    // MEASURED on MKL: iterations 4 through 27 inclusive are armed with no gate advance -- one
    // unbroken run of 24. The three armed iterations that DO advance sit at the ends of the
    // walk (3, 28 and 31), which is the shape the gap predicts.
    EXPECT_EQ(longest_run, 24);
#endif

    // NON-VACUITY: the same problem with the sign of the second curvature
    // flipped is convex, never arms the ladder, and the counter that measures
    // the walk is structurally zero.
    QpProblem convex = long_negative_curvature_walk_qp();
    convex.H = dense_upper({{2.0, 0.0}, {0.0, 0.1}});
    IpqpEngine tier2(tight_opts());
    const IpqpResult c = tier2.solve(convex, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(c.status, QpStatus::kOptimal);
    EXPECT_EQ(c.counters.ipqp_iters_ladder_armed_no_advance, 0);
    EXPECT_EQ(c.counters.ipqp_iters_at_elevated_rho, 0);
    EXPECT_DOUBLE_EQ(c.counters.ipqp_rho_demanded_max, 0.0);
}

TEST(IpqpCertificationTest, AHealthySolveNeverArmsTheEvidenceFailurePath) {
    // Section 2.2's evidence-failure policy is a REPAIR TRIGGER, and one that fires on a healthy
    // solve is worse than no trigger at all. MKL reports observed inertia on every fixture here,
    // so the flag must be false everywhere; the seam file is what makes it true.
    for (const QpProblem &qp : {convex_qp(), saddle_qp(), indefinite_box_qp(0.5)}) {
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
        EXPECT_FALSE(r.inertia_evidence_failed);
    }
}

// ---------------------------------------------------------------------------
// Section 6.2 -- the early-stall test
// ---------------------------------------------------------------------------

namespace {

/// THE STALL FIXTURE'S MECHANISM: five CONSECUTIVE accepted steps under `kIpqpStallAlpha` is
/// hard to provoke, so the stall is provoked through a SHIPPED, legal option (`ipqp_tau = 1e-3`)
/// rather than through a contrived problem. `.superpowers/w1-t5-report.md` finding 4.
IpqpOptions crawling_opts(double tau) {
    IpqpOptions io;
    io.ipqp_tau = tau;
    return io;
}

/// A feasible QP whose Newton direction overshoots its box by ten decades on the first step:
/// `H` spans fourteen decades against a `1e10` gradient, so `alpha_p` is ~8e-10 once and ~1
/// thereafter. The healthy-solve partner for conjunct (iii).
QpProblem one_tiny_step_qp() {
    QpProblem qp;
    qp.H = dense_upper({{1e-14, 0.0}, {0.0, 1.0}});
    qp.g = vec({1e10, 0.0});
    qp.Ae = dense_rows({}, 2);
    qp.be = Vec(0);
    qp.Ai = dense_rows({}, 2);
    qp.bi = Vec(0);
    qp.lower = vec({-1.0, -1.0});
    qp.upper = vec({1.0, 1.0});
    return qp;
}

/// A feasible QP whose single equality row lands EXACTLY on a variable bound,
/// so the barrier can approach `x0 = 1` only asymptotically: one step at
/// `alpha_p ~ 3e-3`, then recovery. The second healthy-solve partner.
QpProblem equality_on_the_bound_qp() {
    QpProblem qp;
    qp.H = dense_upper({{1.0, 0.0}, {0.0, 1.0}});
    qp.g = vec({0.0, 0.0});
    qp.Ae = dense_rows({{1.0, 0.0}}, 2);
    qp.be = vec({1.0});
    qp.Ai = dense_rows({}, 2);
    qp.bi = Vec(0);
    qp.lower = vec({-1.0, -1.0});
    qp.upper = vec({1.0, 1.0});
    return qp;
}

/// An INFEASIBLE QP: `x0 + x1 <= -2` and `x0 + x1 >= 2` on a box containing neither. The primal
/// residual is flat on a positive floor and the multipliers price the contradiction without
/// limit -- section 6.3's two signals, from a problem that genuinely has them.
QpProblem infeasible_rows_qp() {
    QpProblem qp = convex_qp();
    qp.Ai = dense_rows({{1.0, 1.0}, {-1.0, -1.0}}, 2);
    qp.bi = vec({-2.0, -2.0});
    return qp;
}

/// A one-variable infeasible QP: `x <= -5` and `x >= 5`.
QpProblem infeasible_scalar_qp() {
    QpProblem qp;
    qp.H = dense_upper({{1.0}});
    qp.g = vec({0.0});
    qp.Ae = dense_rows({}, 1);
    qp.be = Vec(0);
    qp.Ai = dense_rows({{1.0}, {-1.0}}, 1);
    qp.bi = vec({-5.0, -5.0});
    qp.lower = vec({-10.0});
    qp.upper = vec({10.0});
    return qp;
}

/// `infeasible_scalar_qp` with BOTH rows MULTIPLIED by S -- the same (empty) feasible set, in row
/// units six decades off the H block at S = 1e6. That spread is exactly what Ruiz equilibration
/// removes from the KKT, and it moves this solve's multipliers by five decades.
QpProblem badly_scaled_infeasible_qp(double S) {
    QpProblem qp = infeasible_scalar_qp();
    qp.Ai = dense_rows({{S}, {-S}}, 1);
    qp.bi = vec({-5.0 * S, -5.0 * S});
    return qp;
}

/// The inf-norm of the multipliers the tier EXPORTS -- spec 4.3's "duals are unscaled on export",
/// and therefore the caller-scale reading the evidence block's own norms must match.
double exported_dual_norm(const IpqpResult &r) {
    double d = std::max(r.zl.lpNorm<Eigen::Infinity>(), r.zu.lpNorm<Eigen::Infinity>());
    if (r.lambda_e.size() > 0) {
        d = std::max(d, r.lambda_e.lpNorm<Eigen::Infinity>());
    }
    if (r.lambda_i.size() > 0) {
        d = std::max(d, r.lambda_i.lpNorm<Eigen::Infinity>());
    }
    return d;
}

} // namespace

TEST(IpqpStallTest, TheStallEscapeCarriesAllThreeConjunctValuesInOneEvidenceBlock) {
    // PLAN SECTION 7 NOTE (b), FINAL: the three `ipqp_stall_reason_*` counters are DROPPED as
    // ill-posed (all three conjuncts hold at every stall escape), and the three VALUES travel
    // in the escape's own evidence block instead. This is that note's ONE pin.
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(convex_qp(), nullptr, crawling_opts(1e-3), SolveOverrides{});

    ASSERT_EQ(r.escape_reason, IpqpEscape::kStall);
    EXPECT_EQ(r.status, QpStatus::kNumericalError);
    EXPECT_EQ(r.counters.ipqp_escapes, 1);
    EXPECT_EQ(r.counters.ipqp_escape_stall, 1);
    EXPECT_EQ(census_entries(r.counters), 1);
    EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(r.counters));

    // THE EVIDENCE BLOCK CARRIES ALL THREE, and each value satisfies its own
    // conjunct -- which is what makes the block a record of the test rather
    // than three numbers that happen to be present.
    const IpqpStallEvidence &ev = r.stall_evidence;
    ASSERT_TRUE(ev.fired);
    EXPECT_EQ(ev.window, IpqpOptions{}.ipqp_stall_window);
    EXPECT_LT(ev.mu_ratio, detail::kIpqpStallMuFactor); // conjunct (i)
    EXPECT_GT(ev.mu_ratio, 0.0);
    EXPECT_LT(ev.residual_improvement, 1.0 - detail::kSsnStallImproveFactor); // (ii)
    EXPECT_LT(ev.max_step_alpha, detail::kIpqpStallAlpha);                    // (iii)
    EXPECT_LE(ev.min_alpha, ev.max_step_alpha);
    EXPECT_GT(ev.min_alpha, 0.0);

    // SECTION 6.1'S "BUDGET OF LAST RESORT", BOUNDED TIGHTLY. `< 60` is far too weak a form of
    // the claim, so the pin is the structural ceiling `2 * ipqp_stall_window`: the window can
    // close at most twice before firing (evaluated and re-armed, then fired).
    EXPECT_LE(r.counters.ipqp_iters, 2 * IpqpOptions{}.ipqp_stall_window);
    EXPECT_GE(r.counters.ipqp_iters, IpqpOptions{}.ipqp_stall_window);
    // ... and the exact measured value, so a trajectory move is VISIBLE rather
    // than silently absorbed by the bound above. 10 == two closed windows.
    EXPECT_EQ(r.counters.ipqp_iters, 10);
    EXPECT_EQ(r.counters.ipqp_escape_budget, 0);

    // MUTATION PARTNER: `tau = 1e-2` crawls too, but its steps sit at ~6.5e-3 to ~3.3e-1, so
    // conjunct (iii)'s WHOLE-WINDOW demand is not met and the solve pays the full budget. The
    // stall escape is therefore a statement about the trajectory, not about the option.
    IpqpEngine slow(tight_opts());
    const IpqpResult s = slow.solve(convex_qp(), nullptr, crawling_opts(1e-2), SolveOverrides{});
    EXPECT_EQ(s.escape_reason, IpqpEscape::kBudget);
    EXPECT_FALSE(s.stall_evidence.fired);
    EXPECT_EQ(s.counters.ipqp_escape_stall, 0);
}

TEST(IpqpStallTest, AHealthySolveNeverTripsConjunctThreeOnOneTinyAlphaStep) {
    // "NEVER ABORT ON ONE TINY-ALPHA ITERATION" (spec 6.2): conjunct (iii) is a whole-window
    // property BY CONSTRUCTION, and these two fixtures make that testable -- both take a
    // genuinely tiny step (pinned, so a fixture cannot silently stop being one) and both certify.
    for (const QpProblem &qp : {one_tiny_step_qp(), equality_on_the_bound_qp()}) {
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
        // The premise: a step BELOW the conjunct's own threshold was taken.
        ASSERT_LT(r.counters.ipqp_alpha_p_min, detail::kIpqpStallAlpha);
        // The conclusion: it certified anyway.
        EXPECT_EQ(r.status, QpStatus::kOptimal);
        EXPECT_EQ(r.escape_reason, IpqpEscape::kNone);
        EXPECT_FALSE(r.stall_evidence.fired);
        EXPECT_EQ(r.counters.ipqp_escape_stall, 0);
    }
}

// ---------------------------------------------------------------------------
// Section 6.3 -- infeasible-suspect
// ---------------------------------------------------------------------------

TEST(IpqpInfeasibleSuspectTest, TheSignatureEscapesAsASUSPICIONAndNeverAsACertificate) {
    // SPEC 6.3'S HARD RULE: it never returns `QpStatus::kInfeasible` -- a CERTIFICATE word in
    // this driver (solver_status.h) -- because IP-PMM has no homogeneous self-dual embedding
    // and therefore no infeasibility certificate at all.
    for (const QpProblem &qp : {infeasible_rows_qp(), infeasible_scalar_qp()}) {
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});

        ASSERT_EQ(r.escape_reason, IpqpEscape::kInfeasibleSuspect);
        EXPECT_NE(r.status, QpStatus::kInfeasible);
        EXPECT_EQ(r.status, QpStatus::kNumericalError);
        EXPECT_EQ(r.counters.ipqp_escapes, 1);
        EXPECT_EQ(r.counters.ipqp_escape_infeasible_suspect, 1);
        EXPECT_EQ(census_entries(r.counters), 1);
        EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(r.counters));

        // THE EVIDENCE BLOCK: each signal with its value, the window, and the
        // least-infeasible point -- section 6.3's own three requirements.
        const IpqpInfeasibilityEvidence &ev = r.infeasibility_evidence;
        ASSERT_TRUE(ev.fired);
        EXPECT_FALSE(ev.exhaustion_route);
        EXPECT_EQ(ev.window, IpqpOptions{}.ipqp_stall_window);
        // (a) flat on a POSITIVE floor.
        EXPECT_GT(ev.primal_end, 0.0);
        EXPECT_LT(ev.primal_improvement, 1.0 - detail::kSsnStallImproveFactor);
        // (b) the multipliers grew without limit over the same window.
        EXPECT_GE(ev.dual_growth, detail::kSsnDualGrowthFactor);
        EXPECT_DOUBLE_EQ(ev.dual_step_growth, 0.0); // standing route: unread
        // the least-infeasible point.
        EXPECT_EQ(ev.least_infeasible_x.size(), qp.n());
        EXPECT_LE(ev.least_infeasible_primal, ev.primal_end);

        // The tier stopped WELL short of the budget.
        EXPECT_LT(r.counters.ipqp_iters, IpqpOptions{}.ipqp_hard_iter_cap);
    }
}

TEST(IpqpInfeasibleSuspectTest, TheFarkasGateArmsTheReportAndNeverWithdrawsIt) {
    // "Optional Farkas corroboration may ARM the report; IT NEVER CERTIFIES"
    // (spec 6.3). Both arms of the option therefore produce the SAME escape;
    // the only difference is whether the block carries a corroboration.
    IpqpEngine on(tight_opts());
    const IpqpResult armed =
        on.solve(infeasible_rows_qp(), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(armed.escape_reason, IpqpEscape::kInfeasibleSuspect);
    EXPECT_TRUE(armed.infeasibility_evidence.farkas_corroborated);
    EXPECT_LE(armed.infeasibility_evidence.farkas_residual, detail::kSsnFarkasResidualTol);
    EXPECT_LE(armed.infeasibility_evidence.farkas_gap, -detail::kSsnFarkasGapTol);

    IpqpOptions off;
    off.ipqp_farkas_gate = false;
    IpqpEngine tier(tight_opts());
    const IpqpResult bare = tier.solve(infeasible_rows_qp(), nullptr, off, SolveOverrides{});
    EXPECT_EQ(bare.escape_reason, IpqpEscape::kInfeasibleSuspect);
    EXPECT_EQ(bare.counters.ipqp_escape_infeasible_suspect, 1);
    EXPECT_FALSE(bare.infeasibility_evidence.farkas_corroborated);
    EXPECT_DOUBLE_EQ(bare.infeasibility_evidence.farkas_residual, 0.0);
    EXPECT_DOUBLE_EQ(bare.infeasibility_evidence.farkas_gap, 0.0);
    // The escape itself is UNCHANGED by the gate -- which is the pin.
    EXPECT_EQ(bare.counters.ipqp_iters, armed.counters.ipqp_iters);
}

TEST(IpqpInfeasibleSuspectTest, TheExhaustionRouteMeasuresGrowthPerStepAgainstTheStartPoint) {
    // SPEC 6.3'S EXHAUSTION VARIANT, reached by capping the budget so the solve stops BETWEEN
    // window closures. The cap (12) is TRAJECTORY-DEPENDENT and pinned as such, so a trajectory
    // move fails loudly instead of retesting the standing route. `.superpowers/w1-t5-report.md`.
    IpqpOptions io;
    io.ipqp_hard_iter_cap = 11; // T10b re-derivation: 12 at the 0.1 placeholder.
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(infeasible_rows_qp(), nullptr, io, SolveOverrides{});

    ASSERT_EQ(r.escape_reason, IpqpEscape::kInfeasibleSuspect);
    const IpqpInfeasibilityEvidence &ev = r.infeasibility_evidence;
    ASSERT_TRUE(ev.fired);
    ASSERT_TRUE(ev.exhaustion_route);
    EXPECT_LT(ev.window, IpqpOptions{}.ipqp_stall_window); // a PARTIAL window
    EXPECT_GE(ev.window, 1);
    EXPECT_GE(ev.dual_growth, detail::kSsnDualGrowthFactor);
    EXPECT_GE(ev.dual_step_growth, detail::kSsnDualStepGrowth);
    EXPECT_EQ(r.counters.ipqp_escape_infeasible_suspect, 1);
    EXPECT_EQ(r.counters.ipqp_escape_budget, 0);

    // MUTATION PARTNER: one more iteration and the per-step growth conjunct is no longer met,
    // so the same stop is a PLAIN BUDGET escape. Without this the pin above could pass on an
    // engine that relabelled every budget stop as a suspicion.
    IpqpOptions io2;
    io2.ipqp_hard_iter_cap = 12;
    IpqpEngine tier2(tight_opts());
    const IpqpResult r2 = tier2.solve(infeasible_rows_qp(), nullptr, io2, SolveOverrides{});
    EXPECT_EQ(r2.escape_reason, IpqpEscape::kBudget);
    EXPECT_FALSE(r2.infeasibility_evidence.fired);
    EXPECT_EQ(r2.counters.ipqp_escape_budget, 1);
}

TEST(IpqpInfeasibleSuspectTest, AFeasibleSolveNeverRaisesTheSignature) {
    // The non-vacuity partner for the whole section: on a feasible, well-posed subproblem
    // neither signal is ever raised, whatever the outcome. A suspicion generator that fired on
    // healthy problems would route every subproblem to the W2 feasibility hook.
    for (const QpProblem &qp : {convex_qp(), one_tiny_step_qp(), saddle_qp()}) {
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
        EXPECT_FALSE(r.infeasibility_evidence.fired);
        EXPECT_EQ(r.counters.ipqp_escape_infeasible_suspect, 0);
        EXPECT_EQ(r.infeasibility_evidence.least_infeasible_x.size(), 0);
    }
}

TEST(IpqpInfeasibleSuspectTest, TheEvidenceCarriesTheDualsOfTheLeastInfeasiblePointITSELF) {
    // W2's working-set seed classifies the LEAST-INFEASIBLE point with spec 2.3 item 2's ratio
    // rule, so the block must carry that point's own (s, lambda_i, zl, zu) and the `mu` they are
    // stated against -- retained WITH `least_infeasible_x`, never taken from the final iterate.
    for (const QpProblem &qp : {infeasible_rows_qp(), infeasible_scalar_qp()}) {
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
        ASSERT_EQ(r.escape_reason, IpqpEscape::kInfeasibleSuspect);
        const IpqpInfeasibilityEvidence &ev = r.infeasibility_evidence;
        ASSERT_TRUE(ev.fired);

        EXPECT_EQ(ev.least_infeasible_s.size(), qp.mi());
        EXPECT_EQ(ev.least_infeasible_lambda_i.size(), qp.mi());
        EXPECT_EQ(ev.least_infeasible_zl.size(), qp.n());
        EXPECT_EQ(ev.least_infeasible_zu.size(), qp.n());
        EXPECT_GT(ev.least_infeasible_mu, 0.0);
        // A BARRIER ITERATE, not a stored zero: both sides of every pair are strictly interior,
        // which is the property the ratio rule's `s < kappa*z` comparison needs to mean anything.
        EXPECT_GT(ev.least_infeasible_s.minCoeff(), 0.0);
        EXPECT_GT(ev.least_infeasible_lambda_i.minCoeff(), 0.0);

        // ONE ITERATE, NOT A MIXTURE: the duals move with the point they were retained beside.
        // Storing the FINAL iterate's duals against an EARLIER `least_infeasible_x` breaks this.
        const bool x_is_final = (ev.least_infeasible_x.array() == r.x.array()).all();
        const bool duals_are_final =
            (ev.least_infeasible_lambda_i.array() == r.lambda_i.array()).all() &&
            (ev.least_infeasible_zl.array() == r.zl.array()).all();
        EXPECT_EQ(x_is_final, duals_are_final);
        RecordProperty("w2t2_least_infeasible_is_final", x_is_final ? "1" : "0");
    }
}

TEST(IpqpInfeasibleSuspectTest, TheEvidencesDualNormsAreRecordedAFTERTheRuizUnscale) {
    // W2.T2's ASSERTED PRECONDITION. `rho_0 = max(kElasticRhoInit, dual_norm_start)` prices the
    // ACTUAL violation, so a norm taken inside the equilibrated system would be a number about a
    // different problem. The engine asserts it at the write site; this is the fixture behind it.
    const double S = 1.0e6;
    IpqpEngine tier(tight_opts());
    const IpqpResult bad =
        tier.solve(badly_scaled_infeasible_qp(S), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(bad.escape_reason, IpqpEscape::kInfeasibleSuspect);
    ASSERT_TRUE(bad.infeasibility_evidence.fired);

    IpqpEngine unit_tier(tight_opts());
    const IpqpResult unit =
        unit_tier.solve(infeasible_scalar_qp(), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(unit.escape_reason, IpqpEscape::kInfeasibleSuspect);

    // THE GAP THAT MAKES THE PIN NON-VACUOUS: the same solve stated in the caller's units and in
    // the unit-row units Ruiz drives every row toward differ by more than three decades here.
    const double bad_norm = bad.infeasibility_evidence.dual_norm_end;
    const double unit_norm = unit.infeasibility_evidence.dual_norm_end;
    RecordProperty("w2t2_dual_norm_caller_scale", std::to_string(bad_norm));
    RecordProperty("w2t2_dual_norm_unit_rows", std::to_string(unit_norm));
    EXPECT_GE(std::max(bad_norm, unit_norm) / std::min(bad_norm, unit_norm), 1.0e3);

    // AND THE RECORD IS THE CALLER-SCALE ONE, to the bit: the multipliers the tier exports are
    // unscaled (spec 4.3), and the evidence's own norms are norms of exactly those.
    EXPECT_DOUBLE_EQ(bad_norm, exported_dual_norm(bad));
    EXPECT_DOUBLE_EQ(unit_norm, exported_dual_norm(unit));

    // NEITHER NORM MOVES WITH THE EQUILIBRATION, which is the same statement from the other side:
    // Ruiz spans decades on this fixture and the record does not notice.
    IpqpOptions off;
    off.ipqp_ruiz = false;
    IpqpEngine off_tier(tight_opts());
    const IpqpResult bad_off =
        off_tier.solve(badly_scaled_infeasible_qp(S), nullptr, off, SolveOverrides{});
    ASSERT_TRUE(bad_off.infeasibility_evidence.fired);
    EXPECT_LT(std::abs(bad_off.infeasibility_evidence.dual_norm_end - bad_norm) / bad_norm, 1.0e-9);
}

// ---------------------------------------------------------------------------
// Section 6.1 -- the escape ladder
// ---------------------------------------------------------------------------

namespace {

/// A tier outcome shaped by hand. The ladder reads exactly two fields, and building results
/// directly rather than by solving keeps the ladder's own rules separable from the engine's
/// classification -- which is the point of `IpqpEscapeLadder` being a type and not a member.
IpqpResult escaped(IpqpEscape why) {
    IpqpResult r;
    r.escape_reason = why;
    r.status = QpStatus::kNumericalError;
    return r;
}

IpqpResult succeeded() { return IpqpResult{}; }

IpqpResult downgraded_without_escape() {
    IpqpResult r;
    r.status = QpStatus::kNumericalError;
    r.certificate_downgraded = true;
    r.escape_reason = IpqpEscape::kNone;
    return r;
}

IpqpResult declined() {
    IpqpResult r;
    r.declined_pinned = true;
    r.status = QpStatus::kNumericalError;
    r.escape_reason = IpqpEscape::kNone;
    r.counters.ipqp_declined_pinned = 1;
    return r;
}

} // namespace

TEST(IpqpEscapeLadderTest, ThreeConsecutiveEscapesRetireTheTierAtThatMajor) {
    IpqpEscapeLadder ladder{IpqpOptions{}};
    EXPECT_FALSE(ladder.retired());
    EXPECT_EQ(ladder.retired_after(), 0);

    EXPECT_FALSE(ladder.record(escaped(IpqpEscape::kBudget), 1));
    EXPECT_EQ(ladder.consecutive_escapes(), 1);
    EXPECT_FALSE(ladder.record(escaped(IpqpEscape::kStall), 2));
    EXPECT_EQ(ladder.consecutive_escapes(), 2);
    // K = 3 (IpqpOptions::ipqp_retire_after's default).
    EXPECT_TRUE(ladder.record(escaped(IpqpEscape::kIndefinite), 3));
    EXPECT_TRUE(ladder.retired());
    EXPECT_EQ(ladder.retired_after(), 3);

    // FIRES AT MOST ONCE: a fourth escape does not move the marker to major 4.
    EXPECT_TRUE(ladder.record(escaped(IpqpEscape::kNumerical), 4));
    EXPECT_EQ(ladder.retired_after(), 3);
    // ... and neither does a later SUCCESS un-retire it. "Any success resets
    // the count" resets the tally toward a FUTURE retirement, not a fired one.
    EXPECT_TRUE(ladder.record(succeeded(), 5));
    EXPECT_TRUE(ladder.retired());
    EXPECT_EQ(ladder.retired_after(), 3);
}

TEST(IpqpEscapeLadderTest, AnySuccessResetsTheCountIncludingADowngradedOne) {
    IpqpEscapeLadder ladder{IpqpOptions{}};
    EXPECT_FALSE(ladder.record(escaped(IpqpEscape::kBudget), 1));
    EXPECT_FALSE(ladder.record(escaped(IpqpEscape::kBudget), 2));
    EXPECT_EQ(ladder.consecutive_escapes(), 2);

    // ONE success clears the tally, so the two escapes above are no longer
    // "consecutive" with anything that follows.
    EXPECT_FALSE(ladder.record(succeeded(), 3));
    EXPECT_EQ(ladder.consecutive_escapes(), 0);
    EXPECT_FALSE(ladder.record(escaped(IpqpEscape::kBudget), 4));
    EXPECT_FALSE(ladder.record(escaped(IpqpEscape::kBudget), 5));
    EXPECT_FALSE(ladder.retired());

    // A DOWNGRADED-WITHOUT-ESCAPE SOLVE IS A SUCCESS HERE (task 5's ruling, plan section 7 note
    // (j): no census entry, no section 6.1 K = 3 charge). Without this rule, three clean solves
    // under `ipqp_require_final_inertia = false` would sit on top of an old tally.
    EXPECT_FALSE(ladder.record(downgraded_without_escape(), 6));
    EXPECT_EQ(ladder.consecutive_escapes(), 0);
    EXPECT_FALSE(ladder.retired());
}

TEST(IpqpEscapeLadderTest, ADeclineIsNeutralAndNeitherAdvancesNorResetsTheTally) {
    // `ipqp_declined_pinned`'s settled text: the tier never ran, so a decline never counts
    // toward `ipqp_escapes` or the K=3 retirement threshold. Task 5's ruling is the other half,
    // which this fixture separates from the reset rule: a decline is not a SUCCESS either.
    IpqpEscapeLadder ladder{IpqpOptions{}};
    EXPECT_FALSE(ladder.record(escaped(IpqpEscape::kBudget), 1));
    EXPECT_FALSE(ladder.record(escaped(IpqpEscape::kBudget), 2));
    EXPECT_EQ(ladder.consecutive_escapes(), 2);

    // NEUTRAL: the tally survives a decline...
    EXPECT_FALSE(ladder.record(declined(), 3));
    EXPECT_EQ(ladder.consecutive_escapes(), 2);
    EXPECT_FALSE(ladder.retired());
    // ... and the NEXT escape is still the third.
    EXPECT_TRUE(ladder.record(escaped(IpqpEscape::kBudget), 4));
    EXPECT_EQ(ladder.retired_after(), 4);

    // MUTATION PARTNER: had the decline been treated as a success, the run
    // above would have needed three more escapes. This asserts the other
    // reading is genuinely different rather than merely differently spelled.
    IpqpEscapeLadder reset_ladder{IpqpOptions{}};
    EXPECT_FALSE(reset_ladder.record(escaped(IpqpEscape::kBudget), 1));
    EXPECT_FALSE(reset_ladder.record(escaped(IpqpEscape::kBudget), 2));
    EXPECT_FALSE(reset_ladder.record(succeeded(), 3));
    EXPECT_FALSE(reset_ladder.record(escaped(IpqpEscape::kBudget), 4));
    EXPECT_FALSE(reset_ladder.retired());
}

TEST(IpqpEscapeLadderTest, TheThresholdIsTheOptionAndIsValidatedAtTheBoundary) {
    IpqpOptions io;
    io.ipqp_retire_after = 1;
    IpqpEscapeLadder eager{io};
    EXPECT_TRUE(eager.record(escaped(IpqpEscape::kStall), 7));
    EXPECT_EQ(eager.retired_after(), 7);

    io.ipqp_retire_after = 5;
    IpqpEscapeLadder patient{io};
    for (Index k = 1; k <= 4; ++k) {
        EXPECT_FALSE(patient.record(escaped(IpqpEscape::kStall), k));
    }
    EXPECT_TRUE(patient.record(escaped(IpqpEscape::kStall), 5));
    EXPECT_EQ(patient.retired_after(), 5);

    // BOUNDARY VALIDATION (CLAUDE.md section 4): this type is reachable
    // without a driver, so it re-checks the band `validate_sqp_options` owns
    // rather than trusting a caller that may never have run one.
    IpqpOptions bad;
    bad.ipqp_retire_after = 0;
    EXPECT_THROW(IpqpEscapeLadder{bad}, std::invalid_argument);
    IpqpEscapeLadder ok{IpqpOptions{}};
    EXPECT_THROW(ok.record(escaped(IpqpEscape::kBudget), 0), std::invalid_argument);
}

TEST(IpqpEscapeLadderTest, TheLadderIsDrivenByRealTierOutcomesAndNotOnlyByHandBuiltOnes) {
    // The three fixtures above build `IpqpResult`s directly; a ladder that only ever saw
    // hand-built results could be reading fields the engine never sets. This drives it from REAL
    // solves: two escapes and one certifying solve, which must NOT retire, then three, which must.
    IpqpOptions budget_io;
    budget_io.ipqp_hard_iter_cap = 1;

    IpqpEscapeLadder ladder{IpqpOptions{}};
    Index major = 0;
    for (int k = 0; k < 2; ++k) {
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(convex_qp(), nullptr, budget_io, SolveOverrides{});
        ASSERT_EQ(r.escape_reason, IpqpEscape::kBudget);
        EXPECT_FALSE(ladder.record(r, ++major));
    }
    {
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(convex_qp(), nullptr, IpqpOptions{}, SolveOverrides{});
        ASSERT_EQ(r.escape_reason, IpqpEscape::kNone);
        EXPECT_FALSE(ladder.record(r, ++major));
        EXPECT_EQ(ladder.consecutive_escapes(), 0);
    }
    for (int k = 0; k < 3; ++k) {
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(convex_qp(), nullptr, budget_io, SolveOverrides{});
        ladder.record(r, ++major);
    }
    EXPECT_TRUE(ladder.retired());
    EXPECT_EQ(ladder.retired_after(), 6);
}

// T4c non-vacuity: `convex_qp()` (no active bound) is structurally FALSE; fix round 1 makes the
// HS rows a MEANINGFUL second FALSE case (round 0's literal trigger fired on them too).
// See `.superpowers/w1-t4c-report.md`.
TEST(IpqpCertificationTest, T4cKeptTightNonVacuity) {
    {
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(convex_qp(), nullptr, IpqpOptions{}, SolveOverrides{});
        ASSERT_EQ(r.counters.ipqp_final_inertia_read, 0);
        EXPECT_FALSE(r.read_kept_tight);
        EXPECT_EQ(r.counters.ipqp_read_kept_tight_sides, 0);
    }

    // HS multipliers are O(1): z / sqrt(mu) sits 3-4 decades above the band
    // ceiling, so the band excludes them as priced (T4c report, fix round 1).
    [[maybe_unused]] const double max_ratio[3] = {5.0e5, 1.5e6, 2.3e6}; // MKL-only margin bound
    Index i = 0;
    for (const QpProblem &qp :
         {test_support::indefinite_equality_qp(), test_support::indefinite_equality_and_row_qp(),
          test_support::two_negative_eigenvalue_row_qp()}) {
        SCOPED_TRACE(i);
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
        ASSERT_EQ(r.counters.ipqp_final_inertia_read, 0);
        // The margin claim's own inputs, computed unconditionally (data,
        // not a pin): the ACTIVE bound's own `z / sqrt(mu)` (the row's
        // other, near-zero sides are inactive and excluded).
        double max_seen = 0.0;
        for (Index j = 0; j < r.x.size(); ++j) {
            max_seen = std::max({max_seen, r.zl(j) / std::sqrt(r.mu), r.zu(j) / std::sqrt(r.mu)});
        }
        // M1 / R3: every exact value below (the counts, the flag, and the
        // margin bound itself) is a trajectory value, so both this and the
        // `max_seen` bound are MKL-scoped like every other one in this file.
#ifdef USE_ACCELERATE_SPARSE
        RecordProperty("t4c_hs_accelerate_" + std::to_string(i),
                       "UNOBSERVED (max_seen measured " + std::to_string(max_seen) +
                           ") -- the exact band/noise counts and margin are MKL-only");
#else
        EXPECT_FALSE(r.read_kept_tight);
        EXPECT_EQ(r.counters.ipqp_read_kept_tight_sides, 0);
        EXPECT_EQ(r.counters.ipqp_read_barrier_noise_sides, 0);
        EXPECT_GT(max_seen, max_ratio[i]);
        RecordProperty("t4c_hs_kept_tight_" + std::to_string(i),
                       std::to_string(r.counters.ipqp_read_kept_tight_sides));
#endif
        ++i;
    }
}

// T4c pin (c): the f-gap bound -- confined by the box while `s <= sqrt(mu)`,
// `0.5 |h| min(s, sqrt(mu))^2 <= mu`, the O(mu_stop) bound the owner ruling
// asks for. See `.superpowers/w1-t4c-report.md`.
TEST(IpqpCertificationTest, T4cFGapBoundedByMuStop) {
    // The three STANDING members, constructed at their curvatures (T10b) rather than at the
    // half-widths that produced them under the 0.1 placeholder: `Sigma = 2 mu_stop / s^2`.
    const double mu_stop = gate8_mu_stop();
    for (const double target : {250.0, 1000.0, 25000.0}) {
        SCOPED_TRACE(target);
        const double h = -1.0;
        const double s = std::sqrt(2.0 * mu_stop / target);
        const QpProblem qp = weakly_active_indefinite_qp(s, h);
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
        ASSERT_EQ(r.counters.ipqp_final_inertia_read, 0)
            << "the f-gap bound is a statement about a STANDING certificate";
        // R3 (fix round 2): the exposed-regime flag and T1's e pin are both
        // exact trajectory values on this family, so both are MKL-scoped.
#ifdef USE_ACCELERATE_SPARSE
        RecordProperty("t4c_fgap_accelerate_sigma" + fmt::format("{:.0e}", target),
                       "UNOBSERVED -- the exposed-regime trigger and e are MKL-only");
#else
        ASSERT_TRUE(r.read_kept_tight) << "and specifically about the exposed regime";
        EXPECT_GE(r.read_barrier_noise_exponent_min, 0.9);
        EXPECT_LE(r.read_barrier_noise_exponent_max, 1.1);
#endif

        const double delta = std::min(s, std::sqrt(r.mu));
        const double fgap = 0.5 * std::abs(h) * delta * delta;
        EXPECT_LE(fgap, r.mu);
        RecordProperty("t4c_fgap_sigma" + fmt::format("{:.0e}", target),
                       fmt::format("{:.6e}", fgap));
        RecordProperty("t4c_mu_sigma" + fmt::format("{:.0e}", target), fmt::format("{:.6e}", r.mu));
    }
}

// R1 pin (a) / T2's solve-level pin: warm-starting from the gate-8 member's OWN CONVERGED POINT
// leaves < 2 accepted iterates, so the flag must come from the BAND path -- noise structurally
// absent. See `.superpowers/w1-t4c-report.md`.
TEST(IpqpCertificationTest, FewerThanTwoAcceptedIteratesFallsBackToTheBandPath) {
    const QpProblem qp = weakly_active_indefinite_qp_at(250.0, -1.0, gate8_mu_stop());
    IpqpEngine cold_tier(tight_opts());
    const IpqpResult cold = cold_tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(cold.status, QpStatus::kOptimal);
    ASSERT_NE(cold_tier.warm_carry(), nullptr);

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, cold_tier.warm_carry(), IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal);
    ASSERT_EQ(r.counters.ipqp_final_inertia_read, 0);

    // R3 (fix round 3): `ipqp_iters <= 1` is itself an exact trajectory
    // value -- a different backend's warm-repair path is not ruled out to
    // land there too -- so it is MKL-scoped along with what it gates.
#ifdef USE_ACCELERATE_SPARSE
    RecordProperty("t4c_band_fallback_accelerate",
                   "UNOBSERVED -- whether this member still converges in <= 1 step and band-counts "
                   "after a different backend's warm trajectory is MKL-only");
#else
    ASSERT_LE(r.counters.ipqp_iters, 1) << "the whole point of seeding from convergence";
    EXPECT_GT(r.counters.ipqp_read_kept_tight_sides, 0);
    EXPECT_EQ(r.counters.ipqp_read_barrier_noise_sides, 0)
        << "fewer than two accepted iterates -- the exponent count must stay structurally absent";
    EXPECT_TRUE(r.read_kept_tight) << "band fallback still fires the disclosure";
#endif
}

// R1 pin (b) / Codex 1: a killed warm attempt's history must not reach the cold restart. Pinned
// as an EQUIVALENCE, not by iteration count -- a literal <=1-step cold segment is unreachable on
// this family. See `.superpowers/w1-t4c-report.md`.
TEST(IpqpCertificationTest, AKilledWarmAttemptsHistoryNeverReachesTheColdRestartsRead) {
    const QpProblem qp = weakly_active_indefinite_qp_at(250.0, -1.0, gate8_mu_stop());

    IpqpEngine baseline_tier(tight_opts());
    const IpqpResult baseline = baseline_tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(baseline.status, QpStatus::kOptimal);

    // A hand-built STALE seed: `x0 = 3` is off this family's shared optimum (`x* = 0`) along the
    // UNBOUNDED direction, forcing real corrector work at a `mu` two decades above this member's
    // own before the kill. See `.superpowers/w1-t4c-report.md`.
    IpqpSeed stale;
    stale.x = vec({3.0, 0.0});
    stale.s = Vec(0);
    stale.lambda_e = Vec(0);
    stale.lambda_i = Vec(0);
    stale.zl = vec({1.0e-2, 1.0e-2});
    stale.zu = vec({1.0e-2, 1.0e-2});
    stale.zeta = stale.x;
    stale.lambda_est_e = Vec(0);
    stale.lambda_est_i = Vec(0);
    stale.mu = 1.0e-2;
    stale.grade = IpqpRestartGrade::kFullWarm;

    IpqpOptions killed;
    killed.ipqp_warm_iter_budget = 1; // forces the kill after at most one warm step
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, &stale, killed, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal);
    ASSERT_EQ(r.counters.ipqp_warm_restart_abandoned, 1) << "the kill must actually have fired";
    ASSERT_EQ(r.counters.ipqp_final_inertia_read, 0);

#ifdef USE_ACCELERATE_SPARSE
    RecordProperty("t4c_kill_reset_accelerate",
                   "UNOBSERVED -- the post-kill trajectory this equivalence compares is MKL-only");
#else
    EXPECT_EQ(r.counters.ipqp_read_kept_tight_sides, baseline.counters.ipqp_read_kept_tight_sides);
    EXPECT_EQ(r.counters.ipqp_read_barrier_noise_sides,
              baseline.counters.ipqp_read_barrier_noise_sides);
    EXPECT_EQ(r.read_kept_tight, baseline.read_kept_tight);
#endif
}

} // namespace hven::solvers
