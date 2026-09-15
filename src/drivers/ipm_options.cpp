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

#include "hven/detail/drivers/ipm_solver_presets.h"
#include "hven/drivers/ipm_solver_types.h"

#include <fmt/format.h>

#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
// Name of a non-classic acceptance strategy, for the validate() error message
// below. classic_merit never reaches this helper (the guard that calls it is
// gated on acceptance_strategy_ != classic_merit).
const char *acceptance_strategy_name(hven::solvers::AcceptanceStrategies strategy) {
    using hven::solvers::AcceptanceStrategies;
    switch (strategy) {
    case AcceptanceStrategies::merit:
        return "merit";
    case AcceptanceStrategies::funnel:
        return "funnel";
    case AcceptanceStrategies::filter:
        return "filter";
    case AcceptanceStrategies::classic_merit:
        return "classic_merit";
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

void pos_int(int v, const char *name) {
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
void check_fixed_variable_treatment(hven::solvers::FixedVariableTreatments treatment) {
    using hven::solvers::FixedVariableTreatments;
    switch (treatment) {
    case FixedVariableTreatments::MakeParameter:
    case FixedVariableTreatments::MakeConstraint:
    case FixedVariableTreatments::RelaxBounds:
        return;
    }
    throw std::invalid_argument(fmt::format(
        "fixed_variable_treatment must be one of {}, {}, {} (got {})",
        hven::solvers::fixed_variable_treatment_name(FixedVariableTreatments::MakeParameter),
        hven::solvers::fixed_variable_treatment_name(FixedVariableTreatments::MakeConstraint),
        hven::solvers::fixed_variable_treatment_name(FixedVariableTreatments::RelaxBounds),
        static_cast<int>(treatment)));
}

} // namespace

// Option validation

void hven::solvers::validate(const IpmOptions &o) {
    // pos_finite/pos_int/in_open_unit/greater_than/in_open_interval/
    // in_closed_interval (plus check_fixed_variable_treatment, used below) are
    // the file-scope helpers defined above (shared with the individual
    // set_*() methods, so a field's invariant and message can never drift
    // between the two call sites).

    // --- The phase sequence (M6 W5 T8.4) ---
    // The ONE structural rule on it: a solve that runs no phase has nothing to
    // report and is far more likely a caller's mistake than an intention. Any
    // non-empty order of the two phases is legal -- the conditional rule is the
    // engine's, evaluated per phase against the one before it, not a
    // restriction on what may be asked for here.
    if (o.phases.empty()) {
        throw std::invalid_argument("phases must name at least one phase; an empty sequence would "
                                    "run nothing and report nothing");
    }

    // --- Iteration limits ---
    pos_int(o.max_iters, "max_iters");
    pos_int(o.max_acc_iters, "max_acc_iters");
    // Perturbation-ladder attempt budget (factor_impl's Zfac loop). 0 is valid
    // and intentional -- it disables the ladder outright: an unperturbed
    // factorization that fails to reach correct inertia exhausts immediately
    // rather than attempting any correction, which forces the forced-rejection
    // / recovery-chain / SINGULAR_KKT routing (see ipm_solver.cpp's kkt_exhausted
    // handling) on the very first wrong-inertia factorization. Unlike
    // o.max_iters/o.max_acc_iters (which must run at least once), o.max_refac's
    // loop is a `for (i = 0; i < o.max_refac; i++)` correction attempt over an
    // already-computed base factorization, so 0 attempts is well-defined;
    // negative is not.
    if (o.max_refac < 0)
        throw std::invalid_argument(
            fmt::format("max_refac must be non-negative, got {}", o.max_refac));
    if (o.max_ls_iters < 0)
        throw std::invalid_argument(
            fmt::format("max_ls_iters must be non-negative, got {}", o.max_ls_iters));
    if (o.max_soc < 0)
        throw std::invalid_argument(fmt::format("max_soc must be non-negative, got {}", o.max_soc));
    if (o.ls_extended_iters < 0)
        throw std::invalid_argument(
            fmt::format("ls_extended_iters must be non-negative, got {}", o.ls_extended_iters));
    // Per-phase feasibility-restoration entry budget. 0 is valid (disables
    // restoration entry even when o.restoration_mode != off); negative is not.
    if (o.max_feas_rest < 0)
        throw std::invalid_argument(
            fmt::format("max_feas_rest must be non-negative, got {}", o.max_feas_rest));

    // --- Strategy-combination guards ---
    // The SOC and extended-backtracking recovery links re-drive the acceptance
    // backtrack through the mechanism (GlobalizationMechanism::
    // run_acceptance_backtrack), which dispatches to the classic merit test or
    // to the generic AcceptanceStrategy::is_iterate_acceptable surface
    // (merit / funnel / filter) as appropriate — so a corrected or extended
    // step is tested against the SAME acceptance criteria the ordinary step
    // faced. Both links therefore compose with every acceptance strategy; there
    // is no classic-merit-only restriction. (The watchdog was always compatible
    // with every strategy.)

    // funnel/filter are designed to operate above a monotone barrier safeguard
    // (this is a factual dependency statement, not a convergence-guarantee
    // claim — neither strategy is proven to converge with or without it).
    // classic_adaptive (the default governor) is free-mode only, so pairing it
    // with funnel/filter silently drops that safeguard unless the user
    // explicitly opts in via never_monotone. classic_merit/merit are
    // unaffected by this guard in every combination (bit-identity for the
    // default path; merit + monitored is allowed opt-in, same as merit +
    // classic_adaptive).
    if ((o.acceptance_strategy == AcceptanceStrategies::funnel ||
         o.acceptance_strategy == AcceptanceStrategies::filter) &&
        o.barrier_governor == BarrierGovernors::classic_adaptive && !o.never_monotone)
        throw std::invalid_argument(
            fmt::format("acceptance_strategy={} is designed to operate above the monotone barrier "
                        "safeguard, which barrier_governor=classic_adaptive (the default) does not "
                        "provide: set barrier_governor=monitored, or set never_monotone=True to "
                        "explicitly accept adaptive-only operation",
                        acceptance_strategy_name(o.acceptance_strategy)));

    // never_monotone is an expert escape that explicitly accepts running
    // WITHOUT a monotone safeguard; barrier_governor=monitored already
    // provides one, so the combination is a direct contradiction.
    if (o.never_monotone && o.barrier_governor == BarrierGovernors::monitored)
        throw std::invalid_argument(
            "never_monotone=True is contradictory with barrier_governor=monitored: the "
            "monitored governor already provides the monotone safeguard never_monotone "
            "opts out of; set barrier_governor=classic_adaptive or never_monotone=False");

    // --- Convergence tolerances ---
    pos_finite(o.kkt_tol, "kkt_tol");
    pos_finite(o.econ_tol, "econ_tol");
    pos_finite(o.icon_tol, "icon_tol");
    pos_finite(o.bar_tol, "bar_tol");

    // --- Acceptable tolerances ---
    pos_finite(o.acc_kkt_tol, "acc_kkt_tol");
    pos_finite(o.acc_econ_tol, "acc_econ_tol");
    pos_finite(o.acc_icon_tol, "acc_icon_tol");
    pos_finite(o.acc_bar_tol, "acc_bar_tol");

    // --- Divergence tolerances ---
    pos_finite(o.div_kkt_tol, "div_kkt_tol");
    pos_finite(o.div_econ_tol, "div_econ_tol");
    pos_finite(o.div_icon_tol, "div_icon_tol");
    pos_finite(o.div_bar_tol, "div_bar_tol");

    // --- Cross-field: convergence tols <= acceptable tols <= divergence tols ---
    if (o.kkt_tol > o.acc_kkt_tol)
        throw std::invalid_argument(
            fmt::format("kkt_tol ({}) must be <= acc_kkt_tol ({})", o.kkt_tol, o.acc_kkt_tol));
    if (o.econ_tol > o.acc_econ_tol)
        throw std::invalid_argument(
            fmt::format("econ_tol ({}) must be <= acc_econ_tol ({})", o.econ_tol, o.acc_econ_tol));
    if (o.icon_tol > o.acc_icon_tol)
        throw std::invalid_argument(
            fmt::format("icon_tol ({}) must be <= acc_icon_tol ({})", o.icon_tol, o.acc_icon_tol));
    if (o.bar_tol > o.acc_bar_tol)
        throw std::invalid_argument(
            fmt::format("bar_tol ({}) must be <= acc_bar_tol ({})", o.bar_tol, o.acc_bar_tol));
    if (o.acc_kkt_tol > o.div_kkt_tol)
        throw std::invalid_argument(fmt::format("acc_kkt_tol ({}) must be <= div_kkt_tol ({})",
                                                o.acc_kkt_tol, o.div_kkt_tol));
    if (o.acc_econ_tol > o.div_econ_tol)
        throw std::invalid_argument(fmt::format("acc_econ_tol ({}) must be <= div_econ_tol ({})",
                                                o.acc_econ_tol, o.div_econ_tol));
    if (o.acc_icon_tol > o.div_icon_tol)
        throw std::invalid_argument(fmt::format("acc_icon_tol ({}) must be <= div_icon_tol ({})",
                                                o.acc_icon_tol, o.div_icon_tol));
    if (o.acc_bar_tol > o.div_bar_tol)
        throw std::invalid_argument(fmt::format("acc_bar_tol ({}) must be <= div_bar_tol ({})",
                                                o.acc_bar_tol, o.div_bar_tol));

    // --- Barrier parameters ---
    pos_finite(o.init_mu, "init_mu");
    pos_finite(o.min_mu, "min_mu");
    pos_finite(o.max_mu, "max_mu");
    if (o.min_mu > o.max_mu)
        throw std::invalid_argument(
            fmt::format("min_mu ({}) must be <= max_mu ({})", o.min_mu, o.max_mu));
    if (o.init_mu < o.min_mu || o.init_mu > o.max_mu)
        throw std::invalid_argument(
            fmt::format("init_mu ({}) must be within [min_mu ({}), max_mu ({})]", o.init_mu,
                        o.min_mu, o.max_mu));

    // --- Step parameters ---
    in_open_unit(o.bound_fraction, "bound_fraction");
    greater_than(o.bound_push, 0.0, "bound_push");
    // The interior push gives a two-sided variable p_L + p_U <= 2*k2*(u-l) of
    // its own interval, so the lower and upper projections can only cross --
    // landing the point on or outside a bound, whose barrier term then takes
    // the log of a non-positive number with no diagnostic -- once k2 reaches
    // one half. The upper bound is what makes the push's non-crossing property
    // an enforced invariant rather than a documented assumption.
    in_open_interval(o.bound_interval_push, 0.0, 0.5, "bound_interval_push");
    pos_finite(o.neg_slack_reset, "neg_slack_reset");
    greater_than(o.alpha_red, 1.0, "alpha_red");

    // --- Fixed-variable treatment ---
    // Zero is allowed: it records every declared bound verbatim. The ceiling is
    // what keeps a widened box from quietly becoming a different problem than the
    // declared one -- see kMaxBoundRelaxFactor. A NaN factor is rejected by the
    // helper's negated comparisons rather than passing both bounds.
    in_closed_interval(o.bound_relax_factor, 0.0, kMaxBoundRelaxFactor, "bound_relax_factor");
    check_fixed_variable_treatment(o.fixed_variable_treatment);

    // --- Hessian perturbation ---
    greater_than(o.delta_h, 0.0, "delta_h");
    greater_than(o.incr_h, 1.0, "incr_h");
    in_open_unit(o.decr_h, "decr_h");

    // --- QP solver ---
    pos_int(o.common.threads, "common.threads");
    if (o.qp_pivot_perturb < 0)
        throw std::invalid_argument(
            fmt::format("qp_pivot_perturb must be non-negative, got {}", o.qp_pivot_perturb));
    if (o.qp_ref_steps < 0)
        throw std::invalid_argument(
            fmt::format("qp_ref_steps must be non-negative, got {}", o.qp_ref_steps));
    if (o.qp_par_solve != 0 && o.qp_par_solve != 1)
        throw std::invalid_argument(
            fmt::format("qp_par_solve must be 0 or 1, got {}", o.qp_par_solve));
    if (o.qp_matching != 0 && o.qp_matching != 1)
        throw std::invalid_argument(
            fmt::format("qp_matching must be 0 or 1, got {}", o.qp_matching));
    if (o.qp_scaling != 0 && o.qp_scaling != 1)
        throw std::invalid_argument(fmt::format("qp_scaling must be 0 or 1, got {}", o.qp_scaling));

    // --- Objective ---
    // POSITIVE, not merely nonzero. A negative factor does not rescale the
    // problem, it reverses it: minimizing s*f for s < 0 maximizes f, while the
    // multiplier cones the solve reports against do not turn over with it. The
    // inequality multipliers stay non-negative and a bound multiplier keeps its
    // documented sign at an active bound, so the reporting seam would divide a
    // sign-constrained dual by a negative number and hand back a value its own
    // convention says cannot occur. Maximization is a different problem
    // statement, not a scale, and would need its own mode.
    if (!std::isfinite(o.obj_scale) || o.obj_scale <= 0.0)
        throw std::invalid_argument(
            fmt::format("obj_scale must be finite and strictly positive, got {}", o.obj_scale));

    // --- Output ---
    if (o.common.print_level < 0)
        throw std::invalid_argument(
            fmt::format("common.print_level must be non-negative, got {}", o.common.print_level));

#ifdef USE_ACCELERATE_SPARSE
    // --- Accelerate sparse solver ---
    pos_finite(o.accel_pivot_tolerance, "accel_pivot_tolerance");
    pos_finite(o.accel_zero_tolerance, "accel_zero_tolerance");
#endif
}

// Named configuration presets

hven::solvers::IpmOptions hven::solvers::ipm_preset(std::string_view name) {
    IpmOptions o;
    for (const auto &entry : kInteriorPointSolverPresets) {
        if (entry.name_ != name)
            continue;
        const InteriorPointSolverPresetFields &f = entry.fields_;
        o.acceptance_strategy = f.acceptance_strategy_;
        o.merit_penalty_rule = f.merit_penalty_rule_;
        o.barrier_governor = f.barrier_governor_;
        o.never_monotone = f.never_monotone_;
        o.restoration_mode = f.restoration_mode_;
        o.inertia_mode = f.inertia_mode_;
        o.max_soc = f.max_soc_;
        o.ls_extended_iters = f.ls_extended_iters_;
        o.watchdog = f.watchdog_;
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
    // The whole of the old jet_initialize(): one backend thread and silent
    // printing under the IPM convention (3 and above is silent; 10 is what the
    // wrapper wrote). The partition count it also set is the PROGRAM's and is
    // documented on the declaration -- no options value reaches a layout.
    base.common.threads = 1;
    base.common.print_level = 10;
    return base;
}
