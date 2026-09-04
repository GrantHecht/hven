// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_trace_writer.cpp -- M6 W4 task 1: JsonLinesTraceSink, schema v0's
// serializer. Four pin families, in the order the brief lists them.
//
//   (i)   GOLDEN LINES, byte-exact, one per event struct (ten lines over the
//         eight events: `ipqp.iter` twice for present/absent, `fallback.verdict`
//         twice for a present/absent `rho_0`).
//
//   (ii)  ROUND TRIP through a minimal reader that ASSUMES this writer's exact
//         layout -- it is a pin, not a parser -- with every double compared
//         BIT-EQUAL, so "17 significant digits" is a fact and not a claim.
//
//   (iii) WHOLE SOLVE: the stream's line count by `ev` equals the currency the
//         solve reported, on real HS cells at kIpm.
//
//   (iv)  NULL SINK: attaching the writer to a walk-arm solve moves no counter
//         and writes no line.

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ios>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <streambuf>
#include <string>
#include <vector>

#include <Eigen/SparseCore>
#include <gtest/gtest.h>

#include <hven/detail/globalization/sqp/elastic.h>
#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/trace_writer.h>
#include <hven/model/nlp_model.h>

#include "support/hs_problems.h"

namespace hven::solvers {
namespace {

using test_support::HsProblem;
using test_support::make_hs;

// ===========================================================================
// The minimal reader (pin (ii)'s own instrument)
// ===========================================================================

/// @brief The raw token a key maps to, with the writer's layout assumed.
///
/// Nesting and strings are tracked only so that `evidence`, `inertia` and a
/// `facts` carrying a comma come back whole. Returns an empty string when the
/// key is absent, which every caller asserts against.
std::string raw_field(const std::string &line, const std::string &k) {
    const std::string pat = "\"" + k + "\":";
    const std::size_t p = line.find(pat);
    if (p == std::string::npos) {
        return {};
    }
    const std::size_t begin = p + pat.size();
    std::size_t e = begin;
    int nest = 0;
    bool in_str = false;
    bool esc = false;
    for (; e < line.size(); ++e) {
        const char c = line[e];
        if (in_str) {
            if (esc) {
                esc = false;
            } else if (c == '\\') {
                esc = true;
            } else if (c == '"') {
                in_str = false;
            }
            continue;
        }
        if (c == '"') {
            in_str = true;
        } else if (c == '[' || c == '{') {
            ++nest;
        } else if (c == ']' || c == '}') {
            if (nest == 0) {
                break;
            }
            --nest;
        } else if (c == ',' && nest == 0) {
            break;
        }
    }
    return line.substr(begin, e - begin);
}

/// @brief Rule 3 in reverse: the three non-finite JSON STRINGS, else `strtod`.
double read_double(const std::string &token) {
    if (token == "\"nan\"") {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (token == "\"inf\"") {
        return std::numeric_limits<double>::infinity();
    }
    if (token == "\"-inf\"") {
        return -std::numeric_limits<double>::infinity();
    }
    return std::strtod(token.c_str(), nullptr);
}

/// @brief BIT equality, which is the whole point of pin (ii): `==` would call
/// `-0.0` and `0.0` equal, and those are exactly the two the round trip can
/// confuse.
void expect_double_field(const std::string &line, const std::string &k, double expected) {
    const std::string token = raw_field(line, k);
    ASSERT_FALSE(token.empty()) << "no key \"" << k << "\" in: " << line;
    const double got = read_double(token);
    if (std::isnan(expected)) {
        EXPECT_TRUE(std::isnan(got)) << k << " = " << token;
        return;
    }
    EXPECT_EQ(std::bit_cast<std::uint64_t>(got), std::bit_cast<std::uint64_t>(expected))
        << k << " = " << token;
}

std::vector<std::string> split_lines(const std::string &s) {
    std::vector<std::string> out;
    std::istringstream is(s);
    std::string l;
    while (std::getline(is, l)) {
        out.push_back(l);
    }
    return out;
}

/// @brief The `ev` of one line, unquoted.
std::string event_name(const std::string &line) {
    const std::string token = raw_field(line, "ev");
    return (token.size() >= 2) ? token.substr(1, token.size() - 2) : std::string{};
}

// ===========================================================================
// Hand-filled events -- the golden lines' own inputs
// ===========================================================================

IpqpTraceIterEvent golden_iter_present() {
    IpqpTraceIterEvent e;
    e.solve = 3;
    e.major = 7;
    e.it = 2;
    e.mu = 0.25;
    e.rho = 1e-8;
    e.delta = 8.0;
    e.res_p = 0.5;
    e.res_d = 0.125;
    e.res_c = 0.1;
    e.sigma = 0.75;
    e.alpha_p = 0.9;
    e.alpha_d = 1.0;
    e.inertia = std::array<Index, 3>{5, 2, 0};
    e.zero_derived = true;
    e.perturbed = 4;
    // Rule 6's three mandatory escapes in one string: a quote, a backslash, a
    // control character. Nothing else in the schema is caller text.
    e.facts = "a\"b\\c\nd";
    return e;
}

IpqpTraceIterEvent golden_iter_absent() {
    IpqpTraceIterEvent e;
    e.solve = 1;
    e.major = 0;
    e.it = 1;
    e.mu = -0.0;
    e.res_p = std::numeric_limits<double>::quiet_NaN();
    e.res_d = std::numeric_limits<double>::infinity();
    e.res_c = -std::numeric_limits<double>::infinity();
    return e;
}

IpqpTraceEscapeEvent golden_escape() {
    IpqpTraceEscapeEvent e;
    e.reason = IpqpTraceEscapeReason::kInfeasibleSuspect;
    e.evidence.stall.fired = false;
    e.evidence.stall.window = 0;
    e.evidence.stall.mu_ratio = std::numeric_limits<double>::infinity();
    e.evidence.stall.residual_improvement = -0.5;
    e.evidence.stall.min_alpha = 0.25;
    e.evidence.stall.max_step_alpha = 0.75;
    IpqpInfeasibilityEvidence &i = e.evidence.infeasibility;
    i.fired = true;
    i.exhaustion_route = false;
    i.window = 5;
    i.primal_start = 2.0;
    i.primal_end = 1.5;
    i.primal_improvement = 0.25;
    i.dual_norm_start = 10.0;
    i.dual_norm_end = 1e6;
    i.dual_growth = 1e5;
    i.dual_step_growth = 0.0;
    i.farkas_corroborated = true;
    i.farkas_residual = 1e-8;
    i.farkas_gap = -3.5;
    i.least_infeasible_primal = 1.25;
    i.least_infeasible_mu = 0.001;
    // NON-VACUITY for the no-vectors rule: the five Vec members are POPULATED
    // here, so the golden line below fails if any of them ever reaches the
    // stream (plan section 7 amendment G).
    i.least_infeasible_x = Vec::Constant(3, 7.0);
    i.least_infeasible_s = Vec::Constant(2, 8.0);
    i.least_infeasible_lambda_i = Vec::Constant(2, 9.0);
    i.least_infeasible_zl = Vec::Constant(3, 1.0);
    i.least_infeasible_zu = Vec::Constant(3, 2.0);
    return e;
}

// ===========================================================================
// (i) GOLDEN LINES -- byte-exact
// ===========================================================================

TEST(JsonLinesTraceSink, GoldenLineIpqpIterWithInertiaPerturbedAndAnEscapedFactsString) {
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipqp_iter(golden_iter_present());
    EXPECT_EQ(os.str(),
              "{\"v\":0,\"ev\":\"ipqp.iter\",\"seq\":1,\"depth\":0,\"solve\":3,\"major\":7,"
              "\"it\":2,\"mu\":0.25,\"rho\":1e-08,\"delta\":8,\"res_p\":0.5,\"res_d\":0.125,"
              "\"res_c\":0.10000000000000001,\"sigma\":0.75,\"alpha_p\":0.90000000000000002,"
              "\"alpha_d\":1,\"inertia\":[5,2,0],\"zero_derived\":true,\"perturbed\":4,"
              "\"facts\":\"a\\\"b\\\\c\\nd\"}\n");
    EXPECT_EQ(sink.lines_written(), 1);
    EXPECT_EQ(sink.depth(), 0);
}

TEST(JsonLinesTraceSink, GoldenLineIpqpIterWithBothOptionalsAbsentAndTheThreeNonFiniteDoubles) {
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipqp_iter(golden_iter_absent());
    EXPECT_EQ(os.str(),
              "{\"v\":0,\"ev\":\"ipqp.iter\",\"seq\":1,\"depth\":0,\"solve\":1,\"major\":0,"
              "\"it\":1,\"mu\":-0,\"rho\":0,\"delta\":0,\"res_p\":\"nan\",\"res_d\":\"inf\","
              "\"res_c\":\"-inf\",\"sigma\":0,\"alpha_p\":0,\"alpha_d\":0,\"inertia\":null,"
              "\"zero_derived\":false,\"perturbed\":null,\"facts\":\"\"}\n");
}

TEST(JsonLinesTraceSink, GoldenLineIpqpReg) {
    IpqpTraceRegEvent e;
    e.dir = IpqpTraceRegDir::kDown;
    e.rho = 100.0;
    e.delta = 0.001;
    e.reason = IpqpTraceRegReason::kStall;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipqp_reg(e);
    EXPECT_EQ(os.str(), "{\"v\":0,\"ev\":\"ipqp.reg\",\"seq\":1,\"depth\":0,\"dir\":\"down\","
                        "\"rho\":100,\"delta\":0.001,\"reason\":\"stall\"}\n");
}

TEST(JsonLinesTraceSink, GoldenLineIpqpRestart) {
    IpqpTraceRestartEvent e;
    e.grade = IpqpTraceRestartGrade::kFull;
    e.repaired = true;
    e.shift_p = 0.25;
    e.shift_d = 0.125;
    e.mu0 = 0.1;
    e.mu_payload = 1e-8;
    e.adopted = true;
    e.abandoned = false;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipqp_restart(e);
    EXPECT_EQ(os.str(), "{\"v\":0,\"ev\":\"ipqp.restart\",\"seq\":1,\"depth\":0,\"grade\":\"full\","
                        "\"repaired\":true,\"shift_p\":0.25,\"shift_d\":0.125,"
                        "\"mu0\":0.10000000000000001,\"mu_payload\":1e-08,\"adopted\":true,"
                        "\"abandoned\":false}\n");
}

TEST(JsonLinesTraceSink, GoldenLineIpqpRoute) {
    IpqpTraceRouteEvent e;
    e.to = IpqpTraceRouteTo::kSsn;
    e.uncertain = 3;
    e.face_rows = 4;
    e.face_bounds = 2;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipqp_route(e);
    EXPECT_EQ(os.str(), "{\"v\":0,\"ev\":\"ipqp.route\",\"seq\":1,\"depth\":0,\"to\":\"ssn\","
                        "\"uncertain\":3,\"face_rows\":4,\"face_bounds\":2}\n");
}

TEST(JsonLinesTraceSink, GoldenLineIpqpCertify) {
    IpqpTraceCertifyEvent e;
    e.final_inertia = IpqpTraceFinalInertia::kWrong;
    e.downgraded = true;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipqp_certify(e);
    EXPECT_EQ(os.str(), "{\"v\":0,\"ev\":\"ipqp.certify\",\"seq\":1,\"depth\":0,"
                        "\"final_inertia\":\"wrong\",\"downgraded\":true}\n");
}

TEST(JsonLinesTraceSink, GoldenLineIpqpEscapeCarriesBothTypedEvidenceBlocksAndNoVector) {
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipqp_escape(golden_escape());
    EXPECT_EQ(
        os.str(),
        "{\"v\":0,\"ev\":\"ipqp.escape\",\"seq\":1,\"depth\":0,\"reason\":\"infeasible_suspect\","
        "\"evidence\":{\"stall\":{\"fired\":false,\"window\":0,\"mu_ratio\":\"inf\","
        "\"residual_improvement\":-0.5,\"min_alpha\":0.25,\"max_step_alpha\":0.75},"
        "\"infeasibility\":{\"fired\":true,\"exhaustion_route\":false,\"window\":5,"
        "\"primal_start\":2,\"primal_end\":1.5,\"primal_improvement\":0.25,"
        "\"dual_norm_start\":10,\"dual_norm_end\":1000000,\"dual_growth\":100000,"
        "\"dual_step_growth\":0,\"farkas_corroborated\":true,\"farkas_residual\":1e-08,"
        "\"farkas_gap\":-3.5,\"least_infeasible_primal\":1.25,"
        "\"least_infeasible_mu\":0.001}}}\n");
}

TEST(JsonLinesTraceSink, GoldenLineQpMode) {
    QpModeTraceEvent e;
    e.mode = IpqpTraceQpMode::kIpqp;
    e.outcome = IpqpTraceOutcome::kEscaped;
    e.iters = 12;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_qp_mode(e);
    EXPECT_EQ(os.str(), "{\"v\":0,\"ev\":\"qp.mode\",\"seq\":1,\"depth\":0,\"mode\":\"ipqp\","
                        "\"outcome\":\"escaped\",\"facts\":\"\",\"iters\":12}\n");
}

TEST(JsonLinesTraceSink, GoldenLineFallbackVerdictWithAPlacement) {
    SqpFallbackVerdictTraceEvent e;
    e.entered_rung_a = true;
    e.verdict = SqpFallbackVerdict::kRungB;
    e.rho_0 = 1e6;
    e.rho0_ceiling_hit = true;
    e.floor_retry = true;
    e.qp_minor_iters = 7;
    e.qp_factorizations = 3;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_fallback_verdict(e);
    EXPECT_EQ(os.str(), "{\"v\":0,\"ev\":\"fallback.verdict\",\"seq\":1,\"depth\":0,"
                        "\"entered_rung_a\":true,\"verdict\":\"rung_b\",\"rho_0\":1000000,"
                        "\"rho0_ceiling_hit\":true,\"floor_retry\":true,\"qp_minor_iters\":7,"
                        "\"qp_factorizations\":3}\n");
}

TEST(JsonLinesTraceSink, GoldenLineFallbackVerdictUnfiredWritesRhoZeroAsNull) {
    // CLAUDE.md section 6 at its sharpest: an entry with no rung A HAS no
    // placement, and a `0` here would read as one placed at the origin.
    SqpFallbackVerdictTraceEvent e;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_fallback_verdict(e);
    EXPECT_EQ(os.str(), "{\"v\":0,\"ev\":\"fallback.verdict\",\"seq\":1,\"depth\":0,"
                        "\"entered_rung_a\":false,\"verdict\":\"unfired\",\"rho_0\":null,"
                        "\"rho0_ceiling_hit\":false,\"floor_retry\":false,\"qp_minor_iters\":0,"
                        "\"qp_factorizations\":0}\n");
}

// ===========================================================================
// R1(c) -- THE COMPILE-TIME NET the production TU cannot provide
// ===========================================================================
//
// `-Wswitch` in `src/drivers/trace_writer.cpp` only WARNS: this tree carries no
// `-Werror`, so an enumerator added without a spelling ships a `"unknown"`.
//
// Here it is an ERROR, over one exhaustive switch per enum. An added enumerator
// therefore fails to COMPILE this file, and the spellings below are the ORACLE
// the sink is checked against -- an independent copy, in the test.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic error "-Wswitch"
#endif

const char *spec_spelling(IpqpTraceRegDir v) {
    switch (v) {
    case IpqpTraceRegDir::kDown:
        return "\"down\"";
    case IpqpTraceRegDir::kUp:
        return "\"up\"";
    }
    return nullptr;
}

const char *spec_spelling(IpqpTraceRegReason v) {
    switch (v) {
    case IpqpTraceRegReason::kAccept:
        return "\"accept\"";
    case IpqpTraceRegReason::kInertia:
        return "\"inertia\"";
    case IpqpTraceRegReason::kStall:
        return "\"stall\"";
    case IpqpTraceRegReason::kFloor:
        return "\"floor\"";
    }
    return nullptr;
}

const char *spec_spelling(IpqpTraceRestartGrade v) {
    switch (v) {
    case IpqpTraceRestartGrade::kCold:
        return "\"cold\"";
    case IpqpTraceRestartGrade::kBase:
        return "\"base\"";
    case IpqpTraceRestartGrade::kFull:
        return "\"full\"";
    }
    return nullptr;
}

const char *spec_spelling(IpqpTraceRouteTo v) {
    switch (v) {
    case IpqpTraceRouteTo::kRefine:
        return "\"refine\"";
    case IpqpTraceRouteTo::kSsn:
        return "\"ssn\"";
    case IpqpTraceRouteTo::kWalk:
        return "\"walk\"";
    }
    return nullptr;
}

const char *spec_spelling(IpqpTraceFinalInertia v) {
    switch (v) {
    case IpqpTraceFinalInertia::kOk:
        return "\"ok\"";
    case IpqpTraceFinalInertia::kWrong:
        return "\"wrong\"";
    case IpqpTraceFinalInertia::kUnreadable:
        return "\"unreadable\"";
    }
    return nullptr;
}

const char *spec_spelling(IpqpTraceEscapeReason v) {
    switch (v) {
    case IpqpTraceEscapeReason::kBudget:
        return "\"budget\"";
    case IpqpTraceEscapeReason::kStall:
        return "\"stall\"";
    case IpqpTraceEscapeReason::kIndefinite:
        return "\"indefinite\"";
    case IpqpTraceEscapeReason::kNumerical:
        return "\"numerical\"";
    case IpqpTraceEscapeReason::kInfeasibleSuspect:
        return "\"infeasible_suspect\"";
    }
    return nullptr;
}

const char *spec_spelling(IpqpTraceQpMode v) {
    switch (v) {
    case IpqpTraceQpMode::kIpqp:
        return "\"ipqp\"";
    }
    return nullptr;
}

const char *spec_spelling(IpqpTraceOutcome v) {
    switch (v) {
    case IpqpTraceOutcome::kOptimal:
        return "\"optimal\"";
    case IpqpTraceOutcome::kRouted:
        return "\"routed\"";
    case IpqpTraceOutcome::kEscaped:
        return "\"escaped\"";
    }
    return nullptr;
}

const char *spec_spelling(SqpFallbackVerdict v) {
    switch (v) {
    case SqpFallbackVerdict::kDisproved:
        return "\"disproved\"";
    case SqpFallbackVerdict::kRelaxed:
        return "\"relaxed\"";
    case SqpFallbackVerdict::kExhausted:
        return "\"exhausted\"";
    case SqpFallbackVerdict::kRungB:
        return "\"rung_b\"";
    case SqpFallbackVerdict::kUnfired:
        return "\"unfired\"";
    }
    return nullptr;
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

TEST(JsonLinesTraceSink, EveryEnumeratorHasItsSpecSpelling) {
    // The nine enums' 27 values, read off one line each and compared against
    // `spec_spelling` above -- the spec's strings in an independent copy, under
    // a `-Wswitch`-as-ERROR region (R1(c)).
    //
    // The `static_assert`s are the third net: they pin each enum's hand-stated
    // enumerator COUNT through its last value, so an enumerator INSERTED among
    // the existing ones (which the switch would still cover) also fails.
    //
    // BUMP THE COUNT AND ADD A CASE ABOVE WHEN YOU ADD A SPELLING.
    static_assert(static_cast<int>(IpqpTraceRegDir::kUp) == 2 - 1, "2 dir spellings");
    static_assert(static_cast<int>(IpqpTraceRegReason::kFloor) == 4 - 1, "4 reg reasons");
    static_assert(static_cast<int>(IpqpTraceRestartGrade::kFull) == 3 - 1, "3 grades");
    static_assert(static_cast<int>(IpqpTraceRouteTo::kWalk) == 3 - 1, "3 route destinations");
    static_assert(static_cast<int>(IpqpTraceFinalInertia::kUnreadable) == 3 - 1, "3 readings");
    static_assert(static_cast<int>(IpqpTraceEscapeReason::kInfeasibleSuspect) == 5 - 1,
                  "5 escape reasons");
    static_assert(static_cast<int>(IpqpTraceQpMode::kIpqp) == 1 - 1, "1 QP mode (T2 adds two)");
    static_assert(static_cast<int>(IpqpTraceOutcome::kEscaped) == 3 - 1, "3 outcomes");
    static_assert(static_cast<int>(SqpFallbackVerdict::kUnfired) == 5 - 1, "5 verdicts");

    const auto reg_dir = [](IpqpTraceRegDir d) {
        IpqpTraceRegEvent e;
        e.dir = d;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipqp_reg(e);
        EXPECT_EQ(raw_field(os.str(), "dir"), spec_spelling(d));
    };
    reg_dir(IpqpTraceRegDir::kDown);
    reg_dir(IpqpTraceRegDir::kUp);

    const auto reg_reason = [](IpqpTraceRegReason r) {
        IpqpTraceRegEvent e;
        e.reason = r;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipqp_reg(e);
        EXPECT_EQ(raw_field(os.str(), "reason"), spec_spelling(r));
    };
    reg_reason(IpqpTraceRegReason::kAccept);
    reg_reason(IpqpTraceRegReason::kInertia);
    reg_reason(IpqpTraceRegReason::kStall);
    reg_reason(IpqpTraceRegReason::kFloor);

    const auto grade = [](IpqpTraceRestartGrade g) {
        IpqpTraceRestartEvent e;
        e.grade = g;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipqp_restart(e);
        EXPECT_EQ(raw_field(os.str(), "grade"), spec_spelling(g));
    };
    grade(IpqpTraceRestartGrade::kFull);
    grade(IpqpTraceRestartGrade::kBase);
    grade(IpqpTraceRestartGrade::kCold);

    const auto route = [](IpqpTraceRouteTo t) {
        IpqpTraceRouteEvent e;
        e.to = t;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipqp_route(e);
        EXPECT_EQ(raw_field(os.str(), "to"), spec_spelling(t));
    };
    route(IpqpTraceRouteTo::kRefine);
    route(IpqpTraceRouteTo::kSsn);
    route(IpqpTraceRouteTo::kWalk);

    const auto certify = [](IpqpTraceFinalInertia f) {
        IpqpTraceCertifyEvent e;
        e.final_inertia = f;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipqp_certify(e);
        EXPECT_EQ(raw_field(os.str(), "final_inertia"), spec_spelling(f));
    };
    certify(IpqpTraceFinalInertia::kOk);
    certify(IpqpTraceFinalInertia::kWrong);
    certify(IpqpTraceFinalInertia::kUnreadable);

    const auto escape = [](IpqpTraceEscapeReason r) {
        IpqpTraceEscapeEvent e;
        e.reason = r;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipqp_escape(e);
        EXPECT_EQ(raw_field(os.str(), "reason"), spec_spelling(r));
    };
    escape(IpqpTraceEscapeReason::kBudget);
    escape(IpqpTraceEscapeReason::kStall);
    escape(IpqpTraceEscapeReason::kIndefinite);
    escape(IpqpTraceEscapeReason::kNumerical);
    escape(IpqpTraceEscapeReason::kInfeasibleSuspect);

    const auto outcome = [](IpqpTraceOutcome o) {
        QpModeTraceEvent e;
        e.outcome = o;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_qp_mode(e);
        EXPECT_EQ(raw_field(os.str(), "outcome"), spec_spelling(o));
        EXPECT_EQ(raw_field(os.str(), "mode"), spec_spelling(e.mode));
    };
    outcome(IpqpTraceOutcome::kOptimal);
    outcome(IpqpTraceOutcome::kRouted);
    outcome(IpqpTraceOutcome::kEscaped);

    const auto verdict = [](SqpFallbackVerdict v) {
        SqpFallbackVerdictTraceEvent e;
        e.verdict = v;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_fallback_verdict(e);
        EXPECT_EQ(raw_field(os.str(), "verdict"), spec_spelling(v));
    };
    verdict(SqpFallbackVerdict::kDisproved);
    verdict(SqpFallbackVerdict::kRelaxed);
    verdict(SqpFallbackVerdict::kExhausted);
    verdict(SqpFallbackVerdict::kRungB);
    verdict(SqpFallbackVerdict::kUnfired);
}

TEST(JsonLinesTraceSink, StringEscapingIsRfc8259AndUtf8PassesThrough) {
    IpqpTraceIterEvent e;
    e.facts = std::string("\x01\x1f") + "\b\f\r\t" + "\xc3\xa9" + "ok";
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipqp_iter(e);
    EXPECT_EQ(raw_field(os.str(), "facts"), "\"\\u0001\\u001f\\b\\f\\r\\t\xc3\xa9ok\"");
}

TEST(JsonLinesTraceSink, SeqCountsFromOneAndDepthHooksMoveTheEnvelopeSaturatingAtZero) {
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    EXPECT_EQ(sink.depth(), 0);
    sink.on_ipqp_certify(IpqpTraceCertifyEvent{});
    sink.push_depth();
    sink.on_ipqp_certify(IpqpTraceCertifyEvent{});
    sink.pop_depth();
    // T2 wires the calls; T1 pins that an unbalanced pop cannot fabricate a
    // negative depth in the artifact.
    sink.pop_depth();
    sink.on_ipqp_certify(IpqpTraceCertifyEvent{});
    const std::vector<std::string> lines = split_lines(os.str());
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(raw_field(lines[0], "seq"), "1");
    EXPECT_EQ(raw_field(lines[0], "depth"), "0");
    EXPECT_EQ(raw_field(lines[1], "seq"), "2");
    EXPECT_EQ(raw_field(lines[1], "depth"), "1");
    EXPECT_EQ(raw_field(lines[2], "seq"), "3");
    EXPECT_EQ(raw_field(lines[2], "depth"), "0");
    EXPECT_EQ(sink.lines_written(), 3);
}

// ===========================================================================
// (ii) ROUND TRIP -- every double bit-equal
// ===========================================================================

TEST(JsonLinesTraceSink, RoundTripIsBitExactOnTheHardDoubles) {
    // The five the brief names, plus a NaN and both infinities, carried on one
    // event so a single line covers the whole numeric contract.
    IpqpTraceIterEvent e;
    e.mu = -0.0;
    e.rho = 4.9406564584124654e-324;
    e.delta = 0.1;
    e.res_p = 1e-300;
    e.res_d = 1.7976931348623157e308;
    e.res_c = -1.7976931348623157e308;
    e.sigma = std::numeric_limits<double>::quiet_NaN();
    e.alpha_p = std::numeric_limits<double>::infinity();
    e.alpha_d = -std::numeric_limits<double>::infinity();
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipqp_iter(e);
    const std::string line = os.str();

    EXPECT_EQ(raw_field(line, "mu"), "-0") << "the sign of zero survives, per plan rule 3";
    expect_double_field(line, "mu", e.mu);
    expect_double_field(line, "rho", e.rho);
    expect_double_field(line, "delta", e.delta);
    expect_double_field(line, "res_p", e.res_p);
    expect_double_field(line, "res_d", e.res_d);
    expect_double_field(line, "res_c", e.res_c);
    expect_double_field(line, "sigma", e.sigma);
    expect_double_field(line, "alpha_p", e.alpha_p);
    expect_double_field(line, "alpha_d", e.alpha_d);
}

TEST(JsonLinesTraceSink, RoundTripReadsEveryGoldenLineBackBitEqual) {
    {
        const IpqpTraceIterEvent e = golden_iter_present();
        std::ostringstream os;
        JsonLinesTraceSink sink(os);
        sink.on_ipqp_iter(e);
        const std::string l = os.str();
        expect_double_field(l, "mu", e.mu);
        expect_double_field(l, "rho", e.rho);
        expect_double_field(l, "delta", e.delta);
        expect_double_field(l, "res_p", e.res_p);
        expect_double_field(l, "res_d", e.res_d);
        expect_double_field(l, "res_c", e.res_c);
        expect_double_field(l, "sigma", e.sigma);
        expect_double_field(l, "alpha_p", e.alpha_p);
        expect_double_field(l, "alpha_d", e.alpha_d);
        EXPECT_EQ(raw_field(l, "inertia"), "[5,2,0]");
        EXPECT_EQ(raw_field(l, "perturbed"), "4");
        EXPECT_EQ(raw_field(l, "facts"), "\"a\\\"b\\\\c\\nd\"");
    }
    {
        const IpqpTraceIterEvent e = golden_iter_absent();
        std::ostringstream os;
        JsonLinesTraceSink sink(os);
        sink.on_ipqp_iter(e);
        const std::string l = os.str();
        expect_double_field(l, "mu", e.mu);
        expect_double_field(l, "res_p", e.res_p);
        expect_double_field(l, "res_d", e.res_d);
        expect_double_field(l, "res_c", e.res_c);
        EXPECT_EQ(raw_field(l, "inertia"), "null");
        EXPECT_EQ(raw_field(l, "perturbed"), "null");
    }
    {
        IpqpTraceRestartEvent e;
        e.shift_p = 0.25;
        e.shift_d = 0.125;
        e.mu0 = 0.1;
        e.mu_payload = 1e-8;
        std::ostringstream os;
        JsonLinesTraceSink sink(os);
        sink.on_ipqp_restart(e);
        const std::string l = os.str();
        expect_double_field(l, "shift_p", e.shift_p);
        expect_double_field(l, "shift_d", e.shift_d);
        expect_double_field(l, "mu0", e.mu0);
        expect_double_field(l, "mu_payload", e.mu_payload);
    }
    {
        IpqpTraceRegEvent e;
        e.rho = 100.0;
        e.delta = 0.001;
        std::ostringstream os;
        JsonLinesTraceSink sink(os);
        sink.on_ipqp_reg(e);
        expect_double_field(os.str(), "rho", e.rho);
        expect_double_field(os.str(), "delta", e.delta);
    }
    {
        SqpFallbackVerdictTraceEvent e;
        e.entered_rung_a = true;
        e.rho_0 = 1e6;
        std::ostringstream os;
        JsonLinesTraceSink sink(os);
        sink.on_fallback_verdict(e);
        expect_double_field(os.str(), "rho_0", *e.rho_0);
    }
}

TEST(JsonLinesTraceSink, RoundTripReachesInsideTheNestedEvidenceObject) {
    const IpqpTraceEscapeEvent e = golden_escape();
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipqp_escape(e);
    const std::string evidence = raw_field(os.str(), "evidence");
    const std::string stall = raw_field(evidence, "stall");
    const std::string infeas = raw_field(evidence, "infeasibility");
    ASSERT_FALSE(stall.empty());
    ASSERT_FALSE(infeas.empty());

    expect_double_field(stall, "mu_ratio", e.evidence.stall.mu_ratio);
    expect_double_field(stall, "residual_improvement", e.evidence.stall.residual_improvement);
    expect_double_field(stall, "min_alpha", e.evidence.stall.min_alpha);
    expect_double_field(stall, "max_step_alpha", e.evidence.stall.max_step_alpha);

    const IpqpInfeasibilityEvidence &i = e.evidence.infeasibility;
    expect_double_field(infeas, "primal_start", i.primal_start);
    expect_double_field(infeas, "primal_end", i.primal_end);
    expect_double_field(infeas, "primal_improvement", i.primal_improvement);
    expect_double_field(infeas, "dual_norm_start", i.dual_norm_start);
    expect_double_field(infeas, "dual_norm_end", i.dual_norm_end);
    expect_double_field(infeas, "dual_growth", i.dual_growth);
    expect_double_field(infeas, "dual_step_growth", i.dual_step_growth);
    expect_double_field(infeas, "farkas_residual", i.farkas_residual);
    expect_double_field(infeas, "farkas_gap", i.farkas_gap);
    expect_double_field(infeas, "least_infeasible_primal", i.least_infeasible_primal);
    expect_double_field(infeas, "least_infeasible_mu", i.least_infeasible_mu);
    // The five Vec members are not in v0 and must not have leaked a key.
    EXPECT_TRUE(raw_field(infeas, "least_infeasible_x").empty());
    EXPECT_TRUE(raw_field(infeas, "least_infeasible_s").empty());
    EXPECT_TRUE(raw_field(infeas, "least_infeasible_lambda_i").empty());
    EXPECT_TRUE(raw_field(infeas, "least_infeasible_zl").empty());
    EXPECT_TRUE(raw_field(infeas, "least_infeasible_zu").empty());
}

// ===========================================================================
// (iii) WHOLE SOLVE -- the stream and the currency agree
// ===========================================================================

/// @brief Forwards to the writer AND records, so one solve produces both the
/// stream under test and the arrival-order oracle it is checked against.
class TeeSink final : public IpqpTraceSink {
  public:
    explicit TeeSink(JsonLinesTraceSink &json) : json_(json) {}

    Index iters = 0, regs = 0, restarts = 0, routes = 0;
    Index certifies = 0, escapes = 0, modes = 0, fallbacks = 0;

    void on_ipqp_iter(const IpqpTraceIterEvent &e) override {
        ++iters;
        json_.on_ipqp_iter(e);
    }
    void on_ipqp_reg(const IpqpTraceRegEvent &e) override {
        ++regs;
        json_.on_ipqp_reg(e);
    }
    void on_ipqp_restart(const IpqpTraceRestartEvent &e) override {
        ++restarts;
        json_.on_ipqp_restart(e);
    }
    void on_ipqp_route(const IpqpTraceRouteEvent &e) override {
        ++routes;
        json_.on_ipqp_route(e);
    }
    void on_ipqp_certify(const IpqpTraceCertifyEvent &e) override {
        ++certifies;
        json_.on_ipqp_certify(e);
    }
    void on_ipqp_escape(const IpqpTraceEscapeEvent &e) override {
        ++escapes;
        json_.on_ipqp_escape(e);
    }
    void on_qp_mode(const QpModeTraceEvent &e) override {
        ++modes;
        json_.on_qp_mode(e);
    }
    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &e) override {
        ++fallbacks;
        json_.on_fallback_verdict(e);
    }

  private:
    JsonLinesTraceSink &json_;
};

/// Counts the stream's lines by `ev` and asserts `seq` is 1..N contiguous.
std::map<std::string, Index> census(const std::string &stream) {
    std::map<std::string, Index> by_ev;
    Index expected_seq = 0;
    for (const std::string &l : split_lines(stream)) {
        ++expected_seq;
        EXPECT_EQ(raw_field(l, "seq"), std::to_string(expected_seq)) << l;
        EXPECT_EQ(raw_field(l, "v"), "0") << l;
        EXPECT_EQ(raw_field(l, "depth"), "0") << "T1 emits nothing that moves depth";
        // R1(b): the enum fallthrough is REACHABLE (the tree has no -Werror), so
        // the stream itself asserts no out-of-schema value ever reached it.
        EXPECT_EQ(l.find("\"unknown\""), std::string::npos) << l;
        ++by_ev[event_name(l)];
    }
    return by_ev;
}

TEST(JsonLinesTraceSink, WholeSolveStreamCountsEqualTheCurrencyOnHS11AndHS38AtKIpm) {
    // TWO CELLS. HS38 is the natural-stall cell: one escape whose fallback
    // entry NEVER FIRES, so it alone witnesses `unfired`. HS11 escapes twice
    // with both entries fired, so it alone witnesses `fired`.
    //
    // One cell would leave a side of W2 T5's `unfired == entries - fired`
    // reading 0 == 0.
    struct Leg {
        int hs;
        Index expect_unfired;
        Index expect_fired;
    };
    for (const Leg leg : {Leg{11, 0, 2}, Leg{38, 1, 0}}) {
        SqpOptions opts;
        opts.qp_mode = QpMode::kIpm;
        opts.max_iter = 60;
        const HsProblem p = make_hs(leg.hs);
        SqpDriver driver(opts);
        std::ostringstream os;
        JsonLinesTraceSink json(os);
        TeeSink tee(json);
        driver.attach_trace(&tee);
        const SqpSolution sol = driver.solve(*p.model);

        const std::map<std::string, Index> by_ev = census(os.str());
        const auto at = [&](const char *k) {
            const auto it = by_ev.find(k);
            return it == by_ev.end() ? Index{0} : it->second;
        };
        SCOPED_TRACE("HS" + std::to_string(leg.hs));
        EXPECT_EQ(json.lines_written(), static_cast<Index>(split_lines(os.str()).size()));

        // THE COUNTERS, which are the asserted currency (CLAUDE.md section 7).
        ASSERT_GT(sol.counters.ipqp.ipqp_iters, 0) << "the tier must have run at all";
        EXPECT_EQ(at("ipqp.iter"), sol.counters.ipqp.ipqp_iters);
        EXPECT_EQ(at("ipqp.escape"), sol.counters.ipqp.ipqp_escapes);
        // `ipqp_final_inertia_read` is a per-subproblem STATUS, overwritten by
        // the fold, so no counter counts the reads TAKEN; the tee's recording
        // half is the oracle for the event's fires-iff-a-read-was-taken rule.
        EXPECT_EQ(at("ipqp.certify"), tee.certifies);
        EXPECT_EQ(at("ipqp.route"), tee.routes);
        EXPECT_EQ(at("qp.mode"), tee.modes);

        // W2 T5's per-ENTRY rule: one event per entry, kUnfired included. A
        // PINNED DECLINE routes to the walk without entering the fallback.
        const Index entries =
            sol.counters.ipqp.ipqp_to_walk - sol.counters.ipqp.ipqp_declined_pinned;
        EXPECT_EQ(at("fallback.verdict"), entries);

        Index unfired = 0, disproved = 0, relaxed = 0, exhausted = 0, rung_b = 0;
        for (const std::string &l : split_lines(os.str())) {
            if (event_name(l) != "fallback.verdict") {
                continue;
            }
            const std::string v = raw_field(l, "verdict");
            unfired += (v == "\"unfired\"") ? 1 : 0;
            disproved += (v == "\"disproved\"") ? 1 : 0;
            relaxed += (v == "\"relaxed\"") ? 1 : 0;
            exhausted += (v == "\"exhausted\"") ? 1 : 0;
            rung_b += (v == "\"rung_b\"") ? 1 : 0;
        }
        const Index fired = disproved + relaxed + exhausted + rung_b;
        EXPECT_EQ(fired,
                  sol.counters.elastic_from_ipqp_escape - sol.counters.elastic_floor_retries);
        EXPECT_EQ(unfired, entries - fired);
        EXPECT_EQ(disproved, sol.counters.ipqp_suspicion_disproved);
        EXPECT_EQ(rung_b, sol.counters.ipqp_fallback_rung_b);
        EXPECT_EQ(unfired, leg.expect_unfired) << "the cell's own reason for being here";
        EXPECT_EQ(fired, leg.expect_fired);
    }
}

// ===========================================================================
// (iv) NULL SINK -- attaching the writer moves nothing, and writes nothing
// ===========================================================================

void expect_counters_identical(const SqpCounters &a, const SqpCounters &b) {
    EXPECT_EQ(a.major_iters, b.major_iters);
    EXPECT_EQ(a.qp_minor_iters, b.qp_minor_iters);
    EXPECT_EQ(a.factorizations, b.factorizations);
    EXPECT_EQ(a.steps_accepted, b.steps_accepted);
    EXPECT_EQ(a.rejected_steps, b.rejected_steps);
    EXPECT_EQ(a.soc_steps, b.soc_steps);
    EXPECT_EQ(a.soc_applied, b.soc_applied);
    EXPECT_EQ(a.soc_qp_infeasible, b.soc_qp_infeasible);
    EXPECT_EQ(a.soc_rejected, b.soc_rejected);
    EXPECT_EQ(a.elastic_activations, b.elastic_activations);
    EXPECT_EQ(a.elastic_escalations, b.elastic_escalations);
    EXPECT_EQ(a.restoration_iters, b.restoration_iters);
    EXPECT_EQ(a.elastic_from_ipqp_escape, b.elastic_from_ipqp_escape);
    EXPECT_EQ(a.ipqp_suspicion_disproved, b.ipqp_suspicion_disproved);
    EXPECT_EQ(a.ipqp_fallback_rung_b, b.ipqp_fallback_rung_b);
    EXPECT_EQ(a.elastic_rho0_ceiling_hits, b.elastic_rho0_ceiling_hits);
    EXPECT_EQ(a.elastic_floor_retries, b.elastic_floor_retries);
    EXPECT_EQ(a.eqp_refine_steps, b.eqp_refine_steps);
    EXPECT_EQ(a.border_refine_steps, b.border_refine_steps);
    EXPECT_EQ(a.verdict_refine_steps, b.verdict_refine_steps);
    EXPECT_EQ(a.suspect_escalations, b.suspect_escalations);
    EXPECT_EQ(a.symbolic_analyses, b.symbolic_analyses);
    EXPECT_EQ(a.start_level_used, b.start_level_used);
    EXPECT_EQ(a.full_step_majors, b.full_step_majors);
    EXPECT_EQ(a.watchdog_restores, b.watchdog_restores);
    EXPECT_EQ(a.evals_full, b.evals_full);
    EXPECT_EQ(a.evals_values, b.evals_values);
    EXPECT_EQ(a.probe_budget_stops, b.probe_budget_stops);
    EXPECT_EQ(a.crash_seeded_rows, b.crash_seeded_rows);
    EXPECT_EQ(a.crash_seeded_bounds, b.crash_seeded_bounds);
    EXPECT_EQ(a.n_seeded, b.n_seeded);
    EXPECT_EQ(a.seeded_clamped, b.seeded_clamped);
    EXPECT_EQ(a.ip_activity_inferred, b.ip_activity_inferred);
    // R6: the two nested aggregates IN FULL -- 18 + 39 fields. Two of each was
    // enough on a walk cell where they are all zero; the kIpm leg is exactly
    // where they are not.
    EXPECT_EQ(a.ssn.ssn_iters, b.ssn.ssn_iters);
    EXPECT_EQ(a.ssn.ssn_bulk_flips, b.ssn.ssn_bulk_flips);
    EXPECT_EQ(a.ssn.ssn_backtracks, b.ssn.ssn_backtracks);
    EXPECT_EQ(a.ssn.ssn_prox_updates, b.ssn.ssn_prox_updates);
    EXPECT_EQ(a.ssn.ssn_escapes, b.ssn.ssn_escapes);
    EXPECT_EQ(a.ssn.ssn_uncertain_peak, b.ssn.ssn_uncertain_peak);
    EXPECT_EQ(a.ssn.ssn_refinements, b.ssn.ssn_refinements);
    EXPECT_EQ(a.ssn.ssn_refine_refused, b.ssn.ssn_refine_refused);
    EXPECT_EQ(a.ssn.ssn_refine_factorizations, b.ssn.ssn_refine_factorizations);
    EXPECT_EQ(a.ssn.ssn_refine_neg_duals, b.ssn.ssn_refine_neg_duals);
    EXPECT_EQ(a.ssn.ssn_sign_swept, b.ssn.ssn_sign_swept);
    EXPECT_EQ(a.ssn.ssn_sign_sweep_max, b.ssn.ssn_sign_sweep_max);
    EXPECT_EQ(a.ssn.ssn_escape_budget, b.ssn.ssn_escape_budget);
    EXPECT_EQ(a.ssn.ssn_escape_singular, b.ssn.ssn_escape_singular);
    EXPECT_EQ(a.ssn.ssn_escape_no_contraction, b.ssn.ssn_escape_no_contraction);
    EXPECT_EQ(a.ssn.ssn_escape_infeasible_suspect, b.ssn.ssn_escape_infeasible_suspect);
    EXPECT_EQ(a.ssn.ssn_escape_indefinite, b.ssn.ssn_escape_indefinite);
    EXPECT_EQ(a.ssn.ssn_escape_gate_refused, b.ssn.ssn_escape_gate_refused);
    EXPECT_EQ(a.ipqp.ipqp_iters, b.ipqp.ipqp_iters);
    EXPECT_EQ(a.ipqp.ipqp_factorizations, b.ipqp.ipqp_factorizations);
    EXPECT_EQ(a.ipqp.ipqp_symbolic_analyses, b.ipqp.ipqp_symbolic_analyses);
    EXPECT_EQ(a.ipqp.ipqp_solves, b.ipqp.ipqp_solves);
    EXPECT_EQ(a.ipqp.ipqp_pattern_verifies, b.ipqp.ipqp_pattern_verifies);
    EXPECT_EQ(a.ipqp.ipqp_rho_demanded_max, b.ipqp.ipqp_rho_demanded_max);
    EXPECT_EQ(a.ipqp.ipqp_rho_demanded_last, b.ipqp.ipqp_rho_demanded_last);
    EXPECT_EQ(a.ipqp.ipqp_inertia_retries, b.ipqp.ipqp_inertia_retries);
    EXPECT_EQ(a.ipqp.ipqp_iters_at_elevated_rho, b.ipqp.ipqp_iters_at_elevated_rho);
    EXPECT_EQ(a.ipqp.ipqp_ladder_reclimbs, b.ipqp.ipqp_ladder_reclimbs);
    EXPECT_EQ(a.ipqp.ipqp_pivot_reroute_primal, b.ipqp.ipqp_pivot_reroute_primal);
    EXPECT_EQ(a.ipqp.ipqp_pivot_reroute_dual_fallback, b.ipqp.ipqp_pivot_reroute_dual_fallback);
    EXPECT_EQ(a.ipqp.ipqp_iters_ladder_armed_no_advance, b.ipqp.ipqp_iters_ladder_armed_no_advance);
    EXPECT_EQ(a.ipqp.ipqp_final_inertia_read, b.ipqp.ipqp_final_inertia_read);
    EXPECT_EQ(a.ipqp.ipqp_reg_decreases, b.ipqp.ipqp_reg_decreases);
    EXPECT_EQ(a.ipqp.ipqp_reg_increases, b.ipqp.ipqp_reg_increases);
    EXPECT_EQ(a.ipqp.ipqp_prox_center_updates, b.ipqp.ipqp_prox_center_updates);
    EXPECT_EQ(a.ipqp.ipqp_restart_repairs, b.ipqp.ipqp_restart_repairs);
    EXPECT_EQ(a.ipqp.ipqp_restart_shift_max, b.ipqp.ipqp_restart_shift_max);
    EXPECT_EQ(a.ipqp.ipqp_mu_adopted, b.ipqp.ipqp_mu_adopted);
    EXPECT_EQ(a.ipqp.ipqp_warm_restart_abandoned, b.ipqp.ipqp_warm_restart_abandoned);
    EXPECT_EQ(a.ipqp.ipqp_declined_pinned, b.ipqp.ipqp_declined_pinned);
    EXPECT_EQ(a.ipqp.ipqp_tier_retired_after, b.ipqp.ipqp_tier_retired_after);
    EXPECT_EQ(a.ipqp.ipqp_face_uncertain, b.ipqp.ipqp_face_uncertain);
    EXPECT_EQ(a.ipqp.ipqp_refine_accepted, b.ipqp.ipqp_refine_accepted);
    EXPECT_EQ(a.ipqp.ipqp_refine_refused, b.ipqp.ipqp_refine_refused);
    EXPECT_EQ(a.ipqp.ipqp_to_refine, b.ipqp.ipqp_to_refine);
    EXPECT_EQ(a.ipqp.ipqp_to_ssn, b.ipqp.ipqp_to_ssn);
    EXPECT_EQ(a.ipqp.ipqp_to_walk, b.ipqp.ipqp_to_walk);
    EXPECT_EQ(a.ipqp.ipqp_escapes, b.ipqp.ipqp_escapes);
    EXPECT_EQ(a.ipqp.ipqp_escape_budget, b.ipqp.ipqp_escape_budget);
    EXPECT_EQ(a.ipqp.ipqp_escape_stall, b.ipqp.ipqp_escape_stall);
    EXPECT_EQ(a.ipqp.ipqp_escape_indefinite, b.ipqp.ipqp_escape_indefinite);
    EXPECT_EQ(a.ipqp.ipqp_escape_numerical, b.ipqp.ipqp_escape_numerical);
    EXPECT_EQ(a.ipqp.ipqp_escape_infeasible_suspect, b.ipqp.ipqp_escape_infeasible_suspect);
    EXPECT_EQ(a.ipqp.ipqp_alpha_p_min, b.ipqp.ipqp_alpha_p_min);
    EXPECT_EQ(a.ipqp.ipqp_alpha_d_min, b.ipqp.ipqp_alpha_d_min);
    EXPECT_EQ(a.ipqp.ipqp_read_kept_tight_sides, b.ipqp.ipqp_read_kept_tight_sides);
    EXPECT_EQ(a.ipqp.ipqp_read_barrier_noise_sides, b.ipqp.ipqp_read_barrier_noise_sides);
}

TEST(JsonLinesTraceSink, OnAWalkCellTheSinkChangesNoCounterAndWritesNoLine) {
    // A REPLAY-CLASS PIN. HS24 at kWalk: the walk and SSN arms have no emit
    // site in T1, so attaching a sink to them costs nothing AND produces
    // nothing. W4 T2 adds `qp.mode` there and re-derives the zero.
    SqpOptions opts;
    opts.qp_mode = QpMode::kWalk;
    opts.max_iter = 60;
    const HsProblem p = make_hs(24);

    SqpDriver bare(opts);
    const SqpSolution without = bare.solve(*p.model);

    SqpDriver traced(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    traced.attach_trace(&json);
    const SqpSolution with = traced.solve(*p.model);

    ASSERT_EQ(without.status, with.status);
    ASSERT_GT(without.counters.major_iters, 0) << "non-vacuous: the cell really solves";
    expect_counters_identical(without.counters, with.counters);
    EXPECT_EQ(without.history.size(), with.history.size());
    EXPECT_EQ(os.str(), "") << "the walk arm emits nothing in W4 T1";
    EXPECT_EQ(json.lines_written(), 0);
}

// ===========================================================================
// R5 -- BOOL FALSIFIERS: a distinct signature ACROSS lines for every bool
// ===========================================================================
//
// One golden line cannot tell two `true` bools apart. Proven by mutation:
// swapping repaired/adopted, rho0_ceiling_hit/floor_retry or
// infeasibility.fired/farkas_corroborated left round 1's lines BYTE-IDENTICAL.
//
// The lines below give every bool in every event a signature no other bool in
// the same event shares.
//
// The same-typed-and-equal sweep over the other structs found nothing else:
// every Index and every double already differs from its siblings, and
// `ipqp.iter`'s five zeroed doubles are separated by the present line.

TEST(JsonLinesTraceSink, GoldenLineIpqpRestartSecondBoolSignature) {
    // Catches the repaired <-> adopted swap: (T,T) vs (T,F), with abandoned (F,F).
    IpqpTraceRestartEvent e;
    e.grade = IpqpTraceRestartGrade::kBase;
    e.repaired = true;
    e.shift_p = 1.0;
    e.shift_d = 2.0;
    e.mu0 = 4.0;
    e.mu_payload = 8.0;
    e.adopted = false;
    e.abandoned = false;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipqp_restart(e);
    EXPECT_EQ(os.str(), "{\"v\":0,\"ev\":\"ipqp.restart\",\"seq\":1,\"depth\":0,\"grade\":\"base\","
                        "\"repaired\":true,\"shift_p\":1,\"shift_d\":2,\"mu0\":4,\"mu_payload\":8,"
                        "\"adopted\":false,\"abandoned\":false}\n");
}

TEST(JsonLinesTraceSink, GoldenLineFallbackVerdictClampedWithoutARetry) {
    // With the two lines above, entered_rung_a is (T,F,T,T), rho0_ceiling_hit
    // (T,F,T,F) and floor_retry (T,F,F,T) -- so this line and the next catch the
    // rho0_ceiling_hit <-> floor_retry swap, and both against entered_rung_a.
    SqpFallbackVerdictTraceEvent e;
    e.entered_rung_a = true;
    e.verdict = SqpFallbackVerdict::kDisproved;
    e.rho_0 = 250.0;
    e.rho0_ceiling_hit = true;
    e.floor_retry = false;
    e.qp_minor_iters = 5;
    e.qp_factorizations = 2;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_fallback_verdict(e);
    EXPECT_EQ(os.str(), "{\"v\":0,\"ev\":\"fallback.verdict\",\"seq\":1,\"depth\":0,"
                        "\"entered_rung_a\":true,\"verdict\":\"disproved\",\"rho_0\":250,"
                        "\"rho0_ceiling_hit\":true,\"floor_retry\":false,\"qp_minor_iters\":5,"
                        "\"qp_factorizations\":2}\n");
}

TEST(JsonLinesTraceSink, GoldenLineFallbackVerdictRetriedAtTheFloorWithoutAClamp) {
    // The retry's placement IS the floor (kElasticRhoInit), which is why 100 is
    // the value here rather than an arbitrary one.
    SqpFallbackVerdictTraceEvent e;
    e.entered_rung_a = true;
    e.verdict = SqpFallbackVerdict::kExhausted;
    e.rho_0 = kElasticRhoInit;
    e.rho0_ceiling_hit = false;
    e.floor_retry = true;
    e.qp_minor_iters = 11;
    e.qp_factorizations = 6;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_fallback_verdict(e);
    EXPECT_EQ(os.str(), "{\"v\":0,\"ev\":\"fallback.verdict\",\"seq\":1,\"depth\":0,"
                        "\"entered_rung_a\":true,\"verdict\":\"exhausted\",\"rho_0\":100,"
                        "\"rho0_ceiling_hit\":false,\"floor_retry\":true,\"qp_minor_iters\":11,"
                        "\"qp_factorizations\":6}\n");
}

TEST(JsonLinesTraceSink, GoldenLineIpqpEscapeStallBranch) {
    // The `stall.fired == true` branch, which round 1 never reached. Across the
    // three escape lines: stall.fired (F,T,F), infeasibility.fired (T,F,T),
    // exhaustion_route (F,F,T), farkas_corroborated (T,F,F) -- all distinct.
    IpqpTraceEscapeEvent e;
    e.reason = IpqpTraceEscapeReason::kStall;
    e.evidence.stall.fired = true;
    e.evidence.stall.window = 2;
    e.evidence.stall.mu_ratio = 0.5;
    e.evidence.stall.residual_improvement = 0.0625;
    e.evidence.stall.min_alpha = 0.03125;
    e.evidence.stall.max_step_alpha = 0.015625;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipqp_escape(e);
    EXPECT_EQ(os.str(),
              "{\"v\":0,\"ev\":\"ipqp.escape\",\"seq\":1,\"depth\":0,\"reason\":\"stall\","
              "\"evidence\":{\"stall\":{\"fired\":true,\"window\":2,\"mu_ratio\":0.5,"
              "\"residual_improvement\":0.0625,\"min_alpha\":0.03125,\"max_step_alpha\":0.015625},"
              "\"infeasibility\":{\"fired\":false,\"exhaustion_route\":false,\"window\":0,"
              "\"primal_start\":0,\"primal_end\":0,\"primal_improvement\":0,\"dual_norm_start\":0,"
              "\"dual_norm_end\":0,\"dual_growth\":0,\"dual_step_growth\":0,"
              "\"farkas_corroborated\":false,\"farkas_residual\":0,\"farkas_gap\":0,"
              "\"least_infeasible_primal\":0,\"least_infeasible_mu\":0}}}\n");
}

TEST(JsonLinesTraceSink, GoldenLineIpqpEscapeExhaustionRouteWithoutFarkas) {
    // The exhaustion route with the Farkas gate off -- the two zeroed Farkas
    // scalars are the gate's own convention, not an absence.
    IpqpTraceEscapeEvent e;
    e.reason = IpqpTraceEscapeReason::kInfeasibleSuspect;
    IpqpInfeasibilityEvidence &i = e.evidence.infeasibility;
    i.fired = true;
    i.exhaustion_route = true;
    i.window = 7;
    i.primal_start = 3.0;
    i.primal_end = 2.75;
    i.primal_improvement = 0.0625;
    i.dual_norm_start = 20.0;
    i.dual_norm_end = 4e6;
    i.dual_growth = 2e5;
    i.dual_step_growth = 1.5;
    i.farkas_corroborated = false;
    i.least_infeasible_primal = 2.5;
    i.least_infeasible_mu = 0.0078125;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipqp_escape(e);
    EXPECT_EQ(
        os.str(),
        "{\"v\":0,\"ev\":\"ipqp.escape\",\"seq\":1,\"depth\":0,\"reason\":\"infeasible_suspect\","
        "\"evidence\":{\"stall\":{\"fired\":false,\"window\":0,\"mu_ratio\":0,"
        "\"residual_improvement\":0,\"min_alpha\":0,\"max_step_alpha\":0},"
        "\"infeasibility\":{\"fired\":true,\"exhaustion_route\":true,\"window\":7,"
        "\"primal_start\":3,\"primal_end\":2.75,\"primal_improvement\":0.0625,"
        "\"dual_norm_start\":20,\"dual_norm_end\":4000000,\"dual_growth\":200000,"
        "\"dual_step_growth\":1.5,\"farkas_corroborated\":false,\"farkas_residual\":0,"
        "\"farkas_gap\":0,\"least_infeasible_primal\":2.5,"
        "\"least_infeasible_mu\":0.0078125}}}\n");
}

// ===========================================================================
// R3 -- THE FAILURE PREDICATE, and the two exception cases
// ===========================================================================

/// @brief A `streambuf` that accepts `budget` bytes and then refuses everything.
///
/// A short `xsputn` is what a real `filebuf` hitting ENOSPC reports, and it is
/// what sets `badbit` on the ostream above it. `taken` is the artifact that
/// actually reached the "file".
class FailAfterBuf : public std::streambuf {
  public:
    explicit FailAfterBuf(std::streamsize budget) : left_(budget) {}
    std::string taken;

  protected:
    std::streamsize xsputn(const char *s, std::streamsize n) override {
        const std::streamsize k = std::min(n, left_);
        taken.append(s, static_cast<std::size_t>(k));
        left_ -= k;
        return k;
    }
    int_type overflow(int_type c) override {
        if (traits_type::eq_int_type(c, traits_type::eof())) {
            return traits_type::not_eof(c);
        }
        if (left_ <= 0) {
            return traits_type::eof();
        }
        taken.push_back(static_cast<char>(c));
        --left_;
        return c;
    }

  private:
    std::streamsize left_;
};

Index count_lines(const std::string &s) {
    return static_cast<Index>(std::count(s.begin(), s.end(), '\n'));
}

TEST(JsonLinesTraceSink, FailedIsStickyAndSeqKeepsAdvancingPastTheFailure) {
    // A HEALTHY stream first, so the predicate is not vacuously true.
    {
        std::ostringstream ok;
        JsonLinesTraceSink sink(ok);
        sink.on_ipqp_certify(IpqpTraceCertifyEvent{});
        EXPECT_FALSE(sink.failed());
        EXPECT_EQ(sink.lines_written(), 1);
    }
    FailAfterBuf buf(20); // shorter than one line, so the first write fails
    std::ostream out(&buf);
    JsonLinesTraceSink sink(out);
    sink.on_ipqp_certify(IpqpTraceCertifyEvent{});
    EXPECT_TRUE(sink.failed());
    EXPECT_EQ(sink.lines_written(), 1);
    sink.on_ipqp_certify(IpqpTraceCertifyEvent{});
    sink.on_ipqp_certify(IpqpTraceCertifyEvent{});
    EXPECT_TRUE(sink.failed()) << "sticky: it never reads back false";
    EXPECT_EQ(sink.lines_written(), 3) << "seq advances, so the gap counts the lost lines";
    EXPECT_LT(count_lines(buf.taken), sink.lines_written());
}

TEST(JsonLinesTraceSink, AStreamThatFailsMidSolveDoesNotChangeTheSolve) {
    // THE INSTRUMENTATION INVARIANT, under the DEFAULT exception mask: the solve
    // completes and its counters are the bare solve's, byte for byte, while the
    // artifact is short and `failed()` says so.
    SqpOptions opts;
    opts.qp_mode = QpMode::kIpm;
    opts.max_iter = 60;
    const HsProblem p = make_hs(38);

    SqpDriver bare(opts);
    const SqpSolution without = bare.solve(*p.model);

    FailAfterBuf buf(4096); // a few lines in, then ENOSPC
    std::ostream out(&buf);
    SqpDriver traced(opts);
    JsonLinesTraceSink json(out);
    traced.attach_trace(&json);
    const SqpSolution with = traced.solve(*p.model);

    ASSERT_EQ(without.status, with.status);
    expect_counters_identical(without.counters, with.counters);
    EXPECT_TRUE(json.failed());
    ASSERT_GT(count_lines(buf.taken), 0) << "non-vacuous: the failure is MID-solve";
    EXPECT_LT(count_lines(buf.taken), json.lines_written()) << "the gap is the lines lost";
}

TEST(JsonLinesTraceSink, AnArmedExceptionMaskPropagatesOutOfTheSolveByDesign) {
    // THE SECOND CASE. A caller who arms a mask has ASKED for exceptions; the
    // sink neither swallows nor re-labels one, and nothing in the driver catches
    // it. This is the header's contract, pinned.
    SqpOptions opts;
    opts.qp_mode = QpMode::kIpm;
    opts.max_iter = 60;
    const HsProblem p = make_hs(38);
    FailAfterBuf buf(4096);
    std::ostream out(&buf);
    out.exceptions(std::ios::badbit);
    SqpDriver traced(opts);
    JsonLinesTraceSink json(out);
    traced.attach_trace(&json);
    EXPECT_THROW(traced.solve(*p.model), std::ios_base::failure);
}

// ===========================================================================
// R6 -- the null-sink comparison on the arm where the sink actually writes
// ===========================================================================

TEST(JsonLinesTraceSink, OnHS38AtKIpmTheSinkWritesHundredsOfLinesAndStillMovesNoCounter) {
    // The kWalk pin below records that the walk arm emits nothing. THIS one is
    // the question the brief actually asks: attaching a sink to the arm where it
    // formats every event moves no counter either.
    SqpOptions opts;
    opts.qp_mode = QpMode::kIpm;
    opts.max_iter = 60;
    const HsProblem p = make_hs(38);

    SqpDriver bare(opts);
    const SqpSolution without = bare.solve(*p.model);

    SqpDriver traced(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    traced.attach_trace(&json);
    const SqpSolution with = traced.solve(*p.model);

    ASSERT_EQ(without.status, with.status);
    ASSERT_GT(json.lines_written(), 0) << "non-vacuous: the sink really ran";
    ASSERT_GT(without.counters.ipqp.ipqp_iters, 0);
    ASSERT_GT(without.counters.ipqp.ipqp_escapes, 0) << "the cell moves the escape census too";
    expect_counters_identical(without.counters, with.counters);
    EXPECT_EQ(without.history.size(), with.history.size());
    EXPECT_FALSE(json.failed());
}

// ===========================================================================
// R7 -- the identities on populations that make every term non-zero
// ===========================================================================

/// @brief A model whose variable 0 has a zero-width box.
///
/// REPLICATED, unchanged in behaviour, from `PinnedVariableModel` in
/// tests/sqp/test_ipqp_dispatch.cpp:156 (which pins the decline itself). It is
/// here because it is the only in-tree population with
/// `ipqp_declined_pinned > 0`, and that is the term the entry identity
/// subtracts. Neither that test nor its fixture was modified.
class PinnedVariableModel final : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override {
        return 0.5 * ((x(0) - 1.0) * (x(0) - 1.0) + (x(1) - 2.0) * (x(1) - 2.0));
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << x(0) - 1.0, x(1) - 2.0;
        return g;
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        Vec c(1);
        c << x(0) + x(1) - 3.0;
        return c;
    }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale;
        h.insert(1, 1) = obj_scale;
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = 1.0;
        j.insert(0, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override {
        static const Vec v = (Vec(2) << 0.5, -5.0).finished();
        return v;
    }
    const Vec &upper() const override {
        static const Vec v = (Vec(2) << 0.5, 5.0).finished();
        return v;
    }
    Vec start_point() const override { return Vec::Constant(2, 0.25); }
};

TEST(JsonLinesTraceSink, ThePinnedDeclineIsWhyTheEntryCountSubtractsDeclinedPinned) {
    // On HS11 and HS38 `ipqp_declined_pinned` is 0, so the whole-solve pin's
    // subtraction is never exercised there. HERE every walk route IS a decline,
    // so the stream carries ZERO entries against a positive `ipqp_to_walk`.
    SqpOptions opts;
    opts.qp_mode = QpMode::kIpm;
    opts.max_iter = 60;
    PinnedVariableModel model;
    SqpDriver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpSolution sol = driver.solve(model);

    const IpqpCounters &c = sol.counters.ipqp;
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    ASSERT_GT(c.ipqp_declined_pinned, 0) << "variable 0's declared bounds are equal";
    ASSERT_EQ(c.ipqp_to_walk, c.ipqp_declined_pinned) << "every walk route here is a decline";
    EXPECT_EQ(c.ipqp_escapes, 0) << "A DECLINE IS NOT AN ESCAPE";

    const std::map<std::string, Index> by_ev = census(os.str());
    const auto at = [&](const char *k) {
        const auto it = by_ev.find(k);
        return it == by_ev.end() ? Index{0} : it->second;
    };
    const Index entries = c.ipqp_to_walk - c.ipqp_declined_pinned;
    EXPECT_EQ(entries, 0);
    EXPECT_EQ(at("fallback.verdict"), entries)
        << "without the subtraction this reads " << c.ipqp_to_walk;
    EXPECT_EQ(at("ipqp.iter"), c.ipqp_iters);
    EXPECT_EQ(at("ipqp.escape"), c.ipqp_escapes);
}

// --- the five-verdict population, replicated from test_sqp_driver.cpp ------
//
// `w2_box_blocked_qp` (:8536), `w2_antiparallel_eq_qp` (:9023) and `w2_escaped`
// (:8631) are file-local there, so they are REPLICATED here unchanged rather
// than lifted -- that test and its fixture are not touched at all.

QpProblem w2_box_blocked_qp(double b) {
    QpProblem qp;
    qp.H = SpMatRM(2, 2);
    qp.H.insert(0, 0) = 2.0;
    qp.H.insert(1, 1) = 2.0;
    qp.H.makeCompressed();
    qp.g = Vec::Zero(2);
    qp.Ae = SpMatRM(1, 2);
    qp.Ae.insert(0, 0) = 1.0;
    qp.Ae.insert(0, 1) = 1.0;
    qp.Ae.makeCompressed();
    qp.be = Vec(1);
    qp.be << 5.0;
    qp.Ai = SpMatRM(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -b);
    qp.upper = Vec::Constant(2, b);
    return qp;
}

QpProblem w2_antiparallel_eq_qp() {
    QpProblem qp;
    qp.H = SpMatRM(2, 2);
    qp.H.insert(0, 0) = 2.0;
    qp.H.insert(1, 1) = 2.0;
    qp.H.makeCompressed();
    qp.g = Vec::Zero(2);
    qp.Ae = SpMatRM(2, 2);
    qp.Ae.insert(0, 0) = 1.0;
    qp.Ae.insert(0, 1) = 1.0;
    qp.Ae.insert(1, 0) = -1.0;
    qp.Ae.insert(1, 1) = -1.0;
    qp.Ae.makeCompressed();
    qp.be = Vec(2);
    qp.be << 1.0, 1.0;
    qp.Ai = SpMatRM(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -10.0);
    qp.upper = Vec::Constant(2, 10.0);
    return qp;
}

IpqpInfeasibilityEvidence w2_escaped_evidence(const QpProblem &qp) {
    QpOptions qopts;
    qopts.tr_radius = std::numeric_limits<double>::infinity();
    IpqpEngine tier(qopts);
    return tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{}).infeasibility_evidence;
}

TEST(JsonLinesTraceSink, TheVerdictStreamReproducesTheWHOLEPartitionOnAFiveClassPopulation) {
    // W2 T5's own five-class population (test_sqp_driver.cpp:9806), driven
    // through the sink the way the driver drives it (sqp_driver.cpp:3915: the
    // judge fills the out-param, the caller emits it).
    //
    // Plus a SIXTH entry declined ABOVE the floor, so `elastic_floor_retries` is
    // non-zero. Round 1 asserted this partition only on HS11/HS38, where
    // disproved, rung_b and floor_retries are all 0 -- four times `0 == 0`.
    const QpProblem feasible = w2_box_blocked_qp(10.0);
    const QpProblem blocked = w2_box_blocked_qp(0.5);
    const QpProblem antiparallel = w2_antiparallel_eq_qp();
    const IpqpInfeasibilityEvidence blocked_ev = w2_escaped_evidence(blocked);
    const IpqpInfeasibilityEvidence antiparallel_ev = w2_escaped_evidence(antiparallel);
    IpqpInfeasibilityEvidence floored = blocked_ev;
    floored.dual_norm_start = kElasticRhoInit;
    ASSERT_GT(blocked_ev.dual_norm_start, kElasticRhoInit) << "or the retry entry is vacuous";

    const double inf = std::numeric_limits<double>::infinity();
    struct Entry {
        const char *name;
        const QpProblem *qp;
        const IpqpInfeasibilityEvidence *evidence;
        double engine_tr;
    };
    const IpqpInfeasibilityEvidence unfired;
    const std::vector<Entry> entries{{"disproved", &feasible, &blocked_ev, inf},
                                     {"relaxed", &blocked, &blocked_ev, inf},
                                     {"exhausted", &antiparallel, &antiparallel_ev, inf},
                                     {"rung_b", &blocked, &floored, 1.0e-3},
                                     {"unfired", &blocked, &unfired, inf},
                                     {"floor_retry", &blocked, &blocked_ev, 1.0e-3}};

    SqpCounters out;
    const NlpEval nlp_ev;
    const SolveOverrides overrides;
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    for (const Entry &e : entries) {
        SqpOptions opts;
        opts.qp.tr_radius = e.engine_tr;
        QpEngine engine(opts.qp);
        SqpIterate row;
        std::optional<ElasticLadderReport> report;
        SqpFallbackVerdictTraceEvent verdict;
        certified_feasibility_fallback(engine, *e.qp, nlp_ev, nullptr, *e.evidence, overrides, opts,
                                       inf, out, row, report, verdict);
        json.on_fallback_verdict(verdict);
    }

    const std::map<std::string, Index> by_ev = census(os.str());
    ASSERT_EQ(by_ev.at("fallback.verdict"), static_cast<Index>(entries.size()));

    Index unfired_n = 0, disproved_n = 0, relaxed_n = 0, exhausted_n = 0, rung_b_n = 0;
    Index retry_lines = 0;
    for (const std::string &l : split_lines(os.str())) {
        const std::string v = raw_field(l, "verdict");
        unfired_n += (v == "\"unfired\"") ? 1 : 0;
        disproved_n += (v == "\"disproved\"") ? 1 : 0;
        relaxed_n += (v == "\"relaxed\"") ? 1 : 0;
        exhausted_n += (v == "\"exhausted\"") ? 1 : 0;
        rung_b_n += (v == "\"rung_b\"") ? 1 : 0;
        retry_lines += (raw_field(l, "floor_retry") == "true") ? 1 : 0;
        EXPECT_EQ(raw_field(l, "rho_0") == "null", raw_field(l, "entered_rung_a") == "false") << l;
    }

    // ALL FIVE SPELLINGS ARE PRODUCED BY A REAL JUDGE, not a hand-filled struct.
    EXPECT_EQ(disproved_n, 1);
    EXPECT_EQ(relaxed_n, 1);
    EXPECT_EQ(exhausted_n, 1);
    EXPECT_EQ(rung_b_n, 2) << "the declined entry and the declined-above-the-floor retry";
    EXPECT_EQ(unfired_n, 1);
    EXPECT_EQ(retry_lines, 1) << "and the retry flag is on exactly the sixth entry";

    // T5's identities, now with EVERY term non-zero.
    const Index fired = disproved_n + relaxed_n + exhausted_n + rung_b_n;
    const Index n = static_cast<Index>(entries.size());
    EXPECT_EQ(disproved_n, out.ipqp_suspicion_disproved);
    EXPECT_EQ(rung_b_n, out.ipqp_fallback_rung_b);
    EXPECT_GT(out.elastic_floor_retries, 0) << "the subtracted term is live here";
    EXPECT_EQ(fired, out.elastic_from_ipqp_escape - out.elastic_floor_retries);
    EXPECT_EQ(unfired_n, n - fired);
}

} // namespace
} // namespace hven::solvers
