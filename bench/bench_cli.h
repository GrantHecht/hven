#pragma once

// bench/bench_cli.h — BENCH-LOCAL ONLY. The two argument-parsing helpers both
// bench binaries need, factored here in Phase-5 Task 3's second fix round
// because bench_scale.cpp and bench_f7_cold.cpp had grown byte-identical
// copies of them.
//
// THIS IS NOT PART OF THE LIBRARY SURFACE and is deliberately not under
// include/hven/: nothing in include/ or src/ may depend on it, it is
// never installed, and it exists only so two throwaway measurement binaries
// share one implementation of "parse this argument or throw with the usage
// text attached". It follows the same precedent as
// tests/sqp/support/*.h — support code that lives next to its only consumers.
//
// T6 (the project's error rule) is what the helpers exist to get right, and
// they get it right in ONE place: every rejection throws std::invalid_argument
// with the caller's own usage text folded into the exception message, and
// nothing here prints a diagnostic it does not also fold into the throw. Each
// binary keeps its OWN usage text and passes it in, so the two programs' help
// output stays independent while the throwing behaviour cannot drift.

#include <cstddef>
#include <fstream>
#include <ostream>
#include <stdexcept>
#include <string>

#include <fmt/format.h>

namespace hven::solvers::bench_cli {

// Throw std::invalid_argument carrying `detail` followed by `usage`. The
// [[noreturn]] is load-bearing at both call sites: it is what lets the parse
// helpers below end with a throw_usage() call in their catch block without the
// compiler demanding an unreachable return.
[[noreturn]] inline void throw_usage(const char *usage, const std::string &detail) {
    throw std::invalid_argument(fmt::format("{}\n\n{}", detail, usage));
}

// `what` names the thing being parsed in the message -- a flag ("--n") for
// bench_scale.cpp, a positional argument's name ("nodes") for
// bench_f7_cold.cpp -- so one message format serves both shapes.
//
// A TRAILING-CHARACTER CHECK IS PART OF THE CONTRACT, not an extra: std::stoll
// happily parses "12abc" as 12, and a benchmark that silently ran at a size
// the caller did not ask for would corrupt exactly the kind of measurement
// these binaries exist to produce.
inline long long parse_ll(const char *usage, const std::string &what, const std::string &value) {
    try {
        std::size_t pos = 0;
        const long long parsed = std::stoll(value, &pos);
        if (pos != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return parsed;
    } catch (const std::exception &) {
        throw_usage(usage, fmt::format("{}: '{}' is not an integer", what, value));
    }
}

inline double parse_double(const char *usage, const std::string &what, const std::string &value) {
    try {
        std::size_t pos = 0;
        const double parsed = std::stod(value, &pos);
        if (pos != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return parsed;
    } catch (const std::exception &) {
        throw_usage(usage, fmt::format("{}: '{}' is not a number", what, value));
    }
}

// ---------------------------------------------------------------------
// PHASE-5 TASK 9 (the IPM bridge). The two helpers behind
// bench_scale.cpp's --dump-solution flag, here rather than there for the same
// reason the parse helpers are: they are the part a TEST can reach
// (tests/test_bench_dump.cpp), since bench_scale.cpp is a main() with no
// library surface. Nothing about the CSV schema is touched by either.

// Open `path` for writing or throw with the caller's usage text folded in
// (T6). Used for --dump-solution; --csv/--qp-csv keep their own pre-existing
// throw (no usage text), which is deliberately NOT changed here -- their
// message shape is what Tasks 2-8's scripts already match on.
inline std::ofstream open_output_or_throw(const char *usage, const std::string &flag,
                                          const std::string &path) {
    std::ofstream out(path);
    if (!out) {
        throw_usage(usage, fmt::format("{}: could not open '{}' for writing", flag, path));
    }
    return out;
}

// THE SOLUTION-DUMP FORMAT, and it is a cross-language contract: the reader is
// prototypes/psiopt_bridge/run_comparison.py, which parses the `# key: value`
// header with a split on ": " and reads the body with numpy.loadtxt(comments =
// '#'). Hence: every metadata line is a COMMENT, the body is one value per
// line and nothing else, and values are printed at {:.17g} (round-trip exact
// for binary64) so a cross-solver residual is not limited by this file.
//
// THE UNITS TRAP IS WRITTEN INTO THE HEADER ON PURPOSE. `n_flag` is --n as the
// caller gave it, which for F7 is the NODE COUNT N, while `nx` is the variable
// count (F7: nx = N*(ns+nc) = 5N at the bench's ns=3, nc=2). Both are emitted,
// each labelled, because conflating them burned a Phase-5 review.
inline void write_solution_dump(std::ostream &os, const std::string &family, long long n_flag,
                                const std::string &arm, double p, const std::string &status,
                                double f, const double *x, std::size_t nx) {
    os << "# hven_sqp_bench --dump-solution\n";
    os << fmt::format("# family: {}\n", family);
    os << fmt::format("# arm: {}\n", arm);
    os << fmt::format("# n_flag: {}\n", n_flag);
    os << fmt::format("# nx: {}\n", nx);
    os << fmt::format("# p: {:.17g}\n", p);
    os << fmt::format("# status: {}\n", status);
    os << fmt::format("# f: {:.17g}\n", f);
    os << "# columns: x\n";
    for (std::size_t i = 0; i < nx; ++i) {
        os << fmt::format("{:.17g}\n", x[i]);
    }
}

} // namespace hven::solvers::bench_cli
