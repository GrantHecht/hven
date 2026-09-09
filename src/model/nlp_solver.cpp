// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

#include "hven/model/nlp_solver.h"

#include <algorithm>
#include <stdexcept>
#include <string>

#include <fmt/format.h>

#include "hven/detail/interior/utils/get_core_count.h"
#include "hven/detail/interior/utils/thread_pool.h"

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

int NLPSolver::default_num_partitions() {
    int nt = hven::utils::get_num_threads();
    if (nt <= 1)
        return 1;
    return nt * 4;
}

void NLPSolver::init_partitions() {
    this->num_partitions_ = default_num_partitions();
    IpmOptions o = this->optimizer_->options();
    o.common.threads = std::min(HVEN_DEFAULT_QP_THREADS, utils::get_core_count());
    this->optimizer_->set_options(std::move(o));
}

void NLPSolver::set_num_partitions(int num_partitions) {
    if (num_partitions < 1) {
        throw std::invalid_argument("Number of partitions must be positive");
    }
    this->num_partitions_ = num_partitions;
}

hven::solvers::SolveStatus NLPSolver::jet_run() {
    this->jet_initialize();

    hven::solvers::SolveStatus flag;

    switch (this->jet_job_mode_) {
    case JetJobModes::Solve: {
        flag = this->solve();
        break;
    }
    case JetJobModes::Optimize: {
        flag = this->optimize();
        break;
    }
    case JetJobModes::SolveOptimize: {
        flag = this->solve_optimize();
        break;
    }
    case JetJobModes::SolveOptimizeSolve: {
        flag = this->solve_optimize_solve();
        break;
    }
    case JetJobModes::OptimizeSolve: {
        flag = this->optimize_solve();
        break;
    }
    case JetJobModes::NotSet: {
        throw ::std::invalid_argument("jet_job_mode_ not set");
    }
    default:
        throw std::invalid_argument("Unrecognized jet_job_mode");
    }

    this->jet_release();
    return flag;
}

// The mode -> phase-sequence table (M6 W5 T8.4). The five phase-named entries
// this switch used to call are gone; the sequences they ran are options now,
// and this is the one place the old five names are still spelled out.
namespace {
std::vector<hven::solvers::IpmPhase> phases_for(NLPSolver::JetJobModes mode) {
    using hven::solvers::IpmPhase;
    switch (mode) {
    case NLPSolver::JetJobModes::Solve:
        return {IpmPhase::kSolve};
    case NLPSolver::JetJobModes::Optimize:
        return {IpmPhase::kOptimize};
    case NLPSolver::JetJobModes::SolveOptimize:
        return {IpmPhase::kSolve, IpmPhase::kOptimize};
    case NLPSolver::JetJobModes::SolveOptimizeSolve:
        return {IpmPhase::kSolve, IpmPhase::kOptimize, IpmPhase::kSolve};
    case NLPSolver::JetJobModes::OptimizeSolve:
        return {IpmPhase::kOptimize, IpmPhase::kSolve};
    default:
        throw std::invalid_argument("Unrecognized NLP solve mode");
    }
}
} // namespace

NLPSolver::NlpSolveOutput NLPSolver::run_nlp_solver(JetJobModes mode, const Eigen::VectorXd &input,
                                                    const std::optional<WarmStartData> &seed) {
    NlpSolveOutput out;
    // The sequence goes on the options; the solve is one call whatever the
    // mode.
    IpmOptions o = this->optimizer_->options();
    o.phases = phases_for(mode);
    this->optimizer_->set_options(std::move(o));

    // THE PROGRAM IS AN ARGUMENT NOW, borrowed for the call. This class holds
    // the shared_ptr that keeps it alive across calls; the solver holds
    // nothing.
    // ONE CALL EITHER WAY: the payload overload when this job asked for a
    // starting-multiplier seed, the cold one when it did not.
    this->last_result_ = seed.has_value() ? this->optimizer_->solve(*this->nlp_, input, *seed)
                                          : this->optimizer_->solve(*this->nlp_, input);
    const IpmResult &result = this->last_result_;
    out.variables_ = result.x;
    // DECLARED rows only (M6 W5 T8.4): under the MakeConstraint treatment the
    // engine's own equality block carried one internal fixing row per
    // bound-fixed variable in its tail, and this used to hand those rows on to
    // a caller composing user multipliers over the DECLARED rows. IpmResult
    // splits them off (into internal_fixed_lambda_e) before this reads it.
    out.eq_lmults_ = result.lambda_e;
    out.iq_lmults_ = result.lambda_i;
    out.flag_ = result.status;
    return out;
}

NLPSolver::NlpSolveOutput NLPSolver::run_nlp_solver(JetJobModes mode,
                                                    const Eigen::VectorXd &input) {
    // THE PRE-T8.5 SIGNATURE, KEPT (M6 W5 T8.5 fix1). A third argument without
    // a default would have stopped every existing caller compiling, which is
    // not "NLPSolver keeps its surface". One forward, no payload: identical to
    // what this entry did before the warm start became an argument.
    return this->run_nlp_solver(mode, input, std::nullopt);
}

NLPSolver::JetJobModes NLPSolver::strto_jet_job_mode(const std::string &str) {

    if (str == "solve" || str == "Solve")
        return JetJobModes::Solve;
    else if (str == "optimize" || str == "Optimize")
        return JetJobModes::Optimize;
    else if (str == "solve_optimize" || str == "SolveOptimize" || str == "Solve_Optimize")
        return JetJobModes::SolveOptimize;
    else if (str == "solve_optimize_solve" || str == "SolveOptimizeSolve" ||
             str == "Solve_Optimize_Solve")
        return JetJobModes::SolveOptimizeSolve;
    else if (str == "optimize_solve" || str == "OptimizeSolve" || str == "Optimize_Solve")
        return JetJobModes::OptimizeSolve;
    else if (str == "DoNothing" || str == "do_nothing" || str == "Do_Nothing")
        return JetJobModes::DoNothing;
    else {
        auto msg = fmt::format("Unrecognized jet_job_mode: {0}\n", str);
        throw std::invalid_argument(msg);
    }
}

NLPSolver::NLPSolver(std::shared_ptr<NLPProblem> problem) {
    // The folded base constructor's body, verbatim and FIRST: the base
    // subobject was built and configured before the argument was stolen, so
    // problem_ stays default-constructed here and is assigned below.
    this->optimizer_ = std::make_shared<InteriorPointSolver>();
    this->init_partitions();

    this->problem_ = std::move(problem);
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

    // ATOMIC AGAIN (M6 W5 T8.4). The step that used to break it -- set_nlp(),
    // which adopted the program and then re-read dimensions and applied the QP
    // settings, either of which could throw and leave the optimizer holding the
    // new program while the members below still named the old one -- is gone.
    // The solver adopts nothing; the transcription it needs happens inside the
    // next solve, against the program handed to that call. So this function now
    // has no partial state to leave behind at all.

    this->model_ = std::move(model);
    this->core_ = std::move(core);
    this->nlp_ = std::move(nlp);
    this->do_transcription_ = false;
}

hven::solvers::SolveStatus NLPSolver::run(JetJobModes mode, ConstEigenRef<Eigen::VectorXd> x0) {
    if (this->do_transcription_) {
        this->transcribe();
    }
    if (x0.size() != this->core_->n_) {
        throw std::invalid_argument(
            fmt::format("{}: the initial guess has {} elements but the problem has {} variables",
                        this->problem_->name(), x0.size(), this->core_->n_));
    }
    // THE SEED IS AN ARGUMENT NOW (M6 W5 T8.5), built here and carried into the
    // one solve call below rather than staged on the solver between the two.
    auto out = this->run_nlp_solver(mode, Eigen::VectorXd(x0), this->starting_multiplier_seed());
    this->active_variables_ = out.variables_;
    this->active_eq_lmults_ = out.eq_lmults_;
    this->active_iq_lmults_ = out.iq_lmults_;
    return out.flag_;
}

hven::solvers::SolveStatus NLPSolver::solve(ConstEigenRef<Eigen::VectorXd> x0) {
    return this->run(JetJobModes::Solve, x0);
}
hven::solvers::SolveStatus NLPSolver::optimize(ConstEigenRef<Eigen::VectorXd> x0) {
    return this->run(JetJobModes::Optimize, x0);
}
hven::solvers::SolveStatus NLPSolver::solve_optimize(ConstEigenRef<Eigen::VectorXd> x0) {
    return this->run(JetJobModes::SolveOptimize, x0);
}
hven::solvers::SolveStatus NLPSolver::optimize_solve(ConstEigenRef<Eigen::VectorXd> x0) {
    return this->run(JetJobModes::OptimizeSolve, x0);
}
hven::solvers::SolveStatus NLPSolver::solve_optimize_solve(ConstEigenRef<Eigen::VectorXd> x0) {
    return this->run(JetJobModes::SolveOptimizeSolve, x0);
}

// The no-arg entry points, mirroring OptimizationProblem: each reuses whatever
// is currently in active_variables_ as the input iterate.
hven::solvers::SolveStatus NLPSolver::solve() {
    return this->run(JetJobModes::Solve, this->active_variables_);
}
hven::solvers::SolveStatus NLPSolver::optimize() {
    return this->run(JetJobModes::Optimize, this->active_variables_);
}
hven::solvers::SolveStatus NLPSolver::solve_optimize() {
    return this->run(JetJobModes::SolveOptimize, this->active_variables_);
}
hven::solvers::SolveStatus NLPSolver::solve_optimize_solve() {
    return this->run(JetJobModes::SolveOptimizeSolve, this->active_variables_);
}
hven::solvers::SolveStatus NLPSolver::optimize_solve() {
    return this->run(JetJobModes::OptimizeSolve, this->active_variables_);
}

void NLPSolver::jet_initialize() {
    // Single-partition evaluation on the calling thread, and a single QP
    // thread: two independent settings. The thread count and the print level
    // are two fields of ONE options value now, so they arrive together through
    // set_options() -- which validates the whole value and can throw, and so
    // still runs before the partition count is touched.
    IpmOptions o = this->optimizer_->options();
    o.common.threads = 1;
    o.common.print_level = 10;
    this->optimizer_->set_options(std::move(o));
    this->set_num_partitions(1);
    this->transcribe();
}

void NLPSolver::jet_release() {
    // optimizer_->release() is gone with set_nlp() (M6 W5 T8.4): the solver
    // holds no program, so there is nothing to release. Dropping nlp_ below is
    // what ends this job's program, and the next solve transcribes whatever it
    // is handed.
    IpmOptions o = this->optimizer_->options();
    o.common.threads = 1;
    o.common.print_level = 0;
    this->optimizer_->set_options(std::move(o));
    this->set_num_partitions(1);
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

std::optional<hven::solvers::WarmStartData> NLPSolver::starting_multiplier_seed() {
    Eigen::VectorXd lam = Eigen::VectorXd::Zero(this->model_->num_declared_rows());
    if (!this->problem_->starting_multipliers(lam)) {
        // NO SEED REQUESTED FOR THIS CALL, and nothing to clear: the seed is an
        // argument, so a job that does not ask for one simply does not get one
        // -- the class of leak the old clear_initial_multipliers() call
        // defended against cannot be constructed any more.
        return std::nullopt;
    }
    if (!lam.allFinite()) {
        throw std::invalid_argument(fmt::format(
            "{}: starting_multipliers returned a non-finite value", this->problem_->name()));
    }
    Eigen::VectorXd eqm, iqm;
    this->model_->split_user_multipliers(lam, eqm, iqm);

    // THE MULTIPLIERS-ONLY SEED FORM: `primal_` and `bound_lmults_` EMPTY, the
    // two row blocks at the DECLARED row counts, and this program's own
    // declaration stamp -- which the payload route requires and which is
    // trivially available here, the program being the one this call is about to
    // solve. The start point stays the caller's `x0`, exactly as it was when
    // this staged a seed and then solved.
    WarmStartData seed;
    seed.eq_lmults_ = std::move(eqm);
    seed.iq_lmults_ = std::move(iqm);
    seed.structure_key_ = declaration_key(this->nlp_->declaration());
    return seed;
}

} // namespace hven::solvers
