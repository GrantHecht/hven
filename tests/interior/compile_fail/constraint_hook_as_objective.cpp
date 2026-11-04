// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// MUST NOT COMPILE.
//
// A handle with the constraint hook but not the objective hook, reaching
// ObjectiveInterface. Its payload cannot be an objective, and storing the handle
// whole would hand ObjectiveModel a type with neither surface. The expected
// diagnostic is the authored "hven objective adapter:" static_assert in the
// default adapter (hven/solver_interface_adapter.h).

#include "adapter_fixture_functions.h"

hven::solvers::ObjectiveInterface probe_constraint_hook_as_objective() {
    const adapter_fixture::StubConstraintHookOnly f{};
    return hven::solvers::ObjectiveInterface(f);
}
