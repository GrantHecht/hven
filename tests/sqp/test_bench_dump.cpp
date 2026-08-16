// tests/test_bench_dump.cpp — PHASE-5 TASK 9. The asserting cover for
// bench/bench_cli.h's two --dump-solution helpers.
//
// WHY THIS FILE EXISTS AT ALL, given that bench/ is deliberately NOT
// ctest-registered (bench/CMakeLists.txt's own note). The bench BINARIES are
// measurement instruments and their runtimes are measured in minutes, so
// registering them would be registering a benchmark as a correctness gate.
// The two helpers below are different: they are the CONTRACT the Task-9
// cross-validation rests on -- one is a T6 error path (a --dump-solution path
// that cannot be opened must throw with the usage text folded in, never
// silently drop the dump), the other is the FILE FORMAT that
// prototypes/psiopt_bridge/run_comparison.py parses. Both are pure functions
// of their arguments, cost microseconds, and would otherwise be covered by
// nothing at all: a format drift would surface as a Python traceback in a
// throwaway prototype rather than as a failing test here.
//
// bench_cli.h is included by RELATIVE PATH, exactly as bench_scale.cpp
// includes tests/sqp/support/*.h by relative path in the other direction. Neither
// header is part of the library surface (include/hven/), and nothing in
// include/ or src/ depends on either.
//
// ---------------------------------------------------------------------
// THE MUTATION RECORD (Task 9's Step-3 discipline: a new asserting test must
// be shown to actually assert). Three mutations, each applied to
// bench/bench_cli.h, each followed by a COMPLETE, UNFILTERED
// `ctest --test-dir build` run in Release, each reverted immediately after and
// the tree reconfirmed at 443/443 green:
//
//   M1  open_output_or_throw's `if (!out) throw_usage(...)` removed (return
//       the failed stream instead) -> kills EXACTLY
//       BenchDump.OpenOutputThrowsWithFlagPathAndUsageFoldedIn (1 of 443).
//   M2  write_solution_dump's body format {:.17g} -> {:.15g} -> kills EXACTLY
//       BenchDump.BodyIsOneComponentPerLineAndRoundTripsExactly (1 of 443).
//       This is the mutation that matters most for the bridge: 15 significant
//       digits still LOOKS right and still parses, it just silently truncates
//       the cross-solver residual at ~1e-16 relative.
//   M3  the `n_flag` and `nx` header values swapped -> kills EXACTLY
//       BenchDump.HeaderCarriesEveryKeyTheBridgeReads (1 of 443). This is the
//       units trap (F7's --n is the NODE count, nx = 5N) made into a mutation.
//
// Each mutation kills one test and only one, which is the intended
// granularity: the three assertions are independent.

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "../../bench/bench_cli.h"

namespace {

using hven::solvers::bench_cli::open_output_or_throw;
using hven::solvers::bench_cli::write_solution_dump;

constexpr const char *kUsage = "usage: hven_sqp_bench --family F3|F7 ... [THE USAGE TEXT]\n";

// A path inside a directory that does not exist, so ofstream cannot open it.
// Built under the system temp directory rather than a literal so the test does
// not depend on '/' being unwritable (it is not, for root).
std::string unopenable_path() {
    return (std::filesystem::temp_directory_path() /
            "hven_sqp_test_bench_dump_no_such_directory_2ad9/out.txt")
        .string();
}

std::string writable_path(const char *stem) {
    return (std::filesystem::temp_directory_path() / stem).string();
}

// Split bench_cli.h's '# key: value' header out of a dump. Deliberately the
// SAME two-step parse run_comparison.py does (strip the leading "# ", split
// once on ": "), so a change that breaks the Python reader breaks this test.
std::vector<std::pair<std::string, std::string>> parse_header(const std::string &text) {
    std::vector<std::pair<std::string, std::string>> out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] != '#') {
            break;
        }
        const std::string body = line.substr(1, std::string::npos);
        const std::size_t sep = body.find(": ");
        if (sep == std::string::npos) {
            continue; // the banner line, "# hven_sqp_bench --dump-solution"
        }
        out.emplace_back(body.substr(1, sep - 1), body.substr(sep + 2));
    }
    return out;
}

std::string header_value(const std::string &text, const std::string &key) {
    for (const auto &kv : parse_header(text)) {
        if (kv.first == key) {
            return kv.second;
        }
    }
    return {};
}

std::vector<double> parse_body(const std::string &text) {
    std::vector<double> out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        out.push_back(std::stod(line));
    }
    return out;
}

} // namespace

// ---------------------------------------------------------------------
// T6: the error path. A --dump-solution path that cannot be opened must throw
// std::invalid_argument naming the flag AND the path AND carrying the usage
// text -- the project's rule that a library/CLI never prints a diagnostic it
// does not also fold into the thrown exception's message.
TEST(BenchDump, OpenOutputThrowsWithFlagPathAndUsageFoldedIn) {
    const std::string path = unopenable_path();
    ASSERT_FALSE(std::filesystem::exists(path));
    try {
        std::ofstream ignored = open_output_or_throw(kUsage, "--dump-solution", path);
        FAIL() << "open_output_or_throw accepted an unopenable path";
    } catch (const std::invalid_argument &e) {
        const std::string msg = e.what();
        EXPECT_NE(msg.find("--dump-solution"), std::string::npos) << msg;
        EXPECT_NE(msg.find(path), std::string::npos) << msg;
        EXPECT_NE(msg.find("could not open"), std::string::npos) << msg;
        EXPECT_NE(msg.find(kUsage), std::string::npos)
            << "the usage text is not folded into the message: " << msg;
    }
}

// The success path, so the throw above is not vacuously satisfied by a helper
// that rejects everything.
TEST(BenchDump, OpenOutputReturnsAWritableStreamOnAGoodPath) {
    const std::string path = writable_path("hven_sqp_test_bench_dump_ok.txt");
    std::filesystem::remove(path);
    {
        std::ofstream out = open_output_or_throw(kUsage, "--dump-solution", path);
        out << "hello\n";
    }
    EXPECT_TRUE(std::filesystem::exists(path));
    std::filesystem::remove(path);
}

// ---------------------------------------------------------------------
// THE FORMAT CONTRACT with prototypes/psiopt_bridge/run_comparison.py: a
// '# key: value' header, then one x component per line and nothing else.
TEST(BenchDump, HeaderCarriesEveryKeyTheBridgeReads) {
    const std::vector<double> x{1.0, -2.5, 0.0};
    std::ostringstream os;
    write_solution_dump(os, "F7", 12345, "cold", 0.68, "Optimal", -0.5, x.data(), x.size());
    const std::string text = os.str();

    EXPECT_EQ(header_value(text, "family"), "F7");
    EXPECT_EQ(header_value(text, "arm"), "cold");
    EXPECT_EQ(header_value(text, "status"), "Optimal");
    // THE UNITS TRAP, pinned: n_flag is --n as the caller gave it (for F7 the
    // NODE count), nx is the VARIABLE count. They are different numbers and
    // the dump must not conflate them.
    EXPECT_EQ(header_value(text, "n_flag"), "12345");
    EXPECT_EQ(header_value(text, "nx"), "3");
    EXPECT_DOUBLE_EQ(std::stod(header_value(text, "p")), 0.68);
    EXPECT_DOUBLE_EQ(std::stod(header_value(text, "f")), -0.5);
}

TEST(BenchDump, BodyIsOneComponentPerLineAndRoundTripsExactly) {
    // Values chosen so a 15- or 16-digit print would lose bits: the last two
    // are consecutive binary64 neighbours of 0.1 and of 1/3.
    std::vector<double> x{0.1,       std::nextafter(0.1, 1.0),
                          1.0 / 3.0, std::nextafter(1.0 / 3.0, 1.0),
                          -1.0e-300, 4.5e300};
    std::ostringstream os;
    write_solution_dump(os, "F7", 7, "warm", 0.9, "Optimal", 1.0, x.data(), x.size());

    const std::vector<double> got = parse_body(os.str());
    ASSERT_EQ(got.size(), x.size());
    for (std::size_t i = 0; i < x.size(); ++i) {
        // Bit-exact, not approximate: {:.17g} round-trips binary64, and the
        // cross-solver residual the bridge computes must be limited by the
        // SOLVERS, not by this file's precision.
        EXPECT_EQ(got[i], x[i]) << "component " << i;
    }
}
