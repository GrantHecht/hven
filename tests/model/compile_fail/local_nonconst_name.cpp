// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// MUST NOT COMPILE.
//
// A local-block piece whose name() is not const. A name the layout cannot call is
// not silently replaced by the default (C236 R6); the expected diagnostic is the
// one authored "hven local block:" message naming name().

#include <string>

#include "hven/model/piece_interfaces.h"

namespace {
struct NonConstName {
    static constexpr hven::Index kInputs = 1;
    static constexpr auto kHessianPattern = hven::kNoHessian;
    std::string name() { return "mutable name"; }
    double value(hven::ConstVecRef v) const { return v[0]; }
    void gradient(hven::ConstVecRef, hven::VecRef g) const { g[0] = 1.0; }
};
} // namespace

hven::ObjectiveInterface probe_local_nonconst_name() {
    return hven::ObjectiveInterface(NonConstName{});
}
