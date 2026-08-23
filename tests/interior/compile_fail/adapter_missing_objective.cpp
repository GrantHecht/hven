// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// MUST NOT COMPILE.
//
// A type whose adapter provides install_constraint and simply OMITS
// install_objective, without inheriting the mixin. Specialization lookup does
// not fall back to the primary template, so the primary's authored message
// cannot fire here -- only ObjectiveInterface's own backstop can diagnose it.
// The expected output is that backstop's authored "hven objective adapter:"
// static_assert and nothing raw.

#include "adapter_fixture_functions.h"

hven::solvers::ObjectiveInterface probe_adapter_missing_objective() {
    const adapter_fixture::StubAdapterMissingObjective f{};
    return hven::solvers::ObjectiveInterface(f);
}
