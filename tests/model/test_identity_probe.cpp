// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// IdentityProbe: the pair a consumer compares to answer "same structure?" and
// "same point?" in one step, and the candidate-value digest that forms its
// second half.

#include <cstdint>

#include <gtest/gtest.h>

#include "hven/core/types.h"
#include "hven/model/candidate_point.h"
#include "hven/model/structure_identity.h"
#include "support/fake_aggregate.h"

using hven::candidate_value_digest;
using hven::CandidateValues;
using hven::IdentityProbe;
using hven::StructureEpoch;
using hven::Vec;

namespace {

struct ValueBlocks {
    double objective = 0.0;
    Vec equality;
    Vec inequality;

    ValueBlocks(double f, std::initializer_list<double> e, std::initializer_list<double> i)
        : objective(f), equality(static_cast<Eigen::Index>(e.size())),
          inequality(static_cast<Eigen::Index>(i.size())) {
        Eigen::Index k = 0;
        for (const double v : e) {
            equality[k++] = v;
        }
        k = 0;
        for (const double v : i) {
            inequality[k++] = v;
        }
    }

    CandidateValues view() { return CandidateValues{objective, equality, inequality}; }
};

std::uint64_t digest_of(double f, std::initializer_list<double> e,
                        std::initializer_list<double> i) {
    ValueBlocks blocks(f, e, i);
    return candidate_value_digest(blocks.view());
}

} // namespace

TEST(IdentityProbeTest, EqualHalvesCompareEqual) {
    const IdentityProbe a{StructureEpoch(3), 42};
    const IdentityProbe b{StructureEpoch(3), 42};
    EXPECT_EQ(a, b);
}

TEST(IdentityProbeTest, SameDigestUnderADifferentEpochIsNotTheSameProbe) {
    const IdentityProbe a{StructureEpoch(3), 42};
    const IdentityProbe b{StructureEpoch(4), 42};
    EXPECT_NE(a, b);
}

TEST(IdentityProbeTest, SameEpochWithADifferentDigestIsNotTheSameProbe) {
    const IdentityProbe a{StructureEpoch(3), 42};
    const IdentityProbe b{StructureEpoch(3), 43};
    EXPECT_NE(a, b);
}

TEST(IdentityProbeTest, DefaultsToNothingLaidAndNoValues) {
    const IdentityProbe probe;
    EXPECT_EQ(probe.epoch, StructureEpoch());
    EXPECT_EQ(probe.value_digest, 0u);
}

TEST(CandidateValueDigestTest, SameValuesHashEqual) {
    EXPECT_EQ(digest_of(1.5, {1.0, 2.0}, {3.0}), digest_of(1.5, {1.0, 2.0}, {3.0}));
}

TEST(CandidateValueDigestTest, ADifferentObjectiveMovesTheDigest) {
    EXPECT_NE(digest_of(1.5, {1.0, 2.0}, {3.0}), digest_of(1.5000001, {1.0, 2.0}, {3.0}));
}

TEST(CandidateValueDigestTest, ADifferentResidualMovesTheDigest) {
    EXPECT_NE(digest_of(1.5, {1.0, 2.0}, {3.0}), digest_of(1.5, {1.0, 2.5}, {3.0}));
    EXPECT_NE(digest_of(1.5, {1.0, 2.0}, {3.0}), digest_of(1.5, {1.0, 2.0}, {3.5}));
}

TEST(CandidateValueDigestTest, TheBlockSplitIsPartOfTheStream) {
    // The same numbers in different blocks are different candidate values: the
    // stream is self-delimiting because each block's length is fed with it.
    EXPECT_NE(digest_of(1.5, {1.0, 2.0}, {3.0}), digest_of(1.5, {1.0}, {2.0, 3.0}));
}

TEST(CandidateValueDigestTest, TheDigestIsBitwiseNotNumeric) {
    // Bitwise over the value bytes, deliberately: +0.0 and -0.0 compare equal
    // numerically and hash apart here. The digest answers "is this the same
    // point?" for a probe taken in the same process, not "are these numerically
    // equal?".
    EXPECT_NE(digest_of(0.0, {0.0}, {0.0}), digest_of(-0.0, {0.0}, {0.0}));
}

TEST(AggregateProbeContract, TheProbeCarriesTheCurrentStructureEpoch) {
    hven::model_tests::FakeAggregate aggregate;
    Vec x = Vec::Zero(hven::model_tests::FakeAggregate::kPrimalVars);

    const IdentityProbe before = aggregate.probe_identity(x);
    EXPECT_EQ(before.epoch, aggregate.structure_epoch());

    aggregate.relay_structures();
    const IdentityProbe after = aggregate.probe_identity(x);
    EXPECT_EQ(after.epoch, aggregate.structure_epoch());
    EXPECT_NE(before, after);
    // Same point, so only the structural half moved.
    EXPECT_EQ(before.value_digest, after.value_digest);
}

TEST(AggregateProbeContract, TheProbeIsAValuesEvaluationPlusAHash) {
    hven::model_tests::FakeAggregate aggregate;
    Vec x = Vec::Zero(hven::model_tests::FakeAggregate::kPrimalVars);
    const int before = aggregate.values_calls();
    aggregate.probe_identity(x);
    EXPECT_EQ(aggregate.values_calls(), before + 1);
}

TEST(AggregateProbeContract, ADifferentPointMovesTheValueHalf) {
    hven::model_tests::FakeAggregate aggregate;
    Vec x = Vec::Zero(hven::model_tests::FakeAggregate::kPrimalVars);
    Vec y = Vec::Constant(hven::model_tests::FakeAggregate::kPrimalVars, 2.0);

    const IdentityProbe at_x = aggregate.probe_identity(x);
    const IdentityProbe at_y = aggregate.probe_identity(y);
    EXPECT_EQ(at_x.epoch, at_y.epoch);
    EXPECT_NE(at_x.value_digest, at_y.value_digest);
}
