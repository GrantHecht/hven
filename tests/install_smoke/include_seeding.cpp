// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why, and which W5 tasks depend
// on it: this directory's CMakeLists.txt.

// W5 T8.5 GATHERED the three warm-start seeding constants into one public
// header -- the interior-point floor and cap, and the SQP's sign band -- with
// their VALUES deliberately NOT unified. A consumer that wants to state a
// tolerance against one of them (a test oracle, a caller building a payload)
// includes this and nothing else, which is what this TU compiles.

#include "hven/warmstart/seeding.h"

namespace hven_install_smoke {
int standalone_include_seeding() {
    // Touched rather than merely included, and touched in the way that would
    // catch the failure this header exists to make impossible: the three are
    // read TOGETHER, and the check asserts they are three DIFFERENT policies
    // -- the floor strictly below the SQP band, the band strictly below the
    // cap -- so a future "tidy-up" that collapses them to one number fails
    // here rather than silently retuning two engines to each other.
    constexpr double floor_value = hven::solvers::kSeededIqMultFloor;
    constexpr double cap = hven::solvers::kSeededMultInitMax;
    constexpr double band = hven::solvers::kSeededDualClampTol;
    static_assert(floor_value < band, "the IPM floor and the SQP band are distinct policies");
    static_assert(band < cap, "the SQP band and the IPM cap are distinct policies");
    return (floor_value > 0.0 && cap > 0.0 && band > 0.0) ? 1 : 0;
}
} // namespace hven_install_smoke
