#include "hven/detail/drivers/solver_init.h"

#include <mutex>

#include "hven/detail/interior/utils/timer.h"

#ifdef USE_ACCELERATE_SPARSE
#include "hven/detail/interior/utils/accelerate_threads.h"
#else
#include <mkl.h>
#endif

namespace hven::solvers {

double ensure_solver_initialized() {
    static std::once_flag flag;
    double first_init_ms = 0.0;
    std::call_once(flag, [&first_init_ms]() {
        hven::utils::Timer t;
        t.start();
#ifdef USE_ACCELERATE_SPARSE
        ensure_accelerate_initialized(TYCHO_DEFAULT_QP_THREADS);
#else
        dsecnd();
#endif
        t.stop();
        first_init_ms = double(t.count<std::chrono::microseconds>()) / 1000.0;
    });
    return first_init_ms; // 0.0 on every call after the first (header contract)
}

} // namespace hven::solvers
