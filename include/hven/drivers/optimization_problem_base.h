// =============================================================================
// Originally from ASSET (AlabamaASRL/asset_asrl)
// Copyright 2020-present The University of Alabama-Astrodynamics and Space
//   Research Lab. Licensed under the Apache License, Version 2.0
// License: notices/asset-apache2.txt.
// Source: https://github.com/AlabamaASRL/asset_asrl
// Original Developer: James B. Pezent
//
// Modifications in Tycho (Copyright 2026-present Grant R. Hecht,
//   Apache 2.0 — see LICENSE.txt):
//   - Namespace renamed: asset -> tycho (with sub-namespaces tycho::vf, tycho::oc, etc.)
//   - Python binding methods moved to src/bindings/ (nanobind)
// =============================================================================

#pragma once

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>

#include <Eigen/Core>

#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>

#include "hven/drivers/non_linear_program.h"
#include "hven/drivers/psiopt.h"
#include "hven/detail/interior/utils/get_core_count.h"
#include "hven/detail/interior/utils/thread_pool.h"

namespace hven::solvers {

struct OptimizationProblemBase {

    enum class JetJobModes {
        NotSet,
        DoNothing,
        Solve,
        Optimize,
        SolveOptimize,
        SolveOptimizeSolve,
        OptimizeSolve
    };

    int num_partitions_ = 1;
    JetJobModes jet_job_mode_ = JetJobModes::NotSet;

    std::shared_ptr<NonLinearProgram> nlp_;
    std::shared_ptr<PSIOPT> optimizer_;

    virtual ~OptimizationProblemBase() = default;

    OptimizationProblemBase() {
        this->optimizer_ = std::make_shared<PSIOPT>();
        this->init_partitions();
    }

    virtual hven::ConvergenceFlags solve() = 0;
    virtual hven::ConvergenceFlags optimize() = 0;
    virtual hven::ConvergenceFlags solve_optimize() = 0;
    virtual hven::ConvergenceFlags solve_optimize_solve() = 0;
    virtual hven::ConvergenceFlags optimize_solve() = 0;

    /// Compute default partition count from the global thread budget.
    /// Over-partitions by 4x so the work-stealing pool can smooth out
    /// unequal partition costs. make_nlp() further caps this via
    /// MIN_NNZ_PER_PARTITION on small problems.
    static int default_num_partitions() {
        int nt = hven::utils::get_num_threads();
        if (nt <= 1)
            return 1;
        return nt * 4;
    }

    virtual void init_partitions() {
        this->num_partitions_ = default_num_partitions();
        this->optimizer_->set_qp_threads(
            std::min(TYCHO_DEFAULT_QP_THREADS, utils::get_core_count()));
    }

    virtual void set_num_partitions(int num_partitions, int qp_threads) {
        if (num_partitions < 1) {
            throw std::invalid_argument("Number of partitions must be positive");
        }
        this->optimizer_->set_qp_threads(qp_threads); // may throw — do before mutating
        this->num_partitions_ = num_partitions;
    }
    virtual void set_num_partitions(int num_partitions) {
        if (num_partitions < 1) {
            throw std::invalid_argument("Number of partitions must be positive");
        }
        this->num_partitions_ = num_partitions;
    }

    virtual void jet_initialize() = 0;
    virtual void jet_release() = 0;

    // IMPORTANT: jet_run() is called from Jet::map() on pool worker threads.
    // If jet_initialize() did NOT set num_partitions_=1, the NLP eval methods
    // would call parallel_sequence/parallel_task from a pool worker, triggering
    // the nested-dispatch guard (std::logic_error). jet_initialize() MUST set
    // num_partitions_=1 so NLP eval methods run inline.
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
        Eigen::VectorXd variables_;
        Eigen::VectorXd eq_lmults_;
        Eigen::VectorXd iq_lmults_;
        ConvergenceFlags flag_ = ConvergenceFlags::NOTCONVERGED;
    };

    /// Single dispatch point for the five solve modes, mapping each onto the
    /// matching PSIOPT entry point and collecting the uniform result above.
    /// Defined out of line below.
    NlpSolveOutput run_nlp_solver(JetJobModes mode, const Eigen::VectorXd &input);

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

    void set_jet_job_mode(JetJobModes m) { this->jet_job_mode_ = m; }
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
