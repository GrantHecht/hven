// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why, and which W5 tasks depend
// on it: this directory's CMakeLists.txt.

// W5 T3 added this header for the deprecation-suppression bracket a consumer
// needs when it keeps calling a by-value evaluation on purpose. It defines
// macros and nothing else, so it must compile with no include of its own -- and
// the bracket has to WORK from an install tree, which is what the pair below is.

#include "hven/core/compiler.h"

namespace hven_install_smoke {
HVEN_SUPPRESS_DEPRECATED_BEGIN
int standalone_include_compiler() { return 1; }
HVEN_SUPPRESS_DEPRECATED_END
} // namespace hven_install_smoke
