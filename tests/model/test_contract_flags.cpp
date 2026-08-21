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

/// The eight request sets the mapping table names, in the table's own order.
constexpr std::array<EvalRequest, 8> kMappedRequestSets = {
    hven::solvers::kRequestObjectiveOnly,
    hven::solvers::kRequestObjectiveAndConstraints,
    hven::solvers::kRequestObjectiveGradientAndConstraints,
    hven::solvers::kRequestFirstOrderRhs,
    hven::solvers::kRequestConstraintJacobianOnly,
    hven::solvers::kRequestFirstOrderKkt,
    hven::solvers::kRequestConstraintKkt,
    hven::solvers::kRequestFullKkt,
};

} // namespace

TEST(EvalRequestMappingTable, TheEightMappedSetsAreDistinct) {
    for (std::size_t i = 0; i < kMappedRequestSets.size(); ++i) {
        for (std::size_t j = i + 1; j < kMappedRequestSets.size(); ++j) {
            EXPECT_NE(kMappedRequestSets[i], kMappedRequestSets[j])
                << "mapped request sets " << i << " and " << j << " coincide";
        }
    }
}

TEST(EvalRequestMappingTable, NoMappedSetIsEmpty) {
    for (const EvalRequest set : kMappedRequestSets) {
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

TEST(EvalRequestMappingTable, OnlyTheFullKktShapeNamesTheObjectiveHessian) {
    for (const EvalRequest set : kMappedRequestSets) {
        if (set == hven::solvers::kRequestFullKkt) {
            EXPECT_TRUE(has_request(set, EvalRequest::kObjectiveHessian));
        } else {
            EXPECT_FALSE(has_request(set, EvalRequest::kObjectiveHessian));
        }
    }
}

TEST(EvalRequestMappingTable, EveryShapeThatNamesADerivativeAlsoNamesItsValues) {
    // No legacy shape computes a constraint derivative without producing the
    // residual blocks on the way: the piece methods that produce one produce
    // the other. A request set that named a derivative alone would be a shape
    // the engine never had.
    constexpr EvalRequest kConstraintDerivatives = EvalRequest::kConstraintAdjointGradient |
                                                   EvalRequest::kConstraintJacobian |
                                                   EvalRequest::kConstraintAdjointHessian;
    for (const EvalRequest set : kMappedRequestSets) {
        if ((set & kConstraintDerivatives) != EvalRequest::kNone) {
            EXPECT_TRUE(has_request(set, EvalRequest::kConstraintValues));
        }
    }
    // Likewise the objective: every shape producing its gradient produces its
    // value, because one call produces both.
    for (const EvalRequest set : kMappedRequestSets) {
        if (has_request(set, EvalRequest::kObjectiveGradient)) {
            EXPECT_TRUE(has_request(set, EvalRequest::kObjectiveValue));
        }
    }
}

TEST(EvalRequestMappingTable, TheFullKktShapeSubsumesEveryOtherMappedShape) {
    for (const EvalRequest set : kMappedRequestSets) {
        EXPECT_TRUE(has_request(hven::solvers::kRequestFullKkt, set));
    }
}

TEST(EvalRequestMappingTable, ExactlyTheEightMappedSetsAreLegal) {
    for (const EvalRequest set : kMappedRequestSets) {
        EXPECT_TRUE(hven::solvers::is_legal_request(set));
        EXPECT_NO_THROW(hven::solvers::validate_eval_request(set));
    }

    // Every other combination over the seven flags -- and there are many -- is
    // an evaluation shape no piece surface produces in one pass.
    int legal = 0;
    for (std::uint32_t bits = 0; bits < (1u << 7); ++bits) {
        if (hven::solvers::is_legal_request(static_cast<EvalRequest>(bits))) {
            ++legal;
        }
    }
    EXPECT_EQ(legal, 8);
}

TEST(EvalRequestMappingTable, AnUnmappedCombinationIsRefusedByName) {
    // Plausible-looking and still unmapped: a Jacobian without the residuals
    // that every shape producing one also produces.
    const EvalRequest composed = EvalRequest::kConstraintJacobian;
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
