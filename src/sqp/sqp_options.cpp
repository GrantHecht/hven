// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// sqp_options.cpp -- the SqpOptions boundary validation SqpSolver's
// constructor runs, carved out of the class body in sqp_solver.h.
//
// CLAUDE.md section 5 homes "orchestration, drivers, options,
// printing, and instrumentation" in .cpp translation units regardless of how
// hot the surrounding loop is; this block is options validation and runs
// exactly ONCE per driver construction, so nothing about it depends on
// inlining through a template parameter. As an inline body inside
// `class SqpSolver`, the six comparison chains and six
// `fmt::format` call chains were parsed and code-generated in every TU that
// included sqp_solver.h -- and the SQP tree is header-only today, so that is
// every test, bench and library TU that touches the driver at all.
//
// CLAUDE.md section 4 IS THE CONTRACT THIS FILE IMPLEMENTS, verbatim: validate
// sizes and bounds at API boundaries; never print a diagnostic that is not
// also folded into the thrown exception's message; never construct an
// exception without throwing it; never call exit(). Every rejection below is a
// single `throw std::invalid_argument(fmt::format(...))` naming the option the
// caller actually set and the value it actually had.
//
// ---------------------------------------------------------------------------
// THE `!(x > 0.0)` IDIOM IS LOAD-BEARING AND IT IS A FLAG PREMISE, NOT A STYLE
// CHOICE. Read this before "simplifying" any predicate in this file.
//
// Every check is written as the NEGATION of the acceptance condition --
// `!(kkt_tol > 0.0)`, `!(trust_region_initial > 0.0)`, `!(trust_region_max >=
// trust_region_initial)`,
// `!(trust_region_min <= trust_region_initial)` -- because that is the form that rejects NaN. A NaN
// compares false against everything, so `!(x > 0.0)` is TRUE for NaN and the
// option is rejected, whereas the "obvious" complement `x <= 0.0` is FALSE for
// NaN and would let a NaN tolerance or a NaN radius through into the solve.
//
// That reasoning only holds if the compiler is not permitted to assume NaN
// cannot occur. hven builds with `-ffast-math`, which alone implies
// `-ffinite-math-only`, under which the compiler MAY rewrite `!(x > 0.0)` into
// `x <= 0.0` -- the two are equivalent exactly when NaN is excluded by
// assumption. hven therefore appends `-fno-finite-math-only` immediately after
// `-ffast-math` in the SAFER_FAST FP mode that is this library's default and
// its one uniform flag regime
// (cmake/hven_compile_options.cmake, the HVEN_FP_MODE block: `-ffast-math`
// then `-fno-finite-math-only`; on Windows `/fp:fast` plus the probed spelling
// of the same). SAFER_FAST is exactly this: fast math WITHOUT the finiteness
// assumption.
//
// **THIS PREMISE IS PINNED BY DISASSEMBLY, NOT BY ASSERTION.** The proof
// battery for this file disassembles this TU's object and checks that the NaN-catching
// comparisons survive as written -- that the emitted branch is the
// unordered-aware form (`comisd` + `jbe`/`ja`, i.e. a branch that takes the
// reject path when the compare sets PF) and not the finite-math complement.
// If a future flag change drops
// `-fno-finite-math-only`, that pin is what turns a silent NaN admission into
// a caught one.
//
// ---------------------------------------------------------------------------
// FP-ARITHMETIC-FREE. There is not one floating-point operation in this file:
// every use of a double is a COMPARISON (`>`, `>=`, `<=`), a call to
// `std::isinf`, or an argument handed to `fmt::format` for rendering. `fmt`
// reads a double and formats its value; it does not compute with it. So
// nothing here can be re-associated, contracted into an FMA, or otherwise
// re-shaped by the codegen flags -- which, together with the pin above, is why
// this file carries no FP-reassociation risk at all.

#include <cmath>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

#include <hven/detail/solver/common_options_validate.h>
#include <hven/detail/solver/option_info_builder.h>
#include <hven/detail/sqp/sqp_options_fields.h>
#include <hven/qp/qp_types.h>
#include <hven/sqp/sqp_options.h>

namespace hven {

namespace {

// An enum field is a plain aggregate member, so a value outside its enumerators
// is a reachable input. `last` is the final enumerator.
template <typename E> void enum_in_range(E v, E last, const char *name) {
    const auto i = static_cast<long long>(v);
    if (i < 0 || i > static_cast<long long>(last)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: {} ({}) is not one of its enumerators", name, i));
    }
}

} // namespace

void validate(const SqpOptions &opts) {
    // This engine's floors: no iterations and the backend's own thread default are legal.
    detail::validate_common_options(opts.common, {.max_iterations = 0, .threads = 0});
    if (opts.qp.max_iterations && *opts.qp.max_iterations < 1) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: qp.max_iterations ({}) must be empty or >= 1", *opts.qp.max_iterations));
    }
    // THE QP BLOCK'S OWN FIELDS (TU-0): each in the negated form, so NaN is refused.
    if (!(opts.qp.dual_regularization >= 0.0) || std::isinf(opts.qp.dual_regularization)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: qp.dual_regularization ({}) must be finite and >= 0",
                        opts.qp.dual_regularization));
    }
    if (!(opts.qp.primal_regularization >= 0.0) || std::isinf(opts.qp.primal_regularization)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: qp.primal_regularization ({}) must be finite and >= 0",
                        opts.qp.primal_regularization));
    }
    if (!(opts.qp.feasibility_tol > 0.0) || std::isinf(opts.qp.feasibility_tol)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: qp.feasibility_tol ({}) must be finite and > 0", opts.qp.feasibility_tol));
    }
    if (!(opts.qp.optimality_tol > 0.0) || std::isinf(opts.qp.optimality_tol)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: qp.optimality_tol ({}) must be finite and > 0", opts.qp.optimality_tol));
    }
    if (!(opts.qp.schur_condition_max >= 1.0) || std::isinf(opts.qp.schur_condition_max)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: qp.schur_condition_max ({}) must be finite and >= 1; it bounds a condition "
            "estimate, which is never below 1",
            opts.qp.schur_condition_max));
    }
    // 0 is legal: a border of dimension 0 is never admitted, so every working-set change
    // refactorizes (the walk latches on the cap at the first pinned variable).
    if (!(opts.qp.max_schur_border_dimension >= 0)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: qp.max_schur_border_dimension ({}) must be >= 0",
                        opts.qp.max_schur_border_dimension));
    }
    enum_in_range(opts.qp.working_set_algebra, QpWorkingSetAlgebra::kSchurBorder,
                  "qp.working_set_algebra");
    // THE ACCEPTABLE TIER (RF-1): finite and > 0 each, and no tighter than the convergence tier,
    // on the IPM's rule (ipm_options.cpp) -- an acceptable point a converged one would not be is
    // the only order that makes the label weaker than kOptimal.
    const SqpTerminationOptions &t = opts.termination;
    const auto acceptable = [](double v, const char *name, double floor, const char *floor_name) {
        if (!(v > 0.0) || std::isinf(v)) {
            throw std::invalid_argument(
                fmt::format("SqpSolver: termination.{} ({}) must be finite and > 0", name, v));
        }
        if (!(floor <= v)) {
            throw std::invalid_argument(fmt::format(
                "SqpSolver: {} ({}) must be <= termination.{} ({})", floor_name, floor, name, v));
        }
    };
    acceptable(t.acceptable_stationarity_tol, "acceptable_stationarity_tol",
               opts.common.optimality_tol, "common.optimality_tol");
    acceptable(t.acceptable_equality_tol, "acceptable_equality_tol", opts.common.feasibility_tol,
               "common.feasibility_tol");
    acceptable(t.acceptable_inequality_tol, "acceptable_inequality_tol",
               opts.common.feasibility_tol, "common.feasibility_tol");
    acceptable(t.acceptable_complementarity_tol, "acceptable_complementarity_tol",
               opts.common.optimality_tol, "common.optimality_tol");
    if (!(opts.qp.trust_region_radius >= 0.0)) { // +inf passes; NaN and negatives do not
        throw std::invalid_argument(fmt::format(
            "SqpSolver: qp.trust_region_radius ({}) must be >= 0 (+inf is legal and means none)",
            opts.qp.trust_region_radius));
    }
    if (!(opts.trust_region_initial > 0.0)) { // catches NaN, negatives and exactly 0
        throw std::invalid_argument(fmt::format("SqpSolver: trust_region_initial ({}) must be > 0 "
                                                "(use +inf for no trust region; 0 would "
                                                "pin every step to zero and stall until max_iter)",
                                                opts.trust_region_initial));
    }
    // trust_region_max caps the growth rule, so a
    // ceiling below the floor would be a contradiction rather than an
    // ignored field: it would silently mean "the radius may never grow,
    // and the very first subproblem already violates the cap".
    //
    // trust_region_initial == +inf IS EXEMPT FROM THIS PARTICULAR CHECK, because at
    // that value there is no starting radius for trust_region_max to be a ceiling
    // ON. It is NOT exempt from trust_region_max MATTERING: the
    // FIRST rejection lands the radius on trust_region_max (see sqp_solver.h's RADIUS
    // MANAGEMENT), so the pair is meaningful after all -- what +inf now means
    // is "unbounded until the method finds it needs a bound, then trust_region_max".
    // The growth rule is still skipped while the radius is +inf
    // (min(+inf*2, trust_region_max) would REDUCE it, which no growth rule may do).
    if (!std::isinf(opts.trust_region_initial) &&
        !(opts.trust_region_max >= opts.trust_region_initial)) { // catches NaN too
        throw std::invalid_argument(fmt::format("SqpSolver: trust_region_max ({}) must be >= "
                                                "trust_region_initial ({}); it is the ceiling "
                                                "the trust-region growth rule expands toward",
                                                opts.trust_region_max, opts.trust_region_initial));
    }
    // THE FLOOR. It must be positive (0 would make it
    // unreachable, since the radius only ever halves, and the restoration
    // trigger it exists to arm would be dead), and it must not exceed
    // either end of the range it floors -- trust_region_min > trust_region_initial would put the
    // FIRST subproblem below the floor, i.e. request restoration before
    // the solve has tried anything, and trust_region_min > trust_region_max would do the same
    // to the radius a +inf trust_region_initial lands on. Both forms catch NaN.
    if (!(opts.trust_region_min > 0.0)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: trust_region_min ({}) must be > 0; it is the radius floor whose crossing "
            "raises a restoration request, and 0 can never be crossed by halving",
            opts.trust_region_min));
    }
    if (!(opts.trust_region_min <= opts.trust_region_initial) ||
        !(opts.trust_region_min <= opts.trust_region_max)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: trust_region_min ({}) must be <= both trust_region_initial "
                        "({}) and trust_region_max ({}); "
                        "a floor above the starting radius requests restoration before the "
                        "first trial is judged",
                        opts.trust_region_min, opts.trust_region_initial, opts.trust_region_max));
    }
    // THE PROBLEM-SCALING RULE'S TWO PARAMETERS (M6 W0.2). Validated
    // UNCONDITIONALLY, not under `if (opts.row_scaling)`: an options object
    // is a value a caller may flip a bool on later, and a rule that is
    // nonsensical the moment the toggle moves was already nonsensical when it
    // was set. Both forms catch NaN, for this file's banner's reason.
    if (!(opts.scaling_max_gradient > 0.0) || std::isinf(opts.scaling_max_gradient)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: scaling_max_gradient ({}) must be finite and > 0; it is the inf-norm "
            "the scaled objective gradient and each scaled Jacobian row are aimed at",
            opts.scaling_max_gradient));
    }
    if (!(opts.scaling_factor_max >= 1.0) || std::isinf(opts.scaling_factor_max)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: scaling_factor_max ({}) must be finite and >= 1; it clamps every "
            "factor to [1/limit, limit], and a limit below 1 inverts that interval",
            opts.scaling_factor_max));
    }
    if (!(opts.column_scaling_factor_max >= 1.0) || std::isinf(opts.column_scaling_factor_max)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: column_scaling_factor_max ({}) must be finite and >= 1; it clamps every "
            "column factor to [1/limit, limit]",
            opts.column_scaling_factor_max));
    }
    if (!(opts.caller_step_limit >= 0.0) || std::isinf(opts.caller_step_limit)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: caller_step_limit ({}) must be finite and >= 0; 0 is off, and r > 0 bounds "
            "|dx_j| by r * (1 + |x_j|) in caller units",
            opts.caller_step_limit));
    }
    enum_in_range(opts.advanced.qp_dump_policy, QpDumpPolicy::kAll, "advanced.qp_dump_policy");
    if (opts.advanced.qp_dump_policy != QpDumpPolicy::kOff && opts.advanced.qp_dump_dir.empty()) {
        throw std::invalid_argument(
            "SqpSolver: advanced.qp_dump_policy is on but advanced.qp_dump_dir is empty; name the "
            "directory the dump files are written to");
    }
    // `row_scaling_power_of_two` is a BOOL with no refusable value.
    // THE COLUMN LAYER (M7 W2 SCL), validated unconditionally for the same reason.
    enum_in_range(opts.column_scaling, SqpColumnScaling::kAuto, "column_scaling");
    if (!(opts.column_scaling_threshold >= 1.0) || std::isinf(opts.column_scaling_threshold)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: column_scaling_threshold ({}) must be finite and >= 1; it is the "
            "column factors' max/min spread at which kAuto scales, and a spread is never "
            "below 1",
            opts.column_scaling_threshold));
    }
    // THE kIpqp TIER'S OWN OPTIONS (qp/qp_options.h): validated unconditionally, at the
    // position they have always had.
    validate(opts.advanced.ipqp);
    // Every enum field is checked against its enumerators: a value outside them
    // is a reachable input through a plain aggregate field.
    enum_in_range(opts.qp_mode, QpMode::kIpqp, "qp_mode");
    enum_in_range(opts.advanced.elastic_rung_engine, SqpElasticRungEngine::kTier,
                  "advanced.elastic_rung_engine");
    enum_in_range(opts.advanced.ssn_sigma_rule, SsnSigmaRule::kResidualAlways,
                  "advanced.ssn_sigma_rule");
    enum_in_range(opts.advanced.ssn_hint_rule, SsnHintRule::kWatchdog, "advanced.ssn_hint_rule");
    enum_in_range(opts.advanced.ssn_infeasibility_rule, SsnInfeasibilityRule::kFarkasGated,
                  "advanced.ssn_infeasibility_rule");
    // kIpqp is a reachable mode (sqp_solver.cpp's THE QP KERNEL DISPATCH), so
    // nothing here refuses it. The IpqpOptions predicates above are validated
    // UNCONDITIONALLY, at every mode: a field is out of range whether or not
    // this solve will read it.

    // THE START-POINT PROJECTION (M7 W2 M4), in declaration position.
    //
    // `project_start_point` is a BOOL and has NO refusable value: both states
    // are honoured, both are meaningful, and neither can be ill-formed. There
    // is no companion field it can contradict either -- it reads the model's
    // own declared box and nothing on this struct. So this entry is a stated
    // line rather than a predicate, which is what the W2 per-toggle contract
    // (docs/notes/2026-09-m7-w2-a0-signals-design.md section 2, clause 2) asks
    // for: the field is accounted for HERE, in its declaration position, so
    // that the next field added beside it is written with a predicate rather
    // than added to a struct whose validator never mentioned its neighbour.
    // Inventing an always-true test to fill the slot would be dead code that
    // proves nothing and would have to be read as if it did.

    // THE SUBPROBLEM-FAILURE BUDGET (M7 W2 M1), in declaration position. An
    // `Index`, so the direct form applies rather than this file's
    // negation-of-acceptance idiom's NaN reason -- but it is written in the
    // negated form anyway, as the contract asks, so that the predicate READS as
    // the acceptance condition it denies. A budget of 0 would refuse the first
    // retry this engine has always taken, i.e. silently change the shipped
    // behaviour through a value rather than through the field's own meaning,
    // and a negative is not a count at all.
    if (!(opts.advanced.max_consecutive_qp_failures >= 1)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: advanced.max_consecutive_qp_failures ({}) must be >= 1; it is the number "
            "of "
            "CONSECUTIVE "
            "subproblem failures the driver may route at one iterate, and 1 is the shipped "
            "behaviour (route the first, end the solve on the second). 0 would refuse the "
            "first retry, which is a behaviour change spelled as a budget",
            opts.advanced.max_consecutive_qp_failures));
    }

    // THE CAP RETRY (M7 W2 M1), in declaration position and on
    // `project_start_point`'s terms above: `qp_cap_retry` is a BOOL with no
    // refusable value and no companion field it can contradict, so its entry is
    // this stated line rather than an always-true predicate.

    // THE FAILED SUBPROBLEM'S OWN STEP (M7 W2 R2): `qp_failure_step` is a BOOL
    // on the same terms -- no refusable value, no companion it can contradict.

    // THE RESTORATION PROXIMITY WEIGHT (M7 W2 M2b), in declaration position and
    // in this file's negation-of-acceptance idiom, which here does two jobs:
    // `!(w >= 0.0)` refuses a negative AND a NaN, and the finiteness test
    // refuses `+inf`. An infinite weight is not "the term, very heavy": it makes
    // the wrapper's objective +inf at every point but the centre, so the
    // sub-solve would be comparing infinities rather than minimizing anything.
    // 0.0 IS ACCEPTED and is the shipped value -- it is the term's OFF state,
    // not an ill-formed one.
    if (!(opts.advanced.restoration_proximity >= 0.0) ||
        !std::isfinite(opts.advanced.restoration_proximity)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: advanced.restoration_proximity ({}) must be a finite value >= 0; it is the "
            "coefficient zeta on the restoration feasibility wrapper's 1/2 zeta ||D (x - "
            "x_k)||^2 proximity term, 0 is OFF (the shipped value, the wrapper that has "
            "always shipped), and an infinite weight makes that objective +inf everywhere "
            "but at the requesting iterate",
            opts.advanced.restoration_proximity));
    }

    // THE SUFFICIENT-REDUCTION RESUME (M7 W2 P1E): a fraction of the entry's
    // violation, so [0, 1); 0 is OFF. `!(k >= 0) || !(k < 1)` refuses NaN too.
    if (!(opts.advanced.restoration_resume_reduction >= 0.0) ||
        !(opts.advanced.restoration_resume_reduction < 1.0)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: advanced.restoration_resume_reduction ({}) must be in [0, 1); it is the "
            "fraction of the restoration entry's violation at which the phase also "
            "resumes, and 0 is OFF",
            opts.advanced.restoration_resume_reduction));
    }
    if (opts.advanced.restoration_stall_factorizations < 0) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: advanced.restoration_stall_factorizations must be >= 0, got {}",
                        opts.advanced.restoration_stall_factorizations));
    }

    // THE CURVATURE SOURCE (M7 QN stage 3): the mode must be one of the two; the
    // window must hold a pair; and the restoration wrapper's exact zeta D^2 term has
    // no QN split shipped (ruling C30 AMEND-7), so the combination is refused.
    enum_in_range(opts.hessian_mode, SqpHessianMode::kLbfgsDamped, "hessian_mode");
    if (!(opts.lbfgs_memory >= 1)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: lbfgs_memory ({}) must be >= 1 (the L-BFGS window length)",
                        opts.lbfgs_memory));
    }
    if (opts.hessian_mode == SqpHessianMode::kLbfgsDamped &&
        opts.advanced.restoration_proximity > 0.0) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: hessian_mode == kLbfgsDamped with restoration_proximity ({}) > 0 is not "
            "supported: the restoration wrapper's exact zeta D^2 term has no quasi-Newton split; "
            "use restoration_proximity = 0",
            opts.advanced.restoration_proximity));
    }
    if (!(opts.advanced.max_lbfgs_full_memory_dimension >= 0)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: advanced.max_lbfgs_full_memory_dimension ({}) must be >= 0 (0 "
                        "= never full memory)",
                        opts.advanced.max_lbfgs_full_memory_dimension));
    }
    // `qn_update_on_reject` and `qn_nonlinear_only` are BOOLs with no refusable value.

    // THE RESTORATION PHASE'S CURVATURE AND ENTRY COUNT (M7 W2 RST).
    enum_in_range(opts.advanced.restoration_curvature,
                  SqpRestorationCurvature::kModelThenRegularized, "advanced.restoration_curvature");
    if (!(opts.advanced.max_restoration_entries >= 0)) {
        throw std::invalid_argument(fmt::format("SqpSolver: advanced.max_restoration_entries ({}) "
                                                "must be >= 0 (0 refuses restoration)",
                                                opts.advanced.max_restoration_entries));
    }

    // THE TU-0 LEVERS.
    enum_in_range(opts.advanced.trust_region_growth, SqpTrustRegionGrowth::kAnyAccepted,
                  "advanced.trust_region_growth");
    if (!(opts.advanced.ssn_farkas_residual_tol > 0.0) ||
        !(opts.advanced.ssn_farkas_residual_tol < 1.0)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: advanced.ssn_farkas_residual_tol ({}) must be in (0, 1)",
                        opts.advanced.ssn_farkas_residual_tol));
    }
    if (!(opts.advanced.ssn_uncertain_tol >= 0.0) || !(opts.advanced.ssn_uncertain_tol < 1.0)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: advanced.ssn_uncertain_tol ({}) must be in [0, 1)",
                        opts.advanced.ssn_uncertain_tol));
    }
    if (!(opts.advanced.ssn_lm_sigma_c > 0.0) || std::isinf(opts.advanced.ssn_lm_sigma_c)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: advanced.ssn_lm_sigma_c ({}) must be finite and > 0",
                        opts.advanced.ssn_lm_sigma_c));
    }
    if (!(opts.advanced.ssn_lm_sigma_exponent >= 1.0) ||
        !(opts.advanced.ssn_lm_sigma_exponent <= 2.0)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: advanced.ssn_lm_sigma_exponent ({}) must be in [1, 2]",
                        opts.advanced.ssn_lm_sigma_exponent));
    }
    // `restoration_elastic_seed` is a BOOL with no refusable value.
    if (!(opts.advanced.elastic_rho0_headroom_rungs >= 1) ||
        !(opts.advanced.elastic_rho0_headroom_rungs <= 6)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: advanced.elastic_rho0_headroom_rungs ({}) must be in [1, 6]",
                        opts.advanced.elastic_rho0_headroom_rungs));
    }
    if (!(opts.advanced.adaptive_dual_regularization_max >= 1e-12) ||
        !(opts.advanced.adaptive_dual_regularization_max <= 1.0)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: advanced.adaptive_dual_regularization_max ({}) must be in [1e-12, 1]; it "
            "is the schedule's ceiling, whose floor is 1e-12",
            opts.advanced.adaptive_dual_regularization_max));
    }

    // THE DENSE ARM: kNever, kAlways and kAuto (M8 DN-3: the dense arm for each analyzed
    // pattern whose order is at most the library's threshold,
    // detail::kSqpDenseSelectionThreshold). The threshold is not a caller's option yet.
    enum_in_range(opts.linear.dense_factorization, hven::linear::DensePolicy::kAuto,
                  "linear.dense_factorization");
    if (opts.linear.max_dense_dimension) {
        throw std::invalid_argument(
            "SqpSolver: linear.max_dense_dimension is reserved: DensePolicy::kAuto uses the "
            "library's threshold; leave it empty");
    }
}

// The SQP engine's named presets. "default" is the only one until M8's labeled
// configs; it returns a default-constructed value, which is what every caller
// that writes `SqpOptions{}` already gets. The refusal lists the valid names for
// the same reason ipm_preset()'s does.
SqpOptions sqp_preset(std::string_view name) {
    if (name == "default") {
        return SqpOptions{};
    }
    throw std::invalid_argument(
        fmt::format("Unrecognized SqpSolver preset '{}'. Valid options are: default", name));
}

SqpOptions worker_options(SqpOptions base) {
    base.common.threads = 1;
    base.common.verbosity = Verbosity::kSilent;
    return base;
}

// =============================================================================
// The options dump, and the options metadata.
//
// Both are walks over the same per-struct tables (detail/sqp/sqp_options_fields.h and the
// QP-layer tables it uses), one row per field in declaration order, so a field cannot be dumped
// and missing from the metadata or the reverse. The contract -- order, prefixes, enumerator
// names, shortest round-trip doubles -- is documented once at dump_options(const IpmOptions &,
// std::ostream &) in ipm/ipm_options.h and is not restated here. The tables are checked against
// the structs at compile time, below. (M7 W0 T5(b) wrote the dump by hand, one line per field,
// pinned by a drift test; M8 AP-4 G1 replaced it with the tables.)
//
// NOTHING BELOW WEAKENS THIS FILE'S FP PREMISE. The dump performs no floating-point operation
// either: every double it touches is handed to fmt::format for rendering, exactly as the
// rejection messages above do.
// =============================================================================

// -Wreorder-init-list is the order check below: a table whose rows are out of declaration order
// is an error here, not a warning.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic error "-Wreorder-init-list"
#endif
HVEN_CHECK_OPTIONS_TABLE(QpOptions, HVEN_QP_OPTIONS_FIELDS)
HVEN_CHECK_OPTIONS_TABLE(IpqpOptions, HVEN_IPQP_OPTIONS_FIELDS)
HVEN_CHECK_OPTIONS_TABLE(SqpTerminationOptions, HVEN_SQP_TERMINATION_OPTIONS_FIELDS)
HVEN_CHECK_OPTIONS_TABLE(SqpLinearOptions, HVEN_SQP_LINEAR_OPTIONS_FIELDS)
HVEN_CHECK_OPTIONS_TABLE_G(SqpAdvancedOptions, HVEN_SQP_ADVANCED_OPTIONS_FIELDS)
HVEN_CHECK_OPTIONS_TABLE_G(SqpOptions, HVEN_SQP_OPTIONS_FIELDS)
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace {

// CANONICAL BYTES, NOT THE CALLER'S FORMATTING (M7 W0 T5 fix round 1, sol Important 1): every line
// is rendered whole and handed to the stream by `write()`, an UNFORMATTED output function, so the
// caller's flags, precision, width, fill and imbued locale reach no byte of the output. The
// reasoning is recorded in full in src/ipm/ipm_options.cpp.
void emit(std::ostream &os, const std::string &line) {
    os.write(line.data(), static_cast<std::streamsize>(line.size()));
}

} // namespace

void dump_options(const SqpOptions &o, std::ostream &os) {
    detail::for_each_option(o, [&os](const detail::OptionMeta &m, const auto &value) {
        emit(os, m.name() + " = " + detail::render_option_value(value) + "\n");
    });
}

const std::vector<OptionInfo> &sqp_option_info() {
    static const std::vector<OptionInfo> rows = [] {
        const SqpOptions defaults;
        std::vector<OptionInfo> out;
        detail::for_each_option(defaults, [&out](const detail::OptionMeta &m, const auto &value) {
            out.push_back(detail::describe_option(m, value));
        });
        return out;
    }();
    return rows;
}

} // namespace hven
