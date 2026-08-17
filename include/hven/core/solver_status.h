#pragma once

// solver_status.h -- the two solver outcome enums: QpStatus, the verdict on ONE
// QP subproblem, and SqpStatus, the verdict on a WHOLE SQP solve, together with
// SqpStatus's display-string helper.
//
// M3 PHASE-C S2 MOVED BOTH HERE UNCHANGED, QpStatus from `hven/qp/qp_types.h`
// and SqpStatus (with its `to_string`) from `hven/drivers/sqp_types.h`. They are
// diagnostics, which CLAUDE.md section 2 homes in `core/`, and the move is part
// of what lets `core/ledger.h` -- which records both -- stop including a
// `drivers/` header. Both donor headers include this one, so no call site
// changed.

namespace hven::solvers {

// QP solver status enumeration
enum class QpStatus {
    kOptimal = 0,
    kMaxIter = 1,
    kInfeasible = 2,
    kNumericalError = 3,
};

// Outcome of a whole SQP solve. Deliberately mirrors QpStatus one-for-one --
// the driver's Task-4 status routing was a direct map from the subproblem's
// verdict -- but the correspondence is no longer literal, and kInfeasible is
// where it broke: from Task 8 NO subproblem status propagates to it at all,
// and from TASK 9 IT IS A VERDICT ABOUT THE NLP rather than an interim exit.
//
// kInfeasible NOW MEANS "the restoration phase reached a stationary point of
// the infeasibility measure h at which h is still above feas_tol", i.e. the
// Byrd-Curtis-Nocedal rapid-detection conclusion: no direction reduces the
// constraint violation from the returned point, so the returned point is a
// LOCAL certificate that the NLP is infeasible there. SqpSolution::x is that
// point and lambda_e/lambda_i are its subgradient certificate -- see
// SqpSolution's own note, which is where the certificate is spelled out, and
// sqp_driver.h's RESTORATION PHASE for what "stationary" is measured as.
//
// TWO SHAPES OF THIS STATUS ARE NOT A CERTIFICATE, AND ONLY ONE FIELD TELLS
// THEM APART: SqpSolution::infeasibility_certified. If the restoration phase
// could not run to a verdict -- its own subproblems stalled, or the
// ONE-RESTORATION-PER-SOLVE cap was already spent -- the driver still reports
// kInfeasible, because it has a restoration request it cannot service, and
// THEN THE STATUS MEANS "could not make progress", which is a statement about
// this solve and NOT about the model.
//
// AN EARLIER VERSION OF THIS NOTE SAID counters.restoration_iters AND THE
// LAST HISTORY ROW DISTINGUISH THEM. THEY DO NOT. restoration_iters > 0 holds
// on all three (the certified exit, the nested-stuck exit and the cap exit) --
// a violated entry always spends at least one major restoring. But the last
// row's StepVerdict is NOT uniformly kRestore across them: which verdict
// lands there depends on which of the THREE REQUEST SOURCES raised THIS
// restoration, not on which of the three exits it happened to reach --
// sqp_driver.h's RESTORATION PHASE note has the corrected decision table
// (the funnel's signature and the elastic tier's exhaustion leave kRestore;
// the radius floor leaves kReject, told apart by tr_radius sitting at the
// floor instead). Either way, NEITHER field distinguishes the certified exit
// from the other two, which is why this flag exists. A caller following the
// old rule on a FEASIBLE problem that stalls twice would announce local
// infeasibility with nothing behind it. sqp_driver.h's RESTORATION PHASE note
// has the full decision table; the flag is the whole of the contract, and
// tests/test_sqp_restoration.cpp additionally shows how to re-derive the
// certificate from the model if a caller wants to check the driver's own
// claim.
// kBudgetExhausted (Phase-4 Task 6) is a THIRD shape of "ran out of majors",
// distinct from kMaxIter: it is what a max_iter exhaustion of the MAIN
// optimality loop reports when SqpOptions::budget_mode is on, and it comes
// paired with a DIFFERENT (x, lambda_e, lambda_i, z, f) than kMaxIter would
// report for the identical run -- the best iterate by the funnel's own
// ordering rather than the last one reached. See SqpOptions::budget_mode's
// own note for the full contract and sqp_driver.h's BUDGETED MODE note for
// the mechanics. It is NEVER reported when budget_mode is false (every
// max_iter exhaustion is kMaxIter exactly as before this task), and it is
// NEVER reported by the restoration phase's own budget exhaustion (that
// stays kMaxIter regardless of budget_mode -- see the SqpOptions note for
// why).
enum class SqpStatus {
    kOptimal = 0,
    kMaxIter = 1,
    kInfeasible = 2,
    kNumericalError = 3,
    kBudgetExhausted = 4,
};

// SqpStatus -> a short display string. The natural home for the DECLARATION:
// both ledger.h's ledger dump and sqp_driver.h's iteration-table printer
// already depend on this header for the enum itself, so declaring it here
// (rather than keeping a hand-synced copy in each) means a grown enum value
// needs one edit instead of two.
//
// M3 PHASE-C T1 MOVED THE DEFINITION out of this header into a library TU:
// CLAUDE.md section 5 names printing explicitly as .cpp-TU code, the switch is
// O(1) per report and depends on no inlining, and as an `inline` definition
// here it was parsed and code-generated in every TU that included this header.
// T3's follow-up rehomed it from `src/drivers/sqp_print.cpp` into
// `src/core/enum_names.cpp`, so no `core/` object resolves a symbol out of a
// `drivers/` one. Only the definition ever moved; every call site is unchanged.
const char *to_string(SqpStatus status);

} // namespace hven::solvers
