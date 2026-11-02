// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// sqp_solver_types.h — the plain data types the SQP driver produces and consumes:
// status, options, counters, per-major history, solution. The driver itself
// (the loop, the subproblem construction and the KKT measure) lives in
// sqp_solver.h; nothing in this file does any work.
//
// This file is to sqp_solver.h what qp_types.h is to qp_engine.h.

#include <functional>
#include <iosfwd>
#include <limits>
#include <memory>
#include <string_view>
#include <vector>

#include <hven/core/solver_counters.h>
#include <hven/core/solver_status.h>
#include <hven/detail/warmstart/warm_start.h>
#include <hven/drivers/common_options.h>
#include <hven/drivers/solve_result.h>
#include <hven/drivers/sqp_options.h>
#include <hven/drivers/step_verdict.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

// The full-step-first warm rule's two constants. Both are watchdog thresholds:
// they bound how long the driver may keep taking undamped steps under
// globalization.h's full-step mode before restoring the best iterate it saw and
// handing the solve back to the funnel.
//
// kWarmResidualGrowthMax = 2 consecutive majors with a growing max(stationarity, feasibility,
// complementarity); kWarmFullStepWindow = 5 majors without a new best max(stationarity,
// feasibility, complementarity).
//
// Changing either is a behaviour change on every warm solve: the warm-start
// suite pins the majors of both a converging and a watchdog-restored run
// against these exact values.
// @see docs/notes/2026-09-header-prose-archive.md §sqp_solver_types.h

/// Watchdog threshold: consecutive majors with growing max(stationarity,
/// feasibility, complementarity) tolerated
/// under the full-step mode before it restores the best iterate.
inline constexpr Index kWarmResidualGrowthMax = 2;

/// Watchdog threshold: majors without a new best max(stationarity, feasibility,
/// complementarity) tolerated under
/// the full-step mode before it restores the best iterate.
inline constexpr Index kWarmFullStepWindow = 5;

/// @brief Relative margin below which an active row's multiplier -- or an
///        inactive row's slack -- counts as weak/near activity in the per-major
///        census.
///
/// A CONSTANT, NOT A SETTING: nothing in this library reads or branches on either
/// count, so there is no behaviour for a caller to tune.
///
/// It measures strict complementarity BY MAGNITUDE: a row is weakly active when
/// it is in the working set at a price negligible beside the largest one, and
/// nearly active when it is out of the working set at a slack negligible beside
/// the largest one.
///
/// It is NOT the SSN's uncertain set: that set is the dimensionless kink band
/// with leave hysteresis (ssn_engine.h), which reads a PURE state at `s == 0` for
/// any lambda and so never flags a small multiplier on an active row.
inline constexpr double kWeakActivityMargin = 1e-6;

/// @brief One row of the per-major history -- the record of ONE ITERATE and of
///        the subproblem solved from it, if any.
///
/// This field set is the FORMAL CONTRACT and is additive-only: a field may be
/// appended, but none below may change meaning.
///
/// qp_solved == false marks an iterate the solve stood on and left (or stopped AT)
/// without a subproblem solved from it. Its qp_* fields are meaningless and left at
/// their defaults. Such a row is either the last row (the solve stopped AT it; a
/// solve that stopped ON a failing subproblem has none) or a ROLLBACK row
/// (`rolled_back`, below): an iterate the solve stood on and left without a
/// subproblem, because its committed point's derivatives were not finite and the
/// step that reached it was rejected after the fact (M7 W2 R2-1). Rollback rows may
/// appear MID-HISTORY and SEVERAL PER SOLVE (SqpCounters::nonfinite_step_rollbacks
/// counts them); the stopped-AT row is still at most one and last.
///
/// NaN IS A LEGAL VALUE for the four residual fields, on the non-finite-iterate
/// row and on a rollback row whose abandoned point had a non-finite gradient, so a
/// consumer aggregating them must expect it -- mid-history as well as last.
///
/// ONE ROW IS ONE TRIAL, Not one iterate -- which is why the index field is named
/// `trial`. A rejected step leaves the iterate where it was and the driver
/// re-solves the SAME subproblem at a smaller radius, so consecutive rows can
/// describe the same point, differing only in tr_radius, the qp_* fields,
/// step_norm and verdict.
///
/// Recovering the iterate sequence. Row 0 is always an iterate the solve stood
/// on; after that, row k is a NEW point iff row k-1's step was accepted:
///
///     bool is_new_point = k == 0 ||
///                         history[k-1].verdict == StepVerdict::kAcceptF ||
///                         history[k-1].verdict == StepVerdict::kAcceptH;
///
/// The predicate reads the PREVIOUS row, not this one: filtering on a row's own
/// kReject verdict is wrong in both directions.
///
/// Every scale-carrying column IS ON THE CALLER'S SCALE, whether or not the solve
/// ran with `SqpOptions::enable_scaling` on. The number a scaled solve's
/// convergence test gated on is `SqpScalingReport::scaled_kkt_residual`.
struct SqpIterate {
    /// @brief 0-based index of THIS ROW (== subproblem index).
    Index trial = 0;

    /// @brief Objective at the iterate.
    double f = 0.0;
    /// @brief Reduced/projected ||grad L||inf; see sqp_solver.h's CONVERGENCE TEST.
    double stationarity = 0.0;
    /// @brief max(||cE||inf, max(cI)+, bound violation).
    double feasibility = 0.0;
    /// max_j |lambda_i(j) * cI_j(x)|. GATED at kkt_tol by the convergence test
    /// on every major (WX-R1), beside stationarity and feasibility.
    double complementarity = 0.0;
    /// max(stationarity, feasibility) -- the scalar the contraction test
    /// reads. Complementarity is not in it; the convergence test reads it
    /// separately.
    double kkt_residual = 0.0;
    /// h(x) = ||cE(x)||_1 + sum_j max(0, cI_j(x)) at the iterate: the
    /// GLOBALIZATION measure (sqp_solver.h's constraint_violation_l1), a
    /// different quantity from `feasibility` above -- l1 vs inf-norm, and
    /// bounds excluded vs included. Recorded because it is what the funnel
    /// judges against, so the funnel's own guarantee (h stays inside a
    /// monotonically tightening width) is checkable from the history alone.
    double violation_l1 = 0.0;
    /// The l-infinity trust-region radius THIS subproblem was solved at,
    /// i.e. the SolveOverrides::tr_radius the driver passed. On a
    /// stopped-AT-iterate row it is the radius the next subproblem would
    /// have been given.
    double tr_radius = 0.0;
    /// The SolveOverrides::dual_mu THIS subproblem was solved at (the MAIN
    /// trial's QP -- a subsequent SOC/elastic re-solve on the same row uses
    /// its own separate override, per sqp_solver.h's ADAPTIVE DUAL
    /// REGULARIZATION note, so it is not what this field describes there).
    /// Meaningful iff qp_solved; 0.0 on a stopped-AT-iterate row, like the
    /// other qp_* fields. Always opts.qp.dual_mu (the engine default) when
    /// SqpOptions::adaptive_mu is false -- byte-identical across the whole
    /// history.
    double mu = 0.0;
    /// ||p||inf of the step taken FROM this iterate. ON A ROUTED QP-FAILURE ROW
    /// no step was taken: the field records the |p|inf of the iterate the failed
    /// solve returned, which the driver discarded -- diagnostic there, and it must
    /// not be summed into a path length.
    ///
    /// ON A SOC-corrected row this is still ||p||inf of the ORIGINAL, rejected QP
    /// step, not of the SOC re-solve's own step, which is the one the iterate
    /// actually moved by. That keeps this field, and every reader of it, describing
    /// exactly one QpSolution's own box-respecting step.
    double step_norm = 0.0;
    /// @brief False on a stopped-AT-iterate row (see above).
    bool qp_solved = false;

    // --- The interior-point tier's infeasibility evidence, where it arrives ---
    //
    // Two scalars off `IpqpInfeasibilityEvidence`, written by the escape branch's
    // hook (`certified_feasibility_fallback`) on the major it fires on, and
    // zero/false on every other row -- including every row of a kWalk or kSsn
    // solve, where no tier runs.
    //
    // They make the arrival OBSERVABLE: they are the evidence block's own headline
    // scalars, and a call site that substituted default evidence would report 0
    // and false here.
    //
    // NOT A CERTIFICATE: the escape signature is a SUSPICION,
    // `farkas_corroborated == false` means "not corroborated" rather than
    // "withdrawn", and the escape fires either way. Read them as telemetry about
    // why a subproblem was handed on.
    double ipqp_least_infeasible_primal = 0.0;
    bool ipqp_farkas_corroborated = false;
    /// @brief Meaningful iff qp_solved.
    QpStatus qp_status = QpStatus::kOptimal;
    /// @brief Meaningful iff qp_solved.
    Index qp_minor_iters = 0;
    /// @brief Meaningful iff qp_solved.
    Index qp_factorizations = 0;
    /// @brief Any QpSolution::tr_active entry set (radius bit).
    bool tr_binding = false;
    /// The globalization strategy's verdict on this trial. It is the strategy's
    /// OWN verdict iff `qp_solved && qp_status == QpStatus::kOptimal`; on any
    /// other row no strategy was consulted.
    ///
    /// The default is deliberately kReject, and on a failed-subproblem row it is
    /// more than a safe default -- the row really was a rejection. So a consumer
    /// reconstructing the ITERATE sequence may read `verdict != kReject` on the
    /// PREVIOUS row as "the iterate moved" across every row shape.
    StepVerdict verdict = StepVerdict::kReject;
    /// True iff this row's ACCEPTANCE (verdict is kAcceptF/kAcceptH) was
    /// won by a second-order correction, i.e. the strategy rejected the raw
    /// QP step p and a hot-started re-solve rescued it -- the iterate that
    /// follows this row is x + (the SOC re-solve's own step), NOT x + p.
    /// False on every other row, including one where SOC was ATTEMPTED but
    /// its own corrected point was also rejected (that attempt is counted
    /// in SqpCounters::soc_steps, not recorded per-row -- see
    /// sqp_solver.h's SECOND-ORDER CORRECTION note for why a failed attempt
    /// does not get a field of its own). Always false when
    /// SqpOptions::enable_soc is false.
    bool soc_applied = false;
    /// True iff an ELASTIC SOLVE supplied this major: the elastic tier's re-solve
    /// after a plain QP returned kInfeasible, or the certified fallback's rung A
    /// after a kIpm escape. Set on BOTH outcomes -- the row whose step came from
    /// the elastic solve, and the row that ended the solve because the ladder was
    /// exhausted. It is the branch input SOC is gated on.
    ///
    /// ON SUCH A ROW THE qp_* Fields describe the final elastic solve, not the
    /// kInfeasible one that triggered it, with the escalation re-solves' costs
    /// folded into SqpCounters' aggregates only. On a rung-A-owned major they are
    /// the ladder's own counts.
    ///
    /// tr_binding IS STILL MEANINGFUL: the elastic solve gets its trust region as
    /// REAL bounds on the original variables rather than through SolveOverrides,
    /// and the driver re-derives the radius bit exactly as the engine would have.
    bool elastic_applied = false;
    /// True iff THIS row's elastic ladder started at a CLAMPED placement -- the
    /// evidence priced the violation above the escalation headroom or above the
    /// dual_mu safety margin -- including a clamp whose declined ladder was then
    /// retried at the floor. False on every other row. Per-ENTRY, where
    /// `elastic_rho0_ceiling_hits` is per-ladder; they agree only because the
    /// retry's override placement can never clamp.
    bool elastic_rho0_ceiling_hit = false;
    /// True iff THIS row's restoration request started the phase at a CANDIDATE point rather
    /// than at the iterate -- the candidate's MEASURED violation was lower (sqp_solver.h's
    /// RESTORATION PHASE note). False on every other row, INCLUDING a degraded-to-x seed.
    bool restoration_seed_used = false;
    /// True iff the Full-step watchdog restored an earlier best-residual (max of stationarity,
    /// feasibility, complementarity)
    /// iterate ON THIS PASS, i.e. this row's residual columns describe the
    /// RESTORED point rather than the diverged-or-stalled one the previous row
    /// left off at. Without this flag the history's residual column can jump
    /// backward with nothing in the row explaining why, and the watchdog is the
    /// only thing in this driver that can rebase the iterate backward mid-solve.
    /// False on every other row. At most one row per solve is true today, but the
    /// flag is read per-row, so a future relaxation of that bound needs no format
    /// change here.
    bool watchdog_restored = false;

    // --- The mode-selection telemetry ---
    //
    // Six counts off this major's own QpSolution, read by nothing in this
    // library; the shared reading rules are on `active_set_delta` below.

    /// The symmetric-difference count between THIS major's QP active set and the
    /// previous REPORTING major's: one for every inequality row that entered or
    /// left `QpSolution::ineq_active`, plus one for every variable whose
    /// `bound_state` changed at all (a per-variable state change counts once).
    ///
    /// The first reporting major counts against the empty set, so a solve's first
    /// row is a census of where the first subproblem landed rather than a 0. The
    /// restoration sub-solve is a separate sequence with its own empty start.
    ///
    /// All six fields are meaningful exactly where `tr_binding` IS: they read 0 on
    /// any row whose QpSolution never became this major's answer -- the
    /// stopped-AT-iterate and non-finite-iterate rows, and the exhausted-ladder
    /// row. Such a row also leaves the previous set unchanged, so the next
    /// reporting row's delta is measured against the last set really solved for.
    ///
    /// A TR-HELD VARIABLE IS NOT A Bound side here: every producer of a
    /// QpSolution reports `kFree` for a variable held by a TR-tight effective
    /// bound and flags it in `tr_active` instead. So these fields count REAL bound
    /// sides only, and a change of radius that moves a variable between a real
    /// bound and a TR pin DOES move this delta.
    ///
    /// ON A SOC-corrected row the set IS THE ORIGINAL QP's, on `tr_binding`'s own
    /// convention and for the same reason.
    Index active_set_delta = 0;
    /// Active inequality rows whose price is negligible beside the largest one:
    /// `|lambda_i(k)| <= kWeakActivityMargin * max(1, ||lambda_i||inf)`. See
    /// that constant for what the measure is and for what it is NOT.
    ///
    /// THE RELATIVE SCALING IS THE POINT AND ALSO THE CAVEAT: on an ill-scaled
    /// family a row priced at 10 reads weak beside one priced at 5e7. Accepted
    /// for telemetry, and stated so a reader does not take it for an absolute.
    Index weak_active_rows = 0;
    /// INACTIVE inequality rows whose slack is negligible beside the largest
    /// one: `s(k) <= kWeakActivityMargin * max(1, ||s||inf)`, where
    /// `s = bi - Ai p` is the QP's OWN nonnegative row slack (`qp_problem.h`'s
    /// `Ai x <= bi`) -- the units `ineq_active` is decided in, NOT the NLP row
    /// values `NlpEval::ci` carries.
    ///
    /// A VIOLATED ROW (`s(k) < 0`, reachable only on a non-optimal exit) is
    /// counted: it is at or past its own boundary, which is what this field
    /// reports about.
    Index near_active_rows = 0;
    /// Active inequality rows (`ineq_active` entries set).
    Index active_rows = 0;
    /// Variables at their real LOWER bound: `kAtLower`, plus `kFixed`, which is
    /// at both sides at once and so is counted in both census fields.
    Index active_lower_sides = 0;
    /// Variables at their real UPPER bound (`kAtUpper`, plus `kFixed`).
    Index active_upper_sides = 0;

    /// True iff THIS row is a NON-FINITE STEP ROLLBACK (M7 W2 R2-1): it describes the
    /// committed point the first major after a commit found non-finite (a gradient,
    /// or an `H`, `A` or `g`) and abandoned -- `qp_solved` is false, and the next
    /// row is the restored previous iterate at a shrunk radius. The abandoned point's
    /// residual columns may be NaN. False on every other row. Appended per
    /// `watchdog_restored`'s precedent; several rows per solve may carry it.
    bool rolled_back = false;
};

/// @brief Result of a whole SQP solve.
///
/// MULTIPLIERS. lambda_e/lambda_i are the LAST SUBPROBLEM'S multipliers, carried
/// out unchanged -- nlp_model.h's sign convention makes them the NLP's
/// multipliers at the returned point with no flip. z is NOT the subproblem's z;
/// it is the MODEL-IMPLIED bound multiplier at the returned point, computed from
/// grad L there, because the QP's z is forced to 0 at a TR-pinned index.
///
/// On a non-kOptimal exit x is the FINAL ITERATE reached and the multipliers are
/// whatever the last successful subproblem priced -- NOT cleared, because here
/// they are the caller's evidence about where the driver stopped.
///
/// On a certified kInfeasible exit the multipliers mean something stronger and
/// DIFFERENT, and must not be read as prices of the NLP's own constraints: they
/// are the RESTORATION problem's multipliers, a SUBGRADIENT CERTIFICATE that the
/// returned x is a stationary point of h(x) = ||cE(x)||_1 + sum_j max(0, cI_j(x)).
/// They satisfy
///     Je(x)^T lambda_e + Ji(x)^T lambda_i - z = 0,
///     lambda_e in [-1, 1]^me,  lambda_i in [0, 1]^mi,
///     lambda_e(i) = sign(cE_i(x)) wherever cE_i(x) != 0,
///     lambda_i(j) = 1 wherever cI_j(x) > 0 and 0 wherever cI_j(x) < 0,
/// with no grad f term -- which is what makes it a certificate of infeasibility
/// rather than of optimality. z on this exit is the RESTORATION problem's bound
/// price for the same reason: grad f is precisely the term a subgradient of h
/// must not contain.
///
/// The one exception is the non-finite-iterate kNumericalError exit, where
/// lambda_e/lambda_i/z are all cleared: at a NaN iterate nothing was measured. f
/// is still reported as the model returned it, which is to say possibly NaN.
///
/// history describes every iterate visited; whether the last row is an iterate
/// row or a failing-subproblem row depends on the exit -- see SqpCounters.
/// @brief What the problem-scaling layer did to a solve, reported on its
///        solution.
///
/// A DIAGNOSTIC, NOT A COUNTER, and deliberately not a member of SqpCounters: a
/// unit factor neither sums nor peaks meaningfully across the restoration fold,
/// and the sub-solve runs unscaled by design. An INACTIVE report is the identity,
/// field for field, so a reader never has to branch on `active`.
struct SqpScalingReport {
    /// True when the ROW/OBJECTIVE layer ran, i.e. `SqpOptions::enable_scaling` was
    /// on -- or `column_scaling == kAuto` fired, which engages it -- and factors were
    /// installed.
    bool active = false;
    /// The objective factor `sf`. Multiply a caller-scale objective by it to
    /// reach the engine's; divide to come back. 1.0 when inactive.
    double obj = 1.0;
    /// Largest and smallest constraint-row factor over the equality and
    /// inequality blocks together, or 1.0 when the problem has no rows (a
    /// bound-constrained problem has nothing to equilibrate). Their ratio is the
    /// row conditioning the layer removed.
    double row_max = 1.0;
    double row_min = 1.0;
    /// THE SCALED RESIDUAL THE CONVERGENCE TEST ACTUALLY READ, at the returned
    /// point -- the counterpart of SqpResult::kkt_residual, which is on the
    /// CALLER's scale. On a solve that ran in neither layer the two are the same
    /// number by construction; on a column-scaled one (`columns_active`) this is
    /// the residual in the scaled variables x~, whichever row layer ran. On a
    /// scaled one their difference is exactly the gap SqpOptions::enable_scaling
    /// discloses, and reporting both is what makes it computable. NaN whenever
    /// SqpResult::kkt_residual is NaN, for the same reason: nothing was measured.
    double scaled_kkt_residual = std::numeric_limits<double>::quiet_NaN();
    /// THE COLUMN LAYER (M7 W2 SCL, SqpOptions::column_scaling). True when the solve
    /// ran in scaled variables. `col_min`/`col_max` are the installed factors'
    /// extremes over the columns that carry Jacobian data, 1.0 when none was
    /// installed. On a CARRIED decision (SqpWarmStart::column_factors) min, max and
    /// spread are over every variable, identity entries included, and a carried
    /// "unscaled" decision reports spread 1. `col_spread` is the spread the rule COMPUTED whenever
    /// it ran -- also when kAuto's gate did not fire, which is how a caller reads why -- and 1.0
    /// under kOff.
    bool columns_active = false;
    double col_min = 1.0;
    double col_max = 1.0;
    double col_spread = 1.0;
};

/// @brief Why an equality-basis split dropped nothing although it was computed
/// (`detail/drivers/equality_basis.h`, EqualityBasisRefusal, one for one).
enum class SqpEqualityBasisRefusal {
    kNone,        ///< No computation was refused.
    kSpectrum,    ///< No gap, a failed retained-set check, reconstruction or coefficient cap.
    kNonFinite,   ///< A norm, scaled entry, coefficient or Gram value was not finite.
    kLift,        ///< The lift system K failed its factorization checks or probe.
    kCertificate, ///< A certificate factorization's evidence was not clean.
    kSubspace,    ///< The null-basis iteration did not confirm the count.
    kFill,        ///< The Gram matrix would be too dense to form.
};

/// @brief SqpOptions::equality_basis's account of one solve (SqpResult::equality_basis).
/// Row indices are the model's equality rows. "Every computation" below means every
/// split the solve computed plus the warm structural probe's own, when it ran.
struct SqpEqualityBasisReport {
    /// Most dropped-row identities split_dropped_rows carries.
    static constexpr Index kSplitDroppedRowsCap = 64;

    /// A split was computed at least once.
    bool computed = false;
    /// The first split was refused (no pivot gap, a failed check) and dropped nothing.
    bool undecidable = false;
    /// Computations that were refused (kUndecidable), over every computation.
    Index undecidable_count = 0;
    /// The reason of the last refused computation (kNone when none was).
    SqpEqualityBasisRefusal last_refusal = SqpEqualityBasisRefusal::kNone;
    /// |D| of the last split the solve computed (the probe's excluded), whatever
    /// happened to it afterwards (a switch-off, hand-off or refusal clears
    /// dropped_rows, not this).
    Index split_dropped_count = 0;
    /// That split's dropped rows, increasing, the first kSplitDroppedRowsCap of them.
    std::vector<Index> split_dropped_rows;
    /// Rows dropped by the split in force at exit.
    std::vector<Index> dropped_rows;
    /// Largest number of rows any split dropped.
    Index dropped_peak = 0;
    /// Splits computed after the first one.
    Index recomputes = 0;
    /// Numeric factorizations of the equilibrated Gram matrix over every split
    /// computed (a warm probe's included): 1 per full-rank split on the certificate's
    /// fast path, 2 when full rank is certified at the lower shift, 4 per reducing split.
    Index gram_factorizations = 0;
    /// Perturbed pivots the backend reported, summed over every factorization
    /// gram_factorizations counts (a nonzero count refuses its split with
    /// kCertificate, so 0 is the expectation and the IPARM-SURFACE evidence).
    Index gram_perturbed_pivots = 0;
    /// Of those factorizations, the ones whose backend reported no perturbed-pivot
    /// count (0 on MKL; Accelerate UNOBSERVED).
    Index gram_perturbation_unobserved = 0;
    /// Three consecutive majors recomputed; the QPs carried every row thereafter.
    bool switched_off = false;
    /// Splits refused because a norm, coefficient or Gram value was not finite.
    Index nonfinite_refusals = 0;
    /// Lift/projection failures: splits refused because K = [I C; C^T -I] failed
    /// its factorization checks or probe (kLift), plus every lift or projection solve
    /// that failed its forward-error estimate mid-solve (the SOC and QP projections, the
    /// two commit lifts, the SSN prox-centre lift), plus warm_probe_refusals; after a
    /// mid-solve failure the QPs carry every row.
    Index lift_refusals = 0;
    /// The warm structural probe's own split refused with kLift, or its projection
    /// failed its estimate (0 or 1; counted in lift_refusals too). THE WARM LEVEL
    /// DEGRADES: the probe QP stays unreduced, so its structural hash cannot match a
    /// warm start taken from a reducing solve, and the start resolves below kWarm
    /// (kSeeded when the values are ingestible). Safe, never silent.
    Index warm_probe_refusals = 0;
    /// lift_refusals by site; the five plus warm_probe_refusals sum to lift_refusals.
    struct LiftRefusalSites {
        Index compute = 0;        ///< kLift refusals of the solve's own splits.
        Index qp_projection = 0;  ///< The QP subproblem's rhs projection.
        Index soc_projection = 0; ///< The SOC subproblem's rhs projection.
        Index commit_lift = 0;    ///< The two commit lifts (direct and SOC acceptance).
        Index prox_centre = 0;    ///< The SSN prox-centre lift.
    };
    LiftRefusalSites lift_refusal_sites;
    /// Majors at which the QPs' equality rows changed; each dropped the warm state
    /// held in row coordinates (the QP seed, the IPQP carry, a staged IPQP seed).
    Index split_changes = 0;
    /// A zero step at a suspected inconsistency returned every row to the QPs for
    /// the rest of the solve.
    bool handed_off = false;
    /// The dropped row found inconsistent on two-iterate evidence (the exit is
    /// kInfeasible), or -1. Evidence, not a proof: identical Jacobian values at two
    /// iterates do not establish that the rows are linear.
    Index inconsistent_row = -1;
    /// Its combination gap, in the solver's units.
    double inconsistent_gap = 0.0;
    /// The retained rows its reconstruction uses.
    std::vector<Index> inconsistent_support;
    /// The two majors whose evidence found it.
    Index evidence_major_first = -1;
    Index evidence_major_second = -1;
    /// Largest |gap| of a dropped row at any major, in the solver's units.
    double max_gap = 0.0;
};

struct SqpResult : SolveResult {
    // The base carries the ANSWER -- status, x, lambda, z, f, the four shared
    // declared diagnostics, g, iterations, wall_seconds and the warm-start
    // snapshot -- in the problem's row space and caller units
    // (drivers/solve_result.h). Everything below is this engine's own.
    //
    // WHAT MOVED INTO THE BASE, and under which name (M6 W5 T8.4):
    //   status         same name; the type is SolveStatus, the identity of
    //                  SolveStatus on the five values this engine reports.
    //   x, lambda_e, lambda_i, z, f     same names, same meanings.
    //   iterations     NEW on the base: counters.major_iters of the TOP-LEVEL
    //                  solve. Nested restoration majors are NOT added -- they
    //                  are in `counters`.
    //   ce, ci         NEW: the declared constraint residuals at the returned
    //                  point, from the same stashed evaluation the shared
    //                  diagnostics are computed from. Empty when unmeasured.
    //   stationarity, feasibility_e, feasibility_i, complementarity
    //                  NEW, and NOT this engine's own measurements -- see the
    //                  three renamed fields below.
    //   wall_seconds   NEW, and NOT this engine's old field of that name: the
    //                  base's clock runs from the public entry to the return,
    //                  seam and warm setup and the export snapshot included.
    //                  The old measurement survives as solve_impl_seconds.

    /// @brief Work spent by this solve, restoration folded in.
    SqpCounters counters;
    /// One row per iterate visited; whether the last row is an iterate row or a
    /// failing-subproblem row depends on the exit -- see SqpCounters.
    std::vector<SqpIterate> history;

    // THE ENGINE'S OWN TERMINAL KKT MEASUREMENT, taken at the RETURNED
    // (x, lambda_e, lambda_i) by the same evaluate_kkt call the convergence test
    // read. It exists so a consumer can fill an outcome record without
    // reconstructing the history's exit shape: the last history row is NOT
    // reliably the returned point.
    //
    // RENAMED IN M6 W5 T8.4 -- `stationarity`, `feasibility` and
    // `complementarity` became `sqp_*` -- because the base now carries fields
    // of the first and last names with a DIFFERENT definition: the shared ones
    // are over the DECLARED problem in caller units, these are this engine's
    // own, in the space its convergence test gated in. A tolerance comparison
    // must be made against the number the test actually read, which is these.
    //
    // All four are NaN on the non-finite-iterate kNumericalError exit: nothing
    // was measured there, and a 0.0 would read as a converged residual.
    //
    // On a certified kInfeasible exit they measure the NLP's own KKT conditions
    // at the returned point -- an INFEASIBLE point, so `sqp_feasibility` is
    // large by construction and `sqp_stationarity` is the ordinary grad-L
    // measure, NOT the subgradient certificate's residual.
    //
    // One qualification on "at the returned multipliers", and it is THIS
    // ENGINE'S OWN: when `counters.ssn.ssn_sign_swept > 0` the sign sweep
    // clamped negative inequality prices AFTER this measurement, so these four
    // describe the PRE-SWEEP multipliers while `lambda_i` holds the swept
    // ones. `sqp_stationarity` is then optimistic by at most
    // `ssn_sign_sweep_max * ||Ji||inf`, and `sqp_complementarity` can only be
    // over-stated. NOT on a column-scaled solve (`scaling.columns_active`):
    // there the four are re-measured on the caller's model at the RETURNED,
    // post-sweep multipliers.
    //
    // IT DOES NOT REACH THE SHARED BASE (M6 W5 T8.4 fix1). SolveResult's four
    // are a statement about the values the call RETURNS: where the sweep -- or
    // anything else -- moved a price after the measurement they were taken
    // from, `SolveResult::stationarity` and `SolveResult::complementarity` are
    // NaN, UNMEASURED, rather than quietly describing prices the caller is not
    // being handed. An engine-measurement exception is this engine's to make
    // about its own columns and is not inherited by the shared contract.
    /// @brief Reduced/projected ||grad L||inf at the returned point.
    double sqp_stationarity = std::numeric_limits<double>::quiet_NaN();
    /// @brief max(||cE||inf, max(cI)+, bound violation) at the returned point.
    double sqp_feasibility = std::numeric_limits<double>::quiet_NaN();
    /// @brief max_j |lambda_i(j) * cI_j(x)| at the returned point. Gated at
    ///        kkt_tol on a kOptimal exit -- see SqpIterate::complementarity.
    double sqp_complementarity = std::numeric_limits<double>::quiet_NaN();
    /// @brief max(sqp_stationarity, sqp_feasibility). Not the whole convergence
    ///        test: complementarity (sqp_complementarity) is gated separately.
    double kkt_residual = std::numeric_limits<double>::quiet_NaN();

    /// Wall-clock seconds spent inside this solve's `solve_impl` ALONE --
    /// never around model construction, the bridge or seam lay, the staged-value
    /// ingest or the ledger bookkeeping.
    ///
    /// RENAMED from `wall_seconds` in M6 W5 T8.4, with its boundary UNCHANGED.
    /// The base's `wall_seconds` is a different measurement -- the whole public
    /// call -- and reusing the name for it would have made every existing
    /// reading silently mean something else.
    ///
    /// INFORMATIONAL, NEVER ASSERTED -- counters, not timings, are this
    /// project's currency of correctness. Nothing may gate on it. Defaults to
    /// 0.0; every public solve() that returns writes a value >= 0.0.
    double solve_impl_seconds = 0.0;

    /// @brief What the problem-scaling layer did to this solve; the identity
    ///        when it was off, which is the shipped default.
    ///
    /// READ IT BEFORE COMPARING `f`, the multiplier blocks or the four terminal
    /// residuals against another solve's: all of those are reported on the
    /// CALLER's scale whatever this says, so they are directly comparable -- but
    /// `scaled_kkt_residual` is the number the convergence test gated on, and on
    /// an active solve it is the one that was required to be <= kkt_tol.
    SqpScalingReport scaling;

    /// True ONLY on the certified infeasibility exit: the restoration phase ran to
    /// its own KKT test, that test passed, and the returned point fails the
    /// convergence test's inf-norm feasibility reading `SqpKkt::feasibility` --
    /// which the resume test reads too (M7 W2 SNR) -- so (x, lambda_e, lambda_i,
    /// z) is the subgradient certificate documented above. A point the
    /// convergence test calls feasible is no certificate (M7 W2 C12, CERT-1);
    /// since SNR such a point resumes.
    ///
    /// THE CERTIFICATE PREDICATE, STATED WITH ITS MARGIN (QC-1): the certificate
    /// requires the restoration point's inf-norm violation to exceed
    /// `kCertificateViolationMargin * feas_tol` (= 2 * feas_tol,
    /// detail/globalization/sqp/restoration.h). A kOptimal sub-solve whose point
    /// sits at inf-norm <= 2 * feas_tol takes the no-claim exit (kNumericalError),
    /// or the sufficient-reduction resume when `restoration_resume_reduction > 0`.
    ///
    /// False on EVERY OTHER exit, including other kInfeasible ones, and that is
    /// what this flag is for. It is NOT reliably derivable from the status or the
    /// counters. (Before the M7 W2 ladder round a solve that restored once and
    /// then met a second request, and one whose restoration was itself stuck,
    /// both reported kInfeasible with restoration_iters > 0; both are
    /// kNumericalError now, L1 -- the first only when the entry rule refuses that
    /// request: `SqpOptions::restoration_max_entries`, default 10, services a later
    /// request made after an accepted step.) Nor is the last row's verdict a reliable
    /// discriminator: it is kRestore from the funnel's signature or the elastic
    /// tier's exhaustion but kReject from the radius floor, and any of the three
    /// kInfeasible outcomes can pair with either shape.
    ///
    /// READ IT AS A One-way guarantee: true means the certificate holds; false
    /// means NO CLAIM IS MADE about the model.
    ///
    /// AND AT `SqpOptions::restoration_proximity > 0` THE PHASE'S OWN KKT TEST
    /// IS NO LONGER ENOUGH (M7 W2 M2b). The proximity term makes the wrapper's
    /// stationarity condition `grad P + J^T lambda - z_sub = 0`, so a
    /// wrapper-stationary point is stationary for `h + P`. The driver therefore
    /// computes `||J^T lambda - z_sub||_inf` -- with `z_sub` UNCORRECTED --
    /// EXPLICITLY at the restored point, O(nnz(J)), and sets this flag only when
    /// it is `<= kkt_tol`.
    ///
    /// EXPLICITLY, AND NOT VIA `zeta ||D^2 (x_r - x_k)||_inf`. The two coincide
    /// at an EXACTLY stationary point of the wrapper, but a sub-solve returns
    /// kOptimal at a FINITE tolerance: with `e = grad P + J^T lambda - z_sub`
    /// and `||e||_inf <= kkt_tol`, the certificate needs `||e - grad P||_inf`,
    /// which `||grad P||_inf <= kkt_tol` bounds only by `2*kkt_tol`. The
    /// cheaper test can therefore certify a point whose real residual is above
    /// tolerance, and it is not used -- not even as a fast path.
    ///
    /// AT THE SHIPPED `zeta = 0` NOTHING IS RECOMPUTED AT ALL. `grad P` is
    /// identically zero there, so the residual IS the sub-solve's own x-block
    /// stationarity, which its kOptimal already bounded IN THE RAW FRAME IT RAN
    /// IN -- and the driver therefore returns that verdict without reading a
    /// bundle, rather than re-deriving the same number through a scale
    /// round-trip whose last bits need not agree. The OFF path is the pre-M2b
    /// path by construction.
    ///
    /// The practical consequence at `zeta > 0` is stated rather than hidden: at
    /// any weight heavy enough to change a phase, a phase that MOVED does not
    /// certify. The recomputed residual is taken in the caller's frame recovered
    /// from the engine-scaled bundle, which agrees with the raw frame to
    /// ROUNDING and not bit for bit.
    bool infeasibility_certified = false;

    /// What SqpOptions::equality_basis did on this solve.
    SqpEqualityBasisReport equality_basis;

    /// The solve's exit state in warm_start.h's shape, for a LATER solve of a
    /// nearby problem to feed back in. Populated on EVERY exit of
    /// SqpSolver::solve(), including a failed one, from the best-known iterate.
    /// Populated is NOT the same as `valid`: one exit -- a start point the model
    /// could not evaluate -- fills these fields for inspection but reports
    /// valid == false, because feeding that point back would override the
    /// caller's own corrected x0.
    ///
    /// The base's `export_warm_start()` carries the SHARED currency shape
    /// (WarmStartData) taken at the same exit; this is the engine-native one.
    SqpWarmStart warm_start;
};

} // namespace hven::solvers
