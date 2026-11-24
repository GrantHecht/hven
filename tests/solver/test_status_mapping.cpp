// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The status vocabulary's mappings, asserted as tables:
//
//   * detail::resolve_ipm_phase_status / to_stop_reason: every (raw verdict, recorded phase stop)
//     pair the interior-point engine can hand them, and the refusal of a value outside the
//     enumerators.
//   * detail::ipm_result_message / sqp_result_message: the one-line SolveResult::message of every
//     status, with the stop reason and options that select its wording. The message is
//     informational (nothing parses it) but it is the line a caller reads, so each distinct
//     wording is pinned here and the distinct causes are checked to stay distinct.
//   * detail::describe_exception: the three shapes of escaped exception.

#include <exception>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/ipm/ipm_phase_status.h>
#include <hven/detail/solver/result_message.h>
#include <hven/ipm/ipm_options.h>
#include <hven/solver/solve_result.h>
#include <hven/sqp/sqp_options.h>
#include <hven/sqp/sqp_solver_types.h>

namespace {

using hven::IpmOptions;
using hven::SolveBudget;
using hven::SolveResult;
using hven::SolveStatus;
using hven::SqpOptions;
using hven::SqpResult;
using hven::StopReason;
using hven::detail::IpmPhaseStop;

// ---------------------------------------------------------------------------
// resolve_ipm_phase_status / to_stop_reason
// ---------------------------------------------------------------------------

TEST(StatusMapping, AHeldVerdictOutranksEveryRecordedStop) {
    const IpmPhaseStop stops[] = {
        IpmPhaseStop::kNone,           IpmPhaseStop::kIterationLimit,
        IpmPhaseStop::kRestorationLocallyInfeasible, IpmPhaseStop::kStageStalled,
        IpmPhaseStop::kCallbackStop,   IpmPhaseStop::kStopRequested,
        IpmPhaseStop::kWorkLimit,      IpmPhaseStop::kTimeLimit,
        IpmPhaseStop::kNonFiniteStart, IpmPhaseStop::kNonFiniteIterate,
        IpmPhaseStop::kEvaluationFailure, IpmPhaseStop::kException};
    for (SolveStatus held : {SolveStatus::kOptimal, SolveStatus::kAcceptable,
                             SolveStatus::kNumericalError, SolveStatus::kStalled}) {
        for (IpmPhaseStop stop : stops) {
            EXPECT_EQ(hven::detail::resolve_ipm_phase_status(held, stop), held)
                << "a verdict other than kIterationLimit is never reinterpreted";
        }
    }
}

TEST(StatusMapping, TheCapVerdictIsRenamedByTheRecordedStop) {
    using hven::detail::resolve_ipm_phase_status;
    const std::vector<std::pair<IpmPhaseStop, SolveStatus>> table = {
        {IpmPhaseStop::kCallbackStop, SolveStatus::kInterrupted},
        {IpmPhaseStop::kStopRequested, SolveStatus::kInterrupted},
        {IpmPhaseStop::kStageStalled, SolveStatus::kStalled},
        {IpmPhaseStop::kRestorationLocallyInfeasible, SolveStatus::kInfeasible},
        {IpmPhaseStop::kWorkLimit, SolveStatus::kWorkLimit},
        {IpmPhaseStop::kTimeLimit, SolveStatus::kTimeLimit},
        {IpmPhaseStop::kNonFiniteStart, SolveStatus::kNumericalError},
        {IpmPhaseStop::kNonFiniteIterate, SolveStatus::kNumericalError},
        {IpmPhaseStop::kEvaluationFailure, SolveStatus::kNumericalError},
        {IpmPhaseStop::kException, SolveStatus::kInterrupted},
        {IpmPhaseStop::kIterationLimit, SolveStatus::kIterationLimit},
        {IpmPhaseStop::kNone, SolveStatus::kIterationLimit},
    };
    for (const auto &[stop, expected] : table) {
        EXPECT_EQ(resolve_ipm_phase_status(SolveStatus::kIterationLimit, stop), expected)
            << "stop " << static_cast<int>(stop);
        // Idempotent: resolving a resolved status again changes nothing.
        EXPECT_EQ(resolve_ipm_phase_status(expected, stop), expected);
    }
}

TEST(StatusMapping, AnUnrecognizedPhaseStopIsRefusedByBothMappings) {
    const auto bad = static_cast<IpmPhaseStop>(99);
    try {
        (void)hven::detail::resolve_ipm_phase_status(SolveStatus::kIterationLimit, bad);
        FAIL() << "an unrecognized IpmPhaseStop must not resolve";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("unrecognized IpmPhaseStop (99)"), std::string::npos)
            << e.what();
    }
    try {
        (void)hven::detail::to_stop_reason(bad, SolveStatus::kIterationLimit);
        FAIL() << "an unrecognized IpmPhaseStop must not map";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("unrecognized IpmPhaseStop (99)"), std::string::npos)
            << e.what();
    }
    // A converged verdict never reads the stop, so even a bad one maps to kNone there.
    EXPECT_EQ(hven::detail::to_stop_reason(bad, SolveStatus::kOptimal), StopReason::kNone);
}

TEST(StatusMapping, TheStopReasonMapNamesEveryCauseAndFoldsTheThreeLimitDoors) {
    using hven::detail::to_stop_reason;
    const SolveStatus nc = SolveStatus::kStalled; // any non-converged verdict
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kNone, nc), StopReason::kNone);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kIterationLimit, nc), StopReason::kNone);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kWorkLimit, nc), StopReason::kNone);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kTimeLimit, nc), StopReason::kNone);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kRestorationLocallyInfeasible, nc),
              StopReason::kRestorationLocallyInfeasible);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kStageStalled, nc), StopReason::kStageStalled);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kCallbackStop, nc), StopReason::kCallbackStop);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kStopRequested, nc), StopReason::kStopRequested);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kNonFiniteStart, nc), StopReason::kNonFiniteStart);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kNonFiniteIterate, nc), StopReason::kNonFiniteIterate);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kEvaluationFailure, nc), StopReason::kEvaluationFailure);
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kException, nc), StopReason::kException);
    // A converged verdict names its own cause whatever stop sat beside it.
    EXPECT_EQ(to_stop_reason(IpmPhaseStop::kStageStalled, SolveStatus::kAcceptable),
              StopReason::kNone);
}

// ---------------------------------------------------------------------------
// result messages
// ---------------------------------------------------------------------------

std::string ipm_msg(SolveStatus s, StopReason r = StopReason::kNone, const std::string &detail = "",
                    const IpmOptions &o = IpmOptions{}, const SolveBudget &b = SolveBudget{},
                    bool best = false) {
    SolveResult res;
    res.status = s;
    res.returned_best_iterate = best;
    return hven::detail::ipm_result_message(res, r, detail, o, b);
}

std::string sqp_msg(SolveStatus s, StopReason r = StopReason::kNone, const std::string &detail = "",
                    const SqpOptions &o = SqpOptions{}, const SolveBudget &b = SolveBudget{},
                    bool best = false, bool certified = false, long probe_stops = 0) {
    SqpResult res;
    res.status = s;
    res.returned_best_iterate = best;
    res.infeasibility_certified = certified;
    res.counters.probe_budget_stops = probe_stops;
    return hven::detail::sqp_result_message(res, r, detail, o, b);
}

bool has(const std::string &hay, const char *needle) {
    return hay.find(needle) != std::string::npos;
}

TEST(StatusMapping, IpmMessageForEveryStatus) {
    EXPECT_EQ(ipm_msg(SolveStatus::kOptimal), "converged: optimality and feasibility tolerances met");
    EXPECT_EQ(ipm_msg(SolveStatus::kAcceptable),
              "converged to an acceptable level: the acceptable tolerances were met");
    EXPECT_EQ(ipm_msg(SolveStatus::kAcceptable, StopReason::kNone, "no numerics left"),
              "converged to an acceptable level: the acceptable tolerances were met; no numerics "
              "left");
    EXPECT_EQ(ipm_msg(SolveStatus::kInfeasible), "locally infeasible");
    EXPECT_TRUE(has(ipm_msg(SolveStatus::kInfeasible, StopReason::kRestorationLocallyInfeasible),
                    "uncertified"));
    EXPECT_TRUE(has(ipm_msg(SolveStatus::kStalled), "stalled"));
    EXPECT_TRUE(has(ipm_msg(SolveStatus::kDiverging), "diverging"));
    EXPECT_EQ(ipm_msg(SolveStatus::kNumericalError), "numerical error");
    EXPECT_EQ(ipm_msg(SolveStatus::kNumericalError, StopReason::kNone, "inertia ladder"),
              "numerical error: inertia ladder");
    EXPECT_EQ(ipm_msg(SolveStatus::kNumericalError, StopReason::kNonFiniteStart, "grad[3] is NaN"),
              "numerical error: grad[3] is NaN");
    EXPECT_EQ(ipm_msg(SolveStatus::kNumericalError, StopReason::kEvaluationFailure), "numerical error");
    // A reason that carries no detail rule keeps the bare label whatever detail is passed.
    EXPECT_EQ(ipm_msg(SolveStatus::kNumericalError, StopReason::kSubproblemFailure, "ignored"),
              "numerical error");
}

TEST(StatusMapping, IpmMessagesQuoteTheLimitsTheSolveRanUnder) {
    IpmOptions o;
    o.common.max_iterations = 25;
    EXPECT_EQ(ipm_msg(SolveStatus::kIterationLimit, StopReason::kNone, "", o),
              "iteration limit reached (max_iterations = 25)");
    // A per-call budget can only tighten the cap.
    SolveBudget tight;
    tight.max_iterations = 7;
    EXPECT_EQ(ipm_msg(SolveStatus::kIterationLimit, StopReason::kNone, "", o, tight),
              "iteration limit reached (max_iterations = 7)");
    SolveBudget loose;
    loose.max_iterations = 1000;
    EXPECT_EQ(ipm_msg(SolveStatus::kIterationLimit, StopReason::kNone, "", o, loose),
              "iteration limit reached (max_iterations = 25)");

    EXPECT_EQ(ipm_msg(SolveStatus::kWorkLimit), "work limit reached: factorization ceiling");
    o.common.max_factorizations = 12;
    EXPECT_EQ(ipm_msg(SolveStatus::kWorkLimit, StopReason::kNone, "", o),
              "work limit reached: factorization ceiling (max_factorizations = 12)");

    EXPECT_EQ(ipm_msg(SolveStatus::kTimeLimit), "time limit reached");
    o.common.time_limit_seconds = 2.5;
    EXPECT_EQ(ipm_msg(SolveStatus::kTimeLimit, StopReason::kNone, "", o),
              "time limit reached (time_limit_seconds = 2.5)");
}

TEST(StatusMapping, TheThreeInterruptedExitsKeepDistinctWording) {
    const std::string cb = ipm_msg(SolveStatus::kInterrupted, StopReason::kCallbackStop);
    const std::string tok = ipm_msg(SolveStatus::kInterrupted, StopReason::kStopRequested);
    const std::string exc = ipm_msg(SolveStatus::kInterrupted, StopReason::kException, "boom");
    EXPECT_EQ(cb, "interrupted by the iteration callback");
    EXPECT_EQ(tok, "interrupted by a stop request (SolveBudget::stop)");
    EXPECT_EQ(exc, "interrupted by an exception that escaped the solve: boom");
    EXPECT_EQ(ipm_msg(SolveStatus::kInterrupted, StopReason::kException),
              "interrupted by an exception that escaped the solve");
    EXPECT_EQ(ipm_msg(SolveStatus::kInterrupted, StopReason::kStopRequested, "from restoration"),
              tok + "; from restoration");
    EXPECT_EQ(std::set<std::string>({cb, tok, exc}).size(), 3u);
}

TEST(StatusMapping, TheBestIterateClauseRidesOnAnyStatusItIsSetWith) {
    EXPECT_EQ(ipm_msg(SolveStatus::kStalled, StopReason::kNone, "", IpmOptions{}, SolveBudget{}, true),
              ipm_msg(SolveStatus::kStalled) + "; returned the best iterate");
    EXPECT_EQ(sqp_msg(SolveStatus::kInterrupted, StopReason::kCallbackStop, "", SqpOptions{},
                      SolveBudget{}, true),
              "interrupted by the iteration callback; returned the best iterate");
}

TEST(StatusMapping, SqpMessageForEveryStatusAndReason) {
    EXPECT_EQ(sqp_msg(SolveStatus::kOptimal), "converged: optimality and feasibility tolerances met");
    EXPECT_EQ(sqp_msg(SolveStatus::kAcceptable),
              "converged to an acceptable level: the solve stopped at a point inside the "
              "acceptable tolerances");
    EXPECT_TRUE(has(sqp_msg(SolveStatus::kAcceptable, StopReason::kNone, "qp stalled"),
                    "acceptable tolerances: qp stalled"));

    // kInfeasible: three uncertified causes and the certified one.
    EXPECT_TRUE(has(sqp_msg(SolveStatus::kInfeasible, StopReason::kNone, "", SqpOptions{},
                            SolveBudget{}, false, true),
                    "(certified:"));
    EXPECT_TRUE(has(sqp_msg(SolveStatus::kInfeasible, StopReason::kInconsistentEqualities),
                    "equality constraints are inconsistent"));
    EXPECT_TRUE(has(sqp_msg(SolveStatus::kInfeasible, StopReason::kSubproblemFailure),
                    "QP subproblem is infeasible"));
    EXPECT_TRUE(has(sqp_msg(SolveStatus::kInfeasible), "no claim is made about the model"));

    // kStalled: one wording per cause.
    EXPECT_TRUE(has(sqp_msg(SolveStatus::kStalled, StopReason::kSubproblemIterationLimit),
                    "minor-iteration cap"));
    EXPECT_TRUE(has(sqp_msg(SolveStatus::kStalled, StopReason::kRestorationWorkLimit),
                    "restoration phase spent its own factorization budget"));
    EXPECT_TRUE(has(sqp_msg(SolveStatus::kStalled, StopReason::kTrustRegionFloor),
                    "trust region reached its floor"));
    EXPECT_EQ(sqp_msg(SolveStatus::kStalled), "stalled: the iteration stopped making progress");
    EXPECT_TRUE(has(sqp_msg(SolveStatus::kDiverging), "diverging"));
}

TEST(StatusMapping, SqpNumericalErrorWordsEachRestorationCauseDistinctly) {
    EXPECT_EQ(sqp_msg(SolveStatus::kNumericalError, StopReason::kNonFiniteStart, "f is NaN"),
              "numerical error: f is NaN");
    EXPECT_EQ(sqp_msg(SolveStatus::kNumericalError, StopReason::kNonFiniteIterate),
              "numerical error");
    EXPECT_EQ(sqp_msg(SolveStatus::kNumericalError), "numerical error");
    const std::vector<std::string> causes = {
        sqp_msg(SolveStatus::kNumericalError, StopReason::kSubproblemFailure),
        sqp_msg(SolveStatus::kNumericalError, StopReason::kRestorationRefused),
        sqp_msg(SolveStatus::kNumericalError, StopReason::kRestorationNonFinite),
        sqp_msg(SolveStatus::kNumericalError, StopReason::kRestorationNoClaim),
        sqp_msg(SolveStatus::kNumericalError, StopReason::kRestorationFailed),
    };
    EXPECT_TRUE(has(causes[0], "QP subproblem failed"));
    EXPECT_TRUE(has(causes[1], "could not be serviced"));
    EXPECT_TRUE(has(causes[2], "not finite"));
    EXPECT_TRUE(has(causes[3], "without a claim"));
    EXPECT_EQ(causes[4], "numerical error: the feasibility restoration sub-solve failed");
    EXPECT_EQ(sqp_msg(SolveStatus::kNumericalError, StopReason::kRestorationFailed, "bad KKT"),
              "numerical error: the feasibility restoration sub-solve failed: bad KKT");
    EXPECT_EQ(std::set<std::string>(causes.begin(), causes.end()).size(), causes.size());
}

TEST(StatusMapping, SqpWorkLimitDistinguishesTheMinorIterationBudget) {
    // No probe stop: the factorization ceiling.
    EXPECT_EQ(sqp_msg(SolveStatus::kWorkLimit), "work limit reached: factorization ceiling");
    // A probe-budget stop: the minor-iteration budget, quoting it when the call set one.
    EXPECT_EQ(sqp_msg(SolveStatus::kWorkLimit, StopReason::kNone, "", SqpOptions{}, SolveBudget{},
                      false, false, 1),
              "work limit reached: minor-iteration budget");
    SolveBudget b;
    b.max_minor_iterations = 40;
    EXPECT_EQ(sqp_msg(SolveStatus::kWorkLimit, StopReason::kNone, "", SqpOptions{}, b, false, false,
                      3),
              "work limit reached: minor-iteration budget (minor_budget = 40)");
    SqpOptions o;
    o.common.time_limit_seconds = 9.0;
    o.common.max_iterations = 5;
    EXPECT_EQ(sqp_msg(SolveStatus::kTimeLimit, StopReason::kNone, "", o),
              "time limit reached (time_limit_seconds = 9)");
    EXPECT_EQ(sqp_msg(SolveStatus::kIterationLimit, StopReason::kNone, "", o),
              "iteration limit reached (max_iterations = 5)");
}

TEST(StatusMapping, ADefiniteStatusOutsideTheEnumeratorsIsLabelledNotThrown) {
    const auto bad = static_cast<SolveStatus>(99);
    EXPECT_EQ(ipm_msg(bad), "unrecognized status");
    EXPECT_EQ(sqp_msg(bad), "unrecognized status");
}

TEST(StatusMapping, DescribeExceptionHandlesEveryShape) {
    using hven::detail::describe_exception;
    EXPECT_EQ(describe_exception(std::exception_ptr{}), "");
    EXPECT_EQ(describe_exception(std::make_exception_ptr(std::runtime_error("model blew up"))),
              "model blew up");
    EXPECT_EQ(describe_exception(std::make_exception_ptr(42)),
              "an exception of a type not derived from std::exception");
}

} // namespace
