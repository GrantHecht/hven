#include <gtest/gtest.h>
#include <hven/qp/qp_types.h>

TEST(Types, DefaultsMatchSpec) {
    hven::solvers::QpOptions opts;
    EXPECT_DOUBLE_EQ(opts.primal_delta, 1e-8);
    EXPECT_DOUBLE_EQ(opts.dual_mu, 1e-8);
    EXPECT_EQ(opts.schur_cap, 128);
    EXPECT_EQ(static_cast<int>(hven::solvers::BoundState::kFree), 0);
}
