// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

// =============================================================================
// The interior-point engine's option validation and presets.
//
// This file was src/drivers/interior_point_solver_settings.cpp until M6 W5
// T8.3, when the options became a value: the ~50 validated set_*() methods, the
// four strto_*() string-to-enum parsers and the six string-taking setter
// overloads were DELETED, IpmSolver::Settings::validate() became the
// free function validate(const IpmOptions &), and apply_preset() -- which
// mutated a solver's settings in place -- became ipm_preset(), which RETURNS a
// full IpmOptions value. Nothing here touches a solve; the algorithm lives in
// ipm_solver.cpp, the iteration/exit reporting in
// interior_point_solver_print.cpp, and the globalization components in
// ipm_solver_globalization.cpp.
// =============================================================================

#include "hven/drivers/ipm_options.h"
#include "hven/detail/drivers/common_options_validate.h"
#include "hven/detail/drivers/ipm_solver_presets.h"
#include "hven/detail/model/fixed_variable_treatment.h"

#include <fmt/format.h>

#include <cmath>
#include <cstddef>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
// Name of a non-classic acceptance strategy, for the validate() error message
// below. classic_merit never reaches this helper (the guard that calls it is
// gated on acceptance_strategy_ != classic_merit).
const char *acceptance_strategy_name(hven::solvers::AcceptanceKind strategy) {
    using hven::solvers::AcceptanceKind;
    switch (strategy) {
    case AcceptanceKind::kMerit:
        return "kMerit";
    case AcceptanceKind::kFunnel:
        return "kFunnel";
    case AcceptanceKind::kFilter:
        return "kFilter";
    case AcceptanceKind::kClassicMerit:
        return "kClassicMerit";
    }
    return "unknown";
}

// Validation helpers for validate() below. Until M6 W5 T8.3 every numeric-field
// invariant was checked TWICE -- once in that field's set_*() method, once again
// over the whole struct at run_phase_sequence() entry -- and these helpers
// existed so the two call sites could not drift apart in condition or in
// message. The setters are gone and validate() is the single home now, so the
// helpers no longer carry that pairing duty; they stay because they keep the
// messages uniform across the ~40 fields that share a condition.
void pos_finite(double v, const char *name) {
    if (!std::isfinite(v) || v <= 0.0)
        throw std::invalid_argument(fmt::format("{} must be finite and positive, got {}", name, v));
}

void pos_int(long long v, const char *name) {
    if (v < 1)
        throw std::invalid_argument(fmt::format("{} must be >= 1, got {}", name, v));
}

// Written as negated comparisons so that a NaN, which compares false against
// everything, is rejected rather than let through.
void in_open_unit(double v, const char *name) {
    if (!(v > 0.0 && v < 1.0))
        throw std::invalid_argument(fmt::format("{} must be in (0, 1), got {}", name, v));
}

// All four current callers (bound_push, alpha_red, delta_h, incr_h) feed a
// magnitude/rate that downstream arithmetic uses directly -- an interior push
// distance, a backtracking divisor applied as alpha /= v each rejected trial,
// a Hessian-diagonal perturbation, and that perturbation's growth factor.
// None of the four has an "infinity means disabled" reading anywhere in the
// solver: bound_push=inf would push the initial iterate to infinity,
// alpha_red=inf collapses backtracking to a single all-or-nothing trial
// (alpha/inf == 0), and delta_h=inf or incr_h=inf puts an infinite entry on
// the KKT diagonal on the very first perturbation/growth step. +inf is
// therefore refused for all four, same as NaN -- this helper requires
// finiteness, not just "greater than bound".
void greater_than(double v, double bound, const char *name) {
    if (!std::isfinite(v) || !(v > bound))
        throw std::invalid_argument(
            fmt::format("{} must be finite and greater than {}, got {}", name, bound, v));
}

// The two range helpers are written as negated comparisons so that a NaN, which
// compares false against everything, is rejected rather than let through.
void in_open_interval(double v, double lo, double hi, const char *name) {
    if (!(v > lo) || !(v < hi))
        throw std::invalid_argument(fmt::format("{} must be in ({}, {}), got {}", name, lo, hi, v));
}

void in_closed_interval(double v, double lo, double hi, const char *name) {
    if (!(v >= lo) || !(v <= hi))
        throw std::invalid_argument(fmt::format("{} must be in [{}, {}], got {}", name, lo, hi, v));
}

// A closed-set enum still reaches IpmOptions through a plain aggregate field, so
// a value outside the set is a reachable input. Written as a switch with no default
// label so that adding a treatment makes the compiler point here.
void non_negative(long long v, const char *name) {
    if (v < 0)
        throw std::invalid_argument(fmt::format("{} must be non-negative, got {}", name, v));
}

// An enum field is a plain aggregate member, so a value outside its enumerators
// is a reachable input. `last` is the final enumerator.
template <typename E> void enum_in_range(E v, E last, const char *name) {
    const auto i = static_cast<long long>(v);
    if (i < 0 || i > static_cast<long long>(last))
        throw std::invalid_argument(fmt::format("{} is out of range, got {}", name, i));
}

void check_fixed_variable_treatment(hven::solvers::FixedVariableTreatment treatment) {
    using hven::solvers::FixedVariableTreatment;
    switch (treatment) {
    case FixedVariableTreatment::kMakeParameter:
    case FixedVariableTreatment::kMakeConstraint:
    case FixedVariableTreatment::kRelaxBounds:
        return;
    }
    throw std::invalid_argument(fmt::format(
        "fixed_variable_treatment must be one of {}, {}, {} (got {})",
        hven::solvers::fixed_variable_treatment_name(FixedVariableTreatment::kMakeParameter),
        hven::solvers::fixed_variable_treatment_name(FixedVariableTreatment::kMakeConstraint),
        hven::solvers::fixed_variable_treatment_name(FixedVariableTreatment::kRelaxBounds),
        static_cast<int>(treatment)));
}

} // namespace

// Option validation

void hven::solvers::validate(const IpmOptions &o) {
    const IpmTerminationOptions &t = o.termination;
    const IpmLinearOptions &lin = o.linear;
    const IpmAdvancedOptions &adv = o.advanced;

    // --- The phase sequence ---
    // A solve that runs no phase has nothing to report and is far more likely a
    // caller's mistake than an intention. Any non-empty order of the two phases
    // is legal.
    if (o.phases.empty()) {
        throw std::invalid_argument("phases must name at least one phase; an empty sequence would "
                                    "run nothing and report nothing");
    }
    for (IpmPhase ph : o.phases)
        enum_in_range(ph, IpmPhase::kSolve, "phases[i]");

    // --- The shared options ---
    detail::validate_common_options(o.common, {.max_iterations = 1, .threads = 1});

    // --- Termination ---
    // complementarity_tol, when empty, is optimality_tol: that is the value the
    // barrier test reads and the one the ordering checks below compare.
    if (t.complementarity_tol)
        pos_finite(*t.complementarity_tol, "termination.complementarity_tol");
    const double comp_tol = t.complementarity_tol.value_or(o.common.optimality_tol);
    pos_int(t.acceptable_iterations, "termination.acceptable_iterations");
    pos_finite(t.acceptable_kkt_tol, "termination.acceptable_kkt_tol");
    pos_finite(t.acceptable_equality_tol, "termination.acceptable_equality_tol");
    pos_finite(t.acceptable_inequality_tol, "termination.acceptable_inequality_tol");
    pos_finite(t.acceptable_barrier_tol, "termination.acceptable_barrier_tol");
    pos_finite(t.divergence_kkt_tol, "termination.divergence_kkt_tol");
    pos_finite(t.divergence_equality_tol, "termination.divergence_equality_tol");
    pos_finite(t.divergence_inequality_tol, "termination.divergence_inequality_tol");
    pos_finite(t.divergence_barrier_tol, "termination.divergence_barrier_tol");

    // --- Cross-field: convergence tol <= acceptable tol <= divergence tol ---
    const auto ordered = [](double lo, const char *lo_name, double hi, const char *hi_name) {
        if (lo > hi)
            throw std::invalid_argument(
                fmt::format("{} ({}) must be <= {} ({})", lo_name, lo, hi_name, hi));
    };
    ordered(o.common.optimality_tol, "common.optimality_tol", t.acceptable_kkt_tol,
            "termination.acceptable_kkt_tol");
    ordered(o.common.feasibility_tol, "common.feasibility_tol", t.acceptable_equality_tol,
            "termination.acceptable_equality_tol");
    ordered(o.common.feasibility_tol, "common.feasibility_tol", t.acceptable_inequality_tol,
            "termination.acceptable_inequality_tol");
    ordered(comp_tol,
            t.complementarity_tol ? "termination.complementarity_tol"
                                  : "common.optimality_tol (the complementarity tolerance)",
            t.acceptable_barrier_tol, "termination.acceptable_barrier_tol");
    ordered(t.acceptable_kkt_tol, "termination.acceptable_kkt_tol", t.divergence_kkt_tol,
            "termination.divergence_kkt_tol");
    ordered(t.acceptable_equality_tol, "termination.acceptable_equality_tol",
            t.divergence_equality_tol, "termination.divergence_equality_tol");
    ordered(t.acceptable_inequality_tol, "termination.acceptable_inequality_tol",
            t.divergence_inequality_tol, "termination.divergence_inequality_tol");
    ordered(t.acceptable_barrier_tol, "termination.acceptable_barrier_tol",
            t.divergence_barrier_tol, "termination.divergence_barrier_tol");

    // --- Barrier parameters ---
    pos_finite(o.initial_barrier, "initial_barrier");
    pos_finite(o.min_barrier, "min_barrier");
    pos_finite(o.max_barrier, "max_barrier");
    if (o.min_barrier > o.max_barrier)
        throw std::invalid_argument(fmt::format("min_barrier ({}) must be <= max_barrier ({})",
                                                o.min_barrier, o.max_barrier));
    if (o.initial_barrier < o.min_barrier || o.initial_barrier > o.max_barrier)
        throw std::invalid_argument(
            fmt::format("initial_barrier ({}) must be within [min_barrier ({}), max_barrier ({})]",
                        o.initial_barrier, o.min_barrier, o.max_barrier));

    // --- Step parameters ---
    in_open_unit(o.fraction_to_boundary, "fraction_to_boundary");
    greater_than(o.bound_push, 0.0, "bound_push");
    // The interior push gives a two-sided variable p_L + p_U <= 2*k2*(u-l) of
    // its own interval, so the lower and upper projections can only cross --
    // landing the point on or outside a bound -- once k2 reaches one half.
    in_open_interval(o.bound_interval_push, 0.0, 0.5, "bound_interval_push");

    // --- Fixed-variable treatment ---
    // Zero is allowed: it records every declared bound verbatim. The ceiling
    // keeps a widened box from quietly becoming a different problem than the
    // declared one (kMaxBoundRelaxFactor).
    in_closed_interval(o.bound_relax_factor, 0.0, kMaxBoundRelaxFactor, "bound_relax_factor");
    check_fixed_variable_treatment(o.fixed_variable_treatment);
    // kRelaxBounds holds a fixed variable between bounds pushed apart by this
    // factor; at 0 there is no interval to hold it in.
    if (o.fixed_variable_treatment == FixedVariableTreatment::kRelaxBounds &&
        !(o.bound_relax_factor > 0.0))
        throw std::invalid_argument(fmt::format(
            "bound_relax_factor must be > 0 under fixed_variable_treatment = kRelaxBounds, got {}",
            o.bound_relax_factor));

    // --- Objective ---
    // POSITIVE, not merely nonzero: a negative factor reverses the problem
    // (minimizing s*f for s < 0 maximizes f) while the multiplier cones the
    // solve reports against do not turn over with it.
    if (!std::isfinite(o.objective_scale) || o.objective_scale <= 0.0)
        throw std::invalid_argument(fmt::format(
            "objective_scale must be finite and strictly positive, got {}", o.objective_scale));

    enum_in_range(o.best_criterion, BestIterateCriterion::kObjective, "best_criterion");

    // --- Linear ---
    enum_in_range(lin.ordering, KktOrdering::kParallelNestedDissection, "linear.ordering");
    enum_in_range(lin.pivoting, KktPivoting::kTwoByTwo, "linear.pivoting");
    non_negative(lin.pivot_perturbation, "linear.pivot_perturbation");
    non_negative(lin.refinement_steps, "linear.refinement_steps");
    enum_in_range(lin.parallel_solve, KktSolveParallelism::kSequential, "linear.parallel_solve");
#ifdef USE_ACCELERATE_SPARSE
    pos_finite(lin.accelerate_zero_tolerance, "linear.accelerate_zero_tolerance");
#endif
    // THE DENSE ARM IS NAMED, NOT WIRED. Accepting another value would be a
    // setting that silently does nothing.
    if (lin.dense_factorization != hven::linear::DensePolicy::kNever)
        throw std::invalid_argument(
            fmt::format("linear.dense_factorization = {} is reserved: the dense factorization arm "
                        "is not available yet; only DensePolicy::kNever is accepted",
                        static_cast<int>(lin.dense_factorization)));
    if (lin.dense_max_dimension)
        throw std::invalid_argument(
            "linear.dense_max_dimension is reserved: the dense factorization arm is not available "
            "yet; leave it empty");

    // --- Advanced: iteration and attempt budgets ---
    // 0 disables each of these outright, which is well-defined; negative is not.
    // inertia_correction_max_attempts = 0 disables the inertia ladder: an
    // unperturbed factorization with wrong inertia exhausts immediately.
    non_negative(adv.inertia_correction_max_attempts, "advanced.inertia_correction_max_attempts");
    non_negative(adv.line_search_max_backtracks, "advanced.line_search_max_backtracks");
    non_negative(adv.max_second_order_corrections, "advanced.max_second_order_corrections");
    non_negative(adv.line_search_extended_backtracks, "advanced.line_search_extended_backtracks");
    non_negative(adv.restoration_max_entries, "advanced.restoration_max_entries");

    enum_in_range(adv.acceptance_strategy, AcceptanceKind::kFilter, "advanced.acceptance_strategy");
    enum_in_range(adv.merit_penalty_rule, MeritPenaltyRule::kFlexible,
                  "advanced.merit_penalty_rule");
    enum_in_range(adv.barrier_governor, BarrierGovernorKind::kMonitored,
                  "advanced.barrier_governor");
    enum_in_range(adv.restoration_mode, RestorationMode::kL1Nested, "advanced.restoration_mode");
    enum_in_range(adv.inertia_mode, InertiaMode::kProximalRegularization, "advanced.inertia_mode");
    enum_in_range(adv.solve_phase_mode, AlgorithmMode::kInit, "advanced.solve_phase_mode");
    enum_in_range(adv.optimize_barrier_mode, BarrierMode::kLoqo, "advanced.optimize_barrier_mode");
    enum_in_range(adv.solve_barrier_mode, BarrierMode::kLoqo, "advanced.solve_barrier_mode");
    enum_in_range(adv.optimize_line_search_mode, LineSearchMode::kNoLs,
                  "advanced.optimize_line_search_mode");
    enum_in_range(adv.solve_line_search_mode, LineSearchMode::kNoLs,
                  "advanced.solve_line_search_mode");
    enum_in_range(adv.primal_dual_step_strategy, PrimalDualStepStrategy::kMaxEq,
                  "advanced.primal_dual_step_strategy");

    // --- Advanced: strategy-combination guards ---
    // The SOC and extended-backtracking recovery links re-drive the acceptance
    // backtrack through the same acceptance test the ordinary step faced, so
    // both compose with every acceptance kind.
    //
    // Funnel and filter are designed to operate above a monotone barrier
    // safeguard. kClassicAdaptive is free-mode only, so pairing it with them
    // drops that safeguard unless the caller opts in via never_monotone.
    if ((adv.acceptance_strategy == AcceptanceKind::kFunnel ||
         adv.acceptance_strategy == AcceptanceKind::kFilter) &&
        adv.barrier_governor == BarrierGovernorKind::kClassicAdaptive && !adv.never_monotone)
        throw std::invalid_argument(fmt::format(
            "advanced.acceptance_strategy = {} is designed to operate above the monotone barrier "
            "safeguard, which advanced.barrier_governor = kClassicAdaptive does not provide: set "
            "advanced.barrier_governor = kMonitored (the default), or set advanced.never_monotone "
            "= true to explicitly accept adaptive-only operation",
            acceptance_strategy_name(adv.acceptance_strategy)));
    // never_monotone accepts running WITHOUT a monotone safeguard; kMonitored
    // provides one, so the combination is a direct contradiction.
    if (adv.never_monotone && adv.barrier_governor == BarrierGovernorKind::kMonitored)
        throw std::invalid_argument(
            "advanced.never_monotone = true is contradictory with advanced.barrier_governor = "
            "kMonitored: the monitored governor already provides the monotone safeguard "
            "never_monotone opts out of; set advanced.barrier_governor = kClassicAdaptive or "
            "advanced.never_monotone = false");

    // --- Advanced: step and inertia-correction parameters ---
    pos_finite(adv.negative_slack_reset, "advanced.negative_slack_reset");
    greater_than(adv.backtrack_divisor, 1.0, "advanced.backtrack_divisor");
    greater_than(adv.inertia_correction_initial, 0.0, "advanced.inertia_correction_initial");
    greater_than(adv.inertia_correction_increase, 1.0, "advanced.inertia_correction_increase");
    in_open_unit(adv.inertia_correction_decrease, "advanced.inertia_correction_decrease");
}

static_assert(hven::solvers::kDefaultBoundRelaxFactor == 1.0e-8,
              "kDefaultBoundRelaxFactor must equal IpmOptions' bound_relax_factor default");

// Named configuration presets

hven::solvers::IpmOptions hven::solvers::ipm_preset(std::string_view name) {
    IpmOptions o;
    for (const auto &entry : kInteriorPointSolverPresets) {
        if (entry.name_ != name)
            continue;
        const InteriorPointSolverPresetFields &f = entry.fields_;
        IpmAdvancedOptions &adv = o.advanced;
        adv.acceptance_strategy = f.acceptance_strategy_;
        adv.merit_penalty_rule = f.merit_penalty_rule_;
        adv.barrier_governor = f.barrier_governor_;
        adv.never_monotone = f.never_monotone_;
        adv.restoration_mode = f.restoration_mode_;
        adv.inertia_mode = f.inertia_mode_;
        adv.max_second_order_corrections = f.max_soc_;
        adv.line_search_extended_backtracks = f.ls_extended_iters_;
        adv.watchdog = f.watchdog_;
        return o;
    }

    // Unrecognized name: fold the full valid-name list into the exception
    // message rather than printing it separately -- kInteriorPointSolverPresets drives
    // this message directly; the Python binding's docstring repeats the names
    // by hand, and a Python test pins it against this table.
    std::string valid_names;
    for (std::size_t i = 0; i < kInteriorPointSolverPresets.size(); ++i) {
        if (i != 0)
            valid_names += ", ";
        valid_names += kInteriorPointSolverPresets[i].name_;
    }
    throw std::invalid_argument(fmt::format(
        "Unrecognized IpmSolver preset '{}'. Valid options are: {}", name, valid_names));
}

hven::solvers::IpmOptions hven::solvers::ipm_worker_options(IpmOptions base) {
    // One backend thread and silent output. The partition count is the
    // PROGRAM's, documented on the declaration -- no options value reaches a
    // layout.
    base.common.threads = 1;
    base.common.verbosity = Verbosity::kSilent;
    return base;
}

// =============================================================================
// M7 W0 T5(b): the options dump.
//
// A hand-written printer, one line per field, pinned against IpmOptions by
// tests/drivers/test_options_dump.cpp. The contract -- order, prefixes,
// enumerator names, shortest round-trip doubles -- is documented once at the
// declaration in drivers/ipm_options.h and is not restated here.
//
// The four kv_* helpers and the enum_name() overloads are FILE-LOCAL and are
// deliberately duplicated in src/drivers/sqp_options.cpp rather than promoted
// to a shared header: the dump is the only consumer either engine has for an
// enumerator-name table, and a public enum-to-string surface is a bigger
// commitment than this task takes. If a third consumer appears, promote them
// then.
// =============================================================================

namespace {

// CANONICAL BYTES, NOT THE CALLER'S FORMATTING (M7 W0 T5 fix round 1, sol
// Important 1). The declaration promises that the same value dumps the same
// bytes in any process. Composing a line with `os << name << " = " << v` breaks
// that promise the moment the caller hands over a stream it has configured:
// `std::hex` turns `max_iters = 1000` into `max_iters = 3e8`, `showpos` and
// `uppercase` and `scientific` each rewrite a field, a non-zero `width()` pads
// the first insertion with the caller's `fill()`, and a stream imbued with a
// locale whose numpunct groups digits inserts separators into every integer.
// None of that is hypothetical for a harness that records a run into a stream
// it also writes its own report through.
//
// So NOTHING HERE GOES THROUGH A FORMATTED INSERTION. Each line is rendered
// whole by fmt and handed to the stream by `write()`, an UNFORMATTED output
// function: it consults neither the flags, nor the precision, nor the width and
// fill, nor the imbued locale. fmt's own rendering is locale-independent by
// specification -- a locale is consulted only for the `L` specifier, which
// nothing below uses -- so the bytes are the classic locale's for every caller,
// and this TU is compiled with FMT_USE_LOCALE=0 besides.
//
// The width is the one that leaves a visible trace: an unformatted write does
// not CONSUME `width()`, where a formatted insertion would reset it to 0. The
// regression test reads that back, so "we did not use `operator<<`" is checked
// rather than promised.
void emit(std::ostream &os, const std::string &line) {
    os.write(line.data(), static_cast<std::streamsize>(line.size()));
}

void kv_int(std::ostream &os, const char *name, long long v) {
    emit(os, fmt::format("{} = {}\n", name, v));
}
void kv_str(std::ostream &os, const char *name, const char *v) {
    emit(os, fmt::format("{} = {}\n", name, v));
}
void kv_bool(std::ostream &os, const char *name, bool v) {
    emit(os, fmt::format("{} = {}\n", name, v ? "true" : "false"));
}
// SHORTEST ROUND-TRIP: fmt's default rendering of a double is the shortest
// string that reads back as the same double (`1e-06`, not
// `9.9999999999999995e-07`).
void kv_dbl(std::ostream &os, const char *name, double v) {
    emit(os, fmt::format("{} = {}\n", name, v));
}
void kv_opt_int(std::ostream &os, const char *name, const std::optional<hven::Index> &v) {
    if (v)
        kv_int(os, name, static_cast<long long>(*v));
    else
        kv_str(os, name, "unset");
}
void kv_opt_dbl(std::ostream &os, const char *name, const std::optional<double> &v) {
    if (v)
        kv_dbl(os, name, *v);
    else
        kv_str(os, name, "unset");
}

// ENUMERATOR NAMES, not display strings. to_string(StartLevel)
// (src/core/enum_names.cpp) spells "Warm"; a recorded run has to say `kWarm`,
// the token a reader can paste back into source. Each switch is exhaustive with
// no default, so a new enumerator warns here; the trailing return covers a
// value that is in range for no enumerator at all.
const char *enum_name(hven::solvers::AlgorithmMode v) {
    using hven::solvers::AlgorithmMode;
    switch (v) {
    case AlgorithmMode::kOpt:
        return "kOpt";
    case AlgorithmMode::kOptNo:
        return "kOptNo";
    case AlgorithmMode::kSoe:
        return "kSoe";
    case AlgorithmMode::kInit:
        return "kInit";
    }
    return "unknown";
}
const char *enum_name(hven::solvers::BarrierMode v) {
    using hven::solvers::BarrierMode;
    switch (v) {
    case BarrierMode::kProbe:
        return "kProbe";
    case BarrierMode::kLoqo:
        return "kLoqo";
    }
    return "unknown";
}
const char *enum_name(hven::solvers::LineSearchMode v) {
    using hven::solvers::LineSearchMode;
    switch (v) {
    case LineSearchMode::kAugLang:
        return "kAugLang";
    case LineSearchMode::kLang:
        return "kLang";
    case LineSearchMode::kL1:
        return "kL1";
    case LineSearchMode::kNoLs:
        return "kNoLs";
    }
    return "unknown";
}
const char *enum_name(hven::solvers::PrimalDualStepStrategy v) {
    using hven::solvers::PrimalDualStepStrategy;
    switch (v) {
    case PrimalDualStepStrategy::kPrimSlackEqSplitIq:
        return "kPrimSlackEqSplitIq";
    case PrimalDualStepStrategy::kAllMinimum:
        return "kAllMinimum";
    case PrimalDualStepStrategy::kPrimSlackSplitEqIq:
        return "kPrimSlackSplitEqIq";
    case PrimalDualStepStrategy::kMaxEq:
        return "kMaxEq";
    }
    return "unknown";
}
// acceptance_strategy_name() above spells these as their enumerators for
// validate()'s message; the dump reuses that table.
const char *enum_name(hven::solvers::AcceptanceKind v) { return acceptance_strategy_name(v); }
const char *enum_name(hven::solvers::MeritPenaltyRule v) {
    using hven::solvers::MeritPenaltyRule;
    switch (v) {
    case MeritPenaltyRule::kWmno:
        return "kWmno";
    case MeritPenaltyRule::kFlexible:
        return "kFlexible";
    }
    return "unknown";
}
const char *enum_name(hven::solvers::BarrierGovernorKind v) {
    using hven::solvers::BarrierGovernorKind;
    switch (v) {
    case BarrierGovernorKind::kClassicAdaptive:
        return "kClassicAdaptive";
    case BarrierGovernorKind::kMonitored:
        return "kMonitored";
    }
    return "unknown";
}
const char *enum_name(hven::solvers::RestorationMode v) {
    using hven::solvers::RestorationMode;
    switch (v) {
    case RestorationMode::kOff:
        return "kOff";
    case RestorationMode::kProximalSwitch:
        return "kProximalSwitch";
    case RestorationMode::kL1Nested:
        return "kL1Nested";
    }
    return "unknown";
}
const char *enum_name(hven::solvers::FixedVariableTreatment v) {
    using hven::solvers::FixedVariableTreatment;
    switch (v) {
    case FixedVariableTreatment::kMakeParameter:
        return "kMakeParameter";
    case FixedVariableTreatment::kMakeConstraint:
        return "kMakeConstraint";
    case FixedVariableTreatment::kRelaxBounds:
        return "kRelaxBounds";
    }
    return "unknown";
}
const char *enum_name(hven::solvers::InertiaMode v) {
    using hven::solvers::InertiaMode;
    switch (v) {
    case InertiaMode::kClassic:
        return "kClassic";
    case InertiaMode::kProximalRegularization:
        return "kProximalRegularization";
    }
    return "unknown";
}
const char *enum_name(hven::solvers::KktOrdering v) {
    using hven::solvers::KktOrdering;
    switch (v) {
    case KktOrdering::kMinimumDegree:
        return "kMinimumDegree";
    case KktOrdering::kNestedDissection:
        return "kNestedDissection";
    case KktOrdering::kParallelNestedDissection:
        return "kParallelNestedDissection";
    }
    return "unknown";
}
const char *enum_name(hven::solvers::KktPivoting v) {
    using hven::solvers::KktPivoting;
    switch (v) {
    case KktPivoting::kOneByOne:
        return "kOneByOne";
    case KktPivoting::kTwoByTwo:
        return "kTwoByTwo";
    }
    return "unknown";
}
const char *enum_name(hven::solvers::KktSolveParallelism v) {
    using hven::solvers::KktSolveParallelism;
    switch (v) {
    case KktSolveParallelism::kAdaptive:
        return "kAdaptive";
    case KktSolveParallelism::kSequential:
        return "kSequential";
    }
    return "unknown";
}
const char *enum_name(hven::solvers::BestIterateCriterion v) {
    using hven::solvers::BestIterateCriterion;
    switch (v) {
    case BestIterateCriterion::kEquality:
        return "kEquality";
    case BestIterateCriterion::kInequality:
        return "kInequality";
    case BestIterateCriterion::kKkt:
        return "kKkt";
    case BestIterateCriterion::kObjective:
        return "kObjective";
    }
    return "unknown";
}
const char *enum_name(hven::solvers::StartLevel v) {
    using hven::solvers::StartLevel;
    switch (v) {
    case StartLevel::kCold:
        return "kCold";
    case StartLevel::kSeeded:
        return "kSeeded";
    case StartLevel::kWarm:
        return "kWarm";
    case StartLevel::kHot:
        return "kHot";
    }
    return "unknown";
}
const char *enum_name(hven::solvers::IpmPhase v) {
    using hven::solvers::IpmPhase;
    switch (v) {
    case IpmPhase::kOptimize:
        return "kOptimize";
    case IpmPhase::kSolve:
        return "kSolve";
    }
    return "unknown";
}
const char *enum_name(hven::solvers::Verbosity v) {
    using hven::solvers::Verbosity;
    switch (v) {
    case Verbosity::kSilent:
        return "kSilent";
    case Verbosity::kWarnings:
        return "kWarnings";
    case Verbosity::kSummary:
        return "kSummary";
    case Verbosity::kIterations:
        return "kIterations";
    }
    return "unknown";
}
const char *enum_name(hven::linear::DensePolicy v) {
    using hven::linear::DensePolicy;
    switch (v) {
    case DensePolicy::kNever:
        return "kNever";
    case DensePolicy::kAlways:
        return "kAlways";
    case DensePolicy::kAuto:
        return "kAuto";
    }
    return "unknown";
}

} // namespace

void hven::solvers::dump_options(const IpmOptions &o, std::ostream &os) {
    const CommonOptions &c = o.common;
    kv_dbl(os, "common.optimality_tol", c.optimality_tol);
    kv_dbl(os, "common.feasibility_tol", c.feasibility_tol);
    kv_int(os, "common.max_iterations", static_cast<long long>(c.max_iterations));
    kv_opt_int(os, "common.max_factorizations", c.max_factorizations);
    kv_opt_dbl(os, "common.time_limit_seconds", c.time_limit_seconds);
    kv_str(os, "common.verbosity", enum_name(c.verbosity));
    kv_int(os, "common.threads", c.threads);
    kv_str(os, "common.start_level", enum_name(c.start_level));
    kv_dbl(os, "common.infinite_bound", c.infinite_bound);
    // The one sequence-valued field: bracketed, comma-separated, one line. An
    // EMPTY sequence -- which validate() refuses -- still dumps, as `[]`: a dump
    // is a faithful record of the value it was handed, not a second validator.
    std::string seq = "[";
    for (std::size_t i = 0; i < o.phases.size(); ++i) {
        if (i != 0) {
            seq += ", ";
        }
        seq += enum_name(o.phases[i]);
    }
    seq += ']';
    kv_str(os, "phases", seq.c_str());
    const IpmTerminationOptions &t = o.termination;
    kv_opt_dbl(os, "termination.complementarity_tol", t.complementarity_tol);
    kv_int(os, "termination.acceptable_iterations",
           static_cast<long long>(t.acceptable_iterations));
    kv_dbl(os, "termination.acceptable_kkt_tol", t.acceptable_kkt_tol);
    kv_dbl(os, "termination.acceptable_equality_tol", t.acceptable_equality_tol);
    kv_dbl(os, "termination.acceptable_inequality_tol", t.acceptable_inequality_tol);
    kv_dbl(os, "termination.acceptable_barrier_tol", t.acceptable_barrier_tol);
    kv_dbl(os, "termination.divergence_kkt_tol", t.divergence_kkt_tol);
    kv_dbl(os, "termination.divergence_equality_tol", t.divergence_equality_tol);
    kv_dbl(os, "termination.divergence_inequality_tol", t.divergence_inequality_tol);
    kv_dbl(os, "termination.divergence_barrier_tol", t.divergence_barrier_tol);
    kv_dbl(os, "initial_barrier", o.initial_barrier);
    kv_dbl(os, "max_barrier", o.max_barrier);
    kv_dbl(os, "min_barrier", o.min_barrier);
    kv_dbl(os, "fraction_to_boundary", o.fraction_to_boundary);
    kv_dbl(os, "bound_push", o.bound_push);
    kv_dbl(os, "bound_interval_push", o.bound_interval_push);
    kv_str(os, "fixed_variable_treatment", enum_name(o.fixed_variable_treatment));
    kv_dbl(os, "bound_relax_factor", o.bound_relax_factor);
    kv_dbl(os, "objective_scale", o.objective_scale);
    kv_bool(os, "return_best", o.return_best);
    kv_str(os, "best_criterion", enum_name(o.best_criterion));
    kv_bool(os, "wide_console", o.wide_console);
    const IpmLinearOptions &l = o.linear;
    kv_str(os, "linear.ordering", enum_name(l.ordering));
    kv_str(os, "linear.pivoting", enum_name(l.pivoting));
    kv_bool(os, "linear.matching", l.matching);
    kv_bool(os, "linear.scaling", l.scaling);
    kv_int(os, "linear.pivot_perturbation", l.pivot_perturbation);
    kv_int(os, "linear.refinement_steps", l.refinement_steps);
    kv_str(os, "linear.parallel_solve", enum_name(l.parallel_solve));
    kv_bool(os, "linear.conditional_reproducibility", l.conditional_reproducibility);
    kv_bool(os, "linear.force_analysis", l.force_analysis);
#ifdef USE_ACCELERATE_SPARSE
    // THE APPLE-ONLY FIELD, declared under the same guard: on Linux and
    // Windows the dump is one line shorter. The pin in
    // tests/drivers/test_options_dump.cpp carries both counts.
    kv_dbl(os, "linear.accelerate_zero_tolerance", l.accelerate_zero_tolerance);
#endif
    kv_str(os, "linear.dense_factorization", enum_name(l.dense_factorization));
    kv_opt_int(os, "linear.dense_max_dimension", l.dense_max_dimension);
    const IpmAdvancedOptions &a = o.advanced;
    kv_str(os, "advanced.acceptance_strategy", enum_name(a.acceptance_strategy));
    kv_str(os, "advanced.merit_penalty_rule", enum_name(a.merit_penalty_rule));
    kv_str(os, "advanced.barrier_governor", enum_name(a.barrier_governor));
    kv_bool(os, "advanced.never_monotone", a.never_monotone);
    kv_str(os, "advanced.restoration_mode", enum_name(a.restoration_mode));
    kv_str(os, "advanced.inertia_mode", enum_name(a.inertia_mode));
    kv_int(os, "advanced.max_second_order_corrections",
           static_cast<long long>(a.max_second_order_corrections));
    kv_int(os, "advanced.line_search_extended_backtracks",
           static_cast<long long>(a.line_search_extended_backtracks));
    kv_bool(os, "advanced.watchdog", a.watchdog);
    kv_int(os, "advanced.restoration_max_entries",
           static_cast<long long>(a.restoration_max_entries));
    kv_int(os, "advanced.line_search_max_backtracks",
           static_cast<long long>(a.line_search_max_backtracks));
    kv_dbl(os, "advanced.backtrack_divisor", a.backtrack_divisor);
    kv_str(os, "advanced.solve_phase_mode", enum_name(a.solve_phase_mode));
    kv_str(os, "advanced.optimize_barrier_mode", enum_name(a.optimize_barrier_mode));
    kv_str(os, "advanced.solve_barrier_mode", enum_name(a.solve_barrier_mode));
    kv_str(os, "advanced.optimize_line_search_mode", enum_name(a.optimize_line_search_mode));
    kv_str(os, "advanced.solve_line_search_mode", enum_name(a.solve_line_search_mode));
    kv_str(os, "advanced.primal_dual_step_strategy", enum_name(a.primal_dual_step_strategy));
    kv_dbl(os, "advanced.inertia_correction_initial", a.inertia_correction_initial);
    kv_dbl(os, "advanced.inertia_correction_increase", a.inertia_correction_increase);
    kv_dbl(os, "advanced.inertia_correction_decrease", a.inertia_correction_decrease);
    kv_int(os, "advanced.inertia_correction_max_attempts",
           static_cast<long long>(a.inertia_correction_max_attempts));
    kv_bool(os, "advanced.skip_unperturbed_attempts", a.skip_unperturbed_attempts);
    kv_dbl(os, "advanced.negative_slack_reset", a.negative_slack_reset);
}
