// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_ipqp_options.cpp -- M6 W1 task 1: the mode/options surface for the kIpm tier. What this
// file pins is IpqpOptions field validation in validate_sqp_options, one pin per predicate
// class; the kIpm dispatch pin now lives in tests/sqp/test_ipqp_dispatch.cpp (task 6 landed it).

#include <limits>
#include <stdexcept>

#include <gtest/gtest.h>

#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>

namespace hven::solvers {
namespace {

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// ===========================================================================
// (1) IpqpOptions FIELD VALIDATION -- one pin per predicate class, validated at EVERY mode.
// ===========================================================================

TEST(IpqpOptions, TheFieldsAreValidatedAtEveryModeIncludingTheDefault) {
    // The mode itself is accepted -- task 1's temporary refusal is gone, since task 6 landed the
    // dispatch -- and the fields are refused at kWalk, where no solve will ever read them.
    SqpOptions ipm;
    ipm.qp_mode = QpMode::kIpm;
    EXPECT_NO_THROW(validate_sqp_options(ipm));

    SqpOptions defaulted;
    EXPECT_EQ(defaulted.qp_mode, QpMode::kWalk);
    EXPECT_NO_THROW(validate_sqp_options(defaulted));

    SqpOptions bad_at_kwalk;
    bad_at_kwalk.ipqp.ipqp_hard_iter_cap = 0;
    EXPECT_THROW(validate_sqp_options(bad_at_kwalk), std::invalid_argument);

    // AND THE ENUMERATOR-COUNT SENTINEL IS NOT A MODE (fix round 3): a legal `QpMode` value that
    // names no kernel, existing so a fourth mode cannot be added without a dispatch arm. It is
    // refused here at construction, not at a dispatch it can then never reach.
    SqpOptions sentinel;
    sentinel.qp_mode = QpMode::kQpModeCount;
    EXPECT_THROW(validate_sqp_options(sentinel), std::invalid_argument);
    EXPECT_THROW(SqpDriver{sentinel}, std::invalid_argument)
        << "and the constructor validates, so no solve can carry it";

    // The three real modes are all accepted, so the refusal above is about the
    // sentinel rather than about `qp_mode` being validated at all.
    for (QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpm}) {
        SqpOptions real;
        real.qp_mode = mode;
        EXPECT_NO_THROW(validate_sqp_options(real))
            << "mode " << static_cast<int>(mode) << " names a kernel";
    }
}

TEST(IpqpOptions, HardIterCapMustBePositive) {
    SqpOptions o;
    o.ipqp.ipqp_hard_iter_cap = 0;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_hard_iter_cap = -1;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_hard_iter_cap = 1;
    EXPECT_NO_THROW(validate_sqp_options(o));
}

TEST(IpqpOptions, MuBandMustBeWellFormed) {
    // ipqp_min_mu: finite, > 0.
    {
        SqpOptions o;
        o.ipqp.ipqp_min_mu = 0.0;
        EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
        o.ipqp.ipqp_min_mu = -1.0;
        EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
        o.ipqp.ipqp_min_mu = kNan;
        EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
        o.ipqp.ipqp_min_mu = kInf;
        EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    }
    // ipqp_init_mu: finite, > 0.
    {
        SqpOptions o;
        o.ipqp.ipqp_init_mu = 0.0;
        EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
        o.ipqp.ipqp_init_mu = kNan;
        EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
        o.ipqp.ipqp_init_mu = kInf;
        EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    }
    // The pair: min_mu must not exceed init_mu (the clamp band would invert).
    {
        SqpOptions o;
        o.ipqp.ipqp_min_mu = 1.0;
        o.ipqp.ipqp_init_mu = 0.5;
        EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
        o.ipqp.ipqp_min_mu = 0.5;
        o.ipqp.ipqp_init_mu = 0.5; // equality is the identity band, legal
        EXPECT_NO_THROW(validate_sqp_options(o));
    }
}

TEST(IpqpOptions, RegularizationFloorAndCeilingMustBeWellFormed) {
    // ipqp_reg_floor: finite, > 0.
    {
        SqpOptions o;
        o.ipqp.ipqp_reg_floor = 0.0;
        EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
        o.ipqp.ipqp_reg_floor = kNan;
        EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    }
    // ipqp_reg_max: finite, >= ipqp_reg_floor.
    {
        SqpOptions o;
        o.ipqp.ipqp_reg_floor = 1.0;
        o.ipqp.ipqp_reg_max = 0.5;
        EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
        o.ipqp.ipqp_reg_max = kInf;
        EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
        o.ipqp.ipqp_reg_max = 1.0; // equality legal: "never grow"
        o.ipqp.ipqp_rho_init = 1.0;
        o.ipqp.ipqp_delta_init = 1.0; // keep both inits inside the now-narrow band
        EXPECT_NO_THROW(validate_sqp_options(o));
    }
}

TEST(IpqpOptions, RhoInitMustStartInsideTheRegularizationBand) {
    // rho_init IS tied to the floor -- spec section 2.2's (M6 W1 T4b-deleted)
    // monotone floor was
    // stated for rho.
    SqpOptions o;
    o.ipqp.ipqp_reg_floor = 1e-6;
    o.ipqp.ipqp_reg_max = 1e3;
    o.ipqp.ipqp_rho_init = 1e-7; // below the floor
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_rho_init = 1e4; // above the ceiling
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_rho_init = 1.0;
    EXPECT_NO_THROW(validate_sqp_options(o));
}

TEST(IpqpOptions, DeltaInitMustBePositiveAndNoLargerThanTheCeilingButIsNotTiedToTheFloor) {
    // Fix round 1 (plan section 7 note g): delta_init is NOT bound to ipqp_reg_floor -- only rho
    // carries the monotone-floor invariant (spec 2.2) -- so a value below the deliberately high
    // floor set here must still be ACCEPTED.
    SqpOptions o;
    o.ipqp.ipqp_reg_floor = 1e-6;
    o.ipqp.ipqp_reg_max = 1e3;
    o.ipqp.ipqp_delta_init = 1e-7; // below the floor, but the floor does not bind delta
    EXPECT_NO_THROW(validate_sqp_options(o));
    o.ipqp.ipqp_delta_init = 0.0; // non-positive is still rejected
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_delta_init = -1.0;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_delta_init = kNan;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_delta_init = 1e4; // above the ceiling is still rejected
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_delta_init = 1e3; // exactly the ceiling is legal
    EXPECT_NO_THROW(validate_sqp_options(o));
}

TEST(IpqpOptions, RegDecreaseMustBeAStrictFraction) {
    SqpOptions o;
    o.ipqp.ipqp_reg_decrease = 0.0;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_reg_decrease = 1.0;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument) << "1 would never decrease";
    o.ipqp.ipqp_reg_decrease = -0.1;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_reg_decrease = kNan;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_reg_decrease = 0.5;
    EXPECT_NO_THROW(validate_sqp_options(o));
}

TEST(IpqpOptions, TauMustBeAStrictFractionToBoundaryParameter) {
    SqpOptions o;
    o.ipqp.ipqp_tau = 0.0;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_tau = 1.0;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_tau = kNan;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_tau = 0.995;
    EXPECT_NO_THROW(validate_sqp_options(o));
}

TEST(IpqpOptions, FaceKappaMustLieStrictlyBetweenZeroAndOne) {
    // Fix round 1: tightened from finite-and-positive to strictly (0, 1) -- at kappa >= 1 the
    // ratio rule's active test (s_j < kappa*z_j) and inactive test (z_j < kappa*s_j) stop being
    // mutually exclusive, so equal slack and dual values would satisfy both.
    SqpOptions o;
    o.ipqp.ipqp_face_kappa = 0.0;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_face_kappa = -1e-2;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_face_kappa = 1.0;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument)
        << "at kappa == 1 the two ratio tests coincide at equality";
    o.ipqp.ipqp_face_kappa = 2.0;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument)
        << "above 1 the active and inactive tests overlap";
    o.ipqp.ipqp_face_kappa = kInf;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_face_kappa = kNan;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_face_kappa = 1e-2;
    EXPECT_NO_THROW(validate_sqp_options(o));
}

TEST(IpqpOptions, ConvergeSlackMustBeAtLeastOne) {
    SqpOptions o;
    o.ipqp.ipqp_converge_slack = 0.5;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument)
        << "below 1 would out-ask tier 3's own finish";
    o.ipqp.ipqp_converge_slack = kInf;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_converge_slack = 1.0;
    EXPECT_NO_THROW(validate_sqp_options(o)) << "exactly 1 is the identity slack, and is legal";
}

TEST(IpqpOptions, WarmIterBudgetMustBeNonNegative) {
    SqpOptions o;
    o.ipqp.ipqp_warm_iter_budget = -1;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_warm_iter_budget = 0;
    EXPECT_NO_THROW(validate_sqp_options(o)) << "0 is legal: every warm restart killed on iter 1";
}

TEST(IpqpOptions, MuAdoptFactorMustBeFiniteAndNonNegative) {
    SqpOptions o;
    o.ipqp.ipqp_mu_adopt_factor = -0.1;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_mu_adopt_factor = kInf;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_mu_adopt_factor = kNan;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_mu_adopt_factor = 0.0;
    EXPECT_NO_THROW(validate_sqp_options(o)) << "0 legally disables adoption";
}

TEST(IpqpOptions, StallWindowMustBePositive) {
    SqpOptions o;
    o.ipqp.ipqp_stall_window = 0;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_stall_window = -3;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_stall_window = 5;
    EXPECT_NO_THROW(validate_sqp_options(o));
}

TEST(IpqpOptions, RetireAfterMustBePositive) {
    SqpOptions o;
    o.ipqp.ipqp_retire_after = 0;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_retire_after = -1;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.ipqp.ipqp_retire_after = 3;
    EXPECT_NO_THROW(validate_sqp_options(o));
}

TEST(IpqpOptions, TheShippedDefaultsValidate) {
    // The whole struct's own defaults, unmodified, at qp_mode == kWalk: must
    // validate cleanly, since IpqpOptions fields are checked unconditionally.
    SqpOptions o;
    EXPECT_NO_THROW(validate_sqp_options(o));
}

} // namespace
} // namespace hven::solvers
