// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The exception types hven raises and recognises, and the contract both engines
// keep around them.
//
// THE EXCEPTION CONTRACT, both engines, every solve entry:
//
//   * Contract violations -- bad sizes or bounds, invalid options, a structure
//     change between solves, a Hessian-less problem on the interior-point
//     engine, a pieces problem on the SQP engine -- throw std::invalid_argument.
//   * Backend failures the engine cannot report as a status throw BackendError.
//     On the interior-point engine the line is drawn by HOW the failure
//     arrives: a backend code RETURNED by a KKT factorization is a status,
//     never a throw -- the initial factorization's, an iteration's last
//     inertia-correction attempt's, the dense step guard's re-run's (a numeric
//     refusal goes to the solver's recovery first: elastic re-centre, then
//     feasibility restoration; kNumericalError when neither can act, short of
//     an acceptable-tier iterate, which stops kAcceptable -- and any other
//     code is kNumericalError at once); an exception THROWN by the linear layer
//     (backend memory exhaustion, a session failure) propagates as
//     BackendError.
//   * EvaluationError is the model's own signal that it cannot evaluate at the
//     point it was handed. Thrown at a TRIAL point (a line-search or trust-region
//     trial, a second-order correction, a restoration trial, or the derivatives
//     at a point a step has just reached) the engine rejects the trial and backs
//     off; it is counted in SolveResult::evaluation_errors and never leaves the
//     solve. Thrown at the START point it propagates out of `solve`.
//   * A non-finite value (NaN or +-Inf) in the objective, a constraint value or
//     a derivative entry is treated exactly as a thrown EvaluationError at a
//     trial point. At the start point it ends the solve with
//     SolveStatus::kNumericalError (kNonFiniteStart) and a message naming the
//     quantity, before anything is factorized; the solve never continues from
//     it. At a point a step reached, the step is rolled back where it can be;
//     where it cannot, the solve ends kNumericalError (kNonFiniteIterate).
//     Either exit returns the point whose evaluation failed with its
//     multipliers cleared (solver_status.h states which exits roll back).
//   * Any other exception thrown by a user callback (a model evaluation, the
//     iteration callback, a trace sink, the KKT hook) propagates out of `solve`
//     unchanged -- the same object, with its original type. hven never wraps it.
//     The solver keeps what the interrupted solve reached, readable through
//     `last_result()`: status SolveStatus::kInterrupted, the iterations taken,
//     the last accepted iterate and its multipliers, the work spent and a message
//     naming the exception.
//   * After any throw the solver and the Problem stay valid (the basic
//     guarantee): the next solve starts afresh and returns what the same solve
//     returns on a new solver.
//   * Internal invariant breaks throw std::logic_error.
//
// Every failure this contract names as a runtime one derives from Error, so one
// catch clause takes all of them, and hven itself throws no other
// std::runtime_error. The linear layer's precondition checks -- a factor solved
// or shared before a successful factorize(), a factorize() before any analysis,
// an adopted handle whose numerics are stale -- and the internal "cannot
// happen" checks of the QP tiers (a structure-key collision, an engine
// invariant) are programming errors and throw std::logic_error; a correct solve
// reaches none of them.

#include <stdexcept>
#include <string>

namespace hven {

/// @brief The base of every runtime failure hven raises.
class Error : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

/// @brief Thrown by a model callback that cannot evaluate at the point it was
///        given.
///
/// At a trial point the engine rejects the trial and backs off. At the start
/// point it propagates out of `solve`. See this header's contract.
class EvaluationError : public Error {
  public:
    using Error::Error;
};

/// @brief A failure of the sparse linear-algebra backend (MKL Pardiso, Apple
///        Accelerate, LAPACK) that the engine cannot report as a status.
class BackendError : public Error {
  public:
    using Error::Error;
};

} // namespace hven
