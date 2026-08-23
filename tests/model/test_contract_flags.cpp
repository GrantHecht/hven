// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The three flag vocabularies of the contract surface, and the one place they
// must not be confused: ClaimDomainSet names what is CLAIMED at layout time,
// EvalRequest names what is EVALUATED per call, AggregateCapability names what
// a consumer may assume about cost.
//
// The EvalRequest block also carries the injective half of the mapping table's
// bijectivity claim: the eight legacy evaluation shapes map to eight DISTINCT
// request sets. The surjective half -- that each set computes exactly what its
// legacy shape computed, and no more -- is proved call-for-call when the
// engine is retargeted onto this entry.

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "hven/model/candidate_point.h"
#include "hven/model/claim_space.h"
#include "hven/model/nlp_aggregate.h"

using hven::solvers::AggregateCapability;
using hven::solvers::ClaimDomain;
using hven::solvers::ClaimDomainSet;
using hven::solvers::EvalRequest;
using hven::solvers::has_capability;
using hven::solvers::has_request;
using hven::solvers::KktStorage;

// ---------------------------------------------------------------------------
// ClaimDomainSet
// ---------------------------------------------------------------------------

TEST(ClaimDomainSetTest, EmptyByDefault) {
    const ClaimDomainSet set;
    EXPECT_TRUE(set.empty());
    EXPECT_FALSE(set.contains(ClaimDomain::kHessian));
    EXPECT_FALSE(set.contains(ClaimDomain::kEqualityJacobian));
    EXPECT_FALSE(set.contains(ClaimDomain::kInequalityJacobian));
    EXPECT_FALSE(set.contains(ClaimDomain::kVariableBound));
}

TEST(ClaimDomainSetTest, ASingleDomainConvertsToItsOwnSet) {
    const ClaimDomainSet set = ClaimDomain::kHessian;
    EXPECT_FALSE(set.empty());
    EXPECT_TRUE(set.contains(ClaimDomain::kHessian));
    EXPECT_FALSE(set.contains(ClaimDomain::kEqualityJacobian));
}

TEST(ClaimDomainSetTest, UnionCollectsEveryDomain) {
    const ClaimDomainSet set = ClaimDomain::kEqualityJacobian | ClaimDomain::kInequalityJacobian;
    EXPECT_TRUE(set.contains(ClaimDomain::kEqualityJacobian));
    EXPECT_TRUE(set.contains(ClaimDomain::kInequalityJacobian));
    EXPECT_FALSE(set.contains(ClaimDomain::kHessian));
}

TEST(ClaimDomainSetTest, IntersectionKeepsTheCommonDomains) {
    const ClaimDomainSet left = ClaimDomain::kHessian | ClaimDomain::kEqualityJacobian;
    const ClaimDomainSet right = ClaimDomain::kEqualityJacobian | ClaimDomain::kVariableBound;
    const ClaimDomainSet both = left & right;
    EXPECT_TRUE(both.contains(ClaimDomain::kEqualityJacobian));
    EXPECT_FALSE(both.contains(ClaimDomain::kHessian));
    EXPECT_FALSE(both.contains(ClaimDomain::kVariableBound));
}

TEST(ClaimDomainSetTest, UnionIsIdempotentAndComparable) {
    const ClaimDomainSet once = ClaimDomain::kHessian;
    ClaimDomainSet twice = ClaimDomain::kHessian;
    twice |= ClaimDomain::kHessian;
    EXPECT_EQ(once, twice);
    EXPECT_NE(once, ClaimDomainSet());
}

TEST(ClaimDomainSetTest, AllCarriesEveryDomain) {
    const ClaimDomainSet all = ClaimDomainSet::all();
    EXPECT_TRUE(all.contains(ClaimDomain::kHessian));
    EXPECT_TRUE(all.contains(ClaimDomain::kEqualityJacobian));
    EXPECT_TRUE(all.contains(ClaimDomain::kInequalityJacobian));
    EXPECT_TRUE(all.contains(ClaimDomain::kVariableBound));
}

TEST(KktStorageTest, AClaimSpaceResolvesStorageAtClaimTimeNotOnTheHotPath) {
    Eigen::VectorXi rows(4);
    Eigen::VectorXi cols(4);
    hven::solvers::KktClaimSpace space{rows, cols};
    EXPECT_EQ(space.storage_, KktStorage::kUpperTriangle);
    EXPECT_EQ(space.next_free_, 0);
    EXPECT_EQ(space.constraint_row_offset_, 0);
    EXPECT_TRUE(space.domains_.empty());
}

// ---------------------------------------------------------------------------
// AggregateCapability
// ---------------------------------------------------------------------------

TEST(AggregateCapabilityTest, NoneCarriesNothing) {
    EXPECT_FALSE(has_capability(AggregateCapability::kNone, AggregateCapability::kDirectScatter));
    EXPECT_FALSE(has_capability(AggregateCapability::kNone, AggregateCapability::kValuesFastPath));
}

TEST(AggregateCapabilityTest, ProbingForNothingIsVacuouslyTrue) {
    EXPECT_TRUE(has_capability(AggregateCapability::kNone, AggregateCapability::kNone));
    EXPECT_TRUE(has_capability(AggregateCapability::kDirectScatter, AggregateCapability::kNone));
}

TEST(AggregateCapabilityTest, HasCapabilityRequiresEveryProbedBit) {
    const AggregateCapability both =
        AggregateCapability::kDirectScatter | AggregateCapability::kValuesFastPath;
    EXPECT_TRUE(has_capability(both, AggregateCapability::kDirectScatter));
    EXPECT_TRUE(has_capability(both, AggregateCapability::kValuesFastPath));
    EXPECT_TRUE(has_capability(both, both));

    const AggregateCapability one = AggregateCapability::kValuesFastPath;
    EXPECT_TRUE(has_capability(one, AggregateCapability::kValuesFastPath));
    EXPECT_FALSE(has_capability(one, AggregateCapability::kDirectScatter));
    EXPECT_FALSE(has_capability(one, both));
}

TEST(AggregateCapabilityTest, IntersectionKeepsTheCommonBits) {
    const AggregateCapability both =
        AggregateCapability::kDirectScatter | AggregateCapability::kValuesFastPath;
    EXPECT_EQ(both & AggregateCapability::kDirectScatter, AggregateCapability::kDirectScatter);
    EXPECT_EQ(AggregateCapability::kDirectScatter & AggregateCapability::kValuesFastPath,
              AggregateCapability::kNone);
}

TEST(AggregateCapabilityTest, InPlaceUnionAndMaskingBothCompile) {
    AggregateCapability widened = AggregateCapability::kDirectScatter;
    widened |= AggregateCapability::kValuesFastPath;
    EXPECT_TRUE(has_capability(widened, AggregateCapability::kValuesFastPath));

    // Reducing a set to the weakest claim over several -- what a mixed provider
    // does over its pieces -- is in-place masking, so it must compile as one.
    widened &= AggregateCapability::kValuesFastPath;
    EXPECT_EQ(widened, AggregateCapability::kValuesFastPath);
    EXPECT_FALSE(has_capability(widened, AggregateCapability::kDirectScatter));
}

// ---------------------------------------------------------------------------
// EvalRequest and the legacy mapping table
// ---------------------------------------------------------------------------

TEST(EvalRequestTest, NoneNamesNoOutput) {
    EXPECT_FALSE(has_request(EvalRequest::kNone, EvalRequest::kObjectiveValue));
    EXPECT_TRUE(has_request(EvalRequest::kNone, EvalRequest::kNone));
}

TEST(EvalRequestTest, UnionAndIntersection) {
    const EvalRequest set = EvalRequest::kObjectiveValue | EvalRequest::kConstraintValues;
    EXPECT_TRUE(has_request(set, EvalRequest::kObjectiveValue));
    EXPECT_TRUE(has_request(set, EvalRequest::kConstraintValues));
    EXPECT_FALSE(has_request(set, EvalRequest::kObjectiveGradient));
    EXPECT_EQ(set & EvalRequest::kObjectiveValue, EvalRequest::kObjectiveValue);
    EXPECT_EQ(set & EvalRequest::kObjectiveHessian, EvalRequest::kNone);
}

TEST(EvalRequestTest, HasRequestRequiresEveryProbedFlag) {
    const EvalRequest set = EvalRequest::kObjectiveValue | EvalRequest::kConstraintValues;
    EXPECT_FALSE(has_request(set, set | EvalRequest::kObjectiveGradient));
    EXPECT_TRUE(has_request(set, set));
}

TEST(EvalRequestTest, InPlaceUnionAndMaskingBothCompile) {
    EvalRequest widened = EvalRequest::kObjectiveValue;
    widened |= EvalRequest::kConstraintValues;
    EXPECT_EQ(widened, hven::solvers::kRequestObjectiveAndConstraints);

    EvalRequest narrowed = hven::solvers::kRequestFullKkt;
    narrowed &= hven::solvers::kRequestObjectiveAndConstraints;
    EXPECT_EQ(narrowed, hven::solvers::kRequestObjectiveAndConstraints);
}

TEST(EvalRequestTest, TheAdjointRequestsAreTheOnesThatConsumeTheMultipliers) {
    EXPECT_TRUE(
        hven::solvers::request_consumes_multipliers(EvalRequest::kConstraintAdjointGradient));
    EXPECT_TRUE(
        hven::solvers::request_consumes_multipliers(EvalRequest::kConstraintAdjointHessian));
    EXPECT_TRUE(hven::solvers::request_consumes_multipliers(hven::solvers::kRequestFullKkt));
    EXPECT_TRUE(hven::solvers::request_consumes_multipliers(hven::solvers::kRequestConstraintKkt));
    EXPECT_FALSE(hven::solvers::request_consumes_multipliers(hven::solvers::kRequestObjectiveOnly));
    EXPECT_FALSE(
        hven::solvers::request_consumes_multipliers(hven::solvers::kRequestConstraintJacobianOnly));
    EXPECT_FALSE(hven::solvers::request_consumes_multipliers(
        hven::solvers::kRequestObjectiveGradientAndConstraints));
}

namespace {

/// The eight request sets the partitioned evaluation engine owns, in the
/// mapping table's own order (rows 1-8).
constexpr std::array<EvalRequest, 8> kInteriorOwnedRequestSets = {
    hven::solvers::kRequestObjectiveOnly,
    hven::solvers::kRequestObjectiveAndConstraints,
    hven::solvers::kRequestObjectiveGradientAndConstraints,
    hven::solvers::kRequestFirstOrderRhs,
    hven::solvers::kRequestConstraintJacobianOnly,
    hven::solvers::kRequestFirstOrderKkt,
    hven::solvers::kRequestConstraintKkt,
    hven::solvers::kRequestFullKkt,
};

/// The three request sets the SQP driver owns (rows 9-11), added at its own
/// Level 2 consumption task.
constexpr std::array<EvalRequest, 3> kSqpOwnedRequestSets = {
    hven::solvers::kRequestLagrangianHessian,
    hven::solvers::kRequestGradientAndJacobians,
    hven::solvers::kRequestConstraintJacobiansOnly,
};

/// The union of both families -- every set the mapping table names -- for the
/// invariants that hold across the whole table rather than within one family.
constexpr std::array<EvalRequest, 11> kAllMappedRequestSets = {
    hven::solvers::kRequestObjectiveOnly,
    hven::solvers::kRequestObjectiveAndConstraints,
    hven::solvers::kRequestObjectiveGradientAndConstraints,
    hven::solvers::kRequestFirstOrderRhs,
    hven::solvers::kRequestConstraintJacobianOnly,
    hven::solvers::kRequestFirstOrderKkt,
    hven::solvers::kRequestConstraintKkt,
    hven::solvers::kRequestFullKkt,
    hven::solvers::kRequestLagrangianHessian,
    hven::solvers::kRequestGradientAndJacobians,
    hven::solvers::kRequestConstraintJacobiansOnly,
};

} // namespace

TEST(EvalRequestMappingTable, AllMappedSetsAreDistinct) {
    for (std::size_t i = 0; i < kAllMappedRequestSets.size(); ++i) {
        for (std::size_t j = i + 1; j < kAllMappedRequestSets.size(); ++j) {
            EXPECT_NE(kAllMappedRequestSets[i], kAllMappedRequestSets[j])
                << "mapped request sets " << i << " and " << j << " coincide";
        }
    }
}

TEST(EvalRequestMappingTable, NoMappedSetIsEmpty) {
    for (const EvalRequest set : kAllMappedRequestSets) {
        EXPECT_NE(set, EvalRequest::kNone);
    }
}

TEST(EvalRequestMappingTable, TheObjectiveOnlyShapeNamesNothingElse) {
    EXPECT_EQ(hven::solvers::kRequestObjectiveOnly, EvalRequest::kObjectiveValue);
}

TEST(EvalRequestMappingTable, TheConstraintOnlyShapesNameNoObjectiveOutput) {
    constexpr EvalRequest kObjectiveOutputs = EvalRequest::kObjectiveValue |
                                              EvalRequest::kObjectiveGradient |
                                              EvalRequest::kObjectiveHessian;
    EXPECT_EQ(hven::solvers::kRequestConstraintJacobianOnly & kObjectiveOutputs,
              EvalRequest::kNone);
    EXPECT_EQ(hven::solvers::kRequestConstraintKkt & kObjectiveOutputs, EvalRequest::kNone);
}

TEST(EvalRequestMappingTable, OnlyTheFullKktShapeNamesTheObjectiveHessianAmongInteriorOwnedShapes) {
    // Interior-owned only: the SQP-owned shape 9 also names the objective
    // Hessian (it is half of kRequestLagrangianHessian), so this invariant does
    // not extend to the union -- it is a fact about the eight-shape family the
    // partitioned engine grew, not about the mapping table as a whole.
    for (const EvalRequest set : kInteriorOwnedRequestSets) {
        if (set == hven::solvers::kRequestFullKkt) {
            EXPECT_TRUE(has_request(set, EvalRequest::kObjectiveHessian));
        } else {
            EXPECT_FALSE(has_request(set, EvalRequest::kObjectiveHessian));
        }
    }
}

TEST(EvalRequestMappingTable, EveryShapeThatNamesADerivativeAlsoNamesItsValues) {
    // No interior-owned shape computes a constraint derivative without
    // producing the residual blocks on the way: the piece methods that produce
    // one produce the other. A request set that named a derivative alone would
    // be a shape the engine never had. This is an interior-family invariant --
    // the SQP-owned shapes deliberately violate it, which is exactly why they
    // needed their own rows (design note Sec. 4): each omits the values its
    // consumer already holds.
    constexpr EvalRequest kConstraintDerivatives = EvalRequest::kConstraintAdjointGradient |
                                                   EvalRequest::kConstraintJacobian |
                                                   EvalRequest::kConstraintAdjointHessian;
    for (const EvalRequest set : kInteriorOwnedRequestSets) {
        if ((set & kConstraintDerivatives) != EvalRequest::kNone) {
            EXPECT_TRUE(has_request(set, EvalRequest::kConstraintValues));
        }
    }
    // Likewise the objective: every interior-owned shape producing its
    // gradient produces its value, because one call produces both.
    for (const EvalRequest set : kInteriorOwnedRequestSets) {
        if (has_request(set, EvalRequest::kObjectiveGradient)) {
            EXPECT_TRUE(has_request(set, EvalRequest::kObjectiveValue));
        }
    }

    // The companion half: each SQP-owned set does NOT name the values its
    // family deliberately omits.
    // Shape 9 names neither value kind -- the Lagrangian Hessian alone reads
    // no residual and no objective value.
    EXPECT_FALSE(
        has_request(hven::solvers::kRequestLagrangianHessian, EvalRequest::kObjectiveValue));
    EXPECT_FALSE(
        has_request(hven::solvers::kRequestLagrangianHessian, EvalRequest::kConstraintValues));
    // Shape 10 names no constraint values and no objective value -- its values
    // were evaluated on the values-only path and are deliberately not
    // re-evaluated here.
    EXPECT_FALSE(
        has_request(hven::solvers::kRequestGradientAndJacobians, EvalRequest::kConstraintValues));
    EXPECT_FALSE(
        has_request(hven::solvers::kRequestGradientAndJacobians, EvalRequest::kObjectiveValue));
    // Shape 11 names no values at all.
    EXPECT_FALSE(
        has_request(hven::solvers::kRequestConstraintJacobiansOnly, EvalRequest::kObjectiveValue));
    EXPECT_FALSE(has_request(hven::solvers::kRequestConstraintJacobiansOnly,
                             EvalRequest::kConstraintValues));
}

TEST(EvalRequestMappingTable, TheFullKktShapeSubsumesEveryOtherMappedShape) {
    // Family-neutral: kRequestFullKkt is the OR of all seven flags, so it
    // subsumes every legal subset regardless of which family owns it.
    for (const EvalRequest set : kAllMappedRequestSets) {
        EXPECT_TRUE(has_request(hven::solvers::kRequestFullKkt, set));
    }
}

TEST(EvalRequestMappingTable, ExactlyTheMappedSetsAreLegal) {
    for (const EvalRequest set : kAllMappedRequestSets) {
        EXPECT_TRUE(hven::solvers::is_legal_request(set));
        EXPECT_NO_THROW(hven::solvers::validate_eval_request(set));
    }

    // Every other combination over the seven flags -- and there are many -- is
    // an evaluation shape no consumer has.
    int legal = 0;
    for (std::uint32_t bits = 0; bits < (1u << 7); ++bits) {
        if (hven::solvers::is_legal_request(static_cast<EvalRequest>(bits))) {
            ++legal;
        }
    }
    EXPECT_EQ(legal, 11);
}

TEST(EvalRequestMappingTable, AnUnmappedCombinationIsRefusedByName) {
    // Plausible-looking and still unmapped. Bare kConstraintJacobian used to be
    // this example, but the SQP-owned amendment made it shape 11
    // (kRequestConstraintJacobiansOnly) and therefore legal; the objective
    // Hessian alone is unmapped in both families and stays that way.
    const EvalRequest composed = EvalRequest::kObjectiveHessian;
    EXPECT_FALSE(hven::solvers::is_legal_request(composed));
    try {
        hven::solvers::validate_eval_request(composed);
        FAIL() << "an unmapped request must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("EvalRequest"), std::string::npos) << message;
        EXPECT_NE(message.find("kRequestFullKkt"), std::string::npos) << message;
        EXPECT_NE(message.find("mapping table"), std::string::npos) << message;
    }
}

TEST(EvalRequestMappingTable, TheEmptyRequestIsNotLegal) {
    EXPECT_FALSE(hven::solvers::is_legal_request(EvalRequest::kNone));
    EXPECT_THROW(hven::solvers::validate_eval_request(EvalRequest::kNone), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// The SQP-owned shapes (rows 9-11): legality, multiplier consumption,
// distinctness -- pinned per shape rather than only through the table-wide
// loops above.
// ---------------------------------------------------------------------------

TEST(EvalRequestMappingTable, ShapeNineIsLegalAndConsumesMultipliers) {
    const EvalRequest shape = hven::solvers::kRequestLagrangianHessian;
    EXPECT_TRUE(hven::solvers::is_legal_request(shape));
    EXPECT_NO_THROW(hven::solvers::validate_eval_request(shape));
    EXPECT_TRUE(hven::solvers::request_consumes_multipliers(shape));
    for (const EvalRequest other : kInteriorOwnedRequestSets) {
        EXPECT_NE(shape, other) << "shape 9 must be distinct from every interior-owned set";
    }
}

TEST(EvalRequestMappingTable, ShapeTenIsLegalAndDoesNotConsumeMultipliers) {
    const EvalRequest shape = hven::solvers::kRequestGradientAndJacobians;
    EXPECT_TRUE(hven::solvers::is_legal_request(shape));
    EXPECT_NO_THROW(hven::solvers::validate_eval_request(shape));
    EXPECT_FALSE(hven::solvers::request_consumes_multipliers(shape));
    for (const EvalRequest other : kInteriorOwnedRequestSets) {
        EXPECT_NE(shape, other) << "shape 10 must be distinct from every interior-owned set";
    }
    EXPECT_NE(shape, hven::solvers::kRequestLagrangianHessian);
    EXPECT_NE(shape, hven::solvers::kRequestConstraintJacobiansOnly);
}

TEST(EvalRequestMappingTable, ShapeElevenIsLegalAndDoesNotConsumeMultipliers) {
    const EvalRequest shape = hven::solvers::kRequestConstraintJacobiansOnly;
    EXPECT_TRUE(hven::solvers::is_legal_request(shape));
    EXPECT_NO_THROW(hven::solvers::validate_eval_request(shape));
    EXPECT_FALSE(hven::solvers::request_consumes_multipliers(shape));
    for (const EvalRequest other : kInteriorOwnedRequestSets) {
        EXPECT_NE(shape, other) << "shape 11 must be distinct from every interior-owned set";
    }
    EXPECT_NE(shape, hven::solvers::kRequestLagrangianHessian);
    EXPECT_NE(shape, hven::solvers::kRequestGradientAndJacobians);
}

TEST(EvalRequestMappingTable, TheThreeSqpOwnedSetsAreMutuallyDistinct) {
    for (std::size_t i = 0; i < kSqpOwnedRequestSets.size(); ++i) {
        for (std::size_t j = i + 1; j < kSqpOwnedRequestSets.size(); ++j) {
            EXPECT_NE(kSqpOwnedRequestSets[i], kSqpOwnedRequestSets[j])
                << "SQP-owned request sets " << i << " and " << j << " coincide";
        }
    }
}
