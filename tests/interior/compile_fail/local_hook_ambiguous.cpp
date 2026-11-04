// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// MUST NOT COMPILE.
//
// A local-block constraint piece that also carries a pack_into_constraint_interface
// hook: two meanings for one type. Refused as ambiguous (C236 R10); the expected
// diagnostic is the one authored "hven local block:" message.

#include "hven/model/solver_interface_specs.h"

namespace {
struct HookedLocal {
    static constexpr hven::Index kInputs = 1, kOutputs = 1;
    static constexpr auto kHessianPattern = hven::solvers::kNoHessian;
    void value(hven::ConstVecRef v, hven::VecRef out) const { out[0] = v[0]; }
    void jacobian(hven::ConstVecRef, hven::MatRef J) const { J(0, 0) = 1.0; }
    void pack_into_constraint_interface(hven::solvers::ConstraintInterface &) const {}
};
} // namespace

hven::solvers::ConstraintInterface probe_local_hook_ambiguous() {
    return hven::solvers::ConstraintInterface(HookedLocal{});
}
