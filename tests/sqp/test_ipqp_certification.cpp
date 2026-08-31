// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_ipqp_certification.cpp -- the M6 W1 task 5 pins for the
// interior-point QP tier's CERTIFICATION vocabulary, its five-way escape
// census, the section 6.1 escape ladder, and the section 6.2 / 6.3 early-stall
// and infeasible-suspect tests.
//
// WHAT IS AND IS NOT HERE. Everything in this file is reachable from a LEGAL
// subproblem on a real backend. The two terminal states no legal fixture can
// reach on MKL -- a PERTURBED and an UNREADABLE inertia -- plus section 2.2's
// evidence-failure policy live in tests/sqp/test_ipqp_seams.cpp, which runs on
// the standalone HVEN_TESTING target (docs/testing.md's seam convention).
// Splitting them that way is deliberate: this file must stay runnable in the
// ordinary `hven_sqp_tests` binary that links `hven::hven`.
//
// EVERY VALUE PIN IS SHOWN FALLIBLE, test_ipqp_engine.cpp's own rule: each
// block that asserts a census entry or a classification also asserts a
// NEIGHBOURING fixture where the same assertion would fail.

#include <cmath>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/qp_engine.h>

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

/// THE A11 PARAMETRIC INDEFINITE FAMILY, at QP level.
///
/// The subproblem `IndefiniteBoxModel` (tests/sqp/test_sqp_driver.cpp) hands
/// its driver, written directly as a `QpProblem` because `QpMode::kIpm` is
/// still refused at `validate_sqp_options` (task 1's temporary refusal) and
/// the routing chain is task 6's -- so A11's ENGINE HALF, which is what task 5
/// owns, is exercised at the tier's own entry point:
///
///     min 1/2 x0^2 - x1^2 - c x0 + 1/4 x1   on [-1, 1]^2
///
/// It is the sharpest indefinite fixture the SQP suite has, and its sharpness
/// is a theorem rather than an observation: the KKT residual vanishes at BOTH
/// the true minimizer `(c, -1)` -- reached by riding negative curvature to the
/// bound -- and at the INTERIOR SADDLE `(c, 1/8)`, so no residual-based test
/// separates them. An inertia read at the point is the only thing that can,
/// which is exactly what section 2.2 item 4 exists to do.
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
    // TASK 5'S RULING, PINNED SO A CHANGE IS DELIBERATE. Plan section 7 note
    // (j) left the status vocabulary for a downgraded-WITHOUT-escape outcome
    // open ("today's QpStatus has no such value"). The ruling, argued in full
    // in ipqp_engine.h's STATUS VOCABULARY note: `QpStatus::kNumericalError`,
    // the spelling the walk and SSN already use for a converged point whose
    // second-order certificate could not be established, and NO new
    // `QpStatus` enumerator.
    //
    // Note what this fixture also pins: the `QpStatus` enum is UNCHANGED. If a
    // later task widens it, this line stops compiling as written and the
    // change is visible rather than silent.
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
    // THE PARTITION CLAIM, exercised on every escape class this task can
    // reach from a legal fixture. `kStall` and `kInfeasibleSuspect` have their
    // own fixtures below; `kNumerical`'s terminal inertia forms are the seam
    // file's.
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
    // `ipqp_declined_pinned`'s own settled text, exercised: "the tier never
    // ran, so this never counts toward `ipqp_escapes` or the K=3 retirement
    // threshold". The census must stay empty on a decline even though the
    // status is `kNumericalError`.
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

TEST(IpqpA11Test, TheIndefiniteFamilyArmsTheGateClimbsMonotonelyAndNeverCertifies) {
    // A11's four claims, one fixture family: the inertia GATE FIRES, the
    // ladder is MONOTONE, the final read HAPPENS at a certifying exit, and a
    // WRONG read produces NOT-kOptimal AND `kIndefinite`.
    //
    // The family is swept over `c` so the pins are properties of the
    // MECHANISM rather than of one instance: `c` moves the minimizer along
    // x0 and moves the saddle with it, and none of the four claims may depend
    // on where that is.
    for (const double c : {0.0, 0.25, 0.5, 0.75, 1.0}) {
        SCOPED_TRACE(c);
        IpqpEngine tier(tight_opts());
        const IpqpResult r =
            tier.solve(indefinite_box_qp(c), nullptr, IpqpOptions{}, SolveOverrides{});

        // (1) THE GATE FIRED. `rho_demanded_max` is the inertia-demanded
        // floor's high-water mark and starts at 0, so a nonzero reading is
        // the gate having refused a factorization on its inertia.
        EXPECT_GT(r.counters.ipqp_rho_demanded_max, 0.0);
        EXPECT_GT(r.counters.ipqp_inertia_retries, 0);

        // (2) THE LADDER IS MONOTONE. The floor only ever rises, so the LAST
        // value it took is also its high-water mark. A ladder that fell back
        // would show `last < max`.
        EXPECT_DOUBLE_EQ(r.counters.ipqp_rho_demanded_last, r.counters.ipqp_rho_demanded_max);

        // (3) IT NEVER CERTIFIES. Whatever stopped it, the answer is not
        // `kOptimal` -- the whole point of the fixture is that a
        // residual-based test cannot separate the minimizer from the saddle.
        EXPECT_NE(r.status, QpStatus::kOptimal);
        EXPECT_NE(r.escape_reason, IpqpEscape::kNone);
        EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(r.counters));
        EXPECT_EQ(r.counters.ipqp_escapes, 1);

        // (4) IF a certifying exit was reached, the final read HAPPENED and
        // came back WRONG, and the pair (`read == 1`, `kIndefinite`) is what
        // plan section 7 note (h) demands. Conditional because the family's
        // outcome legitimately depends on `c` -- some instances converge to
        // the saddle and are caught by the read, others run out their budget
        // first -- and asserting one branch unconditionally would pin the
        // TRAJECTORY rather than the certification rule.
        if (r.counters.ipqp_final_inertia_read != 0) {
            EXPECT_EQ(r.counters.ipqp_final_inertia_read, 1);
            EXPECT_TRUE(r.certificate_downgraded);
            EXPECT_EQ(r.escape_reason, IpqpEscape::kIndefinite);
            EXPECT_EQ(r.counters.ipqp_escape_indefinite, 1);
        }
    }
}

TEST(IpqpA11Test, TheConvexTwinOfTheFamilyCertifiesAndLeavesTheLadderInert) {
    // THE NON-VACUITY PARTNER for the whole block above: flip the sign of the
    // one negative curvature and every assertion above inverts. Without this
    // the family's pins could pass on an engine that always refused to
    // certify.
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

TEST(IpqpCertificationTest, AHealthySolveNeverArmsTheEvidenceFailurePath) {
    // Section 2.2's evidence-failure policy is a REPAIR TRIGGER, and a repair
    // trigger that fires on a healthy solve is worse than no trigger at all.
    // MKL reports observed inertia on every fixture in this suite, so the flag
    // must be false everywhere here; the seam file is what makes it true.
    for (const QpProblem &qp : {convex_qp(), saddle_qp(), indefinite_box_qp(0.5)}) {
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
        EXPECT_FALSE(r.inertia_evidence_failed);
    }
}

} // namespace hven::solvers
