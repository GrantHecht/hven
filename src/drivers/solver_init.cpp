// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include "hven/detail/drivers/solver_init.h"

#include <mutex>

#include "hven/detail/interior/utils/timer.h"

#ifdef USE_ACCELERATE_SPARSE
#include "hven/detail/interior/utils/accelerate_threads.h"
#else
#include <mkl.h>
#endif

namespace hven {

double detail::ensure_solver_initialized() {
    static std::once_flag flag;
    double first_init_ms = 0.0;
    std::call_once(flag, [&first_init_ms]() {
        hven::detail::Timer t;
        t.start();
#ifdef USE_ACCELERATE_SPARSE
        ensure_accelerate_initialized(HVEN_DEFAULT_IPM_THREADS);
#else
        // Brings up MKL's threading layer (the OpenMP runtime), the first-call
        // cost a first factorization would otherwise carry. Not dsecnd(): its
        // first call calibrates the clock by spinning to a wall-second boundary
        // (~1 s, ~4e9 instructions) and warms nothing Pardiso uses.
        (void)mkl_get_max_threads();
#endif
        t.stop();
        first_init_ms = double(t.count<std::chrono::microseconds>()) / 1000.0;
    });
    return first_init_ms;
}

} // namespace hven
