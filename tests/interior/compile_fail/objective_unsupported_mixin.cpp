// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// MUST NOT COMPILE.
//
// A type whose adapter declares it constraint-only by inheriting
// ObjectiveUnsupported. This is the sanctioned way to register one direction,
// so its refusal must read as a deliberate answer rather than an accident:
// the expected output is the mixin's own authored "hven objective adapter:"
// static_assert and nothing raw.

#include "adapter_fixture_functions.h"

hven::solvers::ObjectiveInterface probe_objective_unsupported_mixin() {
    const adapter_fixture::StubMixinConstraintOnly f{};
    return hven::solvers::ObjectiveInterface(f);
}
