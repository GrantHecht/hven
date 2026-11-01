// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The options both engines share. Each engine's options struct embeds exactly
// one of these, so a knob that means the same thing on both is spelled the same
// way on both: the backend thread count, the console verbosity, and the ceiling
// on how much of a warm start a solve is willing to trust.
//
// The defaults here are the SQP engine's; IpmOptions default-initialises its own
// `common` to the interior-point engine's values instead (see ipm_solver_types.h).

#include <hven/core/start_level.h>

namespace hven::solvers {

/// @brief The options both engines share, embedded once in each engine's own
///        options struct.
struct CommonOptions {
    /// Backend thread count for this solver's factorizations. 0 leaves the
    /// backend's own default alone (the process-wide MKL_NUM_THREADS then
    /// governs). The SQP default is 0; IpmOptions defaults it to
    /// HVEN_DEFAULT_QP_THREADS.
    ///
    /// On the interior-point engine it is applied wherever its factorizations
    /// are configured. On the SQP engine it reaches every factor path: the walk
    /// tier's K0 border and its per-solve, EQP-refine and verdict-refine
    /// temporaries, the SSN tier's persistent factor, the IPQP tier's KKT
    /// factorization, the parametric predictor's factorization, the Schur
    /// border's dense LAPACK factor, and each of those inside a nested
    /// restoration solve (the sub-solver inherits this whole struct).
    ///
    /// On the SQP engine it is applied at call scope and undone afterward, on
    /// every exit: nothing writes a process-global or an environment setting.
    /// It is folded into the options fingerprint a hot handle is keyed on, so
    /// changing it refuses a handle produced at another count.
    ///
    /// On Apple the count is stored and applied to nothing (UNOBSERVED): the
    /// Accelerate sparse session records it without handing it to any backend
    /// call, and the SQP engine deliberately does not mirror the interior-point
    /// engine's process-wide `accelerate_set_num_threads()`, which is never
    /// restored and so cannot keep the call-scoped promise above.
    ///
    /// This is not the interior-point engine's evaluation-pool thread count (a
    /// separate process-global knob, `utils::set_num_threads`), and not CNR,
    /// which is interior-point-only.
    int threads = 0;

    /// Console verbosity, on the interior-point engine's scale in both engines.
    /// Must be non-negative.
    ///   0  -- full output (stats + iteration table + exit + timing)
    ///   1  -- no iteration table (phase banners + timing summary)
    ///   2  -- exit status and warnings only
    ///   3+ -- fully silent
    ///
    /// The default 3 is silent; IpmOptions defaults it to 0.
    int print_level = 3;

    /// The ceiling on how much of an offered warm start a solve will trust. A
    /// ceiling, never a floor, on both engines: it can only lower what a value
    /// would otherwise have resolved to.
    ///
    /// SQP engine:
    ///  - `kSeeded`: ingests a hash-less object's values but never a
    ///    factorization, the funnel or trust-region state, or the
    ///    Kungurtsev-Diehl window, even when the object would have earned
    ///    `kWarm`; it short-circuits the structural-hash probe, whose answer
    ///    could only raise the level above the ceiling.
    ///  - `kCold`: every warm-taking solve behaves exactly as the cold one does.
    ///  - `solve(model, x0)` is always cold, whatever this says.
    ///
    /// Interior-point engine, on its payload route `solve(model, x0, warm,
    /// budget)`:
    ///  - `kCold`: the payload is ignored and counted
    ///    (`IpmResult::payload_ignored`). Its block lengths and declaration
    ///    stamp are still checked: a ceiling says what of a payload to apply,
    ///    never that a foreign one is acceptable.
    ///  - `kSeeded`: the multipliers only, through the engine's own floor and
    ///    cap; the point and the polish extension are ignored, the extension
    ///    counted (`IpmResult::polish_ignored`). `x0` is the start.
    ///  - `kWarm`: the whole payload -- point, multipliers, polish.
    ///  - `kHot`: identical to `kWarm`. The engine has no hot handle to adopt
    ///    (its cross-call factorization reuse is keyed on the program's own
    ///    analysis identity, never on a payload).
    StartLevel start_level = StartLevel::kWarm;
};

} // namespace hven::solvers
