// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The instrumentation ledger's three reporting functions: a histogram tally
// over the recorded solves and two fmt-formatted tables. FP-arithmetic-free:
// integer counters, enum-to-string lookups and string formatting only; `fmt`
// reads the recorded values and formats them, it computes nothing with them.

#include <hven/core/ledger.h>

#include <string>

#include <fmt/format.h>

namespace hven::solvers {

StartLevelHistogram Ledger::level_histogram() const {
    StartLevelHistogram h;
    for (const SqpSolveRecord &rec : sqp_records_) {
        switch (rec.start_level_used) {
        case StartLevel::kCold:
            ++h.cold;
            break;
        case StartLevel::kSeeded:
            ++h.seeded;
            break;
        case StartLevel::kWarm:
            ++h.warm;
            break;
        case StartLevel::kHot:
            ++h.hot;
            break;
        }
    }
    return h;
}

std::string Ledger::summary_table() const {
    if (records_.empty()) {
        return "";
    }

    std::string result;
    const std::string header = fmt::format("{:<30} {:<10} {:<8} {:<16} {:<14}", "Label", "Warm?",
                                           "Iters", "Factorizations", "Schur Updates");
    result += header + "\n";
    result += std::string(header.size(), '-');
    result += "\n";

    for (const auto &rec : records_) {
        result += fmt::format("{:<30} {:<10} {:<8} {:<16} {:<14}\n", rec.label,
                              rec.warm ? "yes" : "no", rec.counters.minor_iters,
                              rec.counters.factorizations, rec.counters.schur_updates);
    }

    return result;
}

std::string Ledger::sqp_summary_table() const {
    if (sqp_records_.empty()) {
        return "";
    }

    std::string result;
    const std::string header =
        fmt::format("{:<30} {:<16} {:<6} {:<8} {:<12} {:<14}", "Label", "Status", "Level", "Majors",
                    "QP Minors", "Factorizations");
    result += header + "\n";
    result += std::string(header.size(), '-');
    result += "\n";

    for (const auto &rec : sqp_records_) {
        result += fmt::format("{:<30} {:<16} {:<6} {:<8} {:<12} {:<14}\n", rec.label,
                              to_string(rec.status), to_string(rec.start_level_used),
                              rec.counters.major_iters, rec.counters.qp_minor_iters,
                              rec.counters.factorizations);
    }

    return result;
}

} // namespace hven::solvers
