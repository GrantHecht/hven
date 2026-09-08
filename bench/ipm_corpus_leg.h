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
// backend thread count explicitly (InteriorLevers below), because NLPSolver's
// default is a function of the box's core count and reaches MKL as a
// thread-local override that MKL_NUM_THREADS does not touch. It pins the
// partition count too, which through this path reaches no layout.
//
// Counters are the asserted currency (CLAUDE.md §7); `wall_s` is
// informational and is the one column the replay comparator excludes.

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <hven/core/types.h>
#include <hven/drivers/interior_point_solver.h>
#include <hven/drivers/solve_status.h>
#include <hven/model/nlp_problem.h>
#include <hven/model/non_linear_program.h>

#include "corpus_cells.h"

namespace hven::solvers::corpus {

/// The id of the fixed-variable cell this leg adds to the corpus's own cells.
inline constexpr const char *kHs071FixedCellId = "hs071_x1_fixed";

/// The two infeasible cells the abnormal-exit rows run on (M6 W5 T8.2).
inline constexpr const char *kSpikeCellId = "infeas2_spike";
inline constexpr const char *kStationaryCellId = "infeas2_stationary";

/// The F7 cell the iteration-cap variant runs on; every cell in the leg needs
/// more than one iteration, and this is the cheapest.
inline constexpr const char *kCap1F7CellId = "f7_n1000_bound_neutral";

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
    /// Evaluation partition count. Set on the wrapper and recorded, but it
    /// reaches no layout through this path: make_nlp_program constructs
    /// NonLinearProgram(1) unconditionally (src/model/nlp_adapter.cpp).
    int num_partitions = 1;
    /// Widening RelaxBounds applies to a fixed pair; a zero factor is refused
    /// under that treatment (non_linear_program.h).
    double bound_relax_factor = kDefaultBoundRelaxFactor;
};

/// @brief What one non-base row changes about the run beneath it.
///
/// The base variant carries an empty name and no overrides, so the 33 base rows
/// keep their two-segment keys. Every field is an override applied on top of
/// InteriorLevers, and 0 (or `off`) means "leave the lever alone".
struct InteriorVariant {
    /// Which top-level entry point the row drives.
    enum class Entry { kOptimize, kSolve };

    /// The key's third segment; empty on the base variant, which writes none.
    const char *name = "";
    Entry entry = Entry::kOptimize;
    /// Iteration cap, when positive.
    int max_iters = 0;
    /// Restoration strategy; `off` leaves the whole restoration surface dead.
    RestorationModes restoration_mode = RestorationModes::off;
    /// Per-phase restoration entry budget; read only when a strategy is built.
    int max_feas_rest = 0;
    /// Equality and inequality tolerance, when positive.
    double con_tol = 0.0;
    /// Acceptable-level tolerances, when acc_con_tol is positive.
    double acc_kkt_tol = 0.0;
    double acc_con_tol = 0.0;
    double acc_barr_tol = 0.0;
    /// All four divergence thresholds, when positive.
    double div_tol = 0.0;
};

/// @brief The base variant: today's levers, `optimize`, no key segment.
const InteriorVariant &interior_base_variant();

/// @brief The variants this leg runs beyond the base one, in write order.
const std::vector<InteriorVariant> &interior_exit_variants();

/// @brief One variant's overrides as a provenance line, without the leading `#`.
std::string interior_variant_stamp(const InteriorVariant &variant);

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
    /// The stop reason the engine recorded for the last phase; `none` on every
    /// converged row. hven::solvers::to_string(IpmStopReason) writes it.
    std::string stop_reason;
    /// The variant's name, carried in the row KEY rather than in a column;
    /// empty on a base row.
    std::string variant;
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
                                 FixedVariableTreatments treatment, const InteriorLevers &levers,
                                 const InteriorVariant &variant);

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
                              const InteriorLevers &levers, const InteriorVariant &variant);

/// @brief Runs the fixed-variable cell: HS071 with x1 pinned by equal bounds.
///
/// No F7 cell has a bound-fixed variable, so this is the only cell in the leg
/// on which the three treatments take three different paths.
///
/// @param treatment The fixed-variable treatment this row runs under.
/// @param levers    The knobs above.
/// @return The finished row.
InteriorRow run_interior_hs071(FixedVariableTreatments treatment, const InteriorLevers &levers,
                               const InteriorVariant &variant);

/// @brief Runs one of the two infeasible cells: `infeas2_spike`, whose
///        feasibility stage stalls, and `infeas2_stationary`, whose restoration
///        converges to a locally infeasible point.
///
/// Neither exit is reachable on a feasible cell by lever, which is why they are
/// cells and not variants of one.
///
/// @param cell_id   kSpikeCellId or kStationaryCellId.
/// @param treatment The fixed-variable treatment this row runs under.
/// @param levers    The knobs above.
/// @param variant   The exit variant this row runs.
/// @return The finished row.
/// @throws std::invalid_argument if @p cell_id names neither cell.
InteriorRow run_interior_infeasible(std::string_view cell_id, FixedVariableTreatments treatment,
                                    const InteriorLevers &levers, const InteriorVariant &variant);

/// @brief The row's key: the cell id joined to the treatment by a slash, plus
///        the variant name on a non-base row.
///
/// The replay comparator keys on the first CSV column and keeps only the last of
/// a repeated key, so this value must be unique across an artifact. A base row
/// gets no third segment, so the 33 base keys are what they always were.
std::string interior_row_key(const InteriorRow &row);

/// @brief The leg's CSV column header, newline terminated.
std::string interior_csv_header();

/// @brief One row as CSV text, newline terminated.
std::string interior_csv_row(const InteriorRow &row);

} // namespace hven::solvers::corpus
