// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

namespace hven::detail {

/// @brief Ensures the math runtime is initialized (thread pool, CPU dispatch,
///        memory allocators). Thread-safe (std::call_once).
///
/// On MKL: calls mkl_get_max_threads(), which brings up the threading layer.
///   Must not call dsecnd() (its first call spins ~1 s calibrating the clock).
/// On Accelerate: sets VECLIB_MAXIMUM_THREADS, triggers a trivial BLAS call,
/// and warms up the sparse solver subsystem (including MT-METIS thread pool).
///
/// @return The wall-clock time in milliseconds the first init took,
///         or 0.0 if already initialized.
double ensure_solver_initialized();

} // namespace hven::detail
