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

using hven::solvers::CommonOptions;
using hven::solvers::IpmOptions;
using hven::solvers::IpqpOptions;
using hven::solvers::QpOptions;
using hven::solvers::SqpOptions;

// ---------------------------------------------------------------------------
// Axis 1: the declared field counts, pinned at compile time.
//
// IpmOptions' 63 on Linux/Windows are 61 scalars/enums, plus `common` (ONE
// initializer, the nested CommonOptions) and `phases` (the one sequence).
// SqpOptions' 33 count `qp`, `ipqp` and `common` as one each (32 before M7 W2 R2
// added `qp_failure_step`).
//
// IPMOPTIONS' FIELD COUNT IS PLATFORM-DEPENDENT and this is not a rounding
// detail: `accel_pivot_tolerance` and `accel_zero_tolerance` are declared
// inside `#ifdef USE_ACCELERATE_SPARSE` (drivers/ipm_solver_types.h), so the
// struct HAS TWO MORE FIELDS on an Accelerate build than on an MKL one, and the
// dump prints two more lines there under the same guard. Both counts are stated
// here rather than one being asserted everywhere, because either single value
// would be false on the other platform.
// ---------------------------------------------------------------------------
#ifdef USE_ACCELERATE_SPARSE
constexpr std::size_t kIpmOptionsFields = 65;
#else
constexpr std::size_t kIpmOptionsFields = 63;
#endif
constexpr std::size_t kSqpOptionsFields = 33;
constexpr std::size_t kIpqpOptionsFields = 22;
constexpr std::size_t kQpOptionsFields = 10;
constexpr std::size_t kCommonOptionsFields = 3;

static_assert(::hven::detail::kAggregateArity<IpmOptions> == kIpmOptionsFields,
              "IpmOptions gained or lost a field. Give it a line in "
              "dump_options(const IpmOptions&, std::ostream&) "
              "(src/drivers/ipm_options.cpp), in its declaration position, then move "
              "kIpmOptionsFields and kIpmDumpLines here together.");
static_assert(::hven::detail::kAggregateArity<SqpOptions> == kSqpOptionsFields,
              "SqpOptions gained or lost a field. Give it a line in "
              "dump_options(const SqpOptions&, std::ostream&) "
              "(src/drivers/sqp_options.cpp), in its declaration position, then move "
              "kSqpOptionsFields and kSqpDumpLines here together.");
static_assert(::hven::detail::kAggregateArity<IpqpOptions> == kIpqpOptionsFields,
              "IpqpOptions gained or lost a field: it is flattened under the `ipqp.` "
              "prefix by dump_options(const SqpOptions&, std::ostream&).");
static_assert(::hven::detail::kAggregateArity<QpOptions> == kQpOptionsFields,
              "QpOptions gained or lost a field: it is flattened under the `qp.` "
              "prefix by dump_options(const SqpOptions&, std::ostream&).");
static_assert(::hven::detail::kAggregateArity<CommonOptions> == kCommonOptionsFields,
              "CommonOptions gained or lost a field: BOTH dumps flatten it under the "
              "`common.` prefix, so both printers need the new line.");

// ---------------------------------------------------------------------------
// Axis 2: sizeof, on the one platform these values were observed on.
//
// LINUX x86-64, clang, the project's own flag regime -- the configuration
// CLAUDE.md documents and all of hven's pins are taken under. These numbers are
// layout, not contract: a different ABI, a different alignment or a different
// standard-library std::function/std::vector will legitimately produce others,
// which is why the asserts are gated rather than written as a portable claim.
// Apple and Windows values are UNOBSERVED and are deliberately not guessed.
// ---------------------------------------------------------------------------
#if defined(__linux__) && defined(__x86_64__)
static_assert(sizeof(IpmOptions) == 376, "IpmOptions' layout moved on Linux x86-64. If a field "
                                         "was added, axis 1 above has already told you so; if "
                                         "only a TYPE changed, this is the only axis that "
                                         "noticed. Check the dump, then re-observe this value.");
static_assert(sizeof(SqpOptions) == 424,
              "SqpOptions' layout moved on Linux x86-64 -- see the IpmOptions note directly "
              "above. 376 -> 384 at M7 W0 T3, which added the `elastic_rung_engine` enum; "
              "UNCHANGED at M7 W0 T4, whose `ipqp_reseed_after_disproval` bool fits the "
              "padding that enum left (observed, not assumed -- axis 1 above is what "
              "noticed the new field); UNCHANGED again at M7 W2 M4, whose "
              "`project_start_point` bool sits in the padding ahead of `common` "
              "(observed at that task, on this platform, the same way); 384 -> 400 at M7 W2 "
              "M1, which added the `qp_failure_budget` Index and the `qp_cap_retry` bool: "
              "SIXTEEN bytes, not eight, and the extra eight are an alignment hole rather "
              "than a field -- the Index is 8-aligned, so it cannot sit in the padding "
              "`project_start_point` (offset 368) leaves, and lands at 376; `qp_cap_retry` "
              "then takes 384 and `common` 388-400; 400 -> 416 at M7 W2 M2b, which added the "
              "`restoration_proximity` double: EIGHT bytes of field and EIGHT of alignment "
              "hole, because a double is 8-aligned and cannot sit in the seven bytes "
              "`qp_cap_retry` (offset 384) leaves -- it lands at 392, `common` moves to "
              "400-412, and the struct's own 8-alignment rounds 412 up. Observed here, on "
              "this platform, the way every value on this axis is: the design page's "
              "arithmetic said 408 and the compiler is the authority. UNCHANGED at M7 W2 E1, "
              "whose `equality_basis` bool fits the padding after `restoration_proximity` "
              "(the size observed; the offsets not); 416 -> 424 at M7 W2 E2, whose "
              "QpOptions `independent_working_set` bool grows the embedded QpOptions "
              "72 -> 80 (its 8-alignment; observed).");
static_assert(sizeof(IpqpOptions) == 152, "IpqpOptions' layout moved on Linux x86-64.");
static_assert(sizeof(QpOptions) == 80, "QpOptions' layout moved on Linux x86-64.");
static_assert(sizeof(CommonOptions) == 12, "CommonOptions' layout moved on Linux x86-64.");
#endif

// ---------------------------------------------------------------------------
// Axis 3: the dumped line count, read off the printer.
//
// IpmOptions: 62 own lines (the 63 fields less `common`, which is nested) plus
// CommonOptions' 3 = 65 -- and 67 on an Accelerate build, where the two
// `accel_*` fields exist and the dump prints them.
// SqpOptions: 30 own lines (the 33 fields less `qp`, `ipqp` and `common`) plus
// QpOptions' 10, IpqpOptions' 22 and CommonOptions' 3 = 65. (29 own lines and 64
// before M7 W2 R2 added `qp_failure_step`; 22 own lines and 56
// before M7 W0 T3 added `elastic_rung_engine`; 23 and 57 before M7 W0 T4 added
// `ipqp_reseed_after_disproval`; 24 and 58 before M7 W2 M4 added
// `project_start_point`; 25 and 59 before M7 W2 M1 added `qp_failure_budget`
// and `qp_cap_retry`; 27 and 61 before M7 W2 M2b added `restoration_proximity`;
// 28 and 62 before M7 W2 E1 added `equality_basis`, whose default E1 fix round 1
// turned to false: the line count is unchanged.)
// ---------------------------------------------------------------------------
#ifdef USE_ACCELERATE_SPARSE
constexpr int kIpmDumpLines = 67;
#else
constexpr int kIpmDumpLines = 65;
#endif
constexpr int kSqpDumpLines = 65;

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
    hven::solvers::dump_options(o, os);
    return os.str();
}

std::string dump_to_string(const SqpOptions &o) {
    std::ostringstream os;
    hven::solvers::dump_options(o, os);
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
// overrides them to HVEN_DEFAULT_QP_THREADS / 0, and the brief pins the SHARED
// struct's values -- so this is the SqpOptions dump.
TEST(OptionsDump, SqpDumpsTheCommonDefaults) {
    const std::string text = dump_to_string(SqpOptions{});
    EXPECT_TRUE(has_line(text, "common.threads = 0")) << text;
    EXPECT_TRUE(has_line(text, "common.print_level = 3")) << text;
}

// SqpOptions::equality_basis is ON by default (M7 W2 E1b; off from E1 fix round 1 until
// E1b moved the split onto the hven::linear backend -- a DECLARED dump move).
TEST(OptionsDump, SqpDumpsTheEqualityBasisOn) {
    EXPECT_TRUE(has_line(dump_to_string(SqpOptions{}), "equality_basis = true"));
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
    EXPECT_TRUE(has_line(sqp, "elastic_rung_engine = kWalk")) << sqp;
    EXPECT_TRUE(has_line(sqp, "common.start_level = kWarm")) << sqp;
    EXPECT_TRUE(has_line(sqp, "qp.ws_algebra = kSchurBorder")) << sqp;

    const std::string ipm = dump_to_string(IpmOptions{});
    EXPECT_TRUE(has_line(ipm, "soe_mode = kSoe")) << ipm;
    EXPECT_TRUE(has_line(ipm, "opt_bar_mode = kLoqo")) << ipm;
    EXPECT_TRUE(has_line(ipm, "restoration_mode = off")) << ipm;
    EXPECT_TRUE(has_line(ipm, "phases = [kOptimize]")) << ipm;
}

// Declaration order, doubles at {:.17g}, bools as words: the head of the IPM
// dump verbatim, which is the cheapest way to pin the format itself.
//
// `max_iters = 1000` is a DECLARED re-derivation (M7 W2 D1, 2026-09-18): the
// default was 500 until that event, and this line is one of exactly two places
// in the tree where the 500 -> 1000 half of it is observable -- every bench leg
// pins its own cap, so no committed measurement moved with it. See
// docs/notes/data/2026-09-m7-w2-d1-ipm-default/.
TEST(OptionsDump, IpmDumpHeadIsVerbatim) {
    const std::string text = dump_to_string(IpmOptions{});
    EXPECT_EQ(text.substr(0, std::string("max_iters = 1000\n"
                                         "max_ls_iters = 2\n"
                                         "max_acc_iters = 50\n"
                                         "max_refac = 15\n"
                                         "max_soc = 0\n"
                                         "ls_extended_iters = 0\n"
                                         "watchdog = false\n"
                                         "max_feas_rest = 2\n"
                                         "kkt_tol = 9.9999999999999995e-07\n")
                                 .size()),
              "max_iters = 1000\n"
              "max_ls_iters = 2\n"
              "max_acc_iters = 50\n"
              "max_refac = 15\n"
              "max_soc = 0\n"
              "ls_extended_iters = 0\n"
              "watchdog = false\n"
              "max_feas_rest = 2\n"
              "kkt_tol = 9.9999999999999995e-07\n")
        << text;
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
        hven::solvers::dump_options(IpmOptions{}, os);
        EXPECT_EQ(os.str(), dump_to_string(IpmOptions{}));
        EXPECT_EQ(os.width(), 40) << "an unformatted write does not consume width(); a formatted "
                                     "insertion would have reset it to 0";
    }
    {
        std::ostringstream os;
        poison(os);
        hven::solvers::dump_options(SqpOptions{}, os);
        EXPECT_EQ(os.str(), dump_to_string(SqpOptions{}));
        EXPECT_EQ(os.width(), 40);
    }
}

// The dump APPENDS: it does not clear, reposition or reformat what a caller has
// already written, and a second dump follows the first exactly.
TEST(OptionsDump, WritesIntoAStreamTheCallerIsAlreadyUsing) {
    std::ostringstream os;
    os << "prologue\n";
    hven::solvers::dump_options(SqpOptions{}, os);
    os << "epilogue\n";
    const std::string expected = "prologue\n" + dump_to_string(SqpOptions{}) + "epilogue\n";
    EXPECT_EQ(os.str(), expected);
}

// A non-default value reaches the dump: the printer reads the value it was
// handed, not a default-constructed one.
TEST(OptionsDump, ReadsTheValueItWasGiven) {
    IpmOptions o;
    o.common.threads = 7;
    o.restoration_mode = hven::solvers::RestorationModes::l1_nested;
    o.phases = {hven::solvers::IpmPhase::kSolve, hven::solvers::IpmPhase::kOptimize};
    const std::string text = dump_to_string(o);
    EXPECT_TRUE(has_line(text, "common.threads = 7")) << text;
    EXPECT_TRUE(has_line(text, "restoration_mode = l1_nested")) << text;
    EXPECT_TRUE(has_line(text, "phases = [kSolve, kOptimize]")) << text;
    EXPECT_EQ(count_lines(text), kIpmDumpLines);
}

} // namespace
