// enum_names.cpp -- the display-string switches for the two solver enums that
// live in core/: SqpStatus (core/solver_status.h) and StartLevel
// (core/start_level.h).
//
// M3 PHASE-C T3 FOLLOW-UP, closing the T1 review's finding F1. T1 carved all
// five solver-enum printers into ONE TU, `src/drivers/sqp_print.cpp`, because
// that is what the plan specified; two of the five print enums that phase-C S2
// had already rehomed into `core/`. The result compiled and linked and changed
// no include closure, but it left `src/core/ledger.cpp` with two undefined
// symbols resolved by a `drivers/` object inside libhven.a -- a `core/` ->
// `drivers/` edge at LINK time, pointing the wrong way up CLAUDE.md section
// 2's tier order.
//
// `tests/core/test_core_layering.cpp` did not catch it, and that is exactly
// why it is worth fixing rather than commenting: that test scans include
// DIRECTIVES under `include/hven/core/`, so a link-time edge is invisible to
// it, and the T1 header comments recorded the property by hand instead. That
// test's own banner is pointed about what happens to a hand-asserted,
// unenforced layering claim. This TU removes the claim's need to exist: the
// two printers whose enums are core/'s are now defined in a core/ TU, and
// `src/core/ledger.cpp` no longer references a `drivers/` object at all.
//
// THE DECLARATIONS DID NOT MOVE. They stay on the headers that own the enums
// (`core/solver_status.h`, `core/start_level.h`), which is where every call
// site already reaches them; only the two switches moved, from
// `src/drivers/sqp_print.cpp` to here. The remaining three printers stay in
// `sqp_print.cpp` with the iteration-table renderer: `StepVerdict` and
// `SqpSolution` are `drivers/` types and `PredictorOutcome` is a
// `detail/warmstart/` one, so a `drivers/` TU defining those points DOWNWARD
// in section 2's order and is not an inversion.
//
// FP-ARITHMETIC-FREE, like the printers it was split from: two switches over
// an enum returning `const char *`. No double is read, written or compared
// here at all.

#include <hven/core/solver_status.h>
#include <hven/core/start_level.h>

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

} // namespace hven::solvers
