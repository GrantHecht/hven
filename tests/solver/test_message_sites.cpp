// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_message_sites.cpp -- M8 TR-B: the message emit sites held by a source scan, in the pattern
// of tests/sqp/test_qp_mode_sites.cpp. Every `MessageKind` an engine file constructs is counted
// and compared with the table below, so a site added, dropped or moved to the wrong engine fails
// here instead of going unrepresented. The table covers EVERY enumerator of `MessageKind`
// (read from solver_status.h), so a new kind forces a decision. Falsified by planted sites.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#ifndef HVEN_SRC_ROOT
#error "HVEN_SRC_ROOT must be defined by tests/drivers/CMakeLists.txt"
#endif

namespace {

using Counts = std::map<std::string, int>;

std::string read(const std::filesystem::path &p) {
    std::ifstream in(p);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string strip_comments(const std::string &src) {
    std::string out;
    for (std::size_t i = 0; i < src.size(); ++i) {
        if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/') {
            while (i < src.size() && src[i] != '\n') {
                ++i;
            }
            out += '\n';
            continue;
        }
        out += src[i];
    }
    return out;
}

/// Emit sites per kind: a `MessageKind::kX` that is assigned to an event (`msg.kind = ...`,
/// possibly through a conditional), passed to the SQP's `make_sqp_message(` or `emit_note(`.
Counts sites_of(const std::string &raw) {
    const std::string src = strip_comments(raw);
    Counts out;
    const std::regex re(
        R"((?:\.kind\s*=\s*(?:[^;?]*\?\s*)?|make_sqp_message\(\s*|emit_note\(\s*)MessageKind::(k[A-Za-z0-9]+)(?:\s*:\s*MessageKind::(k[A-Za-z0-9]+))?)");
    for (auto it = std::sregex_iterator(src.begin(), src.end(), re); it != std::sregex_iterator();
         ++it) {
        ++out[(*it)[1].str()];
        if ((*it)[2].matched) {
            ++out[(*it)[2].str()];
        }
    }
    return out;
}

/// The expected sites per engine file. A kind absent from a file's row is expected zero times.
const Counts &ipm_expected() {
    static const Counts c = {
        {"kSolverInitialized", 1},
        {"kRankDeficiency", 1},
        {"kFactorizationHardError", 1},
        {"kInertiaExhausted", 1},
        {"kFeasibilityStall", 1},
        {"kPhaseDiverged", 1},
        {"kInterruptSkippingPhases", 1},
        {"kStopRequestedSkippingPhases", 1},
        {"kStopAtIteration", 1},
        {"kRestorationEntered", 1},
        // `leave_restoration` (every episode end that is not locally infeasible) and the locally
        // infeasible door, whose line must follow its row.
        {"kRestorationExited", 2},
    };
    return c;
}
const Counts &sqp_expected() {
    static const Counts c =
        {
            {"kRestorationEntered", 1},
            {"kRestorationExited", 1}, // one lambda, called at the three episode ends
            {"kRestorationRefused", 2},
            {"kSubproblemFailure", 2}, // the counted QP exits, and the non-finite guard
            {"kElasticExhausted", 1},
            {"kInconsistentEqualities", 1},
            {"kStartPointClamped", 1},
            {"kScaledConvergenceRefused", 2},
            {"kStopAtIteration", 1},
        };
    return c;
}

std::vector<std::string> enumerators() {
    const std::filesystem::path header =
        std::filesystem::path(HVEN_SRC_ROOT) / ".." / "include/hven/core/solver_status.h";
    const std::string text = strip_comments(read(header));
    const std::size_t at = text.find("enum class MessageKind");
    std::vector<std::string> out;
    if (at == std::string::npos) {
        return out;
    }
    const std::size_t end = text.find('}', at);
    const std::string body = text.substr(at, end - at);
    const std::regex re(R"((k[A-Za-z0-9]+)\s*=)");
    for (auto it = std::sregex_iterator(body.begin(), body.end(), re); it != std::sregex_iterator();
         ++it) {
        out.push_back((*it)[1].str());
    }
    return out;
}

/// Mismatches between the observed sites and a table, as readable lines.
std::vector<std::string> mismatches(const Counts &seen, const Counts &want) {
    std::vector<std::string> out;
    Counts all = want;
    for (const auto &[k, n] : seen) {
        all.emplace(k, 0);
    }
    for (const auto &[k, w] : all) {
        const auto it = seen.find(k);
        const int n = it == seen.end() ? 0 : it->second;
        if (n != w) {
            out.push_back(k + ": " + std::to_string(n) + " sites, expected " + std::to_string(w));
        }
    }
    return out;
}

std::string ipm_text() { return read(std::filesystem::path(HVEN_SRC_ROOT) / "ipm/ipm_solver.cpp"); }
std::string sqp_text() { return read(std::filesystem::path(HVEN_SRC_ROOT) / "sqp/sqp_solver.cpp"); }

TEST(TraceMessageSites, EveryEnumeratorIsAccountedForByAnEngineOrDeclaredUnemitted) {
    const std::vector<std::string> kinds = enumerators();
    ASSERT_EQ(kinds.size(), 17U) << "the v1 message set";
    for (const std::string &k : kinds) {
        const bool ipm = ipm_expected().count(k) != 0;
        const bool sqp = sqp_expected().count(k) != 0;
        EXPECT_TRUE(ipm || sqp) << k << " has no emit site in either engine";
    }
}

TEST(TraceMessageSites, TheIpmSourceConstructsExactlyTheDeclaredSites) {
    const Counts seen = sites_of(ipm_text());
    EXPECT_TRUE(mismatches(seen, ipm_expected()).empty())
        << ::testing::PrintToString(mismatches(seen, ipm_expected()));
}

TEST(TraceMessageSites, TheSqpSourceConstructsExactlyTheDeclaredSites) {
    const Counts seen = sites_of(sqp_text());
    EXPECT_TRUE(mismatches(seen, sqp_expected()).empty())
        << ::testing::PrintToString(mismatches(seen, sqp_expected()));
}

// The builder contract on both engines: `emit_message` calls its builder only after the sink test
// (so a solve with no sink constructs no message). The IPM twin is in test_trace_off_path.cpp.
TEST(TraceMessageSites, TheSqpMessageBuilderRunsOnlyAfterTheSinkTest) {
    const std::string text = strip_comments(sqp_text());
    const std::size_t def = text.find("SqpSolverImpl::emit_message(Build &&build)");
    ASSERT_NE(def, std::string::npos);
    const std::size_t guard = text.find("!= nullptr", def);
    const std::size_t call = text.find("build()", def);
    ASSERT_NE(guard, std::string::npos);
    ASSERT_NE(call, std::string::npos);
    EXPECT_LT(guard, call) << "the builder is invoked before the sink is tested";
}

TEST(TraceMessageSites, PlantedSitesAreCaught) {
    // An added site (a second start-point-clamp emit), a dropped site (the elastic ladder's
    // message rewritten to another kind), and a kind in the wrong engine (an SQP kind in the IPM).
    const std::string sqp = sqp_text();
    {
        const std::string planted =
            sqp + "\nvoid planted() { emit_note(MessageKind::kStartPointClamped, 0); }\n";
        EXPECT_FALSE(mismatches(sites_of(planted), sqp_expected()).empty()) << "an added site";
    }
    {
        std::string dropped = sqp;
        const std::size_t at = dropped.find("MessageKind::kElasticExhausted");
        ASSERT_NE(at, std::string::npos);
        dropped.replace(at, std::string("MessageKind::kElasticExhausted").size(),
                        "MessageKind::kRankDeficiency");
        EXPECT_FALSE(mismatches(sites_of(dropped), sqp_expected()).empty()) << "a dropped site";
    }
    {
        const std::string planted =
            ipm_text() + "\nvoid planted() { msg.kind = MessageKind::kSubproblemFailure; }\n";
        EXPECT_FALSE(mismatches(sites_of(planted), ipm_expected()).empty()) << "a misplaced kind";
    }
    // The scan is not vacuous on the unplanted sources.
    EXPECT_TRUE(mismatches(sites_of(sqp), sqp_expected()).empty());
}

} // namespace
