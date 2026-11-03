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

namespace hven::solvers {
class Problem;
}

namespace hven::solvers::detail {

/// @brief [lambda_e; lambda_i] in @p problem's row space; with a null problem
///        (an internal route) the plain concatenation. Written into @p scratch.
void compose_event_lambda(const Problem *problem, const Vec &lambda_e, const Vec &lambda_i,
                          Vec &scratch);

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

} // namespace hven::solvers::detail
