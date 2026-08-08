#pragma once

// The library-wide structural key for a sparse matrix: a 64-bit FNV-1a hash
// over a matrix's SHAPE AND SPARSITY PATTERN ONLY (its dimensions plus its
// index arrays) -- never its stored values. The linear-algebra layer uses
// this to key a cached factorization (does the matrix's structure still
// match what it was factorized from?), and it is the same key the
// warm-start currency will use later to decide whether a hand-off's own
// structural fingerprint still matches the model being solved.
//
// For this primitive's relationship to a sibling project's own structural
// hash (same offset basis, prime, and per-matrix ingredient order; one
// deliberate generalization in how index-array entries are fed), see
// docs/pattern-hash.md.
#include <cstddef>
#include <cstdint>

#include "hven/core/types.h"

namespace hven {

// A small, composable FNV-1a accumulator. Default-constructs at the FNV-1a
// 64-bit offset basis; `feed`/`feed_index` mix data in one call at a time;
// `value()` reads the current hash without consuming it, so a caller may
// keep feeding after reading an intermediate value.
struct Fnv1a {
    static constexpr std::uint64_t kOffsetBasis = 14695981039346656037ULL;
    static constexpr std::uint64_t kPrime = 1099511628211ULL;

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

    // Feeds one Index value's bytes, least-significant byte first, via
    // explicit shift-and-mask rather than by reading `value`'s object
    // representation through `feed()`. Two consequences: this member is
    // genuinely `constexpr` (no pointer-punning cast, so it is usable in a
    // constant expression), and the byte order fed is fixed by the shift
    // direction rather than by host endianness, so the hash is
    // byte-order-independent on top of being independent of Eigen's sparse
    // storage-index width (see docs/pattern-hash.md). Every ingredient
    // pattern_hash() mixes in (dimensions, nnz, index-array entries) goes
    // through this one call.
    constexpr void feed_index(Index value) noexcept {
        const auto u = static_cast<std::uint64_t>(value);
        for (int i = 0; i < 8; ++i) {
            hash_ ^= static_cast<unsigned char>(u >> (8 * i));
            hash_ *= kPrime;
        }
    }

    constexpr std::uint64_t value() const noexcept { return hash_; }

  private:
    std::uint64_t hash_;
};

// The structural key for a sparse matrix's shape and sparsity pattern:
// FNV-1a over rows, cols, nnz, the outer index array, then the inner index
// array (docs/pattern-hash.md states the exact ingredient order and this
// primitive's relationship to the sibling project's own version). Two
// matrices with the same shape and the same sparsity pattern hash
// identically, REGARDLESS of their stored values; two matrices differing in
// shape or pattern are not expected to collide. The result is stable across
// Eigen sparse storage-index widths and across host byte order (both due to
// `Fnv1a::feed_index`'s shift-based extraction) -- it is not claimed to be
// "portable" in any broader sense.
//
// Requires `A.isCompressed()` -- throws std::invalid_argument otherwise,
// since an uncompressed matrix's index arrays do not describe its pattern
// on their own (call `A.makeCompressed()` first).
std::uint64_t pattern_hash(const SpMatRM &A);

} // namespace hven
