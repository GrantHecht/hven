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

// Fnv1a::feed is the raw-byte composability surface offered alongside
// feed_index/value; it is otherwise unused by pattern_hash() (which goes
// through feed_index exclusively), so it needs its own direct coverage
// rather than relying on pattern_hash tests to exercise it transitively.
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
// matrix. The algorithm is frozen (element-wise, shift-based feed_index
// as the canonical width- and byte-order-stable form), so an exact pin
// is safe. Uses the same fixture as the cross-check test below, which
// provides an independent in-repo derivation of this same value; the pin
// was additionally confirmed unchanged across the feed_index
// byte-cast -> shift-based rewrite (both extract bytes least-significant-
// first on little-endian targets, so the rewrite preserved every value).
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
        // LSB-first shift extraction, matching the advertised byte order
        // independently of host endianness (the implementation's spec is
        // "least-significant byte first", not "object representation").
        const auto u = static_cast<std::uint64_t>(v);
        for (std::size_t i = 0; i < sizeof(u); ++i) {
            h ^= (u >> (8 * i)) & 0xFFu;
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

////////////////////////////////////////////////////////////////////////////////
// The multi-matrix continuation surface, the uncompressed-tolerant path, and
// the cross-width claim.
//
// Everything above this line is untouched by the change that added these
// tests -- deliberately, since the literal pin at LiteralValuePinnedForFixed-
// Fixture is the anchor the whole surface below is defined to agree with.
////////////////////////////////////////////////////////////////////////////////

namespace {

using hven::combined_pattern_hash;
using hven::feed_pattern;

using Entries = std::vector<std::tuple<Index, Index, double>>;

template <class StorageIndex>
using SpRM = Eigen::SparseMatrix<double, Eigen::RowMajor, StorageIndex>;

// make_matrix's storage-index-parameterized twin, so the same structure can
// be built at 32 and at 64 bits and the two digests compared.
template <class StorageIndex>
SpRM<StorageIndex> make_compressed_at(Index rows, Index cols, const Entries &entries) {
    using Triplet = Eigen::Triplet<double, StorageIndex>;
    std::vector<Triplet> triplets;
    triplets.reserve(entries.size());
    for (const auto &[r, c, v] : entries) {
        triplets.emplace_back(static_cast<StorageIndex>(r), static_cast<StorageIndex>(c), v);
    }
    SpRM<StorageIndex> A(rows, cols);
    A.setFromTriplets(triplets.begin(), triplets.end());
    A.makeCompressed();
    return A;
}

// The same structure, left in Eigen's UNCOMPRESSED state with per-row free
// space still reserved. `slack` extra slots per row is what makes this a
// genuinely uncompressed fixture rather than a nominally-uncompressed one:
// the stored entries sit at non-contiguous positions, so reading
// innerIndexPtr()[0..nnz) -- the thing the compressed path is allowed to do
// and the iteration path must not -- lands in reserved slots that hold no
// pattern at all.
template <class StorageIndex>
SpRM<StorageIndex> make_uncompressed_at(Index rows, Index cols, const Entries &entries,
                                        Index slack = 3) {
    SpRM<StorageIndex> A(rows, cols);
    const std::vector<StorageIndex> room(static_cast<std::size_t>(rows),
                                         static_cast<StorageIndex>(slack));
    A.reserve(room);
    for (const auto &[r, c, v] : entries) {
        A.insert(static_cast<StorageIndex>(r), static_cast<StorageIndex>(c)) = v;
    }
    return A;
}

SpMatRM make_uncompressed(Index rows, Index cols, const Entries &entries, Index slack = 3) {
    return make_uncompressed_at<SpMatRM::StorageIndex>(rows, cols, entries, slack);
}

// The fixture the literal pin is taken on (test above), reused here so the
// uncompressed arm is asserted against a KNOWN VALUE and not merely against
// another computation of it.
const Entries kPinnedFixtureEntries = {{0, 1, 5.0}, {1, 3, -2.0}, {2, 0, 7.0}};
constexpr std::uint64_t kPinnedFixtureHash = 14789870936883269507ULL;

// An independent FNV-1a reference over an EXPLICIT stream. Dimensions, outer
// offsets and inner indices are passed in -- written out by hand from the
// structure under test -- so this function never reads a matrix, and in
// particular never reads the outer array whose derivation is the thing under
// test. It does not call `hven::Fnv1a` either: the constants and the mixing
// step are re-stated here deliberately, exactly as
// CrossCheckAgainstIndependentFnv1aReference re-states them, so a
// self-consistent change to both the implementation and its accumulator
// still fails.
std::uint64_t reference_digest(std::int64_t rows, std::int64_t cols,
                               const std::vector<std::int64_t> &outer,
                               const std::vector<std::int64_t> &inner) {
    constexpr std::uint64_t kOffsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t kPrime = 1099511628211ULL;

    std::uint64_t h = kOffsetBasis;
    auto feed = [&h](std::int64_t v) {
        const auto u = static_cast<std::uint64_t>(v);
        for (std::size_t i = 0; i < sizeof(u); ++i) {
            h ^= (u >> (8 * i)) & 0xFFu;
            h *= kPrime;
        }
    };

    feed(rows);
    feed(cols);
    feed(static_cast<std::int64_t>(inner.size())); // nnz
    for (std::int64_t o : outer) {
        feed(o);
    }
    for (std::int64_t i : inner) {
        feed(i);
    }
    return h;
}

} // namespace

// The oracle arm: the tolerant path, on the pinned fixture, in BOTH storage
// states, against the pinned VALUE. An equality-between-two-computations
// assertion cannot catch a self-consistent double error -- one where the
// compressed and the iteration path were changed together -- and this can.
TEST(PatternHash, TolerantPathReproducesThePinnedLiteralInBothStorageStates) {
    SpMatRM compressed = make_matrix(3, 4, kPinnedFixtureEntries);
    ASSERT_TRUE(compressed.isCompressed());
    EXPECT_EQ(combined_pattern_hash(compressed), kPinnedFixtureHash);

    SpMatRM uncompressed = make_uncompressed(3, 4, kPinnedFixtureEntries);
    ASSERT_FALSE(uncompressed.isCompressed());
    ASSERT_EQ(uncompressed.nonZeros(), compressed.nonZeros());
    EXPECT_EQ(combined_pattern_hash(uncompressed), kPinnedFixtureHash);

    // And compressing that very matrix in place moves nothing.
    uncompressed.makeCompressed();
    EXPECT_EQ(pattern_hash(uncompressed), kPinnedFixtureHash);
}

// The gapped case, which is the one the design can actually get wrong: a
// matrix whose outer vectors are not all populated. Two empty rows in the
// middle, and a reserved-but-unused slot in every row of the uncompressed
// arm.
TEST(PatternHash, TolerantPathAgreesWithCompressedOnEmptyOuterRows) {
    const Entries entries = {{0, 2, 1.0}, {0, 4, 2.0}, {3, 0, 3.0}, {4, 4, 4.0}};

    SpMatRM compressed = make_matrix(5, 5, entries);
    SpMatRM uncompressed = make_uncompressed(5, 5, entries);
    ASSERT_FALSE(uncompressed.isCompressed());

    // Rows 1 and 2 are empty in both arms -- the fixture would be pointless
    // otherwise.
    ASSERT_EQ(compressed.outerIndexPtr()[1], compressed.outerIndexPtr()[2]);
    ASSERT_EQ(compressed.outerIndexPtr()[2], compressed.outerIndexPtr()[3]);
    ASSERT_EQ(uncompressed.innerNonZeroPtr()[1], 0);
    ASSERT_EQ(uncompressed.innerNonZeroPtr()[2], 0);

    EXPECT_EQ(combined_pattern_hash(uncompressed), pattern_hash(compressed));

    // The failure mode this fixture guards is not "the two disagree", it is
    // "the two agree on a digest that has forgotten where the empty rows
    // were". Moving the same four entries up into the empty rows -- same
    // dims, same nnz, same column indices -- must change the digest, in both
    // storage states.
    const Entries compacted = {{0, 2, 1.0}, {0, 4, 2.0}, {1, 0, 3.0}, {2, 4, 4.0}};
    EXPECT_NE(pattern_hash(compressed), pattern_hash(make_matrix(5, 5, compacted)));
    EXPECT_NE(combined_pattern_hash(uncompressed),
              combined_pattern_hash(make_uncompressed(5, 5, compacted)));
}

// A trailing empty row and a leading one are the two off-by-one shapes a
// derived-offset loop gets wrong first, so they get their own fixture rather
// than riding on the interior-gap one above.
TEST(PatternHash, TolerantPathAgreesWithCompressedOnLeadingAndTrailingEmptyRows) {
    const Entries entries = {{2, 1, 1.0}};
    SpMatRM compressed = make_matrix(4, 3, entries);
    SpMatRM uncompressed = make_uncompressed(4, 3, entries);
    ASSERT_FALSE(uncompressed.isCompressed());
    EXPECT_EQ(combined_pattern_hash(uncompressed), pattern_hash(compressed));

    // Degenerate shapes. The no-entries pair is a real equality (an
    // uncompressed matrix with room reserved in every row and nothing in
    // any of them); the 0x0 line is a smoke arm -- pattern_hash and
    // combined_pattern_hash share one implementation, so it asserts only
    // that the derived-offset loop is well-defined when there are no outer
    // vectors to walk.
    SpMatRM no_entries_compressed = make_matrix(3, 3, {});
    SpMatRM no_entries_uncompressed = make_uncompressed(3, 3, {});
    ASSERT_FALSE(no_entries_uncompressed.isCompressed());
    EXPECT_EQ(combined_pattern_hash(no_entries_uncompressed), pattern_hash(no_entries_compressed));

    SpMatRM zero_by_zero(0, 0);
    zero_by_zero.makeCompressed();
    EXPECT_EQ(combined_pattern_hash(zero_by_zero), pattern_hash(zero_by_zero));
}

// The ABSOLUTE pin for the gapped shapes. Every other gapped assertion in
// this file compares the new code against another run of the new code, so
// the derived-offset loop -- the one piece of this design that could be
// wrong, on exactly the shape the structure-hash survey called decisive --
// would otherwise be pinned only relative to itself. Here the expected
// digest comes from `reference_digest`, which is handed the outer offsets
// `makeCompressed()` produces for these structures WRITTEN OUT BY HAND, and
// never reads a matrix at all. Both storage states must reproduce it.
TEST(PatternHash, GappedStructureDigestsMatchTheIndependentReference) {
    // Interior gap: 5x5, rows 1 and 2 empty. Row 0 holds columns 2 and 4,
    // row 3 holds column 0, row 4 holds column 4 -- so the compressed outer
    // array is 0,2,2,2,3,4 and the inner array is 2,4,0,4.
    {
        const Entries entries = {{0, 2, 1.0}, {0, 4, 2.0}, {3, 0, 3.0}, {4, 4, 4.0}};
        const std::uint64_t expected = reference_digest(5, 5, {0, 2, 2, 2, 3, 4}, {2, 4, 0, 4});

        SpMatRM compressed = make_matrix(5, 5, entries);
        SpMatRM uncompressed = make_uncompressed(5, 5, entries);
        ASSERT_TRUE(compressed.isCompressed());
        ASSERT_FALSE(uncompressed.isCompressed());

        EXPECT_EQ(pattern_hash(compressed), expected);
        EXPECT_EQ(combined_pattern_hash(uncompressed), expected);
    }

    // Leading and trailing gaps: 4x3, only row 2 populated (column 1) -- so
    // the compressed outer array is 0,0,0,1,1 and the inner array is 1. The
    // repeated leading zeros and the repeated trailing one are what an
    // off-by-one in the running total gets wrong.
    {
        const Entries entries = {{2, 1, 1.0}};
        const std::uint64_t expected = reference_digest(4, 3, {0, 0, 0, 1, 1}, {1});

        SpMatRM compressed = make_matrix(4, 3, entries);
        SpMatRM uncompressed = make_uncompressed(4, 3, entries);
        ASSERT_FALSE(uncompressed.isCompressed());

        EXPECT_EQ(pattern_hash(compressed), expected);
        EXPECT_EQ(combined_pattern_hash(uncompressed), expected);
    }

    // No stored entries at all: every offset is zero, the inner array is
    // empty, and the digest is still a function of the dimensions.
    {
        const std::uint64_t expected = reference_digest(3, 3, {0, 0, 0, 0}, {});

        SpMatRM compressed = make_matrix(3, 3, {});
        SpMatRM uncompressed = make_uncompressed(3, 3, {});
        ASSERT_FALSE(uncompressed.isCompressed());

        EXPECT_EQ(pattern_hash(compressed), expected);
        EXPECT_EQ(combined_pattern_hash(uncompressed), expected);
    }
}

// Mixed storage states across one combined key: the tolerance is per matrix,
// so a caller never has to normalize a sequence before hashing it.
TEST(PatternHash, CombinedKeyToleratesMixedStorageStates) {
    const Entries ea = {{0, 1, 5.0}, {2, 0, 7.0}};
    const Entries eb = {{0, 0, 1.0}, {1, 1, 1.0}};

    const std::uint64_t all_compressed =
        combined_pattern_hash(make_matrix(3, 4, ea), make_matrix(2, 2, eb));

    EXPECT_EQ(combined_pattern_hash(make_uncompressed(3, 4, ea), make_matrix(2, 2, eb)),
              all_compressed);
    EXPECT_EQ(combined_pattern_hash(make_matrix(3, 4, ea), make_uncompressed(2, 2, eb)),
              all_compressed);
    EXPECT_EQ(combined_pattern_hash(make_uncompressed(3, 4, ea), make_uncompressed(2, 2, eb)),
              all_compressed);
}

// The width claim, asserted rather than documented: the same structure built
// on a 32-bit and on a 64-bit storage index hashes identically, because every
// ingredient is widened to 64 bits before it is fed.
TEST(PatternHash, CrossWidthStabilityAtThirtyTwoAndSixtyFourBitStorageIndices) {
    static_assert(sizeof(SpMatRM::StorageIndex) == 4,
                  "this test's premise is that SpMatRM stores 32-bit indices");
    static_assert(sizeof(SpRM<std::int64_t>::StorageIndex) == 8,
                  "this test's premise is that the wide fixture stores 64-bit indices");

    const Entries entries = {{0, 2, 1.0}, {0, 4, 2.0}, {3, 0, 3.0}, {4, 4, 4.0}};

    SpMatRM narrow = make_compressed_at<SpMatRM::StorageIndex>(5, 5, entries);
    SpRM<std::int64_t> wide = make_compressed_at<std::int64_t>(5, 5, entries);
    EXPECT_EQ(combined_pattern_hash(wide), pattern_hash(narrow));

    // Width-stable in the uncompressed state too -- the derived offsets are
    // 64-bit sums whichever width they are read from.
    SpMatRM narrow_u = make_uncompressed_at<SpMatRM::StorageIndex>(5, 5, entries);
    SpRM<std::int64_t> wide_u = make_uncompressed_at<std::int64_t>(5, 5, entries);
    ASSERT_FALSE(narrow_u.isCompressed());
    ASSERT_FALSE(wide_u.isCompressed());
    EXPECT_EQ(combined_pattern_hash(wide_u), combined_pattern_hash(narrow_u));
    EXPECT_EQ(combined_pattern_hash(wide_u), pattern_hash(narrow));

    // Against the pinned VALUE, at the other width: the digest is the same
    // number, not merely the same on both sides of one build.
    SpRM<std::int64_t> wide_pinned = make_compressed_at<std::int64_t>(3, 4, kPinnedFixtureEntries);
    EXPECT_EQ(combined_pattern_hash(wide_pinned), kPinnedFixtureHash);
    EXPECT_EQ(
        combined_pattern_hash(make_uncompressed_at<std::int64_t>(3, 4, kPinnedFixtureEntries)),
        kPinnedFixtureHash);
}

// The design point of the multi-matrix surface, pinned so it cannot be
// quietly re-specified: the combined key CONTINUES one accumulation across
// the matrices; it is not a fold over their finished digests. Both are
// legitimate combined keys and they are different functions -- a fold mixes
// eight bytes per matrix, the continuation mixes every ingredient of every
// matrix.
TEST(PatternHash, CombinedKeyIsContinuedAccumulationNotAFoldOverDigests) {
    SpMatRM a = make_matrix(3, 4, kPinnedFixtureEntries);
    SpMatRM b = make_matrix(2, 2, {{0, 0, 1.0}, {1, 1, 1.0}});

    const std::uint64_t combined = combined_pattern_hash(a, b);

    // Fold 1: the obvious one -- the two digests fed to a fresh accumulator.
    Fnv1a folded;
    folded.feed_index(static_cast<std::int64_t>(pattern_hash(a)));
    folded.feed_index(static_cast<std::int64_t>(pattern_hash(b)));
    EXPECT_NE(combined, folded.value());

    // Fold 2: the other obvious one.
    EXPECT_NE(combined, pattern_hash(a) ^ pattern_hash(b));

    // What it IS: the first matrix's own accumulation, read at the point the
    // single-matrix digest would have stopped, and then carried on.
    Fnv1a running;
    feed_pattern(running, a);
    EXPECT_EQ(running.value(), pattern_hash(a));
    feed_pattern(running, b);
    EXPECT_EQ(running.value(), combined);

    // Order is part of the key, and one matrix is the single-matrix digest.
    EXPECT_NE(combined, combined_pattern_hash(b, a));
    EXPECT_EQ(combined_pattern_hash(a), pattern_hash(a));

    // The per-matrix leading rows/cols/nnz triple is the separator: the same
    // stored entries re-partitioned across a differently-shaped pair of
    // matrices is a different key.
    SpMatRM a_wide = make_matrix(3, 6, kPinnedFixtureEntries);
    EXPECT_NE(combined, combined_pattern_hash(a_wide, b));
}
