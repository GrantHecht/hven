// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

// The thread assignment policy a solver function carries: SolverFunctionBase
// stores one per function and NonLinearProgram's partitioner is the only
// reader.

#pragma once

namespace hven {

/// @brief Selects the thread assignment / dispatch strategy for a VF evaluation.
/// @ingroup vf
///
/// Negative values are special dispatch modes; non-negative values name an
/// explicit worker-thread index.
enum class ThreadingFlags : int {
    kRoundRobin = -4,    ///< @brief Distribute work round-robin across the pool.
    kMainThread = -3,    ///< @brief Run on the calling (main) thread.
    kNeedsPool = -2,     ///< @brief Requires a thread pool to execute.
    kByApplication = -1, ///< @brief Threading decided by the application.
    kThread0 = 0,        ///< @brief Run on worker thread 0.
    kThread1 = 1,        ///< @brief Run on worker thread 1.
    kThread2 = 2,        ///< @brief Run on worker thread 2.
    kThread3 = 3,        ///< @brief Run on worker thread 3.
    kThread4 = 4,        ///< @brief Run on worker thread 4.
    kThread5 = 5,        ///< @brief Run on worker thread 5.
    kThread6 = 6,        ///< @brief Run on worker thread 6.
    kThread7 = 7,        ///< @brief Run on worker thread 7.
    kThread8 = 8,        ///< @brief Run on worker thread 8.
    kThread9 = 9,        ///< @brief Run on worker thread 9.
    kThread10 = 10,      ///< @brief Run on worker thread 10.
    kThread11 = 11,      ///< @brief Run on worker thread 11.
    kThread12 = 12,      ///< @brief Run on worker thread 12.
    kThread13 = 13,      ///< @brief Run on worker thread 13.
    kThread14 = 14,      ///< @brief Run on worker thread 14.
    kThread15 = 15,      ///< @brief Run on worker thread 15.
    kThread16 = 16,      ///< @brief Run on worker thread 16.
    kThread17 = 17,      ///< @brief Run on worker thread 17.
    kThread18 = 18,      ///< @brief Run on worker thread 18.
    kThread19 = 19,      ///< @brief Run on worker thread 19.
    kThread20 = 20,      ///< @brief Run on worker thread 20.
    kThread21 = 21,      ///< @brief Run on worker thread 21.
    kThread22 = 22,      ///< @brief Run on worker thread 22.
    kThread23 = 23,      ///< @brief Run on worker thread 23.
    kThread24 = 24,      ///< @brief Run on worker thread 24.
    kThread25 = 25,      ///< @brief Run on worker thread 25.
    kThread26 = 26,      ///< @brief Run on worker thread 26.
    kThread27 = 27,      ///< @brief Run on worker thread 27.
    kThread28 = 28,      ///< @brief Run on worker thread 28.
};

} // namespace hven
