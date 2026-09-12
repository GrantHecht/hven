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
// instantiated for `IpmSolver` over a `NonLinearProgram` and for
// `SqpSolver` over the same problem's `NlpProblemModel`. A shape that has
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
#include "hven/detail/model/nlp_adapter.h"
#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/solve_result.h"
#include "hven/drivers/solve_status.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/drivers/trace_writer.h"
#include "hven/model/nlp_problem_model.h"
#include "hven/model/nlp_triplet_model.h"
#include "hven/model/non_linear_program.h"
#include "hven/warmstart/warm_start_data.h"

namespace {
constexpr double kInf = std::numeric_limits<double>::infinity();

struct Hs071Problem : hven::solvers::NlpTripletModel {
    int num_vars() const override { return 4; }
    int num_cons() const override { return 2; }
    int num_jac_nonzeros() const override { return 8; }
    int num_hess_nonzeros() const override { return 10; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << 1.0, 1.0, 1.0, 1.0;
        xu << 5.0, 5.0, 5.0, 5.0;
        gl << 25.0, 40.0;
        gu << kInf, 40.0;
    }
    void eval_f(hven::ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = x[0] * x[3] * (x[0] + x[1] + x[2]) + x[2];
    }
    void eval_grad_f(hven::ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[3] * (2.0 * x[0] + x[1] + x[2]);
        g[1] = x[0] * x[3];
        g[2] = x[0] * x[3] + 1.0;
        g[3] = x[0] * (x[0] + x[1] + x[2]);
    }
    void eval_g(hven::ConstEigenRef<Eigen::VectorXd> x,
                Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] * x[1] * x[2] * x[3];
        g[1] = x[0] * x[0] + x[1] * x[1] + x[2] * x[2] + x[3] * x[3];
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0, 0, 0, 1, 1, 1, 1;
        c << 0, 1, 2, 3, 0, 1, 2, 3;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 1, 2, 2, 2, 3, 3, 3, 3;
        c << 0, 0, 1, 0, 1, 2, 0, 1, 2, 3;
    }
    void eval_jac(hven::ConstEigenRef<Eigen::VectorXd> x,
                  Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = x[1] * x[2] * x[3];
        v[1] = x[0] * x[2] * x[3];
        v[2] = x[0] * x[1] * x[3];
        v[3] = x[0] * x[1] * x[2];
        v[4] = 2.0 * x[0];
        v[5] = 2.0 * x[1];
        v[6] = 2.0 * x[2];
        v[7] = 2.0 * x[3];
    }
    void eval_hess(hven::ConstEigenRef<Eigen::VectorXd> x, double obj_factor,
                   hven::ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
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
/// @param model  The model that engine consumes.
/// @param x0     The start point.
/// @param label  Names the arm in the diagnostics this prints.
/// @return true when the arm converged to the known optimum.
template <class Solver, class Model>
bool run_once(Solver &solver, Model &model, const Eigen::VectorXd &x0, const char *label) {
    // (1) options() / set_options: read the value in force, edit it, hand it
    //     back. Print level 10 is silent under the convention both engines now
    //     share.
    auto opts = solver.options();
    opts.common.print_level = 10;
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
    const auto cold = solver.solve(model, x0);
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
    const auto warm = solver.solve(model, x0, *payload);
    if (warm.status != hven::solvers::SolveStatus::kOptimal) {
        std::fprintf(stderr, "install smoke: %s did not converge from its own warm start\n", label);
        return false;
    }

    // (6) the ANSWER, checked against the known optimum on both arms.
    double f_actual = 0.0;
    Hs071Problem{}.eval_f(warm.x, f_actual);
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
// with the retired wrapper and the TWO ENGINE HEADERS took its place, 17 -> 18).
// Each proves COMPILE-TIME self-containment only: none odr-uses anything its
// header declares, so the link proves the objects link, nothing about exports.
namespace hven_install_smoke {
int standalone_include_trace_writer();
int standalone_include_trace();
int standalone_include_ipqp_evidence();
int standalone_include_nlp_problem();
int standalone_include_nlp_aggregate();
int standalone_include_aggregate_declaration();
int standalone_include_nlp_model_aggregate();
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
                               hven_install_smoke::standalone_include_nlp_aggregate() +
                               hven_install_smoke::standalone_include_aggregate_declaration() +
                               hven_install_smoke::standalone_include_nlp_model_aggregate() +
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
    if (standalone_tus != 18) {
        std::fprintf(stderr, "install smoke: %d standalone-include TUs linked, expected 18\n",
                     standalone_tus);
        return 1;
    }

    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;

    // ONE DECLARED PROBLEM, TWO ENGINES. The interior-point engine consumes the
    // program `make_nlp_program` transcribes; the SQP engine consumes the same
    // problem's model conversion. Everything after that is `run_once`, written
    // once.
    const auto problem = std::make_shared<Hs071Problem>();
    const auto program = hven::solvers::make_nlp_program(problem);
    hven::solvers::IpmSolver ipm;
    if (!run_once(ipm, *program, x0, "IpmSolver")) {
        return 1;
    }

    const auto model = std::make_shared<hven::solvers::NlpProblemModel>(problem);
    hven::solvers::SqpSolver sqp{hven::solvers::SqpOptions{}};
    if (!run_once(sqp, *model, x0, "SqpSolver")) {
        return 1;
    }

    std::printf("install smoke: both solvers converged through one generic consumer; "
                "%d standalone-include TUs compiled and linked\n",
                standalone_tus);
    return 0;
}
