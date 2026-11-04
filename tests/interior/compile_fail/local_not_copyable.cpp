// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// MUST NOT COMPILE.
//
// A local-block piece that is move-only. The layout copies a piece once per
// partition, so a piece must be copy-constructible; the expected diagnostic is
// the one authored "hven local block:" message saying so (and pointing shared
// state at std::shared_ptr).

#include <memory>

#include "hven/model/solver_interface_specs.h"

namespace {
struct MoveOnly {
    static constexpr hven::Index kInputs = 1;
    std::unique_ptr<double> weight = std::make_unique<double>(1.0);
    double value(hven::ConstVecRef v) const { return *weight * v[0] * v[0]; }
    void gradient(hven::ConstVecRef v, hven::VecRef g) const { g[0] = 2.0 * *weight * v[0]; }
    void hessian(hven::ConstVecRef, hven::MatRef H) const { H(0, 0) = 2.0 * *weight; }
};
} // namespace

hven::solvers::ObjectiveInterface probe_local_not_copyable() {
    const MoveOnly piece;
    return hven::solvers::ObjectiveInterface(piece);
}
