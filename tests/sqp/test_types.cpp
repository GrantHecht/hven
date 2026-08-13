#include <gtest/gtest.h>
#include <tycho_sqp/types.h>

TEST(Types, DefaultsMatchSpec) {
    tycho::sqp::QpOptions opts;
    EXPECT_DOUBLE_EQ(opts.primal_delta, 1e-8);
    EXPECT_DOUBLE_EQ(opts.dual_mu, 1e-8);
    EXPECT_EQ(opts.schur_cap, 128);
    EXPECT_EQ(static_cast<int>(tycho::sqp::BoundState::kFree), 0);
}
