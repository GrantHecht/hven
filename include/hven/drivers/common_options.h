// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The options both engines share, spelled once.
///
/// Each engine's options struct embeds exactly one `CommonOptions` as `common`.
/// A setting that means the same thing on both engines is spelled the same way on
/// both. Where the two engines' defaults differ (the iteration cap and the thread
/// count), each engine's options struct initializes its own `common`.

#include <optional>

#include <hven/core/start_level.h>
#include <hven/core/types.h>

namespace hven::solvers {

/// @brief How much a solve writes to the console (stdout).
///
/// Each rung prints everything the rung below it prints.
enum class Verbosity {
    /// Nothing. The default on both engines.
    kSilent,
    /// The exit status line and warnings.
    kWarnings,
    /// Adds the banners, the per-phase Beginning/Finished and exit blocks, the
    /// SQP table's header and trailer, and the timing summary. No iteration
    /// rows.
    kSummary,
    /// Adds the interior-point problem statistics and the per-iteration table.
    kIterations,
};

/// @brief The options both engines share, embedded once in each engine's own
///        options struct as `common`.
///
/// "No limit" is an empty `std::optional` on every limit; no numeric sentinel
/// means "unlimited".
struct CommonOptions {
    /// Optimality tolerance. The interior-point engine reads it against its KKT
    /// (dual infeasibility) measure and, unless
    /// `IpmOptions::termination.complementarity_tol` is set, its barrier
    /// (complementarity) measure. The SQP engine reads it against its
    /// stationarity measure and its NLP complementarity. Each engine compares it
    /// with its own measure, reported under its result's `convergence` fields.
    /// Default 1e-6. Must be finite and > 0.
    double optimality_tol = 1e-6;

    /// Feasibility tolerance: the equality and inequality residuals. The SQP
    /// engine also uses it as the bound-activity tolerance of its stationarity
    /// measure. Default 1e-6. Must be finite and > 0.
    double feasibility_tol = 1e-6;

    /// Iteration cap. The interior-point engine applies it PER PHASE of
    /// `IpmOptions::phases` (its default is 1000); the SQP engine counts
    /// subproblems solved, restoration majors included (its default is 100). A
    /// `SolveBudget::max_iterations` can only tighten it. Must be >= 1 on the
    /// interior-point engine and >= 0 on the SQP engine.
    Index max_iterations = 100;

    /// Deterministic work ceiling: the number of KKT factorizations one solve may
    /// perform, counted over the whole solve (every phase, every restoration).
    /// Read at iteration boundaries, so a solve can exceed it by the
    /// factorizations of the one iteration in flight. Reaching it ends the solve
    /// with `SolveStatus::kWorkLimit` at the current iterate. Empty (the default)
    /// is no ceiling. An engaged value must be >= 0.
    std::optional<Index> max_factorizations;

    /// Wall-clock limit in seconds, measured from solve entry. Read at iteration
    /// boundaries; reaching it ends the solve with `SolveStatus::kTimeLimit` at
    /// the current iterate. NOT REPRODUCIBLE: the iteration a timed-out solve stops
    /// at depends on the machine and its load, so a run that must be repeatable
    /// (a benchmark, a regression baseline) leaves this empty and bounds work with
    /// `max_factorizations` or `max_iterations`. Empty (the default) is no limit.
    /// An engaged value must be finite and > 0.
    std::optional<double> time_limit_seconds;

    /// Console output. Default `kSilent` on both engines; one line turns output
    /// on: `options.common.verbosity = Verbosity::kIterations;`.
    Verbosity verbosity = Verbosity::kSilent;

    /// Backend thread count for this solver's factorizations. 0 leaves the
    /// backend's own default alone (the process-wide MKL_NUM_THREADS then
    /// governs). The SQP default is 0; `IpmOptions` defaults it to
    /// HVEN_DEFAULT_QP_THREADS and requires it >= 1.
    ///
    /// The SQP engine applies it at call scope to every factorization path,
    /// nested restoration solves included, and undoes it on every exit; nothing
    /// writes a process-global or an environment setting. It is folded into the
    /// options fingerprint a hot handle is keyed on, so changing it refuses a
    /// handle produced at another count.
    ///
    /// On Apple the count is stored and applied to nothing (UNOBSERVED).
    ///
    /// This is not the interior-point engine's evaluation-pool thread count (a
    /// separate process-global setting, `utils::set_num_threads`).
    int threads = 0;

    /// The ceiling on how much of an offered warm start a solve will trust. A
    /// ceiling, never a floor, on both engines.
    ///
    /// SQP engine:
    ///  - `kSeeded`: takes a value's point and multipliers but never a
    ///    factorization or globalization state.
    ///  - `kCold`: every warm-taking solve behaves exactly as the cold one does.
    ///  - A solve given no warm start is always cold.
    ///
    /// Interior-point engine, on its payload route:
    ///  - `kCold`: the payload is ignored and counted
    ///    (`IpmResult::payload_ignored`). Its block lengths and declaration
    ///    stamp are still checked.
    ///  - `kSeeded`: the multipliers only; `x0` is the start and the polish
    ///    extension is ignored and counted (`IpmResult::polish_ignored`).
    ///  - `kWarm`, `kHot`: the whole payload -- point, multipliers, polish.
    StartLevel start_level = StartLevel::kWarm;

    /// The magnitude at or beyond which a bound is absent. DECLARED, NOT YET
    /// READ: no engine consults it until the absent-bound normalization lands;
    /// until then every finite bound is a real bound on the interior-point
    /// engine, as before. Validated now. Default 1e20. Must be > 0 (and not NaN);
    /// `kInfinity` means "every finite value is a real bound".
    double infinite_bound = 1e20;
};

} // namespace hven::solvers
