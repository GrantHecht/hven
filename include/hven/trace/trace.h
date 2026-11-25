// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// trace.h -- the machine-trace schema v1 (docs/trace-schema-v1.md): the event
// structs and the sink interface. `v`/`ev`/`seq`/`sid`/`depth`/`eng` are the
// serializer's envelope, not carried on any struct here.
//
// PUBLIC: a harness consumes the writer (`json_lines_trace_sink.h`) and
// these events, so the schema is not a `detail/` header. `IpqpTraceSink` is
// `TraceSink` here; the `Ipqp*` EVENT prefixes are unchanged.

#include <array>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include <hven/core/solver_counters.h>
#include <hven/core/solver_status.h>
#include <hven/core/stop.h>
#include <hven/core/types.h>
#include <hven/ipm/ipm_iterate.h>
#include <hven/ipm/ipm_options.h>
#include <hven/ipm/ipm_phase_report.h>
#include <hven/ipm/ipm_solver_types.h>
#include <hven/model/problem.h>
#include <hven/qp/ipqp_evidence.h>
#include <hven/qp/qp_types.h>
#include <hven/solver/iteration_record.h>
#include <hven/solver/solve_result.h>
#include <hven/sqp/sqp_options.h>
#include <hven/sqp/sqp_solver_types.h>

namespace hven {

enum class IpqpTraceRegDirection { kDown, kUp };
enum class IpqpTraceRegReason { kAccept, kInertia, kStall, kFloor };
enum class IpqpTraceRestartGrade { kCold, kBase, kFull };
enum class IpqpTraceRouteTarget { kRefine, kSsn, kWalk };
enum class IpqpTraceFinalInertia { kOk, kWrong, kUnreadable };
enum class IpqpTraceEscapeReason { kBudget, kStall, kIndefinite, kNumerical, kInfeasibleSuspect };
/// Which KERNEL solved one subproblem (schema `qp.mode`). `kIpqp` is the
/// interior-point tier; the walk and the SSN arms name themselves in the
/// stream rather than being read off by absence.
enum class QpTraceOutcome { kOptimal, kRouted, kEscaped };
/// WHICH CALL SITE ran this kernel (schema `qp.mode`'s `site`).
///
/// NAMED WITHOUT THIS HEADER'S `IpqpTrace` PREFIX, deliberately and as the ODD
/// ONE OUT: the prefix is dropped on purpose, because this is the
/// target spelling for the interface.
///
/// `kDispatch` is the major's own dispatch -- the walk invocation, the kSsn arm,
/// and the kIpqp arm's routing chain -- and is the only site whose count is one
/// per major. The other four run INSIDE a dispatch arm that has already written
/// its line, so a stream is read as "one dispatch line per major, plus whatever
/// the arm then paid".
enum class QpTraceModeSite {
    kDispatch,
    kSsnWarmGrade,
    kFallbackRungB,
    kElasticRung,
    kSocResolve,
    kCapRetry,
};

/// @brief One completed predictor+corrector pair (schema `ipqp.iter`).
struct IpqpIterTraceEvent {
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
    std::string facts; ///< Opaque, undefined by spec v0; always empty.
};

/// @brief One (rho, delta) schedule move (schema `ipqp.reg`).
struct IpqpRegTraceEvent {
    IpqpTraceRegDirection dir = IpqpTraceRegDirection::kUp;
    double rho = 0.0;
    double delta = 0.0;
    IpqpTraceRegReason reason = IpqpTraceRegReason::kInertia;
};

/// @brief The section 5 warm restart this solve started from (schema `ipqp.restart`).
/// `shift_p`/`shift_d` are the SAY shift's own `delta_p`/`delta_d` (`warm_start_from`),
/// 0.0 when that shift never ran. See `.superpowers/w1-t8-report.md`.
struct IpqpRestartTraceEvent {
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
/// (schema `ipqp.route`). Driver-emitted (sqp_solver.cpp's kIpqp arm): the
/// engine itself has no notion of refine/ssn/walk.
struct IpqpRouteTraceEvent {
    IpqpTraceRouteTarget to = IpqpTraceRouteTarget::kWalk;
    Index uncertain = 0;   ///< ipqp_face_uncertain for this subproblem.
    Index face_rows = 0;   ///< Active inequality rows in the identified face.
    Index face_bounds = 0; ///< Active bound sides in the identified face.
};

/// @brief The section 2.2 item 4 certification read's outcome (schema
/// `ipqp.certify`). Emitted only when a read was actually attempted --
/// `ipqp_final_inertia_read` values 0/1/2, never the "not performed" value 3.
struct IpqpCertifyTraceEvent {
    IpqpTraceFinalInertia final_inertia = IpqpTraceFinalInertia::kOk;
    bool downgraded = false;
};

/// @brief The schema's single `evidence` object, nesting the two typed
/// blocks `IpqpResult` already carries. `stall.fired` iff `reason==kStall`,
/// `infeasibility.fired` iff `reason==kInfeasibleSuspect`.
struct IpqpTraceEscapeEvidence {
    IpqpStallEvidence stall;
    IpqpInfeasibilityEvidence infeasibility;
};

/// @brief One tier escape (schema `ipqp.escape`).
struct IpqpEscapeTraceEvent {
    IpqpTraceEscapeReason reason = IpqpTraceEscapeReason::kBudget;
    IpqpTraceEscapeEvidence evidence;
};

/// @brief ONE KERNEL INVOCATION on one subproblem (schema `qp.mode`),
/// driver-emitted. EVERY kernel call site in the driver writes it, tagged
/// by `site`, so the stream reconciles against invocation counts rather than
/// against majors alone.
///
/// THE SIX SITES, and what each one costs a reader (`QpTraceModeSite`):
///
///   * `kDispatch` -- the major's own dispatch: the walk invocation, the kSsn
///     arm, and the kIpqp arm's routing chain. ONE PER MAJOR THAT SOLVED A QP; a
///     hand-off between ARMS writes two, the handing arm's and its successor's,
///     so the chain a major walked is readable.
///   * `kSsnWarmGrade` -- `route_through_ssn_warm_grade`'s SSN kernel call, NOT
///     a walk, made once per subproblem the kIpqp chain routes to the grade.
///   * `kFallbackRungB` -- `certified_feasibility_fallback`'s cold walk, at both
///     of its return sites (the evidence never fired; rung A was declined).
///   * `kElasticRung` -- `run_elastic_ladder`'s walk on the elastic QP, ONCE PER
///     RUNG of its climb, reached from the fallback's rung A (entry and floor
///     retry) and from the driver's own elastic branch. UNBOUNDED per major, and
///     the reason the count identity for this site is written against
///     `elastic_activations + elastic_escalations` rather than against majors.
///   * `kSocResolve` -- the second-order correction's walk re-solve.
///   * `kCapRetry` -- the cap retry's walk re-solve, made once per
///     capped subproblem under `SqpAdvancedOptions::qp_iteration_limit_retry` and NEVER at that
///     toggle's default: it is the same subproblem at the same radius on an
///     engine whose `QpOptions::max_iterations` is twice the cap, so its line's
///     `iters` is the retry's own minor count. `== SqpCounters::qp_cap_retries`.
///
/// The restoration sub-solve is NOT a site: it is a whole nested SQP solve and
/// writes its own stream, dispatch lines included, at depth 1.
/// `QpEngine::refine_on_face` is excluded BY KIND -- it is the tier-3 face EQP
/// run on a face a kernel already produced, not a kernel the dispatch chooses
/// between, and it reports no minor count for `iters` to carry.
///
/// THE OUTCOME MAP, per mode, is the driver's and is stated here because the
/// three kernels report three different objects:
///   * `kWalk` -- `QpStatus::kOptimal` is `kOptimal`; kIterationLimit, kInfeasible and
///     kNumericalError are all `kEscaped`. The walk has no successor kernel, so
///     nothing it exits with is a route; a kInfeasible walk hands to the
///     ELASTIC tier, which is the same kernel again.
///   * `kSsn` -- a usable, certified exit is `kOptimal`; every other exit
///     (engine escape or the trust-region gate's refusal) is `kRouted`, since
///     the walk re-solves the subproblem. `kEscaped` is UNREACHABLE in both SSN
///     sites today: no SSN exit ends a subproblem.
///   * `kIpqp`: `kOptimal` on the refine route, `kRouted` to
///     the SSN warm grade, `kEscaped` on a genuine escape to the walk.
/// `iters` is the invocation's own minor count.
struct QpModeTraceEvent {
    QpTraceMode mode = QpTraceMode::kIpqp;
    QpTraceOutcome outcome = QpTraceOutcome::kOptimal;
    std::string facts; ///< Opaque, undefined by spec v0; always empty.
    Index iters = 0;
    /// THE TRAILING KEY, and it is trailing because `qp.mode` is one
    /// of the events plan section 2 rule 6 freezes: a frozen event's golden line
    /// moves only by a declared additive TRAILING key, never otherwise.
    QpTraceModeSite site = QpTraceModeSite::kDispatch;
};

/// @brief The certified fallback's own outcome for one escaped subproblem (schema
/// `fallback.verdict`), driver-emitted in the kIpqp arm beside `ipqp.escape`. The first
/// four values are `SqpCounters`' four-way ENTRY partition: kDisproved and kRungB carry counters
/// of their own, kRelaxed and kExhausted are the rung-A-owned pair told apart by the returned
/// status. kUnfired is OUTSIDE that partition and charges nothing -- it is its own value (fix
/// round 1) so that summing the stream by `verdict` reproduces the counters without also
/// filtering on `entered_rung_a`.
enum class SqpTraceFallbackVerdict { kDisproved, kRelaxed, kExhausted, kRungB, kUnfired };

/// @brief One entry into `certified_feasibility_fallback` (schema `fallback.verdict`).
/// An entry whose evidence never FIRED reports `entered_rung_a == false` with `verdict ==
/// kUnfired`: it ran the cold walk directly and charges no counter in the partition.
struct SqpFallbackVerdictTraceEvent {
    bool entered_rung_a = false;
    SqpTraceFallbackVerdict verdict = SqpTraceFallbackVerdict::kUnfired;
    /// The LAST rung-A attempt's first-rung penalty -- the RETRY's when `floor_retry`, since the
    /// retry replaces the declined attempt. Absent -- not zero-filled -- with no rung A.
    std::optional<double> rho_0;
    /// THIS ENTRY's placement was CLAMPED (headroom or dual_regularization), including a clamp
    /// whose ladder was then declined and retried at the floor -- where `rho_0` is the floor's.
    bool rho0_ceiling_hit = false;
    bool floor_retry = false; ///< A declined rung A above the floor was re-run once at it.
    /// The reported ladder's own stopping rung's counts; 0 with no rung A. A CLASSIFICATION, not
    /// a cost record: rung B's walk and a declined first attempt are not priced here.
    Index qp_minor_iters = 0;
    Index qp_factorizations = 0;
};

/// @brief The variable box's own five-way census: exhaustive and disjoint over
/// the n variables, and the SAME reading for both engines' `begin` line.
///
/// ONE COPY, NOT TWO: `sqp.solve.begin` and `ipm.solve.begin` both
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

/// @brief Which engine wrote an event.
enum class TraceEngine { kIpm, kSqp };

/// @brief `solve.begin`'s interior-point block.
struct IpmSolveBeginInfo {
    Index n_reduced = 0; ///< Primal variables the solver iterates in.
    Index me = 0;
    Index mi = 0;
    Index internal_fixed_rows = 0;
    Index phases = 0;
    Index max_acceptable_iterations = 0;
    double initial_mu = 0.0;
    double obj_scale = 1.0;
    double acceptable_stationarity_tol = 0.0;
    double acceptable_complementarity_tol = 0.0;
    double acceptable_equality_tol = 0.0;
    double acceptable_inequality_tol = 0.0;
    IpmInertiaMode inertia_mode = IpmInertiaMode::kClassic;
    IpmRestorationMode restoration_mode = IpmRestorationMode::kOff;
    Index kkt_dim = 0;
    Index kkt_nnz = 0;
};

/// @brief `solve.begin`'s SQP block.
struct SqpSolveBeginInfo {
    Index me = 0;
    Index mi = 0;
    QpTraceMode qp_mode = QpTraceMode::kWalk; ///< The SETTING, not an outcome.
    QpWorkingSetAlgebra ws_algebra = QpWorkingSetAlgebra::kSchurBorder;
    SqpHessianMode hessian_mode = SqpHessianMode::kExact;
    bool row_scaling = false;
    SqpColumnScaling column_scaling = SqpColumnScaling::kOff;
};

/// @brief The solve's opening record (`solve.begin`), both engines.
struct SolveBeginTraceEvent {
    TraceEngine engine = TraceEngine::kIpm;
    ProblemStatistics statistics; ///< The declared problem's census.
    double optimality_tol = 0.0;
    double feasibility_tol = 0.0;
    Index max_iterations = 0;
    std::optional<IpmSolveBeginInfo> ipm;
    std::optional<SqpSolveBeginInfo> sqp;
};

/// @brief One iteration row (`iter`), both engines. Borrowed for the call.
struct IterTraceEvent {
    const IterationRecord &record;
};

enum class MessageSeverity { kNotice, kWarning };

/// @brief One diagnostic occurrence (`message`), both engines. A slot a kind
/// does not use is `std::nullopt`.
struct MessageTraceEvent {
    TraceEngine engine = TraceEngine::kIpm;
    MessageKind kind = MessageKind::kRankDeficiency;
    MessageSeverity severity = MessageSeverity::kWarning;
    std::optional<Index> phase;
    std::optional<Index> iter;
    std::optional<double> a;
    std::optional<double> b;
    std::optional<Index> k;
    std::optional<std::string_view> s; ///< A per-kind enum value; a literal.
    std::optional<Index> p;            ///< `inertia_exhausted`: observed positive eigenvalues.
    std::optional<Index> n;            ///< `inertia_exhausted`: observed negative eigenvalues.
    std::optional<Index> z;            ///< `inertia_exhausted`: observed zero eigenvalues.
    std::optional<Index> expected_p;
    std::optional<Index> expected_n;
    std::optional<ElasticLadderRow> ladder; ///< `elastic_exhausted`: the row's ladder block.
};

/// @brief The severity a kind carries unless its emit site says otherwise.
constexpr MessageSeverity default_message_severity(MessageKind kind) noexcept {
    switch (kind) {
    case MessageKind::kSolverInitialized:
    case MessageKind::kInterruptSkippingPhases:
    case MessageKind::kStopRequestedSkippingPhases:
    case MessageKind::kRestorationEntered:
    case MessageKind::kStartPointClamped:
    case MessageKind::kScaledConvergenceRefused:
    case MessageKind::kStopAtIteration:
        return MessageSeverity::kNotice;
    default:
        return MessageSeverity::kWarning;
    }
}

/// @brief The declared diagnostics of the returned point (`solve.end.diagnostics`).
struct DeclaredDiagnosticsTrace {
    double stationarity = 0.0;
    double feasibility = 0.0;
    double complementarity = 0.0;
};

/// @brief `solve.end`'s interior-point block. Wall-clock keys are informational.
struct IpmSolveEndInfo {
    IpmConvergence convergence;
    const IpmCounters *counters = nullptr;
    double total_time_s = 0.0;
    double pre_time_s = 0.0;
    double func_time_s = 0.0;
    double kkt_time_s = 0.0;
    double print_time_s = 0.0;
    double solver_init_time_s = 0.0;
    double misc_time_s = 0.0;
    Index phases_ran = 0;
    Index kkt_analyses_total = 0;
    std::optional<Index> factor_memory;
    std::optional<Index> factor_flops;
};

/// @brief `solve.end`'s SQP block.
struct SqpSolveEndInfo {
    SqpConvergence convergence;
    const SqpCounters *counters = nullptr;
    bool scaling_active = false;
    double obj_scale = 1.0;
    double row_scale_min = 1.0;
    double row_scale_max = 1.0;
    double scaled_kkt_residual = 0.0;
    bool columns_active = false;
    double col_spread = 1.0;
    StartLevel start_level_used = StartLevel::kCold;
};

/// @brief The solve's closing record (`solve.end`), both engines. At depth 1
/// (an SQP restoration sub-solve) the result-level fields the handle derives
/// (`stop_reason`, `message`, `diagnostics`, `work`) are `std::nullopt`.
struct SolveEndTraceEvent {
    TraceEngine engine = TraceEngine::kIpm;
    SolveStatus status = SolveStatus::kIterationLimit;
    std::optional<StopReason> stop_reason;
    std::optional<std::string_view> message;
    Index iterations = 0;
    bool returned_best_iterate = false;
    bool infeasibility_certified = false;
    double f = 0.0;
    std::optional<DeclaredDiagnosticsTrace> diagnostics;
    std::optional<WorkCounters> work;
    double wall_s = 0.0;
    std::optional<IpmSolveEndInfo> ipm;
    std::optional<SqpSolveEndInfo> sqp;
};

/// @brief One phase of an interior-point solve opening or closing (schema
/// `ipm.phase.begin` / `ipm.phase.end`).
///
/// ONE STRUCT, TWO EVENTS: the two lines carry exactly the same three facts and
/// differ only in which end of the phase they mark, so a second struct would be
/// a copy to keep in step. They bracket the phase's `ipm.iter` rows and its
/// `ipm.phase.exit`; a CONDITIONAL phase the sequence skipped writes NEITHER,
/// which is how a reader tells a skipped phase from one that ran without
/// consulting `ipm.solve.begin`'s `phases` count.
///
/// WHERE THE KKT ANALYSIS SITS, and it is not inside this bracket: the engine
/// re-initializes BEFORE the phase's `begin` (the entry analysis, and the
/// inter-phase one at the end of the PREVIOUS phase), so the stream
/// reads `kkt_analysis, phase.begin, rows..., phase.exit, phase.end`. See
/// `IpmKktAnalysisTraceEvent`.
struct IpmPhaseTraceEvent {
    /// The phase's 0-based index into `IpmResult::phases`, on
    /// the same reading as the `phase` field of the `ipm.iter` rows.
    Index phase = 0;
    /// The engine's own label for this phase, and the bytes the console
    /// prints: `"Optimization Algorithm "` or `"Solve Algorithm "`, WITH the
    /// trailing space. Borrowed from the phase-step list, valid for the
    /// duration of the call only.
    std::string_view label;
    /// Which phase this is -- `IpmOptions::phases[phase]`.
    IpmPhase entry = IpmPhase::kOptimize;
};

/// @brief One KKT-matrix analysis or re-initialization (schema
/// `ipm.kkt_analysis`).
///
/// ONE EVENT FOR BOTH HALVES of what the console prints: a
/// `Beginning: KKT-Matrix Analysis` line before the factorization and the size,
/// FLOPs and time lines plus `Finished` after it. Nothing can interleave
/// between them, so a single event emitted AFTER the analysis renders both
/// halves in the same order and the same bytes.
///
/// ONE PER ANALYSIS OR RE-INITIALIZATION, which is NOT one per phase that RAN:
/// the entry analysis runs before the first phase and the inter-phase one runs
/// at the end of a phase, ahead of the next phase's conditional-skip test. A
/// sequence whose second phase is skipped therefore carries TWO of these and
/// one phase bracket. The count identity is
/// `1 + #{phases that ran, were not the last step, and did not break}`.
///
/// `docompute` TELLS THE TWO CALLS APART: the entry analysis computes a fresh
/// factorization, an inter-phase re-initialization refactorizes the existing
/// one. The console prints the size and FLOPs lines only when `docompute`, and
/// the FLOPs line only when the count is positive -- both rules stay in the
/// console, not here.
struct IpmKktAnalysisTraceEvent {
    /// The KKT system's dimension. A JOIN KEY ONLY -- the console does not
    /// print it here (`ipm.solve.begin` carries the printed copy).
    Index kkt_dim = 0;
    /// Nonzeros in the assembled KKT matrix. A join key, as `kkt_dim` is.
    Index nnz = 0;
    /// The factor's memory figure, as `KktFactorization` reports it (an `int`
    /// there and on `IpmResult`; carried as `Index` and printed with the same
    /// integer format the old console used, so the bytes do not move).
    ///
    /// STALE WHEN `docompute` IS FALSE -- it is re-read from the last analysis,
    /// which is not this one's. The serializer writes it `null` there.
    Index factor_mem = 0;
    /// The factor's MFLOPs figure. Stale on `!docompute` exactly as
    /// `factor_mem` is, and `null` on the wire there.
    Index factor_flops = 0;
    /// True when this call computed a fresh factorization, false when it
    /// refactorized an existing one.
    bool docompute = false;
    /// Wall-clock SECONDS this analysis took (`IpmResult::pre_time`'s
    /// increment). INFORMATIONAL -- no pin reads it; the console multiplies by
    /// 1000 to print milliseconds, exactly as it does for `ipm.solve.end`.
    double analysis_time_s = 0.0;
};

/// @brief One phase's exit statistics (schema `ipm.phase.exit`).
///
/// THE REPORT IS EMBEDDED, NOT MIRRORED. `IpmPhaseReport` is what `solve()`
/// returns for this phase, and this event carries THAT OBJECT rather than a
/// second copy of its six fields -- so "one shape, no drift" holds by
/// construction and the pin is `event.report == result.phases[phase]` field for
/// field on a live solve.
///
/// EMITTED right after the phase's report is filled and before the phase's
/// `end` line, at the tail of the phase.
///
/// THE STATUS IS THE RESOLVED ONE (`report.status`): the status the phase
/// returns, not the raw exit code of its iteration loop. Resolution only ever
/// rewrites `kIterationLimit`, into `kStalled` or `kInterrupted`, and the console
/// prints `No Solution Found` for all three.
///
/// THE SELECTED ROW IS BORROWED, on `ipm.iter`'s own convention: the five
/// values the block prints are read off it and the record's other keys are not
/// repeated. It is the row the phase RETURNS -- the last one, or the best one
/// when the `return_best` substitution applied. `iterate.prim_obj` is printed
/// RAW: the restoration-contamination NaN rule that governs the iteration
/// CALLBACK's `f` does not apply to this block and never did.
///
/// THE ROW'S INDEX IN THE HISTORY IS NOT CARRIED, and there is nothing to carry:
/// the phase pushes exactly one row per iteration, so a row's index in the
/// history IS its `iter`. A `selected_iter`
/// key beside `iter` would be the same number under a second name.
/// The join to the selected row's `ipm.iter` line is (`phase`, `iter`).
struct IpmPhaseExitTraceEvent {
    /// This phase's report, the object `IpmResult::phases[phase]` holds. Valid
    /// for the duration of the call only.
    const IpmPhaseReport &report;
    /// The row this phase returns. Valid for the duration of the call only.
    const IpmIterate &iterate;
    /// The phase's 0-based index, as on `IpmPhaseTraceEvent`.
    Index phase = 0;
    /// True when `return_best` substituted the best iterate for the last one.
    bool best_substituted = false;
    /// The last non-Success factorization status observed during THIS PHASE.
    /// It resets once per phase, so this is a per-phase fact and not a per-call
    /// one. Named rather than raw.
    /// The console prints a line for it only when it is not `kSuccess`.
    IpmKktFactorStatus kkt_factor_status = IpmKktFactorStatus::kSuccess;
    /// The phase's own four clocks, in SECONDS, from the phase's internal
    /// timers -- the console multiplies by 1000 and divides by
    /// `report.iterations` for its `ms/iter` column. INFORMATIONAL.
    ///
    /// TWO CLOCKS, AND THEY ARE NOT THE SAME ONE: `total_s` is the phase's own
    /// timer (what the block prints as `Total Time`), while
    /// `report.phase_seconds` is a timer around the whole phase. They differ by
    /// the call's own overhead; neither is asserted anywhere.
    double total_s = 0.0;
    double func_s = 0.0;  ///< NLP function evaluation.
    double kkt_s = 0.0;   ///< KKT factor/solve.
    double print_s = 0.0; ///< Console print time.
};

/// @brief The trace hook: one sink, every event delivered through a virtual
/// method that is a no-op by default. A sink overrides the events it wants;
/// events added in later releases are ADDITIVE -- a sink written before one
/// exists keeps compiling and ignores it. `nullptr` is the off state every emit
/// site checks before emitting; the tier events and every message are built only
/// there, while `fallback.verdict` is the judge's own out-param and is filled
/// whether or not a sink is attached (a handful of scalars). The destructor is
/// out-of-line (src/trace/trace.cpp).
///
/// CONTRACT: a sink serves ONE solve at a time on ONE thread (a nested restoration
/// sub-solve shares it sequentially); events are not synchronized. A solve that throws
/// after `on_solve_begin` ends with `on_solve_abort()` instead of `on_solve_end`.
class TraceSink {
  public:
    virtual ~TraceSink();

    virtual void on_ipqp_iter(const IpqpIterTraceEvent &event);
    virtual void on_ipqp_reg(const IpqpRegTraceEvent &event);
    virtual void on_ipqp_restart(const IpqpRestartTraceEvent &event);
    virtual void on_ipqp_route(const IpqpRouteTraceEvent &event);
    virtual void on_ipqp_certify(const IpqpCertifyTraceEvent &event);
    virtual void on_ipqp_escape(const IpqpEscapeTraceEvent &event);
    virtual void on_qp_mode(const QpModeTraceEvent &event);
    virtual void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &event);

    /// The four records both engines share.
    virtual void on_solve_begin(const SolveBeginTraceEvent &event);
    virtual void on_iter(const IterTraceEvent &event);
    virtual void on_message(const MessageTraceEvent &event);
    virtual void on_solve_end(const SolveEndTraceEvent &event);

    /// @brief A solve whose `solve.begin` was delivered is leaving by an exception, so
    /// no `solve.end` will follow. NON-EMITTING: a sink unwinds whatever per-solve state
    /// `on_solve_begin` opened (nesting, header bookkeeping) and writes nothing,
    /// so the next solve on the same sink starts clean with no sink-specific recovery.
    /// Called once per solve level that began, innermost first, on every exceptional
    /// engine exit; must not throw. Default: nothing.
    virtual void on_solve_abort() noexcept;

    virtual void on_ipm_phase_begin(const IpmPhaseTraceEvent &event);
    virtual void on_ipm_phase_end(const IpmPhaseTraceEvent &event);
    virtual void on_ipm_kkt_analysis(const IpmKktAnalysisTraceEvent &event);
    virtual void on_ipm_phase_exit(const IpmPhaseExitTraceEvent &event);
};

} // namespace hven
