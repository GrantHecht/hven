// The hven lane: ONE cell, ONE process.
//
// Both hven lanes are this one generic adapter.  `solve_shared` and
// `fill_shared` below are written against the SHARED SOLVER SHAPE the two engines were given at hven's M6 W5
// T8 -- and against a NARROW SLICE of it: construction from an options value,
// `solve(model, x0, budget)`, and a result whose base `SolveResult` carries the
// answer, the four declared diagnostics and the iteration count in DECLARED
// space and CALLER units.  That slice is instantiated for `IpmSolver` over the
// `NonLinearProgram` that `make_nlp_program` transcribes and for `SqpSolver`
// over the same problem's `NlpProblemModel`, and it is the reason the harness
// needs one lane and not two: a shape that drifted apart on one engine would
// fail to COMPILE here, against an installed prefix.
//
// WHAT IT DOES NOT PROVE, stated so the claim above is not read wider than it
// is.  hven/tests/install_smoke/main.cpp's `run_once` also exercises
// `options()` / `set_options`, the iteration callback, the trace and ledger
// surfaces, and the warm-start payload; none of those is touched here, and a
// drift in any of them would not be caught by this file.  This is the solve
// shape, not the whole shape.
//
// PUBLIC HEADERS ONLY.  Every include below is a public hven header; none is
// under `detail/`.  `make_nlp_program.h` became public at hven's M7 W0 T5(a)
// and this file is the first external consumer that does not have to reach
// into `detail/` to transcribe a problem.
//
// ONE CELL, ONE PROCESS, and that is not an accident.  The wall ceiling is a
// DNF CONDITION (schema/v1.md), and hven has no wall budget of its own: the
// only way to stop a solve at a wall ceiling without teaching the engine to
// watch a clock is for a watchdog OUTSIDE the process to end it.  The runner
// is that watchdog, so the cell must be a process.  It also means a cell that
// crashes takes nothing else with it.
//
// STDOUT IS THE ROW.  `key=value`, one per line, between CELL_BEGIN and
// CELL_END.  Doubles are `%.17g`, which round-trips a double and matches
// hven's own `{:.17g}` trace spelling; the non-finite spellings glibc produces
// (`nan`, `inf`, `-inf`) are schema v1's.  Diagnostics go to stderr, so a
// diagnostic can never be parsed as a row.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hven/core/ledger.h"
#include "hven/core/solver_status.h"
#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/ipm_solver_types.h"
#include "hven/drivers/solve_result.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/drivers/trace_writer.h"
#include "hven/drivers/sqp_solver_types.h"
#include "hven/model/make_nlp_program.h"
#include "hven/model/nlp_problem_model.h"
#include "hven/model/non_linear_program.h"

#include "lanes/roster.h"
#include "problem_source.h"

namespace {

using hven_bench::CountedModel;
using hven_bench::ProblemSource;
using hven_bench::print_roster;
using hven_bench::roster;

// ---------------------------------------------------------------------------
// Spelling
// ---------------------------------------------------------------------------

/// schema v1's float spelling: `{:.17g}`, with hven's non-finite strings.
std::string spell(double v) {
    if (std::isnan(v)) {
        return "nan";
    }
    if (std::isinf(v)) {
        return v > 0 ? "inf" : "-inf";
    }
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.17g", v);
    return std::string(buffer);
}

std::string spell(long long v) { return std::to_string(v); }

/// One `key=value` line of the row.
void emit(const char *key, const std::string &value) {
    std::printf("%s=%s\n", key, value.c_str());
}

// ---------------------------------------------------------------------------
// The shared shape
// ---------------------------------------------------------------------------

/// What every lane reports, whatever engine produced it.
///
/// Every field here comes off `SolveResult`, which is the base BOTH engines
/// return through, so this struct is the shared shape made explicit.  The
/// engine-specific numbers are in `side` and go to the per-lane side file.
struct CellReport {
    std::string status;
    std::string outcome;
    /// schema v1's `budget_iters`: THE CEILING THAT WAS IN FORCE, not the one
    /// the caller asked for.  `SolveBudget::max_iterations` can only TIGHTEN
    /// the engine's own option (hven drivers/solve_result.h: "an EFFECTIVE cap
    /// of min(max_iterations, the engine option)"), and the two engines' own
    /// options differ -- IpmOptions::max_iters is 500, SqpOptions::max_iter is
    /// 100 -- so a single caller number would pin one ceiling on the row and
    /// apply another in the engine.  The lane computes the minimum and reports
    /// it, because the lane is the only place both numbers are in scope.
    long long budget_iters = 0;
    bool converged = false;
    long long iterations = 0;
    long long factorizations = 0;
    double objective = std::numeric_limits<double>::quiet_NaN();
    double stationarity = std::numeric_limits<double>::quiet_NaN();
    double feasibility_e = std::numeric_limits<double>::quiet_NaN();
    double feasibility_i = std::numeric_limits<double>::quiet_NaN();
    double complementarity = std::numeric_limits<double>::quiet_NaN();
    /// max(0, -min(lambda_i)) at the returned point: the classical dual
    /// feasibility residual, which hven's declared quartet does not carry.
    /// Computed here from the PUBLIC `SolveResult::lambda_i`, not invented.
    double dual_sign_violation = std::numeric_limits<double>::quiet_NaN();
    std::vector<std::pair<std::string, std::string>> side;
};

/// schema v1's NORMALISED OUTCOME, from hven's own status enum.
///
/// The mapping lives HERE, beside the enum, and not in the runner: a runner
/// that matched on status STRINGS would silently mis-bucket a status whose
/// spelling changed, and a status hven added would fall through to nothing.
/// The switch has no `default`, so a new enumerator is a COMPILE error in the
/// harness rather than a wrong row.
///
///   * `acceptable` buckets with `optimal`: both mean the engine answered at a
///     point it certified, and a benchmark that scored them differently would
///     be scoring the tolerance rather than the solve.  The lane's own status
///     column keeps the distinction, so no information is lost.
///   * `max_iter` and `budget_exhausted` are both `dnf_budget`: a ceiling this
///     row PINS was reached.  (`dnf_wall` is the runner's, not the engine's --
///     hven has no wall budget, and the watchdog that imposes one is outside
///     the process.)
///   * `stalled`, `diverging`, `numerical_error` and `interrupted` are
///     `error`: the solve produced no answer it stands behind.
const char *normalise_outcome(hven::solvers::SolveStatus status) {
    switch (status) {
    case hven::solvers::SolveStatus::kOptimal:
    case hven::solvers::SolveStatus::kAcceptable:
        return "optimal";
    case hven::solvers::SolveStatus::kInfeasible:
        return "infeasible";
    case hven::solvers::SolveStatus::kMaxIter:
    case hven::solvers::SolveStatus::kBudgetExhausted:
        return "dnf_budget";
    case hven::solvers::SolveStatus::kStalled:
    case hven::solvers::SolveStatus::kDiverging:
    case hven::solvers::SolveStatus::kNumericalError:
    case hven::solvers::SolveStatus::kInterrupted:
        return "error";
    }
    return "error";
}

/// The one measurement hven's four declared diagnostics do not include.
double dual_sign_violation(const hven::solvers::SolveResult &r) {
    // hven's convention is L = f + lambda_e'ce + lambda_i'ci - z with the
    // feasible set ci <= 0 (solve_result.h), so lambda_i >= 0 at a KKT point
    // and the violation is the most negative entry's magnitude.  An empty
    // block has no violation to report and reads 0.
    double worst = 0.0;
    for (Eigen::Index i = 0; i < r.lambda_i.size(); ++i) {
        const double value = r.lambda_i[i];
        if (std::isnan(value)) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        if (-value > worst) {
            worst = -value;
        }
    }
    return worst;
}

/// THE SHARED SHAPE, part one: THE SOLVE.
///
/// This call is spelled identically on `IpmSolver` over a `NonLinearProgram`
/// and on `SqpSolver` over an `NlpProblemModel`.  If the two engines' shape
/// ever drifts apart, this template stops compiling for one of them -- against
/// an INSTALLED prefix, which is the whole point of consuming hven the way an
/// external user does.
///
/// The eval counts are reset here rather than at construction: transcription
/// evaluates `eval_jac` and `eval_hess` once at the start point to learn the
/// sparsity patterns (hven `make_nlp_program.h`'s own note), and those two are
/// setup, not solve.
template <class Solver, class Model>
auto solve_shared(Solver &solver, Model &model, const Eigen::VectorXd &x0,
                  hven::solvers::SolveBudget budget, const CountedModel &counted) {
    counted.reset_eval_counts();
    return solver.solve(model, x0, budget);
}

/// THE SHARED SHAPE, part two: THE ANSWER.
///
/// Every field read here is on `SolveResult`, the non-polymorphic base BOTH
/// engines return through, in DECLARED space and CALLER units.  Nothing
/// engine-specific is reachable from this function, which is what makes it the
/// shared half; the engine-specific reads happen at the call site, off the
/// derived result, and land in the side file.
void fill_shared(const hven::solvers::SolveResult &result, CellReport &report) {
    report.status = hven::solvers::to_string(result.status);
    report.outcome = normalise_outcome(result.status);
    report.converged = result.status == hven::solvers::SolveStatus::kOptimal;
    report.iterations = static_cast<long long>(result.iterations);
    report.objective = result.f;
    report.stationarity = result.stationarity;
    report.feasibility_e = result.feasibility_e;
    report.feasibility_i = result.feasibility_i;
    report.complementarity = result.complementarity;
    report.dual_sign_violation = dual_sign_violation(result);
}

void add_side(CellReport &report, const char *key, long long value) {
    report.side.emplace_back(key, spell(value));
}
void add_side(CellReport &report, const char *key, double value) {
    report.side.emplace_back(key, spell(value));
}
void add_side(CellReport &report, const char *key, const std::string &value) {
    report.side.emplace_back(key, value);
}

void add_harness_counts(CellReport &report, const CountedModel &counted) {
    const auto &c = counted.eval_counts();
    add_side(report, "harness_eval_f", c.f);
    add_side(report, "harness_eval_grad_f", c.grad_f);
    add_side(report, "harness_eval_g", c.g);
    add_side(report, "harness_eval_jac", c.jac);
    add_side(report, "harness_eval_hess", c.hess);
    add_side(report, "harness_dual_sign_violation", report.dual_sign_violation);
}

// ---------------------------------------------------------------------------
// The two lanes
// ---------------------------------------------------------------------------

/// The ceiling actually in force: `SolveBudget::max_iterations` tightens the
/// engine's own option and never loosens it, and 0 means "no budget of the
/// caller's own", so the engine's option stands alone.
long long effective_cap(long long caller, long long engine_option) {
    if (caller <= 0) {
        return engine_option;
    }
    return caller < engine_option ? caller : engine_option;
}

/// `default`: the engine's own options, plus THE TWO -- and it is two, not one
/// -- that the measurement discipline fixes.
///
///   * `common.threads = 1`, set EXPLICITLY on both engines (their own default
///     is 0, "decide for me"), with the process also under `MKL_NUM_THREADS=1`.
///   * `common.print_level = 10`, the silent level both engines share, so the
///     solver cannot write into the row on stdout.
///
/// NEITHER IS ALGORITHMIC and no third field is touched, which is what
/// `options_label = default` is claiming.  `print_level` is named here as
/// deliberately as `threads` is: it is a second departure from the engines'
/// own defaults, a diagnostic one, and a label that mentioned only the first
/// would be a smaller claim than the truth.  Both are in the options dump that
/// `options_digest` hashes, so every row records them whatever this comment
/// says.
/// THE NON-DEFAULT LABELS (M7 W2 A4-0), and the rule they are built under.
///
/// ONE OPTION PER LABEL, with ONE DECLARED EXCEPTION.  Every label below is
/// `default` with exactly ONE algorithmic field assigned -- except the two
/// acceptance strategies, which hven's own validator refuses at one field and
/// which are run as COHERENT TWO-FIELD MODES for the reason set out next.
/// Every companion of an assigned field is left at ITS own default, nothing
/// here is tuned, and no label was chosen because it looked promising: the list
/// is the shipped non-default value of every closed-set algorithmic mode
/// selector the two engines expose on their public options, which is what makes
/// the sweep a census of the modes rather than a search over them.
///
/// THE TWO ACCEPTANCE STRATEGIES, and the exception (owner's ask, 2026-09-17,
/// under the brief's "one coherent mode" clause; these three labels were added
/// AFTER the first ten had already run, which the report records).
/// `acceptance_strategy = funnel` and `= filter` have no one-field spelling:
/// `validate()` refuses either over the DEFAULT `barrier_governor` and NAMES
/// THE COMPANION ITSELF -- "set barrier_governor=monitored, or set
/// never_monotone=True".  Each is therefore run as the strategy PLUS the
/// governor its own validator asks for, with every other option at its default
/// and `never_monotone` deliberately NOT used: that escape hatch accepts
/// running WITHOUT the monotone safeguard, which is a different mode from the
/// one the strategy was designed for.
///
/// `ipm-bar-monitored` is the CONTROL that makes the other two attributable --
/// the governor ALONE, one field -- so a reader can see what the acceptance
/// strategy adds over the governor it required.  The pre-registered decision
/// rule is unchanged and compares every label to `default`; the comparison
/// against the control is descriptive and is not a decision.
///
/// `ipm-acc-funnel-refused` and `ipm-acc-filter-refused` are the ONE-FIELD
/// spellings, kept so `--dump-options-only` can still obtain the refusal text
/// from hven rather than from a transcription of the header.  They are probed
/// and NEVER run.
///
/// WHAT IS NOT HERE, and why.  The claim above -- that this is a census of the
/// mode selectors -- is checked against the INSTALLED headers at the pin, and
/// four groups of selectors are unreachable rather than unchosen.  `soe_mode`,
/// `soe_bar_mode` and `soe_ls_mode` configure the SOE phase, which
/// `phases = {kOptimize}` never runs -- the sweep's own `phases_ran` counter is
/// 1 on every cell of every label, which is the evidence and not the reading.
/// `merit_penalty_rule` is "read only when acceptance_strategy == merit" and
/// `best_criteria` is read only when `return_best` is on, so each needs a
/// companion to have any effect; the three `Ssn*Rule` levers are "inert at
/// qp_mode == QpMode::kWalk" in the header's own words.  `qp_alg`, `qp_ord` and
/// `qp_pivot_strategy` select the sparse backend's factorization, ordering and
/// pivoting -- the Pardiso `iparm` surface, not an algorithmic mode of the
/// engine.  `phases` selects an entry point rather than a mode.  Everything
/// else the two structs expose as a closed-set mode selector has a label.
/// THE BUDGET PROBE (M7 W2 A4-1), and why it is a PREFIX and not a label.
///
/// A4-0 left six CUTEst TRAIN cells at `outcome = dnf_budget` under the IPM
/// default -- they ran out of ITERATIONS, not out of wall -- and the owner's
/// question (2026-09-17, "call the budget into question") is whether the
/// 500-iteration cap is the reason. Answering it means running the SAME label
/// with one more field moved: `IpmOptions::max_iters` from its shipped 500 to
/// 1500. The harness's caller-side `--budget-iters` cannot do it --
/// `effective_cap` above only TIGHTENS the engine's option, by design -- so the
/// raised ceiling has to come from the options value itself.
///
/// It is spelled `probe1500-<base>`, a PREFIX over any existing label, for
/// three reasons. It composes with the five one-up labels without six more
/// branches; the base label's own assignment is reached through the same
/// `ipm_options(base)` the sweep ran, so a probe value differs from its sweep
/// value in exactly one field and the two dumps prove it rather than assert it;
/// and the name says, in the `options_label` column of every row it produces,
/// that the row came from a PROBE and not from a sweep label.
///
/// IT IS NOT A SWEEP LABEL, and is deliberately absent from
/// `sets/options-labels.json`. That file is A4-0's roster -- one algorithmic
/// option per label, each scored against `default` by the pre-registered rule
/// -- and these configurations are none of those things: they move a BUDGET
/// rather than a mode, they are run ONCE (no determinism pair), and no decision
/// rule is applied to them. Their dumps and digests are recorded in the probe
/// run's own directory instead.
///
/// `probe1500-trace-<base>` is the same value with the console turned on
/// (`print_level = 0`, hven's full output) so a stall can be QUOTED rather than
/// described. It is used only for that diagnostic and never for a table row;
/// its output is harmless to the row parser in any case, since the solver
/// prints before `CELL_BEGIN` is written.
constexpr const char *kProbe1500Prefix = "probe1500-";
constexpr const char *kProbe1500TracePrefix = "probe1500-trace-";

/// THE SECOND BUDGET PREFIX (M7 W2 A4-2's preface leg), and why it exists.
///
/// A4-1's §8 reading ends on a measured suggestion -- "if a single number is
/// wanted: 1000" -- and says of its own cost estimate that 185 s on `MSS2` is
/// an INTERPOLATION between two measured points and not a measurement, because
/// no cell was ever run at 1000.  A recommendation whose cost is interpolated
/// is a recommendation with a hole in it, so the number gets measured.
///
/// `probe1000-<base>` is `probe1500-<base>`'s twin in every respect except the
/// value: the same PREFIX spelling, peeled by the same mechanism before the
/// label table so the base label's own assignment is reached through the same
/// `ipm_options(base)` branch, the same one-field proof by dump diff
/// (`max_iters = 500` -> `1000`, and nothing else), and the same deliberate
/// absence from `sets/options-labels.json`: it moves a BUDGET rather than a
/// mode, it is run once, and no decision rule is applied to it.
///
/// There is no `probe1000-trace-` twin.  The trace prefix exists to QUOTE a
/// stall, and A4-1 already quoted the three stalls at 1500; a second transcript
/// at a lower ceiling would add no fact.
constexpr const char *kProbe1000Prefix = "probe1000-";

/// THE THIRD BUDGET PREFIX (M7 W2 H-REPIN), and why it goes DOWN.
///
/// The two above raise the cap to ask what a bigger budget buys.  This one
/// LOWERS it, to 500, and it exists for one reason: to turn hven's own claim
/// about the M7 W2 D1 default event into a check on the bytes.
///
/// D1 moved three `IpmOptions` fields at once -- `acceptance_strategy` to
/// `filter`, `barrier_governor` to `monitored`, `max_iters` from 500 to 1000 --
/// and states that the pre-event value is still reachable BIT-IDENTICALLY as
/// `ipm_preset("classic")` with `max_iters = 500`.  `ipm-preset-classic` is
/// already a label here, and at the new pin it dumps the old globalization
/// fields over the NEW cap, so it differs from the pre-event `default` dump in
/// exactly the one line the claim itself names.  `probe500-ipm-preset-classic`
/// closes that line, and the proof is then a byte comparison against a dump
/// this repository already carries from before the event
/// (`sets/options-dumps/default-ipm.txt`, taken at 3904a477) rather than a
/// reading of a diff.
///
/// It is the same mechanism as its two siblings in every other respect: a
/// PREFIX peeled before the label table, so the base label's own branch is what
/// assigns the algorithmic fields; deliberately absent from
/// `sets/options-labels.json`, because it moves a BUDGET and not a mode; and
/// visible in the `options_label` column of any row it ever produces.  No table
/// row uses it today -- it is a dump-only probe.
constexpr const char *kProbe500Prefix = "probe500-";

/// THE FIVE SHIPPED PRESETS AS LABELS (M7 W2 A4-2).
///
/// A4-0's §6 ends by naming the next measurement: hven ships five named IPM
/// presets that each assign NINE globalization fields at once, a one-option
/// sweep systematically under-samples them, and the winning A4-0 label
/// (`ipm-acc-filter`) is two of `filter_l1`'s nine.  These five labels are that
/// measurement.
///
/// EACH LABEL IS THE PRESET AND NOTHING ELSE.  The value comes from
/// `hven::solvers::ipm_preset(name)` -- hven's own function over hven's own
/// table, not a transcription of the header into this file -- and the only
/// fields assigned on top of it are the SAME TWO `default` sets
/// (`common.threads = 1`, `common.print_level = 10`), which are the
/// measurement discipline's and not algorithmic.  A preset that changes in a
/// future hven therefore changes here, which is the point: the label names the
/// preset, it does not copy it.
///
/// THE LABEL SPELLING IS HYPHENATED and the preset name is not, so the mapping
/// is an EXPLICIT table rather than a `_`-for-`-` substitution.  A substitution
/// would turn a typo into a call to `ipm_preset()` with a name that does not
/// exist, which throws; the table makes a typo fall through to the unknown-label
/// exit 2 that every other misspelled label takes.
///
/// `ipm-preset-classic` IS THE CONTROL AND AN IDENTITY GATE.  The presets
/// header says `classic` is "the stock Settings{} baseline (bit-identical
/// default path)", pinned as literals rather than read off a default-constructed
/// value.  Its rows must therefore be byte-identical to `default`'s on every
/// asserted column.  If they are not, the header's claim is false at this pin
/// and that is a finding, not a thing to explain away.
struct PresetLabel {
    const char *label_;
    const char *preset_;
};
constexpr std::array<PresetLabel, 5> kPresetLabels = {{
    {"ipm-preset-classic", "classic"},
    {"ipm-preset-merit-l1", "merit_l1"},
    {"ipm-preset-filter-l1", "filter_l1"},
    {"ipm-preset-soc-recovery-l1", "soc_recovery_l1"},
    {"ipm-preset-soc-proximal", "soc_proximal"},
}};

hven::solvers::IpmOptions ipm_options(const std::string &label) {
    hven::solvers::IpmOptions o{};
    o.common.threads = 1;
    o.common.print_level = 10;
    // `default` returns HERE, before any other assignment, so its dump is byte
    // for byte what H1/H2/H3 recorded and their `options_digest` still
    // describes the value this binary builds.
    if (label == "default") {
        return o;
    }
    // The probe prefixes are peeled BEFORE the label table, so the base label's
    // own branch below is what actually assigns the algorithmic field.  The
    // trace prefix is tested first because it extends the other.
    if (label.rfind(kProbe1500TracePrefix, 0) == 0) {
        auto probed = ipm_options(label.substr(std::strlen(kProbe1500TracePrefix)));
        probed.max_iters = 1500;
        probed.common.print_level = 0;
        return probed;
    }
    if (label.rfind(kProbe1500Prefix, 0) == 0) {
        auto probed = ipm_options(label.substr(std::strlen(kProbe1500Prefix)));
        probed.max_iters = 1500;
        return probed;
    }
    if (label.rfind(kProbe1000Prefix, 0) == 0) {
        auto probed = ipm_options(label.substr(std::strlen(kProbe1000Prefix)));
        probed.max_iters = 1000;
        return probed;
    }
    if (label.rfind(kProbe500Prefix, 0) == 0) {
        auto probed = ipm_options(label.substr(std::strlen(kProbe500Prefix)));
        probed.max_iters = 500;
        return probed;
    }
    // The presets are looked up BEFORE the one-field table below and return
    // hven's own preset value, not this function's `o`: a preset assigns nine
    // fields and `o` is the default with two of them already touched.
    for (const auto &entry : kPresetLabels) {
        if (label == entry.label_) {
            auto preset = hven::solvers::ipm_preset(entry.preset_);
            preset.common.threads = 1;
            preset.common.print_level = 10;
            return preset;
        }
    }
    using hven::solvers::AcceptanceStrategies;
    using hven::solvers::BarrierGovernors;
    using hven::solvers::BarrierModes;
    using hven::solvers::FixedVariableTreatments;
    using hven::solvers::InertiaModes;
    using hven::solvers::LineSearchModes;
    using hven::solvers::PDStepStrategies;
    using hven::solvers::RestorationModes;
    if (label == "ipm-rest-prox") {
        o.restoration_mode = RestorationModes::proximal_switch;
    } else if (label == "ipm-rest-l1") {
        o.restoration_mode = RestorationModes::l1_nested;
    } else if (label == "ipm-acc-merit") {
        o.acceptance_strategy = AcceptanceStrategies::merit;
    } else if (label == "ipm-bar-monitored") {
        // The CONTROL: the governor alone, one field, so what the two
        // acceptance strategies below add over it is attributable.
        o.barrier_governor = BarrierGovernors::monitored;
    } else if (label == "ipm-acc-funnel") {
        // TWO fields, and the second is the one hven's own refusal names.
        o.acceptance_strategy = AcceptanceStrategies::funnel;
        o.barrier_governor = BarrierGovernors::monitored;
    } else if (label == "ipm-acc-filter") {
        o.acceptance_strategy = AcceptanceStrategies::filter;
        o.barrier_governor = BarrierGovernors::monitored;
    } else if (label == "ipm-acc-funnel-refused") {
        // PROBED, NEVER RUN: the one-field spelling, kept only so the labels
        // file's refusal text comes from validate() and not from a reading.
        o.acceptance_strategy = AcceptanceStrategies::funnel;
    } else if (label == "ipm-acc-filter-refused") {
        o.acceptance_strategy = AcceptanceStrategies::filter;
    } else if (label == "ipm-inertia-prox") {
        o.inertia_mode = InertiaModes::proximal_regularization;
    } else if (label == "ipm-ls-lang") {
        o.opt_ls_mode = LineSearchModes::kLang;
    } else if (label == "ipm-ls-l1") {
        o.opt_ls_mode = LineSearchModes::kL1;
    } else if (label == "ipm-ls-none") {
        // The fourth shipped value of the same selector: no line search at all.
        o.opt_ls_mode = LineSearchModes::kNoLs;
    } else if (label == "ipm-pd-all-min") {
        o.pd_step_strategy = PDStepStrategies::kAllMinimum;
    } else if (label == "ipm-pd-split-eq-iq") {
        o.pd_step_strategy = PDStepStrategies::kPrimSlackSplitEqIq;
    } else if (label == "ipm-pd-max-eq") {
        o.pd_step_strategy = PDStepStrategies::kMaxEq;
    } else if (label == "ipm-bar-probe") {
        o.opt_bar_mode = BarrierModes::kProbe;
    } else if (label == "ipm-fixed-constraint") {
        o.fixed_variable_treatment = FixedVariableTreatments::MakeConstraint;
    } else if (label == "ipm-fixed-relax") {
        o.fixed_variable_treatment = FixedVariableTreatments::RelaxBounds;
    } else {
        std::fprintf(stderr, "hven_bench_cell: unknown options label %s\n", label.c_str());
        std::exit(2);
    }
    return o;
}

/// M7 W2 RETAKE13, a DECLARED BUDGET EVENT (owner-approved, Grant 2026-10-03): the
/// hven-sqp iteration budget of EVERY hven-sqp label goes 100 -> 300, restoration
/// included (hven's `SqpOptions::max_iter` bounds the whole solve: majors plus
/// restoration iterations).  The harness SETS the engine's own option -- the lane's
/// `effective_cap` is `min(caller budget, SqpOptions::max_iter)` and a caller budget
/// can only tighten, so the ceiling cannot be raised from `--budget-iters`; hven's
/// library default (100) is NOT changed.  The runner passes `--budget-iters 0` ("no
/// caller budget"), so the cap in force is this value and every row's `budget_iters`
/// column records 300.  The option is in the options dump, so BOTH hven-sqp
/// `options_digest` values move (a declared options event,
/// runs/2026-10-m7-w2-retake13/README.md).  The 300 s wall is unchanged.
constexpr long long kSqpHarnessMaxIter = 300;

hven::solvers::SqpOptions sqp_options(const std::string &label) {
    hven::solvers::SqpOptions o{};
    o.common.threads = 1;
    o.common.print_level = 10;
    o.max_iter = static_cast<decltype(o.max_iter)>(kSqpHarnessMaxIter);
    // M7 W2 RETAKE16, a DECLARED OPTIONS EVENT (settler ruling C102): the two scored
    // hven-sqp labels run problem scaling's COLUMN layer in its automatic mode,
    // `SqpOptions::column_scaling = kAuto`, LABEL-WIDE -- one fixed configuration for every
    // cell of `default` and of `sqp-qn`, never chosen per cell.  hven's library default is
    // kOff and is NOT changed; `column_scaling_threshold` stays at its shipped 1e3.  The
    // option is in the options dump (`column_scaling = kAuto`, `column_scaling_threshold =
    // 1000`), so both labels' `options_digest` values move
    // (runs/2026-10-m7-w2-retake16/README.md).  The other labels are the library's own.
    if (label == "default" || label == "sqp-qn") {
        o.column_scaling = hven::solvers::SqpColumnScaling::kAuto;
    }
    if (label == "default") {
        return o;
    }
    // `qp_mode` is the ONE field each; the kIpm tier's own `ipqp` block and the
    // kSsn kernel's SSN levers stay at their defaults, which is what the brief's
    // "one coherent mode ... companion options at THEIR defaults only" means.
    if (label == "sqp-ssn") {
        o.qp_mode = hven::solvers::QpMode::kSsn;
    } else if (label == "sqp-ipm") {
        o.qp_mode = hven::solvers::QpMode::kIpm;
    } else if (label == "equality-basis-off") {
        // M7 W2 E1B-RETAKE: the IDENTITY CONTROL for hven's E1b default event.
        // `equality_basis` is ON by default at the E1b pin; this label turns the
        // one field back off, so its rows must reproduce the pre-E1 column
        // (runs/2026-09-m7-w2-e1b-retake/README.md).  Not an A4-0 sweep label:
        // sets/options-labels.json records it in the E2-RETAKE event as a
        // control, outside the scored roster.
        o.equality_basis = false;
    } else if (label == "independent-working-set-off") {
        // M7 W2 E2-RETAKE: the IDENTITY CONTROL for hven's E2 default event.
        // `qp.independent_working_set` is ON by default at the E2 pin; this label
        // turns the one field back off, so its rows must reproduce the E1b
        // default column (runs/2026-09-m7-w2-e2-retake/README.md).  A control,
        // not an A4-0 sweep label: sets/options-labels.json records it in its
        // own event, outside the scored roster.
        o.qp.independent_working_set = false;
    } else if (label == "sqp-qn") {
        // M7 W2 RETAKE14, a DECLARED OPTIONS EVENT (owner ruling): the SNOPT comparison's
        // second hven-sqp configuration.  ONE field moves from `default`:
        // `hessian_mode = kLbfgsDamped`, hven's quasi-Newton Hessian (the model's eval_hess
        // is never asked for the subproblem).  Every other QN option -- qn_memory,
        // qn_update_on_reject, qn_nonlinear_only (off), qn_full_memory_max (400) -- stays at
        // its shipped default, deliberately: this label measures what the library ships.
        // The three lane settings (threads, print_level, max_iter = 300) are `default`'s.
        o.hessian_mode = hven::solvers::SqpHessianMode::kLbfgsDamped;
    } else {
        std::fprintf(stderr, "hven_bench_cell: unknown options label %s\n", label.c_str());
        std::exit(2);
    }
    return o;
}

/// The `hessian_provenance` a hven-sqp row pins.  `sqp-qn` never hands the engine the
/// model's second derivatives (the subproblem curvature is the quasi-Newton B), so its rows
/// pin `none`, the value SNOPT's rows carry for the same reason; every other label takes the
/// problem's own declaration.  schema/columns.py's LABEL_HESSIAN_PROVENANCE is the one
/// table this mirrors (the runner builds the results row from it).
std::string lane_hessian_provenance(const std::string &lane, const std::string &label,
                                    const std::string &declared) {
    if (lane == "hven-sqp" && label == "sqp-qn") {
        return "none";
    }
    return declared;
}

/// `--dump-options-only`: the label's dump and hven's own verdict on it, with
/// NO SOLVE and no problem.
///
/// Two things a label table needs that a solve cannot cheaply give.  First the
/// DUMP, which `sets/options-labels.json` digests -- it is a property of the
/// label alone, so paying for a solve to obtain it would be paying for nothing.
/// Second the VERDICT: `validate()` is what decides whether a one-field label
/// is a legal configuration at all, and its message is hven's text and not the
/// harness's paraphrase of the header.  A refused label's dump is still
/// written, because `dump_options` does not validate and a dump of a rejected
/// value is still a faithful dump (ipm_solver_types.h).
///
/// Exit 0 accepted, 3 refused, 2 an unknown label (via ipm_options/sqp_options).
int dump_options_only(const std::string &lane, const std::string &label,
                      const std::string &path) {
    std::string text;
    {
        std::ostringstream buffer;
        if (lane == "hven-ipm") {
            hven::solvers::dump_options(ipm_options(label), buffer);
        } else if (lane == "hven-sqp") {
            hven::solvers::dump_options(sqp_options(label), buffer);
        } else {
            std::fprintf(stderr, "hven_bench_cell: unknown lane %s\n", lane.c_str());
            return 2;
        }
        text = buffer.str();
    }
    if (!path.empty()) {
        std::ofstream dump(path, std::ios::binary | std::ios::trunc);
        dump << text;
    }
    std::printf("OPTIONS_LABEL %s %s\n", lane.c_str(), label.c_str());
    try {
        if (lane == "hven-ipm") {
            hven::solvers::validate(ipm_options(label));
        } else {
            hven::solvers::validate(sqp_options(label));
        }
    } catch (const std::exception &e) {
        std::printf("OPTIONS_VERDICT refused\n");
        std::printf("OPTIONS_REFUSAL %s\n", e.what());
        return 3;
    }
    std::printf("OPTIONS_VERDICT accepted\n");
    return 0;
}

CellReport run_ipm(const ProblemSource &source, const std::string &options_label,
                   long long budget_iters, const std::string &options_dump_path) {
    const auto model = source.make_model();
    const auto program = hven::solvers::make_nlp_program(model);

    auto options = ipm_options(options_label);
    if (!options_dump_path.empty()) {
        std::ofstream dump(options_dump_path, std::ios::binary | std::ios::trunc);
        hven::solvers::dump_options(options, dump);
    }
    hven::solvers::IpmSolver solver(options);

    hven::solvers::SolveBudget budget{};
    budget.max_iterations = static_cast<hven::Index>(budget_iters);

    CellReport report;
    report.budget_iters = effective_cap(budget_iters, options.max_iters);
    const auto ipm_result = solve_shared(solver, *program, source.x0(), budget, *model);
    fill_shared(ipm_result, report);

    // schema v1 `factorizations`: the interior-point engine has no counters
    // struct; its factorization account is the linear layer's, snapshotted onto
    // the result (ipm_solver_types.h's factorization SNAPSHOT block).
    report.factorizations =
        static_cast<long long>(ipm_result.kkt_factor_counters.factorize_count);

    // The side file: hand-enumerated from the PUBLIC IpmResult, because hven
    // has no counters dump (the twin of dump_options) for a harness to derive
    // this from.  That dump is the harness's standing request.
    add_side(report, "kkt_analyze_count",
             static_cast<long long>(ipm_result.kkt_factor_counters.analyze_count));
    add_side(report, "kkt_factorize_count",
             static_cast<long long>(ipm_result.kkt_factor_counters.factorize_count));
    add_side(report, "kkt_solve_count",
             static_cast<long long>(ipm_result.kkt_factor_counters.solve_count));
    add_side(report, "kkt_partial_solve_count",
             static_cast<long long>(ipm_result.kkt_factor_counters.partial_solve_count));
    add_side(report, "kkt_pattern_verify_count",
             static_cast<long long>(ipm_result.kkt_factor_counters.pattern_verify_count));
    add_side(report, "kkt_analyses_total", static_cast<long long>(ipm_result.kkt_analyses_total));
    add_side(report, "kkt_analyses_this_call",
             static_cast<long long>(ipm_result.kkt_analyses_this_call));
    add_side(report, "soc_steps_taken", static_cast<long long>(ipm_result.soc_steps_taken));
    add_side(report, "watchdog_activations",
             static_cast<long long>(ipm_result.watchdog_activations));
    add_side(report, "payload_ignored", static_cast<long long>(ipm_result.payload_ignored));
    add_side(report, "polish_ignored", static_cast<long long>(ipm_result.polish_ignored));
    add_side(report, "phases_ran", static_cast<long long>(ipm_result.phases.size()));
    add_side(report, "ipm_kkt_inf", ipm_result.kkt_inf);
    add_side(report, "ipm_barr_inf", ipm_result.barr_inf);
    add_side(report, "ipm_econ_inf", ipm_result.econ_inf);
    add_side(report, "ipm_icon_inf", ipm_result.icon_inf);
    add_harness_counts(report, *model);
    return report;
}

/// M7 W2 E2-RETAKE, a DECLARED SCHEMA EVENT (runs/2026-09-m7-w2-e2-retake/
/// README.md): 28 columns APPENDED after every earlier hven-sqp side column,
/// none renamed or moved.  Two SqpCounters fields the side file never carried
/// (the QP-cap exits the diagnosis is about, bare hven names), the ledger's
/// QP-solve count and its kInfeasible count (`harness_qp_ledger_*`), then the
/// SUMS over the ledger's QP records of every QpCounters field hven's E2 added
/// (`harness_qpsum_<field>`: working-set independence, the exchange, the
/// no-admissible-member rule, the zero-motion re-entry rule).  `harness_`
/// because the harness computed them (schema/v1.md): the sums cover the
/// primary QpEngine only, so they are not hven's whole-solve count; see
/// run_sqp's ledger note.
void add_e2_counts(CellReport &report, const hven::solvers::SqpCounters &c,
                   const hven::solvers::Ledger &ledger) {
    add_side(report, "qp_maxiter_exits", static_cast<long long>(c.qp_maxiter_exits));
    add_side(report, "qp_cap_retries", static_cast<long long>(c.qp_cap_retries));
    long long infeasible = 0;
    hven::solvers::QpCounters sum{};
    for (const auto &rec : ledger.records()) {
        if (rec.status == hven::solvers::QpStatus::kInfeasible) {
            ++infeasible;
        }
        const auto &q = rec.counters;
#define HVEN_BENCH_E2_SUM(field) sum.field += q.field
        HVEN_BENCH_E2_SUM(dep_test_solves);
        HVEN_BENCH_E2_SUM(dep_tests);
        HVEN_BENCH_E2_SUM(dep_dependent);
        HVEN_BENCH_E2_SUM(dep_undecidable);
        HVEN_BENCH_E2_SUM(entry_dependency_removals);
        HVEN_BENCH_E2_SUM(mandatory_dependent_entries);
        HVEN_BENCH_E2_SUM(dep_recheck_removals);
        // M7 W2 RETAKE7 schema event: hven NC1 (cef86073) renamed dep_ride_declines
        // to dep_ride_blockers, same meaning, same position (schema/v1.md).
        HVEN_BENCH_E2_SUM(dep_ride_blockers);
        HVEN_BENCH_E2_SUM(dep_pin_skips);
        HVEN_BENCH_E2_SUM(ws_count_violations);
        HVEN_BENCH_E2_SUM(dep_untested_growth);
        HVEN_BENCH_E2_SUM(dep_screen_under_mandatory_dependence);
        HVEN_BENCH_E2_SUM(ws_exchanges);
        HVEN_BENCH_E2_SUM(ws_exchanges_bound);
        HVEN_BENCH_E2_SUM(ws_exchanges_shifted);
        HVEN_BENCH_E2_SUM(dep_infeasible_certificates);
        HVEN_BENCH_E2_SUM(dep_certificate_rejections);
        HVEN_BENCH_E2_SUM(dep_passovers);
        HVEN_BENCH_E2_SUM(dep_blocker_terminations);
        HVEN_BENCH_E2_SUM(dep_deferred_adds);
        HVEN_BENCH_E2_SUM(dep_border_retreats);
        HVEN_BENCH_E2_SUM(reentry_exemptions);
        HVEN_BENCH_E2_SUM(zero_motion_exhausted);
        HVEN_BENCH_E2_SUM(zero_motion_cap_hits);
#undef HVEN_BENCH_E2_SUM
    }
    add_side(report, "harness_qp_ledger_solves", static_cast<long long>(ledger.records().size()));
    add_side(report, "harness_qp_ledger_infeasible", infeasible);
#define HVEN_BENCH_E2_COL(field) \
    add_side(report, "harness_qpsum_" #field, static_cast<long long>(sum.field))
    HVEN_BENCH_E2_COL(dep_test_solves);
    HVEN_BENCH_E2_COL(dep_tests);
    HVEN_BENCH_E2_COL(dep_dependent);
    HVEN_BENCH_E2_COL(dep_undecidable);
    HVEN_BENCH_E2_COL(entry_dependency_removals);
    HVEN_BENCH_E2_COL(mandatory_dependent_entries);
    HVEN_BENCH_E2_COL(dep_recheck_removals);
    HVEN_BENCH_E2_COL(dep_ride_blockers);
    HVEN_BENCH_E2_COL(dep_pin_skips);
    HVEN_BENCH_E2_COL(ws_count_violations);
    HVEN_BENCH_E2_COL(dep_untested_growth);
    HVEN_BENCH_E2_COL(dep_screen_under_mandatory_dependence);
    HVEN_BENCH_E2_COL(ws_exchanges);
    HVEN_BENCH_E2_COL(ws_exchanges_bound);
    HVEN_BENCH_E2_COL(ws_exchanges_shifted);
    HVEN_BENCH_E2_COL(dep_infeasible_certificates);
    HVEN_BENCH_E2_COL(dep_certificate_rejections);
    HVEN_BENCH_E2_COL(dep_passovers);
    HVEN_BENCH_E2_COL(dep_blocker_terminations);
    HVEN_BENCH_E2_COL(dep_deferred_adds);
    HVEN_BENCH_E2_COL(dep_border_retreats);
    HVEN_BENCH_E2_COL(reentry_exemptions);
    HVEN_BENCH_E2_COL(zero_motion_exhausted);
    HVEN_BENCH_E2_COL(zero_motion_cap_hits);
#undef HVEN_BENCH_E2_COL
}

/// M7 W2 E2-RETAKE2, a DECLARED SCHEMA EVENT (runs/2026-09-m7-w2-e2-retake2/
/// README.md): 4 columns APPENDED after every earlier hven-sqp side column (so
/// after the eqb_ block, not next to the 24 E2-RETAKE sums: append-only, none
/// renamed or moved).  The SUMS over the ledger's QP records of the four
/// QpCounters fields hven's E2 fix rounds 1 and 1b added, in the header's
/// order: `dep_structural_exact`, `dep_known_dependent_refusals` (fix1, D3),
/// `dep_uncertified_trims`, `dep_certified_fallbacks` (fix1b, the removal
/// certificate).  `harness_qpsum_` for the same reason as add_e2_counts: the
/// sums cover the primary QpEngine only.
///
/// M7 W2 E2-RETAKE3: hven's E2 fix round 2 RENAMED `dep_certified_fallbacks` to
/// `dep_uncertified_fallbacks` (the same counter, the name corrected), so the
/// fourth column `harness_qpsum_dep_certified_fallbacks` -- the side file's LAST
/// column -- is RETIRED here and re-emitted under its new name by
/// add_e2_fix2_counts, which runs right after this one.
void add_e2_fix1_counts(CellReport &report, const hven::solvers::Ledger &ledger) {
    hven::solvers::QpCounters sum{};
    for (const auto &rec : ledger.records()) {
        const auto &q = rec.counters;
#define HVEN_BENCH_E2F1_SUM(field) sum.field += q.field
        HVEN_BENCH_E2F1_SUM(dep_structural_exact);
        HVEN_BENCH_E2F1_SUM(dep_known_dependent_refusals);
        HVEN_BENCH_E2F1_SUM(dep_uncertified_trims);
#undef HVEN_BENCH_E2F1_SUM
    }
#define HVEN_BENCH_E2F1_COL(field) \
    add_side(report, "harness_qpsum_" #field, static_cast<long long>(sum.field))
    HVEN_BENCH_E2F1_COL(dep_structural_exact);
    HVEN_BENCH_E2F1_COL(dep_known_dependent_refusals);
    HVEN_BENCH_E2F1_COL(dep_uncertified_trims);
#undef HVEN_BENCH_E2F1_COL
}

/// M7 W2 E2-RETAKE3, a DECLARED SCHEMA EVENT (runs/2026-09-m7-w2-e2-retake3/
/// README.md): the retired `harness_qpsum_dep_certified_fallbacks` (above) is
/// followed by 3 columns APPENDED after every other hven-sqp side column.  The
/// SUMS over the ledger's QP records of the three QpCounters fields hven's E2
/// fix round 2 added or renamed, in the pinned header's order
/// (include/hven/core/solver_counters.h at 2543cc7f):
/// `dep_certificate_toleranced` (S25: a verified certificate above the rounding
/// floor, ended kNumericalError), `dep_uncertified_fallbacks` (S24: the renamed
/// fix-1b fallback), `dep_stall_admits` (S23: a stalled test above the
/// undecidable band, admitted as independent).  `harness_qpsum_` for the same
/// reason as add_e2_counts: the sums cover the primary QpEngine only.
void add_e2_fix2_counts(CellReport &report, const hven::solvers::Ledger &ledger) {
    hven::solvers::QpCounters sum{};
    for (const auto &rec : ledger.records()) {
        const auto &q = rec.counters;
#define HVEN_BENCH_E2F2_SUM(field) sum.field += q.field
        HVEN_BENCH_E2F2_SUM(dep_certificate_toleranced);
        HVEN_BENCH_E2F2_SUM(dep_uncertified_fallbacks);
        HVEN_BENCH_E2F2_SUM(dep_stall_admits);
#undef HVEN_BENCH_E2F2_SUM
    }
#define HVEN_BENCH_E2F2_COL(field) \
    add_side(report, "harness_qpsum_" #field, static_cast<long long>(sum.field))
    HVEN_BENCH_E2F2_COL(dep_certificate_toleranced);
    HVEN_BENCH_E2F2_COL(dep_uncertified_fallbacks);
    HVEN_BENCH_E2F2_COL(dep_stall_admits);
#undef HVEN_BENCH_E2F2_COL
}

/// M7 W2 E2-RETAKE4, a DECLARED SCHEMA EVENT (runs/2026-09-m7-w2-e2-retake4/
/// README.md): 6 columns APPENDED after every earlier hven-sqp side column (after
/// add_e2_fix2_counts's three; none renamed, moved or retired).  The SUMS over the
/// ledger's QP records of the six QpCounters fields hven's E2 fix rounds 3, 4 and
/// 5 added, in the pinned header's order (include/hven/core/solver_counters.h at
/// 93dcc96e): `dep_structural_first` (fix3 R-A: a numerical "independent"
/// overturned by an exact structural dependency), `dep_unlocks` (fix5 R-B:
/// zero-motion exchanges that released a member locked only by having entered at
/// this point), `dep_inertia_rereads`, `dep_inertia_reread_flips` (fix3 R-C': kWrong
/// inertia verdicts re-read mu-free, and how many were replaced),
/// `terminal_mu_refines`, `terminal_mu_refine_rescues` (fix4: terminal points
/// re-solved mu-free before a kInfeasible-by-violation verdict, and how many
/// refined points were adopted).  `harness_qpsum_` for the same reason as
/// add_e2_counts: the sums cover the primary QpEngine only.
void add_e2_fix345_counts(CellReport &report, const hven::solvers::Ledger &ledger) {
    hven::solvers::QpCounters sum{};
    for (const auto &rec : ledger.records()) {
        const auto &q = rec.counters;
#define HVEN_BENCH_E2F345_SUM(field) sum.field += q.field
        HVEN_BENCH_E2F345_SUM(dep_structural_first);
        HVEN_BENCH_E2F345_SUM(dep_unlocks);
        HVEN_BENCH_E2F345_SUM(dep_inertia_rereads);
        HVEN_BENCH_E2F345_SUM(dep_inertia_reread_flips);
        HVEN_BENCH_E2F345_SUM(terminal_mu_refines);
        HVEN_BENCH_E2F345_SUM(terminal_mu_refine_rescues);
#undef HVEN_BENCH_E2F345_SUM
    }
#define HVEN_BENCH_E2F345_COL(field) \
    add_side(report, "harness_qpsum_" #field, static_cast<long long>(sum.field))
    HVEN_BENCH_E2F345_COL(dep_structural_first);
    HVEN_BENCH_E2F345_COL(dep_unlocks);
    HVEN_BENCH_E2F345_COL(dep_inertia_rereads);
    HVEN_BENCH_E2F345_COL(dep_inertia_reread_flips);
    HVEN_BENCH_E2F345_COL(terminal_mu_refines);
    HVEN_BENCH_E2F345_COL(terminal_mu_refine_rescues);
#undef HVEN_BENCH_E2F345_COL
}

/// M7 W2 E2-RETAKE6, a DECLARED SCHEMA EVENT (runs/2026-09-m7-w2-e2-retake6/
/// README.md): 2 columns APPENDED after every earlier hven-sqp side column (after
/// add_e2_fix345_counts's six; none renamed, moved or retired).  The SUMS over the
/// ledger's QP records of the two QpCounters fields hven's E2 fix round 6 added,
/// in the pinned header's order (include/hven/core/solver_counters.h at
/// c1f73065): `dep_grey_rereads` (R-E: CONVERGED candidate tests read independent
/// in the grey band kDepTol < rho < kDepUndecidable, re-read with the refinement
/// run to its end on the same factor) and `dep_grey_reread_dependent` (of those,
/// how many the re-read certified DEPENDENT).  `harness_qpsum_` for the same
/// reason as add_e2_counts: the sums cover the primary QpEngine only.
void add_e2_fix6_counts(CellReport &report, const hven::solvers::Ledger &ledger) {
    hven::solvers::QpCounters sum{};
    for (const auto &rec : ledger.records()) {
        const auto &q = rec.counters;
#define HVEN_BENCH_E2F6_SUM(field) sum.field += q.field
        HVEN_BENCH_E2F6_SUM(dep_grey_rereads);
        HVEN_BENCH_E2F6_SUM(dep_grey_reread_dependent);
#undef HVEN_BENCH_E2F6_SUM
    }
#define HVEN_BENCH_E2F6_COL(field) \
    add_side(report, "harness_qpsum_" #field, static_cast<long long>(sum.field))
    HVEN_BENCH_E2F6_COL(dep_grey_rereads);
    HVEN_BENCH_E2F6_COL(dep_grey_reread_dependent);
#undef HVEN_BENCH_E2F6_COL
}

/// M7 W2 RETAKE7, a DECLARED SCHEMA EVENT (runs/2026-09-m7-w2-retake7/README.md):
/// hven's NC1 (the declined ride, cef86073) RENAMED `QpCounters::dep_ride_declines`
/// to `dep_ride_blockers` (same meaning: the ride's first blocker tested
/// dependent), so add_e2_counts's column `harness_qpsum_dep_ride_declines` is
/// renamed IN PLACE to `harness_qpsum_dep_ride_blockers`, and hven ADDED three
/// fields, whose SUMS over the ledger's QP records are APPENDED here after every
/// earlier hven-sqp side column (after add_e2_fix6_counts's two), in the pinned
/// header's order (include/hven/core/solver_counters.h at cef86073):
/// `ride_exchanges` (of the dependent ride blockers, those exchanged; the ride is
/// re-armed), `ride_declines_fallthrough_blocked` (a vetoed ride whose drop was
/// undone instead of the ordinary step) and `ride_sign_vetoes` (ride_sign's veto
/// on the independent working set).  `harness_qpsum_` for the same reason as
/// add_e2_counts: the sums cover the primary QpEngine only.
void add_nc1_counts(CellReport &report, const hven::solvers::Ledger &ledger) {
    hven::solvers::QpCounters sum{};
    for (const auto &rec : ledger.records()) {
        const auto &q = rec.counters;
#define HVEN_BENCH_NC1_SUM(field) sum.field += q.field
        HVEN_BENCH_NC1_SUM(ride_exchanges);
        HVEN_BENCH_NC1_SUM(ride_declines_fallthrough_blocked);
        HVEN_BENCH_NC1_SUM(ride_sign_vetoes);
#undef HVEN_BENCH_NC1_SUM
    }
#define HVEN_BENCH_NC1_COL(field) \
    add_side(report, "harness_qpsum_" #field, static_cast<long long>(sum.field))
    HVEN_BENCH_NC1_COL(ride_exchanges);
    HVEN_BENCH_NC1_COL(ride_declines_fallthrough_blocked);
    HVEN_BENCH_NC1_COL(ride_sign_vetoes);
#undef HVEN_BENCH_NC1_COL
}

/// M7 W2 RETAKE8, a DECLARED SCHEMA EVENT (runs/2026-09-m7-w2-retake8/README.md):
/// hven's NC2 (the flat face, 71f54c50) and its fix round 1 (0fa59cdc) APPENDED
/// two `QpCounters` fields after `ride_sign_vetoes` and renamed, moved or retired
/// none, so their SUMS over the ledger's QP records are APPENDED here after every
/// earlier hven-sqp side column (after add_nc1_counts's three), in the pinned
/// header's order (include/hven/core/solver_counters.h at 85dc01ac):
/// `ride_predicted_signs` (rides whose sign the drop's own multiplier decided, the
/// measured g'p being below its own resolution -- M2's literal floor, which hven
/// measured firing 0 times on 51 dumps) and `eq_drift_violations` (passes at which
/// the iterate carried a STRUCTURAL equality violation after having met the rows; a
/// count of PASSES off the rows, not of drift EVENTS -- the flag never clears, and
/// nothing re-projects the iterate).  `harness_qpsum_` for the same reason as
/// add_e2_counts: the sums cover the primary QpEngine only, so each is a lower
/// bound on the cell.
void add_nc2_counts(CellReport &report, const hven::solvers::Ledger &ledger) {
    hven::solvers::QpCounters sum{};
    for (const auto &rec : ledger.records()) {
        const auto &q = rec.counters;
#define HVEN_BENCH_NC2_SUM(field) sum.field += q.field
        HVEN_BENCH_NC2_SUM(ride_predicted_signs);
        HVEN_BENCH_NC2_SUM(eq_drift_violations);
#undef HVEN_BENCH_NC2_SUM
    }
#define HVEN_BENCH_NC2_COL(field) \
    add_side(report, "harness_qpsum_" #field, static_cast<long long>(sum.field))
    HVEN_BENCH_NC2_COL(ride_predicted_signs);
    HVEN_BENCH_NC2_COL(eq_drift_violations);
#undef HVEN_BENCH_NC2_COL
}

/// M7 W2 RETAKE9, a DECLARED SCHEMA EVENT (runs/2026-09-m7-w2-retake9/README.md;
/// hven settler rulings E4-17, E4-26 and E4-29 in `.superpowers/m7-w2-e4-brief.md`):
/// hven's E4 (THE PROXIMAL EQP, ff02789b / sources e212a308) APPENDED SEVEN fields
/// to `SqpCounters` -- the trailing keys of `HVEN_SQP_COUNTERS_FIELDS`, the direct
/// block 50 -> 57 -- and renamed, moved or retired none.  Eight columns are
/// APPENDED here after every earlier hven-sqp side column, in the pinned header's
/// order (include/hven/core/solver_counters.h at ff02789b): the seven, then ONE
/// DERIVED total.
///
/// THEY ARE BARE hven NAMES, NOT `harness_qpsum_`, and the choice is load-bearing.
/// E4 reaches every QP solve -- subproblems, SOC re-solves, elastic rungs, tier-3
/// face re-solves and the RESTORATION SUB-SOLVE alike -- and hven aggregates all of
/// it into `SqpCounters` itself (`src/drivers/sqp_solver.cpp`'s ~11 charge sites).
/// A `harness_qpsum_` sum over the ledger's records would see the PRIMARY
/// `QpEngine` only and would be a lower bound, exactly the gap RETAKE8's NET3 and
/// YORKNET findings registered (their whole move sat in unledgered QPs).  So these
/// follow `add_e2_counts`' shape for SqpCounters fields (`qp_maxiter_exits`,
/// `qp_cap_retries`): hven's own whole-solve numbers, spelled as hven spells them.
///
///   `eqp_refine_iters`        solves SPENT by the EQP-site residual check driving
///                             an answer's CONSTRAINT-ROW residual against its own
///                             regularized system down to tolerance (cost, not
///                             steps kept; 0 on every pass whose factor solved its
///                             system, which is the overwhelming majority)
///   `eqp_residual_refusals`   EQP answers that still failed it -- the answer does
///                             not solve the system it came from.  THE ONE TO
///                             WATCH.  Each refusal routes the pass to a retreat
///                             the engine already has and is ADVISORY once the
///                             suspect-stall ladder is exhausted (E4-27), so a
///                             solve can read many refusals and still `optimal`
///   `eqp_polish_steps`        UNREGULARIZED polish steps KEPT inside an EQP solve,
///                             against the face's own exact KKT system with both
///                             regularizations backed out (step cap 1e-6).  0 on a
///                             flat face, where the proximal answer is the only one
///   `terminal_polish_solves`  would-be-certified exits whose final point was
///                             RE-SOLVED mu-free on its final working set (the
///                             polish's stage 2).  ONE FACTORIZATION EACH AND IT IS
///                             **NOT** IN `factorizations` -- see the derived
///                             column below
///   `terminal_polish_rejected` of those, the candidates NOT adopted; a rejection
///                             leaves x and every multiplier as the walk had them,
///                             so it is a cost and never a risk
///   `terminal_polish_adopted` candidates ADOPTED over BOTH stages.  Stage 1 is
///                             FREE (the pass's own EQP answer), so this can EXCEED
///                             `terminal_polish_solves`
///   `eqp_row_drift_refusals`  answers refused by THE STEP'S ROW-DRIFT CHECK: the
///                             answer DID solve its regularized system and still
///                             left a working row the incoming iterate was on, by
///                             the dual_mu*|lambda| term.  Disjoint from
///                             `eqp_residual_refusals` by construction
///
/// THE DERIVED COLUMN, and the accounting contract it carries (hven ruling E4-17):
/// `harness_factorizations_total` = `factorizations` + `terminal_polish_solves`.
/// hven charges the terminal polish's stage-2 factorization to its OWN field and
/// not to `factorizations`, because `factorizations` is the active-set SEARCH's
/// cost -- every budget is written against it and the hot-handle contract is
/// stated as `qp_factorizations == 0` on a reused K0, which a polish that searches
/// nothing must not spend.  The TOTAL a solve paid is the sum, so the harness
/// carries the sum as its own column.  It has the `harness_` prefix because the
/// HARNESS computes it (schema/v1.md's rule: a harness-computed column can never
/// be mistaken for an hven counter); it is arithmetic on the two hven fields
/// beside it and nothing else, and the comparator re-checks the arithmetic on
/// every row of both labels.
///
/// `terminal_mu_refines`' factorization IS in `factorizations`, asymmetrically and
/// on purpose (E4-17 states the asymmetry and declines to repair it: moving it
/// would silently break every pin derived with it there).
void add_e4_counts(CellReport &report, const hven::solvers::SqpCounters &c) {
#define HVEN_BENCH_E4_COL(field) \
    add_side(report, #field, static_cast<long long>(c.field))
    HVEN_BENCH_E4_COL(eqp_refine_iters);
    HVEN_BENCH_E4_COL(eqp_residual_refusals);
    HVEN_BENCH_E4_COL(eqp_polish_steps);
    HVEN_BENCH_E4_COL(terminal_polish_solves);
    HVEN_BENCH_E4_COL(terminal_polish_rejected);
    HVEN_BENCH_E4_COL(terminal_polish_adopted);
    HVEN_BENCH_E4_COL(eqp_row_drift_refusals);
#undef HVEN_BENCH_E4_COL
    add_side(report, "harness_factorizations_total",
             static_cast<long long>(c.factorizations) +
                 static_cast<long long>(c.terminal_polish_solves));
}

/// M7 W2 RETAKE10, a DECLARED SCHEMA EVENT (runs/2026-10-m7-w2-retake10/README.md;
/// hven settler rulings S41, S45 (F8-4) and S47 in `.superpowers/m7-w2-e2-brief.md`):
/// EIGHT columns APPENDED after every earlier hven-sqp side column (after
/// add_e4_counts' eight), none renamed, moved or retired (116 -> 124).
///
/// THREE BARE `SqpCounters` FIELDS FIRST, in `HVEN_SQP_COUNTERS_FIELDS` order -- the
/// `add_e2_counts` shape (`qp_maxiter_exits`, `qp_cap_retries`), because they are
/// hven's own DRIVER-LEVEL numbers.  They are not new in hven (M7 W2 M1, beside
/// `qp_maxiter_exits`); the side file never carried them, and S41 asks for them:
///
///   `qp_numerical_exits`         subproblem exits with kNumericalError at the
///                                driver's routing site (the complement of
///                                `qp_maxiter_exits` there; folded from the
///                                restoration sub-solve)
///   `qp_failures_routed`         subproblem failures the driver ROUTED (shrank the
///                                radius and retried) rather than ending the solve
///                                on.  A subset of `rejected_steps`; NOT folded from
///                                the restoration sub-solve
///   `qp_failures_in_a_row_peak`  the high-water mark of consecutive routed
///                                failures: a PEAK, bounded by
///                                `SqpOptions::qp_failure_budget`
///
/// THEN FIVE `harness_qpsum_` SUMS over the ledger's QP records of the `QpCounters`
/// fields hven's E2 fix rounds 7, 8 and 9 added, in the pinned header's order
/// (include/hven/core/solver_counters.h at 1f8d3118).  `harness_qpsum_` for the same
/// reason as add_e2_counts: every increment site is in the walk's
/// independent-working-set code, the sums cover the PRIMARY QpEngine only, and so
/// each is a lower bound on the cell:
///
///   `standstill_retreats`      (fix9, THE STANDSTILL RETREAT) border-stack passes
///                              that stood still off a structurally violated
///                              equality row with the inertia reading kWrong and
///                              moved the rest of the solve to the eliminated path.
///                              NOT counted in `dep_border_retreats` (one counter per
///                              cause), and at most once per QP solve
///   `dep_unrun_tests`          (fix7) candidate tests that did NOT RUN -- no factor
///                              held the pass's working set, so the integer screen
///                              decided alone
///   `dep_repair_screen_pins`   (fix8) start-repair pins TRIED although the integer
///                              screen had fired (the screen may no longer skip one)
///   `dep_repair_budget_stops`  (fix8) start repairs stopped on the measured pin
///                              budget max(8, ceil(n/2)) -- such a repair fails
///   `dep_repair_restarts`      (fix8, F7-1) passes SPENT after a start repair that
///                              undid itself and took the pass's usable factor
void add_e2_fix789_counts(CellReport &report, const hven::solvers::SqpCounters &c,
                          const hven::solvers::Ledger &ledger) {
#define HVEN_BENCH_FIX789_SQP(field) \
    add_side(report, #field, static_cast<long long>(c.field))
    HVEN_BENCH_FIX789_SQP(qp_numerical_exits);
    HVEN_BENCH_FIX789_SQP(qp_failures_routed);
    HVEN_BENCH_FIX789_SQP(qp_failures_in_a_row_peak);
#undef HVEN_BENCH_FIX789_SQP
    hven::solvers::QpCounters sum{};
    for (const auto &rec : ledger.records()) {
        const auto &q = rec.counters;
#define HVEN_BENCH_FIX789_SUM(field) sum.field += q.field
        HVEN_BENCH_FIX789_SUM(standstill_retreats);
        HVEN_BENCH_FIX789_SUM(dep_unrun_tests);
        HVEN_BENCH_FIX789_SUM(dep_repair_screen_pins);
        HVEN_BENCH_FIX789_SUM(dep_repair_budget_stops);
        HVEN_BENCH_FIX789_SUM(dep_repair_restarts);
#undef HVEN_BENCH_FIX789_SUM
    }
#define HVEN_BENCH_FIX789_COL(field) \
    add_side(report, "harness_qpsum_" #field, static_cast<long long>(sum.field))
    HVEN_BENCH_FIX789_COL(standstill_retreats);
    HVEN_BENCH_FIX789_COL(dep_unrun_tests);
    HVEN_BENCH_FIX789_COL(dep_repair_screen_pins);
    HVEN_BENCH_FIX789_COL(dep_repair_budget_stops);
    HVEN_BENCH_FIX789_COL(dep_repair_restarts);
#undef HVEN_BENCH_FIX789_COL
}

/// M7 W2 RETAKE11, a DECLARED SCHEMA EVENT (runs/2026-10-m7-w2-retake11/README.md; hven
/// settler ruling C5 in `.superpowers/m7-w2-c10-report.md`): THREE columns APPENDED after
/// every earlier hven-sqp side column (after add_e2_fix789_counts' eight), none renamed,
/// moved or retired (124 -> 127).  Sums over the ledger's QP records of the three
/// `QpCounters` fields hven's NC3 round (THE SADDLE EXIT) added, in the pinned header's
/// order (include/hven/core/solver_counters.h at b7993013), `harness_qpsum_` for the same
/// reason as add_e2_fix789_counts: the sums cover the PRIMARY QpEngine only, each is a
/// lower bound on the cell.
///
///   `saddle_exits`             trusted-kWrong dead ends that took a VERIFIED
///                              negative-curvature direction of the face
///   `saddle_exit_misses`       dead ends where no such direction was found and the
///                              kNumericalError exit stood
///   `saddle_direction_solves`  triangular solves the direction search cost (its one
///                              factorization per dead end is in `factorizations`)
void add_nc3_counts(CellReport &report, const hven::solvers::Ledger &ledger) {
    hven::solvers::QpCounters sum{};
    for (const auto &rec : ledger.records()) {
        const auto &q = rec.counters;
#define HVEN_BENCH_NC3_SUM(field) sum.field += q.field
        HVEN_BENCH_NC3_SUM(saddle_exits);
        HVEN_BENCH_NC3_SUM(saddle_exit_misses);
        HVEN_BENCH_NC3_SUM(saddle_direction_solves);
#undef HVEN_BENCH_NC3_SUM
    }
#define HVEN_BENCH_NC3_COL(field) \
    add_side(report, "harness_qpsum_" #field, static_cast<long long>(sum.field))
    HVEN_BENCH_NC3_COL(saddle_exits);
    HVEN_BENCH_NC3_COL(saddle_exit_misses);
    HVEN_BENCH_NC3_COL(saddle_direction_solves);
#undef HVEN_BENCH_NC3_COL
}

/// M7 W2 RETAKE12, a DECLARED SCHEMA EVENT (runs/2026-10-m7-w2-retake12/README.md):
/// FOUR columns APPENDED after every earlier hven-sqp side column (after
/// add_nc3_counts' three), none renamed, moved or retired (127 -> 131), in this order:
///
///   `harness_qpsum_convex_certificates`  `QpCounters::convex_certificates` (NC4), summed
///                              over the ledger's QP records like every harness_qpsum_
///                              column (the primary QpEngine only: a lower bound)
///   `qp_failure_steps_judged`  `SqpCounters` (R2): failed walk subproblems whose own step
///                              the globalization strategy JUDGED (read at the driver level,
///                              folded from the restoration sub-solve)
///   `qp_failure_steps_accepted` `SqpCounters` (R2): of those, the steps the driver took
///   `nonfinite_subproblems`    `SqpCounters` (R2): subproblems built with a non-finite
///                              H, g, A or b entry (the non-finite guard's count)
///
/// Names and order are the pinned header's (include/hven/core/solver_counters.h at
/// 21369067).
void add_nc4_r2_counts(CellReport &report, const hven::solvers::SqpCounters &c,
                       const hven::solvers::Ledger &ledger) {
    long long convex = 0;
    for (const auto &rec : ledger.records()) {
        convex += static_cast<long long>(rec.counters.convex_certificates);
    }
    add_side(report, "harness_qpsum_convex_certificates", convex);
#define HVEN_BENCH_R2_COL(field) \
    add_side(report, #field, static_cast<long long>(c.field))
    HVEN_BENCH_R2_COL(qp_failure_steps_judged);
    HVEN_BENCH_R2_COL(qp_failure_steps_accepted);
    HVEN_BENCH_R2_COL(nonfinite_subproblems);
#undef HVEN_BENCH_R2_COL
}

/// M7 W2 RETAKE13, a DECLARED SCHEMA EVENT (runs/2026-10-m7-w2-retake13/README.md):
/// TWO columns APPENDED after every earlier hven-sqp side column (after
/// add_nc4_r2_counts' four), none renamed, moved or retired (131 -> 133), in this order:
///
///   `harness_qpsum_ascent_rides`   `QpCounters::ascent_rides` (W1, THE ASCENT GUARD),
///                                  summed over the ledger's QP records like every
///                                  harness_qpsum_ column (the primary QpEngine only:
///                                  a lower bound)
///   `nonfinite_box_projections`    `SqpCounters` (W1, GPROJ): non-finite subproblems
///                                  built at an iterate outside its box only by the
///                                  rounding of a step onto a bound, re-evaluated at the
///                                  box projection (read at the driver level, folded
///                                  from the restoration sub-solve)
///
/// Names and order are the pinned header's (include/hven/core/solver_counters.h at
/// 6bcffcd9).
void add_w1_counts(CellReport &report, const hven::solvers::SqpCounters &c,
                   const hven::solvers::Ledger &ledger) {
    long long rides = 0;
    for (const auto &rec : ledger.records()) {
        rides += static_cast<long long>(rec.counters.ascent_rides);
    }
    add_side(report, "harness_qpsum_ascent_rides", rides);
    add_side(report, "nonfinite_box_projections",
             static_cast<long long>(c.nonfinite_box_projections));
}

/// M7 W2 RETAKE14, a DECLARED SCHEMA EVENT (runs/2026-10-m7-w2-retake14/README.md):
/// TWENTY columns APPENDED after every earlier hven-sqp side column (after add_w1_counts'
/// two), none renamed, moved or retired (133 -> 153), in this order.  hven's counters since
/// the previous pin (6bcffcd9) -- `git diff 6bcffcd9 <pin> -- include/hven/core/solver_counters.h`
/// ADDED two `QpCounters` fields and eighteen `SqpCounters` fields, and nothing else:
///
///   `harness_qpsum_border_uncertified_fallbacks`  `QpCounters` (M7 W2 BPS): border-stack
///                              passes whose refinement stopped above its own target and
///                              took the bound-eliminated twin, summed over the ledger's
///                              QP records like every harness_qpsum_ column (the primary
///                              QpEngine only: a lower bound)
///   `harness_qpsum_dep_implied_passovers`  `QpCounters` (M7 W2 QC-8): implied-blocker
///                              pass-overs that survive their sequence, summed likewise
///
/// THEN the eighteen bare `SqpCounters` keys, read at the driver level in the header's
/// `HVEN_SQP_COUNTERS_FIELDS` order: `elastic_last_rung_fallbacks`,
/// `restoration_reduction_resumes` (M7 W2 P1E; both levers default OFF), `eval_hess_calls`
/// (the curvature source's bill: 0 under kLbfgsDamped by construction), and the
/// quasi-Newton group `qn_updates` .. `qn_dimension` (fifteen; all 0 under kExact; the peaks
/// `qn_pairs_peak`, `qn_lift_rank_peak` and the level `qn_dimension` are max-folded).
/// `qn_kwrong_seen` and `qn_nonpd_seen` are hven's bug flags.
///
/// Names and order are the pinned header's (include/hven/core/solver_counters.h).
void add_m7_qn_counts(CellReport &report, const hven::solvers::SqpCounters &c,
                      const hven::solvers::Ledger &ledger) {
    long long border_fallbacks = 0;
    long long implied = 0;
    for (const auto &rec : ledger.records()) {
        border_fallbacks += static_cast<long long>(rec.counters.border_uncertified_fallbacks);
        implied += static_cast<long long>(rec.counters.dep_implied_passovers);
    }
    add_side(report, "harness_qpsum_border_uncertified_fallbacks", border_fallbacks);
    add_side(report, "harness_qpsum_dep_implied_passovers", implied);
#define HVEN_BENCH_QN_COL(field) \
    add_side(report, #field, static_cast<long long>(c.field))
    HVEN_BENCH_QN_COL(elastic_last_rung_fallbacks);
    HVEN_BENCH_QN_COL(restoration_reduction_resumes);
    HVEN_BENCH_QN_COL(eval_hess_calls);
    HVEN_BENCH_QN_COL(qn_updates);
    HVEN_BENCH_QN_COL(qn_damped);
    HVEN_BENCH_QN_COL(qn_skipped_nonfinite);
    HVEN_BENCH_QN_COL(qn_skipped_zero_step);
    HVEN_BENCH_QN_COL(qn_skipped_curvature);
    HVEN_BENCH_QN_COL(qn_skipped_conditioning);
    HVEN_BENCH_QN_COL(qn_rank_dropped);
    HVEN_BENCH_QN_COL(qn_resets_consecutive_skips);
    HVEN_BENCH_QN_COL(qn_resets_radius_floor);
    HVEN_BENCH_QN_COL(qn_resets_restoration_resume);
    HVEN_BENCH_QN_COL(qn_pairs_peak);
    HVEN_BENCH_QN_COL(qn_lift_rank_peak);
    HVEN_BENCH_QN_COL(qn_kwrong_seen);
    HVEN_BENCH_QN_COL(qn_nonpd_seen);
    HVEN_BENCH_QN_COL(qn_dimension);
#undef HVEN_BENCH_QN_COL
}

/// M7 W2 E1B-FIX1-PARTIAL, a DECLARED SCHEMA EVENT (runs/2026-09-m7-w2-e1b-fix1-
/// partial/README.md): 31 columns APPENDED after every earlier hven-sqp side
/// column, none renamed or moved.  hven's own `SqpResult::equality_basis`
/// (SqpEqualityBasisReport, drivers/sqp_solver_types.h at the pin), field for
/// field in the header's order.  The `eqb_` prefix namespaces them; they are
/// HVEN's numbers, not the harness's, so they do not carry `harness_`
/// (schema/v1.md).  Spelling: bools 1/0; the refusal as its enumerator name; a
/// row list `;`-joined, increasing as hven holds it, `none` when empty.  hven
/// caps `split_dropped_rows` itself (kSplitDroppedRowsCap); the two vectors it
/// does NOT cap (`dropped_rows`, `inconsistent_support`) are written as their
/// size plus their first kSplitDroppedRowsCap entries -- the one piece of
/// formatting here that is the harness's, declared on the README.
const char *eqb_refusal_name(hven::solvers::SqpEqualityBasisRefusal r) {
    using R = hven::solvers::SqpEqualityBasisRefusal;
    switch (r) {  // no default: a new enumerator is a -Wswitch warning, not a silent spelling
    case R::kNone:
        return "kNone";
    case R::kSpectrum:
        return "kSpectrum";
    case R::kNonFinite:
        return "kNonFinite";
    case R::kLift:
        return "kLift";
    case R::kCertificate:
        return "kCertificate";
    case R::kSubspace:
        return "kSubspace";
    case R::kFill:
        return "kFill";
    }
    return "UNKNOWN";
}

std::string eqb_rows(const std::vector<hven::Index> &rows) {
    constexpr std::size_t cap =
        static_cast<std::size_t>(hven::solvers::SqpEqualityBasisReport::kSplitDroppedRowsCap);
    if (rows.empty()) {
        return "none";
    }
    std::string out;
    for (std::size_t i = 0; i < rows.size() && i < cap; ++i) {
        if (i != 0) {
            out += ';';
        }
        out += std::to_string(static_cast<long long>(rows[i]));
    }
    return out;
}

void add_eqb_report(CellReport &report, const hven::solvers::SqpEqualityBasisReport &e) {
    const auto n = [](hven::Index v) { return static_cast<long long>(v); };
    const auto b = [](bool v) { return static_cast<long long>(v ? 1 : 0); };
    add_side(report, "eqb_computed", b(e.computed));
    add_side(report, "eqb_undecidable", b(e.undecidable));
    add_side(report, "eqb_undecidable_count", n(e.undecidable_count));
    add_side(report, "eqb_last_refusal", std::string(eqb_refusal_name(e.last_refusal)));
    add_side(report, "eqb_split_dropped_count", n(e.split_dropped_count));
    add_side(report, "eqb_split_dropped_rows", eqb_rows(e.split_dropped_rows));
    add_side(report, "eqb_dropped_rows_count", static_cast<long long>(e.dropped_rows.size()));
    add_side(report, "eqb_dropped_rows", eqb_rows(e.dropped_rows));
    add_side(report, "eqb_dropped_peak", n(e.dropped_peak));
    add_side(report, "eqb_recomputes", n(e.recomputes));
    add_side(report, "eqb_gram_factorizations", n(e.gram_factorizations));
    add_side(report, "eqb_gram_perturbed_pivots", n(e.gram_perturbed_pivots));
    add_side(report, "eqb_gram_perturbation_unobserved", n(e.gram_perturbation_unobserved));
    add_side(report, "eqb_switched_off", b(e.switched_off));
    add_side(report, "eqb_nonfinite_refusals", n(e.nonfinite_refusals));
    add_side(report, "eqb_lift_refusals", n(e.lift_refusals));
    add_side(report, "eqb_warm_probe_refusals", n(e.warm_probe_refusals));
    add_side(report, "eqb_lift_refusals_compute", n(e.lift_refusal_sites.compute));
    add_side(report, "eqb_lift_refusals_qp_projection", n(e.lift_refusal_sites.qp_projection));
    add_side(report, "eqb_lift_refusals_soc_projection", n(e.lift_refusal_sites.soc_projection));
    add_side(report, "eqb_lift_refusals_commit_lift", n(e.lift_refusal_sites.commit_lift));
    add_side(report, "eqb_lift_refusals_prox_centre", n(e.lift_refusal_sites.prox_centre));
    add_side(report, "eqb_split_changes", n(e.split_changes));
    add_side(report, "eqb_handed_off", b(e.handed_off));
    add_side(report, "eqb_inconsistent_row", n(e.inconsistent_row));
    add_side(report, "eqb_inconsistent_gap", e.inconsistent_gap);
    add_side(report, "eqb_inconsistent_support_count",
             static_cast<long long>(e.inconsistent_support.size()));
    add_side(report, "eqb_inconsistent_support", eqb_rows(e.inconsistent_support));
    add_side(report, "eqb_evidence_major_first", n(e.evidence_major_first));
    add_side(report, "eqb_evidence_major_second", n(e.evidence_major_second));
    add_side(report, "eqb_max_gap", e.max_gap);
}

CellReport run_sqp(const ProblemSource &source, const std::string &options_label,
                   long long budget_iters, const std::string &options_dump_path) {
    const auto problem = source.make_model();
    auto model = std::make_shared<hven::solvers::NlpProblemModel>(problem);

    auto options = sqp_options(options_label);
    if (!options_dump_path.empty()) {
        std::ofstream dump(options_dump_path, std::ios::binary | std::ios::trunc);
        hven::solvers::dump_options(options, dump);
    }
    hven::solvers::SqpSolver solver(options);
    // Observation only (M7 W2 E2-RETAKE schema event): the per-QP counters E2
    // added live on QpCounters, which SqpCounters does not aggregate, so the
    // lane reads them off hven's own ledger (one SolveRecord per solve of the
    // driver's PRIMARY QpEngine; the cap-retry engine and the restoration
    // sub-driver are not recorded there, by hven's attach_ledger contract).
    hven::solvers::Ledger ledger;
    solver.attach_ledger(&ledger, "cell");

    hven::solvers::SolveBudget budget{};
    budget.max_iterations = static_cast<hven::Index>(budget_iters);

    std::unique_ptr<std::ofstream> rd8_trace_file;
    std::unique_ptr<hven::solvers::JsonLinesTraceSink> rd8_trace;
    if (const char *tp = std::getenv("RD8_TRACE"); tp != nullptr && *tp != 0) {
        rd8_trace_file = std::make_unique<std::ofstream>(tp);
        rd8_trace = std::make_unique<hven::solvers::JsonLinesTraceSink>(*rd8_trace_file);
        solver.attach_trace(rd8_trace.get());
    }
    CellReport report;
    report.budget_iters = effective_cap(budget_iters, static_cast<long long>(options.max_iter));
    const auto sqp_result = solve_shared(solver, *model, source.x0(), budget, *problem);
    fill_shared(sqp_result, report);

    report.factorizations = static_cast<long long>(sqp_result.counters.factorizations);

    const auto &c = sqp_result.counters;
    add_side(report, "major_iters", static_cast<long long>(c.major_iters));
    add_side(report, "qp_minor_iters", static_cast<long long>(c.qp_minor_iters));
    add_side(report, "factorizations", static_cast<long long>(c.factorizations));
    add_side(report, "steps_accepted", static_cast<long long>(c.steps_accepted));
    add_side(report, "rejected_steps", static_cast<long long>(c.rejected_steps));
    add_side(report, "soc_steps", static_cast<long long>(c.soc_steps));
    add_side(report, "soc_applied", static_cast<long long>(c.soc_applied));
    add_side(report, "elastic_activations", static_cast<long long>(c.elastic_activations));
    add_side(report, "restoration_iters", static_cast<long long>(c.restoration_iters));
    add_side(report, "eqp_refine_steps", static_cast<long long>(c.eqp_refine_steps));
    add_side(report, "border_refine_steps", static_cast<long long>(c.border_refine_steps));
    add_side(report, "verdict_refine_steps", static_cast<long long>(c.verdict_refine_steps));
    add_side(report, "suspect_escalations", static_cast<long long>(c.suspect_escalations));
    add_side(report, "symbolic_analyses", static_cast<long long>(c.symbolic_analyses));
    add_side(report, "full_step_majors", static_cast<long long>(c.full_step_majors));
    add_side(report, "watchdog_restores", static_cast<long long>(c.watchdog_restores));
    add_side(report, "evals_full", static_cast<long long>(c.evals_full));
    add_side(report, "evals_values", static_cast<long long>(c.evals_values));
    add_side(report, "sqp_stationarity", sqp_result.sqp_stationarity);
    add_side(report, "sqp_feasibility", sqp_result.sqp_feasibility);
    add_side(report, "sqp_complementarity", sqp_result.sqp_complementarity);
    add_side(report, "sqp_kkt_residual", sqp_result.kkt_residual);

    // THE ONLY FIELD THAT TELLS THE TWO kInfeasible SHAPES APART, and the row
    // is unreadable without it.  hven's own contract says so
    // (core/solver_status.h: "THE ONLY FIELD DISTINGUISHING THE TWO SHAPES IS
    // SqpResult::infeasibility_certified. counters.restoration_iters does NOT
    // distinguish them ... Nor does the last history row"), and it is a one-way
    // guarantee: true means the certificate holds, false means NO CLAIM IS MADE
    // ABOUT THE MODEL.  Without it a `kInfeasible` row cannot say whether the
    // engine certified the model infeasible or merely could not make progress
    // -- which is exactly the question YORKNET raises, where this lane reports
    // infeasible and the interior-point lane returns `optimal` on the same
    // declared model.  Spelled 1/0 rather than true/false to match the side
    // file's integer columns.
    add_side(report, "sqp_infeasibility_certified",
             static_cast<long long>(sqp_result.infeasibility_certified ? 1 : 0));
    add_harness_counts(report, *problem);
    add_e2_counts(report, c, ledger);
    add_eqb_report(report, sqp_result.equality_basis);
    add_e2_fix1_counts(report, ledger);
    add_e2_fix2_counts(report, ledger);
    add_e2_fix345_counts(report, ledger);
    add_e2_fix6_counts(report, ledger);
    add_nc1_counts(report, ledger);
    add_nc2_counts(report, ledger);
    add_e4_counts(report, c);
    add_e2_fix789_counts(report, c, ledger);
    add_nc3_counts(report, ledger);
    add_nc4_r2_counts(report, c, ledger);
    add_w1_counts(report, c, ledger);
    add_m7_qn_counts(report, c, ledger);
    return report;
}

// ---------------------------------------------------------------------------
// The CLI
// ---------------------------------------------------------------------------
//
// THE ROSTER MOVED at M7 W1 H4, unchanged, to `lanes/roster.cpp`.  There are now
// three lane binaries -- this one, `hven_bench_ipopt_cell` and
// `hven_bench_snopt_cell` -- and a problem list spelled once per binary is the
// second spelling that goes stale; a four-lane table whose lanes ran on subtly
// different rosters would be comparing solvers on different sets and saying so
// nowhere.  `runners/run_serial.py` re-proves on every run that all three print
// byte-identical `--list` output.

int usage() {
    std::fprintf(stderr,
                 "usage: hven_bench_cell --lane hven-ipm|hven-sqp --problem NAME\n"
                 "                       [--options LABEL] [--budget-iters N (0 = the\n"
                 "                       engine's own cap; N can only tighten it)]\n"
                 "                       [--options-dump PATH] [--assert-reference]\n"
                 "       hven_bench_cell --lane LANE --options LABEL \n"
                 "                       --dump-options-only [--options-dump PATH]\n"
                 "       hven_bench_cell --list\n");
    return 2;
}

} // namespace

int main(int argc, char **argv) {
    std::string lane;
    std::string problem;
    std::string options_label = "default";
    std::string options_dump_path;
    long long budget_iters = 0;
    bool assert_reference = false;
    bool dump_only = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&](const char *what) -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "hven_bench_cell: %s needs a value\n", what);
                std::exit(usage());
            }
            return argv[++i];
        };
        if (arg == "--list") {
            print_roster();
            return 0;
        } else if (arg == "--lane") {
            lane = next("--lane");
        } else if (arg == "--problem") {
            problem = next("--problem");
        } else if (arg == "--options") {
            options_label = next("--options");
        } else if (arg == "--options-dump") {
            options_dump_path = next("--options-dump");
        } else if (arg == "--budget-iters") {
            budget_iters = std::stoll(next("--budget-iters"));
        } else if (arg == "--assert-reference") {
            assert_reference = true;
        } else if (arg == "--dump-options-only") {
            dump_only = true;
        } else {
            std::fprintf(stderr, "hven_bench_cell: unknown argument %s\n", arg.c_str());
            return usage();
        }
    }
    // `--dump-options-only` takes NO problem: the dump and the verdict are
    // properties of the label, so demanding a problem would tie a label's
    // record to whichever cell happened to be named when it was taken.
    if (dump_only) {
        if (lane.empty()) {
            return usage();
        }
        return dump_options_only(lane, options_label, options_dump_path);
    }
    if (lane.empty() || problem.empty()) {
        return usage();
    }

    std::shared_ptr<ProblemSource> source;
    for (const auto &candidate : roster()) {
        if (candidate->problem() == problem) {
            source = candidate;
            break;
        }
    }
    if (!source) {
        std::fprintf(stderr, "hven_bench_cell: no problem named %s\n", problem.c_str());
        return 2;
    }

    CellReport report;
    try {
        if (lane == "hven-ipm") {
            report = run_ipm(*source, options_label, budget_iters, options_dump_path);
        } else if (lane == "hven-sqp") {
            report = run_sqp(*source, options_label, budget_iters, options_dump_path);
        } else {
            std::fprintf(stderr, "hven_bench_cell: unknown lane %s\n", lane.c_str());
            return usage();
        }
    } catch (const std::exception &e) {
        // The row is still written -- an exception is an OUTCOME of the cell,
        // not an absence of one -- and the runner normalises it to `error`.
        std::printf("CELL_BEGIN\n");
        emit("lane", lane);
        emit("problem", problem);
        emit("options_label", options_label);
        emit("threw", "1");
        emit("exception", std::string(e.what()).substr(0, 300));
        std::printf("CELL_END\n");
        std::fprintf(stderr, "hven_bench_cell: %s\n", e.what());
        return 0;
    }

    std::printf("CELL_BEGIN\n");
    emit("lane", lane);
    emit("problem", problem);
    emit("problem_set", source->problem_set());
    emit("set_version", source->set_version());
    emit("source_path", source->source_path());
    emit("hessian_provenance",
         lane_hessian_provenance(lane, options_label, source->hessian_provenance()));
    emit("options_label", options_label);
    emit("hven_pin_sha", HVEN_BENCH_PIN_SHA);
    emit("threw", "0");
    emit("budget_iters", spell(report.budget_iters));
    emit("status", report.status);
    emit("outcome", report.outcome);
    emit("iterations", spell(report.iterations));
    emit("factorizations", spell(report.factorizations));
    emit("objective", spell(report.objective));
    emit("kkt_stationarity", spell(report.stationarity));
    emit("kkt_primal_eq", spell(report.feasibility_e));
    emit("kkt_primal_ineq", spell(report.feasibility_i));
    emit("kkt_complementarity", spell(report.complementarity));
    for (const auto &entry : report.side) {
        std::printf("side.%s=%s\n", entry.first.c_str(), entry.second.c_str());
    }

    // THE REFERENCE CHECK IS THE LANE'S, not the comparator's.  A comparator
    // gate would only say two runs agree; it would say nothing about whether
    // they agree on the right answer.  This says so, here, against the set's
    // own published optimum.
    //
    // THE BAR IS 1e-6 RELATIVE, WITH AN ABSOLUTE FLOOR OF 1e-6.  H1's single
    // problem had f* = 17 and an absolute 1e-6 was the same thing; the HS set
    // does not, and both arms are needed.  Seven of its twenty-seven problems
    // have f* = 0 exactly, where a relative bar is not a bar at all, and HS15's
    // f* = 306.5 and HS43's -44 are where an absolute 1e-6 asks for more digits
    // than the published optimum has.  So the bound is
    // `1e-6 * max(1, |f*|)` -- relative where the optimum is large, absolute
    // where it is small, which is the same mixed rule hven's derivative checker
    // uses and for the same reason.
    int exit_code = 0;
    const auto reference = source->reference_objective();
    if (reference) {
        const double error = std::abs(report.objective - *reference);
        const double bound = 1e-6 * std::max(1.0, std::abs(*reference));
        emit("reference_objective", spell(*reference));
        emit("reference_abs_err", spell(error));
        emit("reference_bound", spell(bound));
        const bool ok = !report.converged || (error <= bound);
        emit("reference_ok", ok ? "1" : "0");
        if (!ok && assert_reference) {
            // NOT A FAILURE OF THE HARNESS, and the message says so: several HS
            // problems have known alternate local optima, and a cell that
            // converged to one is REPORTED BY NAME with its objective rather
            // than tuned until it agrees.
            std::fprintf(stderr,
                         "hven_bench_cell: %s/%s converged to %.17g, reference %.17g "
                         "(|err| = %.3e > %.3e)\n",
                         lane.c_str(), problem.c_str(), report.objective, *reference, error,
                         bound);
            exit_code = 3;
        }
    } else {
        emit("reference_objective", "null");
        emit("reference_abs_err", "null");
        emit("reference_bound", "null");
        emit("reference_ok", "null");
    }
    std::printf("CELL_END\n");
    return exit_code;
}
