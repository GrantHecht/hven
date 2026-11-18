// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M8 TR-E: dump_counters(IpmResult) and dump_counters(SqpResult), the twins of dump_options.
// Every check reads a result a real solve produced. The dump is held against the trace's
// `solve.end.counters` object: the two are written from one table, so a key the trace writes the
// dump must write with the same value.

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include <hven/core/solver_counters.h>
#include <hven/ipm/ipm_solver.h>
#include <hven/trace/json_lines_trace_sink.h>
#include <hven/sqp/sqp_solver.h>
#include <hven/model/problem.h>

#include "model/support/hs071_problem.h"

namespace {

using namespace hven;
using namespace hven_drivers_tests;

struct Dump {
    std::vector<std::string> keys;
    std::map<std::string, std::string> values;
};

Dump parse(const std::string &text) {
    Dump d;
    std::istringstream in(text);
    std::string l;
    while (std::getline(in, l)) {
        const auto eq = l.find(" = ");
        EXPECT_NE(eq, std::string::npos) << l;
        const std::string k = l.substr(0, eq);
        EXPECT_EQ(d.values.count(k), 0u) << "duplicate key " << k;
        d.keys.push_back(k);
        d.values[k] = l.substr(eq + 3);
    }
    return d;
}

// The `counters` object of the stream's solve.end line, as key -> raw value token. Nested
// objects (`ssn`, `ipqp`) are flattened under `ssn.` / `ipqp.`; the histogram array is kept as
// its bracketed text.
std::map<std::string, std::string> trace_counters(const std::string &stream) {
    std::map<std::string, std::string> out;
    const auto end_line = stream.rfind("\"ev\":\"solve.end\"");
    EXPECT_NE(end_line, std::string::npos);
    const auto at = stream.find("\"counters\":{", end_line);
    EXPECT_NE(at, std::string::npos);
    std::size_t i = at + std::string("\"counters\":{").size();
    std::string prefix;
    while (i < stream.size()) {
        if (stream[i] == '}') {
            if (prefix.empty()) {
                break;
            }
            prefix.clear();
            ++i;
            if (stream[i] == ',') {
                ++i;
            }
            continue;
        }
        EXPECT_EQ(stream[i], '"');
        const auto q = stream.find('"', i + 1);
        const std::string key = stream.substr(i + 1, q - i - 1);
        i = q + 2; // past `":`
        if (stream[i] == '{') {
            prefix = key + ".";
            ++i;
            continue;
        }
        std::size_t j = i;
        if (stream[i] == '[') {
            j = stream.find(']', i) + 1;
        } else {
            while (stream[j] != ',' && stream[j] != '}') {
                ++j;
            }
        }
        out[prefix + key] = stream.substr(i, j - i);
        i = j;
        if (stream[i] == ',') {
            ++i;
        }
    }
    return out;
}

// The trace's raw token in the dump's spelling: a JSON array becomes `[a, b]`, a quoted enum
// string loses its quotes and is compared case-insensitively (the trace spells enums in lower
// snake, the dump by `to_string`).
std::string lower(std::string s) {
    for (char &c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

void expect_matches_trace(const Dump &d, const std::map<std::string, std::string> &t,
                          const std::vector<std::string> &dump_only_prefixes) {
    for (const auto &[k, v] : t) {
        ASSERT_TRUE(d.values.count(k)) << "trace key missing from the dump: " << k;
        std::string tv = v;
        if (!tv.empty() && tv.front() == '[') {
            tv = std::regex_replace(tv, std::regex(","), ", ");
        }
        if (!tv.empty() && tv.front() == '"') {
            tv = tv.substr(1, tv.size() - 2);
            EXPECT_EQ(lower(d.values.at(k)), lower(tv)) << k;
        } else if (tv.front() == '[' || tv == "null" || tv == "\"nan\"" ||
                   d.values.at(k) == "null") {
            EXPECT_EQ(d.values.at(k), tv) << k;
        } else {
            // The trace writes a double at %.17g and the dump at its shortest round-trip form:
            // two spellings of one value, compared as the value.
            EXPECT_EQ(std::stod(d.values.at(k)), std::stod(tv)) << k;
        }
    }
    // Every dump key is either a trace counter or under one of the declared dump-only prefixes.
    for (const auto &k : d.keys) {
        if (t.count(k) == 0) {
            bool declared = false;
            for (const auto &p : dump_only_prefixes) {
                declared = declared || k.rfind(p, 0) == 0;
            }
            EXPECT_TRUE(declared) << "dump key with no trace twin and no declared prefix: " << k;
        }
    }
}

TEST(DumpCounters, IpmDumpIsTheTraceCountersTableOnARealSolve) {
    Problem problem(std::make_shared<Hs071Problem>());
    IpmOptions o;
    o.common.verbosity = Verbosity::kSilent;
    IpmSolver solver(o);
    std::ostringstream json;
    JsonLinesTraceSink sink(json);
    solver.attach_trace(&sink);
    const IpmResult r = solver.solve(problem, hs071_start());
    ASSERT_EQ(r.status, SolveStatus::kOptimal);

    std::ostringstream os;
    dump_counters(r, os);
    const Dump d = parse(os.str());

    // IpmCounters' table, then kkt_analyses_total, then the 13 linear counters.
    EXPECT_EQ(d.keys.size(), kIpmCountersFieldCount + 1 + 13);
    EXPECT_EQ(d.keys.front(), "factorizations");
    EXPECT_EQ(d.keys[kIpmCountersFieldCount], "kkt_analyses_total");
    EXPECT_EQ(d.keys[kIpmCountersFieldCount + 1], "kkt_factor_counters.analyze_count");
    expect_matches_trace(d, trace_counters(json.str()),
                         {"kkt_analyses_total", "kkt_factor_counters."});

    // The IPM's factorization account the harness has always read, by its new key.
    EXPECT_EQ(d.values.at("kkt_factor_counters.factorize_count"),
              std::to_string(r.kkt_factor_counters.factorize_count));
    EXPECT_EQ(d.values.at("kkt_analyses_total"), std::to_string(r.kkt_analyses_total));
    EXPECT_GT(r.kkt_factor_counters.factorize_count, 0);
}

TEST(DumpCounters, SqpDumpIsTheTraceCountersTablesOnARealSolve) {
    Problem problem(std::make_shared<Hs071Problem>());
    SqpOptions o;
    o.common.verbosity = Verbosity::kSilent;
    SqpSolver solver(o);
    std::ostringstream json;
    JsonLinesTraceSink sink(json);
    solver.attach_trace(&sink);
    const SqpResult r = solver.solve(problem, hs071_start());
    ASSERT_EQ(r.status, SolveStatus::kOptimal);

    std::ostringstream os;
    dump_counters(r, os);
    const Dump d = parse(os.str());
    EXPECT_EQ(d.keys.size(),
              kSqpCountersFieldCount + kSsnCountersFieldCount + kIpqpCountersFieldCount);
    EXPECT_EQ(d.keys.front(), "major_iters");
    expect_matches_trace(d, trace_counters(json.str()), {});
    EXPECT_EQ(d.values.at("factorizations"), std::to_string(r.counters.factorizations));
}

TEST(DumpCounters, IsDeterministicAndLeavesTheStreamStateAlone) {
    Problem problem(std::make_shared<Hs071Problem>());
    IpmOptions o;
    o.common.verbosity = Verbosity::kSilent;
    IpmSolver solver(o);
    const IpmResult r = solver.solve(problem, hs071_start());

    std::ostringstream plain;
    dump_counters(r, plain);

    std::ostringstream odd;
    odd.imbue(std::locale::classic());
    odd.precision(3);
    odd.width(40);
    odd.flags(std::ios::hex | std::ios::showpos | std::ios::scientific);
    const auto flags = odd.flags();
    dump_counters(r, odd);
    dump_counters(r, odd);
    EXPECT_EQ(odd.flags(), flags);
    EXPECT_EQ(odd.precision(), 3);
    EXPECT_EQ(odd.str(), plain.str() + plain.str());
}

TEST(DumpCounters, AnAbsentCounterPrintsNullAndADefaultResultDumpsWholeTables) {
    SqpResult r;
    std::ostringstream os;
    dump_counters(r, os);
    const Dump d = parse(os.str());
    // ipqp_alpha_p_min's default is +inf, the table's "never measured" sentinel.
    ASSERT_TRUE(d.values.count("ipqp.ipqp_alpha_p_min"));
    EXPECT_EQ(d.values.at("ipqp.ipqp_alpha_p_min"), "null");
    EXPECT_EQ(d.values.at("major_iters"), "0");

    IpmResult ir;
    std::ostringstream is;
    dump_counters(ir, is);
    const Dump di = parse(is.str());
    EXPECT_EQ(di.values.at("recovery_depth_histogram"), "[0, 0, 0, 0, 0]");
    EXPECT_EQ(di.values.at("start_level_used"), "Cold");
}

} // namespace
