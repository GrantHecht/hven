// ModelStructureKey: its equality, its folded digest, its partition-count
// conjunct, and the two digest streams it is composed from -- the declared
// claim stream, opened by its dimension preamble, and the MATERIALIZED
// variable-bound structure.

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Core>

#include <gtest/gtest.h>

#include "hven/core/pattern_hash.h"
#include "hven/model/aggregate_declaration.h"
#include "hven/model/structure_identity.h"

using hven::Fnv1a;
using hven::solvers::AggregateDeclaration;
using hven::solvers::claim_stream_digest;
using hven::solvers::materialized_bound_digest;
using hven::solvers::ModelStructureKey;
using hven::solvers::VariableBound;
// The stream primitives are internal on purpose -- the builders above are the
// public paths. These tests reach past that to keep the primitives themselves
// under test; nothing outside this file should.
using hven::solvers::detail::feed_variable_bound;

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

/// A declaration at the given dimensions, with no bounds.
AggregateDeclaration sized(int primal_vars, int equality_rows, int inequality_rows) {
    AggregateDeclaration declaration;
    declaration.primal_vars_ = primal_vars;
    declaration.equality_rows_ = equality_rows;
    declaration.inequality_rows_ = inequality_rows;
    return declaration;
}

/// A claim stream in the shape a claim arena holds one.
struct ClaimStream {
    Eigen::VectorXi rows_;
    Eigen::VectorXi cols_;

    explicit ClaimStream(std::initializer_list<std::pair<int, int>> claims)
        : rows_(static_cast<Eigen::Index>(claims.size())),
          cols_(static_cast<Eigen::Index>(claims.size())) {
        Eigen::Index slot = 0;
        for (const auto &claim : claims) {
            rows_[slot] = claim.first;
            cols_[slot] = claim.second;
            ++slot;
        }
    }
};

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
    const AggregateDeclaration declaration = sized(4, 2, 3);
    const ClaimStream forward({{0, 1}, {2, 3}});
    const ClaimStream reversed({{2, 3}, {0, 1}});
    EXPECT_NE(claim_stream_digest(declaration, forward.rows_, forward.cols_),
              claim_stream_digest(declaration, reversed.rows_, reversed.cols_));
}

TEST(ClaimStreamDigest, DistinctClaimsHashDistinctly) {
    const AggregateDeclaration declaration = sized(6, 2, 3);
    const ClaimStream a({{4, 5}});
    const ClaimStream b({{5, 4}});
    EXPECT_NE(claim_stream_digest(declaration, a.rows_, a.cols_),
              claim_stream_digest(declaration, b.rows_, b.cols_));
}

TEST(ClaimStreamDigest, TheSameStreamHashesEqual) {
    const AggregateDeclaration declaration = sized(16, 2, 3);
    const ClaimStream stream({{0, 0}, {1, 0}, {2, 1}, {3, 1}});
    EXPECT_EQ(claim_stream_digest(declaration, stream.rows_, stream.cols_),
              claim_stream_digest(declaration, stream.rows_, stream.cols_));
}

TEST(ClaimStreamDigest, TheDimensionPreambleIsPartOfTheStream) {
    // The trailing-unclaimed case, through the builder: one declaration has a
    // variable no claim mentions, and the claim streams are identical. A claim
    // names rows and columns that exist, never the size of the space they live
    // in, so without the preamble these two would key identically while laying
    // different systems.
    const ClaimStream stream({{0, 0}, {1, 0}, {1, 1}, {2, 2}});
    EXPECT_NE(claim_stream_digest(sized(4, 2, 3), stream.rows_, stream.cols_),
              claim_stream_digest(sized(5, 2, 3), stream.rows_, stream.cols_));
}

TEST(ClaimStreamDigest, EachDimensionOfThePreambleIsAConjunct) {
    const ClaimStream stream({{0, 0}});
    const auto digest = [&stream](int n, int me, int mi) {
        return claim_stream_digest(sized(n, me, mi), stream.rows_, stream.cols_);
    };
    EXPECT_NE(digest(4, 2, 3), digest(5, 2, 3));
    EXPECT_NE(digest(4, 2, 3), digest(4, 1, 3));
    EXPECT_NE(digest(4, 2, 3), digest(4, 2, 4));
    EXPECT_EQ(digest(4, 2, 3), digest(4, 2, 3));
}

TEST(ClaimStreamDigest, AnEmptyStreamStillCarriesItsDimensions) {
    const Eigen::VectorXi none;
    EXPECT_NE(claim_stream_digest(sized(4, 2, 3), none, none),
              claim_stream_digest(sized(5, 2, 3), none, none));
    EXPECT_EQ(claim_stream_digest(sized(4, 2, 3), none, none),
              claim_stream_digest(sized(4, 2, 3), none, none));
}

TEST(ClaimStreamDigest, RejectsAHalfClaimStream) {
    Eigen::VectorXi rows(3);
    rows << 0, 1, 2;
    Eigen::VectorXi cols(2);
    cols << 0, 1;
    try {
        claim_stream_digest(sized(4, 2, 3), rows, cols);
        FAIL() << "a stream whose two arrays disagree in length must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("claim_stream_digest"), std::string::npos) << message;
        EXPECT_NE(message.find('3'), std::string::npos) << message;
        EXPECT_NE(message.find('2'), std::string::npos) << message;
    }
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
