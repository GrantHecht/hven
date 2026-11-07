// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// MUST NOT COMPILE.
//
// A local-block constraint piece whose kJacobianPattern names an entry outside its
// kOutputs x kInputs block. The expected diagnostic is the authored "hven local
// block:" static_assert in model/local_blocks.h.

#include <array>

#include "hven/model/local_blocks.h"

namespace {
struct OutOfRange {
    static constexpr hven::Index kInputs = 2, kOutputs = 1;
    static constexpr std::array<hven::BlockEntry, 2> kJacobianPattern{{{0, 0}, {0, 2}}};
    static constexpr auto kHessianPattern = hven::kNoHessian;
    void value(hven::ConstVecRef, hven::VecRef) const {}
    void jacobian(hven::ConstVecRef, hven::MatRef) const {}
};
} // namespace

hven::ConstraintInterface probe_local_jacobian_pattern_out_of_range() {
    return hven::ConstraintInterface(OutOfRange{});
}
