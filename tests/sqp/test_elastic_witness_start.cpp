// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_elastic_witness_start.cpp -- M7 W2 P1E: THE WITNESS START (elastic.h,
// `place_elastic_seed_at_witness`; `SqpOptions::elastic_witness_start`).
//
// The dumps are the C11 census's major-0 pair of two TRAIN cells (scratch diagnostic build of
// m7 21369067): qp0 = the primary subproblem, kInfeasible inside its radius-1 window; qp1 = the
// first elastic rung built from it (rho 1e2), whose walk started at (0, 0) -- off every relaxed
// equality row -- and ended kNumericalError at the dependent-blocker terminal, still off them
// (YORKNET 1333, SAWPATH 2.56). The witness (p_ref, |r| / sigma) is feasible for that QP by
// construction; started there, the walk has no phase-1 work to do.
//
// Premises asserted: qp1 IS build_elastic_subproblem(qp0, 1, 1e2, feas_tol), byte for byte, and
// the dumped seed still fails off its rows at this head. Minor and factorization counts are
// printed, never asserted.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include <gtest/gtest.h>

#include <hven/drivers/sqp_solver.h>

#include "../common_support/route_reference.h"
#include "../qp/support/qp_dump.h"

using namespace hven::solvers;
using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using hven::test_support::QpDump;

namespace {

QpDump dump(const std::string &name) {
    return hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) + "/" + name);
}

bool same_matrix(const SpMatRM &a, const SpMatRM &b) {
    if (a.rows() != b.rows() || a.cols() != b.cols()) {
        return false;
    }
    return a.rows() == 0 || a.cols() == 0 ||
           (Eigen::MatrixXd(a) - Eigen::MatrixXd(b)).cwiseAbs().maxCoeff() == 0.0;
}

bool same_vec(const Vec &a, const Vec &b) {
    return a.size() == b.size() && (a.size() == 0 || (a - b).cwiseAbs().maxCoeff() == 0.0);
}

/// The largest violation of `x` against every row and bound of `qp` (no window).
double worst_violation(const QpProblem &qp, const Vec &x) {
    double w = 0.0;
    if (qp.me() > 0) {
        w = std::max(w, (qp.Ae * x - qp.be).lpNorm<Eigen::Infinity>());
    }
    if (qp.mi() > 0) {
        w = std::max(w, std::max(0.0, (qp.Ai * x - qp.bi).maxCoeff()));
    }
    for (Index i = 0; i < qp.n(); ++i) {
        w = std::max({w, qp.lower(i) - x(i), x(i) - qp.upper(i)});
    }
    return w;
}

struct Pair {
    QpDump d0;
    QpDump d1;
    ElasticQp e;
};

Pair load_pair(const char *cell) {
    Pair p{dump(std::string("C11_") + cell + "_qp0.txt"),
           dump(std::string("C11_") + cell + "_qp1.txt"), ElasticQp{}};
    p.e = build_elastic_subproblem(p.d0.qp, p.d0.overrides.tr_radius, kElasticRhoInit,
                                   SqpOptions{}.feas_tol);
    return p;
}

void expect_premise(const Pair &p) {
    // The primary failed kInfeasible in its radius-1 window; the rung failed numerically.
    ASSERT_NE(p.d0.result.find("status=2"), std::string::npos) << p.d0.result;
    ASSERT_EQ(p.d0.overrides.tr_radius, 1.0);
    ASSERT_NE(p.d1.result.find("status=3"), std::string::npos) << p.d1.result;
    ASSERT_TRUE(p.d1.has_seed);
    ASSERT_EQ(p.d1.seed.x.lpNorm<Eigen::Infinity>(), 0.0) << "the shipped seed starts at (0, 0)";
    // The dumped rung IS the construction (rho 1e2, the default feas_tol), byte for byte.
    ASSERT_EQ(p.e.qp.n(), p.d1.qp.n());
    EXPECT_TRUE(same_matrix(p.e.qp.H, p.d1.qp.H));
    EXPECT_TRUE(same_matrix(p.e.qp.Ae, p.d1.qp.Ae));
    EXPECT_TRUE(same_matrix(p.e.qp.Ai, p.d1.qp.Ai));
    EXPECT_TRUE(same_vec(p.e.qp.g, p.d1.qp.g));
    EXPECT_TRUE(same_vec(p.e.qp.be, p.d1.qp.be));
    EXPECT_TRUE(same_vec(p.e.qp.bi, p.d1.qp.bi));
    EXPECT_TRUE(same_vec(p.e.qp.lower, p.d1.qp.lower));
    EXPECT_TRUE(same_vec(p.e.qp.upper, p.d1.qp.upper));
}

/// The rung on its dumped seed (before) and on the same seed placed at the witness (after).
/// `f_star` / `rel`: an independent convex-QP oracle's optimum (Clarabel, scratch venv, never
/// vendored) and the relative band the walk's KKT point must land in. Both rungs are convex (H
/// PSD), so a KKT point is a global minimizer.
///
/// `route_pinned`: the cell's replay statuses and counters are a ROUTE (SAWPATH, an exactly
/// rank-deficient equality block decided by Pardiso's pivot perturbation: WX-R2), pinned on the
/// reference build only (the reference arm below is the original text, byte for byte). Off that
/// build a route-pinned cell records the route and asserts the portable contract: an honest
/// status (the rung is feasible by construction, never kInfeasible), and, when the answer is
/// kOptimal, the full KKT battery and the convex-QP oracle objective. A cell that is not
/// route-pinned keeps its asserts on every build.
void expect_rung_before_after(const char *cell, double f_star, double rel,
                              bool seed_optimal_in_debug, bool route_pinned = false) {
    (void)route_pinned;
    const Pair p = load_pair(cell);
    expect_premise(p);
    if (::testing::Test::HasFatalFailure()) {
        return;
    }
    const QpEngine engine(p.d1.opts, p.d1.threads);

    const QpSolution before = engine.solve(p.d1.qp, p.d1.seed, p.d1.overrides);
    const double before_off = worst_violation(p.d1.qp, before.x);

    QpSolution seed = p.d1.seed;
    place_elastic_seed_at_witness(p.e, p.d0.qp, seed);
    const double start_off = worst_violation(p.d1.qp, seed.x);
    Index pins_seeded = 0;
    Index pins_kept = 0;
    for (std::size_t i = 0; i < seed.bound_state.size(); ++i) {
        const bool was = p.d1.seed.bound_state[i] == BoundState::kAtLower ||
                         p.d1.seed.bound_state[i] == BoundState::kAtUpper;
        pins_seeded += was ? 1 : 0;
        pins_kept += was && seed.bound_state[i] == p.d1.seed.bound_state[i] ? 1 : 0;
    }
    const QpSolution after = engine.solve(p.d1.qp, seed, p.d1.overrides);
    QpDump d_after = p.d1;
    d_after.seed = seed;
    const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d_after, after);
    const double slack_l1 = p.e.slack_violations(after.x).lpNorm<1>();
    const double f = hven::test_support::qp_dump_objective(p.d1, after.x);
    std::printf("[ p1e ] %s rung1 before: status=%d minors=%ld facts=%ld off_rows=%.6g | witness: "
                "start_off=%.3e pins seeded=%ld kept=%ld | after: status=%d minors=%ld facts=%ld "
                "f=%.12g slack_l1=%.6g (witness %.6g) eq=%.2e box=%.2e stat=%.2e sign=%.2e "
                "comp=%.2e\n",
                cell, static_cast<int>(before.status),
                static_cast<long>(before.counters.minor_iters),
                static_cast<long>(before.counters.factorizations), before_off, start_off,
                static_cast<long>(pins_seeded), static_cast<long>(pins_kept),
                static_cast<int>(after.status), static_cast<long>(after.counters.minor_iters),
                static_cast<long>(after.counters.factorizations), f, slack_l1, p.e.violation_l1,
                k.eq, k.box, k.stationarity, k.sign, k.complementarity);

    // BEFORE: the dumped seed now solves. DECLARED CORRECTNESS MOVE (commit 77dbf843, the
    // dead-end rank repair, ruling C58): this premise used to assert the defect -- the shipped
    // seed ended status != kOptimal, off its own rows by > 1e-3. The repair removed it: the seed
    // is repaired (no dead-end exit, no false certificate) and ends on its rows to ~1e-13.
    // What the walk reaches after the repair is configuration-dependent for SAWPATH (see
    // `seed_optimal_in_debug`); the witness arm below is unchanged.
#if HVEN_TEST_ROUTE_REFERENCE
    EXPECT_LE(before_off, 1e-9 * std::max(1.0, p.e.violation_l1));
    EXPECT_GE(before.counters.rank_repairs, 1);
    EXPECT_EQ(before.counters.dep_blocker_terminations, 0);
    EXPECT_EQ(before.counters.dep_certificate_rejections, 0);
    EXPECT_NE(before.status, QpStatus::kInfeasible);
    EXPECT_NE(before.status, QpStatus::kNumericalError);
#ifdef NDEBUG
    EXPECT_EQ(before.status, QpStatus::kOptimal);
#else
    if (seed_optimal_in_debug) {
        EXPECT_EQ(before.status, QpStatus::kOptimal);
    }
#endif
#else
    if (route_pinned) {
        hven::test_support::record_off_reference_route(
            std::string(cell) + "_rung_before",
            "status=" + std::to_string(static_cast<int>(before.status)) +
                " minors=" + std::to_string(before.counters.minor_iters) + " rank_repairs=" +
                std::to_string(before.counters.rank_repairs) + " dep_blocker_terminations=" +
                std::to_string(before.counters.dep_blocker_terminations) +
                " dep_certificate_rejections=" +
                std::to_string(before.counters.dep_certificate_rejections));
        EXPECT_NE(before.status, QpStatus::kInfeasible) << "the rung is feasible by construction";
    } else {
        // The repair / blocker / rejection counters are the route (a rank repair taken, no
        // blocker termination, no certificate rejection): reference-only, recorded here. The
        // portable contract is the honest status and, when kOptimal, the rows.
        hven::test_support::record_off_reference_route(
            std::string(cell) + "_rung_before",
            "status=" + std::to_string(static_cast<int>(before.status)) +
                " minors=" + std::to_string(before.counters.minor_iters) + " rank_repairs=" +
                std::to_string(before.counters.rank_repairs) + " dep_blocker_terminations=" +
                std::to_string(before.counters.dep_blocker_terminations) +
                " dep_certificate_rejections=" +
                std::to_string(before.counters.dep_certificate_rejections));
        if (before.status == QpStatus::kOptimal) {
            EXPECT_LE(before_off, 1e-9 * std::max(1.0, p.e.violation_l1));
        }
        EXPECT_NE(before.status, QpStatus::kInfeasible);
        EXPECT_NE(before.status, QpStatus::kNumericalError);
#ifdef NDEBUG
        EXPECT_EQ(before.status, QpStatus::kOptimal);
#else
        if (seed_optimal_in_debug) {
            EXPECT_EQ(before.status, QpStatus::kOptimal);
        }
#endif
    }
#endif
    // THE WITNESS: feasible for every row and bound, to rounding.
    const double scale = std::max(1.0, p.e.violation_l1);
    EXPECT_LE(start_off, 1e-12 * scale);
    // AFTER: a KKT point of the rung, with the relaxation reduced.
#if HVEN_TEST_ROUTE_REFERENCE
    ASSERT_EQ(after.status, QpStatus::kOptimal);
#else
    if (route_pinned) {
        hven::test_support::record_off_reference_route(
            std::string(cell) + "_rung_after",
            "status=" + std::to_string(static_cast<int>(after.status)) +
                " minors=" + std::to_string(after.counters.minor_iters));
        EXPECT_NE(after.status, QpStatus::kInfeasible) << "the rung is feasible by construction";
        if (after.status != QpStatus::kOptimal) {
            return; // an honest non-optimal end: no KKT claim is made
        }
    } else {
        ASSERT_EQ(after.status, QpStatus::kOptimal);
    }
#endif
    EXPECT_LE(k.eq, 1e-8 * scale);
    EXPECT_LE(k.ineq, 1e-8 * scale);
    EXPECT_LE(k.box, 1e-12 * scale);
    EXPECT_LE(k.stationarity, 1e-6);
    EXPECT_LE(k.sign, 1e-8);
    EXPECT_LE(k.complementarity, 1e-8);
#if HVEN_TEST_ROUTE_REFERENCE
    EXPECT_LT(slack_l1, p.e.violation_l1);
#else
    if (!route_pinned) {
        EXPECT_LT(slack_l1, p.e.violation_l1);
    }
#endif
    EXPECT_LE(std::abs(f - f_star), rel * std::abs(f_star)) << f << " vs oracle " << f_star;
}

/// The ladder end to end on qp0's own failed solve: shipped, witness start, witness start plus
/// the last optimal rung. Returns the three reports' fallback counts through `out`.
struct LadderArms {
    ElasticLadderReport rep[3];
    SqpCounters out[3];
};

LadderArms run_ladder_arms(const char *cell) {
    LadderArms a;
    const Pair p = load_pair(cell);
    EXPECT_NE(p.d0.result.find("status=2"), std::string::npos) << p.d0.result;
    SqpOptions opts;
    opts.qp = p.d0.opts;
    QpEngine engine0(p.d0.opts, p.d0.threads);
    const QpSolution failed = engine0.solve(p.d0.qp, p.d0.overrides);
    EXPECT_EQ(failed.status, QpStatus::kInfeasible) << "qp0's replay at this head";
    for (int arm = 0; arm < 3; ++arm) {
        opts.elastic_witness_start = arm >= 1;
        opts.elastic_last_optimal_rung = arm == 2;
        QpEngine engine(p.d0.opts, p.d0.threads);
        a.rep[arm] = run_elastic_ladder(engine, p.d0.qp, ElasticSeedSource{&failed, nullptr},
                                        p.d0.overrides.tr_radius, opts, a.out[arm]);
        std::printf("[ p1e ] %s ladder arm=%d (witness %d, last %d): usable=%d closed=%d "
                    "reduced=%d status=%d slack_l1=%.6g (witness %.6g) rungs=%ld minors=%ld "
                    "facts=%ld fallbacks=%ld\n",
                    cell, arm, static_cast<int>(opts.elastic_witness_start),
                    static_cast<int>(opts.elastic_last_optimal_rung),
                    static_cast<int>(a.rep[arm].usable), static_cast<int>(a.rep[arm].closed),
                    static_cast<int>(a.rep[arm].reduced), static_cast<int>(a.rep[arm].qp_status),
                    a.rep[arm].slack_l1, a.rep[arm].elastic.violation_l1,
                    static_cast<long>(a.out[arm].elastic_escalations + 1),
                    static_cast<long>(a.out[arm].qp_minor_iters),
                    static_cast<long>(a.out[arm].factorizations),
                    static_cast<long>(a.out[arm].elastic_last_rung_fallbacks));
    }
    return a;
}

} // namespace

TEST(SqpElasticWitnessStart, YorknetMajor0RungSolvesFromItsWitness) {
    // Oracle: Clarabel Solved, f* = 1696529.72937, rows to 9e-13.
    // Re-premised (declared correctness move, 77dbf843 dead-end rank repair, ruling C58): the
    // shipped seed used to end status != kOptimal, off its rows by > 1e-3. Now kOptimal with
    // off_rows 4.68e-13 (minors 614 / facts 668 Release, 605 / 659 Debug): identical in both
    // configs, so the seed is asserted kOptimal in Debug too.
    expect_rung_before_after("YORKNET", 1696529.72937, 1e-9, true);
}

TEST(SqpElasticWitnessStart, SawpathMajor0RungSolvesFromItsWitness) {
    // Oracle (the C11 census): Clarabel AlmostSolved, f* = -376.225617384 at a point 8e-4 off
    // its rows -- a looser reading, so the band is 1e-3.
    // Re-premised (declared correctness move, 77dbf843 dead-end rank repair, ruling C58): the
    // shipped seed used to end status != kOptimal, off its rows by > 1e-3. Now, Release: kOptimal,
    // off_rows 2.41e-13 (43 minors). Debug: the repair is taken but rounding leads the walk into
    // the known downstream two-pass cycle and the minor cap ends it (kMaxIter, 6830 minors,
    // off_rows 7.6e-13) -- so Debug asserts only "repaired, no dead-end exit, no false
    // certificate", as tests/qp/test_qp_rank_repair.cpp does for this fixture's sister.
    expect_rung_before_after("SAWPATH", -376.225617384, 1e-3, false, true);
}

// YORKNET: every rung of the ladder solves from the shipped start and from the witness; the
// last-rung option never fires. Re-premised (declared correctness move, 77dbf843 dead-end rank
// repair, ruling C58): the shipped (0, 0) start's ladder used to be unusable (usable == false);
// it is now usable, status kOptimal, reduced, 7 rungs, slack_l1 17032 -- the same as the witness
// arms, in Release (678 minors) and Debug (685 minors) alike.
TEST(SqpElasticWitnessStart, YorknetMajor0LadderIsUsableFromTheShippedStartAndTheWitness) {
    const LadderArms a = run_ladder_arms("YORKNET");
    EXPECT_TRUE(a.rep[0].usable) << "the shipped (0, 0) start";
    EXPECT_EQ(a.rep[0].qp_status, QpStatus::kOptimal) << "the shipped (0, 0) start";
    EXPECT_TRUE(a.rep[0].reduced) << "the shipped (0, 0) start";
    EXPECT_EQ(a.out[0].elastic_last_rung_fallbacks, 0) << "the shipped (0, 0) start";
    for (int arm = 1; arm < 3; ++arm) {
        EXPECT_TRUE(a.rep[arm].usable) << arm;
        EXPECT_EQ(a.rep[arm].qp_status, QpStatus::kOptimal) << arm;
        EXPECT_TRUE(a.rep[arm].reduced) << arm;
        EXPECT_EQ(a.out[arm].elastic_last_rung_fallbacks, 0) << arm;
    }
}

// SAWPATH: rung 1 solves from the witness, rung 2 fails from rung 1's own optimum (the
// rank-deficient equality block, C11-5); the last optimal rung hands back rung 1.
TEST(SqpElasticWitnessStart, SawpathMajor0LadderNeedsTheLastOptimalRung) {
    const LadderArms a = run_ladder_arms("SAWPATH");
    // The failing rung 2, read for the C11-5 diagnosis (printed, never asserted).
    const QpCounters &c2 = a.rep[1].qs_e.counters;
    Index pins = 0;
    for (const BoundState b : a.rep[1].qs_e.bound_state) {
        pins += b != BoundState::kFree ? 1 : 0;
    }
    std::printf("[ p1e ] SAWPATH rung 2: status=%d minors=%ld blk_term=%ld cert_rej=%ld "
                "ws_count_violations=%ld dep_tests=%ld dep_dependent=%ld pins=%ld n=%ld me=%ld\n",
                static_cast<int>(a.rep[1].qs_e.status), static_cast<long>(c2.minor_iters),
                static_cast<long>(c2.dep_blocker_terminations),
                static_cast<long>(c2.dep_certificate_rejections),
                static_cast<long>(c2.ws_count_violations), static_cast<long>(c2.dep_tests),
                static_cast<long>(c2.dep_dependent), static_cast<long>(pins),
                static_cast<long>(a.rep[1].elastic.qp.n()),
                static_cast<long>(a.rep[1].elastic.qp.me()));
#if HVEN_TEST_ROUTE_REFERENCE
    EXPECT_FALSE(a.rep[0].usable) << "the shipped (0, 0) start";
    EXPECT_FALSE(a.rep[1].usable) << "rung 2 fails from rung 1's optimum";
    EXPECT_GE(a.out[1].elastic_escalations, 1);
    EXPECT_EQ(a.out[1].elastic_last_rung_fallbacks, 0);
    EXPECT_TRUE(a.rep[2].usable);
    EXPECT_EQ(a.rep[2].qp_status, QpStatus::kOptimal);
    EXPECT_TRUE(a.rep[2].reduced);
    EXPECT_EQ(a.out[2].elastic_last_rung_fallbacks, 1);
#else
    // Which rung fails, and so whether the last-optimal-rung hand-back is needed, is the route
    // (an exactly dependent equality block: WX-R2): recorded, not pinned. PORTABLE: every rung
    // report is honest -- the elastic rung is feasible by construction, so none reads
    // kInfeasible, and a report that calls itself usable is a kOptimal one.
    for (int arm = 0; arm < 3; ++arm) {
        hven::test_support::record_off_reference_route(
            "sawpath_ladder_arm" + std::to_string(arm),
            "usable=" + std::to_string(a.rep[arm].usable) +
                " status=" + std::to_string(static_cast<int>(a.rep[arm].qp_status)) +
                " reduced=" + std::to_string(a.rep[arm].reduced) +
                " escalations=" + std::to_string(a.out[arm].elastic_escalations) +
                " last_rung_fallbacks=" + std::to_string(a.out[arm].elastic_last_rung_fallbacks));
        EXPECT_NE(a.rep[arm].qp_status, QpStatus::kInfeasible) << arm;
        if (a.rep[arm].usable) {
            EXPECT_EQ(a.rep[arm].qp_status, QpStatus::kOptimal) << arm;
        }
    }
#endif
}
