// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

///////////////////////////////////////////////////////////////////////////////
// Shared ProgressMeasures literal builder for acceptance/restoration tests.
//
// Deliberately includes ONLY detail/ipm/globalization/progress_measures.h -- NOT
// solver_test_utils.h. This hven-side solver_test_utils.h is itself the
// VF-free half of the old combined header (InertSolverContext only; it
// includes just solver_context.h and jet.h, not the full ODE-phase umbrella -- see its own
// header comment) and is cheap on its own, but pm() has no reason to acquire
// even that dependency. Several of this helper's call sites live in leaf-
// header test TUs (they test a single globalization component in isolation
// and include nothing heavier than that component's own header); routing
// pm() through solver_test_utils.h would still be an unjustified coupling for
// a helper that only ever touches the plain-data ProgressMeasures type. (The
// consumer-side copy of solver_test_utils.h, tests/ipm/support/solver_test_utils.h,
// is the one that pulls the full ODE-phase umbrella, for its SolverTest
// fixture and make_brach_solver_phase() -- not this one.)
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <hven/detail/ipm/globalization/progress_measures.h>

namespace HvenTest {

/// @brief Build a ProgressMeasures(infeasibility, objective, auxiliary) triple.
/// `obj` and `aux` default to 0.0 so callers that only care about theta (e.g.
/// the classic-merit restoration exit test, which reads only
/// opts_.econ_tol / infeasibility) can write `pm(theta)`.
inline hven::detail::ProgressMeasures pm(double inf, double obj = 0.0, double aux = 0.0) {
    hven::detail::ProgressMeasures p;
    p.infeasibility = inf;
    p.objective = obj;
    p.auxiliary = aux;
    return p;
}

} // namespace HvenTest
