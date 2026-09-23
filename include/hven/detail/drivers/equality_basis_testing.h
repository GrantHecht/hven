// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// equality_basis_testing.h — SqpOptions::equality_basis's test seam (M7 W2 E1b).
// Compiles to nothing unless HVEN_TESTING is defined, which only the standalone
// hven_sqp_seam_tests executable does (docs/testing.md, the fifth seam).

#ifdef HVEN_TESTING

namespace hven::solvers::detail::testing {

struct EqualityBasisSeam {
    /// The zero-step hand-off at a suspected inconsistency does nothing (the
    /// negative control of SqpEqualityBasis.F9).
    static inline bool disable_hand_off = false;
    /// When positive, an SSN prox-centre lift is accepted only if its forward-error
    /// bound is <= this * ||lambda_B||_2; a rejected one takes the zero-scatter lift,
    /// exactly as a lift that failed its own bound.
    static inline double prox_lift_forward_tol = 0.0;
    /// When positive, the warm structural probe's projection is accepted only if its
    /// forward-error estimate is <= this (absolute); a rejected one leaves the probe
    /// unreduced, exactly as reduce() returning false does.
    static inline double warm_probe_forward_tol = 0.0;

    static void reset() {
        disable_hand_off = false;
        prox_lift_forward_tol = 0.0;
        warm_probe_forward_tol = 0.0;
    }
};

} // namespace hven::solvers::detail::testing

#endif
