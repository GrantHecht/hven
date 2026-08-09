#pragma once

// Shared scaffolding every trace is written on top of: the per-(trace, arm)
// context that loads the expected table, records observations, applies the
// comparison policy, and feeds the report.
//
// TWO MODES, ONE SET OF TRACE BODIES. In ASSERT mode (the default, and what
// ctest runs) an observation that contradicts a committed expectation fails
// the test. In REPORT mode every observation is recorded and printed and
// nothing fails on a missing expectation -- that is the mode a derivation run
// uses, since the expectations do not exist yet and the report's own lines are
// the raw material for them. Structural self-checks (a contract clause the
// authority states outright, like "the symbolic is analyzed once across this
// whole sequence") are ordinary assertions in the trace body and hold in both
// modes; they are not expectations and do not live in a table.
//
// WHAT A MISSING EXPECTATION IS. Not a pass and not a failure: a recorded
// kNoExpectation, printed in the report's summary. This distinction matters
// while the tables are still headers -- a suite that silently passed with
// nothing to compare against would look exactly like a suite that was gating
// anything.

#include <memory>
#include <string>
#include <vector>

#include "expected_table.h"
#include "hven/core/types.h"
#include "recipes.h"
#include "seam.h"

namespace hven::rig {

// Which mode this process runs in, taken once from the HVEN_RIG_MODE
// environment variable ("assert" -- the default -- or "report").
enum class RunMode { kAssert, kReport };
RunMode run_mode();

// Provenance for this process, as it goes into a report row.
struct RunProvenance {
    std::string machine;
    std::string backend; // backend name AND version
    std::string commit;
    std::string date;
    // "Release", "Debug", ... -- CMAKE_BUILD_TYPE at configure time, stamped
    // in the same way commit is: a compile definition the build system sets,
    // never read from the environment, because unlike the machine name this
    // is a real build-system fact and not something a derivation run needs
    // to override. Compared against ExpectedTable::build_configs() before a
    // float row is asserted -- see expected_table.h's ObservedContext.
    std::string build_config;

    // Provenance for the two TEMPORARY old-seam checkouts this build may
    // consume, one line each: "not configured (...)" when this build has no
    // arm for that seam at all, otherwise the commit (and, for SQP, tag) it
    // was pinned to plus "verified"/"unverified" -- the outcome the CMake
    // configure-time pin check (tests/golden_rig/CMakeLists.txt) actually
    // reached, not merely the commit it claims. "unverified" here does not
    // mean configure failed to notice a problem: a FATAL_ERROR pin mismatch
    // stops the build before this string is ever read, so "unverified"
    // reaching a report means either git was unavailable to check with, or
    // (psiopt only) HVEN_RIG_ALLOW_UNPINNED_PSIOPT_SEAM downgraded a real
    // mismatch to a warning-and-proceed.
    std::string psiopt_seam_provenance;
    std::string sqp_seam_provenance;
};
const RunProvenance &run_provenance();

// Everything the report knows about one observation after comparison.
struct RecordedObservation {
    Observation obs;
    Comparison comparison;
    std::string csv_row; // the row a derived table would carry
};

// The process-wide sink the report target drains. Traces write to it through
// TraceRun; nothing else should touch it.
class ObservationSink {
  public:
    static ObservationSink &instance();

    void add(RecordedObservation r);
    void add_skip(std::string trace, std::string arm, std::string reason);
    void add_note(std::string trace, std::string arm, std::string note);

    struct Skip {
        std::string trace;
        std::string arm;
        std::string reason;
    };
    struct Note {
        std::string trace;
        std::string arm;
        std::string text;
    };

    const std::vector<RecordedObservation> &observations() const { return observations_; }
    const std::vector<Skip> &skips() const { return skips_; }
    const std::vector<Note> &notes() const { return notes_; }

  private:
    std::vector<RecordedObservation> observations_;
    std::vector<Skip> skips_;
    std::vector<Note> notes_;
};

// One trace running on one arm.
class TraceRun {
  public:
    TraceRun(std::string trace, const ArmSpec &arm);

    const std::string &trace() const { return trace_; }
    const ArmSpec &arm() const { return arm_; }
    // The arm's full row key, "<arm>@<backend>".
    const std::string &label() const { return label_; }

    // Builds this arm's seam, once. Every trace goes through here rather than
    // calling make_seam directly, so the arm's configuration and its
    // configuration note reach the report without each trace remembering to
    // forward them.
    SeamUnderTest &seam();
    // An additional, independent engine on the same arm, for the traces that
    // need two.
    std::unique_ptr<SeamUnderTest> another_seam();

    // An engine on the same seam with a DIFFERENT configuration, for the two
    // traces whose subject is a configuration knob (the thread pin, the
    // refinement cap). Every use notes the override, so a report row from such
    // a trace is never read as though it came from the arm's own settings.
    std::unique_ptr<SeamUnderTest> seam_with(const SeamOptions &opts, const std::string &why);

    // Records an observation: compares it against the committed table under
    // the comparison policy, files it with the sink, and -- in assert mode
    // only -- fails the test on a contradiction.
    void record(Observation obs);

    // Convenience wrappers. `quantity` is the column name in the table.
    void record_counters(const Counters &c);
    void record_inertia(const hven::linear::InertiaEvidence &e, const std::string &prefix = "");
    void record_solve_info(const hven::linear::SolveInfo &s, const std::string &prefix = "");
    void record_vector_head(const std::string &quantity, const Vec &v, Index count,
                            double tolerance);

    // Records a quantity that must never become an expectation, stamped with
    // the thread pin of the engine it was actually measured under rather than
    // the run's own. `why` is printed beside it. See ValueKind::kRecordOnly
    // for what makes such a row structurally unassertable rather than merely
    // discouraged.
    void record_only(const std::string &quantity, const std::string &value, const std::string &why,
                     const SeamUnderTest &measured_under);

    // Free text attached to this (trace, arm) in the report.
    void note(std::string text);

    // Marks this run skipped for a missing capability and returns the message
    // to hand to GTEST_SKIP.
    std::string skip(const std::string &what);

    // Default relative tolerance for value comparisons, per the tolerance
    // policy. A trace with a different one states it at the call site.
    static constexpr double kDefaultTolerance = 1e-14;

  private:
    std::string trace_;
    ArmSpec arm_;
    std::string label_;
    // Records the thread pin of the most recently built seam on this run, so
    // an observation's provenance is right even for the traces that build
    // their engines through another_seam()/seam_with() and never touch seam().
    // A row whose thread-pin columns said "unknown" would be refused by this
    // very reader once it were committed, which is exactly why this is tracked
    // rather than read off a member that may still be null.
    void remember_pin(const SeamUnderTest &s);

    std::unique_ptr<ExpectedTable> table_;
    std::unique_ptr<SeamUnderTest> seam_;
    bool configuration_noted_ = false;
    std::string pin_mechanism_ = thread_pin_mechanism_name(ThreadPinMechanism::kAbsent);
    int pin_value_ = 0;
};

// --- helpers the traces share ----------------------------------------------

// ||K x - b||_inf / max(1, ||b||_inf), with K read as the upper triangle of a
// symmetric matrix -- the convention every recipe produces.
double relative_residual(const SpMatRM &K, const Vec &x, const Vec &b);

// Bitwise equality over a whole vector. Used only between two observations of
// ONE process run, which is the only place the comparison policy allows it.
bool bitwise_equal(const Vec &a, const Vec &b);

const char *inertia_state_name(hven::linear::InertiaEvidence::State s);

// Skips the whole run when the arm cannot do what the trace needs. Written as
// a macro because GTEST_SKIP has to expand in the test body itself.
#define RIG_REQUIRE(run, condition, what)                                                          \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            GTEST_SKIP() << (run).skip(what);                                                      \
        }                                                                                          \
    } while (false)

} // namespace hven::rig
