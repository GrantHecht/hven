// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <memory>

#include <Eigen/Core>

#include "hven/detail/model/nlp_adapter.h"
#include "hven/drivers/optimization_problem_base.h"
#include "hven/model/nlp_problem.h"
#include "hven/model/nlp_problem_model.h"

namespace hven::solvers {

/// @brief Solves an NLPProblem with InteriorPointSolver; owns the optimizer
///        and its settings via OptimizationProblemBase.
///
/// Transcription happens lazily on the first solve: NlpProblemModel converts
/// the triplet declaration to the native model contract and the piece host
/// carries it onto NonLinearProgram -- the problem reaches the engine no other
/// way. Transcription evaluates eval_jac and eval_hess once, before any solve
/// iterate exists, at the model's start point (the origin projected onto the
/// declared variable bounds); the values are discarded and only the sparsity
/// patterns are kept. Those two callbacks must therefore be defined there (see
/// NLPProblem). A transcription that faults commits nothing and leaves the
/// solver retriable. The problem is evaluated single-partition on the calling
/// thread. Every entry point throws std::invalid_argument when the initial
/// guess's size does not match the transcribed problem's variable count.
struct NLPSolver : OptimizationProblemBase {
    /// The problem being solved.
    std::shared_ptr<NLPProblem> problem_;
    /// The converted model from the last transcription; null before one.
    std::shared_ptr<NlpProblemModel> model_;
    /// The adapter core from the last transcription; null before one.
    std::shared_ptr<NLPAdapterCore> core_;

    Eigen::VectorXd active_variables_;
    Eigen::VectorXd active_eq_lmults_;
    Eigen::VectorXd active_iq_lmults_;
    /// True until the first successful transcription; jet_release() restores it.
    bool do_transcription_ = true;

    /// @brief Takes ownership of @p problem; transcription waits for the first solve.
    /// @throws std::invalid_argument if @p problem is null.
    explicit NLPSolver(std::shared_ptr<NLPProblem> problem);

    /// @brief Transcribes now: builds the model, core and program and adopts
    ///        the program. A failure leaves the previous state whole.
    void transcribe();

    hven::ConvergenceFlags solve(ConstEigenRef<Eigen::VectorXd> x0);
    hven::ConvergenceFlags optimize(ConstEigenRef<Eigen::VectorXd> x0);
    hven::ConvergenceFlags solve_optimize(ConstEigenRef<Eigen::VectorXd> x0);
    hven::ConvergenceFlags optimize_solve(ConstEigenRef<Eigen::VectorXd> x0);
    hven::ConvergenceFlags solve_optimize_solve(ConstEigenRef<Eigen::VectorXd> x0);

    /// Solution primal vector, in the problem's own variable space.
    Eigen::VectorXd return_x() const { return this->active_variables_; }

    /// Solution constraint multipliers in the problem's own row space, Ipopt
    /// sign convention (L = obj_factor*f + lambda^T g). Zero for rows the
    /// classification dropped.
    ///
    /// @throws std::runtime_error if no solve has run yet.
    Eigen::VectorXd return_multipliers() const;

    // Jet-batch surface: the no-arg overloads reuse whatever is currently in
    // active_variables_; the x0-taking overloads are the primary entry points.
    hven::ConvergenceFlags solve() override;
    hven::ConvergenceFlags optimize() override;
    hven::ConvergenceFlags solve_optimize() override;
    hven::ConvergenceFlags solve_optimize_solve() override;
    hven::ConvergenceFlags optimize_solve() override;
    void jet_initialize() override;
    void jet_release() override;

  private:
    hven::ConvergenceFlags run(JetJobModes mode, ConstEigenRef<Eigen::VectorXd> x0);
    void apply_starting_multipliers();
};

} // namespace hven::solvers
