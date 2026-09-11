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
// backend thread count explicitly (InteriorLevers below), because the engine's
// default is a function of the box's core count and reaches MKL as a
// thread-local override that MKL_NUM_THREADS does not touch. It pins the
// partition count too, which since M6 W5 T8.9 reaches the LAYOUT -- as N layout
// partitions with the adapter's work on the calling thread, and CLAMPED.
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
    /// The corpus's own tolerances, in kkt/econ/icon/barr order.
    double kkt_tol = detail::kKktTol;
    double econ_tol = detail::kFeasTol;
    double icon_tol = detail::kFeasTol;
    double barr_tol = detail::kKktTol;
    /// Backend thread count, set explicitly rather than left at the engine's
    /// core-count default; the default reaches MKL as a thread-local override.
    int qp_threads = 1;
    /// REQUESTED evaluation partition count, handed to make_nlp_program (M6 W5
    /// T8.9). It reaches the LAYOUT, but LAYOUT ONLY: the adapter's three
    /// pieces are MainThread, so N means N laid partitions with the whole
    /// problem in the last one, evaluated serially on the calling thread; only
    /// treatment-added rows populate the others. The count is CLAMPED by the
    /// 1000-element rule, so the ADOPTED count -- `program->num_partitions_` --
    /// is what a row is stamped with, never this request.
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
    /// Which PHASE SEQUENCE the row drives.
    ///
    /// M6 W5 T8.4: the engine has one entry and the sequence is an option, so
    /// this names a sequence rather than an entry point. kOptimize and kSolve
    /// are the one-phase sequences the two old entries ran; kSolveOptimize is
    /// {kSolve, kOptimize} -- what solve_optimize() ran -- and is the leg's
    /// live proof that a multi-phase call reports each phase separately.
    enum class Entry { kOptimize, kSolve, kSolveOptimize };

    /// WHAT WARM START, if any, the MEASURED solve is handed (M6 W5 T8.5).
    ///
    /// `kNone` is every row before T8.5 and is inert. The other two run the
    /// cell TWICE: a converged producing solve from cold, whose result is
    /// exported, and then the measured solve, handed that export through the
    /// PAYLOAD route `solve(model, x0, const WarmStartData &, budget)` -- the
    /// entry that replaced `stage_warm_start`. Both measured solves start from
    /// the SAME `x0` the base row uses, so `iter_num` is directly comparable
    /// against it, and that comparison is the leg's only observable for the
    /// payload: this engine reports no `start_level_used`.
    ///
    ///   kPayload   the whole export -- point, multipliers, polish extension.
    ///   kSeed      the MULTIPLIERS-ONLY form: `primal_` and `bound_lmults_`
    ///              emptied and the extension dropped, so `x0` is the start
    ///              and only the prices travel.
    ///
    /// WHAT EACH ROW IS READ BY, measured rather than assumed (M6 W5 T8.5):
    ///   kPayload   `iter_num`, STRICTLY BELOW the base row's -- restarting the
    ///              converged point is worth iterations and the column shows it.
    ///   kSeed      the TERMINAL RESIDUALS. On the cell this leg runs it on,
    ///              the multipliers alone change no iteration count, so
    ///              `iter_num` matches the base row's exactly and what shows
    ///              the seed reached the solve is that kkt_inf/econ_inf/icon_inf
    ///              do not. That is what the form is worth there, recorded as
    ///              such.
    ///
    /// THE PRODUCING SOLVE RUNS ON A SEPARATE WRAPPER, because
    /// IpmResult::kkt_factor_counters and kkt_analyses_total ACCUMULATE across
    /// calls on one solver -- a shared solver would report the producing
    /// solve's factorizations in the measured row's own columns.
    enum class Warm { kNone, kPayload, kSeed };

    /// The key's third segment; empty on the base variant, which writes none.
    const char *name = "";
    Entry entry = Entry::kOptimize;
    Warm warm = Warm::kNone;
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
    /// REQUESTED evaluation partition count, when positive; 0 leaves the lever
    /// alone (M6 W5 T8.9). LAYOUT ONLY and CLAMPED -- see InteriorLevers above.
    int num_partitions = 0;
};

/// @brief What a cell's transcription ADOPTS for a requested partition count,
///        and the evaluation pool it would dispatch to.
///
/// The request is not the answer: make_nlp clamps the count at
/// num_user_kkt_elems_ / kMinKktElementsPerPartition, so a small cell adopts
/// fewer. A partitioned row's provenance carries the ADOPTED number, which is
/// what this reports, alongside the PROCESS-GLOBAL evaluation pool size -- a
/// dispatched partition runs on that pool, and this leg does not set it.
struct InteriorPartitionStamp {
    int requested = 0;
    int adopted = 0;
    int pool_threads = 0;
};

/// @brief Transcribes @p cell at @p requested partitions and reports what was
///        adopted.
/// @throws std::invalid_argument if @p cell does not dual-bind.
InteriorPartitionStamp interior_partition_stamp(const CorpusCell &cell, int requested);

/// @brief The variant that lays TWO partitions, run under two treatments.
///
/// Its two rows are the leg's only partitioned ones, and they are LAYOUT rows:
/// the adapter's pieces are all MainThread, so partition N-1 holds the whole
/// problem and runs inline on the calling thread. Which is which is stated in
/// the artifact's own header.
inline constexpr const char *kParts2VariantName = "parts2";

/// @brief The base variant: today's levers, `optimize`, no key segment.
const InteriorVariant &interior_base_variant();

/// @brief The variants this leg runs beyond the base one, in write order.
///
/// The two rules the abnormal-exit rows these variants produce are selected by,
/// stated here, in `--help` and in the artifact's own provenance header:
///   1. They run under MakeParameter ONLY -- an exit is not a treatment
///      question, so the three-treatment sweep does not apply to them.
///   2. They run UNCONDITIONALLY, whatever `--cells` names -- what they pin is a
///      stop reason, not a cell, so they are the leg's own fixed set (like the
///      bound-fixed-variable cell) rather than a selection over the corpus.
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

    // --- THE PER-PHASE ACCOUNT (M6 W5 T8.4) -------------------------------
    /// How many phases the sequence NAMED. 1 on every single-phase row.
    Index phase_count = 0;
    /// How many of them actually RAN. Lower than `phase_count` when a
    /// conditional phase was skipped, which is what the packed column below
    /// lets a reader see per phase.
    Index phases_ran = 0;
    /// The whole per-phase account in one column, so the schema does not grow
    /// with the longest sequence the leg ever runs:
    /// `kSolve:optimal:7|kOptimize:max_iter:3`, with `:skipped` in place of the
    /// status and iterations of a phase that did not run.
    std::string phases;

    // --- THE RETURNED VECTORS' DIMENSIONS (M6 W5 T8.4) --------------------
    //
    // Integers, so no tolerance applies. They are here because the result base
    // moved the four blocks into DECLARED space and width, and a schema that
    // reported only their contents could not show a width regression at all.
    Index x_size = -1;
    Index lambda_e_size = -1;
    Index lambda_i_size = -1;
    Index z_size = -1;

    // --- THE FOUR SHARED DECLARED DIAGNOSTICS (M6 W5 T8.4) ----------------
    //
    // NOT kkt_inf/barr_inf/econ_inf/icon_inf beside them, which are the
    // ENGINE's own measurements in the engine's own space. These four are over
    // the DECLARED problem in CALLER units, by the one definition both engines
    // feed (drivers/solve_result.h) -- which is what makes them comparable
    // across the three fixed-variable treatments, and what the bound-fixed
    // cell's three rows are pinned on.
    double stationarity = 0.0;
    double feasibility_e = 0.0;
    double feasibility_i = 0.0;
    double complementarity = 0.0;

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
/// These are two separate cells rather than two variants of one because the
/// search behind them found no single fixture that reaches both exits, and no
/// lever set that reaches either one on any of the leg's feasible cells (the
/// grid is in the T8.2 report's §4a). That is what was measured, not a proof
/// that no feasible problem can reach either exit -- both are local algorithmic
/// failure doors, not infeasibility certificates.
///
/// @param cell_id   kSpikeCellId or kStationaryCellId.
/// @param treatment The fixed-variable treatment this row runs under.
/// @param levers    The knobs above.
/// @param variant   The exit variant this row runs.
/// @return The finished row.
/// @throws std::invalid_argument if @p cell_id names neither cell.
InteriorRow run_interior_infeasible(std::string_view cell_id, FixedVariableTreatments treatment,
                                    const InteriorLevers &levers, const InteriorVariant &variant);

/// @brief The treatment named by @p tag, spelled as interior_treatment_tag
///        spells it.
///
/// The CLI's own parse for the single-row mode below. It lives beside the tag
/// writer so the two spellings can never drift: a tag this leg writes into a
/// row is a tag this function reads back.
///
/// @param tag MakeParameter | MakeConstraint | RelaxBounds.
/// @throws std::invalid_argument naming @p tag and the three accepted values.
FixedVariableTreatments interior_treatment_from_tag(std::string_view tag);

/// @brief Runs EXACTLY ONE BASE ROW of this leg, in process, and returns it.
///
/// THE SINGLE-ROW MODE (M6 W5 T8.9r). The leg proper writes three rows per
/// cell plus the fixed-variable cell plus the abnormal-exit rows, all in one
/// process, which is what makes a whole-process instruction count of it
/// unreadable: the arms of a comparison do not run the same row set, and the
/// count is dominated by the first row's warm-up either way (reading.md §5 (iv)
/// and §11). One row per process is the instrument that fixes that, and this is
/// the function it needs. It runs NO variant row, NO other cell and NO
/// fork.
///
/// The row is the SAME row the leg writes for the same key: this function is
/// the routing only -- @p cell_id selects between run_interior_hs071 and
/// run_interior_cell exactly as the leg's own two loops do, at
/// interior_base_variant().
///
/// @param cell_id   kHs071FixedCellId, or a corpus cell id that dual-binds.
/// @param treatment The fixed-variable treatment the row runs under.
/// @param levers    The knobs above.
/// @return The finished row, `wall_s` included.
/// @throws std::invalid_argument if @p cell_id is neither the fixed-variable
///         cell nor a known corpus cell, or (from run_interior_cell) if the
///         cell does not dual-bind.
InteriorRow run_interior_single_row(std::string_view cell_id, FixedVariableTreatments treatment,
                                    const InteriorLevers &levers);

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
