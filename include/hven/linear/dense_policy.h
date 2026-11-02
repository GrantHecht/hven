// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief When a KKT system is factorized with a dense factor instead of the sparse
///        backend.

namespace hven::linear {

/// @brief When the dense factorization arm is used for a KKT system.
///
/// The name is fixed now; the arm itself is not wired yet. Until it is, both
/// engines' `validate()` accept `kNever` only and refuse the other two by name.
enum class DensePolicy {
    /// Always the sparse backend. The default.
    kNever,
    /// Always the dense factor.
    kAlways,
    /// The dense factor at or below a dimension threshold, the sparse backend
    /// above it.
    kAuto,
};

} // namespace hven::linear
