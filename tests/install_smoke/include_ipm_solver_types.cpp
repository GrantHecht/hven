// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why, and which W5 tasks depend
// on it: this directory's CMakeLists.txt.

// W5 T8.3 added this header; it carries the interior-point engine's options and
// the eight mode enums that used to be nested inside the solver class, and must
// stand alone for a consumer that names an option without including the engine.

#include "hven/drivers/ipm_solver_types.h"

namespace hven_install_smoke {
int standalone_include_ipm_solver_types() { return 1; }
} // namespace hven_install_smoke
