// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// Shared Eigen-based type aliases used across hven: only the aliases the
// linear-algebra layer and its test rig need right now — nothing speculative.

#include <type_traits>

#include <Eigen/Core>
#include <Eigen/SparseCore>

namespace hven {

/// hven's index type for sizes/indices at its own API boundaries (row/column
/// counts, nnz, ...): Eigen's index type, i.e. `std::ptrdiff_t` --
/// pointer-width, signed, and 64-bit on every supported platform (x86-64
/// Linux, x86-64 Windows, Apple arm64). Both properties are pinned by the
/// static_asserts below.
///
/// IT IS EIGEN'S TYPE ON PURPOSE: hven's boundary index type and Eigen's are
/// ONE type on every target, so a value never changes identity crossing
/// between hven's own signatures and the Eigen expressions behind them.
///
/// WHERE THAT STOPS, because "64-bit indices" otherwise reads as a promise
/// the library does not keep: the width is 64-bit at the SIGNATURES, not in
/// the sparse storage they carry. SpMatRM below uses Eigen's default `int`
/// storage index, and both sparse backends require exactly that width (the
/// static_assert in hven/detail/linear/pardiso_session.h; the equivalent
/// check in src/linear/accelerate_session.cpp). A sparse matrix whose
/// dimensions or nnz exceed a 32-bit index cannot be handed to this library
/// today, whatever this alias's own width is. Widening it is a backend
/// interface change -- an ILP64 MKL build at minimum -- not a typedef change.
using Index = Eigen::Index;

// The width/signedness pin: hven's boundary arithmetic, its counters and its
// serialized diagnostics all assume a signed 64-bit index; a narrower
// `ptrdiff_t` target would truncate silently rather than fail.
static_assert(sizeof(Index) == 8 && std::is_signed_v<Index>,
              "hven::Index must be a signed 64-bit type: hven's boundary sizes, counters and "
              "diagnostics all assume that width, and a narrower ptrdiff_t would truncate "
              "silently rather than fail (hven has no supported 32-bit target).");

// The identity pin: trivially true of the definition above -- which is the
// point. It fails the moment someone redefines `Index` to a fixed-width alias
// again, on the one target (Apple arm64) where `std::int64_t` and
// `std::ptrdiff_t` are distinct types despite matching width and signedness.
static_assert(std::is_same_v<Index, Eigen::Index>,
              "hven::Index must BE Eigen::Index, not merely match its width: on Apple arm64 "
              "std::int64_t (long long) and std::ptrdiff_t (long) are distinct types, and "
              "hven commits to one index vocabulary across every supported platform.");

/// Dense vector alias (column vector of double).
using Vec = Eigen::VectorXd;

/// Dense matrix alias (dense matrix of double).
using Mat = Eigen::MatrixXd;

/// The sparse matrix type used throughout hven: row-major double, matching
/// the KKT/Jacobian assembly layout and the storage layout both sparse
/// backends (MKL Pardiso, Apple Accelerate) expect.
///
/// StorageIndex is Eigen's default (`int`) deliberately and load-bearingly:
/// hven hands these index arrays straight to the backend with no reindexing
/// pass, which is sound only while the widths agree, and both backends are
/// built against 32-bit indices. This, not `Index` above, is the width that
/// bounds how large a matrix this library accepts.
using SpMatRM = Eigen::SparseMatrix<double, Eigen::RowMajor>;

/// Ref aliases for passing dense vectors/matrices across API boundaries
/// without forcing a copy or committing the caller to a specific storage
/// layout. The const versions additionally bind to a temporary/expression at
/// the call site (e.g. `x.head(3)`), which a plain `Eigen::Ref<const Vec>`
/// cannot do without the wrapping `const Eigen::Ref<...> &`.
using ConstVecRef = const Eigen::Ref<const Vec> &;
using VecRef = Eigen::Ref<Vec>;
using ConstMatRef = const Eigen::Ref<const Mat> &;
using MatRef = Eigen::Ref<Mat>;

} // namespace hven
