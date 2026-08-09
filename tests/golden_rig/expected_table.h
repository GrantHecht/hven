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
    kBool,     // "true" / "false"

    // RECORDED AND PRINTED, BUT UNASSERTABLE BY CONSTRUCTION. A quantity whose
    // value is real and worth having in the artifact, but which must never
    // become an expectation -- and which therefore must not merely be
    // discouraged from becoming one. The reader REFUSES this kind in a
    // committed table, the same way it refuses a bitwise kind, so a row copied
    // out of a report and pasted into a CSV fails to load with the reason
    // attached rather than quietly becoming a gate.
    //
    // The case it exists for: a measurement taken under a DIFFERENT
    // configuration from the one the row's provenance columns describe. The
    // cross-thread deviation is the example -- it is measured between a
    // multithreaded leg and a pinned one, so no single thread-pin stamp
    // describes it, and the pin-refusal that guards every other float row
    // cannot catch it because the row would look perfectly pinned. Recording
    // the true mechanism and value alongside is necessary but not sufficient;
    // the kind is what makes it structurally safe.
    kRecordOnly
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

    // The build configurations (e.g. "Release", "Debug") this table's
    // committed float rows are pinned to, parsed from the table's own
    // "build-config" metadata line. A row's build configuration is not a
    // per-row column -- every row in one committed table comes from one
    // derivation run, so the pin is table-wide -- and every entry here was
    // independently reproduced under it (see docs/testing.md's "What a row
    // has to survive before it is committed"). load() REFUSES a table that
    // carries a non-UNOBSERVED float row but no "build-config" line, the
    // same way it refuses a row without a thread pin: without it, a float
    // row's context could never be soundly checked.
    const std::vector<std::string> &build_configs() const { return build_configs_; }

    // The row for one (arm, quantity), or nothing if the table has no such row.
    const ExpectedRow *find(const std::string &arm, const std::string &quantity) const;

  private:
    std::string trace_;
    std::map<std::string, std::string> metadata_;
    std::vector<ExpectedRow> rows_;
    std::vector<std::string> build_configs_;
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

    // A recorded-but-unassertable quantity. `why` is printed with it in the
    // report, because a reader meeting an unassertable row is owed the reason
    // on the spot.
    static Observation record_only(std::string trace, std::string arm, std::string quantity,
                                   std::string value, std::string why);

    // The thread pin this particular observation was taken under, when that
    // differs from the run's own. Set only by record_only() observations
    // measured under another configuration; empty otherwise, and the run's pin
    // is used.
    std::optional<std::string> pin_mechanism_override;
    std::optional<int> pin_value_override;
};

// The result of comparing one observation against a table.
struct Comparison {
    enum class Verdict {
        kMatch,
        kMismatch,
        kNoExpectation, // the table has no row for this (arm, quantity)
        kUnobserved,    // the row exists and is an unfilled slot

        // The row's PINNED context (machine, build configuration, thread
        // pin) does not match the OBSERVING run's own -- see kFloat's own
        // doc comment for why only a float row can land here. The row is
        // NOT asserted: neither a match nor a contradiction, because the
        // comparison it would take to decide that is not sound across a
        // context the row's expectation was never shown to hold under.
        // Reported loudly rather than folded into kNoExpectation, because
        // an expectation genuinely exists here -- it is simply not safe to
        // apply on this machine, in this build, at this thread pin.
        kContextMismatch,

        kRecordOnly // the observation is unassertable by construction
    };
    Verdict verdict = Verdict::kNoExpectation;
    std::string detail; // human-readable, always populated
};

// The OBSERVING run's own context, checked against a float row's pinned
// context (ExpectedRow::machine, ExpectedTable::build_configs(),
// ExpectedRow::thread_pin_mechanism / thread_pin_value) before that row is
// asserted. Built by the caller from RunProvenance plus the thread pin the
// engine under test actually reports -- expected_table.cpp has no way to
// detect either on its own, and taking them as parameters keeps this file
// free of any dependency on how a live run determines them.
struct ObservedContext {
    std::string machine;
    std::string build_config;
    std::string thread_pin_mechanism;
    std::string thread_pin_value;
};

// Compares one observation against a table under the policy above. `ctx` is
// the observing run's own context; consulted only for kFloat rows -- a
// counter, state, presence or bool row is exact and machine-independent by
// construction and is asserted regardless of context, same as always.
Comparison compare(const Observation &obs, const ExpectedTable *table, const ObservedContext &ctx);

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
