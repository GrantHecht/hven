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
