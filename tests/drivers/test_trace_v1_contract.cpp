// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_trace_v1_contract.cpp -- M8 TR-A: the trace-v1 contract on both engines.
//   V1  row timing (the step fields describe the step LEAVING the point; the exit rows have none;
//       the IPM callback event for point k+1 carries row k's step -- identity 5)
//   V2  the IPM step norm is written with only a sink attached (no callback)
//   V3  the IPM soc / watchdog / rolled_back flags against their counters
//   V4  the running factorization count
//   identities 1, 2, 3 and 5 on the four-problem battery
//   proof 3: counters, statuses and points are the same with nothing attached, with the JSON sink
//   and the kDetail console, and with a callback on top.

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <hven/core/solver_counters.h>
#include <hven/drivers/ipm_solver.h>
#include <hven/drivers/json_lines_trace_sink.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/model/problem.h>

#include "../common_support/console_capture.h"
#include "support/trace_battery.h"

namespace {

using namespace hven;
using namespace hven_drivers_tests;

enum class Engine { kIpm, kSqp };

const char *name(Engine e) { return e == Engine::kIpm ? "ipm" : "sqp"; }

struct Run {
    SolveStatus status = SolveStatus::kNumericalError;
    Index iterations = 0;
    Vec x;
    WorkCounters work;
    std::optional<IpmCounters> ipm_counters;
    std::optional<SqpCounters> sqp_counters;
    IpmConvergence ipm_convergence;
    SqpConvergence sqp_convergence;
    bool returned_best = false;
    std::vector<IterationEvent> events; // scalars only (the Eigen refs are not kept)
    std::vector<std::optional<IpmIterationInfo>> ipm_infos;
    std::vector<std::optional<SqpIterationInfo>> sqp_infos;
};

struct Attach {
    TraceSink *sink = nullptr;
    bool callback = false;
    IterationPayload payload = IterationPayload::kScalars;
    Verbosity verbosity = Verbosity::kSilent;
    std::FILE *console = nullptr;
};

Run run(Engine engine, const BatteryCase &c, const Attach &a) {
    Problem problem(c.make());
    Run out;
    const auto record = [&out](const IterationEvent &e) {
        IterationEvent copy = e;
        copy.x = Eigen::Ref<const Vec>(Vec());
        out.ipm_infos.push_back(e.ipm);
        out.sqp_infos.push_back(e.sqp);
        out.events.push_back(copy);
        return CallbackAction::kContinue;
    };
    if (engine == Engine::kIpm) {
        IpmOptions o;
        o.common.verbosity = a.verbosity;
        IpmSolver solver(o);
        solver.attach_trace(a.sink);
        if (a.callback) {
            solver.set_iteration_callback(record, a.payload);
        }
        const IpmResult r = solver.solve(problem, c.start());
        out.status = r.status;
        out.iterations = r.iterations;
        out.x = r.x;
        out.work = r.work;
        out.ipm_counters = r.counters;
        out.ipm_convergence = r.convergence;
        out.returned_best = r.returned_best_iterate;
    } else {
        SqpOptions o;
        o.common.verbosity = a.verbosity;
        SqpSolver solver(o);
        solver.attach_trace(a.sink);
        if (a.callback) {
            solver.set_iteration_callback(record, a.payload);
        }
        const SqpResult r = solver.solve(problem, c.start());
        out.status = r.status;
        out.iterations = r.iterations;
        out.x = r.x;
        out.work = r.work;
        out.sqp_counters = r.counters;
        out.sqp_convergence = r.convergence;
        out.returned_best = r.returned_best_iterate;
    }
    return out;
}

bool bit_equal(double a, double b) {
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

template <class Counters> void expect_same_counters(const Counters &a, const Counters &b);

template <> void expect_same_counters<IpmCounters>(const IpmCounters &a, const IpmCounters &b) {
#define HVEN_CMP(f, absent) EXPECT_EQ(a.f, b.f) << #f;
    HVEN_IPM_COUNTERS_FIELDS(HVEN_CMP)
#undef HVEN_CMP
}

template <> void expect_same_counters<SqpCounters>(const SqpCounters &a, const SqpCounters &b) {
#define HVEN_CMP(f, absent) EXPECT_EQ(a.f, b.f) << #f;
    HVEN_SQP_COUNTERS_FIELDS(HVEN_CMP)
#undef HVEN_CMP
#define HVEN_CMP(f, absent) EXPECT_EQ(a.ssn.f, b.ssn.f) << "ssn." #f;
    HVEN_SSN_COUNTERS_FIELDS(HVEN_CMP)
#undef HVEN_CMP
#define HVEN_CMP(f, absent) EXPECT_EQ(a.ipqp.f, b.ipqp.f) << "ipqp." #f;
    HVEN_IPQP_COUNTERS_FIELDS(HVEN_CMP)
#undef HVEN_CMP
}

void expect_same_run(const Run &a, const Run &b) {
    EXPECT_EQ(a.status, b.status);
    EXPECT_EQ(a.iterations, b.iterations);
    ASSERT_EQ(a.x.size(), b.x.size());
    for (Eigen::Index i = 0; i < a.x.size(); ++i) {
        EXPECT_TRUE(bit_equal(a.x[i], b.x[i])) << "x[" << i << "]";
    }
    if (a.ipm_counters) {
        expect_same_counters(*a.ipm_counters, *b.ipm_counters);
    }
    if (a.sqp_counters) {
        expect_same_counters(*a.sqp_counters, *b.sqp_counters);
    }
}

// --- identities 1, 2, 3 on the battery -------------------------------------

TEST(TraceV1Contract, TheIterLinesAreTheIterationsAndTheLastRowsAgreeWithTheResult) {
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        for (const BatteryCase &c : battery()) {
            SCOPED_TRACE(std::string(name(e)) + " " + c.name);
            CaptureSink sink;
            const Run r = run(e, c, {.sink = &sink});
            ASSERT_EQ(sink.begins.size(), 1U);
            ASSERT_EQ(sink.ends.size(), 1U);
            ASSERT_TRUE(sink.begins[0].ipm.has_value() == (e == Engine::kIpm));
            ASSERT_TRUE(sink.begins[0].sqp.has_value() == (e == Engine::kSqp));
            // Identity 1.
            EXPECT_EQ(sink.depth0_rows(), r.iterations);
            // Exactly one engine block per row, the right one.
            for (const CapturedRow &row : sink.rows) {
                EXPECT_EQ(row.record.ipm.has_value(), e == Engine::kIpm);
                EXPECT_EQ(row.record.sqp.has_value(), e == Engine::kSqp);
            }
            ASSERT_FALSE(sink.rows.empty());
            const IterationRecord *last = nullptr;
            for (const CapturedRow &row : sink.rows) {
                if (row.depth == 0) {
                    last = &row.record;
                }
            }
            ASSERT_NE(last, nullptr);
            // Identity 3.
            EXPECT_LE(last->factorizations, r.work.factorizations);
            // Identity 2: asserted where the result describes the last iterate. The IPM engine
            // measures its row and its result in one place; the SQP result re-measures on the
            // caller's model, so it is compared to a tolerance.
            if (!r.returned_best) {
                if (e == Engine::kIpm) {
                    EXPECT_EQ(last->feasibility, r.ipm_convergence.feasibility);
                    EXPECT_EQ(last->stationarity, r.ipm_convergence.stationarity);
                    EXPECT_EQ(last->complementarity, r.ipm_convergence.complementarity);
                } else {
                    const double tol = 1e-9;
                    EXPECT_NEAR(last->stationarity, r.sqp_convergence.stationarity,
                                tol * (1.0 + std::abs(r.sqp_convergence.stationarity)));
                    EXPECT_NEAR(last->feasibility, r.sqp_convergence.feasibility,
                                tol * (1.0 + std::abs(r.sqp_convergence.feasibility)));
                }
            }
        }
    }
}

// --- identity 5 (SQP) and V1 (IPM) -------------------------------------------

TEST(TraceV1Contract, SqpEventAndRowAgreeRowForRow) {
    for (const BatteryCase &c : battery()) {
        SCOPED_TRACE(c.name);
        CaptureSink sink;
        const Run r = run(Engine::kSqp, c, {.sink = &sink, .callback = true});
        std::vector<const IterationRecord *> d0;
        for (const CapturedRow &row : sink.rows) {
            if (row.depth == 0) {
                d0.push_back(&row.record);
            }
        }
        ASSERT_EQ(r.events.size(), d0.size()) << "one event per depth-0 row";
        for (std::size_t k = 0; k < d0.size(); ++k) {
            const IterationRecord &row = *d0[k];
            const IterationEvent &ev = r.events[k];
            EXPECT_EQ(ev.iteration, row.iteration);
            EXPECT_TRUE(bit_equal(ev.stationarity, row.stationarity));
            EXPECT_TRUE(bit_equal(ev.complementarity, row.complementarity));
            if (row.step_norm) {
                EXPECT_TRUE(bit_equal(ev.step_norm, *row.step_norm));
            }
            ASSERT_TRUE(r.sqp_infos[k].has_value());
            const SqpIterationInfo &info = *r.sqp_infos[k];
            ASSERT_TRUE(row.sqp.has_value());
            EXPECT_EQ(info.qp_solved, row.sqp->qp_solved);
            EXPECT_TRUE(bit_equal(info.trust_radius, row.sqp->trust_radius));
            EXPECT_EQ(info.elastic, row.sqp->elastic);
            EXPECT_EQ(info.active_set_size, row.sqp->active_set_size);
        }
    }
}

TEST(TraceV1Contract, V1TheIpmRowIsThePointPlusTheStepLeavingItAndTheEventRunsOneRowBehind) {
    for (const BatteryCase &c : battery()) {
        SCOPED_TRACE(c.name);
        CaptureSink sink;
        const Run r = run(Engine::kIpm, c, {.sink = &sink, .callback = true});
        std::vector<const IterationRecord *> rows;
        for (const CapturedRow &row : sink.rows) {
            rows.push_back(&row.record);
        }
        ASSERT_EQ(r.events.size(), rows.size()) << "one event per row";
        ASSERT_GE(rows.size(), 2U);
        // The final row of an Optimal solve is reached by a step and has none leaving it.
        ASSERT_EQ(r.status, SolveStatus::kOptimal);
        const IterationRecord &final_row = *rows.back();
        EXPECT_FALSE(final_row.step_norm.has_value());
        EXPECT_FALSE(final_row.accepted.has_value());
        ASSERT_TRUE(final_row.ipm.has_value());
        EXPECT_FALSE(final_row.ipm->alpha_primal.has_value());
        EXPECT_FALSE(final_row.ipm->line_search_trials.has_value());
        EXPECT_FALSE(final_row.ipm->barrier_objective.has_value());
        EXPECT_FALSE(final_row.ipm->merit.has_value());
        // The event for point k+1 carries row k's step fields.
        for (std::size_t k = 0; k + 1 < rows.size(); ++k) {
            const IterationRecord &row = *rows[k];
            const IterationEvent &next = r.events[k + 1];
            ASSERT_TRUE(r.ipm_infos[k + 1].has_value());
            const IpmIterationInfo &info = *r.ipm_infos[k + 1];
            if (next.iteration == 0 || !row.step_norm) {
                continue; // a phase's first event has no step, and a row without one has none to
                          // carry
            }
            ASSERT_TRUE(row.ipm->alpha_primal.has_value());
            EXPECT_TRUE(bit_equal(next.step_norm, *row.step_norm)) << "row " << k;
            // The row's alpha is the effective length (alpha_scale * the fraction-to-boundary
            // maximum the event reports): the one place the two differ, by that factor.
            const double scale = *row.ipm->alpha_scale;
            EXPECT_NEAR(info.alpha_primal * scale, *row.ipm->alpha_primal, 1e-15) << "row " << k;
            EXPECT_NEAR(info.alpha_dual * scale, *row.ipm->alpha_dual, 1e-15) << "row " << k;
            EXPECT_EQ(info.line_search_trials, *row.ipm->line_search_trials);
            if (row.ipm->inertia_corrections) {
                EXPECT_EQ(info.inertia_corrections, *row.ipm->inertia_corrections);
            }
            if (row.ipm->regularization_primal) {
                EXPECT_TRUE(bit_equal(info.regularization_primal, *row.ipm->regularization_primal));
            }
        }
    }
}

TEST(TraceV1Contract, V2TheIpmStepNormIsWrittenWithOnlyASinkAttached) {
    for (const BatteryCase &c : battery()) {
        SCOPED_TRACE(c.name);
        CaptureSink sink;
        (void)run(Engine::kIpm, c, {.sink = &sink});
        int stepped = 0;
        for (const CapturedRow &row : sink.rows) {
            if (row.record.step_norm) {
                EXPECT_TRUE(std::isfinite(*row.record.step_norm));
                EXPECT_GE(*row.record.step_norm, 0.0);
                ++stepped;
            }
        }
        EXPECT_GT(stepped, 0) << "non-vacuous: some row has a step leaving it";
    }
}

// --- V3: the flags against their counters --------------------------------------

struct Maratos final : hven::Nlp {
    hven::NlpDimensions dimensions() const override { return {2, 1, 2, 2}; }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        const double inf = std::numeric_limits<double>::infinity();
        xl.setConstant(-inf);
        xu.setConstant(inf);
        gl << 0.0;
        gu << 0.0;
    }
    double eval_f(hven::ConstVecRef x) const override {
        return 2.0 * (x[0] * x[0] + x[1] * x[1] - 1.0) - x[0];
    }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override {
        g << 4.0 * x[0] - 1.0, 4.0 * x[1];
    }
    void eval_g(hven::ConstVecRef x, hven::VecRef g) const override {
        g << x[0] * x[0] + x[1] * x[1] - 1.0;
    }
    void jacobian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void eval_jacobian(hven::ConstVecRef x, hven::VecRef v) const override {
        v << 2.0 * x[0], 2.0 * x[1];
    }
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_hessian(hven::ConstVecRef, double obj_factor, hven::ConstVecRef lambda,
                      hven::VecRef v) const override {
        v << 4.0 * obj_factor + 2.0 * lambda[0], 4.0 * obj_factor + 2.0 * lambda[0];
    }
    std::string name() const override { return "Maratos"; }
};

TEST(TraceV1Contract, V3TheSocFlagIsTheRowsWhoseStepASecondOrderCorrectionWon) {
    IpmOptions o;
    o.advanced.max_second_order_corrections = 4;
    IpmSolver solver(o);
    CaptureSink sink;
    solver.attach_trace(&sink);
    Problem problem(std::make_shared<Maratos>());
    Vec x0(2);
    x0 << std::cos(0.5), std::sin(0.5);
    const IpmResult r = solver.solve(problem, x0);
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    ASSERT_GT(r.counters.soc_steps, 0) << "non-vacuous: the correction ran";
    Index soc_rows = 0;
    Index watchdog_rows = 0;
    Index rolled_back_rows = 0;
    for (const CapturedRow &row : sink.rows) {
        soc_rows += row.record.soc ? 1 : 0;
        watchdog_rows += row.record.watchdog ? 1 : 0;
        rolled_back_rows += row.record.rolled_back ? 1 : 0;
    }
    EXPECT_GT(soc_rows, 0);
    EXPECT_EQ(soc_rows, r.counters.recovery_depth_histogram[0]) << "SOC resolved those rejections";
    EXPECT_LE(soc_rows, r.counters.soc_steps);
    EXPECT_LE(watchdog_rows, r.counters.watchdog_activations);
    EXPECT_LE(rolled_back_rows, r.counters.nonfinite_step_rollbacks);
}

// --- V4: the running factorization count ----------------------------------------

TEST(TraceV1Contract, V4TheFactorizationCountIsMonotoneAndBoundedByTheWork) {
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        for (const BatteryCase &c : battery()) {
            SCOPED_TRACE(std::string(name(e)) + " " + c.name);
            CaptureSink sink;
            const Run r = run(e, c, {.sink = &sink});
            Index prev = 0;
            for (const CapturedRow &row : sink.rows) {
                if (row.depth != 0) {
                    continue;
                }
                EXPECT_GE(row.record.factorizations, prev);
                prev = row.record.factorizations;
            }
            EXPECT_GT(prev, 0);
            EXPECT_LE(prev, r.work.factorizations);
        }
    }
}

// --- proof 3: observation changes no counter ---------------------------------------

TEST(TraceV1Contract, ObservationChangesNoCounterStatusOrPoint) {
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        for (const BatteryCase &c : battery()) {
            SCOPED_TRACE(std::string(name(e)) + " " + c.name);
            const Run bare = run(e, c, {});

            std::ostringstream json;
            JsonLinesTraceSink sink(json);
            std::string console_text;
            Run traced;
            {
                // The kDetail console writes to stdout; capture it so the run is quiet.
                hven::testing::StdoutCapture capture;
                traced = run(e, c, {.sink = &sink, .verbosity = Verbosity::kDetail});
                console_text = capture.text();
            }
            expect_same_run(bare, traced);
            EXPECT_GT(sink.lines_written(), 0);
            EXPECT_NE(console_text.find("exit "), std::string::npos) << console_text;

            std::ostringstream json2;
            JsonLinesTraceSink sink2(json2);
            const Run both =
                run(e, c, {.sink = &sink2, .callback = true, .payload = IterationPayload::kAll});
            expect_same_run(bare, both);
            EXPECT_EQ(json.str(), json2.str())
                << "the console and the callback change no byte of the trace";
        }
    }
}

// --- the JSON stream's own shape ------------------------------------------------------

TEST(TraceV1Contract, TheStreamIsV1WithContiguousSeqAndOneSidPerSolve) {
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        const BatteryCase &c = battery()[2];
        SCOPED_TRACE(name(e));
        std::ostringstream json;
        JsonLinesTraceSink sink(json);
        const Run r1 = run(e, c, {.sink = &sink});
        const Run r2 = run(e, c, {.sink = &sink});
        std::istringstream in(json.str());
        std::string line;
        long long seq = 0;
        long long iter_lines_sid1 = 0;
        long long iter_lines_sid2 = 0;
        int begins = 0;
        int ends = 0;
        while (std::getline(in, line)) {
            ++seq;
            ASSERT_EQ(line.rfind("{\"v\":1,\"ev\":\"", 0), 0U) << line;
            EXPECT_NE(line.find("\"seq\":" + std::to_string(seq) + ",\"sid\":"), std::string::npos)
                << line;
            EXPECT_NE(line.find(std::string("\"eng\":\"") + name(e) + "\""), std::string::npos)
                << line;
            EXPECT_EQ(line.find("unknown"), std::string::npos) << line;
            const bool sid1 = line.find(",\"sid\":1,") != std::string::npos;
            const bool sid2 = line.find(",\"sid\":2,") != std::string::npos;
            EXPECT_TRUE(sid1 || sid2) << line;
            if (line.find("\"ev\":\"iter\"") != std::string::npos) {
                (sid1 ? iter_lines_sid1 : iter_lines_sid2) += 1;
            }
            begins += line.find("\"ev\":\"solve.begin\"") != std::string::npos ? 1 : 0;
            ends += line.find("\"ev\":\"solve.end\"") != std::string::npos ? 1 : 0;
        }
        EXPECT_EQ(begins, 2);
        EXPECT_EQ(ends, 2);
        EXPECT_EQ(iter_lines_sid1, r1.iterations);
        EXPECT_EQ(iter_lines_sid2, r2.iterations);
    }
}

} // namespace
