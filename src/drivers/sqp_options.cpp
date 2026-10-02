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
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include <hven/drivers/sqp_solver_types.h>

namespace hven::solvers {

void validate(const SqpOptions &opts) {
    if (!(opts.kkt_tol > 0.0) || !(opts.feas_tol > 0.0)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: kkt_tol ({}) and feas_tol ({}) must both be > 0", opts.kkt_tol,
                        opts.feas_tol));
    }
    if (opts.max_iter < 0) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: max_iter ({}) must be >= 0", opts.max_iter));
    }
    if (!(opts.tr_init > 0.0)) { // catches NaN, negatives and exactly 0
        throw std::invalid_argument(fmt::format(
            "SqpSolver: tr_init ({}) must be > 0 (use +inf for no trust region; 0 would "
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
    // FIRST rejection lands the radius on tr_max (see sqp_solver.h's RADIUS
    // MANAGEMENT), so the pair is meaningful after all -- what +inf now means
    // is "unbounded until the method finds it needs a bound, then tr_max".
    // The growth rule is still skipped while the radius is +inf
    // (min(+inf*2, tr_max) would REDUCE it, which no growth rule may do).
    if (!std::isinf(opts.tr_init) && !(opts.tr_max >= opts.tr_init)) { // catches NaN too
        throw std::invalid_argument(
            fmt::format("SqpSolver: tr_max ({}) must be >= tr_init ({}); it is the ceiling "
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
            fmt::format("SqpSolver: tr_min ({}) must be > 0; it is the radius floor whose crossing "
                        "raises a restoration request, and 0 can never be crossed by halving",
                        opts.tr_min));
    }
    if (!(opts.tr_min <= opts.tr_init) || !(opts.tr_min <= opts.tr_max)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: tr_min ({}) must be <= both tr_init ({}) and tr_max ({}); "
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
            "SqpSolver: scaling_max_gradient ({}) must be finite and > 0; it is the inf-norm "
            "the scaled objective gradient and each scaled Jacobian row are aimed at",
            opts.scaling_max_gradient));
    }
    if (!(opts.scaling_factor_limit >= 1.0) || std::isinf(opts.scaling_factor_limit)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: scaling_factor_limit ({}) must be finite and >= 1; it clamps every "
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
            "SqpSolver: ipqp.ipqp_hard_iter_cap ({}) must be > 0; it is the budget of last "
            "resort and, unlike ipqp_max_iter, carries no size-derived sentinel reading",
            opts.ipqp.ipqp_hard_iter_cap));
    }
    if (!(opts.ipqp.ipqp_min_mu > 0.0) || std::isinf(opts.ipqp.ipqp_min_mu)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: ipqp.ipqp_min_mu ({}) must be finite and > 0; it is the "
                        "floor of the mu_0 clamp and the tier's own barrier-decay floor",
                        opts.ipqp.ipqp_min_mu));
    }
    if (!(opts.ipqp.ipqp_init_mu > 0.0) || std::isinf(opts.ipqp.ipqp_init_mu)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: ipqp.ipqp_init_mu ({}) must be finite and > 0; it is the cold starting "
            "mu and the ceiling of the mu_0 clamp",
            opts.ipqp.ipqp_init_mu));
    }
    if (!(opts.ipqp.ipqp_min_mu <= opts.ipqp.ipqp_init_mu)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: ipqp.ipqp_min_mu ({}) must be <= ipqp.ipqp_init_mu ({}); the mu_0 "
            "clamp's band [ipqp_min_mu, ipqp_init_mu] is otherwise inverted",
            opts.ipqp.ipqp_min_mu, opts.ipqp.ipqp_init_mu));
    }
    if (!(opts.ipqp.ipqp_reg_floor > 0.0) || std::isinf(opts.ipqp.ipqp_reg_floor)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: ipqp.ipqp_reg_floor ({}) must be finite and > 0; it is the absolute "
            "floor the (rho, delta) ladder's gated decrease may never cross",
            opts.ipqp.ipqp_reg_floor));
    }
    if (!(opts.ipqp.ipqp_reg_max >= opts.ipqp.ipqp_reg_floor) ||
        std::isinf(opts.ipqp.ipqp_reg_max)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: ipqp.ipqp_reg_max ({}) must be finite and >= ipqp.ipqp_reg_floor ({}); "
            "it is the ceiling the (rho, delta) ladder's growth may never cross",
            opts.ipqp.ipqp_reg_max, opts.ipqp.ipqp_reg_floor));
    }
    if (!(opts.ipqp.ipqp_rho_init >= opts.ipqp.ipqp_reg_floor) ||
        !(opts.ipqp.ipqp_rho_init <= opts.ipqp.ipqp_reg_max)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: ipqp.ipqp_rho_init ({}) must lie in [ipqp_reg_floor ({}), ipqp_reg_max "
            "({})]; the starting value of a quantity the ladder only ever moves within that "
            "band must itself start inside it",
            opts.ipqp.ipqp_rho_init, opts.ipqp.ipqp_reg_floor, opts.ipqp.ipqp_reg_max));
    }
    if (!(opts.ipqp.ipqp_delta_init > 0.0) ||
        !(opts.ipqp.ipqp_delta_init <= opts.ipqp.ipqp_reg_max)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: ipqp.ipqp_delta_init ({}) must lie in (0, ipqp_reg_max ({})]; unlike "
            "ipqp_rho_init it is NOT tied to ipqp_reg_floor -- spec section 2.2's monotone "
            "floor is stated for rho only",
            opts.ipqp.ipqp_delta_init, opts.ipqp.ipqp_reg_max));
    }
    if (!(opts.ipqp.ipqp_reg_decrease > 0.0) || !(opts.ipqp.ipqp_reg_decrease < 1.0)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: ipqp.ipqp_reg_decrease ({}) must lie strictly in (0, 1); it is a "
            "gated multiplicative decrease -- 0 would collapse the regularization in one "
            "gated step and >= 1 would never decrease it",
            opts.ipqp.ipqp_reg_decrease));
    }
    if (!(opts.ipqp.ipqp_tau > 0.0) || !(opts.ipqp.ipqp_tau < 1.0)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: ipqp.ipqp_tau ({}) must lie strictly in (0, 1); it is the "
            "fraction-to-boundary parameter -- 0 permits no step and >= 1 permits stepping "
            "onto or past a bound",
            opts.ipqp.ipqp_tau));
    }
    if (!(opts.ipqp.ipqp_face_kappa > 0.0) || !(opts.ipqp.ipqp_face_kappa < 1.0)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: ipqp.ipqp_face_kappa ({}) must lie strictly in (0, 1); a non-positive "
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
            "SqpSolver: ipqp.ipqp_converge_slack ({}) must be finite and >= 1; a slack below "
            "1 would ask the barrier phase for more accuracy than tier 3's own finish, "
            "inverting the two-tier division of labor",
            opts.ipqp.ipqp_converge_slack));
    }
    if (opts.ipqp.ipqp_warm_iter_budget < 0) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: ipqp.ipqp_warm_iter_budget ({}) must be >= 0; 0 is legal (every warm "
            "restart is killed on its first iteration) but a negative budget is not a count",
            opts.ipqp.ipqp_warm_iter_budget));
    }
    if (!(opts.ipqp.ipqp_mu_adopt_factor >= 0.0) || std::isinf(opts.ipqp.ipqp_mu_adopt_factor)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: ipqp.ipqp_mu_adopt_factor ({}) must be finite and >= 0; 0 is a legal, "
            "deliberate reading (adoption disabled), a negative one is not",
            opts.ipqp.ipqp_mu_adopt_factor));
    }
    if (opts.ipqp.ipqp_stall_window <= 0) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: ipqp.ipqp_stall_window ({}) must be > 0; a zero-width window can "
            "never accumulate the whole-window evidence the early-stall test is built on",
            opts.ipqp.ipqp_stall_window));
    }
    if (opts.ipqp.ipqp_retire_after <= 0) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: ipqp.ipqp_retire_after ({}) must be > 0; retiring \"after zero "
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
            "SqpSolver: qp_mode == QpMode::kQpModeCount is the enumerator-count sentinel, not a "
            "kernel -- it exists so a fourth QpMode cannot be added without an arm in the QP "
            "kernel dispatch. Use kWalk, kSsn or kIpm");
    }
    // THE ELASTIC RUNG'S ENGINE (M7 W0 T3), on the same terms and for the same
    // reason: the count sentinel is a legal `ElasticRungEngine` value naming no
    // engine, and the ladder's dispatch enumerates the two that do.
    if (opts.elastic_rung_engine == ElasticRungEngine::kElasticRungEngineCount) {
        throw std::invalid_argument(
            "SqpSolver: elastic_rung_engine == ElasticRungEngine::kElasticRungEngineCount is the "
            "enumerator-count sentinel, not an engine -- it exists so a third engine cannot be "
            "added without an arm in the elastic ladder's rung dispatch. Use kWalk or kTier");
    }
    // TASK 1's TEMPORARY kIpm REFUSAL WAS HERE, AND IS GONE (M6 W1 task 6).
    // The routing chain it was waiting on now dispatches the mode
    // (sqp_solver.cpp's THE QP KERNEL DISPATCH), so the mode is reachable and
    // nothing here refuses it. The IpqpOptions predicates above are validated
    // UNCONDITIONALLY, at every mode, exactly as they were: a field is
    // out of range whether or not this solve will read it.

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
    if (!(opts.qp_failure_budget >= 1)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: qp_failure_budget ({}) must be >= 1; it is the number of CONSECUTIVE "
            "subproblem failures the driver may route at one iterate, and 1 is the shipped "
            "behaviour (route the first, end the solve on the second). 0 would refuse the "
            "first retry, which is a behaviour change spelled as a budget",
            opts.qp_failure_budget));
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
    if (!(opts.restoration_proximity >= 0.0) || !std::isfinite(opts.restoration_proximity)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: restoration_proximity ({}) must be a finite value >= 0; it is the "
            "coefficient zeta on the restoration feasibility wrapper's 1/2 zeta ||D (x - "
            "x_k)||^2 proximity term, 0 is OFF (the shipped value, the wrapper that has "
            "always shipped), and an infinite weight makes that objective +inf everywhere "
            "but at the requesting iterate",
            opts.restoration_proximity));
    }

    // The two CommonOptions fields (M6 W5 T8.3). Neither is read by this engine
    // yet -- T8.7 reads print_level, T8.8 reads threads -- but both are part of
    // the value a caller hands over and both are checked here, so an
    // out-of-range knob is refused at the boundary rather than at the task that
    // starts reading it. Integers, so no NaN idiom applies.
    if (opts.common.threads < 0) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: common.threads ({}) must be >= 0 (0 = leave the backend's own "
                        "default alone)",
                        opts.common.threads));
    }
    if (opts.common.print_level < 0) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: common.print_level ({}) must be >= 0 (0 = full output, "
                        "3 and above = silent)",
                        opts.common.print_level));
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

// =============================================================================
// M7 W0 T5(b): the options dump.
//
// A hand-written printer, one line per field, pinned against SqpOptions (and
// the three nested structs) by tests/drivers/test_options_dump.cpp. The
// contract -- order, prefixes, enumerator names, {:.17g} doubles -- is
// documented once at dump_options(const IpmOptions &, std::ostream &) in
// drivers/ipm_solver_types.h and is not restated here.
//
// NOTHING BELOW WEAKENS THIS FILE'S FP PREMISE. The dump performs no
// floating-point operation either: every double it touches is handed to
// fmt::format for rendering, exactly as the rejection messages above do.
//
// The kv_* helpers and the enum_name() overloads are FILE-LOCAL and are
// deliberately duplicated from src/drivers/ipm_options.cpp; the reasoning is
// recorded there.
// =============================================================================

namespace {

// CANONICAL BYTES, NOT THE CALLER'S FORMATTING (M7 W0 T5 fix round 1, sol
// Important 1): every line is rendered whole by fmt and handed to the stream by
// `write()`, an UNFORMATTED output function, so the caller's flags, precision,
// width, fill and imbued locale reach no byte of the output. The reasoning is
// recorded in full in src/drivers/ipm_options.cpp.
void emit(std::ostream &os, const std::string &line) {
    os.write(line.data(), static_cast<std::streamsize>(line.size()));
}

void kv_int(std::ostream &os, const char *name, long long v) {
    emit(os, fmt::format("{} = {}\n", name, v));
}
void kv_bool(std::ostream &os, const char *name, bool v) {
    emit(os, fmt::format("{} = {}\n", name, v ? "true" : "false"));
}
void kv_dbl(std::ostream &os, const char *name, double v) {
    emit(os, fmt::format("{} = {:.17g}\n", name, v));
}
void kv_str(std::ostream &os, const char *name, const char *v) {
    emit(os, fmt::format("{} = {}\n", name, v));
}

// ENUMERATOR NAMES, not display strings -- see src/drivers/ipm_options.cpp.
const char *enum_name(QpMode v) {
    switch (v) {
    case QpMode::kWalk:
        return "kWalk";
    case QpMode::kSsn:
        return "kSsn";
    case QpMode::kIpm:
        return "kIpm";
    case QpMode::kQpModeCount:
        // NOT a mode; validate() refuses it. It is nonetheless a legal QpMode
        // value, so the dump names it rather than reporting "unknown".
        return "kQpModeCount";
    }
    return "unknown";
}
const char *enum_name(ElasticRungEngine v) {
    switch (v) {
    case ElasticRungEngine::kWalk:
        return "kWalk";
    case ElasticRungEngine::kTier:
        return "kTier";
    case ElasticRungEngine::kElasticRungEngineCount:
        // NOT an engine; validate() refuses it. Named rather than "unknown",
        // exactly as kQpModeCount is.
        return "kElasticRungEngineCount";
    }
    return "unknown";
}
const char *enum_name(SsnSigmaRule v) {
    switch (v) {
    case SsnSigmaRule::kLadder:
        return "kLadder";
    case SsnSigmaRule::kResidualArmed:
        return "kResidualArmed";
    case SsnSigmaRule::kResidualAlways:
        return "kResidualAlways";
    }
    return "unknown";
}
const char *enum_name(SsnHintRule v) {
    switch (v) {
    case SsnHintRule::kIterationZeroFree:
        return "kIterationZeroFree";
    case SsnHintRule::kWatchdog:
        return "kWatchdog";
    }
    return "unknown";
}
const char *enum_name(SsnInfeasibilityRule v) {
    switch (v) {
    case SsnInfeasibilityRule::kSymptoms:
        return "kSymptoms";
    case SsnInfeasibilityRule::kFarkasGated:
        return "kFarkasGated";
    }
    return "unknown";
}
const char *enum_name(WorkingSetLinearAlgebra v) {
    switch (v) {
    case WorkingSetLinearAlgebra::kRefactorize:
        return "kRefactorize";
    case WorkingSetLinearAlgebra::kSchurBorder:
        return "kSchurBorder";
    }
    return "unknown";
}
const char *enum_name(StartLevel v) {
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

} // namespace

void dump_options(const SqpOptions &o, std::ostream &os) {
    kv_dbl(os, "kkt_tol", o.kkt_tol);
    kv_dbl(os, "feas_tol", o.feas_tol);
    kv_int(os, "max_iter", static_cast<long long>(o.max_iter));
    kv_dbl(os, "tr_init", o.tr_init);
    kv_dbl(os, "tr_max", o.tr_max);
    kv_dbl(os, "tr_min", o.tr_min);
    kv_bool(os, "enable_soc", o.enable_soc);
    kv_bool(os, "adaptive_mu", o.adaptive_mu);
    kv_dbl(os, "qp.primal_delta", o.qp.primal_delta);
    kv_dbl(os, "qp.dual_mu", o.qp.dual_mu);
    kv_dbl(os, "qp.feas_tol", o.qp.feas_tol);
    kv_dbl(os, "qp.opt_tol", o.qp.opt_tol);
    kv_int(os, "qp.max_iter", static_cast<long long>(o.qp.max_iter));
    kv_int(os, "qp.schur_cap", static_cast<long long>(o.qp.schur_cap));
    kv_dbl(os, "qp.schur_cond_max", o.qp.schur_cond_max);
    kv_str(os, "qp.ws_algebra", enum_name(o.qp.ws_algebra));
    kv_dbl(os, "qp.tr_radius", o.qp.tr_radius);
    kv_bool(os, "qp.independent_working_set", o.qp.independent_working_set);
    // A CALLABLE, not a configuration: see the declaration's note.
    kv_str(os, "make_strategy", o.make_strategy ? "set" : "unset");
    kv_bool(os, "warm_full_step", o.warm_full_step);
    kv_bool(os, "budget_mode", o.budget_mode);
    kv_bool(os, "elastic_ladder_early_exit", o.elastic_ladder_early_exit);
    kv_bool(os, "crash_basis", o.crash_basis);
    kv_str(os, "qp_mode", enum_name(o.qp_mode));
    kv_str(os, "elastic_rung_engine", enum_name(o.elastic_rung_engine));
    kv_bool(os, "ipqp_reseed_after_disproval", o.ipqp_reseed_after_disproval);
    kv_int(os, "ipqp.ipqp_max_iter", static_cast<long long>(o.ipqp.ipqp_max_iter));
    kv_int(os, "ipqp.ipqp_hard_iter_cap", static_cast<long long>(o.ipqp.ipqp_hard_iter_cap));
    kv_int(os, "ipqp.ipqp_max_factorizations",
           static_cast<long long>(o.ipqp.ipqp_max_factorizations));
    kv_dbl(os, "ipqp.ipqp_init_mu", o.ipqp.ipqp_init_mu);
    kv_dbl(os, "ipqp.ipqp_min_mu", o.ipqp.ipqp_min_mu);
    kv_dbl(os, "ipqp.ipqp_rho_init", o.ipqp.ipqp_rho_init);
    kv_dbl(os, "ipqp.ipqp_delta_init", o.ipqp.ipqp_delta_init);
    kv_dbl(os, "ipqp.ipqp_reg_floor", o.ipqp.ipqp_reg_floor);
    kv_dbl(os, "ipqp.ipqp_reg_max", o.ipqp.ipqp_reg_max);
    kv_dbl(os, "ipqp.ipqp_reg_decrease", o.ipqp.ipqp_reg_decrease);
    kv_dbl(os, "ipqp.ipqp_tau", o.ipqp.ipqp_tau);
    kv_dbl(os, "ipqp.ipqp_face_kappa", o.ipqp.ipqp_face_kappa);
    kv_dbl(os, "ipqp.ipqp_converge_slack", o.ipqp.ipqp_converge_slack);
    kv_bool(os, "ipqp.ipqp_ruiz", o.ipqp.ipqp_ruiz);
    kv_bool(os, "ipqp.ipqp_warm_repair", o.ipqp.ipqp_warm_repair);
    kv_int(os, "ipqp.ipqp_warm_iter_budget", static_cast<long long>(o.ipqp.ipqp_warm_iter_budget));
    kv_dbl(os, "ipqp.ipqp_mu_adopt_factor", o.ipqp.ipqp_mu_adopt_factor);
    kv_int(os, "ipqp.ipqp_stall_window", static_cast<long long>(o.ipqp.ipqp_stall_window));
    kv_int(os, "ipqp.ipqp_retire_after", static_cast<long long>(o.ipqp.ipqp_retire_after));
    kv_bool(os, "ipqp.ipqp_farkas_gate", o.ipqp.ipqp_farkas_gate);
    kv_bool(os, "ipqp.ipqp_hoist_symbolic", o.ipqp.ipqp_hoist_symbolic);
    kv_bool(os, "ipqp.ipqp_require_final_inertia", o.ipqp.ipqp_require_final_inertia);
    kv_bool(os, "ssn_prox_carry", o.ssn_prox_carry);
    kv_bool(os, "ssn_certify_from_face", o.ssn_certify_from_face);
    kv_str(os, "ssn_sigma_rule", enum_name(o.ssn_sigma_rule));
    kv_str(os, "ssn_hint_rule", enum_name(o.ssn_hint_rule));
    kv_str(os, "ssn_infeasibility_rule", enum_name(o.ssn_infeasibility_rule));
    kv_bool(os, "enable_scaling", o.enable_scaling);
    kv_dbl(os, "scaling_max_gradient", o.scaling_max_gradient);
    kv_dbl(os, "scaling_factor_limit", o.scaling_factor_limit);
    kv_bool(os, "project_start_point", o.project_start_point);
    kv_int(os, "qp_failure_budget", static_cast<long long>(o.qp_failure_budget));
    kv_bool(os, "qp_cap_retry", o.qp_cap_retry);
    kv_bool(os, "qp_failure_step", o.qp_failure_step);
    kv_dbl(os, "restoration_proximity", o.restoration_proximity);
    kv_bool(os, "equality_basis", o.equality_basis);
    kv_int(os, "common.threads", o.common.threads);
    kv_int(os, "common.print_level", o.common.print_level);
    kv_str(os, "common.start_level", enum_name(o.common.start_level));
}

} // namespace hven::solvers
