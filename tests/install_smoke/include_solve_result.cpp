// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why, and which W5 tasks depend
// on it: this directory's CMakeLists.txt.

// W5 T8.4 added this header; it is the result core both engines report through
// and must stand alone for a consumer that includes neither engine.
//
// W5 T8.6 added the SHARED PER-ITERATION CALLBACK to it -- CallbackAction,
// IterationEvent and IterationCallback -- rather than a header of its own, so
// this TU is also the install-surface proof for those three. The body below
// names all three, so "it is installed" is a compiled fact and not an
// inference from the glob.

#include "hven/drivers/solve_result.h"

namespace hven_install_smoke {
int standalone_include_solve_result() {
    const hven::solvers::IterationCallback cb = [](const hven::solvers::IterationEvent &e) {
        return e.iteration < 0 ? hven::solvers::CallbackAction::kStop
                               : hven::solvers::CallbackAction::kContinue;
    };
    return cb ? 1 : 0;
}
} // namespace hven_install_smoke
