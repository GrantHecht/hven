// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// ipqp_trace.h -- the IPQP tier's machine-trace schema v0 (M6 W1 task 8, the
// W4 hook: docs/notes/2026-08-m6-w1-ipqp-spec.md section 7, :733-749). The
// SEVEN event structs and the sink interface the private emit sites call;
// W1 ships structs + call sites, W4 the JSON-lines serializer. Field names
// match the schema's JSON keys (`inertia:[np,nn,nz]` split into three named
// fields; `v`/`ev` are the serializer's envelope, not carried here). See the
// task's report for the `facts` field's undefined-by-spec status.

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
    Index inertia_pos = 0;
    Index inertia_neg = 0;
    Index inertia_zero = 0;
    bool zero_derived = false; ///< InertiaEvidence::zero_is_derived.
    bool perturbed = false;    ///< This iteration's accepted read reported a perturbed pivot.
    std::string facts;         ///< See this header's banner.
};

/// @brief One (rho, delta) schedule move (schema `ipqp.reg`).
struct IpqpTraceRegEvent {
    IpqpTraceRegDir dir = IpqpTraceRegDir::kUp;
    double rho = 0.0;
    double delta = 0.0;
    IpqpTraceRegReason reason = IpqpTraceRegReason::kInertia;
};

/// @brief The section 5 warm restart this solve started from (schema
/// `ipqp.restart`).
///
/// `shift_p`/`shift_d` BOTH carry `IpqpCounters::ipqp_restart_shift_max`: the
/// engine tracks one undifferentiated repair magnitude across every primal
/// and dual move the restart makes (`shift_max` in `warm_start_from`), not a
/// primal/dual split, and inventing one here would fabricate a distinction
/// the engine does not measure.
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

/// @brief One tier escape (schema `ipqp.escape`). The schema's single
/// `evidence` object is the union of the two evidence blocks `IpqpResult`
/// already carries -- reused rather than re-invented; `stall` is populated
/// iff `reason == kStall`, `infeasibility` iff `reason == kInfeasibleSuspect`.
struct IpqpTraceEscapeEvent {
    IpqpTraceEscapeReason reason = IpqpTraceEscapeReason::kBudget;
    IpqpStallEvidence stall;
    IpqpInfeasibilityEvidence infeasibility;
};

/// @brief The driver dispatch's own outcome for one subproblem (schema
/// `qp.mode`). Driver-emitted, in the kIpm arm only -- the one place a mode's
/// dispatch knows whether the subproblem was solved outright, routed onward,
/// or escaped.
struct QpModeTraceEvent {
    IpqpTraceQpMode mode = IpqpTraceQpMode::kIpqp;
    IpqpTraceOutcome outcome = IpqpTraceOutcome::kOptimal;
    std::string facts; ///< See this header's banner.
    Index iters = 0;
};

/// @brief The W4 hook: one sink, seven event methods, no default
/// implementation -- a concrete subclass (W4's JSON-lines writer, or a test
/// double) must answer all seven. `attach_trace(nullptr)` (the default on
/// both `IpqpEngine` and `SqpDriver`) is the off state every emit site checks
/// before building its argument struct, so an unattached sink costs one
/// pointer compare per event point.
class IpqpTraceSink {
  public:
    virtual ~IpqpTraceSink() = default;
    virtual void on_ipqp_iter(const IpqpTraceIterEvent &event) = 0;
    virtual void on_ipqp_reg(const IpqpTraceRegEvent &event) = 0;
    virtual void on_ipqp_restart(const IpqpTraceRestartEvent &event) = 0;
    virtual void on_ipqp_route(const IpqpTraceRouteEvent &event) = 0;
    virtual void on_ipqp_certify(const IpqpTraceCertifyEvent &event) = 0;
    virtual void on_ipqp_escape(const IpqpTraceEscapeEvent &event) = 0;
    virtual void on_qp_mode(const QpModeTraceEvent &event) = 0;
};

} // namespace hven::solvers
