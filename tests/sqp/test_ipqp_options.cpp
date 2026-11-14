// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_ipqp_options.cpp -- M6 W1 task 1: the mode/options surface for the kIpqp tier. What this
// file pins is IpqpOptions field validation in validate, one pin per predicate
// class; the kIpqp dispatch pin now lives in tests/sqp/test_ipqp_dispatch.cpp (task 6 landed it).

#include <limits>
#include <stdexcept>

#include <gtest/gtest.h>

#include <hven/detail/drivers/sqp_solver_impl.h>
#include <hven/drivers/sqp_solver_types.h>

namespace hven {
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
    ipm.qp_mode = QpMode::kIpqp;
    EXPECT_NO_THROW(validate(ipm));

    SqpOptions defaulted;
    EXPECT_EQ(defaulted.qp_mode, QpMode::kWalk);
    EXPECT_NO_THROW(validate(defaulted));

    SqpOptions bad_at_kwalk;
    bad_at_kwalk.advanced.ipqp.hard_max_iterations = 0;
    EXPECT_THROW(validate(bad_at_kwalk), std::invalid_argument);

    // AND THE ENUMERATOR-COUNT SENTINEL IS NOT A MODE (fix round 3): a legal `QpMode` value that
    // names no kernel, existing so a fourth mode cannot be added without a dispatch arm. It is
    // refused here at construction, not at a dispatch it can then never reach.
    SqpOptions sentinel;
    sentinel.qp_mode = static_cast<QpMode>(3);
    EXPECT_THROW(validate(sentinel), std::invalid_argument);
    EXPECT_THROW(SqpSolver{sentinel}, std::invalid_argument)
        << "and the constructor validates, so no solve can carry it";

    // The three real modes are all accepted, so the refusal above is about the
    // sentinel rather than about `qp_mode` being validated at all.
    for (QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpqp}) {
        SqpOptions real;
        real.qp_mode = mode;
        EXPECT_NO_THROW(validate(real)) << "mode " << static_cast<int>(mode) << " names a kernel";
    }
}

// M7 W0 T3. THE ELASTIC RUNG'S ENGINE: the default, and the sentinel refusal on
// `qp_mode`'s own terms.
//
// THE DEFAULT IS PART OF THE CONTRACT, not an implementation detail: `kWalk` is
// what every pinned counter in this suite was derived under, and a silent flip
// would move all of them at once.
TEST(IpqpOptions, TheElasticRungEngineDefaultsToTheWalkAndRefusesTheCountSentinel) {
    SqpOptions defaulted;
    EXPECT_EQ(defaulted.advanced.elastic_rung_engine, SqpElasticRungEngine::kWalk)
        << "M7 W0 T3 shipped the tier-solved rung behind a toggle that is OFF: the measured win "
           "is on a CONSTRUCTED family, not on the suite's TRAIN split, which is what the M7 "
           "exit criterion asks for";
    EXPECT_NO_THROW(validate(defaulted));

    SqpOptions sentinel;
    sentinel.advanced.elastic_rung_engine = static_cast<SqpElasticRungEngine>(2);
    EXPECT_THROW(validate(sentinel), std::invalid_argument);
    EXPECT_THROW(SqpSolver{sentinel}, std::invalid_argument)
        << "and the constructor validates, so no solve can carry it to the rung dispatch";

    // BOTH REAL ENGINES ARE ACCEPTED IN EVERY MODE, which is the field's own contract: the
    // elastic ladder is reachable from the walk route's kInfeasible consumer too, not only
    // from the kIpqp arm's certified fallback, so this is deliberately NOT gated on qp_mode.
    for (SqpElasticRungEngine rung : {SqpElasticRungEngine::kWalk, SqpElasticRungEngine::kTier}) {
        for (QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpqp}) {
            SqpOptions real;
            real.advanced.elastic_rung_engine = rung;
            real.qp_mode = mode;
            EXPECT_NO_THROW(validate(real)) << "rung engine " << static_cast<int>(rung)
                                            << " in mode " << static_cast<int>(mode);
        }
    }
}

TEST(IpqpOptions, HardIterCapMustBePositive) {
    SqpOptions o;
    o.advanced.ipqp.hard_max_iterations = 0;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.hard_max_iterations = -1;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.hard_max_iterations = 1;
    EXPECT_NO_THROW(validate(o));
}

TEST(IpqpOptions, MuBandMustBeWellFormed) {
    // ipqp_min_mu: finite, > 0.
    {
        SqpOptions o;
        o.advanced.ipqp.barrier_min = 0.0;
        EXPECT_THROW(validate(o), std::invalid_argument);
        o.advanced.ipqp.barrier_min = -1.0;
        EXPECT_THROW(validate(o), std::invalid_argument);
        o.advanced.ipqp.barrier_min = kNan;
        EXPECT_THROW(validate(o), std::invalid_argument);
        o.advanced.ipqp.barrier_min = kInf;
        EXPECT_THROW(validate(o), std::invalid_argument);
    }
    // ipqp_init_mu: finite, > 0.
    {
        SqpOptions o;
        o.advanced.ipqp.barrier_initial = 0.0;
        EXPECT_THROW(validate(o), std::invalid_argument);
        o.advanced.ipqp.barrier_initial = kNan;
        EXPECT_THROW(validate(o), std::invalid_argument);
        o.advanced.ipqp.barrier_initial = kInf;
        EXPECT_THROW(validate(o), std::invalid_argument);
    }
    // The pair: min_mu must not exceed init_mu (the clamp band would invert).
    {
        SqpOptions o;
        o.advanced.ipqp.barrier_min = 1.0;
        o.advanced.ipqp.barrier_initial = 0.5;
        EXPECT_THROW(validate(o), std::invalid_argument);
        o.advanced.ipqp.barrier_min = 0.5;
        o.advanced.ipqp.barrier_initial = 0.5; // equality is the identity band, legal
        EXPECT_NO_THROW(validate(o));
    }
}

TEST(IpqpOptions, RegularizationFloorAndCeilingMustBeWellFormed) {
    // ipqp_reg_floor: finite, > 0.
    {
        SqpOptions o;
        o.advanced.ipqp.regularization_min = 0.0;
        EXPECT_THROW(validate(o), std::invalid_argument);
        o.advanced.ipqp.regularization_min = kNan;
        EXPECT_THROW(validate(o), std::invalid_argument);
    }
    // ipqp_reg_max: finite, >= ipqp_reg_floor.
    {
        SqpOptions o;
        o.advanced.ipqp.regularization_min = 1.0;
        o.advanced.ipqp.regularization_max = 0.5;
        EXPECT_THROW(validate(o), std::invalid_argument);
        o.advanced.ipqp.regularization_max = kInf;
        EXPECT_THROW(validate(o), std::invalid_argument);
        o.advanced.ipqp.regularization_max = 1.0; // equality legal: "never grow"
        o.advanced.ipqp.primal_regularization_initial = 1.0;
        o.advanced.ipqp.dual_regularization_initial =
            1.0; // keep both inits inside the now-narrow band
        EXPECT_NO_THROW(validate(o));
    }
}

TEST(IpqpOptions, RhoInitMustStartInsideTheRegularizationBand) {
    // primal_regularization_initial IS tied to the floor -- spec section 2.2's (M6 W1 T4b-deleted)
    // monotone floor was
    // stated for rho.
    SqpOptions o;
    o.advanced.ipqp.regularization_min = 1e-6;
    o.advanced.ipqp.regularization_max = 1e3;
    o.advanced.ipqp.primal_regularization_initial = 1e-7; // below the floor
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.primal_regularization_initial = 1e4; // above the ceiling
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.primal_regularization_initial = 1.0;
    EXPECT_NO_THROW(validate(o));
}

TEST(IpqpOptions, DeltaInitMustBePositiveAndNoLargerThanTheCeilingButIsNotTiedToTheFloor) {
    // Fix round 1 (plan section 7 note g): dual_regularization_initial is NOT bound to
    // ipqp_reg_floor -- only rho carries the monotone-floor invariant (spec 2.2) -- so a value
    // below the deliberately high floor set here must still be ACCEPTED.
    SqpOptions o;
    o.advanced.ipqp.regularization_min = 1e-6;
    o.advanced.ipqp.regularization_max = 1e3;
    o.advanced.ipqp.dual_regularization_initial =
        1e-7; // below the floor, but the floor does not bind delta
    EXPECT_NO_THROW(validate(o));
    o.advanced.ipqp.dual_regularization_initial = 0.0; // non-positive is still rejected
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.dual_regularization_initial = -1.0;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.dual_regularization_initial = kNan;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.dual_regularization_initial = 1e4; // above the ceiling is still rejected
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.dual_regularization_initial = 1e3; // exactly the ceiling is legal
    EXPECT_NO_THROW(validate(o));
}

TEST(IpqpOptions, RegDecreaseMustBeAStrictFraction) {
    SqpOptions o;
    o.advanced.ipqp.regularization_decrease = 0.0;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.regularization_decrease = 1.0;
    EXPECT_THROW(validate(o), std::invalid_argument) << "1 would never decrease";
    o.advanced.ipqp.regularization_decrease = -0.1;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.regularization_decrease = kNan;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.regularization_decrease = 0.5;
    EXPECT_NO_THROW(validate(o));
}

TEST(IpqpOptions, TauMustBeAStrictFractionToBoundaryParameter) {
    SqpOptions o;
    o.advanced.ipqp.fraction_to_boundary = 0.0;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.fraction_to_boundary = 1.0;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.fraction_to_boundary = kNan;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.fraction_to_boundary = 0.995;
    EXPECT_NO_THROW(validate(o));
}

TEST(IpqpOptions, FaceKappaMustLieStrictlyBetweenZeroAndOne) {
    // Fix round 1: tightened from finite-and-positive to strictly (0, 1) -- at kappa >= 1 the
    // ratio rule's active test (s_j < kappa*z_j) and inactive test (z_j < kappa*s_j) stop being
    // mutually exclusive, so equal slack and dual values would satisfy both.
    SqpOptions o;
    o.advanced.ipqp.face_ratio_threshold = 0.0;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.face_ratio_threshold = -1e-2;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.face_ratio_threshold = 1.0;
    EXPECT_THROW(validate(o), std::invalid_argument)
        << "at kappa == 1 the two ratio tests coincide at equality";
    o.advanced.ipqp.face_ratio_threshold = 2.0;
    EXPECT_THROW(validate(o), std::invalid_argument)
        << "above 1 the active and inactive tests overlap";
    o.advanced.ipqp.face_ratio_threshold = kInf;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.face_ratio_threshold = kNan;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.face_ratio_threshold = 1e-2;
    EXPECT_NO_THROW(validate(o));
}

TEST(IpqpOptions, ConvergeSlackMustBeAtLeastOne) {
    SqpOptions o;
    o.advanced.ipqp.convergence_slack = 0.5;
    EXPECT_THROW(validate(o), std::invalid_argument) << "below 1 would out-ask tier 3's own finish";
    o.advanced.ipqp.convergence_slack = kInf;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.convergence_slack = 1.0;
    EXPECT_NO_THROW(validate(o)) << "exactly 1 is the identity slack, and is legal";
}

TEST(IpqpOptions, WarmIterBudgetMustBeNonNegative) {
    SqpOptions o;
    o.advanced.ipqp.max_warm_iterations = -1;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.max_warm_iterations = 0;
    EXPECT_NO_THROW(validate(o)) << "0 is legal: every warm restart killed on iter 1";
}

TEST(IpqpOptions, MuAdoptFactorMustBeFiniteAndNonNegative) {
    SqpOptions o;
    o.advanced.ipqp.barrier_adoption_factor = -0.1;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.barrier_adoption_factor = kInf;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.barrier_adoption_factor = kNan;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.barrier_adoption_factor = 0.0;
    EXPECT_NO_THROW(validate(o)) << "0 legally disables adoption";
}

TEST(IpqpOptions, StallWindowMustBePositive) {
    SqpOptions o;
    o.advanced.ipqp.stall_window = 0;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.stall_window = -3;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.stall_window = 5;
    EXPECT_NO_THROW(validate(o));
}

TEST(IpqpOptions, RetireAfterMustBePositive) {
    SqpOptions o;
    o.advanced.ipqp.max_consecutive_escapes = 0;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.max_consecutive_escapes = -1;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.advanced.ipqp.max_consecutive_escapes = 3;
    EXPECT_NO_THROW(validate(o));
}

TEST(IpqpOptions, TheShippedDefaultsValidate) {
    // The whole struct's own defaults, unmodified, at qp_mode == kWalk: must
    // validate cleanly, since IpqpOptions fields are checked unconditionally.
    SqpOptions o;
    EXPECT_NO_THROW(validate(o));
}

} // namespace
} // namespace hven
