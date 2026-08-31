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
#include <stdexcept>
#include <vector>

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
    Index reached_read = 0;
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

        // (4) WHERE A CERTIFYING EXIT IS REACHED, the final read HAPPENED and
        // came back WRONG -- the pair (`read == 1`, `kIndefinite`) plan
        // section 7 note (h) demands.
        if (r.counters.ipqp_final_inertia_read != 0) {
            ++reached_read;
            EXPECT_EQ(r.counters.ipqp_final_inertia_read, 1);
            EXPECT_TRUE(r.certificate_downgraded);
            EXPECT_EQ(r.escape_reason, IpqpEscape::kIndefinite);
            EXPECT_EQ(r.counters.ipqp_escape_indefinite, 1);
        }
    }

    // THE COUNT, ASSERTED, so the conditional above cannot be vacuous
    // (co-review I-5). MEASURED TODAY: **no member of this family reaches the
    // final read at all.** Every one runs its 60-iteration budget out, because
    // the ladder's monotone floor biases the proximal problem and the
    // UNREGULARIZED residual the stopping rule reads never reaches target --
    // T4 concern 1's mechanism, seen here on the whole family (confirmed at a
    // 400-iteration budget too: still every member).
    //
    // Pinning the 0 is the point. It says what actually happens instead of
    // leaving an `if` that nothing enters, and if a future change makes a
    // member converge, this fires and whoever made it converge gets to
    // strengthen the block above. The final read's own A11 coverage is
    // `TheFinalReadIsReachedOnAConstrainedIndefiniteKkt` below, which
    // exists precisely because this number is 0.
    EXPECT_EQ(reached_read, 0);
}

TEST(IpqpA11Test, TheHSIndefiniteRowsArmTheGateAndClimbMonotonelyButNeverReachTheRead) {
    // A11'S ROW HALF (spec section 8.4: "A11 -- HS indefinite-Hessian rows
    // with the final-inertia certificate asserted ... asserts: the inertia
    // gate fires, the ladder is monotone within the solve, THE REQUIRED FINAL
    // UNREGULARIZED INERTIA READ HAPPENS, and a wrong read downgrades the
    // certificate rather than reporting kOptimal"). The parametric family
    // above is box-only -- its KKT signature counts no rows at all -- so on
    // its own it cannot exercise the part of section 2.2 item 4 that depends
    // on `expect_pos = n + mi` and `expect_neg = me + mi` being nonzero in the
    // row blocks.
    //
    // =====================================================================
    // A11'S HS HALF IS **NOT MET** BY THE ENGINE AS BUILT AT W1 HEAD.
    // =====================================================================
    //
    // SETTLER RULING (fix round 2): this is an ENGINE DEFICIENCY TO BE
    // SURFACED, not a fixture to be tuned. **NO HS indefinite row reaches the
    // section 2.2 item 4 read at all.** Measured on all three, at the 60-
    // iteration default cap AND at a 400-iteration cap: every one runs its
    // budget out and returns `kBudget` with `ipqp_final_inertia_read == 0`.
    // The third of A11's four claims -- "the required final unregularized
    // inertia read happens" -- therefore has no HS witness, and the fourth
    // ("a wrong read downgrades") cannot be reached on an HS row either.
    //
    // WHAT THIS TEST DOES ABOUT IT. It asserts what IS true and load-bearing
    // on each HS row (claims one and two, plus never-kOptimal), and it PINS
    // THE DEFICIENCY at `reached_read_hs == 0` so the day the engine starts
    // reaching the read is the day this test FAILS LOUDLY and someone must
    // flip the assertion to `== 3`. Budgets are NOT raised and tolerances are
    // NOT weakened to manufacture a read; a read manufactured that way would
    // assert the fixture rather than the engine, and would hide exactly the
    // thing that needs to stay visible.
    //
    // THE MECHANISM, from the fix-round-2 diagnosis (informational, in the
    // report and the ledger): the ladder's first escalation lands `rho_floor`
    // at a value FAR above the inertia-demanded minimum, the floor is monotone
    // per solve so it never comes back down, and the iteration degenerates
    // into a damped-gradient crawl whose step is O(1/rho). The unregularized
    // residual the stopping rule reads is then dominated by `rho (x - zeta)`
    // and converges only at the proximal outer rate, which 400 iterations do
    // not reach.
    //
    // THE ROW FIXTURES ARE THE SUITE'S OWN, NOT NEW ONES. All three come from
    // `tests/sqp/support/indefinite_fixtures.h`, which is where
    // test_qp_engine_indefinite.cpp's battery keeps them -- one implementation
    // shared by both consumers, with their multiplier derivations attached.
    // Two carry an equality row, two carry an inequality row, and one has two
    // negative eigenvalues.
    RecordProperty("a11_hs_half",
                   "NOT MET -- no HS indefinite row reaches the section 2.2 item 4 read at W1 "
                   "head; see the M6 W1 ledger and the T5 fix-round-2 diagnosis");

    struct HsCase {
        const char *name;
        QpProblem qp;
    };
    const std::vector<HsCase> hs = {
        {"hs_indefinite_equality", test_support::indefinite_equality_qp()},
        {"hs_indefinite_equality_and_row", test_support::indefinite_equality_and_row_qp()},
        {"hs_two_negative_eigenvalue_row", test_support::two_negative_eigenvalue_row_qp()},
    };

    Index reached_read_hs = 0;
    for (const HsCase &c : hs) {
        SCOPED_TRACE(c.name);
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(c.qp, nullptr, IpqpOptions{}, SolveOverrides{});

        // A11 CLAIM 1 -- THE INERTIA GATE FIRES on a CONSTRAINED indefinite
        // KKT. Both counters, because either alone can be satisfied without
        // the other being meaningful: `inertia_retries` is the count of
        // factorizations the gate REFUSED, `rho_demanded_max` is the level it
        // demanded, and both start at 0 on a convex subproblem.
        EXPECT_GT(r.counters.ipqp_inertia_retries, 0);
        EXPECT_GT(r.counters.ipqp_rho_demanded_max, 0.0);

        // A11 CLAIM 2 -- THE LADDER IS MONOTONE WITHIN THE SOLVE. The floor
        // only ever rises, so its LAST value is also its high-water mark, and
        // no down-then-up cycle was attempted (`ipqp_rho_flaps`, which counts
        // exactly that). A ladder that fell back would break the first; a
        // schedule that pushed under the floor and was refused would show in
        // the second.
        EXPECT_DOUBLE_EQ(r.counters.ipqp_rho_demanded_last, r.counters.ipqp_rho_demanded_max);
        EXPECT_EQ(r.counters.ipqp_rho_flaps, 0);
        EXPECT_GT(r.counters.ipqp_iters_at_elevated_rho, 0);

        // NEVER kOptimal -- the claim that survives whatever else happens.
        EXPECT_NE(r.status, QpStatus::kOptimal);
        EXPECT_NE(r.escape_reason, IpqpEscape::kNone);
        EXPECT_FALSE(r.certificate_downgraded);

        // WHAT ACTUALLY STOPS THEM, pinned rather than described: the budget.
        EXPECT_EQ(r.escape_reason, IpqpEscape::kBudget);
        EXPECT_EQ(r.status, QpStatus::kMaxIter);
        EXPECT_EQ(r.counters.ipqp_escapes, 1);
        EXPECT_EQ(r.counters.ipqp_escape_budget, 1);
        EXPECT_EQ(census_entries(r.counters), 1);
        EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(r.counters));
        EXPECT_EQ(r.counters.ipqp_iters, IpqpOptions{}.ipqp_hard_iter_cap);

        if (r.counters.ipqp_final_inertia_read != 0) {
            ++reached_read_hs;
        }
    }

    // THE DEFICIENCY, PINNED. **THE DAY THIS FAILS IS THE DAY A11'S HS HALF IS
    // MET** -- and whoever makes the engine reach the read on these rows must
    // flip this to `EXPECT_EQ(reached_read_hs, 3)`, add A11's claims three and
    // four to the loop above (`read == 1`, `certificate_downgraded`,
    // `kIndefinite`), delete the RecordProperty, and close the ledger item.
    // Until then the pin is the honest statement of where the tier is.
    EXPECT_EQ(reached_read_hs, 0)
        << "A11 HS half NOT MET -- no HS indefinite row reaches the section 2.2 item 4 read at "
           "W1 head; see ledger. If this now passes the read, flip the pin to 3 and assert the "
           "read's own outcome.";
}

TEST(IpqpA11Test, TheFinalReadIsReachedOnAConstrainedIndefiniteKkt) {
    // A11'S CLAIMS THREE AND FOUR -- "the required final unregularized inertia
    // read HAPPENS, and a wrong read DOWNGRADES the certificate rather than
    // reporting kOptimal" -- on a CONSTRAINED indefinite KKT, since the test
    // above records that no HS row reaches them.
    //
    // THIS IS NOT AN HS WITNESS AND IS NOT OFFERED AS ONE. It is the coverage
    // that exists today for the read itself; A11's HS half stays open above.
    //
    // `saddle_qp()` is task 4's own fixture (H = diag(2, -1), g = 0, symmetric
    // box: the origin is an exact KKT point of the barrier problem at every
    // `mu`, so the tier converges to a SADDLE in three iterations and the item
    // 4 read is what catches it). The second member is that same fixture
    // carrying ONE EQUALITY ROW satisfied at the origin, which is what puts a
    // row into the inertia signature (`expect_neg = me + mi` becomes nonzero)
    // while keeping the convergence property.
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

// ---------------------------------------------------------------------------
// Section 6.2 -- the early-stall test
// ---------------------------------------------------------------------------

namespace {

/// THE STALL FIXTURE'S MECHANISM, stated here rather than repeated per test.
///
/// Five CONSECUTIVE accepted steps with `min(alpha_p, alpha_d) < 1e-2` is a
/// hard thing to provoke, and that is section 6.2's own claim about itself:
/// "five accepted steps is an order of magnitude above the local regime, so no
/// healthy trajectory reaches it." Every naturally-jamming fixture tried --
/// razor-thin feasible slivers, an equality landing exactly on a bound, twelve
/// decades of Hessian spread against a huge gradient -- produces ONE tiny step
/// and then recovers, which is exactly the behaviour conjunct (iii) exists to
/// tolerate. (Two of them are the non-vacuity partners below.)
///
/// So the stall is provoked THROUGH A SHIPPED OPTION rather than through a
/// contrived problem: `ipqp_tau`, the fraction-to-boundary parameter, at
/// `1e-3`. That is legal (`validate_sqp_options` admits any value in `(0, 1)`)
/// and it manufactures precisely the trajectory section 6.2 describes -- every
/// blocked step capped at a thousandth, `mu` therefore barely moving, and the
/// residual therefore barely moving -- on a problem the tier otherwise solves
/// in eleven iterations. The fixture is honest about what it is: a caller who
/// asks for a crawl gets a STALL DIAGNOSIS at ten iterations instead of a
/// sixty-iteration budget burn, which is the whole point of section 6.1's
/// "budget of LAST resort".
IpqpOptions crawling_opts(double tau) {
    IpqpOptions io;
    io.ipqp_tau = tau;
    return io;
}

/// A feasible QP whose Newton direction overshoots its box by ten decades on
/// the first step: `H` spans fourteen decades and the gradient is `1e10`, so
/// `alpha_p` is ~8e-10 once and ~1 thereafter. The healthy-solve partner for
/// conjunct (iii).
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

/// An INFEASIBLE QP: `x0 + x1 <= -2` and `x0 + x1 >= 2` on a box containing
/// neither. The primal residual is flat on a positive floor and the
/// multipliers price the contradiction without limit -- section 6.3's two
/// signals, from a problem that genuinely has them.
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

} // namespace

TEST(IpqpStallTest, TheStallEscapeCarriesAllThreeConjunctValuesInOneEvidenceBlock) {
    // PLAN SECTION 7 NOTE (b), FINAL: the three `ipqp_stall_reason_*`
    // counters are DROPPED as ill-posed -- all three conjuncts hold at every
    // stall escape, so a partition among them is degenerate -- and the three
    // VALUES travel in the escape's own evidence block instead. This is that
    // note's ONE pin, replacing v2's three per-reason fixtures.
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

    // SECTION 6.1'S "BUDGET OF LAST RESORT", MEASURED AND BOUNDED TIGHTLY.
    // `< 60` is far too weak a form of the claim -- 59 would pass it -- so the
    // pin is the STRUCTURAL bound the mechanism implies: the window can close
    // at most twice before firing here (once evaluated and re-armed on the
    // first close, once fired on the second), so `2 * ipqp_stall_window` is
    // the ceiling and a stall that crept toward the cap fails.
    EXPECT_LE(r.counters.ipqp_iters, 2 * IpqpOptions{}.ipqp_stall_window);
    EXPECT_GE(r.counters.ipqp_iters, IpqpOptions{}.ipqp_stall_window);
    // ... and the exact measured value, so a trajectory move is VISIBLE rather
    // than silently absorbed by the bound above. 10 == two closed windows.
    EXPECT_EQ(r.counters.ipqp_iters, 10);
    EXPECT_EQ(r.counters.ipqp_escape_budget, 0);

    // MUTATION PARTNER: `tau = 1e-2` on the same problem crawls too, but its
    // steps sit at ~6.5e-3 to ~3.3e-1 -- so conjunct (iii)'s WHOLE-WINDOW
    // demand is not met and the solve pays the full budget instead. The stall
    // escape is therefore a statement about the trajectory and not about the
    // option.
    IpqpEngine slow(tight_opts());
    const IpqpResult s = slow.solve(convex_qp(), nullptr, crawling_opts(1e-2), SolveOverrides{});
    EXPECT_EQ(s.escape_reason, IpqpEscape::kBudget);
    EXPECT_FALSE(s.stall_evidence.fired);
    EXPECT_EQ(s.counters.ipqp_escape_stall, 0);
}

TEST(IpqpStallTest, AHealthySolveNeverTripsConjunctThreeOnOneTinyAlphaStep) {
    // "NEVER ABORT ON ONE TINY-ALPHA ITERATION" (spec 6.2) -- conjunct (iii)
    // is a whole-window property BY CONSTRUCTION, and these two fixtures are
    // what makes that testable: both take a genuinely tiny step (the pin
    // asserts it, so the fixture cannot silently stop being one) and both
    // certify.
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
    // SPEC 6.3'S HARD RULE: "It never returns `QpStatus::kInfeasible`" -- that
    // is a CERTIFICATE word in this driver (solver_status.h; under kSsn a
    // `kInfeasible` can only have come from the walk), and IP-PMM has no
    // homogeneous self-dual embedding and therefore no infeasibility
    // certificate at all.
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
    // SPEC 6.3'S EXHAUSTION VARIANT. Reached by capping the iteration budget
    // so the solve stops BETWEEN window closures: the standing route has not
    // fired, the windowed growth reference is only two steps old, and the
    // route falls back on the start-point reference plus
    // `kSsnDualStepGrowth` across the most recently accepted step.
    //
    // THE CAP IS TRAJECTORY-DEPENDENT AND IS PINNED AS SUCH. 12 is where this
    // fixture's divergence and the budget stop coincide; if the trajectory
    // ever moves, this fails loudly rather than silently testing the standing
    // route again -- which the `exhaustion_route` assertion below is what
    // makes true.
    IpqpOptions io;
    io.ipqp_hard_iter_cap = 12;
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

    // MUTATION PARTNER: one more iteration and the per-step growth conjunct is
    // no longer met, so the same stop is a PLAIN BUDGET escape. Without this
    // the pin above could pass on an engine that relabelled every budget stop
    // as a suspicion.
    IpqpOptions io2;
    io2.ipqp_hard_iter_cap = 13;
    IpqpEngine tier2(tight_opts());
    const IpqpResult r2 = tier2.solve(infeasible_rows_qp(), nullptr, io2, SolveOverrides{});
    EXPECT_EQ(r2.escape_reason, IpqpEscape::kBudget);
    EXPECT_FALSE(r2.infeasibility_evidence.fired);
    EXPECT_EQ(r2.counters.ipqp_escape_budget, 1);
}

TEST(IpqpInfeasibleSuspectTest, AFeasibleSolveNeverRaisesTheSignature) {
    // The non-vacuity partner for the whole section: on a feasible,
    // well-posed subproblem neither signal is ever raised, whatever the
    // outcome. A suspicion generator that fires on healthy problems would
    // route every subproblem to the W2 feasibility hook.
    for (const QpProblem &qp : {convex_qp(), one_tiny_step_qp(), saddle_qp()}) {
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
        EXPECT_FALSE(r.infeasibility_evidence.fired);
        EXPECT_EQ(r.counters.ipqp_escape_infeasible_suspect, 0);
        EXPECT_EQ(r.infeasibility_evidence.least_infeasible_x.size(), 0);
    }
}

// ---------------------------------------------------------------------------
// Section 6.1 -- the escape ladder
// ---------------------------------------------------------------------------

namespace {

/// A tier outcome shaped by hand. The ladder reads exactly two fields, and
/// building results directly rather than by solving keeps the ladder's own
/// rules separable from the engine's classification -- which is the point of
/// `IpqpEscapeLadder` being a type and not a member.
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

    // A DOWNGRADED-WITHOUT-ESCAPE SOLVE IS A SUCCESS HERE (task 5's ruling,
    // from plan section 7 note (j): "no census entry, no section 6.1 K = 3
    // charge"). Without this rule, three clean solves under
    // `ipqp_require_final_inertia = false` would sit on top of an old tally.
    EXPECT_FALSE(ladder.record(downgraded_without_escape(), 6));
    EXPECT_EQ(ladder.consecutive_escapes(), 0);
    EXPECT_FALSE(ladder.retired());
}

TEST(IpqpEscapeLadderTest, ADeclineIsNeutralAndNeitherAdvancesNorResetsTheTally) {
    // `ipqp_declined_pinned`'s settled text: "the tier never ran, so this
    // never counts toward `ipqp_escapes` or the K=3 retirement threshold".
    // The other half is task 5's ruling and is what this fixture separates
    // from the reset rule: a decline is not a SUCCESS either.
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
    // The three fixtures above build `IpqpResult`s directly, which keeps the
    // ladder's rules separable -- but a ladder that only ever saw hand-built
    // results could be reading fields the engine never sets. This drives it
    // from three REAL solves: two escapes and one certifying solve, in the
    // order that must NOT retire, then three escapes in a row, which must.
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

} // namespace hven::solvers
