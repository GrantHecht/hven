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
// This file was (retired path) src/drivers/interior_point_solver_settings.cpp until M6 W5
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

#include <cmath>
#include <cstddef>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

#include <hven/detail/ipm/ipm_options_fields.h>
#include <hven/detail/ipm/ipm_solver_presets.h>
#include <hven/detail/model/fixed_variable_treatment.h>
#include <hven/detail/solver/common_options_validate.h>
#include <hven/detail/solver/option_info_builder.h>
#include <hven/ipm/ipm_options.h>

using namespace hven::detail;

namespace {
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
    pos_finite(t.acceptable_stationarity_tol, "termination.acceptable_stationarity_tol");
    pos_finite(t.acceptable_equality_tol, "termination.acceptable_equality_tol");
    pos_finite(t.acceptable_inequality_tol, "termination.acceptable_inequality_tol");
    pos_finite(t.acceptable_complementarity_tol, "termination.acceptable_complementarity_tol");
    pos_finite(t.divergence_stationarity_tol, "termination.divergence_stationarity_tol");
    pos_finite(t.divergence_equality_tol, "termination.divergence_equality_tol");
    pos_finite(t.divergence_inequality_tol, "termination.divergence_inequality_tol");
    pos_finite(t.divergence_complementarity_tol, "termination.divergence_complementarity_tol");

    // --- Cross-field: convergence tol <= acceptable tol <= divergence tol ---
    const auto ordered = [](double lo, const char *lo_name, double hi, const char *hi_name) {
        if (lo > hi)
            throw std::invalid_argument(
                fmt::format("{} ({}) must be <= {} ({})", lo_name, lo, hi_name, hi));
    };
    ordered(o.common.optimality_tol, "common.optimality_tol", t.acceptable_stationarity_tol,
            "termination.acceptable_stationarity_tol");
    ordered(o.common.feasibility_tol, "common.feasibility_tol", t.acceptable_equality_tol,
            "termination.acceptable_equality_tol");
    ordered(o.common.feasibility_tol, "common.feasibility_tol", t.acceptable_inequality_tol,
            "termination.acceptable_inequality_tol");
    ordered(comp_tol,
            t.complementarity_tol ? "termination.complementarity_tol"
                                  : "common.optimality_tol (the complementarity tolerance)",
            t.acceptable_complementarity_tol, "termination.acceptable_complementarity_tol");
    ordered(t.acceptable_stationarity_tol, "termination.acceptable_stationarity_tol",
            t.divergence_stationarity_tol, "termination.divergence_stationarity_tol");
    ordered(t.acceptable_equality_tol, "termination.acceptable_equality_tol",
            t.divergence_equality_tol, "termination.divergence_equality_tol");
    ordered(t.acceptable_inequality_tol, "termination.acceptable_inequality_tol",
            t.divergence_inequality_tol, "termination.divergence_inequality_tol");
    ordered(t.acceptable_complementarity_tol, "termination.acceptable_complementarity_tol",
            t.divergence_complementarity_tol, "termination.divergence_complementarity_tol");

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

    // --- Linear ---
    enum_in_range(lin.ordering, IpmOrdering::kParallelNestedDissection, "linear.ordering");
    enum_in_range(lin.pivot_strategy, IpmPivotStrategy::kTwoByTwo, "linear.pivot_strategy");
    non_negative(lin.pivot_perturbation_exponent, "linear.pivot_perturbation_exponent");
    non_negative(lin.max_refinement_steps, "linear.max_refinement_steps");
    enum_in_range(lin.solve_parallelism, IpmSolveParallelism::kSequential,
                  "linear.solve_parallelism");
    pos_finite(lin.accelerate_zero_tol, "linear.accelerate_zero_tol");
    // THE DENSE ARM: kNever, kAlways and kAuto (M8 DN-3: the dense arm iff the KKT
    // pattern's order is at most the library's threshold, detail::kIpmDenseSelectionThreshold).
    // The threshold is not a caller's option yet. CNR is a per-session Pardiso guarantee the
    // dense arm cannot keep.
    enum_in_range(lin.dense_factorization, hven::linear::DensePolicy::kAuto,
                  "linear.dense_factorization");
    if (lin.max_dense_dimension)
        throw std::invalid_argument(
            "linear.max_dense_dimension is reserved: DensePolicy::kAuto uses the library's "
            "threshold; leave it empty");
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
            EnumTraits<IpmAcceptanceMode>::name(adv.acceptance_mode)));
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

hven::IpmOptions hven::worker_options(IpmOptions base) {
    // One backend thread and silent output. The partition count is the
    // PROGRAM's, documented on the declaration -- no options value reaches a
    // layout.
    base.common.threads = 1;
    base.common.verbosity = Verbosity::kSilent;
    return base;
}

// =============================================================================
// The options dump, and the options metadata.
//
// Both are walks over the same per-struct tables (detail/ipm/ipm_options_fields.h), one row per
// field in declaration order, so a field cannot be dumped and missing from the metadata or the
// reverse. The contract -- order,
// prefixes, enumerator names, shortest round-trip doubles -- is documented once at the declaration
// in ipm/ipm_options.h and is not restated here. The tables are checked against the structs at
// compile time, below: a field added without a row, or a row out of declaration order, does not
// compile. (M7 W0 T5(b) wrote the dump by hand, one line per field, pinned by a drift test; M8 AP-4
// G1 replaced it with the tables.)
// =============================================================================

// -Wreorder-init-list is the order check below: a table whose rows are out of declaration order
// is an error here, not a warning.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic error "-Wreorder-init-list"
#endif
namespace hven {
HVEN_CHECK_OPTIONS_TABLE(CommonOptions, HVEN_COMMON_OPTIONS_FIELDS)
HVEN_CHECK_OPTIONS_TABLE(IpmLinearOptions, HVEN_IPM_LINEAR_OPTIONS_FIELDS)
HVEN_CHECK_OPTIONS_TABLE(IpmTerminationOptions, HVEN_IPM_TERMINATION_OPTIONS_FIELDS)
HVEN_CHECK_OPTIONS_TABLE(IpmAdvancedOptions, HVEN_IPM_ADVANCED_OPTIONS_FIELDS)
HVEN_CHECK_OPTIONS_TABLE_G(IpmOptions, HVEN_IPM_OPTIONS_FIELDS)
} // namespace hven
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace {

// CANONICAL BYTES, NOT THE CALLER'S FORMATTING (M7 W0 T5 fix round 1, sol Important 1). The
// declaration promises that the same value dumps the same bytes in any process. Composing a line
// with `os << name << " = " << v` breaks that promise the moment the caller hands over a stream
// it has configured: `std::hex` turns `max_iters = 1000` into `max_iters = 3e8`, `showpos` and
// `uppercase` and `scientific` each rewrite a field, a non-zero `width()` pads the first
// insertion with the caller's `fill()`, and a stream imbued with a locale whose numpunct groups
// digits inserts separators into every integer.
//
// So NOTHING HERE GOES THROUGH A FORMATTED INSERTION. Each line is rendered whole and handed to
// the stream by `write()`, an UNFORMATTED output function: it consults neither the flags, nor the
// precision, nor the width and fill, nor the imbued locale. The numbers are rendered by fmt
// (detail::format_double, format_int), whose output is locale-independent by specification and
// whose TU is compiled with FMT_USE_LOCALE=0 besides.
//
// The width is the one that leaves a visible trace: an unformatted write does not CONSUME
// `width()`, where a formatted insertion would reset it to 0. The regression test reads that
// back, so "we did not use `operator<<`" is checked rather than promised.
void emit(std::ostream &os, const std::string &line) {
    os.write(line.data(), static_cast<std::streamsize>(line.size()));
}

} // namespace

void hven::dump_options(const IpmOptions &o, std::ostream &os) {
    detail::for_each_option(o, [&os](const detail::OptionMeta &m, const auto &value) {
        emit(os, m.name() + " = " + detail::render_option_value(value) + "\n");
    });
}

const std::vector<hven::OptionInfo> &hven::ipm_option_info() {
    static const std::vector<OptionInfo> rows = [] {
        const IpmOptions defaults;
        std::vector<OptionInfo> out;
        detail::for_each_option(defaults, [&out](const detail::OptionMeta &m, const auto &value) {
            out.push_back(detail::describe_option(m, value));
        });
        return out;
    }();
    return rows;
}
