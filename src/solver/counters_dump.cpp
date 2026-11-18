// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// counters_dump.cpp -- dump_counters(IpmResult), dump_counters(SqpResult): the twins of
// dump_options, written from the X-macro tables in core/solver_counters.h that
// trace_writer.cpp writes `solve.end.counters` from. A field added to a table appears in both
// the trace and this dump; a field with no table entry is refused at compile time by the tables'
// own arity asserts, so the dump cannot drift from the struct.

#include <array>
#include <cstddef>
#include <ostream>
#include <string>

#include <fmt/format.h>

#include "hven/core/solver_counters.h"
#include "hven/core/solver_status.h"
#include "hven/core/start_level.h"
#include "hven/ipm/ipm_solver_types.h"
#include "hven/sqp/sqp_solver_types.h"

namespace hven {

namespace {

// The stream is written as one string, so the caller's flags, precision, width and locale reach
// no byte and none is touched.
void line(std::string &out, const std::string &prefix, const char *name, const std::string &v) {
    out += prefix;
    out += name;
    out += " = ";
    out += v;
    out += '\n';
}

std::string spell(Index v) { return fmt::format("{}", static_cast<long long>(v)); }
std::string spell(double v) { return fmt::format("{}", v); }
std::string spell(StartLevel v) { return to_string(v); }
std::string spell(RestorationSubStatus v) { return to_string(v); }
template <std::size_t N> std::string spell(const std::array<Index, N> &v) {
    std::string s = "[";
    for (std::size_t i = 0; i < N; ++i) {
        if (i != 0) {
            s += ", ";
        }
        s += spell(v[i]);
    }
    return s + "]";
}

template <typename T>
void put(std::string &out, const std::string &prefix, const char *name, const T &v, bool absent) {
    line(out, prefix, name, absent ? std::string("null") : spell(v));
}

// The linear layer's call counters have no table of their own (they are not hven counters in the
// trace's sense), so the dump names them here, pinned to the struct by the size assert.
#define HVEN_DUMP_LINEAR_COUNTERS(X)                                                               \
    X(analyze_count)                                                                               \
    X(factorize_count)                                                                             \
    X(solve_count)                                                                                 \
    X(partial_solve_count)                                                                         \
    X(pattern_verify_count)                                                                        \
    X(dense_factorize_count)                                                                       \
    X(dense_fallback_count)                                                                        \
    X(dense_singular_count)                                                                        \
    X(dense_flagged_count)                                                                         \
    X(dense_condition_estimate_count)                                                              \
    X(dense_equilibrated_estimate_count)                                                           \
    X(dense_shift_refactor_count)                                                                  \
    X(dense_retake_count)

#define HVEN_DUMP_COUNT_ONE(f) +1
static_assert(sizeof(linear::SymmetricFactor::Counters) ==
                  (0 HVEN_DUMP_LINEAR_COUNTERS(HVEN_DUMP_COUNT_ONE)) * sizeof(Index),
              "SymmetricFactor::Counters gained or lost a field: update "
              "HVEN_DUMP_LINEAR_COUNTERS in drivers/counters_dump.cpp.");
#undef HVEN_DUMP_COUNT_ONE

} // namespace

void dump_counters(const IpmResult &r, std::ostream &os) {
    std::string out;
    const std::string none;
    const IpmCounters &c = r.counters;
#define HVEN_DUMP_FIELD(f, absent) put(out, none, #f, c.f, absent(c.f));
    HVEN_IPM_COUNTERS_FIELDS(HVEN_DUMP_FIELD)
#undef HVEN_DUMP_FIELD
    put(out, none, "kkt_analyses_total", r.kkt_analyses_total, false);
    const std::string kkt = "kkt_factor_counters.";
#define HVEN_DUMP_LINEAR(f) put(out, kkt, #f, r.kkt_factor_counters.f, false);
    HVEN_DUMP_LINEAR_COUNTERS(HVEN_DUMP_LINEAR)
#undef HVEN_DUMP_LINEAR
    os << out;
}

void dump_counters(const SqpResult &r, std::ostream &os) {
    std::string out;
    const std::string none;
    const SqpCounters &c = r.counters;
#define HVEN_DUMP_FIELD(f, absent) put(out, none, #f, c.f, absent(c.f));
    HVEN_SQP_COUNTERS_FIELDS(HVEN_DUMP_FIELD)
#undef HVEN_DUMP_FIELD
    const std::string ssn = "ssn.";
#define HVEN_DUMP_FIELD(f, absent) put(out, ssn, #f, c.ssn.f, absent(c.ssn.f));
    HVEN_SSN_COUNTERS_FIELDS(HVEN_DUMP_FIELD)
#undef HVEN_DUMP_FIELD
    const std::string ipqp = "ipqp.";
#define HVEN_DUMP_FIELD(f, absent) put(out, ipqp, #f, c.ipqp.f, absent(c.ipqp.f));
    HVEN_IPQP_COUNTERS_FIELDS(HVEN_DUMP_FIELD)
#undef HVEN_DUMP_FIELD
    os << out;
}

} // namespace hven
