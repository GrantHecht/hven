// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why, and which W5 tasks depend
// on it: this directory's CMakeLists.txt.

// W5 T4 splits the evidence structs out of ipqp_engine.h; both headers must stand alone.

#include "hven/detail/qp/ipqp_trace.h"

namespace hven_install_smoke {
int standalone_include_ipqp_trace() { return 1; }
} // namespace hven_install_smoke
