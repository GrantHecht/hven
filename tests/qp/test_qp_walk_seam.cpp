// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_walk_seam.cpp — the primal active-set walk's READ-ONLY per-minor
// observer (hven/detail/qp/qp_walk_trace.h), on the test-seam convention
// docs/testing.md describes.
//
// WHAT THIS FILE HAS TO PROVE, and it is not "the observer records something":
//
//   (1) NEUTRALITY. With the observer attached, the engine's answer and its
//       counters are BIT-IDENTICAL to the unattached run. That is the whole
//       licence for using this instrument to take a measurement whose identity
//       gate is a committed counter row, so it is asserted per fixture, field
//       by field AND over the whole `QpCounters` object, and over the returned
//       x / multipliers / bound states / status as exact bit patterns.
//   (2) FAITHFULNESS of each recorded quantity against the loop's own public
//       counters: one record per minor, `degenerate` summing to
//       `degenerate_steps`, additions summing to `ws_adds`, drops to
//       `ws_drops`, bound additions to `ws_adds_bound`, `ratio_tied > 1`
//       summing to `ratio_ties`, `drop_tied > 1` to `drop_ties`, and the
//       working-set signature's transitions agreeing with the actions.
//   (3) The SIGNATURE is what it claims: order-independent, side-sensitive and
//       identity-sensitive.
//
// This target defines HVEN_TESTING target-wide and does NOT link hven::hven —
// see tests/CMakeLists.txt.

#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>

using namespace hven::solvers;
using hven::Index;
using hven::Vec;
using Obs = hven::solvers::detail::testing::QpWalkObserver;
using Action = hven::solvers::detail::testing::QpWalkAction;
using Block = hven::solvers::detail::testing::QpWalkBlock;
using Route = hven::solvers::detail::testing::QpWalkDropRoute;
using Record = hven::solvers::detail::testing::QpWalkMinorRecord;

namespace {

// --- The three fixtures -------------------------------------------------
//
// Transcribed from tests/sqp/test_qp_engine.cpp's battery (that file is not
// included: a test TU never includes another test TU) so that the degenerate
// one is the SAME degenerate problem the ordinary QP suite pins.

/// min 1/2(x0^2+x1^2) - x0 - 2x1 s.t. x0+x1 <= 1, 0 <= x <= 10. The optimum
/// (0,1) is a DEGENERATE vertex: the row is active with lambda = 1 and the
/// bound x0 >= 0 is active with a ZERO multiplier, which is exactly what §4b's
/// zero-multiplier probe is written for.
QpProblem simple_box_qp() {
    QpProblem qp;
    const Eigen::MatrixXd Hd = Eigen::MatrixXd::Identity(2, 2);
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -1, -2;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << 1, 1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, 1.0);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 10.0);
    return qp;
}

/// simple_box_qp with the inequality row DUPLICATED: at the optimum the two
/// identical rows are both active, so the working set is linearly DEPENDENT —
/// the case `qp_engine.h`'s "Degeneracy" note is about.
QpProblem degenerate_qp() {
    QpProblem qp = simple_box_qp();
    Eigen::MatrixXd Aid(2, 2);
    Aid << 1, 1, 1, 1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(2, 1.0);
    return qp;
}

/// A larger box-and-rows QP whose walk takes many minors: a deterministic
/// tridiagonal Hessian, a linear term that pulls the solution outside the box,
/// and two general rows. It exists so neutrality is not proved only on
/// two-variable problems.
QpProblem many_minor_qp() {
    constexpr Index kN = 24;
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(kN, kN);
    for (Index i = 0; i < kN; ++i) {
        Hd(i, i) = 2.0 + 0.01 * static_cast<double>(i);
        if (i + 1 < kN) {
            Hd(i, i + 1) = -0.5;
            Hd(i + 1, i) = -0.5;
        }
    }
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(kN);
    for (Index i = 0; i < kN; ++i) {
        qp.g(i) = -3.0 - 0.25 * static_cast<double>((i * 7) % 11);
    }
    qp.Ae.resize(0, kN);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid = Eigen::MatrixXd::Zero(2, kN);
    for (Index i = 0; i < kN; ++i) {
        Aid(0, i) = 1.0;
        Aid(1, i) = (i % 2 == 0) ? 1.0 : -1.0;
    }
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << 6.0, 2.0;
    qp.lower = Vec::Zero(kN);
    qp.upper = Vec::Constant(kN, 4.0);
    return qp;
}

/// A working set that is WRONG on purpose: every variable pinned at its lower
/// bound, no row active. On these fixtures the optimum has neither property,
/// so the walk has to DROP its way out — which is how the drop paths of this
/// seam are reached at all. Without it every assertion about a drop would pass
/// vacuously, because a cold walk on these problems only ever adds.
QpSolution all_at_lower_seed(const QpProblem &qp) {
    QpSolution seed;
    seed.x = qp.lower;
    seed.bound_state.assign(static_cast<std::size_t>(qp.n()), BoundState::kAtLower);
    seed.ineq_active.assign(static_cast<std::size_t>(qp.mi()), false);
    return seed;
}

struct Case {
    const char *name;
    QpProblem (*build)();
    bool seeded;
};

const Case kCases[] = {
    {"simple_box", &simple_box_qp, false},
    {"degenerate", &degenerate_qp, false},
    {"many_minor", &many_minor_qp, false},
    {"simple_box@wrong-seed", &simple_box_qp, true},
    {"degenerate@wrong-seed", &degenerate_qp, true},
    {"many_minor@wrong-seed", &many_minor_qp, true},
};

QpSolution run_case(const Case &c, const QpProblem &qp, QpEngine &engine) {
    if (!c.seeded) {
        return engine.solve(qp);
    }
    const QpSolution seed = all_at_lower_seed(qp);
    return engine.solve(qp, seed);
}

/// Bit equality, so a NaN compares equal to itself and a signed zero does not
/// compare equal to its negative. `EXPECT_DOUBLE_EQ` would tolerate a
/// difference this gate exists to refuse.
::testing::AssertionResult same_bits(const Vec &a, const Vec &b, const char *what) {
    if (a.size() != b.size()) {
        return ::testing::AssertionFailure()
               << what << ": sizes " << a.size() << " vs " << b.size();
    }
    for (Index i = 0; i < a.size(); ++i) {
        const double x = a(i);
        const double y = b(i);
        if (std::memcmp(&x, &y, sizeof(double)) != 0) {
            return ::testing::AssertionFailure()
                   << what << ": element " << i << " differs (" << x << " vs " << y << ")";
        }
    }
    return ::testing::AssertionSuccess();
}

void expect_same_counters(const QpCounters &a, const QpCounters &b) {
    // Named first, so a failure says WHICH quantity moved.
    EXPECT_EQ(a.factorizations, b.factorizations);
    EXPECT_EQ(a.schur_updates, b.schur_updates);
    EXPECT_EQ(a.minor_iters, b.minor_iters);
    EXPECT_EQ(a.eqp_refine_steps, b.eqp_refine_steps);
    EXPECT_EQ(a.border_refine_steps, b.border_refine_steps);
    EXPECT_EQ(a.verdict_refine_steps, b.verdict_refine_steps);
    EXPECT_EQ(a.suspect_escalations, b.suspect_escalations);
    EXPECT_EQ(a.k0_reused, b.k0_reused);
    EXPECT_EQ(a.symbolic_analyses, b.symbolic_analyses);
    EXPECT_EQ(a.ws_adds, b.ws_adds);
    EXPECT_EQ(a.ws_drops, b.ws_drops);
    EXPECT_EQ(a.shift_adds, b.shift_adds);
    EXPECT_EQ(a.degenerate_steps, b.degenerate_steps);
    EXPECT_EQ(a.degenerate_run_max, b.degenerate_run_max);
    EXPECT_EQ(a.ws_adds_bound, b.ws_adds_bound);
    EXPECT_EQ(a.ws_drops_bound, b.ws_drops_bound);
    EXPECT_EQ(a.distinct_ineq_added, b.distinct_ineq_added);
    EXPECT_EQ(a.distinct_bound_added, b.distinct_bound_added);
    EXPECT_EQ(a.drop_ties, b.drop_ties);
    EXPECT_EQ(a.ratio_ties, b.ratio_ties);
    // Then the WHOLE object, so a field this list forgets cannot slip through.
    // QpCounters is trivially copyable and both objects are copies of
    // value-initialized locals, so their padding is equal too.
    static_assert(std::is_trivially_copyable_v<QpCounters>);
    EXPECT_EQ(0, std::memcmp(&a, &b, sizeof(QpCounters)));
}

/// A scope guard, so a fixture that fails an assertion cannot leave the
/// observer armed for the next one.
struct ObserverGuard {
    ObserverGuard() { Obs::reset(); }
    ~ObserverGuard() { Obs::reset(); }
};

} // namespace

// --- (1) NEUTRALITY -----------------------------------------------------

TEST(QpWalkSeam, AttachingTheObserverChangesNoCounterAndNoAnswer) {
    for (const Case &c : kCases) {
        SCOPED_TRACE(c.name);
        const QpProblem qp = c.build();

        Obs::reset();
        QpEngine unobserved{QpOptions{}};
        const QpSolution without = run_case(c, qp, unobserved);
        ASSERT_FALSE(Obs::active);
        EXPECT_TRUE(Obs::records.empty()) << "an inactive observer recorded something";

        ObserverGuard guard;
        Obs::active = true;
        QpEngine observed{QpOptions{}};
        const QpSolution with = run_case(c, qp, observed);

        EXPECT_EQ(static_cast<int>(without.status), static_cast<int>(with.status));
        expect_same_counters(without.counters, with.counters);
        EXPECT_TRUE(same_bits(without.x, with.x, "x"));
        EXPECT_TRUE(same_bits(without.lambda_e, with.lambda_e, "lambda_e"));
        EXPECT_TRUE(same_bits(without.lambda_i, with.lambda_i, "lambda_i"));
        EXPECT_TRUE(same_bits(without.z, with.z, "z"));
        EXPECT_EQ(without.bound_state, with.bound_state);
        EXPECT_EQ(without.ineq_active, with.ineq_active);
        EXPECT_EQ(without.tr_active, with.tr_active);
        EXPECT_FALSE(Obs::records.empty()) << "an ACTIVE observer recorded nothing";
    }
}

TEST(QpWalkSeam, TheObserverIsNeutralOnAWarmRESOLVETOO) {
    // The neutrality that matters for a driver measurement is neutrality on
    // the SECOND solve of the same engine as well as the first: a warm
    // re-solve reuses border state, and an observer that perturbed that state
    // would show up here and nowhere else.
    const QpProblem qp = many_minor_qp();

    Obs::reset();
    QpEngine a{QpOptions{}};
    const QpSolution first_without = a.solve(qp);
    const QpSolution second_without = a.solve(qp, first_without);

    ObserverGuard guard;
    Obs::active = true;
    QpEngine b{QpOptions{}};
    const QpSolution first_with = b.solve(qp);
    const QpSolution second_with = b.solve(qp, first_with);

    expect_same_counters(first_without.counters, first_with.counters);
    expect_same_counters(second_without.counters, second_with.counters);
    EXPECT_TRUE(same_bits(second_without.x, second_with.x, "x"));
    EXPECT_EQ(2, Obs::solves);
}

TEST(QpWalkSeam, CaptureSolveSelectsOneSolveAndTheRestAreNotRecorded) {
    const QpProblem qp = many_minor_qp();
    ObserverGuard guard;
    Obs::active = true;
    Obs::capture_solve = 1; // the SECOND solve only

    QpEngine engine{QpOptions{}};
    const QpSolution first = engine.solve(qp);
    engine.solve(qp, first);

    EXPECT_EQ(2, Obs::solves);
    ASSERT_FALSE(Obs::records.empty());
    for (const Record &r : Obs::records) {
        EXPECT_EQ(1, r.solve);
    }
    EXPECT_EQ(1, Obs::solve_index);
}

// --- (2) FAITHFULNESS against the loop's own counters --------------------

TEST(QpWalkSeam, TheTraceAgreesWithTheCountersItMustNotHaveTouched) {
    for (const Case &cs : kCases) {
        SCOPED_TRACE(cs.name);
        const QpProblem qp = cs.build();
        ObserverGuard guard;
        Obs::active = true;
        QpEngine engine{QpOptions{}};
        const QpSolution sol = run_case(cs, qp, engine);
        const QpCounters &c = sol.counters;

        ASSERT_FALSE(Obs::truncated);
        // ONE RECORD PER MINOR, in order, numbered from zero.
        ASSERT_EQ(static_cast<std::size_t>(c.minor_iters), Obs::records.size());
        for (std::size_t i = 0; i < Obs::records.size(); ++i) {
            EXPECT_EQ(static_cast<Index>(i), Obs::records[i].minor);
            EXPECT_NE(Action::kUnset, Obs::records[i].action)
                << "minor " << i << " took a branch the seam does not enumerate";
        }

        Index degenerate = 0;
        Index adds = 0;
        Index adds_bound = 0;
        Index drops = 0;
        Index drops_bound = 0;
        Index ratio_ties = 0;
        Index drop_ties = 0;
        for (const Record &r : Obs::records) {
            if (r.degenerate) {
                ++degenerate;
            }
            if (r.action == Action::kAdd || r.action == Action::kRideStepped) {
                ++adds;
                if (r.block == Block::kBound) {
                    ++adds_bound;
                }
            }
            if (r.drop_route != Route::kNone) {
                ++drops;
                if (r.block == Block::kBound) {
                    ++drops_bound;
                }
            }
            if (r.ratio_tied > 1) {
                ++ratio_ties;
            }
            if (r.drop_tied > 1) {
                ++drop_ties;
            }
        }
        EXPECT_EQ(c.degenerate_steps, degenerate);
        EXPECT_EQ(c.ws_adds, adds);
        EXPECT_EQ(c.ws_adds_bound, adds_bound);
        EXPECT_EQ(c.ws_drops, drops);
        EXPECT_EQ(c.ws_drops_bound, drops_bound);
        EXPECT_EQ(c.ratio_ties, ratio_ties);
        EXPECT_EQ(c.drop_ties, drop_ties);
        EXPECT_EQ(c.suspect_escalations, [] {
            Index n = 0;
            for (const Record &r : Obs::records) {
                if (r.action == Action::kSuspectEscalation) {
                    ++n;
                }
            }
            return n;
        }());
    }
}

TEST(QpWalkSeam, TheWorkingSetSignatureTracksTheActionsThatChangeIt) {
    for (const Case &c : kCases) {
        SCOPED_TRACE(c.name);
        const QpProblem qp = c.build();
        ObserverGuard guard;
        Obs::active = true;
        QpEngine engine{QpOptions{}};
        run_case(c, qp, engine);
        ASSERT_FALSE(Obs::records.empty());

        // The first minor starts from the INCOMING working set.
        EXPECT_EQ(Obs::incoming_ws_signature, Obs::records.front().ws_sig_in);
        EXPECT_EQ(Obs::incoming_ws_size, Obs::records.front().ws_size_in);

        for (std::size_t i = 0; i + 1 < Obs::records.size(); ++i) {
            // The set one minor ends with is the set the next one begins with:
            // nothing between two passes of the loop may move it.
            EXPECT_EQ(Obs::records[i].ws_sig_out, Obs::records[i + 1].ws_sig_in)
                << "between minors " << i << " and " << i + 1;
            EXPECT_EQ(Obs::records[i].ws_size_out, Obs::records[i + 1].ws_size_in);
        }
        // THE SIZE ARITHMETIC OF A PASS, exactly. The working set grows in two
        // unrelated ways — the action, and `refresh_shifts` admitting rows the
        // homotopy is driving to feasibility — so the identity is stated with
        // the homotopy's own term in it rather than as an inequality.
        for (const Record &r : Obs::records) {
            const Index base = r.ws_size_in + r.shift_adds;
            SCOPED_TRACE(::testing::Message()
                         << "minor " << r.minor << " action " << static_cast<int>(r.action));
            EXPECT_GE(r.shift_adds, 0);
            switch (r.action) {
            case Action::kAdd:
            case Action::kRideStepped:
                EXPECT_EQ(base + 1, r.ws_size_out) << "an addition grows the set by one";
                EXPECT_NE(r.ws_sig_in, r.ws_sig_out);
                EXPECT_NE(Block::kNone, r.block);
                EXPECT_GE(r.block_index, 0);
                break;
            case Action::kDropDantzig:
            case Action::kDropZeroProbe:
                EXPECT_EQ(base - 1, r.ws_size_out) << "a drop shrinks the set by one";
                EXPECT_NE(Route::kNone, r.drop_route);
                break;
            case Action::kMoveOnly:
            case Action::kSuspectEscalation:
            case Action::kTerminal:
            case Action::kRideUnbounded:
                EXPECT_EQ(base, r.ws_size_out);
                break;
            case Action::kStartRepair:
            case Action::kPostProbeRestart:
                // The repair pins as many variables as it needs to reach a
                // second-order consistent vertex, so no size identity holds
                // here and none is asserted. What is asserted of it is that it
                // is recorded as the repair rather than as a step.
                EXPECT_EQ(Block::kNone, r.block);
                break;
            case Action::kUnset:
                ADD_FAILURE() << "a minor recorded no action";
                break;
            }
        }
    }
}

TEST(QpWalkSeam, ADropRecordsItsRouteItsMultiplierAndTheToleranceItWasComparedAgainst) {
    // A drop has to be PROVOKED: on these fixtures the cold walk only ever
    // adds. Seeding a working set that pins BOTH variables at their lower
    // bounds, when the optimum of simple_box_qp is (0, 1), makes the drop rule
    // release x1 — the constraint whose multiplier has the wrong sign.
    const QpProblem qp = simple_box_qp();
    QpSolution seed;
    seed.x = Vec::Zero(2);
    seed.bound_state = {BoundState::kAtLower, BoundState::kAtLower};
    seed.ineq_active = {false};

    ObserverGuard guard;
    Obs::active = true;
    QpEngine engine{QpOptions{}};
    const QpSolution sol = engine.solve(qp, seed);
    ASSERT_GE(sol.counters.ws_drops, 1) << "the seeded walk dropped nothing";

    bool saw_drop = false;
    for (const Record &r : Obs::records) {
        if (r.drop_route == Route::kNone) {
            continue;
        }
        saw_drop = true;
        EXPECT_NE(Block::kNone, r.block);
        EXPECT_GE(r.block_index, 0);
        EXPECT_FALSE(std::isnan(r.decision_multiplier));
        EXPECT_FALSE(std::isnan(r.decision_tol));
        EXPECT_GT(r.decision_tol, 0.0);
        if (r.drop_route == Route::kDantzig) {
            // The Dantzig rule drops on a violation that CLEARED the tolerance;
            // the probe drops on one that did not.
            EXPECT_GT(r.decision_multiplier, r.decision_tol);
            EXPECT_GE(r.best_multiplier, r.decision_multiplier);
            EXPECT_GE(r.drop_candidates, 1);
        } else {
            EXPECT_LE(r.decision_multiplier, r.decision_tol);
        }
    }
    EXPECT_TRUE(saw_drop) << "the degenerate fixture dropped nothing; the assertions above "
                             "never ran and this test would pass vacuously";
}

TEST(QpWalkSeam, ADegenerateStepIsAnAdditionThatMovedNoFurtherThanTheStepTolerance) {
    for (const Case &c : kCases) {
        SCOPED_TRACE(c.name);
        const QpProblem qp = c.build();
        ObserverGuard guard;
        Obs::active = true;
        QpEngine engine{QpOptions{}};
        run_case(c, qp, engine);
        for (const Record &r : Obs::records) {
            if (!r.degenerate) {
                continue;
            }
            ASSERT_NE(Block::kNone, r.block);
            ASSERT_FALSE(std::isnan(r.dx_inf));
            ASSERT_FALSE(std::isnan(r.step_tol));
            EXPECT_LE(r.dx_inf, r.step_tol);
        }
    }
}

// --- (3) THE SIGNATURE ---------------------------------------------------

TEST(QpWalkSeam, TheSignatureIsOrderIndependentSideSensitiveAndIdentitySensitive) {
    using hven::solvers::detail::testing::qp_walk_ws_signature;
    using hven::solvers::detail::testing::qp_walk_ws_size;

    WorkingSet a(4, 3);
    a.add_ineq(2);
    a.add_ineq(0);
    a.bound_state()[1] = BoundState::kAtLower;
    a.bound_state()[3] = BoundState::kAtUpper;

    // The same MEMBERS, built in a different order.
    WorkingSet b(4, 3);
    b.bound_state()[3] = BoundState::kAtUpper;
    b.add_ineq(0);
    b.bound_state()[1] = BoundState::kAtLower;
    b.add_ineq(2);
    EXPECT_EQ(qp_walk_ws_signature(a), qp_walk_ws_signature(b));
    EXPECT_EQ(4, qp_walk_ws_size(a));
    EXPECT_EQ(qp_walk_ws_size(a), qp_walk_ws_size(b));

    // The SIDE matters: the same variable at the other bound is a different
    // working set, which is precisely what a lower/upper ping-pong looks like.
    WorkingSet c = b;
    c.bound_state()[1] = BoundState::kAtUpper;
    EXPECT_NE(qp_walk_ws_signature(a), qp_walk_ws_signature(c));
    EXPECT_EQ(qp_walk_ws_size(a), qp_walk_ws_size(c));

    // The IDENTITY matters, and a row never collides with a variable of the
    // same index.
    WorkingSet d(4, 3);
    d.add_ineq(1);
    WorkingSet e(4, 3);
    e.bound_state()[1] = BoundState::kAtLower;
    EXPECT_NE(qp_walk_ws_signature(d), qp_walk_ws_signature(e));

    // An empty set is zero, so "nothing pinned" is not a hash of nothing.
    const WorkingSet empty(4, 3);
    EXPECT_EQ(0u, qp_walk_ws_signature(empty));
    EXPECT_EQ(0, qp_walk_ws_size(empty));
}

TEST(QpWalkSeam, ResetClearsEverythingTheObserverHolds) {
    ObserverGuard guard;
    Obs::active = true;
    Obs::capture_solve = 3;
    Obs::max_records = 7;
    QpEngine engine{QpOptions{}};
    engine.solve(simple_box_qp());
    Obs::reset();
    EXPECT_FALSE(Obs::active);
    EXPECT_EQ(0, Obs::solves);
    EXPECT_EQ(-1, Obs::capture_solve);
    EXPECT_FALSE(Obs::truncated);
    EXPECT_TRUE(Obs::records.empty());
    EXPECT_FALSE(Obs::capturing);
    EXPECT_EQ(-1, Obs::exit_status);
    EXPECT_EQ(nullptr, Obs::current);
}

TEST(QpWalkSeam, TheRecordCapIsHonouredAndReported) {
    ObserverGuard guard;
    Obs::active = true;
    Obs::max_records = 1;
    QpEngine engine{QpOptions{}};
    engine.solve(many_minor_qp());
    EXPECT_EQ(1u, Obs::records.size());
    EXPECT_TRUE(Obs::truncated);
}

// --- (4) THE NEUTRALITY CHECK'S OWN NON-VACUITY --------------------------

TEST(QpWalkSeam, TheNeutralityComparisonWouldCatchAMoveIfThereWereOne) {
    // A gate that cannot fail is not a gate. `expect_same_counters` and
    // `same_bits` are what the neutrality test rests on, and both are asserted
    // here to REJECT a difference -- the field-by-field arm, the whole-object
    // `memcmp` arm (which is the one that would carry a field the named list
    // forgets), and the bit comparison on the returned vectors.
    ObserverGuard guard;
    Obs::active = true;
    QpEngine engine{QpOptions{}};
    const QpSolution sol = engine.solve(many_minor_qp());
    ASSERT_GT(sol.counters.minor_iters, 0);

    QpCounters bumped = sol.counters;
    ++bumped.minor_iters;
    EXPECT_NE(0, std::memcmp(&sol.counters, &bumped, sizeof(QpCounters)))
        << "the whole-object arm cannot see a one-count difference";
    EXPECT_NE(sol.counters.minor_iters, bumped.minor_iters);

    // A field the NAMED list does carry, moved by the smallest possible
    // amount, and a field it carries as a bool.
    QpCounters flipped = sol.counters;
    flipped.k0_reused = !flipped.k0_reused;
    EXPECT_NE(0, std::memcmp(&sol.counters, &flipped, sizeof(QpCounters)));

    // And the float comparison: one ULP, and a sign flip on a zero.
    Vec a = sol.x;
    ASSERT_GT(a.size(), 0);
    Vec b = a;
    b(0) = std::nextafter(b(0), std::numeric_limits<double>::infinity());
    EXPECT_FALSE(same_bits(a, b, "one ulp"));
    Vec zero_pos = Vec::Zero(1);
    Vec zero_neg = Vec::Constant(1, -0.0);
    EXPECT_FALSE(same_bits(zero_pos, zero_neg, "signed zero"));
    EXPECT_TRUE(same_bits(a, a, "self"));
}
