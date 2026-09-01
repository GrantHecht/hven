// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M6 W1 T9 -- the IPQP tier's ACCEPTANCE battery (spec section 8: A1, A2, A3,
// A5, A11). The engine-, routing-, certification- and restart-level pins live
// in their own files; the acceptance question is whether the tier does the job.

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>

#include "support/e1_cells.h"
#include "support/hs_problems.h"
#include "support/indefinite_fixtures.h"
#include "support/ipqp_test_support.h"
#include "support/scale_problems.h"

namespace hven::solvers {
namespace {

using test_support::e1_rule_counts;
using test_support::E1Cell;
using test_support::E1RuleCounts;
using test_support::E1Spec;
using test_support::F7CollocationChain;
using test_support::HsProblem;
using test_support::make_e1_cell;
using test_support::make_hs;

/// E1's own gate: every cell converges in fewer than 40 tier iterations.
constexpr Index kE1IterGate = 40;

/// A2's MKL-scoped trajectory pin, measured at T9's head.
constexpr Index kA2Iters = 18;
constexpr Index kA2Factorizations = 19;

QpOptions tight_opts() {
    QpOptions o;
    o.tr_radius = std::numeric_limits<double>::infinity();
    return o;
}

SqpOptions ipm_options(Index max_iter = 60) {
    SqpOptions o;
    o.qp_mode = QpMode::kIpm;
    o.max_iter = max_iter;
    return o;
}

Index tier_entries(const IpqpCounters &c) {
    return c.ipqp_symbolic_analyses + c.ipqp_pattern_verifies;
}

// ---------------------------------------------------------------------------
// A1 -- block placement at F7's OWN junctions.
// ---------------------------------------------------------------------------

/// The realized active set of a solved F7 cell, as contiguous index blocks.
struct RealizedFace {
    std::vector<std::pair<Index, Index>> row_blocks;
    Index active_rows = 0;
    Index active_bounds = 0;
};

RealizedFace solve_and_read_face(Index nodes, double p) {
    F7CollocationChain model(nodes, /*states=*/3, /*controls=*/2, p, /*radius=*/1.0);
    model.set_parameters(Vec::Constant(1, p));
    SqpOptions o;
    o.max_iter = 60;
    SqpDriver driver(o);
    const SqpSolution sol = driver.solve(model);
    EXPECT_EQ(sol.status, SqpStatus::kOptimal) << "n=" << nodes << " p=" << p;

    RealizedFace out;
    bool open = false;
    Index start = 0, prev = 0;
    for (Index j = 0; j < model.mi(); ++j) {
        const bool on = sol.lambda_i(j) > 1e-8;
        if (!on) {
            continue;
        }
        ++out.active_rows;
        if (open && j == prev + 1) {
            prev = j;
            continue;
        }
        if (open) {
            out.row_blocks.emplace_back(start, prev);
        }
        open = true;
        start = j;
        prev = j;
    }
    if (open) {
        out.row_blocks.emplace_back(start, prev);
    }
    for (Index i = 0; i < model.n(); ++i) {
        if (std::abs(sol.z(i)) > 1e-8) {
            ++out.active_bounds;
        }
    }
    return out;
}

/// The junction READ, cached: one walk solve, and every A1 cell below places
/// its blocks at the indices it returns.
struct JunctionRead {
    Index nodes = 0;
    Index left = 0;  ///< first active row index
    Index right = 0; ///< last active row index
};

const JunctionRead &junction_read() {
    static const JunctionRead read = [] {
        constexpr Index kNodes = 40;
        constexpr double kWideP = 0.85;
        const RealizedFace face = solve_and_read_face(kNodes, kWideP);
        EXPECT_EQ(face.row_blocks.size(), 1u)
            << "F7's own geometry produces ONE window bounded by TWO junctions";
        JunctionRead r;
        r.nodes = kNodes;
        if (!face.row_blocks.empty()) {
            r.left = face.row_blocks.front().first;
            r.right = face.row_blocks.front().second;
        }
        return r;
    }();
    return read;
}

/// A1/A3's recovery check: no row is read on the WRONG side of the face. An
/// UNCERTAIN row is not a misclassification (spec section 2.3 makes it a
/// legitimate third answer), so it is counted separately.
struct FaceVerdict {
    Index misclassified = 0;
    Index uncertain = 0;
    std::string detail;
};

FaceVerdict check_row_face(const E1Cell &cell, const IpqpResult &r) {
    FaceVerdict v;
    std::vector<char> truth(static_cast<std::size_t>(cell.qp.bi.size()), 0);
    for (const Index j : cell.active_rows) {
        truth[static_cast<std::size_t>(j)] = 1;
    }
    for (Index j = 0; j < cell.qp.bi.size(); ++j) {
        const IpqpFace got = r.ineq_face[static_cast<std::size_t>(j)];
        if (got == IpqpFace::kUncertain) {
            ++v.uncertain;
            continue;
        }
        const IpqpFace want =
            truth[static_cast<std::size_t>(j)] == 1 ? IpqpFace::kActive : IpqpFace::kInactive;
        if (got != want) {
            ++v.misclassified;
            v.detail +=
                fmt::format(" row {} want {} got {} s={:.3e} li={:.3e};", j, static_cast<int>(want),
                            static_cast<int>(got), r.s(j), r.lambda_i(j));
        }
    }
    return v;
}

} // namespace

TEST(IpqpAcceptanceA1, TheBoundArcRegimeHasNoJunctionToReadAndTheWideWindowHasTwo) {
    // RULING 9's REAL SURFACE, measured rather than assumed: at `p <= R/2` the
    // two junctions coincide at `t = 1/2` and NOTHING is active at the
    // solution, so the indices must be read where the window exists.
    F7CollocationChain model(40, 3, 2, 0.45, 1.0);
    EXPECT_DOUBLE_EQ(model.junction_left(0.45), 0.5);
    EXPECT_DOUBLE_EQ(model.junction_right(0.45), 0.5);

    const RealizedFace bound_arc = solve_and_read_face(40, 0.45);
    EXPECT_EQ(bound_arc.active_rows, 0) << "the empty-window regime is empty at the solution";
    EXPECT_EQ(bound_arc.active_bounds, 0) << "and F7's box is inactive at x* by construction";

    const JunctionRead &read = junction_read();
    F7CollocationChain wide(read.nodes, 3, 2, 0.85, 1.0);
    const Index jl = static_cast<Index>(
        std::llround(wide.junction_left(0.85) * static_cast<double>(read.nodes - 1)));
    const Index jr = static_cast<Index>(
        std::llround(wide.junction_right(0.85) * static_cast<double>(read.nodes - 1)));
    // The realized window is the analytic one inset by the strict inequality:
    // the two nodes ON the junction are not active.
    EXPECT_EQ(read.left, jl + 1);
    EXPECT_EQ(read.right, jr - 1);
}

TEST(IpqpAcceptanceA1, TheWindowAtF7sOwnJunctionsIsRecoveredExactly) {
    const JunctionRead &read = junction_read();
    ASSERT_GT(read.right, read.left);

    E1Spec spec;
    spec.id = "a1_window_at_junctions";
    spec.nodes = read.nodes;
    spec.margin = 1e-4;
    spec.seed = 20260901;
    for (Index j = read.left; j <= read.right; ++j) {
        spec.active_rows.push_back(j);
    }
    const E1Cell cell = make_e1_cell(spec);

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(cell.qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal);
    EXPECT_LT(r.counters.ipqp_iters, kE1IterGate);
    EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(r.counters));

    const FaceVerdict v = check_row_face(cell, r);
    EXPECT_EQ(v.misclassified, 0) << "the ratio rule must recover the window exactly:" << v.detail;
    EXPECT_EQ(v.uncertain, 0) << "and at this margin class no row is a tie";
    EXPECT_EQ(r.counters.ipqp_face_uncertain, v.uncertain);

    const E1RuleCounts c = e1_rule_counts(cell, r.x, r.lambda_i);
    RecordProperty("a1_window_rule_a", static_cast<int>(c.rule_a));
    RecordProperty("a1_window_rule_b", static_cast<int>(c.rule_b));
    RecordProperty("a1_window_iters", static_cast<int>(r.counters.ipqp_iters));
    // E1's Rule A is REPORTED, not asserted: its absolute 1e-8 threshold is
    // precisely the thresholding artifact the ratio rule exists to retire.
    RecordProperty("a1_window_rule_a_missed", static_cast<int>(c.missed));
}

TEST(IpqpAcceptanceA1, TwoBlocksOneAtEachJunctionAreRecoveredExactly) {
    // The TWO-BLOCK structure of spec section 8.1, which E1's single uniformly
    // offset window could not produce: a band at each junction with the
    // interior of the window left inactive.
    const JunctionRead &read = junction_read();
    constexpr Index kBand = 3;
    ASSERT_GT(read.right - read.left, 2 * kBand + 1);

    E1Spec spec;
    spec.id = "a1_two_blocks_at_junctions";
    spec.nodes = read.nodes;
    spec.margin = 1e-4;
    spec.seed = 20260902;
    for (Index j = read.left; j < read.left + kBand; ++j) {
        spec.active_rows.push_back(j);
    }
    for (Index j = read.right - kBand + 1; j <= read.right; ++j) {
        spec.active_rows.push_back(j);
    }
    const E1Cell cell = make_e1_cell(spec);
    ASSERT_EQ(cell.active_rows.size(), static_cast<std::size_t>(2 * kBand));

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(cell.qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal);
    EXPECT_LT(r.counters.ipqp_iters, kE1IterGate);

    const FaceVerdict v = check_row_face(cell, r);
    EXPECT_EQ(v.misclassified, 0) << v.detail;
    // ONE tie row, measured: the interior row nearest the block draws a margin
    // that lands inside the ratio rule's uncertain band at the tier's own
    // tolerance. Never forced either way -- that is section 2.3's design.
    EXPECT_LE(v.uncertain, 1);
    EXPECT_EQ(r.counters.ipqp_face_uncertain, v.uncertain);
    const E1RuleCounts c = e1_rule_counts(cell, r.x, r.lambda_i);
    RecordProperty("a1_two_block_rule_a", static_cast<int>(c.rule_a));
    RecordProperty("a1_two_block_rule_b", static_cast<int>(c.rule_b));
    RecordProperty("a1_two_block_iters", static_cast<int>(r.counters.ipqp_iters));
    RecordProperty("a1_two_block_rule_a_missed", static_cast<int>(c.missed));
    EXPECT_GT(tier_entries(r.counters), 0);
}

// ---------------------------------------------------------------------------
// A2 -- a REAL mid-solve F7 subproblem, not a manufactured x*.
// ---------------------------------------------------------------------------

// Q-S5 ANSWERED: --dump-solution carries ONE dense vector and no matrix at
// all; --dump-qp carries the whole QP but only a cell's FIRST one. Neither
// reaches a mid-solve major.

// So A2 runs the driver to major k and rebuilds that major's subproblem from
// the iterate it stopped at, through the same `build_subproblem` the driver
// itself calls. See .superpowers/w1-t9-report.md.
TEST(IpqpAcceptanceA2, ARealMidSolveSubproblemIsSolvedAndAgreesWithTheWalk) {
    constexpr Index kNodes = 40;
    constexpr double kWideP = 0.85;
    constexpr Index kMajor = 3;

    F7CollocationChain model(kNodes, /*states=*/3, /*controls=*/2, kWideP, /*radius=*/1.0);
    model.set_parameters(Vec::Constant(1, kWideP));
    SqpOptions truncated;
    truncated.max_iter = kMajor;
    SqpDriver driver(truncated);
    const SqpSolution mid = driver.solve(model);
    ASSERT_NE(mid.status, SqpStatus::kOptimal) << "the point must be MID-solve, not the answer";
    ASSERT_TRUE(mid.x.allFinite());

    const QpProblem qp = build_subproblem(model, mid.x, mid.lambda_e, mid.lambda_i);
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal);
    EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(r.counters));

    QpEngine walk(tight_opts());
    const QpSolution w = walk.solve(qp);
    ASSERT_EQ(w.status, QpStatus::kOptimal);

    // THE TIER'S OWN BAND (spec section 2.3 step 1): it stops at the QP
    // tolerances with a 1e2 slack and hands the last two decades to tier 3.
    const double scale = std::max(1.0, w.x.lpNorm<Eigen::Infinity>());
    EXPECT_LT((r.x - w.x).lpNorm<Eigen::Infinity>(), 1e-4 * scale);
    RecordProperty("a2_iters", static_cast<int>(r.counters.ipqp_iters));
    RecordProperty("a2_factorizations", static_cast<int>(r.counters.ipqp_factorizations));
#ifdef USE_ACCELERATE_SPARSE
    RecordProperty("a2_accelerate", "UNOBSERVED -- the exact trajectory is MKL-only");
    EXPECT_GT(r.counters.ipqp_iters, 0);
#else
    EXPECT_EQ(r.counters.ipqp_iters, kA2Iters);
    EXPECT_EQ(r.counters.ipqp_factorizations, kA2Factorizations);
#endif
    // STRUCTURAL, on every backend: one analysis per tier entry, and the
    // factorization count is at least the iteration count (spec section 7).
    EXPECT_GE(r.counters.ipqp_factorizations, r.counters.ipqp_iters);
    EXPECT_EQ(r.counters.ipqp_symbolic_analyses, 1);
}

// ---------------------------------------------------------------------------
// A3 -- simultaneous BOUND and ROW activity, per margin class.
// ---------------------------------------------------------------------------

TEST(IpqpAcceptanceA3, BothSetsAndTheBoundMultiplierSignsAreRecoveredAtEveryMarginClass) {
    const JunctionRead &read = junction_read();
    // Two control indices well away from the active window, so the bound
    // activity is independent of the row activity rather than a consequence.
    const Index lower_idx = 3;                        // node 0's first control
    const Index upper_idx = (read.nodes - 1) * 5 + 4; // the last node's second control

    for (const double margin : {1e-2, 1e-4, 1e-6}) {
        SCOPED_TRACE(fmt::format("margin {:g}", margin));
        E1Spec spec;
        spec.id = fmt::format("a3_margin_{:g}", margin);
        spec.nodes = read.nodes;
        spec.margin = margin;
        spec.seed = 20260903;
        for (Index j = read.left; j < read.left + 4; ++j) {
            spec.active_rows.push_back(j);
        }
        spec.active_lower.push_back(lower_idx);
        spec.active_upper.push_back(upper_idx);
        const E1Cell cell = make_e1_cell(spec);
        ASSERT_GE(cell.active_rows.size(), 1u);
        ASSERT_GE(cell.active_lower.size(), 1u);

        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(cell.qp, nullptr, IpqpOptions{}, SolveOverrides{});
        ASSERT_EQ(r.status, QpStatus::kOptimal);
        EXPECT_LT(r.counters.ipqp_iters, kE1IterGate);

        // THE ROW SET. Exact at the two looser classes; at 1e-6 the ladder's
        // TIGHTEST inactive row sits at 1.4e-7 of row scale, inside the ratio
        // rule's active band at a tier that stops at 1e-8 (measured).
        const FaceVerdict v = check_row_face(cell, r);
        const Index allowed = margin <= 1e-6 ? 1 : 0;
        EXPECT_LE(v.misclassified, allowed) << v.detail;
#ifndef USE_ACCELERATE_SPARSE
        EXPECT_EQ(v.misclassified, allowed) << v.detail;
#endif

        // THE BOUND FACE, both sides, and the SIGNS: a lower-active bound
        // prices through zL > 0 with zU at zero, and the other way up.
        EXPECT_EQ(r.lower_face[static_cast<std::size_t>(lower_idx)], IpqpFace::kActive);
        EXPECT_EQ(r.upper_face[static_cast<std::size_t>(upper_idx)], IpqpFace::kActive);
        EXPECT_EQ(r.bound_state[static_cast<std::size_t>(lower_idx)], BoundState::kAtLower);
        EXPECT_EQ(r.bound_state[static_cast<std::size_t>(upper_idx)], BoundState::kAtUpper);
        EXPECT_GT(r.zl(lower_idx), r.zu(lower_idx));
        EXPECT_GT(r.zu(upper_idx), r.zl(upper_idx));
        EXPECT_NEAR(r.x(lower_idx), cell.qp.lower(lower_idx), 1e-6);
        EXPECT_NEAR(r.x(upper_idx), cell.qp.upper(upper_idx), 1e-6);

        // NO OTHER BOUND IS READ ACTIVE -- the recovery is of the set, not of
        // a superset that happens to contain it.
        Index extra = 0;
        for (Index i = 0; i < cell.qp.g.size(); ++i) {
            if (i == lower_idx || i == upper_idx) {
                continue;
            }
            if (r.lower_face[static_cast<std::size_t>(i)] == IpqpFace::kActive ||
                r.upper_face[static_cast<std::size_t>(i)] == IpqpFace::kActive) {
                ++extra;
            }
        }
        EXPECT_EQ(extra, 0);
        RecordProperty(fmt::format("a3_margin_{:g}_uncertain", margin),
                       static_cast<int>(v.uncertain));
        RecordProperty(fmt::format("a3_margin_{:g}_iters", margin),
                       static_cast<int>(r.counters.ipqp_iters));
        // T4c item 6: the C1 disclosure instrument on a cell that really has
        // bound activity, which the U0 corpus does not.
        RecordProperty(fmt::format("a3_margin_{:g}_read_kept_tight", margin),
                       static_cast<int>(r.counters.ipqp_read_kept_tight_sides));
        RecordProperty(fmt::format("a3_margin_{:g}_read_noise", margin),
                       static_cast<int>(r.counters.ipqp_read_barrier_noise_sides));
    }
}

// ---------------------------------------------------------------------------
// A5 -- the BUILD ruling's surrogate: a COLD WIDE-WINDOW F7 subproblem at size.
// ---------------------------------------------------------------------------

TEST(IpqpAcceptanceA5, AColdWideWindowSubproblemAtSizeIsSolvedInBudget) {
    constexpr Index kNodes = 1000; // nx = 5000, the surrogate's size
    constexpr double kWideP = 0.85;
    F7CollocationChain model(kNodes, /*states=*/3, /*controls=*/2, kWideP, /*radius=*/1.0);
    model.set_parameters(Vec::Constant(1, kWideP));
    const Vec x0 = model.start_point();
    const QpProblem qp = build_subproblem(model, x0, Vec::Zero(model.me()), Vec::Zero(model.mi()));

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal) << "the 2/8 dead population cells' named fix";
    EXPECT_EQ(r.escape_reason, IpqpEscape::kNone);
    EXPECT_LT(r.counters.ipqp_iters, kE1IterGate) << "in budget, at E1's own gate";
    EXPECT_EQ(r.counters.ipqp_symbolic_analyses, 1);
    EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(r.counters));
    RecordProperty("a5_iters", static_cast<int>(r.counters.ipqp_iters));
    RecordProperty("a5_factorizations", static_cast<int>(r.counters.ipqp_factorizations));

    QpEngine walk(tight_opts());
    const QpSolution w = walk.solve(qp);
    ASSERT_EQ(w.status, QpStatus::kOptimal);
    const double scale = std::max(1.0, w.x.lpNorm<Eigen::Infinity>());
    EXPECT_LT((r.x - w.x).lpNorm<Eigen::Infinity>(), 1e-4 * scale);
}

// ---------------------------------------------------------------------------
// A11 -- the HS leg under kIpm on the INDEFINITE rows, plus the QP family.
// ---------------------------------------------------------------------------

TEST(IpqpAcceptanceA11, TheIndefiniteHsRowsSolveUnderKIpmAndCertifyHonestly) {
    struct Row {
        int number;
        const char *why;
    };
    // hs_problems.h's own classification: HS10/HS24 are indefinite through the
    // OBJECTIVE, HS33 through a reverse-convex row with a positive multiplier.
    const std::vector<Row> rows = {{10, "linear objective, H = li * hess(cI1)"},
                                   {24, "cubic x quadratic objective, polyhedral set"},
                                   {33, "reverse-convex row, negative-definite contribution"}};
    for (const Row &row : rows) {
        SCOPED_TRACE(fmt::format("HS{} -- {}", row.number, row.why));
        const HsProblem p = make_hs(row.number);
        SqpDriver driver(ipm_options());
        const SqpSolution sol = driver.solve(*p.model);
        ASSERT_EQ(sol.status, SqpStatus::kOptimal);
        EXPECT_NEAR(sol.f, p.f_star, 1e-6 * std::max(1.0, std::abs(p.f_star)));

        const IpqpCounters &c = sol.counters.ipqp;
        EXPECT_GT(c.ipqp_iters, 0) << "the tier really solved the subproblems";
        EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(c));
        EXPECT_TRUE(test_support::assert_ipqp_routing_partition(c, tier_entries(c)));
        // THE REQUIRED FINAL READ (section 2.2 item 4) HAPPENED, and read
        // RIGHT: a wrong or unreadable one downgrades the certificate, which
        // the seam tests pin from the other side.
        EXPECT_EQ(c.ipqp_final_inertia_read, 0);
        RecordProperty(fmt::format("a11_hs{}_iters", row.number), static_cast<int>(c.ipqp_iters));
        RecordProperty(fmt::format("a11_hs{}_facts", row.number),
                       static_cast<int>(c.ipqp_factorizations));
        RecordProperty(fmt::format("a11_hs{}_reclimbs", row.number),
                       static_cast<int>(c.ipqp_ladder_reclimbs));
        RecordProperty(fmt::format("a11_hs{}_armed_no_advance", row.number),
                       static_cast<int>(c.ipqp_iters_ladder_armed_no_advance));
        RecordProperty(fmt::format("a11_hs{}_rho_dem_max", row.number),
                       fmt::format("{:.6e}", c.ipqp_rho_demanded_max));
        // ITEM 5's WATCH: an armed run with no gate advance is NOT a stall.
        EXPECT_EQ(c.ipqp_escape_stall, 0);
    }
}

TEST(IpqpAcceptanceA11, TheIndefiniteQpFamilyArmsTheLadderAndReachesTheFinalRead) {
    struct Row {
        const char *name;
        QpProblem qp;
    };
    std::vector<Row> rows;
    rows.push_back({"indefinite_equality", test_support::indefinite_equality_qp()});
    rows.push_back({"indefinite_equality_and_row", test_support::indefinite_equality_and_row_qp()});
    rows.push_back({"two_negative_eigenvalue_row", test_support::two_negative_eigenvalue_row_qp()});

    for (Row &row : rows) {
        SCOPED_TRACE(row.name);
        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(row.qp, nullptr, IpqpOptions{}, SolveOverrides{});
        ASSERT_EQ(r.status, QpStatus::kOptimal);
        // THE INERTIA GATE FIRED: an indefinite Hessian cannot be stepped on
        // without a demanded modification, so the high-water mark is positive.
        EXPECT_GT(r.counters.ipqp_rho_demanded_max, 0.0);
        EXPECT_GT(r.counters.ipqp_inertia_retries, 0);
        EXPECT_EQ(r.counters.ipqp_final_inertia_read, 0);
        EXPECT_FALSE(r.certificate_downgraded);
        EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(r.counters));
        RecordProperty(fmt::format("a11_{}_iters", row.name),
                       static_cast<int>(r.counters.ipqp_iters));
        RecordProperty(fmt::format("a11_{}_facts", row.name),
                       static_cast<int>(r.counters.ipqp_factorizations));
        RecordProperty(fmt::format("a11_{}_reclimbs", row.name),
                       static_cast<int>(r.counters.ipqp_ladder_reclimbs));
        RecordProperty(fmt::format("a11_{}_armed_no_advance", row.name),
                       static_cast<int>(r.counters.ipqp_iters_ladder_armed_no_advance));
        EXPECT_EQ(r.counters.ipqp_escape_stall, 0)
            << "an ARMED run with no gate advance is not a stall (T4b C7)";
    }
}

// ---------------------------------------------------------------------------
// T7's acceptance rows: the warm restart on a real continuation hop, kIpm.
// ---------------------------------------------------------------------------

TEST(IpqpAcceptanceWarm, AWarmContinuationHopCostsFewerBarrierIterationsAndKillsNothing) {
    constexpr Index kNodes = 40;
    constexpr double kP0 = 0.80;
    constexpr double kP1 = 0.85;

    F7CollocationChain model(kNodes, /*states=*/3, /*controls=*/2, kP0, /*radius=*/1.0);
    model.set_parameters(Vec::Constant(1, kP0));
    SqpDriver seed_driver(ipm_options());
    const SqpSolution seed = seed_driver.solve(model, model.start_point());
    ASSERT_EQ(seed.status, SqpStatus::kOptimal);

    model.set_parameters(Vec::Constant(1, kP1));
    SqpDriver warm_driver(ipm_options());
    const SqpSolution warm = warm_driver.solve(model, seed.warm_start.x, seed.warm_start);
    ASSERT_EQ(warm.status, SqpStatus::kOptimal);

    SqpDriver cold_driver(ipm_options());
    const SqpSolution cold = cold_driver.solve(model, model.start_point());
    ASSERT_EQ(cold.status, SqpStatus::kOptimal);

    const IpqpCounters &wc = warm.counters.ipqp;
    const IpqpCounters &cc = cold.counters.ipqp;
    RecordProperty("warm_ipqp_iters", static_cast<int>(wc.ipqp_iters));
    RecordProperty("cold_ipqp_iters", static_cast<int>(cc.ipqp_iters));
    RecordProperty("warm_restart_repairs", static_cast<int>(wc.ipqp_restart_repairs));
    RecordProperty("warm_mu_adopted", static_cast<int>(wc.ipqp_mu_adopted));

    // THE ACCEPTANCE CLAIM: the hop is cheaper in barrier iterations, and the
    // warm-kill rule never fires -- a kill would mean the seed was worse than
    // nothing, which is exactly what section 5.5 exists to bound.
    EXPECT_LT(wc.ipqp_iters, cc.ipqp_iters);
    EXPECT_EQ(wc.ipqp_warm_restart_abandoned, 0);
    EXPECT_EQ(wc.ipqp_escapes, 0);
    EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(wc));
    EXPECT_NEAR(warm.f, cold.f, 1e-6 * std::max(1.0, std::abs(cold.f)));
}

} // namespace hven::solvers
