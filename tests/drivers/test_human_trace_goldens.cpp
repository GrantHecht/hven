// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_human_trace_goldens.cpp -- M8 TR-A: the human trace rendered from REAL solves.
//
// Four Hock-Schittkowski problems x both engines x the four rungs that print (kWarnings, kSummary,
// kIterations, kDetail), plus a restoration fixture and an elastic fixture, with
// `Format{color: kNever, timing: false}`. Each transcript is compared to
// tests/drivers/golden/human_trace/<engine>_<case>_<rung>.txt.
//
// Regenerate (a DECLARED event, with the console-bytes event of TR-A):
//   HVEN_REGEN_GOLDENS=1 ./hven_drivers_tests --gtest_filter='HumanTraceGoldens.*'
// and read the diff. Wall-clock never appears: `timing: false`, and the `solver_initialized`
// notice is suppressed by warming the process first.

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <regex>
#include <sstream>
#include <string>

#include <hven/drivers/console_trace_sink.h>
#include <hven/ipm/ipm_solver.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/model/problem.h>

#include "support/trace_battery.h"

#ifndef HVEN_GOLDEN_DIR
#error "HVEN_GOLDEN_DIR must be defined by tests/drivers/CMakeLists.txt"
#endif

namespace {

using namespace hven;
using namespace hven_drivers_tests;

// c(x) = x0^2 + x1^2 + 1 = 0: no real solution and a strict stationary point of its violation at
// the origin -- the engines enter feasibility restoration, and (on the SQP, where the linearization
// at the origin is inconsistent) the elastic tier.
struct LocallyInfeasible final : hven::Nlp {
    hven::NlpDimensions dimensions() const override { return {2, 1, 2, 2}; }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        const double inf = std::numeric_limits<double>::infinity();
        xl << -inf, -inf;
        xu << inf, inf;
        gl << 0.0;
        gu << 0.0;
    }
    double eval_f(hven::ConstVecRef x) const override { return x[0] + x[1]; }
    void eval_grad_f(hven::ConstVecRef, hven::VecRef g) const override { g << 1.0, 1.0; }
    void eval_g(hven::ConstVecRef x, hven::VecRef g) const override {
        g[0] = x[0] * x[0] + x[1] * x[1] + 1.0;
    }
    void jacobian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_jacobian(hven::ConstVecRef x, hven::VecRef v) const override {
        v << 2.0 * x[0], 2.0 * x[1];
    }
    void eval_hessian(hven::ConstVecRef, double, hven::ConstVecRef lambda,
                      hven::VecRef v) const override {
        v << 2.0 * lambda[0], 2.0 * lambda[0];
    }
    std::string name() const override { return "LocallyInfeasible"; }
};

struct Rung {
    const char *name;
    Verbosity verbosity;
};
constexpr Rung kRungs[] = {{"warnings", Verbosity::kWarnings},
                           {"summary", Verbosity::kSummary},
                           {"iterations", Verbosity::kIterations},
                           {"detail", Verbosity::kDetail}};

std::string render(bool ipm, const std::shared_ptr<hven::Nlp> &nlp, const Vec &x0, Verbosity v,
                   const IpmOptions &ipm_options = IpmOptions{}) {
    std::FILE *f = std::tmpfile();
    std::string text;
    {
        ConsoleTraceSink sink(
            ConsoleTraceSink::Format{
                .verbosity = v, .color = ConsoleColor::kNever, .legend = true, .timing = false},
            f);
        Problem problem(nlp);
        if (ipm) {
            IpmSolver solver{ipm_options};
            solver.attach_trace(&sink);
            (void)solver.solve(problem, x0);
        } else {
            SqpSolver solver{SqpOptions{}};
            solver.attach_trace(&sink);
            (void)solver.solve(problem, x0);
        }
    }
    std::fflush(f);
    std::rewind(f);
    char buf[4096];
    std::size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
        text.append(buf, n);
    }
    std::fclose(f);
    return text;
}

// THE NUMERIC GATE. A golden pins the transcript's structure (every word, column, mark and
// integer, exactly) and its numbers to a few significant digits: two builds of one source (Release
// and Debug, FMA contraction, the platform's libm) differ in the last digits of a residual, and a
// residual at roundoff (1e-13 and below) differs in its leading digits -- the CLAUDE.md section 7
// near-ulp rule, applied to text. So each scientific-notation token is compared by value:
// equal within 2 % relative, or both below 1e-9 in magnitude; everything else must match byte
// for byte. Counters are integers and are never numeric tokens, so they stay exact.
bool same_transcript(const std::string &want, const std::string &got) {
    static const std::regex number(R"([-+]?[0-9]\.[0-9]+e[-+][0-9]+)");
    const auto skeleton = [](const std::string &s) { return std::regex_replace(s, number, "#"); };
    if (skeleton(want) != skeleton(got)) {
        return false;
    }
    std::sregex_iterator a(want.begin(), want.end(), number), b(got.begin(), got.end(), number);
    for (const std::sregex_iterator end; a != end && b != end; ++a, ++b) {
        const double x = std::stod(a->str());
        const double y = std::stod(b->str());
        const bool both_noise = std::abs(x) < 1e-9 && std::abs(y) < 1e-9;
        if (!both_noise && std::abs(x - y) > 0.02 * std::max(std::abs(x), std::abs(y))) {
            return false;
        }
    }
    return true;
}

void check(const std::string &file, const std::string &text) {
    const std::filesystem::path path = std::filesystem::path(HVEN_GOLDEN_DIR) / file;
    if (std::getenv("HVEN_REGEN_GOLDENS") != nullptr) {
        std::ofstream(path) << text;
        return;
    }
    std::ifstream in(path);
    ASSERT_TRUE(in.good()) << "missing golden " << path
                           << " (regenerate with HVEN_REGEN_GOLDENS=1)";
    std::stringstream ss;
    ss << in.rdbuf();
    if (!same_transcript(ss.str(), text)) {
        EXPECT_EQ(ss.str(), text) << file;
    }
}

TEST(HumanTraceGoldens, TheNumericGateAcceptsRoundoffAndRefusesRealDrift) {
    const std::string want = "row 3.72e-13 1.13e+00 7 iters 1.0e-12\n";
    EXPECT_TRUE(same_transcript(want, "row 3.71e-13 1.12e+00 7 iters 1.0e-12\n"));
    EXPECT_TRUE(same_transcript(want, "row 6.24e-15 1.13e+00 7 iters 1.0e-12\n"));
    EXPECT_FALSE(same_transcript(want, "row 3.72e-13 1.30e+00 7 iters 1.0e-12\n")) << "value";
    EXPECT_FALSE(same_transcript(want, "row 3.72e-13 1.13e+00 8 iters 1.0e-12\n")) << "integer";
    EXPECT_FALSE(same_transcript(want, "row 3.72e-13 1.13e+00 7 steps 1.0e-12\n")) << "word";
}

TEST(HumanTraceGoldens, FourProblemsTwoEnginesFourRungs) {
    // Warm the process so the one-time `solver_initialized` notice is out of every transcript.
    (void)render(true, battery()[0].make(), battery()[0].start(), Verbosity::kSilent);
    for (const bool ipm : {true, false}) {
        for (const BatteryCase &c : battery()) {
            for (const Rung &rung : kRungs) {
                SCOPED_TRACE(std::string(ipm ? "ipm " : "sqp ") + c.name + " " + rung.name);
                check(std::string(ipm ? "ipm_" : "sqp_") + c.name + "_" + rung.name + ".txt",
                      render(ipm, c.make(), c.start(), rung.verbosity));
            }
        }
    }
}

// Rows of a tier in a transcript: `<iter> <tier> ` at the start of a line (the legend's own
// letters never start a line with a number).
bool has_tier_row(const std::string &text, char tier) {
    for (std::size_t at = 0; at < text.size();) {
        std::size_t end = text.find('\n', at);
        if (end == std::string::npos) {
            end = text.size();
        }
        const std::string line = text.substr(at, end - at);
        std::size_t i = line.find_first_not_of(' ');
        if (i != std::string::npos && std::isdigit(static_cast<unsigned char>(line[i]))) {
            const std::size_t sp = line.find(' ', i);
            if (sp != std::string::npos && sp + 2 < line.size() && line[sp + 1] == tier &&
                line[sp + 2] == ' ') {
                return true;
            }
        }
        at = end + 1;
    }
    return false;
}

TEST(HumanTraceGoldens, TheRestorationFixture) {
    (void)render(true, battery()[0].make(), battery()[0].start(), Verbosity::kSilent);
    Vec x0(2);
    x0 << 1.0, 1.0;
    // The IPM enters its nested l1 restoration phase (one entry), whose rows read tier R.
    IpmOptions restoring;
    restoring.advanced.restoration_mode = IpmRestorationMode::kL1Nested;
    restoring.advanced.max_restoration_entries = 1;
    const std::string ipm =
        render(true, std::make_shared<LocallyInfeasible>(), x0, Verbosity::kIterations, restoring);
    EXPECT_TRUE(has_tier_row(ipm, 'R')) << "the IPM fixture shows restoration rows\n" << ipm;
    check("ipm_restoration_iterations.txt", ipm);
    const std::string sqp =
        render(false, std::make_shared<LocallyInfeasible>(), x0, Verbosity::kIterations);
    EXPECT_TRUE(has_tier_row(sqp, 'R')) << "the SQP fixture shows restoration rows\n" << sqp;
    check("sqp_restoration_iterations.txt", sqp);
}

TEST(HumanTraceGoldens, TheElasticFixture) {
    (void)render(true, battery()[0].make(), battery()[0].start(), Verbosity::kSilent);
    Vec x0(2);
    x0 << 0.0, 0.0;
    // Started at the origin the linearization of c is inconsistent: the SQP's elastic tier supplies
    // the step.
    const std::string sqp =
        render(false, std::make_shared<LocallyInfeasible>(), x0, Verbosity::kDetail);
    EXPECT_TRUE(has_tier_row(sqp, 'E')) << "the SQP fixture shows elastic rows\n" << sqp;
    check("sqp_elastic_detail.txt", sqp);
}

} // namespace
