// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

namespace hven::solvers {

/// @brief The (theta, f, aux) triple used by every AcceptanceStrategy.
///
/// Plain value type: no ownership, no solver-state references. Constructed
/// fresh per call and passed by const reference into acceptance/mechanism
/// methods. The same shape serves three distinct roles by design — current
/// iterate, trial point, and predicted reduction (what a model of the step
/// predicts theta/f/aux will become); introducing a separate
/// predicted-reduction type is deliberately deferred until a strategy needs
/// fields this struct does not carry.
///
/// Invariants:
/// - auxiliary carries barrier/proximal terms OUTSIDE the (theta, f) pair on
///   purpose: folding a barrier term into the merit objective would
///   contaminate the filter/funnel machinery.
/// - infeasibility is the KKT constraint block and nothing else (norm over
///   equality and inequality rows). Native primal variable bounds are not
///   rows — they are condensed into the primal diagonal — so a bound never
///   enters theta no matter how tightly an iterate is pressed against it;
///   their barrier contribution arrives in auxiliary. This structural (not
///   guarded) exclusion is what lets the filter, funnel, feasibility-stall
///   detector, and restoration-entry gate read the same purified
///   constraint-violation signal on bounded and unbounded problems alike.
struct ProgressMeasures {
    double infeasibility = 0.0; ///< θ — constraint violation measure.
    double objective = 0.0;     ///< σ-scaled objective measure (f, not the raw objective).
    double auxiliary = 0.0;     ///< Barrier/proximal terms; never folded into (f, θ).
};

} // namespace hven::solvers
