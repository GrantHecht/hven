// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <optional>

#include <hven/core/types.h>
#include <hven/linear/dense_policy.h>

namespace hven::detail {

/// @brief DensePolicy::kAuto's dimension thresholds, one per engine (M8 DN-3): an
/// analyzed pattern of order n <= the engine's threshold takes the dense arm.
/// Read from DN-0's replay (docs/notes/2026-11-m8-dn3-design.md section 2.2, P-A):
/// the largest n at which every replayed matrix of that order or smaller had a dense
/// to sparse instruction ratio below 1.
inline constexpr Index kSqpDenseSelectionThreshold = 105;
inline constexpr Index kIpmDenseSelectionThreshold = 167;

/// @brief The factor option `max_dense_dimension` for an engine's policy: the engine's
/// threshold under kAuto, empty otherwise (the factor layer refuses a threshold under
/// kNever and kAlways).
inline std::optional<Index> dense_selection_threshold(hven::linear::DensePolicy policy,
                                                      Index engine_threshold) {
    if (policy == hven::linear::DensePolicy::kAuto) {
        return engine_threshold;
    }
    return std::nullopt;
}

} // namespace hven::detail
