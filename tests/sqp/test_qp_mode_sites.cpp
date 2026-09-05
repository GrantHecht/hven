// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_mode_sites.cpp -- M6 W4 task 5, part 3: `qp.mode`'s `site` key.
//
// TWO FAMILIES, and they hold each other up.
//
//   (i)  THE COUNT IDENTITIES. Every kernel call site in the SQP driver writes
//        a `qp.mode` line, so the stream partitioned by `site` reconciles
//        against the counters that price those invocations.
//
//        `run_elastic_ladder`'s climb is the reason: its length is not known in
//        advance, so it is priced by `elastic_activations + elastic_escalations`
//        rather than by majors.
//
//   (ii) THE SOURCE SCAN. The list in (i) is complete only while the driver's
//        call sites are the ones it names, and nothing in the type system says
//        so.
//
//        The scan reads src/drivers/sqp_driver.cpp and requires every
//        `QpEngine::solve` / SSN `solve` / `refine_on_face` invocation to be
//        covered by a nearby emit or by an explicit `// trace:` marker.
//
//        A site added later then fails this test rather than going silently
//        unrepresented. Precedent: tests/core/test_core_layering.cpp.

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/trace_writer.h>

#include "support/hs_problems.h"

#ifndef HVEN_SQP_DRIVER_SOURCE
#error "HVEN_SQP_DRIVER_SOURCE must be defined by tests/sqp/CMakeLists.txt"
#endif

namespace hven::solvers {
namespace {

using ::hven::solvers::test_support::HsProblem;
using ::hven::solvers::test_support::make_hs;

// ===========================================================================
// (i) THE COUNT IDENTITIES
// ===========================================================================

std::vector<std::string> split_lines(const std::string &s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    std::string line;
    while (std::getline(in, line)) {
        out.push_back(line);
    }
    return out;
}

/// The `site` token of every `qp.mode` line, counted. Deliberately textual: the
/// pin is on the SERIALIZED stream, which is what a reader of the artifact gets.
std::map<std::string, Index> site_census(const std::string &stream) {
    std::map<std::string, Index> out;
    for (const std::string &l : split_lines(stream)) {
        if (l.find("\"ev\":\"qp.mode\"") == std::string::npos) {
            continue;
        }
        const auto k = l.find("\"site\":\"");
        if (k == std::string::npos) {
            ++out["MISSING"];
            continue;
        }
        const auto b = k + 8;
        const auto e = l.find('"', b);
        ++out[l.substr(b, e - b)];
    }
    return out;
}

Index at(const std::map<std::string, Index> &m, const char *k) {
    const auto it = m.find(k);
    return it == m.end() ? Index{0} : it->second;
}

/// One traced solve of an HS cell, returned as its site census beside the
/// counters the identities are written against.
struct SiteRun {
    std::map<std::string, Index> sites;
    SqpCounters counters;
    Index unfired_entries = 0;
    std::size_t majors_with_a_qp = 0;
};

SiteRun run_cell(int hs, QpMode mode) {
    SqpOptions opts;
    opts.qp_mode = mode;
    opts.max_iter = 60;
    const HsProblem p = make_hs(hs);
    SqpDriver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpSolution sol = driver.solve(*p.model);

    SiteRun r;
    r.sites = site_census(os.str());
    r.counters = sol.counters;
    for (const std::string &l : split_lines(os.str())) {
        if (l.find("\"ev\":\"fallback.verdict\"") != std::string::npos &&
            l.find("\"verdict\":\"unfired\"") != std::string::npos) {
            ++r.unfired_entries;
        }
    }
    // COUNTED FROM THE STREAM, not from `sol.history`: the restoration
    // sub-solve's rows are in the stream at depth 1 and its dispatch lines are
    // too, while the parent's `history` carries neither.
    for (const std::string &l : split_lines(os.str())) {
        if (l.find("\"ev\":\"sqp.major\"") != std::string::npos &&
            l.find("\"qp_solved\":true") != std::string::npos) {
            ++r.majors_with_a_qp;
        }
    }
    return r;
}

/// THE FIVE IDENTITIES, checked on one cell. Each site's line count equals the
/// counter that PRICES those invocations, and the whole stream is compared
/// against the solve's FOLDED counters -- the restoration sub-solve's lines are
/// in the stream (at depth 1) and its `elastic_*`/`soc_steps` are in the fold,
/// so the two sides describe the same population.
void expect_site_identities(const SiteRun &r) {
    EXPECT_EQ(at(r.sites, "MISSING"), 0) << "every qp.mode line carries a site key";

    // `run_elastic_ladder` walks the elastic QP ONCE PER RUNG: one walk per
    // activation, plus one more per escalation. This is the identity the
    // unbounded climb makes necessary -- no per-major count can stand in.
    EXPECT_EQ(at(r.sites, "elastic_rung"),
              r.counters.elastic_activations + r.counters.elastic_escalations)
        << "elastic_rung == elastic_activations + elastic_escalations";

    // The fallback returns the cold walk on exactly two branches: an entry
    // whose evidence never FIRED, and a rung A the engine declined.
    EXPECT_EQ(at(r.sites, "fallback_rung_b"), r.unfired_entries + r.counters.ipqp_fallback_rung_b)
        << "fallback_rung_b == unfired entries + ipqp_fallback_rung_b";

    // The warm grade runs once per subproblem the routing chain sends it, and
    // `ipqp_to_ssn` is incremented at that function's own entry.
    EXPECT_EQ(at(r.sites, "ssn_warm_grade"), r.counters.ipqp.ipqp_to_ssn)
        << "ssn_warm_grade == ipqp_to_ssn";

    // One walk per correction ATTEMPT, which is what soc_steps counts.
    EXPECT_EQ(at(r.sites, "soc_resolve"), r.counters.soc_steps) << "soc_resolve == soc_steps";

    // THE DISPATCH SITE IS BOUNDED, NOT EXACT, and the bound is the reading: a
    // major that solves a QP writes at least its own arm's line, and at most two
    // -- the handing arm's and the walk's, when an arm hands off.
    const Index majors = static_cast<Index>(r.majors_with_a_qp);
    EXPECT_GE(at(r.sites, "dispatch"), majors) << "one dispatch line per major that solved a QP";
    EXPECT_LE(at(r.sites, "dispatch"), 2 * majors) << "at most the handing arm's and the walk's";
}

/// One cell of the table below: the solve, and the ONE site it is here to make
/// non-vacuous. `nullptr` means "no non-dispatch site fires here", which is
/// itself a reading -- the baseline a walk cell writes.
struct Cell {
    int hs;
    QpMode mode;
    const char *exercises;
};

TEST(QpModeSites, EveryKernelInvocationIsAccountedForBySite) {
    // OBSERVED, MKL/Linux, both configs. The IDENTITIES are asserted; the
    // per-cell `exercises` column is the NON-VACUITY premise, so a cell that
    // stops reaching its site fails here rather than passing 0 == 0.
    const std::vector<Cell> cells{
        {24, QpMode::kWalk, nullptr}, // the baseline: dispatch and nothing else
        {33, QpMode::kSsn, nullptr},  // the same, through the SSN arm
        {11, QpMode::kIpm, "elastic_rung"},
        {10, QpMode::kSsn, "elastic_rung"},    // a LONG climb (48 escalations)
        {15, QpMode::kIpm, "fallback_rung_b"}, // an entry whose evidence never fired
        {27, QpMode::kIpm, "ssn_warm_grade"},  // three sites on one cell
        {45, QpMode::kIpm, "ssn_warm_grade"},  // the grade three times over
        {38, QpMode::kIpm, "fallback_rung_b"}, // the natural-stall cell
        {77, QpMode::kWalk, "soc_resolve"},    // the battery's own SOC cell
    };

    std::map<std::string, Index> seen;
    for (const Cell &c : cells) {
        SCOPED_TRACE("HS" + std::to_string(c.hs) + " mode " +
                     std::to_string(static_cast<int>(c.mode)));
        const SiteRun r = run_cell(c.hs, c.mode);
        expect_site_identities(r);
        if (c.exercises != nullptr) {
            EXPECT_GT(at(r.sites, c.exercises), 0) << "this cell's own reason for being here";
        }
        for (const auto &kv : r.sites) {
            seen[kv.first] += kv.second;
        }
    }

    // AND EVERY SPELLING REACHED A REAL STREAM. The enum has five values; a
    // table that never exercises one would leave its string unproven, which is
    // exactly how `"unknown"` reaches an artifact.
    for (const char *site :
         {"dispatch", "ssn_warm_grade", "fallback_rung_b", "elastic_rung", "soc_resolve"}) {
        EXPECT_GT(at(seen, site), 0) << "no cell in the table exercises site " << site;
    }
    EXPECT_EQ(at(seen, "unknown"), 0) << "an unspelled QpModeSite enumerator reached the stream";
}

TEST(QpModeSites, TheLadderRungCountIsTheClimbAndNotTheMajorCount) {
    // THE IDENTITY THAT MAKES THE `site` KEY NECESSARY, isolated. HS10 at kSsn
    // runs 8 ladders over 14 majors and climbs 48 rungs between them.
    //
    // So no per-major reading can reconcile the walk invocations this solve
    // made -- only `elastic_activations + elastic_escalations` can.
    const SiteRun r = run_cell(10, QpMode::kSsn);
    ASSERT_GT(r.counters.elastic_escalations, r.counters.elastic_activations)
        << "fixture premise: the climb is longer than the number of ladders";
    EXPECT_EQ(at(r.sites, "elastic_rung"),
              r.counters.elastic_activations + r.counters.elastic_escalations);
    EXPECT_GT(at(r.sites, "elastic_rung"), at(r.sites, "dispatch"))
        << "and it outnumbers the dispatch lines, which is the whole point";
}

// ===========================================================================
// (ii) THE SOURCE SCAN
// ===========================================================================
//
// WHY IT EXISTS. The per-site list above is complete only while the driver's
// kernel call sites are the ones it names. Nothing in the type system says so.
//
// A new `engine_.solve(...)` compiles, passes every other test, and writes no
// `qp.mode` line -- leaving the schema document quietly wrong about what the
// stream reconciles against. This test is the thing that fails.
//
// WHAT IT DOES NOT DO. It does not parse C++. It reads LINES, skips comment
// lines when looking for invocations, and asks one question per invocation:
// is there an emit close after it, or an explicit marker beside it?

/// One kernel invocation, with enough context to name it in a failure message.
struct Invocation {
    int line = 0; // 1-based
    std::string text;
};

std::vector<std::string> read_driver_source() {
    std::vector<std::string> lines;
    std::ifstream in{std::filesystem::path(HVEN_SQP_DRIVER_SOURCE)};
    EXPECT_TRUE(in.good()) << "could not open " << HVEN_SQP_DRIVER_SOURCE;
    std::string l;
    while (std::getline(in, l)) {
        lines.push_back(l);
    }
    return lines;
}

bool is_comment_line(const std::string &l) {
    const auto first = l.find_first_not_of(" \t");
    return first != std::string::npos && l.compare(first, 2, "//") == 0;
}

/// AN EMIT **CALL**, not the emit helper's own definition. Without this the
/// window would count `void SqpDriver::emit_trace_qp_mode(...) {` as coverage
/// for anything within 20 lines of it -- proven by mutation: a bare
/// `e.solve(q, ...)` planted beside that definition passed the rule until this
/// predicate was added.
bool is_emit_call(const std::string &l) {
    if (is_comment_line(l)) {
        return false;
    }
    if (l.find("emit_trace_qp_mode(") == std::string::npos &&
        l.find("emit_qp_mode_line(") == std::string::npos) {
        return false;
    }
    return l.find("void ") == std::string::npos; // a definition, not a call
}

/// The two SANCTIONED marker forms. Anything else spelled `// trace:` is a typo
/// or a new convention, and either way must not satisfy the rule silently.
bool is_trace_marker(const std::string &l) {
    return l.find("// trace: qp.mode") != std::string::npos ||
           l.find("// trace: silent -- ") != std::string::npos;
}

/// THE KERNEL INVOCATIONS, matched by SHAPE rather than by a receiver list: any
/// `.solve(` / `->solve(` / `.refine_on_face(` in a code line. Matching by shape
/// is the fail-safe direction -- a call through a receiver nobody anticipated is
/// CAUGHT rather than missed.
///
/// ONE EXCLUSION, and it is by kind: `sub.solve(...)` is the restoration
/// sub-driver, a whole nested SQP solve that writes its own stream at depth 1
/// (its own dispatch lines included), not a kernel this driver dispatches to.
std::vector<Invocation> kernel_invocations(const std::vector<std::string> &lines) {
    std::vector<Invocation> out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::string &l = lines[i];
        if (is_comment_line(l)) {
            continue;
        }
        const bool is_call = l.find(".solve(") != std::string::npos ||
                             l.find("->solve(") != std::string::npos ||
                             l.find(".refine_on_face(") != std::string::npos;
        if (!is_call || l.find("sub.solve(") != std::string::npos) {
            continue;
        }
        out.push_back(Invocation{static_cast<int>(i) + 1, l});
    }
    return out;
}

/// How far after an invocation an emit still counts as ITS emit. Wide enough
/// for the accumulate block that follows every kernel call, narrow enough that
/// a distant emit belonging to some other site cannot stand in -- and where an
/// arm genuinely defers its line further than this, the marker form is what
/// says so.
constexpr int kEmitWindow = 20;
/// How far BEFORE (and one line after) an invocation a marker binds to it.
constexpr int kMarkerBefore = 4;

TEST(QpModeSiteScan, TheScanActuallyReadsTheDriverAndFindsItsKernelCalls) {
    // THE VACUOUS-PASS GUARD, and it comes first for the reason
    // tests/core/test_core_layering.cpp's does: a wrong path, a renamed file or
    // a missing definition would make the rule below check nothing and pass.
    ASSERT_TRUE(std::filesystem::is_regular_file(std::filesystem::path(HVEN_SQP_DRIVER_SOURCE)))
        << "HVEN_SQP_DRIVER_SOURCE does not name a file: " << HVEN_SQP_DRIVER_SOURCE;

    const std::vector<std::string> lines = read_driver_source();
    ASSERT_GT(lines.size(), 1000u) << "the driver TU is far shorter than it should be";

    const std::vector<Invocation> calls = kernel_invocations(lines);
    // NINE distinct sites at W4 T5 (the four-way ternary counts as four lines):
    // the ladder rung, rung B twice, the kSsn arm, the tier, the walk ternary,
    // the SOC re-solve, the warm grade, and four `refine_on_face` calls.
    EXPECT_GE(calls.size(), 12u) << "the scan found only " << calls.size()
                                 << " kernel invocations in the driver, which means the reader is "
                                    "broken, not that the driver stopped solving QPs";

    // BOTH COVERAGE KINDS MUST BE IN USE, or a rule that only ever sees one of
    // them is not the rule this file claims to enforce.
    int markers = 0;
    for (const std::string &l : lines) {
        markers += is_trace_marker(l) ? 1 : 0;
    }
    EXPECT_GE(markers, 4) << "no `// trace:` markers found -- the marker arm of the rule is dead";

    // AND NO MARKER MAY BE MALFORMED. A `// trace:` line that matches neither
    // sanctioned form is a typo that would silently stop covering its site.
    std::vector<std::string> malformed;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].find("// trace:") != std::string::npos && !is_trace_marker(lines[i])) {
            malformed.push_back(std::to_string(i + 1) + ": " + lines[i]);
        }
    }
    std::string report;
    for (const std::string &m : malformed) {
        report += "\n  " + m;
    }
    EXPECT_TRUE(malformed.empty())
        << "a `// trace:` marker must read either `// trace: qp.mode <where>` or "
           "`// trace: silent -- <reason>`; these read neither:"
        << report;
}

TEST(QpModeSiteScan, EveryKernelCallSiteEmitsAQpModeLineOrSaysWhyItDoesNot) {
    const std::vector<std::string> lines = read_driver_source();
    const std::vector<Invocation> calls = kernel_invocations(lines);

    std::vector<std::string> uncovered;
    for (const Invocation &c : calls) {
        const int idx = c.line - 1;
        bool covered = false;
        for (int j = idx; j <= idx + kEmitWindow && j < static_cast<int>(lines.size()); ++j) {
            if (is_emit_call(lines[static_cast<std::size_t>(j)])) {
                covered = true;
                break;
            }
        }
        for (int j = std::max(0, idx - kMarkerBefore);
             !covered && j <= idx + 1 && j < static_cast<int>(lines.size()); ++j) {
            covered = is_trace_marker(lines[static_cast<std::size_t>(j)]);
        }
        if (!covered) {
            uncovered.push_back("src/drivers/sqp_driver.cpp:" + std::to_string(c.line) + ": " +
                                c.text);
        }
    }

    std::string report;
    for (const std::string &u : uncovered) {
        report += "\n  " + u;
    }
    EXPECT_TRUE(uncovered.empty())
        << "Since M6 W4 T5 EVERY kernel invocation in the SQP driver writes a `qp.mode` line, "
           "tagged by `site`, so the stream reconciles against invocation counts "
           "(docs/trace-schema-v0.md). A call site that writes none makes the schema document "
           "wrong about what the stream contains.\n"
           "Each site below must either emit within "
        << kEmitWindow
        << " lines (`emit_trace_qp_mode` for a driver member, `emit_qp_mode_line` for a free "
           "function) or carry a marker within "
        << kMarkerBefore
        << " lines above it: `// trace: qp.mode <where the line is written>` when the emit is "
           "further off, or `// trace: silent -- <reason>` when the call is not a dispatched "
           "kernel at all. Adding a `site` value means updating `QpModeSite`, the serializer's "
           "spelling, the identity table in this file, and the schema document.\n"
           "Uncovered site(s):"
        << report;
}

} // namespace
} // namespace hven::solvers
