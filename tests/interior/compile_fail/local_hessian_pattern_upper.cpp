// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// MUST NOT COMPILE.
//
// A local-block objective piece whose kHessianPattern names an entry above the
// diagonal. Hessian blocks are read in the lower triangle only, so the entry would
// name nothing. The expected diagnostic is the authored "hven local block:"
// static_assert in model/local_blocks.h.

#include <array>

#include "hven/model/local_blocks.h"

namespace {
struct UpperEntry {
    static constexpr hven::Index kInputs = 2;
    static constexpr std::array<hven::solvers::BlockEntry, 2> kHessianPattern{{{0, 0}, {0, 1}}};
    double value(hven::ConstVecRef) const { return 0.0; }
    void gradient(hven::ConstVecRef, hven::VecRef) const {}
    void hessian(hven::ConstVecRef, hven::MatRef) const {}
};
} // namespace

hven::solvers::ObjectiveInterface probe_local_hessian_pattern_upper() {
    return hven::solvers::ObjectiveInterface(UpperEntry{});
}
