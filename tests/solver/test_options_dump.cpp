// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M7 W0 T5(b): THE DRIFT PIN for dump_options(); re-derived at M8 AP-4 G1.
//
// The dump is a walk over the per-struct options tables (detail/*/..._options_fields.h). Nothing in
// the language stops a field being added to IpmOptions or SqpOptions without a row, so the
// instruments are:
//
//   1. THE TABLES vs THE STRUCTS, at compile time, in the library's own TUs
//      (HVEN_CHECK_OPTIONS_TABLE in src/ipm/ipm_options.cpp and src/sqp/sqp_options.cpp): the row
//      count must equal kAggregateArity<T> -- the number of initializers T{...} accepts, which is
//      the field count (a nested aggregate counts as ONE; detail/core/aggregate_arity.h states the
//      rule and its unsupported shapes) -- and the rows must name the members in declaration
//      order. A field added without a row, or a row out of order, does not compile. These replace
//      the ten hand-kept field-count constants this file used to carry.
//   2. sizeof, at compile time. Weaker than (1) -- a field that lands in existing padding does not
//      move it -- but it is the axis that notices a TYPE change (an int widened to a long long)
//      that leaves the count alone. PLATFORM-DEPENDENT BY CONSTRUCTION, so gated to Linux x86-64.
//   3. THE DUMPED BYTES, at run time: the verbatim head of the IPM dump below, and the whole dump
//      of IpmOptions{}, every ipm_preset, SqpOptions{} and one all-fields-perturbed value of each
//      against fixtures captured from the hand-written dump that the tables replaced
//      (tests/solver/fixtures/options_dump/, base 0cecdc6d0). The line count is derived from a
//      walk over the tables, so it follows them and is checked against the dump.
//   4. EVERY FIELD PERTURBED: no line of a perturbed dump equals the same line of the default
//      dump, so a field added without a perturbation in support/options_perturbed.h is named.

#include <cstddef>
#include <fstream>
#include <iomanip>
#include <ios>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/ipm/ipm_options_fields.h>
#include <hven/detail/ipm/ipm_solver_presets.h>
#include <hven/detail/sqp/sqp_options_fields.h>
#include <hven/ipm/ipm_solver_types.h>
#include <hven/qp/qp_types.h>
#include <hven/solver/common_options.h>
#include <hven/sqp/sqp_solver_types.h>

#include "solver/support/options_perturbed.h"

namespace {

using hven::CommonOptions;
using hven::IpmOptions;
using hven::IpqpOptions;
using hven::QpOptions;
using hven::SqpOptions;

// ---------------------------------------------------------------------------
// Axis 2: sizeof, on the one platform these values were observed on.
//
// LINUX x86-64, clang, the project's own flag regime. These numbers are layout,
// not contract: a different ABI, alignment or standard library legitimately
// produces others, which is why the asserts are gated. Apple and Windows values
// are UNOBSERVED and deliberately not guessed. IpmOptions re-observed at the M8 AP-5 G-RB rider
// (472, one enum removed); SqpOptions at M8 RF-1 (760, `termination` added). All other values
// re-observed at M8 AP-2 B3, which re-laid every options struct (the history of earlier values is
// in docs/notes and in git).
// ---------------------------------------------------------------------------
#if defined(__linux__) && defined(__x86_64__)
static_assert(sizeof(IpmOptions) == 472, "IpmOptions' layout moved on Linux x86-64. If a field "
                                         "was added, axis 1 above has already told you so; if "
                                         "only a TYPE changed, this is the only axis that "
                                         "noticed. Check the dump, then re-observe this value.");
static_assert(sizeof(hven::IpmLinearOptions) == 56,
              "IpmLinearOptions' layout moved on Linux x86-64.");
static_assert(sizeof(hven::IpmTerminationOptions) == 88,
              "IpmTerminationOptions' layout moved on Linux x86-64.");
static_assert(sizeof(hven::IpmAdvancedOptions) == 144,
              "IpmAdvancedOptions' layout moved on Linux x86-64.");
static_assert(sizeof(SqpOptions) == 776, "SqpOptions' layout moved on Linux x86-64.");
static_assert(sizeof(hven::SqpTerminationOptions) == 32,
              "SqpTerminationOptions' layout moved on Linux x86-64.");
static_assert(sizeof(hven::SqpLinearOptions) == 24,
              "SqpLinearOptions' layout moved on Linux x86-64.");
static_assert(sizeof(hven::SqpAdvancedOptions) == 448,
              "SqpAdvancedOptions' layout moved on Linux x86-64.");
static_assert(sizeof(IpqpOptions) == 184, "IpqpOptions' layout moved on Linux x86-64.");
static_assert(sizeof(QpOptions) == 88, "QpOptions' layout moved on Linux x86-64.");
static_assert(sizeof(CommonOptions) == 88, "CommonOptions' layout moved on Linux x86-64.");
#endif

// ---------------------------------------------------------------------------
// Axis 3: the dumped line count, derived from the registry, checked against the printer.
// ---------------------------------------------------------------------------
// The number of rows the tables hold for a struct: one walk, one tick per field.
template <class Options> std::size_t table_rows() {
    Options o;
    std::size_t rows = 0;
    hven::detail::for_each_option(o, [&rows](const auto &, const auto &) { ++rows; });
    return rows;
}

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
    EXPECT_EQ(static_cast<std::size_t>(count_lines(dump_to_string(IpmOptions{}))),
              table_rows<IpmOptions>());
}

TEST(OptionsDump, SqpDumpsOneLinePerField) {
    EXPECT_EQ(static_cast<std::size_t>(count_lines(dump_to_string(SqpOptions{}))),
              table_rows<SqpOptions>());
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

TEST(OptionsDump, SqpDumpsTheTu0LeversAtTheirOldConstants) {
    const std::string text = dump_to_string(SqpOptions{});
    for (const char *line :
         {"row_scaling_power_of_two = false", "advanced.trust_region_growth = kRatio",
          "advanced.ssn_farkas_residual_tol = 1e-06", "advanced.ssn_uncertain_tol = 0.1",
          "advanced.ssn_lm_sigma_c = 1", "advanced.ssn_lm_sigma_exponent = 2",
          "advanced.restoration_elastic_seed = true", "advanced.elastic_rho0_headroom_rungs = 1",
          "advanced.adaptive_dual_regularization_max = 1e-08"}) {
        EXPECT_TRUE(has_line(text, line)) << line << "\n" << text;
    }
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
    // M8 DN-3 flip (declared): the default moved from kNever to kAuto.
    EXPECT_TRUE(has_line(ipm, "linear.dense_factorization = kAuto")) << ipm;
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
                             "common.return_best = kNever\n"
                             "phases = [kOptimize]\n"
                             "termination.complementarity_tol = unset\n"
                             "termination.acceptable_iterations = 50\n";
    const std::string text = dump_to_string(IpmOptions{});
    EXPECT_EQ(text.substr(0, head.size()), head) << text;
}

// M8 AP-5 G7 (Q7): the four termination tolerances print under their renamed keys
// (the field count, the layout and the line count above are unchanged by a rename).
TEST(OptionsDump, TheTerminationTolerancesPrintUnderTheStationarityAndComplementarityNames) {
    const std::string text = dump_to_string(IpmOptions{});
    for (const char *line : {"termination.acceptable_stationarity_tol = 0.01\n",
                             "termination.acceptable_complementarity_tol = 0.001\n",
                             "termination.divergence_stationarity_tol = 1000000000000000\n",
                             "termination.divergence_complementarity_tol = 1000000000000000\n"}) {
        EXPECT_NE(text.find(line), std::string::npos) << line << text;
    }
    for (const char *old : {"acceptable_kkt_tol", "acceptable_barrier_tol", "divergence_kkt_tol",
                            "divergence_barrier_tol"}) {
        EXPECT_EQ(text.find(old), std::string::npos) << old;
    }
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
    EXPECT_EQ(static_cast<std::size_t>(count_lines(text)), table_rows<IpmOptions>());
}

// ---------------------------------------------------------------------------
// THE REGISTRY vs THE HAND-WRITTEN DUMP IT REPLACED (M8 AP-4 G1).
//
// tests/solver/fixtures/options_dump/ holds the bytes the hand-written printer produced at base
// 0cecdc6d0, before it was re-implemented over the options tables: IpmOptions{}, each ipm_preset,
// SqpOptions{}, and one all-fields-perturbed value of each (support/options_perturbed.h). The
// registry's dump must be byte-identical. The one substitution: the IPM default thread count is
// the build's HVEN_DEFAULT_IPM_THREADS, which the fixtures carry as captured (8); a build that
// overrides it compares against that line rewritten.
// ---------------------------------------------------------------------------
std::string read_fixture(const std::string &name) {
    const std::string path = std::string(HVEN_OPTIONS_FIXTURE_DIR) + "/" + name;
    std::ifstream in(path, std::ios::binary);
    EXPECT_TRUE(in.good()) << "missing fixture " << path;
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

// The captured fixtures carry `common.threads = 8`; the IPM defaults carry the build's value.
std::string with_build_threads(std::string text) {
    const std::string captured = "common.threads = 8\n";
    const std::string build = "common.threads = " + std::to_string(HVEN_DEFAULT_IPM_THREADS) + "\n";
    const std::size_t at = text.find(captured);
    if (at != std::string::npos) {
        text.replace(at, captured.size(), build);
    }
    return text;
}

TEST(OptionsDumpFixtures, IpmDefaultsMatchTheHandWrittenDump) {
    EXPECT_EQ(dump_to_string(IpmOptions{}), with_build_threads(read_fixture("ipm_default.txt")));
}

TEST(OptionsDumpFixtures, EveryIpmPresetMatchesTheHandWrittenDump) {
    for (const auto &entry : hven::detail::kInteriorPointSolverPresets) {
        const std::string name(entry.name_);
        EXPECT_EQ(dump_to_string(hven::ipm_preset(name)),
                  with_build_threads(read_fixture("ipm_preset_" + name + ".txt")))
            << name;
    }
}

TEST(OptionsDumpFixtures, TheIpmAllFieldsPerturbedValueMatchesTheHandWrittenDump) {
    EXPECT_EQ(dump_to_string(hven::test::ipm_all_perturbed()), read_fixture("ipm_perturbed.txt"));
}

TEST(OptionsDumpFixtures, SqpDefaultsMatchTheHandWrittenDump) {
    EXPECT_EQ(dump_to_string(SqpOptions{}), read_fixture("sqp_default.txt"));
}

TEST(OptionsDumpFixtures, TheSqpAllFieldsPerturbedValueMatchesTheHandWrittenDump) {
    EXPECT_EQ(dump_to_string(hven::test::sqp_all_perturbed()), read_fixture("sqp_perturbed.txt"));
}

std::vector<std::string> split_lines(const std::string &text) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t end = text.find('\n', start);
        out.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return out;
}

// A field added to a struct without a perturbation in support/options_perturbed.h leaves its line
// equal to the default's, and is named here.
TEST(OptionsDumpFixtures, NoLineOfAPerturbedDumpEqualsTheDefaultsLine) {
    const auto check = [](const std::string &perturbed, const std::string &defaults) {
        const std::vector<std::string> p = split_lines(perturbed);
        const std::vector<std::string> d = split_lines(defaults);
        ASSERT_EQ(p.size(), d.size());
        for (std::size_t i = 0; i < p.size(); ++i) {
            EXPECT_NE(p[i], d[i]) << "field not perturbed: " << d[i];
        }
    };
    check(dump_to_string(hven::test::ipm_all_perturbed()), dump_to_string(IpmOptions{}));
    check(dump_to_string(hven::test::sqp_all_perturbed()), dump_to_string(SqpOptions{}));
}

} // namespace
