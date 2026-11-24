// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_i19_sens_grid.cpp -- the I19-2a SENS-P grid runner's implementation (bench/i19_sens_grid.h):
// the cell file, the declared --set keys, the options a cell is solved under, and the CSV row.

#include <algorithm>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "bench/i19_sens_grid.h"

namespace hven {
namespace {

using bench::i19::SensCell;

const char *kHeader = "cell_id\tset\tproblem\tbase\tfamily\tsize\tp\tcaller_step_limit\n";

std::vector<SensCell> parse(const std::string &text) {
    std::istringstream is(text);
    return bench::i19::parse_cells(is);
}

std::size_t commas(const std::string &s) { return std::count(s.begin(), s.end(), ','); }

TEST(I19SensGrid, ParsesACellFile) {
    const auto cells = parse(std::string("# comment\n") + kHeader +
                             "c1\tsens-x\tF2-p0.5\tdefault\tF2CircleNlp\t0\t0.5\t0\n"
                             "c2\tsenscap-q\tF3-n20-p0.25\tsqp-qn\tF3SpringChain\t20\t0.25\t0.1\n");
    ASSERT_EQ(cells.size(), 2u);
    EXPECT_EQ(cells[1].cell_id, "c2");
    EXPECT_EQ(cells[1].set, "senscap-q");
    EXPECT_EQ(cells[1].base, "sqp-qn");
    EXPECT_EQ(cells[1].size, 20);
    EXPECT_EQ(cells[1].p, 0.25);
    EXPECT_EQ(cells[1].caller_step_limit, 0.1);
}

TEST(I19SensGrid, RefusesAMalformedCellFile) {
    EXPECT_THROW(parse(""), std::invalid_argument) << "no header";
    EXPECT_THROW(parse("cell_id\tset\n"), std::invalid_argument) << "wrong header";
    EXPECT_THROW(parse(std::string(kHeader) + "c1\tsens-x\tF2\tdefault\tF2CircleNlp\t0\t0.5\n"),
                 std::invalid_argument)
        << "seven fields";
    EXPECT_THROW(parse(std::string(kHeader) + "c1\tsens-x\tF2\texact\tF2CircleNlp\t0\t0.5\t0\n"),
                 std::invalid_argument)
        << "an unknown base";
    EXPECT_THROW(parse(std::string(kHeader) + "c1\tsens-x\tF2\tdefault\tF2CircleNlp\t0\tx\t0\n"),
                 std::invalid_argument)
        << "a non-numeric p";
    EXPECT_THROW(parse(std::string(kHeader) + "c1\tsens-x\tF2\tdefault\tF2CircleNlp\t0\t0.5\t0\n" +
                       "c1\tsens-x\tF2\tdefault\tF2CircleNlp\t0\t0.5\t0\n"),
                 std::invalid_argument)
        << "a duplicate id";
}

TEST(I19SensGrid, EveryDeclaredKeyReachesItsOptionAndNothingElseIsAccepted) {
    SensCell c{"c", "sens-x", "F2", "default", "F2CircleNlp", 0, 0.5, 0.0};
    const SqpOptions o = bench::i19::cell_options(
        c,
        {"advanced.trust_region_growth=kRatioByStepType",
         "advanced.trust_region_growth_after_soc=false", "advanced.trust_region_rejection_memory=3",
         "advanced.trust_region_growth_ratio_max=3", "advanced.trust_region_shrink_to_step=true",
         "advanced.trust_region_growth_on_step_limit=false",
         "advanced.trust_region_growth_funnel_guard=true", "advanced.second_order_correction=false",
         "trust_region_initial=1.000000000000001", "caller_step_limit=0.1"});
    EXPECT_EQ(o.advanced.trust_region_growth, SqpTrustRegionGrowth::kRatioByStepType);
    EXPECT_FALSE(o.advanced.trust_region_growth_after_soc);
    EXPECT_EQ(o.advanced.trust_region_rejection_memory, 3);
    EXPECT_EQ(o.advanced.trust_region_growth_ratio_max, 3.0);
    EXPECT_TRUE(o.advanced.trust_region_shrink_to_step);
    EXPECT_FALSE(o.advanced.trust_region_growth_on_step_limit);
    EXPECT_TRUE(o.advanced.trust_region_growth_funnel_guard);
    EXPECT_FALSE(o.advanced.second_order_correction);
    EXPECT_EQ(o.trust_region_initial, 1.000000000000001);
    EXPECT_NE(o.trust_region_initial, 1.0);
    EXPECT_EQ(o.caller_step_limit, 0.1);
    EXPECT_EQ(o.hessian_mode, SqpHessianMode::kExact);
    EXPECT_EQ(bench::i19::cell_options(c, {"advanced.trust_region_growth_ratio_max=inf"})
                  .advanced.trust_region_growth_ratio_max,
              std::numeric_limits<double>::infinity());
    EXPECT_THROW(bench::i19::cell_options(c, {"advanced.qp_mode=kSsn"}), std::invalid_argument);
    EXPECT_THROW(bench::i19::cell_options(c, {"advanced.trust_region_shrink_to_step=1"}),
                 std::invalid_argument);
    EXPECT_THROW(bench::i19::cell_options(c, {"advanced.trust_region_growth=kBogus"}),
                 std::invalid_argument);
    EXPECT_THROW(bench::i19::cell_options(c, {"advanced.trust_region_growth_ratio_max=1"}),
                 std::invalid_argument)
        << "validate() refuses a ceiling at 1";
    EXPECT_THROW(bench::i19::cell_options(c, {"noequals"}), std::invalid_argument);
}

TEST(I19SensGrid, TheBaseSelectsTheHessianModeAndTheCellItsCap) {
    SensCell c{"c", "senscap-q", "F2", "sqp-qn", "F2CircleNlp", 0, 0.5, 0.1};
    const SqpOptions o = bench::i19::cell_options(c, {});
    EXPECT_EQ(o.hessian_mode, SqpHessianMode::kLbfgsDamped);
    EXPECT_EQ(o.caller_step_limit, 0.1);
    EXPECT_EQ(bench::i19::cell_options(c, {"caller_step_limit=0"}).caller_step_limit, 0.0)
        << "an arm's --set caller_step_limit wins";
}

TEST(I19SensGrid, TheFrozenCellFileParsesAndEveryCellBuilds) {
    std::ifstream in(HVEN_I19_SENS_P_CELLS);
    ASSERT_TRUE(in) << HVEN_I19_SENS_P_CELLS;
    const std::vector<SensCell> cells = bench::i19::parse_cells(in);
    EXPECT_EQ(cells.size(), 104u) << "96 parametric cells + 8 sensfix cells";
    std::set<std::string> sets;
    for (const SensCell &c : cells) {
        sets.insert(c.set);
        EXPECT_NO_THROW((void)bench::i19::make_model(c)) << c.cell_id;
        EXPECT_EQ(c.caller_step_limit > 0.0, c.set.rfind("senscap", 0) == 0 || c.family == "F-CAP")
            << c.cell_id;
        EXPECT_EQ(bench::i19::is_fixture(c), c.set.rfind("sensfix", 0) == 0) << c.cell_id;
        EXPECT_EQ(c.base == "sqp-qn", c.set.back() == 'q') << c.cell_id;
    }
    EXPECT_EQ(sets, (std::set<std::string>{"sens-x", "sens-q", "senscap-x", "senscap-q",
                                           "sensfix-x", "sensfix-q"}));
}

TEST(I19SensGrid, TheSensfixCellsRunTheFourFixturesAtTheirPinnedParameters) {
    std::ifstream in(HVEN_I19_SENS_P_CELLS);
    ASSERT_TRUE(in);
    std::size_t fixtures = 0;
    for (const SensCell &c : bench::i19::parse_cells(in)) {
        if (!bench::i19::is_fixture(c)) {
            continue;
        }
        ++fixtures;
        const hven::test_support::I19Fixture f = bench::i19::make_fixture(c);
        const SqpOptions o = bench::i19::cell_options(c, {});
        EXPECT_EQ(o.advanced.second_order_correction, f.options.advanced.second_order_correction)
            << c.cell_id;
        EXPECT_EQ(o.common.max_iterations, f.options.common.max_iterations) << c.cell_id;
        EXPECT_EQ(o.caller_step_limit, f.options.caller_step_limit) << c.cell_id;
        EXPECT_EQ(o.hessian_mode, f.options.hessian_mode) << c.cell_id;
        EXPECT_EQ(o.common.verbosity, Verbosity::kSilent);
        // The grid solve is the fixture's own solve (same options, same start): same counters.
        const SqpResult g = bench::i19::run_cell(c, {});
        const SqpResult ref = test_support::i19_solve(f);
        EXPECT_EQ(g.status, SolveStatus::kOptimal) << c.cell_id;
        EXPECT_EQ(g.counters.major_iters, ref.counters.major_iters) << c.cell_id;
        EXPECT_EQ(g.work.factorizations, ref.work.factorizations) << c.cell_id;
        EXPECT_EQ(g.counters.rejected_steps, ref.counters.rejected_steps) << c.cell_id;
        // An arm's --set reaches the fixture's options.
        EXPECT_TRUE(bench::i19::cell_options(c, {"advanced.trust_region_shrink_to_step=true"})
                        .advanced.trust_region_shrink_to_step);
    }
    EXPECT_EQ(fixtures, 8u);
    // A cell whose rho or cap drifts from the fixture is refused.
    EXPECT_THROW(bench::i19::make_fixture(
                     SensCell{"c", "sensfix-x", "F-CAP", "default", "F-CAP", 0, 100.0, 0.0}),
                 std::invalid_argument);
    EXPECT_THROW(
        bench::i19::make_fixture(SensCell{"c", "sensfix-x", "F-H", "default", "F-H", 0, 31.0, 0.0}),
        std::invalid_argument);
}

TEST(I19SensGrid, ACellSolvesAndItsRowMatchesTheHeader) {
    const SensCell c{"sens-x-F2-p0.5", "sens-x", "F2-p0.5", "default", "F2CircleNlp", 0, 0.5, 0.0};
    const std::vector<std::string> sets = {"advanced.trust_region_rejection_memory=3"};
    const SqpResult r = bench::i19::run_cell(c, sets);
    EXPECT_EQ(r.status, SolveStatus::kOptimal);
    const std::string header = bench::i19::csv_header(r);
    const std::string row = bench::i19::csv_row(c, sets, r);
    EXPECT_EQ(commas(header), commas(row));
    EXPECT_EQ(header.rfind(
                  "cell_id,set,problem,base,overrides,status,factorizations,eval_f,"
                  "eval_jac,major_iters,rejected_steps,reject_funnel_ceiling,trust_region_growths,"
                  "trust_region_growth_refusals,trust_region_shrinks_to_step,trust_region_grown_"
                  "step_rejections,"
                  "trust_region_unbound_rejections,step_limit_binding_majors,objective,counter.",
                  0),
              0u);
    EXPECT_EQ(row.rfind("sens-x-F2-p0.5,sens-x,F2-p0.5,default,"
                        "advanced.trust_region_rejection_memory=3,optimal,",
                        0),
              0u);
    // Deterministic: the same cell solved again writes the same row.
    EXPECT_EQ(bench::i19::csv_row(c, sets, bench::i19::run_cell(c, sets)), row);
}

} // namespace
} // namespace hven
