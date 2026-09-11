// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why, and which W5 tasks depend
// on it: this directory's CMakeLists.txt.

// W5 T8.10 renames this to sqp_solver.h / SqpSolver.

#include "hven/drivers/sqp_driver.h"

namespace hven_install_smoke {
int standalone_include_sqp_driver() { return 1; }
} // namespace hven_install_smoke
