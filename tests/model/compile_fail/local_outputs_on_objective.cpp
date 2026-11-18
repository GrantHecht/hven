// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// MUST NOT COMPILE.
//
// A piece written as an objective (double value(v), gradient, hessian(v, H)) that
// also declares kOutputs. kOutputs makes it a constraint piece, which it is not;
// the expected diagnostic is the one authored "hven local block:" message naming
// the kOutputs.

#include "hven/model/piece_interfaces.h"

namespace {
struct ObjectiveWithOutputs {
    static constexpr hven::Index kInputs = 1, kOutputs = 1;
    double value(hven::ConstVecRef v) const { return v[0] * v[0]; }
    void gradient(hven::ConstVecRef v, hven::VecRef g) const { g[0] = 2.0 * v[0]; }
    void hessian(hven::ConstVecRef, hven::MatRef H) const { H(0, 0) = 2.0; }
};
} // namespace

hven::ObjectiveInterface probe_local_outputs_on_objective() {
    return hven::ObjectiveInterface(ObjectiveWithOutputs{});
}
