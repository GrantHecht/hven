// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_trace_off_path.cpp -- M8 TR-A, cost contract proof 1 (spec section 4, rule C1): with nothing
// attached the engines construct no trace event and call no emit helper.
//
// A source scan, in the pattern of tests/sqp/test_qp_mode_sites.cpp. In src/{ipm,sqp,solver,qp,warmstart}/
// every
// `*TraceEvent` construction and every `emit_*`, `make_iteration_record` or sink `on_*` call must
// sit lexically inside a block opened by a sink guard (`!= nullptr`) or inside a builder lambda
// handed to `emit_message`, except for the named allowlist. Falsified below by a planted unguarded
// construction and a planted unguarded call.
//
// WHAT THE SCAN CANNOT SEE: a STATE mutation done only to feed the trace (an accumulation, a
// buffer write) has no event construction to match, so it is out of the scan's reach in general.
// Those are pinned by behaviour (no-sink regression tests, e.g. RiderNonFiniteAbort in
// test_evaluation_failure.cpp), plus the one targeted scan below for the shape that actually
// occurred: the non-finite abort block accumulating the gradient into the retained right-hand
// side more than once.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#ifndef HVEN_SRC_ROOT
#error "HVEN_SRC_ROOT must be defined by tests/solver/CMakeLists.txt"
#endif

namespace {

// Constructions or calls the rule polices.
const std::regex &site_regex() {
    static const std::regex re(
        R"(\b[A-Za-z]+TraceEvent\s*(\{|[a-z_]+\s*[;={(])|\bemit_(iter|message|trace_[a-z_]+)\s*\(|\bmake_iteration_record\s*\(|->\s*on_(iter|message|solve_begin|solve_end|qp_mode|ipqp_[a-z]+|fallback_verdict|ipm_[a-z_]+)\s*\()");
    return re;
}

// Sites that are allowed outside a guard, by text, with the reason.
struct Allowed {
    const char *needle;
    const char *reason;
};
const std::vector<Allowed> &allowlist() {
    static const std::vector<Allowed> a = {
        {"SqpFallbackVerdictTraceEvent", "the judge's own out-param: read by control flow, so "
                                         "filled whether or not a sink listens"},
        // Declarations and definitions of the helpers themselves, and their forward declarations.
        {"void emit_trace_", "a definition or declaration of an emit helper; its body is scanned"},
        {"void emit_iter(", "a definition or declaration of an emit helper; its body is scanned"},
        {"void emit_message(",
         "a definition or declaration of an emit helper; its body is scanned"},
        {"make_iteration_record(const",
         "a definition of the row builder; called only under a guard"},
        {"result_.history.push_back(",
         "IpmResult::history is a result feature, always on (spec C5): "
         "the counted rows rendered once at phase exit, not a sink emit"},
        {"struct ", "a type declaration"},
        {"emit_trace_fallback_verdict(", "the fallback verdict (spec section 4's one named "
                                         "allowlist entry); the helper tests the sink itself"},
    };
    return a;
}

std::string strip(const std::string &src) {
    std::string out;
    out.reserve(src.size());
    bool in_str = false;
    for (std::size_t i = 0; i < src.size(); ++i) {
        const char c = src[i];
        if (!in_str && c == '/' && i + 1 < src.size() && src[i + 1] == '/') {
            while (i < src.size() && src[i] != '\n') {
                ++i;
            }
            out += '\n';
            continue;
        }
        if (c == '"' && (i == 0 || src[i - 1] != '\\')) {
            in_str = !in_str;
            out += c;
            continue;
        }
        out += in_str && c != '\n' ? ' ' : c;
    }
    return out;
}

bool guard_opener(const std::string &line) {
    return line.find("!= nullptr") != std::string::npos ||
           line.find("emit_message([&]") != std::string::npos ||
           line.find("emit_message([") != std::string::npos;
}

// Returns one violation message per unguarded site.
std::vector<std::string> scan(const std::string &raw, const std::string &file) {
    const std::string src = strip(raw);
    std::vector<std::string> lines;
    {
        std::istringstream in(src);
        for (std::string l; std::getline(in, l);) {
            lines.push_back(l);
        }
    }
    std::vector<std::string> violations;
    for (std::size_t n = 0; n < lines.size(); ++n) {
        const std::string &line = lines[n];
        if (!std::regex_search(line, site_regex())) {
            continue;
        }
        // A definition or declaration starts in column 0; builders (`make_*`) run only under a
        // guard.
        if (!line.empty() && line[0] != ' ' && line[0] != '\t' &&
            line.find('(') != std::string::npos) {
            continue;
        }
        {
            std::string fn;
            for (std::size_t m = n + 1; m-- > 0;) {
                if (!lines[m].empty() && lines[m][0] != ' ' && lines[m][0] != '\t' &&
                    lines[m].find('(') != std::string::npos) {
                    fn = lines[m];
                    break;
                }
            }
            if (fn.find(" make_") != std::string::npos ||
                fn.find("::emit_iter(TraceSink &") != std::string::npos ||
                fn.find("::emit_solve_end(") != std::string::npos) {
                continue;
            }
        }
        bool allowed = false;
        for (const Allowed &a : allowlist()) {
            if (line.find(a.needle) != std::string::npos) {
                allowed = true;
            }
        }
        if (allowed) {
            continue;
        }
        // A guard on the same line (`if (x != nullptr) emit...`) counts.
        bool guarded = guard_opener(line);
        // An early-return guard shortly above in the same function.
        for (std::size_t m = n; m-- > 0 && m + 30 > n && !guarded;) {
            if (!lines[m].empty() && lines[m][0] != ' ' && lines[m][0] != '\t') {
                break;
            }
            if (lines[m].find("== nullptr") != std::string::npos && m + 1 < lines.size() &&
                lines[m + 1].find("return") != std::string::npos) {
                guarded = true;
            }
        }
        // Walk outward through the enclosing blocks.
        int depth = 0;
        for (std::size_t m = n + 1; m-- > 0 && !guarded;) {
            const std::string &l = lines[m];
            for (std::size_t i = l.size(); i-- > 0;) {
                if (m == n && i >= l.size()) {
                    continue;
                }
                if (l[i] == '}') {
                    ++depth;
                } else if (l[i] == '{') {
                    if (depth == 0) {
                        guarded =
                            guard_opener(l) || (m > 0 && guard_opener(lines[m - 1]) &&
                                                l.find_first_not_of(" \t{") == std::string::npos);
                        // Keep walking outward only if this opener is not itself a guard.
                        if (!guarded) {
                            depth = 0;
                        }
                        break;
                    }
                    --depth;
                }
            }
        }
        if (!guarded) {
            violations.push_back(file + ":" + std::to_string(n + 1) + ": " + line);
        }
    }
    return violations;
}

std::string read(const std::filesystem::path &p) {
    std::ifstream in(p);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

TEST(TraceOffPath, EveryEventConstructionAndEmitCallIsInsideASinkGuardOrABuilder) {
    const std::filesystem::path root(HVEN_SRC_ROOT);
    std::vector<std::filesystem::path> files;
    for (const char *dir : {"ipm", "sqp", "solver", "qp", "warmstart"}) {
        for (const auto &e : std::filesystem::recursive_directory_iterator(root / dir)) {
            if (e.is_regular_file() && e.path().extension() == ".cpp") {
                files.push_back(e.path());
            }
        }
    }
    ASSERT_FALSE(files.empty());
    std::size_t sites = 0;
    std::vector<std::string> all;
    for (const auto &f : files) {
        const std::string text = read(f);
        // The sinks themselves (writer, console, defaults) are the consumers: policed by being
        // consumers.
        const std::string stem = f.stem().string();
        if (stem == "trace_writer" || stem == "console_trace_sink" || stem == "trace") {
            continue;
        }
        sites += static_cast<std::size_t>(std::distance(
            std::sregex_iterator(text.begin(), text.end(), site_regex()), std::sregex_iterator()));
        for (const std::string &v : scan(text, f.filename().string())) {
            all.push_back(v);
        }
    }
    // Non-vacuous AND exact: the scan found precisely the emit sites the pre-restructure scan found
    // (130 over 23 files, measured at RS-0 by an independent run of this same regex). The
    // restructure moves files between the scanned directories; it must neither lose a file from the
    // scan nor gain one. A new emit site is acknowledged by changing this number in the same
    // commit.
    EXPECT_EQ(sites, 130U) << "the scan found the engines' emit sites";
    for (const std::string &v : all) {
        ADD_FAILURE() << "unguarded trace site: " << v;
    }
}

TEST(TraceOffPath, ThePlantedUnguardedConstructionAndCallAreCaught) {
    const std::string construction =
        "void f() {\n    MessageTraceEvent msg;\n    msg.kind = x;\n}\n";
    EXPECT_EQ(scan(construction, "planted").size(), 1U);
    const std::string call = "void g() {\n    emit_trace_iter(row, 1, mode, 2);\n}\n";
    EXPECT_EQ(scan(call, "planted").size(), 1U);
    const std::string unguarded_builder_call =
        "void h() {\n    auto e = build();\n    sink->on_message(e);\n}\n";
    EXPECT_EQ(scan(unguarded_builder_call, "planted").size(), 1U);
}

TEST(TraceOffPath, TheGuardedFormsPass) {
    const std::string guarded =
        "void f() {\n    if (trace_ != nullptr) {\n        MessageTraceEvent msg;\n        "
        "sink->on_message(msg);\n    }\n}\n";
    EXPECT_TRUE(scan(guarded, "ok").empty());
    const std::string builder = "void g() {\n    this->emit_message([&] {\n        "
                                "MessageTraceEvent msg;\n        return msg;\n    });\n}\n";
    EXPECT_TRUE(scan(builder, "ok").empty());
    const std::string nested = "void h() {\n    if (TraceSink *const sink = this->trace_; sink != "
                               "nullptr) {\n        for (;;) {\n            "
                               "this->emit_iter(*sink, row);\n        }\n    }\n}\n";
    EXPECT_TRUE(scan(nested, "ok").empty());
}

// The builder contract itself: `emit_message` must call its builder only after the sink test.
TEST(TraceOffPath, TheMessageBuilderRunsOnlyAfterTheSinkTest) {
    const std::filesystem::path root(HVEN_SRC_ROOT);
    const std::string text = strip(read(root / "ipm" / "ipm_solver.cpp"));
    const std::size_t def = text.find("IpmSolverImpl::emit_message(Build &&build)");
    ASSERT_NE(def, std::string::npos);
    const std::size_t guard = text.find("!= nullptr", def);
    const std::size_t call = text.find("build()", def);
    ASSERT_NE(guard, std::string::npos);
    ASSERT_NE(call, std::string::npos);
    EXPECT_LT(guard, call) << "the builder is invoked before the sink is tested";
}

// The non-finite abort block adds the gradient block to the retained right-hand side exactly
// once (a second accumulation corrupted the retained exit gradient with no sink attached).
TEST(TraceOffPath, TheNonFiniteAbortBlockAccumulatesTheGradientExactlyOnce) {
    const std::filesystem::path root(HVEN_SRC_ROOT);
    const std::string text = strip(read(root / "ipm" / "ipm_solver.cpp"));
    const std::size_t open = text.find("if (nonfinite_abort) [[unlikely]] {");
    ASSERT_NE(open, std::string::npos);
    const std::size_t close = text.find("ExitCode = SolveStatus::kNumericalError;", open);
    ASSERT_NE(close, std::string::npos);
    const std::string block = text.substr(open, close - open);
    std::size_t n = 0;
    for (std::size_t at = block.find("prim_grad() += PGX"); at != std::string::npos;
         at = block.find("prim_grad() += PGX", at + 1)) {
        ++n;
    }
    EXPECT_EQ(n, 1U);
}

} // namespace
