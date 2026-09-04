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

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/trace_writer.h>

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

TEST(JsonLinesTraceSink, EveryEnumeratorHasItsSpecSpelling) {
    // The nine enums' 27 values, read off one line each. A renamed or
    // reordered spelling fails here rather than silently in an exemplar.
    const auto reg_dir = [](IpqpTraceRegDir d) {
        IpqpTraceRegEvent e;
        e.dir = d;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipqp_reg(e);
        return raw_field(os.str(), "dir");
    };
    EXPECT_EQ(reg_dir(IpqpTraceRegDir::kDown), "\"down\"");
    EXPECT_EQ(reg_dir(IpqpTraceRegDir::kUp), "\"up\"");

    const auto reg_reason = [](IpqpTraceRegReason r) {
        IpqpTraceRegEvent e;
        e.reason = r;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipqp_reg(e);
        return raw_field(os.str(), "reason");
    };
    EXPECT_EQ(reg_reason(IpqpTraceRegReason::kAccept), "\"accept\"");
    EXPECT_EQ(reg_reason(IpqpTraceRegReason::kInertia), "\"inertia\"");
    EXPECT_EQ(reg_reason(IpqpTraceRegReason::kStall), "\"stall\"");
    EXPECT_EQ(reg_reason(IpqpTraceRegReason::kFloor), "\"floor\"");

    const auto grade = [](IpqpTraceRestartGrade g) {
        IpqpTraceRestartEvent e;
        e.grade = g;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipqp_restart(e);
        return raw_field(os.str(), "grade");
    };
    EXPECT_EQ(grade(IpqpTraceRestartGrade::kFull), "\"full\"");
    EXPECT_EQ(grade(IpqpTraceRestartGrade::kBase), "\"base\"");
    EXPECT_EQ(grade(IpqpTraceRestartGrade::kCold), "\"cold\"");

    const auto route = [](IpqpTraceRouteTo t) {
        IpqpTraceRouteEvent e;
        e.to = t;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipqp_route(e);
        return raw_field(os.str(), "to");
    };
    EXPECT_EQ(route(IpqpTraceRouteTo::kRefine), "\"refine\"");
    EXPECT_EQ(route(IpqpTraceRouteTo::kSsn), "\"ssn\"");
    EXPECT_EQ(route(IpqpTraceRouteTo::kWalk), "\"walk\"");

    const auto certify = [](IpqpTraceFinalInertia f) {
        IpqpTraceCertifyEvent e;
        e.final_inertia = f;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipqp_certify(e);
        return raw_field(os.str(), "final_inertia");
    };
    EXPECT_EQ(certify(IpqpTraceFinalInertia::kOk), "\"ok\"");
    EXPECT_EQ(certify(IpqpTraceFinalInertia::kWrong), "\"wrong\"");
    EXPECT_EQ(certify(IpqpTraceFinalInertia::kUnreadable), "\"unreadable\"");

    const auto escape = [](IpqpTraceEscapeReason r) {
        IpqpTraceEscapeEvent e;
        e.reason = r;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipqp_escape(e);
        return raw_field(os.str(), "reason");
    };
    EXPECT_EQ(escape(IpqpTraceEscapeReason::kBudget), "\"budget\"");
    EXPECT_EQ(escape(IpqpTraceEscapeReason::kStall), "\"stall\"");
    EXPECT_EQ(escape(IpqpTraceEscapeReason::kIndefinite), "\"indefinite\"");
    EXPECT_EQ(escape(IpqpTraceEscapeReason::kNumerical), "\"numerical\"");
    EXPECT_EQ(escape(IpqpTraceEscapeReason::kInfeasibleSuspect), "\"infeasible_suspect\"");

    const auto outcome = [](IpqpTraceOutcome o) {
        QpModeTraceEvent e;
        e.outcome = o;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_qp_mode(e);
        return raw_field(os.str(), "outcome");
    };
    EXPECT_EQ(outcome(IpqpTraceOutcome::kOptimal), "\"optimal\"");
    EXPECT_EQ(outcome(IpqpTraceOutcome::kRouted), "\"routed\"");
    EXPECT_EQ(outcome(IpqpTraceOutcome::kEscaped), "\"escaped\"");
    {
        QpModeTraceEvent e;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_qp_mode(e);
        EXPECT_EQ(raw_field(os.str(), "mode"), "\"ipqp\"");
    }

    const auto verdict = [](SqpFallbackVerdict v) {
        SqpFallbackVerdictTraceEvent e;
        e.verdict = v;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_fallback_verdict(e);
        return raw_field(os.str(), "verdict");
    };
    EXPECT_EQ(verdict(SqpFallbackVerdict::kDisproved), "\"disproved\"");
    EXPECT_EQ(verdict(SqpFallbackVerdict::kRelaxed), "\"relaxed\"");
    EXPECT_EQ(verdict(SqpFallbackVerdict::kExhausted), "\"exhausted\"");
    EXPECT_EQ(verdict(SqpFallbackVerdict::kRungB), "\"rung_b\"");
    EXPECT_EQ(verdict(SqpFallbackVerdict::kUnfired), "\"unfired\"");
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
    EXPECT_EQ(a.ssn.ssn_iters, b.ssn.ssn_iters);
    EXPECT_EQ(a.ssn.ssn_escapes, b.ssn.ssn_escapes);
    EXPECT_EQ(a.ipqp.ipqp_iters, b.ipqp.ipqp_iters);
    EXPECT_EQ(a.ipqp.ipqp_escapes, b.ipqp.ipqp_escapes);
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

} // namespace
} // namespace hven::solvers
