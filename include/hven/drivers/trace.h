// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// trace.h -- the machine-trace schema v0 (spec section 7, :733-749), plus W2's
// driver-side `fallback.verdict` and W4 T2's whole-solve events. The event
// structs + the sink interface; `v`/`ev`/`seq`/`depth` are the serializer's
// envelope, not carried on any struct here.
//
// PUBLIC since M6 W5 T4: a harness consumes the writer (`trace_writer.h`) and
// these events, so the schema is not a `detail/` header. `IpqpTraceSink` is
// `TraceSink` here; the `Ipqp*` EVENT prefixes are unchanged.

#include <array>
#include <optional>
#include <string>

#include <hven/core/solver_status.h>
#include <hven/core/types.h>
#include <hven/detail/drivers/interior_point_solver_fwd.h>
#include <hven/detail/interior/iterate_info.h>
#include <hven/detail/qp/ipqp_evidence.h>
#include <hven/drivers/solve_status.h>
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
/// WHICH CALL SITE ran this kernel (schema `qp.mode`'s `site`, M6 W4 T5).
///
/// NAMED WITHOUT THIS HEADER'S `IpqpTrace` PREFIX, deliberately and as the ODD
/// ONE OUT today: W5's registered rename moves this interface out of
/// `detail/qp/` and drops that prefix, so this is the target spelling arriving
/// early rather than an oversight. Registered with that rename.
///
/// `kDispatch` is the major's own dispatch -- the walk invocation, the kSsn arm,
/// and the kIpm arm's routing chain -- and is the only site whose count is one
/// per major. The other four run INSIDE a dispatch arm that has already written
/// its line, so a stream is read as "one dispatch line per major, plus whatever
/// the arm then paid".
enum class QpModeSite { kDispatch, kSsnWarmGrade, kFallbackRungB, kElasticRung, kSocResolve };

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

/// @brief ONE KERNEL INVOCATION on one subproblem (schema `qp.mode`),
/// driver-emitted. W4 T2(b) made the three DISPATCH arms write it; M6 W4 T5
/// (settler ruling) made EVERY kernel call site in the driver write it, tagged
/// by `site`, so the stream reconciles against invocation counts rather than
/// against majors alone.
///
/// THE FIVE SITES, and what each one costs a reader (`QpModeSite`):
///
///   * `kDispatch` -- the major's own dispatch: the walk invocation, the kSsn
///     arm, and the kIpm arm's routing chain. ONE PER MAJOR THAT SOLVED A QP; a
///     hand-off between ARMS writes two, the handing arm's and its successor's,
///     so the chain a major walked is readable.
///   * `kSsnWarmGrade` -- `route_through_ssn_warm_grade`'s SSN kernel call, NOT
///     a walk, made once per subproblem the kIpm chain routes to the grade.
///   * `kFallbackRungB` -- `certified_feasibility_fallback`'s cold walk, at both
///     of its return sites (the evidence never fired; rung A was declined).
///   * `kElasticRung` -- `run_elastic_ladder`'s walk on the elastic QP, ONCE PER
///     RUNG of its climb, reached from the fallback's rung A (entry and floor
///     retry) and from the driver's own elastic branch. UNBOUNDED per major, and
///     the reason the count identity for this site is written against
///     `elastic_activations + elastic_escalations` rather than against majors.
///   * `kSocResolve` -- the second-order correction's walk re-solve.
///
/// The restoration sub-solve is NOT a site: it is a whole nested SQP solve and
/// writes its own stream, dispatch lines included, at depth 1.
/// `QpEngine::refine_on_face` is excluded BY KIND -- it is the tier-3 face EQP
/// run on a face a kernel already produced, not a kernel the dispatch chooses
/// between, and it reports no minor count for `iters` to carry.
///
/// THE OUTCOME MAP, per mode, is the driver's and is stated here because the
/// three kernels report three different objects:
///   * `kWalk` -- `QpStatus::kOptimal` is `kOptimal`; kMaxIter, kInfeasible and
///     kNumericalError are all `kEscaped`. The walk has no successor kernel, so
///     nothing it exits with is a route; a kInfeasible walk hands to the
///     ELASTIC tier, which is the same kernel again.
///   * `kSsn` -- a usable, certified exit is `kOptimal`; every other exit
///     (engine escape or the trust-region gate's refusal) is `kRouted`, since
///     the walk re-solves the subproblem. `kEscaped` is UNREACHABLE in both SSN
///     sites today: no SSN exit ends a subproblem.
///   * `kIpqp` -- unchanged (W2): `kOptimal` on the refine route, `kRouted` to
///     the SSN warm grade, `kEscaped` on a genuine escape to the walk.
/// `iters` is the invocation's own minor count.
struct QpModeTraceEvent {
    IpqpTraceQpMode mode = IpqpTraceQpMode::kIpqp;
    IpqpTraceOutcome outcome = IpqpTraceOutcome::kOptimal;
    std::string facts; ///< Opaque, undefined by spec v0; always empty from W1.
    Index iters = 0;
    /// THE TRAILING KEY (M6 W4 T5), and it is trailing because `qp.mode` is one
    /// of the events plan section 2 rule 6 freezes: a frozen event's golden line
    /// moves only by a declared additive TRAILING key, never otherwise.
    QpModeSite site = QpModeSite::kDispatch;
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
    /// THE ARM THAT PRODUCED THIS ROW'S STEP, which is not the same reading as
    /// `qp.mode`'s: that event records a DISPATCH DECISION, this field records
    /// an outcome.
    ///
    /// They agree at kWalk and kSsn. Under kIpm they DIFFER on exactly the
    /// majors the tier routes to the SSN warm grade: the row reads `ssn` (the
    /// grade produced the step) while the major's DISPATCH line reads `ipqp`
    /// with `outcome` `routed`. Since M6 W4 T5 the grade does write an `ssn`
    /// `qp.mode` line of its own, at `site` `ssn_warm_grade` -- so the two
    /// readings are reconciled by the `site` key, not by absence. A kernel that
    /// runs INSIDE the kIpm arm -- the fallback's rung B, either elastic
    /// ladder, the SOC re-solve -- leaves the ROW's reading at `ipqp`, because
    /// no other ARM owned the major, and writes its own non-dispatch line.
    ///
    /// On a row with `qp_solved == false` no kernel ran and this reports the
    /// solve's CONFIGURED mode; the same line's `qp_solved` tells the two
    /// apart.
    IpqpTraceQpMode mode = IpqpTraceQpMode::kWalk;
};

/// @brief The variable box's own five-way census: exhaustive and disjoint over
/// the n variables, and the SAME reading for both engines' `begin` line.
///
/// ONE COPY, NOT TWO (M6 W4 T4): `sqp.solve.begin` and `ipm.solve.begin` both
/// report this census, so it is computed here rather than once per driver --
/// otherwise "free" could come to mean two different things in one stream.
struct VariableBoundCensus {
    Index vars_free = 0;       ///< Both sides infinite.
    Index vars_lower_only = 0; ///< Finite lower, infinite upper.
    Index vars_upper_only = 0; ///< Infinite lower, finite upper.
    Index vars_ranged = 0;     ///< Both finite and NOT equal.
    Index vars_fixed = 0;      ///< Both finite and equal (a zero-width box).
};

/// @brief Censuses `n` variables against the declared box.
///
/// An infinite side is unbounded there (every finite value is a real bound) and
/// both-finite splits on equality. A SIZE-0 vector means "no variable is bounded
/// on that side" -- the interior-point NLP materializes its bound vectors only
/// once a bound has been declared, and an empty vector is that problem's honest
/// reading, not a missing input.
///
/// @throws std::invalid_argument if `n` is negative, or if either vector's size
///         is neither `n` nor 0.
VariableBoundCensus census_variable_bounds(const Vec &lower, const Vec &upper, Index n);

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

/// @brief One interior-point iteration record (schema `ipm.iter`, M6 W4 T4).
///
/// THE RECORD IS HELD BY REFERENCE, exactly as `sqp.major` holds its row: the
/// serializer writes `IterateInfo`'s own fields in DECLARATION ORDER with the
/// trailing underscores dropped, and a `static_assert` on the record's aggregate
/// arity fails the build until a field added there gets a key.
///
/// EMITTED AT BOTH OF `alg_impl`'s LATE-CALLBACK SITES, immediately before the
/// callback itself, so the late callback is the event's oracle: what a caller
/// with a callback sees is what the stream carries.
///
/// THE TWO SITES ARE ONE LOOP'S TWO EXITS: the converge-check early exit, which
/// leaves the iterate unfactorized, and the ordinary end-of-iteration site. A
/// line from the first carries `barr_obj`, `merit_val` and `ls_iters` at 0 with
/// all three alphas at 1 -- the fields `fill_iter_info` never got to write. NOT
/// `h_facs`, which counts inertia-perturbation ladder steps, not
/// factorizations, and reads 0 at both sites on a cell that needs none.
///
/// `XSL`/`RHS` -- the two vectors the callback also receives -- are NOT carried:
/// schema v0 has no vector-valued field anywhere.
///
/// THE ABSENCE CONTRACT, PER FIELD (plan section 2 rule 5; fix round 1 R3).
/// Six keys can read `null`, for FIVE different reasons, and a reader that
/// collapses them to one loses the distinction:
///
///   * `prox_reg_primal`, `prox_reg_dual` -- proximal regularization is OFF,
///     OR this iteration never factorized (the converge-check exit's record
///     keeps both -1 defaults even with proximal mode on).
///   * `first_rejection_iter` -- no rejection was recorded this line search.
///     `0` is a READING, not an absence: the FIRST trial was rejected.
///   * `theta_at_first_rejection` -- unavailable: no rejection was recorded,
///     OR the LANG acceptance variant ran, which records no theta
///     (iterate_info.h). `0.0` is a feasible reading, not an absence.
///   * `h_facs` -- the Newton direction came back non-finite, so no inertia
///     ladder ran and there is no step count (alg_impl's `!GoodStep` branch
///     writes -1). `0` is a reading: the factorization needed no ladder step,
///     and an unfactorized converge-check record honestly took none.
///   * `p_pivots` -- no perturbed-pivot count was OBSERVED: the backend keeps
///     none (Apple Accelerate), or this iterate was never factorized. The
///     engine's own projection substitutes the integer 0 for an absent count
///     and the stream must not repeat that fabrication (CLAUDE.md section 6);
///     `IterateInfo::p_pivots_observed_` is the predicate and carries no key.
struct IpmIterTraceEvent {
    /// The iteration record, valid for the duration of the `on_ipm_iter` call
    /// only.
    const IterateInfo &iterate;
    /// THE PHASE this iteration belongs to, 0-based over the phase sequence the
    /// call requested. Carried because `IterateInfo::iter_` restarts at 0 in
    /// every phase, so on a multi-phase entry point (`solve_optimize`, ...) it
    /// is not a key on its own.
    Index phase = 0;
};

/// @brief The interior-point solve's opening line (schema `ipm.solve.begin`,
/// M6 W4 T4), written once per public entry point.
///
/// THE CENSUS IS THE DECLARED BOX'S, shared with `sqp.solve.begin` through
/// `census_variable_bounds`. `n` is the caller's own variable count and
/// `n_reduced` the space the solver iterates in; they differ exactly when the
/// fixed-variable treatment eliminated a variable.
struct IpmSolveBeginTraceEvent {
    Index n = 0;         ///< Declared primal variables (the caller's space).
    Index n_reduced = 0; ///< Primal variables the solver iterates in.
    /// Equality rows the solver will factorize -- the caller's own, PLUS the
    /// internal fixing rows under the MakeConstraint treatment.
    Index me = 0;
    Index mi = 0; ///< Inequality rows.
    /// The declared box's census, FLAT and in `sqp.solve.begin`'s own five keys
    /// rather than nested: the arity net that pins this struct's field list
    /// cannot see through a nested aggregate (brace elision), and the two
    /// engines' openers are easier to read side by side this way. Filled from
    /// `census_variable_bounds`, which is still the only copy of the arithmetic.
    Index vars_free = 0;
    Index vars_lower_only = 0;
    Index vars_upper_only = 0;
    Index vars_ranged = 0;
    Index vars_fixed = 0;
    /// Phase steps this entry point REQUESTED; a conditional step that is later
    /// skipped still counts.
    Index phases = 0;
    Index max_iters = 0;     ///< Settings::max_iters_, the per-phase cap.
    Index max_acc_iters = 0; ///< Settings::max_acc_iters_, the ACCEPTABLE run length.
    double kkt_tol = 0.0;    ///< Settings::kkt_tol_.
    double econ_tol = 0.0;   ///< Settings::econ_tol_.
    double icon_tol = 0.0;   ///< Settings::icon_tol_.
    double bar_tol = 0.0;    ///< Settings::bar_tol_.
    double init_mu = 0.0;    ///< Settings::init_mu_, the barrier start.
    double obj_scale = 1.0;  ///< Settings::obj_scale_, captured for this call.
    InertiaModes inertia_mode = InertiaModes::classic;
    RestorationModes restoration_mode = RestorationModes::off;
};

/// @brief The interior-point solve's closing line (schema `ipm.solve.end`,
/// M6 W4 T4).
///
/// EVERY `_s` FIELD IS WALL-CLOCK AND INFORMATIONAL (plan section 2 rule 7,
/// CLAUDE.md section 7): no pin reads one. They are the driver's own
/// `IpmResult` timing set in full -- a hand-picked subset would let a reader
/// believe the parts summed to the whole.
///
/// Written on the entry point's single normal return; a call that leaves by an
/// exception writes its `begin` and no `end`.
struct IpmSolveEndTraceEvent {
    SolveStatus status = SolveStatus::kMaxIter;
    Index iters = 0; ///< `IpmResult::iterations`, summed over the phases.
    double total_time_s = 0.0;
    double pre_time_s = 0.0;
    double func_time_s = 0.0;
    double kkt_time_s = 0.0;
    double print_time_s = 0.0;
    double solver_init_time_s = 0.0;
    double misc_time_s = 0.0; ///< `IpmResult::misc_time()`, the derived remainder.
};

/// @brief The W4 hook: one sink; the eight W1/W2 methods are PURE, and every
/// method W4 adds is non-pure with an empty default (Q-S3). `nullptr` is
/// the off state every emit site checks before EMITTING; the seven tier
/// events are also built there, while `fallback.verdict` is the judge's own
/// out-param and is filled whether or not a sink is attached (a handful of
/// scalars). Destructor out-of-line (src/drivers/trace.cpp, CLAUDE.md section 5).
class TraceSink {
  public:
    virtual ~TraceSink();
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

    /// @brief One interior-point iteration. NON-PURE with an empty out-of-line
    /// default, on the same terms as the three above (plan section 6 Q-S3).
    virtual void on_ipm_iter(const IpmIterTraceEvent &event);

    /// @brief The interior-point solve's opening and closing lines.
    ///
    /// THESE DO NOT MOVE A SINK'S `depth` and must not: the interior-point
    /// driver has no nested driver of its own, so every line it writes belongs
    /// to whatever nesting level the SQP-side pair last established -- 0 for a
    /// stream that is only ever an IPM's.
    virtual void on_ipm_solve_begin(const IpmSolveBeginTraceEvent &event);
    virtual void on_ipm_solve_end(const IpmSolveEndTraceEvent &event);
};

} // namespace hven::solvers
