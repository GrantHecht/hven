// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// MUST NOT COMPILE.
//
// An unregistered function type reaching ConstraintInterface is the exact
// case the seam exists to catch: without the adapter it would be accepted
// silently, and an erased handle among such types would pay a second virtual
// dispatch on every solver evaluation with nothing to say so. The expected
// diagnostic is the authored "hven constraint adapter:" static_assert in
// include/hven/solver_interface_adapter.h.

#include "adapter_fixture_functions.h"

hven::solvers::ConstraintInterface probe_unregistered_constraint() {
    const adapter_fixture::StubUnregistered f{};
    return hven::solvers::ConstraintInterface(f);
}
