// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The options table of `CommonOptions` and its walker.
///
/// One row per field, in declaration order (`detail/core/options_registry.h` states the row
/// grammar and the checks). The `doc` strings are the one-line statement the generated options
/// reference prints; the header comments on the struct carry the full contract. A range that
/// differs per engine reads `c.engine`.

#include <string>

#include <hven/detail/core/options_registry.h>
#include <hven/detail/solver/common_option_enum_names.h>
#include <hven/solver/common_options.h>

// clang-format off
#define HVEN_COMMON_OPTIONS_FIELDS(X)                                                              \
    X(optimality_tol, r_gt(0.0),                                                                   \
      "Optimality tolerance: the IPM reads it against its KKT (and, unless "                       \
      "termination.complementarity_tol is set, barrier) measure, the SQP against its "             \
      "stationarity measure and NLP complementarity.")                                             \
    X(feasibility_tol, r_gt(0.0),                                                                  \
      "Feasibility tolerance on the equality and inequality residuals; the SQP also uses it as "   \
      "the bound-activity tolerance of its stationarity measure.")                                 \
    X(max_iterations,                                                                              \
      r_closed(c.engine == OptionEngine::kIpm ? 1.0 : 0.0, 2147483647.0),                          \
      "Iteration cap: per phase of `phases` on the IPM, subproblems solved (restoration "          \
      "included) on the SQP. A SolveBudget can only tighten it.")                                  \
    X(max_factorizations, r_ge(0.0),                                                               \
      "Deterministic work ceiling: KKT factorizations over the whole solve. Reaching it ends "     \
      "the solve with kWorkLimit. Unset is no ceiling.")                                           \
    X(time_limit_seconds, r_gt(0.0),                                                               \
      "Wall-clock limit in seconds from solve entry, read at iteration boundaries (kTimeLimit). "  \
      "Not reproducible. Unset is no limit.")                                                      \
    X(verbosity, r_any(), "Console output; each rung prints what the one below it prints.")        \
    X(threads, r_ge(c.engine == OptionEngine::kIpm ? 1.0 : 0.0),                                   \
      "Backend thread count for this solver's factorizations. 0 leaves the backend's own "         \
      "default alone (the process-wide MKL_NUM_THREADS then governs). Stored and applied to "      \
      "nothing on Apple.")                                                                         \
    X(start_level, r_any(),                                                                        \
      "The ceiling on how much of an offered warm start a solve will trust; never a floor.")       \
    X(infinite_bound, r_gt(0.0, false),                                                            \
      "The magnitude at or beyond which a declared bound is absent. kInfinity means every "        \
      "finite value is a real bound.")                                                             \
    X(return_best, r_any(),                                                                        \
      "Which non-optimal exits hand back the best iterate instead of the last one.")
// clang-format on

namespace hven::detail {

/// @brief Calls `f(const OptionMeta &, T &value)` for every field of a (const) `CommonOptions`.
template <class S, class F> void visit_common(S &o, const OptionCtx &c, F &f) {
    constexpr std::string_view owner = "CommonOptions";
#define X(m, r, d) f(OptionMeta{c.prefix, #m, d, (r), c.advanced, owner}, o.m);
    HVEN_COMMON_OPTIONS_FIELDS(X)
#undef X
}

} // namespace hven::detail
