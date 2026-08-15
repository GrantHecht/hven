#pragma once

// Shared Eigen-based type aliases used across hven. Mirrors the shape of
// the origin project's own Eigen typedefs but keeps ONLY the aliases the linear-algebra layer and
// its test rig need right now — nothing speculative.

#include <type_traits>

#include <Eigen/Core>
#include <Eigen/SparseCore>

namespace hven {

// The index type used for sizes/indices at hven's own API BOUNDARIES
// (row/column counts, nnz, ...): Eigen's index type, `std::ptrdiff_t` --
// pointer-width, and 64-bit and signed on every supported platform (x86-64
// Linux, x86-64 Windows, Apple arm64). Both properties are pinned below.
//
// IT IS EIGEN'S TYPE ON PURPOSE, and that is a change of contract made
// deliberately in M3 phase-C S2c (owner-ordered), not an accident of
// spelling. This alias used to be a fixed-width `std::int64_t`, chosen to be
// independent of Eigen's platform-defined one. Independence turned out to
// cost more than it bought: on Apple arm64 `std::int64_t` is `long long`
// while `std::ptrdiff_t` is `long`, so the library's boundary type and the
// type every Eigen expression in it already produced were two DISTINCT types
// on one supported target -- same width, same signedness, different identity,
// with the overload-resolution and template-matching divergence that implies.
// (That divergence is not hypothetical: it was measured at the CI
// macos-clang-release lane, run 31902660573.) Naming Eigen's type directly
// makes the library one index vocabulary everywhere, and the `is_same_v` pin
// below is what keeps it that way.
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
using Index = Eigen::Index;

// The width/signedness pin. hven's boundary arithmetic, its counters and its
// serialized diagnostics all assume a signed 64-bit index; a 32-bit
// `ptrdiff_t` target would silently truncate rather than fail, so it must
// fail here instead.
static_assert(sizeof(Index) == 8 && std::is_signed_v<Index>,
              "hven::Index must be a signed 64-bit type: hven's boundary sizes, counters and "
              "diagnostics all assume that width, and a narrower ptrdiff_t would truncate "
              "silently rather than fail (hven has no supported 32-bit target).");

// The identity pin. This is the contract S2c established: hven's boundary
// index type and Eigen's are ONE type, on every target, so a value never
// changes identity crossing between hven's own signatures and the Eigen
// expressions behind them. It is trivially true of the definition above --
// which is the point: it fails the moment someone redefines `Index` to a
// fixed-width alias again, on the one target (Apple arm64) where that is not
// the same type.
static_assert(std::is_same_v<Index, Eigen::Index>,
              "hven::Index must BE Eigen::Index, not merely match its width: on Apple arm64 "
              "std::int64_t (long long) and std::ptrdiff_t (long) are distinct types, and "
              "hven commits to one index vocabulary across every supported platform.");

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
