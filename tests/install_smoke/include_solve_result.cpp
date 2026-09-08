// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why, and which W5 tasks depend
// on it: this directory's CMakeLists.txt.

// W5 T8.4 added this header; it is the result core both engines report through
// and must stand alone for a consumer that includes neither engine.

#include "hven/drivers/solve_result.h"

namespace hven_install_smoke {
int standalone_include_solve_result() { return 1; }
} // namespace hven_install_smoke
