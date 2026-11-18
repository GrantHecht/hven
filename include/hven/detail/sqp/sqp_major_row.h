// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// sqp_major_row.h -- the SQP driver's internal per-major working row. It is not part of the
// result: `SqpResult::history` carries `IterationRecord` (solver/iteration_record.h), built
// from this row when it is pushed.

#include <hven/core/types.h>
#include <hven/solver/step_verdict.h>
#include <hven/qp/qp_types.h>

namespace hven {

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
/// step that reached it was rejected after the fact. Rollback rows may
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
///                         history[k-1].verdict == StepVerdict::kAcceptObjectiveStep ||
///                         history[k-1].verdict == StepVerdict::kAcceptFeasibilityStep;
///
/// The predicate reads the PREVIOUS row, not this one: filtering on a row's own
/// kReject verdict is wrong in both directions.
///
/// Every scale-carrying column IS ON THE CALLER'S SCALE, whether or not the solve
/// ran with `SqpOptions::row_scaling` on. The number a scaled solve's
/// convergence test gated on is `SqpScalingReport::scaled_kkt_residual`.
struct SqpMajorRow {
    /// @brief 0-based index of THIS ROW (== subproblem index).
    Index trial = 0;

    /// @brief Objective at the iterate.
    double f = 0.0;
    /// @brief Reduced/projected ||grad L||inf; see sqp_solver.h's CONVERGENCE TEST.
    double stationarity = 0.0;
    /// @brief max(||cE||inf, max(cI)+, bound violation).
    double feasibility = 0.0;
    /// max_j |lambda_i(j) * cI_j(x)|. GATED at kkt_tol by the convergence test
    /// on every major, beside stationarity and feasibility.
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
    /// i.e. the QpSolveOverrides::trust_region_radius the driver passed. On a
    /// stopped-AT-iterate row it is the radius the next subproblem would
    /// have been given.
    double tr_radius = 0.0;
    /// The QpSolveOverrides::dual_regularization THIS subproblem was solved at (the MAIN
    /// trial's QP -- a subsequent SOC/elastic re-solve on the same row uses
    /// its own separate override, per sqp_solver.h's ADAPTIVE DUAL
    /// REGULARIZATION note, so it is not what this field describes there).
    /// Meaningful iff qp_solved; 0.0 on a stopped-AT-iterate row, like the
    /// other qp_* fields. Always opts.qp.dual_regularization (the engine default) when
    /// SqpAdvancedOptions::adaptive_dual_regularization is false -- byte-identical across the whole
    /// history. This is the dual_regularization the subproblem was DISPATCHED at: the walk may
    /// lower its effective value within the solve (QpCounters::dual_regularization_decreases > 0),
    /// in which case the subproblem exited at a smaller mu than recorded and the warm-start stamp
    /// over-states the footprint, which is the safe direction.
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
    /// True iff this row's ACCEPTANCE (verdict is kAcceptObjectiveStep/kAcceptFeasibilityStep) was
    /// won by a second-order correction, i.e. the strategy rejected the raw
    /// QP step p and a hot-started re-solve rescued it -- the iterate that
    /// follows this row is x + (the SOC re-solve's own step), NOT x + p.
    /// False on every other row, including one where SOC was ATTEMPTED but
    /// its own corrected point was also rejected (that attempt is counted
    /// in SqpCounters::soc_steps, not recorded per-row -- see
    /// sqp_solver.h's SECOND-ORDER CORRECTION note for why a failed attempt
    /// does not get a field of its own). Always false when
    /// SqpAdvancedOptions::second_order_correction is false.
    bool soc_applied = false;
    /// True iff an ELASTIC SOLVE supplied this major: the elastic tier's re-solve
    /// after a plain QP returned kInfeasible, or the certified fallback's rung A
    /// after a kIpqp escape. Set on BOTH outcomes -- the row whose step came from
    /// the elastic solve, and the row that ended the solve because the ladder was
    /// exhausted. It is the branch input SOC is gated on.
    ///
    /// ON SUCH A ROW THE qp_* Fields describe the final elastic solve, not the
    /// kInfeasible one that triggered it, with the escalation re-solves' costs
    /// folded into SqpCounters' aggregates only. On a rung-A-owned major they are
    /// the ladder's own counts.
    ///
    /// tr_binding IS STILL MEANINGFUL: the elastic solve gets its trust region as
    /// REAL bounds on the original variables rather than through QpSolveOverrides,
    /// and the driver re-derives the radius bit exactly as the engine would have.
    bool elastic_applied = false;
    /// True iff THIS row's elastic ladder started at a CLAMPED placement -- the
    /// evidence priced the violation above the escalation headroom or above the
    /// dual_regularization safety margin -- including a clamp whose declined ladder was then
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

    /// True iff THIS row is a NON-FINITE STEP ROLLBACK: it describes the
    /// committed point the first major after a commit found non-finite (a gradient,
    /// or an `H`, `A` or `g`) and abandoned -- `qp_solved` is false, and the next
    /// row is the restored previous iterate at a shrunk radius. The abandoned point's
    /// residual columns may be NaN. False on every other row. Appended per
    /// `watchdog_restored`'s precedent; several rows per solve may carry it.
    bool rolled_back = false;
};

} // namespace hven
