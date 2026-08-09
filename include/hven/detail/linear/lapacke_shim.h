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
// hven's build defines ACCELERATE_NEW_LAPACK project-wide on Apple, next to
// USE_ACCELERATE_SPARSE (cmake/hven_sparse_backend.cmake, the same
// arrangement the source project uses in its own root CMakeLists.txt).
// ACCELERATE_NEW_LAPACK selects Apple's non-deprecated, const-correct f77
// LAPACK declarations from <Accelerate/Accelerate.h> -- in particular,
// `dsytrs_`'s `a` parameter as `const double *` (matching the `const
// double *a` this shim passes through) and the `__LAPACK_int` spelling
// `lapack_int` aliases below. Without it, Accelerate.h resolves to the
// legacy declarations instead, which are not const-correct and do not
// provide `__LAPACK_int`; the #ifndef guard immediately below turns that
// mismatch into a compile-time diagnostic here rather than a confusing
// error at the first call site. This build-time claim is checked by the
// guard itself, and macOS CI compiles and exercises the shim against the
// real Accelerate framework through the dense-factor tests.
//
// Both functions follow LAPACKE's error-code contract (0 success, -i illegal
// argument i, +i exact zero pivot at i) and never print or throw; the call
// site in dense_symmetric_factor.cpp folds nonzero info values into thrown
// exceptions, which is where hven's error-handling rules are satisfied.

#ifndef ACCELERATE_NEW_LAPACK
#error "lapacke_shim.h requires ACCELERATE_NEW_LAPACK (see cmake/hven_sparse_backend.cmake)"
#endif

#include <algorithm>
#include <limits>
#include <type_traits>
#include <vector>

#include <Accelerate/Accelerate.h>

// LAPACKE's layout constants. Guarded rather than assumed unowned: if a
// future Accelerate release ever ships its own LAPACKE (making this whole
// shim unnecessary), these would already be defined and the source file
// this was migrated from would need to drop this header entirely -- this
// guard just keeps a same-named future definition from producing a
// redefinition error in the meantime. LAPACK_ROW_MAJOR is defined for
// LAPACKE parity; this shim only ever calls with LAPACK_COL_MAJOR.
#ifndef LAPACK_ROW_MAJOR
#define LAPACK_ROW_MAJOR 101
#endif
#ifndef LAPACK_COL_MAJOR
#define LAPACK_COL_MAJOR 102
#endif

namespace hven::linear::detail {

using lapack_int = __LAPACK_int;

// dense_symmetric_factor.cpp passes ipiv_.data() (a std::vector<int>,
// mandated by DenseSymmetricFactor's frozen public surface) directly as a
// lapack_int* argument to LAPACKE_dsytrf/LAPACKE_dsytrs below -- a raw
// pointer conversion that is only well-formed when lapack_int IS int (not
// merely same-sized), since std::vector<int>::data() returns int* and no
// implicit pointer conversion exists between distinct same-size types. The
// LP64 Accelerate LAPACK interface (the default; ACCELERATE_LAPACK_ILP64
// not enabled) satisfies this on every macOS CI compile. This assert keeps a
// future interface change loud instead of allowing silent corruption.
static_assert(std::is_same_v<lapack_int, int>,
              "lapacke_shim.h requires the LP64 Accelerate LAPACK interface "
              "(__LAPACK_int == int); ACCELERATE_LAPACK_ILP64 is not supported here.");

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
