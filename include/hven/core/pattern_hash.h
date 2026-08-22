#pragma once

// The library-wide structural key for a sparse matrix: a 64-bit FNV-1a hash
// over a matrix's SHAPE AND SPARSITY PATTERN ONLY (its dimensions plus its
// index arrays) -- never its stored values. The linear-algebra layer uses
// this to key a cached factorization (does the matrix's structure still
// match what it was factorized from?), and it is the same key the
// warm-start currency will use later to decide whether a hand-off's own
// structural fingerprint still matches the model being solved.
//
// The SQP engine keeps two structural keys of its own
// (`hven/detail/qp/qp_engine.h`, `hven/detail/qp/ssn_engine.h`); both are
// built ON this surface -- the QP engine's is `combined_pattern_hash` over
// its three matrices, the SSN engine's threads `feed_index` and
// `feed_pattern` through one accumulator and pairs the digest with a
// non-hashed conjunct of its own. docs/pattern-hash.md states the recipe for
// the combined key over several matrices and how those two keys sit on it.
#include <array>
#include <cstddef>
#include <cstdint>

#include <Eigen/SparseCore>

#include "hven/core/types.h"

namespace hven {

namespace detail {

// The FNV-1a 64-bit prime, and the prime raised to 0..8.
//
// The powers exist so that a RUN OF ZERO BYTES can be mixed in one multiply
// instead of one per byte: feeding a zero byte is exactly `h *= prime` (the
// XOR contributes nothing), and multiplication modulo 2^64 is associative, so
// folding k of them into one multiply by prime^k is an identity rather than an
// approximation. Every value the accumulator produces is bit-for-bit what the
// byte-at-a-time form produces.
inline constexpr std::uint64_t kFnv1aPrime = 1099511628211ULL;

constexpr std::array<std::uint64_t, 9> fnv1a_prime_powers() noexcept {
    std::array<std::uint64_t, 9> powers{};
    powers[0] = 1;
    for (std::size_t i = 1; i < powers.size(); ++i) {
        powers[i] = powers[i - 1] * kFnv1aPrime;
    }
    return powers;
}

inline constexpr std::array<std::uint64_t, 9> kFnv1aPrimePowers = fnv1a_prime_powers();

// The low `Count` bytes of `u` byte-at-a-time, then the 8 - Count high bytes --
// all of them zero at every call site that reaches this -- as one multiply.
// A free function rather than a member so it is DEFINED before the accumulator
// uses it, which is what keeps feed_index usable in a constant expression.
template <int Count>
constexpr void fnv1a_feed_low_bytes(std::uint64_t &hash, std::uint64_t u) noexcept {
    for (int i = 0; i < Count; ++i) {
        hash ^= static_cast<unsigned char>(u >> (8 * i));
        hash *= kFnv1aPrime;
    }
    hash *= kFnv1aPrimePowers[8 - Count];
}

} // namespace detail

// A small, composable FNV-1a accumulator. Default-constructs at the FNV-1a
// 64-bit offset basis; `feed`/`feed_index` mix data in one call at a time;
// `value()` reads the current hash without consuming it, so a caller may
// keep feeding after reading an intermediate value.
struct Fnv1a {
    static constexpr std::uint64_t kOffsetBasis = 14695981039346656037ULL;
    static constexpr std::uint64_t kPrime = detail::kFnv1aPrime;

    constexpr Fnv1a() noexcept : hash_(kOffsetBasis) {}

    // Feeds `len` bytes starting at `data` through the standard byte-at-a-
    // time FNV-1a mixing step (XOR the byte in, then multiply by the
    // prime). Runtime-only: reading through a `static_cast<const unsigned
    // char *>` from `const void *` is not usable in a constant expression
    // before C++26 ([expr.const]), so this member is deliberately not
    // marked `constexpr` -- doing so would be an inert, technically
    // ill-formed-no-diagnostic-required claim (see feed_index's own note
    // for the member that actually is constexpr).
    void feed(const void *data, std::size_t len) noexcept {
        const auto *bytes = static_cast<const unsigned char *>(data);
        for (std::size_t i = 0; i < len; ++i) {
            hash_ ^= bytes[i];
            hash_ *= kPrime;
        }
    }

    // Feeds one index value's eight bytes, least-significant byte first, via
    // explicit shift-and-mask rather than by reading `value`'s object
    // representation through `feed()`. Two consequences: this member is
    // genuinely `constexpr` (no pointer-punning cast, so it is usable in a
    // constant expression), and the byte order fed is fixed by the shift
    // direction rather than by host endianness, so the hash is
    // byte-order-independent on top of being independent of Eigen's sparse
    // storage-index width (see docs/pattern-hash.md). Every ingredient
    // pattern_hash() mixes in (dimensions, nnz, index-array entries) goes
    // through this one call.
    //
    // THE PARAMETER IS LITERALLY `std::int64_t`, NOT `hven::Index`, and that
    // is the width contract rather than a spelling preference: the hash's
    // stability claim is "evaluated at a fixed 64-bit width", so it must not
    // ride an alias that a future retypedef could move. `Index` converts to
    // it exactly on every supported target (both are signed 64-bit), so no
    // hash value depends on which of the two a caller passes.
    constexpr void feed_index(std::int64_t value) noexcept {
        const auto u = static_cast<std::uint64_t>(value);
        // ALL EIGHT BYTES ARE STILL FED, in the same order, to the same
        // mixing step: the three arms differ only in how many of the HIGH
        // bytes are known to be zero, and a run of zero bytes is one multiply
        // by a power of the prime rather than one multiply each (see
        // detail::kFnv1aPrimePowers). Index streams are dominated by small
        // non-negative values, so the two-byte arm is the one taken, and the
        // branch predicts on a stream whose magnitudes barely vary.
        if ((u >> 16) == 0) {
            detail::fnv1a_feed_low_bytes<2>(hash_, u);
        } else if ((u >> 32) == 0) {
            detail::fnv1a_feed_low_bytes<4>(hash_, u);
        } else {
            detail::fnv1a_feed_low_bytes<8>(hash_, u);
        }
    }

    // Feeds a stream of index PAIRS -- first[i] then second[i], for each i in
    // order -- from two CONTIGUOUS arrays in one pass.
    //
    // Exactly the stream `feed_index(first[i]); feed_index(second[i]);` would
    // produce, and the same value; what it removes is the per-element call
    // through whatever accessor the caller holds the two halves in. A digest
    // over an interleaved pair stream is otherwise the one shape a caller
    // cannot express as two bulk feeds -- feeding one whole array and then the
    // other is a DIFFERENT stream -- so it gets an entry of its own rather than
    // an assemble-it-yourself loop at each call site.
    void feed_index_pairs(const int *first, const int *second, std::size_t count) noexcept {
        for (std::size_t i = 0; i < count; ++i) {
            feed_index(first[i]);
            feed_index(second[i]);
        }
    }

    constexpr std::uint64_t value() const noexcept { return hash_; }

  private:
    std::uint64_t hash_;
};

// Feeds ONE matrix's shape and sparsity pattern into an accumulator that is
// already running: rows, cols, nnz, the outer offset array, then the inner
// index array -- in that order, every ingredient through `feed_index`, so
// every ingredient is 64-bit widened and byte-order-fixed. This is the whole
// recipe: `pattern_hash(A)` below is exactly `Fnv1a h; feed_pattern(h, A);
// h.value()`, and the multi-matrix key is this call repeated on one
// accumulator. It is the surface a caller reaches for to interleave a
// matrix's pattern with ingredients of its own (sizes, flags, a bound-row
// list) in one digest.
//
// UNCOMPRESSED-TOLERANT, AND THAT IS A CONTRACT RATHER THAN A CONVENIENCE:
// the digest of a matrix in Eigen's uncompressed state is bit-identical to
// the digest of that same matrix after `makeCompressed()`. Equal structures
// hash equal whatever storage state they are in, so a hash-keyed reuse
// decision cannot be flipped by a caller's compression bookkeeping.
//
// The equality is not maintained by comparing the two states; there is one
// stream and both states produce it. The outer offsets fed are DERIVED, not
// read: a running total of stored entries per outer vector, starting at
// zero. Compressed, that derivation reproduces `outerIndexPtr()[0..rows]`
// element for element -- reproducing it is what "compressed" means.
// Uncompressed, it reproduces what `makeCompressed()` would write there,
// because Eigen builds that array by the same prefix sum
// (`SparseMatrix::makeCompressed`). The inner indices then follow in stored
// order, walked with `InnerIterator`, which is exact in both states.
//
// The one thing the two states genuinely spell differently is where a vector
// keeps its stored count -- `innerNonZeroPtr()` when the matrix carries
// per-vector free space, the difference of two outer offsets when it does
// not. That is the same pair of accessors Eigen's own `InnerIterator` uses
// to bound a vector, and asking it is a storage question, not a digest
// adjustment: no ingredient's VALUE depends on which arm answers.
//
// Row-major only, and enforced: the stream's "outer" is rows. Hashing a
// column-major matrix through it would silently key a transpose.
template <class Scalar, int Options, class StorageIndex>
void feed_pattern(Fnv1a &h, const Eigen::SparseMatrix<Scalar, Options, StorageIndex> &A) {
    static_assert((Options & Eigen::RowMajor) != 0,
                  "hven::feed_pattern: the pattern stream is defined with rows as the outer "
                  "dimension, so it takes a row-major sparse matrix (hven::SpMatRM); feeding a "
                  "column-major one would key its transpose without saying so.");

    // Row-major, so outerSize() == rows(); named `rows` because that is what
    // the stream's outer array indexes.
    const std::int64_t rows = static_cast<std::int64_t>(A.rows());
    h.feed_index(rows);
    h.feed_index(static_cast<std::int64_t>(A.cols()));
    h.feed_index(static_cast<std::int64_t>(A.nonZeros()));

    // Null exactly when the matrix is compressed -- Eigen's documented
    // signal that every outer vector's stored count is the difference of two
    // outer offsets rather than an entry of its own array.
    const StorageIndex *const stored_counts = A.innerNonZeroPtr();
    const StorageIndex *const outer = A.outerIndexPtr();

    std::int64_t offset = 0;
    h.feed_index(offset);
    for (std::int64_t r = 0; r < rows; ++r) {
        offset += stored_counts != nullptr ? static_cast<std::int64_t>(stored_counts[r])
                                           : static_cast<std::int64_t>(outer[r + 1]) -
                                                 static_cast<std::int64_t>(outer[r]);
        h.feed_index(offset);
    }

    using Matrix = Eigen::SparseMatrix<Scalar, Options, StorageIndex>;
    for (std::int64_t r = 0; r < rows; ++r) {
        for (typename Matrix::InnerIterator it(A, r); it; ++it) {
            h.feed_index(static_cast<std::int64_t>(it.index()));
        }
    }
}

// The structural key for one sparse matrix's shape and sparsity pattern:
// FNV-1a over rows, cols, nnz, the outer index array, then the inner index
// array, per `feed_pattern` above (docs/pattern-hash.md states the exact
// ingredient order and how the SQP engine's own fingerprints relate to it).
// Two matrices with the same shape and the same sparsity pattern hash
// identically, REGARDLESS of their stored values; two matrices differing in
// shape or pattern are not expected to collide. The result is stable across
// Eigen sparse storage-index widths and across host byte order (both due to
// `Fnv1a::feed_index`'s shift-based extraction) -- it is not claimed to be
// "portable" in any broader sense.
//
// Requires `A.isCompressed()` -- throws std::invalid_argument otherwise.
// The requirement is this ENTRY POINT's, not the recipe's: `feed_pattern`
// hashes either storage state to the same value, and a caller who wants that
// tolerance calls it (or `combined_pattern_hash`) directly. What this
// signature keeps is a boundary check for the tier it serves -- the KKT and
// linear layers, whose backends read a compressed CSR and reject anything
// else (`SymmetricFactor::analyze`/`factorize`) -- so a caller who forgot
// `makeCompressed()` fails here, at the first call that touches the matrix,
// instead of one call later.
std::uint64_t pattern_hash(const SpMatRM &A);

// The combined structural key over SEVERAL matrices, in argument order: ONE
// `Fnv1a` threaded through them by `feed_pattern`, each matrix appending its
// own ingredients to the accumulation the previous ones left running. This
// is append-style continued accumulation and NOT a fold over per-matrix
// digests -- the two are different functions of the same inputs (a fold
// mixes eight bytes per matrix; this mixes every index-array entry), and the
// tests pin them apart. docs/pattern-hash.md records why the continuation is
// the recipe hven adopted.
//
// No separator is inserted between matrices: each one's leading rows/cols/nnz
// triple is the separator, which is what makes the serialization
// self-delimiting -- a re-partitioned sequence feeds a DIFFERENT stream than
// the original, so the two can collide only at the ordinary 64-bit-digest
// level, never structurally. Order is significant.
//
// The one-matrix case is the tolerant single-matrix digest:
// `combined_pattern_hash(A) == pattern_hash(A)` for every compressed `A`,
// and for an uncompressed `A` it is the digest `A` would have compressed.
template <class... Matrices> std::uint64_t combined_pattern_hash(const Matrices &...matrices) {
    static_assert(sizeof...(Matrices) >= 1,
                  "hven::combined_pattern_hash: a key over no matrices is the bare FNV-1a offset "
                  "basis, which is a constant rather than a fingerprint -- pass at least one.");
    Fnv1a h;
    (feed_pattern(h, matrices), ...);
    return h.value();
}

} // namespace hven
