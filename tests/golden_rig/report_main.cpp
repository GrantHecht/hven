// hven_golden_rig_report -- runs every trace in REPORT mode and dumps the
// observed-vs-expected table.
//
// Same trace bodies as the ctest suite, same seams, same comparison engine;
// the only differences are the mode (a missing expectation is recorded rather
// than being nothing at all) and the output. Two audiences:
//
//   * PIN DERIVATION. The "observations" block emits each observation as a
//     row in exactly the committed tables' CSV format, provenance columns
//     already filled from this run. Deriving a table is a copy of those lines
//     into tests/golden_rig/expected/<trace>.csv, not a transcription -- which
//     is the difference between a provenance banner that is true and one that
//     was typed.
//   * FAILURE FORENSICS. When a trace fails on an OLD seam -- which two of
//     them are designed to -- the block for that arm is what the finding gets
//     written from: the observed value, the expectation it contradicts, and
//     the configuration that was in force.
//
// Writes to stdout, and additionally to the path in HVEN_RIG_REPORT_OUT when
// that is set.

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "seam.h"
#include "trace_support.h"

namespace {

using hven::rig::Comparison;
using hven::rig::ObservationSink;

const char *verdict_name(Comparison::Verdict v) {
    switch (v) {
    case Comparison::Verdict::kMatch:
        return "MATCH";
    case Comparison::Verdict::kMismatch:
        return "MISMATCH";
    case Comparison::Verdict::kNoExpectation:
        return "NO-EXPECTATION";
    case Comparison::Verdict::kUnobserved:
        return "UNOBSERVED-SLOT";
    case Comparison::Verdict::kContextMismatch:
        return "CONTEXT-MISMATCH";
    case Comparison::Verdict::kRecordOnly:
        return "RECORD-ONLY";
    }
    return "?";
}

void write_report(std::ostream &out) {
    const hven::rig::RunProvenance &p = hven::rig::run_provenance();
    const ObservationSink &sink = ObservationSink::instance();

    out << "================================================================\n"
        << "hven golden-numerics rig -- observed vs expected\n"
        << "================================================================\n"
        << "machine : " << p.machine << "\n"
        << "backend : " << p.backend << "\n"
        << "commit  : " << p.commit << "\n"
        << "date    : " << p.date << "\n"
        << "psiopt old-seam checkout : " << p.psiopt_seam_provenance << "\n"
        << "sqp old-seam checkout    : " << p.sqp_seam_provenance << "\n\n";

    out << "arms in this build:\n";
    for (const hven::rig::ArmSpec &a : hven::rig::arms()) {
        out << "  " << a.label() << "\n      " << a.parity_note << "\n";
    }
    out << "\n";

    std::map<Comparison::Verdict, int> tally;
    for (const auto &r : sink.observations()) {
        ++tally[r.comparison.verdict];
    }
    out << "summary: " << sink.observations().size() << " observations, "
        << tally[Comparison::Verdict::kMatch] << " matched, "
        << tally[Comparison::Verdict::kMismatch] << " contradicted, "
        << tally[Comparison::Verdict::kNoExpectation] << " with no expectation, "
        << tally[Comparison::Verdict::kUnobserved] << " against unfilled slots, "
        << tally[Comparison::Verdict::kContextMismatch]
        << " NOT ASSERTED for a context mismatch (machine, build configuration or thread pin --"
           " see the comparisons block below for which), "
        << tally[Comparison::Verdict::kRecordOnly]
        << " recorded as documentation (unassertable by construction); " << sink.skips().size()
        << " runs skipped for a missing capability.\n\n";

    if (!sink.notes().empty()) {
        out << "---------------- notes (fixtures, parity, overrides) ----------------\n";
        for (const auto &n : sink.notes()) {
            out << "[" << n.trace << " / " << n.arm << "] " << n.text << "\n";
        }
        out << "\n";
    }

    if (!sink.skips().empty()) {
        out << "---------------- skipped runs ----------------\n";
        for (const auto &s : sink.skips()) {
            out << "[" << s.trace << " / " << s.arm << "] " << s.reason << "\n";
        }
        out << "\n";
    }

    out << "---------------- comparisons ----------------\n";
    for (const auto &r : sink.observations()) {
        out << verdict_name(r.comparison.verdict) << "  [" << r.obs.trace << "] "
            << r.comparison.detail << "\n";
    }
    out << "\n";

    out << "---------------- observations, as expected-table rows ----------------\n"
        << "# Grouped by trace. Copy a group's rows into\n"
        << "# tests/golden_rig/expected/<trace>.csv beneath that file's header. The\n"
        << "# provenance columns are already filled from this run; do not retype them.\n";

    // Group rather than interleave: a derivation copies one trace's rows at a
    // time, and a per-row comment between them would have to be stripped by
    // hand from every paste.
    std::map<std::string, std::vector<const hven::rig::RecordedObservation *>> by_trace;
    for (const auto &r : sink.observations()) {
        by_trace[r.obs.trace].push_back(&r);
    }
    for (const auto &[trace, rows] : by_trace) {
        bool wrote_header = false;
        for (const auto *r : rows) {
            if (r->obs.kind == hven::rig::ValueKind::kRecordOnly) {
                continue; // emitted separately below -- never paste these
            }
            if (!wrote_header) {
                out << "\n# ==== " << trace << " ====\n"
                    << hven::rig::expected_table_header() << "\n";
                wrote_header = true;
            }
            out << r->csv_row << "\n";
        }
    }

    // Kept OUT of the block above on purpose. These rows are documentation and
    // the reader refuses their kind in a committed table, so pasting one in
    // would break the file at load. Printing them here, plainly labelled and
    // with the configuration each was measured under, gives a derivation the
    // number without giving it a row to copy by reflex.
    bool any_record_only = false;
    for (const auto &r : sink.observations()) {
        if (r.obs.kind != hven::rig::ValueKind::kRecordOnly) {
            continue;
        }
        if (!any_record_only) {
            out << "\n---------------- recorded as documentation (DO NOT PASTE INTO A TABLE) "
                   "----------------\n"
                   "# These are unassertable by construction -- each describes a comparison\n"
                   "# BETWEEN configurations, so no single provenance stamp describes it, and\n"
                   "# the expected-table reader refuses the kind outright.\n";
            any_record_only = true;
        }
        out << "[" << r.obs.trace << " / " << r.obs.arm << "] " << r.obs.quantity << " = "
            << r.obs.value << "   (" << r.obs.note << ")\n";
    }
}

} // namespace

int main(int argc, char **argv) {
    // Set before any trace body runs, so the mode's lazy read sees it.
#if defined(_WIN32)
    _putenv_s("HVEN_RIG_MODE", "report");
#else
    setenv("HVEN_RIG_MODE", "report", /*overwrite=*/1);
#endif

    testing::InitGoogleTest(&argc, argv);
    const int rc = RUN_ALL_TESTS();

    write_report(std::cout);
    if (const char *path = std::getenv("HVEN_RIG_REPORT_OUT")) {
        std::ofstream file(path);
        if (!file) {
            std::cerr << "hven_golden_rig_report: cannot write " << path << "\n";
            return 2;
        }
        write_report(file);
        std::cout << "\nreport also written to " << path << "\n";
    }

    // The report's own exit status reflects the trace bodies' structural
    // self-checks only: a missing expectation is not a failure in this mode,
    // which is the whole reason the mode exists.
    return rc;
}
