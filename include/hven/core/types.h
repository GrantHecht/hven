#pragma once

// Shared Eigen-based type aliases used across hven. Mirrors the shape of
// tycho's own typedefs (psiopt/include/tycho/detail/typedefs/eigen_types.h)
// but keeps ONLY the aliases the linear-algebra layer and its test rig need
// right now — nothing speculative.

#include <cstdint>

#include <Eigen/Core>
#include <Eigen/SparseCore>

namespace hven {

// A signed 64-bit index type, used for sizes/indices at hven's own API
// boundaries (row/column counts, nnz, ...). Kept as a fixed-width alias
// independent of Eigen's own Index typedef (platform-defined, typically
// std::ptrdiff_t) so hven's public interfaces commit to one portable index
// type regardless of build/platform.
using Index = std::int64_t;

// Dense vector/matrix aliases.
using Vec = Eigen::VectorXd;
using Mat = Eigen::MatrixXd;

// The sparse matrix type used throughout hven: row-major double, matching
// the KKT/Jacobian assembly layout the linear-algebra layer and the sparse
// backends (MKL Pardiso, Apple Accelerate) both expect.
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
