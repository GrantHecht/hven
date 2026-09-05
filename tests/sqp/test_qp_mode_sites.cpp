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
#include <iostream>
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
// THE RULE IS COUNT-MATCHED PAIRING IN FILE ORDER, not "some emit nearby"
// (fix round 1, astra's F1). Between one EMITTING kernel call site and the next
// there is EXACTLY ONE `qp.mode` emit.
//
// So every emit is consumed by exactly one call, and a call that borrows its
// neighbour's emit fails.
//
// A SITE MARKED SILENT IS TRANSPARENT to that count: it consumes no emit, which
// is what "silent" means. The kSsn arm needs this -- its own emit is written at
// the end of the arm, past two `refine_on_face` calls silent by kind.
//
// It is no loophole: a bare call carries no marker, so it becomes an EMITTING
// site and must find an emit of its own.
//
// TWO NEGATIVE PROBES the previous, window-based rule ACCEPTED and this one
// rejects. Both were run against a mutated copy of the driver and both now
// fail; neither mutant is committed.
//
//   (a) A SECOND BARE CALL. Insert `(void)engine.solve(qp, overrides);` on the
//       line after `certified_feasibility_fallback`'s unfired-path walk.
//
//       The old rule PASSED it: the emit two lines below was inside its 20-line
//       window and covered both calls.
//
//       The new rule sees two EMITTING sites with ZERO emits between the first
//       and the second, and fails naming the FIRST -- "sqp_driver.cpp:1169:
//       expected exactly 1 qp.mode emit ... found 0".
//
//   (b) AN EMPTY MARKER. Truncate one silent marker to
//       `// trace: qp.mode silent --`.
//
//       The old rule matched the prefix and ACCEPTED it, so the call it covered
//       vanished from the check.
//
//       A reason must now carry at least `kMinReason` non-space characters, so
//       the truncated line is not a marker.
//
//       It fails TWICE: the malformed check names the line, and the pairing then
//       reports its call site ("sqp_driver.cpp:4131: ... found 0") as uncovered.
//
// WHAT IT DELIBERATELY DOES NOT DO. It does not parse C++. It reads LINES, skips
// comment lines when looking for invocations, and asks one counting question per
// call site.
//
// A NEW MARKER IS A THING TO QUESTION, not to accept: it says a kernel runs and
// writes nothing. The test PRINTS every marker with its reason so a reviewer
// sees the whole silent list without opening the driver.

/// A maximal run of consecutive call LINES that form one invocation expression.
///
/// The run extends only across a line that does NOT end in `;` -- the walk
/// dispatch's four-armed ternary is one site spread over four lines, while a
/// second statement beside an existing call is its own site, which is exactly
/// what probe (a) turns on.
struct CallSite {
    int first = 0; // 1-based
    int last = 0;
    std::string text; // the first line, for the failure message
};

struct Marker {
    int line = 0;
    std::string reason;
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

std::string rstrip(const std::string &l) {
    const auto e = l.find_last_not_of(" \t\r");
    return e == std::string::npos ? std::string() : l.substr(0, e + 1);
}

/// A KERNEL INVOCATION LINE, matched by SHAPE rather than by a receiver list:
/// any `.solve(` / `->solve(` / `.refine_on_face(` in a code line. Matching by
/// shape is the fail-safe direction -- a call through a receiver nobody
/// anticipated is CAUGHT rather than missed.
///
/// ONE EXCLUSION, and it is by kind: `sub.solve(...)` is the restoration
/// sub-driver, a whole nested SQP solve that writes its own stream at depth 1
/// (its own dispatch lines included), not a kernel this driver dispatches to.
bool is_call_line(const std::string &l) {
    if (is_comment_line(l)) {
        return false;
    }
    const bool call = l.find(".solve(") != std::string::npos ||
                      l.find("->solve(") != std::string::npos ||
                      l.find(".refine_on_face(") != std::string::npos;
    return call && l.find("sub.solve(") == std::string::npos;
}

/// AN EMIT **CALL**, not the emit helper's own definition. Without this the
/// count would treat `void SqpDriver::emit_trace_qp_mode(...) {` as an emit --
/// proven by mutation before fix round 1, when a bare call planted beside that
/// definition passed.
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

/// THE ONE SANCTIONED MARKER FORM: `// trace: qp.mode silent -- <reason>`, with
/// at least `kMinReason` non-space characters of reason. An empty or
/// whitespace-only reason is NOT a marker -- probe (b).
constexpr std::size_t kMinReason = 3;
constexpr std::string_view kMarkerPrefix = "// trace: qp.mode silent --";

/// Does this line CLAIM to be a marker? Any `// trace:` line does, which is what
/// makes a malformed one a loud failure rather than a silent non-match.
bool claims_to_be_marker(const std::string &l) { return l.find("// trace:") != std::string::npos; }

/// The reason text, or empty when the line is not a well-formed marker.
std::string marker_reason(const std::string &l) {
    const auto p = l.find(kMarkerPrefix);
    if (p == std::string::npos) {
        return {};
    }
    std::string r = rstrip(l.substr(p + kMarkerPrefix.size()));
    const auto b = r.find_first_not_of(" \t");
    if (b == std::string::npos) {
        return {};
    }
    r = r.substr(b);
    return r.size() >= kMinReason ? r : std::string();
}

std::vector<CallSite> call_sites(const std::vector<std::string> &lines) {
    std::vector<CallSite> out;
    for (std::size_t i = 0; i < lines.size();) {
        if (!is_call_line(lines[i])) {
            ++i;
            continue;
        }
        CallSite s;
        s.first = static_cast<int>(i) + 1;
        s.text = lines[i];
        std::size_t j = i;
        // Extend across a continuation only: a line ending in `;` closes the
        // statement, so the next call is a site of its own.
        while (!rstrip(lines[j]).ends_with(';') && j + 1 < lines.size() &&
               is_call_line(lines[j + 1])) {
            ++j;
        }
        s.last = static_cast<int>(j) + 1;
        out.push_back(s);
        i = j + 1;
    }
    return out;
}

/// How far ABOVE a call site's first line a marker binds to it.
constexpr int kMarkerBefore = 4;

/// The marker bound to this call site, or -1. A marker binds to at most one
/// site: the search is downward from the marker, and the nearest site wins.
int marker_for(const std::vector<std::string> &lines, const CallSite &site) {
    for (int j = site.first - 2; j >= site.first - 1 - kMarkerBefore && j >= 0; --j) {
        if (!marker_reason(lines[static_cast<std::size_t>(j)]).empty()) {
            return j + 1;
        }
    }
    return -1;
}

TEST(QpModeSiteScan, TheScanActuallyReadsTheDriverAndFindsItsKernelCalls) {
    // THE VACUOUS-PASS GUARD, and it comes first for the reason
    // tests/core/test_core_layering.cpp's does: a wrong path, a renamed file or
    // a missing definition would make the rule below check nothing and pass.
    ASSERT_TRUE(std::filesystem::is_regular_file(std::filesystem::path(HVEN_SQP_DRIVER_SOURCE)))
        << "HVEN_SQP_DRIVER_SOURCE does not name a file: " << HVEN_SQP_DRIVER_SOURCE;

    const std::vector<std::string> lines = read_driver_source();
    ASSERT_GT(lines.size(), 1000u) << "the driver TU is far shorter than it should be";

    const std::vector<CallSite> sites = call_sites(lines);
    EXPECT_GE(sites.size(), 10u) << "the scan found only " << sites.size()
                                 << " kernel call sites in the driver, which means the reader is "
                                    "broken, not that the driver stopped solving QPs";

    Index emits = 0;
    for (const std::string &l : lines) {
        emits += is_emit_call(l) ? 1 : 0;
    }
    EXPECT_GE(emits, 5) << "no qp.mode emits found -- the pairing rule would be vacuous";

    // BOTH COVERAGE KINDS MUST BE IN USE, or a rule that only ever sees one of
    // them is not the rule this file claims to enforce.
    std::vector<Marker> markers;
    std::vector<std::string> malformed;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::string r = marker_reason(lines[i]);
        if (r.empty()) {
            if (claims_to_be_marker(lines[i])) {
                malformed.push_back(std::to_string(i + 1) + ": " + lines[i]);
            }
            continue;
        }
        // A reason may WRAP, and the printed list is worth reading, so the
        // continuation comment lines below it join it.
        Marker m{static_cast<int>(i) + 1, r};
        for (std::size_t j = i + 1;
             j < lines.size() && is_comment_line(lines[j]) && marker_reason(lines[j]).empty() &&
             !claims_to_be_marker(lines[j]);
             ++j) {
            const std::string t = rstrip(lines[j]);
            const auto b = t.find("//");
            const std::string body = t.substr(b + 2);
            const auto nb = body.find_first_not_of(" \t");
            if (nb == std::string::npos) {
                break;
            }
            m.reason += " " + body.substr(nb);
        }
        markers.push_back(m);
    }
    EXPECT_GE(markers.size(), 4u) << "no silent markers found -- that arm of the rule is dead";

    // AND NO LINE MAY CLAIM TO BE A MARKER WITHOUT BEING ONE. An empty or
    // truncated reason is the second blind spot fix round 1 closed (probe (b)):
    // it must fail here rather than quietly covering a call site.
    std::string report;
    for (const std::string &m : malformed) {
        report += "\n  " + m;
    }
    EXPECT_TRUE(malformed.empty())
        << "a silent marker must read `// trace: qp.mode silent -- <reason>` with at least "
        << kMinReason
        << " characters of reason. These lines claim to be markers and are not, so the call "
           "sites they were meant to cover are UNCOVERED:"
        << report;

    // THE SILENT LIST, PRINTED. A reviewer sees every kernel call the driver
    // makes without a `qp.mode` line, and the reason each gives.
    //
    // A NEW ENTRY IS A THING TO QUESTION: it says a kernel ran and the stream
    // does not know.
    std::string listing;
    for (const Marker &m : markers) {
        listing += "\n  sqp_driver.cpp:" + std::to_string(m.line) + "  " + m.reason;
    }
    std::cout << "qp.mode SILENT CALL SITES (" << markers.size() << "):" << listing << "\n";
    // Every marker must bind to a call site, or it is decoration that has drifted
    // away from the call it once explained.
    std::vector<std::string> orphans;
    for (const Marker &m : markers) {
        bool bound = false;
        for (const CallSite &s : sites) {
            bound = bound || marker_for(lines, s) == m.line;
        }
        if (!bound) {
            orphans.push_back("sqp_driver.cpp:" + std::to_string(m.line) + "  " + m.reason);
        }
    }
    std::string orphan_report;
    for (const std::string &o : orphans) {
        orphan_report += "\n  " + o;
    }
    EXPECT_TRUE(orphans.empty()) << "a silent marker must sit within " << kMarkerBefore
                                 << " lines above the call it explains; these bind to no call:"
                                 << orphan_report;
}

TEST(QpModeSiteScan, EveryKernelCallSitePairsWithItsOwnQpModeEmit) {
    const std::vector<std::string> lines = read_driver_source();
    const std::vector<CallSite> sites = call_sites(lines);
    ASSERT_FALSE(sites.empty());

    const auto emits_in = [&](int from, int to) {
        Index n = 0;
        for (int j = from; j < to && j < static_cast<int>(lines.size()); ++j) {
            n += is_emit_call(lines[static_cast<std::size_t>(j)]) ? 1 : 0;
        }
        return n;
    };

    // THE EMITTING SITES, in file order. A site marked silent drops out here and
    // consumes nothing; its marker is checked, printed and bound by the test
    // above, so it is accounted for -- just not by the emit count.
    std::vector<CallSite> emitting;
    Index silent = 0;
    for (const CallSite &s : sites) {
        if (marker_for(lines, s) >= 0) {
            ++silent;
        } else {
            emitting.push_back(s);
        }
    }
    ASSERT_FALSE(emitting.empty());
    ASSERT_GT(silent, 0) << "non-vacuous: the silent arm of the rule is exercised";

    std::vector<std::string> problems;
    // Nothing may emit before the first call site: such an emit belongs to no
    // invocation at all.
    if (const Index before = emits_in(0, emitting.front().first - 1); before != 0) {
        problems.push_back("an emit appears BEFORE the first kernel call site (" +
                           std::to_string(before) + " of them)");
    }
    for (std::size_t k = 0; k < emitting.size(); ++k) {
        const CallSite &s = emitting[k];
        const int stop =
            (k + 1 < emitting.size()) ? emitting[k + 1].first - 1 : static_cast<int>(lines.size());
        const Index found = emits_in(s.last, stop);
        if (found == 1) {
            continue;
        }
        problems.push_back("sqp_driver.cpp:" + std::to_string(s.first) +
                           (s.last != s.first ? "-" + std::to_string(s.last) : "") +
                           ": expected exactly 1 qp.mode emit before the next EMITTING kernel "
                           "call, found " +
                           std::to_string(found) + "  |" + s.text);
    }

    std::string report;
    for (const std::string &p : problems) {
        report += "\n  " + p;
    }
    EXPECT_TRUE(problems.empty())
        << "Since M6 W4 T5 EVERY kernel invocation in the SQP driver writes a `qp.mode` line, "
           "tagged by `site`, so the stream reconciles against invocation counts "
           "(docs/trace-schema-v0.md). The pairing is COUNT-MATCHED in file order: between one "
           "EMITTING kernel call site and the next there is exactly one emit "
           "(`emit_trace_qp_mode` for a driver member, `emit_qp_mode_line` for a free function), "
           "so every emit is consumed by exactly one call and a call that borrows its "
           "neighbour's emit fails here. A site carrying `// trace: qp.mode silent -- <reason>` "
           "within "
        << kMarkerBefore
        << " lines above it consumes none. Adding a `site` value means updating `QpModeSite`, the "
           "serializer's spelling, the identity table in this file, and the schema document.\n"
           "Unpaired site(s):"
        << report;
}

} // namespace
} // namespace hven::solvers
