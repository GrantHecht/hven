// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// bench/ipm_corpus_leg.h — the TOP-LEVEL interior-point replay leg
// (`--engine interior`). The corpus's `ipm` arm is the SQP driver's
// interior-point QP tier; nothing in bench/ replays the InteriorPointSolver
// loop itself. This leg does, over the dual-bindable U0 cells plus one
// bound-fixed-variable cell, under all three fixed-variable treatments.
//
// Determinism, and what the two-capture gate is a gate on: the leg pins the
// evaluation partition count and the backend thread count explicitly
// (InteriorLevers below), because NLPSolver's own defaults are functions of
// the box's core count and the backend one is a thread-local MKL override
// that MKL_NUM_THREADS does not reach.
//
// Counters are the asserted currency (CLAUDE.md §7); `wall_s` is
// informational and is the one column the replay comparator excludes.

#include <memory>
#include <string>
#include <vector>

#include <hven/core/types.h>
#include <hven/model/nlp_problem.h>
#include <hven/model/non_linear_program.h>

#include "corpus_cells.h"

namespace hven::solvers::corpus {

/// The id of the fixed-variable cell this leg adds to the corpus's own cells.
inline constexpr const char *kHs071FixedCellId = "hs071_x1_fixed";

/// @brief Every knob one leg runs under, stamped into the artifact's
///        provenance header so a row is reproducible from the CSV alone.
struct InteriorLevers {
    /// Iteration cap per phase.
    int max_iters = 200;
    /// Console verbosity; 3 and above is silent under the IPM convention.
    int print_level = 10;
    /// The corpus's own tolerances, in set_tols order.
    double kkt_tol = detail::kKktTol;
    double econ_tol = detail::kFeasTol;
    double icon_tol = detail::kFeasTol;
    double barr_tol = detail::kKktTol;
    /// Backend thread count, set explicitly rather than left at NLPSolver's
    /// core-count default; the default reaches MKL as a thread-local override.
    int qp_threads = 1;
    /// Evaluation partition count, adopted at the lay, so set before transcribe().
    int num_partitions = 1;
    /// Widening RelaxBounds applies to a fixed pair; a zero factor is refused
    /// under that treatment (non_linear_program.h).
    double bound_relax_factor = kDefaultBoundRelaxFactor;
};

/// @brief The four identity columns a row carries ahead of its measurements.
struct InteriorRowIdentity {
    std::string cell_id;
    std::string family;
    int n_nodes = 0;
    std::string window;
    std::string taxonomy;
};

/// @brief One cell's outcome under one treatment: the row this leg writes.
///
/// The CSV's first column is `cell_id` joined to `fixed_treatment` by a slash,
/// because the replay comparator keys rows on column 0 alone; the plain
/// treatment stays in its own column. interior_csv_row builds that join.
struct InteriorRow {
    std::string cell_id;
    std::string family;
    int n_nodes = 0;
    std::string window;
    std::string taxonomy;
    std::string status;
    int iter_num = -1;
    double obj_val = 0.0;
    double kkt_inf = 0.0;
    double barr_inf = 0.0;
    double econ_inf = 0.0;
    double icon_inf = 0.0;
    Index factorizations = -1;
    Index solves = -1;
    Index analyses = -1;
    int soc_steps = -1;
    int watchdog_activations = -1;
    std::string fixed_treatment;
    /// Informational only; excluded by the replay comparator by name.
    double wall_s = 0.0;
};

/// @brief The treatment's enumerator spelling, which is what the CSV column and
///        the row key carry.
///
/// Not non_linear_program.h's fixed_variable_treatment_name, which spells the
/// same three values in snake_case for diagnostics.
const char *interior_treatment_tag(FixedVariableTreatments treatment);

/// @brief The three treatments, in the order the leg runs them.
const std::vector<FixedVariableTreatments> &interior_treatments();

/// @brief Runs one declared problem through the top-level interior-point driver
///        under one fixed-variable treatment.
///
/// The one function T8.9 rewrites to construct InteriorPointSolver directly.
///
/// @param problem   The declared problem; retained for the call only.
/// @param identity  The row's four identity columns.
/// @param x0        Start point; must have the problem's variable count.
/// @param treatment The fixed-variable treatment this row runs under.
/// @param levers    The knobs above.
/// @return The finished row, `wall_s` included.
/// @throws std::invalid_argument if @p problem is null, or from the solver's own
///         boundary validation (a treatment the declaration refuses included).
InteriorRow run_interior_problem(const std::shared_ptr<NLPProblem> &problem,
                                 const InteriorRowIdentity &identity, const Vec &x0,
                                 FixedVariableTreatments treatment, const InteriorLevers &levers);

/// @brief Why @p cell cannot be stated as an NLPProblem, or an empty string when
///        it can.
///
/// crossover_legs.h's dual_bind_refusal, forwarded so the CLI glue spells one
/// header.
std::string interior_cell_refusal(const CorpusCell &cell);

/// @brief Runs one U0 corpus cell through the same driver, stating it as an
///        NLPProblem exactly as the crossover leg (a) does.
/// @param cell      The cell; must dual-bind.
/// @param treatment The fixed-variable treatment this row runs under.
/// @param levers    The knobs above.
/// @return The finished row.
/// @throws std::invalid_argument if @p cell does not dual-bind
///         (crossover_legs.h's dual_bind_refusal is non-empty).
InteriorRow run_interior_cell(const CorpusCell &cell, FixedVariableTreatments treatment,
                              const InteriorLevers &levers);

/// @brief Runs the fixed-variable cell: HS071 with x1 pinned by equal bounds.
///
/// No F7 cell has a bound-fixed variable, so this is the only cell in the leg
/// on which the three treatments take three different paths.
///
/// @param treatment The fixed-variable treatment this row runs under.
/// @param levers    The knobs above.
/// @return The finished row.
InteriorRow run_interior_hs071(FixedVariableTreatments treatment, const InteriorLevers &levers);

/// @brief The leg's CSV column header, newline terminated.
std::string interior_csv_header();

/// @brief One row as CSV text, newline terminated.
std::string interior_csv_row(const InteriorRow &row);

} // namespace hven::solvers::corpus
