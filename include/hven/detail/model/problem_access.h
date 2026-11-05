// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The internal routes onto a Problem, and the engines' one door into it.
//
// problem_from_* wrap an engine's own internal form -- a NonLinearProgram for the
// interior-point engine, an NlpModel (or its claim-stream bridge) for the SQP -- in
// a Problem, so tests, bench and library internals reach the engines through the
// same solve entry a consumer uses while the engine runs exactly the route it ran
// before. A borrowed form must outlive the Problem.

#include <memory>
#include <utility>

#include "hven/detail/model/absent_bound.h"
#include "hven/model/problem.h"
#include "hven/model/structure_identity.h"

namespace hven::solvers {

struct NonLinearProgram;
class NlpModel;
class NlpModelAssembly;
class NlpProblemModel;
struct SolveResult;

namespace detail {

/// @brief A Problem over an interior-point program; the SQP refuses it.
Problem problem_from_program(std::shared_ptr<NonLinearProgram> program);
/// @brief A Problem over a borrowed interior-point program.
Problem problem_from_program(NonLinearProgram &program);

/// @brief A Problem over an SQP model. The interior-point engine transcribes it
///        on its first solve (NLPAdapterCore, then make_nlp_program).
Problem problem_from_model(std::shared_ptr<const NlpModel> model);
/// @brief A Problem over a borrowed SQP model.
Problem problem_from_model(const NlpModel &model);

/// @brief A Problem over a borrowed claim-stream bridge; SQP only.
Problem problem_from_assembly(NlpModelAssembly &bridge);

/// @brief A temporary Problem that binds to a solver's `Problem &` parameter for
///        the length of one full expression:
///        `solver.solve(detail::problem_ref(model), x0)`.
class ProblemRef {
  public:
    explicit ProblemRef(Problem problem) noexcept : problem_(std::move(problem)) {}
    operator Problem &() noexcept { return problem_; } // NOLINT(google-explicit-constructor)
    Problem &get() noexcept { return problem_; }

  private:
    Problem problem_;
};

inline ProblemRef problem_ref(NonLinearProgram &program) {
    return ProblemRef(problem_from_program(program));
}
inline ProblemRef problem_ref(const NlpModel &model) {
    return ProblemRef(problem_from_model(model));
}
inline ProblemRef problem_ref(NlpModelAssembly &bridge) {
    return ProblemRef(problem_from_assembly(bridge));
}

/// @brief Which SQP entry a Problem routes to: exactly one member is set.
struct SqpTarget {
    const NlpModel *model = nullptr;
    NlpModelAssembly *assembly = nullptr;
};

/// @brief The engines' door into a Problem.
struct ProblemAccess {
    /// @brief Wraps a state in a Problem (the internal routes' constructor).
    static Problem make(std::unique_ptr<ProblemState> state) noexcept;
    /// @brief The state behind a Problem.
    /// @throws std::logic_error on a moved-from Problem.
    static ProblemState &state(Problem &problem);
    static const ProblemState &state(const Problem &problem);

    /// @brief The interior-point program, refreshed for this solve (re-read,
    ///        caches cleared) and built on first use.
    ///
    /// @p infinite_bound is the solve's CommonOptions::infinite_bound. A Problem
    /// laid under another threshold is re-laid under this one first (an Nlp's
    /// transcription rebuilt, a pieces program rebuilt from its declaration with
    /// the bounds as declared), which the engine counts as a fresh analysis; a
    /// pre-laid program (problem_from_program) cannot be re-laid and is refused.
    /// @throws std::logic_error on a moved-from Problem; std::invalid_argument on
    ///         a refused change, a Problem the interior-point engine cannot solve
    ///         (a bridge, or an Nlp without a Hessian), a pre-laid program under
    ///         another threshold, or a refused transcription.
    static NonLinearProgram &ipm_program(Problem &problem,
                                         double infinite_bound = kDefaultInfiniteBound);

    /// @brief The interior-point program if one is built; nullptr otherwise.
    static const NonLinearProgram *built_ipm_program(const Problem &problem) noexcept;

    /// @brief The SQP's form, refreshed for this solve.
    /// @param exact_hessian Whether the solve reads the problem's Hessian.
    /// @param infinite_bound The solve's CommonOptions::infinite_bound: an Nlp's
    ///        transcription laid under another threshold is rebuilt under this one.
    /// @throws std::logic_error on a moved-from Problem; std::invalid_argument on
    ///         a refused change, a pieces problem, or an Nlp without a Hessian
    ///         under an exact-Hessian solve.
    static SqpTarget sqp_target(Problem &problem, bool exact_hessian,
                                double infinite_bound = kDefaultInfiniteBound);

    /// @brief Writes lambda and g, in the problem's row space, from the engine's
    ///        own row blocks.
    static void report(const Problem &problem, SolveResult &result);

    /// @brief Starts the evaluation tally of the next solve at zero.
    static void reset_work(Problem &problem);

    /// @brief Writes the evaluation counts of the solve just run into
    ///        `result.work` (not `factorizations`).
    ///
    /// An Nlp problem: the five Nlp callbacks, counted at the Nlp boundary by
    /// the decorator the problem installs under both transcriptions. A pieces
    /// or program problem: the interior-point program's evaluation passes, by
    /// the quantities each pass forms (NonLinearProgram::EvaluationTally). The
    /// SQP's internal model and assembly routes (problem_from_model,
    /// problem_from_assembly) are not counted and report 0.
    static void report_work(const Problem &problem, SolveResult &result);

    /// @brief The declaration key both engines stamp on a warm-start export for
    ///        this problem (declaration_key over the declaration each engine
    ///        consumes), as of the last solve entry or construction.
    static DeclarationKey declaration_key(const Problem &problem);

    /// @brief The declared variable bounds, as the solve entry sees them (an
    ///        absent bound is infinite).
    static void variable_bounds(const Problem &problem, Vec &lower, Vec &upper);

    /// @brief The problem's equality and inequality row counts: the two blocks
    ///        of the engines' row split.
    static void row_counts(const Problem &problem, Index &me, Index &mi);

    /// @brief The engines' split multipliers in the problem's row space.
    /// @throws std::invalid_argument if a block is not at its row count.
    static Vec compose_rows(const Problem &problem, const Vec &lambda_e, const Vec &lambda_i);

    /// @brief compose_rows writing into @p out in place (resized, overwritten):
    ///        no allocation once @p out has the capacity. Same refusals.
    static void compose_rows_into(const Problem &problem, const Vec &lambda_e, const Vec &lambda_i,
                                  Vec &out);

    /// @brief A row-space multiplier vector in the engines' split.
    /// @throws std::invalid_argument if @p lambda is not one entry per row.
    static void split_rows(const Problem &problem, const Vec &lambda, Vec &lambda_e, Vec &lambda_i);

    /// @brief The Nlp transcription's model, or nullptr for any other problem.
    static const NlpProblemModel *nlp_model(const Problem &problem) noexcept;
};

} // namespace detail
} // namespace hven::solvers
