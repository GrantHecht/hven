// Configure-time probe: prints the value Eigen resolves EIGEN_MAX_ALIGN_BYTES
// to under whatever SIMD flags the caller compiles this with. Compiled and
// run via try_run() from the root CMakeLists.txt, using the SAME SIMD_FLAGS
// hven's own sources are compiled with, so the exported
// EIGEN_MAX_ALIGN_BYTES compile definition (a real ABI-layout quantity: it
// determines the alignment Eigen assumes for its own heap allocations and
// fixed-size vectorized types) can never drift out of sync with what Eigen
// itself would compute for hven's own build -- no ISA-to-alignment table is
// duplicated here.
#include <cstdio>

#include <Eigen/Core>

int main() {
    std::printf("%d", EIGEN_MAX_ALIGN_BYTES);
    return 0;
}
