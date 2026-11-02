// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Standalone install-tree smoke consumer for hven's install/export rules.
// Not part of hven's own build (see this directory's CMakeLists.txt) --
// built against an *installed* hven package via find_package(hven), the
// same way an external consumer would. Solves the canonical Ipopt HS071
// example (borrowed from tests/interior/test_ipm_solver_entry.cpp) and checks
// the known optimum, so a green run proves both link-time (libhven.a's symbols
// actually resolve) and run-time (the sparse backend the installed package
// re-found is the real one) correctness -- not just "it configured".
//
// M6 W5 T8.9: THE CONSUMER IS GENERIC AND COMPILES AGAINST BOTH ENGINES.
// `run_once` below is written once, against the SHARED SHAPE W5 T8 gives the
// two solvers -- construction, `options()`/`set_options`, the solve family with
// and without a warm-start payload, `set_iteration_callback`, `attach_trace`,
// `attach_ledger`, and `export_warm_start()` off the returned result -- and is
// instantiated for `IpmSolver` and for `SqpSolver`, both over ONE `Problem`
// (M8 AP-2 B2: the one problem type both engines solve). A shape that has
// drifted apart on one engine fails to COMPILE here, against an installed
// prefix, which is the whole point of the exercise.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

#include "hven/core/ledger.h"
#include "hven/core/solver_status.h"
#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/solve_result.h"
#include "hven/drivers/solve_status.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/drivers/trace_writer.h"
#include "hven/model/nlp.h"
#include "hven/model/problem.h"
#include "hven/warmstart/warm_start_data.h"

namespace {
constexpr double kInf = std::numeric_limits<double>::infinity();

struct Hs071Problem : hven::solvers::Nlp {
    hven::solvers::NlpDimensions dimensions() const override {
        return {.n = 4, .m = 2, .jacobian_nonzeros = 8, .hessian_nonzeros = 10};
    }

    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        xl << 1.0, 1.0, 1.0, 1.0;
        xu << 5.0, 5.0, 5.0, 5.0;
        gl << 25.0, 40.0;
        gu << kInf, 40.0;
    }
    double eval_f(hven::ConstVecRef x) const override {
        return x[0] * x[3] * (x[0] + x[1] + x[2]) + x[2];
    }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override {
        g[0] = x[3] * (2.0 * x[0] + x[1] + x[2]);
        g[1] = x[0] * x[3];
        g[2] = x[0] * x[3] + 1.0;
        g[3] = x[0] * (x[0] + x[1] + x[2]);
    }
    void eval_g(hven::ConstVecRef x, hven::VecRef g) const override {
        g[0] = x[0] * x[1] * x[2] * x[3];
        g[1] = x[0] * x[0] + x[1] * x[1] + x[2] * x[2] + x[3] * x[3];
    }
    void jacobian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 0, 0, 0, 1, 1, 1, 1;
        c << 0, 1, 2, 3, 0, 1, 2, 3;
    }
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 1, 1, 2, 2, 2, 3, 3, 3, 3;
        c << 0, 0, 1, 0, 1, 2, 0, 1, 2, 3;
    }
    void eval_jacobian(hven::ConstVecRef x, hven::VecRef v) const override {
        v[0] = x[1] * x[2] * x[3];
        v[1] = x[0] * x[2] * x[3];
        v[2] = x[0] * x[1] * x[3];
        v[3] = x[0] * x[1] * x[2];
        v[4] = 2.0 * x[0];
        v[5] = 2.0 * x[1];
        v[6] = 2.0 * x[2];
        v[7] = 2.0 * x[3];
    }
    void eval_hessian(hven::ConstVecRef x, double obj_factor, hven::ConstVecRef lambda,
                      hven::VecRef v) const override {
        const double x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3];
        v[0] = obj_factor * 2 * x3 + lambda[1] * 2;
        v[1] = obj_factor * x3 + lambda[0] * x2 * x3;
        v[2] = lambda[1] * 2;
        v[3] = obj_factor * x3 + lambda[0] * x1 * x3;
        v[4] = lambda[0] * x0 * x3;
        v[5] = lambda[1] * 2;
        v[6] = obj_factor * (2 * x0 + x1 + x2) + lambda[0] * x1 * x2;
        v[7] = obj_factor * x0 + lambda[0] * x0 * x2;
        v[8] = obj_factor * x0 + lambda[0] * x0 * x1;
        v[9] = lambda[1] * 2;
    }
    std::string name() const override { return "Hs071Problem"; }
};

/// @brief The known optimum's objective value.
constexpr double kHs071Optimum = 17.0140172;

/// @brief The whole shared shape, exercised once, on whichever engine.
///
/// Written against the SHAPE and not against either engine: every call below is
/// spelled the same way on `IpmSolver` and on `SqpSolver`, which is
/// what W5 T8 set out to make true and what this instantiation pair checks from
/// outside the project.
///
/// @param solver The engine, already constructed with its own options type.
/// @param problem The problem both engines solve.
/// @param x0     The start point.
/// @param label  Names the arm in the diagnostics this prints.
/// @return true when the arm converged to the known optimum.
template <class Solver>
bool run_once(Solver &solver, hven::solvers::Problem &problem, const Eigen::VectorXd &x0,
              const char *label) {
    // (1) options() / set_options: read the value in force, edit it, hand it
    //     back. Print level 10 is silent under the convention both engines now
    //     share.
    auto opts = solver.options();
    opts.common.verbosity = hven::solvers::Verbosity::kSilent;
    solver.set_options(std::move(opts));

    // (2) the per-iteration callback, on both engines, with the same event type
    //     and the same continue/stop verdict.
    long long iterations_seen = 0;
    solver.set_iteration_callback([&iterations_seen](const hven::solvers::IterationEvent &) {
        ++iterations_seen;
        return hven::solvers::CallbackAction::kContinue;
    });

    // (3) attach_trace and attach_ledger, both public, both the same call.
    std::ostringstream trace;
    hven::solvers::JsonLinesTraceSink sink(trace);
    solver.attach_trace(&sink);
    hven::solvers::Ledger ledger;
    solver.attach_ledger(&ledger, std::string(label));

    // (4) the COLD solve, and its result by value.
    const auto cold = solver.solve(problem, x0);
    if (cold.status != hven::solvers::SolveStatus::kOptimal) {
        std::fprintf(stderr, "install smoke: %s did not converge (status %s)\n", label,
                     hven::solvers::to_string(cold.status));
        return false;
    }
    if (iterations_seen <= 0) {
        std::fprintf(stderr, "install smoke: %s installed a callback that never fired\n", label);
        return false;
    }
    if (trace.str().empty()) {
        std::fprintf(stderr, "install smoke: %s wrote no trace\n", label);
        return false;
    }

    // (5) export_warm_start() off the result, and the PAYLOAD solve that takes
    //     it back -- one currency, one entry, on both engines.
    const std::optional<hven::solvers::WarmStartData> payload = cold.export_warm_start();
    if (!payload.has_value()) {
        std::fprintf(stderr, "install smoke: %s exported no warm start\n", label);
        return false;
    }
    solver.clear_iteration_callback();
    solver.attach_trace(nullptr);
    const auto warm = solver.solve(problem, x0, *payload);
    if (warm.status != hven::solvers::SolveStatus::kOptimal) {
        std::fprintf(stderr, "install smoke: %s did not converge from its own warm start\n", label);
        return false;
    }

    // (6) the ANSWER, checked against the known optimum on both arms.
    const double f_actual = Hs071Problem{}.eval_f(warm.x);
    if (std::abs(f_actual - kHs071Optimum) > 1e-5) {
        std::fprintf(stderr,
                     "install smoke: %s objective %.7f does not match the known optimum %.7f\n",
                     label, f_actual, kHs071Optimum);
        return false;
    }
    std::printf("install smoke: %s converged, objective = %.7f (OK)\n", label, f_actual);
    return true;
}
} // namespace

// The standalone-include TUs (M6 W5 T0; nlp_solver joined at T1 fix1,
// ipqp_evidence at T4, compiler at T3, solve_status at T8.2, common_options and
// ipm_solver_types at T8.3, console_trace_sink at T8.7; at T8.9 nlp_solver went
// with the retired wrapper and the TWO ENGINE HEADERS took its place, 17 -> 18;
// make_nlp_program at M7 W0 T5(a), 18 -> 19; at M8 AP-2 B2 the three model-layer
// TUs became nlp, problem and fixed_variable_treatment, 19 -> 19; at M8 AP-2 B3
// fixed_variable_treatment left and the six options-break headers joined, 19 -> 24).
// Each proves COMPILE-TIME self-containment only: none odr-uses anything its
// header declares, so the link proves the objects link, nothing about exports.
namespace hven_install_smoke {
int standalone_include_trace_writer();
int standalone_include_trace();
int standalone_include_ipqp_evidence();
int standalone_include_nlp_problem();
int standalone_include_problem();
int standalone_include_nlp_aggregate();
int standalone_include_aggregate_declaration();
int standalone_include_ipm_options();
int standalone_include_sqp_options();
int standalone_include_ipm_enums();
int standalone_include_ipm_phase_report();
int standalone_include_step_verdict();
int standalone_include_dense_policy();
int standalone_include_interior_point_solver();
int standalone_include_sqp_driver();
int standalone_include_solve_status();
int standalone_include_core_solver_status();
int standalone_include_common_options();
int standalone_include_ipm_solver_types();
int standalone_include_solve_result();
int standalone_include_sqp_warm_start();
int standalone_include_seeding();
int standalone_include_console_trace_sink();
int standalone_include_compiler();
} // namespace hven_install_smoke

int main() {
    const int standalone_tus = hven_install_smoke::standalone_include_trace_writer() +
                               hven_install_smoke::standalone_include_trace() +
                               hven_install_smoke::standalone_include_ipqp_evidence() +
                               hven_install_smoke::standalone_include_nlp_problem() +
                               hven_install_smoke::standalone_include_problem() +
                               hven_install_smoke::standalone_include_nlp_aggregate() +
                               hven_install_smoke::standalone_include_aggregate_declaration() +
                               hven_install_smoke::standalone_include_ipm_options() +
                               hven_install_smoke::standalone_include_sqp_options() +
                               hven_install_smoke::standalone_include_ipm_enums() +
                               hven_install_smoke::standalone_include_ipm_phase_report() +
                               hven_install_smoke::standalone_include_step_verdict() +
                               hven_install_smoke::standalone_include_dense_policy() +
                               hven_install_smoke::standalone_include_interior_point_solver() +
                               hven_install_smoke::standalone_include_sqp_driver() +
                               hven_install_smoke::standalone_include_solve_status() +
                               hven_install_smoke::standalone_include_core_solver_status() +
                               hven_install_smoke::standalone_include_common_options() +
                               hven_install_smoke::standalone_include_ipm_solver_types() +
                               hven_install_smoke::standalone_include_solve_result() +
                               hven_install_smoke::standalone_include_sqp_warm_start() +
                               hven_install_smoke::standalone_include_seeding() +
                               hven_install_smoke::standalone_include_console_trace_sink() +
                               hven_install_smoke::standalone_include_compiler();
    if (standalone_tus != 24) {
        std::fprintf(stderr, "install smoke: %d standalone-include TUs linked, expected 24\n",
                     standalone_tus);
        return 1;
    }

    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;

    // ONE PROBLEM, TWO ENGINES. Both solvers take the same Problem; everything
    // after that is `run_once`, written once.
    hven::solvers::Problem problem(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver ipm;
    if (!run_once(ipm, problem, x0, "IpmSolver")) {
        return 1;
    }

    hven::solvers::SqpSolver sqp;
    if (!run_once(sqp, problem, x0, "SqpSolver")) {
        return 1;
    }

    std::printf("install smoke: both solvers converged through one generic consumer; "
                "%d standalone-include TUs compiled and linked\n",
                standalone_tus);
    return 0;
}
