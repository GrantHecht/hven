// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_solver_indexing_data.cpp — the index map a declared piece carries
// (model/solver_indexing_data.h) and the thread-mode lock of SolverFunctionBase
// (model/solver_function_base.h), asserted directly: the shape refusals of every setter,
// the continuity classification, the reduced-output redirection, the gradient and
// constraint slot layout, and thread_split's partition arithmetic.

#include <stdexcept>
#include <string>

#include <Eigen/Core>
#include <gtest/gtest.h>

#include <hven/model/parsed_io_flags.h>
#include <hven/model/solver_function_base.h>
#include <hven/model/solver_indexing_data.h>
#include <hven/model/threading_flags.h>

namespace hven {
namespace {

using Mi = Eigen::MatrixXi;
using Vi = Eigen::VectorXi;

template <class F> void expect_refusal(F &&f, std::initializer_list<const char *> fragments) {
    try {
        f();
        ADD_FAILURE() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        const std::string message(e.what());
        for (const char *fragment : fragments) {
            EXPECT_NE(message.find(fragment), std::string::npos)
                << "missing '" << fragment << "' in: " << message;
        }
    }
}

TEST(SolverIndexingData, ADefaultMapIsEmptyAndUninitialised) {
    const SolverIndexingData d;
    EXPECT_EQ(d.input_size, 0);
    EXPECT_EQ(d.output_size, 0);
    EXPECT_EQ(d.num_appl(), 0);
    EXPECT_FALSE(d.vindex_init);
    EXPECT_FALSE(d.cindex_init);
    EXPECT_TRUE(d.unique_constraints);
    EXPECT_FALSE(d.v_out_reduced);
    EXPECT_TRUE(d.get_v_index().size() == 0);
    EXPECT_TRUE(d.get_c_index().size() == 0);
}

TEST(SolverIndexingData, TheObjectiveConstructorTakesAOneRowOutputAndMarksOnlyTheVariableMap) {
    Mi v(2, 3);
    v << 0, 2, 4, 1, 3, 5;
    const SolverIndexingData d(2, v);
    EXPECT_EQ(d.input_size, 2);
    EXPECT_EQ(d.output_size, 1);
    EXPECT_EQ(d.num_appl(), 3);
    EXPECT_TRUE(d.vindex_init);
    EXPECT_FALSE(d.cindex_init);
    EXPECT_EQ(d.v_loc(1, 2), 5);
    EXPECT_EQ(d.get_v_index(), v);
}

TEST(SolverIndexingData, TheConstraintConstructorTakesBothMaps) {
    Mi v(2, 2), c(1, 2);
    v << 0, 1, 1, 2;
    c << 7, 8;
    const SolverIndexingData d(2, 1, v, c);
    EXPECT_TRUE(d.vindex_init);
    EXPECT_TRUE(d.cindex_init);
    EXPECT_EQ(d.num_appl(), 2);
    EXPECT_EQ(d.c_loc(0, 1), 8);
    EXPECT_EQ(d.get_c_index(), c);
}

TEST(SolverIndexingData, SettersRefuseAMapWhoseRowsDisagreeWithTheDeclaredSizesByName) {
    SolverIndexingData d;
    d.input_size = 3;
    d.output_size = 2;
    expect_refusal([&] { d.set_v_index(Mi::Zero(2, 4)); },
                   {"SolverIndexingData::set_v_index", "expected 3 rows (input_size_)", "got 2"});
    expect_refusal([&] { d.set_c_index(Mi::Zero(3, 4)); },
                   {"SolverIndexingData::set_c_index", "expected 2 rows (output_size_)", "got 3"});
    expect_refusal([&] { d.set_v_index_c_index(Mi::Zero(1, 1), Mi::Zero(2, 1)); },
                   {"set_v_index", "got 1"});
    // A refused setter leaves the flags where they were.
    EXPECT_FALSE(d.vindex_init);
    EXPECT_FALSE(d.cindex_init);
    expect_refusal([&] { SolverIndexingData bad(3, Mi::Zero(5, 1)); }, {"expected 3 rows"});
    expect_refusal([&] { SolverIndexingData bad(2, 2, Mi::Zero(2, 1), Mi::Zero(1, 1)); },
                   {"set_c_index", "expected 2 rows"});
}

TEST(SolverIndexingData, EachColumnIsClassifiedContiguousOrNot) {
    Mi v(3, 4);
    // col 0: 4,5,6 contiguous; col 1: 0,2,4 not; col 2: 9,9,9 repeats are not contiguous;
    // col 3: descending 6,5,4 is not contiguous under an ascending-run rule.
    v << 4, 0, 9, 6, 5, 2, 9, 5, 6, 4, 9, 4;
    const SolverIndexingData d(3, v);
    ASSERT_EQ(d.v_index_continuity.size(), 4u);
    EXPECT_EQ(d.v_index_continuity[0], ParsedIoFlags::kContiguous);
    EXPECT_EQ(d.v_index_continuity[1], ParsedIoFlags::kNotContiguous);
    EXPECT_EQ(d.v_index_continuity[2], ParsedIoFlags::kNotContiguous);
    EXPECT_EQ(d.v_index_continuity[3], ParsedIoFlags::kNotContiguous);

    EXPECT_EQ(SolverIndexingData::check_continuity(Vi::LinSpaced(5, 10, 14)),
              ParsedIoFlags::kContiguous);
    // A single entry and an empty column have no gap to find.
    EXPECT_EQ(SolverIndexingData::check_continuity(Vi::Constant(1, 7)), ParsedIoFlags::kContiguous);
    EXPECT_EQ(SolverIndexingData::check_continuity(Vi(0)), ParsedIoFlags::kContiguous);

    // The constraint map is classified by the same rule.
    Mi c(2, 2);
    c << 0, 3, 1, 5;
    SolverIndexingData e;
    e.input_size = 2;
    e.output_size = 2;
    e.set_v_index_c_index(v.topRows(2).leftCols(2), c);
    ASSERT_EQ(e.c_index_continuity.size(), 2u);
    EXPECT_EQ(e.c_index_continuity[0], ParsedIoFlags::kContiguous);
    EXPECT_EQ(e.c_index_continuity[1], ParsedIoFlags::kNotContiguous);
}

TEST(SolverIndexingData, ReplacingAMapResizesTheContinuityRecord) {
    SolverIndexingData d(2, Mi::Zero(2, 5));
    EXPECT_EQ(d.v_index_continuity.size(), 5u);
    d.set_v_index(Mi::Zero(2, 2));
    EXPECT_EQ(d.v_index_continuity.size(), 2u);
    EXPECT_EQ(d.num_appl(), 2);
}

TEST(SolverIndexingData, ASeparateOutputMapMustMatchTheInputMapAndRedirectsTheScatter) {
    Mi v(2, 2);
    v << 0, 1, 1, 2;
    SolverIndexingData d(2, v);
    EXPECT_FALSE(d.v_out_reduced);
    EXPECT_EQ(d.v_scatter_loc(1, 1), 2);

    expect_refusal([&] { d.set_output_v_index(Mi::Zero(2, 3)); },
                   {"set_output_v_index", "expected a 2x2 map", "got 2x3"});
    expect_refusal([&] { d.set_output_v_index(Mi::Zero(1, 2)); }, {"got 1x2"});
    EXPECT_FALSE(d.v_out_reduced) << "a refused map is not installed";

    Mi out(2, 2);
    out << 10, 11, 11, 12;
    d.set_output_v_index(out);
    EXPECT_TRUE(d.v_out_reduced);
    EXPECT_EQ(d.v_out_loc(0, 1), 11);
    EXPECT_EQ(d.v_scatter_loc(1, 1), 12);
    EXPECT_EQ(d.v_loc(1, 1), 2) << "the input map is untouched";

    d.clear_output_v_index();
    EXPECT_FALSE(d.v_out_reduced);
    EXPECT_EQ(d.v_out_index.size(), 0);
    EXPECT_EQ(d.v_scatter_loc(1, 1), 2);
}

TEST(SolverIndexingData, GradientAndConstraintSpacePublishOneSlotPerInputAndOutputInOrder) {
    Mi v(2, 3), c(2, 3);
    v << 0, 2, 4, 1, 3, 5;
    c << 10, 12, 14, 11, 13, 15;
    SolverIndexingData d(2, 2, v, c);

    Vi gx = Vi::Constant(10, -9);
    int free = 3; // slots before 3 belong to someone else
    d.get_gradient_space(gx, free);
    EXPECT_EQ(free, 9);
    EXPECT_EQ(d.inner_gradient_starts, (Vi(3) << 3, 5, 7).finished());
    EXPECT_EQ(gx.segment(3, 6), (Vi(6) << 0, 1, 2, 3, 4, 5).finished());
    EXPECT_EQ(gx(2), -9);
    EXPECT_EQ(gx(9), -9);

    Vi fx = Vi::Constant(8, -9);
    free = 1;
    d.get_constraint_space(fx, free);
    EXPECT_EQ(free, 7);
    EXPECT_EQ(d.inner_constraint_starts, (Vi(3) << 1, 3, 5).finished());
    EXPECT_EQ(fx.segment(1, 6), (Vi(6) << 10, 11, 12, 13, 14, 15).finished());

    // With a reduced output map the gradient slots follow it, not the input map.
    Mi out(2, 3);
    out << 20, 22, 24, 21, 23, 25;
    d.set_output_v_index(out);
    Vi gr = Vi::Constant(6, -9);
    free = 0;
    d.get_gradient_space(gr, free);
    EXPECT_EQ(gr, (Vi(6) << 20, 21, 22, 23, 24, 25).finished());
}

TEST(SolverIndexingData, ThreadSplitPartitionsTheApplicationsByColumnAndKeepsTheFlags) {
    Mi v(2, 5), c(1, 5);
    v << 0, 2, 4, 6, 8, 1, 3, 5, 7, 9;
    c << 0, 1, 2, 3, 4;
    SolverIndexingData d(2, 1, v, c);
    d.unique_constraints = false;

    const auto parts = d.thread_split(3); // 5 = 2 + 2 + 1
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0].num_appl(), 2);
    EXPECT_EQ(parts[1].num_appl(), 2);
    EXPECT_EQ(parts[2].num_appl(), 1);
    EXPECT_EQ(parts[0].get_v_index(), v.middleCols(0, 2));
    EXPECT_EQ(parts[1].get_v_index(), v.middleCols(2, 2));
    EXPECT_EQ(parts[2].get_v_index(), v.middleCols(4, 1));
    EXPECT_EQ(parts[2].get_c_index(), c.middleCols(4, 1));
    for (const auto &p : parts) {
        EXPECT_FALSE(p.unique_constraints) << "the sharing flag travels with every partition";
        EXPECT_TRUE(p.cindex_init);
        EXPECT_EQ(p.input_size, 2);
        EXPECT_EQ(p.output_size, 1);
    }

    // One thread gets everything; more threads than applications yield one per application.
    EXPECT_EQ(d.thread_split(1).size(), 1u);
    EXPECT_EQ(d.thread_split(1)[0].num_appl(), 5);
    const auto many = d.thread_split(9);
    ASSERT_EQ(many.size(), 5u);
    for (const auto &p : many) {
        EXPECT_EQ(p.num_appl(), 1);
    }
}

TEST(SolverIndexingData, ThreadSplitOfAnObjectiveMapCarriesNoConstraintMap) {
    Mi v(1, 4);
    v << 0, 1, 2, 3;
    const SolverIndexingData d(1, v);
    const auto parts = d.thread_split(2);
    ASSERT_EQ(parts.size(), 2u);
    for (const auto &p : parts) {
        EXPECT_FALSE(p.cindex_init);
        EXPECT_EQ(p.num_appl(), 2);
        EXPECT_TRUE(p.unique_constraints);
    }
}

TEST(SolverIndexingData, ThreadSplitRefusesANonPositiveCountAndSplitsNothingToNothing) {
    SolverIndexingData d(1, Mi::Zero(1, 2));
    expect_refusal([&] { d.thread_split(0); }, {"thread_split", "Threads must be positive", "got 0"});
    expect_refusal([&] { d.thread_split(-2); }, {"got -2"});
    EXPECT_TRUE(SolverIndexingData().thread_split(4).empty());
}

TEST(SolverIndexingData, TheCanonicalLockColumnIsTheSmallerOfTheTwo) {
    EXPECT_EQ(kkt_canonical_lock_col(3, 7), 3);
    EXPECT_EQ(kkt_canonical_lock_col(7, 3), 3);
    EXPECT_EQ(kkt_canonical_lock_col(5, 5), 5);
}

// ---------------------------------------------------------------------------
// SolverFunctionBase: the thread mode is a lay-time property.
// ---------------------------------------------------------------------------

struct StubFunction {
    std::string name() const { return "stub"; }
    int output_rows() const { return 2; }
    int input_rows() const { return 3; }
    int num_kkt_elements(bool dojac, bool dohess) const { return (dojac ? 6 : 0) + (dohess ? 1 : 0); }
    void get_kkt_space(Eigen::Ref<Vi> rows, Eigen::Ref<Vi> cols, int &free, int offset, bool, bool,
                       const SolverIndexingData &data) {
        for (int v = 0; v < data.num_appl(); ++v) {
            rows[free] = offset + v;
            cols[free] = data.v_loc(0, v);
            ++free;
        }
    }
};

TEST(SolverFunctionBaseBoundaries, CountsScaleWithTheApplicationsTheMapCarries) {
    SolverFunctionBase<StubFunction> f;
    EXPECT_EQ(f.num_con_eles(), 0);
    EXPECT_EQ(f.num_grad_eles(), 0);
    EXPECT_EQ(f.num_kkt_elements(true, true), 0);
    f.index_data = SolverIndexingData(3, 2, Mi::Zero(3, 4), Mi::Zero(2, 4));
    EXPECT_EQ(f.num_con_eles(), 8);
    EXPECT_EQ(f.num_grad_eles(), 12);
    EXPECT_EQ(f.num_kkt_elements(true, false), 24);
    EXPECT_EQ(f.num_kkt_elements(false, true), 4);
}

TEST(SolverFunctionBaseBoundaries, SpaceCallsForwardToTheFunctionAndTheIndexData) {
    SolverFunctionBase<StubFunction> f;
    Mi v(3, 2);
    v << 5, 6, 7, 8, 9, 10;
    f.index_data = SolverIndexingData(3, 2, v, (Mi(2, 2) << 0, 2, 1, 3).finished());
    Vi rows = Vi::Zero(4), cols = Vi::Zero(4);
    int free = 1;
    f.get_kkt_space(rows, cols, free, 100, true, false);
    EXPECT_EQ(free, 3);
    EXPECT_EQ(rows.segment(1, 2), (Vi(2) << 100, 101).finished());
    EXPECT_EQ(cols.segment(1, 2), (Vi(2) << 5, 6).finished());

    Vi gx = Vi::Zero(6), fx = Vi::Zero(4);
    free = 0;
    f.get_gradient_space(gx, free);
    EXPECT_EQ(free, 6);
    EXPECT_EQ(gx, (Vi(6) << 5, 7, 9, 6, 8, 10).finished());
    free = 0;
    f.get_constraint_space(fx, free);
    EXPECT_EQ(fx, (Vi(4) << 0, 1, 2, 3).finished());
}

TEST(SolverFunctionBaseBoundaries, TheThreadModeStartsByApplicationAndIsWritableBeforeALay) {
    SolverFunctionBase<StubFunction> f;
    EXPECT_EQ(f.get_thread_mode(), ThreadingFlags::kByApplication);
    f.set_thread_mode(ThreadingFlags::kByApplication); // idempotent while unlaid
    EXPECT_EQ(f.get_thread_mode(), ThreadingFlags::kByApplication);
    f.set_thread_mode(ThreadingFlags::kThread3);
    EXPECT_EQ(f.get_thread_mode(), ThreadingFlags::kThread3);
}

} // namespace
} // namespace hven
