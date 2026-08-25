// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The two solver outcome enums: QpStatus, the verdict on ONE QP subproblem,
// and SqpStatus, the verdict on a WHOLE SQP solve, together with SqpStatus's
// display-string helper.

namespace hven::solvers {

/// @brief The verdict on ONE QP subproblem solve.
enum class QpStatus {
    kOptimal = 0,
    kMaxIter = 1,
    kInfeasible = 2,
    kNumericalError = 3,
};

/// Verdict on a whole SQP solve. Deliberately mirrors QpStatus's shape, but
/// the correspondence is not literal: no subproblem status propagates into
/// kInfeasible at all, and kBudgetExhausted has no QP counterpart.
///
/// kInfeasible has TWO shapes, and only one field tells them apart:
/// - Certified: the restoration phase reached a stationary point of the
///   infeasibility measure h at which h is still above feas_tol (the
///   Byrd-Curtis-Nocedal rapid-detection conclusion). No direction reduces
///   the constraint violation from the returned point, so SqpSolution::x is
///   a LOCAL certificate that the NLP is infeasible there, and lambda_e /
///   lambda_i are its subgradient certificate -- see SqpSolution's own note
///   for the certificate and sqp_driver.h's RESTORATION PHASE for what
///   "stationary" is measured as.
/// - Not certified: restoration could not run to a verdict -- its own
///   subproblems stalled, or the one-restoration-per-solve cap was already
///   spent -- so the driver still reports kInfeasible because it holds a
///   restoration request it cannot service. Then the status means "could not
///   make progress", a statement about THIS solve and NOT about the model.
///
/// THE ONLY FIELD DISTINGUISHING THE TWO SHAPES IS
/// SqpSolution::infeasibility_certified. counters.restoration_iters does NOT
/// distinguish them: it is > 0 on all three exits (certified, nested-stuck,
/// cap) because a violated entry always spends at least one major restoring.
/// Nor does the last history row: its StepVerdict depends on which of the
/// THREE REQUEST SOURCES raised the restoration (the funnel's signature and
/// the elastic tier's exhaustion leave kRestore; the radius floor leaves
/// kReject, told apart by tr_radius sitting at the floor), not on which exit
/// was reached -- sqp_driver.h's RESTORATION PHASE has the decision table.
/// tests/sqp/test_sqp_restoration.cpp shows how to re-derive the certificate from
/// the model if a caller wants to check the driver's claim.
///
/// kBudgetExhausted is a THIRD shape of "ran out of majors", distinct from
/// kMaxIter: what a max_iter exhaustion of the MAIN optimality loop reports
/// when SqpOptions::budget_mode is on, paired with a DIFFERENT
/// (x, lambda_e, lambda_i, z, f) than kMaxIter would report for the identical
/// run -- the best iterate by the funnel's own ordering rather than the last
/// one reached. See SqpOptions::budget_mode's own note for the full contract
/// and sqp_driver.h's BUDGETED MODE note for the mechanics. It is NEVER
/// reported when budget_mode is false (every max_iter exhaustion is kMaxIter)
/// and NEVER reported by the restoration phase's own budget exhaustion (that
/// stays kMaxIter regardless of budget_mode).
enum class SqpStatus {
    kOptimal = 0,
    kMaxIter = 1,
    kInfeasible = 2,
    kNumericalError = 3,
    kBudgetExhausted = 4,
};

/// @brief Maps SqpStatus to a short display string; defined in a library TU.
const char *to_string(SqpStatus status);

} // namespace hven::solvers
