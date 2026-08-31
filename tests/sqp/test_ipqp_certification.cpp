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
#include <string>
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

TEST(IpqpA11Test, TheIndefiniteFamilyRidesNegativeCurvatureToABoundAndCertifiesHonestly) {
    // A11's four claims on the parametric family, RE-DERIVED AT T4b. Before
    // T4b every member of this family burnt its whole 60-iteration budget at
    // an exact frozen fixed point of the modified problem, and this test
    // pinned that (`reached_read == 0`). With section 3.2's schedule and
    // section 2.2's modification separated, the step's right-hand side and the
    // gate describe the SAME subproblem again, and every member now walks the
    // negative curvature to its bound and reaches the section 2.2 item 4 read.
    //
    // WHAT MAKES THE FAMILY SHARP, restated because it is now being used the
    // other way round. `indefinite_box_qp(c)` has a vanishing KKT residual at
    // BOTH `(c, -1)` (a bound minimizer) and `(c, 1/8)` (an INTERIOR SADDLE),
    // so no residual-based test separates them. What this test asserts is that
    // the tier lands on a BOUND -- a genuine local minimizer of the QP -- and
    // never on the saddle, and that the read it then takes agrees. Both ends
    // of the box are local minimizers of a concave one-dimensional section, so
    // "which end" is not asserted: A11 is about the HONESTY of the
    // certificate, not about global optimality (T4b plan section 5, risk 3).
    //
    // The family is swept over `c` so the pins are properties of the
    // MECHANISM rather than of one instance: `c` moves the minimizer along x0
    // and moves the saddle with it, and no claim may depend on where that is.
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

        // (2) THE LADDER IS ALGORITHM IC'S, NOT A MONOTONE FLOOR. This line
        // used to read `EXPECT_DOUBLE_EQ(last, max)` -- the executable form of
        // "the floor only ever rises". The memory is now free to fall, and on
        // four of the five members it ends orders of magnitude below the
        // solve's own peak. `<=` is asserted on every member (a memory ABOVE
        // the high-water mark would be a bookkeeping bug); the strict
        // inequality is counted, so the non-monotone claim is a measurement
        // rather than a possibility.
        EXPECT_LE(r.counters.ipqp_rho_demanded_last, r.counters.ipqp_rho_demanded_max);
        if (r.counters.ipqp_rho_demanded_last < r.counters.ipqp_rho_demanded_max) {
            ++non_monotone;
        }

        // (3) IT CONVERGES, AND THE CERTIFICATE IT REPORTS IS HONEST. The
        // point is at a BOUND of the negative-curvature coordinate, where
        // `H + Sigma` really is positive definite, and it is NOT the interior
        // saddle at `x1 = 1/8` -- which is the discriminating assertion this
        // fixture was built for and which nothing could satisfy before T4b.
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

        // (4) THE FINAL READ HAPPENED AND AGREED. `0` is both "agreed" and the
        // structural value on a solve that never reached the read, so the
        // certifying exit above is what makes it unambiguous here; the
        // option-off partner below turns the same solve's read into a `3` and
        // is what proves the factorization is genuinely being paid.
        EXPECT_EQ(r.counters.ipqp_final_inertia_read, 0);
        ++reached_read;

        // (5) THE MODIFICATION IS ABSENT FROM THE ANSWER. `rho` is the
        // schedule's final value and `rho_mod` the modification the last step
        // ran at; the read itself is taken at `rho_dem = 0` by construction,
        // which is what makes `read == 0` a statement about the caller's QP.
        EXPECT_LT(r.rho, IpqpOptions{}.ipqp_rho_init);
        EXPECT_GE(r.rho_mod, 0.0);
        EXPECT_LE(r.rho_mod, r.counters.ipqp_rho_demanded_max);
    }

    EXPECT_EQ(armed, 5);
    EXPECT_EQ(reached_read, 5) << "every member reaches the section 2.2 item 4 read";
    EXPECT_GT(non_monotone, 0) << "and at least one settles below its own peak -- the executable "
                                  "statement that the monotone floor is gone";

    // THE READ IS REALLY BEING PAID, and this is how that is proved rather
    // than assumed: the same fixture with `ipqp_require_final_inertia` off
    // reports `3` (NOT PERFORMED), a downgrade with no escape, and one
    // factorization fewer. Without this partner, `read == 0` above would be
    // compatible with an engine that never took the read at all.
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
    // A11'S HS HALF IS **MET** AS OF M6 W1 T4b. IT WAS NOT AT W1 HEAD.
    // =====================================================================
    //
    // WHAT THIS TEST USED TO SAY, kept because the delta is the evidence:
    // "NO HS indefinite row reaches the section 2.2 item 4 read at all",
    // measured on all three at the 60-iteration default cap AND at a
    // 400-iteration cap -- every one ran its budget out and returned
    // `kBudget` with `ipqp_final_inertia_read == 0`, at an EXACT FROZEN FIXED
    // POINT (full steps, `|dx|` at 1e-17, a bit-identical residual for the
    // last ~55 iterations). The pin was `reached_read_hs == 0`, written so
    // that the day the engine reached the read this test would fail loudly.
    // It did.
    //
    // WHAT CHANGED (T4b, plan section 7 note (p)). The tier carried TWO
    // regularizations and used ONE number for both. The step's right-hand side
    // was built with the total `max(rho_sched, rho_floor)` while the section
    // 3.2 gate measured the residual of the subproblem `rho_sched` defines, so
    // the iterate converged to the KKT point of a DIFFERENT problem than the
    // gate was watching -- the gate's residual then sat at a nonzero constant
    // (`0.99 x` the stopping residual, traced and pinned in
    // docs/notes/2026-08-31-m6-w1-t4b-step0-trace.md) that could not contract
    // because `x` did not move, so the schedule never advanced again and the
    // monotone floor made the first climb's overshoot permanent. T4b separates
    // the two, deletes the monotone floor in favour of Algorithm IC's memory,
    // and applies the modification uniformly in the Ruiz-scaled system.
    //
    // ALL THREE ROWS NOW CONVERGE TO A LOCAL MINIMIZER, REACH THE READ, AND
    // REPORT AN HONEST STANDING CERTIFICATE. Each returned point is one of the
    // minimizers derived by hand in `support/indefinite_fixtures.h`'s own
    // header comments -- which is what makes "honest" checkable rather than
    // asserted.
    //
    // THE ROW FIXTURES ARE THE SUITE'S OWN, NOT NEW ONES: two carry an
    // equality row, two an inequality row, one has two negative eigenvalues.
    RecordProperty("a11_hs_half", "MET at M6 W1 T4b: all three HS indefinite rows converge, reach "
                                  "the section 2.2 item 4 read and certify at a derived local "
                                  "minimizer");

    // THE ADMISSIBLE TRAJECTORIES, EXACT, ONE ROW PER MEASURED OUTCOME.
    //
    // WHY TWO ENTRIES FOR THE FIRST ROW, said plainly rather than hidden in a
    // loose bound. `indefinite_equality_qp` has TWO local minimizers -- its own
    // header derives both, `(1, -2, 2)` at objective -6 and `(1, 2, -2)` at
    // -4 -- and which one a negative-curvature walk reaches is decided early,
    // by which of two nearly-tied inertia readings comes back first. That
    // decision is not stable across FLAG REGIMES: Debug reaches `(1, -2, 2)`
    // in 27 iterations and 42 factorizations, Release reaches `(1, 2, -2)` in
    // 24 and 36, and both are correct answers with a standing certificate.
    // Rather than weaken the pin to a bound, both trajectories are written
    // out and the result must match ONE OF THEM EXACTLY -- so a change to the
    // ladder still fails here, and the sensitivity is documented instead of
    // being absorbed. The other two rows reach the same point with the same
    // counts in both regimes and have one entry each. Recorded as a T4b
    // finding: an inertia ladder near its own threshold is flag-sensitive on
    // a fixture with tied minimizers, which T9 should measure rather than be
    // surprised by.
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
         {{27, 42, vec({1.0, -2.0, 2.0})}, {24, 36, vec({1.0, 2.0, -2.0})}}},
        // x = (1, -2, -0.5), objective -1.375: the second of that fixture's
        // two derived minimizers, x1 at LOWER and the general row active.
        {"hs_indefinite_equality_and_row",
         test_support::indefinite_equality_and_row_qp(),
         {{22, 31, vec({1.0, -2.0, -0.5})}}},
        // x = (-2, -2, -0.1), objective -4.205: one of the five vertices that
        // fixture's header enumerates, row slack. A LOCAL and not the global
        // minimizer, which is exactly what that fixture exists to accept.
        {"hs_two_negative_eigenvalue_row",
         test_support::two_negative_eigenvalue_row_qp(),
         {{50, 75, vec({-2.0, -2.0, -0.1})}}},
    };

    Index reached_read_hs = 0;
    for (const HsCase &c : hs) {
        SCOPED_TRACE(c.name);
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(c.qp, nullptr, IpqpOptions{}, SolveOverrides{});

        // A11 CLAIM 1 -- THE INERTIA GATE FIRES on a CONSTRAINED indefinite
        // KKT. Both counters, because either alone can be satisfied without
        // the other being meaningful: `inertia_retries` counts factorizations
        // the gate REFUSED, `rho_demanded_max` is the level it demanded, and
        // both start at 0 on a convex subproblem.
        EXPECT_GT(r.counters.ipqp_inertia_retries, 0);
        EXPECT_GT(r.counters.ipqp_rho_demanded_max, 0.0);

        // A11 CLAIM 2 -- THE LADDER IS ALGORITHM IC'S. This used to assert
        // `last == max` (the monotone floor's executable form). The memory now
        // ends BELOW the peak on every one of these rows, which is the
        // property that lets the walk finish: a floor pinned at the first
        // climb's overshoot is what froze them.
        EXPECT_LT(r.counters.ipqp_rho_demanded_last, r.counters.ipqp_rho_demanded_max);
        EXPECT_GT(r.counters.ipqp_iters_at_elevated_rho, 0);

        // A11 CLAIMS 3 AND 4 -- THE READ HAPPENS, AND THE CERTIFICATE IT
        // REPORTS IS HONEST. Here it STANDS, because each row really does
        // converge to a local minimizer; the "a wrong read downgrades" half of
        // claim 4 is carried by
        // `TheFinalReadIsReachedOnAConstrainedIndefiniteKkt` below and by the
        // settled-band fixture, both of which converge to genuine saddles.
        EXPECT_EQ(r.status, QpStatus::kOptimal);
        EXPECT_EQ(r.escape_reason, IpqpEscape::kNone);
        EXPECT_FALSE(r.certificate_downgraded);
        EXPECT_EQ(r.counters.ipqp_final_inertia_read, 0);
        EXPECT_EQ(r.counters.ipqp_escapes, 0);
        EXPECT_EQ(census_entries(r.counters), 0);
        EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(r.counters));
        EXPECT_LE(r.residuals.worst(), tight_opts().opt_tol * IpqpOptions{}.ipqp_converge_slack);
        ++reached_read_hs;

        // WHERE IT LANDED, AND WHAT IT COST -- matched as ONE WHOLE
        // TRAJECTORY against the admissible list, so the point and its exact
        // iteration and factorization counts are pinned together and a change
        // in the ladder cannot be absorbed by a loose bound on either.
        // "Honest certificate" is checked against arithmetic done by hand in
        // the fixture's own header rather than against whatever the engine
        // produced.
        RecordProperty(std::string(c.name) + "_iters", std::to_string(r.counters.ipqp_iters));
        RecordProperty(std::string(c.name) + "_factorizations",
                       std::to_string(r.counters.ipqp_factorizations));
#ifdef USE_ACCELERATE_SPARSE
        // The admissible list is an exact trajectory pin, so it is MKL-scoped
        // and UNOBSERVED elsewhere (CLAUDE.md section 6; T4b fix round 2, F5).
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

    // NON-VACUITY FOR THE READ ITSELF, on the HS rows specifically: with
    // `ipqp_require_final_inertia` off the same row reports `3` (NOT
    // PERFORMED) and a downgrade, and pays exactly one factorization fewer. A
    // `read == 0` that came from a solve which never took the read would be
    // indistinguishable without this.
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

// ---------------------------------------------------------------------------
// T4b close gates 7-10: what the separated ladder is and is not
// ---------------------------------------------------------------------------

namespace {

/// T4b close gate 7's fixture: INDEFINITE ON THE EQUALITY'S NULL SPACE, WITH
/// NO ACTIVE BOUND.
///
///     min 1/2 (x0^2 + lam x1^2)   s.t.  x0 = 0.5,  x in [-10, 10]^2
///
/// The equality eliminates x0, so `null(A) = span(e1)` and the REDUCED Hessian
/// is the scalar `lam` -- known analytically, which is the whole point: on a
/// fixture with active bounds the threshold is set by the assembled Schur
/// complement with `Sigma` and a finite `delta_sched` and no closed form is
/// available. `g1 = 0` and a SYMMETRIC box make `x1 = 0` an exact KKT point of
/// the barrier problem at every `mu` (the two bound terms cancel), so the tier
/// converges there with the bounds ten units away and `Sigma ~ 2 mu / 100`,
/// i.e. numerically absent from the inertia. It is a SADDLE, so the section
/// 2.2 item 4 read must also downgrade.
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

/// T4b close gate 8's fixture: a direction whose binding bounds are WEAKLY
/// ACTIVE. `H = diag(2, h)`, `g = 0`, and a SYMMETRIC box of half-width `s` on
/// the second coordinate, so `x1 = 0` is an exact KKT point of the barrier
/// problem at every `mu` and the tier converges there with `x1 - l = s`,
/// `z = mu / s` and `Sigma = 2 mu / s^2`. Sweeping `s` sweeps the activity of
/// the two bounds through the `sqrt(mu)` regime.
///
/// `h < 0` makes `x1 = 0` a SADDLE (the maximum of a concave section on a
/// symmetric interval) whose certificate must FALL; `h > 0` makes it the
/// MINIMIZER, on identical geometry, whose certificate must STAND. The pair is
/// what separates "the read verifies the critical cone" from "the read
/// downgrades anything with a weak bound".
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

/// T4b close gate 10's fixture: an ADVERSARIAL long negative-curvature walk.
///
///     min 1/2 (2 x0^2 - 0.1 x1^2) - 2 x0 - 0.01 x1   on [-100, 100]^2
///
/// Three properties, each chosen to make the walk as long as the design
/// allows. The additional shift's threshold is SMALL (`theta = 0.1 -
/// rho_sched`, about 0.099 once the schedule has decayed), so the ladder is
/// armed at a level that buys very little curvature; the binding bound is 100
/// units away FROM THE START, so the walk down the negative-curvature
/// direction is long; and the objective's slope along that direction is small,
/// so nothing hurries it. While the ladder is armed the section 3.2 gate is
/// SILENT by construction -- the schedule's residual GROWS along a direction
/// of negative reduced curvature -- so this is the fixture that says how long
/// "silent" can get and whether section 6.2 mistakes it for a stall.
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
    // T4b CLOSE GATE 7. On a fixture whose reduced Hessian is known in closed
    // form, the shift the ladder SETTLES at must sit inside the band Algorithm
    // IC's two factors define: it is refused below the threshold and accepted
    // at most `kIpqpLadderUp` times a refused value, so
    //
    //     theta < rho_dem_settled <= kIpqpLadderUp * theta,
    //     theta = |lambda_min(reduced H)| - rho_sched.
    //
    // WHY THIS GATE EXISTS. "The A11 pin flips 0 -> 3" is compatible with a
    // ladder that settles at 40 when 2 would do -- the solve would still
    // converge, just expensively, and the counter table would not say so. The
    // band is the statement that the /3 probe really does walk the shift back
    // down to the smallest value the curvature needs.
    const double lam = -2.0;
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(equality_null_space_indefinite_qp(lam), nullptr, IpqpOptions{},
                                    SolveOverrides{});

    // THE PRECONDITIONS THE ANALYTIC BAND RESTS ON, asserted rather than
    // assumed. (a) it converged, so there IS a settled value; (b) the point is
    // the one the fixture was built around; (c) NO BOUND IS ACTIVE, so the
    // reduced Hessian really is the analytic `lam` and not `lam + Sigma`.
#ifdef USE_ACCELERATE_SPARSE
    RecordProperty("t4b_gate7_accelerate", "UNOBSERVED -- the exact iteration count is MKL-only");
    ASSERT_GT(r.counters.ipqp_iters, 0);
#else
    ASSERT_EQ(r.counters.ipqp_iters, 30);
#endif
    EXPECT_NEAR(r.x(0), 0.5, 1e-9);
    EXPECT_NEAR(r.x(1), 0.0, 1e-6);
    EXPECT_LT(sigma_at(r, equality_null_space_indefinite_qp(lam), 1), 1e-6)
        << "no active bound: Sigma must be numerically absent from the read";

    const double theta = std::abs(lam) - r.rho;
    EXPECT_GT(r.rho_mod, theta) << "a settled value at or below the threshold would have been "
                                   "refused by the inertia gate";
    EXPECT_LE(r.rho_mod, detail::kIpqpLadderUp * theta)
        << "and one above 8 x the threshold means the ladder never walked its first climb's "
           "overshoot back down";
    RecordProperty("t4b_gate7_theta", std::to_string(theta));
    RecordProperty("t4b_gate7_rho_mod", std::to_string(r.rho_mod));
    RecordProperty("t4b_gate7_rho_demanded_max", std::to_string(r.counters.ipqp_rho_demanded_max));

    // THE FIRST CLIMB IS *NOT* IN THE BAND, and that is the non-vacuity
    // partner: the solve's PEAK is the two-decade first climb's landing point,
    // far outside `(theta, 8 theta]`. If the band pin ever passed by accident
    // -- because the peak happened to be small -- this would fail first.
    EXPECT_GT(r.counters.ipqp_rho_demanded_max, detail::kIpqpLadderUp * theta)
        << "the first climb overshoots by construction; the band is a claim about where the "
           "ladder SETTLES, not about where it starts";

    // ... AND THE POINT IS A SADDLE, so the section 2.2 item 4 read must
    // downgrade. `x1 = 0` maximizes `-x1^2` on a symmetric box.
    EXPECT_EQ(r.counters.ipqp_final_inertia_read, 1);
    EXPECT_TRUE(r.certificate_downgraded);
    EXPECT_EQ(r.escape_reason, IpqpEscape::kIndefinite);
    EXPECT_NE(r.status, QpStatus::kOptimal);

    // THE HS ROWS GET THE OBSERVED-THRESHOLD FORM (gate 7's own scoping). With
    // bounds active, a finite `delta_sched` and Ruiz on, the target inertia is
    // `G + A'A/delta > 0` rather than "G positive definite on null(A)", so the
    // ANALYTIC band is not valid there and the plan asks for the OBSERVED
    // threshold instead: record the smallest sufficient shift and assert
    // `rho_d_settled <= kIpqpLadderUp x` it.
    //
    // HOW THE THRESHOLD IS MEASURED. No boundary knob sets `rho_dem`, so the
    // probe reaches the same diagonal through `ipqp_rho_init`: a solve capped
    // at one iteration reports `ipqp_inertia_retries == 0` iff that shift
    // already sufficed at iteration 0. `write_diagonals` makes the two
    // interchangeable only where `dsq[i] == 1`, so BOTH sides run with
    // equilibration off; the default solve keeps the weaker round-1 property
    // beside them. Descending powers of two bracket the threshold to a factor
    // of two, which is what `2 * kIpqpLadderUp` carries, along with the fact
    // that the threshold is read at iteration 0 and the settled value at a
    // later one. (T4b fix round 2, F3; argument, the measured table and the two
    // residual concerns in `.superpowers/w1-t4b-report.md`.)
    int hs_row = 0;
    for (const QpProblem &qp :
         {test_support::indefinite_equality_qp(), test_support::indefinite_equality_and_row_qp(),
          test_support::two_negative_eigenvalue_row_qp()}) {
        // One suffix per row: a bare key records only the last fixture.
        const std::string row = "_hs" + std::to_string(hs_row++);
        SCOPED_TRACE(row);
        IpqpEngine hs_tier(tight_opts());
        const IpqpResult h = hs_tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
        ASSERT_GT(h.counters.ipqp_rho_demanded_max, 0.0);
        EXPECT_LE(h.counters.ipqp_rho_demanded_last,
                  h.counters.ipqp_rho_demanded_max / detail::kIpqpLadderDown);
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
}

TEST(IpqpCertificationTest, TheFinalReadVerifiesDirectionsOffAWeaklyActiveBoundInsteadOfMasking) {
    // T4b CLOSE GATE 8, **REPAIRED** IN FIX ROUND 1 (settler ruling R1, on the
    // tycho-sqp lane's escalation and Codex's CRITICAL finding).
    //
    // WHAT THE ROUND-1 ENGINE DID, and why it was a wrong answer rather than a
    // registered gap. Second-order conditions are stated on the CRITICAL CONE.
    // At a bound whose multiplier is essentially zero, the direction moving OFF
    // that bound is IN the cone, so the section 2.2 item 4 read has to verify
    // it. The barrier's own curvature `Sigma = z / gap` masks it: at a weakly
    // active bound `z` and `gap` are BOTH of order `sqrt(mu)`, so `Sigma` is of
    // order one and cancels a genuinely negative eigenvalue. Measured on this
    // very family: for every `s` with `2 mu / s^2 > 1` the tier certified
    // `x = 0` -- a SADDLE -- as `kOptimal`. That is
    // `kOptimal`-at-a-non-minimizer, the exact class task 4 closed for the SSN
    // kernel.
    //
    // WHAT THE READ DOES NOW: it drops the `Sigma` contribution of a bound side
    // that is weakly active -- BOTH its slack and its multiplier below
    // `kIpqpWeakActiveFactor * sqrt(mu)` -- before its single factorization.
    // Strongly active sides keep their curvature (they are not in the cone) and
    // inactive sides are unaffected. Same one factorization.
    //
    // THE FIXTURE. `H = diag(2, h)`, `g = 0`, and a SYMMETRIC box of half-width
    // `s` on the second coordinate: `x1 = 0` is an exact KKT point of the
    // barrier problem at every `mu` (the two bound terms cancel), so the tier
    // always converges there, in three iterations, and NEVER arms the ladder
    // (`rho_0 = 8` covers `|h| = 1` from the first factorization) -- which is
    // what makes this a test of the READ. Sweeping `s` sweeps the activity of
    // the two bounds. With `h = -1` the point is a SADDLE and the certificate
    // must fall; with `h = +1` it is the MINIMIZER and the certificate must
    // stand, on the same geometry.
    Index dropped_and_downgraded = 0;
    Index kept_and_stood = 0;
    Index skipped_ties = 0;
    for (const double s :
         {5.0e-4, 3.0e-4, 2.0e-4, 1.8e-4, 1.58e-4, 1.4e-4, 1.2e-4, 1.0e-4, 5.0e-5, 1.0e-5}) {
        SCOPED_TRACE(s);
        const QpProblem saddle = weakly_active_indefinite_qp(s, -1.0);
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(saddle, nullptr, IpqpOptions{}, SolveOverrides{});

        // The family's invariants, so the sweep is one experiment with one
        // variable moving. The iteration count is an exact trajectory value and
        // is MKL-scoped (T4b fix round 2, F5).
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

        // THE KNIFE-EDGE MEMBERS ARE SKIPPED, not asserted (fix round 1, I5).
        // A member whose `Sigma` sits within 1% of `|H11|` leaves the read's
        // matrix within a hair of singular, and demanding that a factorization
        // resolve that sign is the registered tie-flake class
        // (`WeaklyActiveRowFinishesUncertain`, M6 register 2026-08-26). The
        // sweep still crosses the boundary; it just does not stand on it.
        const double sigma = sigma_at(r, saddle, 1);
        if (std::abs(sigma - 1.0) < 1e-2) {
            ++skipped_ties;
            continue;
        }

        // THE RULE THE READ NOW APPLIES, recomputed here from the returned
        // point rather than read out of the engine: a bound side is weakly
        // active iff BOTH its gap and its multiplier are below
        // `kIpqpWeakActiveFactor * sqrt(mu)`. This family's two bounds are
        // symmetric, so they are weak together or strong together.
        const double weak_scale = detail::kIpqpWeakActiveFactor * std::sqrt(r.mu);
        const double gap = r.x(1) - saddle.lower(1);
        const bool weak = gap <= weak_scale && r.zl(1) <= weak_scale;

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
            // STRONGLY ACTIVE: the direction off the bound is NOT in the
            // critical cone, the curvature is genuinely constrained, and the
            // read keeps it. `Sigma` here is 250, not 1.28: the barrier has
            // resolved this bound, and the residual exposure is an accuracy
            // question about where the solve stopped rather than a critical-cone
            // question. Recorded as a concern in the T4b report, not papered
            // over.
            EXPECT_GT(sigma, 1.0);
            EXPECT_EQ(r.counters.ipqp_final_inertia_read, 0);
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

    // The exact split, not just its existence: a drift in WHICH members are
    // weak moves a member between the two branches and leaves every `> 0` form
    // true. Eight, not nine, because `s = 1.58e-4` is skipped as a tie before
    // it is classified. (T4b fix round 2, F1; reconciliation with the report's
    // FR-2 table in `.superpowers/w1-t4b-report.md`.)
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
    // loop: `weakly_active_indefinite_qp(1.4e-4)` has `Sigma ~ 1.28 > |-1|` and
    // was the concrete case filed as CRITICAL. It downgrades.
    {
        const QpProblem qp = weakly_active_indefinite_qp(1.4e-4, -1.0);
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
        EXPECT_GT(sigma_at(r, qp, 1), 1.0) << "the masking really is in force at this member";
        EXPECT_EQ(r.counters.ipqp_final_inertia_read, 1);
        EXPECT_TRUE(r.certificate_downgraded);
        EXPECT_EQ(r.escape_reason, IpqpEscape::kIndefinite);
        EXPECT_NE(r.status, QpStatus::kOptimal);
    }

    // **THE DEFECT, PINNED AS A MUTATION PARTNER.** Restoring the masked
    // `Sigma` is what the un-repaired read did, and it flips the same saddle
    // back to standing. There is no option that restores it -- the repair is
    // unconditional -- so the partner is built out of the arithmetic instead:
    // at the returned point the read's matrix is `H + Sigma` with the weak
    // sides dropped, and `H11 + sigma` is what the OLD read factorized. The
    // assertion is that the old quantity is POSITIVE (so the old read would
    // have agreed and the certificate would have stood) while the new one is
    // NEGATIVE. A regression that put the masking back would make the two
    // agree and this would fail.
    {
        const QpProblem qp = weakly_active_indefinite_qp(1.4e-4, -1.0);
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
        const double h11 = -1.0;
        EXPECT_GT(h11 + sigma_at(r, qp, 1), 0.0)
            << "the UN-repaired read's matrix is positive definite here -- that is the defect";
        EXPECT_LT(h11, 0.0) << "and the repaired read's is not, because Sigma is dropped";
        EXPECT_TRUE(r.certificate_downgraded) << "and the engine takes the repaired answer";
    }

    // **THE PARTNER THE RULING ASKS FOR: A TRUE WEAKLY-ACTIVE MINIMIZER STILL
    // CERTIFIES.** Same geometry, same weak bounds, curvature flipped -- so the
    // dropped `Sigma` leaves `H11 = +1 > 0` and the read agrees. Without this,
    // "the saddle downgrades" would be satisfied by a rule that downgrades
    // everything with a weakly active bound.
    Index minimizers_certified = 0;
    for (const double s : {5.0e-4, 1.8e-4, 1.4e-4, 1.0e-4, 5.0e-5}) {
        SCOPED_TRACE(s);
        const QpProblem qp = weakly_active_indefinite_qp(s, 1.0);
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
    // T4b CLOSE GATE 9 -- MECHANISM 4'S INVARIANT, in the form the tier's
    // public result can carry.
    //
    // THE INVARIANT, STATED: under the separation, the right-hand side a step
    // is built from IS the section 3.2 subproblem's own residual, so a point
    // where the step vanishes is a point where THAT residual is zero -- and
    // the gate, which measures exactly that residual, advances there. The
    // schedule then decays and the solve moves on. A fixed point cannot be
    // frozen.
    //
    // WHAT IS AND IS NOT ASSERTED HERE, said plainly. The literal per-step
    // form -- "at any accepted step with `||(dx, ds, dy, dz)||inf < 1e-12` the
    // regularized KKT residual is within the stopping tolerance" -- is not
    // observable through `IpqpResult`, which publishes no step norms, and T4b
    // does not add a counter for one. What IS observable is the invariant's
    // whole-solve consequence, and it is the consequence that was violated
    // before T4b on every one of these fixtures: a solve that reaches a fixed
    // point either CONVERGES or is stopped by something other than the
    // iteration budget. Every fixture below burnt its full budget at a frozen
    // point at W1 head. The per-step form was checked by SOURCE MUTATION
    // (re-introducing the total shift in `build_rhs`), recorded in the T4b
    // report with the measurements; it is not runnable from here.
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
    }

    // THE SECOND HALF OF GATE 9: the item 4 read runs at `rho_dem = 0` and
    // `rho_sched = ipqp_reg_floor` BY CONSTRUCTION, never at the ladder's
    // memory. The saddle fixture is the witness: its ladder reaches a shift at
    // which the inertia reads RIGHT (`rho_demanded_max = 100`), and the read
    // still comes back WRONG -- which it could not do if it were seeded from
    // `rho_dem_last`.
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
    // T4b CLOSE GATE 10 -- THE ADVERSARIAL LONG WALK.
    //
    // THE GAP THIS FIXTURE EXISTS TO TEST (T4b plan 2.1). Separating the two
    // regularizations makes the inner iteration a descent method on the
    // schedule's barrier-proximal subproblem, but its RESIDUAL IS NOT
    // MONOTONE: along a direction where `H + rho_sched I + Sigma` still has
    // negative curvature the residual GROWS, by a factor
    // `rho_dem / (lambda_i + rho_sched + rho_dem) > 1`. The section 3.2 gate
    // measures residual CONTRACTION, so it is SILENT for the whole walk to the
    // bound face -- `zeta` pinned, `rho_sched` pinned -- and resumes only when
    // the curvature turns positive and `rho_dem` falls back to 0.
    //
    // THE TWO RISKS, and which one is real. Section 6.2's stall test needs all
    // three conjuncts, and conjunct (iii) demands `min(alpha_p, alpha_d) <
    // 1e-2` on EVERY step of the window -- which a full-step walk cannot
    // satisfy -- so section 6.2 was PREDICTED not to fire. The real risk is
    // the iteration BUDGET. This fixture is built to make both as hard as the
    // design allows (see the fixture's own banner) and then measures.
    //
    // RESULT: 26 armed iterations with no gate advance, and the solve
    // converges in 31 of its 60, at the minimizer, with a standing
    // certificate. Section 6.2 does NOT fire, so plan 2.1(a)'s phi-decrease
    // window reset is NOT built -- the condition on it was "only if section
    // 6.2 fires on this fixture".
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
    // UNOBSERVED on Apple/Accelerate (CLAUDE.md section 6: a value that has not
    // been observed on real Mac hardware is never guessed). The structural
    // assertions above and below hold on every backend; the exact trajectory
    // counts are MKL evidence and are pinned only there. The Mac session
    // measures and fills these in.
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

    // **THE WALK IS UNINTERRUPTED, AND THAT IS PROVED RATHER THAN INFERRED**
    // (fix round 1, CX8). Aggregate counts cannot establish contiguity: 26
    // silent armed iterations and 29 armed ones are equally consistent with
    // three advances scattered through the middle. What settles it is a
    // TRUNCATION SWEEP -- the same solve run with `ipqp_hard_iter_cap = k` for
    // each `k`, which follows the identical trajectory and simply stops
    // earlier, so the differences `prox(k) - prox(k-1)` and `elev(k) -
    // elev(k-1)` say exactly whether iteration `k` advanced and whether it was
    // armed. The longest contiguous run of armed-and-silent iterations is then
    // a measurement, not a bound.
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
    // MEASURED on MKL: iterations 4 through 27 inclusive are armed with no gate
    // advance -- one unbroken run of 24. The three armed iterations that DO
    // advance sit at the ends of the walk (3, 28 and 31), which is the shape
    // the gap predicts: the gate speaks when the curvature turns, and is silent
    // in between.
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
