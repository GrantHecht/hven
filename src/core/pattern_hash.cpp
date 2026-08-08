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

    const Index rows = static_cast<Index>(A.rows());
    const Index cols = static_cast<Index>(A.cols());
    const Index nnz = static_cast<Index>(A.nonZeros());

    Fnv1a h;
    h.feed_index(rows);
    h.feed_index(cols);
    h.feed_index(nnz);

    const auto *outer = A.outerIndexPtr();
    for (Index i = 0; i <= rows; ++i) {
        h.feed_index(static_cast<Index>(outer[i]));
    }

    const auto *inner = A.innerIndexPtr();
    for (Index i = 0; i < nnz; ++i) {
        h.feed_index(static_cast<Index>(inner[i]));
    }

    return h.value();
}

} // namespace hven
