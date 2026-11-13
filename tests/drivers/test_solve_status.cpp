// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The shared status vocabulary: the severity order, the interior-point PHASE
// RESOLUTION with its stop-reason split, and the SQP mapping. The LIVE pins on
// the stop reason itself are in tests/interior/ -- they need a solve.
//
// M6 W5 T8.4: the engine's own ConvergenceFlags is gone, so what used to be a
// MAPPING from that enum onto SolveStatus is now a RESOLUTION of one SolveStatus
// against a stop reason -- resolve_ipm_phase_status, applied per phase. The
// rules it pins are the same rules, one enum lighter.

#include <gtest/gtest.h>

#include <set>
#include <stdexcept>
#include <string>

#include <hven/core/solver_counters.h>
#include <hven/detail/drivers/ipm_phase_status.h>

using hven::severity;
using hven::SolveStatus;
using hven::StopReason;
using hven::to_string;
using hven::detail::IpmPhaseStop;
using hven::detail::resolve_ipm_phase_status;
using hven::detail::to_stop_reason;

namespace {

// Eight of the ten statuses in the documented reporting order, weakest first.
const SolveStatus kSeverityOrder[] = {SolveStatus::kOptimal,     SolveStatus::kAcceptable,
                                      SolveStatus::kInterrupted, SolveStatus::kIterationLimit,
                                      SolveStatus::kStalled,     SolveStatus::kInfeasible,
                                      SolveStatus::kDiverging,   SolveStatus::kNumericalError};

} // namespace

TEST(SolveStatus, SeverityIsTotalAndOrderedAsDocumented) {
    for (int i = 1; i < 8; ++i) {
        SCOPED_TRACE(to_string(kSeverityOrder[i]));
        EXPECT_LT(severity(kSeverityOrder[i - 1]), severity(kSeverityOrder[i]));
    }
    // Total, not merely monotone on this list: eight distinct ranks over eight
    // distinct statuses, and every name distinct too.
    std::set<int> ranks;
    std::set<std::string> names;
    for (const SolveStatus s : kSeverityOrder) {
        ranks.insert(severity(s));
        names.insert(to_string(s));
    }
    EXPECT_EQ(ranks.size(), 8u);
    EXPECT_EQ(names.size(), 8u);
}

TEST(SolveStatus, IpmPhaseResolutionSplitsTheCapExitByStopReason) {
    EXPECT_EQ(resolve_ipm_phase_status(SolveStatus::kIterationLimit, IpmPhaseStop::kIterationLimit),
              SolveStatus::kIterationLimit);
    EXPECT_EQ(resolve_ipm_phase_status(SolveStatus::kIterationLimit, IpmPhaseStop::kStageStalled),
              SolveStatus::kStalled);
    EXPECT_EQ(resolve_ipm_phase_status(SolveStatus::kIterationLimit,
                                       IpmPhaseStop::kRestorationLocallyInfeasible),
              SolveStatus::kStalled);
    EXPECT_EQ(resolve_ipm_phase_status(SolveStatus::kIterationLimit, IpmPhaseStop::kNone),
              SolveStatus::kIterationLimit);
    // The stronger verdict survives the stall: the mapping reads the flag first.
    // This is the rule the live pins cannot reach -- ACCEPTABLE needs fifty
    // consecutive acceptable iterates coinciding with a stall.
    EXPECT_EQ(resolve_ipm_phase_status(SolveStatus::kAcceptable, IpmPhaseStop::kStageStalled),
              SolveStatus::kAcceptable);
    EXPECT_EQ(resolve_ipm_phase_status(SolveStatus::kOptimal, IpmPhaseStop::kStageStalled),
              SolveStatus::kOptimal);
    EXPECT_EQ(resolve_ipm_phase_status(SolveStatus::kNumericalError, IpmPhaseStop::kNone),
              SolveStatus::kNumericalError);
    EXPECT_EQ(resolve_ipm_phase_status(SolveStatus::kDiverging, IpmPhaseStop::kNone),
              SolveStatus::kDiverging);
    // Reachability, stated: the interior-point engine reports neither of these.
    for (const SolveStatus f :
         {SolveStatus::kOptimal, SolveStatus::kAcceptable, SolveStatus::kIterationLimit,
          SolveStatus::kDiverging, SolveStatus::kNumericalError}) {
        for (const IpmPhaseStop r :
             {IpmPhaseStop::kNone, IpmPhaseStop::kIterationLimit,
              IpmPhaseStop::kRestorationLocallyInfeasible, IpmPhaseStop::kStageStalled}) {
            const SolveStatus s = resolve_ipm_phase_status(f, r);
            EXPECT_NE(s, SolveStatus::kInfeasible);
            EXPECT_NE(s, SolveStatus::kInterrupted);
        }
    }
}

// THE INTERIOR-POINT ENGINE'S PHASE STOP ON THE PUBLIC VOCABULARY (M8 AP-5 G1):
// the three limit doors fold into kNone (the status names them); every other
// value maps to its namesake, both stops apart.
TEST(SolveStatus, TheIpmPhaseStopMapsOntoStopReason) {
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kNone), StopReason::kNone);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kIterationLimit), StopReason::kNone);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kWorkLimit), StopReason::kNone);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kTimeLimit), StopReason::kNone);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kCallbackStop), StopReason::kCallbackStop);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kStopRequested), StopReason::kStopRequested);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kStageStalled), StopReason::kStageStalled);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kRestorationLocallyInfeasible),
              StopReason::kRestorationLocallyInfeasible);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kNonFiniteStart), StopReason::kNonFiniteStart);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kNonFiniteIterate), StopReason::kNonFiniteIterate);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kEvaluationFailure), StopReason::kEvaluationFailure);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kException), StopReason::kException);
    EXPECT_THROW((void)to_stop_reason(static_cast<IpmPhaseStop>(99)), std::invalid_argument);
    // Both stops resolve the cap verdict to kInterrupted.
    EXPECT_EQ(resolve_ipm_phase_status(SolveStatus::kIterationLimit, IpmPhaseStop::kCallbackStop),
              SolveStatus::kInterrupted);
    EXPECT_EQ(resolve_ipm_phase_status(SolveStatus::kIterationLimit, IpmPhaseStop::kStopRequested),
              SolveStatus::kInterrupted);
    EXPECT_EQ(resolve_ipm_phase_status(SolveStatus::kOptimal, IpmPhaseStop::kStopRequested),
              SolveStatus::kOptimal);
}

// THE SQP MAPPING IS GONE with the enum it mapped from (M6 W5 T8.4): the SQP
// engine reports SolveStatus directly, and the five values it can report are
// the first five enumerators under the same names. What is left to pin is the
// vocabulary itself, which is the CSV column and the printed status.
TEST(SolveStatus, TheDisplayNamesArePinnedLiterals) {
    // Every spelling, not a sample: these strings are the CSV
    // column and the printed status, so each one is a pinned literal.
    EXPECT_STREQ(to_string(SolveStatus::kOptimal), "optimal");
    EXPECT_STREQ(to_string(SolveStatus::kAcceptable), "acceptable");
    EXPECT_STREQ(to_string(SolveStatus::kIterationLimit), "max_iter");
    EXPECT_STREQ(to_string(SolveStatus::kInfeasible), "infeasible");
    EXPECT_STREQ(to_string(SolveStatus::kStalled), "stalled");
    EXPECT_STREQ(to_string(SolveStatus::kDiverging), "diverging");
    EXPECT_STREQ(to_string(SolveStatus::kNumericalError), "numerical_error");
    EXPECT_STREQ(to_string(SolveStatus::kInterrupted), "interrupted");
    // THE ONE STOP-REASON VOCABULARY (M8 AP-5 G1), every spelling: the trace's
    // `stop_reason` key and the interior corpus leg's column.
    EXPECT_STREQ(to_string(StopReason::kNone), "none");
    EXPECT_STREQ(to_string(StopReason::kCallbackStop), "callback_stop");
    EXPECT_STREQ(to_string(StopReason::kStopRequested), "stop_requested");
    EXPECT_STREQ(to_string(StopReason::kException), "exception");
    EXPECT_STREQ(to_string(StopReason::kNonFiniteStart), "non_finite_start");
    EXPECT_STREQ(to_string(StopReason::kNonFiniteIterate), "non_finite_iterate");
    EXPECT_STREQ(to_string(StopReason::kEvaluationFailure), "evaluation_failure");
    EXPECT_STREQ(to_string(StopReason::kStageStalled), "stage_stalled");
    EXPECT_STREQ(to_string(StopReason::kRestorationLocallyInfeasible),
                 "restoration_locally_infeasible");
    EXPECT_STREQ(to_string(StopReason::kSubproblemFailure), "subproblem_failure");
    EXPECT_STREQ(to_string(StopReason::kInconsistentEqualities), "inconsistent_equalities");
    EXPECT_STREQ(to_string(StopReason::kRestorationRefused), "restoration_refused");
    EXPECT_STREQ(to_string(StopReason::kRestorationNonFinite), "restoration_non_finite");
    EXPECT_STREQ(to_string(StopReason::kRestorationNoClaim), "restoration_no_claim");
    EXPECT_STREQ(to_string(StopReason::kRestorationFailed), "restoration_failed");
    // Out-of-range values are refused, not silently mapped: the CSV column and
    // the printed status come from these switches. Every switch in the TU has
    // its boundary here -- both to_string families, severity, and
    // resolve_ipm_phase_status's reason switch, which only the cap verdict
    // reaches.
    EXPECT_THROW((void)to_string(static_cast<SolveStatus>(99)), std::invalid_argument);
    EXPECT_THROW((void)to_string(static_cast<StopReason>(99)), std::invalid_argument);
    EXPECT_THROW((void)severity(static_cast<SolveStatus>(99)), std::invalid_argument);
    // An out-of-range VERDICT is now passed through untouched rather than
    // refused: resolve_ipm_phase_status reinterprets one verdict (the cap exit)
    // and returns every other unchanged, so it has no switch over the verdict
    // to guard. to_string() and severity() above are where an out-of-range
    // status is refused.
    EXPECT_EQ(resolve_ipm_phase_status(static_cast<SolveStatus>(99), IpmPhaseStop::kNone),
              static_cast<SolveStatus>(99));
    EXPECT_THROW(
        (void)resolve_ipm_phase_status(SolveStatus::kIterationLimit, static_cast<IpmPhaseStop>(99)),
        std::invalid_argument);
    // An out-of-range reason under a STRONGER verdict is ignored, not refused:
    // that switch never reaches the reason at all.
    EXPECT_EQ(resolve_ipm_phase_status(SolveStatus::kOptimal, static_cast<IpmPhaseStop>(99)),
              SolveStatus::kOptimal);
}

// M7 W2 M3a fix1. THE COUNTER'S OWN STATUS VOCABULARY: `RestorationSubStatus`,
// which is `SolveStatus` plus the `kNotRun` that a restoration record needs and
// `SolveStatus` has no value to spare for. Two properties are the whole point
// of it and neither is visible from a solve, so they are pinned here:
//
//   * the map into it is TOTAL -- every one of the statuses a sub-solve
//     could return has an image, so the writer needs no default arm and cannot
//     mistranslate one;
//   * the spellings AGREE, name for name, with `to_string(SolveStatus)`. A
//     consumer compares `counters.restoration_sub_status` against the
//     `sqp.solve.end` line's own `status` key without a translation table, and
//     a drift between the two tables would break that silently.
TEST(RestorationSubStatus, TheMapIsTotalAndTheSpellingsAreSolveStatusSPlusNotRun) {
    using hven::RestorationSubStatus;
    using hven::to_restoration_sub_status;

    // TOTAL, and spelling-preserving on every one of the eight. The severity
    // list above IS the eight, so this loop is exhaustive by construction.
    std::set<std::string> names;
    std::set<int> codes;
    for (const SolveStatus s : kSeverityOrder) {
        SCOPED_TRACE(to_string(s));
        const RestorationSubStatus sub = to_restoration_sub_status(s);
        EXPECT_STREQ(to_string(sub), to_string(s));
        // And no status maps onto the not-run value: a caller of this map is
        // holding a status, so a sub-solve ran.
        EXPECT_NE(sub, RestorationSubStatus::kNotRun);
        names.insert(to_string(sub));
        codes.insert(static_cast<int>(sub));
    }
    EXPECT_EQ(names.size(), 8u) << "the map must be INJECTIVE: eight statuses, eight images";
    EXPECT_EQ(codes.size(), 8u);

    // THE TENTH VALUE, which is the reason the enum exists.
    EXPECT_STREQ(to_string(RestorationSubStatus::kNotRun), "not_run");
    EXPECT_EQ(static_cast<int>(RestorationSubStatus::kNotRun), 0)
        << "kNotRun must be the ZERO: it is what a default-constructed SqpCounters holds";
    EXPECT_EQ(names.count("not_run"), 0u) << "no status may spell itself not_run";

    // The two switches' boundaries, on this TU's own rule above.
    EXPECT_THROW((void)to_string(static_cast<RestorationSubStatus>(99)), std::invalid_argument);
    EXPECT_THROW((void)to_restoration_sub_status(static_cast<SolveStatus>(99)),
                 std::invalid_argument);

    // AND THE DEFAULT IS THE NOT-RUN VALUE where it matters: on the counters
    // aggregate itself. A fresh SqpCounters has run no restoration.
    EXPECT_EQ(hven::SqpCounters{}.restoration_sub_status, RestorationSubStatus::kNotRun);
}
