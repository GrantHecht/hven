// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// ipqp_trace.h -- the IPQP tier's machine-trace schema v0 (spec section 7,
// :733-749), plus W2's driver-side `fallback.verdict`. Eight event
// structs + sink interface; `v`/`ev` are the serializer's envelope, not
// carried on any struct here.

#include <array>
#include <optional>
#include <string>

#include <hven/core/types.h>
#include <hven/detail/qp/ipqp_engine.h>

namespace hven::solvers {

enum class IpqpTraceRegDir { kDown, kUp };
enum class IpqpTraceRegReason { kAccept, kInertia, kStall, kFloor };
enum class IpqpTraceRestartGrade { kCold, kBase, kFull };
enum class IpqpTraceRouteTo { kRefine, kSsn, kWalk };
enum class IpqpTraceFinalInertia { kOk, kWrong, kUnreadable };
enum class IpqpTraceEscapeReason { kBudget, kStall, kIndefinite, kNumerical, kInfeasibleSuspect };
enum class IpqpTraceQpMode { kIpqp };
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

/// @brief The driver dispatch's outcome for one subproblem (schema
/// `qp.mode`), driver-emitted in the kIpm arm only.
struct QpModeTraceEvent {
    IpqpTraceQpMode mode = IpqpTraceQpMode::kIpqp;
    IpqpTraceOutcome outcome = IpqpTraceOutcome::kOptimal;
    std::string facts; ///< Opaque, undefined by spec v0; always empty from W1.
    Index iters = 0;
};

/// @brief The certified fallback's own outcome for one escaped subproblem (schema
/// `fallback.verdict`, M6 W2 T5), driver-emitted in the kIpm arm beside `ipqp.escape`. The four
/// values are `SqpCounters`' four-way partition: kDisproved and kRungB carry counters of their
/// own, kRelaxed and kExhausted are the rung-A-owned pair told apart by the returned status.
enum class SqpFallbackVerdict { kDisproved, kRelaxed, kExhausted, kRungB };

/// @brief One entry into `certified_feasibility_fallback` (schema `fallback.verdict`).
/// An entry whose evidence never FIRED reports `entered_rung_a == false` with `verdict ==
/// kRungB`: it ran W1's cold walk directly and charges no counter in the partition.
struct SqpFallbackVerdictTraceEvent {
    bool entered_rung_a = false;
    SqpFallbackVerdict verdict = SqpFallbackVerdict::kRungB;
    /// The first rung's penalty, of the ladder whose outcome `verdict` reports (the RETRY's when
    /// `floor_retry`). Absent -- not zero-filled -- when no rung A was entered.
    std::optional<double> rho_0;
    bool rho0_ceiling_hit = false; ///< That ladder's placement was CLAMPED (headroom or dual_mu).
    bool floor_retry = false;      ///< A declined rung A above the floor was re-run once at it.
    Index qp_minor_iters = 0;      ///< That ladder's own stopping rung's counts; 0 with no rung A.
    Index qp_factorizations = 0;
};

/// @brief The W4 hook: one sink, eight pure-virtual methods. `nullptr` is
/// the off state every emit site checks before building its argument
/// struct. Destructor out-of-line (ipqp_trace.cpp, CLAUDE.md section 5).
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
};

} // namespace hven::solvers
