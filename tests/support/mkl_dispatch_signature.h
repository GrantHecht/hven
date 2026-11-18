// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#ifndef HVEN_TESTS_SUPPORT_MKL_DISPATCH_SIGNATURE_H
#define HVEN_TESTS_SUPPORT_MKL_DISPATCH_SIGNATURE_H

// The MKL dispatch the exact route pins were measured at (route_reference.h; docs/testing.md
// "The reference build"), as an observable signature of the host that runs MKL: the CPU vendor,
// its AVX2 / FMA / AVX-512F bits, and MKL's own mkl_cbwr_get_auto_branch(). The derivation box
// (AMD Zen 3, AVX2, no AVX-512) reads AuthenticAMD, 1, 1, 0 and MKL_CBWR_AUTO -- MKL names no
// concrete CNR branch for that CPU. CI run 40985070468 attempt 2 moved 39 route cells on a runner
// whose native compile reported AVX-512/AVX10.1, with the asserting binary byte-for-byte the one
// attempt 1 passed with (x86-64-v3): the pins are a statement about this dispatch, not about
// "Linux x86-64 MKL" (WX-R13).
//
// Used twice, so the two cannot drift: compiled into the configure-time probe
// (mkl_dispatch_probe.cpp, which decides HVEN_TEST_REFERENCE_DISPATCH), and into the sentinel
// RouteReference.TheReferenceBuildRunsAtMklsAutoDispatch, which re-checks it on the host that
// RUNS the suite. Linux x86-64 MKL only.

#include <string>

#include <cpuid.h>
#include <mkl.h>

namespace hven::test_support {

struct MklDispatchSignature {
    std::string vendor;
    bool avx2 = false;
    bool fma = false;
    bool avx512f = false;
    int mkl_auto_branch = -1;
};

inline MklDispatchSignature observe_mkl_dispatch() {
    MklDispatchSignature s;
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (__get_cpuid(0, &a, &b, &c, &d) != 0) {
        char v[13] = {0};
        __builtin_memcpy(v + 0, &b, 4);
        __builtin_memcpy(v + 4, &d, 4);
        __builtin_memcpy(v + 8, &c, 4);
        s.vendor = v;
    }
    if (__get_cpuid(1, &a, &b, &c, &d) != 0) {
        s.fma = (c & bit_FMA) != 0;
    }
    if (__get_cpuid_count(7, 0, &a, &b, &c, &d) != 0) {
        s.avx2 = (b & bit_AVX2) != 0;
        s.avx512f = (b & bit_AVX512F) != 0;
    }
    s.mkl_auto_branch = ::mkl_cbwr_get_auto_branch();
    return s;
}

inline std::string describe(const MklDispatchSignature &s) {
    return "vendor=" + s.vendor + " avx2=" + std::to_string(int{s.avx2}) +
           " fma=" + std::to_string(int{s.fma}) + " avx512f=" + std::to_string(int{s.avx512f}) +
           " mkl_auto_branch=" + std::to_string(s.mkl_auto_branch) +
           " (MKL_CBWR_AUTO=" + std::to_string(MKL_CBWR_AUTO) + ")";
}

/// @brief True exactly on the derivation box's dispatch. Nothing else is the reference: an
///        unobserved dispatch is off-reference until a measurement says otherwise.
inline bool is_reference_mkl_dispatch(const MklDispatchSignature &s) {
    return s.vendor == "AuthenticAMD" && s.avx2 && s.fma && !s.avx512f &&
           s.mkl_auto_branch == MKL_CBWR_AUTO;
}

} // namespace hven::test_support

#endif // HVEN_TESTS_SUPPORT_MKL_DISPATCH_SIGNATURE_H
