// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include "hven/core/pattern_hash.h"

#include <stdexcept>

#include <fmt/format.h>

namespace hven {

std::uint64_t pattern_hash(const SpMatRM &A) {
    if (!A.isCompressed()) {
        throw std::invalid_argument(fmt::format(
            "pattern_hash: matrix must be compressed (call A.makeCompressed() first) -- got a "
            "{}x{} matrix with {} stored entries in an uncompressed state",
            A.rows(), A.cols(), A.nonZeros()));
    }

    // One recipe, one implementation: this entry point's compressed-only
    // contract is the guard above, and the ingredient stream below it is the
    // same `feed_pattern` the multi-matrix key and the uncompressed-tolerant
    // callers use. The digest is unchanged by that -- it is pinned by value
    // in tests/core/test_pattern_hash.cpp, against a reference derived there
    // without calling `Fnv1a` at all.
    Fnv1a h;
    feed_pattern(h, A);
    return h.value();
}

} // namespace hven
