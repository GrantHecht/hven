// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

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
#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <hven/core/types.h>
#include <hven/detail/qp/qp_problem.h>

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

// M6 W1 T9 FIX ROUND 1 -- THE MID-SOLVE QP DUMP, format version 2, a DECLARED
// versioned extension: v1 (`PIQP_QP_DUMP 1`, bench_corpus.cpp) dumps only a
// cell's FIRST QP and --dump-solution above carries no matrix at all.

// v2 = v1's body plus `major` and the major's DUAL state, so a reader can
// re-solve the dumped subproblem WARM as the driver entered it. v1's writer is
// untouched: its reader is an out-of-tree tool pinned to that byte format.

// Round-tripped by tests/sqp/test_bench_dump.cpp; `.superpowers/w1-t9-report.md`.

/// One dumped subproblem. The three dual blocks are the SQP major's state at
/// the point the subproblem was built, empty when the producer had none.
struct QpDumpV2 {
    std::string family;
    std::string status;
    long long n_flag = 0;
    long long major = 0;
    double p = 0.0;
    hven::solvers::QpProblem qp;
    hven::Vec lambda_e, lambda_i, z;
};

namespace detail {

inline void dump_sparse_block(std::ostream &os, const char *tag,
                              const Eigen::SparseMatrix<double, Eigen::RowMajor> &a) {
    std::size_t nnz = 0;
    for (int k = 0; k < a.outerSize(); ++k) {
        for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(a, k); it; ++it) {
            ++nnz;
        }
    }
    os << fmt::format("{}_NNZ {}\n", tag, nnz);
    for (int k = 0; k < a.outerSize(); ++k) {
        for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(a, k); it; ++it) {
            os << fmt::format("{} {} {:.17g}\n", it.row(), it.col(), it.value());
        }
    }
}

inline void dump_vec_block(std::ostream &os, const char *tag, const hven::Vec &v) {
    os << fmt::format("{}_VEC {}\n", tag, v.size());
    for (hven::Index i = 0; i < v.size(); ++i) {
        os << fmt::format("{:.17g}\n", v(i));
    }
}

} // namespace detail

/// Write `d` in format version 2, `{:.17g}` throughout for the same reason
/// the solution dump uses it: round-trip exact for binary64.
inline void write_qp_dump_v2(std::ostream &os, const QpDumpV2 &d) {
    os << "HVEN_QP_DUMP 2\n";
    os << fmt::format("# family: {}  major: {}\n", d.family, d.major);
    os << fmt::format("family {}\n", d.family);
    os << fmt::format("status {}\n", d.status);
    os << fmt::format("n_flag {}\n", d.n_flag);
    os << fmt::format("major {}\n", d.major);
    os << fmt::format("p {:.17g}\n", d.p);
    os << fmt::format("n {}\n", d.qp.n());
    os << fmt::format("me {}\n", d.qp.me());
    os << fmt::format("mi {}\n", d.qp.mi());
    detail::dump_sparse_block(os, "H", d.qp.H);
    detail::dump_vec_block(os, "G", d.qp.g);
    detail::dump_sparse_block(os, "AE", d.qp.Ae);
    detail::dump_vec_block(os, "BE", d.qp.be);
    detail::dump_sparse_block(os, "AI", d.qp.Ai);
    detail::dump_vec_block(os, "BI", d.qp.bi);
    detail::dump_vec_block(os, "LOWER", d.qp.lower);
    detail::dump_vec_block(os, "UPPER", d.qp.upper);
    detail::dump_vec_block(os, "LAMBDA_E", d.lambda_e);
    detail::dump_vec_block(os, "LAMBDA_I", d.lambda_i);
    detail::dump_vec_block(os, "Z", d.z);
    os << "END\n";
}

namespace detail {

// Every read below throws rather than defaulting: a dump that does not say
// what it claims to say must never be scored, and a truncated write is
// exactly what the trailing `END` exists to catch.
[[noreturn]] inline void throw_dump(const std::string &detail) {
    throw std::invalid_argument(fmt::format("read_qp_dump_v2: {}", detail));
}

inline std::string next_token_line(std::istream &is, const std::string &what) {
    std::string line;
    while (std::getline(is, line)) {
        if (!line.empty() && line[0] == '#') {
            continue;
        }
        return line;
    }
    throw_dump(fmt::format("unexpected end of file, expected {}", what));
}

inline std::string tagged_string(std::istream &is, const std::string &tag) {
    const std::string line = next_token_line(is, tag);
    if (line.rfind(tag + " ", 0) != 0) {
        throw_dump(fmt::format("expected '{} <value>', found '{}'", tag, line));
    }
    return line.substr(tag.size() + 1);
}

inline long long tagged_count(std::istream &is, const std::string &tag) {
    const std::string value = tagged_string(is, tag);
    std::size_t pos = 0;
    long long parsed = -1;
    try {
        parsed = std::stoll(value, &pos);
    } catch (const std::exception &) {
        throw_dump(fmt::format("'{}' is not a count for {}", value, tag));
    }
    if (pos != value.size() || parsed < 0) {
        throw_dump(fmt::format("'{}' is not a non-negative count for {}", value, tag));
    }
    return parsed;
}

inline double parse_dump_double(const std::string &value, const std::string &what) {
    try {
        std::size_t pos = 0;
        const double parsed = std::stod(value, &pos);
        if (pos != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return parsed;
    } catch (const std::exception &) {
        throw_dump(fmt::format("'{}' is not a number ({})", value, what));
    }
}

inline hven::Vec read_vec_block(std::istream &is, const std::string &tag) {
    const long long m = tagged_count(is, tag + "_VEC");
    hven::Vec v(static_cast<hven::Index>(m));
    for (long long i = 0; i < m; ++i) {
        v(static_cast<hven::Index>(i)) = parse_dump_double(next_token_line(is, tag), tag);
    }
    return v;
}

inline void read_sparse_block(std::istream &is, const std::string &tag, hven::Index rows,
                              hven::Index cols, Eigen::SparseMatrix<double, Eigen::RowMajor> &out) {
    const long long nnz = tagged_count(is, tag + "_NNZ");
    std::vector<Eigen::Triplet<double>> trips;
    trips.reserve(static_cast<std::size_t>(nnz));
    for (long long k = 0; k < nnz; ++k) {
        const std::string line = next_token_line(is, tag);
        std::istringstream ls(line);
        std::string rs, cs, vs;
        if (!(ls >> rs >> cs >> vs)) {
            throw_dump(fmt::format("expected '<row> <col> <value>' in {}, found '{}'", tag, line));
        }
        const long long r = static_cast<long long>(parse_dump_double(rs, tag));
        const long long c = static_cast<long long>(parse_dump_double(cs, tag));
        if (r < 0 || c < 0 || r >= rows || c >= cols) {
            throw_dump(fmt::format("{} triplet ({}, {}) is outside {}x{}", tag, r, c, rows, cols));
        }
        trips.emplace_back(static_cast<int>(r), static_cast<int>(c), parse_dump_double(vs, tag));
    }
    out.resize(rows, cols);
    out.setFromTriplets(trips.begin(), trips.end());
    out.makeCompressed();
}

} // namespace detail

/// Read a version-2 dump, throwing `std::invalid_argument` on a wrong banner,
/// a count that disagrees with the lines after it, an out-of-range triplet, a
/// block width that contradicts the header, or a missing `END`.
inline QpDumpV2 read_qp_dump_v2(std::istream &is) {
    QpDumpV2 d;
    const std::string banner = detail::next_token_line(is, "the version banner");
    if (banner != "HVEN_QP_DUMP 2") {
        detail::throw_dump(fmt::format("expected banner 'HVEN_QP_DUMP 2', found '{}'", banner));
    }
    d.family = detail::tagged_string(is, "family");
    d.status = detail::tagged_string(is, "status");
    d.n_flag = detail::tagged_count(is, "n_flag");
    d.major = detail::tagged_count(is, "major");
    d.p = detail::parse_dump_double(detail::tagged_string(is, "p"), "p");
    const auto n = static_cast<hven::Index>(detail::tagged_count(is, "n"));
    const auto me = static_cast<hven::Index>(detail::tagged_count(is, "me"));
    const auto mi = static_cast<hven::Index>(detail::tagged_count(is, "mi"));
    detail::read_sparse_block(is, "H", n, n, d.qp.H);
    d.qp.g = detail::read_vec_block(is, "G");
    detail::read_sparse_block(is, "AE", me, n, d.qp.Ae);
    d.qp.be = detail::read_vec_block(is, "BE");
    detail::read_sparse_block(is, "AI", mi, n, d.qp.Ai);
    d.qp.bi = detail::read_vec_block(is, "BI");
    d.qp.lower = detail::read_vec_block(is, "LOWER");
    d.qp.upper = detail::read_vec_block(is, "UPPER");
    d.lambda_e = detail::read_vec_block(is, "LAMBDA_E");
    d.lambda_i = detail::read_vec_block(is, "LAMBDA_I");
    d.z = detail::read_vec_block(is, "Z");
    const std::string end = detail::next_token_line(is, "END");
    if (end != "END") {
        detail::throw_dump(fmt::format("expected 'END', found '{}' -- the dump is truncated", end));
    }
    if (d.qp.g.size() != n) {
        detail::throw_dump(
            fmt::format("G_VEC carries {} entries, expected n = {}", d.qp.g.size(), n));
    }
    d.qp.validate();
    return d;
}

} // namespace hven::solvers::bench_cli
