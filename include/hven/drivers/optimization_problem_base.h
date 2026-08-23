// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>

#include <Eigen/Core>

#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>

#include "hven/detail/interior/utils/get_core_count.h"
#include "hven/detail/interior/utils/thread_pool.h"
#include "hven/drivers/interior_point_solver.h"
#include "hven/model/non_linear_program.h"

namespace hven::solvers {

/// Base class binding a NonLinearProgram to an InteriorPointSolver: it owns
/// the shared solver/NLP handles and the partitioned-evaluation settings, and
/// leaves the five solve-mode entry points to the derived problem.
struct OptimizationProblemBase {

    /// Which solve-mode entry point jet_run() dispatches to.
    enum class JetJobModes {
        NotSet,
        /// Parsed by strto_jet_job_mode, but dispatched by nothing: both
        /// jet_run() and run_nlp_solver() reject it with
        /// std::invalid_argument.
        DoNothing,
        /// Dispatches solve().
        Solve,
        /// Dispatches optimize().
        Optimize,
        /// Dispatches solve_optimize().
        SolveOptimize,
        /// Dispatches solve_optimize_solve().
        SolveOptimizeSolve,
        /// Dispatches optimize_solve().
        OptimizeSolve
    };

    /// Number of evaluation partitions the NLP is split over.
    int num_partitions_ = 1;
    /// The mode jet_run() dispatches on.
    JetJobModes jet_job_mode_ = JetJobModes::NotSet;

    /// The problem's program (built by the derived class's make_nlp()).
    std::shared_ptr<NonLinearProgram> nlp_;
    /// The shared interior-point solver instance.
    std::shared_ptr<InteriorPointSolver> optimizer_;

    virtual ~OptimizationProblemBase() = default;

    /// Constructs the solver and applies the default partitioning.
    OptimizationProblemBase() {
        this->optimizer_ = std::make_shared<InteriorPointSolver>();
        this->init_partitions();
    }

    /// Runs the feasibility (SOE-mode) phase sequence for the derived
    /// problem's NLP.
    virtual hven::ConvergenceFlags solve() = 0;
    /// Runs the optimality (OPT-mode) phase sequence for the derived
    /// problem's NLP.
    virtual hven::ConvergenceFlags optimize() = 0;
    /// Runs the SOE-mode phase sequence, then the OPT-mode one. Both always
    /// run.
    virtual hven::ConvergenceFlags solve_optimize() = 0;
    /// Runs SOE, then OPT, then SOE again. The trailing SOE phase is
    /// conditional: it is skipped when OPT reported
    /// ConvergenceFlags::CONVERGED.
    virtual hven::ConvergenceFlags solve_optimize_solve() = 0;
    /// Runs the OPT-mode phase sequence, then the SOE-mode one. The trailing
    /// SOE phase is conditional: it is skipped when OPT reported
    /// ConvergenceFlags::CONVERGED.
    virtual hven::ConvergenceFlags optimize_solve() = 0;

    /// Compute default partition count from the global thread budget.
    /// Over-partitions by 4x so the work-stealing pool can smooth out
    /// unequal partition costs.
    ///
    /// @return 1 on a single-thread budget, else 4x the thread count.
    static int default_num_partitions() {
        int nt = hven::utils::get_num_threads();
        if (nt <= 1)
            return 1;
        return nt * 4;
    }

    /// Applies the default partitioning: num_partitions_ from
    /// default_num_partitions(), and the solver's QP thread count capped at
    /// the physical core count.
    virtual void init_partitions() {
        this->num_partitions_ = default_num_partitions();
        this->optimizer_->set_qp_threads(
            std::min(HVEN_DEFAULT_QP_THREADS, utils::get_core_count()));
    }

    /// @brief Sets the number of evaluation partitions the problem is split over.
    /// @param num_partitions Partition count; must be positive.
    ///
    /// Partition count and QP thread count are independent settings and are set
    /// independently: the solver's own QP thread count is
    /// `optimizer_->set_qp_threads(n)`.
    ///
    /// @throws std::invalid_argument if `num_partitions < 1`.
    virtual void set_num_partitions(int num_partitions) {
        if (num_partitions < 1) {
            throw std::invalid_argument("Number of partitions must be positive");
        }
        this->num_partitions_ = num_partitions;
    }

    /// Prepares the problem for inline (non-partitioned) evaluation inside
    /// jet_run(); must leave num_partitions_ == 1.
    virtual void jet_initialize() = 0;

    /// Releases whatever jet_initialize() acquired.
    virtual void jet_release() = 0;

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
    virtual hven::ConvergenceFlags jet_run() {
        this->jet_initialize();

        hven::ConvergenceFlags flag;

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

    /// Uniform output of one solve: the updated variable vector, the
    /// constraint multipliers, and the convergence flag.
    struct NlpSolveOutput {
        /// Updated variable vector from the solve.
        Eigen::VectorXd variables_;
        /// Equality-constraint multipliers from the final result.
        Eigen::VectorXd eq_lmults_;
        /// Inequality-constraint multipliers from the final result.
        Eigen::VectorXd iq_lmults_;
        /// Convergence flag from the final result.
        ConvergenceFlags flag_ = ConvergenceFlags::NOTCONVERGED;
    };

    /// Single dispatch point for the five solve modes, mapping each onto the
    /// matching InteriorPointSolver entry point and collecting the uniform result above.
    /// Defined out of line below.
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
    static JetJobModes strto_jet_job_mode(const std::string &str) {

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

    /// Sets the mode jet_run() dispatches on (enum overload).
    void set_jet_job_mode(JetJobModes m) { this->jet_job_mode_ = m; }

    /// Sets the mode jet_run() dispatches on, parsing the same spellings
    /// strto_jet_job_mode accepts.
    void set_jet_job_mode(const std::string &str) {
        this->set_jet_job_mode(strto_jet_job_mode(str));
    }
};

inline OptimizationProblemBase::NlpSolveOutput
OptimizationProblemBase::run_nlp_solver(JetJobModes mode, const Eigen::VectorXd &input) {
    NlpSolveOutput out;
    switch (mode) {
    case JetJobModes::Solve:
        out.variables_ = this->optimizer_->solve(input);
        break;
    case JetJobModes::Optimize:
        out.variables_ = this->optimizer_->optimize(input);
        break;
    case JetJobModes::SolveOptimize:
        out.variables_ = this->optimizer_->solve_optimize(input);
        break;
    case JetJobModes::SolveOptimizeSolve:
        out.variables_ = this->optimizer_->solve_optimize_solve(input);
        break;
    case JetJobModes::OptimizeSolve:
        out.variables_ = this->optimizer_->optimize_solve(input);
        break;
    default:
        throw std::invalid_argument("Unrecognized NLP solve mode");
    }
    out.eq_lmults_ = this->optimizer_->result().eq_lmults_;
    out.iq_lmults_ = this->optimizer_->result().iq_lmults_;
    out.flag_ = this->optimizer_->result().converge_flag_;
    return out;
}

} // namespace hven::solvers
