// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// sqp_warm_start.h -- SqpWarmStart, the SQP engine's OWN native warm-start
// value, and the options of the interior-point crossover that builds one.
//
// TWO IDENTITIES, ONE PROTOCOL. The shared, cross-engine currency is
// `warmstart/warm_start.h::WarmStart` -- a declared-space PAYLOAD both
// engines accept through `solve(problem, x0, const WarmStart &, SolveBudget)`,
// whose identity stamp REFUSES a foreign problem in every mode. This header
// carries the OTHER identity: the SQP's native object, which is not a payload,
// carries no declaration stamp, is never serialized, and reaches the engine only
// through the LABELLED SQP-ONLY entry
// `SqpResult SqpSolver::solve(Problem &, const Vec &x0, const SqpWarmStart &,
//                             SolveBudget)`.
// It is the exact counterpart of the IPM-only KKT hook: one engine-only
// extension per engine. Without it `StartLevel::kWarm` and `kHot` are
// unreachable -- a payload carries no structure hash and so caps at `kSeeded`,
// and the hot handle is process-local and never travels in a payload.
//
// OPAQUE. The value holds a shared handle to the engine's state and nothing
// else, so the state can grow (a serialized form among other things) without
// breaking a consumer. A consumer obtains one from a result
// (`SqpResult::sqp_warm_start`), from `to_sqp_warm_start`
// (warmstart/ipm_polish_extension.h), or by copying another; it passes it back to
// the native solve entry. It is cheap to copy (the copies share one immutable
// state). It declares NO default constructor, so a braced `{}` third argument to
// `solve` selects the `WarmStart` overload.
//
// VALIDATION ON THE NATIVE ROUTE IS DEGRADE-ONLY: nothing here is ever "refused
// for identity", because nothing here claims one. `valid() == false`, a size
// disagreement, or a non-finite ingested vector resolves `kCold`; a
// structure-hash mismatch resolves `kSeeded`; a match resolves `kWarm`; a match
// plus a live hot handle resolves `kHot`, subject to the backend's own reuse
// checks. Every `SqpSolver::solve()` exit holds a valid state EXCEPT one: a solve
// that could not EVALUATE its own start point, whose state is not valid (feeding
// that point back would override the corrected `x0` a caller retried with).
//
// SAME-PROCESS ONLY: the state may hold a live factorization.

#include <memory>

#include <hven/core/types.h>

namespace hven {

struct SqpResult;

namespace detail {
struct SqpWarmState;
struct SqpWarmAccess;
} // namespace detail

/// @brief The activity rule's safety factor `nu` for the interior-point
///        crossover, NOT an option: one decade of margin over the larger of the
///        two scales the rule compares against. Its product with the default
///        `IpmCrossoverOptions::activity_relative_tol` is 1e-3.
inline constexpr double kIpmActivityFactor = 10.0;

/// @brief The interior-point crossover's two thresholds (see
///        `to_sqp_warm_start`).
struct IpmCrossoverOptions {
    /// eps_rel: how far below the LARGEST price a live price may sit, as a
    /// fraction. 1e-4 against kIpmActivityFactor = 10 puts the floor three decades
    /// below the largest inequality price -- measured on F7 at N = 100 as the
    /// widest separation available between genuinely-active and barrier-noise
    /// prices on that family.
    double activity_relative_tol = 1e-4;
    /// Absolute "is this multiplier zero?" floor.
    double dual_tol = 1e-6;
};

/// @brief The SQP engine's native warm-start value: the point, the prices, the
///        active set, the QP working set and the globalization state a solve
///        exited with, for a later solve of a nearby problem.
class SqpWarmStart {
  public:
    /// @brief Whether the value holds a state a solve may be handed: false for a
    ///        value taken from nothing and for the one exit that could not
    ///        evaluate its own start point. A solve given an invalid value runs
    ///        cold.
    bool valid() const noexcept;

    /// @brief The primal point of the state (empty when there is none).
    const Vec &x() const noexcept;

  private:
    friend struct detail::SqpWarmAccess;
    friend struct SqpResult;

    /// Tag for the internal "nothing captured" value that SqpResult starts from.
    struct Cold {};
    explicit SqpWarmStart(Cold) noexcept {}

    // Null means nothing captured. The pointee is never modified while shared;
    // detail::SqpWarmAccess clones before it writes.
    std::shared_ptr<const detail::SqpWarmState> state_;
};

} // namespace hven
