#pragma once

// Shared Eigen-based type aliases used across hven. Mirrors the shape of
// the origin project's own Eigen typedefs but keeps ONLY the aliases the linear-algebra layer and its test rig need
// right now — nothing speculative.

#include <cstdint>

#include <Eigen/Core>
#include <Eigen/SparseCore>

namespace hven {

// A signed 64-bit index type, used for sizes/indices at hven's own API
// BOUNDARIES (row/column counts, nnz, ...). Kept as a fixed-width alias
// independent of Eigen's own Index typedef (platform-defined, typically
// std::ptrdiff_t) so hven's public interfaces commit to one portable index
// type regardless of build/platform.
//
// WHERE THAT STOPS, stated because "64-bit indices" otherwise reads as a
// promise the library does not keep: the width is 64-bit at the SIGNATURES,
// not in the sparse storage they carry. SpMatRM below uses Eigen's default
// `int` storage index, and both sparse backends require exactly that width
// (the static_assert in hven/detail/linear/pardiso_session.h; the equivalent
// check in src/linear/accelerate_session.cpp). A sparse matrix whose
// dimensions or nnz exceed a 32-bit index cannot be handed to this library
// today, whatever this alias's own width is. Widening it is a backend
// interface change -- an ILP64 MKL build at minimum -- not a typedef change.
using Index = std::int64_t;

// Dense vector/matrix aliases.
using Vec = Eigen::VectorXd;
using Mat = Eigen::MatrixXd;

// The sparse matrix type used throughout hven: row-major double, matching
// the KKT/Jacobian assembly layout the linear-algebra layer and the sparse
// backends (MKL Pardiso, Apple Accelerate) both expect.
//
// StorageIndex is Eigen's default -- `int` -- deliberately and load-bearingly:
// hven hands these index arrays straight to the backend with no reindexing
// pass, which is sound only while the widths agree, and both backends are
// built against 32-bit indices here. That, not `Index` above, is the width
// that bounds how large a matrix this library accepts.
using SpMatRM = Eigen::SparseMatrix<double, Eigen::RowMajor>;

// Ref aliases for passing dense vectors/matrices across API boundaries
// without forcing a copy or committing the caller to a specific storage
// layout. The const versions additionally bind to a temporary/expression at
// the call site (e.g. `x.head(3)`), which a plain `Eigen::Ref<const Vec>`
// cannot do without the wrapping `const Eigen::Ref<...> &`.
using ConstVecRef = const Eigen::Ref<const Vec> &;
using VecRef = Eigen::Ref<Vec>;
using ConstMatRef = const Eigen::Ref<const Mat> &;
using MatRef = Eigen::Ref<Mat>;

} // namespace hven
