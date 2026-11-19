// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// trace_names.h -- the schema's lower-snake spelling of each trace enum. Shared by the
// JSON writer and the console sink so a spelling has one definition.

#include <hven/core/solver_status.h>
#include <hven/ipm/ipm_options.h>
#include <hven/sqp/sqp_options.h>
#include <hven/trace/trace.h>

namespace hven::detail {

// --- rule 8: the enum strings, verbatim from spec section 7 ---------------
//
// No `default` label anywhere below, so `-Wswitch` WARNS when an enumerator is
// added without a spelling. It does NOT fail the build -- this tree carries no
// `-Werror` -- so the trailing `"unknown"` is REACHABLE.
//
// The net is in the tests: `-Wswitch` is an ERROR there, over one exhaustive
// switch per enum, and no emitted line may contain `"unknown"`
// (tests/trace/test_json_lines_trace_sink.cpp).
inline const char *to_json(IpqpTraceRegDirection v) {
    switch (v) {
    case IpqpTraceRegDirection::kDown:
        return "down";
    case IpqpTraceRegDirection::kUp:
        return "up";
    }
    return "unknown";
}

inline const char *to_json(IpqpTraceRegReason v) {
    switch (v) {
    case IpqpTraceRegReason::kAccept:
        return "accept";
    case IpqpTraceRegReason::kInertia:
        return "inertia";
    case IpqpTraceRegReason::kStall:
        return "stall";
    case IpqpTraceRegReason::kFloor:
        return "floor";
    }
    return "unknown";
}

inline const char *to_json(IpqpTraceRestartGrade v) {
    switch (v) {
    case IpqpTraceRestartGrade::kCold:
        return "cold";
    case IpqpTraceRestartGrade::kBase:
        return "base";
    case IpqpTraceRestartGrade::kFull:
        return "full";
    }
    return "unknown";
}

inline const char *to_json(IpqpTraceRouteTarget v) {
    switch (v) {
    case IpqpTraceRouteTarget::kRefine:
        return "refine";
    case IpqpTraceRouteTarget::kSsn:
        return "ssn";
    case IpqpTraceRouteTarget::kWalk:
        return "walk";
    }
    return "unknown";
}

inline const char *to_json(IpqpTraceFinalInertia v) {
    switch (v) {
    case IpqpTraceFinalInertia::kOk:
        return "ok";
    case IpqpTraceFinalInertia::kWrong:
        return "wrong";
    case IpqpTraceFinalInertia::kUnreadable:
        return "unreadable";
    }
    return "unknown";
}

inline const char *to_json(IpqpTraceEscapeReason v) {
    switch (v) {
    case IpqpTraceEscapeReason::kBudget:
        return "budget";
    case IpqpTraceEscapeReason::kStall:
        return "stall";
    case IpqpTraceEscapeReason::kIndefinite:
        return "indefinite";
    case IpqpTraceEscapeReason::kNumerical:
        return "numerical";
    case IpqpTraceEscapeReason::kInfeasibleSuspect:
        return "infeasible_suspect";
    }
    return "unknown";
}

inline const char *to_json(QpTraceMode v) {
    switch (v) {
    case QpTraceMode::kIpqp:
        return "ipqp";
    case QpTraceMode::kWalk:
        return "walk";
    case QpTraceMode::kSsn:
        return "ssn";
    }
    return "unknown";
}

inline const char *to_json(QpTraceModeSite v) {
    switch (v) {
    case QpTraceModeSite::kDispatch:
        return "dispatch";
    case QpTraceModeSite::kSsnWarmGrade:
        return "ssn_warm_grade";
    case QpTraceModeSite::kFallbackRungB:
        return "fallback_rung_b";
    case QpTraceModeSite::kElasticRung:
        return "elastic_rung";
    case QpTraceModeSite::kSocResolve:
        return "soc_resolve";
    case QpTraceModeSite::kCapRetry:
        return "cap_retry";
    }
    return "unknown";
}

// Two DRIVER enums the `sqp.major` row carries. Lower-case snake per plan
// section 2 rule 8, not the PascalCase display strings core/ has -- those name
// a column in a printed table, and this is machine text.
inline const char *to_json(QpStatus v) {
    switch (v) {
    case QpStatus::kOptimal:
        return "optimal";
    case QpStatus::kIterationLimit:
        return "max_iter";
    case QpStatus::kInfeasible:
        return "infeasible";
    case QpStatus::kNumericalError:
        return "numerical_error";
    }
    return "unknown";
}

inline const char *to_json(StepVerdict v) {
    switch (v) {
    case StepVerdict::kAcceptObjectiveStep:
        return "accept_f";
    case StepVerdict::kAcceptFeasibilityStep:
        return "accept_h";
    case StepVerdict::kReject:
        return "reject";
    case StepVerdict::kRestore:
        return "restore";
    }
    return "unknown";
}

inline const char *to_json(QpTraceOutcome v) {
    switch (v) {
    case QpTraceOutcome::kOptimal:
        return "optimal";
    case QpTraceOutcome::kRouted:
        return "routed";
    case QpTraceOutcome::kEscaped:
        return "escaped";
    }
    return "unknown";
}

inline const char *to_json(SqpTraceFallbackVerdict v) {
    switch (v) {
    case SqpTraceFallbackVerdict::kDisproved:
        return "disproved";
    case SqpTraceFallbackVerdict::kRelaxed:
        return "relaxed";
    case SqpTraceFallbackVerdict::kExhausted:
        return "exhausted";
    case SqpTraceFallbackVerdict::kRungB:
        return "rung_b";
    case SqpTraceFallbackVerdict::kUnfired:
        return "unfired";
    }
    return "unknown";
}

// The SQP end event's own to_json(SqpStatus) stood here until M6 W5 T8.4. Its
// five spellings were BYTE-IDENTICAL to to_string(SolveStatus)'s five --
// optimal / max_iter / infeasible / numerical_error / budget_exhausted -- so
// folding it into the one overload below moves no trace byte and the golden
// rig's traces do not move.

// LOWER SNAKE, NOT core's display `to_string` (fix round 1, R2): plan section 2
// rule 8 governs the schema's alphabet, and `to_string` names a printed column.
inline const char *to_json(StartLevel v) {
    switch (v) {
    case StartLevel::kCold:
        return "cold";
    case StartLevel::kSeeded:
        return "seeded";
    case StartLevel::kWarm:
        return "warm";
    case StartLevel::kHot:
        return "hot";
    }
    return "unknown";
}

inline const char *to_json(QpWorkingSetAlgebra v) {
    switch (v) {
    case QpWorkingSetAlgebra::kRefactorize:
        return "refactorize";
    case QpWorkingSetAlgebra::kSchurBorder:
        return "schur_border";
    }
    return "unknown";
}

// THE INTERIOR-POINT DRIVER'S OWN THREE (W4 T4). The exit status is
// `SolveStatus` since M6 W5 T8.4 -- the engine's own `ConvergenceFlags` is gone
// -- and the two mode selectors shape the run `ipm.solve.begin` announces.
//
// THE SPELLINGS MOVED WITH THE ENUM, and that is a DECLARED change (T8.4):
// `converged` -> `optimal`, `not_converged` -> `max_iter` (or `stalled` at the
// stall and locally-infeasible-restoration exits, which the old vocabulary
// could not tell apart at all), `singular_kkt` -> `numerical_error`;
// `acceptable` and `diverging` are unchanged. Nothing pinned compares these
// bytes -- there is no committed interior-point trace baseline and no control
// -- so unlike the corpus CSV's status column (which keeps its capitalised
// spellings through a bench-local table for exactly that reason) this one takes
// the new vocabulary now. The SQP's `to_json(SolveStatus)` above already spelled
// its five the same way `to_string(SolveStatus)` does, so the golden rig's
// traces do not move.
inline const char *to_json(SolveStatus v) { return to_string(v); }

// AND THE COUNTER'S OWN TEN (M7 W2 M3a fix1), forwarding for the same reason
// the three above do: `to_string(RestorationSubStatus)` already spells them in
// the schema's lower snake, and it spells its nine status values EXACTLY as
// `to_string(SolveStatus)` spells the same names, so a second table here could
// only drift the two apart.
inline const char *to_json(RestorationSubStatus v) { return to_string(v); }

inline const char *to_json(IpmInertiaMode v) {
    switch (v) {
    case IpmInertiaMode::kClassic:
        return "classic";
    case IpmInertiaMode::kProximalRegularization:
        return "proximal_regularization";
    }
    return "unknown";
}

inline const char *to_json(IpmRestorationMode v) {
    switch (v) {
    case IpmRestorationMode::kOff:
        return "off";
    case IpmRestorationMode::kProximalSwitch:
        return "proximal_switch";
    case IpmRestorationMode::kL1Nested:
        return "l1_nested";
    }
    return "unknown";
}

// M6 W5 T8.7b'S FOUR. Three of them FORWARD to core's `to_string` rather than
// keeping a second table: unlike `to_string(StartLevel)` -- which names a
// printed COLUMN and is PascalCase -- these three already spell their values in
// the schema's own lower snake, so a second copy could only drift. `IpmPhase`
// is the one that needs a table here: it has no `to_string` anywhere.
inline const char *to_json(MessageKind v) { return to_string(v); }

inline const char *to_json(MessageSeverity v) {
    return v == MessageSeverity::kNotice ? "notice" : "warning";
}

inline const char *to_json(IterationTier v) {
    switch (v) {
    case IterationTier::kBarrier:
        return "bar";
    case IterationTier::kIpqp:
        return "ipqp";
    case IterationTier::kSsn:
        return "ssn";
    case IterationTier::kWalk:
        return "walk";
    case IterationTier::kElastic:
        return "elastic";
    case IterationTier::kRestoration:
        return "rest";
    }
    return "unknown";
}

inline const char *to_json(SqpHessianMode v) {
    switch (v) {
    case SqpHessianMode::kExact:
        return "exact";
    case SqpHessianMode::kLbfgsDamped:
        return "lbfgs_damped";
    }
    return "unknown";
}

inline const char *to_json(SqpColumnScaling v) {
    switch (v) {
    case SqpColumnScaling::kOff:
        return "off";
    case SqpColumnScaling::kAlways:
        return "always";
    case SqpColumnScaling::kAuto:
        return "auto";
    }
    return "unknown";
}

inline const char *to_json(IpmKktFactorStatus v) { return to_string(v); }

inline const char *to_json(StopReason v) { return to_string(v); }

inline const char *to_json(IpmPhase v) {
    switch (v) {
    case IpmPhase::kOptimize:
        return "optimize";
    case IpmPhase::kSolve:
        return "solve";
    }
    return "unknown";
}

} // namespace hven::detail
