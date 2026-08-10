// =============================================================================
// Tycho (Copyright 2026-present Grant R. Hecht, Apache 2.0 — see LICENSE.txt)
// =============================================================================
//
// Fallback implementation of the Ipopt backend entry points for builds without
// Ipopt. Selecting the ipopt backend in such a build is a configuration error,
// reported as a std::runtime_error naming the build option that enables it.

#include "hven/detail/drivers/ipopt_backend.h"

#include <stdexcept>

#include "hven/drivers/optimization_problem_base.h"

namespace hven::solvers::ipopt_backend {

bool available() { return false; }

OptimizationProblemBase::NlpSolveOutput solve(OptimizationProblemBase &prob,
                                              OptimizationProblemBase::JetJobModes mode,
                                              const Eigen::VectorXd &input) {
    (void)prob;
    (void)mode;
    (void)input;
    throw std::runtime_error("Tycho was built without Ipopt support; configure with "
                             "-DHVEN_ENABLE_IPOPT=ON (requires an installed Ipopt discoverable "
                             "via pkg-config) to use nlp_solver = ipopt");
}

} // namespace hven::solvers::ipopt_backend
