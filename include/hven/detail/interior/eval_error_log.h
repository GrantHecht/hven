// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <string>

namespace hven::solvers {

/// @brief Latched trial-evaluation exception state for one solve call.
///
/// The line-search / recovery trial evaluations convert an NLP evaluation
/// exception into a rejected-trial signal instead of letting it unwind the
/// solve; this log records how often that happened and keeps the most recent
/// message so the solver can fold it into diagnostics (or into the abort
/// message when no recovery path exists). Reset once per solve call, alongside
/// SolveResult::reset_accumulators().
struct EvalErrorLog {
    int count_ = 0;
    std::string last_message_;

    void record(const char *what) {
        ++count_;
        last_message_ = what;
    }
    void record_unknown() { record("unknown exception type (not derived from std::exception)"); }
    void reset() {
        count_ = 0;
        last_message_.clear();
    }
};

} // namespace hven::solvers
