// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <limits>
#include <stdexcept>

#include <fmt/format.h>

#include <hven/model/nlp.h>

namespace hven {

namespace {

// The defaults serve an empty block. A non-empty destination means the problem
// declared entries this callback was not overridden to produce.
[[noreturn]] void throw_not_overridden(const Nlp &nlp, const char *callback, Index entries) {
    throw std::logic_error(fmt::format("Nlp '{}': {}() is not overridden, but the problem "
                                       "declares {} entries for it",
                                       nlp.name(), callback, entries));
}

[[noreturn]] void throw_bad_dense_size(const char *what, Index have, Index want) {
    throw std::invalid_argument(
        fmt::format("{}: output has {} entries, the dense pattern has {}", what, have, want));
}

} // namespace

Index dense_jacobian_nonzeros(Index m, Index n) {
    if (m < 0 || n < 1) {
        throw std::invalid_argument(fmt::format(
            "dense_jacobian_nonzeros: need m >= 0 and n >= 1, got m = {}, n = {}", m, n));
    }
    if (m > std::numeric_limits<Index>::max() / n) {
        throw std::invalid_argument(
            fmt::format("dense_jacobian_nonzeros: m * n overflows Index (m = {}, n = {})", m, n));
    }
    return m * n;
}

Index dense_lower_hessian_nonzeros(Index n) {
    if (n < 1) {
        throw std::invalid_argument(
            fmt::format("dense_lower_hessian_nonzeros: need n >= 1, got n = {}", n));
    }
    // n * (n + 1) / 2 with n <= 2^31 stays below 2^62; beyond that it cannot be an Eigen size.
    if (n > (Index{1} << 31)) {
        throw std::invalid_argument(
            fmt::format("dense_lower_hessian_nonzeros: n = {} is too large", n));
    }
    return n * (n + 1) / 2;
}

void dense_jacobian_structure(Index m, Index n, IndexVecRef rows, IndexVecRef cols) {
    const Index nnz = dense_jacobian_nonzeros(m, n);
    if (rows.size() != nnz) {
        throw_bad_dense_size("dense_jacobian_structure rows", rows.size(), nnz);
    }
    if (cols.size() != nnz) {
        throw_bad_dense_size("dense_jacobian_structure cols", cols.size(), nnz);
    }
    Index k = 0;
    for (Index i = 0; i < m; ++i) {
        for (Index j = 0; j < n; ++j, ++k) {
            rows[k] = i;
            cols[k] = j;
        }
    }
}

void dense_lower_hessian_structure(Index n, IndexVecRef rows, IndexVecRef cols) {
    const Index nnz = dense_lower_hessian_nonzeros(n);
    if (rows.size() != nnz) {
        throw_bad_dense_size("dense_lower_hessian_structure rows", rows.size(), nnz);
    }
    if (cols.size() != nnz) {
        throw_bad_dense_size("dense_lower_hessian_structure cols", cols.size(), nnz);
    }
    Index k = 0;
    for (Index i = 0; i < n; ++i) {
        for (Index j = 0; j <= i; ++j, ++k) {
            rows[k] = i;
            cols[k] = j;
        }
    }
}

void Nlp::eval_g(ConstVecRef /*x*/, VecRef g) const {
    if (g.size() > 0) {
        throw_not_overridden(*this, "eval_g", g.size());
    }
}

void Nlp::jacobian_structure(IndexVecRef rows, IndexVecRef /*cols*/) const {
    if (rows.size() > 0) {
        throw_not_overridden(*this, "jacobian_structure", rows.size());
    }
}

void Nlp::eval_jacobian(ConstVecRef /*x*/, VecRef values) const {
    if (values.size() > 0) {
        throw_not_overridden(*this, "eval_jacobian", values.size());
    }
}

void Nlp::hessian_structure(IndexVecRef rows, IndexVecRef /*cols*/) const {
    if (rows.size() > 0) {
        throw_not_overridden(*this, "hessian_structure", rows.size());
    }
}

void Nlp::eval_hessian(ConstVecRef /*x*/, double /*obj_factor*/, ConstVecRef /*lambda*/,
                       VecRef values) const {
    if (values.size() > 0) {
        throw_not_overridden(*this, "eval_hessian", values.size());
    }
}

} // namespace hven
