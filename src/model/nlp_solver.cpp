// =============================================================================
// New file in Tycho, carried into hven (Copyright 2026-present Grant R. Hecht,
//   Apache 2.0 — see LICENSE.txt)
// =============================================================================

#include "hven/model/nlp_solver.h"

#include <cmath>
#include <string>

#include <fmt/format.h>

namespace hven::solvers {

namespace {

/// The model's own block of a solver-space multiplier vector.
///
/// The solver's vector carries the engine's own rows after the model's when a
/// fixed variable was turned into an equality row, and is empty when no solve
/// has run yet; either way the model's block is the leading @p rows entries.
Eigen::VectorXd model_multiplier_block(const Eigen::VectorXd &solver_block, Index rows,
                                       const char *which, const std::string &name) {
    if (solver_block.size() == 0) {
        return Eigen::VectorXd::Zero(rows);
    }
    if (solver_block.size() < rows) {
        throw std::runtime_error(
            fmt::format("{}: the solver reported {} {} multipliers for a problem transcribed with "
                        "{} {} rows",
                        name, solver_block.size(), which, rows, which));
    }
    return solver_block.head(rows);
}

} // namespace

NLPSolver::NLPSolver(std::shared_ptr<NLPProblem> problem) : problem_(std::move(problem)) {
    if (!this->problem_) {
        throw std::invalid_argument("NLPSolver: the problem pointer is null");
    }
}

void NLPSolver::transcribe() {
    // Built whole, then committed. Every step below can throw -- the
    // conversion validates the declaration, the host runs the model's
    // derivative callbacks at its start point, the layout sizes the program --
    // so nothing is written to a member until all of them have succeeded. A
    // transcription that faults therefore leaves this solver exactly as it
    // was: the previous transcription still whole, or none at all, and
    // do_transcription_ still true so the next solve retries rather than
    // running against a half-replaced state.
    auto model = std::make_shared<NlpProblemModel>(this->problem_);
    auto core = std::make_shared<NLPAdapterCore>(model, this->problem_->name());
    auto nlp = make_nlp_program(core);
    this->optimizer_->set_nlp(nlp);

    this->model_ = std::move(model);
    this->core_ = std::move(core);
    this->nlp_ = std::move(nlp);
    this->do_transcription_ = false;
}

hven::ConvergenceFlags NLPSolver::run(JetJobModes mode, ConstEigenRef<Eigen::VectorXd> x0) {
    if (this->do_transcription_) {
        this->transcribe();
    }
    if (x0.size() != this->core_->n_) {
        throw std::invalid_argument(
            fmt::format("{}: the initial guess has {} elements but the problem has {} variables",
                        this->problem_->name(), x0.size(), this->core_->n_));
    }
    this->apply_starting_multipliers();
    auto out = this->run_nlp_solver(mode, Eigen::VectorXd(x0));
    this->active_variables_ = out.variables_;
    this->active_eq_lmults_ = out.eq_lmults_;
    this->active_iq_lmults_ = out.iq_lmults_;
    return out.flag_;
}

hven::ConvergenceFlags NLPSolver::solve(ConstEigenRef<Eigen::VectorXd> x0) {
    return this->run(JetJobModes::Solve, x0);
}
hven::ConvergenceFlags NLPSolver::optimize(ConstEigenRef<Eigen::VectorXd> x0) {
    return this->run(JetJobModes::Optimize, x0);
}
hven::ConvergenceFlags NLPSolver::solve_optimize(ConstEigenRef<Eigen::VectorXd> x0) {
    return this->run(JetJobModes::SolveOptimize, x0);
}
hven::ConvergenceFlags NLPSolver::optimize_solve(ConstEigenRef<Eigen::VectorXd> x0) {
    return this->run(JetJobModes::OptimizeSolve, x0);
}
hven::ConvergenceFlags NLPSolver::solve_optimize_solve(ConstEigenRef<Eigen::VectorXd> x0) {
    return this->run(JetJobModes::SolveOptimizeSolve, x0);
}

// OptimizationProblemBase's no-arg entry points, mirroring OptimizationProblem:
// each reuses whatever is currently in active_variables_ as the input iterate.
hven::ConvergenceFlags NLPSolver::solve() {
    return this->run(JetJobModes::Solve, this->active_variables_);
}
hven::ConvergenceFlags NLPSolver::optimize() {
    return this->run(JetJobModes::Optimize, this->active_variables_);
}
hven::ConvergenceFlags NLPSolver::solve_optimize() {
    return this->run(JetJobModes::SolveOptimize, this->active_variables_);
}
hven::ConvergenceFlags NLPSolver::solve_optimize_solve() {
    return this->run(JetJobModes::SolveOptimizeSolve, this->active_variables_);
}
hven::ConvergenceFlags NLPSolver::optimize_solve() {
    return this->run(JetJobModes::OptimizeSolve, this->active_variables_);
}

void NLPSolver::jet_initialize() {
    // Single-partition evaluation on the calling thread, and a single QP
    // thread: two independent settings, set independently. set_qp_threads
    // goes first because it validates and can throw.
    this->optimizer_->set_qp_threads(1);
    this->set_num_partitions(1);
    this->optimizer_->set_print_level(10);
    this->transcribe();
}

void NLPSolver::jet_release() {
    this->optimizer_->release();
    this->optimizer_->set_qp_threads(1);
    this->set_num_partitions(1);
    this->optimizer_->set_print_level(0);
    this->nlp_ = std::shared_ptr<NonLinearProgram>();
    this->do_transcription_ = true;
}

Eigen::VectorXd NLPSolver::return_multipliers() const {
    if (!this->model_) {
        throw std::runtime_error("NLPSolver::return_multipliers: nothing has been solved yet");
    }
    const std::string &name = this->problem_->name();
    return this->model_->compose_user_multipliers(
        model_multiplier_block(this->active_eq_lmults_, this->model_->me(), "equality", name),
        model_multiplier_block(this->active_iq_lmults_, this->model_->mi(), "inequality", name));
}

void NLPSolver::apply_starting_multipliers() {
    Eigen::VectorXd lam = Eigen::VectorXd::Zero(this->model_->num_declared_rows());
    if (!this->problem_->starting_multipliers(lam)) {
        // No seed requested for THIS call. Any staging already armed on the
        // optimizer -- e.g. a direct optimizer_->set_initial_multipliers()
        // call -- must not silently leak into a solve that never asked for it.
        this->optimizer_->clear_initial_multipliers();
        return;
    }
    if (!lam.allFinite()) {
        throw std::invalid_argument(fmt::format(
            "{}: starting_multipliers returned a non-finite value", this->problem_->name()));
    }
    Eigen::VectorXd eqm, iqm;
    this->model_->split_user_multipliers(lam, eqm, iqm);
    this->optimizer_->set_initial_multipliers(eqm, iqm);
}

} // namespace hven::solvers
