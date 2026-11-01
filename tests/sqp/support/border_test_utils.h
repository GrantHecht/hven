// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// tests/sqp/support/border_test_utils.h — test-support only, NOT part of the
// public library surface. Thin alias for the shared iterative-refinement
// helper used by tests that compare a Schur-complement border solve against a
// DIFFERENT formulation of the same problem (e.g. solve_eqp's
// bound-elimination path) rather than against a dense factorization of the
// identical bordered system.
//
// The mechanism itself is PRODUCTION code as of Task 3 -- it is what
// solve_bordered_eqp (bordered_eqp.h) applies to every border-mode EQP solve
// -- so this header no longer implements it, only forwards, keeping the
// existing call sites (QpEngineBorder.BorderPinEquivalence,
// BorderOps.PinVariableMatchesEliminated) reading as before while guaranteeing
// the tests exercise the same refinement the engine ships.
//
// --- Why this refinement is needed here but not for the other BorderOps
// tests ---
//
// AddIneqRowMatchesDirect/DeleteK0RowMatchesDirect/AddThenDropRoundTrip all
// compare a bordered solve against a dense factorization of the EXACT SAME
// regularized system (mod row/column permutation) -- no refinement needed,
// same as test_schur.cpp's BorderedSolveMatchesDirectFactorization. But
// PinVariableMatchesEliminated compares against solve_eqp (eqp_solve.h),
// which solves a DIFFERENT (bound-eliminated) K and additionally refines its
// own result against the fully UNREGULARIZED system (see eqp_solve.h's
// header comment). To match solve_eqp at 1e-9 rather than only
// O(primal_delta) ~ 1e-8, the border-path solve must be refined the same
// way: toward the same unregularized target.
//
// K0 (size `full.K.rows()`, from assemble_kkt_full) carries +primal_delta on
// its first `var_count` (Hessian) diagonal entries and -dual_mu on every
// remaining (equality/working) row; each border row i carries whatever d_i
// was passed to that add_border call. Every one of those diagonal entries
// regularizes a mathematically exact equation with "true" value 0
// (primal_delta relaxes stationarity; -mu and each border's own d relax a
// constraint/pin so the system stays well-posed even when that row's own
// multiplier is exactly 0), so backing all of them out of the residual --
// computable without ever forming the unregularized matrix, since
// K0_true = K0 - diag(reg) implies r = rhs - K0_true*sol = (rhs - K0*sol) +
// diag(reg)*sol -- refines toward the same target solve_eqp's own
// refinement step converges to.
//
// --- AND THE UNREGULARIZED POLISH (M7 W2 E4 fix round 1) ---
//
// As of E4 that refinement step targets the PROXIMAL system: the primal delta is
// a term of the model there, not an error, so `bordered_regularization` no longer
// reports it and `refine_bordered_solve` no longer backs it out (bordered_eqp.h
// states why). What takes it back out, on BOTH paths and at the same point in
// each solve, is the UNREGULARIZED POLISH -- solve_eqp's own
// `polish_eqp_core` and `solve_bordered_eqp`'s `polish_bordered_unregularized`,
// each guarded by strict decrease of the FACE'S OWN exact residual. So a test
// that hand-rolls the border path and compares against solve_eqp must apply the
// border polish too, or it compares a delta-free answer against one that still
// carries O(primal_delta) ~ 1e-8 and the 1e-9 assertion cannot hold. Both call
// sites do, through the second alias below, with x_k = 0 -- the centre those two
// tests pass to solve_eqp.

#include <hven/detail/kkt/bordered_eqp.h>

namespace hven::solvers::test_support {

using hven::solvers::polish_bordered_unregularized;
using hven::solvers::refine_bordered_solve;

} // namespace hven::solvers::test_support
