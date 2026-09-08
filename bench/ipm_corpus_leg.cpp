// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include "ipm_corpus_leg.h"

#include <chrono>
#include <limits>
#include <stdexcept>
#include <utility>

#include <fmt/format.h>

#include <hven/core/solver_status.h>
#include <hven/model/nlp_solver.h>

#include "crossover_legs.h"

namespace hven::solvers::corpus {

namespace {

using hven::solvers::crossover::ModelAsNlpProblem;

// The test fixture's own spelling of an absent upper bound
// (tests/interior/test_nlp_solver.cpp:20), copied with it.
constexpr double kSolverInf = std::numeric_limits<double>::infinity();

double seconds_since(const std::chrono::steady_clock::time_point &t0) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// The Ipopt HS071 example with x1 PINNED by equal bounds, copied from
// tests/interior/test_nlp_solver.cpp:33 because a bench binary does not include
// a test file. The known optimum has x1 = 1.0 exactly, so the pin does not move
// the answer and the three treatments stay comparable on obj_val.
struct Hs071FixedProblem final : NLPProblem {
    int num_vars() const override { return 4; }
    int num_cons() const override { return 2; }
    int num_jac_nonzeros() const override { return 8; }
    int num_hess_nonzeros() const override { return 10; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << 1.0, 1.0, 1.0, 1.0;
        xu << 1.0, 5.0, 5.0, 5.0;
        gl << 25.0, 40.0;
        gu << kSolverInf, 40.0;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = x[0] * x[3] * (x[0] + x[1] + x[2]) + x[2];
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[3] * (2.0 * x[0] + x[1] + x[2]);
        g[1] = x[0] * x[3];
        g[2] = x[0] * x[3] + 1.0;
        g[3] = x[0] * (x[0] + x[1] + x[2]);
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
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
    void eval_jac(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = x[1] * x[2] * x[3];
        v[1] = x[0] * x[2] * x[3];
        v[2] = x[0] * x[1] * x[3];
        v[3] = x[0] * x[1] * x[2];
        v[4] = 2.0 * x[0];
        v[5] = 2.0 * x[1];
        v[6] = 2.0 * x[2];
        v[7] = 2.0 * x[3];
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd> x, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd> lambda,
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
    std::string name() const override { return "Hs071FixedProblem"; }
};

InteriorRowIdentity hs071_fixed_identity() {
    // `hs`/`none`/`fixed_variable` are this cell's own words: family, window and
    // taxonomy are F7 vocabulary and none of them applies to it.
    InteriorRowIdentity id;
    id.cell_id = kHs071FixedCellId;
    id.family = "hs";
    id.n_nodes = 4;
    id.window = "none";
    id.taxonomy = "fixed_variable";
    return id;
}

Vec hs071_fixed_start() {
    Vec x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    return x0;
}

} // namespace

const char *interior_treatment_tag(FixedVariableTreatments treatment) {
    switch (treatment) {
    case FixedVariableTreatments::MakeParameter:
        return "MakeParameter";
    case FixedVariableTreatments::MakeConstraint:
        return "MakeConstraint";
    case FixedVariableTreatments::RelaxBounds:
        return "RelaxBounds";
    }
    throw std::invalid_argument(fmt::format("interior_treatment_tag: unrecognized treatment ({})",
                                            static_cast<int>(treatment)));
}

const std::vector<FixedVariableTreatments> &interior_treatments() {
    static const std::vector<FixedVariableTreatments> kAll{FixedVariableTreatments::MakeParameter,
                                                           FixedVariableTreatments::MakeConstraint,
                                                           FixedVariableTreatments::RelaxBounds};
    return kAll;
}

InteriorRow run_interior_problem(const std::shared_ptr<NLPProblem> &problem,
                                 const InteriorRowIdentity &identity, const Vec &x0,
                                 FixedVariableTreatments treatment, const InteriorLevers &levers) {
    if (problem == nullptr) {
        throw std::invalid_argument(
            fmt::format("run_interior_problem: cell '{}' has a null problem", identity.cell_id));
    }

    NLPSolver ipm(problem);
    // Both counts BEFORE the lay and the first solve, and both explicit: the
    // partition count is adopted at transcribe() and the thread count reaches
    // the backend at every solve entry.
    ipm.set_num_partitions(levers.num_partitions);
    ipm.optimizer_->set_qp_threads(levers.qp_threads);
    ipm.optimizer_->set_print_level(levers.print_level);
    ipm.optimizer_->set_max_iters(levers.max_iters);
    ipm.optimizer_->set_tols(levers.kkt_tol, levers.econ_tol, levers.icon_tol, levers.barr_tol);
    ipm.optimizer_->set_fixed_variable_treatment(treatment);
    ipm.optimizer_->set_bound_relax_factor(levers.bound_relax_factor);
    ipm.transcribe();

    const auto t0 = std::chrono::steady_clock::now();
    const hven::ConvergenceFlags flag = ipm.optimize(x0);
    const double wall_s = seconds_since(t0);

    const auto &result = ipm.optimizer_->result();
    InteriorRow row;
    row.cell_id = identity.cell_id;
    row.family = identity.family;
    row.n_nodes = identity.n_nodes;
    row.window = identity.window;
    row.taxonomy = identity.taxonomy;
    row.status = crossover::flag_string(flag);
    row.iter_num = result.iter_num_;
    row.obj_val = result.obj_val_;
    row.kkt_inf = result.kkt_inf_;
    row.barr_inf = result.barr_inf_;
    row.econ_inf = result.econ_inf_;
    row.icon_inf = result.icon_inf_;
    row.factorizations = ipm.optimizer_->kkt_factor_counters().factorize_count;
    row.solves = ipm.optimizer_->kkt_factor_counters().solve_count;
    row.analyses = ipm.optimizer_->kkt_analysis_count();
    row.soc_steps = result.soc_steps_taken_;
    row.watchdog_activations = result.watchdog_activations_;
    row.fixed_treatment = interior_treatment_tag(result.fixed_variable_treatment_);
    row.wall_s = wall_s;
    return row;
}

std::string interior_cell_refusal(const CorpusCell &cell) {
    return crossover::dual_bind_refusal(cell);
}

InteriorRow run_interior_cell(const CorpusCell &cell, FixedVariableTreatments treatment,
                              const InteriorLevers &levers) {
    if (!crossover::cell_dual_binds(cell)) {
        throw std::invalid_argument(
            fmt::format("run_interior_cell: cell '{}' does not dual-bind: {}", cell.id,
                        crossover::dual_bind_refusal(cell)));
    }
    const auto f7 = std::make_shared<F7CollocationChain>(detail::make_model(cell));
    const Vec x0 = crossover::detail::start_point_for(cell, *f7);
    const auto declared = std::make_shared<ModelAsNlpProblem>(f7, std::string(cell.id));

    InteriorRowIdentity id;
    id.cell_id = cell.id;
    id.family = to_string(cell.family);
    id.n_nodes = static_cast<int>(cell.n_nodes);
    id.window = to_string(cell.ctag);
    id.taxonomy = to_string(cell.start);
    return run_interior_problem(declared, id, x0, treatment, levers);
}

InteriorRow run_interior_hs071(FixedVariableTreatments treatment, const InteriorLevers &levers) {
    return run_interior_problem(std::make_shared<Hs071FixedProblem>(), hs071_fixed_identity(),
                                hs071_fixed_start(), treatment, levers);
}

std::string interior_csv_header() {
    return "cell_id,family,n_nodes,window,taxonomy,status,iter_num,obj_val,kkt_inf,barr_inf,"
           "econ_inf,icon_inf,factorizations,solves,analyses,soc_steps,watchdog_activations,"
           "fixed_treatment,wall_s\n";
}

std::string interior_csv_row(const InteriorRow &row) {
    return fmt::format("{}/{},{},{},{},{},{},{},{:.9e},{:.9e},{:.9e},{:.9e},{:.9e},{},{},{},{},{},"
                       "{},{:.9f}\n",
                       row.cell_id, row.fixed_treatment, row.family, row.n_nodes, row.window,
                       row.taxonomy, row.status, row.iter_num, row.obj_val, row.kkt_inf,
                       row.barr_inf, row.econ_inf, row.icon_inf, row.factorizations, row.solves,
                       row.analyses, row.soc_steps, row.watchdog_activations, row.fixed_treatment,
                       row.wall_s);
}

} // namespace hven::solvers::corpus
