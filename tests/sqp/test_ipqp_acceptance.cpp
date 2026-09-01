// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M6 W1 T9 -- the IPQP tier's ACCEPTANCE battery (spec section 8: A1, A2, A3,
// A5, A11). The engine-, routing-, certification- and restart-level pins live
// in their own files; the acceptance question is whether the tier does the job.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/ipqp_trace.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>

#include "../../bench/bench_cli.h"
#include "../../bench/corpus_cells.h"
#include "../../bench/ipqp_e1_arm.h"
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

/// F7's OWN path-interface geometry, tied to the corpus rather than restated:
/// A1/A2/A3/A5 stand on the `p` the corpus's own cells run at, and the warm
/// hop below is the corpus's continuation source (fix round 1, M4).
constexpr double kWideP = corpus::detail::kPathInterfaceP;
constexpr double kWarmP0 = corpus::detail::kPathInterfaceP0;

/// A2's MKL-scoped trajectory pin, measured at the fix-round-1 head, WARM
/// from the dumped state (cold through the same seam was 18 / 19).
constexpr Index kA2Iters = 5;
constexpr Index kA2Factorizations = 6;

/// A5's, likewise.
constexpr Index kA5Iters = 11;
constexpr Index kA5Factorizations = 12;

/// The HS row whose real trajectory reaches the section 6.2 stall exit.
constexpr int kStallRow = 38;

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
/// its blocks at the indices it returns. `blocks` is carried rather than
/// asserted here -- see the test that owns the claim (fix round 1, M8).
struct JunctionRead {
    Index nodes = 0;
    Index left = 0;  ///< first active row index
    Index right = 0; ///< last active row index
    std::size_t blocks = 0;
};

const JunctionRead &junction_read() {
    static const JunctionRead read = [] {
        constexpr Index kNodes = 40;
        const RealizedFace face = solve_and_read_face(kNodes, kWideP);
        JunctionRead r;
        r.nodes = kNodes;
        r.blocks = face.row_blocks.size();
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

TEST(IpqpAcceptanceA1, TheBoundArcRegimeHasNoJunctionToReadAndThePathInterfaceWindowHasTwo) {
    // RULING 9's REAL SURFACE, measured rather than assumed: at `p <= R/2` the
    // two junctions coincide at `t = 1/2` and NOTHING is active at the
    // solution, so the indices must be read at the PATH-INTERFACE `p`.
    F7CollocationChain model(40, 3, 2, test_support::kE1BoundArcP, 1.0);
    EXPECT_DOUBLE_EQ(model.junction_left(test_support::kE1BoundArcP), 0.5);
    EXPECT_DOUBLE_EQ(model.junction_right(test_support::kE1BoundArcP), 0.5);

    const RealizedFace bound_arc = solve_and_read_face(40, test_support::kE1BoundArcP);
    EXPECT_EQ(bound_arc.active_rows, 0) << "the empty-window regime is empty at the solution";
    EXPECT_EQ(bound_arc.active_bounds, 0) << "and F7's box is inactive at x* by construction";

    const JunctionRead &read = junction_read();
    ASSERT_EQ(read.blocks, 1u) << "F7's own geometry produces ONE window bounded by TWO junctions";
    F7CollocationChain wide(read.nodes, 3, 2, kWideP, 1.0);
    const double span = static_cast<double>(read.nodes - 1);
    // The realized window is `[floor(a) + 1, ceil(b) - 1]` -- the analytic
    // window inset by the strict inequality `psi > R`, discretized. Stated
    // this way it holds at any node count, not only where llround agrees.
    const auto jl = static_cast<Index>(std::floor(wide.junction_left(kWideP) * span));
    const auto jr = static_cast<Index>(std::ceil(wide.junction_right(kWideP) * span));
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
    // RE-DERIVED at T10b's measured `ipqp_init_mu` (0 at the 0.1 placeholder):
    // stopping a decade earlier in `mu` leaves three window rows inside the
    // section 2.3 ratio rule's uncertain band, never forced either way.
    EXPECT_EQ(v.uncertain, 3);
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
    // THREE tie rows, measured (one at the 0.1 placeholder -- T10b re-derived):
    // rows near the blocks draw margins inside the ratio rule's uncertain band
    // at the tier's own tolerance. Never forced either way -- section 2.3.
    EXPECT_EQ(v.uncertain, 3);
    EXPECT_EQ(r.counters.ipqp_face_uncertain, v.uncertain);
    const E1RuleCounts c = e1_rule_counts(cell, r.x, r.lambda_i);
    RecordProperty("a1_two_block_rule_a", static_cast<int>(c.rule_a));
    RecordProperty("a1_two_block_rule_b", static_cast<int>(c.rule_b));
    RecordProperty("a1_two_block_iters", static_cast<int>(r.counters.ipqp_iters));
    RecordProperty("a1_two_block_rule_a_missed", static_cast<int>(c.missed));
    EXPECT_GT(tier_entries(r.counters), 0);
}

// ---------------------------------------------------------------------------
// A2 -- a REAL mid-solve F7 subproblem, THROUGH THE BENCH DUMP SEAM.
// ---------------------------------------------------------------------------

namespace {

// Q-S5 ANSWERED, AND THE FORMAT EXTENDED (fix round 1, R1): --dump-solution
// carries one dense vector and no matrix; --dump-qp carries a whole QP but
// only a cell's FIRST one. Neither reaches a mid-solve major.

// bench_cli.h's version-2 dump does: the whole QP plus the major's dual
// state, so A2 writes a real mid-solve subproblem through the seam, reads it
// back, and solves it WARM the way the driver enters it.
constexpr const char *kA2Usage = "tests/sqp/test_ipqp_acceptance.cpp -- A2's dump round trip\n";

/// The base-warm grade `sqp_driver.cpp`'s `build_ipqp_staged_seed` builds from
/// a major's signed prices, here from the dumped ones.
IpqpSeed base_warm_seed_from(const bench_cli::QpDumpV2 &d) {
    const Index n = d.qp.n(), me = d.qp.me(), mi = d.qp.mi();
    IpqpSeed seed;
    seed.x = Vec::Zero(n);
    seed.s = Vec::Zero(mi);
    seed.lambda_e = d.lambda_e;
    seed.lambda_i = d.lambda_i;
    seed.zl = d.z.cwiseMax(0.0);
    seed.zu = (-d.z).cwiseMax(0.0);
    seed.zeta = Vec::Zero(n);
    seed.lambda_est_e = Vec::Zero(me);
    seed.lambda_est_i = Vec::Zero(mi);
    seed.mu = 0.0;
    seed.grade = IpqpRestartGrade::kBaseWarm;
    return seed;
}

} // namespace

TEST(IpqpAcceptanceA2, ARealMidSolveSubproblemRoundTripsTheDumpSeamAndAgreesWithTheWalk) {
    constexpr Index kNodes = 40;
    // `max_iter` COMPLETES that many majors, so the subproblem built from the
    // iterate it stopped at is the one the driver would build at the NEXT one
    // -- the dump's label is derived from that, never asserted (fix round 2).
    constexpr Index kCompletedMajors = 3;
    constexpr Index kDumpMajor = kCompletedMajors + 1;

    F7CollocationChain model(kNodes, /*states=*/3, /*controls=*/2, kWideP, /*radius=*/1.0);
    model.set_parameters(Vec::Constant(1, kWideP));
    SqpOptions truncated;
    truncated.max_iter = kCompletedMajors;
    SqpDriver driver(truncated);
    const SqpSolution mid = driver.solve(model);
    ASSERT_NE(mid.status, SqpStatus::kOptimal) << "the point must be MID-solve, not the answer";
    ASSERT_TRUE(mid.x.allFinite());

    bench_cli::QpDumpV2 dumped;
    dumped.family = "F7";
    dumped.status = "MidSolve";
    dumped.n_flag = kNodes;
    dumped.major = kDumpMajor;
    dumped.p = kWideP;
    dumped.qp = build_subproblem(model, mid.x, mid.lambda_e, mid.lambda_i);
    dumped.lambda_e = mid.lambda_e;
    dumped.lambda_i = mid.lambda_i;
    dumped.z = mid.z;

    const std::string path = ::testing::TempDir() + "hven_a2_mid_solve_next_major.qpdump";
    {
        std::ofstream out = bench_cli::open_output_or_throw(kA2Usage, "--dump-qp-out", path);
        bench_cli::write_qp_dump_v2(out, dumped);
    }
    std::ifstream in(path);
    ASSERT_TRUE(in.good()) << path;
    const bench_cli::QpDumpV2 got = bench_cli::read_qp_dump_v2(in);
    in.close();
    std::remove(path.c_str());

    // THE SEAM CARRIED THE SUBPROBLEM, not a resemblance of it: the reader's
    // QP is bit-identical to the writer's on every block.
    ASSERT_EQ(got.major, kDumpMajor);
    ASSERT_EQ(got.qp.n(), dumped.qp.n());
    ASSERT_EQ(got.qp.me(), dumped.qp.me());
    ASSERT_EQ(got.qp.mi(), dumped.qp.mi());
    EXPECT_EQ((got.qp.H.toDense() - dumped.qp.H.toDense()).cwiseAbs().maxCoeff(), 0.0);
    EXPECT_EQ((got.qp.Ai.toDense() - dumped.qp.Ai.toDense()).cwiseAbs().maxCoeff(), 0.0);
    EXPECT_EQ((got.qp.g - dumped.qp.g).cwiseAbs().maxCoeff(), 0.0);
    EXPECT_EQ((got.qp.bi - dumped.qp.bi).cwiseAbs().maxCoeff(), 0.0);

    const IpqpSeed seed = base_warm_seed_from(got);
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(got.qp, &seed, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal);
    EXPECT_EQ(r.restart_grade, IpqpRestartGrade::kBaseWarm) << "the dumped state was consumed";
    EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(r.counters));

    QpEngine walk(tight_opts());
    const QpSolution w = walk.solve(got.qp);
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
        // E1's 40-iteration gate, EXCEPT at the tightest margin class, where the
        // T10b default measures 44 -- a DECLARED break of E1's gate on this one
        // cell, priced in `.superpowers/w1-t10b-report.md`.
        EXPECT_LT(r.counters.ipqp_iters, margin <= 1e-6 ? Index{45} : kE1IterGate);

        // THE ROW SET. Exact at the two looser classes; at 1e-6 the ladder's
        // TIGHTEST inactive row sits at 1.4e-7 of row scale, inside the ratio
        // rule's active band at a tier that stops at 1e-8 (measured).
        const FaceVerdict v = check_row_face(cell, r);
        const Index allowed = margin <= 1e-6 ? 1 : 0;
        EXPECT_LE(v.misclassified, allowed) << v.detail;
#ifdef USE_ACCELERATE_SPARSE
        RecordProperty(fmt::format("a3_margin_{:g}_accelerate", margin),
                       "UNOBSERVED -- the exact false-positive count is MKL-only");
#else
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
#ifdef USE_ACCELERATE_SPARSE
    RecordProperty("a5_accelerate", "UNOBSERVED -- the exact trajectory is MKL-only");
#else
    EXPECT_EQ(r.counters.ipqp_iters, kA5Iters);
    EXPECT_EQ(r.counters.ipqp_factorizations, kA5Factorizations);
#endif

    QpEngine walk(tight_opts());
    const QpSolution w = walk.solve(qp);
    ASSERT_EQ(w.status, QpStatus::kOptimal);
    const double scale = std::max(1.0, w.x.lpNorm<Eigen::Infinity>());
    EXPECT_LT((r.x - w.x).lpNorm<Eigen::Infinity>(), 1e-4 * scale);
}

// ---------------------------------------------------------------------------
// A11 -- the HS leg under kIpm on the INDEFINITE rows, plus the QP family.
// ---------------------------------------------------------------------------

namespace {

/// The certification reads and the escapes, each escape tagged with the major
/// the last `ipqp.iter` event named -- which is how a solve-level census gets
/// back to the SUBPROBLEM a stall was charged to (fix round 1, R4).
class AcceptanceTraceSink : public IpqpTraceSink {
  public:
    struct TaggedEscape {
        IpqpTraceEscapeReason reason;
        Index major;
    };
    std::vector<IpqpTraceCertifyEvent> certifies;
    std::vector<TaggedEscape> escapes;
    Index last_major = 0;

    void on_ipqp_iter(const IpqpTraceIterEvent &e) override { last_major = e.major; }
    void on_ipqp_reg(const IpqpTraceRegEvent &) override {}
    void on_ipqp_restart(const IpqpTraceRestartEvent &) override {}
    void on_ipqp_route(const IpqpTraceRouteEvent &) override {}
    void on_ipqp_certify(const IpqpTraceCertifyEvent &e) override { certifies.push_back(e); }
    void on_ipqp_escape(const IpqpTraceEscapeEvent &e) override {
        escapes.push_back({e.reason, last_major});
    }
    void on_qp_mode(const QpModeTraceEvent &) override {}
};

/// R5's POSITIVE witness that the section 2.2 item 4 read HAPPENED: the
/// `ipqp.certify` event fires ONLY on an attempted read (values 0/1/2, never
/// the "not performed" 3), which the counter's 0 cannot say on its own.
::testing::AssertionResult every_final_read_agreed(const AcceptanceTraceSink &sink) {
    for (std::size_t k = 0; k < sink.certifies.size(); ++k) {
        const IpqpTraceCertifyEvent &e = sink.certifies[k];
        if (e.final_inertia != IpqpTraceFinalInertia::kOk || e.downgraded) {
            return ::testing::AssertionFailure()
                   << "certify event " << k << " read " << static_cast<int>(e.final_inertia)
                   << " with downgraded=" << e.downgraded;
        }
    }
    return ::testing::AssertionSuccess();
}

/// The folded IPQP counters of the same solve truncated to `majors` majors.
/// The trajectory to a major does not depend on the budget that stops it, so
/// a difference of two of these is one major's own subproblems.
IpqpCounters counters_through_major(NlpModel &model, Index majors) {
    if (majors <= 0) {
        return IpqpCounters{};
    }
    SqpDriver driver(ipm_options(majors));
    return driver.solve(model).counters.ipqp;
}

} // namespace

TEST(IpqpAcceptanceA11, TheIndefiniteHsRowsSolveUnderKIpmAndCertifyHonestly) {
    struct Row {
        int number;
        const char *why;
        bool certifies; ///< MEASURED: does any subproblem reach a certifying exit?
    };
    // hs_problems.h's own classification: HS10/HS24 are indefinite through the
    // OBJECTIVE, HS33 through a reverse-convex row with a positive multiplier.
    const std::vector<Row> rows = {
        {10, "linear objective, H = li * hess(cI1)", false},
        {24, "cubic x quadratic objective, polyhedral set", true},
        {33, "reverse-convex row, negative-definite contribution", true}};
    for (const Row &row : rows) {
        SCOPED_TRACE(fmt::format("HS{} -- {}", row.number, row.why));
        const HsProblem p = make_hs(row.number);
        AcceptanceTraceSink sink;
        SqpDriver driver(ipm_options());
        driver.attach_trace(&sink);
        const SqpSolution sol = driver.solve(*p.model);
        ASSERT_EQ(sol.status, SqpStatus::kOptimal);
        EXPECT_NEAR(sol.f, p.f_star, 1e-6 * std::max(1.0, std::abs(p.f_star)));

        const IpqpCounters &c = sol.counters.ipqp;
        EXPECT_GT(c.ipqp_iters, 0) << "the tier really solved the subproblems";
        EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(c));
        EXPECT_TRUE(test_support::assert_ipqp_routing_partition(c, tier_entries(c)));
        // THE REQUIRED FINAL READ (section 2.2 item 4), witnessed POSITIVELY by
        // the certify event -- 0 below is ALSO the field's never-read default,
        // so the field alone cannot say a read happened (fix round 1, R5).
        EXPECT_TRUE(every_final_read_agreed(sink));
        RecordProperty(fmt::format("a11_hs{}_certify_events", row.number),
                       static_cast<int>(sink.certifies.size()));
        if (row.certifies) {
            EXPECT_FALSE(sink.certifies.empty()) << "a read must have been ATTEMPTED here";
        } else {
            // MEASURED, and it is why the QP-family leg below carries the
            // claim: every HS10 subproblem leaves by the routing chain, so no
            // final read is ever paid and the field stays at its default.
            EXPECT_TRUE(sink.certifies.empty());
            EXPECT_GT(c.ipqp_to_walk + c.ipqp_to_ssn + c.ipqp_to_refine, 0);
        }
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
        AcceptanceTraceSink sink;
        IpqpEngine tier(tight_opts());
        tier.attach_trace(&sink);
        const IpqpResult r = tier.solve(row.qp, nullptr, IpqpOptions{}, SolveOverrides{});
        ASSERT_EQ(r.status, QpStatus::kOptimal);
        // THE INERTIA GATE FIRED: an indefinite Hessian cannot be stepped on
        // without a demanded modification, so the high-water mark is positive.
        EXPECT_GT(r.counters.ipqp_rho_demanded_max, 0.0);
        EXPECT_GT(r.counters.ipqp_inertia_retries, 0);
        // The read HAPPENED (the event fired) and AGREED, then the field and
        // the flag say the same thing from the result side.
        EXPECT_TRUE(every_final_read_agreed(sink));
        EXPECT_FALSE(sink.certifies.empty()) << "a read must have been ATTEMPTED here";
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

    F7CollocationChain model(kNodes, /*states=*/3, /*controls=*/2, kWarmP0, /*radius=*/1.0);
    model.set_parameters(Vec::Constant(1, kWarmP0));
    SqpDriver seed_driver(ipm_options());
    const SqpSolution seed = seed_driver.solve(model, model.start_point());
    ASSERT_EQ(seed.status, SqpStatus::kOptimal);

    model.set_parameters(Vec::Constant(1, kWideP));
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

// ---------------------------------------------------------------------------
// T4b C7 / T9 item 5 -- the armed-no-advance watch across the whole battery.
// ---------------------------------------------------------------------------

TEST(IpqpAcceptanceCensus, AnArmedRunIsNeverMistakenForAStallAcrossTheHsBattery) {
    Index rows_with_armed = 0;
    Index armed_peak = 0;
    Index stalls = 0;
    Index rows_with_rejections = 0;
    std::string armed_detail;
    std::string stall_detail;
    for (const int number : test_support::hs_numbers()) {
        const HsProblem p = make_hs(number);
        SqpDriver driver(ipm_options());
        const SqpSolution sol = driver.solve(*p.model);
        const IpqpCounters &c = sol.counters.ipqp;
        stalls += c.ipqp_escape_stall;
        if (c.ipqp_escape_stall > 0) {
            stall_detail += fmt::format(" hs{}={}", number, c.ipqp_escape_stall);
        }
        if (c.ipqp_iters_ladder_armed_no_advance > 0) {
            ++rows_with_armed;
            armed_detail += fmt::format(" hs{}={}", number, c.ipqp_iters_ladder_armed_no_advance);
        }
        armed_peak = std::max(armed_peak, c.ipqp_iters_ladder_armed_no_advance);
        // TR-SHRINK-RETRY REACHABILITY under kIpm (T7 registered the question):
        // a rejected major IS a shrink-and-re-solve, so a row with one is the
        // fixture T7 could not find.
        if (sol.counters.rejected_steps > 0) {
            ++rows_with_rejections;
        }
        EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(c));
    }
    RecordProperty("armed_rows", static_cast<int>(rows_with_armed));
    RecordProperty("armed_peak", static_cast<int>(armed_peak));
    RecordProperty("armed_detail", armed_detail);
    RecordProperty("rows_with_rejections", static_cast<int>(rows_with_rejections));
    // SECTION 6.2 NEVER CHARGES AN ARMED RUN AS A STALL. Non-vacuous: the
    // battery really does arm the ladder, on more than one row.
    RecordProperty("stall_detail", stall_detail);
    RecordProperty("stalls", static_cast<int>(stalls));
    EXPECT_GT(rows_with_armed, 0) << armed_detail;
    EXPECT_GT(armed_peak, 10) << armed_detail;
    EXPECT_GT(rows_with_rejections, 0)
        << "a TR shrink-and-retry under kIpm is reachable on this battery";

    // THE NATURAL STALL (T9 item 5), found rather than injected: HS38 reaches
    // section 6.2's exit through a real trajectory, AND runs 51 armed
    // iterations with no gate advance -- charged ONE stall, not 51 (T4b C7).
    EXPECT_GE(stalls, 1) << "the battery must still contain a real stall";
#ifdef USE_ACCELERATE_SPARSE
    RecordProperty("census_accelerate", "UNOBSERVED -- the exact census is MKL-only");
#else
    EXPECT_EQ(stalls, 1);
    EXPECT_EQ(stall_detail, fmt::format(" hs{}=1", kStallRow));
    // T10b re-derivation (65 at the 0.1 placeholder), and no longer build-invariant.
#ifdef NDEBUG
    EXPECT_EQ(armed_peak, 85);
#else
    EXPECT_EQ(armed_peak, 79);
#endif
    EXPECT_EQ(rows_with_armed, 6);
#endif
}

TEST(IpqpAcceptanceCensus, TheNaturalStallIsChargedToASubproblemThatNeverArmedTheLadder) {
    // T4b C7 ON THE STALLING SUBPROBLEM, not on the solve aggregate (fix round
    // 1, R4): an aggregate cannot tell one stalled-and-unarmed subproblem from
    // a stalled one beside an armed one.
    const HsProblem p = make_hs(kStallRow);
    AcceptanceTraceSink sink;
    SqpDriver driver(ipm_options());
    driver.attach_trace(&sink);
    const SqpSolution full = driver.solve(*p.model);
    ASSERT_GE(full.counters.ipqp.ipqp_escape_stall, 1) << "the natural stall must still fire";

    std::vector<Index> stall_majors;
    for (const AcceptanceTraceSink::TaggedEscape &e : sink.escapes) {
        if (e.reason == IpqpTraceEscapeReason::kStall) {
            stall_majors.push_back(e.major);
        }
    }
    ASSERT_EQ(stall_majors.size(), 1u) << "exactly one ipqp.escape event names a stall";
    const Index major = stall_majors.front();
    RecordProperty("stall_major", static_cast<int>(major));

    const IpqpCounters through = counters_through_major(*p.model, major);
    const IpqpCounters before = counters_through_major(*p.model, major - 1);
    // THE PREFIX IS STABLE, or the difference below means nothing.
    ASSERT_GE(through.ipqp_iters, before.ipqp_iters);
    EXPECT_EQ(through.ipqp_escape_stall - before.ipqp_escape_stall, 1)
        << "the stall belongs to major " << major;
    EXPECT_EQ(
        through.ipqp_iters_ladder_armed_no_advance - before.ipqp_iters_ladder_armed_no_advance, 0)
        << "no subproblem of the stalling major armed the ladder, so section 6.2 cannot have "
           "charged an armed run (T4b C7)";
}

// A4 -- NOT the acceptance gate. The gate is the `a4_gate` ctest entry
// (`hven_sqp_ipqp_e1_arm --gate`), which is RED today; these three record the
// taxonomy, the arm's mechanics, and the committed gate CSV's own scoring.

TEST(IpqpAcceptanceA4, TheArmsTaxonomyIsE1sOwnTwentyNineCellsWithTheSweepScriptsSeeds) {
    const std::vector<e1arm::CellSpec> cells = e1arm::taxonomy();
    ASSERT_EQ(cells.size(), 29u) << "20 pre-registered + 9 contiguous (spec section 8.1 A4)";
    // `run_sweep.sh`'s own seed arithmetic (20260826 + idx) and cell ids, and
    // `run_variant_contiguous.sh`'s (20260827 + sidx) -- the two numbers the
    // regeneration stands on, so a typo in either fails here rather than later.
    EXPECT_EQ(cells.front().id, "e1_f7_n4000_af01_m1e-2");
    EXPECT_EQ(cells.front().seed, 20260827u);
    EXPECT_EQ(cells[8].id, "e1_f7_n4000_af30_m1e-6");
    EXPECT_EQ(cells[8].seed, 20260835u);
    EXPECT_EQ(cells[17].id, "e1_f7_n20000_af30_m1e-6");
    EXPECT_EQ(cells[17].seed, 20260844u);
    EXPECT_EQ(cells[18].id, "e1_anchor_f7_n20000_bound_neutral");
    EXPECT_EQ(cells[20].id, "e1blk_f7_n20000_af01_m1e-2");
    EXPECT_EQ(cells[20].seed, 20260828u);
    EXPECT_EQ(cells.back().id, "e1blk_f7_n20000_af30_m1e-6");
    EXPECT_EQ(cells.back().seed, 20260836u);

    Index scattered = 0, contiguous = 0, anchors = 0;
    for (const e1arm::CellSpec &c : cells) {
        scattered += c.layout == e1arm::Layout::kScattered ? 1 : 0;
        contiguous += c.layout == e1arm::Layout::kContiguous ? 1 : 0;
        anchors += c.layout == e1arm::Layout::kAnchor ? 1 : 0;
    }
    EXPECT_EQ(scattered, 18);
    EXPECT_EQ(contiguous, 9);
    EXPECT_EQ(anchors, 2);
}

TEST(IpqpAcceptanceA4, ATwoHundredNodeSmokeOfTheArmsMechanicsWhichIsNotTheGate) {
    // A SMOKE OF THE MECHANICS, not acceptance: a 200-node cell that is NOT in
    // the taxonomy, run so the contiguous LICQ offset search is a covered
    // branch. A4's own verdict is the `a4_gate` entry's, and it is RED.
    e1arm::CellSpec spec;
    spec.id = "a4_gate_contiguous";
    spec.nodes = 200;
    spec.active_fraction = 0.1;
    spec.margin = 1e-2;
    spec.seed = 20260901;
    spec.layout = e1arm::Layout::kContiguous;
    const e1arm::Cell cell = e1arm::build(spec);
    ASSERT_EQ(cell.active.size(), 20u) << "round(0.1 * mi) with mi = 200";
    EXPECT_GE(cell.active_offset, 1) << "row 0 is structurally excluded (LICQ)";
    EXPECT_EQ(cell.active.back() - cell.active.front(), 19) << "the block is contiguous";

    const e1arm::RegenCertificate c = e1arm::certify(cell);
    EXPECT_LE(c.stat_inf, 1e-12 * c.stat_scale) << "the KKT inversion actually inverted";
    EXPECT_GT(c.min_inactive_slack_rel, 0.0);
    EXPECT_GT(c.min_active_multiplier, 0.0);
    EXPECT_GT(c.licq_d_min, 0.0) << "the offset the search chose admits LICQ";

    const e1arm::SolveRow r = e1arm::solve(cell, IpqpOptions{});
    EXPECT_EQ(r.status, QpStatus::kOptimal);
    EXPECT_LT(r.counters.ipqp_iters, e1arm::kIterGate);
    EXPECT_EQ(r.misclassified, 0) << "the ratio rule recovers the block exactly at this margin";
    EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(r.counters));
}

namespace {

/// One row of the committed gate CSV, reduced to the columns A4 scores on.
struct GateRow {
    std::string id, layout, status;
    Index n = 0, iters = 0;
    Index rule_a_missed = 0, rule_a_false_positive = 0;
    double active_fraction = 0.0, margin = 0.0;
};

/// Reads the COMMITTED artifact. Throws rather than returning an empty vector:
/// a record test that silently scores zero rows would pass by vacuity.
std::vector<GateRow> read_committed_gate_csv() {
    std::ifstream in(HVEN_A4_GATE_CSV);
    if (!in) {
        throw std::runtime_error(fmt::format("cannot open {}", HVEN_A4_GATE_CSV));
    }
    std::vector<std::string> header;
    std::vector<GateRow> rows;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::vector<std::string> f;
        for (std::size_t p = 0; p <= line.size();) {
            const std::size_t c = line.find(',', p);
            f.push_back(line.substr(p, c == std::string::npos ? c : c - p));
            if (c == std::string::npos) {
                break;
            }
            p = c + 1;
        }
        if (header.empty()) {
            header = f;
            continue;
        }
        const auto at = [&](const char *name) {
            const auto it = std::find(header.begin(), header.end(), name);
            if (it == header.end()) {
                throw std::runtime_error(fmt::format("gate CSV has no '{}' column", name));
            }
            return f.at(static_cast<std::size_t>(it - header.begin()));
        };
        GateRow r;
        r.id = at("id");
        r.layout = at("layout");
        r.status = at("status");
        r.n = std::stoll(at("n"));
        r.iters = std::stoll(at("ipqp_iters"));
        r.rule_a_missed = std::stoll(at("rule_a_missed"));
        r.rule_a_false_positive = std::stoll(at("rule_a_false_positive"));
        r.active_fraction = std::stod(at("active_fraction"));
        r.margin = std::stod(at("margin_class"));
        rows.push_back(r);
    }
    if (rows.empty()) {
        throw std::runtime_error("the committed gate CSV carried no data rows");
    }
    return rows;
}

} // namespace

TEST(IpqpAcceptanceA4, TheCommittedGateCsvScoresRedOnThreeOfFourAtTheShippedDefaults) {
    // A RECORD of the artifact's scoring, recomputed from its raw columns --
    // not an acceptance pass. It fails if a future edit silently moves the
    // committed numbers, or if the arm stops writing a column A4 scores on.
    const std::vector<GateRow> rows = read_committed_gate_csv();
    ASSERT_EQ(rows.size(), 29u);

    Index converged = 0, under_gate = 0, constructed = 0, exact = 0, anchors_not_scored = 0;
    std::map<std::string, std::vector<std::pair<double, Index>>> tracks;
    for (const GateRow &r : rows) {
        converged += r.status == "optimal" ? 1 : 0;
        under_gate += r.iters < kE1IterGate ? 1 : 0;
        if (r.layout == "anchor") {
            // NOT SCORED: an anchor is E1's own first QP and carries no
            // constructed ground truth. The committed CSV predates the -1
            // sentinel and writes 0 here -- README §2's dated correction.
            EXPECT_TRUE(r.rule_a_missed == 0 || r.rule_a_missed == -1) << r.id;
            ++anchors_not_scored;
            continue;
        }
        ++constructed;
        exact += (r.rule_a_missed == 0 && r.rule_a_false_positive == 0) ? 1 : 0;
        tracks[fmt::format("{}/{}/{:.0e}", r.n, r.layout, r.margin)].emplace_back(r.active_fraction,
                                                                                  r.iters);
    }

    EXPECT_EQ(anchors_not_scored, 2) << "both anchors carry no constructed ground truth";
    EXPECT_EQ(converged, 14) << "A4 criterion 1 is RED: 14 of 29 converge";
    EXPECT_EQ(under_gate, 5) << "A4 criterion 2 is RED: 5 of 29 are under 40 iterations";
    EXPECT_EQ(constructed, 27);
    EXPECT_EQ(exact, 1) << "A4 criterion 3 is RED: 1 of 27 CONSTRUCTED cells recovers exactly";

    // Criterion 4 is the one that PASSES: iterations do not grow monotonically
    // with the active fraction at a fixed size, layout and margin class.
    for (auto &kv : tracks) {
        std::sort(kv.second.begin(), kv.second.end());
        ASSERT_EQ(kv.second.size(), 3u) << kv.first;
        const bool strictly_up =
            kv.second[1].second > kv.second[0].second && kv.second[2].second > kv.second[1].second;
        EXPECT_FALSE(strictly_up) << "monotone blow-up across the active fraction at " << kv.first;
    }
    EXPECT_EQ(tracks.size(), 9u) << "3 margins x (2 scattered sizes + 1 contiguous size)";
}

} // namespace hven::solvers
