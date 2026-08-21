// ModelStructureKey: its equality, its folded digest, its partition-count
// conjunct, and the two digest streams it is composed from -- the declared
// claim stream, opened by its dimension preamble, and the MATERIALIZED
// variable-bound structure.

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "hven/core/pattern_hash.h"
#include "hven/model/aggregate_declaration.h"
#include "hven/model/structure_identity.h"

using hven::Fnv1a;
using hven::solvers::AggregateDeclaration;
using hven::solvers::feed_claim;
using hven::solvers::feed_dimensions;
using hven::solvers::feed_variable_bound;
using hven::solvers::materialized_bound_digest;
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

/// A declaration over `primal_vars` variables carrying the given bound records
/// in declaration order.
AggregateDeclaration bounded(int primal_vars, std::initializer_list<VariableBound> records) {
    AggregateDeclaration declaration;
    declaration.primal_vars_ = primal_vars;
    declaration.variable_bounds_.assign(records.begin(), records.end());
    return declaration;
}

/// The bound conjunct a declaration's key would carry.
std::uint64_t bound_digest(int primal_vars, std::initializer_list<VariableBound> records) {
    return materialized_bound_digest(bounded(primal_vars, records));
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

TEST(ClaimStreamDigest, TheDimensionPreambleIsPartOfTheStream) {
    // Identical claims at different dimensions. A claim names rows and columns
    // that exist, never the size of the space they live in, so without the
    // preamble these two would key identically while laying different systems.
    Fnv1a narrow;
    feed_dimensions(narrow, 4, 2, 3);
    Fnv1a wide;
    feed_dimensions(wide, 5, 2, 3);
    for (int i = 0; i < 8; ++i) {
        feed_claim(narrow, i, i / 2);
        feed_claim(wide, i, i / 2);
    }
    EXPECT_NE(narrow.value(), wide.value());
}

TEST(ClaimStreamDigest, EachDimensionOfThePreambleIsAConjunct) {
    const auto stream = [](int n, int me, int mi) {
        Fnv1a hash;
        feed_dimensions(hash, n, me, mi);
        feed_claim(hash, 0, 0);
        return hash.value();
    };
    EXPECT_NE(stream(4, 2, 3), stream(5, 2, 3));
    EXPECT_NE(stream(4, 2, 3), stream(4, 1, 3));
    EXPECT_NE(stream(4, 2, 3), stream(4, 2, 4));
    EXPECT_EQ(stream(4, 2, 3), stream(4, 2, 3));
}

TEST(BoundStructureDigest, IsTakenOverTheMaterializedStructure) {
    // Nothing declared: every variable unbounded on both sides, and the digest
    // is a function of the variable count as much as of the records.
    EXPECT_EQ(bound_digest(4, {}), bound_digest(4, {}));
    EXPECT_NE(bound_digest(4, {}), bound_digest(5, {}));
}

TEST(BoundStructureDigest, AnIntersectionThatFixesAVariableRekeys) {
    // [0,1] then [1,2] intersects to the single point 1: the variable is FIXED
    // and can be eliminated from the solved system entirely. [0,1] then [0.5,2]
    // intersects to [0.5,1], which is not. Same records at a glance, different
    // layouts, and so different keys.
    EXPECT_NE(bound_digest(4, {VariableBound{1, 0.0, 1.0}, VariableBound{1, 1.0, 2.0}}),
              bound_digest(4, {VariableBound{1, 0.0, 1.0}, VariableBound{1, 0.5, 2.0}}));
}

TEST(BoundStructureDigest, AValueNudgeWithinOneStructureDoesNotRekey) {
    // [0,1] then [0.9,2] and [0,1] then [0.5,2] intersect to [0.9,1] and
    // [0.5,1]: different numbers, same materialized structure -- two-sided,
    // not fixed -- so the key must not move. This is the property that keeps a
    // bound-value nudge from killing warm reuse.
    EXPECT_EQ(bound_digest(4, {VariableBound{1, 0.0, 1.0}, VariableBound{1, 0.9, 2.0}}),
              bound_digest(4, {VariableBound{1, 0.0, 1.0}, VariableBound{1, 0.5, 2.0}}));
}

TEST(BoundStructureDigest, AnIntersectionThatMakesASideFiniteRekeys) {
    // [0,inf) then (-inf,5] intersects to [0,5] -- both sides finite. [0,inf)
    // then [-1,inf) stays half-open. Intersection changed the finite-side set,
    // which changes which barrier terms exist, so it must re-key.
    EXPECT_NE(bound_digest(4, {VariableBound{1, 0.0, kInf}, VariableBound{1, -kInf, 5.0}}),
              bound_digest(4, {VariableBound{1, 0.0, kInf}, VariableBound{1, -1.0, kInf}}));
}

TEST(BoundStructureDigest, TwoHistoriesWithTheSameIntersectionKeyTheSame) {
    // The rule stated from the other side: what is hashed is the intersection,
    // so the number of records that produced it is invisible.
    EXPECT_EQ(bound_digest(4, {VariableBound{2, -1.0, 1.0}}),
              bound_digest(4, {VariableBound{2, -5.0, 9.0}, VariableBound{2, -1.0, 1.0}}));
}

TEST(BoundStructureDigest, WhichVariableIsBoundedIsPartOfTheStructure) {
    EXPECT_NE(bound_digest(4, {VariableBound{1, 0.0, 1.0}}),
              bound_digest(4, {VariableBound{2, 0.0, 1.0}}));
}

TEST(BoundStructureDigest, ThePerVariableRecordCarriesSideAndFixedness) {
    // The per-variable primitive the materialized stream is built from.
    EXPECT_EQ(digest_of(VariableBound{3, -1.0, 1.0}), digest_of(VariableBound{3, -50.0, 2.5}));
    EXPECT_NE(digest_of(VariableBound{3, -1.0, kInf}), digest_of(VariableBound{3, -kInf, 1.0}));
    EXPECT_NE(digest_of(VariableBound{3, 2.0, 2.0}), digest_of(VariableBound{3, 2.0, 3.0}));
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
