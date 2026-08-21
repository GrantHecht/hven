// ModelStructureKey: its equality, its folded digest, its partition-count
// conjunct, and the two digest streams it is composed from -- the declared
// claim stream in claim order, and the variable bounds' STRUCTURE.

#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

#include "hven/core/pattern_hash.h"
#include "hven/model/aggregate_declaration.h"
#include "hven/model/structure_identity.h"

using hven::Fnv1a;
using hven::solvers::feed_claim;
using hven::solvers::feed_variable_bound;
using hven::solvers::ModelStructureKey;
using hven::solvers::VariableBound;

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

ModelStructureKey make_key(std::uint64_t claims, int partitions, std::uint64_t bounds) {
    ModelStructureKey key;
    key.claim_digest_ = claims;
    key.partition_count_ = partitions;
    key.bound_digest_ = bounds;
    return key;
}

std::uint64_t digest_of(const VariableBound &bound) {
    Fnv1a h;
    feed_variable_bound(h, bound);
    return h.value();
}

} // namespace

TEST(ModelStructureKeyTest, EqualFieldsCompareEqual) {
    EXPECT_EQ(make_key(11, 2, 33), make_key(11, 2, 33));
    EXPECT_EQ(make_key(11, 2, 33).digest(), make_key(11, 2, 33).digest());
}

TEST(ModelStructureKeyTest, DefaultsToAKeyOfNothing) {
    const ModelStructureKey key;
    EXPECT_EQ(key.claim_digest_, 0u);
    EXPECT_EQ(key.partition_count_, 0);
    EXPECT_EQ(key.bound_digest_, 0u);
}

TEST(ModelStructureKeyTest, PartitionCountIsAConjunct) {
    EXPECT_NE(make_key(11, 2, 33), make_key(11, 3, 33));
    EXPECT_NE(make_key(11, 2, 33).digest(), make_key(11, 3, 33).digest());
}

TEST(ModelStructureKeyTest, ClaimDigestIsAConjunct) {
    EXPECT_NE(make_key(11, 2, 33), make_key(12, 2, 33));
    EXPECT_NE(make_key(11, 2, 33).digest(), make_key(12, 2, 33).digest());
}

TEST(ModelStructureKeyTest, BoundDigestIsAConjunct) {
    EXPECT_NE(make_key(11, 2, 33), make_key(11, 2, 34));
    EXPECT_NE(make_key(11, 2, 33).digest(), make_key(11, 2, 34).digest());
}

TEST(ModelStructureKeyTest, DigestIsStableAcrossCalls) {
    const ModelStructureKey key = make_key(1234567, 4, 7654321);
    EXPECT_EQ(key.digest(), key.digest());
}

TEST(ModelStructureKeyTest, TheFoldedDigestIsNotAnyOneConjunct) {
    const ModelStructureKey key = make_key(11, 2, 33);
    EXPECT_NE(key.digest(), key.claim_digest_);
    EXPECT_NE(key.digest(), key.bound_digest_);
}

TEST(ClaimStreamDigest, OrderIsSignificant) {
    Fnv1a forward;
    feed_claim(forward, 0, 1);
    feed_claim(forward, 2, 3);

    Fnv1a reversed;
    feed_claim(reversed, 2, 3);
    feed_claim(reversed, 0, 1);

    EXPECT_NE(forward.value(), reversed.value());
}

TEST(ClaimStreamDigest, DistinctClaimsHashDistinctly) {
    Fnv1a a;
    feed_claim(a, 4, 5);
    Fnv1a b;
    feed_claim(b, 5, 4);
    EXPECT_NE(a.value(), b.value());
}

TEST(ClaimStreamDigest, TheSameStreamHashesEqual) {
    Fnv1a a;
    Fnv1a b;
    for (int i = 0; i < 16; ++i) {
        feed_claim(a, i, i / 2);
        feed_claim(b, i, i / 2);
    }
    EXPECT_EQ(a.value(), b.value());
}

TEST(BoundStructureDigest, FiniteValuesDoNotMoveTheDigest) {
    // Two bounds on the same variable, both two-sided and neither fixing: the
    // LAYOUT is the same, so the structural digest is the same. Bound VALUES
    // are not part of the claim structure.
    EXPECT_EQ(digest_of(VariableBound{3, -1.0, 1.0}), digest_of(VariableBound{3, -50.0, 2.5}));
}

TEST(BoundStructureDigest, TheBoundedSideIsPartOfTheStructure) {
    EXPECT_NE(digest_of(VariableBound{3, -1.0, kInf}), digest_of(VariableBound{3, -kInf, 1.0}));
    EXPECT_NE(digest_of(VariableBound{3, -1.0, 1.0}), digest_of(VariableBound{3, -1.0, kInf}));
}

TEST(BoundStructureDigest, AVariableBecomingFixedMovesTheDigest) {
    // lower == upper is the one value change that DOES move the layout: the
    // variable can be eliminated from the solved system entirely.
    EXPECT_NE(digest_of(VariableBound{3, 2.0, 2.0}), digest_of(VariableBound{3, 2.0, 3.0}));
}

TEST(BoundStructureDigest, TheVariableIndexIsPartOfTheStructure) {
    EXPECT_NE(digest_of(VariableBound{3, -1.0, 1.0}), digest_of(VariableBound{4, -1.0, 1.0}));
}

TEST(BoundStructureDigest, OrderIsSignificant) {
    Fnv1a forward;
    feed_variable_bound(forward, VariableBound{1, 0.0, 1.0});
    feed_variable_bound(forward, VariableBound{2, 0.0, kInf});

    Fnv1a reversed;
    feed_variable_bound(reversed, VariableBound{2, 0.0, kInf});
    feed_variable_bound(reversed, VariableBound{1, 0.0, 1.0});

    EXPECT_NE(forward.value(), reversed.value());
}
