// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief When a KKT system is factorized with a dense factor instead of the sparse
///        backend.

namespace hven::linear {

/// @brief When the dense factorization arm is used for a KKT system.
///
/// Both engines' `validate()` accept `kNever` and `kAlways` and refuse `kAuto`
/// until its selection rule lands; `SymmetricFactor::Options` implements all three.
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
