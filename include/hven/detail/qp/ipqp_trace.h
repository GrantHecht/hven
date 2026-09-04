// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// ipqp_trace.h -- the IPQP tier's machine-trace schema v0 (spec section 7,
// :733-749), plus W2's driver-side `fallback.verdict` and W4 T2's whole-solve
// events. The event structs + the sink interface; `v`/`ev`/`seq`/`depth` are
// the serializer's envelope, not carried on any struct here.

#include <array>
#include <optional>
#include <string>

#include <hven/core/solver_status.h>
#include <hven/core/types.h>
#include <hven/detail/qp/ipqp_engine.h>
#include <hven/drivers/sqp_types.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

enum class IpqpTraceRegDir { kDown, kUp };
enum class IpqpTraceRegReason { kAccept, kInertia, kStall, kFloor };
enum class IpqpTraceRestartGrade { kCold, kBase, kFull };
enum class IpqpTraceRouteTo { kRefine, kSsn, kWalk };
enum class IpqpTraceFinalInertia { kOk, kWrong, kUnreadable };
enum class IpqpTraceEscapeReason { kBudget, kStall, kIndefinite, kNumerical, kInfeasibleSuspect };
/// Which KERNEL solved one subproblem (schema `qp.mode`). `kIpqp` is the
/// interior-point tier; M6 W4 T2 added the other two so the walk and the SSN
/// arms name themselves in the stream instead of being read off by absence.
enum class IpqpTraceQpMode { kIpqp, kWalk, kSsn };
enum class IpqpTraceOutcome { kOptimal, kRouted, kEscaped };

/// @brief One completed predictor+corrector pair (schema `ipqp.iter`).
struct IpqpTraceIterEvent {
    Index solve = 0; ///< This engine instance's own per-attach solve counter.
    Index major = 0; ///< The SQP major this subproblem belongs to (driver-set).
    Index it = 0;    ///< 1-based iteration index within this solve.
    double mu = 0.0;
    double rho = 0.0;   ///< rho_sched + rho_dem, the total primal shift in force.
    double delta = 0.0; ///< The final (possibly escalated) dual shift.
    double res_p = 0.0; ///< max(primal_eq, primal_iq).
    double res_d = 0.0; ///< stationarity.
    double res_c = 0.0; ///< complementarity.
    double sigma = 0.0; ///< Mehrotra's centering parameter.
    double alpha_p = 0.0;
    double alpha_d = 0.0;
    /// {n_pos, n_neg, n_zero}; absent (not zero-filled) when the read was
    /// unavailable or unreadable -- CLAUDE.md section 6.
    std::optional<std::array<Index, 3>> inertia;
    bool zero_derived = false; ///< InertiaEvidence::zero_is_derived; set iff inertia has_value().
    /// Perturbed-pivot count; absent when the backend does not report one
    /// (Accelerate), never zero-filled.
    std::optional<Index> perturbed;
    std::string facts; ///< Opaque, undefined by spec v0; always empty from W1.
};

/// @brief One (rho, delta) schedule move (schema `ipqp.reg`).
struct IpqpTraceRegEvent {
    IpqpTraceRegDir dir = IpqpTraceRegDir::kUp;
    double rho = 0.0;
    double delta = 0.0;
    IpqpTraceRegReason reason = IpqpTraceRegReason::kInertia;
};

/// @brief The section 5 warm restart this solve started from (schema `ipqp.restart`).
/// `shift_p`/`shift_d` are the SAY shift's own `delta_p`/`delta_d` (`warm_start_from`),
/// 0.0 when that shift never ran. See `.superpowers/w1-t8-report.md`.
struct IpqpTraceRestartEvent {
    IpqpTraceRestartGrade grade = IpqpTraceRestartGrade::kCold;
    bool repaired = false;
    double shift_p = 0.0;
    double shift_d = 0.0;
    double mu0 = 0.0;
    double mu_payload = 0.0;
    bool adopted = false;
    bool abandoned = false;
};

/// @brief The section 2.3 routing chain's destination for this subproblem
/// (schema `ipqp.route`). Driver-emitted (sqp_driver.cpp's kIpm arm): the
/// engine itself has no notion of refine/ssn/walk.
struct IpqpTraceRouteEvent {
    IpqpTraceRouteTo to = IpqpTraceRouteTo::kWalk;
    Index uncertain = 0;   ///< ipqp_face_uncertain for this subproblem.
    Index face_rows = 0;   ///< Active inequality rows in the identified face.
    Index face_bounds = 0; ///< Active bound sides in the identified face.
};

/// @brief The section 2.2 item 4 certification read's outcome (schema
/// `ipqp.certify`). Emitted only when a read was actually attempted --
/// `ipqp_final_inertia_read` values 0/1/2, never the "not performed" value 3.
struct IpqpTraceCertifyEvent {
    IpqpTraceFinalInertia final_inertia = IpqpTraceFinalInertia::kOk;
    bool downgraded = false;
};

/// @brief The schema's single `evidence` object (R2), nesting the two typed
/// blocks `IpqpResult` already carries. `stall.fired` iff `reason==kStall`,
/// `infeasibility.fired` iff `reason==kInfeasibleSuspect`.
struct IpqpTraceEscapeEvidence {
    IpqpStallEvidence stall;
    IpqpInfeasibilityEvidence infeasibility;
};

/// @brief One tier escape (schema `ipqp.escape`).
struct IpqpTraceEscapeEvent {
    IpqpTraceEscapeReason reason = IpqpTraceEscapeReason::kBudget;
    IpqpTraceEscapeEvidence evidence;
};

/// @brief One KERNEL INVOCATION on one subproblem (schema `qp.mode`),
/// driver-emitted in all three arms since M6 W4 T2(b).
///
/// ONE LINE PER INVOCATION, NOT PER SUBPROBLEM: a hand-off writes two, the
/// handing kernel's and its successor's, so the chain a major walked is
/// readable rather than inferred. The kIpm arm's own line is unchanged from W2.
///
/// THE OUTCOME MAP, per arm, is the driver's and is stated here because the
/// three arms report three different objects:
///   * `kWalk` -- `QpStatus::kOptimal` is `kOptimal`; kMaxIter, kInfeasible and
///     kNumericalError are all `kEscaped`. The walk has no successor kernel, so
///     nothing it exits with is a route; a kInfeasible walk hands to the
///     ELASTIC tier, which is the same kernel again.
///   * `kSsn` -- a usable, certified exit is `kOptimal`; every other exit
///     (engine escape or the trust-region gate's refusal) is `kRouted`, since
///     the walk re-solves the subproblem. `kEscaped` is UNREACHABLE in this arm
///     today: no SSN exit ends a subproblem.
///   * `kIpqp` -- unchanged (W2): `kOptimal` on the refine route, `kRouted` to
///     the SSN warm grade, `kEscaped` on a genuine escape to the walk.
/// `iters` is the invocation's own minor count.
struct QpModeTraceEvent {
    IpqpTraceQpMode mode = IpqpTraceQpMode::kIpqp;
    IpqpTraceOutcome outcome = IpqpTraceOutcome::kOptimal;
    std::string facts; ///< Opaque, undefined by spec v0; always empty from W1.
    Index iters = 0;
};

/// @brief The certified fallback's own outcome for one escaped subproblem (schema
/// `fallback.verdict`, M6 W2 T5), driver-emitted in the kIpm arm beside `ipqp.escape`. The first
/// four values are `SqpCounters`' four-way ENTRY partition: kDisproved and kRungB carry counters
/// of their own, kRelaxed and kExhausted are the rung-A-owned pair told apart by the returned
/// status. kUnfired is OUTSIDE that partition and charges nothing -- it is its own value (fix
/// round 1) so that summing the stream by `verdict` reproduces the counters without also
/// filtering on `entered_rung_a`.
enum class SqpFallbackVerdict { kDisproved, kRelaxed, kExhausted, kRungB, kUnfired };

/// @brief One entry into `certified_feasibility_fallback` (schema `fallback.verdict`).
/// An entry whose evidence never FIRED reports `entered_rung_a == false` with `verdict ==
/// kUnfired`: it ran W1's cold walk directly and charges no counter in the partition.
struct SqpFallbackVerdictTraceEvent {
    bool entered_rung_a = false;
    SqpFallbackVerdict verdict = SqpFallbackVerdict::kUnfired;
    /// The LAST rung-A attempt's first-rung penalty -- the RETRY's when `floor_retry`, since the
    /// retry replaces the declined attempt. Absent -- not zero-filled -- with no rung A.
    std::optional<double> rho_0;
    /// THIS ENTRY's placement was CLAMPED (headroom or dual_mu), including a clamp whose ladder
    /// was then declined and retried at the floor -- where `rho_0` is the floor's.
    bool rho0_ceiling_hit = false;
    bool floor_retry = false; ///< A declined rung A above the floor was re-run once at it.
    /// The reported ladder's own stopping rung's counts; 0 with no rung A. A CLASSIFICATION, not
    /// a cost record: rung B's walk and a declined first attempt are not priced here.
    Index qp_minor_iters = 0;
    Index qp_factorizations = 0;
};

/// @brief One exported `SqpSolution::history` row (schema `sqp.major`, M6 W4 T2).
///
/// THE ROW IS HELD BY REFERENCE, not copied into a second struct: the serializer
/// writes `SqpIterate`'s own fields in DECLARATION ORDER, so a field added to
/// the row cannot be forgotten by the stream (a `static_assert` on the row's
/// aggregate arity in the serializer fails the build until the key is added).
///
/// Emitted from inside the driver's `push_history`, AFTER its scaling map, so
/// the stream carries exactly what `history` carries -- CALLER units on a
/// scaled solve, one event per row, from all six push sites at once.
struct SqpMajorTraceEvent {
    /// The row, in the units it will be exported in. Valid for the duration of
    /// the `on_sqp_major` call only.
    const SqpIterate &row;
    /// This row's index in `SqpSolution::history` -- the value `history.size()`
    /// had before the push, so the stream and the vector share one numbering.
    Index major = 0;
    /// The arm that OWNED this row's QP. On a row with `qp_solved == false` no
    /// kernel ran and this reports the solve's CONFIGURED mode instead; the
    /// same line's `qp_solved` is what tells the two apart.
    IpqpTraceQpMode mode = IpqpTraceQpMode::kWalk;
};

/// @brief The solve's opening line (schema `sqp.solve.begin`, M6 W4 T2).
///
/// THE ROW COUNTS ARE THE SPLIT FORM'S OWN: `NlpModel` carries `eval_ce` (= 0)
/// and `eval_ci` (<= 0) with `me()`/`mi()` and has NO row bounds, so there is no
/// five-kind row census to derive here and none is invented (settler ruling,
/// 2026-09-04). `lower()`/`upper()` are on VARIABLES, and the five counts below
/// are that box's own census -- exhaustive and disjoint over the n variables.
struct SqpSolveBeginTraceEvent {
    Index n = 0;
    Index me = 0; ///< Equality rows (`eval_ce`).
    Index mi = 0; ///< Inequality rows (`eval_ci`).
    /// Both sides infinite.
    Index vars_free = 0;
    Index vars_lower_only = 0;                        ///< Finite lower, infinite upper.
    Index vars_upper_only = 0;                        ///< Infinite lower, finite upper.
    Index vars_ranged = 0;                            ///< Both finite and NOT equal.
    Index vars_fixed = 0;                             ///< Both finite and equal (a zero-width box).
    IpqpTraceQpMode qp_mode = IpqpTraceQpMode::kWalk; ///< The SETTING, not an outcome.
    WorkingSetLinearAlgebra ws_algebra = WorkingSetLinearAlgebra::kSchurBorder;
};

/// @brief The solve's closing line (schema `sqp.solve.end`, M6 W4 T2).
///
/// THE COUNTERS ARE HELD BY REFERENCE and serialized through
/// `solver_counters.h`'s three field tables, so the object is generated from the
/// structs rather than hand-typed. Emitted on every NORMAL exit of the solve; a
/// solve that leaves by an exception writes its `begin` and no `end`, which is
/// the honest record of one.
struct SqpSolveEndTraceEvent {
    SqpStatus status = SqpStatus::kOptimal;
    Index majors = 0; ///< `SqpCounters::major_iters`, the currency's own count.
    const SqpCounters &counters;
};

/// @brief The W4 hook: one sink; the eight W1/W2 methods are PURE, and every
/// method W4 adds is non-pure with an empty default (Q-S3). `nullptr` is
/// the off state every emit site checks before EMITTING; the seven tier
/// events are also built there, while `fallback.verdict` is the judge's own
/// out-param and is filled whether or not a sink is attached (a handful of
/// scalars). Destructor out-of-line (ipqp_trace.cpp, CLAUDE.md section 5).
class IpqpTraceSink {
  public:
    virtual ~IpqpTraceSink();
    virtual void on_ipqp_iter(const IpqpTraceIterEvent &event) = 0;
    virtual void on_ipqp_reg(const IpqpTraceRegEvent &event) = 0;
    virtual void on_ipqp_restart(const IpqpTraceRestartEvent &event) = 0;
    virtual void on_ipqp_route(const IpqpTraceRouteEvent &event) = 0;
    virtual void on_ipqp_certify(const IpqpTraceCertifyEvent &event) = 0;
    virtual void on_ipqp_escape(const IpqpTraceEscapeEvent &event) = 0;
    virtual void on_qp_mode(const QpModeTraceEvent &event) = 0;
    virtual void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &event) = 0;

    /// @brief One exported history row. NON-PURE, with an empty out-of-line
    /// default (plan section 6 Q-S3): the four recording sinks W1/W2 left in
    /// the tests do not want this event and are not touched by its arrival.
    virtual void on_sqp_major(const SqpMajorTraceEvent &event);

    /// @brief The solve's opening line. A sink that counts nesting does it here
    /// and at `on_sqp_solve_end`; no event carries a depth of its own.
    virtual void on_sqp_solve_begin(const SqpSolveBeginTraceEvent &event);
    virtual void on_sqp_solve_end(const SqpSolveEndTraceEvent &event);
};

} // namespace hven::solvers
