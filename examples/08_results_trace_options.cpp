// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Example 8: reading results and diagnostics, attaching a trace sink, dumping
// options.
//
// Solves HS071 (see example 1) on both engines and shows where the information
// lives:
//   * the result: status, point, the three kinds of multiplier, the objective,
//     the four diagnostics both engines report the same way, the iteration
//     count, and the engine-specific extras (IpmResult / SqpResult);
//   * an iteration callback, which sees every iterate and may stop the solve;
//   * a trace sink, which receives the engines' structured events -- here
//     JsonLinesTraceSink writing to a string, whose lines are tallied by event;
//   * dump_options(), which prints every option in force, one per line.

#include <cmath>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>

#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/drivers/trace_writer.h"
#include "hven/model/nlp.h"
#include "hven/model/problem.h"

namespace {

using namespace hven::solvers;
using hven::ConstVecRef;
using hven::IndexVecRef;
using hven::kInfinity;
using hven::VecRef;

struct Hs071 : Nlp {
    NlpDimensions dimensions() const override {
        return {.n = 4, .m = 2, .jacobian_nonzeros = 8, .hessian_nonzeros = 10};
    }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl.setConstant(1.0);
        xu.setConstant(5.0);
        gl << 25.0, 40.0; // row 0: x0*x1*x2*x3 >= 25 ; row 1: sum x^2 == 40
        gu << kInfinity, 40.0;
    }
    double eval_f(ConstVecRef x) const override {
        return x[0] * x[3] * (x[0] + x[1] + x[2]) + x[2];
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g << x[3] * (2 * x[0] + x[1] + x[2]), x[0] * x[3], x[0] * x[3] + 1,
            x[0] * (x[0] + x[1] + x[2]);
    }
    void eval_g(ConstVecRef x, VecRef g) const override {
        g << x[0] * x[1] * x[2] * x[3], x.squaredNorm();
    }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 0, 0, 0, 1, 1, 1, 1;
        c << 0, 1, 2, 3, 0, 1, 2, 3;
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override {
        v << x[1] * x[2] * x[3], x[0] * x[2] * x[3], x[0] * x[1] * x[3], x[0] * x[1] * x[2],
            2 * x[0], 2 * x[1], 2 * x[2], 2 * x[3];
    }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 1, 1, 2, 2, 2, 3, 3, 3, 3; // lower triangle
        c << 0, 0, 1, 0, 1, 2, 0, 1, 2, 3;
    }
    void eval_hessian(ConstVecRef x, double obj_factor, ConstVecRef lambda,
                      VecRef v) const override {
        v << obj_factor * 2 * x[3] + lambda[1] * 2,                          // (0,0)
            obj_factor * x[3] + lambda[0] * x[2] * x[3],                     // (1,0)
            lambda[1] * 2,                                                   // (1,1)
            obj_factor * x[3] + lambda[0] * x[1] * x[3],                     // (2,0)
            lambda[0] * x[0] * x[3],                                         // (2,1)
            lambda[1] * 2,                                                   // (2,2)
            obj_factor * (2 * x[0] + x[1] + x[2]) + lambda[0] * x[1] * x[2], // (3,0)
            obj_factor * x[0] + lambda[0] * x[0] * x[2],                     // (3,1)
            obj_factor * x[0] + lambda[0] * x[0] * x[1],                     // (3,2)
            lambda[1] * 2;                                                   // (3,3)
    }
};

// The common readout: everything SolveResult carries, for either engine.
void print_result(const char *engine, const SolveResult &r) {
    std::printf("--- %s ---\n", engine);
    std::printf("status          %s\n", to_string(r.status));
    std::printf("message         %s\n", r.message.c_str());
    std::printf("work            f=%lld grad=%lld g=%lld jac=%lld hess=%lld factorizations=%lld\n",
                static_cast<long long>(r.work.objective_evals),
                static_cast<long long>(r.work.gradient_evals),
                static_cast<long long>(r.work.constraint_evals),
                static_cast<long long>(r.work.jacobian_evals),
                static_cast<long long>(r.work.hessian_evals),
                static_cast<long long>(r.work.factorizations));
    std::printf("iterations      %lld\n", static_cast<long long>(r.iterations));
    std::printf("objective       %.9f\n", r.f);
    std::cout << "x               " << r.x.transpose() << "\n";
    std::cout << "lambda          " << r.lambda.transpose() << "   (one per row)\n";
    std::cout << "g               " << r.g.transpose() << "   (row values)\n";
    std::cout << "z (bounds)      " << r.z.transpose() << "\n";
    std::printf("stationarity    %.2e\n", r.stationarity);
    std::printf("feasibility     %.2e\n", r.feasibility);
    std::printf("complementarity %.2e\n", r.complementarity);
    std::printf("wall seconds    %.4f (informational)\n", r.wall_seconds);
}

// Tally a JSON-lines trace by its "ev" field.
std::map<std::string, int> tally(const std::string &trace) {
    std::map<std::string, int> counts;
    std::istringstream lines(trace);
    for (std::string line; std::getline(lines, line);) {
        const std::size_t at = line.find("\"ev\":\"");
        if (at != std::string::npos) {
            const std::size_t begin = at + 6;
            counts[line.substr(begin, line.find('"', begin) - begin)]++;
        }
    }
    return counts;
}

} // namespace

int main() {
    Problem problem(std::make_shared<Hs071>());
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;

    // ---- Interior-point engine: callback + trace ----
    IpmOptions ipm_options;
    std::printf("=== IpmOptions (first lines of dump_options) ===\n");
    {
        std::ostringstream dump;
        dump_options(ipm_options, dump);
        std::istringstream lines(dump.str());
        int shown = 0;
        for (std::string line; shown < 6 && std::getline(lines, line); shown++) {
            std::printf("%s\n", line.c_str());
        }
    }
    IpmSolver ipm(ipm_options);

    std::ostringstream ipm_trace;
    JsonLinesTraceSink ipm_sink(ipm_trace);
    ipm.attach_trace(&ipm_sink);
    int ipm_callbacks = 0;
    // The mask says what the callback reads; scalars are always delivered.
    ipm.set_iteration_callback(
        [&](const IterationEvent &) {
            ipm_callbacks++;
            return CallbackAction::kContinue;
        },
        IterationPayload::kScalars);

    const IpmResult ipm_result = ipm.solve(problem, x0);
    print_result("IpmSolver", ipm_result);
    // Engine-specific extras live on IpmResult.
    std::printf("kkt_inf         %.2e   (the interior-point engine's own measure)\n",
                ipm_result.convergence.kkt);
    std::printf("callbacks seen  %d\n", ipm_callbacks);

    // ---- SQP engine: callback + trace + counters ----
    SqpOptions sqp_options;
    std::printf("=== SqpOptions (first lines of dump_options) ===\n");
    {
        std::ostringstream dump;
        dump_options(sqp_options, dump);
        std::istringstream lines(dump.str());
        int shown = 0;
        for (std::string line; shown < 6 && std::getline(lines, line); shown++) {
            std::printf("%s\n", line.c_str());
        }
    }
    SqpSolver sqp(sqp_options);
    std::ostringstream sqp_trace;
    JsonLinesTraceSink sqp_sink(sqp_trace);
    sqp.attach_trace(&sqp_sink);
    int sqp_callbacks = 0;
    sqp.set_iteration_callback([&](const IterationEvent &e) {
        // depth 0 is the caller's own problem; deeper rows belong to restoration.
        if (e.depth.value_or(0) == 0) {
            sqp_callbacks++;
        }
        return CallbackAction::kContinue;
    });
    const SqpResult sqp_result = sqp.solve(problem, x0);
    print_result("SqpSolver", sqp_result);
    std::printf("sqp_stationarity %.2e   (the SQP engine's own measure)\n",
                sqp_result.convergence.stationarity);
    std::printf("callbacks seen  %d\n", sqp_callbacks);
    std::printf("counters        major=%lld factorizations=%lld eval_hess_calls=%lld\n",
                static_cast<long long>(sqp_result.counters.major_iters),
                static_cast<long long>(sqp_result.counters.factorizations),
                static_cast<long long>(sqp_result.counters.eval_hess_calls));

    // ---- The traces ----
    ipm_sink.flush();
    sqp_sink.flush();
    for (const auto &[name, trace] : {std::pair{"ipm", &ipm_trace}, std::pair{"sqp", &sqp_trace}}) {
        std::printf("trace (%s):", name);
        for (const auto &[ev, count] : tally(trace->str())) {
            std::printf(" %s=%d", ev.c_str(), count);
        }
        std::printf("\n");
    }

    const bool ok = ipm_result.status == SolveStatus::kOptimal &&
                    sqp_result.status == SolveStatus::kOptimal &&
                    std::abs(ipm_result.f - 17.0140173) < 1e-5 &&
                    std::abs(sqp_result.f - 17.0140173) < 1e-5 && ipm_callbacks > 0 &&
                    sqp_callbacks > 0 && !ipm_trace.str().empty() && !sqp_trace.str().empty();
    std::printf("%s\n", ok ? "OK" : "FAILED");
    return ok ? 0 : 1;
}
