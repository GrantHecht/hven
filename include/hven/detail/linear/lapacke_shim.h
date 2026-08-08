#pragma once

// LAPACKE-compatible shim over Apple Accelerate's f77 LAPACK.
//
// Apple Accelerate does not ship LAPACKE: its LAPACK is the classic
// Fortran-ABI interface (trailing-underscore symbols, scalar arguments by
// pointer) with no automatic workspace sizing, so `dsytrf_` needs the
// caller-side workspace query (`lwork = -1`, read back `work[0]`) that
// `LAPACKE_dsytrf` performs internally on MKL. This header implements ONLY
// the two entry points `DenseSymmetricFactor` uses --
// `LAPACKE_dsytrf`/`LAPACKE_dsytrs`, column-major layout only -- so the
// dense factor/solve logic itself needs no `#ifdef` branching once these two
// names resolve, whether to MKL's own LAPACKE header or to this shim.
//
// Migrated from Tycho. This file originated in the SQP solver's standalone
// development (a companion engine now sharing this repository's identity;
// see docs/pattern-hash.md for another primitive with the same lineage)
// before being adjusted for hven's use here -- the shimmed entry points and
// their behavior are unchanged.
//
// It relies on ACCELERATE_NEW_LAPACK being defined project-wide (see the
// root CMakeLists.txt) so `dsytrf_`/`dsytrs_` come from Accelerate's own
// <lapack.h> with `__LAPACK_int == int` under the default LP64 interface --
// declaring the symbols locally would collide with Accelerate's own
// declarations in TUs that include both this header and Accelerate.h.
//
// Both functions follow LAPACKE's error-code contract (0 success, -i illegal
// argument i, +i exact zero pivot at i) and never print or throw; the call
// site in dense_symmetric_factor.cpp folds nonzero info values into thrown
// exceptions, which is where hven's error-handling rules are satisfied.

#include <algorithm>
#include <limits>
#include <vector>

#include <Accelerate/Accelerate.h>

namespace hven::linear::detail {

using lapack_int = __LAPACK_int;

// dense_symmetric_factor.cpp stores ipiv_ as std::vector<int> and reads the
// entries as ints; the ILP64 interface (ACCELERATE_LAPACK_ILP64) must not be
// enabled silently underneath it.
static_assert(sizeof(lapack_int) == sizeof(int),
              "lapacke_shim.h requires the LP64 Accelerate LAPACK interface "
              "(__LAPACK_int == int); ACCELERATE_LAPACK_ILP64 is not supported here.");

} // namespace hven::linear::detail

#define LAPACK_ROW_MAJOR 101
#define LAPACK_COL_MAJOR 102

namespace hven::linear::detail {

inline lapack_int LAPACKE_dsytrf(int matrix_layout, char uplo, lapack_int n, double *a,
                                 lapack_int lda, lapack_int *ipiv) {
    if (matrix_layout != LAPACK_COL_MAJOR) {
        return -1; // LAPACKE convention: argument 1 (matrix_layout) is illegal
    }
    if (n == 0) {
        return 0;
    }
    lapack_int info = 0;
    // Workspace query: lwork = -1 asks dsytrf_ to report the optimal size in
    // work[0] without factorizing.
    double work_query = 0.0;
    lapack_int lwork = -1;
    dsytrf_(&uplo, &n, a, &lda, ipiv, &work_query, &lwork, &info);
    if (info != 0) {
        return info;
    }
    // Clamp before narrowing double -> lapack_int: a query beyond INT_MAX
    // would wrap. Unreachable at this component's small-matrix sizes,
    // guarded anyway.
    const double query_max = static_cast<double>(std::numeric_limits<lapack_int>::max());
    lwork = static_cast<lapack_int>(std::min(work_query, query_max));
    if (lwork < 1) {
        lwork = 1;
    }
    std::vector<double> work(static_cast<std::size_t>(lwork));
    dsytrf_(&uplo, &n, a, &lda, ipiv, work.data(), &lwork, &info);
    return info;
}

inline lapack_int LAPACKE_dsytrs(int matrix_layout, char uplo, lapack_int n, lapack_int nrhs,
                                 const double *a, lapack_int lda, const lapack_int *ipiv, double *b,
                                 lapack_int ldb) {
    if (matrix_layout != LAPACK_COL_MAJOR) {
        return -1; // LAPACKE convention: argument 1 (matrix_layout) is illegal
    }
    lapack_int info = 0;
    dsytrs_(&uplo, &n, &nrhs, a, &lda, ipiv, b, &ldb, &info);
    return info;
}

} // namespace hven::linear::detail
