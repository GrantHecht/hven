// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why, and which W5 tasks depend
// on it: this directory's CMakeLists.txt.

// W5 T4 split the two escape-evidence blocks out of ipqp_engine.h into this
// header, whose whole point is that it needs core/types.h and nothing else.

#include "hven/detail/qp/ipqp_evidence.h"

namespace hven_install_smoke {
int standalone_include_ipqp_evidence() { return 1; }
} // namespace hven_install_smoke
