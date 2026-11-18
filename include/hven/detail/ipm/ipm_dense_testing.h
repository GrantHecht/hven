// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The interior-point engine's dense-arm test seam (docs/testing.md; the convention of
// hven/detail/linear/fault_injection.h). Compiles to nothing unless HVEN_TESTING is defined, which
// only the standalone seam executables do.
//
// The IPM factors under DenseSingularPolicy::kReport whenever the dense arm is enabled. Under it
// the platform session never takes a dense-selected factorization, so FactorizeFaultInjector can
// no longer fail one there, and the hard-backend-error route of the no-numerics dispatch loses its
// only faithful vehicle (the sparse arm's initial factorization succeeds; see docs/testing.md).
// While `active`, the IPM keeps kSparseFallback instead: the policy those seam tests were written
// against.

#ifdef HVEN_TESTING

namespace hven::detail::testing {

struct IpmDenseSparseFallbackOverride {
    static inline bool active = false;
};

} // namespace hven::detail::testing

#endif
