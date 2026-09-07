// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// sqp_kernels_internal.h — the ONE declaration the M6 W5 T6 cut (d) boundary
// needs, and the reason it is HERE rather than under `include/`.
//
// THIS HEADER IS SOURCE-PRIVATE. It lives under `src/`, it is included by
// exactly two implementation TUs (`sqp_driver.cpp` and `sqp_kernels.cpp`), and
// it is NOT INSTALLED. That last property is the whole point and it is
// structural, not conventional: the install step copies `include/hven/**/*.h`
// WHOLESALE (`CMakeLists.txt:613`), `include/hven/detail/` included, so a
// header placed anywhere under `include/` is a shipped file whatever its
// directory name suggests. A declaration that must not become surface
// therefore cannot live there at all. `scripts/check_install_smoke.sh` proves
// the absence at every landing.
//
// WHAT IT DECLARES, and why the function has external linkage at all.
// `trace_outcome_of` was an anonymous-namespace helper in `sqp_driver.cpp`
// (ownership doc §1.2). It is the ONE symbol used by BOTH sides of cut (d)'s
// boundary: three call sites inside the elastic ladder and the certified
// fallback, which moved to `sqp_kernels.cpp`, and two inside the driver
// (`solve_with_walk`, and `run_major` after an SOC solve). Internal linkage
// cannot span two TUs, so the split forces exactly one of three things — a
// duplicated definition, a private inline definition, or a declared symbol —
// and this boundary takes the third.
//
// ITS EXTERNAL LINKAGE EXISTS SOLELY FOR THIS INTERNAL TU BOUNDARY. It is not
// public API, there is no supported consumer migration, and nothing outside
// these two TUs may name it. The migration guide's T6 entry says exactly that.
//
// AND THE ALTERNATIVE IS RECORDED RATHER THAN FORGOTTEN: a private INLINE
// definition of the same mapping, in this same header, would emit no external
// symbol and would preserve optimisation THROUGH the mapping. It is the first
// targeted REDRAW if this call turns out to cost measurable work — the mapper
// is a two-branch pure function with no standalone symbol in the pre-split
// object at all, which makes it the clearest new call exposure of the whole
// cut. See the ownership doc §1.2 and §11.1.1.

#include <hven/core/solver_status.h>
#include <hven/drivers/trace.h>

namespace hven::solvers::detail {

/// @brief The WALK's own exit, in the trace's alphabet (the map is stated at
/// `QpModeTraceEvent`): the walk has no successor kernel, so every non-optimal
/// exit is an escape rather than a route.
///
/// Defined in `src/drivers/sqp_kernels.cpp`. Called from that TU (the ladder's
/// per-rung line and the fallback's two rung-B lines) and from
/// `src/drivers/sqp_driver.cpp` (`solve_with_walk`'s dispatch line and
/// `run_major`'s SOC line) — five sites, all of them behind a null-sink check
/// on the driver side.
IpqpTraceOutcome trace_outcome_of(QpStatus status);

} // namespace hven::solvers::detail
