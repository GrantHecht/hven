// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why, and which W5 tasks depend
// on it: this directory's CMakeLists.txt.

// W5 T8.4 MOVED the shared status vocabulary down a tier, out of drivers/ and
// into core/, so that core/ledger.h could keep reporting a status without a
// core header depending upward on drivers/. The drivers/ header that used to
// own it is smoke-tested beside this one, but it now merely re-exports what
// lives here -- so a consumer that wants the vocabulary ALONE (the ledger's own
// position) includes this, and it is this that has to stand up on its own.
// Added in T8.4 fix1 (astra Minor 8).

#include "hven/core/solver_status.h"

namespace hven_install_smoke {
int standalone_include_core_solver_status() {
    // Touched rather than merely included: the two enums, the two spellings and
    // the severity fold are the surface a consumer reaches for, and a header
    // that declared them without defining what they need would compile above
    // and fail here.
    const hven::solvers::SolveStatus s = hven::solvers::SolveStatus::kOptimal;
    const hven::solvers::IpmStopReason r = hven::solvers::IpmStopReason::kIterationCap;
    return (hven::solvers::to_string(s) != nullptr && hven::solvers::to_string(r) != nullptr &&
            hven::solvers::severity(s) >= 0)
               ? 1
               : 0;
}
} // namespace hven_install_smoke
