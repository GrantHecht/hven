// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// sqp_options.cpp -- the SqpOptions boundary validation SqpDriver's
// constructor runs, carved out of the class body in sqp_driver.h.
//
// CLAUDE.md section 5 homes "orchestration, drivers, options,
// printing, and instrumentation" in .cpp translation units regardless of how
// hot the surrounding loop is; this block is options validation and runs
// exactly ONCE per driver construction, so nothing about it depends on
// inlining through a template parameter. As an inline body inside
// `class SqpDriver`, the six comparison chains and six
// `fmt::format` call chains were parsed and code-generated in every TU that
// included sqp_driver.h -- and the SQP tree is header-only today, so that is
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
// `!(kkt_tol > 0.0)`, `!(tr_init > 0.0)`, `!(tr_max >= tr_init)`,
// `!(tr_min <= tr_init)` -- because that is the form that rejects NaN. A NaN
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
#include <stdexcept>
#include <string_view>

#include <fmt/format.h>

#include <hven/drivers/sqp_types.h>

namespace hven::solvers {

void validate(const SqpOptions &opts) {
    if (!(opts.kkt_tol > 0.0) || !(opts.feas_tol > 0.0)) {
        throw std::invalid_argument(
            fmt::format("SqpDriver: kkt_tol ({}) and feas_tol ({}) must both be > 0", opts.kkt_tol,
                        opts.feas_tol));
    }
    if (opts.max_iter < 0) {
        throw std::invalid_argument(
            fmt::format("SqpDriver: max_iter ({}) must be >= 0", opts.max_iter));
    }
    if (!(opts.tr_init > 0.0)) { // catches NaN, negatives and exactly 0
        throw std::invalid_argument(fmt::format(
            "SqpDriver: tr_init ({}) must be > 0 (use +inf for no trust region; 0 would "
            "pin every step to zero and stall until max_iter)",
            opts.tr_init));
    }
    // tr_max caps the growth rule, so a
    // ceiling below the floor would be a contradiction rather than an
    // ignored field: it would silently mean "the radius may never grow,
    // and the very first subproblem already violates the cap".
    //
    // tr_init == +inf IS EXEMPT FROM THIS PARTICULAR CHECK, because at
    // that value there is no starting radius for tr_max to be a ceiling
    // ON. It is NOT exempt from tr_max MATTERING: the
    // FIRST rejection lands the radius on tr_max (see sqp_driver.h's RADIUS
    // MANAGEMENT), so the pair is meaningful after all -- what +inf now means
    // is "unbounded until the method finds it needs a bound, then tr_max".
    // The growth rule is still skipped while the radius is +inf
    // (min(+inf*2, tr_max) would REDUCE it, which no growth rule may do).
    if (!std::isinf(opts.tr_init) && !(opts.tr_max >= opts.tr_init)) { // catches NaN too
        throw std::invalid_argument(
            fmt::format("SqpDriver: tr_max ({}) must be >= tr_init ({}); it is the ceiling "
                        "the trust-region growth rule expands toward",
                        opts.tr_max, opts.tr_init));
    }
    // THE FLOOR. It must be positive (0 would make it
    // unreachable, since the radius only ever halves, and the restoration
    // trigger it exists to arm would be dead), and it must not exceed
    // either end of the range it floors -- tr_min > tr_init would put the
    // FIRST subproblem below the floor, i.e. request restoration before
    // the solve has tried anything, and tr_min > tr_max would do the same
    // to the radius a +inf tr_init lands on. Both forms catch NaN.
    if (!(opts.tr_min > 0.0)) {
        throw std::invalid_argument(
            fmt::format("SqpDriver: tr_min ({}) must be > 0; it is the radius floor whose crossing "
                        "raises a restoration request, and 0 can never be crossed by halving",
                        opts.tr_min));
    }
    if (!(opts.tr_min <= opts.tr_init) || !(opts.tr_min <= opts.tr_max)) {
        throw std::invalid_argument(
            fmt::format("SqpDriver: tr_min ({}) must be <= both tr_init ({}) and tr_max ({}); "
                        "a floor above the starting radius requests restoration before the "
                        "first trial is judged",
                        opts.tr_min, opts.tr_init, opts.tr_max));
    }
    // THE PROBLEM-SCALING RULE'S TWO PARAMETERS (M6 W0.2). Validated
    // UNCONDITIONALLY, not under `if (opts.enable_scaling)`: an options object
    // is a value a caller may flip a bool on later, and a rule that is
    // nonsensical the moment the toggle moves was already nonsensical when it
    // was set. Both forms catch NaN, for this file's banner's reason.
    if (!(opts.scaling_max_gradient > 0.0) || std::isinf(opts.scaling_max_gradient)) {
        throw std::invalid_argument(fmt::format(
            "SqpDriver: scaling_max_gradient ({}) must be finite and > 0; it is the inf-norm "
            "the scaled objective gradient and each scaled Jacobian row are aimed at",
            opts.scaling_max_gradient));
    }
    if (!(opts.scaling_factor_limit >= 1.0) || std::isinf(opts.scaling_factor_limit)) {
        throw std::invalid_argument(fmt::format(
            "SqpDriver: scaling_factor_limit ({}) must be finite and >= 1; it clamps every "
            "factor to [1/limit, limit], and a limit below 1 inverts that interval",
            opts.scaling_factor_limit));
    }
    // THE kIpm TIER'S OWN OPTIONS (M6 W1 task 1). Validated UNCONDITIONALLY,
    // exactly like the scaling fields just above and for the same reason: an
    // options object is a value whose qp_mode a caller may set later, so a
    // nonsensical IpqpOptions field was already nonsensical when it was set.
    // The `Index` (integer) fields below use the same direct form max_iter
    // does above -- an Index has no NaN to guard against, so the
    // negation-of-acceptance idiom's NaN-rejection reason does not apply to
    // them; every `double` field below uses that idiom, for this file's own
    // reason.
    if (opts.ipqp.ipqp_hard_iter_cap <= 0) {
        throw std::invalid_argument(fmt::format(
            "SqpDriver: ipqp.ipqp_hard_iter_cap ({}) must be > 0; it is the budget of last "
            "resort and, unlike ipqp_max_iter, carries no size-derived sentinel reading",
            opts.ipqp.ipqp_hard_iter_cap));
    }
    if (!(opts.ipqp.ipqp_min_mu > 0.0) || std::isinf(opts.ipqp.ipqp_min_mu)) {
        throw std::invalid_argument(
            fmt::format("SqpDriver: ipqp.ipqp_min_mu ({}) must be finite and > 0; it is the "
                        "floor of the mu_0 clamp and the tier's own barrier-decay floor",
                        opts.ipqp.ipqp_min_mu));
    }
    if (!(opts.ipqp.ipqp_init_mu > 0.0) || std::isinf(opts.ipqp.ipqp_init_mu)) {
        throw std::invalid_argument(fmt::format(
            "SqpDriver: ipqp.ipqp_init_mu ({}) must be finite and > 0; it is the cold starting "
            "mu and the ceiling of the mu_0 clamp",
            opts.ipqp.ipqp_init_mu));
    }
    if (!(opts.ipqp.ipqp_min_mu <= opts.ipqp.ipqp_init_mu)) {
        throw std::invalid_argument(fmt::format(
            "SqpDriver: ipqp.ipqp_min_mu ({}) must be <= ipqp.ipqp_init_mu ({}); the mu_0 "
            "clamp's band [ipqp_min_mu, ipqp_init_mu] is otherwise inverted",
            opts.ipqp.ipqp_min_mu, opts.ipqp.ipqp_init_mu));
    }
    if (!(opts.ipqp.ipqp_reg_floor > 0.0) || std::isinf(opts.ipqp.ipqp_reg_floor)) {
        throw std::invalid_argument(fmt::format(
            "SqpDriver: ipqp.ipqp_reg_floor ({}) must be finite and > 0; it is the absolute "
            "floor the (rho, delta) ladder's gated decrease may never cross",
            opts.ipqp.ipqp_reg_floor));
    }
    if (!(opts.ipqp.ipqp_reg_max >= opts.ipqp.ipqp_reg_floor) ||
        std::isinf(opts.ipqp.ipqp_reg_max)) {
        throw std::invalid_argument(fmt::format(
            "SqpDriver: ipqp.ipqp_reg_max ({}) must be finite and >= ipqp.ipqp_reg_floor ({}); "
            "it is the ceiling the (rho, delta) ladder's growth may never cross",
            opts.ipqp.ipqp_reg_max, opts.ipqp.ipqp_reg_floor));
    }
    if (!(opts.ipqp.ipqp_rho_init >= opts.ipqp.ipqp_reg_floor) ||
        !(opts.ipqp.ipqp_rho_init <= opts.ipqp.ipqp_reg_max)) {
        throw std::invalid_argument(fmt::format(
            "SqpDriver: ipqp.ipqp_rho_init ({}) must lie in [ipqp_reg_floor ({}), ipqp_reg_max "
            "({})]; the starting value of a quantity the ladder only ever moves within that "
            "band must itself start inside it",
            opts.ipqp.ipqp_rho_init, opts.ipqp.ipqp_reg_floor, opts.ipqp.ipqp_reg_max));
    }
    if (!(opts.ipqp.ipqp_delta_init > 0.0) ||
        !(opts.ipqp.ipqp_delta_init <= opts.ipqp.ipqp_reg_max)) {
        throw std::invalid_argument(fmt::format(
            "SqpDriver: ipqp.ipqp_delta_init ({}) must lie in (0, ipqp_reg_max ({})]; unlike "
            "ipqp_rho_init it is NOT tied to ipqp_reg_floor -- spec section 2.2's monotone "
            "floor is stated for rho only",
            opts.ipqp.ipqp_delta_init, opts.ipqp.ipqp_reg_max));
    }
    if (!(opts.ipqp.ipqp_reg_decrease > 0.0) || !(opts.ipqp.ipqp_reg_decrease < 1.0)) {
        throw std::invalid_argument(fmt::format(
            "SqpDriver: ipqp.ipqp_reg_decrease ({}) must lie strictly in (0, 1); it is a "
            "gated multiplicative decrease -- 0 would collapse the regularization in one "
            "gated step and >= 1 would never decrease it",
            opts.ipqp.ipqp_reg_decrease));
    }
    if (!(opts.ipqp.ipqp_tau > 0.0) || !(opts.ipqp.ipqp_tau < 1.0)) {
        throw std::invalid_argument(fmt::format(
            "SqpDriver: ipqp.ipqp_tau ({}) must lie strictly in (0, 1); it is the "
            "fraction-to-boundary parameter -- 0 permits no step and >= 1 permits stepping "
            "onto or past a bound",
            opts.ipqp.ipqp_tau));
    }
    if (!(opts.ipqp.ipqp_face_kappa > 0.0) || !(opts.ipqp.ipqp_face_kappa < 1.0)) {
        throw std::invalid_argument(fmt::format(
            "SqpDriver: ipqp.ipqp_face_kappa ({}) must lie strictly in (0, 1); a non-positive "
            "ratio-rule threshold can never classify a row active, and at kappa >= 1 the active "
            "test (s_j < kappa*z_j) and the inactive test (z_j < kappa*s_j) can no longer "
            "partition: at kappa == 1 an exact tie (s_j == z_j) satisfies NEITHER strict "
            "inequality (both tests false, so every tie becomes UNCERTAIN with no genuine "
            "margin), and above 1 the two tests can both fire on the same pair (a contradictory "
            "active-AND-inactive verdict)",
            opts.ipqp.ipqp_face_kappa));
    }
    if (!(opts.ipqp.ipqp_converge_slack >= 1.0) || std::isinf(opts.ipqp.ipqp_converge_slack)) {
        throw std::invalid_argument(fmt::format(
            "SqpDriver: ipqp.ipqp_converge_slack ({}) must be finite and >= 1; a slack below "
            "1 would ask the barrier phase for more accuracy than tier 3's own finish, "
            "inverting the two-tier division of labor",
            opts.ipqp.ipqp_converge_slack));
    }
    if (opts.ipqp.ipqp_warm_iter_budget < 0) {
        throw std::invalid_argument(fmt::format(
            "SqpDriver: ipqp.ipqp_warm_iter_budget ({}) must be >= 0; 0 is legal (every warm "
            "restart is killed on its first iteration) but a negative budget is not a count",
            opts.ipqp.ipqp_warm_iter_budget));
    }
    if (!(opts.ipqp.ipqp_mu_adopt_factor >= 0.0) || std::isinf(opts.ipqp.ipqp_mu_adopt_factor)) {
        throw std::invalid_argument(fmt::format(
            "SqpDriver: ipqp.ipqp_mu_adopt_factor ({}) must be finite and >= 0; 0 is a legal, "
            "deliberate reading (adoption disabled), a negative one is not",
            opts.ipqp.ipqp_mu_adopt_factor));
    }
    if (opts.ipqp.ipqp_stall_window <= 0) {
        throw std::invalid_argument(fmt::format(
            "SqpDriver: ipqp.ipqp_stall_window ({}) must be > 0; a zero-width window can "
            "never accumulate the whole-window evidence the early-stall test is built on",
            opts.ipqp.ipqp_stall_window));
    }
    if (opts.ipqp.ipqp_retire_after <= 0) {
        throw std::invalid_argument(
            fmt::format("SqpDriver: ipqp.ipqp_retire_after ({}) must be > 0; retiring \"after zero "
                        "consecutive escapes\" is not a count, it disables the tier outright",
                        opts.ipqp.ipqp_retire_after));
    }
    // THE ENUMERATOR-COUNT SENTINEL IS NOT A MODE (fix round 3). It is a
    // legal `QpMode` value naming no kernel, so it is refused here rather than
    // left to reach the dispatch, where the arm that enumerates it throws --
    // a caller who sets it gets the diagnosis at construction, which is where
    // every other out-of-range setting gets it.
    if (opts.qp_mode == QpMode::kQpModeCount) {
        throw std::invalid_argument(
            "SqpDriver: qp_mode == QpMode::kQpModeCount is the enumerator-count sentinel, not a "
            "kernel -- it exists so a fourth QpMode cannot be added without an arm in the QP "
            "kernel dispatch. Use kWalk, kSsn or kIpm");
    }
    // TASK 1's TEMPORARY kIpm REFUSAL WAS HERE, AND IS GONE (M6 W1 task 6).
    // The routing chain it was waiting on now dispatches the mode
    // (sqp_driver.cpp's THE QP KERNEL DISPATCH), so the mode is reachable and
    // nothing here refuses it. The IpqpOptions predicates above are validated
    // UNCONDITIONALLY, at every mode, exactly as they were: a field is
    // out of range whether or not this solve will read it.

    // The two CommonOptions fields (M6 W5 T8.3). Neither is read by this engine
    // yet -- T8.7 reads print_level, T8.8 reads threads -- but both are part of
    // the value a caller hands over and both are checked here, so an
    // out-of-range knob is refused at the boundary rather than at the task that
    // starts reading it. Integers, so no NaN idiom applies.
    if (opts.common.threads < 0) {
        throw std::invalid_argument(
            fmt::format("SqpDriver: common.threads ({}) must be >= 0 (0 = leave the backend's own "
                        "default alone)",
                        opts.common.threads));
    }
    if (opts.common.print_level < 0) {
        throw std::invalid_argument(
            fmt::format("SqpDriver: common.print_level ({}) must be >= 0 (0 = full output, "
                        "3 and above = silent)",
                        opts.common.print_level));
    }
}

// The pre-T8.3 name, kept as a forwarder so the ~60 call sites that spell it
// this way stay byte-identical. T8.10 removes it.
void validate_sqp_options(const SqpOptions &opts) { validate(opts); }

// The SQP engine's named presets. "default" is the only one until M8's labeled
// configs; it returns a default-constructed value, which is what every caller
// that writes `SqpOptions{}` already gets. The refusal lists the valid names for
// the same reason ipm_preset()'s does.
SqpOptions sqp_preset(std::string_view name) {
    if (name == "default") {
        return SqpOptions{};
    }
    throw std::invalid_argument(
        fmt::format("Unrecognized SqpDriver preset '{}'. Valid options are: default", name));
}

} // namespace hven::solvers
