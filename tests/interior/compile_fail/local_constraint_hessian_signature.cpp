// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// MUST NOT COMPILE.
//
// A local-block constraint piece whose hessian() has the objective's signature
// (v, H): a constraint piece's Hessian takes the multipliers. The expected
// diagnostic is the one authored "hven local block:" message saying so.

#include "hven/model/piece_interfaces.h"

namespace {
struct WrongHessian {
    static constexpr hven::Index kInputs = 1, kOutputs = 1;
    void value(hven::ConstVecRef v, hven::VecRef out) const { out[0] = v[0] * v[0]; }
    void jacobian(hven::ConstVecRef v, hven::MatRef J) const { J(0, 0) = 2.0 * v[0]; }
    void hessian(hven::ConstVecRef, hven::MatRef H) const { H(0, 0) = 2.0; }
};
} // namespace

hven::ConstraintInterface probe_local_constraint_hessian_signature() {
    return hven::ConstraintInterface(WrongHessian{});
}
