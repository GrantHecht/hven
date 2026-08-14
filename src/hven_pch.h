#pragma once

// Precompiled header for the hven library target.
//
// WHY THIS EXISTS
//
// Measured on this tree (Linux, clang, -O3, ccache disabled): parsing the
// header set below costs 3.01 s, and that cost is paid once per translation
// unit that includes it. For the engine's largest TU,
// drivers/interior_point_solver.cpp, 3.01 s of its ~7.0 s total is header
// parsing -- only 4.01 s is its own 3722 lines of body. Header parsing, not
// file length, is what the engine's build time is made of: core/pattern_hash.cpp
// is 39 lines and still costs 2.98 s.
//
// Precompiling amortizes that floor across the TUs that pay it. The PCH is
// built once (3.38 s) and then each participating TU drops ~1.5-2.0 s.
//
// This is also why the engine's big TUs are NOT split into smaller ones: a
// split multiplies the 3.01 s floor by the number of pieces instead of
// amortizing it, and (with LINK_TIME_OPT off) turns intra-TU helper calls in
// the interior-point hot loop into un-inlinable cross-TU references. The
// measured comparison behind that decision is summarized in docs/build.md.
//
// WHAT IS IN IT, AND WHY THE ORDER MATTERS
//
// The list below is the include block of drivers/interior_point_solver.cpp,
// verbatim and in its original order. That is deliberate and load-bearing:
// prefixing a TU with a *differently ordered* header set changes the order in
// which templates are instantiated, which changes the order functions are
// emitted into the object file. Keeping this list byte-for-byte in step with
// that TU's own include block is what lets the PCH build produce an object
// that is byte-identical to the non-PCH build -- the property the engine's
// numerics gate relies on. If you edit this list, run
// scripts/check_pch_neutrality.sh and update the participating-TU list in
// src/CMakeLists.txt to match what still qualifies. Both this list and that
// TU's carry a // clang-format off guard, because alphabetizing either one
// silently costs the property.
//
// Membership is opt-in: src/CMakeLists.txt names the TUs that use this PCH and
// opts every other source out, so a TU joins only after it is measured to get
// faster AND to still produce a byte-identical object. See the measured table
// there.

// clang-format off
//
// Include sorting is disabled for this block on purpose. clang-format's
// default is to alphabetize, and alphabetizing here would silently cost the
// byte-identity property described above: the order below is the order
// drivers/interior_point_solver.cpp uses (dependency order, not alphabetical),
// and matching it is what makes the PCH build emit the same object as the
// non-PCH build. Note that clang-format would also reorder that .cpp's own
// include block, so if it is ever reformatted, this list has to be brought
// back into step with it and the byte-identity check re-run.

#include "hven/drivers/interior_point_solver.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "hven/detail/interior/barrier_math.h"
#include "hven/detail/drivers/solver_init.h"
#include "hven/detail/interior/utils/timer.h"

#ifdef USE_ACCELERATE_SPARSE
// The engine's own Accelerate thread control, which the sparse linear surface
// deliberately does not provide -- see that header for the ownership split.
#include "hven/detail/interior/utils/accelerate_threads.h"
#endif

// Globalization component interfaces. Included here (rather than from
// interior_point_solver.h) so the TU that builds InteriorPointSolver exercises
// them on every build without interior_point_solver.h having to include a
// directory of headers that themselves need the complete InteriorPointSolver
// class (a circular-include arrangement that is fragile for the "middle"
// headers below -- see the include-discipline note in solver_context.h).
// Dependency-ordered.
#include "hven/detail/globalization/progress_measures.h"
#include "hven/detail/globalization/solver_context.h"
#include "hven/detail/globalization/acceptance_strategy.h"
#include "hven/detail/globalization/globalization_mechanism.h"
#include "hven/detail/globalization/backtracking_line_search.h"
#include "hven/detail/globalization/barrier_governor.h"
#include "hven/detail/globalization/classic_adaptive_governor.h"
#include "hven/detail/globalization/monitored_governor.h"
#include "hven/detail/globalization/merit_acceptance.h"
#include "hven/detail/globalization/modern_merit.h"
#include "hven/detail/globalization/funnel_acceptance.h"
#include "hven/detail/globalization/filter_acceptance.h"
#include "hven/detail/globalization/inertia_regularization.h"
#include "hven/detail/globalization/recovery_chain.h"
#include "hven/detail/globalization/noop_recovery.h"
#include "hven/detail/globalization/soc.h"
#include "hven/detail/globalization/watchdog.h"
#include "hven/detail/globalization/restoration.h"
#include "hven/detail/globalization/proximal_restoration.h"
#include "hven/detail/globalization/l1_restoration.h"
#include "hven/detail/globalization/feasibility_stall.h"
#include "hven/detail/globalization/feasibility_switch_recovery.h"

// clang-format on
