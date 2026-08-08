#pragma once

// The expected-table format and the comparison engine that reads it.
//
// One committed CSV per trace, at tests/golden_rig/expected/<trace>.csv. A
// table is metadata comment lines, then a fixed header row, then one row per
// (arm, quantity) pair. Counters are exact; float values carry the trace's own
// tolerance; evidence presence and failure states are their own kinds, because
// asserting them as numbers is how a fabricated zero passes for an
// observation.
//
// THE COMPARISON POLICY IS ENFORCED BY THIS READER, not left to each trace to
// remember:
//
//   * A row that carries an observed value must carry its full provenance,
//     INCLUDING the thread-pin mechanism and value. A row without a thread pin
//     is invalid and the reader refuses the file -- float residuals at nine
//     digits are demonstrably run-to-run nondeterministic under multithreaded
//     backends, so an unpinned expectation is not an expectation.
//   * A bitwise/0-ULP kind is REFUSED in a table. Bitwise equality is valid
//     only within one pinned-thread process run, so it belongs inside a trace
//     comparing two of its own observations, never across a committed file and
//     a later run on another machine.
//   * A float row must state a tolerance.
//   * The literal UNOBSERVED is a legal value and means exactly what it says.
//     It never compares equal to anything; it records that nobody has run this
//     arm yet. Every slot for a backend arm this project has no hardware for
//     ships as UNOBSERVED and is filled only from a hardware run.

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "hven/core/types.h"

namespace hven::rig {

// What kind of thing a row asserts, which decides how it is compared.
enum class ValueKind {
    kCounter, // exact integer
    // Compared at the row's stated tolerance, RELATIVE TO THE EXPECTED VALUE
    // BUT FLOORED AT ONE: the test is |observed - expected| <= tolerance *
    // max(1, |expected|), so for an expected value below one the comparison is
    // effectively absolute. That floor is deliberate -- these tables carry
    // solution components and residuals, and a relative test on a quantity
    // that happens to be near zero demands agreement far below what the
    // arithmetic that produced it can offer. Stated here, and in every table's
    // own banner, because "relative" alone would misdescribe it.
    kFloat,
    kState,    // an enumerated state, compared as a name
    kPresence, // "present" / "absent" -- whether an optional evidence field holds a value
    kBool      // "true" / "false"
};

const char *value_kind_name(ValueKind k);
// Throws std::invalid_argument on an unknown name, and specifically on a
// bitwise kind, with the reason.
ValueKind value_kind_from_name(const std::string &name);

// The literal that marks a slot nobody has observed yet.
inline constexpr const char *kUnobserved = "UNOBSERVED";

// One row of an expected table.
struct ExpectedRow {
    std::string arm;      // "<arm name>@<backend>"
    std::string quantity; // e.g. "analyze_count", "x[0]", "inertia_state"
    ValueKind kind = ValueKind::kCounter;
    std::string value; // the expected value, or kUnobserved
    double tolerance = 0.0;

    // Provenance. Required in full on any row whose value is not kUnobserved.
    std::string machine;
    std::string backend; // backend name AND version
    std::string thread_pin_mechanism;
    std::string thread_pin_value;
    std::string commit;
    std::string date;

    bool unobserved() const { return value == kUnobserved; }
};

// A parsed expected table: the trace's metadata plus its rows.
class ExpectedTable {
  public:
    // Reads tests/golden_rig/expected/<trace>.csv. Throws std::runtime_error
    // if the file is missing or malformed, and std::invalid_argument if a row
    // violates the comparison policy above -- a broken table is a broken gate,
    // so it fails loudly rather than degrading into "no expectation".
    static ExpectedTable load(const std::string &trace);

    // True iff a table file exists for this trace.
    static bool exists(const std::string &trace);

    const std::string &trace() const { return trace_; }
    // Metadata read from the leading comment block, keyed by the word before
    // the colon (e.g. "title", "tolerance-policy", "fixture").
    const std::map<std::string, std::string> &metadata() const { return metadata_; }
    const std::vector<ExpectedRow> &rows() const { return rows_; }

    // The row for one (arm, quantity), or nothing if the table has no such row.
    const ExpectedRow *find(const std::string &arm, const std::string &quantity) const;

  private:
    std::string trace_;
    std::map<std::string, std::string> metadata_;
    std::vector<ExpectedRow> rows_;
};

// One thing a trace observed on one arm.
struct Observation {
    std::string trace;
    std::string arm;
    std::string quantity;
    ValueKind kind = ValueKind::kCounter;
    std::string value;
    double tolerance = 0.0;
    // Free-text context the report prints beside the row (which fixture, which
    // configuration was in force). Never compared.
    std::string note;

    static Observation counter(std::string trace, std::string arm, std::string quantity, Index v);
    static Observation real(std::string trace, std::string arm, std::string quantity, double v,
                            double tolerance);
    static Observation state(std::string trace, std::string arm, std::string quantity,
                             std::string s);
    static Observation presence(std::string trace, std::string arm, std::string quantity,
                                bool present);
    static Observation boolean(std::string trace, std::string arm, std::string quantity, bool v);
};

// The result of comparing one observation against a table.
struct Comparison {
    enum class Verdict {
        kMatch,
        kMismatch,
        kNoExpectation, // the table has no row for this (arm, quantity)
        kUnobserved     // the row exists and is an unfilled slot
    };
    Verdict verdict = Verdict::kNoExpectation;
    std::string detail; // human-readable, always populated
};

// Compares one observation against a table under the policy above.
Comparison compare(const Observation &obs, const ExpectedTable *table);

// The comparison engine's rendering of an observation as a CSV row, with the
// provenance columns filled from the live run. This is what the report emits,
// so deriving a table is a copy of the report's own lines rather than a
// transcription.
std::string to_csv_row(const Observation &obs, const std::string &machine,
                       const std::string &backend, const std::string &thread_pin_mechanism,
                       int thread_pin_value, const std::string &commit, const std::string &date);

// The header line every expected table carries, and the one to_csv_row fills.
const char *expected_table_header();

} // namespace hven::rig
