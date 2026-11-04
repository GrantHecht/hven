// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// AP-2 B6 rider (C236 R4): the include-order hole is closed. This translation
// unit includes the handles' header ONLY -- not local_blocks.h, not pieces.h --
// and a local-block piece still enters a handle as its local block, never as a
// directly stored type through the default adapter. The handles' header
// includes the local-block layer at its end, so no translation unit that can
// convert a piece can disagree with another about which adapter it takes.

#include <gtest/gtest.h>

#include <typeinfo>

#include "hven/model/solver_interface_specs.h"

namespace {

struct IncludeOrderDefect {
    static constexpr hven::Index kInputs = 2, kOutputs = 1;
    void value(hven::ConstVecRef v, hven::VecRef out) const { out[0] = v[1] - v[0]; }
    void jacobian(hven::ConstVecRef, hven::MatRef J) const { J << -1.0, 1.0; }
    static constexpr auto kHessianPattern = hven::solvers::kNoHessian;
};

struct IncludeOrderTerm {
    static constexpr hven::Index kInputs = 1;
    double value(hven::ConstVecRef v) const { return v[0] * v[0]; }
    void gradient(hven::ConstVecRef v, hven::VecRef g) const { g[0] = 2.0 * v[0]; }
    void hessian(hven::ConstVecRef, hven::MatRef H) const { H(0, 0) = 2.0; }
};

TEST(LocalBlockIncludeOrder, TheHandlesHeaderAloneTakesTheLocalBlockRoute) {
    using hven::solvers::ConstraintModel;
    using hven::solvers::LocalConstraintBlock;
    using hven::solvers::LocalObjectiveBlock;
    using hven::solvers::ObjectiveModel;

    const hven::solvers::ConstraintInterface ci(IncludeOrderDefect{});
    const auto &stored_constraint = ci.storage_.get();
    EXPECT_EQ(typeid(stored_constraint),
              typeid(ConstraintModel<LocalConstraintBlock<IncludeOrderDefect>>));
    EXPECT_EQ(ci.num_kkt_elements(true, true), 2);

    const hven::solvers::ObjectiveInterface oi(IncludeOrderTerm{});
    const auto &stored_objective = oi.storage_.get();
    EXPECT_EQ(typeid(stored_objective),
              typeid(ObjectiveModel<LocalObjectiveBlock<IncludeOrderTerm>>));
    EXPECT_TRUE(hven::solvers::SolverInterfaceAdapter<IncludeOrderDefect>::registered);
}

} // namespace
