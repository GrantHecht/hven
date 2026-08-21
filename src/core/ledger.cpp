// ledger.cpp -- the instrumentation ledger's three REPORTING functions,
// carved out of hven/core/ledger.h.
//
// M3 PHASE-C T2. CLAUDE.md section 5 names ledger code explicitly as code that
// belongs in a .cpp translation unit. These three run once per report -- a
// histogram tally over the recorded solves and two fmt-formatted tables -- so
// nothing about them depends on inlining, and as header `inline` definitions
// they were parsed and code-generated in every TU that included ledger.h,
// which on this library is nearly every SQP TU (sqp_driver.h includes it).
//
// FP-ARITHMETIC-FREE, which is what let this land as an early phase-C split:
// integer counters, enum-to-string lookups and string formatting. `fmt` reads
// the recorded values and formats them; it computes nothing with them.
//
// WHAT DELIBERATELY DID NOT MOVE: `Ledger::record()` and the two accessors.
// record() is one `push_back` per solve and the accessors return a reference;
// out-of-lining them would add a call to the recording path and buy no build
// time worth measuring. They stay inline in the header, as the phase-C plan
// (section 6, T2) directs.

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

    // Build the table with header
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
