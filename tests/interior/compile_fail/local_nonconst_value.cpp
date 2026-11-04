// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// MUST NOT COMPILE.
//
// A local-block constraint piece whose value() is not const. The layout calls a
// piece through const members only, so this is a near miss; the expected
// diagnostic is the one authored "hven local block:" message naming value(v, out)
// as non-const (model/local_blocks.h), and nothing else.

#include "hven/model/solver_interface_specs.h"

namespace {
struct NonConstValue {
    static constexpr hven::Index kInputs = 2, kOutputs = 1;
    static constexpr auto kHessianPattern = hven::solvers::kNoHessian;
    void value(hven::ConstVecRef v, hven::VecRef out) { out[0] = v[0] - v[1]; }
    void jacobian(hven::ConstVecRef, hven::MatRef J) const { J << 1.0, -1.0; }
};
} // namespace

hven::solvers::ConstraintInterface probe_local_nonconst_value() {
    return hven::solvers::ConstraintInterface(NonConstValue{});
}
