// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The key AnalysisTally (hven/linear/symmetric_factor.h) remembers an analysis
// by: the pattern hash, the order and stored-entry count, and every Options
// field that reaches the symbolic phase. Both platform adapters compute it the
// same way, after a completed platform analysis.

#include <cstdint>

#include <hven/core/types.h>
#include <hven/linear/symmetric_factor.h>

namespace hven::linear::detail {

inline std::uint64_t analysis_key(std::uint64_t pattern_hash, const SpMatRM &A,
                                  const SymmetricFactor::Options &o) noexcept {
    std::uint64_t h = pattern_hash;
    const auto mix = [&h](std::uint64_t v) {
        // splitmix64's finalizer over a running combine: order-sensitive.
        h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
        h ^= h >> 30;
        h *= 0xBF58476D1CE4E5B9ull;
        h ^= h >> 27;
        h *= 0x94D049BB133111EBull;
        h ^= h >> 31;
    };
    mix(static_cast<std::uint64_t>(A.rows()));
    mix(static_cast<std::uint64_t>(A.nonZeros()));
    mix(static_cast<std::uint64_t>(o.decomposition));
    mix(static_cast<std::uint64_t>(o.ordering));
    mix(o.weighted_matching ? 1u : 0u);
    mix(o.matrix_scaling ? 1u : 0u);
    mix(static_cast<std::uint64_t>(o.pivot_strategy));
    mix(static_cast<std::uint64_t>(o.factorization_algorithm));
    mix(o.pivot_perturbation_exponent
            ? 1u + static_cast<std::uint64_t>(*o.pivot_perturbation_exponent)
            : 0u);
    return h;
}

} // namespace hven::linear::detail
