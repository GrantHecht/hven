#include <gtest/gtest.h>

// Proves the sparse-backend PUBLIC linkage on hven::hven actually resolves a
// real backend symbol at link time, not just an unused, present-but-
// unreferenced dependency. A regression in the MKL --start-group/--end-group
// static-archive wrapping, the sparse-backend include dirs, or the RPATH
// handling in cmake/FindMKL.cmake (Linux/Windows) breaks this test at build
// (missing mkl.h) or link (undefined mkl_get_version) time, well before it
// would ever surface inside a solver.
#if defined(USE_ACCELERATE_SPARSE)

// Apple Accelerate backend: hven_resolve_sparse_backend() (cmake/
// hven_sparse_backend.cmake) defines USE_ACCELERATE_SPARSE and links
// AccelerateSparse::AccelerateSparse instead of MKL. Accelerate has no
// mkl_get_version()-shaped self-report entry point to probe the same way;
// this leg is skipped because there is no equivalent entry point to probe.
TEST(SparseBackendLink, AcceleratePlaceholderUnobserved) {
    GTEST_SKIP() << "Accelerate backend: no MKL-equivalent entry point to "
                    "probe here; see docs/ci.md Mac lane.";
}

#else

#include <mkl.h>

TEST(SparseBackendLink, MklGetVersionResolvesThroughPublicLinkage) {
    MKLVersion v;
    mkl_get_version(&v);
    EXPECT_GT(v.MajorVersion, 0);
}

#endif
