// sqp_print.cpp -- the SQP engine's display-string helpers and its iteration
// -table renderer, carved out of the four headers that declare them.
//
// M3 PHASE-C T1. CLAUDE.md section 5 names printing explicitly as code that
// belongs in a .cpp translation unit: none of it is a per-element hot path,
// none of it depends on inlining through a template parameter, and every one
// of these functions runs O(1) times per solve or once per report row. Before
// this carve each of the five was an `inline` definition in a header, so the
// switch (or the fmt call chain) was parsed and code-generated in every TU
// that included the header, and the linker discarded all but one copy.
//
// THE WHOLE SET IS FP-ARITHMETIC-FREE, which is what let it land as an
// early phase-C split: four switch-to-`const char *` functions, and one
// renderer that hands already-computed doubles to `fmt::format`. `fmt` READS
// a double and formats its value; it does not compute with it, so no
// floating-point expression in this file can be re-associated, contracted or
// otherwise re-shaped by the codegen flags.
//
// ONE TU FOR ALL FIVE, INCLUDING THE TWO WHOSE ENUMS LIVE IN core/.
// `SqpStatus` and `StartLevel` were hoisted into core/ by phase-C S2, after
// the plan named this TU. Keeping their printers here with the other three
// costs one link-time edge from core/ledger.cpp to this object inside
// libhven.a; it costs nothing at compile time, and it changes no include
// closure -- core/ headers still include nothing above core/, which is the
// layering rule tests/core/test_core_layering.cpp actually enforces.

#include <string>

#include <fmt/format.h>

#include <hven/core/solver_status.h>
#include <hven/core/start_level.h>
#include <hven/detail/warmstart/predictor.h>
#include <hven/drivers/sqp_driver.h>

namespace hven::solvers {

const char *to_string(SqpStatus status) {
    switch (status) {
    case SqpStatus::kOptimal:
        return "Optimal";
    case SqpStatus::kMaxIter:
        return "MaxIter";
    case SqpStatus::kInfeasible:
        return "Infeasible";
    case SqpStatus::kNumericalError:
        return "NumericalError";
    case SqpStatus::kBudgetExhausted:
        return "BudgetExhausted";
    }
    return "Unknown";
}

const char *to_string(StartLevel level) {
    switch (level) {
    case StartLevel::kCold:
        return "Cold";
    case StartLevel::kSeeded:
        return "Seeded";
    case StartLevel::kWarm:
        return "Warm";
    case StartLevel::kHot:
        return "Hot";
    }
    return "Unknown";
}

const char *to_string(PredictorOutcome outcome) {
    switch (outcome) {
    case PredictorOutcome::kPredicted:
        return "Predicted";
    case PredictorOutcome::kZeroStep:
        return "ZeroStep";
    case PredictorOutcome::kDegraded:
        return "Degraded";
    }
    return "Unknown";
}

const char *to_string(StepVerdict v) {
    switch (v) {
    case StepVerdict::kAcceptF:
        return "AcceptF";
    case StepVerdict::kAcceptH:
        return "AcceptH";
    case StepVerdict::kReject:
        return "Reject";
    case StepVerdict::kRestore:
        return "Restore";
    }
    return "?";
}

std::string format_iteration_table(const SqpSolution &sol) {
    std::string result;
    const std::string header =
        fmt::format("{:>5} {:>14} {:>14} {:>14} {:>12} {:>9} {:>7} {:>7} {:>3}", "Trial", "f",
                    "KKT Res", "h", "Delta", "Verdict", "QP It", "QP Fact", "WD");
    result += header + "\n";
    result += std::string(header.size(), '-');
    result += "\n";

    for (const SqpIterate &row : sol.history) {
        const char *wd_marker = row.watchdog_restored ? "*" : "";
        if (row.qp_solved) {
            result += fmt::format(
                "{:>5} {:>14.6e} {:>14.6e} {:>14.6e} {:>12.6e} {:>9} {:>7} {:>7} {:>3}\n",
                row.trial, row.f, row.kkt_residual, row.violation_l1, row.tr_radius,
                to_string(row.verdict), row.qp_minor_iters, row.qp_factorizations, wd_marker);
        } else {
            result += fmt::format(
                "{:>5} {:>14.6e} {:>14.6e} {:>14.6e} {:>12.6e} {:>9} {:>7} {:>7} {:>3}\n",
                row.trial, row.f, row.kkt_residual, row.violation_l1, row.tr_radius, "-", "-", "-",
                wd_marker);
        }
    }

    result += fmt::format("\nStatus: {}\n", to_string(sol.status));
    result += fmt::format("Start Level: {}\n", to_string(sol.counters.start_level_used));
    return result;
}

} // namespace hven::solvers
