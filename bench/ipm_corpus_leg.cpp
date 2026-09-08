// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include "ipm_corpus_leg.h"

#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

#include <fmt/format.h>

#include <hven/core/solver_status.h>
#include <hven/drivers/solve_status.h>
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

// c(x) = 1 + 1e-4*x0 + x0^10 = 0 has no real root: the near-flat Jacobian at the
// start throws the unlinesearched feasibility step out to |x0| ~ 1e4 and the
// pure power walks it back a tenth at a time, which is the sustained worsening
// the stall detector certifies. Shares its shape with the live pin in
// tests/interior/test_ipm_stop_reason.cpp; a bench binary does not include a
// test file, so it is a copy.
struct PowerSpikeProblem final : NLPProblem {
    static constexpr double kEps = 1.0e-4;
    static constexpr int kPower = 10;

    int num_vars() const override { return 2; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 2; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kSolverInf, -kSolverInf;
        xu << kSolverInf, kSolverInf;
        gl << 0.0;
        gu << 0.0;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override { f = x[1]; }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> g) const override {
        g << 0.0, 1.0;
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = 1.0 + kEps * x[0] + std::pow(x[0], kPower);
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> v) const override {
        v << kEps + kPower * std::pow(x[0], kPower - 1), 0.0;
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd> x, double, ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v << lambda[0] * kPower * (kPower - 1) * std::pow(x[0], kPower - 2), 0.0;
    }
    std::string name() const override { return "PowerSpikeProblem"; }
};

// c(x) = x0^2 + x1^2 + 1 = 0 has no real solution and its violation has a STRICT
// stationary point at the origin -- the shape a restoration phase converges to
// and then declares locally infeasible. Copied for the same reason as above.
struct LocallyInfeasibleProblem final : NLPProblem {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 2; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kSolverInf, -kSolverInf;
        xu << kSolverInf, kSolverInf;
        gl << 0.0;
        gu << 0.0;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override { f = x[0] + x[1]; }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> g) const override {
        g << 1.0, 1.0;
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] * x[0] + x[1] * x[1] + 1.0;
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> v) const override {
        v << 2.0 * x[0], 2.0 * x[1];
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double, ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v << 2.0 * lambda[0], 2.0 * lambda[0];
    }
    std::string name() const override { return "LocallyInfeasibleProblem"; }
};

InteriorRowIdentity infeasible_identity(std::string_view cell_id) {
    // `infeasible` is this pair's taxonomy word: they exist to reach the two
    // abnormal exits, and no F7 start class describes them.
    InteriorRowIdentity id;
    id.cell_id = std::string(cell_id);
    id.family = "synthetic";
    id.n_nodes = 2;
    id.window = "none";
    id.taxonomy = "infeasible";
    return id;
}

Vec two_var_start(double a, double b) {
    Vec x0(2);
    x0 << a, b;
    return x0;
}

const char *entry_tag(InteriorVariant::Entry entry) {
    return entry == InteriorVariant::Entry::kOptimize ? "optimize" : "solve";
}

const char *restoration_tag(RestorationModes mode) {
    switch (mode) {
    case RestorationModes::off:
        return "off";
    case RestorationModes::proximal_switch:
        return "proximal_switch";
    case RestorationModes::l1_nested:
        return "l1_nested";
    }
    throw std::invalid_argument(
        fmt::format("restoration_tag: unrecognized mode ({})", static_cast<int>(mode)));
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

const InteriorVariant &interior_base_variant() {
    static const InteriorVariant kBase{};
    return kBase;
}

const std::vector<InteriorVariant> &interior_exit_variants() {
    // The three abnormal exits, with the levers each one needs. `cap1` is a true
    // variant of a converging cell; the other two only exist on the infeasible
    // cells, because a feasible problem reaches neither exit by lever.
    static const std::vector<InteriorVariant> kVariants = [] {
        std::vector<InteriorVariant> v;
        InteriorVariant cap1;
        cap1.name = "cap1";
        cap1.entry = InteriorVariant::Entry::kOptimize;
        cap1.max_iters = 1;
        v.push_back(cap1);

        InteriorVariant stalled;
        stalled.name = "stalled";
        stalled.entry = InteriorVariant::Entry::kSolve;
        stalled.max_iters = 600;
        stalled.restoration_mode = RestorationModes::l1_nested;
        stalled.max_feas_rest = 1;
        stalled.con_tol = 2.0e-2;
        stalled.acc_kkt_tol = 1.0e-6;
        stalled.acc_con_tol = 2.0e-1;
        stalled.acc_barr_tol = 1.0e-6;
        stalled.div_tol = 1.0e300;
        v.push_back(stalled);

        InteriorVariant resto;
        resto.name = "resto_infeasible";
        resto.entry = InteriorVariant::Entry::kOptimize;
        resto.max_iters = 200;
        resto.restoration_mode = RestorationModes::l1_nested;
        resto.max_feas_rest = 1;
        v.push_back(resto);
        return v;
    }();
    return kVariants;
}

std::string interior_variant_stamp(const InteriorVariant &variant) {
    return fmt::format(
        "variant: {} entry={} max_iters={} restoration={} max_feas_rest={} "
        "con_tol={} acc_tols={}/{}/{} div_tol={}",
        variant.name[0] == '\0' ? "base" : variant.name, entry_tag(variant.entry),
        variant.max_iters > 0 ? std::to_string(variant.max_iters) : std::string("<levers>"),
        restoration_tag(variant.restoration_mode),
        variant.restoration_mode == RestorationModes::off ? std::string("<unused>")
                                                          : std::to_string(variant.max_feas_rest),
        variant.con_tol > 0.0 ? fmt::format("{:.9e}", variant.con_tol) : std::string("<levers>"),
        variant.acc_con_tol > 0.0 ? fmt::format("{:.9e}", variant.acc_kkt_tol)
                                  : std::string("<default>"),
        variant.acc_con_tol > 0.0 ? fmt::format("{:.9e}", variant.acc_con_tol)
                                  : std::string("<default>"),
        variant.acc_con_tol > 0.0 ? fmt::format("{:.9e}", variant.acc_barr_tol)
                                  : std::string("<default>"),
        variant.div_tol > 0.0 ? fmt::format("{:.9e}", variant.div_tol) : std::string("<default>"));
}

InteriorRow run_interior_problem(const std::shared_ptr<NLPProblem> &problem,
                                 const InteriorRowIdentity &identity, const Vec &x0,
                                 FixedVariableTreatments treatment, const InteriorLevers &levers,
                                 const InteriorVariant &variant) {
    if (problem == nullptr) {
        throw std::invalid_argument(
            fmt::format("run_interior_problem: cell '{}' has a null problem", identity.cell_id));
    }

    NLPSolver ipm(problem);
    // Both explicit, before the first solve. The thread count reaches the
    // backend at every solve entry; the partition count reaches no layout
    // through this path -- make_nlp_program constructs NonLinearProgram(1)
    // unconditionally (src/model/nlp_adapter.cpp) -- and is set and recorded so
    // the artifact states what was asked for.
    ipm.set_num_partitions(levers.num_partitions);
    // One options value, built here and handed over once. Every lever and every
    // variant override below is a field write on it, so a dependent pair (the
    // stall variant's restoration_mode and max_feas_rest) can never reach the
    // solver half-applied.
    IpmOptions o = ipm.optimizer_->options();
    o.common.threads = levers.qp_threads;
    o.common.print_level = levers.print_level;
    o.max_iters = levers.max_iters;
    o.kkt_tol = levers.kkt_tol;
    o.econ_tol = levers.econ_tol;
    o.icon_tol = levers.icon_tol;
    o.bar_tol = levers.barr_tol;
    o.fixed_variable_treatment = treatment;
    o.bound_relax_factor = levers.bound_relax_factor;
    // The variant's overrides, applied on top of the levers above. Every field
    // is inert at its default, so a base row runs exactly what T8.1 captured.
    if (variant.max_iters > 0) {
        o.max_iters = variant.max_iters;
    }
    if (variant.con_tol > 0.0) {
        o.econ_tol = variant.con_tol;
        o.icon_tol = variant.con_tol;
    }
    if (variant.acc_con_tol > 0.0) {
        o.acc_kkt_tol = variant.acc_kkt_tol;
        o.acc_econ_tol = variant.acc_con_tol;
        o.acc_icon_tol = variant.acc_con_tol;
        o.acc_bar_tol = variant.acc_barr_tol;
    }
    if (variant.div_tol > 0.0) {
        o.div_kkt_tol = variant.div_tol;
        o.div_econ_tol = variant.div_tol;
        o.div_icon_tol = variant.div_tol;
        o.div_bar_tol = variant.div_tol;
    }
    if (variant.restoration_mode != RestorationModes::off) {
        o.restoration_mode = variant.restoration_mode;
        o.max_feas_rest = variant.max_feas_rest;
    }
    ipm.optimizer_->set_options(std::move(o));
    ipm.transcribe();

    const auto t0 = std::chrono::steady_clock::now();
    const hven::solvers::SolveStatus flag =
        variant.entry == InteriorVariant::Entry::kSolve ? ipm.solve(x0) : ipm.optimize(x0);
    const double wall_s = seconds_since(t0);

    const auto &result = ipm.result();
    InteriorRow row;
    row.cell_id = identity.cell_id;
    row.family = identity.family;
    row.n_nodes = identity.n_nodes;
    row.window = identity.window;
    row.taxonomy = identity.taxonomy;
    row.status = crossover::flag_string(flag);
    row.iter_num = result.iterations;
    row.obj_val = result.f;
    row.kkt_inf = result.kkt_inf;
    row.barr_inf = result.barr_inf;
    row.econ_inf = result.econ_inf;
    row.icon_inf = result.icon_inf;
    row.factorizations = ipm.result().kkt_factor_counters.factorize_count;
    row.solves = ipm.result().kkt_factor_counters.solve_count;
    row.analyses = ipm.result().kkt_analyses_total;
    row.soc_steps = result.soc_steps_taken;
    row.watchdog_activations = result.watchdog_activations;
    row.fixed_treatment = interior_treatment_tag(result.fixed_variable_treatment);
    row.stop_reason = to_string(ipm.optimizer_->last_stop_reason());
    row.variant = variant.name;
    row.wall_s = wall_s;
    return row;
}

std::string interior_cell_refusal(const CorpusCell &cell) {
    return crossover::dual_bind_refusal(cell);
}

InteriorRow run_interior_cell(const CorpusCell &cell, FixedVariableTreatments treatment,
                              const InteriorLevers &levers, const InteriorVariant &variant) {
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
    return run_interior_problem(declared, id, x0, treatment, levers, variant);
}

InteriorRow run_interior_hs071(FixedVariableTreatments treatment, const InteriorLevers &levers,
                               const InteriorVariant &variant) {
    return run_interior_problem(std::make_shared<Hs071FixedProblem>(), hs071_fixed_identity(),
                                hs071_fixed_start(), treatment, levers, variant);
}

InteriorRow run_interior_infeasible(std::string_view cell_id, FixedVariableTreatments treatment,
                                    const InteriorLevers &levers, const InteriorVariant &variant) {
    std::shared_ptr<NLPProblem> problem;
    if (cell_id == kSpikeCellId) {
        problem = std::make_shared<PowerSpikeProblem>();
    } else if (cell_id == kStationaryCellId) {
        problem = std::make_shared<LocallyInfeasibleProblem>();
    } else {
        throw std::invalid_argument(
            fmt::format("run_interior_infeasible: '{}' is neither '{}' nor '{}'", cell_id,
                        kSpikeCellId, kStationaryCellId));
    }
    // Both fixtures start where their own exit needs them to: the spike at the
    // flat point its first step is thrown from, the stationary one away from the
    // origin its restoration converges to.
    const Vec x0 = cell_id == kSpikeCellId ? two_var_start(0.0, 0.0) : two_var_start(1.0, 1.0);
    return run_interior_problem(problem, infeasible_identity(cell_id), x0, treatment, levers,
                                variant);
}

std::string interior_row_key(const InteriorRow &row) {
    if (row.variant.empty()) {
        return fmt::format("{}/{}", row.cell_id, row.fixed_treatment);
    }
    return fmt::format("{}/{}/{}", row.cell_id, row.fixed_treatment, row.variant);
}

std::string interior_csv_header() {
    return "cell_id,family,n_nodes,window,taxonomy,status,iter_num,obj_val,kkt_inf,barr_inf,"
           "econ_inf,icon_inf,factorizations,solves,analyses,soc_steps,watchdog_activations,"
           "fixed_treatment,stop_reason,wall_s\n";
}

std::string interior_csv_row(const InteriorRow &row) {
    return fmt::format("{},{},{},{},{},{},{},{:.9e},{:.9e},{:.9e},{:.9e},{:.9e},{},{},{},{},{},"
                       "{},{},{:.9f}\n",
                       interior_row_key(row), row.family, row.n_nodes, row.window, row.taxonomy,
                       row.status, row.iter_num, row.obj_val, row.kkt_inf, row.barr_inf,
                       row.econ_inf, row.icon_inf, row.factorizations, row.solves, row.analyses,
                       row.soc_steps, row.watchdog_activations, row.fixed_treatment,
                       row.stop_reason, row.wall_s);
}

} // namespace hven::solvers::corpus
