// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// Precompiled header for the hven library target.
//
// WHY THIS EXISTS
//
// Parsing the header set below is a large fixed cost paid once per
// translation unit that includes it; precompiling amortizes that floor across
// the TUs that pay it. It is also why the engine's big TUs are NOT split into
// smaller ones: a split multiplies the header-parsing floor by the number of
// pieces instead of amortizing it, and (with HVEN_LINK_TIME_OPT off) turns
// intra-TU helper calls in the interior-point hot loop into un-inlinable
// cross-TU references. The measured comparison behind that decision is
// summarized in docs/build.md.
//
// WHAT IS IN IT, AND WHY THE ORDER MATTERS
//
// The list below is the include block of ipm/ipm_solver.cpp,
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
// Include sorting is disabled for this block on purpose: clang-format's
// default is to alphabetize, which would silently cost the byte-identity
// property described above -- the order below matches
// ipm/ipm_solver.cpp (dependency order, not alphabetical).
// Note that clang-format would also reorder that .cpp's own include block,
// so if it is ever reformatted, this list has to be brought back into step
// with it and the byte-identity check re-run.

#include <hven/detail/ipm/ipm_solver_impl.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <hven/detail/barrier/barrier_math.h>
#include <hven/detail/ipm/solver_init.h>
#include <hven/detail/util/get_core_count.h>
#include <hven/detail/util/timer.h>

#ifdef USE_ACCELERATE_SPARSE
// The engine's own Accelerate thread control, which the sparse linear surface
// deliberately does not provide -- see that header for the ownership split.
#include <hven/detail/ipm/accelerate_threads.h>
#endif

// Globalization component interfaces, dependency-ordered. Included here
// (rather than from ipm_solver.h) so the TU that builds
// IpmSolver exercises them on every build without
// ipm_solver.h having to include a directory of headers that
// themselves need the complete IpmSolver class (a circular-include
// arrangement that is fragile for the "middle" headers below -- see the
// include-discipline note in solver_context.h).
#include <hven/detail/ipm/globalization/progress_measures.h>
#include <hven/detail/ipm/solver_context.h>
#include <hven/detail/ipm/globalization/acceptance_mode.h>
#include <hven/detail/ipm/globalization/globalization_mechanism.h>
#include <hven/detail/ipm/globalization/backtracking_line_search.h>
#include <hven/detail/ipm/barrier_governor.h>
#include <hven/detail/ipm/classic_adaptive_governor.h>
#include <hven/detail/ipm/monitored_governor.h>
#include <hven/detail/ipm/globalization/merit_acceptance.h>
#include <hven/detail/ipm/globalization/modern_merit.h>
#include <hven/detail/ipm/globalization/funnel_acceptance.h>
#include <hven/detail/ipm/globalization/filter_acceptance.h>
#include <hven/detail/kkt/inertia_regularization.h>
#include <hven/detail/ipm/globalization/recovery_chain.h>
#include <hven/detail/ipm/globalization/noop_recovery.h>
#include <hven/detail/ipm/globalization/soc.h>
#include <hven/detail/ipm/globalization/watchdog.h>
#include <hven/detail/ipm/globalization/restoration.h>
#include <hven/detail/ipm/globalization/proximal_restoration.h>
#include <hven/detail/ipm/globalization/l1_restoration.h>
#include <hven/detail/ipm/globalization/feasibility_stall.h>
#include <hven/detail/ipm/globalization/feasibility_switch_recovery.h>

// clang-format on
