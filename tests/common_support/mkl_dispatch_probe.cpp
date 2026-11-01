// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Configure-time probe (tests/CMakeLists.txt, try_run): prints the host's MKL dispatch signature
// and exits 0 iff it is the reference dispatch (mkl_dispatch_signature.h). Not a test.

#include <cstdio>

#include "mkl_dispatch_signature.h"

int main() {
    const hven::test_support::MklDispatchSignature s = hven::test_support::observe_mkl_dispatch();
    std::printf("%s\n", hven::test_support::describe(s).c_str());
    return hven::test_support::is_reference_mkl_dispatch(s) ? 0 : 1;
}
