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
//   * Backend failures the engine cannot report as a status (a factorization
//     library error, backend memory exhaustion) throw BackendError.
//   * EvaluationError is the model's own signal that it cannot evaluate at the
//     point it was handed. Thrown at a TRIAL point (a line-search or trust-region
//     trial, a second-order correction, a restoration trial, or the derivatives
//     at a point a step has just reached) the engine rejects the trial and backs
//     off; it is counted in SolveResult::evaluation_errors and never leaves the
//     solve. Thrown at the START point it propagates out of `solve`.
//   * A non-finite value (NaN or +-Inf) in the objective, a constraint value or
//     a derivative entry is treated exactly as a thrown EvaluationError at a
//     trial point. At the start point it ends the solve with
//     SolveStatus::kNumericalError and a message naming the quantity; the solve
//     never continues from it.
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
// catch clause takes all of them. (The linear layer's own precondition checks --
// a factor solved before it was factorized, a stale handle -- and a handful of
// internal "cannot happen" checks in the QP tiers still throw a plain
// std::runtime_error; a correct solve reaches none of them.)

#include <stdexcept>
#include <string>

namespace hven::solvers {

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

} // namespace hven::solvers
