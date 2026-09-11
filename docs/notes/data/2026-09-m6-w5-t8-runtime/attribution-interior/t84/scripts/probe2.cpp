#include "hven/drivers/interior_point_solver.h"
#include <cstdio>
namespace hven::solvers {
#if HAS_IPMRESULT
using RT = IpmResult;
#else
using RT = InteriorPointSolver::SolveResult;
#endif
}
int main() {
    std::printf("sizeof(solver)=%zu sizeof(result)=%zu\n",
                sizeof(hven::solvers::InteriorPointSolver), sizeof(hven::solvers::RT));
    return 0;
}
