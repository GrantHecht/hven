// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_ipqp_options.cpp — M6 W1 task 1: the mode/options surface for the
// kIpm tier. Landed inert -- no engine, no counters, no dispatch -- so this
// file pins exactly the two things that exist yet:
//
//   (1) IpqpOptions' fields are boundary-validated in validate_sqp_options,
//       one pin per predicate class, following test_problem_scaling.cpp's
//       ProblemScalingOptions.TheRuleIsValidatedAtTheBoundary pattern.
//   (2) qp_mode == QpMode::kIpm is TEMPORARILY refused at validate_sqp_options
//       ("not yet dispatchable") -- removed once the routing chain lands in a
//       later W1 task. This is the pin that must be DELETED (not just
//       adjusted) when that happens, so its own name says so.

#include <limits>
#include <stdexcept>

#include <gtest/gtest.h>

#include <hven/drivers/sqp_types.h>

namespace hven::solvers {
namespace {

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// ===========================================================================
// (2) THE TEMPORARY kIpm GATE.
// ===========================================================================

TEST(IpqpOptions, KIpmIsNotYetDispatchableDeleteThisPinWhenTheRoutingChainLands) {
    SqpOptions o;
    o.qp_mode = QpMode::kIpm;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);

    // The default stays kWalk, and a default-constructed IpqpOptions changes
    // nothing about that: qp_mode alone is what the temporary gate reads.
    SqpOptions defaulted;
    EXPECT_EQ(defaulted.qp_mode, QpMode::kWalk);
    EXPECT_NO_THROW(validate_sqp_options(defaulted));
}

// ===========================================================================
// (1) IpqpOptions FIELD VALIDATION, ONE PREDICATE CLASS PER PIN.
//
// Every arm below leaves qp_mode at its kWalk default, so the temporary kIpm
// gate above never fires and each pin isolates the field predicate it names.
// ===========================================================================

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
    // rho_init IS tied to the floor -- spec section 2.2's monotone floor is
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
    // Fix round 1 (2026-08-30, plan section 7 note g): delta_init is NOT
    // bound to ipqp_reg_floor -- only rho carries the monotone-floor
    // invariant (spec 2.2) -- so a value below the (deliberately high) floor
    // set here must still be ACCEPTED.
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
    // Fix round 1 (2026-08-30): tightened from "finite and > 0" to strictly
    // (0, 1) -- at kappa >= 1 the ratio rule's active test (s_j < kappa*z_j)
    // and inactive test (z_j < kappa*s_j) stop being mutually exclusive, so
    // equal slack and dual values would satisfy both.
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
