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

using namespace hven::detail;

namespace {
// Name of a non-classic acceptance strategy, for the validate() error message
// below. classic_merit never reaches this helper (the guard that calls it is
// gated on acceptance_mode_ != classic_merit).
const char *acceptance_strategy_name(hven::IpmAcceptanceMode strategy) {
    using hven::IpmAcceptanceMode;
    switch (strategy) {
    case IpmAcceptanceMode::kMerit:
        return "kMerit";
    case IpmAcceptanceMode::kFunnel:
        return "kFunnel";
    case IpmAcceptanceMode::kFilter:
        return "kFilter";
    case IpmAcceptanceMode::kClassicMerit:
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

void check_fixed_variable_treatment(hven::IpmFixedVariableTreatment treatment) {
    using hven::IpmFixedVariableTreatment;
    switch (treatment) {
    case IpmFixedVariableTreatment::kMakeParameter:
    case IpmFixedVariableTreatment::kMakeConstraint:
    case IpmFixedVariableTreatment::kRelaxBounds:
        return;
    }
    throw std::invalid_argument(fmt::format(
        "fixed_variable_treatment must be one of {}, {}, {} (got {})",
        hven::detail::fixed_variable_treatment_name(IpmFixedVariableTreatment::kMakeParameter),
        hven::detail::fixed_variable_treatment_name(IpmFixedVariableTreatment::kMakeConstraint),
        hven::detail::fixed_variable_treatment_name(IpmFixedVariableTreatment::kRelaxBounds),
        static_cast<int>(treatment)));
}

} // namespace

// Option validation

void hven::validate(const IpmOptions &o) {
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
    pos_finite(o.barrier_initial, "barrier_initial");
    pos_finite(o.barrier_min, "barrier_min");
    pos_finite(o.barrier_max, "barrier_max");
    if (o.barrier_min > o.barrier_max)
        throw std::invalid_argument(fmt::format("barrier_min ({}) must be <= barrier_max ({})",
                                                o.barrier_min, o.barrier_max));
    if (o.barrier_initial < o.barrier_min || o.barrier_initial > o.barrier_max)
        throw std::invalid_argument(
            fmt::format("barrier_initial ({}) must be within [barrier_min ({}), barrier_max ({})]",
                        o.barrier_initial, o.barrier_min, o.barrier_max));

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
    if (o.fixed_variable_treatment == IpmFixedVariableTreatment::kRelaxBounds &&
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

    enum_in_range(o.best_iterate_criterion, IpmBestIterateCriterion::kObjective,
                  "best_iterate_criterion");

    // --- Linear ---
    enum_in_range(lin.ordering, IpmOrdering::kParallelNestedDissection, "linear.ordering");
    enum_in_range(lin.pivot_strategy, IpmPivotStrategy::kTwoByTwo, "linear.pivot_strategy");
    non_negative(lin.pivot_perturbation_exponent, "linear.pivot_perturbation_exponent");
    non_negative(lin.max_refinement_steps, "linear.max_refinement_steps");
    enum_in_range(lin.solve_parallelism, IpmSolveParallelism::kSequential,
                  "linear.solve_parallelism");
    pos_finite(lin.accelerate_zero_tol, "linear.accelerate_zero_tol");
    // THE DENSE ARM: kNever and kAlways. kAuto and its threshold stay refused
    // until the selection rule lands (DN-3). CNR is a per-session Pardiso
    // guarantee the dense arm cannot keep.
    if (lin.dense_factorization != hven::linear::DensePolicy::kNever &&
        lin.dense_factorization != hven::linear::DensePolicy::kAlways)
        throw std::invalid_argument(
            fmt::format("linear.dense_factorization = {} is reserved: automatic dense selection "
                        "is not available yet; DensePolicy::kNever and kAlways are accepted",
                        static_cast<int>(lin.dense_factorization)));
    if (lin.max_dense_dimension)
        throw std::invalid_argument(
            "linear.max_dense_dimension is reserved: automatic dense selection is not available "
            "yet; leave it empty");
    if (lin.dense_factorization != hven::linear::DensePolicy::kNever &&
        lin.conditional_reproducibility)
        throw std::invalid_argument(
            "linear.conditional_reproducibility requires linear.dense_factorization = kNever: "
            "the dense arm's LAPACK has no per-session reproducibility mode");

    // --- Advanced: iteration and attempt budgets ---
    // 0 disables each of these outright, which is well-defined; negative is not.
    // max_inertia_correction_attempts = 0 disables the inertia ladder: an
    // unperturbed factorization with wrong inertia exhausts immediately.
    non_negative(adv.max_inertia_correction_attempts, "advanced.max_inertia_correction_attempts");
    non_negative(adv.max_line_search_backtracks, "advanced.max_line_search_backtracks");
    non_negative(adv.max_second_order_corrections, "advanced.max_second_order_corrections");
    non_negative(adv.max_extended_backtracks, "advanced.max_extended_backtracks");
    non_negative(adv.max_restoration_entries, "advanced.max_restoration_entries");

    enum_in_range(adv.acceptance_mode, IpmAcceptanceMode::kFilter, "advanced.acceptance_mode");
    enum_in_range(adv.merit_penalty_rule, IpmMeritPenaltyRule::kFlexible,
                  "advanced.merit_penalty_rule");
    enum_in_range(adv.barrier_governor_mode, IpmBarrierGovernorMode::kMonitored,
                  "advanced.barrier_governor_mode");
    enum_in_range(adv.restoration_mode, IpmRestorationMode::kL1Nested, "advanced.restoration_mode");
    enum_in_range(adv.inertia_mode, IpmInertiaMode::kProximalRegularization,
                  "advanced.inertia_mode");
    enum_in_range(adv.solve_phase_mode, IpmSolvePhaseMode::kSolveEquations,
                  "advanced.solve_phase_mode");
    enum_in_range(adv.optimize_barrier_mode, IpmBarrierMode::kLoqo,
                  "advanced.optimize_barrier_mode");
    enum_in_range(adv.solve_barrier_mode, IpmBarrierMode::kLoqo, "advanced.solve_barrier_mode");
    enum_in_range(adv.optimize_line_search_mode, IpmLineSearchMode::kNone,
                  "advanced.optimize_line_search_mode");
    enum_in_range(adv.solve_line_search_mode, IpmLineSearchMode::kNone,
                  "advanced.solve_line_search_mode");
    enum_in_range(adv.step_length_rule, IpmStepLengthRule::kEqualityMultipliersMaximum,
                  "advanced.step_length_rule");

    // --- Advanced: strategy-combination guards ---
    // The SOC and extended-backtracking recovery links re-drive the acceptance
    // backtrack through the same acceptance test the ordinary step faced, so
    // both compose with every acceptance kind.
    //
    // Funnel and filter are designed to operate above a monotone barrier
    // safeguard. kClassicAdaptive is free-mode only, so pairing it with them
    // drops that safeguard unless the caller opts in via never_monotone.
    if ((adv.acceptance_mode == IpmAcceptanceMode::kFunnel ||
         adv.acceptance_mode == IpmAcceptanceMode::kFilter) &&
        adv.barrier_governor_mode == IpmBarrierGovernorMode::kClassicAdaptive &&
        !adv.never_monotone)
        throw std::invalid_argument(fmt::format(
            "advanced.acceptance_mode = {} is designed to operate above the monotone barrier "
            "safeguard, which advanced.barrier_governor_mode = kClassicAdaptive does not provide: "
            "set "
            "advanced.barrier_governor_mode = kMonitored (the default), or set "
            "advanced.never_monotone "
            "= true to explicitly accept adaptive-only operation",
            acceptance_strategy_name(adv.acceptance_mode)));
    // never_monotone accepts running WITHOUT a monotone safeguard; kMonitored
    // provides one, so the combination is a direct contradiction.
    if (adv.never_monotone && adv.barrier_governor_mode == IpmBarrierGovernorMode::kMonitored)
        throw std::invalid_argument(
            "advanced.never_monotone = true is contradictory with advanced.barrier_governor_mode = "
            "kMonitored: the monitored governor already provides the monotone safeguard "
            "never_monotone opts out of; set advanced.barrier_governor_mode = kClassicAdaptive or "
            "advanced.never_monotone = false");

    // --- Advanced: step and inertia-correction parameters ---
    pos_finite(adv.negative_slack_reset, "advanced.negative_slack_reset");
    greater_than(adv.backtrack_divisor, 1.0, "advanced.backtrack_divisor");
    greater_than(adv.inertia_correction_initial, 0.0, "advanced.inertia_correction_initial");
    greater_than(adv.inertia_correction_increase, 1.0, "advanced.inertia_correction_increase");
    in_open_unit(adv.inertia_correction_decrease, "advanced.inertia_correction_decrease");
}

static_assert(hven::detail::kDefaultBoundRelaxFactor == 1.0e-8,
              "kDefaultBoundRelaxFactor must equal IpmOptions' bound_relax_factor default");

// Named configuration presets

hven::IpmOptions hven::ipm_preset(std::string_view name) {
    IpmOptions o;
    for (const auto &entry : kInteriorPointSolverPresets) {
        if (entry.name_ != name)
            continue;
        const InteriorPointSolverPresetFields &f = entry.fields_;
        IpmAdvancedOptions &adv = o.advanced;
        adv.acceptance_mode = f.acceptance_mode_;
        adv.merit_penalty_rule = f.merit_penalty_rule_;
        adv.barrier_governor_mode = f.barrier_governor_;
        adv.never_monotone = f.never_monotone_;
        adv.restoration_mode = f.restoration_mode_;
        adv.inertia_mode = f.inertia_mode_;
        adv.max_second_order_corrections = f.max_soc_;
        adv.max_extended_backtracks = f.ls_extended_iters_;
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

hven::IpmOptions hven::ipm_worker_options(IpmOptions base) {
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
const char *enum_name(hven::IpmSolvePhaseMode v) {
    using hven::IpmSolvePhaseMode;
    switch (v) {
    case IpmSolvePhaseMode::kOptimize:
        return "kOptimize";
    case IpmSolvePhaseMode::kOptimizeWithoutObjective:
        return "kOptimizeWithoutObjective";
    case IpmSolvePhaseMode::kSolveEquations:
        return "kSolveEquations";
    }
    return "unknown";
}
const char *enum_name(hven::IpmBarrierMode v) {
    using hven::IpmBarrierMode;
    switch (v) {
    case IpmBarrierMode::kProbe:
        return "kProbe";
    case IpmBarrierMode::kLoqo:
        return "kLoqo";
    }
    return "unknown";
}
const char *enum_name(hven::IpmLineSearchMode v) {
    using hven::IpmLineSearchMode;
    switch (v) {
    case IpmLineSearchMode::kAugmentedLagrangian:
        return "kAugmentedLagrangian";
    case IpmLineSearchMode::kLagrangian:
        return "kLagrangian";
    case IpmLineSearchMode::kL1:
        return "kL1";
    case IpmLineSearchMode::kNone:
        return "kNone";
    }
    return "unknown";
}
const char *enum_name(hven::IpmStepLengthRule v) {
    using hven::IpmStepLengthRule;
    switch (v) {
    case IpmStepLengthRule::kEqualityMultipliersPrimal:
        return "kEqualityMultipliersPrimal";
    case IpmStepLengthRule::kAllMinimum:
        return "kAllMinimum";
    case IpmStepLengthRule::kEqualityMultipliersDual:
        return "kEqualityMultipliersDual";
    case IpmStepLengthRule::kEqualityMultipliersMaximum:
        return "kEqualityMultipliersMaximum";
    }
    return "unknown";
}
// acceptance_strategy_name() above spells these as their enumerators for
// validate()'s message; the dump reuses that table.
const char *enum_name(hven::IpmAcceptanceMode v) { return acceptance_strategy_name(v); }
const char *enum_name(hven::IpmMeritPenaltyRule v) {
    using hven::IpmMeritPenaltyRule;
    switch (v) {
    case IpmMeritPenaltyRule::kSinglePenalty:
        return "kSinglePenalty";
    case IpmMeritPenaltyRule::kFlexible:
        return "kFlexible";
    }
    return "unknown";
}
const char *enum_name(hven::IpmBarrierGovernorMode v) {
    using hven::IpmBarrierGovernorMode;
    switch (v) {
    case IpmBarrierGovernorMode::kClassicAdaptive:
        return "kClassicAdaptive";
    case IpmBarrierGovernorMode::kMonitored:
        return "kMonitored";
    }
    return "unknown";
}
const char *enum_name(hven::IpmRestorationMode v) {
    using hven::IpmRestorationMode;
    switch (v) {
    case IpmRestorationMode::kOff:
        return "kOff";
    case IpmRestorationMode::kProximalSwitch:
        return "kProximalSwitch";
    case IpmRestorationMode::kL1Nested:
        return "kL1Nested";
    }
    return "unknown";
}
const char *enum_name(hven::IpmFixedVariableTreatment v) {
    using hven::IpmFixedVariableTreatment;
    switch (v) {
    case IpmFixedVariableTreatment::kMakeParameter:
        return "kMakeParameter";
    case IpmFixedVariableTreatment::kMakeConstraint:
        return "kMakeConstraint";
    case IpmFixedVariableTreatment::kRelaxBounds:
        return "kRelaxBounds";
    }
    return "unknown";
}
const char *enum_name(hven::IpmInertiaMode v) {
    using hven::IpmInertiaMode;
    switch (v) {
    case IpmInertiaMode::kClassic:
        return "kClassic";
    case IpmInertiaMode::kProximalRegularization:
        return "kProximalRegularization";
    }
    return "unknown";
}
const char *enum_name(hven::IpmOrdering v) {
    using hven::IpmOrdering;
    switch (v) {
    case IpmOrdering::kMinimumDegree:
        return "kMinimumDegree";
    case IpmOrdering::kNestedDissection:
        return "kNestedDissection";
    case IpmOrdering::kParallelNestedDissection:
        return "kParallelNestedDissection";
    }
    return "unknown";
}
const char *enum_name(hven::IpmPivotStrategy v) {
    using hven::IpmPivotStrategy;
    switch (v) {
    case IpmPivotStrategy::kOneByOne:
        return "kOneByOne";
    case IpmPivotStrategy::kTwoByTwo:
        return "kTwoByTwo";
    }
    return "unknown";
}
const char *enum_name(hven::IpmSolveParallelism v) {
    using hven::IpmSolveParallelism;
    switch (v) {
    case IpmSolveParallelism::kAdaptivePartitioning:
        return "kAdaptivePartitioning";
    case IpmSolveParallelism::kSequential:
        return "kSequential";
    }
    return "unknown";
}
const char *enum_name(hven::IpmBestIterateCriterion v) {
    using hven::IpmBestIterateCriterion;
    switch (v) {
    case IpmBestIterateCriterion::kEquality:
        return "kEquality";
    case IpmBestIterateCriterion::kInequality:
        return "kInequality";
    case IpmBestIterateCriterion::kKkt:
        return "kKkt";
    case IpmBestIterateCriterion::kObjective:
        return "kObjective";
    }
    return "unknown";
}
const char *enum_name(hven::StartLevel v) {
    using hven::StartLevel;
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
const char *enum_name(hven::IpmPhase v) {
    using hven::IpmPhase;
    switch (v) {
    case IpmPhase::kOptimize:
        return "kOptimize";
    case IpmPhase::kSolve:
        return "kSolve";
    }
    return "unknown";
}
const char *enum_name(hven::Verbosity v) {
    using hven::Verbosity;
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

void hven::dump_options(const IpmOptions &o, std::ostream &os) {
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
    kv_dbl(os, "barrier_initial", o.barrier_initial);
    kv_dbl(os, "barrier_max", o.barrier_max);
    kv_dbl(os, "barrier_min", o.barrier_min);
    kv_dbl(os, "fraction_to_boundary", o.fraction_to_boundary);
    kv_dbl(os, "bound_push", o.bound_push);
    kv_dbl(os, "bound_interval_push", o.bound_interval_push);
    kv_str(os, "fixed_variable_treatment", enum_name(o.fixed_variable_treatment));
    kv_dbl(os, "bound_relax_factor", o.bound_relax_factor);
    kv_dbl(os, "objective_scale", o.objective_scale);
    kv_bool(os, "return_best", o.return_best);
    kv_str(os, "best_iterate_criterion", enum_name(o.best_iterate_criterion));
    kv_bool(os, "wide_console", o.wide_console);
    const IpmLinearOptions &l = o.linear;
    kv_str(os, "linear.ordering", enum_name(l.ordering));
    kv_str(os, "linear.pivot_strategy", enum_name(l.pivot_strategy));
    kv_bool(os, "linear.weighted_matching", l.weighted_matching);
    kv_bool(os, "linear.matrix_scaling", l.matrix_scaling);
    kv_int(os, "linear.pivot_perturbation_exponent", l.pivot_perturbation_exponent);
    kv_int(os, "linear.max_refinement_steps", l.max_refinement_steps);
    kv_str(os, "linear.solve_parallelism", enum_name(l.solve_parallelism));
    kv_bool(os, "linear.conditional_reproducibility", l.conditional_reproducibility);
    kv_bool(os, "linear.force_analysis", l.force_analysis);
    kv_str(os, "linear.dense_factorization", enum_name(l.dense_factorization));
    kv_dbl(os, "linear.accelerate_zero_tol", l.accelerate_zero_tol);
    kv_opt_int(os, "linear.max_dense_dimension", l.max_dense_dimension);
    const IpmAdvancedOptions &a = o.advanced;
    kv_str(os, "advanced.acceptance_mode", enum_name(a.acceptance_mode));
    kv_str(os, "advanced.merit_penalty_rule", enum_name(a.merit_penalty_rule));
    kv_str(os, "advanced.barrier_governor_mode", enum_name(a.barrier_governor_mode));
    kv_bool(os, "advanced.never_monotone", a.never_monotone);
    kv_str(os, "advanced.restoration_mode", enum_name(a.restoration_mode));
    kv_str(os, "advanced.inertia_mode", enum_name(a.inertia_mode));
    kv_int(os, "advanced.max_second_order_corrections",
           static_cast<long long>(a.max_second_order_corrections));
    kv_int(os, "advanced.max_extended_backtracks",
           static_cast<long long>(a.max_extended_backtracks));
    kv_bool(os, "advanced.watchdog", a.watchdog);
    kv_int(os, "advanced.max_restoration_entries",
           static_cast<long long>(a.max_restoration_entries));
    kv_int(os, "advanced.max_line_search_backtracks",
           static_cast<long long>(a.max_line_search_backtracks));
    kv_dbl(os, "advanced.backtrack_divisor", a.backtrack_divisor);
    kv_str(os, "advanced.solve_phase_mode", enum_name(a.solve_phase_mode));
    kv_str(os, "advanced.optimize_barrier_mode", enum_name(a.optimize_barrier_mode));
    kv_str(os, "advanced.solve_barrier_mode", enum_name(a.solve_barrier_mode));
    kv_str(os, "advanced.optimize_line_search_mode", enum_name(a.optimize_line_search_mode));
    kv_str(os, "advanced.solve_line_search_mode", enum_name(a.solve_line_search_mode));
    kv_str(os, "advanced.step_length_rule", enum_name(a.step_length_rule));
    kv_dbl(os, "advanced.inertia_correction_initial", a.inertia_correction_initial);
    kv_dbl(os, "advanced.inertia_correction_increase", a.inertia_correction_increase);
    kv_dbl(os, "advanced.inertia_correction_decrease", a.inertia_correction_decrease);
    kv_int(os, "advanced.max_inertia_correction_attempts",
           static_cast<long long>(a.max_inertia_correction_attempts));
    kv_bool(os, "advanced.skip_unperturbed_attempts", a.skip_unperturbed_attempts);
    kv_dbl(os, "advanced.negative_slack_reset", a.negative_slack_reset);
}
