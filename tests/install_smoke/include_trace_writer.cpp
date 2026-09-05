// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why, and which W5 tasks depend
// on it: this directory's CMakeLists.txt.

// W5 T4 moves the sink type into a public drivers/ header; this is the consumer view of it.

#include "hven/drivers/trace_writer.h"

namespace hven_install_smoke {
int standalone_include_trace_writer() { return 1; }
} // namespace hven_install_smoke
