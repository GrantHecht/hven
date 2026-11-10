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

/// @brief What the dense arm does with a factorization it finds singular.
enum class DenseSingularPolicy {
    /// An exactly singular factorization (a zero pivot, a zero 2x2 determinant,
    /// a non-finite D) is retaken by the platform session in the same call. The
    /// default.
    kSparseFallback,
    /// Rook pivoting plus a singularity detector; the factor layer reports and
    /// the caller responds (SymmetricFactor::Options::dense_singular_policy).
    kReport,
};

} // namespace hven::linear
