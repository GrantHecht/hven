#pragma once

// The library-wide structural key for a sparse matrix: a 64-bit FNV-1a hash
// over a matrix's SHAPE AND SPARSITY PATTERN ONLY (its dimensions plus its
// index arrays) -- never its stored values. The linear-algebra layer uses
// this to key a cached factorization (does the matrix's structure still
// match what it was factorized from?), and it is the same key the
// warm-start currency will use later to decide whether a hand-off's own
// structural fingerprint still matches the model being solved.
//
// RELATIONSHIP TO THE SQP DRIVER'S OWN STRUCTURE HASH. A sibling project (an
// SQP driver whose engine originated outside this repository) already has a
// hash exactly like this one -- an FNV-1a fingerprint over a sparse
// matrix's dimensions and index arrays, computed with the same offset basis
// and prime, in the same ingredient order (rows, then cols, then nnz, then
// the outer index array, then the inner index array). That code is expected
// to migrate into this repository eventually, and its own warm-start object
// already carries a hash produced this way, so this primitive is written to
// stay comparable with it rather than reinvent the scheme:
//   - same offset basis (14695981039346656037) and prime (1099511628211);
//   - same per-matrix ingredient order: rows, cols, nnz, outer index array,
//     inner index array;
//   - the sibling project actually hashes THREE matrices (an objective
//     Hessian and two constraint Jacobians) into one combined key, one
//     after another, each with its own leading rows/cols/nnz triple and no
//     separator beyond that. This header provides the SINGLE-matrix
//     primitive with the same per-array feeding discipline; a caller that
//     needs a combined key over several matrices gets it by calling this
//     matrix by matrix and folding the results together, or equivalently by
//     extending one running Fnv1a accumulator across matrices -- nothing
//     here special-cases "three".
//
// ONE DELIBERATE GENERALIZATION, stated so it is not mistaken for a
// transcription error: the sibling project's version feeds each index array
// as one contiguous block of raw bytes, at whatever machine width Eigen's
// sparse storage index happens to be on that build (typically a 32-bit
// int). This header instead feeds each outer/inner index array ELEMENT BY
// ELEMENT, each entry widened to the portable 64-bit `Index` alias
// (types.h), through `Fnv1a::feed_index`. The two are not guaranteed
// byte-identical to each other on a platform where the sparse storage index
// is narrower than 64 bits, but they share every other ingredient
// (constants, order, which fields are hashed). What "comparable" means for
// this key in practice is a caller comparing hashes produced by the SAME
// implementation across solves within one process, and that property holds
// for both versions independently of this generalization.
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
    // prime).
    constexpr void feed(const void *data, std::size_t len) noexcept {
        const auto *bytes = static_cast<const unsigned char *>(data);
        for (std::size_t i = 0; i < len; ++i) {
            hash_ ^= bytes[i];
            hash_ *= kPrime;
        }
    }

    // Feeds one Index value's bytes. Every ingredient pattern_hash() mixes
    // in below (dimensions, nnz, index-array entries) goes through this one
    // call -- see this header's own note above for why that is element-wise
    // rather than a bulk byte-block feed.
    constexpr void feed_index(Index value) noexcept { feed(&value, sizeof(value)); }

    constexpr std::uint64_t value() const noexcept { return hash_; }

  private:
    std::uint64_t hash_;
};

// The structural key for a sparse matrix's shape and sparsity pattern:
// FNV-1a over rows, cols, nnz, the outer index array, then the inner index
// array (this header's own note above states the exact ingredient order and
// its relationship to the sibling project's own version). Two matrices with
// the same shape and the same sparsity pattern hash identically, REGARDLESS
// of their stored values; two matrices differing in shape or pattern are
// not expected to collide.
//
// Requires `A.isCompressed()` -- throws std::invalid_argument otherwise,
// since an uncompressed matrix's index arrays do not describe its pattern
// on their own (call `A.makeCompressed()` first).
std::uint64_t pattern_hash(const SpMatRM &A);

} // namespace hven
