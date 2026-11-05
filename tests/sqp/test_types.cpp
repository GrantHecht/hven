// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <gtest/gtest.h>
#include <hven/qp/qp_types.h>

TEST(Types, DefaultsMatchSpec) {
    hven::QpOptions opts;
    EXPECT_DOUBLE_EQ(opts.primal_regularization, 1e-8);
    EXPECT_DOUBLE_EQ(opts.dual_regularization, 1e-8);
    EXPECT_EQ(opts.max_schur_border_dimension, 128);
    EXPECT_EQ(static_cast<int>(hven::BoundState::kFree), 0);
}
