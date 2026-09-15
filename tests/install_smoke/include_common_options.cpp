// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why, and which W5 tasks depend
// on it: this directory's CMakeLists.txt.

// W5 T8.3 added this header; it is the options both engines share and must
// stand alone for a consumer that includes nothing else.

#include "hven/drivers/common_options.h"

namespace hven_install_smoke {
int standalone_include_common_options() { return 1; }
} // namespace hven_install_smoke
