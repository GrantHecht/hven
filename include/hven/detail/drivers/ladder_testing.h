// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// ladder_testing.h — the SQP driver's ladder seam (M7 W2 ladder round, L1/L3).
// Compiles to nothing unless HVEN_TESTING is defined, which only the standalone
// hven_sqp_seam_tests executable does (docs/testing.md, the sixth seam).

#ifdef HVEN_TESTING

namespace hven::solvers::detail::testing {

/// Ends the TOP-LEVEL driver's next `fail_walk_qps` kOptimal walk subproblems as
/// the engine's own N3 exit does: status kNumericalError, multipliers cleared,
/// the iterate kept. It stands in for a QP the engine does not solve (E2-D3: the
/// DALLASM major-1 QP at a feasible point, the READING6 E2 terminations), which
/// no small model reproduces. The restoration sub-driver is never touched.
struct SqpLadderSeam {
    /// How many walk subproblems to end kNumericalError; 0 = off.
    static inline int fail_walk_qps = 0;
    /// Only once the solve's one restoration has resumed (the DALLASM shape).
    static inline bool only_after_resume = false;
    /// Stamp the exit as an E2 termination (`dep_blocker_terminations` = 1).
    static inline bool as_e2_termination = true;
    /// How many exits were forced (read by the test).
    static inline int forced = 0;

    static void reset() {
        fail_walk_qps = 0;
        only_after_resume = false;
        as_e2_termination = true;
        forced = 0;
    }
};

} // namespace hven::solvers::detail::testing

#endif
