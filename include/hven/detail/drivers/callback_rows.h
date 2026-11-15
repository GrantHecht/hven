// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The row space of an iteration event's multipliers.
//
// The engines carry multipliers split into an equality block and an inequality
// block; an event reports them in the problem's row space, the form of
// SolveResult::lambda. The public handle installs the problem for the duration
// of one solve (CallbackRowsScope); the engine composes into its own scratch
// vector, once per event, and only when the payload asks for vectors.
#pragma once

#include <hven/core/types.h>

namespace hven {
class Problem;
}

namespace hven::detail {

/// @brief [lambda_e; lambda_i] in @p problem's row space; with a null problem
///        (an internal route) the plain concatenation. Written into @p scratch.
void compose_event_lambda(const Problem *problem, const Vec &lambda_e, const Vec &lambda_i,
                          Vec &scratch);

/// @brief [ce; ci] as row values @p g and row violation @p violation in
///        @p problem's row space; with a null problem [ce; ci] and
///        [|ce|; max(ci, 0)]. Both empty when a block is off its row count.
void compose_event_rows(const Problem *problem, const Vec &ce, const Vec &ci, Vec &g,
                        Vec &violation);

/// @brief z split by sign: @p z_lower = max(z, 0), @p z_upper = max(-z, 0);
///        a NaN entry stays NaN on both sides.
void split_bound_multipliers(const Vec &z, Vec &z_lower, Vec &z_upper);

/// @brief Installs @p problem as the row map of @p engine for its scope, and
///        removes it on every exit, a throw included.
template <class Engine> class CallbackRowsScope {
  public:
    CallbackRowsScope(Engine &engine, const Problem &problem) : engine_(engine) {
        engine_.set_callback_rows(&problem);
    }
    ~CallbackRowsScope() { engine_.set_callback_rows(nullptr); }
    CallbackRowsScope(const CallbackRowsScope &) = delete;
    CallbackRowsScope &operator=(const CallbackRowsScope &) = delete;

  private:
    Engine &engine_;
};

} // namespace hven::detail
