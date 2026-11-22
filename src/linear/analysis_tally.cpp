// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// AnalysisTally and SolveTally (symmetric_factor.h): the per-thread stacks of open
// tallies and the records the adapters call after a completed analysis / solve.

#include <unordered_set>

#include <hven/linear/symmetric_factor.h>

namespace hven::linear {

struct AnalysisTally::Seen {
    std::unordered_set<std::uint64_t> keys;
};

namespace {
thread_local AnalysisTally *t_innermost = nullptr;
thread_local SolveTally *t_solve_innermost = nullptr;
thread_local SolveContext t_solve_context = SolveContext::kOrdinary;
} // namespace

AnalysisTally::AnalysisTally() : outer_(t_innermost), seen_(std::make_unique<Seen>()) {
    t_innermost = this;
}

AnalysisTally::~AnalysisTally() {
    // Tallies are scoped objects, so they close in reverse order of opening;
    // restoring the enclosing one keeps the stack exact.
    t_innermost = outer_;
}

void AnalysisTally::record(std::uint64_t key) noexcept {
    for (AnalysisTally *t = t_innermost; t != nullptr; t = t->outer_) {
        ++t->analyses_;
        try {
            if (!t->seen_->keys.insert(key).second) {
                ++t->repeats_;
            }
        } catch (...) {
            // An allocation failure while remembering the key: the analysis
            // stays counted; a later repeat of this key would read as new.
        }
    }
}

SolveContextScope::SolveContextScope(SolveContext ctx) noexcept : prev_(t_solve_context) {
    t_solve_context = ctx;
}

SolveContextScope::~SolveContextScope() { t_solve_context = prev_; }

SolveTally::SolveTally() : outer_(t_solve_innermost) { t_solve_innermost = this; }

SolveTally::~SolveTally() { t_solve_innermost = outer_; }

void SolveTally::record(Index rhs_columns, Index refinement_steps) noexcept {
    const auto c = static_cast<std::size_t>(t_solve_context);
    for (SolveTally *t = t_solve_innermost; t != nullptr; t = t->outer_) {
        ++t->solves_[c];
        t->rhs_columns_[c] += rhs_columns;
        t->refinement_steps_[c] += refinement_steps;
    }
}

} // namespace hven::linear
