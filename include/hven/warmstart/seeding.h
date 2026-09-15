// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// seeding.h -- the three constants that govern how a warm start's MULTIPLIERS
// are admitted, in one place and DELIBERATELY NOT UNIFIED (design 2.4).
//
// Each of the three is a policy of ONE engine, with its own derivation, and
// they are gathered here because a reader looking for "what does this project
// do to a seeded multiplier" should find all three at once -- not because they
// are three spellings of one number. Unifying their VALUES would silently
// retune two engines to each other; the header exists to make the difference
// legible, not to erase it.
//
//   kSeededIqMultFloor   IPM   the lower end of the interior the barrier
//                              method is defined on.
//   kSeededMultInitMax   IPM   the magnitude ceiling on any seeded multiplier.
//   kSeededDualClampTol  SQP   the sign band inside which a slightly negative
//                              inequality price is a rounding artefact rather
//                              than a corrupt object.
//
// COMPATIBILITY, AND ITS END: the two IPM values were also reachable as static
// members of the solver class -- `InteriorPointSolver::kSeededIqMultFloor` and
// `::kSeededMultInitMax` -- until M6 W5 T8.10 swept the call sites and DROPPED
// those aliases. `hven::solvers::kSeededIqMultFloor` and `::kSeededMultInitMax`
// are the only names for them now. `hven::solvers::kSeededDualClampTol` was
// already at this namespace scope and is simply defined here.

namespace hven::solvers {

/// @brief IPM. Floor applied to seeded INEQUALITY multipliers when they are
///        installed.
///
/// The slack-complementarity update divides by these values, so a seed at or
/// below zero would put the very first iterate outside the interior the method
/// is defined on. An IPM policy about an IPM invariant; the SQP has no such
/// division and applies no floor.
inline constexpr double kSeededIqMultFloor = 1.0e-8;

/// @brief IPM. Ceiling applied to EVERY seeded multiplier.
///
/// Both signs for equality rows; the upper end for inequality rows, alongside
/// kSeededIqMultFloor's lower end. Parity with Ipopt's own seeded-multiplier
/// ceiling (`warm_start_mult_init_max`, default 1e6) and with the interior
/// solver's own bound-multiplier seeding precedent (`kBoundMultInitCap = 1e3`
/// in `push_initial_point_interior`, bound_set.h).
inline constexpr double kSeededMultInitMax = 1.0e6;

/// @brief SQP. The sign band on an INGESTED inequality price at kSeeded.
///
/// A `lambda_i(j)` in [-kSeededDualClampTol, 0) is clamped to 0 and counted
/// (`SqpCounters::seeded_clamped`); anything more negative DEGRADES the whole
/// object to kCold rather than being repaired. A tolerance about what a
/// producer's rounding can plausibly have done to a price that is
/// mathematically non-negative -- not a magnitude policy, which is why it is
/// not the IPM's floor and does not move with it.
inline constexpr double kSeededDualClampTol = 1e-6;

} // namespace hven::solvers
