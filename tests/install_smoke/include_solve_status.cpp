// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why, and which W5 tasks depend
// on it: this directory's CMakeLists.txt.

// W5 T8.2 added this header; it names both engines' status enums and must stand
// alone for a consumer that includes nothing else.

#include "hven/drivers/solve_status.h"

namespace hven_install_smoke {
int standalone_include_solve_status() { return 1; }
} // namespace hven_install_smoke
