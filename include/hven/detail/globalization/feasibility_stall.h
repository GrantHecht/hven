// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <limits>

namespace hven::solvers {

/// Consecutive feasibility-stage iterations at an elevated violation before the
/// stage is declared worsening: sized so a real worsening is caught promptly
/// while nothing transient is mistaken for one — a single dip back to (or
/// below) best restarts the count.
inline constexpr int kFeasStallWindow = 50;

/// Multiple of the best-seen L1 violation at or above which an observation
/// counts as elevated: far outside any floating-point noise, so only genuine
/// sustained worsening accumulates a window.
inline constexpr double kFeasStallGrowthFactor = 1.25;

/// Relative floor for the caller's net-progress test against the violation
/// recorded at the last restoration dispatch: a rounding-noise floor, not a
/// progress standard, so an ulp of drift at a flat stage does not read as
/// ground gained. The detector itself does not use it — best-seen tracking
/// takes any strict decrease.
inline constexpr double kFeasStallMinRelImprovement = 1.0e-12;

/// @brief Windowed sustained-worsening detector for the feasibility-only stage.
///
/// Under its default no-line-search configuration the feasibility stage accepts
/// every fraction-to-boundary step, so the rejected-trial gate that dispatches
/// the recovery chain — and through it feasibility restoration — is never
/// consulted from that stage: such a stage would otherwise burn its whole
/// iteration budget with no mechanism ever consulted. This detector supplies
/// the missing dispatch signal: it watches the L1 constraint violation once per
/// feasibility-stage iteration and reports a worsening stage when that violation
/// has sat a full window of consecutive iterations at or above a fixed multiple
/// of the best value the stage has ever held.
///
/// Deliberate narrowing: plateaus (flat at best) and slow improvement never
/// fire — worsening is the only class in which a dispatched episode has
/// demonstrated value; episodes injected into quietly succeeding stages have
/// steered solvable problems off acceptable exits. The elevation margin is many
/// orders of magnitude above double-precision rounding noise, so run-to-run FP
/// drift cannot move an observation across the mark.
///
/// One instance per phase, owned by alg_impl; observe() is called once per
/// feasibility-stage iteration. Per-phase freshness needs no explicit clear —
/// each phase begins with a default-constructed detector.
struct FeasibilityStallDetector {
    /// Smallest L1 constraint violation observed since the window was armed.
    double best_theta_ = std::numeric_limits<double>::infinity();

    /// Consecutive observations at or above kFeasStallGrowthFactor * best_theta_.
    int iters_elevated_ = 0;

    /// L1 constraint violation at this phase's MOST RECENT restoration dispatch,
    /// infinity if never dispatched. Rewritten by every note_dispatch() and
    /// preserved across reset_window(), so it always marks the point at which
    /// recovery last handed the stage back.
    double theta_at_last_dispatch_ = std::numeric_limits<double>::infinity();

    /// @brief Records one observation. Returns true once the stage has spent
    /// kFeasStallWindow consecutive observations at or above
    /// kFeasStallGrowthFactor times its best-seen violation; any observation
    /// below that mark breaks the run — and, when it is also a new low,
    /// records it. A stage sitting exactly at its best, or improving at any
    /// rate, therefore never accumulates a window. The best-positive guard
    /// keeps that true at an exactly feasible best (best_theta_ == 0 would
    /// make every observation "elevated"; such a stage is finishing, not
    /// worsening).
    bool observe(double theta) {
        if (best_theta_ > 0.0 && theta >= kFeasStallGrowthFactor * best_theta_) {
            ++iters_elevated_;
            return iters_elevated_ >= kFeasStallWindow;
        }
        if (theta < best_theta_)
            best_theta_ = theta;
        iters_elevated_ = 0;
        return false;
    }

    /// @brief Records the violation at which this phase entered restoration.
    /// Every dispatch overwrites the reference: it answers whether the stage
    /// has gained anything since recovery LAST handed it back, not since the
    /// first episode of the phase.
    void note_dispatch(double theta) { theta_at_last_dispatch_ = theta; }

    /// @brief Re-arms the elevation window only; theta_at_last_dispatch_
    /// survives deliberately — it remains the caller's reference for whether
    /// the resumed stage has gained ground since the dispatch.
    void reset_window() {
        best_theta_ = std::numeric_limits<double>::infinity();
        iters_elevated_ = 0;
    }
};

} // namespace hven::solvers
