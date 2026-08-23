// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// MUST NOT COMPILE.
//
// A registered type whose objective surface is INCOMPLETE -- objective() is
// present, objective_gradient and objective_gradient_hessian are not. A guard
// that probed only objective() would accept it and let ObjectiveModel's
// forwarding methods produce a raw "no member named" for each of the other
// two, alongside the authored message. The expected output is the authored
// "hven objective adapter:" static_assert and nothing raw.

#include "adapter_fixture_functions.h"

hven::solvers::ObjectiveInterface probe_partial_objective_surface() {
    const adapter_fixture::StubPartialObjective f{};
    return hven::solvers::ObjectiveInterface(f);
}
