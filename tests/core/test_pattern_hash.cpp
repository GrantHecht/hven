#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <tuple>
#include <vector>

#include <Eigen/SparseCore>
#include <gtest/gtest.h>

#include "hven/core/pattern_hash.h"
#include "hven/core/types.h"

namespace {

using hven::Fnv1a;
using hven::Index;
using hven::pattern_hash;
using hven::SpMatRM;

// Builds a compressed row-major sparse matrix from an explicit (row, col,
// value) entry list. Every entry list used below has at most one entry per
// (row, col) pair, so setFromTriplets' default sum-duplicates behavior
// never fires.
SpMatRM make_matrix(Index rows, Index cols,
                    const std::vector<std::tuple<Index, Index, double>> &entries) {
    using Triplet = Eigen::Triplet<double>;
    std::vector<Triplet> triplets;
    triplets.reserve(entries.size());
    for (const auto &[r, c, v] : entries) {
        triplets.emplace_back(static_cast<int>(r), static_cast<int>(c), v);
    }
    SpMatRM A(rows, cols);
    A.setFromTriplets(triplets.begin(), triplets.end());
    A.makeCompressed();
    return A;
}

} // namespace

TEST(PatternHash, SameMatrixSameHash) {
    SpMatRM a = make_matrix(3, 3, {{0, 0, 1.0}, {1, 1, 2.0}, {2, 2, 3.0}});
    SpMatRM b = make_matrix(3, 3, {{0, 0, 1.0}, {1, 1, 2.0}, {2, 2, 3.0}});
    EXPECT_EQ(pattern_hash(a), pattern_hash(b));
}

TEST(PatternHash, StructurallyIdenticalDifferentValuesSameHash) {
    SpMatRM a = make_matrix(3, 3, {{0, 0, 1.0}, {1, 1, 2.0}, {2, 2, 3.0}});
    SpMatRM b = make_matrix(3, 3, {{0, 0, 42.0}, {1, 1, -7.5}, {2, 2, 1.0e6}});
    EXPECT_EQ(pattern_hash(a), pattern_hash(b));
}

TEST(PatternHash, OneMovedNonzeroDifferentHash) {
    SpMatRM a = make_matrix(3, 3, {{0, 0, 1.0}, {1, 1, 2.0}, {2, 2, 3.0}});
    // Same dims, same nnz, same values -- but the last nonzero moved from
    // (2, 2) to (2, 1).
    SpMatRM b = make_matrix(3, 3, {{0, 0, 1.0}, {1, 1, 2.0}, {2, 1, 3.0}});
    EXPECT_NE(pattern_hash(a), pattern_hash(b));
}

TEST(PatternHash, DifferentDimsSameNnzDifferentHash) {
    SpMatRM a = make_matrix(2, 4, {{0, 0, 1.0}, {1, 1, 2.0}});
    SpMatRM b = make_matrix(4, 2, {{0, 0, 1.0}, {1, 1, 2.0}});
    EXPECT_NE(pattern_hash(a), pattern_hash(b));
}

TEST(PatternHash, EmptyMatrixWellDefined) {
    SpMatRM zero_by_zero(0, 0);
    zero_by_zero.makeCompressed();
    std::uint64_t h0 = 0;
    EXPECT_NO_THROW(h0 = pattern_hash(zero_by_zero));

    SpMatRM no_nonzeros(3, 3); // positive dims, zero stored entries
    no_nonzeros.makeCompressed();
    std::uint64_t h1 = 0;
    EXPECT_NO_THROW(h1 = pattern_hash(no_nonzeros));

    // Well-defined does not mean identical: differing dims still produce
    // differing hashes even with no nonzeros in either matrix.
    EXPECT_NE(h0, h1);
}

TEST(PatternHash, UncompressedInputThrows) {
    SpMatRM a(3, 3);
    a.insert(0, 0) = 1.0;
    a.insert(1, 1) = 2.0;
    ASSERT_FALSE(a.isCompressed());
    EXPECT_THROW({ (void)pattern_hash(a); }, std::invalid_argument);
}

// Fnv1a::feed is the raw-byte composability surface the brief's public
// surface names alongside feed_index/value; it is otherwise unused by
// pattern_hash() (which goes through feed_index exclusively), so it needs
// its own direct coverage rather than relying on pattern_hash tests to
// exercise it transitively.
TEST(PatternHash, Fnv1aFeedMixesRawBytesByteAtATime) {
    constexpr std::uint64_t kOffsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t kPrime = 1099511628211ULL;

    const unsigned char bytes[] = {0x01, 0x02, 0x03, 0xFF, 0x00, 0xAB};

    std::uint64_t expected = kOffsetBasis;
    for (unsigned char b : bytes) {
        expected ^= b;
        expected *= kPrime;
    }

    Fnv1a h;
    h.feed(bytes, sizeof(bytes));
    EXPECT_EQ(h.value(), expected);

    // Composability: splitting one feed() call into two adjacent calls over
    // the same byte range accumulates to the same hash as one call over the
    // whole range -- the property `feed`'s own doc comment claims ("mix
    // data in one call at a time").
    Fnv1a split;
    split.feed(bytes, 3);
    split.feed(bytes + 3, sizeof(bytes) - 3);
    EXPECT_EQ(split.value(), expected);
}

// Literal-value hardening: pins one exact hash value for a fixed, small
// matrix. Safe to pin now that the algorithm is frozen by spec review's
// named adjudication (task-3-review.md section 3: hven's element-wise,
// shift-based feed_index is accepted as the canonical, width- and
// byte-order-stable form of the sibling project's own scheme). Uses the
// same fixture as the cross-check test below; the expected value was
// computed independently against the shipped algorithm and additionally
// confirmed unchanged across the feed_index byte-cast -> shift-based
// rewrite (both extract bytes least-significant-first on every supported
// little-endian target, so the rewrite is a value-preserving change).
TEST(PatternHash, LiteralValuePinnedForFixedFixture) {
    SpMatRM A = make_matrix(3, 4, {{0, 1, 5.0}, {1, 3, -2.0}, {2, 0, 7.0}});
    EXPECT_EQ(pattern_hash(A), 14789870936883269507ULL);
}

// Cross-check: pins the FNV-1a constants and feeding order against silent
// change. This reference is written independently here -- it does NOT call
// hven::Fnv1a -- so a change to pattern_hash()'s constants or ingredient
// order fails this test even if it also (self-consistently) changed
// Fnv1a's own definition.
TEST(PatternHash, CrossCheckAgainstIndependentFnv1aReference) {
    constexpr std::uint64_t kOffsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t kPrime = 1099511628211ULL;

    auto ref_feed_index = [](std::uint64_t &h, std::int64_t v) {
        const auto *bytes = reinterpret_cast<const unsigned char *>(&v);
        for (std::size_t i = 0; i < sizeof(v); ++i) {
            h ^= bytes[i];
            h *= kPrime;
        }
    };

    SpMatRM A = make_matrix(3, 4, {{0, 1, 5.0}, {1, 3, -2.0}, {2, 0, 7.0}});
    ASSERT_TRUE(A.isCompressed());

    std::uint64_t h = kOffsetBasis;
    ref_feed_index(h, static_cast<std::int64_t>(A.rows()));
    ref_feed_index(h, static_cast<std::int64_t>(A.cols()));
    ref_feed_index(h, static_cast<std::int64_t>(A.nonZeros()));
    for (Eigen::Index i = 0; i <= A.rows(); ++i) {
        ref_feed_index(h, static_cast<std::int64_t>(A.outerIndexPtr()[i]));
    }
    for (Eigen::Index i = 0; i < A.nonZeros(); ++i) {
        ref_feed_index(h, static_cast<std::int64_t>(A.innerIndexPtr()[i]));
    }

    EXPECT_EQ(h, pattern_hash(A));
}
