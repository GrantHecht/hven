// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_json_lines_trace_sink.cpp -- M6 W4 task 1: JsonLinesTraceSink, schema v0's
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
//         solve reported, on real HS cells at kIpqp.
//
//   (iv)  NULL SINK: attaching the writer to a walk-arm solve moves no counter
//         and writes no line.

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ios>
#include <limits>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <vector>

#include <Eigen/SparseCore>
#include <gtest/gtest.h>

#include <hven/detail/core/aggregate_arity.h>
#include <hven/detail/model/nlp_model.h>
#include <hven/detail/model/problem_access.h>
#include <hven/detail/qp/ipqp/ipqp_engine.h>
#include <hven/detail/qp/walk/walk_engine.h>
#include <hven/detail/sqp/globalization/elastic.h>
#include <hven/detail/sqp/sqp_solver_impl.h>
#include <hven/trace/console_trace_sink.h>
#include <hven/trace/json_lines_trace_sink.h>

// The portable stdout capture both console suites share (M6 W5 T8.7 fix1,
// astra's I2): this file used to carry its own copy, which included <unistd.h>
// and called the POSIX descriptor functions unconditionally -- source that
// cannot be compiled on Windows, where this target is also built.
#include "model/support/hs_problems.h"
#include "qp/support/ipqp_test_support.h"
#include "support/console_capture.h"

namespace hven {
namespace {

using test_support::HsProblem;
using test_support::make_hs;
// W5 T5: these four were replicas of test_sqp_solver.cpp's own W2 fixtures and
// now have ONE home. `w2_escaped_evidence` is gone: its consumers take the
// evidence block by value off the full result.
using test_support::PinnedVariableModel;
using test_support::w2_antiparallel_eq_qp;
using test_support::w2_box_blocked_qp;
using test_support::w2_escaped;

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

IpqpIterTraceEvent golden_iter_present() {
    IpqpIterTraceEvent e;
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

IpqpIterTraceEvent golden_iter_absent() {
    IpqpIterTraceEvent e;
    e.solve = 1;
    e.major = 0;
    e.it = 1;
    e.mu = -0.0;
    e.res_p = std::numeric_limits<double>::quiet_NaN();
    e.res_d = std::numeric_limits<double>::infinity();
    e.res_c = -std::numeric_limits<double>::infinity();
    return e;
}

IpqpEscapeTraceEvent golden_escape() {
    IpqpEscapeTraceEvent e;
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
    // NON-VACUITY for the settler's rule that v0 carries no vector-valued field
    // anywhere: the five Vec members are POPULATED here, so the golden line
    // fails if any of them ever reaches the stream.
    i.least_infeasible_x = Vec::Constant(3, 7.0);
    i.least_infeasible_s = Vec::Constant(2, 8.0);
    i.least_infeasible_lambda_i = Vec::Constant(2, 9.0);
    i.least_infeasible_zl = Vec::Constant(3, 1.0);
    i.least_infeasible_zu = Vec::Constant(3, 2.0);
    return e;
}

// ===========================================================================
// Trace v1: the hand-filled `solve.begin`, `iter` and `solve.end` inputs
// ===========================================================================

/// A depth-0 SQP row with the shared core and the `sqp` block at their defaults.
IterationRecord default_sqp_record() {
    IterationRecord r;
    r.tier = IterationTier::kWalk;
    r.sqp.emplace();
    return r;
}

/// An interior-point row with the shared core and the `ipm` block at their defaults.
IterationRecord default_ipm_record() {
    IterationRecord r;
    r.ipm.emplace();
    return r;
}

ProblemStatistics golden_statistics() {
    ProblemStatistics st;
    st.variables = 11;
    st.free_variables = 1;
    st.lower_bounded_variables = 2;
    st.upper_bounded_variables = 3;
    st.boxed_variables = 4;
    st.fixed_variables = 1;
    st.constraints = 8;
    st.equality_rows = 3;
    st.lower_bounded_rows = 2;
    st.upper_bounded_rows = 1;
    st.range_rows = 1;
    st.free_rows = 1;
    st.absent_bounds = 5;
    return st;
}

SolveBeginTraceEvent default_sqp_begin() {
    SolveBeginTraceEvent e;
    e.engine = TraceEngine::kSqp;
    e.statistics = golden_statistics();
    e.optimality_tol = 1e-6;
    e.feasibility_tol = 1e-7;
    e.max_iterations = 100;
    e.sqp.emplace();
    return e;
}

SolveBeginTraceEvent default_ipm_begin() {
    SolveBeginTraceEvent e;
    e.engine = TraceEngine::kIpm;
    e.statistics = golden_statistics();
    e.optimality_tol = 1e-6;
    e.feasibility_tol = 1e-7;
    e.max_iterations = 100;
    e.ipm.emplace();
    return e;
}

SolveEndTraceEvent default_sqp_end(const SqpCounters &counters) {
    SolveEndTraceEvent e;
    e.engine = TraceEngine::kSqp;
    e.sqp.emplace();
    e.sqp->counters = &counters;
    return e;
}

SolveEndTraceEvent default_ipm_end(const IpmCounters &counters) {
    SolveEndTraceEvent e;
    e.engine = TraceEngine::kIpm;
    e.ipm.emplace();
    e.ipm->counters = &counters;
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
              "{\"v\":1,\"ev\":\"ipqp.iter\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\","
              "\"solve\":3,\"major\":7,"
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
              "{\"v\":1,\"ev\":\"ipqp.iter\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\","
              "\"solve\":1,\"major\":0,"
              "\"it\":1,\"mu\":-0,\"rho\":0,\"delta\":0,\"res_p\":\"nan\",\"res_d\":\"inf\","
              "\"res_c\":\"-inf\",\"sigma\":0,\"alpha_p\":0,\"alpha_d\":0,\"inertia\":null,"
              "\"zero_derived\":false,\"perturbed\":null,\"facts\":\"\"}\n");
}

TEST(JsonLinesTraceSink, GoldenLineIpqpReg) {
    IpqpRegTraceEvent e;
    e.dir = IpqpTraceRegDirection::kDown;
    e.rho = 100.0;
    e.delta = 0.001;
    e.reason = IpqpTraceRegReason::kStall;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipqp_reg(e);
    EXPECT_EQ(os.str(), "{\"v\":1,\"ev\":\"ipqp.reg\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":"
                        "\"sqp\",\"dir\":\"down\","
                        "\"rho\":100,\"delta\":0.001,\"reason\":\"stall\"}\n");
}

TEST(JsonLinesTraceSink, GoldenLineIpqpRestart) {
    IpqpRestartTraceEvent e;
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
    EXPECT_EQ(os.str(), "{\"v\":1,\"ev\":\"ipqp.restart\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":"
                        "\"sqp\",\"grade\":\"full\","
                        "\"repaired\":true,\"shift_p\":0.25,\"shift_d\":0.125,"
                        "\"mu0\":0.10000000000000001,\"mu_payload\":1e-08,\"adopted\":true,"
                        "\"abandoned\":false}\n");
}

TEST(JsonLinesTraceSink, GoldenLineIpqpRoute) {
    IpqpRouteTraceEvent e;
    e.to = IpqpTraceRouteTarget::kSsn;
    e.uncertain = 3;
    e.face_rows = 4;
    e.face_bounds = 2;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipqp_route(e);
    EXPECT_EQ(os.str(), "{\"v\":1,\"ev\":\"ipqp.route\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":"
                        "\"sqp\",\"to\":\"ssn\","
                        "\"uncertain\":3,\"face_rows\":4,\"face_bounds\":2}\n");
}

TEST(JsonLinesTraceSink, GoldenLineIpqpCertify) {
    IpqpCertifyTraceEvent e;
    e.final_inertia = IpqpTraceFinalInertia::kWrong;
    e.downgraded = true;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipqp_certify(e);
    EXPECT_EQ(os.str(),
              "{\"v\":1,\"ev\":\"ipqp.certify\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\","
              "\"final_inertia\":\"wrong\",\"downgraded\":true}\n");
}

TEST(JsonLinesTraceSink, GoldenLineIpqpEscapeCarriesBothTypedEvidenceBlocksAndNoVector) {
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipqp_escape(golden_escape());
    EXPECT_EQ(os.str(),
              "{\"v\":1,\"ev\":\"ipqp.escape\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\","
              "\"reason\":\"infeasible_suspect\","
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
    // RE-DERIVED AT M6 W4 T5 (DECLARED; settler ruling, brief addendum 3). This
    // is the ONE W1/W2 golden line the window moves, and it moves by EXACTLY
    // one trailing key: every byte before `,"site"` is T1's, unchanged.
    //
    // Rule 6 as clarified in docs/trace-schema-v0.md: a frozen event's golden
    // line moves only by a declared additive TRAILING key, never otherwise.
    QpModeTraceEvent e;
    e.mode = QpTraceMode::kIpqp;
    e.outcome = QpTraceOutcome::kEscaped;
    e.iters = 12;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_qp_mode(e);
    EXPECT_EQ(os.str(), "{\"v\":1,\"ev\":\"qp.mode\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":"
                        "\"sqp\",\"mode\":\"ipqp\","
                        "\"outcome\":\"escaped\",\"facts\":\"\",\"iters\":12,"
                        "\"site\":\"dispatch\"}\n");
}

TEST(JsonLinesTraceSink, GoldenLineQpModeAtANonDispatchSite) {
    // THE FALSIFIER FOR THE LINE ABOVE. `kDispatch` is the field's DEFAULT, so
    // that golden cannot tell "the key is written from `event.site`" from "the
    // key is a literal".
    //
    // This one differs in every field, so a hard-coded `dispatch` fails here.
    QpModeTraceEvent e;
    e.mode = QpTraceMode::kWalk;
    e.outcome = QpTraceOutcome::kOptimal;
    e.iters = 3;
    e.site = QpTraceModeSite::kElasticRung;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_qp_mode(e);
    EXPECT_EQ(os.str(), "{\"v\":1,\"ev\":\"qp.mode\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":"
                        "\"sqp\",\"mode\":\"walk\","
                        "\"outcome\":\"optimal\",\"facts\":\"\",\"iters\":3,"
                        "\"site\":\"elastic_rung\"}\n");
}

TEST(JsonLinesTraceSink, GoldenLineFallbackVerdictWithAPlacement) {
    SqpFallbackVerdictTraceEvent e;
    e.entered_rung_a = true;
    e.verdict = SqpTraceFallbackVerdict::kRungB;
    e.rho_0 = 1e6;
    e.rho0_ceiling_hit = true;
    e.floor_retry = true;
    e.qp_minor_iters = 7;
    e.qp_factorizations = 3;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_fallback_verdict(e);
    EXPECT_EQ(
        os.str(),
        "{\"v\":1,\"ev\":\"fallback.verdict\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\","
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
    EXPECT_EQ(
        os.str(),
        "{\"v\":1,\"ev\":\"fallback.verdict\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\","
        "\"entered_rung_a\":false,\"verdict\":\"unfired\",\"rho_0\":null,"
        "\"rho0_ceiling_hit\":false,\"floor_retry\":false,\"qp_minor_iters\":0,"
        "\"qp_factorizations\":0}\n");
}

// ===========================================================================
// R1(c) -- THE COMPILE-TIME NET the production TU cannot provide
// ===========================================================================
// `-Wswitch` in `src/trace/json_lines_trace_sink.cpp` only WARNS: this tree carries no
// `-Werror`, so an enumerator added without a spelling ships a `"unknown"`.
//
// Here it is an ERROR, over one exhaustive switch per enum. An added enumerator
// therefore fails to COMPILE this file, and the spellings below are the ORACLE
// the sink is checked against -- an independent copy, in the test.
//
// CLANG-FAMILY ONLY, which is every lane this project builds: linux clang,
// AppleClang and clang-cl all define `__clang__`. On gcc or MSVC the net is
// absent and an appended enumerator meets only the no-`"unknown"` assertion.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic error "-Wswitch"
#endif

/// The total oracle's out-of-range answer. A `nullptr` here would make
/// `EXPECT_EQ(std::string, nullptr)` undefined behaviour on the very failure
/// the switches exist to report; this reads as a value instead, and matches no
/// spelling the sink can produce.
const char *const kUnspelled = "\"UNSPELLED\"";

const char *spec_spelling(IpqpTraceRegDirection v) {
    switch (v) {
    case IpqpTraceRegDirection::kDown:
        return "\"down\"";
    case IpqpTraceRegDirection::kUp:
        return "\"up\"";
    }
    return kUnspelled;
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
    return kUnspelled;
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
    return kUnspelled;
}

const char *spec_spelling(IpqpTraceRouteTarget v) {
    switch (v) {
    case IpqpTraceRouteTarget::kRefine:
        return "\"refine\"";
    case IpqpTraceRouteTarget::kSsn:
        return "\"ssn\"";
    case IpqpTraceRouteTarget::kWalk:
        return "\"walk\"";
    }
    return kUnspelled;
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
    return kUnspelled;
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
    return kUnspelled;
}

const char *spec_spelling(QpTraceMode v) {
    switch (v) {
    case QpTraceMode::kIpqp:
        return "\"ipqp\"";
    case QpTraceMode::kWalk:
        return "\"walk\"";
    case QpTraceMode::kSsn:
        return "\"ssn\"";
    }
    return kUnspelled;
}

/// M6 W4 T5's fifth alphabet on `qp.mode`.
const char *spec_spelling(QpTraceModeSite v) {
    switch (v) {
    case QpTraceModeSite::kDispatch:
        return "\"dispatch\"";
    case QpTraceModeSite::kSsnWarmGrade:
        return "\"ssn_warm_grade\"";
    case QpTraceModeSite::kFallbackRungB:
        return "\"fallback_rung_b\"";
    case QpTraceModeSite::kElasticRung:
        return "\"elastic_rung\"";
    case QpTraceModeSite::kSocResolve:
        return "\"soc_resolve\"";
    case QpTraceModeSite::kCapRetry:
        return "\"cap_retry\"";
    }
    return kUnspelled;
}

const char *spec_spelling(QpStatus v) {
    switch (v) {
    case QpStatus::kOptimal:
        return "\"optimal\"";
    case QpStatus::kIterationLimit:
        return "\"max_iter\"";
    case QpStatus::kInfeasible:
        return "\"infeasible\"";
    case QpStatus::kNumericalError:
        return "\"numerical_error\"";
    }
    return kUnspelled;
}

// spec_spelling(SqpStatus) stood here. SqpStatus is gone (M6 W5 T8.4) and the
// SolveStatus overload below -- the one the interior-point end event already
// used -- spells the five SQP-reachable values identically, so the two folded
// into one.

const char *spec_spelling(QpWorkingSetAlgebra v) {
    switch (v) {
    case QpWorkingSetAlgebra::kRefactorize:
        return "\"refactorize\"";
    case QpWorkingSetAlgebra::kSchurBorder:
        return "\"schur_border\"";
    }
    return kUnspelled;
}

const char *spec_spelling(StartLevel v) {
    switch (v) {
    case StartLevel::kCold:
        return "\"cold\"";
    case StartLevel::kSeeded:
        return "\"seeded\"";
    case StartLevel::kWarm:
        return "\"warm\"";
    case StartLevel::kHot:
        return "\"hot\"";
    }
    return kUnspelled;
}

const char *spec_spelling(StepVerdict v) {
    switch (v) {
    case StepVerdict::kAcceptObjectiveStep:
        return "\"accept_f\"";
    case StepVerdict::kAcceptFeasibilityStep:
        return "\"accept_h\"";
    case StepVerdict::kReject:
        return "\"reject\"";
    case StepVerdict::kRestore:
        return "\"restore\"";
    }
    return kUnspelled;
}

const char *spec_spelling(QpTraceOutcome v) {
    switch (v) {
    case QpTraceOutcome::kOptimal:
        return "\"optimal\"";
    case QpTraceOutcome::kRouted:
        return "\"routed\"";
    case QpTraceOutcome::kEscaped:
        return "\"escaped\"";
    }
    return kUnspelled;
}

const char *spec_spelling(SqpTraceFallbackVerdict v) {
    switch (v) {
    case SqpTraceFallbackVerdict::kDisproved:
        return "\"disproved\"";
    case SqpTraceFallbackVerdict::kRelaxed:
        return "\"relaxed\"";
    case SqpTraceFallbackVerdict::kExhausted:
        return "\"exhausted\"";
    case SqpTraceFallbackVerdict::kRungB:
        return "\"rung_b\"";
    case SqpTraceFallbackVerdict::kUnfired:
        return "\"unfired\"";
    }
    return kUnspelled;
}

// THE INTERIOR-POINT END EVENT'S STATUS SPELLINGS MOVED IN M6 W5 T8.4, and the
// move is declared: with ConvergenceFlags gone the event carries SolveStatus,
// whose display names are the ones below. `converged` -> `optimal`,
// `not_converged` -> `max_iter` (and the exits the old vocabulary could not tell
// apart now reach `stalled`), `singular_kkt` -> `numerical_error`.
const char *spec_spelling(hven::SolveStatus v) {
    switch (v) {
    case hven::SolveStatus::kOptimal:
        return "\"optimal\"";
    case hven::SolveStatus::kAcceptable:
        return "\"acceptable\"";
    case hven::SolveStatus::kIterationLimit:
        return "\"max_iter\"";
    case hven::SolveStatus::kWorkLimit:
        return "\"work_limit\"";
    case hven::SolveStatus::kTimeLimit:
        return "\"time_limit\"";
    case hven::SolveStatus::kInfeasible:
        return "\"infeasible\"";
    case hven::SolveStatus::kStalled:
        return "\"stalled\"";
    case hven::SolveStatus::kDiverging:
        return "\"diverging\"";
    case hven::SolveStatus::kNumericalError:
        return "\"numerical_error\"";
    case hven::SolveStatus::kInterrupted:
        return "\"interrupted\"";
    }
    return kUnspelled;
}

// M7 W2 M3a fix1. THE COUNTER'S TEN, in an independent copy like every other
// alphabet here: `not_run` plus SolveStatus's nine, spelled EXACTLY as the
// overload above spells them -- written out again rather than forwarded,
// because a forwarding "spec" would agree with the writer by construction and
// pin nothing.
const char *spec_spelling(hven::RestorationSubStatus v) {
    switch (v) {
    case hven::RestorationSubStatus::kNotRun:
        return "\"not_run\"";
    case hven::RestorationSubStatus::kOptimal:
        return "\"optimal\"";
    case hven::RestorationSubStatus::kAcceptable:
        return "\"acceptable\"";
    case hven::RestorationSubStatus::kIterationLimit:
        return "\"max_iter\"";
    case hven::RestorationSubStatus::kWorkLimit:
        return "\"work_limit\"";
    case hven::RestorationSubStatus::kTimeLimit:
        return "\"time_limit\"";
    case hven::RestorationSubStatus::kInfeasible:
        return "\"infeasible\"";
    case hven::RestorationSubStatus::kStalled:
        return "\"stalled\"";
    case hven::RestorationSubStatus::kDiverging:
        return "\"diverging\"";
    case hven::RestorationSubStatus::kNumericalError:
        return "\"numerical_error\"";
    case hven::RestorationSubStatus::kInterrupted:
        return "\"interrupted\"";
    }
    return kUnspelled;
}

const char *spec_spelling(IpmInertiaMode v) {
    switch (v) {
    case IpmInertiaMode::kClassic:
        return "\"classic\"";
    case IpmInertiaMode::kProximalRegularization:
        return "\"proximal_regularization\"";
    }
    return kUnspelled;
}

/// M6 W5 T8.7b's four alphabets: the phase vocabulary `ipm.phase.*` carries,
/// the stop reason and the factorization status `ipm.phase.exit` carries, and
/// the eleven message kinds.
const char *spec_spelling(IpmPhase v) {
    switch (v) {
    case IpmPhase::kOptimize:
        return "\"optimize\"";
    case IpmPhase::kSolve:
        return "\"solve\"";
    }
    return kUnspelled;
}

const char *spec_spelling(StopReason v) {
    // M8 AP-5 G1: ONE vocabulary for both engines; the IPM's three limit
    // reasons fold into `none` and `interrupted` became `callback_stop`.
    switch (v) {
    case StopReason::kNone:
        return "\"none\"";
    case StopReason::kCallbackStop:
        return "\"callback_stop\"";
    case StopReason::kStopRequested:
        return "\"stop_requested\"";
    case StopReason::kException:
        return "\"exception\"";
    case StopReason::kNonFiniteStart:
        return "\"non_finite_start\"";
    case StopReason::kNonFiniteIterate:
        return "\"non_finite_iterate\"";
    case StopReason::kEvaluationFailure:
        return "\"evaluation_failure\"";
    case StopReason::kStageStalled:
        return "\"stage_stalled\"";
    case StopReason::kRestorationLocallyInfeasible:
        return "\"restoration_locally_infeasible\"";
    case StopReason::kSubproblemFailure:
        return "\"subproblem_failure\"";
    case StopReason::kInconsistentEqualities:
        return "\"inconsistent_equalities\"";
    case StopReason::kRestorationRefused:
        return "\"restoration_refused\"";
    case StopReason::kRestorationNonFinite:
        return "\"restoration_non_finite\"";
    case StopReason::kRestorationNoClaim:
        return "\"restoration_no_claim\"";
    case StopReason::kRestorationFailed:
        return "\"restoration_failed\"";
    case StopReason::kSubproblemIterationLimit:
        return "\"subproblem_iteration_limit\"";
    case StopReason::kTrustRegionFloor:
        return "\"trust_region_floor\"";
    case StopReason::kRestorationWorkLimit:
        return "\"restoration_work_limit\"";
    case StopReason::kRestorationNoProgress:
        return "\"restoration_no_progress\"";
    }
    return kUnspelled;
}

const char *spec_spelling(IpmKktFactorStatus v) {
    switch (v) {
    case IpmKktFactorStatus::kSuccess:
        return "\"success\"";
    case IpmKktFactorStatus::kNumericalIssue:
        return "\"numerical_issue\"";
    case IpmKktFactorStatus::kNoConvergence:
        return "\"no_convergence\"";
    case IpmKktFactorStatus::kInvalidInput:
        return "\"invalid_input\"";
    }
    return kUnspelled;
}

const char *spec_spelling(MessageKind v) {
    switch (v) {
    case MessageKind::kSolverInitialized:
        return "\"solver_initialized\"";
    case MessageKind::kRankDeficiency:
        return "\"rank_deficiency\"";
    case MessageKind::kFactorizationHardError:
        return "\"factorization_hard_error\"";
    case MessageKind::kInertiaExhausted:
        return "\"inertia_exhausted\"";
    case MessageKind::kFeasibilityStall:
        return "\"feasibility_stall\"";
    case MessageKind::kPhaseDiverged:
        return "\"phase_diverged\"";
    case MessageKind::kInterruptSkippingPhases:
        return "\"interrupt_skipping_phases\"";
    case MessageKind::kStopRequestedSkippingPhases:
        return "\"stop_requested_skipping_phases\"";
    case MessageKind::kRestorationEntered:
        return "\"restoration_entered\"";
    case MessageKind::kRestorationExited:
        return "\"restoration_exited\"";
    case MessageKind::kRestorationRefused:
        return "\"restoration_refused\"";
    case MessageKind::kSubproblemFailure:
        return "\"subproblem_failure\"";
    case MessageKind::kElasticExhausted:
        return "\"elastic_exhausted\"";
    case MessageKind::kInconsistentEqualities:
        return "\"inconsistent_equalities\"";
    case MessageKind::kStartPointClamped:
        return "\"start_point_clamped\"";
    case MessageKind::kScaledConvergenceRefused:
        return "\"scaled_convergence_refused\"";
    case MessageKind::kStopAtIteration:
        return "\"stop_at_iteration\"";
    }
    return kUnspelled;
}

const char *spec_spelling(MessageSeverity v) {
    switch (v) {
    case MessageSeverity::kNotice:
        return "\"notice\"";
    case MessageSeverity::kWarning:
        return "\"warning\"";
    }
    return kUnspelled;
}

const char *spec_spelling(IterationTier v) {
    switch (v) {
    case IterationTier::kBarrier:
        return "\"bar\"";
    case IterationTier::kIpqp:
        return "\"ipqp\"";
    case IterationTier::kSsn:
        return "\"ssn\"";
    case IterationTier::kWalk:
        return "\"walk\"";
    case IterationTier::kElastic:
        return "\"elastic\"";
    case IterationTier::kRestoration:
        return "\"rest\"";
    }
    return kUnspelled;
}

const char *spec_spelling(SqpHessianMode v) {
    switch (v) {
    case SqpHessianMode::kExact:
        return "\"exact\"";
    case SqpHessianMode::kLbfgsDamped:
        return "\"lbfgs_damped\"";
    }
    return kUnspelled;
}

const char *spec_spelling(SqpColumnScaling v) {
    switch (v) {
    case SqpColumnScaling::kOff:
        return "\"off\"";
    case SqpColumnScaling::kAlways:
        return "\"always\"";
    case SqpColumnScaling::kAuto:
        return "\"auto\"";
    }
    return kUnspelled;
}

const char *spec_spelling(IpmRestorationMode v) {
    switch (v) {
    case IpmRestorationMode::kOff:
        return "\"off\"";
    case IpmRestorationMode::kProximalSwitch:
        return "\"proximal_switch\"";
    case IpmRestorationMode::kL1Nested:
        return "\"l1_nested\"";
    }
    return kUnspelled;
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
    static_assert(static_cast<int>(IpqpTraceRegDirection::kUp) == 2 - 1, "2 dir spellings");
    static_assert(static_cast<int>(IpqpTraceRegReason::kFloor) == 4 - 1, "4 reg reasons");
    static_assert(static_cast<int>(IpqpTraceRestartGrade::kFull) == 3 - 1, "3 grades");
    static_assert(static_cast<int>(IpqpTraceRouteTarget::kWalk) == 3 - 1, "3 route destinations");
    static_assert(static_cast<int>(IpqpTraceFinalInertia::kUnreadable) == 3 - 1, "3 readings");
    static_assert(static_cast<int>(IpqpTraceEscapeReason::kInfeasibleSuspect) == 5 - 1,
                  "5 escape reasons");
    static_assert(static_cast<int>(QpTraceMode::kSsn) == 3 - 1, "3 QP modes");
    static_assert(static_cast<int>(QpStatus::kNumericalError) == 4 - 1, "4 QP statuses");
    static_assert(static_cast<int>(StepVerdict::kRestore) == 4 - 1, "4 step verdicts");
    static_assert(static_cast<int>(SolveStatus::kInterrupted) == 9 - 1, "9 solve statuses");
    static_assert(static_cast<int>(QpWorkingSetAlgebra::kSchurBorder) == 2 - 1,
                  "2 working-set algebras");
    static_assert(static_cast<int>(StartLevel::kHot) == 4 - 1, "4 start levels");
    static_assert(static_cast<int>(QpTraceOutcome::kEscaped) == 3 - 1, "3 outcomes");
    static_assert(static_cast<int>(SqpTraceFallbackVerdict::kUnfired) == 5 - 1, "5 verdicts");
    static_assert(static_cast<int>(IpmInertiaMode::kProximalRegularization) == 2 - 1,
                  "2 inertia modes");
    static_assert(static_cast<int>(IpmRestorationMode::kL1Nested) == 3 - 1, "3 restoration modes");
    // M6 W5 T8.7b's four.
    static_assert(static_cast<int>(IpmPhase::kSolve) == 2 - 1, "2 phases");
    static_assert(static_cast<int>(StopReason::kRestorationNoProgress) == 19 - 1,
                  "19 stop reasons");
    static_assert(static_cast<int>(IpmKktFactorStatus::kInvalidInput) == 4 - 1,
                  "4 factorization statuses");
    static_assert(static_cast<int>(MessageKind::kStopAtIteration) == 17 - 1, "17 message kinds");
    // Trace v1's four additional alphabets.
    static_assert(static_cast<int>(MessageSeverity::kWarning) == 2 - 1, "2 severities");
    static_assert(static_cast<int>(IterationTier::kRestoration) == 6 - 1, "6 tiers");
    static_assert(static_cast<int>(SqpHessianMode::kLbfgsDamped) == 2 - 1, "2 Hessian modes");
    static_assert(static_cast<int>(SqpColumnScaling::kAuto) == 3 - 1, "3 column scalings");

    const auto reg_dir = [](IpqpTraceRegDirection d) {
        IpqpRegTraceEvent e;
        e.dir = d;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipqp_reg(e);
        EXPECT_EQ(raw_field(os.str(), "dir"), spec_spelling(d));
    };
    reg_dir(IpqpTraceRegDirection::kDown);
    reg_dir(IpqpTraceRegDirection::kUp);

    const auto reg_reason = [](IpqpTraceRegReason r) {
        IpqpRegTraceEvent e;
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
        IpqpRestartTraceEvent e;
        e.grade = g;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipqp_restart(e);
        EXPECT_EQ(raw_field(os.str(), "grade"), spec_spelling(g));
    };
    grade(IpqpTraceRestartGrade::kFull);
    grade(IpqpTraceRestartGrade::kBase);
    grade(IpqpTraceRestartGrade::kCold);

    const auto route = [](IpqpTraceRouteTarget t) {
        IpqpRouteTraceEvent e;
        e.to = t;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipqp_route(e);
        EXPECT_EQ(raw_field(os.str(), "to"), spec_spelling(t));
    };
    route(IpqpTraceRouteTarget::kRefine);
    route(IpqpTraceRouteTarget::kSsn);
    route(IpqpTraceRouteTarget::kWalk);

    const auto certify = [](IpqpTraceFinalInertia f) {
        IpqpCertifyTraceEvent e;
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
        IpqpEscapeTraceEvent e;
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

    const auto outcome = [](QpTraceOutcome o) {
        QpModeTraceEvent e;
        e.outcome = o;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_qp_mode(e);
        EXPECT_EQ(raw_field(os.str(), "outcome"), spec_spelling(o));
        EXPECT_EQ(raw_field(os.str(), "mode"), spec_spelling(e.mode));
    };
    outcome(QpTraceOutcome::kOptimal);
    outcome(QpTraceOutcome::kRouted);
    outcome(QpTraceOutcome::kEscaped);

    // W4 T5's `site`, on the same event.
    const auto site = [](QpTraceModeSite v) {
        QpModeTraceEvent e;
        e.site = v;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_qp_mode(e);
        EXPECT_EQ(raw_field(os.str(), "site"), spec_spelling(v));
    };
    site(QpTraceModeSite::kDispatch);
    site(QpTraceModeSite::kSsnWarmGrade);
    site(QpTraceModeSite::kFallbackRungB);
    site(QpTraceModeSite::kElasticRung);
    site(QpTraceModeSite::kSocResolve);

    const auto verdict = [](SqpTraceFallbackVerdict v) {
        SqpFallbackVerdictTraceEvent e;
        e.verdict = v;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_fallback_verdict(e);
        EXPECT_EQ(raw_field(os.str(), "verdict"), spec_spelling(v));
    };
    verdict(SqpTraceFallbackVerdict::kDisproved);
    verdict(SqpTraceFallbackVerdict::kRelaxed);
    verdict(SqpTraceFallbackVerdict::kExhausted);
    verdict(SqpTraceFallbackVerdict::kRungB);
    verdict(SqpTraceFallbackVerdict::kUnfired);

    // W4 T2's three added alphabets, driven through `sqp.major`: the two new
    // QP modes, and the two DRIVER enums the row carries.
    const auto major_mode = [](QpTraceMode m) {
        IterationRecord r = default_sqp_record();
        r.sqp->mode = m;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_iter(IterTraceEvent{r});
        EXPECT_EQ(raw_field(os.str(), "mode"), spec_spelling(m));
    };
    major_mode(QpTraceMode::kIpqp);
    major_mode(QpTraceMode::kWalk);
    major_mode(QpTraceMode::kSsn);

    const auto qp_status = [](QpStatus q) {
        IterationRecord r = default_sqp_record();
        r.sqp->qp_status = q;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_iter(IterTraceEvent{r});
        EXPECT_EQ(raw_field(os.str(), "qp_status"), spec_spelling(q));
    };
    qp_status(QpStatus::kOptimal);
    qp_status(QpStatus::kIterationLimit);
    qp_status(QpStatus::kInfeasible);
    qp_status(QpStatus::kNumericalError);

    const auto step_verdict = [](StepVerdict v) {
        IterationRecord r = default_sqp_record();
        r.sqp->verdict = v;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_iter(IterTraceEvent{r});
        EXPECT_EQ(raw_field(os.str(), "verdict"), spec_spelling(v));
    };
    step_verdict(StepVerdict::kAcceptObjectiveStep);
    step_verdict(StepVerdict::kAcceptFeasibilityStep);
    step_verdict(StepVerdict::kReject);
    step_verdict(StepVerdict::kRestore);

    // W4 T2 fix round 1 (R6): part (c)'s three alphabets, which shipped with a
    // `to_json` case and no net at all.
    const auto solve_status = [](SolveStatus st) {
        const SqpCounters counters;
        SolveEndTraceEvent e = default_sqp_end(counters);
        e.status = st;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_solve_end(e);
        EXPECT_EQ(raw_field(os.str(), "status"), spec_spelling(st));
    };
    solve_status(SolveStatus::kOptimal);
    solve_status(SolveStatus::kIterationLimit);
    solve_status(SolveStatus::kInfeasible);
    solve_status(SolveStatus::kNumericalError);
    solve_status(SolveStatus::kWorkLimit);

    const auto ws_algebra = [](QpWorkingSetAlgebra a) {
        SolveBeginTraceEvent e = default_sqp_begin();
        e.sqp->ws_algebra = a;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_solve_begin(e);
        EXPECT_EQ(raw_field(os.str(), "ws_algebra"), spec_spelling(a));
    };
    ws_algebra(QpWorkingSetAlgebra::kRefactorize);
    ws_algebra(QpWorkingSetAlgebra::kSchurBorder);

    const auto start_level = [](StartLevel lv) {
        SqpCounters counters;
        SolveEndTraceEvent e = default_sqp_end(counters);
        e.sqp->start_level_used = lv;
        counters.start_level_used = lv;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_solve_end(e);
        EXPECT_EQ(raw_field(os.str(), "start_level_used"), spec_spelling(lv));
        EXPECT_EQ(raw_field(raw_field(os.str(), "sqp"), "start_level_used"), spec_spelling(lv))
            << "and in the `sqp` block";
    };
    // M6 W5 T8.7b's four alphabets, driven through the two phase events and
    // `ipm.phase.exit`. The message kinds go through `message`.
    const auto ipm_phase = [](IpmPhase v) {
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipm_phase_begin(IpmPhaseTraceEvent{0, "Phase ", v});
        EXPECT_EQ(raw_field(os.str(), "entry"), spec_spelling(v));
    };
    ipm_phase(IpmPhase::kOptimize);
    ipm_phase(IpmPhase::kSolve);

    const auto stop_reason = [](StopReason v) {
        const IpmIterate row;
        IpmPhaseReport report;
        report.stop_reason = v;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipm_phase_exit(IpmPhaseExitTraceEvent{report, row});
        EXPECT_EQ(raw_field(os.str(), "stop_reason"), spec_spelling(v));
    };
    for (const StopReason v :
         {StopReason::kNone, StopReason::kCallbackStop, StopReason::kStopRequested,
          StopReason::kException, StopReason::kNonFiniteStart, StopReason::kNonFiniteIterate,
          StopReason::kEvaluationFailure, StopReason::kStageStalled,
          StopReason::kRestorationLocallyInfeasible, StopReason::kSubproblemFailure,
          StopReason::kInconsistentEqualities, StopReason::kRestorationRefused,
          StopReason::kRestorationNonFinite, StopReason::kRestorationNoClaim,
          StopReason::kRestorationFailed, StopReason::kSubproblemIterationLimit,
          StopReason::kTrustRegionFloor, StopReason::kRestorationWorkLimit,
          StopReason::kRestorationNoProgress}) {
        stop_reason(v);
    }

    const auto kkt_status = [](IpmKktFactorStatus v) {
        const IpmIterate row;
        const IpmPhaseReport report;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        IpmPhaseExitTraceEvent e{report, row};
        e.kkt_factor_status = v;
        s.on_ipm_phase_exit(e);
        EXPECT_EQ(raw_field(os.str(), "kkt_factor_status"), spec_spelling(v));
    };
    kkt_status(IpmKktFactorStatus::kSuccess);
    kkt_status(IpmKktFactorStatus::kNumericalIssue);
    kkt_status(IpmKktFactorStatus::kNoConvergence);
    kkt_status(IpmKktFactorStatus::kInvalidInput);

    const auto message_kind = [](MessageKind v) {
        MessageTraceEvent e;
        e.kind = v;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_message(e);
        EXPECT_EQ(raw_field(os.str(), "kind"), spec_spelling(v));
    };
    message_kind(MessageKind::kSolverInitialized);
    message_kind(MessageKind::kRankDeficiency);
    message_kind(MessageKind::kFactorizationHardError);
    message_kind(MessageKind::kInertiaExhausted);
    message_kind(MessageKind::kFeasibilityStall);
    message_kind(MessageKind::kPhaseDiverged);
    message_kind(MessageKind::kInterruptSkippingPhases);
    message_kind(MessageKind::kStopRequestedSkippingPhases);
    message_kind(MessageKind::kRestorationEntered);
    message_kind(MessageKind::kRestorationExited);
    message_kind(MessageKind::kRestorationRefused);
    message_kind(MessageKind::kSubproblemFailure);
    message_kind(MessageKind::kElasticExhausted);
    message_kind(MessageKind::kInconsistentEqualities);
    message_kind(MessageKind::kStartPointClamped);
    message_kind(MessageKind::kScaledConvergenceRefused);
    message_kind(MessageKind::kStopAtIteration);

    const auto severity = [](MessageSeverity v) {
        MessageTraceEvent e;
        e.severity = v;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_message(e);
        EXPECT_EQ(raw_field(os.str(), "severity"), spec_spelling(v));
    };
    severity(MessageSeverity::kNotice);
    severity(MessageSeverity::kWarning);

    const auto tier = [](IterationTier v) {
        IterationRecord r = default_sqp_record();
        r.tier = v;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_iter(IterTraceEvent{r});
        EXPECT_EQ(raw_field(os.str(), "tier"), spec_spelling(v));
    };
    tier(IterationTier::kBarrier);
    tier(IterationTier::kIpqp);
    tier(IterationTier::kSsn);
    tier(IterationTier::kWalk);
    tier(IterationTier::kElastic);
    tier(IterationTier::kRestoration);

    const auto hessian_mode = [](SqpHessianMode v) {
        SolveBeginTraceEvent e = default_sqp_begin();
        e.sqp->hessian_mode = v;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_solve_begin(e);
        EXPECT_EQ(raw_field(os.str(), "hessian_mode"), spec_spelling(v));
    };
    hessian_mode(SqpHessianMode::kExact);
    hessian_mode(SqpHessianMode::kLbfgsDamped);

    const auto column_scaling = [](SqpColumnScaling v) {
        SolveBeginTraceEvent e = default_sqp_begin();
        e.sqp->column_scaling = v;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_solve_begin(e);
        EXPECT_EQ(raw_field(os.str(), "column_scaling"), spec_spelling(v));
    };
    column_scaling(SqpColumnScaling::kOff);
    column_scaling(SqpColumnScaling::kAlways);
    column_scaling(SqpColumnScaling::kAuto);

    start_level(StartLevel::kCold);
    start_level(StartLevel::kSeeded);
    start_level(StartLevel::kWarm);
    start_level(StartLevel::kHot);

    // M7 W2 M3a fix1's alphabet: the counters object's SECOND enum, all TEN
    // values including the `not_run` that is the reason the enum exists. Driven
    // through the same end event, so this covers the `key_value` overload the
    // field's own type selects.
    const auto sub_status = [](hven::RestorationSubStatus v) {
        SqpCounters counters;
        counters.restoration_sub_status = v;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_solve_end(default_sqp_end(counters));
        EXPECT_EQ(raw_field(os.str(), "restoration_sub_status"), spec_spelling(v));
    };
    sub_status(hven::RestorationSubStatus::kNotRun);
    sub_status(hven::RestorationSubStatus::kOptimal);
    sub_status(hven::RestorationSubStatus::kAcceptable);
    sub_status(hven::RestorationSubStatus::kIterationLimit);
    sub_status(hven::RestorationSubStatus::kInfeasible);
    sub_status(hven::RestorationSubStatus::kStalled);
    sub_status(hven::RestorationSubStatus::kDiverging);
    sub_status(hven::RestorationSubStatus::kNumericalError);
    sub_status(hven::RestorationSubStatus::kWorkLimit);
    sub_status(hven::RestorationSubStatus::kInterrupted);

    // W4 T4: the interior-point driver's three alphabets. `SolveStatus` is
    // that driver's own exit status; the two mode selectors shape the run.
    const auto ipm_status = [](hven::SolveStatus st) {
        const IpmCounters counters;
        SolveEndTraceEvent e = default_ipm_end(counters);
        e.status = st;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_solve_end(e);
        EXPECT_EQ(raw_field(os.str(), "status"), spec_spelling(st));
    };
    ipm_status(hven::SolveStatus::kOptimal);
    ipm_status(hven::SolveStatus::kAcceptable);
    ipm_status(hven::SolveStatus::kIterationLimit);
    ipm_status(hven::SolveStatus::kDiverging);
    ipm_status(hven::SolveStatus::kNumericalError);

    const auto inertia_mode = [](IpmInertiaMode m) {
        SolveBeginTraceEvent e = default_ipm_begin();
        e.ipm->inertia_mode = m;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_solve_begin(e);
        EXPECT_EQ(raw_field(os.str(), "inertia_mode"), spec_spelling(m));
    };
    inertia_mode(IpmInertiaMode::kClassic);
    inertia_mode(IpmInertiaMode::kProximalRegularization);

    const auto restoration_mode = [](IpmRestorationMode m) {
        SolveBeginTraceEvent e = default_ipm_begin();
        e.ipm->restoration_mode = m;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_solve_begin(e);
        EXPECT_EQ(raw_field(os.str(), "restoration_mode"), spec_spelling(m));
    };
    restoration_mode(IpmRestorationMode::kOff);
    restoration_mode(IpmRestorationMode::kProximalSwitch);
    restoration_mode(IpmRestorationMode::kL1Nested);
}

TEST(JsonLinesTraceSink, StringEscapingIsRfc8259AndUtf8PassesThrough) {
    IpqpIterTraceEvent e;
    e.facts = std::string("\x01\x1f") + "\b\f\r\t" + "\xc3\xa9" + "ok";
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipqp_iter(e);
    EXPECT_EQ(raw_field(os.str(), "facts"), "\"\\u0001\\u001f\\b\\f\\r\\t\xc3\xa9ok\"");
}

TEST(JsonLinesTraceSink, SeqCountsFromOneAndTheSolvePairMovesTheEnvelope) {
    // RE-POINTED AT THE BEGIN/END PATH (W4 T2 fix round 1, R3(c)), which is
    // the real mechanism now.
    //
    // T1's `push_depth`/`pop_depth` are private: a caller moving `depth_`
    // without the open-solve count would desynchronize the two permanently.
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    const SqpCounters counters;
    EXPECT_EQ(sink.depth(), 0);
    sink.on_solve_begin(default_sqp_begin());
    sink.on_ipqp_certify(IpqpCertifyTraceEvent{});
    sink.on_solve_begin(default_sqp_begin());
    sink.on_ipqp_certify(IpqpCertifyTraceEvent{});
    sink.on_solve_end(default_sqp_end(counters));
    sink.on_ipqp_certify(IpqpCertifyTraceEvent{});
    sink.on_solve_end(default_sqp_end(counters));
    const std::vector<std::string> lines = split_lines(os.str());
    ASSERT_EQ(lines.size(), 7u);
    const char *const kDepths[] = {"0", "0", "1", "1", "1", "0", "0"};
    for (std::size_t i = 0; i < lines.size(); ++i) {
        EXPECT_EQ(raw_field(lines[i], "seq"), std::to_string(i + 1));
        EXPECT_EQ(raw_field(lines[i], "depth"), kDepths[i]) << lines[i];
    }
    EXPECT_EQ(sink.lines_written(), 7);
    EXPECT_EQ(sink.depth(), 0) << "the pair is balanced";

    // THE SATURATION LEG, restored at fix round 2 (F4(i)): T1 pinned that an
    // unbalanced POP cannot fabricate a negative depth. Through the begin/end
    // path the same statement is that a stray `end` leaves depth at 0, not -1.
    sink.on_solve_end(default_sqp_end(counters));
    sink.on_ipqp_certify(IpqpCertifyTraceEvent{});
    const std::vector<std::string> after = split_lines(os.str());
    ASSERT_EQ(after.size(), 9u);
    EXPECT_EQ(raw_field(after[7], "depth"), "0") << after[7];
    EXPECT_EQ(raw_field(after[8], "depth"), "0") << after[8];
    EXPECT_EQ(sink.depth(), 0);
}

// ===========================================================================
// (ii) ROUND TRIP -- every double bit-equal
// ===========================================================================

TEST(JsonLinesTraceSink, RoundTripIsBitExactOnTheHardDoubles) {
    // The five the brief names, plus a NaN and both infinities, carried on one
    // event so a single line covers the whole numeric contract.
    IpqpIterTraceEvent e;
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
        const IpqpIterTraceEvent e = golden_iter_present();
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
        const IpqpIterTraceEvent e = golden_iter_absent();
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
        IpqpRestartTraceEvent e;
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
        IpqpRegTraceEvent e;
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
    const IpqpEscapeTraceEvent e = golden_escape();
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
class TeeSink final : public TraceSink {
  public:
    explicit TeeSink(JsonLinesTraceSink &json) : json_(json) {}

    Index iters = 0, regs = 0, restarts = 0, routes = 0;
    Index certifies = 0, escapes = 0, modes = 0, fallbacks = 0;

    void on_ipqp_iter(const IpqpIterTraceEvent &e) override {
        ++iters;
        json_.on_ipqp_iter(e);
    }
    void on_ipqp_reg(const IpqpRegTraceEvent &e) override {
        ++regs;
        json_.on_ipqp_reg(e);
    }
    void on_ipqp_restart(const IpqpRestartTraceEvent &e) override {
        ++restarts;
        json_.on_ipqp_restart(e);
    }
    void on_ipqp_route(const IpqpRouteTraceEvent &e) override {
        ++routes;
        json_.on_ipqp_route(e);
    }
    void on_ipqp_certify(const IpqpCertifyTraceEvent &e) override {
        ++certifies;
        json_.on_ipqp_certify(e);
    }
    void on_ipqp_escape(const IpqpEscapeTraceEvent &e) override {
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
    void on_solve_begin(const SolveBeginTraceEvent &e) override { json_.on_solve_begin(e); }
    void on_iter(const IterTraceEvent &e) override { json_.on_iter(e); }
    void on_message(const MessageTraceEvent &e) override { json_.on_message(e); }
    void on_solve_end(const SolveEndTraceEvent &e) override { json_.on_solve_end(e); }

  private:
    JsonLinesTraceSink &json_;
};

/// Counts the stream's lines by `ev` and asserts `seq` is 1..N contiguous.
std::map<std::string, Index> census(const std::string &stream, const char *sid = "1") {
    std::map<std::string, Index> by_ev;
    Index expected_seq = 0;
    for (const std::string &l : split_lines(stream)) {
        ++expected_seq;
        EXPECT_EQ(raw_field(l, "seq"), std::to_string(expected_seq)) << l;
        EXPECT_EQ(raw_field(l, "v"), "1") << l;
        EXPECT_EQ(raw_field(l, "sid"), sid) << "one depth-0 solve, one sid";
        EXPECT_EQ(raw_field(l, "eng"), "\"sqp\"") << l;
        EXPECT_EQ(raw_field(l, "depth"), "0") << "a solve with no restoration stays at depth 0";
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
        opts.qp_mode = QpMode::kIpqp;
        opts.common.max_iterations = 60;
        const HsProblem p = make_hs(leg.hs);
        SqpSolver driver(opts);
        std::ostringstream os;
        JsonLinesTraceSink json(os);
        TeeSink tee(json);
        driver.attach_trace(&tee);
        const SqpResult sol =
            driver.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());

        const std::map<std::string, Index> by_ev = census(os.str());
        const auto at = [&](const char *k) {
            const auto it = by_ev.find(k);
            return it == by_ev.end() ? Index{0} : it->second;
        };
        SCOPED_TRACE("HS" + std::to_string(leg.hs));
        EXPECT_EQ(json.lines_written(), static_cast<Index>(split_lines(os.str()).size()));
        // THE WHOLE-SOLVE RECORDS: one begin, one end, a row per history entry, and
        // the machine trace's `iter` lines are `iterations + 1` (row 0 is the start).
        EXPECT_EQ(at("solve.begin"), 1);
        EXPECT_EQ(at("solve.end"), 1);
        EXPECT_EQ(at("iter"), sol.iterations + 1);
        EXPECT_EQ(at("iter"), static_cast<Index>(sol.history.size()));

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

/// FIELD-COMPLETE BY GENERATION (W4 T3 fix round 1, review I-3). Driven off the
/// three T2 X-macro tables rather than a hand list, so a counter added later
/// joins the null-sink claim by construction instead of falling outside it --
/// which is how the four W4 T3 folds slipped past the hand list this replaced.
void expect_counters_identical(const SqpCounters &a, const SqpCounters &b) {
#define HVEN_TEST_SAME(f, absent) EXPECT_EQ(a.f, b.f) << #f;
    HVEN_SQP_COUNTERS_FIELDS(HVEN_TEST_SAME)
#undef HVEN_TEST_SAME
#define HVEN_TEST_SAME(f, absent) EXPECT_EQ(a.ssn.f, b.ssn.f) << #f;
    HVEN_SSN_COUNTERS_FIELDS(HVEN_TEST_SAME)
#undef HVEN_TEST_SAME
#define HVEN_TEST_SAME(f, absent) EXPECT_EQ(a.ipqp.f, b.ipqp.f) << #f;
    HVEN_IPQP_COUNTERS_FIELDS(HVEN_TEST_SAME)
#undef HVEN_TEST_SAME
}

TEST(JsonLinesTraceSink, OnAWalkCellTheSinkChangesNoCounterAndWritesOnlyRows) {
    // A REPLAY-CLASS PIN. T1 recorded that the walk arm wrote NOTHING; the pin
    // now records the whole line census beside the unchanged counters: a
    // `solve.begin`/`solve.end` pair, one `iter` line per history row and one
    // `qp.mode` line per major.
    SqpOptions opts;
    opts.qp_mode = QpMode::kWalk;
    opts.common.max_iterations = 60;
    const HsProblem p = make_hs(24);

    SqpSolver bare(opts);
    const SqpResult without =
        bare.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());

    SqpSolver traced(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    traced.attach_trace(&json);
    const SqpResult with =
        traced.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());

    ASSERT_EQ(without.status, with.status);
    ASSERT_GT(without.counters.major_iters, 0) << "non-vacuous: the cell really solves";
    expect_counters_identical(without.counters, with.counters);
    EXPECT_EQ(without.history.size(), with.history.size());
    const std::map<std::string, Index> by_ev = census(os.str());
    EXPECT_EQ(by_ev.at("iter"), static_cast<Index>(with.history.size()));
    EXPECT_EQ(by_ev.at("qp.mode"), with.counters.major_iters);
    EXPECT_EQ(by_ev.at("solve.begin"), 1);
    EXPECT_EQ(by_ev.at("solve.end"), 1);
    EXPECT_EQ(by_ev.size(), 4u) << "a walk solve's whole alphabet after W4 T2";
    EXPECT_EQ(json.lines_written(),
              static_cast<Index>(with.history.size()) + with.counters.major_iters + 2);
}

// ===========================================================================
// R5 -- BOOL FALSIFIERS: a distinct signature ACROSS lines for every bool
// ===========================================================================
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
    IpqpRestartTraceEvent e;
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
    EXPECT_EQ(os.str(), "{\"v\":1,\"ev\":\"ipqp.restart\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":"
                        "\"sqp\",\"grade\":\"base\","
                        "\"repaired\":true,\"shift_p\":1,\"shift_d\":2,\"mu0\":4,\"mu_payload\":8,"
                        "\"adopted\":false,\"abandoned\":false}\n");
}

TEST(JsonLinesTraceSink, GoldenLineFallbackVerdictClampedWithoutARetry) {
    // With the two lines above, entered_rung_a is (T,F,T,T), rho0_ceiling_hit
    // (T,F,T,F) and floor_retry (T,F,F,T) -- so this line and the next catch the
    // rho0_ceiling_hit <-> floor_retry swap, and both against entered_rung_a.
    SqpFallbackVerdictTraceEvent e;
    e.entered_rung_a = true;
    e.verdict = SqpTraceFallbackVerdict::kDisproved;
    e.rho_0 = 250.0;
    e.rho0_ceiling_hit = true;
    e.floor_retry = false;
    e.qp_minor_iters = 5;
    e.qp_factorizations = 2;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_fallback_verdict(e);
    EXPECT_EQ(
        os.str(),
        "{\"v\":1,\"ev\":\"fallback.verdict\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\","
        "\"entered_rung_a\":true,\"verdict\":\"disproved\",\"rho_0\":250,"
        "\"rho0_ceiling_hit\":true,\"floor_retry\":false,\"qp_minor_iters\":5,"
        "\"qp_factorizations\":2}\n");
}

TEST(JsonLinesTraceSink, GoldenLineFallbackVerdictRetriedAtTheFloorWithoutAClamp) {
    // The retry's placement IS the floor (kElasticRhoInit), which is why 100 is
    // the value here rather than an arbitrary one.
    SqpFallbackVerdictTraceEvent e;
    e.entered_rung_a = true;
    e.verdict = SqpTraceFallbackVerdict::kExhausted;
    e.rho_0 = kElasticRhoInit;
    e.rho0_ceiling_hit = false;
    e.floor_retry = true;
    e.qp_minor_iters = 11;
    e.qp_factorizations = 6;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_fallback_verdict(e);
    EXPECT_EQ(
        os.str(),
        "{\"v\":1,\"ev\":\"fallback.verdict\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\","
        "\"entered_rung_a\":true,\"verdict\":\"exhausted\",\"rho_0\":100,"
        "\"rho0_ceiling_hit\":false,\"floor_retry\":true,\"qp_minor_iters\":11,"
        "\"qp_factorizations\":6}\n");
}

TEST(JsonLinesTraceSink, GoldenLineIpqpEscapeStallBranch) {
    // The `stall.fired == true` branch, which round 1 never reached. Across the
    // three escape lines: stall.fired (F,T,F), infeasibility.fired (T,F,T),
    // exhaustion_route (F,F,T), farkas_corroborated (T,F,F) -- all distinct.
    IpqpEscapeTraceEvent e;
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
              "{\"v\":1,\"ev\":\"ipqp.escape\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\","
              "\"reason\":\"stall\","
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
    IpqpEscapeTraceEvent e;
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
    EXPECT_EQ(os.str(),
              "{\"v\":1,\"ev\":\"ipqp.escape\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\","
              "\"reason\":\"infeasible_suspect\","
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
        sink.on_ipqp_certify(IpqpCertifyTraceEvent{});
        EXPECT_FALSE(sink.failed());
        EXPECT_EQ(sink.lines_written(), 1);
    }
    FailAfterBuf buf(20); // shorter than one line, so the first write fails
    std::ostream out(&buf);
    JsonLinesTraceSink sink(out);
    sink.on_ipqp_certify(IpqpCertifyTraceEvent{});
    EXPECT_TRUE(sink.failed());
    EXPECT_EQ(sink.lines_written(), 1);
    sink.on_ipqp_certify(IpqpCertifyTraceEvent{});
    sink.on_ipqp_certify(IpqpCertifyTraceEvent{});
    EXPECT_TRUE(sink.failed()) << "sticky: it never reads back false";
    EXPECT_EQ(sink.lines_written(), 3) << "seq advances, so the gap counts the lost lines";
    EXPECT_LT(count_lines(buf.taken), sink.lines_written());
}

TEST(JsonLinesTraceSink, AStreamThatFailsMidSolveDoesNotChangeTheSolve) {
    // THE INSTRUMENTATION INVARIANT, under the DEFAULT exception mask: the solve
    // completes and its counters are the bare solve's, byte for byte, while the
    // artifact is short and `failed()` says so.
    SqpOptions opts;
    opts.qp_mode = QpMode::kIpqp;
    opts.common.max_iterations = 60;
    const HsProblem p = make_hs(38);

    SqpSolver bare(opts);
    const SqpResult without =
        bare.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());

    FailAfterBuf buf(4096); // a few lines in, then ENOSPC
    std::ostream out(&buf);
    SqpSolver traced(opts);
    JsonLinesTraceSink json(out);
    traced.attach_trace(&json);
    const SqpResult with =
        traced.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());

    ASSERT_EQ(without.status, with.status);
    expect_counters_identical(without.counters, with.counters);
    EXPECT_TRUE(json.failed());
    ASSERT_GT(count_lines(buf.taken), 0) << "non-vacuous: the failure is MID-solve";
    EXPECT_LT(count_lines(buf.taken), json.lines_written()) << "the gap is the lines lost";
}

TEST(JsonLinesTraceSink, AnArmedExceptionMaskPropagatesOutOfTheSolveByDesign) {
    // THE SECOND CASE. A caller who arms a mask has ASKED for exceptions; the
    // sink neither swallows nor re-labels one, and nothing in the driver catches
    // it today.
    //
    // NO RULE FORBIDS ONE. `sqp_solver.cpp:1746`'s `catch (const std::exception
    // &)` would already swallow `std::ios_base::failure` if an emit site ever
    // moved inside its try, and THIS PIN is the only thing that would notice.
    SqpOptions opts;
    opts.qp_mode = QpMode::kIpqp;
    opts.common.max_iterations = 60;
    const HsProblem p = make_hs(38);
    FailAfterBuf buf(4096);
    std::ostream out(&buf);
    out.exceptions(std::ios::badbit);
    SqpSolver traced(opts);
    JsonLinesTraceSink json(out);
    traced.attach_trace(&json);
    EXPECT_THROW(traced.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point()),
                 std::ios_base::failure);
}

// ===========================================================================
// M7 W0 T5(c) -- flush(), and the destructor's guarded flush
// ===========================================================================

/// @brief A `streambuf` that COUNTS syncs and can be made to refuse them.
///
/// `sync()` is what `std::ostream::flush()` reaches through `pubsync()`, so the
/// counter is a direct reading of "how many times was this stream flushed", and
/// `fail_sync` is the only way to make a flush -- as opposed to a write -- fail.
/// Writes always succeed here, which keeps the two failure paths apart.
class CountingSyncBuf : public std::streambuf {
  public:
    int syncs = 0;
    bool fail_sync = false;
    std::string taken;

  protected:
    int sync() override {
        ++syncs;
        return fail_sync ? -1 : 0;
    }
    std::streamsize xsputn(const char *s, std::streamsize n) override {
        taken.append(s, static_cast<std::size_t>(n));
        return n;
    }
    int_type overflow(int_type c) override {
        if (!traits_type::eq_int_type(c, traits_type::eof())) {
            taken.push_back(static_cast<char>(c));
        }
        return traits_type::not_eof(c);
    }
};

TEST(JsonLinesTraceSink, FlushSyncsOnceAndDestructionSyncsOnceMore) {
    CountingSyncBuf buf;
    std::ostream out(&buf);
    std::string after_flush;
    {
        JsonLinesTraceSink sink(out);
        sink.on_ipqp_certify(IpqpCertifyTraceEvent{});
        EXPECT_EQ(buf.syncs, 0) << "the sink does not flush per line";
        sink.flush();
        EXPECT_EQ(buf.syncs, 1);
        EXPECT_FALSE(sink.failed());
        EXPECT_EQ(sink.lines_written(), 1) << "a flush writes no line and moves no seq";
        after_flush = buf.taken;
    }
    EXPECT_EQ(buf.syncs, 2) << "destruction flushes once more";
    EXPECT_EQ(buf.taken, after_flush) << "flushing changes no byte of the artifact";
    EXPECT_EQ(count_lines(buf.taken), 1);
}

TEST(JsonLinesTraceSink, UnderTheDefaultMaskAFailingFlushSetsFailedAndDoesNotThrow) {
    CountingSyncBuf buf;
    buf.fail_sync = true;
    std::ostream out(&buf);
    JsonLinesTraceSink sink(out);
    sink.on_ipqp_certify(IpqpCertifyTraceEvent{});
    EXPECT_FALSE(sink.failed()) << "the WRITE succeeded; only the flush will fail";
    EXPECT_NO_THROW(sink.flush());
    EXPECT_TRUE(sink.failed()) << "a flush failure reads exactly like a write failure";
}

TEST(JsonLinesTraceSink, UnderAnArmedMaskFlushThrows) {
    // THE FIRST HALF of the armed contract: flush() is an emitting call like any
    // `on_*`, and a caller who armed a mask is told.
    CountingSyncBuf buf;
    buf.fail_sync = true;
    std::ostream out(&buf);
    out.exceptions(std::ios::badbit);
    JsonLinesTraceSink sink(out);
    sink.on_ipqp_certify(IpqpCertifyTraceEvent{});
    EXPECT_THROW(sink.flush(), std::ios_base::failure);
    EXPECT_EQ(buf.syncs, 1);
    // The sink is destroyed at the end of this test. Its flush finds the stream
    // ALREADY bad, so `std::ostream::flush`'s sentry refuses before reaching
    // `pubsync` and nothing is thrown to swallow -- which is why the destructor
    // is exercised against a stream that is still good in the test below.
}

TEST(JsonLinesTraceSink, UnderAnArmedMaskAFailingFlushATDESTRUCTIONIsSwallowed) {
    // THE REASON THE DESTRUCTOR'S FLUSH IS GUARDED, proven rather than asserted:
    // the stream is GOOD when the sink goes out of scope and the flush fails
    // there for the first time, so the destructor really does face a live
    // `std::ios_base::failure`. Unguarded, that would propagate out of a
    // destructor; here it is dropped and this test simply returns.
    CountingSyncBuf buf;
    std::ostream out(&buf);
    out.exceptions(std::ios::badbit);
    {
        JsonLinesTraceSink sink(out);
        sink.on_ipqp_certify(IpqpCertifyTraceEvent{});
        ASSERT_TRUE(out.good()) << "non-vacuous: the write itself succeeded";
        buf.fail_sync = true;
    }
    EXPECT_EQ(buf.syncs, 1) << "destruction did reach the stream";
    EXPECT_TRUE(out.bad()) << "and the failure it swallowed is still visible on the stream";
}

// ===========================================================================
// R6 -- the null-sink comparison on the arm where the sink actually writes
// ===========================================================================

TEST(JsonLinesTraceSink, OnHS38AtKIpmTheSinkWritesHundredsOfLinesAndStillMovesNoCounter) {
    // The kWalk pin below records that the walk arm emits nothing. THIS one is
    // the question the brief actually asks: attaching a sink to the arm where it
    // formats every event moves no counter either.
    SqpOptions opts;
    opts.qp_mode = QpMode::kIpqp;
    opts.common.max_iterations = 60;
    const HsProblem p = make_hs(38);

    SqpSolver bare(opts);
    const SqpResult without =
        bare.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());

    SqpSolver traced(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    traced.attach_trace(&json);
    const SqpResult with =
        traced.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());

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

TEST(JsonLinesTraceSink, ThePinnedDeclineIsWhyTheEntryCountSubtractsDeclinedPinned) {
    // On HS11 and HS38 `ipqp_declined_pinned` is 0, so the whole-solve pin's
    // subtraction is never exercised there. HERE every walk route IS a decline,
    // so the stream carries ZERO entries against a positive `ipqp_to_walk`.
    SqpOptions opts;
    opts.qp_mode = QpMode::kIpqp;
    opts.common.max_iterations = 60;
    PinnedVariableModel model(true);
    SqpSolver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());

    const IpqpCounters &c = sol.counters.ipqp;
    ASSERT_EQ(sol.status, SolveStatus::kOptimal);
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

TEST(JsonLinesTraceSink, TheVerdictStreamReproducesTheWHOLEPartitionOnAFiveClassPopulation) {
    // W2 T5's own five-class population (test_sqp_solver.cpp:9806), driven
    // through the sink the way the driver drives it (sqp_solver.cpp:3915: the
    // judge fills the out-param, the caller emits it).
    //
    // Plus a SIXTH entry declined ABOVE the floor, so `elastic_floor_retries` is
    // non-zero. Round 1 asserted this partition only on HS11/HS38, where
    // disproved, rung_b and floor_retries are all 0 -- four times `0 == 0`.
    const QpProblem feasible = w2_box_blocked_qp(10.0);
    const QpProblem blocked = w2_box_blocked_qp(0.5);
    const QpProblem antiparallel = w2_antiparallel_eq_qp();
    const IpqpInfeasibilityEvidence blocked_ev = w2_escaped(blocked).infeasibility_evidence;
    const IpqpInfeasibilityEvidence antiparallel_ev =
        w2_escaped(antiparallel).infeasibility_evidence;
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
    const QpSolveOverrides overrides;
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    for (const Entry &e : entries) {
        SqpOptions opts;
        opts.qp.trust_region_radius = e.engine_tr;
        QpEngine engine(opts.qp, 0, hven::linear::DensePolicy::kNever);
        SqpMajorRow row;
        std::optional<ElasticLadderReport> report;
        SqpFallbackVerdictTraceEvent verdict;
        certified_feasibility_fallback(engine, *e.qp, nlp_ev, nullptr, *e.evidence, overrides, opts,
                                       inf, out, row, report, verdict);
        json.on_fallback_verdict(verdict);
    }

    const std::map<std::string, Index> by_ev = census(os.str(), "0");
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

/// The `mode` token back as its enumerator -- the pin compares through
/// `spec_spelling`, so this is the inverse the comparison needs.
QpTraceMode mode_of_token(const std::string &token) {
    if (token == "\"ipqp\"") {
        return QpTraceMode::kIpqp;
    }
    if (token == "\"ssn\"") {
        return QpTraceMode::kSsn;
    }
    return QpTraceMode::kWalk;
}

// ===========================================================================
// Trace v1 `iter`, the SQP block: THE ROW, IN CALLER UNITS
// ===========================================================================
// FOUR LINES, NOT ONE, AND THE REASON IS THE TWELVE BOOLS: one line cannot tell
// two `true`s apart. Bool i (1-based, in `set_sqp_signature`'s order) is true on line
// j (0..3) iff bit j of i is set, so all twelve carry DISTINCT four-line
// signatures and any swap of two of them changes at least one line's bytes. In
// particular `tr_binding` (7) and `cap_binding` (12) each hold true on a line where
// the other is false, so a writer swapping the two keys changes bytes.

/// The twelve bools of an SQP row (core plus block), in a stable slot order so a
/// swap in the writer is visible.
void set_sqp_signature(IterationRecord &r, int line) {
    const auto bit = [line](int i) { return ((i >> line) & 1) != 0; };
    r.recovering = bit(1);
    r.accepted = bit(2);
    r.soc = bit(3);
    r.watchdog = bit(4);
    r.rolled_back = bit(5);
    r.sqp->qp_solved = bit(6);
    r.sqp->tr_binding = bit(7);
    r.sqp->elastic = bit(8);
    r.sqp->elastic_clamped = bit(9);
    r.sqp->restoration_seed_used = bit(10);
    r.sqp->ipqp_farkas_corroborated = bit(11);
    r.sqp->cap_binding = bit(12);
}

IterationRecord golden_sqp_row(int line) {
    IterationRecord r = default_sqp_record();
    set_sqp_signature(r, line);
    switch (line) {
    case 0:
        r.iteration = 4;
        r.tier = IterationTier::kWalk;
        r.f = -2.5;
        r.stationarity = 1e-7;
        r.tested_stationarity = 1e-7;
        r.feasibility = 0.25;
        r.tested_feasibility = 0.25;
        r.complementarity = 0.125;
        r.tested_complementarity = 0.125;
        r.step_norm = 0.75;
        r.factorizations = 31;
        r.sqp->mode = QpTraceMode::kWalk;
        r.sqp->verdict = StepVerdict::kAcceptObjectiveStep;
        r.sqp->trust_radius = 2.0;
        r.sqp->violation_l1 = 1.5;
        r.sqp->regularization_dual = 1e-8;
        r.sqp->qp_status = QpStatus::kIterationLimit;
        r.sqp->qp_minor_iterations = 9;
        r.sqp->qp_factorizations = 3;
        r.sqp->ipqp_least_infeasible_primal = 3.25;
        r.sqp->active_set_size = 10;
        r.sqp->active_set_delta = 17;
        r.sqp->weak_active_rows = 1;
        r.sqp->near_active_rows = 2;
        r.sqp->active_rows = 3;
        r.sqp->active_lower_sides = 5;
        r.sqp->active_upper_sides = 8;
        r.sqp->switching_ratio = 1.25;
        r.sqp->pred_df = 0.5;
        r.sqp->f_trial = -2.25;
        r.sqp->h_trial = 0.375;
        r.sqp->h_lin = 0.0625;
        r.sqp->qp_minor_iterations_total = 7;
        r.sqp->qp_factorizations_total = 4;
        r.sqp->factorizations_delta = 5;
        r.sqp->t_s = 1.5;
        break;
    case 1:
        // The numeric contract: -0.0, a subnormal, DBL_MAX, 1e-300 and the three
        // non-finite spellings, on the row's own doubles.
        r.iteration = 0;
        r.tier = IterationTier::kSsn;
        r.f = 0.1;
        r.stationarity = -0.0;
        r.tested_stationarity = -0.0;
        r.feasibility = std::numeric_limits<double>::quiet_NaN();
        r.tested_feasibility = std::numeric_limits<double>::quiet_NaN();
        r.complementarity = std::numeric_limits<double>::infinity();
        r.tested_complementarity = std::numeric_limits<double>::infinity();
        r.step_norm = 0.0;
        r.sqp->mode = QpTraceMode::kSsn;
        r.sqp->verdict = StepVerdict::kRestore;
        r.sqp->trust_radius = 1.7976931348623157e308;
        r.sqp->violation_l1 = 1e-300;
        r.sqp->regularization_dual = 4.9406564584124654e-324;
        r.sqp->qp_status = QpStatus::kInfeasible;
        r.sqp->qp_minor_iterations = 0;
        r.sqp->qp_factorizations = 0;
        r.sqp->ipqp_least_infeasible_primal = -std::numeric_limits<double>::infinity();
        r.sqp->active_set_size = 6;
        r.sqp->active_set_delta = 6;
        r.sqp->weak_active_rows = 5;
        r.sqp->near_active_rows = 4;
        r.sqp->active_rows = 3;
        r.sqp->active_lower_sides = 2;
        r.sqp->active_upper_sides = 1;
        r.sqp->switching_ratio = std::numeric_limits<double>::infinity();
        break;
    case 2:
        r.iteration = 11;
        r.tier = IterationTier::kIpqp;
        r.f = 1234.5;
        r.stationarity = 1e-12;
        r.tested_stationarity = 1e-12;
        r.feasibility = 6.25;
        r.tested_feasibility = 6.25;
        r.complementarity = 7.5;
        r.tested_complementarity = 7.5;
        r.step_norm = 12.5;
        r.factorizations = 7;
        r.sqp->mode = QpTraceMode::kIpqp;
        r.sqp->verdict = StepVerdict::kAcceptFeasibilityStep;
        r.sqp->trust_radius = 0.03125;
        r.sqp->violation_l1 = 9.0;
        r.sqp->regularization_dual = 1e-6;
        r.sqp->qp_status = QpStatus::kNumericalError;
        r.sqp->qp_minor_iterations = 21;
        r.sqp->qp_factorizations = 13;
        r.sqp->ipqp_least_infeasible_primal = -1.5;
        r.sqp->active_set_size = 9;
        r.sqp->active_set_delta = 101;
        r.sqp->near_active_rows = 7;
        r.sqp->active_lower_sides = 9;
        break;
    default:
        r.iteration = 2;
        r.tier = IterationTier::kElastic;
        r.f = 3.0;
        r.stationarity = 4.0;
        r.tested_stationarity = 4.0;
        r.feasibility = 5.0;
        r.tested_feasibility = 5.0;
        r.complementarity = 6.0;
        r.tested_complementarity = 6.0;
        r.step_norm = 11.0;
        r.factorizations = 14;
        r.sqp->mode = QpTraceMode::kWalk;
        r.sqp->verdict = StepVerdict::kReject;
        r.sqp->trust_radius = 9.0;
        r.sqp->violation_l1 = 8.0;
        r.sqp->regularization_dual = 10.0;
        r.sqp->qp_status = QpStatus::kOptimal;
        r.sqp->qp_minor_iterations = 13;
        r.sqp->qp_factorizations = 14;
        r.sqp->ipqp_least_infeasible_primal = 12.0;
        r.sqp->active_set_size = 57;
        r.sqp->active_set_delta = 15;
        r.sqp->weak_active_rows = 16;
        r.sqp->near_active_rows = 17;
        r.sqp->active_rows = 18;
        r.sqp->active_lower_sides = 19;
        r.sqp->active_upper_sides = 20;
        r.sqp->switching_ratio = -0.5;
        r.sqp->soc_f = -0.75;
        r.sqp->soc_h = 0.125;
        break;
    }
    return r;
}

/// The one line `r` writes, from a fresh sink.
std::string iter_line(const IterationRecord &r) {
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_iter(IterTraceEvent{r});
    return os.str();
}

TEST(JsonLinesTraceSink, GoldenLineIterSqpFirstBoolSignatureAndTheWalkTier) {
    EXPECT_EQ(
        iter_line(golden_sqp_row(0)),
        "{\"v\":1,\"ev\":\"iter\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\",\"iter\":4,"
        "\"recovering\":true,\"tier\":\"walk\",\"f\":-2.5,\"f_recovery\":null,\"stationarity\":9."
        "9999999999999995e-08,\"feasibility\":0.25,\"complementarity\":0.125,\"step\":0.75,"
        "\"accepted\":false,\"soc\":true,\"watchdog\":false,\"rolled_back\":true,"
        "\"factorizations\":31,\"stationarity_caller\":9.9999999999999995e-08,\"feasibility_"
        "caller\":0.25,\"complementarity_caller\":0.125,\"sqp\":{\"depth\":0,\"mode\":\"walk\","
        "\"verdict\":\"accept_f\","
        "\"trust_radius\":2,\"violation_l1\":1.5,\"regularization_dual\":1e-08,\"qp_solved\":false,"
        "\"qp_status\":\"max_iter\",\"qp_minor_iterations\":9,\"qp_factorizations\":3,\"tr_"
        "binding\":true,\"elastic\":false,\"elastic_clamped\":true,\"restoration_seed_used\":false,"
        "\"ipqp_least_infeasible_primal\":3.25,\"ipqp_farkas_corroborated\":true,\"active_set_"
        "size\":10,\"active_set_delta\":17,\"weak_active_rows\":1,\"near_active_rows\":2,\"active_"
        "rows\":3,\"active_lower_sides\":5,\"active_upper_sides\":8,\"switching_ratio\":1.25,"
        "\"ladder\":null,"
        "\"cap_binding\":false,\"pred_df\":0.5,\"f_trial\":-2.25,\"h_trial\":0.375,\"soc_f\":null,"
        "\"soc_h\":null,\"h_lin\":0.0625,\"qp_minor_iterations_total\":7,\"qp_factorizations_"
        "total\":"
        "4,\"factorizations_delta\":5,\"t_s\":1.5}}\n");
}

TEST(JsonLinesTraceSink, GoldenLineIterSqpSecondBoolSignatureTheHardDoublesAndTheSsnTier) {
    EXPECT_EQ(iter_line(golden_sqp_row(1)),
              "{\"v\":1,\"ev\":\"iter\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\",\"iter\":0,"
              "\"recovering\":false,\"tier\":\"ssn\",\"f\":0.10000000000000001,\"f_recovery\":null,"
              "\"stationarity\":-0,\"feasibility\":\"nan\",\"complementarity\":\"inf\",\"step\":0,"
              "\"accepted\":true,\"soc\":true,\"watchdog\":false,\"rolled_back\":false,"
              "\"factorizations\":0,\"stationarity_caller\":-0,\"feasibility_caller\":\"nan\","
              "\"complementarity_caller\":\"inf\",\"sqp\":{\"depth\":0,\"mode\":\"ssn\","
              "\"verdict\":\"restore\","
              "\"trust_radius\":1.7976931348623157e+308,\"violation_l1\":1e-300,\"regularization_"
              "dual\":4.9406564584124654e-324,\"qp_solved\":true,\"qp_status\":"
              "\"infeasible\",\"qp_minor_iterations\":0,\"qp_factorizations\":0,\"tr_binding\":"
              "true,\"elastic\":false,\"elastic_clamped\":false,\"restoration_seed_used\":true,"
              "\"ipqp_least_infeasible_primal\":\"-inf\",\"ipqp_farkas_corroborated\":true,"
              "\"active_set_size\":6,\"active_set_delta\":6,\"weak_active_rows\":5,\"near_active_"
              "rows\":4,\"active_rows\":3,\"active_lower_sides\":2,\"active_upper_sides\":1,"
              "\"switching_ratio\":\"inf\",\"ladder\":null,"
              "\"cap_binding\":false,\"pred_df\":null,\"f_trial\":null,\"h_trial\":null,\"soc_f\":"
              "null,\"soc_h\":null,\"h_lin\":null,\"qp_minor_iterations_total\":0,\"qp_"
              "factorizations_total\":0,\"factorizations_delta\":0,\"t_s\":0}}\n");
}

TEST(JsonLinesTraceSink, GoldenLineIterSqpThirdBoolSignatureAndTheIpqpTier) {
    EXPECT_EQ(iter_line(golden_sqp_row(2)),
              "{\"v\":1,\"ev\":\"iter\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\",\"iter\":"
              "11,\"recovering\":false,\"tier\":\"ipqp\",\"f\":1234.5,\"f_recovery\":null,"
              "\"stationarity\":9.9999999999999998e-13,\"feasibility\":6.25,\"complementarity\":7."
              "5,\"step\":12.5,\"accepted\":false,\"soc\":false,\"watchdog\":true,\"rolled_back\":"
              "true,\"factorizations\":7,\"stationarity_caller\":9.9999999999999998e-13,"
              "\"feasibility_caller\":6.25,\"complementarity_caller\":7.5,\"sqp\":{\"depth\":0,"
              "\"mode\":\"ipqp\",\"verdict\":"
              "\"accept_h\",\"trust_radius\":0.03125,\"violation_l1\":9,\"regularization_dual\":9."
              "9999999999999995e-07,\"qp_solved\":true,\"qp_status\":\"numerical_error\",\"qp_"
              "minor_iterations\":21,\"qp_factorizations\":13,\"tr_binding\":true,\"elastic\":"
              "false,\"elastic_clamped\":false,\"restoration_seed_used\":false,\"ipqp_least_"
              "infeasible_primal\":-1.5,\"ipqp_farkas_corroborated\":false,\"active_set_size\":9,"
              "\"active_set_delta\":101,\"weak_active_rows\":0,\"near_active_rows\":7,\"active_"
              "rows\":0,\"active_lower_sides\":9,\"active_upper_sides\":0,\"switching_ratio\":null,"
              "\"ladder\":null,"
              "\"cap_binding\":true,\"pred_df\":null,\"f_trial\":null,\"h_trial\":null,\"soc_f\":"
              "null,\"soc_h\":null,\"h_lin\":null,\"qp_minor_iterations_total\":0,\"qp_"
              "factorizations_total\":0,\"factorizations_delta\":0,\"t_s\":0}}\n");
}

TEST(JsonLinesTraceSink, GoldenLineIterSqpFourthBoolSignatureAndTheElasticTier) {
    EXPECT_EQ(iter_line(golden_sqp_row(3)),
              "{\"v\":1,\"ev\":\"iter\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\",\"iter\":2,"
              "\"recovering\":false,\"tier\":\"elastic\",\"f\":3,\"f_recovery\":null,"
              "\"stationarity\":4,\"feasibility\":5,\"complementarity\":6,\"step\":11,\"accepted\":"
              "false,\"soc\":false,\"watchdog\":false,\"rolled_back\":false,\"factorizations\":14,"
              "\"stationarity_caller\":4,\"feasibility_caller\":5,\"complementarity_caller\":6,"
              "\"sqp\":{\"depth\":0,\"mode\":\"walk\",\"verdict\":\"reject\",\"trust_radius\":9,"
              "\"violation_l1\":8,\"regularization_dual\":10,\"qp_solved\":false,\"qp_status\":"
              "\"optimal\",\"qp_minor_iterations\":13,\"qp_factorizations\":14,\"tr_binding\":"
              "false,\"elastic\":true,\"elastic_clamped\":true,\"restoration_seed_used\":true,"
              "\"ipqp_least_infeasible_primal\":12,\"ipqp_farkas_corroborated\":true,\"active_set_"
              "size\":57,\"active_set_delta\":15,\"weak_active_rows\":16,\"near_active_rows\":17,"
              "\"active_rows\":18,\"active_lower_sides\":19,\"active_upper_sides\":20,\"switching_"
              "ratio\":-0.5,\"ladder\":null,"
              "\"cap_binding\":true,\"pred_df\":null,\"f_trial\":null,\"h_trial\":null,\"soc_f\":"
              "-0.75,\"soc_h\":0.125,\"h_lin\":null,\"qp_minor_iterations_total\":0,\"qp_"
              "factorizations_total\":0,\"factorizations_delta\":0,\"t_s\":0}}\n");
}

TEST(JsonLinesTraceSink, GoldenLineIterSqpRowWithoutASubproblemWritesNulls) {
    // A row no subproblem produced (the stopped-AT-iterate row, or a routed QP
    // failure): the step, the verdict and the four QP figures are `null`, not
    // the zeros and the `reject` default the struct happens to hold.
    IterationRecord r = default_sqp_record();
    r.iteration = 5;
    r.f = 2.0;
    r.stationarity = 0.5;
    r.tested_stationarity = 0.5;
    r.feasibility = 0.25;
    r.tested_feasibility = 0.25;
    r.complementarity = 0.125;
    r.tested_complementarity = 0.125;
    r.factorizations = 9;
    r.sqp->trust_radius = 0.5;
    r.sqp->violation_l1 = 0.75;
    r.sqp->active_set_size = 1;
    r.sqp->active_rows = 1;
    EXPECT_EQ(iter_line(r),
              "{\"v\":1,\"ev\":\"iter\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\",\"iter\":5,"
              "\"recovering\":false,\"tier\":\"walk\",\"f\":2,\"f_recovery\":null,\"stationarity\":"
              "0.5,\"feasibility\":0.25,\"complementarity\":0.125,\"step\":null,\"accepted\":null,"
              "\"soc\":false,\"watchdog\":false,\"rolled_back\":false,\"factorizations\":9,"
              "\"stationarity_caller\":0.5,\"feasibility_caller\":0.25,\"complementarity_caller\":"
              "0.125,\"sqp\":"
              "{\"depth\":0,\"mode\":\"walk\",\"verdict\":null,\"trust_radius\":0.5,\"violation_"
              "l1\":0.75,\"regularization_dual\":null,\"qp_solved\":false,\"qp_status\":null,\"qp_"
              "minor_iterations\":null,\"qp_factorizations\":null,\"tr_binding\":false,\"elastic\":"
              "false,\"elastic_clamped\":false,\"restoration_seed_used\":false,\"ipqp_least_"
              "infeasible_primal\":null,\"ipqp_farkas_corroborated\":null,\"active_set_size\":1,"
              "\"active_set_delta\":0,\"weak_active_rows\":0,\"near_active_rows\":0,\"active_"
              "rows\":1,\"active_lower_sides\":0,\"active_upper_sides\":0,\"switching_ratio\":null,"
              "\"ladder\":null,"
              "\"cap_binding\":false,\"pred_df\":null,\"f_trial\":null,\"h_trial\":null,\"soc_f\":"
              "null,\"soc_h\":null,\"h_lin\":null,\"qp_minor_iterations_total\":0,\"qp_"
              "factorizations_total\":0,\"factorizations_delta\":0,\"t_s\":0}}\n");
    const std::string line = iter_line(r);
    for (const char *k : {"step", "accepted", "verdict", "regularization_dual", "qp_status",
                          "qp_minor_iterations", "qp_factorizations"}) {
        EXPECT_EQ(raw_field(line, k), "null") << k;
    }
    EXPECT_EQ(raw_field(line, "qp_solved"), "false");
}

TEST(JsonLinesTraceSink, GoldenLineIterSqpElasticRow) {
    IterationRecord r = default_sqp_record();
    r.iteration = 3;
    r.tier = IterationTier::kElastic;
    r.f = 19.0;
    r.stationarity = 0.4;
    r.tested_stationarity = 0.4;
    r.feasibility = 0.9;
    r.tested_feasibility = 0.9;
    r.complementarity = 0.02;
    r.tested_complementarity = 0.02;
    r.step_norm = 0.5;
    r.accepted = true;
    r.factorizations = 11;
    r.sqp->verdict = StepVerdict::kAcceptFeasibilityStep;
    r.sqp->trust_radius = 0.5;
    r.sqp->violation_l1 = 1.4;
    r.sqp->regularization_dual = 1e-9;
    r.sqp->qp_solved = true;
    r.sqp->qp_status = QpStatus::kOptimal;
    r.sqp->qp_minor_iterations = 11;
    r.sqp->qp_factorizations = 3;
    r.sqp->elastic = true;
    r.sqp->elastic_clamped = true;
    EXPECT_EQ(iter_line(r),
              "{\"v\":1,\"ev\":\"iter\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\",\"iter\":3,"
              "\"recovering\":false,\"tier\":\"elastic\",\"f\":19,\"f_recovery\":null,"
              "\"stationarity\":0.40000000000000002,\"feasibility\":0.90000000000000002,"
              "\"complementarity\":0.02,\"step\":0.5,\"accepted\":true,\"soc\":false,\"watchdog\":"
              "false,\"rolled_back\":false,\"factorizations\":11,\"stationarity_caller\":0."
              "40000000000000002,\"feasibility_caller\":0.90000000000000002,\"complementarity_"
              "caller\":0.02,\"sqp\":{\"depth\":0,\"mode\":"
              "\"walk\",\"verdict\":\"accept_h\",\"trust_radius\":0.5,\"violation_l1\":1."
              "3999999999999999,\"regularization_dual\":1.0000000000000001e-09,\"qp_solved\":true,"
              "\"qp_status\":\"optimal\",\"qp_minor_iterations\":11,\"qp_factorizations\":3,\"tr_"
              "binding\":false,\"elastic\":true,\"elastic_clamped\":true,\"restoration_seed_used\":"
              "false,\"ipqp_least_infeasible_primal\":null,\"ipqp_farkas_corroborated\":null,"
              "\"active_set_size\":0,\"active_set_delta\":0,\"weak_active_rows\":0,\"near_active_"
              "rows\":0,\"active_rows\":0,\"active_lower_sides\":0,\"active_upper_sides\":0,"
              "\"switching_ratio\":null,\"ladder\":null,"
              "\"cap_binding\":false,\"pred_df\":null,\"f_trial\":null,\"h_trial\":null,\"soc_f\":"
              "null,\"soc_h\":null,\"h_lin\":null,\"qp_minor_iterations_total\":0,\"qp_"
              "factorizations_total\":0,\"factorizations_delta\":0,\"t_s\":0}}\n");
}

TEST(JsonLinesTraceSink, GoldenLineIterSqpDepthOneRestorationRow) {
    // A restoration sub-solve's row: recovering, tier `rest`, the objective in
    // `f_recovery` and `f` null, depth 1 in the envelope (the sink-owned
    // counter) and in the block.
    IterationRecord r = default_sqp_record();
    r.iteration = 1;
    r.recovering = true;
    r.tier = IterationTier::kRestoration;
    r.f_recovery = 0.02;
    r.stationarity = 0.01;
    r.tested_stationarity = 0.01;
    r.feasibility = 0.015;
    r.tested_feasibility = 0.015;
    r.complementarity = 0.0;
    r.tested_complementarity = 0.0;
    r.step_norm = 0.04;
    r.accepted = true;
    r.factorizations = 2;
    r.sqp->depth = 1;
    r.sqp->verdict = StepVerdict::kAcceptObjectiveStep;
    r.sqp->trust_radius = 1.0;
    r.sqp->violation_l1 = 0.02;
    r.sqp->regularization_dual = 0.0;
    r.sqp->qp_solved = true;
    r.sqp->qp_status = QpStatus::kOptimal;
    r.sqp->qp_minor_iterations = 3;
    r.sqp->qp_factorizations = 1;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_solve_begin(default_sqp_begin());
    sink.on_solve_begin(default_sqp_begin());
    sink.on_iter(IterTraceEvent{r});
    const std::vector<std::string> lines = split_lines(os.str());
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(
        lines[2],
        "{\"v\":1,\"ev\":\"iter\",\"seq\":3,\"sid\":1,\"depth\":1,\"eng\":\"sqp\",\"iter\":1,"
        "\"recovering\":true,\"tier\":\"rest\",\"f\":null,\"f_recovery\":0.02,\"stationarity\":0."
        "01,\"feasibility\":0.014999999999999999,\"complementarity\":0,\"step\":0."
        "040000000000000001,\"accepted\":true,\"soc\":false,\"watchdog\":false,\"rolled_back\":"
        "false,\"factorizations\":2,\"stationarity_caller\":0.01,\"feasibility_caller\":0."
        "014999999999999999,\"complementarity_caller\":0,\"sqp\":{\"depth\":1,\"mode\":\"walk\","
        "\"verdict\":\"accept_"
        "f\",\"trust_radius\":1,\"violation_l1\":0.02,\"regularization_dual\":0,\"qp_solved\":true,"
        "\"qp_status\":\"optimal\",\"qp_minor_iterations\":3,\"qp_factorizations\":1,\"tr_"
        "binding\":false,\"elastic\":false,\"elastic_clamped\":false,\"restoration_seed_used\":"
        "false,\"ipqp_least_infeasible_primal\":null,\"ipqp_farkas_corroborated\":null,\"active_"
        "set_size\":0,\"active_set_delta\":0,\"weak_active_rows\":0,\"near_active_rows\":0,"
        "\"active_rows\":0,\"active_lower_sides\":0,\"active_upper_sides\":0,\"switching_ratio\":"
        "null,\"ladder\":null,"
        "\"cap_binding\":false,\"pred_df\":null,\"f_trial\":null,\"h_trial\":null,\"soc_f\":null,"
        "\"soc_h\":null,\"h_lin\":null,\"qp_minor_iterations_total\":0,\"qp_factorizations_total\":"
        "0,\"factorizations_delta\":0,\"t_s\":0}}");
    EXPECT_EQ(raw_field(lines[2], "depth"), "1");
    EXPECT_EQ(raw_field(lines[2], "f"), "null");
}

// ===========================================================================
// Trace v1 `iter`, the SQP block, on whole solves: the stream IS `history`
// ===========================================================================

/// @brief Every field of one SQP `iter` line against the row it claims to be.
///
/// Hand-listed, and the `static_assert` beside it is what keeps the list
/// complete: a field added to `SqpIterationRow` moves the arity and fails HERE as
/// well as at the writer, so the pin cannot silently stop covering the row.
void expect_iter_line_is_row(const std::string &line, const IterationRecord &r, Index major,
                             QpTraceMode mode, Index depth) {
    static_assert(
        ::hven::detail::kAggregateArity<SqpIterationRow> == 36,
        "SqpIterationRow gained a field: compare it below, and in the writer's own key list.");
    const bool stepped = r.sqp->qp_solved &&
                         r.sqp->qp_status.value_or(hven::QpStatus::kOptimal) == QpStatus::kOptimal;
    EXPECT_EQ(raw_field(line, "iter"), std::to_string(major)) << line;
    EXPECT_EQ(raw_field(line, "recovering"), depth >= 1 ? "true" : "false");
    if (depth >= 1) {
        EXPECT_EQ(raw_field(line, "tier"), "\"rest\"");
        EXPECT_EQ(raw_field(line, "f"), "null");
        expect_double_field(line, "f_recovery", r.f_recovery.value());
    } else {
        EXPECT_EQ(raw_field(line, "f_recovery"), "null");
        expect_double_field(line, "f", r.f.value());
        // The tier of a dispatch arm spells the same word as `mode`.
        EXPECT_EQ(raw_field(line, "tier"),
                  std::string(r.sqp->elastic ? "\"elastic\"" : spec_spelling(mode)))
            << line;
    }
    // TR-S1: the core keys are the tested (scaled) measures; the `_caller` keys are the
    // caller-unit ones.
    expect_double_field(line, "stationarity", r.tested_stationarity);
    expect_double_field(line, "feasibility", r.tested_feasibility);
    expect_double_field(line, "complementarity", r.tested_complementarity);
    expect_double_field(line, "stationarity_caller", r.stationarity);
    expect_double_field(line, "feasibility_caller", r.feasibility);
    expect_double_field(line, "complementarity_caller", r.complementarity);
    if (stepped) {
        expect_double_field(line, "step", r.step_norm.value_or(0.0));
        const bool accepted = r.sqp->verdict.value_or(hven::StepVerdict::kReject) ==
                                  StepVerdict::kAcceptObjectiveStep ||
                              r.sqp->verdict.value_or(hven::StepVerdict::kReject) ==
                                  StepVerdict::kAcceptFeasibilityStep;
        EXPECT_EQ(raw_field(line, "accepted"), accepted ? "true" : "false");
    } else {
        EXPECT_EQ(raw_field(line, "step"), "null") << line;
        EXPECT_EQ(raw_field(line, "accepted"), "null") << line;
    }
    EXPECT_EQ(raw_field(line, "soc"), r.soc ? "true" : "false");
    EXPECT_EQ(raw_field(line, "watchdog"), r.watchdog ? "true" : "false");
    EXPECT_EQ(raw_field(line, "rolled_back"), r.rolled_back ? "true" : "false");

    EXPECT_EQ(raw_field(line, "depth"), std::to_string(depth));
    EXPECT_EQ(raw_field(line, "mode"), spec_spelling(mode));
    expect_double_field(line, "trust_radius", r.sqp->trust_radius);
    expect_double_field(line, "violation_l1", r.sqp->violation_l1);
    EXPECT_EQ(raw_field(line, "qp_solved"), r.sqp->qp_solved ? "true" : "false");
    if (r.sqp->qp_solved) {
        EXPECT_EQ(raw_field(line, "verdict"),
                  spec_spelling(r.sqp->verdict.value_or(hven::StepVerdict::kReject)));
        expect_double_field(line, "regularization_dual", r.sqp->regularization_dual.value_or(0.0));
        EXPECT_EQ(raw_field(line, "qp_status"),
                  spec_spelling(r.sqp->qp_status.value_or(hven::QpStatus::kOptimal)));
        EXPECT_EQ(raw_field(line, "qp_minor_iterations"),
                  std::to_string(r.sqp->qp_minor_iterations.value_or(0)));
        EXPECT_EQ(raw_field(line, "qp_factorizations"),
                  std::to_string(r.sqp->qp_factorizations.value_or(0)));
    } else {
        for (const char *k : {"verdict", "regularization_dual", "qp_status", "qp_minor_iterations",
                              "qp_factorizations"}) {
            EXPECT_EQ(raw_field(line, k), "null") << k << " in " << line;
        }
    }
    EXPECT_EQ(raw_field(line, "tr_binding"), r.sqp->tr_binding ? "true" : "false");
    EXPECT_EQ(raw_field(line, "elastic"), r.sqp->elastic ? "true" : "false");
    EXPECT_EQ(raw_field(line, "elastic_clamped"), r.sqp->elastic_clamped ? "true" : "false");
    EXPECT_EQ(raw_field(line, "restoration_seed_used"),
              r.sqp->restoration_seed_used ? "true" : "false");
    if (r.sqp->ipqp_least_infeasible_primal.has_value()) {
        expect_double_field(line, "ipqp_least_infeasible_primal",
                            *r.sqp->ipqp_least_infeasible_primal);
        EXPECT_EQ(raw_field(line, "ipqp_farkas_corroborated"),
                  r.sqp->ipqp_farkas_corroborated.value_or(false) ? "true" : "false");
    } else {
        EXPECT_EQ(raw_field(line, "ipqp_least_infeasible_primal"), "null") << line;
        EXPECT_EQ(raw_field(line, "ipqp_farkas_corroborated"), "null") << line;
    }
    EXPECT_EQ(
        raw_field(line, "active_set_size"),
        std::to_string(r.sqp->active_rows + r.sqp->active_lower_sides + r.sqp->active_upper_sides));
    EXPECT_EQ(raw_field(line, "active_set_delta"), std::to_string(r.sqp->active_set_delta));
    EXPECT_EQ(raw_field(line, "weak_active_rows"), std::to_string(r.sqp->weak_active_rows));
    EXPECT_EQ(raw_field(line, "near_active_rows"), std::to_string(r.sqp->near_active_rows));
    EXPECT_EQ(raw_field(line, "active_rows"), std::to_string(r.sqp->active_rows));
    EXPECT_EQ(raw_field(line, "active_lower_sides"), std::to_string(r.sqp->active_lower_sides));
    EXPECT_EQ(raw_field(line, "active_upper_sides"), std::to_string(r.sqp->active_upper_sides));
    if (r.sqp->switching_ratio.has_value()) {
        expect_double_field(line, "switching_ratio", *r.sqp->switching_ratio);
    } else {
        EXPECT_EQ(raw_field(line, "switching_ratio"), "null") << line;
    }
    if (!r.sqp->ladder.has_value()) {
        EXPECT_EQ(raw_field(line, "ladder"), "null") << line;
    }
    // I19-1: the report-only telemetry.
    EXPECT_EQ(raw_field(line, "cap_binding"), r.sqp->cap_binding ? "true" : "false");
    const auto opt_field = [&](const char *key, const std::optional<double> &v) {
        if (v.has_value()) {
            expect_double_field(line, key, *v);
        } else {
            EXPECT_EQ(raw_field(line, key), "null") << key << " in " << line;
        }
    };
    opt_field("pred_df", r.sqp->pred_df);
    opt_field("f_trial", r.sqp->f_trial);
    opt_field("h_trial", r.sqp->h_trial);
    opt_field("soc_f", r.sqp->soc_f);
    opt_field("soc_h", r.sqp->soc_h);
    opt_field("h_lin", r.sqp->h_lin);
    EXPECT_EQ(raw_field(line, "qp_minor_iterations_total"),
              std::to_string(r.sqp->qp_minor_iterations_total));
    EXPECT_EQ(raw_field(line, "qp_factorizations_total"),
              std::to_string(r.sqp->qp_factorizations_total));
    EXPECT_EQ(raw_field(line, "factorizations_delta"), std::to_string(r.sqp->factorizations_delta));
    expect_double_field(line, "t_s", r.sqp->t_s);
}

/// The depth-0 `iter` lines of a stream, in order.
std::vector<std::string> major_lines(const std::string &stream) {
    std::vector<std::string> out;
    for (const std::string &l : split_lines(stream)) {
        if (event_name(l) == "iter" && raw_field(l, "depth") == "0") {
            out.push_back(l);
        }
    }
    return out;
}

TEST(JsonLinesTraceSink, SqpIterReproducesTheHistoryRowByRowOnAWalkCellAndAKIpmCell) {
    // TWO ARMS, because `mode` is the field that differs: HS24 at kWalk reports
    // "walk" on every row, HS38 at kIpqp reports whichever arm owned the QP.
    struct Leg {
        int hs;
        QpMode mode;
    };
    for (const Leg leg : {Leg{24, QpMode::kWalk}, Leg{38, QpMode::kIpqp}}) {
        SqpOptions opts;
        opts.qp_mode = leg.mode;
        opts.common.max_iterations = 60;
        const HsProblem p = make_hs(leg.hs);
        SqpSolver driver(opts);
        std::ostringstream os;
        JsonLinesTraceSink json(os);
        driver.attach_trace(&json);
        const SqpResult sol =
            driver.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());

        SCOPED_TRACE("HS" + std::to_string(leg.hs));
        const std::vector<std::string> rows = major_lines(os.str());
        ASSERT_GT(sol.history.size(), 1u) << "non-vacuous: the cell really iterates";
        ASSERT_EQ(rows.size(), sol.history.size());
        // Row 0 is the start point, so a plain exit holds `iterations + 1` depth-0
        // rows (the callback's documented event count). Trace spec 2.8's identity
        // 1 states `iterations`; the documented SqpResult semantics win here.
        EXPECT_EQ(static_cast<Index>(rows.size()), sol.iterations + 1);
        for (std::size_t i = 0; i < rows.size(); ++i) {
            SCOPED_TRACE("row " + std::to_string(i));
            const std::string got = raw_field(rows[i], "mode");
            EXPECT_TRUE(got == "\"walk\"" || got == "\"ssn\"" || got == "\"ipqp\"") << got;
            expect_iter_line_is_row(rows[i], sol.history[i], static_cast<Index>(i),
                                    mode_of_token(got), 0);
        }
    }
}

TEST(JsonLinesTraceSink, SqpIterCarriesTestedAndCallerUnitsOnAScaledSolve) {
    // THE UNITS CLAIM, and the only cell that can make it: the emit sits INSIDE
    // `push_history`, after its scaling map, so the stream must equal `history` field for
    // field even where the two spaces differ: `stationarity`/`feasibility`/`complementarity`
    // are the scaled problem's (the tested) values, `*_caller` the mapped ones (TR-S1).
    SqpOptions opts;
    opts.row_scaling = true;
    opts.common.max_iterations = 60;
    const HsProblem p = make_hs(25);
    SqpSolver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpResult sol =
        driver.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());

    ASSERT_TRUE(sol.scaling.active) << "non-vacuous: the solve really scaled";
    bool any_differs = false;
    for (const IterationRecord &r : sol.history) {
        any_differs = any_differs || r.tested_stationarity != r.stationarity ||
                      r.tested_feasibility != r.feasibility;
    }
    EXPECT_TRUE(any_differs) << "non-vacuous: tested and caller values really differ";
    const std::vector<std::string> rows = major_lines(os.str());
    ASSERT_GT(sol.history.size(), 1u);
    ASSERT_EQ(rows.size(), sol.history.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        SCOPED_TRACE("row " + std::to_string(i));
        expect_iter_line_is_row(rows[i], sol.history[i], static_cast<Index>(i), QpTraceMode::kWalk,
                                0);
    }
}

// ===========================================================================
// W4 T2 (b) -- `qp.mode` IN ALL THREE ARMS
// ===========================================================================
// ONE LINE PER KERNEL INVOCATION (the rule stated at `QpModeTraceEvent`), so a
// hand-off writes two: the handing kernel's `routed` line and its successor's.
//
// The counts below are therefore INVOCATION counts, each asserted against a
// counter rather than against the stream.

/// The `mode` tokens of a stream's `qp.mode` lines, counted.
std::map<std::string, Index> qp_mode_census(const std::string &stream) {
    std::map<std::string, Index> by_mode;
    for (const std::string &l : split_lines(stream)) {
        if (event_name(l) == "qp.mode") {
            ++by_mode[raw_field(l, "mode")];
        }
    }
    return by_mode;
}

Index rows_with_a_qp(const SqpResult &sol) {
    Index rows = 0;
    for (const IterationRecord &r : sol.history) {
        rows += r.sqp->qp_solved ? 1 : 0;
    }
    return rows;
}

TEST(JsonLinesTraceSink, QpModeOnAWalkCellIsOneLinePerMajorAndNamesTheWalk) {
    SqpOptions opts;
    opts.qp_mode = QpMode::kWalk;
    opts.common.max_iterations = 60;
    const HsProblem p = make_hs(24);
    SqpSolver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpResult sol =
        driver.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());

    const std::map<std::string, Index> by_mode = qp_mode_census(os.str());
    ASSERT_GT(sol.counters.major_iters, 1);
    EXPECT_EQ(by_mode.size(), 1u) << "one arm, one mode string";
    // THE WALK IS INVOKED ONCE PER MAJOR, so its line count is `major_iters`
    // (the counter is `iter + 1` at the dispatch, i.e. the dispatch count).
    EXPECT_EQ(by_mode.at("\"walk\""), sol.counters.major_iters);
    // AND ON THIS CELL that is also the row count, because no subproblem
    // FAILED: a routed QP failure re-dispatches at a shrunken radius without
    // pushing a row, which is the one way the two counts can differ.
    EXPECT_EQ(by_mode.at("\"walk\""), rows_with_a_qp(sol));
}

TEST(JsonLinesTraceSink, QpModeOnAnSsnCellCountsTheArmAndItsHandOffs) {
    SqpOptions opts;
    opts.qp_mode = QpMode::kSsn;
    opts.common.max_iterations = 60;
    const HsProblem p = make_hs(33);
    SqpSolver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpResult sol =
        driver.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());

    const std::map<std::string, Index> by_mode = qp_mode_census(os.str());
    const auto at = [&](const char *k) {
        const auto it = by_mode.find(k);
        return it == by_mode.end() ? Index{0} : it->second;
    };
    ASSERT_GT(sol.counters.major_iters, 1);
    // THE ARM RUNS ON EVERY SUBPROBLEM, so its own count is the dispatch count.
    EXPECT_EQ(at("\"ssn\""), sol.counters.major_iters);
    // AND EVERY HAND-OFF IS A WALK INVOCATION. `ssn_escapes` at driver scale is
    // exactly "subproblems handed off" -- engine escapes plus the trust-region
    // gate's refusals (solver_counters.h's own note).
    EXPECT_EQ(at("\"walk\""), sol.counters.ssn.ssn_escapes);

    Index routed = 0;
    for (const std::string &l : split_lines(os.str())) {
        if (event_name(l) == "qp.mode" && raw_field(l, "mode") == "\"ssn\"") {
            routed += (raw_field(l, "outcome") == "\"routed\"") ? 1 : 0;
        }
    }
    EXPECT_EQ(routed, sol.counters.ssn.ssn_escapes) << "the routed lines ARE the hand-offs";
}

TEST(JsonLinesTraceSink, QpModeOnAKIpmCellNamesTheTierAndTheDeclineRoutesToTheWalk) {
    // TWO LEGS, because the kIpqp arm reaches the walk two different ways and
    // only ONE of them is a dispatch-level invocation.
    //
    // HS38 ESCAPES, and an escape is serviced by `certified_feasibility_fallback`,
    // which runs its own walk INSIDE itself -- so `ipqp_to_walk` moves while the
    // dispatch's walk site never runs.
    //
    // RE-DERIVED AT M6 W4 T5 (declared, additive): that walk is no longer
    // MISSING from the stream, it is TAGGED. The registered gap is closed.
    //
    // The line exists at `site` `fallback_rung_b` while the dispatch site still
    // reads 0 -- the distinction the `site` key was added to carry.
    //
    // THE PINNED-VARIABLE MODEL DECLINES instead: the domain gate refuses the
    // subproblem before the engine is entered, `walk_owns_this_qp` becomes true,
    // and the dispatch's own walk runs and writes its line.
    {
        SqpOptions opts;
        opts.qp_mode = QpMode::kIpqp;
        opts.common.max_iterations = 60;
        const HsProblem p = make_hs(38);
        SqpSolver driver(opts);
        std::ostringstream os;
        JsonLinesTraceSink json(os);
        TeeSink tee(json);
        driver.attach_trace(&tee);
        const SqpResult sol =
            driver.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());

        const std::map<std::string, Index> by_mode = qp_mode_census(os.str());
        const auto at = [&](const char *k) {
            const auto it = by_mode.find(k);
            return it == by_mode.end() ? Index{0} : it->second;
        };
        EXPECT_EQ(at("\"ipqp\"") + at("\"walk\"") + at("\"ssn\""), tee.modes);
        ASSERT_GT(at("\"ipqp\""), 0);
        ASSERT_GT(sol.counters.ipqp.ipqp_to_walk, 0) << "non-vacuous: the cell really escapes";
        EXPECT_EQ(at("\"ssn\""), 0) << "the kSsn ARM did not run, and neither did the warm grade";
        // THE WALK LINES ARE THE FALLBACK'S, NOT THE DISPATCH'S -- split by site.
        Index walk_dispatch = 0, walk_rung_b = 0;
        for (const std::string &l : split_lines(os.str())) {
            if (event_name(l) != "qp.mode" || raw_field(l, "mode") != "\"walk\"") {
                continue;
            }
            walk_dispatch += (raw_field(l, "site") == "\"dispatch\"") ? 1 : 0;
            walk_rung_b += (raw_field(l, "site") == "\"fallback_rung_b\"") ? 1 : 0;
        }
        EXPECT_EQ(walk_dispatch, 0) << "the fallback's own walk is not a dispatch invocation";
        EXPECT_EQ(walk_rung_b, at("\"walk\"")) << "and every walk line here is the fallback's";
        EXPECT_GT(walk_rung_b, 0) << "non-vacuous: T5 made that walk visible";
    }
    {
        PinnedVariableModel model(true);
        SqpOptions opts;
        opts.qp_mode = QpMode::kIpqp;
        opts.common.max_iterations = 60;
        SqpSolver driver(opts);
        std::ostringstream os;
        JsonLinesTraceSink json(os);
        driver.attach_trace(&json);
        const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());

        const std::map<std::string, Index> by_mode = qp_mode_census(os.str());
        ASSERT_GT(sol.counters.ipqp.ipqp_declined_pinned, 0) << "non-vacuous: it really declines";
        EXPECT_EQ(by_mode.size(), 1u) << "a declined subproblem never enters the tier";
        EXPECT_EQ(by_mode.at("\"walk\""), sol.counters.major_iters);
        EXPECT_EQ(sol.counters.ipqp.ipqp_declined_pinned, sol.counters.major_iters);
    }
}

// ===========================================================================
// W4 T2 (d) -- THE COUNTERS FIELD TABLES
// ===========================================================================

TEST(SqpCountersFieldTables, EachTableEnumeratesItsWholeStruct) {
    // THE PIN IS THE `static_assert`S IN solver_counters.h, which fire at
    // COMPILE time in every TU that includes the header.
    //
    // These read the same numbers back at run time, so a reader sees what they
    // are and a table edited to a different length is named here too.
    EXPECT_EQ(kSsnCountersFieldCount, 18u);
    // 39 -> 42 at M7 W2 A2, which appended the three `ipqp_mu*` fire counters LAST in
    // IpqpCounters: both golden lines below gain those three keys at the end of the `ipqp`
    // object, re-derived ONCE (docs/trace-schema-v0.md section 8: declared additive trailing
    // keys). Nothing else moved. 42 -> 43 at M7 W2 A3 (`ipqp_ladder_zero_trial_skips`, the
    // same route, one key).
    EXPECT_EQ(kIpqpCountersFieldCount, 43u);
    // 37 -> 38 at M6 W5 T8.5, which appended `polish_ignored` LAST -- after
    // both nested aggregates -- so every pre-existing field offset is unmoved
    // and the two golden lines below gain exactly one key, at the end of the
    // SqpCounters object. DECLARED: the counters JSON is a trace surface.
    //
    // 38 -> 39 at M7 W2 M4, which appended `start_point_clamps` the same way
    // and for the same reason, and re-derived the same two golden lines by the
    // same one key. Equally DECLARED (docs/trace-schema-v0.md section 8: a
    // frozen event's golden line moves only by a declared additive TRAILING
    // key, which is what this is).
    //
    // 39 -> 44 at M7 W2 M1, which appended the FIVE-field subproblem-exit group
    // (`qp_maxiter_exits`, `qp_numerical_exits`, `qp_failures_routed`,
    // `qp_failures_in_a_row_peak`, `qp_cap_retries`) the same way and for the
    // same reason, and re-derived the same two golden lines ONCE, by five keys
    // -- one re-derivation for the task, not one per field.
    //
    // 44 -> 49 at M7 W2 M2, which appended the FIVE-field restoration excursion
    // record (`restoration_entries` and the four absolutes
    // `restoration_h_in`/`_h_out`/`_f_in`/`_f_out`) the same way and for the
    // same reason, and re-derived the same two golden lines ONCE, by five keys.
    // It is the first table entry group whose members are `double`s rather than
    // `Index`es; the arity and offset asserts are indifferent to that, and the
    // count below is the same kind of statement it always was.
    //
    // 49 -> 50 at M7 W2 M3a, which appended ONE field to that same restoration
    // record -- `restoration_sub_status`, the feasibility sub-solve's own
    // terminal status -- the same way and for the same reason, and re-derived
    // the same two golden lines ONCE, by one key. It is the table's SECOND
    // enum (after `start_level_used`) and its first sub-8-byte field; the
    // arity and offset asserts are indifferent to both, and the count below is
    // the same kind of statement it always was.
    //
    // 50 -> 56 at M7 W2 E4 fix round 1, which appended the QP engine's six-field
    // E4 GROUP -- `eqp_refine_iters`, `eqp_residual_refusals`, `eqp_polish_steps`,
    // `terminal_polish_solves`, `terminal_polish_rejected`,
    // `terminal_polish_adopted` -- the same way and for the same reason, and
    // re-derived the same two golden lines ONCE, by six keys. They are `Index`
    // again, appended after the table's only sub-8-byte field, so the arity and
    // offset asserts are indifferent and the count below is the same kind of
    // statement it always was.
    //
    // 56 -> 57 at M7 W2 E4 fix round 2, which made that group SEVEN by appending
    // `eqp_row_drift_refusals` -- the same refusal in the STEP's currency -- on
    // the same DECLARED ADDITIVE TRAILING KEY route: it is last in the table and
    // last in the struct, so no existing key's value or bytes move and the two
    // golden lines are re-derived ONCE, by one key, at the end of the direct
    // block. Its place in `QpCounters` is beside its residual twin, where the
    // meaning reads; the TABLE's order is what this count is about.
    //
    // 57 -> 60 at M7 W2 R2, which appended `qp_failure_steps_judged`,
    // `qp_failure_steps_accepted` and `nonfinite_subproblems` on the same DECLARED
    // ADDITIVE TRAILING KEY route, re-deriving the same two golden lines ONCE, by
    // three keys.
    //
    // 60 -> 61 at M7 W2 W1, `nonfinite_box_projections`, the same route, by one key.
    //
    // 61 -> 63 at M7 W2 P1E, `elastic_last_rung_fallbacks` and
    // `restoration_reduction_resumes`, the same route, by two keys.
    //
    // 63 -> 77 at M7 QN stage 3, `eval_hess_calls` and the thirteen-field quasi-Newton
    // group, the same route, by fourteen keys.
    //
    // 77 -> 78 at the QN stages 1-3 review, `qn_nonpd_seen`, the same route, by one key.
    //
    // 78 -> 79 at the QN-quality riders, `qn_dimension`, the same route, by one key.
    //
    // 79 -> 80 at M7 W2 R2-1, `nonfinite_step_rollbacks`, the same route, by one key.
    //
    // 80 -> 81 at M7 W2 SNR, `restoration_feas_out`, the same route, by one key.
    //
    // 81 -> 82 at M7 W2 SCL, `scaled_convergence_refusals`, the same route, by one key.
    //
    // 82 -> 83 at M7 W2 RST, `restoration_curvature_switches`, the same route, by one key.
    //
    // 83 -> 85 at M8 DN-1, `inertia_suspect_verdicts` and `inertia_wrong_verdicts`, the same
    // route, by two keys.
    //
    // 85 -> 86 at M8 DN-1c, `dense_platform_retakes`, the same route, by one key.
    //
    // 86 -> 88 at M8 DN-1d, `dense_flagged_factorizations` and `dense_singular_refactors`, the
    // same route, by two keys; 88 -> 89, `dense_shifted_solve_refusals`, by one key; 89 -> 90,
    // `dense_shifted_k0_fallbacks`, by one key; 90 -> 91, `dense_shifted_terminations`, by one key.
    //
    // 91 -> 92 at M8 DN-3a, `dense_solve_residual_refusals`, the same route, by one key.
    //
    // 92 -> 93 at M8 DN-3, `dense_factorizations`, the same route, by one key.
    //
    // 93 -> 94 at M8 kopt-1x, `optimal_row_gray_exits`, the same route, by one key.
    //
    // 94 -> 95 at M8 AP-5 G7, `payload_ignored`, the same route, by one key.
    //
    // 95 -> 103 at M8 GL-0, the seven reject/accept-type counters and `rejection_run_histogram`.
    //
    // 103 -> 106 at M8 sparse-equil-v4, `sparse_scaled_retakes`, `sparse_matched_retakes` and
    // `sparse_retake_refusals`, the same route, by three keys.
    //
    // 106 -> 108 at M8 PF-4, `backend_symbolic_analyses` and `backend_symbolic_repeats`, the same
    // route, by two keys.
    //
    // 108 -> 120 at M8 PF-2b, the twelve `backend_solve*` tally counters, the same route.
    //
    // 120 -> 121 at M8 PF-5, `symbolic_analysis_reuses`, appended after PF-2b's twelve, the
    // same route, by one key.
    //
    // 121 -> 129 at M8 RF-1 (A + B2), the four `qp_retry_rung_*` counters, `qp_retry_replays`,
    // `qp_failure_box_projections`, `trust_region_floor_exits` and `soc_qp_iteration_limits`,
    // appended after PF-5's one, the same route, by eight keys.
    //
    // 129 -> 132 at M8 RST-B, `restoration_factorizations`, `restoration_unproductive_entries` and
    // `restoration_no_progress_requests`, appended after RF-1's eight, the same route, by three
    // keys.
    //
    // 132 -> 135 at M8 RST-B (C562), `restoration_budget_cuts`, `restoration_no_accept_refusals`
    // and `restoration_refusal_feasibility` (absent at +infinity), appended after those three.
    EXPECT_EQ(kSqpCountersFieldCount, 135u);
    EXPECT_EQ(::hven::detail::kAggregateArity<SsnCounters>, kSsnCountersFieldCount);
    EXPECT_EQ(::hven::detail::kAggregateArity<IpqpCounters>, kIpqpCountersFieldCount);
    // PLUS TWO: `ssn` and `ipqp` are nested aggregates, one initializer each.
    EXPECT_EQ(::hven::detail::kAggregateArity<SqpCounters>, kSqpCountersFieldCount + 2);
}

// ===========================================================================
// W4 T2 (c) -- `sqp.solve` begin/end
// ===========================================================================

void expect_counter_field(const std::string &line, const char *k, Index v, bool absent) {
    if (absent) {
        EXPECT_EQ(raw_field(line, k), "null") << k;
        return;
    }
    EXPECT_EQ(raw_field(line, k), std::to_string(v)) << k;
}

void expect_counter_field(const std::string &line, const char *k, double v, bool absent) {
    if (absent) {
        EXPECT_EQ(raw_field(line, k), "null") << k;
        return;
    }
    expect_double_field(line, k, v);
}

void expect_counter_field(const std::string &line, const char *k, const std::array<Index, 5> &v,
                          bool) {
    std::string want = "[";
    for (std::size_t i = 0; i < v.size(); ++i) {
        want += (i == 0 ? "" : ",") + std::to_string(v[i]);
    }
    EXPECT_EQ(raw_field(line, k), want + "]") << k;
}

void expect_counter_field(const std::string &line, const char *k, StartLevel v, bool) {
    EXPECT_EQ(raw_field(line, k), spec_spelling(v)) << k;
}

void expect_counter_field(const std::string &line, const char *k, hven::RestorationSubStatus v,
                          bool) {
    EXPECT_EQ(raw_field(line, k), spec_spelling(v)) << k;
}

/// @brief The whole counters object against the solution's own counters.
///
/// GENERATED FROM THE SAME TABLES the writer uses, so the pin cannot fall
/// behind the struct: a field added without a table entry fails the header's
/// `static_assert`, and one added WITH an entry is compared here automatically.
void expect_end_line_counters(const std::string &line, const SqpCounters &c) {
#define HVEN_TEST_CHECK_FIELD(f, absent) expect_counter_field(line, #f, c.f, absent(c.f));
    HVEN_SQP_COUNTERS_FIELDS(HVEN_TEST_CHECK_FIELD)
#undef HVEN_TEST_CHECK_FIELD
#define HVEN_TEST_CHECK_FIELD(f, absent) expect_counter_field(line, #f, c.ssn.f, absent(c.ssn.f));
    HVEN_SSN_COUNTERS_FIELDS(HVEN_TEST_CHECK_FIELD)
#undef HVEN_TEST_CHECK_FIELD
#define HVEN_TEST_CHECK_FIELD(f, absent) expect_counter_field(line, #f, c.ipqp.f, absent(c.ipqp.f));
    HVEN_IPQP_COUNTERS_FIELDS(HVEN_TEST_CHECK_FIELD)
#undef HVEN_TEST_CHECK_FIELD
}

/// The one line of a stream carrying `ev`, at `depth`.
std::string only_line(const std::string &stream, const char *ev, const char *depth = "0") {
    std::string found;
    Index hits = 0;
    for (const std::string &l : split_lines(stream)) {
        if (event_name(l) == ev && raw_field(l, "depth") == depth) {
            found = l;
            ++hits;
        }
    }
    if (hits != 1) {
        ADD_FAILURE() << "expected exactly one " << ev << " line at depth " << depth << ", found "
                      << hits;
        return {};
    }
    return found;
}

TEST(JsonLinesTraceSink, SqpSolveEndCarriesTheWholeCountersObject) {
    SqpOptions opts;
    opts.qp_mode = QpMode::kIpqp;
    opts.common.max_iterations = 60;
    const HsProblem p = make_hs(38);
    SqpSolver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpResult sol =
        driver.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());

    const std::string end = only_line(os.str(), "solve.end");
    ASSERT_FALSE(end.empty());
    EXPECT_EQ(raw_field(end, "iterations"), std::to_string(sol.iterations));
    EXPECT_EQ(raw_field(end, "status"), "\"" + std::string(to_string(sol.status)) + "\"");
    EXPECT_EQ(raw_field(end, "stop_reason"), "\"" + std::string(to_string(sol.stop_reason)) + "\"");
    // A kIpqp cell so the two NESTED aggregates are not all-zero: HS38 escapes.
    ASSERT_GT(sol.counters.ipqp.ipqp_iters, 0);
    expect_end_line_counters(end, sol.counters);

    const std::string begin = only_line(os.str(), "solve.begin");
    EXPECT_EQ(raw_field(begin, "variables"), std::to_string(p.model->n()));
    EXPECT_EQ(raw_field(begin, "constraints"), std::to_string(p.model->me() + p.model->mi()));
    EXPECT_EQ(raw_field(begin, "equality_rows"), std::to_string(p.model->me()));
    EXPECT_EQ(raw_field(begin, "optimality_tol").empty(), false);
    EXPECT_EQ(raw_field(begin, "max_iterations"), "60");
    const std::string block = raw_field(begin, "sqp");
    ASSERT_FALSE(block.empty());
    EXPECT_EQ(raw_field(block, "me"), std::to_string(p.model->me()));
    EXPECT_EQ(raw_field(block, "mi"), std::to_string(p.model->mi()));
    EXPECT_EQ(raw_field(block, "qp_mode"), "\"ipqp\"");
    EXPECT_EQ(raw_field(block, "ws_algebra"), "\"schur_border\"");
    EXPECT_EQ(raw_field(begin, "ipm"), "") << "an SQP solve carries no `ipm` block";
    // THE VARIABLE-BOUND CENSUS IS EXHAUSTIVE AND DISJOINT, so its five counts
    // sum to n whatever the model's box looks like.
    Index total = 0;
    for (const char *k : {"free_variables", "lower_bounded_variables", "upper_bounded_variables",
                          "boxed_variables", "fixed_variables"}) {
        total += std::stoll(raw_field(begin, k));
    }
    EXPECT_EQ(total, p.model->n());
    EXPECT_EQ(json.depth(), 0) << "the pair is balanced";
}

TEST(JsonLinesTraceSink, SqpSolvePartitionsTwoSolvesOnOneSinkAndSeqStaysContiguous) {
    // Rider 2: one sink, two solves. `seq` is per-SINK, so it runs
    // 1..N across both, and every line of solve k lies inside solve k's own
    // pair -- which is what makes the pair a partition rather than a marker.
    SqpOptions opts;
    opts.common.max_iterations = 60;
    const HsProblem p24 = make_hs(24);
    const HsProblem p11 = make_hs(11);
    std::ostringstream os;
    JsonLinesTraceSink json(os);

    SqpSolver a(opts);
    a.attach_trace(&json);
    const SqpResult first =
        a.solve(hven::detail::problem_ref(*p24.model), (*p24.model).start_point());
    SqpSolver b(opts);
    b.attach_trace(&json);
    const SqpResult second =
        b.solve(hven::detail::problem_ref(*p11.model), (*p11.model).start_point());

    const std::vector<std::string> lines = split_lines(os.str());
    ASSERT_GT(lines.size(), 4u);
    Index open = 0;
    Index pairs = 0;
    Index lines_inside = 0;
    Index messages = 0;
    Index expected_seq = 0;
    for (const std::string &l : lines) {
        ++expected_seq;
        EXPECT_EQ(raw_field(l, "seq"), std::to_string(expected_seq)) << l;
        EXPECT_EQ(raw_field(l, "depth"), "0");
        const std::string ev = event_name(l);
        if (ev == "solve.begin") {
            EXPECT_EQ(open, 0) << "a second begin before the first end";
            ++open;
            ++pairs;
            // `sid` is the per-sink ordinal of the depth-0 begin: 1, then 2.
            EXPECT_EQ(raw_field(l, "sid"), std::to_string(pairs)) << l;
            continue;
        }
        EXPECT_EQ(raw_field(l, "sid"), std::to_string(pairs)) << "a line carries its solve's sid";
        if (ev == "solve.end") {
            EXPECT_EQ(open, 1) << "an end with no begin";
            --open;
            continue;
        }
        EXPECT_EQ(open, 1) << "a line outside every pair: " << l;
        ++lines_inside;
        messages += (ev == "message") ? 1 : 0;
    }
    EXPECT_EQ(open, 0);
    EXPECT_EQ(pairs, 2);
    // RE-DERIVED AT M6 W4 T5 (declared, additive): HS11 runs the elastic ladder,
    // and since T5 each of its RUNGS writes a `qp.mode` line of its own.
    //
    // The rung term is `elastic_activations + elastic_escalations` -- one walk
    // per ladder, plus one per escalation. HS24 contributes 0 to it.
    const Index rungs = first.counters.elastic_activations + first.counters.elastic_escalations +
                        second.counters.elastic_activations + second.counters.elastic_escalations;
    ASSERT_GT(rungs, 0) << "non-vacuous: HS11 really climbs";
    EXPECT_EQ(lines_inside, static_cast<Index>(first.history.size() + second.history.size()) +
                                first.counters.major_iters + second.counters.major_iters + rungs +
                                messages)
        << "one row and one dispatch qp.mode line per major, plus the ladder's own rungs and "
           "the messages";
    EXPECT_EQ(json.lines_written(), static_cast<Index>(lines.size()));
    EXPECT_EQ(json.depth(), 0);
}

TEST(JsonLinesTraceSink, OrderIsTheJoinKeyOnHS38AtKIpm) {
    // Rider 1. Every event line belongs to the major whose `sqp.major`
    // line comes NEXT: the row is pushed at the end of its own major, so the
    // bracket is (previous row, this row].
    //
    // DEPTH 0 ONLY. A nested restoration sub-solve writes its own lines into
    // the same stream at depth 1, and they belong to its own brackets.
    SqpOptions opts;
    opts.qp_mode = QpMode::kIpqp;
    opts.common.max_iterations = 60;
    const HsProblem p = make_hs(38);
    SqpSolver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpResult sol =
        driver.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());

    Index brackets = 0;
    Index pending = 0;
    Index trailing = 0;
    bool inside = false;
    for (const std::string &l : split_lines(os.str())) {
        if (raw_field(l, "depth") != "0") {
            continue;
        }
        const std::string ev = event_name(l);
        if (ev == "solve.begin") {
            inside = true;
            continue;
        }
        if (ev == "solve.end") {
            inside = false;
            trailing = pending;
            continue;
        }
        EXPECT_TRUE(inside) << "outside the solve's own pair: " << l;
        if (ev == "iter") {
            ++brackets;
            pending = 0;
            continue;
        }
        // EVERY non-row, non-pair event (co-review M-4): the tier's own
        // `ipqp.*` lines belong to a major's bracket too, and counting only
        // the two driver events would let an `ipqp.iter` trail the last row.
        ++pending;
    }
    EXPECT_EQ(brackets, static_cast<Index>(sol.history.size()));
    EXPECT_EQ(trailing, 0) << "every event line has a row after it";
    ASSERT_GT(sol.counters.ipqp.ipqp_escapes, 0) << "non-vacuous: the cell escapes and falls back";
}

/// @brief An NLP with NO feasible point: the unit circle meets the line x0 = 3
/// nowhere.
///
/// WRITTEN HERE, NOT LIFTED. `tests/sqp/test_sqp_restoration.cpp` has a
/// circle/line fixture of its own, and copying it would be a FOURTH replication
/// of a driver fixture (T1's registered item). This one exists for two lines of
/// the stream: the kInfeasible exit, and the nested restoration solve those two
/// pins need.
class CircleAndFarLineModel final : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 2; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        return 0.5 * ((x(0) - 2.0) * (x(0) - 2.0) + x(1) * x(1));
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << x(0) - 2.0, x(1);
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(2);
        c << x(0) * x(0) + x(1) * x(1) - 1.0, x(0) - 3.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &lambda_e,
                      const Vec &) const override {
        const double d = obj_scale + 2.0 * lambda_e(0);
        SpMatRM h(2, 2);
        h.insert(0, 0) = d;
        h.insert(1, 1) = d;
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(2, 2);
        j.insert(0, 0) = 2.0 * x(0);
        j.insert(0, 1) = 2.0 * x(1);
        j.insert(1, 0) = 1.0;
        j.makeCompressed();
        return j;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    const Vec &lower() const override {
        static const Vec v = Vec::Constant(2, -std::numeric_limits<double>::infinity());
        return v;
    }
    const Vec &upper() const override {
        static const Vec v = Vec::Constant(2, std::numeric_limits<double>::infinity());
        return v;
    }
    Vec start_point() const override { return Vec::Constant(2, 0.5); }
};

TEST(JsonLinesTraceSink, SqpSolveEndFiresOnEveryExitPathIncludingTheOnesThatSkipFinish) {
    // THREE DISTINCT EXITS, one per shape the guard has to survive: the ordinary
    // `finish` return, the budget exit, and the restoration-driven kInfeasible
    // exit -- which is also the one that runs a NESTED solve first.
    //
    // The guard is RAII over the whole function, so this is a demonstration
    // rather than an enumeration of the sixteen returns; what it shows is that
    // the three shapes all reach it.
    struct Leg {
        const char *name;
        SolveStatus status;
    };
    {
        SqpOptions opts;
        opts.common.max_iterations = 60;
        const HsProblem p = make_hs(24);
        SqpSolver driver(opts);
        std::ostringstream os;
        JsonLinesTraceSink json(os);
        driver.attach_trace(&json);
        const SqpResult sol =
            driver.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());
        ASSERT_EQ(sol.status, SolveStatus::kOptimal);
        EXPECT_EQ(raw_field(only_line(os.str(), "solve.end"), "status"), "\"optimal\"");
        EXPECT_EQ(json.depth(), 0);
    }
    {
        SqpOptions opts;
        opts.common.max_iterations = 1;
        const HsProblem p = make_hs(38);
        SqpSolver driver(opts);
        std::ostringstream os;
        JsonLinesTraceSink json(os);
        driver.attach_trace(&json);
        const SqpResult sol =
            driver.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());
        ASSERT_EQ(sol.status, SolveStatus::kIterationLimit);
        EXPECT_EQ(raw_field(only_line(os.str(), "solve.end"), "status"), "\"max_iter\"");
        EXPECT_EQ(json.depth(), 0);
    }
    {
        CircleAndFarLineModel model;
        SqpOptions opts;
        opts.common.max_iterations = 200;
        SqpSolver driver(opts);
        std::ostringstream os;
        JsonLinesTraceSink json(os);
        driver.attach_trace(&json);
        const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());
        ASSERT_EQ(sol.status, SolveStatus::kInfeasible);
        EXPECT_EQ(raw_field(only_line(os.str(), "solve.end"), "status"), "\"infeasible\"");
        EXPECT_EQ(json.depth(), 0);
    }
}

TEST(JsonLinesTraceSink, TheNestedSolvesFirstRowCountsAgainstItsOwnEmptySet) {
    // W4 T3, pin 6. The restoration sub-solve is a SEPARATE sequence: its first
    // reporting row's `active_set_delta` is a count against the EMPTY set, not
    // against whatever the outer solve's last major left behind.
    //
    // The empty-set count IS the census on a fixture with no `kFixed` variable
    // (the wrapper's slacks are bounded below only), so the identity below is
    // the claim, read off the sub-solve's own first depth-1 row.
    CircleAndFarLineModel model;
    SqpOptions opts;
    opts.common.max_iterations = 200;
    SqpSolver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());
    ASSERT_EQ(sol.status, SolveStatus::kInfeasible);
    ASSERT_GE(sol.counters.restoration_iters, 1) << "the phase must have RUN";

    bool seen = false;
    for (const std::string &l : split_lines(os.str())) {
        if (event_name(l) != "iter" || raw_field(l, "depth") != "1") {
            continue;
        }
        if (raw_field(l, "qp_solved") != "true") {
            continue; // not a reporting row; it carries zeros by contract
        }
        const Index delta = std::stoll(raw_field(l, "active_set_delta"));
        const Index census = std::stoll(raw_field(l, "active_rows")) +
                             std::stoll(raw_field(l, "active_lower_sides")) +
                             std::stoll(raw_field(l, "active_upper_sides"));
        EXPECT_EQ(delta, census) << "the sub-solve's first row is not an empty-set count: " << l;
        EXPECT_GT(census, 0) << "vacuous: the sub-solve's first QP had an empty active set";
        seen = true;
        break;
    }
    EXPECT_TRUE(seen) << "no depth-1 reporting row -- the pin claimed nothing";
}

TEST(JsonLinesTraceSink, TheNestedSolvesActivityChurnStaysOutOfTheOuterTotals) {
    // W4 T3. `active_set_delta_total` is the OUTER solve's own sum: the
    // sub-solve's QP lives in the feasibility wrapper's variables and rows, so
    // folding its churn in would report a churn over a set that never existed.
    //
    // NON-VACUOUS BY THE SECOND ASSERTION: the sub-solve really did report
    // deltas of its own, and none of them reached the outer counter.
    CircleAndFarLineModel model;
    SqpOptions opts;
    opts.common.max_iterations = 200;
    SqpSolver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());
    ASSERT_EQ(sol.status, SolveStatus::kInfeasible);
    ASSERT_GE(sol.counters.restoration_iters, 1);

    Index outer = 0;
    for (const IterationRecord &r : sol.history) {
        outer += r.sqp->active_set_delta;
    }
    EXPECT_EQ(sol.counters.active_set_delta_total, outer);

    Index nested = 0;
    for (const std::string &l : split_lines(os.str())) {
        if (event_name(l) == "iter" && raw_field(l, "depth") == "1") {
            nested += std::stoll(raw_field(l, "active_set_delta"));
        }
    }
    EXPECT_GT(nested, 0) << "the sub-solve reported no churn -- the pin claimed nothing";
}

TEST(JsonLinesTraceSink, TheNestedRestorationSolveIsBracketedAtDepthOne) {
    // PLAN AMENDMENT A. The restoration sub-driver is attached to the SAME sink,
    // and its lines are told apart by the sink-owned `depth` alone -- no field on
    // any event, no stack in the reader.
    CircleAndFarLineModel model;
    SqpOptions opts;
    opts.common.max_iterations = 200;
    SqpSolver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());

    ASSERT_EQ(sol.status, SolveStatus::kInfeasible);
    ASSERT_GE(sol.counters.restoration_iters, 1) << "the phase must have RUN";

    Index depth1_begins = 0;
    Index depth1_ends = 0;
    Index depth1_rows = 0;
    Index depth1_other = 0;
    bool in_nested = false;
    std::string nested_end;
    for (const std::string &l : split_lines(os.str())) {
        const std::string ev = event_name(l);
        const std::string depth = raw_field(l, "depth");
        if (depth == "0") {
            EXPECT_FALSE(in_nested) << "a depth-0 line inside the nested pair: " << l;
            continue;
        }
        EXPECT_EQ(depth, "1") << "v0 nests exactly one level: " << l;
        if (ev == "solve.begin") {
            ++depth1_begins;
            in_nested = true;
            continue;
        }
        if (ev == "solve.end") {
            ++depth1_ends;
            nested_end = l;
            in_nested = false;
            continue;
        }
        EXPECT_TRUE(in_nested) << "a depth-1 line outside the nested pair: " << l;
        if (ev == "iter") {
            ++depth1_rows;
        } else {
            ++depth1_other;
        }
    }
    EXPECT_EQ(depth1_begins, 1);
    EXPECT_EQ(depth1_ends, 1);
    ASSERT_FALSE(nested_end.empty());
    // THE SUB-SOLVE'S MAJORS ARE THE OUTER SOLVE'S `restoration_iters`: the fold
    // is `restoration_iters += rs.counters.major_iters`, and the phase runs once
    // (the sub-driver is built with allow_restoration = false).
    EXPECT_EQ(raw_field(nested_end, "iterations"), std::to_string(sol.counters.restoration_iters));
    EXPECT_EQ(raw_field(nested_end, "stop_reason"), "null") << "a depth-1 end is not declared";
    EXPECT_EQ(raw_field(nested_end, "message"), "null");
    EXPECT_EQ(raw_field(nested_end, "diagnostics"), "null");
    EXPECT_EQ(raw_field(nested_end, "work"), "null");
    // ONE walk `qp.mode` line per sub-MAJOR. The sub-solve has one row MORE
    // than that -- the stopped-AT-iterate row, which no subproblem produced.
    EXPECT_EQ(depth1_other, sol.counters.restoration_iters);
    EXPECT_GT(depth1_rows, depth1_other);
    // THE BALANCE, PINNED DIRECTLY (rider 3) rather than left to
    // `pop_depth`'s saturation.
    EXPECT_EQ(json.depth(), 0);
}

// ===========================================================================
// W4 T2 fix round 1 -- the whole-solve line's own golden lines and pins
// ===========================================================================

/// Assigns a DISTINCT value to each counter, so a swapped, duplicated or
/// dropped key changes the golden line's bytes.
void assign_distinct(Index &field, Index &next) { field = next++; }
void assign_distinct(double &field, Index &next) { field = static_cast<double>(next++); }
void assign_distinct(std::array<Index, 5> &field, Index &next) {
    for (Index &e : field) {
        e = next++;
    }
}
void assign_distinct(StartLevel &field, Index &next) {
    field = StartLevel::kHot;
    ++next;
}
// An enum has no distinct integer to take, so it takes a value that is NOT its
// default -- `kNotRun` is `restoration_sub_status`'s default (it was `kOptimal`
// until M7 W2 M3a fix1 gave the field an enum with a not-run zero), and a
// golden line that could not tell the filled counters from the empty ones at
// this key would pin nothing here. `kWorkLimit` is a non-default value
// under BOTH enums.
void assign_distinct(hven::RestorationSubStatus &field, Index &next) {
    field = hven::RestorationSubStatus::kWorkLimit;
    ++next;
}

/// @brief `SqpCounters` with every field distinct, filled THROUGH the tables.
///
/// A loop over the tables rather than 90 hand-written lines: the golden line
/// below is the thing being maintained, and it must move when a field is added.
SqpCounters distinct_counters() {
    SqpCounters c;
    Index next = 1;
#define HVEN_TEST_FILL(f, absent) assign_distinct(c.f, next);
    HVEN_SQP_COUNTERS_FIELDS(HVEN_TEST_FILL)
#undef HVEN_TEST_FILL
#define HVEN_TEST_FILL(f, absent) assign_distinct(c.ssn.f, next);
    HVEN_SSN_COUNTERS_FIELDS(HVEN_TEST_FILL)
#undef HVEN_TEST_FILL
#define HVEN_TEST_FILL(f, absent) assign_distinct(c.ipqp.f, next);
    HVEN_IPQP_COUNTERS_FIELDS(HVEN_TEST_FILL)
#undef HVEN_TEST_FILL
    return c;
}

SolveBeginTraceEvent golden_sqp_begin() {
    SolveBeginTraceEvent e = default_sqp_begin();
    e.optimality_tol = 1e-6;
    e.feasibility_tol = 2e-6;
    e.max_iterations = 300;
    e.sqp->me = 3;
    e.sqp->mi = 5;
    e.sqp->qp_mode = QpTraceMode::kSsn;
    e.sqp->ws_algebra = QpWorkingSetAlgebra::kRefactorize;
    e.sqp->hessian_mode = SqpHessianMode::kLbfgsDamped;
    e.sqp->row_scaling = true;
    e.sqp->column_scaling = SqpColumnScaling::kAlways;
    return e;
}

/// The KEYS of the `counters` object, in stream order, nesting included.
std::vector<std::string> counters_keys(const std::string &line) {
    const std::string token = raw_field(line, "counters");
    std::vector<std::string> keys;
    for (std::size_t i = 0; i + 1 < token.size(); ++i) {
        if (token[i] != '"') {
            continue;
        }
        const std::size_t close = token.find('"', i + 1);
        if (close == std::string::npos || close + 1 >= token.size() || token[close + 1] != ':') {
            continue;
        }
        keys.push_back(token.substr(i + 1, close - i - 1));
        i = close;
    }
    return keys;
}

TEST(JsonLinesTraceSink, GoldenLineSolveBeginSqpBlock) {
    // BYTE-EXACT, from a hand-filled struct with every field DISTINCT.
    //
    // Nothing else in the suite tests key ORDER on this line: every other check
    // is a key lookup, generated from the same table as the writer.
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_solve_begin(golden_sqp_begin());
    EXPECT_EQ(
        os.str(),
        "{\"v\":1,\"ev\":\"solve.begin\",\"seq\":1,\"sid\":1,\"depth\":0,\"eng\":\"sqp\","
        "\"variables\":11,\"free_variables\":1,\"lower_bounded_variables\":2,\"upper_bounded_"
        "variables\":3,\"boxed_variables\":4,\"fixed_variables\":1,\"constraints\":8,\"equality_"
        "rows\":3,\"lower_bounded_rows\":2,\"upper_bounded_rows\":1,\"range_rows\":1,\"free_rows\":"
        "1,\"absent_bounds\":5,\"optimality_tol\":9.9999999999999995e-07,\"feasibility_tol\":1."
        "9999999999999999e-06,\"max_iterations\":300,\"hven_version\":\"0.2.0\",\"sqp\":{\"me\":3,"
        "\"mi\":5,\"qp_mode\":\"ssn\",\"ws_algebra\":\"refactorize\",\"hessian_mode\":\"lbfgs_"
        "damped\",\"row_scaling\":true,\"column_scaling\":\"always\"}}\n");
    EXPECT_EQ(sink.depth(), 0);
}

/// The `counters` object of a `solve.end` line for `distinct_counters()`: the
/// counters table's own bytes, which trace v1 left unchanged.
std::string distinct_counters_json() {
    return "{\"major_iters\":1,\"qp_minor_iters\":2,\"factorizations\":3,\"steps_accepted\":4,"
           "\"rejected_steps\":5,\"soc_steps\":6,\"soc_applied\":7,\"soc_qp_infeasible\":8,\"s"
           "oc_rejected\":9,\"elastic_activations\":10,\"elastic_escalations\":11,\"restoratio"
           "n_iters\":12,\"elastic_from_ipqp_escape\":13,\"ipqp_suspicion_disproved\":14,\"ipq"
           "p_fallback_rung_b\":15,\"elastic_rho0_ceiling_hits\":16,\"elastic_floor_retries\":"
           "17,\"eqp_refine_steps\":18,\"border_refine_steps\":19,\"verdict_refine_steps\":20,"
           "\"suspect_escalations\":21,\"symbolic_analyses\":22,\"start_level_used\":\"hot\",\""
           "full_step_majors\":24,\"watchdog_restores\":25,\"evals_full\":26,\"evals_values\":"
           "27,\"probe_budget_stops\":28,\"crash_seeded_rows\":29,\"crash_seeded_bounds\":30,\""
           "n_seeded\":31,\"seeded_clamped\":32,\"ip_activity_inferred\":33,\"active_set_delta"
           "_total\":34,\"active_set_delta_peak\":35,\"weak_active_peak\":36,\"near_active_pea"
           "k\":37,\"polish_ignored\":38,\"start_point_clamps\":39,\"qp_maxiter_exits\":40,\"q"
           "p_numerical_exits\":41,\"qp_failures_routed\":42,\"qp_failures_in_a_row_peak\":43,"
           "\"qp_cap_retries\":44,\"restoration_entries\":45,\"restoration_h_in\":46,\"restora"
           "tion_h_out\":47,\"restoration_f_in\":48,\"restoration_f_out\":49,\"restoration_sub"
           "_status\":\"work_limit\",\"eqp_refine_iters\":51,\"eqp_residual_refusals\":52,\"eq"
           "p_polish_steps\":53,\"terminal_polish_solves\":54,\"terminal_polish_rejected\":55,"
           "\"terminal_polish_adopted\":56,\"eqp_row_drift_refusals\":57,\"qp_failure_steps_ju"
           "dged\":58,\"qp_failure_steps_accepted\":59,\"nonfinite_subproblems\":60,\"nonfinit"
           "e_box_projections\":61,\"elastic_last_rung_fallbacks\":62,\"restoration_reduction_"
           "resumes\":63,\"eval_hess_calls\":64,\"qn_updates\":65,\"qn_damped\":66,\"qn_skippe"
           "d_nonfinite\":67,\"qn_skipped_zero_step\":68,\"qn_skipped_curvature\":69,\"qn_skip"
           "ped_conditioning\":70,\"qn_rank_dropped\":71,\"qn_resets_consecutive_skips\":72,\""
           "qn_resets_radius_floor\":73,\"qn_resets_restoration_resume\":74,\"qn_pairs_peak\":"
           "75,\"qn_lift_rank_peak\":76,\"qn_kwrong_seen\":77,\"qn_nonpd_seen\":78,\"qn_dimens"
           "ion\":79,\"nonfinite_step_rollbacks\":80,\"restoration_feas_out\":81,\"scaled_conv"
           "ergence_refusals\":82,\"restoration_curvature_switches\":83,\"inertia_suspect_verd"
           "icts\":84,\"inertia_wrong_verdicts\":85,\"dense_platform_retakes\":86,\"dense_flag"
           "ged_factorizations\":87,\"dense_singular_refactors\":88,\"dense_shifted_solve_refu"
           "sals\":89,\"dense_shifted_k0_fallbacks\":90,\"dense_shifted_terminations\":91,\"de"
           "nse_solve_residual_refusals\":92,\"dense_factorizations\":93,\"optimal_row_gray_ex"
           "its\":94,\"payload_ignored\":95,\"reject_funnel_ceiling\":96,\"reject_f_armijo\":9"
           "7,\"reject_h_funnel\":98,\"reject_qp_failed\":99,\"reject_other\":100,\"accepted_f"
           "_type\":101,\"accepted_h_type\":102,\"rejection_run_histogram\":[103,104,105,106,1"
           "07],\"sparse_scaled_retakes\":108,\"sparse_matched_retakes\":109,\"sparse_retake_r"
           "efusals\":110,\"backend_symbolic_analyses\":111,\"backend_symbolic_repeats\":112,\""
           "backend_solves_ordinary\":113,\"backend_solves_detector\":114,\"backend_solves_pol"
           "ish\":115,\"backend_solves_border_refine\":116,\"backend_solve_rhs_ordinary\":117,"
           "\"backend_solve_rhs_detector\":118,\"backend_solve_rhs_polish\":119,\"backend_solv"
           "e_rhs_border_refine\":120,\"backend_solve_refine_ordinary\":121,\"backend_solve_re"
           "fine_detector\":122,\"backend_solve_refine_polish\":123,\"backend_solve_refine_bor"
           "der_refine\":124,\"symbolic_analysis_reuses\":125,\"qp_retry_rung_shrink\":126,\"q"
           "p_retry_rung_cold\":127,\"qp_retry_rung_algebra\":128,\"qp_retry_rung_regularizati"
           "on\":129,\"qp_retry_replays\":130,\"qp_failure_box_projections\":131,\"trust_regio"
           "n_floor_exits\":132,\"soc_qp_iteration_limits\":133,\"restoration_factorizations\""
           ":134,\"restoration_unproductive_entries\":135,\"restoration_no_progress_requests\""
           ":136,\"restoration_budget_cuts\":137,\"restoration_no_accept_refusals\":138,\"rest"
           "oration_refusal_feasibility\":139,\"ssn\":{\"ssn_iters\":140,\"ssn_bulk_flips\":14"
           "1,\"ssn_backtracks\":142,\"ssn_prox_updates\":143,\"ssn_escapes\":144,\"ssn_uncert"
           "ain_peak\":145,\"ssn_refinements\":146,\"ssn_refine_refused\":147,\"ssn_refine_fac"
           "torizations\":148,\"ssn_refine_neg_duals\":149,\"ssn_sign_swept\":150,\"ssn_sign_s"
           "weep_max\":151,\"ssn_escape_budget\":152,\"ssn_escape_singular\":153,\"ssn_escape_"
           "no_contraction\":154,\"ssn_escape_infeasible_suspect\":155,\"ssn_escape_indefinite"
           "\":156,\"ssn_escape_gate_refused\":157},\"ipqp\":{\"ipqp_iters\":158,\"ipqp_factor"
           "izations\":159,\"ipqp_symbolic_analyses\":160,\"ipqp_solves\":161,\"ipqp_pattern_v"
           "erifies\":162,\"ipqp_rho_demanded_max\":163,\"ipqp_rho_demanded_last\":164,\"ipqp_"
           "inertia_retries\":165,\"ipqp_iters_at_elevated_rho\":166,\"ipqp_ladder_reclimbs\":"
           "167,\"ipqp_pivot_reroute_primal\":168,\"ipqp_pivot_reroute_dual_fallback\":169,\"i"
           "pqp_iters_ladder_armed_no_advance\":170,\"ipqp_final_inertia_read\":171,\"ipqp_reg"
           "_decreases\":172,\"ipqp_reg_increases\":173,\"ipqp_prox_center_updates\":174,\"ipq"
           "p_restart_repairs\":175,\"ipqp_restart_shift_max\":176,\"ipqp_mu_adopted\":177,\"i"
           "pqp_warm_restart_abandoned\":178,\"ipqp_declined_pinned\":179,\"ipqp_tier_retired_"
           "after\":180,\"ipqp_face_uncertain\":181,\"ipqp_refine_accepted\":182,\"ipqp_refine"
           "_refused\":183,\"ipqp_to_refine\":184,\"ipqp_to_ssn\":185,\"ipqp_to_walk\":186,\"i"
           "pqp_escapes\":187,\"ipqp_escape_budget\":188,\"ipqp_escape_stall\":189,\"ipqp_esca"
           "pe_indefinite\":190,\"ipqp_escape_numerical\":191,\"ipqp_escape_infeasible_suspect"
           "\":192,\"ipqp_alpha_p_min\":193,\"ipqp_alpha_d_min\":194,\"ipqp_read_kept_tight_si"
           "des\":195,\"ipqp_read_barrier_noise_sides\":196,\"ipqp_mu0_derived\":197,\"ipqp_mu"
           "0_fallbacks\":198,\"ipqp_mu_floor_binds\":199,\"ipqp_ladder_zero_trial_skips\":200"
           "}}";
}

/// The `counters` object for a default-constructed `SqpCounters`.
std::string default_counters_json() {
    return "{\"major_iters\":0,\"qp_minor_iters\":0,\"factorizations\":0,\"steps_accepted\":0,"
           "\"rejected_steps\":0,\"soc_steps\":0,\"soc_applied\":0,\"soc_qp_infeasible\":0,\"s"
           "oc_rejected\":0,\"elastic_activations\":0,\"elastic_escalations\":0,\"restoration_"
           "iters\":0,\"elastic_from_ipqp_escape\":0,\"ipqp_suspicion_disproved\":0,\"ipqp_fal"
           "lback_rung_b\":0,\"elastic_rho0_ceiling_hits\":0,\"elastic_floor_retries\":0,\"eqp"
           "_refine_steps\":0,\"border_refine_steps\":0,\"verdict_refine_steps\":0,\"suspect_e"
           "scalations\":0,\"symbolic_analyses\":0,\"start_level_used\":\"cold\",\"full_step_m"
           "ajors\":0,\"watchdog_restores\":0,\"evals_full\":0,\"evals_values\":0,\"probe_budg"
           "et_stops\":0,\"crash_seeded_rows\":0,\"crash_seeded_bounds\":0,\"n_seeded\":0,\"se"
           "eded_clamped\":0,\"ip_activity_inferred\":0,\"active_set_delta_total\":0,\"active_"
           "set_delta_peak\":0,\"weak_active_peak\":0,\"near_active_peak\":0,\"polish_ignored\""
           ":0,\"start_point_clamps\":0,\"qp_maxiter_exits\":0,\"qp_numerical_exits\":0,\"qp_f"
           "ailures_routed\":0,\"qp_failures_in_a_row_peak\":0,\"qp_cap_retries\":0,\"restorat"
           "ion_entries\":0,\"restoration_h_in\":0,\"restoration_h_out\":0,\"restoration_f_in\""
           ":0,\"restoration_f_out\":0,\"restoration_sub_status\":\"not_run\",\"eqp_refine_ite"
           "rs\":0,\"eqp_residual_refusals\":0,\"eqp_polish_steps\":0,\"terminal_polish_solves"
           "\":0,\"terminal_polish_rejected\":0,\"terminal_polish_adopted\":0,\"eqp_row_drift_"
           "refusals\":0,\"qp_failure_steps_judged\":0,\"qp_failure_steps_accepted\":0,\"nonfi"
           "nite_subproblems\":0,\"nonfinite_box_projections\":0,\"elastic_last_rung_fallbacks"
           "\":0,\"restoration_reduction_resumes\":0,\"eval_hess_calls\":0,\"qn_updates\":0,\""
           "qn_damped\":0,\"qn_skipped_nonfinite\":0,\"qn_skipped_zero_step\":0,\"qn_skipped_c"
           "urvature\":0,\"qn_skipped_conditioning\":0,\"qn_rank_dropped\":0,\"qn_resets_conse"
           "cutive_skips\":0,\"qn_resets_radius_floor\":0,\"qn_resets_restoration_resume\":0,\""
           "qn_pairs_peak\":0,\"qn_lift_rank_peak\":0,\"qn_kwrong_seen\":0,\"qn_nonpd_seen\":0"
           ",\"qn_dimension\":0,\"nonfinite_step_rollbacks\":0,\"restoration_feas_out\":0,\"sc"
           "aled_convergence_refusals\":0,\"restoration_curvature_switches\":0,\"inertia_suspe"
           "ct_verdicts\":0,\"inertia_wrong_verdicts\":0,\"dense_platform_retakes\":0,\"dense_"
           "flagged_factorizations\":0,\"dense_singular_refactors\":0,\"dense_shifted_solve_re"
           "fusals\":0,\"dense_shifted_k0_fallbacks\":0,\"dense_shifted_terminations\":0,\"den"
           "se_solve_residual_refusals\":0,\"dense_factorizations\":0,\"optimal_row_gray_exits"
           "\":0,\"payload_ignored\":0,\"reject_funnel_ceiling\":0,\"reject_f_armijo\":0,\"rej"
           "ect_h_funnel\":0,\"reject_qp_failed\":0,\"reject_other\":0,\"accepted_f_type\":0,\""
           "accepted_h_type\":0,\"rejection_run_histogram\":[0,0,0,0,0],\"sparse_scaled_retake"
           "s\":0,\"sparse_matched_retakes\":0,\"sparse_retake_refusals\":0,\"backend_symbolic"
           "_analyses\":0,\"backend_symbolic_repeats\":0,\"backend_solves_ordinary\":0,\"backe"
           "nd_solves_detector\":0,\"backend_solves_polish\":0,\"backend_solves_border_refine\""
           ":0,\"backend_solve_rhs_ordinary\":0,\"backend_solve_rhs_detector\":0,\"backend_sol"
           "ve_rhs_polish\":0,\"backend_solve_rhs_border_refine\":0,\"backend_solve_refine_ord"
           "inary\":0,\"backend_solve_refine_detector\":0,\"backend_solve_refine_polish\":0,\""
           "backend_solve_refine_border_refine\":0,\"symbolic_analysis_reuses\":0,\"qp_retry_r"
           "ung_shrink\":0,\"qp_retry_rung_cold\":0,\"qp_retry_rung_algebra\":0,\"qp_retry_run"
           "g_regularization\":0,\"qp_retry_replays\":0,\"qp_failure_box_projections\":0,\"tru"
           "st_region_floor_exits\":0,\"soc_qp_iteration_limits\":0,\"restoration_factorizatio"
           "ns\":0,\"restoration_unproductive_entries\":0,\"restoration_no_progress_requests\""
           ":0,\"restoration_budget_cuts\":0,\"restoration_no_accept_refusals\":0,\"restoratio"
           "n_refusal_feasibility\":null,\"ssn\":{\"ssn_iters\":0,\"ssn_bulk_flips\":0,\"ssn_b"
           "acktracks\":0,\"ssn_prox_updates\":0,\"ssn_escapes\":0,\"ssn_uncertain_peak\":0,\""
           "ssn_refinements\":0,\"ssn_refine_refused\":0,\"ssn_refine_factorizations\":0,\"ssn"
           "_refine_neg_duals\":0,\"ssn_sign_swept\":0,\"ssn_sign_sweep_max\":0,\"ssn_escape_b"
           "udget\":0,\"ssn_escape_singular\":0,\"ssn_escape_no_contraction\":0,\"ssn_escape_i"
           "nfeasible_suspect\":0,\"ssn_escape_indefinite\":0,\"ssn_escape_gate_refused\":0},\""
           "ipqp\":{\"ipqp_iters\":0,\"ipqp_factorizations\":0,\"ipqp_symbolic_analyses\":0,\""
           "ipqp_solves\":0,\"ipqp_pattern_verifies\":0,\"ipqp_rho_demanded_max\":0,\"ipqp_rho"
           "_demanded_last\":0,\"ipqp_inertia_retries\":0,\"ipqp_iters_at_elevated_rho\":0,\"i"
           "pqp_ladder_reclimbs\":0,\"ipqp_pivot_reroute_primal\":0,\"ipqp_pivot_reroute_dual_"
           "fallback\":0,\"ipqp_iters_ladder_armed_no_advance\":0,\"ipqp_final_inertia_read\":"
           "0,\"ipqp_reg_decreases\":0,\"ipqp_reg_increases\":0,\"ipqp_prox_center_updates\":0"
           ",\"ipqp_restart_repairs\":0,\"ipqp_restart_shift_max\":0,\"ipqp_mu_adopted\":0,\"i"
           "pqp_warm_restart_abandoned\":0,\"ipqp_declined_pinned\":0,\"ipqp_tier_retired_afte"
           "r\":null,\"ipqp_face_uncertain\":0,\"ipqp_refine_accepted\":0,\"ipqp_refine_refuse"
           "d\":0,\"ipqp_to_refine\":0,\"ipqp_to_ssn\":0,\"ipqp_to_walk\":0,\"ipqp_escapes\":0"
           ",\"ipqp_escape_budget\":0,\"ipqp_escape_stall\":0,\"ipqp_escape_indefinite\":0,\"i"
           "pqp_escape_numerical\":0,\"ipqp_escape_infeasible_suspect\":0,\"ipqp_alpha_p_min\""
           ":null,\"ipqp_alpha_d_min\":null,\"ipqp_read_kept_tight_sides\":0,\"ipqp_read_barri"
           "er_noise_sides\":0,\"ipqp_mu0_derived\":0,\"ipqp_mu0_fallbacks\":0,\"ipqp_mu_floor"
           "_binds\":0,\"ipqp_ladder_zero_trial_skips\":0}}";
}

TEST(JsonLinesTraceSink, GoldenLineSolveEndSqpBlockWithEveryCounterDistinct) {
    // THE COUNTERS IN TABLE ORDER, every value distinct, so a reorder or a drop
    // moves these bytes. The counters object is the table's own text
    // (`distinct_counters_json`, unchanged by trace v1); the bytes around it
    // are the v1 `solve.end` core and the `sqp` block.
    const SqpCounters c = distinct_counters();
    SolveEndTraceEvent e = default_sqp_end(c);
    e.status = SolveStatus::kWorkLimit;
    e.stop_reason = StopReason::kNone;
    e.message = std::string_view("work limit reached");
    e.iterations = 42;
    e.returned_best_iterate = true;
    e.infeasibility_certified = false;
    e.f = -1.5;
    e.diagnostics = DeclaredDiagnosticsTrace{1e-7, 2e-8, 0.125};
    e.work = WorkCounters{11, 12, 13, 14, 15, 16};
    e.wall_s = 0.5;
    e.sqp->convergence.stationarity = 3e-7;
    e.sqp->convergence.feasibility = 4e-8;
    e.sqp->convergence.complementarity = 0.0625;
    e.sqp->convergence.kkt_residual = 3e-7;
    e.sqp->scaling_active = true;
    e.sqp->obj_scale = 0.25;
    e.sqp->row_scale_min = 0.5;
    e.sqp->row_scale_max = 4.0;
    e.sqp->scaled_kkt_residual = 7.5e-09;
    e.sqp->columns_active = true;
    e.sqp->col_spread = 2048.0;
    e.sqp->start_level_used = StartLevel::kHot;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_solve_end(e);
    EXPECT_EQ(
        os.str(),
        std::string(
            "{\"v\":1,\"ev\":\"solve.end\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\","
            "\"status\":\"work_limit\",\"stop_reason\":\"none\",\"message\":\"work limit "
            "reached\",\"iterations\":42,\"returned_best_iterate\":true,\"infeasibility_"
            "certified\":false,\"f\":-1.5,\"diagnostics\":{\"stationarity\":9.9999999999999995e-08,"
            "\"feasibility\":2e-08,\"complementarity\":0.125},\"convergence\":{\"stationarity\":2."
            "9999999999999999e-07,\"feasibility\":4.0000000000000001e-08,\"complementarity\":0."
            "0625,\"kkt_residual\":2.9999999999999999e-07},\"work\":{\"objective_evals\":11,"
            "\"gradient_evals\":12,\"constraint_evals\":13,\"jacobian_evals\":14,\"hessian_evals\":"
            "15,\"factorizations\":16},\"counters\":") +
            distinct_counters_json() +
            ",\"wall_s\":0.5,\"sqp\":{\"scaling_active\":true,\"obj_scale\":0.25,\"row_scale_min\":"
            "0.5,\"row_scale_max\":4,\"scaled_kkt_residual\":7.4999999999999993e-09,\"columns_"
            "active\":true,\"col_spread\":2048,\"start_level_used\":\"hot\"}}\n");
    EXPECT_EQ(sink.depth(), 0);
}

TEST(JsonLinesTraceSink, GoldenLineSolveEndSqpBlockWritesTheAbsenceSentinelsAsNull) {
    // THE OTHER HALF OF THE ABSENCE RULE: a default-constructed `SqpCounters`
    // holds the documented absence sentinels. `ipqp_alpha_p_min` and
    // `ipqp_alpha_d_min` at `+infinity` ("no step observed yet") and
    // `ipqp_tier_retired_after` at 0 ("never retired") must every one read
    // `null`, not a value; and `restoration_sub_status` reads `not_run`, a VALUE
    // of that counter and not a sentinel the stream maps.
    //
    // The same event carries the other absences a solve.end can have: the
    // depth-1 shape, with no stop reason, message, diagnostics or work.
    const SqpCounters c;
    SolveEndTraceEvent e = default_sqp_end(c);
    e.status = SolveStatus::kNumericalError;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_solve_end(e);
    EXPECT_EQ(os.str(),
              std::string("{\"v\":1,\"ev\":\"solve.end\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":"
                          "\"sqp\",\"status\":\"numerical_error\",\"stop_reason\":null,\"message\":"
                          "null,\"iterations\":0,\"returned_best_iterate\":false,\"infeasibility_"
                          "certified\":false,\"f\":0,\"diagnostics\":null,\"convergence\":{"
                          "\"stationarity\":\"nan\",\"feasibility\":\"nan\",\"complementarity\":"
                          "\"nan\",\"kkt_residual\":\"nan\"},\"work\":null,\"counters\":") +
                  default_counters_json() +
                  ",\"wall_s\":0,\"sqp\":{\"scaling_active\":false,\"obj_scale\":1,\"row_scale_"
                  "min\":1,\"row_scale_max\":1,\"scaled_kkt_residual\":0,\"columns_active\":false,"
                  "\"col_spread\":1,\"start_level_used\":\"cold\"}}\n");
    const std::string line = os.str();
    EXPECT_EQ(raw_field(line, "stop_reason"), "null");
    EXPECT_EQ(raw_field(line, "message"), "null");
    EXPECT_EQ(raw_field(line, "diagnostics"), "null");
    EXPECT_EQ(raw_field(line, "work"), "null");
}

TEST(JsonLinesTraceSink, SqpSolveEndKeysAreExactlyTheTablesAndAllDistinct) {
    // THE COUNT ASSERTS CANNOT SEE A DROP PAIRED WITH A DUPLICATE (fix round
    // 1, R10): 90 entries of which one names a field twice and one is missing
    // still counts 90.
    //
    // The emitted KEYS are what settles it.
    const SqpCounters c = distinct_counters();
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_solve_end(default_sqp_end(c));
    const std::vector<std::string> keys = counters_keys(os.str());
    // The two nesting keys are the tables' own structure, not fields.
    EXPECT_EQ(keys.size(),
              kSqpCountersFieldCount + kSsnCountersFieldCount + kIpqpCountersFieldCount + 2);
    const std::set<std::string> unique(keys.begin(), keys.end());
    EXPECT_EQ(unique.size(), keys.size()) << "a key is emitted twice";
    EXPECT_EQ(unique.count("ssn"), 1u);
    EXPECT_EQ(unique.count("ipqp"), 1u);
}

TEST(JsonLinesTraceSink, SqpSolveEndReadsTheReturnedSolutionAndNotAMovedFromLocal) {
    // FIX ROUND 1, R1. The `end` line is now written by an explicit statement
    // in the wrapper, from the object the wrapper is about to return -- not by
    // a destructor running after the return statement has begun to move it.
    //
    // What this pin can see: the line equals the counters the CALLER receives,
    // on a solve with a non-empty history that the caller then MOVES.
    //
    // A read of a moved-from local becomes visible here the day a counter stops
    // being trivially copyable -- the failure the old shape could not pin.
    SqpOptions opts;
    opts.qp_mode = QpMode::kIpqp;
    opts.common.max_iterations = 60;
    const HsProblem p = make_hs(38);
    SqpSolver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    SqpResult sol = driver.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());
    ASSERT_FALSE(sol.history.empty());
    const SqpResult moved = std::move(sol);

    const std::string end = only_line(os.str(), "solve.end");
    ASSERT_FALSE(end.empty());
    EXPECT_EQ(raw_field(end, "iterations"), std::to_string(moved.iterations));
    expect_end_line_counters(end, moved.counters);
}

TEST(JsonLinesTraceSink, AnArmedMaskFailingOnTheVERYLASTLinePropagatesRatherThanTerminating) {
    // THE LEG THAT WOULD HAVE CAUGHT IT. The T1 armed-mask pin's 4096-byte
    // budget fails MID-solve, on an ordinary emit.
    //
    // The `end` line is the last write of every solve, and written from a
    // `noexcept` destructor a failure there called `std::terminate`.
    //
    // The budget is measured, not guessed: solve once to learn the stream, then
    // re-solve with room for everything except the final line.
    SqpOptions opts;
    opts.common.max_iterations = 60;
    const HsProblem p = make_hs(24);
    std::ostringstream measure;
    {
        JsonLinesTraceSink json(measure);
        SqpSolver driver(opts);
        driver.attach_trace(&json);
        driver.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());
    }
    const std::vector<std::string> lines = split_lines(measure.str());
    ASSERT_GE(lines.size(), 2u);
    ASSERT_EQ(event_name(lines.back()), "solve.end") << "the end line must be the last write";
    const std::streamsize budget =
        static_cast<std::streamsize>(measure.str().size() - lines.back().size() - 1);

    FailAfterBuf buf(budget);
    std::ostream out(&buf);
    out.exceptions(std::ios::badbit);
    SqpSolver traced(opts);
    JsonLinesTraceSink json(out);
    traced.attach_trace(&json);
    EXPECT_THROW(traced.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point()),
                 std::ios_base::failure);
    EXPECT_EQ(buf.taken.size(), static_cast<std::size_t>(budget)) << "it really failed on the last";
}

TEST(JsonLinesTraceSink, ARefusedArgumentWritesNothingAndLeavesTheNextSolveAtDepthZero) {
    // FIX ROUND 1, R3(a)/R11. `SqpSolver::solve` refuses a wrong-sized `x0`
    // with `std::invalid_argument` -- a recoverable API refusal.
    //
    // Emitting `begin` first left the sink one solve deep for ever, and every
    // later stream on it read `"depth":1`.
    //
    // ASSERTED ON THE STREAM, not on `depth()`: the leak is invisible to the
    // accessor, which reads 0 between solves either way.
    SqpOptions opts;
    opts.common.max_iterations = 60;
    const HsProblem p = make_hs(24);
    std::ostringstream os;
    JsonLinesTraceSink json(os);

    SqpSolver bad(opts);
    bad.attach_trace(&json);
    EXPECT_THROW(bad.solve(hven::detail::problem_ref(*p.model), Vec::Zero(p.model->n() + 1)),
                 std::invalid_argument);
    EXPECT_EQ(os.str(), "") << "a refused call writes nothing at all";
    EXPECT_EQ(json.lines_written(), 0);

    SqpSolver good(opts);
    good.attach_trace(&json);
    const SqpResult sol = good.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());
    ASSERT_GT(sol.counters.major_iters, 0);
    const std::vector<std::string> lines = split_lines(os.str());
    ASSERT_FALSE(lines.empty());
    for (const std::string &l : lines) {
        EXPECT_EQ(raw_field(l, "depth"), "0") << l;
    }
    EXPECT_EQ(event_name(lines.front()), "solve.begin");
    EXPECT_EQ(event_name(lines.back()), "solve.end");
}

/// @brief A model whose objective gradient throws on the second evaluation.
///
/// The one shape that reaches a throw AFTER `begin`: a caller callback failing
/// mid-solve, which no argument check can refuse in advance.
class ThrowsOnSecondGradientModel final : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override { return 0.5 * x.squaredNorm(); }
    Vec eval_grad(const Vec &x) const override {
        if (++calls_ > 1) {
            throw std::runtime_error("model callback failed mid-solve");
        }
        return x;
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
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
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    const Vec &lower() const override {
        static const Vec v = Vec::Constant(2, -10.0);
        return v;
    }
    const Vec &upper() const override {
        static const Vec v = Vec::Constant(2, 10.0);
        return v;
    }
    Vec start_point() const override { return Vec::Constant(2, 3.0); }

  private:
    mutable Index calls_ = 0;
};

TEST(JsonLinesTraceSink, AThrowingSolveLeavesTheSinkCleanForTheNextSolve) {
    // FIX ROUND 1, R3(b). "Exceptions excluded" stands: a solve that threw
    // writes `begin` and no `end`, which is the honest record.
    //
    // The engine unwinds the sink itself (`on_solve_abort`) on the way out, so the next solve
    // on the same sink reads depth 0 and takes a fresh sid with NO recovery call. `seq` is
    // never reset: the lines already written are part of the artifact.
    SqpOptions opts;
    opts.common.max_iterations = 60;
    std::ostringstream os;
    JsonLinesTraceSink json(os);

    ThrowsOnSecondGradientModel bad_model;
    SqpSolver thrower(opts);
    thrower.attach_trace(&json);
    EXPECT_THROW(thrower.solve(hven::detail::problem_ref(bad_model), (bad_model).start_point()),
                 std::runtime_error);
    const Index lines_after_throw = json.lines_written();
    ASSERT_GT(lines_after_throw, 0);

    // THE ACCEPTED MAJOR'S ROW IS ALREADY IN THE STREAM WHEN THE SECOND
    // GRADIENT THROWS (M6 W5 T6 cut (c); ownership doc section 7).
    //
    // This is an ORDERING assertion and it is the only executable one there
    // is. The trace goldens, the row count and the `sqp.major` == history
    // identity all pin CONTENT: a push moved BELOW the accepted commit would
    // preserve every one of them, because the row was measured before either
    // ordering and its values do not depend on which. What distinguishes the
    // two is that this model THROWS between them.
    //
    // The shape: `eval_grad` throws on its SECOND call. The first is the
    // solve's own initial evaluation; major 0 then solves an unconstrained
    // quadratic, accepts the step, PUSHES ITS ROW, and the direct-accept
    // branch upgrades the trial in place -- which is the second gradient, and
    // the throw. So exactly one `sqp.major` line must already be written, and
    // it must be an ACCEPTED row. Move the push below the derivative refresh
    // and there are zero.
    {
        const std::vector<std::string> before_throw = split_lines(os.str());
        std::vector<std::string> majors;
        for (const std::string &l : before_throw) {
            if (event_name(l) == "iter") {
                majors.push_back(l);
            }
        }
        ASSERT_EQ(majors.size(), 1u)
            << "the accepted major's row must be emitted BEFORE the commit that throws";
        EXPECT_EQ(raw_field(majors.front(), "iter"), "0");
        const std::string verdict = raw_field(majors.front(), "verdict");
        EXPECT_TRUE(verdict == "\"accept_f\"" || verdict == "\"accept_h\"")
            << "the row already in the stream is the ACCEPTED one, verdict=" << verdict;
    }

    const HsProblem p = make_hs(24);
    SqpSolver good(opts);
    good.attach_trace(&json);
    const SqpResult sol = good.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());
    ASSERT_GT(sol.counters.major_iters, 0);

    const std::vector<std::string> lines = split_lines(os.str());
    Index begins = 0;
    Index ends = 0;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        EXPECT_EQ(raw_field(lines[i], "seq"), std::to_string(i + 1)) << "seq is NOT reset";
        if (static_cast<Index>(i) >= lines_after_throw) {
            EXPECT_EQ(raw_field(lines[i], "depth"), "0") << lines[i];
        }
        begins += (event_name(lines[i]) == "solve.begin") ? 1 : 0;
        ends += (event_name(lines[i]) == "solve.end") ? 1 : 0;
    }
    EXPECT_EQ(begins, 2);
    EXPECT_EQ(ends, 1) << "the solve that threw wrote no end, by design";
}

TEST(JsonLinesTraceSink, SqpMajorReproducesTheHistoryOnTheRowThatSeedsRestoration) {
    // FIX ROUND 1, R9. `restoration_seed_used` was decided AFTER the row had
    // been pushed and emitted, so the stream disagreed with `history` on
    // exactly the rows that seeded a restoration.
    //
    // No row-by-row pin reached one, because HS24/HS38/HS25 never restore.
    CircleAndFarLineModel model;
    SqpOptions opts;
    opts.common.max_iterations = 200;
    SqpSolver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());

    ASSERT_GE(sol.counters.restoration_iters, 1) << "the phase must have RUN";
    Index seeded_rows = 0;
    for (const IterationRecord &r : sol.history) {
        seeded_rows += r.sqp->restoration_seed_used ? 1 : 0;
    }
    ASSERT_GT(seeded_rows, 0) << "non-vacuous: a row really takes a candidate seed";

    std::vector<std::string> rows;
    for (const std::string &l : split_lines(os.str())) {
        if (event_name(l) == "iter" && raw_field(l, "depth") == "0") {
            rows.push_back(l);
        }
    }
    ASSERT_EQ(rows.size(), sol.history.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        SCOPED_TRACE("row " + std::to_string(i));
        expect_iter_line_is_row(rows[i], sol.history[i], static_cast<Index>(i),
                                mode_of_token(raw_field(rows[i], "mode")), 0);
    }
}

TEST(JsonLinesTraceSink, TheRowsModeAndTheDispatchRecordDifferOnTheSsnWarmGrade) {
    // FIX ROUND 1, R4. `sqp.major.mode` is the arm that produced the ROW's
    // step; `qp.mode` is the DISPATCH record.
    //
    // They agree at kWalk and kSsn, and differ under kIpqp on the majors routed
    // to the SSN warm grade -- not the kSsn arm, and writing no line.
    //
    // HS3 at kIpqp is the cell that exhibits it (the same cell
    // test_ipqp_trace.cpp uses for the `to_ssn` row).
    SqpOptions opts;
    opts.qp_mode = QpMode::kIpqp;
    opts.common.max_iterations = 60;
    const HsProblem p = make_hs(3);
    SqpSolver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpResult sol =
        driver.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());

    ASSERT_GT(sol.counters.ipqp.ipqp_to_ssn, 0) << "non-vacuous: the grade must be reached";
    Index rows_reading_ssn = 0;
    for (const std::string &l : split_lines(os.str())) {
        if (event_name(l) == "iter" && raw_field(l, "mode") == "\"ssn\"") {
            ++rows_reading_ssn;
        }
    }
    EXPECT_GT(rows_reading_ssn, 0) << "some row's step came from the warm grade";
    // RE-DERIVED AT M6 W4 T5 (declared, additive). The grade now writes a line
    // of its own, so "no ssn line exists" is no longer the right statement --
    // "no ssn DISPATCH line exists" is.
    //
    // The grade's line carries `site` `ssn_warm_grade`: the same distinction,
    // now stated in the stream rather than by absence.
    Index ssn_dispatch = 0, ssn_grade = 0;
    for (const std::string &l : split_lines(os.str())) {
        if (event_name(l) != "qp.mode" || raw_field(l, "mode") != "\"ssn\"") {
            continue;
        }
        ssn_dispatch += (raw_field(l, "site") == "\"dispatch\"") ? 1 : 0;
        ssn_grade += (raw_field(l, "site") == "\"ssn_warm_grade\"") ? 1 : 0;
    }
    EXPECT_EQ(ssn_dispatch, 0)
        << "no ssn dispatch record exists: the two readings are different questions";
    EXPECT_EQ(ssn_grade, sol.counters.ipqp.ipqp_to_ssn) << "one grade line per routed subproblem";
}

TEST(JsonLinesTraceSink, TheRequestingRowIsWrittenAFTERTheNestedSolveItAskedFor) {
    // FIX ROUND 2, F4(iv). R9 pushes the requesting row AFTER the restoration
    // call, so its `sqp.major` line now FOLLOWS the whole depth-1 solve.
    //
    // `history` is unaffected -- nothing else pushes to the outer vector during
    // a restoration -- but the stream order moved, and a reader joining on
    // `seq` must know that a depth-0 bracket can contain a depth-1 solve.
    CircleAndFarLineModel model;
    SqpOptions opts;
    opts.common.max_iterations = 200;
    SqpSolver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());
    ASSERT_GE(sol.counters.restoration_iters, 1);

    Index nested_end_seq = 0;
    Index first_row_after = 0;
    for (const std::string &l : split_lines(os.str())) {
        const Index seq = static_cast<Index>(std::stoll(raw_field(l, "seq")));
        if (raw_field(l, "depth") == "1" && event_name(l) == "solve.end") {
            nested_end_seq = seq;
            continue;
        }
        if (nested_end_seq != 0 && first_row_after == 0 && raw_field(l, "depth") == "0" &&
            event_name(l) == "iter") {
            first_row_after = seq;
        }
    }
    ASSERT_GT(nested_end_seq, 0) << "the fixture must really restore";
    ASSERT_GT(first_row_after, 0) << "the requesting row must be written at all";
    EXPECT_GT(first_row_after, nested_end_seq)
        << "the requesting row is written after the solve it asked for";
}

// ===========================================================================
// Trace v1 `iter`, the interior-point block, and the two whole-solve records
// ===========================================================================
//
// The DRIVER-side pins (both emit sites, the count identity, the callback as
// the oracle, the null sink, the exit statuses, the mixed stream) are in
// tests/ipm/test_ipm_trace.cpp; these are the serializer's.

/// Three interior-point rows, one per shape the block has: a barrier row with
/// the step leaving it (every optional present), a barrier row with NO step
/// leaving it (every step-bearing optional absent), and a restoration row.
///
/// The five core bools carry distinct signatures across the three lines
/// (`accepted` 1, `soc` 2, `watchdog` 3, `rolled_back` 4; bit j of the number
/// says whether line j reads `true`), so a swap of two of them moves a line;
/// `recovering` is the restoration line.
IterationRecord golden_ipm_row(int line) {
    IterationRecord r = default_ipm_record();
    const auto bit = [line](int i) { return ((i >> line) & 1) != 0; };
    r.soc = bit(2);
    r.watchdog = bit(3);
    r.rolled_back = bit(4);
    IpmIterationRow &o = *r.ipm;
    switch (line) {
    case 0:
        r.iteration = 7;
        r.tier = IterationTier::kBarrier;
        r.f = 1.5;
        r.stationarity = 0.5;
        r.tested_stationarity = 0.5;
        r.feasibility = 0.0625;
        r.tested_feasibility = 0.0625;
        r.complementarity = 0.125;
        r.tested_complementarity = 0.125;
        r.step_norm = 0.375;
        r.accepted = bit(1);
        r.factorizations = 19;
        o.phase = 1;
        o.mu = 0.25;
        o.alpha_primal = 0.63;
        o.alpha_dual = 0.56;
        o.alpha_scale = 0.7;
        o.line_search_trials = 2;
        o.regularization_primal = 1e-8;
        o.regularization_primal_total = 2e-8;
        o.regularization_dual = 3e-8;
        o.proximal_primal = 4e-8;
        o.inertia_corrections = 3;
        o.perturbed_pivots = 5;
        o.barrier_objective = 2.25;
        o.feasibility_eq = 0.0625;
        o.feasibility_ineq = 0.03125;
        o.merit = 13.25;
        o.merit_penalty_1 = 3.5;
        o.merit_penalty_2 = 4.75;
        o.max_multiplier_eq = 11.0;
        o.max_multiplier_ineq = 12.5;
        o.first_rejected_trial = 0;
        o.theta_first_rejected = 0.0;
        o.eval_exceptions = 4;
        break;
    case 1:
        // Reached by a step and left by none: the four exit sites. Everything
        // the step produced is `null`, and the v0 zeros and sentinels with it.
        r.iteration = 8;
        r.tier = IterationTier::kBarrier;
        r.f = 1.25;
        r.stationarity = 3.1e-9;
        r.tested_stationarity = 3.1e-9;
        r.feasibility = 2.2e-15;
        r.tested_feasibility = 2.2e-15;
        r.complementarity = 2.5e-9;
        r.tested_complementarity = 2.5e-9;
        r.factorizations = 20;
        o.phase = 1;
        o.mu = 1e-9;
        o.feasibility_eq = 2.2e-15;
        o.feasibility_ineq = 1e-16;
        o.merit_penalty_1 = 3.5;
        o.merit_penalty_2 = 4.75;
        o.max_multiplier_eq = 11.0;
        o.max_multiplier_ineq = 12.5;
        break;
    default:
        r.iteration = 3;
        r.recovering = true;
        r.tier = IterationTier::kRestoration;
        r.f_recovery = 0.45;
        r.stationarity = 0.2;
        r.tested_stationarity = 0.2;
        r.feasibility = 0.9;
        r.tested_feasibility = 0.9;
        r.complementarity = 0.05;
        r.tested_complementarity = 0.05;
        r.step_norm = 0.3;
        r.accepted = bit(1);
        r.factorizations = 11;
        o.phase = 0;
        o.mu = 0.1;
        o.alpha_primal = 1.0;
        o.alpha_dual = 1.0;
        o.alpha_scale = 1.0;
        o.line_search_trials = 0;
        o.regularization_primal = 0.0;
        o.regularization_primal_total = 0.0;
        o.inertia_corrections = 0;
        o.barrier_objective = -0.25;
        o.feasibility_eq = 0.9;
        o.feasibility_ineq = 0.5;
        o.merit = 0.1;
        o.merit_penalty_1 = 100.0;
        o.merit_penalty_2 = 1000.0;
        o.max_multiplier_eq = 1e3;
        o.max_multiplier_ineq = 1e4;
        o.first_rejected_trial = 1;
        o.theta_first_rejected = 0.9;
        break;
    }
    return r;
}

TEST(JsonLinesTraceSink, GoldenLineIterIpmRowWithEveryOptionalPresent) {
    EXPECT_EQ(
        iter_line(golden_ipm_row(0)),
        "{\"v\":1,\"ev\":\"iter\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"ipm\",\"iter\":7,"
        "\"recovering\":false,\"tier\":\"bar\",\"f\":1.5,\"f_recovery\":null,\"stationarity\":0.5,"
        "\"feasibility\":0.0625,\"complementarity\":0.125,\"step\":0.375,\"accepted\":true,\"soc\":"
        "false,\"watchdog\":true,\"rolled_back\":false,\"factorizations\":19,\"stationarity_"
        "caller\":0.5,\"feasibility_caller\":0.0625,\"complementarity_caller\":0.125,\"ipm\":{"
        "\"phase\":1,"
        "\"mu\":0.25,\"alpha_primal\":0.63,\"alpha_dual\":0.56000000000000005,\"alpha_scale\":0."
        "69999999999999996,\"line_search_trials\":2,\"regularization_primal\":1e-08,"
        "\"regularization_primal_total\":2e-08,\"regularization_dual\":2.9999999999999997e-08,"
        "\"proximal_primal\":4.0000000000000001e-08,\"inertia_corrections\":3,\"perturbed_pivots\":"
        "5,\"barrier_objective\":2.25,\"feasibility_eq\":0.0625,\"feasibility_ineq\":0.03125,"
        "\"merit\":13.25,\"merit_penalty_1\":3.5,\"merit_penalty_2\":4.75,\"max_multiplier_eq\":11,"
        "\"max_multiplier_ineq\":12.5,\"first_rejected_trial\":0,\"theta_first_rejected\":0,\"eval_"
        "exceptions\":4}}\n");
}

TEST(JsonLinesTraceSink, GoldenLineIterIpmRowWithNoStepLeavingItWritesNulls) {
    const IterationRecord r = golden_ipm_row(1);
    EXPECT_EQ(
        iter_line(r),
        "{\"v\":1,\"ev\":\"iter\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"ipm\",\"iter\":8,"
        "\"recovering\":false,\"tier\":\"bar\",\"f\":1.25,\"f_recovery\":null,\"stationarity\":3."
        "1e-09,\"feasibility\":2.1999999999999999e-15,\"complementarity\":2.5000000000000001e-09,"
        "\"step\":null,\"accepted\":null,\"soc\":true,\"watchdog\":true,\"rolled_back\":false,"
        "\"factorizations\":20,\"stationarity_caller\":3.1e-09,\"feasibility_caller\":2."
        "1999999999999999e-15,\"complementarity_caller\":2.5000000000000001e-09,\"ipm\":{\"phase\":"
        "1,\"mu\":1.0000000000000001e-09,\"alpha_primal\":"
        "null,\"alpha_dual\":null,\"alpha_scale\":null,\"line_search_trials\":null,"
        "\"regularization_primal\":null,\"regularization_primal_total\":null,\"regularization_"
        "dual\":null,\"proximal_primal\":null,\"inertia_corrections\":null,\"perturbed_pivots\":"
        "null,\"barrier_objective\":null,\"feasibility_eq\":2.1999999999999999e-15,\"feasibility_"
        "ineq\":9.9999999999999998e-17,\"merit\":null,\"merit_penalty_1\":3.5,\"merit_penalty_2\":"
        "4.75,\"max_multiplier_eq\":11,\"max_multiplier_ineq\":12.5,\"first_rejected_trial\":null,"
        "\"theta_first_rejected\":null,\"eval_exceptions\":0}}\n");
    const std::string line = iter_line(r);
    // Every field the step would have produced is `null`: never a zero, and never
    // the v0 `-1` sentinels.
    for (const char *k :
         {"step", "accepted", "alpha_primal", "alpha_dual", "alpha_scale", "line_search_trials",
          "regularization_primal", "regularization_primal_total", "regularization_dual",
          "proximal_primal", "inertia_corrections", "perturbed_pivots", "barrier_objective",
          "merit", "first_rejected_trial", "theta_first_rejected"}) {
        EXPECT_EQ(raw_field(line, k), "null") << k;
    }
    EXPECT_EQ(line.find("\":-1"), std::string::npos) << "no sentinel escapes as a negative count";
}

TEST(JsonLinesTraceSink, GoldenLineIterIpmRestorationRow) {
    // Restoration runs in place: depth 0, tier `rest`, the restoration objective
    // in `f_recovery` and the declared objective `null`.
    const IterationRecord r = golden_ipm_row(2);
    EXPECT_EQ(iter_line(r),
              "{\"v\":1,\"ev\":\"iter\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"ipm\",\"iter\":3,"
              "\"recovering\":true,\"tier\":\"rest\",\"f\":null,\"f_recovery\":0.45000000000000001,"
              "\"stationarity\":0.20000000000000001,\"feasibility\":0.90000000000000002,"
              "\"complementarity\":0.050000000000000003,\"step\":0.29999999999999999,\"accepted\":"
              "false,\"soc\":false,\"watchdog\":false,\"rolled_back\":true,\"factorizations\":11,"
              "\"stationarity_caller\":0.20000000000000001,\"feasibility_caller\":0."
              "90000000000000002,\"complementarity_caller\":0.050000000000000003,"
              "\"ipm\":{\"phase\":0,\"mu\":0.10000000000000001,\"alpha_primal\":1,\"alpha_dual\":1,"
              "\"alpha_scale\":1,\"line_search_trials\":0,\"regularization_primal\":0,"
              "\"regularization_primal_total\":0,\"regularization_dual\":null,\"proximal_primal\":"
              "null,\"inertia_corrections\":0,\"perturbed_pivots\":null,\"barrier_objective\":-0."
              "25,\"feasibility_eq\":0.90000000000000002,\"feasibility_ineq\":0.5,\"merit\":0."
              "10000000000000001,\"merit_penalty_1\":100,\"merit_penalty_2\":1000,\"max_multiplier_"
              "eq\":1000,\"max_multiplier_ineq\":10000,\"first_rejected_trial\":1,\"theta_first_"
              "rejected\":0.90000000000000002,\"eval_exceptions\":0}}\n");
    EXPECT_EQ(raw_field(iter_line(r), "f"), "null");
    EXPECT_EQ(raw_field(iter_line(r), "recovering"), "true");
    EXPECT_EQ(raw_field(iter_line(r), "depth"), "0");
}

TEST(JsonLinesTraceSink, TheThreeIpmRowsIsolateEveryNullSlotAndTheBoolSignatures) {
    // FALSIFIABILITY: every null-bearing key reads `null` in at least one line
    // and a number in at least one other, so no predicate can be dropped
    // without moving a line; and the four flags read distinct patterns.
    const std::string a = iter_line(golden_ipm_row(0));
    const std::string b = iter_line(golden_ipm_row(1));
    const std::string c = iter_line(golden_ipm_row(2));
    EXPECT_EQ(raw_field(a, "accepted"), "true");
    EXPECT_EQ(raw_field(b, "accepted"), "null");
    EXPECT_EQ(raw_field(c, "accepted"), "false");
    EXPECT_EQ(raw_field(a, "soc") + raw_field(b, "soc") + raw_field(c, "soc"), "falsetruefalse");
    EXPECT_EQ(raw_field(a, "watchdog") + raw_field(b, "watchdog") + raw_field(c, "watchdog"),
              "truetruefalse");
    EXPECT_EQ(raw_field(a, "rolled_back") + raw_field(b, "rolled_back") +
                  raw_field(c, "rolled_back"),
              "falsefalsetrue");
    for (const char *k :
         {"alpha_primal", "regularization_dual", "proximal_primal", "perturbed_pivots"}) {
        EXPECT_NE(raw_field(a, k), "null") << k;
        EXPECT_EQ(raw_field(b, k), "null") << k;
    }
    // The perturbed-pivot ABSENCE (an unobserved backend count) is `null` on
    // the restoration row too: it is not the projected 0.
    EXPECT_EQ(raw_field(c, "perturbed_pivots"), "null");
    EXPECT_EQ(raw_field(c, "proximal_primal"), "null");
    EXPECT_EQ(raw_field(c, "first_rejected_trial"), "1");
    EXPECT_EQ(raw_field(b, "first_rejected_trial"), "null");
}

TEST(JsonLinesTraceSink, GoldenLineSolveBeginIpmBlock) {
    SolveBeginTraceEvent e = default_ipm_begin();
    e.statistics = golden_statistics();
    e.optimality_tol = 1e-6;
    e.feasibility_tol = 2e-6;
    e.max_iterations = 200;
    IpmSolveBeginInfo &i = *e.ipm;
    i.n_reduced = 10;
    i.me = 6;
    i.mi = 7;
    i.internal_fixed_rows = 3;
    i.phases = 9;
    i.max_acceptable_iterations = 25;
    i.initial_mu = 0.001;
    i.obj_scale = 2.5;
    i.acceptable_stationarity_tol = 5e-6;
    i.acceptable_complementarity_tol = 6e-6;
    i.acceptable_equality_tol = 7e-6;
    i.acceptable_inequality_tol = 8e-6;
    i.inertia_mode = IpmInertiaMode::kProximalRegularization;
    i.restoration_mode = IpmRestorationMode::kL1Nested;
    i.kkt_dim = 23;
    i.kkt_nnz = 101;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_solve_begin(e);
    EXPECT_EQ(
        os.str(),
        "{\"v\":1,\"ev\":\"solve.begin\",\"seq\":1,\"sid\":1,\"depth\":0,\"eng\":\"ipm\","
        "\"variables\":11,\"free_variables\":1,\"lower_bounded_variables\":2,\"upper_bounded_"
        "variables\":3,\"boxed_variables\":4,\"fixed_variables\":1,\"constraints\":8,\"equality_"
        "rows\":3,\"lower_bounded_rows\":2,\"upper_bounded_rows\":1,\"range_rows\":1,\"free_rows\":"
        "1,\"absent_bounds\":5,\"optimality_tol\":9.9999999999999995e-07,\"feasibility_tol\":1."
        "9999999999999999e-06,\"max_iterations\":200,\"hven_version\":\"0.2.0\",\"ipm\":{\"n_"
        "reduced\":10,\"me\":6,\"mi\":7,\"internal_fixed_rows\":3,\"phases\":9,\"max_acceptable_"
        "iterations\":25,\"initial_mu\":0.001,\"obj_scale\":2.5,\"acceptable_stationarity_tol\":5."
        "0000000000000004e-06,\"acceptable_complementarity_tol\":6.0000000000000002e-06,"
        "\"acceptable_equality_tol\":6.9999999999999999e-06,\"acceptable_inequality_tol\":7."
        "9999999999999996e-06,\"inertia_mode\":\"proximal_regularization\",\"restoration_mode\":"
        "\"l1_nested\",\"kkt_dim\":23,\"kkt_nnz\":101}}\n");
    // The pair moves NO depth: this driver nests no driver of its own.
    EXPECT_EQ(sink.depth(), 0);
}

IpmCounters distinct_ipm_counters() {
    IpmCounters c;
    c.factorizations = 21;
    c.symbolic_analyses = 22;
    c.restoration_entries = 23;
    c.restoration_iters = 24;
    c.soc_steps = 25;
    c.nonfinite_step_rollbacks = 26;
    c.polish_ignored = 27;
    c.start_level_used = StartLevel::kHot;
    c.payload_ignored = 28;
    c.watchdog_activations = 29;
    c.recovery_depth_histogram = {30, 31, 32, 33, 34};
    c.inertia_corrections = 35;
    c.dense_solve_residual_refusals = 36;
    c.dense_factorizations = 37;
    return c;
}

TEST(JsonLinesTraceSink, GoldenLineSolveEndIpmBlock) {
    const IpmCounters c = distinct_ipm_counters();
    SolveEndTraceEvent e = default_ipm_end(c);
    e.status = hven::SolveStatus::kAcceptable;
    e.stop_reason = StopReason::kNone;
    e.message = std::string_view("converged to the acceptable tolerances");
    e.iterations = 42;
    e.returned_best_iterate = false;
    e.infeasibility_certified = false;
    e.f = 17.014017;
    e.diagnostics = DeclaredDiagnosticsTrace{1.2e-9, 2.2e-15, 2.5e-9};
    e.work = WorkCounters{51, 52, 53, 54, 55, 56};
    e.wall_s = 0.75;
    IpmSolveEndInfo &i = *e.ipm;
    i.convergence.stationarity = 1.0e-9;
    i.convergence.complementarity = 2.0e-9;
    i.convergence.equality = 3.0e-15;
    i.convergence.inequality = 4.0e-15;
    i.convergence.feasibility = 4.0e-15;
    i.total_time_s = 1.5;
    i.pre_time_s = 0.25;
    i.func_time_s = 0.125;
    i.kkt_time_s = 0.0625;
    i.print_time_s = 0.03125;
    i.solver_init_time_s = 0.015625;
    i.misc_time_s = 1.015625;
    i.phases_ran = 2;
    i.kkt_analyses_total = 3;
    i.factor_memory = 4096;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_solve_end(e);
    EXPECT_EQ(
        os.str(),
        "{\"v\":1,\"ev\":\"solve.end\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"ipm\",\"status\":"
        "\"acceptable\",\"stop_reason\":\"none\",\"message\":\"converged to the acceptable "
        "tolerances\",\"iterations\":42,\"returned_best_iterate\":false,\"infeasibility_"
        "certified\":false,\"f\":17.014016999999999,\"diagnostics\":{\"stationarity\":1.2e-09,"
        "\"feasibility\":2.1999999999999999e-15,\"complementarity\":2.5000000000000001e-09},"
        "\"convergence\":{\"stationarity\":1.0000000000000001e-09,\"complementarity\":2."
        "0000000000000001e-09,\"equality\":2.9999999999999998e-15,\"inequality\":4."
        "0000000000000003e-15,\"feasibility\":4.0000000000000003e-15},\"work\":{\"objective_"
        "evals\":51,\"gradient_evals\":52,\"constraint_evals\":53,\"jacobian_evals\":54,\"hessian_"
        "evals\":55,\"factorizations\":56},\"counters\":{\"factorizations\":21,\"symbolic_"
        "analyses\":22,\"restoration_entries\":23,\"restoration_iters\":24,\"soc_steps\":25,"
        "\"nonfinite_step_rollbacks\":26,\"polish_ignored\":27,\"start_level_used\":\"hot\","
        "\"payload_ignored\":28,\"watchdog_activations\":29,\"recovery_depth_histogram\":[30,31,32,"
        "33,34],\"inertia_corrections\":35,\"dense_solve_residual_refusals\":36,\"dense_"
        "factorizations\":37},\"wall_s\":0.75,\"ipm\":{\"total_time_s\":1.5,\"pre_time_s\":0.25,"
        "\"func_time_s\":0.125,\"kkt_time_s\":0.0625,\"print_time_s\":0.03125,\"solver_init_time_"
        "s\":0.015625,\"misc_time_s\":1.015625,\"phases_ran\":2,\"kkt_analyses_total\":3,\"factor_"
        "memory\":4096,\"factor_flops\":null}}\n");
    // `factor_flops` was never analysed: `null`, and `factor_memory` is not.
    EXPECT_EQ(raw_field(os.str(), "factor_flops"), "null");
    EXPECT_EQ(raw_field(os.str(), "factor_memory"), "4096");
    EXPECT_EQ(sink.depth(), 0);
}

TEST(JsonLinesTraceSink, GoldenLineSolveEndDepthOneWritesNullStopReasonMessageDiagnosticsAndWork) {
    // The shape a restoration sub-solve's end has: no declared result, so the
    // four declared fields are `null`, never an empty string or a zero.
    const SqpCounters c;
    SolveEndTraceEvent e = default_sqp_end(c);
    e.status = SolveStatus::kOptimal;
    e.iterations = 3;
    e.f = 0.0;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_solve_begin(default_sqp_begin());
    sink.on_solve_begin(default_sqp_begin());
    sink.on_solve_end(e);
    const std::vector<std::string> lines = split_lines(os.str());
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(raw_field(lines[2], "depth"), "1");
    EXPECT_EQ(raw_field(lines[2], "sid"), "1");
    for (const char *k : {"stop_reason", "message", "diagnostics", "work"}) {
        EXPECT_EQ(raw_field(lines[2], k), "null") << k;
    }
    EXPECT_EQ(raw_field(lines[2], "iterations"), "3");
    EXPECT_EQ(sink.depth(), 0) << "the end of the nested solve pops the depth";
}

TEST(JsonLinesTraceSink, SidIncrementsAtEachDepthZeroBeginAndIsSharedByTheLinesInsideTheSolve) {
    // `sid` is sink-owned like `seq`: the per-sink ordinal of the enclosing
    // depth-0 `solve.begin`, from 1. A nested begin takes no new sid; a line
    // written between solves reads the last one, and a hand-filled event with no
    // begin reads 0.
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    const SqpCounters counters;
    sink.on_ipqp_certify(IpqpCertifyTraceEvent{}); // no solve yet: sid 0
    sink.on_solve_begin(default_sqp_begin());      // sid 1
    sink.on_ipqp_certify(IpqpCertifyTraceEvent{});
    sink.on_solve_begin(default_sqp_begin()); // nested, still sid 1
    sink.on_solve_end(default_sqp_end(counters));
    sink.on_iter(IterTraceEvent{default_sqp_record()});
    sink.on_solve_end(default_sqp_end(counters));
    sink.on_solve_begin(default_ipm_begin()); // sid 2, an interior-point solve on the same sink
    sink.on_message(MessageTraceEvent{});
    const IpmCounters ipm_counters;
    sink.on_solve_end(default_ipm_end(ipm_counters));
    const std::vector<std::string> lines = split_lines(os.str());
    const char *const kSids[] = {"0", "1", "1", "1", "1", "1", "1", "2", "2", "2"};
    ASSERT_EQ(lines.size(), std::size(kSids));
    for (std::size_t i = 0; i < lines.size(); ++i) {
        EXPECT_EQ(raw_field(lines[i], "sid"), kSids[i]) << i << ": " << lines[i];
    }
    EXPECT_EQ(raw_field(lines[7], "eng"), "\"ipm\"");
    EXPECT_EQ(raw_field(lines[1], "eng"), "\"sqp\"");
}

TEST(JsonLinesTraceSink, EveryLineCarriesTheEnvelopeInItsOrder) {
    // `{"v":1,"ev":..,"seq":..,"sid":..,"depth":..,"eng":..` leads every line, in
    // that order, whichever record wrote it.
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    const SqpCounters counters;
    const IpmCounters ipm_counters;
    sink.on_solve_begin(default_sqp_begin());
    sink.on_iter(IterTraceEvent{default_sqp_record()});
    sink.on_message(MessageTraceEvent{});
    sink.on_qp_mode(QpModeTraceEvent{});
    sink.on_solve_end(default_sqp_end(counters));
    sink.on_solve_begin(default_ipm_begin());
    sink.on_iter(IterTraceEvent{default_ipm_record()});
    sink.on_ipm_kkt_analysis(IpmKktAnalysisTraceEvent{});
    sink.on_solve_end(default_ipm_end(ipm_counters));
    const std::vector<std::string> lines = split_lines(os.str());
    ASSERT_EQ(lines.size(), 9u);
    for (const std::string &l : lines) {
        const std::size_t v = l.find("{\"v\":1,\"ev\":\"");
        const std::size_t seq = l.find(",\"seq\":");
        const std::size_t sid = l.find(",\"sid\":");
        const std::size_t depth = l.find(",\"depth\":");
        const std::size_t eng = l.find(",\"eng\":\"");
        EXPECT_EQ(v, 0u) << l;
        EXPECT_LT(seq, sid) << l;
        EXPECT_LT(sid, depth) << l;
        EXPECT_LT(depth, eng) << l;
    }
    // The engine key: QP-tier lines say `sqp`, `ipm.*` lines say `ipm`.
    EXPECT_EQ(raw_field(lines[3], "eng"), "\"sqp\"");
    EXPECT_EQ(raw_field(lines[7], "eng"), "\"ipm\"");
}

// ===========================================================================
// M6 W5 T8.7b -- THE FIVE ADDED GOLDEN LINES.
//
// FIVE ADDED, NOTHING MOVED. No existing event struct gained or lost a field
// this round, so no golden line that predates this task changes by one byte;
// what is new is five records the interior-point engine did not previously
// write at all, because the facts they carry went to stdout.
//
// Each is built from a hand-filled struct with a DISTINCT value per field, so a
// reordered key or a dropped one moves these bytes.
// ===========================================================================

TEST(JsonLinesTraceSink, GoldenLineIpmPhaseBegin) {
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    // The event still carries the engine's label, and the line does not: the
    // label is derivable from `entry`, and v1 dropped the key.
    sink.on_ipm_phase_begin(IpmPhaseTraceEvent{3, "Solve Algorithm ", IpmPhase::kSolve});
    EXPECT_EQ(os.str(), "{\"v\":1,\"ev\":\"ipm.phase.begin\",\"seq\":1,\"sid\":0,\"depth\":0,"
                        "\"eng\":\"ipm\",\"phase\":3,\"entry\":\"solve\"}\n");
    EXPECT_EQ(raw_field(os.str(), "label"), "") << "no `label` key";
    // Like every other `ipm.*` line, this moves no depth.
    EXPECT_EQ(sink.depth(), 0);
}

TEST(JsonLinesTraceSink, GoldenLineIpmPhaseEnd) {
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipm_phase_end(IpmPhaseTraceEvent{0, "Optimization Algorithm ", IpmPhase::kOptimize});
    EXPECT_EQ(os.str(), "{\"v\":1,\"ev\":\"ipm.phase.end\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":"
                        "\"ipm\",\"phase\":0,\"entry\":\"optimize\"}\n");
    EXPECT_EQ(raw_field(os.str(), "label"), "") << "no `label` key";
    EXPECT_EQ(sink.depth(), 0);
}

TEST(JsonLinesTraceSink, GoldenLineIpmKktAnalysis) {
    IpmKktAnalysisTraceEvent e;
    e.kkt_dim = 23;
    e.nnz = 101;
    e.factor_mem = 31;
    e.factor_flops = 7;
    e.docompute = true;
    e.analysis_time_s = 0.00125;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipm_kkt_analysis(e);
    EXPECT_EQ(os.str(), "{\"v\":1,\"ev\":\"ipm.kkt_analysis\",\"seq\":1,\"sid\":0,\"depth\":0,"
                        "\"eng\":\"ipm\",\"kkt_dim\":23,"
                        "\"nnz\":101,\"docompute\":true,\"factor_mem\":31,\"factor_flops\":7,"
                        "\"analysis_time_s\":0.00125}\n");
    EXPECT_EQ(sink.depth(), 0);
}

TEST(JsonLinesTraceSink, TheKktAnalysisFactorFiguresAreNullOnARefactorization) {
    // THE ONE ABSENCE ON THIS RECORD, and it is not a byte literal because the
    // line above already pins the wire order: on `docompute == false` the two
    // factor figures are the LAST analysis's, re-read from the result, so
    // repeating them would report one analysis's size for another's.
    IpmKktAnalysisTraceEvent e;
    e.kkt_dim = 23;
    e.nnz = 101;
    e.factor_mem = 31;
    e.factor_flops = 7;
    e.docompute = false;
    e.analysis_time_s = 0.00125;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipm_kkt_analysis(e);
    EXPECT_EQ(raw_field(os.str(), "docompute"), "false");
    EXPECT_EQ(raw_field(os.str(), "factor_mem"), "null");
    EXPECT_EQ(raw_field(os.str(), "factor_flops"), "null");
    // The join keys and the time are NOT absent: they describe this call.
    EXPECT_EQ(raw_field(os.str(), "kkt_dim"), "23");
    EXPECT_EQ(raw_field(os.str(), "nnz"), "101");
    EXPECT_EQ(raw_field(os.str(), "analysis_time_s"), "0.00125");
}

TEST(JsonLinesTraceSink, GoldenLineIpmPhaseExit) {
    // THE ROW IS BORROWED and only the five values the console block prints are
    // written; the selected row's own `ipm.iter` line carries the record's
    // other keys and is joined by (`phase`, `iter`).
    //
    // NO `selected_iter` (M6 W5 T8.7b fix1, the lane's M4): the phase loop
    // stamps `iter_` with the loop counter and pushes one row per iteration, so
    // the row's INDEX in the history is its `iter_` and the key was the same
    // number twice. Dropped before the record froze; this literal is the one
    // line this fix round moves, and it moves by exactly that removal.
    // THE REPORT IS EMBEDDED and flattened as the trailing keys, so a reader
    // sees the returned `IpmPhaseReport` on the same line as the exit itself.
    IpmIterate row;
    row.iter = 11;
    row.prim_obj = 17.25;
    row.kkt_inf = 0.00048828125;
    row.barr_inf = 0.000244140625;
    row.econ_inf = 0.0001220703125;
    row.icon_inf = 6.103515625e-05;
    IpmPhaseReport report;
    report.phase = IpmPhase::kOptimize;
    report.status = SolveStatus::kAcceptable;
    report.iterations = 12;
    report.phase_seconds = 0.25;
    report.stop_reason = StopReason::kStageStalled;
    report.ran = true;
    IpmPhaseExitTraceEvent e{report, row};
    e.phase = 1;
    e.best_substituted = true;
    e.kkt_factor_status = IpmKktFactorStatus::kNumericalIssue;
    e.total_s = 0.5;
    e.func_s = 0.125;
    e.kkt_s = 0.0625;
    e.print_s = 0.03125;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipm_phase_exit(e);
    EXPECT_EQ(os.str(),
              "{\"v\":1,\"ev\":\"ipm.phase.exit\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"ipm\","
              "\"phase\":1,\"iter\":11,"
              "\"best_substituted\":true,\"prim_obj\":17.25,"
              "\"stationarity\":0.00048828125,\"complementarity\":0.000244140625,"
              "\"econ_inf\":0.0001220703125,\"icon_inf\":6.103515625e-05,"
              "\"kkt_factor_status\":\"numerical_issue\",\"total_s\":0.5,\"func_s\":0.125,"
              "\"kkt_s\":0.0625,\"print_s\":0.03125,\"entry\":\"optimize\","
              "\"status\":\"acceptable\",\"iterations\":12,\"phase_seconds\":0.25,"
              "\"stop_reason\":\"stage_stalled\",\"ran\":true}\n");
    EXPECT_EQ(sink.depth(), 0);
}

struct MessageGolden {
    const char *name;
    MessageTraceEvent event;
};

MessageTraceEvent make_message(TraceEngine engine, MessageKind kind) {
    MessageTraceEvent m;
    m.engine = engine;
    m.kind = kind;
    m.severity = default_message_severity(kind);
    return m;
}

/// One event per `MessageKind`, each filled in the slots its template reads (the
/// console's `message_text` is the table of which), plus the variants that change
/// a severity or an engine.
std::vector<MessageGolden> message_goldens() {
    std::vector<MessageGolden> out;
    const auto add = [&out](const char *name, MessageTraceEvent m) {
        out.push_back(MessageGolden{name, m});
    };
    {
        MessageTraceEvent m = make_message(TraceEngine::kIpm, MessageKind::kSolverInitialized);
        m.a = 627.5; // wall-clock: the wire carries `null`
        add("solver_initialized", m);
    }
    {
        MessageTraceEvent m = make_message(TraceEngine::kIpm, MessageKind::kRankDeficiency);
        m.phase = 1;
        m.iter = 7;
        add("rank_deficiency", m);
    }
    {
        MessageTraceEvent m = make_message(TraceEngine::kIpm, MessageKind::kFactorizationHardError);
        m.phase = 0;
        m.iter = 3;
        m.k = -4;
        add("factorization_hard_error", m);
    }
    {
        MessageTraceEvent m = make_message(TraceEngine::kIpm, MessageKind::kInertiaExhausted);
        m.phase = 0;
        m.iter = 4;
        m.k = 15;
        m.p = 3;
        m.n = 2;
        m.z = 1;
        m.expected_p = 5;
        m.expected_n = 2;
        add("inertia_exhausted", m);
    }
    {
        MessageTraceEvent m = make_message(TraceEngine::kIpm, MessageKind::kFeasibilityStall);
        m.phase = 1;
        m.iter = 12;
        m.a = 0.0015;
        m.b = 1.0e-6;
        add("feasibility_stall", m);
    }
    {
        MessageTraceEvent m = make_message(TraceEngine::kIpm, MessageKind::kPhaseDiverged);
        m.phase = 2;
        add("phase_diverged", m);
    }
    add("interrupt_skipping_phases",
        make_message(TraceEngine::kIpm, MessageKind::kInterruptSkippingPhases));
    add("stop_requested_skipping_phases",
        make_message(TraceEngine::kIpm, MessageKind::kStopRequestedSkippingPhases));
    {
        MessageTraceEvent m = make_message(TraceEngine::kIpm, MessageKind::kRestorationEntered);
        m.phase = 0;
        m.iter = 3;
        m.a = 0.9;
        add("restoration_entered_ipm", m);
        m.engine = TraceEngine::kSqp;
        m.phase.reset();
        m.iter = 4;
        m.a = 1.4;
        add("restoration_entered_sqp", m);
    }
    {
        // The locally-infeasible door: a warning, with the threshold judged against.
        MessageTraceEvent m = make_message(TraceEngine::kIpm, MessageKind::kRestorationExited);
        m.severity = MessageSeverity::kWarning;
        m.phase = 0;
        m.iter = 7;
        m.s = "locally_infeasible";
        m.a = 1.5e-3;
        m.b = 1.0e-6;
        add("restoration_exited_locally_infeasible", m);
        // The ordinary return: a notice.
        m.engine = TraceEngine::kSqp;
        m.severity = MessageSeverity::kNotice;
        m.phase.reset();
        m.iter = 4;
        m.s = "resumed";
        m.a = 1e-12;
        m.b = 1e-6;
        add("restoration_exited_resumed", m);
    }
    {
        MessageTraceEvent m = make_message(TraceEngine::kSqp, MessageKind::kRestorationRefused);
        m.s = "nested";
        add("restoration_refused", m);
    }
    {
        MessageTraceEvent m = make_message(TraceEngine::kSqp, MessageKind::kSubproblemFailure);
        m.s = "iteration_limit";
        m.k = 2;
        add("subproblem_failure", m);
    }
    {
        MessageTraceEvent m = make_message(TraceEngine::kSqp, MessageKind::kElasticExhausted);
        m.k = 6;
        add("elastic_exhausted", m);
    }
    add("inconsistent_equalities",
        make_message(TraceEngine::kSqp, MessageKind::kInconsistentEqualities));
    {
        MessageTraceEvent m = make_message(TraceEngine::kSqp, MessageKind::kStartPointClamped);
        m.k = 3;
        add("start_point_clamped", m);
    }
    {
        MessageTraceEvent m =
            make_message(TraceEngine::kSqp, MessageKind::kScaledConvergenceRefused);
        m.iter = 9;
        add("scaled_convergence_refused", m);
    }
    {
        MessageTraceEvent m = make_message(TraceEngine::kIpm, MessageKind::kStopAtIteration);
        m.iter = 5;
        m.s = "callback";
        add("stop_at_iteration_callback_ipm", m);
        m.engine = TraceEngine::kSqp;
        m.s = "token";
        add("stop_at_iteration_token_sqp", m);
    }
    return out;
}

const std::map<std::string, std::string> kMessageGoldenLines = {
    {"solver_initialized",
     "{\"v\":1,\"ev\":\"message\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"ipm\",\"kind\":"
     "\"solver_initialized\",\"severity\":\"notice\",\"phase\":null,\"iter\":null,\"a\":null,\"b\":"
     "null,\"k\":null,\"s\":null,\"p\":null,\"n\":null,\"z\":null,\"expected_p\":null,\"expected_"
     "n\":null,\"ladder\":null}\n"},
    {"rank_deficiency",
     "{\"v\":1,\"ev\":\"message\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"ipm\",\"kind\":\"rank_"
     "deficiency\",\"severity\":\"warning\",\"phase\":1,\"iter\":7,\"a\":null,\"b\":null,\"k\":"
     "null,\"s\":null,\"p\":null,\"n\":null,\"z\":null,\"expected_p\":null,\"expected_n\":null,"
     "\"ladder\":null}\n"},
    {"factorization_hard_error",
     "{\"v\":1,\"ev\":\"message\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"ipm\",\"kind\":"
     "\"factorization_hard_error\",\"severity\":\"warning\",\"phase\":0,\"iter\":3,\"a\":null,"
     "\"b\":null,\"k\":-4,\"s\":null,\"p\":null,\"n\":null,\"z\":null,\"expected_p\":null,"
     "\"expected_n\":null,\"ladder\":null}\n"},
    {"inertia_exhausted",
     "{\"v\":1,\"ev\":\"message\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"ipm\",\"kind\":"
     "\"inertia_exhausted\",\"severity\":\"warning\",\"phase\":0,\"iter\":4,\"a\":null,\"b\":null,"
     "\"k\":15,\"s\":null,\"p\":3,\"n\":2,\"z\":1,\"expected_p\":5,\"expected_n\":2,\"ladder\":"
     "null}\n"},
    {"feasibility_stall",
     "{\"v\":1,\"ev\":\"message\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"ipm\",\"kind\":"
     "\"feasibility_stall\",\"severity\":\"warning\",\"phase\":1,\"iter\":12,\"a\":0.0015,\"b\":9."
     "9999999999999995e-07,\"k\":null,\"s\":null,\"p\":null,\"n\":null,\"z\":null,\"expected_p\":"
     "null,\"expected_n\":null,\"ladder\":null}\n"},
    {"phase_diverged",
     "{\"v\":1,\"ev\":\"message\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"ipm\",\"kind\":\"phase_"
     "diverged\",\"severity\":\"warning\",\"phase\":2,\"iter\":null,\"a\":null,\"b\":null,\"k\":"
     "null,\"s\":null,\"p\":null,\"n\":null,\"z\":null,\"expected_p\":null,\"expected_n\":null,"
     "\"ladder\":null}\n"},
    {"interrupt_skipping_phases",
     "{\"v\":1,\"ev\":\"message\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"ipm\",\"kind\":"
     "\"interrupt_skipping_phases\",\"severity\":\"notice\",\"phase\":null,\"iter\":null,\"a\":"
     "null,\"b\":null,\"k\":null,\"s\":null,\"p\":null,\"n\":null,\"z\":null,\"expected_p\":null,"
     "\"expected_n\":null,\"ladder\":null}\n"},
    {"stop_requested_skipping_phases",
     "{\"v\":1,\"ev\":\"message\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"ipm\",\"kind\":\"stop_"
     "requested_skipping_phases\",\"severity\":\"notice\",\"phase\":null,\"iter\":null,\"a\":null,"
     "\"b\":null,\"k\":null,\"s\":null,\"p\":null,\"n\":null,\"z\":null,\"expected_p\":null,"
     "\"expected_n\":null,\"ladder\":null}\n"},
    {"restoration_entered_ipm",
     "{\"v\":1,\"ev\":\"message\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"ipm\",\"kind\":"
     "\"restoration_entered\",\"severity\":\"notice\",\"phase\":0,\"iter\":3,\"a\":0."
     "90000000000000002,\"b\":null,\"k\":null,\"s\":null,\"p\":null,\"n\":null,\"z\":null,"
     "\"expected_p\":null,\"expected_n\":null,\"ladder\":null}\n"},
    {"restoration_entered_sqp",
     "{\"v\":1,\"ev\":\"message\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\",\"kind\":"
     "\"restoration_entered\",\"severity\":\"notice\",\"phase\":null,\"iter\":4,\"a\":1."
     "3999999999999999,\"b\":null,\"k\":null,\"s\":null,\"p\":null,\"n\":null,\"z\":null,"
     "\"expected_p\":null,\"expected_n\":null,\"ladder\":null}\n"},
    {"restoration_exited_locally_infeasible",
     "{\"v\":1,\"ev\":\"message\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"ipm\",\"kind\":"
     "\"restoration_exited\",\"severity\":\"warning\",\"phase\":0,\"iter\":7,\"a\":0.0015,\"b\":9."
     "9999999999999995e-07,\"k\":null,\"s\":\"locally_infeasible\",\"p\":null,\"n\":null,\"z\":"
     "null,\"expected_p\":null,\"expected_n\":null,\"ladder\":null}\n"},
    {"restoration_exited_resumed",
     "{\"v\":1,\"ev\":\"message\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\",\"kind\":"
     "\"restoration_exited\",\"severity\":\"notice\",\"phase\":null,\"iter\":4,\"a\":9."
     "9999999999999998e-13,\"b\":9.9999999999999995e-07,\"k\":null,\"s\":\"resumed\",\"p\":null,"
     "\"n\":null,\"z\":null,\"expected_p\":null,\"expected_n\":null,\"ladder\":null}\n"},
    {"restoration_refused",
     "{\"v\":1,\"ev\":\"message\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\",\"kind\":"
     "\"restoration_refused\",\"severity\":\"warning\",\"phase\":null,\"iter\":null,\"a\":null,"
     "\"b\":null,\"k\":null,\"s\":\"nested\",\"p\":null,\"n\":null,\"z\":null,\"expected_p\":null,"
     "\"expected_n\":null,\"ladder\":null}\n"},
    {"subproblem_failure",
     "{\"v\":1,\"ev\":\"message\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\",\"kind\":"
     "\"subproblem_failure\",\"severity\":\"warning\",\"phase\":null,\"iter\":null,\"a\":null,"
     "\"b\":null,\"k\":2,\"s\":\"iteration_limit\",\"p\":null,\"n\":null,\"z\":null,\"expected_p\":"
     "null,\"expected_n\":null,\"ladder\":null}\n"},
    {"elastic_exhausted",
     "{\"v\":1,\"ev\":\"message\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\",\"kind\":"
     "\"elastic_exhausted\",\"severity\":\"warning\",\"phase\":null,\"iter\":null,\"a\":null,\"b\":"
     "null,\"k\":6,\"s\":null,\"p\":null,\"n\":null,\"z\":null,\"expected_p\":null,\"expected_n\":"
     "null,\"ladder\":null}\n"},
    {"inconsistent_equalities",
     "{\"v\":1,\"ev\":\"message\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\",\"kind\":"
     "\"inconsistent_equalities\",\"severity\":\"warning\",\"phase\":null,\"iter\":null,\"a\":null,"
     "\"b\":null,\"k\":null,\"s\":null,\"p\":null,\"n\":null,\"z\":null,\"expected_p\":null,"
     "\"expected_n\":null,\"ladder\":null}\n"},
    {"start_point_clamped",
     "{\"v\":1,\"ev\":\"message\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\",\"kind\":\"start_"
     "point_clamped\",\"severity\":\"notice\",\"phase\":null,\"iter\":null,\"a\":null,\"b\":null,"
     "\"k\":3,\"s\":null,\"p\":null,\"n\":null,\"z\":null,\"expected_p\":null,\"expected_n\":null,"
     "\"ladder\":null}"
     "\n"},
    {"scaled_convergence_refused",
     "{\"v\":1,\"ev\":\"message\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\",\"kind\":"
     "\"scaled_convergence_refused\",\"severity\":\"notice\",\"phase\":null,\"iter\":9,\"a\":null,"
     "\"b\":null,\"k\":null,\"s\":null,\"p\":null,\"n\":null,\"z\":null,\"expected_p\":null,"
     "\"expected_n\":null,\"ladder\":null}\n"},
    {"stop_at_iteration_callback_ipm",
     "{\"v\":1,\"ev\":\"message\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"ipm\",\"kind\":\"stop_"
     "at_iteration\",\"severity\":\"notice\",\"phase\":null,\"iter\":5,\"a\":null,\"b\":null,\"k\":"
     "null,\"s\":\"callback\",\"p\":null,\"n\":null,\"z\":null,\"expected_p\":null,\"expected_n\":"
     "null,\"ladder\":null}\n"},
    {"stop_at_iteration_token_sqp",
     "{\"v\":1,\"ev\":\"message\",\"seq\":1,\"sid\":0,\"depth\":0,\"eng\":\"sqp\",\"kind\":\"stop_"
     "at_iteration\",\"severity\":\"notice\",\"phase\":null,\"iter\":5,\"a\":null,\"b\":null,\"k\":"
     "null,\"s\":\"token\",\"p\":null,\"n\":null,\"z\":null,\"expected_p\":null,\"expected_n\":"
     "null,\"ladder\":null}\n"},
};

TEST(JsonLinesTraceSink, GoldenLineMessageOnePerKind) {
    // A golden per MessageKind. The slots a kind does not use are `null`, which
    // the lines show rather than describe; `severity` is the kind's default
    // except on the locally-infeasible door, which is a warning.
    const std::vector<MessageGolden> goldens = message_goldens();
    std::set<std::string> kinds;
    for (const MessageGolden &g : goldens) {
        SCOPED_TRACE(g.name);
        std::ostringstream os;
        JsonLinesTraceSink sink(os);
        sink.on_message(g.event);
        const std::string line = os.str();
        EXPECT_EQ(line, kMessageGoldenLines.at(g.name));
        kinds.insert(raw_field(line, "kind"));
        EXPECT_EQ(raw_field(line, "eng"),
                  g.event.engine == TraceEngine::kIpm ? "\"ipm\"" : "\"sqp\"");
        EXPECT_EQ(sink.depth(), 0);
    }
    EXPECT_EQ(kinds.size(), 17u) << "every MessageKind has a golden";
}

TEST(JsonLinesTraceSink, TheSolverInitializedMessageCarriesNoMillisecondsOnTheWire) {
    // `a` is wall-clock, so a deterministic solve's stream would stop being
    // byte-reproducible if it were written; the console prints it from the event.
    MessageTraceEvent m = make_message(TraceEngine::kIpm, MessageKind::kSolverInitialized);
    m.a = 627.5;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_message(m);
    EXPECT_EQ(raw_field(os.str(), "a"), "null");
    EXPECT_EQ(raw_field(os.str(), "severity"), "\"notice\"");
}

TEST(JsonLinesTraceSink, TheElasticExhaustionMessageCarriesTheRowsLadderBlockAndNullElsewhere) {
    MessageTraceEvent m = make_message(TraceEngine::kSqp, MessageKind::kElasticExhausted);
    m.iter = 3;
    m.k = 5;
    ElasticLadderRow l;
    l.entry = ElasticEntry::kVerifiedInfeasible;
    l.main_status = QpStatus::kInfeasible;
    l.rung_status = QpStatus::kNumericalError;
    l.rho = 1e8;
    l.rungs = 5;
    l.promises_f = true;
    l.slack_l1 = 0.25;
    l.exhausted = true;
    l.rho_0 = 100.0;
    l.minor_iterations = 37;
    l.factorizations = 11;
    m.ladder = l;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_message(m);
    const std::string line = os.str();
    EXPECT_NE(line.find("\"expected_n\":null,\"ladder\":{\"entry\":\"verified_infeasible\","
                        "\"main_status\":\"infeasible\",\"rung_status\":\"numerical_error\","
                        "\"rho\":100000000,\"rungs\":5,\"closed\":false,\"reduced\":false,"
                        "\"promises_f\":true,\"slack_l1\":0.25,\"exhausted\":true,"
                        "\"rho_0\":100,\"minor_iterations\":37,\"factorizations\":11}"),
              std::string::npos)
        << line;
    EXPECT_EQ(raw_field(line, "k"), "5");
    EXPECT_EQ(raw_field(line, "a"), "null");
    // Every other kind writes the key as null.
    std::ostringstream os2;
    JsonLinesTraceSink sink2(os2);
    sink2.on_message(make_message(TraceEngine::kSqp, MessageKind::kInconsistentEqualities));
    EXPECT_EQ(raw_field(os2.str(), "ladder"), "null") << os2.str();
}

TEST(JsonLinesTraceSink, ARowsLadderBlockIsAnObjectAndNullWithoutOne) {
    IterationRecord r;
    r.iteration = 2;
    r.sqp.emplace();
    r.sqp->qp_solved = true;
    ElasticLadderRow l;
    l.entry = ElasticEntry::kRefusedCertificate;
    l.main_status = QpStatus::kNumericalError;
    l.rung_status = QpStatus::kOptimal;
    l.rho = 100.0;
    l.rungs = 4;
    l.closed = true;
    l.reduced = true;
    l.promises_f = false;
    l.slack_l1 = 0.0;
    l.exhausted = false;
    l.rho_0 = 1.0;
    l.minor_iterations = 9;
    l.factorizations = 4;
    r.sqp->ladder = l;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_iter(IterTraceEvent{r});
    const std::string line = os.str();
    EXPECT_NE(
        line.find("\"ladder\":{\"entry\":\"refused_certificate\",\"main_status\":\"numerical_"
                  "error\",\"rung_status\":\"optimal\",\"rho\":100,\"rungs\":4,\"closed\":true,"
                  "\"reduced\":true,\"promises_f\":false,\"slack_l1\":0,\"exhausted\":false,"
                  "\"rho_0\":1,\"minor_iterations\":9,\"factorizations\":4}"),
        std::string::npos)
        << line;
}

TEST(JsonLinesTraceSink, TheInertiaMessageFillsEverySlotTheOtherKindsLeaveNull) {
    // THE OTHER HALF OF THE MESSAGE PAYLOAD, by FIELD rather than by a byte
    // literal: the wire order is pinned above, and what this adds is that the one
    // kind the wide payload exists for really does fill all six integers -- and
    // that the two double slots it does not use read `null`.
    MessageTraceEvent m = make_message(TraceEngine::kIpm, MessageKind::kInertiaExhausted);
    m.phase = 0;
    m.iter = 4;
    m.k = 15;
    m.p = 3;
    m.n = 2;
    m.z = 1;
    m.expected_p = 5;
    m.expected_n = 2;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_message(m);
    const std::string line = os.str();
    EXPECT_EQ(raw_field(line, "kind"), "\"inertia_exhausted\"");
    EXPECT_EQ(raw_field(line, "severity"), "\"warning\"");
    EXPECT_EQ(raw_field(line, "a"), "null");
    EXPECT_EQ(raw_field(line, "b"), "null");
    EXPECT_EQ(raw_field(line, "s"), "null");
    EXPECT_EQ(raw_field(line, "k"), "15");
    EXPECT_EQ(raw_field(line, "p"), "3");
    EXPECT_EQ(raw_field(line, "n"), "2");
    EXPECT_EQ(raw_field(line, "z"), "1");
    EXPECT_EQ(raw_field(line, "expected_p"), "5");
    EXPECT_EQ(raw_field(line, "expected_n"), "2");

    // And the kind with NO payload at all reports every slot absent, phase and
    // iteration included -- `inconsistent_equalities` names nothing but itself.
    std::ostringstream os2;
    JsonLinesTraceSink sink2(os2);
    sink2.on_message(make_message(TraceEngine::kSqp, MessageKind::kInconsistentEqualities));
    for (const char *k : {"phase", "iter", "a", "b", "k", "s", "p", "n", "z", "expected_p",
                          "expected_n", "ladder"}) {
        EXPECT_EQ(raw_field(os2.str(), k), "null") << k;
    }
}

TEST(JsonLinesTraceSink, AnInteriorPointSolveBetweenSqpPairsMovesNoDepth) {
    // The two engines can share one sink. A depth-0 interior-point solve opens
    // no nesting: the depth is the SQP's restoration recursion's, and the
    // interior-point engine's restoration runs in place.
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    const SqpCounters counters;
    const IpmCounters ipm_counters;
    sink.on_solve_begin(default_ipm_begin());
    sink.on_iter(IterTraceEvent{golden_ipm_row(0)});
    sink.on_solve_end(default_ipm_end(ipm_counters));
    EXPECT_EQ(sink.depth(), 0);
    sink.on_solve_begin(default_sqp_begin());
    sink.on_solve_end(default_sqp_end(counters));
    EXPECT_EQ(sink.depth(), 0);
    for (const std::string &l : split_lines(os.str())) {
        EXPECT_EQ(raw_field(l, "depth"), "0") << l;
    }
    EXPECT_EQ(sink.lines_written(), 5);
}

TEST(JsonLinesTraceSink, NoInteriorPointLineCarriesAnUnknownEnumString) {
    // The production `to_json` has no `default` label and this tree has no
    // `-Werror`, so `"unknown"` is reachable from a library-only build. Here it
    // never is.
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    const IpmCounters ipm_counters;
    sink.on_solve_begin(default_ipm_begin());
    for (int line = 0; line < 3; ++line) {
        sink.on_iter(IterTraceEvent{golden_ipm_row(line)});
    }
    // Every alphabet on the interior-point records exercised at BOTH ends of its
    // range so a missing `to_json` case cannot hide behind a default value.
    const IpmIterate row;
    IpmPhaseReport report;
    sink.on_ipm_phase_begin(IpmPhaseTraceEvent{0, "Optimization Algorithm ", IpmPhase::kOptimize});
    sink.on_ipm_kkt_analysis(IpmKktAnalysisTraceEvent{});
    for (const StopReason reason :
         {StopReason::kNone, StopReason::kRestorationLocallyInfeasible, StopReason::kStageStalled,
          StopReason::kCallbackStop, StopReason::kStopRequested}) {
        report.stop_reason = reason;
        for (const SolveStatus st :
             {SolveStatus::kOptimal, SolveStatus::kAcceptable, SolveStatus::kIterationLimit,
              SolveStatus::kInfeasible, SolveStatus::kStalled, SolveStatus::kDiverging,
              SolveStatus::kNumericalError, SolveStatus::kInterrupted}) {
            report.status = st;
            for (const IpmKktFactorStatus fs :
                 {IpmKktFactorStatus::kSuccess, IpmKktFactorStatus::kNumericalIssue,
                  IpmKktFactorStatus::kNoConvergence, IpmKktFactorStatus::kInvalidInput}) {
                IpmPhaseExitTraceEvent e{report, row};
                e.kkt_factor_status = fs;
                sink.on_ipm_phase_exit(e);
            }
        }
    }
    for (const MessageGolden &g : message_goldens()) {
        sink.on_message(g.event);
    }
    sink.on_ipm_phase_end(IpmPhaseTraceEvent{1, "Solve Algorithm ", IpmPhase::kSolve});
    sink.on_solve_end(default_ipm_end(ipm_counters));
    EXPECT_EQ(os.str().find("unknown"), std::string::npos);
}

// ===========================================================================
// M6 W5 T8.7 -- THE SQP CONSOLE COMPOSITION.
//
// The driver now attaches a `ConsoleTraceSink` of its own when
// `common.print_level` says printing is on, and hands a FAN-OUT over the
// caller's sink and that console to every emit site, to the IPQP engine and to
// the restoration sub-driver. These are the four pins the composition owes:
// the caller's stream is unchanged, the console equals `format_iteration_table`
// on a live solve, the SQP's own default level writes nothing, and a counting
// sink sees the same events either way.
// ===========================================================================

namespace {

/// The live console pins below read the process's real `stdout`: the DRIVER
/// builds its own console and gives it `stdout`, so there is no `FILE *` for a
/// test to hand it. `hven::testing::StdoutCapture` (tests/support/) is that
/// redirection, in one portable place.
using hven::testing::StdoutCapture;

/// Counts every event, by name, without rendering anything.
struct EventTally : TraceSink {
    std::vector<std::string> seen;
    void on_ipqp_iter(const IpqpIterTraceEvent &) override { seen.push_back("ipqp.iter"); }
    void on_ipqp_reg(const IpqpRegTraceEvent &) override { seen.push_back("ipqp.reg"); }
    void on_ipqp_restart(const IpqpRestartTraceEvent &) override { seen.push_back("ipqp.restart"); }
    void on_ipqp_route(const IpqpRouteTraceEvent &) override { seen.push_back("ipqp.route"); }
    void on_ipqp_certify(const IpqpCertifyTraceEvent &) override { seen.push_back("ipqp.certify"); }
    void on_ipqp_escape(const IpqpEscapeTraceEvent &) override { seen.push_back("ipqp.escape"); }
    void on_qp_mode(const QpModeTraceEvent &) override { seen.push_back("qp.mode"); }
    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &) override {
        seen.push_back("fallback.verdict");
    }
    void on_iter(const IterTraceEvent &) override { seen.push_back("iter"); }
    void on_message(const MessageTraceEvent &) override { seen.push_back("message"); }
    void on_solve_begin(const SolveBeginTraceEvent &) override { seen.push_back("solve.begin"); }
    void on_solve_end(const SolveEndTraceEvent &) override { seen.push_back("solve.end"); }
};

} // namespace

TEST(SqpConsole, TheUsersJsonStreamIsByteIdenticalWithAndWithoutTheConsole) {
    // THE INVARIANT THE FAN-OUT EXISTS FOR, on the fixture that exercises it
    // hardest: this model ENTERS RESTORATION, so the stream carries depth-1
    // lines from the sub-driver. Handing the sub-driver the caller's half
    // instead of the fan-out -- or handing it the console -- moves those lines.
    auto run = [](hven::Verbosity print_level) {
        CircleAndFarLineModel model;
        SqpOptions opts;
        opts.common.max_iterations = 200;
        opts.common.verbosity = print_level;
        SqpSolver driver(opts);
        std::ostringstream os;
        JsonLinesTraceSink json(os);
        driver.attach_trace(&json);
        StdoutCapture capture;
        EXPECT_TRUE(capture.active());
        const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());
        // `wall_s` is wall-clock and informational: a deterministic solve's stream is
        // byte-reproducible once it is masked.
        // The SQP `iter` rows' `t_s` (I19-1) is the same kind of key.
        const std::string masked =
            std::regex_replace(os.str(), std::regex("\"(wall_s|t_s)\":[^,}]*"), "\"$1\":0");
        return std::pair<std::string, Index>{masked, sol.counters.restoration_iters};
    };
    const auto silent = run(hven::Verbosity::kSilent);
    const auto printing = run(hven::Verbosity::kIterations);
    ASSERT_GE(silent.second, 1) << "fixture premise: this cell enters restoration";
    EXPECT_EQ(silent.first, printing.first)
        << "the console displaced or perturbed the caller's stream";
    // Non-vacuous: the stream really does carry depth-1 lines.
    Index depth1 = 0;
    for (const std::string &l : split_lines(silent.first)) {
        depth1 += (raw_field(l, "depth") == "1") ? 1 : 0;
    }
    EXPECT_GT(depth1, 0);
}

/// The captured console text's lines that are iteration rows: `{iter:>5} {tier} ...`.
Index count_row_lines(const std::string &text) {
    Index rows = 0;
    for (const std::string &l : split_lines(text)) {
        if (l.size() > 8 && l[5] == ' ' && l[7] == ' ' &&
            std::string_view("IWSER").find(l[6]) != std::string_view::npos &&
            std::isdigit(static_cast<unsigned char>(l[4])) != 0) {
            ++rows;
        }
    }
    return rows;
}

Index count_lines_starting(const std::string &text, std::string_view prefix) {
    Index n = 0;
    for (const std::string &l : split_lines(text)) {
        n += (l.rfind(prefix, 0) == 0) ? 1 : 0;
    }
    return n;
}

TEST(SqpConsole, TheLiveConsoleWritesOneRowPerIterLineIncludingTheRestorationRows) {
    // THE SQP'S LIVE PIN. A solve that restores, at the rung that prints rows:
    // the console shows exactly the rows the machine trace carries, depth-1
    // `R` rows included, one header, one footer.
    CircleAndFarLineModel model;
    SqpOptions opts;
    opts.common.max_iterations = 200;
    opts.common.verbosity = hven::Verbosity::kIterations;
    SqpSolver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    std::string written;
    SqpResult sol;
    {
        StdoutCapture capture;
        EXPECT_TRUE(capture.active());
        sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());
        written = capture.text();
    }
    ASSERT_GE(sol.counters.restoration_iters, 1) << "fixture premise: restoration runs";
    Index iter_lines = 0, rest_rows = 0;
    for (const std::string &l : split_lines(os.str())) {
        if (event_name(l) == "iter") {
            ++iter_lines;
            rest_rows += (raw_field(l, "tier") == "\"rest\"") ? 1 : 0;
        }
    }
    ASSERT_GT(rest_rows, 0);
    EXPECT_EQ(count_row_lines(written), iter_lines);
    EXPECT_EQ(count_lines_starting(written, "hven "), 1) << "one header";
    EXPECT_EQ(count_lines_starting(written, "exit "), 1) << "one exit line";
    EXPECT_EQ(count_lines_starting(written, "legend "), 0);
}

TEST(SqpConsole, TheSqpDefaultPrintLevelWritesNothing) {
    // BEHAVIOUR CHANGE (11) IS A GAIN, NOT A CHANGE. `CommonOptions`'
    // shipped `print_level` is 3, and at 3 this driver writes exactly what it
    // wrote before this task: nothing.
    HsProblem p = make_hs(38);
    SqpOptions opts;
    opts.common.max_iterations = 60;
    SqpSolver driver(opts);
    std::string written;
    {
        StdoutCapture capture;
        EXPECT_TRUE(capture.active());
        const SqpResult sol =
            driver.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());
        written = capture.text();
        EXPECT_FALSE(sol.history.empty());
    }
    EXPECT_EQ(written, "");
}

TEST(SqpConsole, ACountingSinkSeesTheSameEventsWithPrintingOnAndOff) {
    auto run = [](hven::Verbosity print_level, bool attach_first) {
        HsProblem p = make_hs(38);
        SqpOptions opts;
        opts.qp_mode = QpMode::kIpqp;
        opts.common.max_iterations = 60;
        opts.common.verbosity = print_level;
        EventTally tally;
        // ATTACH ORDER IS FREE, and this is where that is proved: the sink is
        // attached before or after `set_options` and the composition happens at
        // solve entry either way.
        SqpSolver driver(attach_first ? opts : SqpOptions{});
        if (attach_first) {
            driver.attach_trace(&tally);
        } else {
            driver.attach_trace(&tally);
            driver.set_options(opts);
        }
        StdoutCapture capture;
        EXPECT_TRUE(capture.active());
        driver.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());
        return tally.seen;
    };
    const std::vector<std::string> off = run(hven::Verbosity::kSilent, true);
    const std::vector<std::string> on = run(hven::Verbosity::kIterations, true);
    const std::vector<std::string> on_late = run(hven::Verbosity::kIterations, false);
    EXPECT_FALSE(off.empty());
    EXPECT_EQ(off, on);
    EXPECT_EQ(off, on_late);
}

TEST(SqpConsole, TheRestorationSubDriverIsTheOneDriverThatNeverPrints) {
    // M6 W5 T8.7 fix1 (the lane's M2). The console composition used to be
    // guarded on `allow_restoration_` -- "may I restore" -- where what it means
    // is "am I the restoration phase's own driver". The two coincide on the
    // shipped surface (the two-argument constructor is PRIVATE and the
    // restoration phase is its only caller), so no caller could reach the wrong
    // branch; what the predicate change buys is that the console's condition
    // says which fact it depends on, and stays right if that surface widens.
    //
    // WHAT IS OBSERVABLE, AND IS PINNED HERE: on a solve that ENTERS
    // restoration, exactly ONE header is written -- the parent's. A sub-driver
    // that built a console of its own would write a second header and its own
    // rows into the same `stdout`.
    CircleAndFarLineModel model;
    SqpOptions opts;
    opts.common.max_iterations = 200;
    opts.common.verbosity = hven::Verbosity::kIterations;
    SqpSolver driver(opts);
    std::string written;
    SqpResult sol;
    {
        StdoutCapture capture;
        EXPECT_TRUE(capture.active());
        sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());
        written = capture.text();
    }
    ASSERT_GE(sol.counters.restoration_iters, 1) << "fixture premise: restoration runs";

    // The sub-driver is handed the parent's fan-out and no console of its own, so
    // the parent's console is the one writer: one header, one exit.
    EXPECT_EQ(count_lines_starting(written, "hven "), 1)
        << "the restoration sub-driver wrote a header of its own";
    EXPECT_EQ(count_lines_starting(written, "legend "), 0);
    EXPECT_EQ(count_lines_starting(written, "exit "), 1);
    // The column header precedes the first row, then every 20 printed rows and the first row
    // after a message line (TR-B: the engine now writes the restoration entry and exit): the
    // restoration rows count among the rows, and a sub-driver's own table would add a header at
    // its first row beyond that bound.
    const Index rows = count_row_lines(written);
    const Index messages =
        count_lines_starting(written, "      ! ") + count_lines_starting(written, "      - ");
    EXPECT_GE(messages, 2) << "the entry and the exit of the restoration";
    EXPECT_GE(count_lines_starting(written, " iter t"), 1 + (rows - 1) / 20);
    EXPECT_LE(count_lines_starting(written, " iter t"), 1 + (rows - 1) / 20 + messages);
}

TEST(SqpConsole, AttachTraceDuringASolveIsRefused) {
    // THE IN-FLIGHT RULE `set_options` uses, extended to the sink (M6 W5 T8.7):
    // the effective sink is fixed for the solve, so a mid-solve change could
    // not take effect in it. The refusal says so.
    HsProblem p = make_hs(38);
    SqpOptions opts;
    opts.common.max_iterations = 60;
    SqpSolver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    bool threw = false;
    driver.set_iteration_callback([&](const IterationEvent &) {
        try {
            driver.attach_trace(&json);
        } catch (const std::logic_error &) {
            threw = true;
        }
        return CallbackAction::kContinue;
    });
    driver.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());
    EXPECT_TRUE(threw) << "attach_trace inside a solve must be refused";
    // ... and it is legal again once the solve has returned.
    EXPECT_NO_THROW(driver.attach_trace(&json));
}

} // namespace
} // namespace hven
