// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why, and which W5 tasks depend
// on it: this directory's CMakeLists.txt.

// W5 T1 folded OptimizationProblemBase into NLPSolver here and dropped seven of
// this header's own includes: the header the window changed most, and the one
// whose self-containedness a consumer notices first.
//
// main.cpp includes it too, but main.cpp also includes <cmath>, <memory> and
// the rest for its own use -- exactly the cover this TU refuses to give it.

#include "hven/model/nlp_solver.h"

namespace hven_install_smoke {
int standalone_include_nlp_solver() { return 1; }
} // namespace hven_install_smoke
