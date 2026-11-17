// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M8 TR-E: the v1 READER FIXTURES. Three small streams written by JsonLinesTraceSink from
// hand-filled records, committed under tests/drivers/golden/trace_v1/, so an outside reader (the
// hven-bench trace reader, schema/trace_v1.py) has golden lines to test against that this suite
// holds byte-exact:
//   ipm_complete.jsonl  solve.begin, two iter rows (one restoration), a message, solve.end
//   sqp_complete.jsonl  a depth-0 solve with a nested depth-1 sub-solve (begin, row, end) between
//   its rows ipm_aborted.jsonl   solve.begin and one iter row, no solve.end (an aborted solve,
//   schema 2.1)
// Every value is hand-filled and every double is exactly representable or a pinned literal, so no
// solver, MKL kernel or platform reaches a byte.
//
// Regenerate (a DECLARED event: every outside reader's fixtures move with it):
//   HVEN_REGEN_GOLDENS=1 ./hven_drivers_tests --gtest_filter='TraceV1Fixtures.*'

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <hven/core/solver_counters.h>
#include <hven/drivers/json_lines_trace_sink.h>

#ifndef HVEN_TRACE_FIXTURE_DIR
#error "HVEN_TRACE_FIXTURE_DIR must be defined by tests/drivers/CMakeLists.txt"
#endif

namespace {

using namespace hven;

ProblemStatistics statistics() {
    ProblemStatistics st;
    st.variables = 4;
    st.free_variables = 1;
    st.lower_bounded_variables = 1;
    st.upper_bounded_variables = 1;
    st.boxed_variables = 1;
    st.fixed_variables = 0;
    st.constraints = 2;
    st.equality_rows = 1;
    st.lower_bounded_rows = 1;
    return st;
}

SolveBeginTraceEvent begin(TraceEngine engine) {
    SolveBeginTraceEvent e;
    e.engine = engine;
    e.statistics = statistics();
    e.optimality_tol = 1e-8;
    e.feasibility_tol = 1e-8;
    e.max_iterations = 50;
    if (engine == TraceEngine::kIpm) {
        e.ipm.emplace();
    } else {
        e.sqp.emplace();
    }
    return e;
}

IterationRecord ipm_row(Index iteration, bool restoration) {
    IterationRecord r;
    r.ipm.emplace();
    r.iteration = iteration;
    r.tier = restoration ? IterationTier::kRestoration : IterationTier::kBarrier;
    r.recovering = restoration;
    r.f = restoration ? 0.0 : 17.5;
    r.stationarity = 0.5 / static_cast<double>(1 << iteration);
    r.feasibility = 0.25;
    r.complementarity = 0.125;
    r.step_norm = 0.5;
    r.accepted = true;
    r.factorizations = 3 * (iteration + 1);
    r.ipm->mu = 0.1;
    r.ipm->alpha_primal = 1.0;
    r.ipm->alpha_dual = 0.5;
    return r;
}

IterationRecord sqp_row(Index iteration, int depth) {
    IterationRecord r;
    r.sqp.emplace();
    r.iteration = iteration;
    r.tier = IterationTier::kWalk;
    r.f = -2.5;
    r.stationarity = 0.0625;
    r.feasibility = 0.25;
    r.complementarity = 0.125;
    r.step_norm = 0.75;
    r.accepted = depth == 0;
    r.factorizations = 7 + iteration;
    r.sqp->depth = depth;
    r.sqp->mode = QpTraceMode::kWalk;
    r.sqp->trust_radius = 2.0;
    r.sqp->violation_l1 = 1.5;
    r.sqp->qp_solved = true;
    return r;
}

std::string write_ipm_complete() {
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_solve_begin(begin(TraceEngine::kIpm));
    const IterationRecord a = ipm_row(0, false);
    const IterationRecord b = ipm_row(1, true);
    sink.on_iter(IterTraceEvent{a});
    sink.on_iter(IterTraceEvent{b});
    MessageTraceEvent m;
    m.engine = TraceEngine::kIpm;
    m.kind = MessageKind::kSolverInitialized;
    m.severity = MessageSeverity::kNotice;
    sink.on_message(m);
    const IpmCounters counters{};
    SolveEndTraceEvent e;
    e.engine = TraceEngine::kIpm;
    e.status = SolveStatus::kOptimal;
    e.message = "converged to the requested tolerances";
    e.iterations = 2;
    e.f = 17.5;
    e.ipm.emplace();
    e.ipm->counters = &counters;
    sink.on_solve_end(e);
    return os.str();
}

SolveEndTraceEvent sqp_end(const SqpCounters &counters, double f) {
    SolveEndTraceEvent e;
    e.engine = TraceEngine::kSqp;
    e.status = SolveStatus::kOptimal;
    e.message = "converged to the requested tolerances";
    e.iterations = 2;
    e.f = f;
    e.sqp.emplace();
    e.sqp->counters = &counters;
    return e;
}

// A depth-0 solve whose second row comes from a nested (depth-1) restoration sub-solve: the nested
// begin/end carry the enclosing solve's `sid` and `depth` 1.
std::string write_sqp_complete() {
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    const SqpCounters counters{};
    sink.on_solve_begin(begin(TraceEngine::kSqp));
    const IterationRecord a = sqp_row(0, 0);
    const IterationRecord b = sqp_row(1, 1);
    const IterationRecord c = sqp_row(2, 0);
    sink.on_iter(IterTraceEvent{a});
    sink.on_solve_begin(begin(TraceEngine::kSqp));
    sink.on_iter(IterTraceEvent{b});
    sink.on_solve_end(sqp_end(counters, -2.0));
    sink.on_iter(IterTraceEvent{c});
    sink.on_solve_end(sqp_end(counters, -2.5));
    return os.str();
}

std::string write_ipm_aborted() {
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_solve_begin(begin(TraceEngine::kIpm));
    const IterationRecord a = ipm_row(0, false);
    sink.on_iter(IterTraceEvent{a});
    return os.str();
}

void check(const char *file, const std::string &actual) {
    const std::filesystem::path path = std::filesystem::path(HVEN_TRACE_FIXTURE_DIR) / file;
    if (std::getenv("HVEN_REGEN_GOLDENS") != nullptr) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << actual;
        return;
    }
    std::ifstream in(path, std::ios::binary);
    ASSERT_TRUE(in) << "missing fixture " << path << " (regenerate with HVEN_REGEN_GOLDENS=1)";
    std::ostringstream expected;
    expected << in.rdbuf();
    EXPECT_EQ(actual, expected.str()) << file;
}

TEST(TraceV1Fixtures, IpmCompleteStream) { check("ipm_complete.jsonl", write_ipm_complete()); }
TEST(TraceV1Fixtures, SqpCompleteStream) { check("sqp_complete.jsonl", write_sqp_complete()); }
TEST(TraceV1Fixtures, IpmAbortedStreamHasNoEnd) {
    const std::string s = write_ipm_aborted();
    EXPECT_EQ(s.find("solve.end"), std::string::npos);
    check("ipm_aborted.jsonl", s);
}

} // namespace
