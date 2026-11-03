// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why, and which W5 tasks depend
// on it: this directory's CMakeLists.txt.

// SqpWarmStart is OPAQUE (M8 AP-2 B4): the public header holds a shared handle and
// includes nothing from `detail/` (it used to hold a WorkingSet by value). This TU
// is the proof that the glob-installed header stands alone against the INSTALLED
// prefix, the way a consumer sees it.

#include <type_traits>

#include "hven/warmstart/sqp_warm_start.h"

namespace hven_install_smoke {
int standalone_include_sqp_warm_start() {
    // TOUCHED, not merely included: the handle declares no default constructor (so a
    // braced `{}` third argument to solve selects the WarmStart overload), is
    // copyable and movable, and a value taken from an installed solve reads through
    // valid() and x(). An invalid handle cannot be built from the public surface,
    // so the readers are named without being called.
    static_assert(!std::is_default_constructible_v<hven::solvers::SqpWarmStart>);
    static_assert(std::is_copy_constructible_v<hven::solvers::SqpWarmStart>);
    static_assert(std::is_nothrow_move_constructible_v<hven::solvers::SqpWarmStart>);
    bool (hven::solvers::SqpWarmStart::*valid)() const noexcept =
        &hven::solvers::SqpWarmStart::valid;
    const hven::Vec &(hven::solvers::SqpWarmStart::*x)() const noexcept =
        &hven::solvers::SqpWarmStart::x;
    const hven::solvers::IpCrossoverOptions options;
    return (valid != nullptr && x != nullptr && options.dual_tol > 0.0) ? 1 : 0;
}
} // namespace hven_install_smoke
