// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// AnalysisTally (symmetric_factor.h): the per-thread stack of open tallies and
// the record both platform adapters call after a completed analysis.

#include <unordered_set>

#include <hven/linear/symmetric_factor.h>

namespace hven::linear {

struct AnalysisTally::Seen {
    std::unordered_set<std::uint64_t> keys;
};

namespace {
thread_local AnalysisTally *t_innermost = nullptr;
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

} // namespace hven::linear
