// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <memory>
#include <string>

#include <Eigen/Core>

#include "hven/detail/model/nlp_adapter.h"
#include "hven/drivers/interior_point_solver.h"
#include "hven/model/nlp_problem.h"
#include "hven/model/nlp_problem_model.h"
#include "hven/model/non_linear_program.h"

namespace hven::solvers {

/// @brief Solves an NLPProblem with InteriorPointSolver; owns the optimizer,
///        the program it runs and the partitioned-evaluation settings.
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
///
/// FINAL AND NON-POLYMORPHIC. The folded base's virtuals existed for a
/// derived-class contract that never had a second member; there is no vtable,
/// no virtual destructor, and jet_run()'s dispatch is five direct calls. A
/// consumer that derived from this type, or deleted one through a base
/// pointer, is broken by that and is the subject of the W5 migration guide.
struct NLPSolver final {

    /// @brief Which solve-mode entry point jet_run() dispatches to.
    enum class JetJobModes {
        NotSet,
        /// Parsed by strto_jet_job_mode, but dispatched by nothing: both
        /// jet_run() and run_nlp_solver() reject it with
        /// std::invalid_argument.
        DoNothing,
        /// @brief Dispatches solve().
        Solve,
        /// @brief Dispatches optimize().
        Optimize,
        /// @brief Dispatches solve_optimize().
        SolveOptimize,
        /// @brief Dispatches solve_optimize_solve().
        SolveOptimizeSolve,
        /// @brief Dispatches optimize_solve().
        OptimizeSolve
    };

    /// @brief Number of evaluation partitions the NLP is split over.
    int num_partitions_ = 1;
    /// @brief The mode jet_run() dispatches on.
    JetJobModes jet_job_mode_ = JetJobModes::NotSet;

    /// @brief The problem's program (built by transcribe()).
    std::shared_ptr<NonLinearProgram> nlp_;
    /// @brief The shared interior-point solver instance.
    std::shared_ptr<InteriorPointSolver> optimizer_;

    /// The problem being solved.
    std::shared_ptr<NLPProblem> problem_;
    /// The converted model from the last transcription; null before one.
    std::shared_ptr<NlpProblemModel> model_;
    /// The adapter core from the last transcription; null before one.
    std::shared_ptr<NLPAdapterCore> core_;

    Eigen::VectorXd active_variables_;
    Eigen::VectorXd active_eq_lmults_;
    Eigen::VectorXd active_iq_lmults_;
    /// The last solve's whole result, kept so consumers of this wrapper can
    /// still ask a question about a finished solve after the fact.
    ///
    /// THIS IS THE WRAPPER'S ACCOMMODATION, NOT THE ENGINE'S (M6 W5 T8.4). The
    /// engine returns its result by value and holds nothing between calls; it
    /// was InteriorPointSolver::result() that made a finished solve readable
    /// from a solver that had gone on living, and that accessor is gone. This
    /// class's own entry points return only a status, so the rest of the result
    /// would otherwise be unreachable through it. NLPSolver is retired in T8.9,
    /// and this goes with it.
    IpmResult last_result_;
    /// True until the first successful transcription; jet_release() restores it.
    bool do_transcription_ = true;

    /// @brief Takes ownership of @p problem; transcription waits for the first solve.
    ///
    /// Construction order is the folded base's, argument steal included: the
    /// optimizer is made and the default partitioning applied first, and
    /// @p problem is moved into this solver only after that -- so an LVALUE
    /// caller whose make_shared throws still holds its own reference (a sole
    /// owner handed over by std::move was already empty), as before the fold.
    ///
    /// @throws std::invalid_argument if @p problem is null.
    explicit NLPSolver(std::shared_ptr<NLPProblem> problem);

    // Jet-batch surface: the no-arg overloads reuse whatever is currently in
    // active_variables_; the x0-taking overloads below are the primary entry
    // points.

    /// Runs the feasibility (SOE-mode) phase sequence on active_variables_.
    hven::solvers::SolveStatus solve();
    /// Runs the optimality (OPT-mode) phase sequence on active_variables_.
    hven::solvers::SolveStatus optimize();
    /// Runs the SOE-mode phase sequence, then the OPT-mode one. Both always
    /// run.
    hven::solvers::SolveStatus solve_optimize();
    /// Runs SOE, then OPT, then SOE again. The trailing SOE phase is
    /// conditional: it is skipped when OPT reported
    /// SolveStatus::kOptimal.
    hven::solvers::SolveStatus solve_optimize_solve();
    /// Runs the OPT-mode phase sequence, then the SOE-mode one. The trailing
    /// SOE phase is conditional: it is skipped when OPT reported
    /// SolveStatus::kOptimal.
    hven::solvers::SolveStatus optimize_solve();

    /// Compute default partition count from the global thread budget.
    /// Over-partitions by 4x so the work-stealing pool can smooth out
    /// unequal partition costs.
    ///
    /// @return 1 on a single-thread budget, else 4x the thread count.
    static int default_num_partitions();

    /// Applies the default partitioning: num_partitions_ from
    /// default_num_partitions(), and the solver's QP thread count capped at
    /// the physical core count.
    void init_partitions();

    /// @brief Sets the number of evaluation partitions the problem is split over.
    /// @param num_partitions Partition count; must be positive.
    ///
    /// Partition count and QP thread count are independent settings and are set
    /// independently: the solver's own QP thread count is
    /// `optimizer_->options().common.threads`, replaced through
    /// `optimizer_->set_options(o)`.
    ///
    /// @throws std::invalid_argument if `num_partitions < 1`.
    void set_num_partitions(int num_partitions);

    /// Prepares the problem for inline (non-partitioned) evaluation inside
    /// jet_run(); must leave num_partitions_ == 1.
    void jet_initialize();

    /// @brief Releases whatever jet_initialize() acquired.
    void jet_release();

    /// Runs the configured job mode between jet_initialize()/jet_release(),
    /// returning the dispatched mode's convergence flag.
    ///
    /// IMPORTANT: jet_run() is called from Jet::map() on pool worker threads.
    /// If jet_initialize() did NOT set num_partitions_=1, the NLP eval methods
    /// would call parallel_sequence/parallel_task from a pool worker,
    /// triggering the nested-dispatch guard (std::logic_error).
    /// jet_initialize() MUST set num_partitions_=1 so NLP eval methods run
    /// inline.
    ///
    /// @throws std::invalid_argument if jet_job_mode_ is NotSet or otherwise
    /// unrecognized.
    hven::solvers::SolveStatus jet_run();

    /// Uniform output of one solve: the updated variable vector, the
    /// constraint multipliers, and the convergence flag.
    /// @brief The whole result of the most recent solve through this wrapper.
    ///
    /// Default-constructed -- every diagnostic NaN, every block empty, status
    /// kNumericalError -- before the first solve.
    const IpmResult &result() const noexcept { return this->last_result_; }

    struct NlpSolveOutput {
        /// @brief Updated variable vector from the solve.
        Eigen::VectorXd variables_;
        /// @brief Equality-constraint multipliers from the final result.
        Eigen::VectorXd eq_lmults_;
        /// @brief Inequality-constraint multipliers from the final result.
        Eigen::VectorXd iq_lmults_;
        /// @brief Convergence flag from the final result.
        SolveStatus flag_ = SolveStatus::kMaxIter;
    };

    /// Single dispatch point for the five solve modes, mapping each onto the
    /// matching InteriorPointSolver entry point and collecting the uniform result above.
    ///
    /// @throws std::invalid_argument if `mode` is NotSet, DoNothing, or any
    /// other value with no entry point.
    NlpSolveOutput run_nlp_solver(JetJobModes mode, const Eigen::VectorXd &input);

    /// Parses a job-mode name into its enum value. Accepted spellings:
    /// "solve"/"Solve", "optimize"/"Optimize",
    /// "solve_optimize"/"SolveOptimize"/"Solve_Optimize",
    /// "solve_optimize_solve"/"SolveOptimizeSolve"/"Solve_Optimize_Solve",
    /// "optimize_solve"/"OptimizeSolve"/"Optimize_Solve",
    /// "DoNothing"/"do_nothing"/"Do_Nothing".
    ///
    /// @throws std::invalid_argument on any other spelling.
    static JetJobModes strto_jet_job_mode(const std::string &str);

    /// @brief Sets the mode jet_run() dispatches on (enum overload).
    void set_jet_job_mode(JetJobModes m) { this->jet_job_mode_ = m; }

    /// Sets the mode jet_run() dispatches on, parsing the same spellings
    /// strto_jet_job_mode accepts.
    void set_jet_job_mode(const std::string &str) {
        this->set_jet_job_mode(strto_jet_job_mode(str));
    }

    /// @brief Transcribes now: builds the model, core and program and adopts
    ///        the program. A failure leaves this solver retriable --
    ///        do_transcription_ stays true and the next solve re-transcribes.
    void transcribe();

    hven::solvers::SolveStatus solve(ConstEigenRef<Eigen::VectorXd> x0);
    hven::solvers::SolveStatus optimize(ConstEigenRef<Eigen::VectorXd> x0);
    hven::solvers::SolveStatus solve_optimize(ConstEigenRef<Eigen::VectorXd> x0);
    hven::solvers::SolveStatus optimize_solve(ConstEigenRef<Eigen::VectorXd> x0);
    hven::solvers::SolveStatus solve_optimize_solve(ConstEigenRef<Eigen::VectorXd> x0);

    /// Solution primal vector, in the problem's own variable space.
    Eigen::VectorXd return_x() const { return this->active_variables_; }

    /// Solution constraint multipliers in the problem's own row space, Ipopt
    /// sign convention (L = obj_factor*f + lambda^T g). Zero for rows the
    /// classification dropped.
    ///
    /// @throws std::runtime_error if the problem has not been transcribed, or if
    ///         the solver's multiplier block is shorter than the transcribed row
    ///         count.
    Eigen::VectorXd return_multipliers() const;

  private:
    hven::solvers::SolveStatus run(JetJobModes mode, ConstEigenRef<Eigen::VectorXd> x0);
    void apply_starting_multipliers();
};

} // namespace hven::solvers
