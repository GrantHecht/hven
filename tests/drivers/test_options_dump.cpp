// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M7 W0 T5(b): THE DRIFT PIN for dump_options().
//
// The dump is hand-written -- hven has no options registry, no name->field map
// and no reflection -- so nothing in the language stops a field being added to
// IpmOptions or SqpOptions without a line in the printer. This TU is what does,
// on three independent axes, because no single one of them catches everything:
//
//   1. DECLARED FIELD COUNT, at compile time. kAggregateArity<T> is the number
//      of initializers T{...} accepts, which for these aggregates is the field
//      count (a nested aggregate counts as ONE -- core/detail/aggregate_arity.h
//      states the rule and its three unsupported shapes). This is the sharp
//      instrument: it moves for an added field and for nothing else.
//   2. sizeof, at compile time. Weaker than (1) -- a field that lands in
//      existing padding does not move it -- but it is the axis that notices a
//      TYPE change (an int widened to a long long) that leaves the count alone.
//      PLATFORM-DEPENDENT BY CONSTRUCTION, so gated to Linux x86-64; on any
//      other target the asserts simply do not run, and (1) and (3) still do.
//   3. DUMPED LINE COUNT, at run time. The one axis that reads the printer
//      itself: (1) and (2) both pass if a field is added AND given a line, and
//      both pass if a field is added and the line is put in twice. This does
//      not.
//
// The count beside each struct below is what that struct carried when this file
// was written; a diff that moves one moves the other in the same commit.

#include <cstddef>
#include <iomanip>
#include <ios>
#include <locale>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include <hven/core/detail/aggregate_arity.h>
#include <hven/drivers/common_options.h>
#include <hven/drivers/ipm_solver_types.h>
#include <hven/drivers/sqp_solver_types.h>
#include <hven/qp/qp_types.h>

namespace {

using hven::CommonOptions;
using hven::IpmOptions;
using hven::IpqpOptions;
using hven::QpOptions;
using hven::SqpOptions;

// ---------------------------------------------------------------------------
// Axis 1: the declared field counts, pinned at compile time.
// ---------------------------------------------------------------------------
// M8 AP-2 B3 (the options break) re-laid both structs into tiers, so the counts
// below are per struct. Each nested struct is flattened under its member name.
// `accelerate_zero_tol` is declared on every platform (ignored on MKL), so the
// counts are the same everywhere.
constexpr std::size_t kIpmLinearOptionsFields = 12;
constexpr std::size_t kIpmOptionsFields = 17;
constexpr std::size_t kIpmTerminationOptionsFields = 10;
constexpr std::size_t kIpmAdvancedOptionsFields = 24;
constexpr std::size_t kSqpOptionsFields = 17;
constexpr std::size_t kSqpLinearOptionsFields = 2;
constexpr std::size_t kSqpAdvancedOptionsFields = 27;
constexpr std::size_t kIpqpOptionsFields = 25;
constexpr std::size_t kQpOptionsFields = 11;
constexpr std::size_t kCommonOptionsFields = 9;

static_assert(::hven::detail::kAggregateArity<IpmOptions> == kIpmOptionsFields,
              "IpmOptions gained or lost a field. Give it a line in "
              "dump_options(const IpmOptions&, std::ostream&) "
              "(src/drivers/ipm_options.cpp), in its declaration position, then move "
              "kIpmOptionsFields and kIpmDumpLines here together.");
static_assert(::hven::detail::kAggregateArity<hven::IpmLinearOptions> == kIpmLinearOptionsFields,
              "IpmLinearOptions gained or lost a field: it is flattened under `linear.`.");
static_assert(::hven::detail::kAggregateArity<hven::IpmTerminationOptions> ==
                  kIpmTerminationOptionsFields,
              "IpmTerminationOptions gained or lost a field: it is flattened under "
              "`termination.`.");
static_assert(::hven::detail::kAggregateArity<hven::IpmAdvancedOptions> ==
                  kIpmAdvancedOptionsFields,
              "IpmAdvancedOptions gained or lost a field: it is flattened under `advanced.`.");
static_assert(::hven::detail::kAggregateArity<SqpOptions> == kSqpOptionsFields,
              "SqpOptions gained or lost a field. Give it a line in "
              "dump_options(const SqpOptions&, std::ostream&) "
              "(src/drivers/sqp_options.cpp), in its declaration position, then move "
              "kSqpOptionsFields and kSqpDumpLines here together.");
static_assert(::hven::detail::kAggregateArity<hven::SqpLinearOptions> == kSqpLinearOptionsFields,
              "SqpLinearOptions gained or lost a field: it is flattened under `linear.`.");
static_assert(::hven::detail::kAggregateArity<hven::SqpAdvancedOptions> ==
                  kSqpAdvancedOptionsFields,
              "SqpAdvancedOptions gained or lost a field: it is flattened under `advanced.`.");
static_assert(::hven::detail::kAggregateArity<IpqpOptions> == kIpqpOptionsFields,
              "IpqpOptions gained or lost a field: it is flattened under the "
              "`advanced.ipqp.` prefix by dump_options(const SqpOptions&, std::ostream&).");
static_assert(::hven::detail::kAggregateArity<QpOptions> == kQpOptionsFields,
              "QpOptions gained or lost a field: it is flattened under the `qp.` "
              "prefix by dump_options(const SqpOptions&, std::ostream&).");
static_assert(::hven::detail::kAggregateArity<CommonOptions> == kCommonOptionsFields,
              "CommonOptions gained or lost a field: BOTH dumps flatten it under the "
              "`common.` prefix, so both printers need the new line.");

// ---------------------------------------------------------------------------
// Axis 2: sizeof, on the one platform these values were observed on.
//
// LINUX x86-64, clang, the project's own flag regime. These numbers are layout,
// not contract: a different ABI, alignment or standard library legitimately
// produces others, which is why the asserts are gated. Apple and Windows values
// are UNOBSERVED and deliberately not guessed. All values re-observed at M8 AP-2
// B3, which re-laid every options struct (the history of earlier values is in
// docs/notes and in git).
// ---------------------------------------------------------------------------
#if defined(__linux__) && defined(__x86_64__)
static_assert(sizeof(IpmOptions) == 480, "IpmOptions' layout moved on Linux x86-64. If a field "
                                         "was added, axis 1 above has already told you so; if "
                                         "only a TYPE changed, this is the only axis that "
                                         "noticed. Check the dump, then re-observe this value.");
static_assert(sizeof(hven::IpmLinearOptions) == 56,
              "IpmLinearOptions' layout moved on Linux x86-64.");
static_assert(sizeof(hven::IpmTerminationOptions) == 88,
              "IpmTerminationOptions' layout moved on Linux x86-64.");
static_assert(sizeof(hven::IpmAdvancedOptions) == 144,
              "IpmAdvancedOptions' layout moved on Linux x86-64.");
static_assert(sizeof(SqpOptions) == 600, "SqpOptions' layout moved on Linux x86-64.");
static_assert(sizeof(hven::SqpLinearOptions) == 24,
              "SqpLinearOptions' layout moved on Linux x86-64.");
static_assert(sizeof(hven::SqpAdvancedOptions) == 328,
              "SqpAdvancedOptions' layout moved on Linux x86-64.");
static_assert(sizeof(IpqpOptions) == 184, "IpqpOptions' layout moved on Linux x86-64.");
static_assert(sizeof(QpOptions) == 88, "QpOptions' layout moved on Linux x86-64.");
static_assert(sizeof(CommonOptions) == 80, "CommonOptions' layout moved on Linux x86-64.");
#endif

// ---------------------------------------------------------------------------
// Axis 3: the dumped line count, read off the printer.
//
// IpmOptions: common 9 + phases 1 + termination 10 + 12 own scalars + linear 12
// + advanced 24 = 68.
// SqpOptions: common 9 + 13 own scalars + qp 11 + linear 2 + advanced 26 own +
// advanced.ipqp 25 = 86.
// ---------------------------------------------------------------------------
constexpr int kIpmDumpLines = 68;
constexpr int kSqpDumpLines = 86;

int count_lines(const std::string &text) {
    int n = 0;
    for (const char c : text) {
        if (c == '\n') {
            ++n;
        }
    }
    return n;
}

std::string dump_to_string(const IpmOptions &o) {
    std::ostringstream os;
    hven::dump_options(o, os);
    return os.str();
}

std::string dump_to_string(const SqpOptions &o) {
    std::ostringstream os;
    hven::dump_options(o, os);
    return os.str();
}

// Whole-line search, so `common.threads = 0` cannot be satisfied by a prefix of
// some other field's line.
bool has_line(const std::string &text, const std::string &line) {
    const std::string needle = line + "\n";
    if (text.compare(0, needle.size(), needle) == 0) {
        return true;
    }
    return text.find("\n" + needle) != std::string::npos;
}

TEST(OptionsDump, IpmDumpsOneLinePerField) {
    EXPECT_EQ(count_lines(dump_to_string(IpmOptions{})), kIpmDumpLines);
}

TEST(OptionsDump, SqpDumpsOneLinePerField) {
    EXPECT_EQ(count_lines(dump_to_string(SqpOptions{})), kSqpDumpLines);
}

// The two fields whose defaults differ between the engines, which is what makes
// them worth reading back: CommonOptions' own defaults are 0 / 3, IpmOptions
// overrides them to HVEN_DEFAULT_IPM_THREADS / 0, and the brief pins the SHARED
// struct's values -- so this is the SqpOptions dump.
TEST(OptionsDump, SqpDumpsTheCommonDefaults) {
    const std::string text = dump_to_string(SqpOptions{});
    EXPECT_TRUE(has_line(text, "common.threads = 0")) << text;
    EXPECT_TRUE(has_line(text, "common.verbosity = kSilent")) << text;
}

// SqpOptions::equality_basis is ON by default (M7 W2 E1b; off from E1 fix round 1 until
// E1b moved the split onto the hven::linear backend -- a DECLARED dump move).
TEST(OptionsDump, SqpDumpsTheEqualityBasisOn) {
    EXPECT_TRUE(has_line(dump_to_string(SqpOptions{}), "advanced.equality_basis = true"));
}

// SqpOptions::project_start_point is ON by default (M7 W2 ladder round, L2 = proposal E5;
// off from M7 W2 M4 until then -- a DECLARED dump move, digest in
// docs/notes/data/2026-09-m7-w2-ladder/README.md). The line count is unchanged.
TEST(OptionsDump, SqpDumpsTheStartPointProjectionOn) {
    EXPECT_TRUE(has_line(dump_to_string(SqpOptions{}), "project_start_point = true"));
}

// An enum prints its ENUMERATOR NAME, never its integer and never the display
// string to_string() spells -- `kWarm`, not `3` and not `Warm`.
TEST(OptionsDump, EnumsPrintEnumeratorNames) {
    const std::string sqp = dump_to_string(SqpOptions{});
    EXPECT_TRUE(has_line(sqp, "qp_mode = kWalk")) << sqp;
    EXPECT_TRUE(has_line(sqp, "advanced.elastic_rung_engine = kWalk")) << sqp;
    EXPECT_TRUE(has_line(sqp, "hessian_mode = kExact")) << sqp;
    EXPECT_TRUE(has_line(sqp, "lbfgs_memory = 10")) << sqp;
    EXPECT_TRUE(has_line(sqp, "advanced.lbfgs_update_on_reject = false")) << sqp;
    EXPECT_TRUE(has_line(sqp, "advanced.lbfgs_nonlinear_only = false")) << sqp;
    EXPECT_TRUE(has_line(sqp, "advanced.max_lbfgs_full_memory_dimension = 400")) << sqp;
    EXPECT_TRUE(has_line(sqp, "common.start_level = kWarm")) << sqp;
    EXPECT_TRUE(has_line(sqp, "qp.working_set_algebra = kSchurBorder")) << sqp;

    const std::string ipm = dump_to_string(IpmOptions{});
    EXPECT_TRUE(has_line(ipm, "advanced.solve_phase_mode = kSolveEquations")) << ipm;
    EXPECT_TRUE(has_line(ipm, "advanced.optimize_barrier_mode = kLoqo")) << ipm;
    EXPECT_TRUE(has_line(ipm, "advanced.restoration_mode = kOff")) << ipm;
    EXPECT_TRUE(has_line(ipm, "linear.dense_factorization = kNever")) << ipm;
    EXPECT_TRUE(has_line(ipm, "common.verbosity = kSilent")) << ipm;
    EXPECT_TRUE(has_line(ipm, "phases = [kOptimize]")) << ipm;
}

// Declaration order, shortest round-trip doubles, optionals as `unset`, bools as
// words: the head of the IPM dump verbatim, which is the cheapest way to pin the
// format itself. RE-DERIVED at M8 AP-2 B3 (declared): the options were re-laid
// into tiers, and doubles moved from `{:.17g}` (`9.9999999999999995e-07`) to the
// shortest round-trip form (`1e-06`).
TEST(OptionsDump, IpmDumpHeadIsVerbatim) {
    const std::string head = "common.optimality_tol = 1e-06\n"
                             "common.feasibility_tol = 1e-06\n"
                             "common.max_iterations = 1000\n"
                             "common.max_factorizations = unset\n"
                             "common.time_limit_seconds = unset\n"
                             "common.verbosity = kSilent\n"
                             "common.threads = " +
                             std::to_string(HVEN_DEFAULT_IPM_THREADS) +
                             "\n"
                             "common.start_level = kWarm\n"
                             "common.infinite_bound = 1e+20\n"
                             "phases = [kOptimize]\n"
                             "termination.complementarity_tol = unset\n"
                             "termination.acceptable_iterations = 50\n";
    const std::string text = dump_to_string(IpmOptions{});
    EXPECT_EQ(text.substr(0, head.size()), head) << text;
}

// The optional limits print their value once set.
TEST(OptionsDump, EngagedLimitsPrintTheirValues) {
    SqpOptions o;
    o.common.max_factorizations = 250;
    o.common.time_limit_seconds = 1.5;
    const std::string text = dump_to_string(o);
    EXPECT_TRUE(has_line(text, "common.max_factorizations = 250")) << text;
    EXPECT_TRUE(has_line(text, "common.time_limit_seconds = 1.5")) << text;
    EXPECT_TRUE(has_line(text, "advanced.ipqp.max_iterations = unset")) << text;
}

// Deterministic and side-effect-free: dumping the same value twice, and
// dumping it after a solve-unrelated copy, produces identical bytes.
TEST(OptionsDump, IsDeterministic) {
    const IpmOptions o;
    EXPECT_EQ(dump_to_string(o), dump_to_string(o));
    const IpmOptions copy = o;
    EXPECT_EQ(dump_to_string(o), dump_to_string(copy));
}

// No trailing whitespace on any line, and every line ends in one.
TEST(OptionsDump, NoTrailingWhitespace) {
    for (const std::string &text : {dump_to_string(IpmOptions{}), dump_to_string(SqpOptions{})}) {
        ASSERT_FALSE(text.empty());
        EXPECT_EQ(text.back(), '\n') << text;
        std::size_t start = 0;
        while (start < text.size()) {
            const std::size_t end = text.find('\n', start);
            ASSERT_NE(end, std::string::npos) << text;
            const std::string line = text.substr(start, end - start);
            EXPECT_FALSE(line.empty()) << text;
            EXPECT_NE(line.back(), ' ') << line;
            EXPECT_NE(line.back(), '\t') << line;
            EXPECT_NE(line.find(" = "), std::string::npos) << line;
            start = end + 1;
        }
    }
}

// ---------------------------------------------------------------------------
// THE POISONED STREAM (M7 W0 T5 fix round 1, sol Important 1).
//
// The declaration promises "the same value dumps the same bytes in any
// process". A printer built out of `os << name << " = " << v` does not keep
// that promise: every one of the settings below rewrites at least one field of
// the output, and a harness that records its options into the same stream it
// writes its own report through is exactly the caller that has them set.
//
// THE LOCALE IS BUILT HERE RATHER THAN NAMED. `std::locale("en_US.UTF-8")` is
// not guaranteed to exist on a build machine -- the C++ standard requires only
// the classic and "C" locales -- so testing against an installed locale would
// pass vacuously wherever it is missing. A `std::numpunct` facet with a
// thousands separator and a grouping rule is portable, is a genuinely
// non-classic locale, and makes the failure it guards against concrete: under
// it, a formatted `500` reads `5,00`.
// ---------------------------------------------------------------------------
class GroupingPunct : public std::numpunct<char> {
  protected:
    char do_thousands_sep() const override { return ','; }
    std::string do_grouping() const override { return "\2"; }
};

// Every knob a caller can leave set on a stream that this dump then writes to.
void poison(std::ostream &os) {
    os.imbue(std::locale(std::locale::classic(), new GroupingPunct));
    os << std::hex << std::showbase << std::showpos << std::uppercase << std::scientific
       << std::boolalpha;
    os << std::setprecision(3);
    os.width(40);
    os.fill('*');
}

TEST(OptionsDump, APoisonedStreamProducesTheSameBytes) {
    // Non-vacuous first: prove the settings DO move ordinary formatted output,
    // so a green assertion below is the dump's doing and not a no-op `poison`.
    {
        std::ostringstream probe;
        poison(probe);
        probe << 500;
        EXPECT_NE(probe.str(), "500") << "the poison did nothing; this test would prove nothing";
    }

    {
        std::ostringstream os;
        poison(os);
        hven::dump_options(IpmOptions{}, os);
        EXPECT_EQ(os.str(), dump_to_string(IpmOptions{}));
        EXPECT_EQ(os.width(), 40) << "an unformatted write does not consume width(); a formatted "
                                     "insertion would have reset it to 0";
    }
    {
        std::ostringstream os;
        poison(os);
        hven::dump_options(SqpOptions{}, os);
        EXPECT_EQ(os.str(), dump_to_string(SqpOptions{}));
        EXPECT_EQ(os.width(), 40);
    }
}

// The dump APPENDS: it does not clear, reposition or reformat what a caller has
// already written, and a second dump follows the first exactly.
TEST(OptionsDump, WritesIntoAStreamTheCallerIsAlreadyUsing) {
    std::ostringstream os;
    os << "prologue\n";
    hven::dump_options(SqpOptions{}, os);
    os << "epilogue\n";
    const std::string expected = "prologue\n" + dump_to_string(SqpOptions{}) + "epilogue\n";
    EXPECT_EQ(os.str(), expected);
}

// A non-default value reaches the dump: the printer reads the value it was
// handed, not a default-constructed one.
TEST(OptionsDump, ReadsTheValueItWasGiven) {
    IpmOptions o;
    o.common.threads = 7;
    o.advanced.restoration_mode = hven::IpmRestorationMode::kL1Nested;
    o.phases = {hven::IpmPhase::kSolve, hven::IpmPhase::kOptimize};
    const std::string text = dump_to_string(o);
    EXPECT_TRUE(has_line(text, "common.threads = 7")) << text;
    EXPECT_TRUE(has_line(text, "advanced.restoration_mode = kL1Nested")) << text;
    EXPECT_TRUE(has_line(text, "phases = [kSolve, kOptimize]")) << text;
    EXPECT_EQ(count_lines(text), kIpmDumpLines);
}

} // namespace
