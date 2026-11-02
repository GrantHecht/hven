// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why, and which W5 tasks depend
// on it: this directory's CMakeLists.txt.
//
// M8 AP-2 B2: the fixed-variable vocabulary IpmOptions carries, split out of the
// interior-point program's header when that header left the public surface.

#include "hven/model/fixed_variable_treatment.h"

namespace hven_install_smoke {
int standalone_include_fixed_variable_treatment() { return 1; }
} // namespace hven_install_smoke
