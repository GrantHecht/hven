// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// MUST NOT COMPILE.
//
// A type with no piece surface, no hook and no specialization reaching
// ConstraintInterface: the default adapter would store it as itself, and the
// direct-storage install refuses it before instantiating a model on it. The
// expected diagnostic is the authored "hven constraint adapter:" static_assert in
// model/solver_interface_specs.h (DirectFunctionModel::install_constraint).

#include "adapter_fixture_functions.h"

hven::solvers::ConstraintInterface probe_no_constraint_surface() {
    const adapter_fixture::StubNoSurface f{};
    return hven::solvers::ConstraintInterface(f);
}
