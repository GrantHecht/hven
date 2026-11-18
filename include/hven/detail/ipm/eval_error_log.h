// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <string>

namespace hven::detail {

/// @brief Latched trial-evaluation failure state for one solve call.
///
/// The line-search / recovery trial evaluations convert a thrown EvaluationError,
/// or a non-finite value, into a rejected-trial signal instead of letting it end
/// the solve; this log records how often that happened and keeps the most recent
/// message, which the solve reports as SolveResult::evaluation_errors and folds
/// into the stop message when no recovery path exists. Reset once per solve call.
struct EvalErrorLog {
    int count_ = 0;
    std::string last_message_;

    void record(const char *what) {
        ++count_;
        last_message_ = what;
    }
    void record(const std::string &what) {
        ++count_;
        last_message_ = what;
    }
    void reset() {
        count_ = 0;
        last_message_.clear();
    }
};

} // namespace hven::detail
