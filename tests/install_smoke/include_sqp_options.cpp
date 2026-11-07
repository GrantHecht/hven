// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why: this directory's
// CMakeLists.txt. Added at M8 AP-2 B3 (the options headers).

#include "hven/drivers/sqp_options.h"

namespace hven_install_smoke {
int standalone_include_sqp_options() { return 1; }
} // namespace hven_install_smoke
