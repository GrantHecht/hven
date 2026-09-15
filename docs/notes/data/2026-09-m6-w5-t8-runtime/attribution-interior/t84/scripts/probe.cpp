#include "hven/drivers/interior_point_solver.h"
#include <cstdio>
int main() {
    std::printf("sizeof(InteriorPointSolver) = %zu\n", sizeof(hven::solvers::InteriorPointSolver));
    std::printf("alignof(InteriorPointSolver) = %zu\n", alignof(hven::solvers::InteriorPointSolver));
    return 0;
}
