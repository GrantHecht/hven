// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

///////////////////////////////////////////////////////////////////////////////
// declared_route.h -- the transcription chain and the multipliers-only seed,
// spelled out once for the suites in this directory that need both (M6 W5
// T8.9, in place of the retired NLPSolver wrapper).
//
// Nothing here is a solver and nothing here is behaviour: `transcribe` is the
// three public calls make_nlp_program(problem) makes, kept apart only because
// a caller that wants to compose or split user multipliers needs the middle
// one; `starting_multiplier_seed` is the payload NLPSolver::run() used to
// build from the problem's own hook, built here by its named replacement,
// NlpProblemModel::split_user_multipliers.
//
// A test that needs only the program calls make_nlp_program(problem) directly
// and does not include this header.
///////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <Eigen/Core>

#include <fmt/format.h>

#include "hven/detail/model/nlp_adapter.h"
#include "hven/drivers/interior_point_solver.h"
#include "hven/model/nlp_problem.h"
#include "hven/model/nlp_problem_model.h"
#include "hven/model/non_linear_program.h"
#include "hven/model/structure_identity.h"
#include "hven/warmstart/warm_start_data.h"

namespace hven_interior_tests {

/// @brief A transcribed program and the engine that solves it, owned together
///        by a factory that builds both.
///
/// Ownership and nothing else -- there is no entry point on this and no state
/// between calls; every solve is `c.engine->solve(*c.program, x0)`, and the
/// PHASE SEQUENCE a fixture runs is written on the engine's options rather than
/// chosen by which entry point a caller reaches for.
struct IpmCase {
    std::shared_ptr<hven::solvers::NonLinearProgram> program;
    std::unique_ptr<hven::solvers::InteriorPointSolver> engine;
};

/// @brief A declared problem, the model it converts to, and the program the
///        interior-point engine consumes.
struct DeclaredRoute {
    std::shared_ptr<hven::solvers::NLPProblem> problem;
    std::shared_ptr<hven::solvers::NlpProblemModel> model;
    std::shared_ptr<hven::solvers::NonLinearProgram> program;
};

/// @brief Walks the transcription chain, keeping the model.
/// @param problem The declared problem.
/// @param num_partitions Requested partition count; LAYOUT ONLY and clamped
///        (see make_nlp_program). Read the adopted count off
///        `route.program->num_partitions_`.
inline DeclaredRoute transcribe(std::shared_ptr<hven::solvers::NLPProblem> problem,
                                int num_partitions = 1) {
    DeclaredRoute route;
    route.problem = std::move(problem);
    route.model = std::make_shared<hven::solvers::NlpProblemModel>(route.problem);
    route.program = hven::solvers::make_nlp_program(
        std::make_shared<hven::solvers::NLPAdapterCore>(route.model, route.problem->name()),
        num_partitions);
    return route;
}

/// @brief The MULTIPLIERS-ONLY warm start built from the problem's own
///        starting_multipliers() hook, or nullopt when it asks for none.
///
/// `primal_` and `bound_lmults_` stay EMPTY, the two row blocks come from
/// NlpProblemModel::split_user_multipliers, and the stamp is this program's own
/// declaration key -- which is what the payload route requires.
///
/// @throws std::invalid_argument if the hook returns a non-finite value.
inline std::optional<hven::solvers::WarmStartData>
starting_multiplier_seed(const DeclaredRoute &route) {
    Eigen::VectorXd lam = Eigen::VectorXd::Zero(route.model->num_declared_rows());
    if (!route.problem->starting_multipliers(lam)) {
        return std::nullopt;
    }
    if (!lam.allFinite()) {
        throw std::invalid_argument(fmt::format(
            "{}: starting_multipliers returned a non-finite value", route.problem->name()));
    }
    Eigen::VectorXd eqm, iqm;
    route.model->split_user_multipliers(lam, eqm, iqm);
    hven::solvers::WarmStartData seed;
    seed.eq_lmults_ = std::move(eqm);
    seed.iq_lmults_ = std::move(iqm);
    seed.structure_key_ = hven::solvers::declaration_key(route.program->declaration());
    return seed;
}

/// @brief One solve through the route, applying the problem's own seed when it
///        asks for one -- the two lines NLPSolver::run() ran.
inline hven::solvers::IpmResult solve_declared(hven::solvers::InteriorPointSolver &engine,
                                               const DeclaredRoute &route,
                                               const Eigen::VectorXd &x0) {
    const std::optional<hven::solvers::WarmStartData> seed = starting_multiplier_seed(route);
    return seed.has_value() ? engine.solve(*route.program, x0, *seed)
                            : engine.solve(*route.program, x0);
}

} // namespace hven_interior_tests
