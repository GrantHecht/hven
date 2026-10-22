// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// ladder_testing.h — the SQP driver's ladder seam (M7 W2 ladder round, L1/L3).
// Compiles to nothing unless HVEN_TESTING is defined, which only the standalone
// hven_sqp_seam_tests executable does (docs/testing.md, the sixth seam).

#ifdef HVEN_TESTING

#include <vector>

namespace hven::solvers::detail::testing {

/// Ends the TOP-LEVEL driver's next `fail_walk_qps` kOptimal walk subproblems as
/// the engine's own N3 exit does: status kNumericalError, multipliers cleared,
/// the iterate kept. It stands in for a QP the engine does not solve (E2-D3: the
/// DALLASM major-1 QP at a feasible point, the READING6 E2 terminations), which
/// no small model reproduces. The restoration sub-driver is touched only by
/// `fail_restoration_walk_qps`.
struct SqpLadderSeam {
    /// How many walk subproblems to end kNumericalError; 0 = off.
    static inline int fail_walk_qps = 0;
    /// How many kOptimal walk subproblems to let through before the first forced exit.
    static inline int pass_walk_qps = 0;
    /// Non-empty: the forced exit's iterate is the step to THIS point (engine variables), a walk
    /// that stopped somewhere other than the optimum (M7 W2 R2 fix 1). Empty: the solved
    /// optimum is kept, as before.
    static inline std::vector<double> forced_point;
    /// Only once the solve's one restoration has resumed (the DALLASM shape).
    static inline bool only_after_resume = false;
    /// Stamp the exit as an E2 termination (`dep_blocker_terminations` = 1).
    static inline bool as_e2_termination = true;
    /// Stamp the exit as a certificate bound-half refusal (`certificate_bound_refusals` = 1).
    static inline bool as_bound_refusal = false;
    /// Stamp the exit as a certificate row-half refusal (`certificate_row_refusals` = 1).
    static inline bool as_row_refusal = false;
    /// How many exits were forced (read by the test).
    static inline int forced = 0;
    /// THE RESTORATION SUB-DRIVER'S OWN (M7 W2 RST): how many of ITS kOptimal walk
    /// subproblems to end kNumericalError the same way (multipliers cleared, the iterate
    /// kept, no E2 stamp); 0 = off. The knobs above never touch the sub-driver.
    static inline int fail_restoration_walk_qps = 0;
    /// How many restoration sub-driver exits were forced (read by the test).
    static inline int forced_restoration = 0;

    static void reset() {
        fail_walk_qps = 0;
        pass_walk_qps = 0;
        forced_point.clear();
        only_after_resume = false;
        as_e2_termination = true;
        as_bound_refusal = false;
        as_row_refusal = false;
        forced = 0;
        fail_restoration_walk_qps = 0;
        forced_restoration = 0;
    }
};

} // namespace hven::solvers::detail::testing

#endif
