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
#include <cstdio>
#include <cstdlib>
#include <ios>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <streambuf>
#include <string>
#include <vector>

#include <Eigen/SparseCore>
#include <gtest/gtest.h>

#include <hven/core/detail/aggregate_arity.h>
#include <hven/detail/globalization/sqp/elastic.h>
#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/drivers/console_trace_sink.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/trace_writer.h>
#include <hven/model/nlp_model.h>

// The portable stdout capture both console suites share (M6 W5 T8.7 fix1,
// astra's I2): this file used to carry its own copy, which included <unistd.h>
// and called the POSIX descriptor functions unconditionally -- source that
// cannot be compiled on Windows, where this target is also built.
#include "../common_support/console_capture.h"
#include "support/hs_problems.h"
#include "support/ipqp_test_support.h"

namespace hven::solvers {
namespace {

using test_support::HsProblem;
using test_support::make_hs;
// W5 T5: these four were replicas of test_sqp_driver.cpp's own W2 fixtures and
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
    // RE-DERIVED AT M6 W4 T5 (DECLARED; settler ruling, brief addendum 3). This
    // is the ONE W1/W2 golden line the window moves, and it moves by EXACTLY
    // one trailing key: every byte before `,"site"` is T1's, unchanged.
    //
    // Rule 6 as clarified in docs/trace-schema-v0.md: a frozen event's golden
    // line moves only by a declared additive TRAILING key, never otherwise.
    QpModeTraceEvent e;
    e.mode = IpqpTraceQpMode::kIpqp;
    e.outcome = IpqpTraceOutcome::kEscaped;
    e.iters = 12;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_qp_mode(e);
    EXPECT_EQ(os.str(), "{\"v\":0,\"ev\":\"qp.mode\",\"seq\":1,\"depth\":0,\"mode\":\"ipqp\","
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
    e.mode = IpqpTraceQpMode::kWalk;
    e.outcome = IpqpTraceOutcome::kOptimal;
    e.iters = 3;
    e.site = QpModeSite::kElasticRung;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_qp_mode(e);
    EXPECT_EQ(os.str(), "{\"v\":0,\"ev\":\"qp.mode\",\"seq\":1,\"depth\":0,\"mode\":\"walk\","
                        "\"outcome\":\"optimal\",\"facts\":\"\",\"iters\":3,"
                        "\"site\":\"elastic_rung\"}\n");
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
// `-Wswitch` in `src/drivers/trace_writer.cpp` only WARNS: this tree carries no
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

const char *spec_spelling(IpqpTraceRegDir v) {
    switch (v) {
    case IpqpTraceRegDir::kDown:
        return "\"down\"";
    case IpqpTraceRegDir::kUp:
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

const char *spec_spelling(IpqpTraceRouteTo v) {
    switch (v) {
    case IpqpTraceRouteTo::kRefine:
        return "\"refine\"";
    case IpqpTraceRouteTo::kSsn:
        return "\"ssn\"";
    case IpqpTraceRouteTo::kWalk:
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

const char *spec_spelling(IpqpTraceQpMode v) {
    switch (v) {
    case IpqpTraceQpMode::kIpqp:
        return "\"ipqp\"";
    case IpqpTraceQpMode::kWalk:
        return "\"walk\"";
    case IpqpTraceQpMode::kSsn:
        return "\"ssn\"";
    }
    return kUnspelled;
}

/// M6 W4 T5's fifth alphabet on `qp.mode`.
const char *spec_spelling(QpModeSite v) {
    switch (v) {
    case QpModeSite::kDispatch:
        return "\"dispatch\"";
    case QpModeSite::kSsnWarmGrade:
        return "\"ssn_warm_grade\"";
    case QpModeSite::kFallbackRungB:
        return "\"fallback_rung_b\"";
    case QpModeSite::kElasticRung:
        return "\"elastic_rung\"";
    case QpModeSite::kSocResolve:
        return "\"soc_resolve\"";
    }
    return kUnspelled;
}

const char *spec_spelling(QpStatus v) {
    switch (v) {
    case QpStatus::kOptimal:
        return "\"optimal\"";
    case QpStatus::kMaxIter:
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

const char *spec_spelling(WorkingSetLinearAlgebra v) {
    switch (v) {
    case WorkingSetLinearAlgebra::kRefactorize:
        return "\"refactorize\"";
    case WorkingSetLinearAlgebra::kSchurBorder:
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
    case StepVerdict::kAcceptF:
        return "\"accept_f\"";
    case StepVerdict::kAcceptH:
        return "\"accept_h\"";
    case StepVerdict::kReject:
        return "\"reject\"";
    case StepVerdict::kRestore:
        return "\"restore\"";
    }
    return kUnspelled;
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
    return kUnspelled;
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
    return kUnspelled;
}

// THE INTERIOR-POINT END EVENT'S STATUS SPELLINGS MOVED IN M6 W5 T8.4, and the
// move is declared: with ConvergenceFlags gone the event carries SolveStatus,
// whose display names are the ones below. `converged` -> `optimal`,
// `not_converged` -> `max_iter` (and the exits the old vocabulary could not tell
// apart now reach `stalled`), `singular_kkt` -> `numerical_error`.
const char *spec_spelling(hven::solvers::SolveStatus v) {
    switch (v) {
    case hven::solvers::SolveStatus::kOptimal:
        return "\"optimal\"";
    case hven::solvers::SolveStatus::kAcceptable:
        return "\"acceptable\"";
    case hven::solvers::SolveStatus::kMaxIter:
        return "\"max_iter\"";
    case hven::solvers::SolveStatus::kInfeasible:
        return "\"infeasible\"";
    case hven::solvers::SolveStatus::kStalled:
        return "\"stalled\"";
    case hven::solvers::SolveStatus::kDiverging:
        return "\"diverging\"";
    case hven::solvers::SolveStatus::kNumericalError:
        return "\"numerical_error\"";
    case hven::solvers::SolveStatus::kBudgetExhausted:
        return "\"budget_exhausted\"";
    case hven::solvers::SolveStatus::kInterrupted:
        return "\"interrupted\"";
    }
    return kUnspelled;
}

const char *spec_spelling(InertiaModes v) {
    switch (v) {
    case InertiaModes::classic:
        return "\"classic\"";
    case InertiaModes::proximal_regularization:
        return "\"proximal_regularization\"";
    }
    return kUnspelled;
}

/// M6 W5 T8.7b's four alphabets: the phase vocabulary `ipm.phase.*` carries,
/// the stop reason and the factorization status `ipm.phase.exit` carries, and
/// the nine message kinds.
const char *spec_spelling(IpmPhase v) {
    switch (v) {
    case IpmPhase::kOptimize:
        return "\"optimize\"";
    case IpmPhase::kSolve:
        return "\"solve\"";
    }
    return kUnspelled;
}

const char *spec_spelling(IpmStopReason v) {
    switch (v) {
    case IpmStopReason::kNone:
        return "\"none\"";
    case IpmStopReason::kIterationCap:
        return "\"iteration_cap\"";
    case IpmStopReason::kRestorationLocallyInfeasible:
        return "\"restoration_locally_infeasible\"";
    case IpmStopReason::kStageStalled:
        return "\"stage_stalled\"";
    case IpmStopReason::kInterrupted:
        return "\"interrupted\"";
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

const char *spec_spelling(IpmMessageKind v) {
    switch (v) {
    case IpmMessageKind::kSolverInitialized:
        return "\"solver_initialized\"";
    case IpmMessageKind::kRankDeficiency:
        return "\"rank_deficiency\"";
    case IpmMessageKind::kFactorizationHardError:
        return "\"factorization_hard_error\"";
    case IpmMessageKind::kInertiaExhausted:
        return "\"inertia_exhausted\"";
    case IpmMessageKind::kRestorationLocallyInfeasible:
        return "\"restoration_locally_infeasible\"";
    case IpmMessageKind::kFeasibilityStall:
        return "\"feasibility_stall\"";
    case IpmMessageKind::kInterruptAtIteration:
        return "\"interrupt_at_iteration\"";
    case IpmMessageKind::kPhaseDiverged:
        return "\"phase_diverged\"";
    case IpmMessageKind::kInterruptSkippingPhases:
        return "\"interrupt_skipping_phases\"";
    }
    return kUnspelled;
}

const char *spec_spelling(RestorationModes v) {
    switch (v) {
    case RestorationModes::off:
        return "\"off\"";
    case RestorationModes::proximal_switch:
        return "\"proximal_switch\"";
    case RestorationModes::l1_nested:
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
    static_assert(static_cast<int>(IpqpTraceRegDir::kUp) == 2 - 1, "2 dir spellings");
    static_assert(static_cast<int>(IpqpTraceRegReason::kFloor) == 4 - 1, "4 reg reasons");
    static_assert(static_cast<int>(IpqpTraceRestartGrade::kFull) == 3 - 1, "3 grades");
    static_assert(static_cast<int>(IpqpTraceRouteTo::kWalk) == 3 - 1, "3 route destinations");
    static_assert(static_cast<int>(IpqpTraceFinalInertia::kUnreadable) == 3 - 1, "3 readings");
    static_assert(static_cast<int>(IpqpTraceEscapeReason::kInfeasibleSuspect) == 5 - 1,
                  "5 escape reasons");
    static_assert(static_cast<int>(IpqpTraceQpMode::kSsn) == 3 - 1, "3 QP modes");
    static_assert(static_cast<int>(QpStatus::kNumericalError) == 4 - 1, "4 QP statuses");
    static_assert(static_cast<int>(StepVerdict::kRestore) == 4 - 1, "4 step verdicts");
    static_assert(static_cast<int>(SolveStatus::kInterrupted) == 9 - 1, "9 solve statuses");
    static_assert(static_cast<int>(WorkingSetLinearAlgebra::kSchurBorder) == 2 - 1,
                  "2 working-set algebras");
    static_assert(static_cast<int>(StartLevel::kHot) == 4 - 1, "4 start levels");
    static_assert(static_cast<int>(IpqpTraceOutcome::kEscaped) == 3 - 1, "3 outcomes");
    static_assert(static_cast<int>(SqpFallbackVerdict::kUnfired) == 5 - 1, "5 verdicts");
    static_assert(static_cast<int>(InertiaModes::proximal_regularization) == 2 - 1,
                  "2 inertia modes");
    static_assert(static_cast<int>(RestorationModes::l1_nested) == 3 - 1, "3 restoration modes");
    // M6 W5 T8.7b's four.
    static_assert(static_cast<int>(IpmPhase::kSolve) == 2 - 1, "2 phases");
    static_assert(static_cast<int>(IpmStopReason::kInterrupted) == 5 - 1, "5 stop reasons");
    static_assert(static_cast<int>(IpmKktFactorStatus::kInvalidInput) == 4 - 1,
                  "4 factorization statuses");
    static_assert(static_cast<int>(IpmMessageKind::kInterruptSkippingPhases) == 9 - 1,
                  "9 message kinds");

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

    // W4 T5's `site`, on the same event.
    const auto site = [](QpModeSite v) {
        QpModeTraceEvent e;
        e.site = v;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_qp_mode(e);
        EXPECT_EQ(raw_field(os.str(), "site"), spec_spelling(v));
    };
    site(QpModeSite::kDispatch);
    site(QpModeSite::kSsnWarmGrade);
    site(QpModeSite::kFallbackRungB);
    site(QpModeSite::kElasticRung);
    site(QpModeSite::kSocResolve);

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

    // W4 T2's three added alphabets, driven through `sqp.major`: the two new
    // QP modes, and the two DRIVER enums the row carries.
    const auto major_mode = [](IpqpTraceQpMode m) {
        const SqpIterate r;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_sqp_major(SqpMajorTraceEvent{r, 0, m});
        EXPECT_EQ(raw_field(os.str(), "mode"), spec_spelling(m));
    };
    major_mode(IpqpTraceQpMode::kIpqp);
    major_mode(IpqpTraceQpMode::kWalk);
    major_mode(IpqpTraceQpMode::kSsn);

    const auto qp_status = [](QpStatus q) {
        SqpIterate r;
        r.qp_status = q;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_sqp_major(SqpMajorTraceEvent{r, 0, IpqpTraceQpMode::kWalk});
        EXPECT_EQ(raw_field(os.str(), "qp_status"), spec_spelling(q));
    };
    qp_status(QpStatus::kOptimal);
    qp_status(QpStatus::kMaxIter);
    qp_status(QpStatus::kInfeasible);
    qp_status(QpStatus::kNumericalError);

    const auto step_verdict = [](StepVerdict v) {
        SqpIterate r;
        r.verdict = v;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_sqp_major(SqpMajorTraceEvent{r, 0, IpqpTraceQpMode::kWalk});
        EXPECT_EQ(raw_field(os.str(), "verdict"), spec_spelling(v));
    };
    step_verdict(StepVerdict::kAcceptF);
    step_verdict(StepVerdict::kAcceptH);
    step_verdict(StepVerdict::kReject);
    step_verdict(StepVerdict::kRestore);

    // W4 T2 fix round 1 (R6): part (c)'s three alphabets, which shipped with a
    // `to_json` case and no net at all.
    const auto solve_status = [](SolveStatus st) {
        const SqpCounters counters;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_sqp_solve_end(SqpSolveEndTraceEvent{st, 0, counters});
        EXPECT_EQ(raw_field(os.str(), "status"), spec_spelling(st));
    };
    solve_status(SolveStatus::kOptimal);
    solve_status(SolveStatus::kMaxIter);
    solve_status(SolveStatus::kInfeasible);
    solve_status(SolveStatus::kNumericalError);
    solve_status(SolveStatus::kBudgetExhausted);

    const auto ws_algebra = [](WorkingSetLinearAlgebra a) {
        SqpSolveBeginTraceEvent e;
        e.ws_algebra = a;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_sqp_solve_begin(e);
        EXPECT_EQ(raw_field(os.str(), "ws_algebra"), spec_spelling(a));
    };
    ws_algebra(WorkingSetLinearAlgebra::kRefactorize);
    ws_algebra(WorkingSetLinearAlgebra::kSchurBorder);

    const auto start_level = [](StartLevel lv) {
        SqpCounters counters;
        counters.start_level_used = lv;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_sqp_solve_end(SqpSolveEndTraceEvent{SolveStatus::kOptimal, 0, counters});
        EXPECT_EQ(raw_field(os.str(), "start_level_used"), spec_spelling(lv));
    };
    // M6 W5 T8.7b's four alphabets, driven through the two phase events and
    // `ipm.phase.exit`. The message kinds go through `ipm.message`.
    const auto ipm_phase = [](IpmPhase v) {
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipm_phase_begin(IpmPhaseTraceEvent{0, "Phase ", v});
        EXPECT_EQ(raw_field(os.str(), "entry"), spec_spelling(v));
    };
    ipm_phase(IpmPhase::kOptimize);
    ipm_phase(IpmPhase::kSolve);

    const auto stop_reason = [](IpmStopReason v) {
        const IterateInfo row;
        IpmPhaseReport report;
        report.stop_reason = v;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipm_phase_exit(IpmPhaseExitTraceEvent{report, row});
        EXPECT_EQ(raw_field(os.str(), "stop_reason"), spec_spelling(v));
    };
    stop_reason(IpmStopReason::kNone);
    stop_reason(IpmStopReason::kIterationCap);
    stop_reason(IpmStopReason::kRestorationLocallyInfeasible);
    stop_reason(IpmStopReason::kStageStalled);
    stop_reason(IpmStopReason::kInterrupted);

    const auto kkt_status = [](IpmKktFactorStatus v) {
        const IterateInfo row;
        const IpmPhaseReport report;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        IpmPhaseExitTraceEvent e{report, row};
        e.last_kkt_info = v;
        s.on_ipm_phase_exit(e);
        EXPECT_EQ(raw_field(os.str(), "last_kkt_info"), spec_spelling(v));
    };
    kkt_status(IpmKktFactorStatus::kSuccess);
    kkt_status(IpmKktFactorStatus::kNumericalIssue);
    kkt_status(IpmKktFactorStatus::kNoConvergence);
    kkt_status(IpmKktFactorStatus::kInvalidInput);

    const auto message_kind = [](IpmMessageKind v) {
        IpmMessageTraceEvent e;
        e.kind = v;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipm_message(e);
        EXPECT_EQ(raw_field(os.str(), "kind"), spec_spelling(v));
    };
    message_kind(IpmMessageKind::kSolverInitialized);
    message_kind(IpmMessageKind::kRankDeficiency);
    message_kind(IpmMessageKind::kFactorizationHardError);
    message_kind(IpmMessageKind::kInertiaExhausted);
    message_kind(IpmMessageKind::kRestorationLocallyInfeasible);
    message_kind(IpmMessageKind::kFeasibilityStall);
    message_kind(IpmMessageKind::kInterruptAtIteration);
    message_kind(IpmMessageKind::kPhaseDiverged);
    message_kind(IpmMessageKind::kInterruptSkippingPhases);

    start_level(StartLevel::kCold);
    start_level(StartLevel::kSeeded);
    start_level(StartLevel::kWarm);
    start_level(StartLevel::kHot);

    // W4 T4: the interior-point driver's three alphabets. `SolveStatus` is
    // that driver's own exit status; the two mode selectors shape the run.
    const auto ipm_status = [](hven::solvers::SolveStatus st) {
        IpmSolveEndTraceEvent e;
        e.status = st;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipm_solve_end(e);
        EXPECT_EQ(raw_field(os.str(), "status"), spec_spelling(st));
    };
    ipm_status(hven::solvers::SolveStatus::kOptimal);
    ipm_status(hven::solvers::SolveStatus::kAcceptable);
    ipm_status(hven::solvers::SolveStatus::kMaxIter);
    ipm_status(hven::solvers::SolveStatus::kDiverging);
    ipm_status(hven::solvers::SolveStatus::kNumericalError);

    const auto inertia_mode = [](InertiaModes m) {
        IpmSolveBeginTraceEvent e;
        e.inertia_mode = m;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipm_solve_begin(e);
        EXPECT_EQ(raw_field(os.str(), "inertia_mode"), spec_spelling(m));
    };
    inertia_mode(InertiaModes::classic);
    inertia_mode(InertiaModes::proximal_regularization);

    const auto restoration_mode = [](RestorationModes m) {
        IpmSolveBeginTraceEvent e;
        e.restoration_mode = m;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipm_solve_begin(e);
        EXPECT_EQ(raw_field(os.str(), "restoration_mode"), spec_spelling(m));
    };
    restoration_mode(RestorationModes::off);
    restoration_mode(RestorationModes::proximal_switch);
    restoration_mode(RestorationModes::l1_nested);
}

TEST(JsonLinesTraceSink, StringEscapingIsRfc8259AndUtf8PassesThrough) {
    IpqpTraceIterEvent e;
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
    sink.on_sqp_solve_begin(SqpSolveBeginTraceEvent{});
    sink.on_ipqp_certify(IpqpTraceCertifyEvent{});
    sink.on_sqp_solve_begin(SqpSolveBeginTraceEvent{});
    sink.on_ipqp_certify(IpqpTraceCertifyEvent{});
    sink.on_sqp_solve_end(SqpSolveEndTraceEvent{SolveStatus::kOptimal, 0, counters});
    sink.on_ipqp_certify(IpqpTraceCertifyEvent{});
    sink.on_sqp_solve_end(SqpSolveEndTraceEvent{SolveStatus::kOptimal, 0, counters});
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
    sink.on_sqp_solve_end(SqpSolveEndTraceEvent{SolveStatus::kOptimal, 0, counters});
    sink.on_ipqp_certify(IpqpTraceCertifyEvent{});
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
class TeeSink final : public TraceSink {
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
    // A REPLAY-CLASS PIN, RE-DERIVED AT W4 T2 (declared). T1 recorded that the
    // walk arm wrote NOTHING.
    //
    // T2 gives it a `sqp.solve` pair, one `sqp.major` line per history row and
    // one `qp.mode` line per major, so the pin now records the whole line
    // census beside the unchanged counters.
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
    const std::map<std::string, Index> by_ev = census(os.str());
    EXPECT_EQ(by_ev.at("sqp.major"), static_cast<Index>(with.history.size()));
    EXPECT_EQ(by_ev.at("qp.mode"), with.counters.major_iters);
    EXPECT_EQ(by_ev.at("sqp.solve.begin"), 1);
    EXPECT_EQ(by_ev.at("sqp.solve.end"), 1);
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
    // it today.
    //
    // NO RULE FORBIDS ONE. `sqp_driver.cpp:1746`'s `catch (const std::exception
    // &)` would already swallow `std::ios_base::failure` if an emit site ever
    // moved inside its try, and THIS PIN is the only thing that would notice.
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

TEST(JsonLinesTraceSink, ThePinnedDeclineIsWhyTheEntryCountSubtractsDeclinedPinned) {
    // On HS11 and HS38 `ipqp_declined_pinned` is 0, so the whole-solve pin's
    // subtraction is never exercised there. HERE every walk route IS a decline,
    // so the stream carries ZERO entries against a positive `ipqp_to_walk`.
    SqpOptions opts;
    opts.qp_mode = QpMode::kIpm;
    opts.max_iter = 60;
    PinnedVariableModel model(true);
    SqpDriver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpSolution sol = driver.solve(model);

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

/// The `mode` token back as its enumerator -- the pin compares through
/// `spec_spelling`, so this is the inverse the comparison needs.
IpqpTraceQpMode mode_of_token(const std::string &token) {
    if (token == "\"ipqp\"") {
        return IpqpTraceQpMode::kIpqp;
    }
    if (token == "\"ssn\"") {
        return IpqpTraceQpMode::kSsn;
    }
    return IpqpTraceQpMode::kWalk;
}

// ===========================================================================
// W4 T2 (a) -- `sqp.major`: THE ROW, IN CALLER UNITS
// ===========================================================================
// THE GOLDEN LINES ARE DECLARED RE-DERIVABLE AT T3 (plan section 2 rule 6):
// that task ADDS `SqpIterate` fields, and the writer's arity `static_assert`
// stops the build until they get keys.
//
// These four lines move with it, as a declared additive re-derivation. They are
// not frozen; the eight W1/W2 events' lines are.
//
// FOUR LINES, NOT ONE, AND THE REASON IS THE EIGHT BOOLS: one line cannot tell
// two `true`s apart. Bool i (1-based, declaration order) is true on line A/B/C
// iff bit 0/1/2 of (i-1) is set, and true on line D unconditionally.
//
// So all eight carry DISTINCT four-line signatures, and any swap of two of them
// changes at least one line's bytes.

/// Bit 0 of (i-1): ipqp_farkas_corroborated, soc_applied,
/// elastic_rho0_ceiling_hit, watchdog_restored.
SqpIterate golden_major_a() {
    SqpIterate r;
    r.trial = 4;
    r.f = -2.5;
    r.stationarity = 1e-7;
    r.feasibility = 0.25;
    r.complementarity = 0.125;
    r.kkt_residual = 0.5;
    r.violation_l1 = 1.5;
    r.tr_radius = 2.0;
    r.mu = 1e-8;
    r.step_norm = 0.75;
    r.qp_solved = false;
    r.ipqp_least_infeasible_primal = 3.25;
    r.ipqp_farkas_corroborated = true;
    r.qp_status = QpStatus::kMaxIter;
    r.qp_minor_iters = 9;
    r.qp_factorizations = 3;
    r.tr_binding = false;
    r.verdict = StepVerdict::kAcceptF;
    r.soc_applied = true;
    r.elastic_applied = false;
    r.elastic_rho0_ceiling_hit = true;
    r.restoration_seed_used = false;
    r.watchdog_restored = true;
    r.active_set_delta = 17;
    r.weak_active_rows = 1;
    r.near_active_rows = 2;
    r.active_rows = 3;
    r.active_lower_sides = 5;
    r.active_upper_sides = 8;
    return r;
}

/// Bit 1: tr_binding, soc_applied, restoration_seed_used, watchdog_restored.
/// Carries the numeric contract too -- -0.0, a subnormal, DBL_MAX, 1e-300 and
/// the three non-finite spellings, on the row's own doubles.
SqpIterate golden_major_b() {
    SqpIterate r;
    r.trial = 0;
    r.f = 0.1;
    r.stationarity = -0.0;
    r.feasibility = std::numeric_limits<double>::quiet_NaN();
    r.complementarity = std::numeric_limits<double>::infinity();
    r.kkt_residual = -std::numeric_limits<double>::infinity();
    r.violation_l1 = 1e-300;
    r.tr_radius = 1.7976931348623157e308;
    r.mu = 4.9406564584124654e-324;
    r.step_norm = 0.0;
    r.qp_solved = false;
    r.ipqp_least_infeasible_primal = 0.0;
    r.ipqp_farkas_corroborated = false;
    r.qp_status = QpStatus::kInfeasible;
    r.qp_minor_iters = 0;
    r.qp_factorizations = 0;
    r.tr_binding = true;
    r.verdict = StepVerdict::kRestore;
    r.soc_applied = true;
    r.elastic_applied = false;
    r.elastic_rho0_ceiling_hit = false;
    r.restoration_seed_used = true;
    r.watchdog_restored = true;
    r.active_set_delta = 6;
    r.weak_active_rows = 5;
    r.near_active_rows = 4;
    r.active_rows = 3;
    r.active_lower_sides = 2;
    r.active_upper_sides = 1;
    return r;
}

/// Bit 2: elastic_applied, elastic_rho0_ceiling_hit, restoration_seed_used,
/// watchdog_restored.
SqpIterate golden_major_c() {
    SqpIterate r;
    r.trial = 11;
    r.f = 1234.5;
    r.stationarity = 1e-12;
    r.feasibility = 6.25;
    r.complementarity = 7.5;
    r.kkt_residual = 8.75;
    r.violation_l1 = 9.0;
    r.tr_radius = 0.03125;
    r.mu = 1e-6;
    r.step_norm = 12.5;
    r.qp_solved = false;
    r.ipqp_least_infeasible_primal = -1.5;
    r.ipqp_farkas_corroborated = false;
    r.qp_status = QpStatus::kNumericalError;
    r.qp_minor_iters = 21;
    r.qp_factorizations = 13;
    r.tr_binding = false;
    r.verdict = StepVerdict::kAcceptH;
    r.soc_applied = false;
    r.elastic_applied = true;
    r.elastic_rho0_ceiling_hit = true;
    r.restoration_seed_used = true;
    r.watchdog_restored = true;
    r.active_set_delta = 101;
    r.weak_active_rows = 0;
    r.near_active_rows = 7;
    r.active_rows = 0;
    r.active_lower_sides = 9;
    r.active_upper_sides = 0;
    return r;
}

/// Every bool true, which is what gives `qp_solved` -- alone among the eight in
/// having bit pattern 0 -- a line where it reads `true`.
SqpIterate golden_major_d() {
    SqpIterate r;
    r.trial = 2;
    r.f = 3.0;
    r.stationarity = 4.0;
    r.feasibility = 5.0;
    r.complementarity = 6.0;
    r.kkt_residual = 7.0;
    r.violation_l1 = 8.0;
    r.tr_radius = 9.0;
    r.mu = 10.0;
    r.step_norm = 11.0;
    r.qp_solved = true;
    r.ipqp_least_infeasible_primal = 12.0;
    r.ipqp_farkas_corroborated = true;
    r.qp_status = QpStatus::kOptimal;
    r.qp_minor_iters = 13;
    r.qp_factorizations = 14;
    r.tr_binding = true;
    r.verdict = StepVerdict::kReject;
    r.soc_applied = true;
    r.elastic_applied = true;
    r.elastic_rho0_ceiling_hit = true;
    r.restoration_seed_used = true;
    r.watchdog_restored = true;
    r.active_set_delta = 15;
    r.weak_active_rows = 16;
    r.near_active_rows = 17;
    r.active_rows = 18;
    r.active_lower_sides = 19;
    r.active_upper_sides = 20;
    return r;
}

std::string major_line(const SqpIterate &row, Index major, IpqpTraceQpMode mode) {
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_sqp_major(SqpMajorTraceEvent{row, major, mode});
    return os.str();
}

TEST(JsonLinesTraceSink, GoldenLineSqpMajorFirstBoolSignatureAndTheWalkMode) {
    EXPECT_EQ(major_line(golden_major_a(), 4, IpqpTraceQpMode::kWalk),
              "{\"v\":0,\"ev\":\"sqp.major\",\"seq\":1,\"depth\":0,\"trial\":4,\"f\":-2.5,"
              "\"stationarity\":9.9999999999999995e-08,\"feasibility\":0.25,\"complementarity\":0."
              "125,\"kkt_residual\":0.5,\"violation_l1\":1.5,\"tr_radius\":2,\"mu\":1e-08,\"step_"
              "norm\":0.75,\"qp_solved\":false,\"ipqp_least_infeasible_primal\":3.25,\"ipqp_farkas_"
              "corroborated\":true,\"qp_status\":\"max_iter\",\"qp_minor_iters\":9,\"qp_"
              "factorizations\":3,\"tr_binding\":false,\"verdict\":\"accept_f\",\"soc_applied\":"
              "true,\"elastic_applied\":false,\"elastic_rho0_ceiling_hit\":true,\"restoration_seed_"
              "used\":false,\"watchdog_restored\":true,\"active_set_delta\":17,\"weak_active_"
              "rows\":1,\"near_active_rows\":2,\"active_rows\":3,\"active_lower_sides\":5,\"active_"
              "upper_sides\":8,\"major\":4,\"mode\":\"walk\"}\n");
}

TEST(JsonLinesTraceSink, GoldenLineSqpMajorSecondBoolSignatureTheHardDoublesAndTheSsnMode) {
    EXPECT_EQ(
        major_line(golden_major_b(), 0, IpqpTraceQpMode::kSsn),
        "{\"v\":0,\"ev\":\"sqp.major\",\"seq\":1,\"depth\":0,\"trial\":0,\"f\":0.10000000000000001,"
        "\"stationarity\":-0,\"feasibility\":\"nan\",\"complementarity\":\"inf\",\"kkt_residual\":"
        "\"-inf\",\"violation_l1\":1e-300,\"tr_radius\":1.7976931348623157e+308,\"mu\":4."
        "9406564584124654e-324,\"step_norm\":0,\"qp_solved\":false,\"ipqp_least_infeasible_"
        "primal\":0,\"ipqp_farkas_corroborated\":false,\"qp_status\":\"infeasible\",\"qp_minor_"
        "iters\":0,\"qp_factorizations\":0,\"tr_binding\":true,\"verdict\":\"restore\",\"soc_"
        "applied\":true,\"elastic_applied\":false,\"elastic_rho0_ceiling_hit\":false,\"restoration_"
        "seed_used\":true,\"watchdog_restored\":true,\"active_set_delta\":6,\"weak_active_rows\":5,"
        "\"near_active_rows\":4,\"active_rows\":3,\"active_lower_sides\":2,\"active_upper_sides\":"
        "1,\"major\":0,\"mode\":\"ssn\"}\n");
}

TEST(JsonLinesTraceSink, GoldenLineSqpMajorThirdBoolSignatureAndTheIpqpMode) {
    EXPECT_EQ(
        major_line(golden_major_c(), 11, IpqpTraceQpMode::kIpqp),
        "{\"v\":0,\"ev\":\"sqp.major\",\"seq\":1,\"depth\":0,\"trial\":11,\"f\":1234.5,"
        "\"stationarity\":9.9999999999999998e-13,\"feasibility\":6.25,\"complementarity\":7.5,"
        "\"kkt_residual\":8.75,\"violation_l1\":9,\"tr_radius\":0.03125,\"mu\":9.9999999999999995e-"
        "07,\"step_norm\":12.5,\"qp_solved\":false,\"ipqp_least_infeasible_primal\":-1.5,\"ipqp_"
        "farkas_corroborated\":false,\"qp_status\":\"numerical_error\",\"qp_minor_iters\":21,\"qp_"
        "factorizations\":13,\"tr_binding\":false,\"verdict\":\"accept_h\",\"soc_applied\":false,"
        "\"elastic_applied\":true,\"elastic_rho0_ceiling_hit\":true,\"restoration_seed_used\":true,"
        "\"watchdog_restored\":true,\"active_set_delta\":101,\"weak_active_rows\":0,\"near_active_"
        "rows\":7,\"active_rows\":0,\"active_lower_sides\":9,\"active_upper_sides\":0,\"major\":11,"
        "\"mode\":\"ipqp\"}\n");
}

TEST(JsonLinesTraceSink, GoldenLineSqpMajorEveryBoolTrue) {
    EXPECT_EQ(
        major_line(golden_major_d(), 2, IpqpTraceQpMode::kWalk),
        "{\"v\":0,\"ev\":\"sqp.major\",\"seq\":1,\"depth\":0,\"trial\":2,\"f\":3,\"stationarity\":"
        "4,\"feasibility\":5,\"complementarity\":6,\"kkt_residual\":7,\"violation_l1\":8,\"tr_"
        "radius\":9,\"mu\":10,\"step_norm\":11,\"qp_solved\":true,\"ipqp_least_infeasible_primal\":"
        "12,\"ipqp_farkas_corroborated\":true,\"qp_status\":\"optimal\",\"qp_minor_iters\":13,\"qp_"
        "factorizations\":14,\"tr_binding\":true,\"verdict\":\"reject\",\"soc_applied\":true,"
        "\"elastic_applied\":true,\"elastic_rho0_ceiling_hit\":true,\"restoration_seed_used\":true,"
        "\"watchdog_restored\":true,\"active_set_delta\":15,\"weak_active_rows\":16,\"near_active_"
        "rows\":17,\"active_rows\":18,\"active_lower_sides\":19,\"active_upper_sides\":20,"
        "\"major\":2,\"mode\":\"walk\"}\n");
}

// ===========================================================================
// W4 T2 (a) -- THE WHOLE-SOLVE CLAIM: the stream IS `history`
// ===========================================================================

/// @brief Every field of one `sqp.major` line against the row it claims to be.
///
/// Hand-listed, and the `static_assert` beside it is what keeps the list
/// complete: a field added to `SqpIterate` moves the arity and fails HERE as
/// well as at the writer, so the pin cannot silently stop covering the row.
void expect_major_line_is_row(const std::string &line, const SqpIterate &r, Index major,
                              IpqpTraceQpMode mode) {
    static_assert(::hven::detail::kAggregateArity<SqpIterate> == 29,
                  "SqpIterate gained a field: compare it below, and in the writer's own key list.");
    EXPECT_EQ(raw_field(line, "trial"), std::to_string(r.trial)) << line;
    expect_double_field(line, "f", r.f);
    expect_double_field(line, "stationarity", r.stationarity);
    expect_double_field(line, "feasibility", r.feasibility);
    expect_double_field(line, "complementarity", r.complementarity);
    expect_double_field(line, "kkt_residual", r.kkt_residual);
    expect_double_field(line, "violation_l1", r.violation_l1);
    expect_double_field(line, "tr_radius", r.tr_radius);
    expect_double_field(line, "mu", r.mu);
    expect_double_field(line, "step_norm", r.step_norm);
    EXPECT_EQ(raw_field(line, "qp_solved"), r.qp_solved ? "true" : "false");
    expect_double_field(line, "ipqp_least_infeasible_primal", r.ipqp_least_infeasible_primal);
    EXPECT_EQ(raw_field(line, "ipqp_farkas_corroborated"),
              r.ipqp_farkas_corroborated ? "true" : "false");
    EXPECT_EQ(raw_field(line, "qp_status"), spec_spelling(r.qp_status));
    EXPECT_EQ(raw_field(line, "qp_minor_iters"), std::to_string(r.qp_minor_iters));
    EXPECT_EQ(raw_field(line, "qp_factorizations"), std::to_string(r.qp_factorizations));
    EXPECT_EQ(raw_field(line, "tr_binding"), r.tr_binding ? "true" : "false");
    EXPECT_EQ(raw_field(line, "verdict"), spec_spelling(r.verdict));
    EXPECT_EQ(raw_field(line, "soc_applied"), r.soc_applied ? "true" : "false");
    EXPECT_EQ(raw_field(line, "elastic_applied"), r.elastic_applied ? "true" : "false");
    EXPECT_EQ(raw_field(line, "elastic_rho0_ceiling_hit"),
              r.elastic_rho0_ceiling_hit ? "true" : "false");
    EXPECT_EQ(raw_field(line, "restoration_seed_used"), r.restoration_seed_used ? "true" : "false");
    EXPECT_EQ(raw_field(line, "watchdog_restored"), r.watchdog_restored ? "true" : "false");
    EXPECT_EQ(raw_field(line, "active_set_delta"), std::to_string(r.active_set_delta));
    EXPECT_EQ(raw_field(line, "weak_active_rows"), std::to_string(r.weak_active_rows));
    EXPECT_EQ(raw_field(line, "near_active_rows"), std::to_string(r.near_active_rows));
    EXPECT_EQ(raw_field(line, "active_rows"), std::to_string(r.active_rows));
    EXPECT_EQ(raw_field(line, "active_lower_sides"), std::to_string(r.active_lower_sides));
    EXPECT_EQ(raw_field(line, "active_upper_sides"), std::to_string(r.active_upper_sides));
    EXPECT_EQ(raw_field(line, "major"), std::to_string(major));
    EXPECT_EQ(raw_field(line, "mode"), spec_spelling(mode));
}

/// The `sqp.major` lines of a stream, in order.
std::vector<std::string> major_lines(const std::string &stream) {
    std::vector<std::string> out;
    for (const std::string &l : split_lines(stream)) {
        if (event_name(l) == "sqp.major") {
            out.push_back(l);
        }
    }
    return out;
}

TEST(JsonLinesTraceSink, SqpMajorReproducesTheHistoryRowByRowOnAWalkCellAndAKIpmCell) {
    // TWO ARMS, because `mode` is the field that differs: HS24 at kWalk reports
    // "walk" on every row, HS38 at kIpm reports whichever arm owned the QP.
    struct Leg {
        int hs;
        QpMode mode;
    };
    for (const Leg leg : {Leg{24, QpMode::kWalk}, Leg{38, QpMode::kIpm}}) {
        SqpOptions opts;
        opts.qp_mode = leg.mode;
        opts.max_iter = 60;
        const HsProblem p = make_hs(leg.hs);
        SqpDriver driver(opts);
        std::ostringstream os;
        JsonLinesTraceSink json(os);
        driver.attach_trace(&json);
        const SqpSolution sol = driver.solve(*p.model);

        SCOPED_TRACE("HS" + std::to_string(leg.hs));
        const std::vector<std::string> rows = major_lines(os.str());
        ASSERT_GT(sol.history.size(), 1u) << "non-vacuous: the cell really iterates";
        ASSERT_EQ(rows.size(), sol.history.size());
        for (std::size_t i = 0; i < rows.size(); ++i) {
            SCOPED_TRACE("row " + std::to_string(i));
            const std::string got = raw_field(rows[i], "mode");
            EXPECT_TRUE(got == "\"walk\"" || got == "\"ssn\"" || got == "\"ipqp\"") << got;
            expect_major_line_is_row(rows[i], sol.history[i], static_cast<Index>(i),
                                     mode_of_token(got));
        }
    }
}

TEST(JsonLinesTraceSink, SqpMajorIsInCallerUnitsOnAScaledSolve) {
    // THE CALLER-UNITS CLAIM, and the only cell that can make it: the emit sits
    // INSIDE `push_history`, after its scaling map, so the stream must equal
    // `history` field for field even where the two spaces differ.
    SqpOptions opts;
    opts.enable_scaling = true;
    opts.max_iter = 60;
    const HsProblem p = make_hs(25);
    SqpDriver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpSolution sol = driver.solve(*p.model);

    ASSERT_TRUE(sol.scaling.active) << "non-vacuous: the solve really scaled";
    const std::vector<std::string> rows = major_lines(os.str());
    ASSERT_GT(sol.history.size(), 1u);
    ASSERT_EQ(rows.size(), sol.history.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        SCOPED_TRACE("row " + std::to_string(i));
        expect_major_line_is_row(rows[i], sol.history[i], static_cast<Index>(i),
                                 IpqpTraceQpMode::kWalk);
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

Index rows_with_a_qp(const SqpSolution &sol) {
    Index rows = 0;
    for (const SqpIterate &r : sol.history) {
        rows += r.qp_solved ? 1 : 0;
    }
    return rows;
}

TEST(JsonLinesTraceSink, QpModeOnAWalkCellIsOneLinePerMajorAndNamesTheWalk) {
    SqpOptions opts;
    opts.qp_mode = QpMode::kWalk;
    opts.max_iter = 60;
    const HsProblem p = make_hs(24);
    SqpDriver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpSolution sol = driver.solve(*p.model);

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
    opts.max_iter = 60;
    const HsProblem p = make_hs(33);
    SqpDriver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpSolution sol = driver.solve(*p.model);

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
    // TWO LEGS, because the kIpm arm reaches the walk two different ways and
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
        opts.qp_mode = QpMode::kIpm;
        opts.max_iter = 60;
        const HsProblem p = make_hs(38);
        SqpDriver driver(opts);
        std::ostringstream os;
        JsonLinesTraceSink json(os);
        TeeSink tee(json);
        driver.attach_trace(&tee);
        const SqpSolution sol = driver.solve(*p.model);

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
        opts.qp_mode = QpMode::kIpm;
        opts.max_iter = 60;
        SqpDriver driver(opts);
        std::ostringstream os;
        JsonLinesTraceSink json(os);
        driver.attach_trace(&json);
        const SqpSolution sol = driver.solve(model);

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
    EXPECT_EQ(kIpqpCountersFieldCount, 39u);
    // 37 -> 38 at M6 W5 T8.5, which appended `polish_ignored` LAST -- after
    // both nested aggregates -- so every pre-existing field offset is unmoved
    // and the two golden lines below gain exactly one key, at the end of the
    // SqpCounters object. DECLARED: the counters JSON is a trace surface.
    EXPECT_EQ(kSqpCountersFieldCount, 38u);
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

void expect_counter_field(const std::string &line, const char *k, StartLevel v, bool) {
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
    opts.qp_mode = QpMode::kIpm;
    opts.max_iter = 60;
    const HsProblem p = make_hs(38);
    SqpDriver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpSolution sol = driver.solve(*p.model);

    const std::string end = only_line(os.str(), "sqp.solve.end");
    ASSERT_FALSE(end.empty());
    EXPECT_EQ(raw_field(end, "majors"), std::to_string(sol.counters.major_iters));
    // A kIpm cell so the two NESTED aggregates are not all-zero: HS38 escapes.
    ASSERT_GT(sol.counters.ipqp.ipqp_iters, 0);
    expect_end_line_counters(end, sol.counters);

    const std::string begin = only_line(os.str(), "sqp.solve.begin");
    EXPECT_EQ(raw_field(begin, "n"), std::to_string(p.model->n()));
    EXPECT_EQ(raw_field(begin, "me"), std::to_string(p.model->me()));
    EXPECT_EQ(raw_field(begin, "mi"), std::to_string(p.model->mi()));
    EXPECT_EQ(raw_field(begin, "qp_mode"), "\"ipqp\"");
    EXPECT_EQ(raw_field(begin, "ws_algebra"), "\"schur_border\"");
    // THE VARIABLE-BOUND CENSUS IS EXHAUSTIVE AND DISJOINT, so its five counts
    // sum to n whatever the model's box looks like.
    Index total = 0;
    for (const char *k :
         {"vars_free", "vars_lower_only", "vars_upper_only", "vars_ranged", "vars_fixed"}) {
        total += std::stoll(raw_field(begin, k));
    }
    EXPECT_EQ(total, p.model->n());
    EXPECT_EQ(json.depth(), 0) << "the pair is balanced";
}

TEST(JsonLinesTraceSink, SqpSolvePartitionsTwoSolvesOnOneSinkAndSeqStaysContiguous) {
    // TYCHO RIDER 2: one sink, two solves. `seq` is per-SINK, so it runs
    // 1..N across both, and every line of solve k lies inside solve k's own
    // pair -- which is what makes the pair a partition rather than a marker.
    SqpOptions opts;
    opts.max_iter = 60;
    const HsProblem p24 = make_hs(24);
    const HsProblem p11 = make_hs(11);
    std::ostringstream os;
    JsonLinesTraceSink json(os);

    SqpDriver a(opts);
    a.attach_trace(&json);
    const SqpSolution first = a.solve(*p24.model);
    SqpDriver b(opts);
    b.attach_trace(&json);
    const SqpSolution second = b.solve(*p11.model);

    const std::vector<std::string> lines = split_lines(os.str());
    ASSERT_GT(lines.size(), 4u);
    Index open = 0;
    Index pairs = 0;
    Index lines_inside = 0;
    Index expected_seq = 0;
    for (const std::string &l : lines) {
        ++expected_seq;
        EXPECT_EQ(raw_field(l, "seq"), std::to_string(expected_seq)) << l;
        EXPECT_EQ(raw_field(l, "depth"), "0");
        const std::string ev = event_name(l);
        if (ev == "sqp.solve.begin") {
            EXPECT_EQ(open, 0) << "a second begin before the first end";
            ++open;
            continue;
        }
        if (ev == "sqp.solve.end") {
            EXPECT_EQ(open, 1) << "an end with no begin";
            --open;
            ++pairs;
            continue;
        }
        EXPECT_EQ(open, 1) << "a line outside every pair: " << l;
        ++lines_inside;
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
                                first.counters.major_iters + second.counters.major_iters + rungs)
        << "one row and one dispatch qp.mode line per major, plus the ladder's own rungs";
    EXPECT_EQ(json.lines_written(), static_cast<Index>(lines.size()));
    EXPECT_EQ(json.depth(), 0);
}

TEST(JsonLinesTraceSink, OrderIsTheJoinKeyOnHS38AtKIpm) {
    // TYCHO RIDER 1. Every event line belongs to the major whose `sqp.major`
    // line comes NEXT: the row is pushed at the end of its own major, so the
    // bracket is (previous row, this row].
    //
    // DEPTH 0 ONLY. A nested restoration sub-solve writes its own lines into
    // the same stream at depth 1, and they belong to its own brackets.
    SqpOptions opts;
    opts.qp_mode = QpMode::kIpm;
    opts.max_iter = 60;
    const HsProblem p = make_hs(38);
    SqpDriver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpSolution sol = driver.solve(*p.model);

    Index brackets = 0;
    Index pending = 0;
    Index trailing = 0;
    bool inside = false;
    for (const std::string &l : split_lines(os.str())) {
        if (raw_field(l, "depth") != "0") {
            continue;
        }
        const std::string ev = event_name(l);
        if (ev == "sqp.solve.begin") {
            inside = true;
            continue;
        }
        if (ev == "sqp.solve.end") {
            inside = false;
            trailing = pending;
            continue;
        }
        EXPECT_TRUE(inside) << "outside the solve's own pair: " << l;
        if (ev == "sqp.major") {
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
        opts.max_iter = 60;
        const HsProblem p = make_hs(24);
        SqpDriver driver(opts);
        std::ostringstream os;
        JsonLinesTraceSink json(os);
        driver.attach_trace(&json);
        const SqpSolution sol = driver.solve(*p.model);
        ASSERT_EQ(sol.status, SolveStatus::kOptimal);
        EXPECT_EQ(raw_field(only_line(os.str(), "sqp.solve.end"), "status"), "\"optimal\"");
        EXPECT_EQ(json.depth(), 0);
    }
    {
        SqpOptions opts;
        opts.max_iter = 1;
        const HsProblem p = make_hs(38);
        SqpDriver driver(opts);
        std::ostringstream os;
        JsonLinesTraceSink json(os);
        driver.attach_trace(&json);
        const SqpSolution sol = driver.solve(*p.model);
        ASSERT_EQ(sol.status, SolveStatus::kMaxIter);
        EXPECT_EQ(raw_field(only_line(os.str(), "sqp.solve.end"), "status"), "\"max_iter\"");
        EXPECT_EQ(json.depth(), 0);
    }
    {
        CircleAndFarLineModel model;
        SqpOptions opts;
        opts.max_iter = 200;
        SqpDriver driver(opts);
        std::ostringstream os;
        JsonLinesTraceSink json(os);
        driver.attach_trace(&json);
        const SqpSolution sol = driver.solve(model);
        ASSERT_EQ(sol.status, SolveStatus::kInfeasible);
        EXPECT_EQ(raw_field(only_line(os.str(), "sqp.solve.end"), "status"), "\"infeasible\"");
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
    opts.max_iter = 200;
    SqpDriver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpSolution sol = driver.solve(model);
    ASSERT_EQ(sol.status, SolveStatus::kInfeasible);
    ASSERT_GE(sol.counters.restoration_iters, 1) << "the phase must have RUN";

    bool seen = false;
    for (const std::string &l : split_lines(os.str())) {
        if (event_name(l) != "sqp.major" || raw_field(l, "depth") != "1") {
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
    opts.max_iter = 200;
    SqpDriver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpSolution sol = driver.solve(model);
    ASSERT_EQ(sol.status, SolveStatus::kInfeasible);
    ASSERT_GE(sol.counters.restoration_iters, 1);

    Index outer = 0;
    for (const SqpIterate &r : sol.history) {
        outer += r.active_set_delta;
    }
    EXPECT_EQ(sol.counters.active_set_delta_total, outer);

    Index nested = 0;
    for (const std::string &l : split_lines(os.str())) {
        if (event_name(l) == "sqp.major" && raw_field(l, "depth") == "1") {
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
    opts.max_iter = 200;
    SqpDriver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpSolution sol = driver.solve(model);

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
        if (ev == "sqp.solve.begin") {
            ++depth1_begins;
            in_nested = true;
            continue;
        }
        if (ev == "sqp.solve.end") {
            ++depth1_ends;
            nested_end = l;
            in_nested = false;
            continue;
        }
        EXPECT_TRUE(in_nested) << "a depth-1 line outside the nested pair: " << l;
        if (ev == "sqp.major") {
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
    EXPECT_EQ(raw_field(nested_end, "majors"), std::to_string(sol.counters.restoration_iters));
    // ONE walk `qp.mode` line per sub-MAJOR. The sub-solve has one row MORE
    // than that -- the stopped-AT-iterate row, which no subproblem produced.
    EXPECT_EQ(depth1_other, sol.counters.restoration_iters);
    EXPECT_GT(depth1_rows, depth1_other);
    // THE BALANCE, PINNED DIRECTLY (tycho rider 3) rather than left to
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
void assign_distinct(StartLevel &field, Index &next) {
    field = StartLevel::kHot;
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

SqpSolveBeginTraceEvent golden_begin() {
    SqpSolveBeginTraceEvent e;
    e.n = 11;
    e.me = 3;
    e.mi = 5;
    e.vars_free = 1;
    e.vars_lower_only = 2;
    e.vars_upper_only = 4;
    e.vars_ranged = 6;
    e.vars_fixed = 7;
    e.qp_mode = IpqpTraceQpMode::kSsn;
    e.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;
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

TEST(JsonLinesTraceSink, GoldenLineSqpSolveBegin) {
    // BYTE-EXACT, from a hand-filled struct with every field DISTINCT.
    //
    // Nothing else in the suite tests key ORDER on this line: every other check
    // is a key lookup, generated from the same table as the writer.
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_sqp_solve_begin(golden_begin());
    EXPECT_EQ(os.str(),
              "{\"v\":0,\"ev\":\"sqp.solve.begin\",\"seq\":1,\"depth\":0,\"n\":11,\"me\":3,\"mi\":5"
              ",\"vars_free\":1,\"vars_lower_only\":2,\"vars_upper_only\":4,\"vars_ranged\":6,\"var"
              "s_fixed\":7,\"qp_mode\":\"ssn\",\"ws_algebra\":\"refactorize\"}\n");
}

TEST(JsonLinesTraceSink, GoldenLineSqpSolveEndWithEveryCounterDistinct) {
    // THE 94 COUNTERS IN TABLE ORDER, each carrying its own number, so a
    // reorder, a duplicate or a dropped entry moves these bytes.
    //
    // Declared re-derivable while v0 is open (plan section 2 rule 6).
    //
    // RE-DERIVED AT M6 W5 T8.5, which appended `polish_ignored` to
    // `SqpCounters` -- the counter that says a multipliers-only payload's
    // polish extension was dropped. It is LAST in the table, so the line gains
    // exactly one key between `near_active_peak` and the `ssn` object, and the
    // nested tables' distinct values shift up by one. Nothing else moved.
    const SqpCounters c = distinct_counters();
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    // RE-DERIVED AT M6 W5 T8.7, which appended the FIVE scaling fields the
    // console's `Scaling:` trailer reads. They are LAST on the line, after the
    // counters object, each with its own value.
    sink.on_sqp_solve_end(SqpSolveEndTraceEvent{SolveStatus::kBudgetExhausted, 42, c,
                                                /*scaling_active=*/true, 0.25, 0.5, 4.0, 7.5e-09});
    EXPECT_EQ(os.str(),
              "{\"v\":0,\"ev\":\"sqp.solve.end\",\"seq\":1,\"depth\":0,\"status\":\"budget_exhauste"
              "d\",\"majors\":42,\"counters\":{\"major_iters\":1,\"qp_minor_iters\":2,\"factorizati"
              "ons\":3,\"steps_accepted\":4,\"rejected_steps\":5,\"soc_steps\":6,\"soc_applied\":7,"
              "\"soc_qp_infeasible\":8,\"soc_rejected\":9,\"elastic_activations\":10,\"elastic_esca"
              "lations\":11,\"restoration_iters\":12,\"elastic_from_ipqp_escape\":13,\"ipqp_suspici"
              "on_disproved\":14,\"ipqp_fallback_rung_b\":15,\"elastic_rho0_ceiling_hits\":16,\"ela"
              "stic_floor_retries\":17,\"eqp_refine_steps\":18,\"border_refine_steps\":19,\"verdict"
              "_refine_steps\":20,\"suspect_escalations\":21,\"symbolic_analyses\":22,\"start_level"
              "_used\":\"hot\",\"full_step_majors\":24,\"watchdog_restores\":25,\"evals_full\":26,"
              "\"evals_values\":27,\"probe_budget_stops\":28,\"crash_seeded_rows\":29,\"crash_seede"
              "d_bounds\":30,\"n_seeded\":31,\"seeded_clamped\":32,\"ip_activity_inferred\":33,\"ac"
              "tive_set_delta_total\":34,\"active_set_delta_peak\":35,\"weak_active_peak\":36,\"nea"
              "r_active_peak\":37,\"polish_ignored\":38,\"ssn\":{\"ssn_iters\":39,\"ssn_bulk_flips"
              "\":40,\"ssn_backtracks\":41,\"ssn_prox_updates\":42,\"ssn_escapes\":43,\"ssn_uncerta"
              "in_peak\":44,\"ssn_refinements\":45,\"ssn_refine_refused\":46,\"ssn_refine_factoriza"
              "tions\":47,\"ssn_refine_neg_duals\":48,\"ssn_sign_swept\":49,\"ssn_sign_sweep_max\":"
              "50,\"ssn_escape_budget\":51,\"ssn_escape_singular\":52,\"ssn_escape_no_contraction\""
              ":53,\"ssn_escape_infeasible_suspect\":54,\"ssn_escape_indefinite\":55,\"ssn_escape_g"
              "ate_refused\":56},\"ipqp\":{\"ipqp_iters\":57,\"ipqp_factorizations\":58,\"ipqp_symb"
              "olic_analyses\":59,\"ipqp_solves\":60,\"ipqp_pattern_verifies\":61,\"ipqp_rho_demand"
              "ed_max\":62,\"ipqp_rho_demanded_last\":63,\"ipqp_inertia_retries\":64,\"ipqp_iters_a"
              "t_elevated_rho\":65,\"ipqp_ladder_reclimbs\":66,\"ipqp_pivot_reroute_primal\":67,\"i"
              "pqp_pivot_reroute_dual_fallback\":68,\"ipqp_iters_ladder_armed_no_advance\":69,\"ipq"
              "p_final_inertia_read\":70,\"ipqp_reg_decreases\":71,\"ipqp_reg_increases\":72,\"ipqp"
              "_prox_center_updates\":73,\"ipqp_restart_repairs\":74,\"ipqp_restart_shift_max\":75,"
              "\"ipqp_mu_adopted\":76,\"ipqp_warm_restart_abandoned\":77,\"ipqp_declined_pinned\":7"
              "8,\"ipqp_tier_retired_after\":79,\"ipqp_face_uncertain\":80,\"ipqp_refine_accepted\""
              ":81,\"ipqp_refine_refused\":82,\"ipqp_to_refine\":83,\"ipqp_to_ssn\":84,\"ipqp_to_wa"
              "lk\":85,\"ipqp_escapes\":86,\"ipqp_escape_budget\":87,\"ipqp_escape_stall\":88,\"ipq"
              "p_escape_indefinite\":89,\"ipqp_escape_numerical\":90,\"ipqp_escape_infeasible_suspe"
              "ct\":91,\"ipqp_alpha_p_min\":92,\"ipqp_alpha_d_min\":93,\"ipqp_read_kept_tight_sides"
              "\":94,\"ipqp_read_barrier_noise_sides\":95}},\"scaling_active\":true,\"obj_scale\":0"
              ".25,\"row_scale_min\":0.5,\"row_scale_max\":4,\"scaled_kkt_residual\":7.499999999999"
              "9993e-09}\n");
}

TEST(JsonLinesTraceSink, GoldenLineSqpSolveEndWritesTheAbsenceSentinelsAsNull) {
    // THE OTHER HALF OF RULE 5 (fix round 1, R7): a default-constructed
    // `SqpCounters` holds the three documented absence sentinels.
    //
    // `ipqp_alpha_p_min` and `ipqp_alpha_d_min` at `+infinity` ("no step
    // observed yet") and `ipqp_tier_retired_after` at 0 ("never retired") must
    // every one read `null`, not a value.
    //
    // RE-DERIVED AT M6 W5 T8.5 for the reason the line above was: one new key,
    // `polish_ignored`, whose own absence sentinel is simply 0.
    const SqpCounters c;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    // The default scaling block rides here (M6 W5 T8.7): `active` false and the
    // three factors at 1.0, which is the INACTIVE report's identity -- a reader
    // never has to branch on `active` to use it.
    sink.on_sqp_solve_end(SqpSolveEndTraceEvent{SolveStatus::kNumericalError, 0, c});
    EXPECT_EQ(os.str(),
              "{\"v\":0,\"ev\":\"sqp.solve.end\",\"seq\":1,\"depth\":0,\"status\":\"numerical_error"
              "\",\"majors\":0,\"counters\":{\"major_iters\":0,\"qp_minor_iters\":0,\"factorization"
              "s\":0,\"steps_accepted\":0,\"rejected_steps\":0,\"soc_steps\":0,\"soc_applied\":0,\""
              "soc_qp_infeasible\":0,\"soc_rejected\":0,\"elastic_activations\":0,\"elastic_escalat"
              "ions\":0,\"restoration_iters\":0,\"elastic_from_ipqp_escape\":0,\"ipqp_suspicion_dis"
              "proved\":0,\"ipqp_fallback_rung_b\":0,\"elastic_rho0_ceiling_hits\":0,\"elastic_floo"
              "r_retries\":0,\"eqp_refine_steps\":0,\"border_refine_steps\":0,\"verdict_refine_step"
              "s\":0,\"suspect_escalations\":0,\"symbolic_analyses\":0,\"start_level_used\":\"cold"
              "\",\"full_step_majors\":0,\"watchdog_restores\":0,\"evals_full\":0,\"evals_values\":"
              "0,\"probe_budget_stops\":0,\"crash_seeded_rows\":0,\"crash_seeded_bounds\":0,\"n_see"
              "ded\":0,\"seeded_clamped\":0,\"ip_activity_inferred\":0,\"active_set_delta_total\":0"
              ",\"active_set_delta_peak\":0,\"weak_active_peak\":0,\"near_active_peak\":0,\"polish_"
              "ignored\":0,\"ssn\":{\"ssn_iters\":0,\"ssn_bulk_flips\":0,\"ssn_backtracks\":0,\"ssn"
              "_prox_updates\":0,\"ssn_escapes\":0,\"ssn_uncertain_peak\":0,\"ssn_refinements\":0,"
              "\"ssn_refine_refused\":0,\"ssn_refine_factorizations\":0,\"ssn_refine_neg_duals\":0,"
              "\"ssn_sign_swept\":0,\"ssn_sign_sweep_max\":0,\"ssn_escape_budget\":0,\"ssn_escape_s"
              "ingular\":0,\"ssn_escape_no_contraction\":0,\"ssn_escape_infeasible_suspect\":0,\"ss"
              "n_escape_indefinite\":0,\"ssn_escape_gate_refused\":0},\"ipqp\":{\"ipqp_iters\":0,\""
              "ipqp_factorizations\":0,\"ipqp_symbolic_analyses\":0,\"ipqp_solves\":0,\"ipqp_patter"
              "n_verifies\":0,\"ipqp_rho_demanded_max\":0,\"ipqp_rho_demanded_last\":0,\"ipqp_inert"
              "ia_retries\":0,\"ipqp_iters_at_elevated_rho\":0,\"ipqp_ladder_reclimbs\":0,\"ipqp_pi"
              "vot_reroute_primal\":0,\"ipqp_pivot_reroute_dual_fallback\":0,\"ipqp_iters_ladder_ar"
              "med_no_advance\":0,\"ipqp_final_inertia_read\":0,\"ipqp_reg_decreases\":0,\"ipqp_reg"
              "_increases\":0,\"ipqp_prox_center_updates\":0,\"ipqp_restart_repairs\":0,\"ipqp_rest"
              "art_shift_max\":0,\"ipqp_mu_adopted\":0,\"ipqp_warm_restart_abandoned\":0,\"ipqp_dec"
              "lined_pinned\":0,\"ipqp_tier_retired_after\":null,\"ipqp_face_uncertain\":0,\"ipqp_r"
              "efine_accepted\":0,\"ipqp_refine_refused\":0,\"ipqp_to_refine\":0,\"ipqp_to_ssn\":0,"
              "\"ipqp_to_walk\":0,\"ipqp_escapes\":0,\"ipqp_escape_budget\":0,\"ipqp_escape_stall\""
              ":0,\"ipqp_escape_indefinite\":0,\"ipqp_escape_numerical\":0,\"ipqp_escape_infeasible"
              "_suspect\":0,\"ipqp_alpha_p_min\":null,\"ipqp_alpha_d_min\":null,\"ipqp_read_kept_ti"
              "ght_sides\":0,\"ipqp_read_barrier_noise_sides\":0}},\"scaling_active\":false,\"obj_s"
              "cale\":1,\"row_scale_min\":1,\"row_scale_max\":1,\"scaled_kkt_residual\":0}\n");
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
    sink.on_sqp_solve_end(SqpSolveEndTraceEvent{SolveStatus::kOptimal, 0, c});
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
    opts.qp_mode = QpMode::kIpm;
    opts.max_iter = 60;
    const HsProblem p = make_hs(38);
    SqpDriver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    SqpSolution sol = driver.solve(*p.model);
    ASSERT_FALSE(sol.history.empty());
    const SqpSolution moved = std::move(sol);

    const std::string end = only_line(os.str(), "sqp.solve.end");
    ASSERT_FALSE(end.empty());
    EXPECT_EQ(raw_field(end, "majors"), std::to_string(moved.counters.major_iters));
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
    opts.max_iter = 60;
    const HsProblem p = make_hs(24);
    std::ostringstream measure;
    {
        JsonLinesTraceSink json(measure);
        SqpDriver driver(opts);
        driver.attach_trace(&json);
        driver.solve(*p.model);
    }
    const std::vector<std::string> lines = split_lines(measure.str());
    ASSERT_GE(lines.size(), 2u);
    ASSERT_EQ(event_name(lines.back()), "sqp.solve.end") << "the end line must be the last write";
    const std::streamsize budget =
        static_cast<std::streamsize>(measure.str().size() - lines.back().size() - 1);

    FailAfterBuf buf(budget);
    std::ostream out(&buf);
    out.exceptions(std::ios::badbit);
    SqpDriver traced(opts);
    JsonLinesTraceSink json(out);
    traced.attach_trace(&json);
    EXPECT_THROW(traced.solve(*p.model), std::ios_base::failure);
    EXPECT_EQ(buf.taken.size(), static_cast<std::size_t>(budget)) << "it really failed on the last";
}

TEST(JsonLinesTraceSink, ARefusedArgumentWritesNothingAndLeavesTheNextSolveAtDepthZero) {
    // FIX ROUND 1, R3(a)/R11. `SqpDriver::solve` refuses a wrong-sized `x0`
    // with `std::invalid_argument` -- a recoverable API refusal.
    //
    // Emitting `begin` first left the sink one solve deep for ever, and every
    // later stream on it read `"depth":1`.
    //
    // ASSERTED ON THE STREAM, not on `depth()`: the leak is invisible to the
    // accessor, which reads 0 between solves either way.
    SqpOptions opts;
    opts.max_iter = 60;
    const HsProblem p = make_hs(24);
    std::ostringstream os;
    JsonLinesTraceSink json(os);

    SqpDriver bad(opts);
    bad.attach_trace(&json);
    EXPECT_THROW(bad.solve(*p.model, Vec::Zero(p.model->n() + 1)), std::invalid_argument);
    EXPECT_EQ(os.str(), "") << "a refused call writes nothing at all";
    EXPECT_EQ(json.lines_written(), 0);

    SqpDriver good(opts);
    good.attach_trace(&json);
    const SqpSolution sol = good.solve(*p.model);
    ASSERT_GT(sol.counters.major_iters, 0);
    const std::vector<std::string> lines = split_lines(os.str());
    ASSERT_FALSE(lines.empty());
    for (const std::string &l : lines) {
        EXPECT_EQ(raw_field(l, "depth"), "0") << l;
    }
    EXPECT_EQ(event_name(lines.front()), "sqp.solve.begin");
    EXPECT_EQ(event_name(lines.back()), "sqp.solve.end");
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

TEST(JsonLinesTraceSink, ResetNestingRecoversASinkAfterASolveThatThrewMidBody) {
    // FIX ROUND 1, R3(b). "Exceptions excluded" stands: a solve that threw
    // writes `begin` and no `end`, which is the honest record.
    //
    // The sink is then one level open, and `reset_nesting()` is how a harness
    // that keeps using it says so. `seq` is deliberately NOT reset: the lines
    // already written are part of the artifact.
    SqpOptions opts;
    opts.max_iter = 60;
    std::ostringstream os;
    JsonLinesTraceSink json(os);

    ThrowsOnSecondGradientModel bad_model;
    SqpDriver thrower(opts);
    thrower.attach_trace(&json);
    EXPECT_THROW(thrower.solve(bad_model), std::runtime_error);
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
            if (event_name(l) == "sqp.major") {
                majors.push_back(l);
            }
        }
        ASSERT_EQ(majors.size(), 1u)
            << "the accepted major's row must be emitted BEFORE the commit that throws";
        EXPECT_EQ(raw_field(majors.front(), "trial"), "0");
        EXPECT_EQ(raw_field(majors.front(), "major"), "0");
        const std::string verdict = raw_field(majors.front(), "verdict");
        EXPECT_TRUE(verdict == "\"accept_f\"" || verdict == "\"accept_h\"")
            << "the row already in the stream is the ACCEPTED one, verdict=" << verdict;
    }

    json.reset_nesting();

    const HsProblem p = make_hs(24);
    SqpDriver good(opts);
    good.attach_trace(&json);
    const SqpSolution sol = good.solve(*p.model);
    ASSERT_GT(sol.counters.major_iters, 0);

    const std::vector<std::string> lines = split_lines(os.str());
    Index begins = 0;
    Index ends = 0;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        EXPECT_EQ(raw_field(lines[i], "seq"), std::to_string(i + 1)) << "seq is NOT reset";
        if (static_cast<Index>(i) >= lines_after_throw) {
            EXPECT_EQ(raw_field(lines[i], "depth"), "0") << lines[i];
        }
        begins += (event_name(lines[i]) == "sqp.solve.begin") ? 1 : 0;
        ends += (event_name(lines[i]) == "sqp.solve.end") ? 1 : 0;
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
    opts.max_iter = 200;
    SqpDriver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpSolution sol = driver.solve(model);

    ASSERT_GE(sol.counters.restoration_iters, 1) << "the phase must have RUN";
    Index seeded_rows = 0;
    for (const SqpIterate &r : sol.history) {
        seeded_rows += r.restoration_seed_used ? 1 : 0;
    }
    ASSERT_GT(seeded_rows, 0) << "non-vacuous: a row really takes a candidate seed";

    std::vector<std::string> rows;
    for (const std::string &l : split_lines(os.str())) {
        if (event_name(l) == "sqp.major" && raw_field(l, "depth") == "0") {
            rows.push_back(l);
        }
    }
    ASSERT_EQ(rows.size(), sol.history.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        SCOPED_TRACE("row " + std::to_string(i));
        expect_major_line_is_row(rows[i], sol.history[i], static_cast<Index>(i),
                                 mode_of_token(raw_field(rows[i], "mode")));
    }
}

TEST(JsonLinesTraceSink, TheRowsModeAndTheDispatchRecordDifferOnTheSsnWarmGrade) {
    // FIX ROUND 1, R4. `sqp.major.mode` is the arm that produced the ROW's
    // step; `qp.mode` is the DISPATCH record.
    //
    // They agree at kWalk and kSsn, and differ under kIpm on the majors routed
    // to the SSN warm grade -- not the kSsn arm, and writing no line.
    //
    // HS3 at kIpm is the cell that exhibits it (the same cell
    // test_ipqp_trace.cpp uses for the `to_ssn` row).
    SqpOptions opts;
    opts.qp_mode = QpMode::kIpm;
    opts.max_iter = 60;
    const HsProblem p = make_hs(3);
    SqpDriver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpSolution sol = driver.solve(*p.model);

    ASSERT_GT(sol.counters.ipqp.ipqp_to_ssn, 0) << "non-vacuous: the grade must be reached";
    Index rows_reading_ssn = 0;
    for (const std::string &l : split_lines(os.str())) {
        if (event_name(l) == "sqp.major" && raw_field(l, "mode") == "\"ssn\"") {
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
    opts.max_iter = 200;
    SqpDriver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpSolution sol = driver.solve(model);
    ASSERT_GE(sol.counters.restoration_iters, 1);

    Index nested_end_seq = 0;
    Index first_row_after = 0;
    for (const std::string &l : split_lines(os.str())) {
        const Index seq = static_cast<Index>(std::stoll(raw_field(l, "seq")));
        if (raw_field(l, "depth") == "1" && event_name(l) == "sqp.solve.end") {
            nested_end_seq = seq;
            continue;
        }
        if (nested_end_seq != 0 && first_row_after == 0 && raw_field(l, "depth") == "0" &&
            event_name(l) == "sqp.major") {
            first_row_after = seq;
        }
    }
    ASSERT_GT(nested_end_seq, 0) << "the fixture must really restore";
    ASSERT_GT(first_row_after, 0) << "the requesting row must be written at all";
    EXPECT_GT(first_row_after, nested_end_seq)
        << "the requesting row is written after the solve it asked for";
}

// ===========================================================================
// W4 T4 -- the interior-point events' golden lines
// ===========================================================================
//
// The DRIVER-side pins (both emit sites, the count identity, the callback as
// the oracle, the null sink, the two exit statuses, the mixed stream) are in
// tests/interior/test_ipm_trace.cpp; these four are the serializer's.

/// @brief The classic path's record: every field distinct, with `prox_reg_*` at
/// "proximal mode off", the rejection pair at "no rejection recorded", and the
/// perturbed-pivot count NOT OBSERVED. Five `null`s, three different meanings.
IterateInfo golden_ipm_iter_sentinels() {
    IterateInfo r;
    r.iter_ = 7;
    r.mu_ = 0.25;
    r.prim_obj_ = 1.5;
    r.barr_obj_ = 2.25;
    r.kkt_inf_ = 0.5;
    r.barr_inf_ = 0.125;
    r.econ_inf_ = 0.0625;
    r.icon_inf_ = 0.03125;
    r.pen_par1_ = 3.5;
    r.pen_par2_ = 4.75;
    r.ls_iters_ = 2;
    r.alpha_p_ = 0.9;
    r.alpha_d_ = 0.8;
    r.alpha_t_ = 0.7;
    r.h_pert_ = 1e-8;
    r.h_facs_ = 3;
    r.h_pert_cum_ = 2e-8;
    r.prox_reg_primal_ = -1.0;
    r.prox_reg_dual_ = -1.0;
    r.p_pivots_ = 5;
    // NOT observed: the projected 5 is the substitute an absent backend count
    // leaves behind, so the line must read `null` and not repeat it.
    r.p_pivots_observed_ = false;
    r.max_e_mult_ = 11.0;
    r.max_i_mult_ = 12.5;
    r.merit_val_ = 13.25;
    r.accepted_ = false;
    r.first_rejection_iter_ = -1;
    r.theta_at_first_rejection_ = -1.0;
    r.eval_exceptions_ = 4;
    return r;
}

/// @brief The same record with every sentinel-bearing field carrying a REAL
/// value, including the two the boundary is easiest to get wrong:
/// `first_rejection_iter_ == 0` (the FIRST trial was rejected) and
/// `theta_at_first_rejection_ == 0.0` (a feasible reading). Both are numbers,
/// not `null`, which is what makes the `< 0` predicate falsifiable.
IterateInfo golden_ipm_iter_present() {
    IterateInfo r;
    r.iter_ = 1;
    r.mu_ = 0.1;
    r.prim_obj_ = -0.5;
    r.barr_obj_ = -0.25;
    r.kkt_inf_ = std::numeric_limits<double>::quiet_NaN();
    r.barr_inf_ = std::numeric_limits<double>::infinity();
    r.econ_inf_ = -std::numeric_limits<double>::infinity();
    r.icon_inf_ = -0.0;
    r.pen_par1_ = 100.0;
    r.pen_par2_ = 1000.0;
    r.ls_iters_ = 1;
    r.alpha_p_ = 0.5;
    r.alpha_d_ = 0.25;
    r.alpha_t_ = 0.125;
    r.h_pert_ = 1e-4;
    r.h_facs_ = 2;
    r.h_pert_cum_ = 3e-4;
    r.prox_reg_primal_ = 1e-6;
    r.prox_reg_dual_ = 2e-6;
    r.p_pivots_ = 9;
    r.p_pivots_observed_ = true;
    r.max_e_mult_ = 1e3;
    r.max_i_mult_ = 1e4;
    r.merit_val_ = 0.1;
    r.accepted_ = true;
    r.first_rejection_iter_ = 0;
    r.theta_at_first_rejection_ = 0.0;
    r.eval_exceptions_ = 0;
    return r;
}

TEST(JsonLinesTraceSink, GoldenLineIpmIterWithBothMinusOneConventionsActive) {
    const IterateInfo r = golden_ipm_iter_sentinels();
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipm_iter(IpmIterTraceEvent{r, 1});
    EXPECT_EQ(os.str(),
              "{\"v\":0,\"ev\":\"ipm.iter\",\"seq\":1,\"depth\":0,\"iter\":7,\"mu\":0.25,"
              "\"prim_obj\":1.5,\"barr_obj\":2.25,\"kkt_inf\":0.5,\"barr_inf\":0.125,"
              "\"econ_inf\":0.0625,\"icon_inf\":0.03125,\"pen_par1\":3.5,\"pen_par2\":4.75,"
              "\"ls_iters\":2,\"alpha_p\":0.90000000000000002,\"alpha_d\":0.80000000000000004,"
              "\"alpha_t\":0.69999999999999996,\"h_pert\":1e-08,\"h_facs\":3,\"h_pert_cum\":2e-08,"
              "\"prox_reg_primal\":null,\"prox_reg_dual\":null,\"p_pivots\":null,\"max_e_mult\":11,"
              "\"max_i_mult\":12.5,\"merit_val\":13.25,\"accepted\":false,"
              "\"first_rejection_iter\":null,\"theta_at_first_rejection\":null,"
              "\"eval_exceptions\":4,\"phase\":1}\n");
}

TEST(JsonLinesTraceSink, GoldenLineIpmIterWithEverySentinelBearingFieldPresent) {
    const IterateInfo r = golden_ipm_iter_present();
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipm_iter(IpmIterTraceEvent{r, 0});
    EXPECT_EQ(os.str(),
              "{\"v\":0,\"ev\":\"ipm.iter\",\"seq\":1,\"depth\":0,\"iter\":1,"
              "\"mu\":0.10000000000000001,\"prim_obj\":-0.5,\"barr_obj\":-0.25,\"kkt_inf\":\"nan\","
              "\"barr_inf\":\"inf\",\"econ_inf\":\"-inf\",\"icon_inf\":-0,\"pen_par1\":100,"
              "\"pen_par2\":1000,\"ls_iters\":1,\"alpha_p\":0.5,\"alpha_d\":0.25,\"alpha_t\":0.125,"
              "\"h_pert\":0.0001,\"h_facs\":2,\"h_pert_cum\":0.00029999999999999997,"
              "\"prox_reg_primal\":9.9999999999999995e-07,\"prox_reg_dual\":1.9999999999999999e-06,"
              "\"p_pivots\":9,\"max_e_mult\":1000,\"max_i_mult\":10000,"
              "\"merit_val\":0.10000000000000001,\"accepted\":true,\"first_rejection_iter\":0,"
              "\"theta_at_first_rejection\":0,\"eval_exceptions\":0,\"phase\":0}\n");
}

/// @brief The NON-FINITE NEWTON record (fix round 1, R1): `alg_impl`'s
/// `!GoodStep` branch writes `h_facs_ = -1` and that record reaches the ordinary
/// emit site before the driver reports DIVERGING, so the stream really can carry
/// it. Everything else here is a real reading, so this line isolates the third
/// sentinel from the other two.
IterateInfo golden_ipm_iter_nonfinite_newton() {
    IterateInfo r = golden_ipm_iter_present();
    r.iter_ = 4;
    r.h_facs_ = -1;
    r.h_pert_ = 0.0;
    r.h_pert_cum_ = 0.0;
    r.accepted_ = false;
    r.merit_val_ = 6.5;
    r.kkt_inf_ = 2.0;
    r.barr_inf_ = 1.5;
    r.econ_inf_ = 1.25;
    r.icon_inf_ = 0.75;
    return r;
}

TEST(JsonLinesTraceSink, GoldenLineIpmIterWithTheNonFiniteNewtonLadderMarker) {
    const IterateInfo r = golden_ipm_iter_nonfinite_newton();
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipm_iter(IpmIterTraceEvent{r, 2});
    EXPECT_EQ(os.str(),
              "{\"v\":0,\"ev\":\"ipm.iter\",\"seq\":1,\"depth\":0,\"iter\":4,"
              "\"mu\":0.10000000000000001,\"prim_obj\":-0.5,\"barr_obj\":-0.25,\"kkt_inf\":2,"
              "\"barr_inf\":1.5,\"econ_inf\":1.25,\"icon_inf\":0.75,\"pen_par1\":100,"
              "\"pen_par2\":1000,\"ls_iters\":1,\"alpha_p\":0.5,\"alpha_d\":0.25,"
              "\"alpha_t\":0.125,\"h_pert\":0,\"h_facs\":null,\"h_pert_cum\":0,"
              "\"prox_reg_primal\":9.9999999999999995e-07,"
              "\"prox_reg_dual\":1.9999999999999999e-06,\"p_pivots\":9,\"max_e_mult\":1000,"
              "\"max_i_mult\":10000,\"merit_val\":6.5,\"accepted\":false,"
              "\"first_rejection_iter\":0,\"theta_at_first_rejection\":0,\"eval_exceptions\":0,"
              "\"phase\":2}\n");
}

TEST(JsonLinesTraceSink, TheThreeIpmIterGoldenLinesIsolateEverySentinelSlot) {
    // FALSIFIABILITY, the T1 convention, over SIX null-bearing keys and THREE
    // records: every key reads `null` in at least one line and a number in at
    // least one other, so no predicate can be dropped without moving a line.
    const auto line = [](const IterateInfo &r, Index phase) {
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_ipm_iter(IpmIterTraceEvent{r, phase});
        return os.str();
    };
    const std::string a = line(golden_ipm_iter_sentinels(), 1);
    const std::string b = line(golden_ipm_iter_present(), 0);
    const std::string c = line(golden_ipm_iter_nonfinite_newton(), 2);
    EXPECT_NE(a, b);
    EXPECT_NE(b, c);
    EXPECT_NE(a, c);
    EXPECT_EQ(raw_field(a, "accepted"), "false");
    EXPECT_EQ(raw_field(b, "accepted"), "true");

    // The four `< 0` doubles/ints of the two -1 conventions: null in A only.
    for (const char *k :
         {"prox_reg_primal", "prox_reg_dual", "first_rejection_iter", "theta_at_first_rejection"}) {
        EXPECT_EQ(raw_field(a, k), "null") << k;
        EXPECT_NE(raw_field(b, k), "null") << k;
        EXPECT_NE(raw_field(c, k), "null") << k;
    }
    // The perturbed-pivot ABSENCE: null in A only, and NOT the projected 5.
    EXPECT_EQ(raw_field(a, "p_pivots"), "null");
    EXPECT_EQ(raw_field(b, "p_pivots"), "9");
    EXPECT_EQ(raw_field(c, "p_pivots"), "9");
    // The non-finite-Newton ladder marker: null in C only, and NOT -1.
    EXPECT_EQ(raw_field(c, "h_facs"), "null");
    EXPECT_EQ(raw_field(a, "h_facs"), "3");
    EXPECT_EQ(raw_field(b, "h_facs"), "2");
    EXPECT_EQ(a.find("-1"), std::string::npos) << "no sentinel escapes as a negative count";
    EXPECT_EQ(c.find("h_facs\":-1"), std::string::npos);
}

TEST(JsonLinesTraceSink, GoldenLineIpmSolveBegin) {
    IpmSolveBeginTraceEvent e;
    e.n = 15;
    e.n_reduced = 10;
    e.me = 6;
    e.mi = 7;
    e.vars_free = 1;
    e.vars_lower_only = 2;
    e.vars_upper_only = 3;
    e.vars_ranged = 4;
    e.vars_fixed = 5;
    e.phases = 9;
    e.max_iters = 200;
    e.max_acc_iters = 25;
    e.kkt_tol = 1e-6;
    e.econ_tol = 2e-6;
    e.icon_tol = 3e-6;
    e.bar_tol = 4e-6;
    e.init_mu = 0.001;
    e.obj_scale = 2.5;
    e.inertia_mode = InertiaModes::proximal_regularization;
    e.restoration_mode = RestorationModes::l1_nested;
    // RE-DERIVED AT M6 W5 T8.7, which appended EIGHT fields in one step so this
    // line moves once: the four ACCEPTABLE tolerances and `wide_console` (what
    // a sink needs to render the interior-point iteration table) and the three
    // `print_stats` inputs. Each carries its own value here, so a reorder or a
    // dropped key moves these bytes.
    e.acc_kkt_tol = 5e-6;
    e.acc_econ_tol = 6e-6;
    e.acc_icon_tol = 7e-6;
    e.acc_bar_tol = 8e-6;
    e.wide_console = true;
    e.kkt_dim = 23;
    e.kkt_nnz = 101;
    e.internal_fixed_rows = 3;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipm_solve_begin(e);
    EXPECT_EQ(os.str(),
              "{\"v\":0,\"ev\":\"ipm.solve.begin\",\"seq\":1,\"depth\":0,\"n\":15,\"n_reduc"
              "ed\":10,\"me\":6,\"mi\":7,\"vars_free\":1,\"vars_lower_only\":2,\"vars_upper"
              "_only\":3,\"vars_ranged\":4,\"vars_fixed\":5,\"phases\":9,\"max_iters\":200,"
              "\"max_acc_iters\":25,\"kkt_tol\":9.9999999999999995e-07,\"econ_tol\":1.99999"
              "99999999999e-06,\"icon_tol\":3.0000000000000001e-06,\"bar_tol\":3.9999999999"
              "999998e-06,\"init_mu\":0.001,\"obj_scale\":2.5,\"inertia_mode\":\"proximal_r"
              "egularization\",\"restoration_mode\":\"l1_nested\",\"acc_kkt_tol\":5.0000000"
              "000000004e-06,\"acc_econ_tol\":6.0000000000000002e-06,\"acc_icon_tol\":6.999"
              "9999999999999e-06,\"acc_bar_tol\":7.9999999999999996e-06,\"wide_console\":tr"
              "ue,\"kkt_dim\":23,\"kkt_nnz\":101,\"internal_fixed_rows\":3}\n");
    // The pair moves NO depth: this driver nests no driver of its own.
    EXPECT_EQ(sink.depth(), 0);
}

TEST(JsonLinesTraceSink, GoldenLineIpmSolveEnd) {
    IpmSolveEndTraceEvent e;
    e.status = hven::solvers::SolveStatus::kAcceptable;
    e.iters = 42;
    e.total_time_s = 1.5;
    e.pre_time_s = 0.25;
    e.func_time_s = 0.125;
    e.kkt_time_s = 0.0625;
    e.print_time_s = 0.03125;
    e.solver_init_time_s = 0.015625;
    e.misc_time_s = 1.015625;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipm_solve_end(e);
    EXPECT_EQ(os.str(),
              "{\"v\":0,\"ev\":\"ipm.solve.end\",\"seq\":1,\"depth\":0,\"status\":\"acceptable\","
              "\"iters\":42,\"total_time_s\":1.5,\"pre_time_s\":0.25,\"func_time_s\":0.125,"
              "\"kkt_time_s\":0.0625,\"print_time_s\":0.03125,\"solver_init_time_s\":0.015625,"
              "\"misc_time_s\":1.015625}\n");
    EXPECT_EQ(sink.depth(), 0);
}

TEST(JsonLinesTraceSink, GoldenLineIpmRestorationExitRow) {
    // M6 W5 T8.7's ADDED line. The row itself is NOT repeated here: the
    // adjacent `ipm.iter` line carries all 27 of its keys, and two copies of a
    // record in one stream is two things for a reader to reconcile. What this
    // line adds is the door's identity and the two numbers that decided it,
    // plus `iter`/`phase` so the pairing survives a filtered read.
    IterateInfo row;
    row.iter_ = 7;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipm_restoration_exit_row(IpmRestorationExitRowTraceEvent{row, 2, 1.5e-3, 1.0e-6});
    EXPECT_EQ(os.str(),
              "{\"v\":0,\"ev\":\"ipm.restoration_exit_row\",\"seq\":1,\"depth\":0,"
              "\"iter\":7,\"phase\":2,\"theta\":0.0015,\"threshold\":9.9999999999999995e-07}"
              "\n");
    // Like `ipm.iter`, this moves no depth.
    EXPECT_EQ(sink.depth(), 0);
}

TEST(JsonLinesTraceSink, TheRestorationExitRowIsAdjacentToItsOwnIterLine) {
    // THE WRITER'S HALF ONLY, and this test says so (M6 W5 T8.7 fix1, astra's
    // Minor): it invokes the two methods BY HAND, so it pins what the writer
    // does with that pair and nothing about whether the SOLVER still emits it.
    // The solver's half is
    // `IpmStopReason.TheDoorsMarkerFollowsItsOwnIterLineOnALiveSolve`
    // (tests/interior/test_ipm_stop_reason.cpp), which runs the solve that
    // opens the door and reads the stream it produced -- that one fails if the
    // marker emit is removed or reordered; this one cannot.
    //
    // THE ORDER THE ENGINE EMITS IN, pinned so a later change that separates
    // them has to say so: the row's `ipm.iter` line, then the marker, with the
    // same `iter` and `phase` on both.
    IterateInfo row;
    row.iter_ = 4;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipm_iter(IpmIterTraceEvent{row, 1});
    sink.on_ipm_restoration_exit_row(IpmRestorationExitRowTraceEvent{row, 1, 2.0, 1e-8});
    const std::vector<std::string> lines = split_lines(os.str());
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(event_name(lines[0]), "ipm.iter");
    EXPECT_EQ(event_name(lines[1]), "ipm.restoration_exit_row");
    EXPECT_EQ(raw_field(lines[0], "iter"), raw_field(lines[1], "iter"));
    EXPECT_EQ(raw_field(lines[0], "phase"), raw_field(lines[1], "phase"));
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
    // THE LABEL'S TRAILING SPACE IS PART OF THE VALUE: it is what the console
    // prints, and the JSON carries the engine's own `PhaseStep::label_`
    // verbatim rather than trimming a byte the transcript depends on.
    sink.on_ipm_phase_begin(IpmPhaseTraceEvent{3, "Solve Algorithm ", IpmPhase::kSolve});
    EXPECT_EQ(os.str(), "{\"v\":0,\"ev\":\"ipm.phase.begin\",\"seq\":1,\"depth\":0,\"phase\":3,"
                        "\"label\":\"Solve Algorithm \",\"entry\":\"solve\"}\n");
    // Like every other `ipm.*` line, this moves no depth.
    EXPECT_EQ(sink.depth(), 0);
}

TEST(JsonLinesTraceSink, GoldenLineIpmPhaseEnd) {
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipm_phase_end(IpmPhaseTraceEvent{0, "Optimization Algorithm ", IpmPhase::kOptimize});
    EXPECT_EQ(os.str(), "{\"v\":0,\"ev\":\"ipm.phase.end\",\"seq\":1,\"depth\":0,\"phase\":0,"
                        "\"label\":\"Optimization Algorithm \",\"entry\":\"optimize\"}\n");
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
    EXPECT_EQ(os.str(), "{\"v\":0,\"ev\":\"ipm.kkt_analysis\",\"seq\":1,\"depth\":0,\"kkt_dim\":23,"
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
    IterateInfo row;
    row.iter_ = 11;
    row.prim_obj_ = 17.25;
    row.kkt_inf_ = 0.00048828125;
    row.barr_inf_ = 0.000244140625;
    row.econ_inf_ = 0.0001220703125;
    row.icon_inf_ = 6.103515625e-05;
    IpmPhaseReport report;
    report.phase = IpmPhase::kOptimize;
    report.status = SolveStatus::kAcceptable;
    report.iterations = 12;
    report.phase_seconds = 0.25;
    report.stop_reason = IpmStopReason::kStageStalled;
    report.ran = true;
    IpmPhaseExitTraceEvent e{report, row};
    e.phase = 1;
    e.best_substituted = true;
    e.last_kkt_info = IpmKktFactorStatus::kNumericalIssue;
    e.total_s = 0.5;
    e.func_s = 0.125;
    e.kkt_s = 0.0625;
    e.print_s = 0.03125;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipm_phase_exit(e);
    EXPECT_EQ(os.str(),
              "{\"v\":0,\"ev\":\"ipm.phase.exit\",\"seq\":1,\"depth\":0,\"phase\":1,\"iter\":11,"
              "\"best_substituted\":true,\"prim_obj\":17.25,"
              "\"kkt_inf\":0.00048828125,\"barr_inf\":0.000244140625,"
              "\"econ_inf\":0.0001220703125,\"icon_inf\":6.103515625e-05,"
              "\"last_kkt_info\":\"numerical_issue\",\"total_s\":0.5,\"func_s\":0.125,"
              "\"kkt_s\":0.0625,\"print_s\":0.03125,\"entry\":\"optimize\","
              "\"status\":\"acceptable\",\"iterations\":12,\"phase_seconds\":0.25,"
              "\"stop_reason\":\"stage_stalled\",\"ran\":true}\n");
    EXPECT_EQ(sink.depth(), 0);
}

TEST(JsonLinesTraceSink, GoldenLineIpmMessage) {
    // THE KIND CHOSEN IS THE ONE THAT EXERCISES THE NULL SLOTS: two doubles
    // present, six integer slots absent. A flat `{a, b, k}` payload could not
    // carry `inertia_exhausted`, and the price of the wider one is exactly
    // these nulls -- which the golden shows rather than describes.
    IpmMessageTraceEvent m;
    m.kind = IpmMessageKind::kRestorationLocallyInfeasible;
    m.phase = 1;
    m.iter = 7;
    m.a = 0.0015;
    m.b = 1.0e-6;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipm_message(m);
    EXPECT_EQ(os.str(),
              "{\"v\":0,\"ev\":\"ipm.message\",\"seq\":1,\"depth\":0,"
              "\"kind\":\"restoration_locally_infeasible\",\"phase\":1,\"iter\":7,"
              "\"a\":0.0015,\"b\":9.9999999999999995e-07,\"k\":null,\"p\":null,\"n\":null,"
              "\"z\":null,\"expected_p\":null,\"expected_n\":null}\n");
    EXPECT_EQ(sink.depth(), 0);
}

TEST(JsonLinesTraceSink, TheInertiaMessageFillsEverySlotTheOtherKindsLeaveNull) {
    // THE OTHER HALF OF THE MESSAGE PAYLOAD, by FIELD rather than by a sixth
    // byte literal: the wire order is already pinned above, and what this adds
    // is that the one kind the wide payload exists for really does fill all six
    // integers -- and that the two double slots it does not use read `null`.
    IpmMessageTraceEvent m;
    m.kind = IpmMessageKind::kInertiaExhausted;
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
    sink.on_ipm_message(m);
    const std::string line = os.str();
    EXPECT_EQ(raw_field(line, "kind"), "\"inertia_exhausted\"");
    EXPECT_EQ(raw_field(line, "a"), "null");
    EXPECT_EQ(raw_field(line, "b"), "null");
    EXPECT_EQ(raw_field(line, "k"), "15");
    EXPECT_EQ(raw_field(line, "p"), "3");
    EXPECT_EQ(raw_field(line, "n"), "2");
    EXPECT_EQ(raw_field(line, "z"), "1");
    EXPECT_EQ(raw_field(line, "expected_p"), "5");
    EXPECT_EQ(raw_field(line, "expected_n"), "2");

    // And the kind with NO payload at all reports every slot absent, phase and
    // iteration included -- `solver_initialized` fires before any phase begins.
    IpmMessageTraceEvent init;
    init.kind = IpmMessageKind::kSolverInitialized;
    init.a = 627.5;
    std::ostringstream os2;
    JsonLinesTraceSink sink2(os2);
    sink2.on_ipm_message(init);
    EXPECT_EQ(raw_field(os2.str(), "phase"), "null");
    EXPECT_EQ(raw_field(os2.str(), "iter"), "null");
    EXPECT_EQ(raw_field(os2.str(), "a"), "627.5");
    EXPECT_EQ(raw_field(os2.str(), "b"), "null");
    EXPECT_EQ(raw_field(os2.str(), "k"), "null");
}

TEST(JsonLinesTraceSink, TheIpmPairMovesNoDepthAndDoesNotDisturbTheSqpNesting) {
    // The two engines share one sink. An `ipm.solve` pair written between an
    // SQP pair's begin and end must leave the SQP nesting exactly as it found
    // it -- so it neither pushes nor pops.
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    const SqpCounters counters;
    sink.on_sqp_solve_begin(SqpSolveBeginTraceEvent{});
    sink.on_ipm_solve_begin(IpmSolveBeginTraceEvent{});
    sink.on_ipm_iter(IpmIterTraceEvent{golden_ipm_iter_sentinels(), 0});
    sink.on_ipm_solve_end(IpmSolveEndTraceEvent{});
    EXPECT_EQ(sink.depth(), 0);
    sink.on_sqp_solve_end(SqpSolveEndTraceEvent{SolveStatus::kOptimal, 0, counters});
    EXPECT_EQ(sink.depth(), 0);
    for (const std::string &l : split_lines(os.str())) {
        EXPECT_EQ(raw_field(l, "depth"), "0") << l;
    }
    EXPECT_EQ(sink.lines_written(), 5);
}

TEST(JsonLinesTraceSink, NoIpmLineCarriesAnUnknownEnumString) {
    // The production `to_json` has no `default` label and this tree has no
    // `-Werror`, so `"unknown"` is reachable from a library-only build. Here it
    // never is.
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipm_solve_begin(IpmSolveBeginTraceEvent{});
    sink.on_ipm_iter(IpmIterTraceEvent{golden_ipm_iter_present(), 0});
    sink.on_ipm_iter(IpmIterTraceEvent{golden_ipm_iter_nonfinite_newton(), 0});
    // M6 W5 T8.7b's five records, every alphabet on them exercised at BOTH
    // ends of its range so a missing `to_json` case cannot hide behind a
    // default-constructed value.
    const IterateInfo row;
    IpmPhaseReport report;
    sink.on_ipm_phase_begin(IpmPhaseTraceEvent{0, "Optimization Algorithm ", IpmPhase::kOptimize});
    sink.on_ipm_kkt_analysis(IpmKktAnalysisTraceEvent{});
    for (const IpmStopReason reason : {IpmStopReason::kNone, IpmStopReason::kIterationCap,
                                       IpmStopReason::kRestorationLocallyInfeasible,
                                       IpmStopReason::kStageStalled, IpmStopReason::kInterrupted}) {
        report.stop_reason = reason;
        for (const SolveStatus st :
             {SolveStatus::kOptimal, SolveStatus::kAcceptable, SolveStatus::kMaxIter,
              SolveStatus::kInfeasible, SolveStatus::kStalled, SolveStatus::kDiverging,
              SolveStatus::kNumericalError, SolveStatus::kBudgetExhausted,
              SolveStatus::kInterrupted}) {
            report.status = st;
            for (const IpmKktFactorStatus fs :
                 {IpmKktFactorStatus::kSuccess, IpmKktFactorStatus::kNumericalIssue,
                  IpmKktFactorStatus::kNoConvergence, IpmKktFactorStatus::kInvalidInput}) {
                IpmPhaseExitTraceEvent e{report, row};
                e.last_kkt_info = fs;
                sink.on_ipm_phase_exit(e);
            }
        }
    }
    for (const IpmMessageKind kind :
         {IpmMessageKind::kSolverInitialized, IpmMessageKind::kRankDeficiency,
          IpmMessageKind::kFactorizationHardError, IpmMessageKind::kInertiaExhausted,
          IpmMessageKind::kRestorationLocallyInfeasible, IpmMessageKind::kFeasibilityStall,
          IpmMessageKind::kInterruptAtIteration, IpmMessageKind::kPhaseDiverged,
          IpmMessageKind::kInterruptSkippingPhases}) {
        IpmMessageTraceEvent m;
        m.kind = kind;
        sink.on_ipm_message(m);
    }
    sink.on_ipm_phase_end(IpmPhaseTraceEvent{1, "Solve Algorithm ", IpmPhase::kSolve});
    sink.on_ipm_solve_end(IpmSolveEndTraceEvent{});
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
/// test to hand it. `hven::testing::StdoutCapture` (tests/common_support/) is that
/// redirection, in one portable place.
using hven::testing::StdoutCapture;

/// Counts every event, by name, without rendering anything.
struct EventTally : TraceSink {
    std::vector<std::string> seen;
    void on_ipqp_iter(const IpqpTraceIterEvent &) override { seen.push_back("ipqp.iter"); }
    void on_ipqp_reg(const IpqpTraceRegEvent &) override { seen.push_back("ipqp.reg"); }
    void on_ipqp_restart(const IpqpTraceRestartEvent &) override { seen.push_back("ipqp.restart"); }
    void on_ipqp_route(const IpqpTraceRouteEvent &) override { seen.push_back("ipqp.route"); }
    void on_ipqp_certify(const IpqpTraceCertifyEvent &) override { seen.push_back("ipqp.certify"); }
    void on_ipqp_escape(const IpqpTraceEscapeEvent &) override { seen.push_back("ipqp.escape"); }
    void on_qp_mode(const QpModeTraceEvent &) override { seen.push_back("qp.mode"); }
    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &) override {
        seen.push_back("fallback.verdict");
    }
    void on_sqp_major(const SqpMajorTraceEvent &) override { seen.push_back("sqp.major"); }
    void on_sqp_solve_begin(const SqpSolveBeginTraceEvent &) override {
        seen.push_back("sqp.solve.begin");
    }
    void on_sqp_solve_end(const SqpSolveEndTraceEvent &) override {
        seen.push_back("sqp.solve.end");
    }
};

} // namespace

TEST(SqpConsole, TheUsersJsonStreamIsByteIdenticalWithAndWithoutTheConsole) {
    // THE INVARIANT THE FAN-OUT EXISTS FOR, on the fixture that exercises it
    // hardest: this model ENTERS RESTORATION, so the stream carries depth-1
    // lines from the sub-driver. Handing the sub-driver the caller's half
    // instead of the fan-out -- or handing it the console -- moves those lines.
    auto run = [](int print_level) {
        CircleAndFarLineModel model;
        SqpOptions opts;
        opts.max_iter = 200;
        opts.common.print_level = print_level;
        SqpDriver driver(opts);
        std::ostringstream os;
        JsonLinesTraceSink json(os);
        driver.attach_trace(&json);
        StdoutCapture capture;
        EXPECT_TRUE(capture.active());
        const SqpSolution sol = driver.solve(model);
        return std::pair<std::string, Index>{os.str(), sol.counters.restoration_iters};
    };
    const auto silent = run(3);
    const auto printing = run(0);
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

TEST(SqpConsole, TheLiveConsoleEqualsFormatIterationTable) {
    // THE SQP's LIVE BYTE-PIN. Everything the console wrote during the solve,
    // against the function it is pinned to, on the returned solution.
    CircleAndFarLineModel model;
    SqpOptions opts;
    opts.max_iter = 200;
    opts.common.print_level = 0;
    SqpDriver driver(opts);
    std::string written;
    SqpSolution sol;
    {
        StdoutCapture capture;
        EXPECT_TRUE(capture.active());
        sol = driver.solve(model);
        written = capture.text();
    }
    ASSERT_GE(sol.counters.restoration_iters, 1) << "fixture premise: restoration runs";
    ASSERT_FALSE(sol.history.empty());
    EXPECT_EQ(written, format_iteration_table(sol));
}

TEST(SqpConsole, TheSqpDefaultPrintLevelWritesNothing) {
    // BEHAVIOUR CHANGE (11) IS A GAIN, NOT A CHANGE. `CommonOptions`'
    // shipped `print_level` is 3, and at 3 this driver writes exactly what it
    // wrote before this task: nothing.
    HsProblem p = make_hs(38);
    SqpOptions opts;
    opts.max_iter = 60;
    SqpDriver driver(opts);
    std::string written;
    {
        StdoutCapture capture;
        EXPECT_TRUE(capture.active());
        const SqpSolution sol = driver.solve(*p.model);
        written = capture.text();
        EXPECT_FALSE(sol.history.empty());
    }
    EXPECT_EQ(written, "");
}

TEST(SqpConsole, ACountingSinkSeesTheSameEventsWithPrintingOnAndOff) {
    auto run = [](int print_level, bool attach_first) {
        HsProblem p = make_hs(38);
        SqpOptions opts;
        opts.qp_mode = QpMode::kIpm;
        opts.max_iter = 60;
        opts.common.print_level = print_level;
        EventTally tally;
        // ATTACH ORDER IS FREE, and this is where that is proved: the sink is
        // attached before or after `set_options` and the composition happens at
        // solve entry either way.
        SqpDriver driver(attach_first ? opts : SqpOptions{});
        if (attach_first) {
            driver.attach_trace(&tally);
        } else {
            driver.attach_trace(&tally);
            driver.set_options(opts);
        }
        StdoutCapture capture;
        EXPECT_TRUE(capture.active());
        driver.solve(*p.model);
        return tally.seen;
    };
    const std::vector<std::string> off = run(3, true);
    const std::vector<std::string> on = run(0, true);
    const std::vector<std::string> on_late = run(0, false);
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
    // restoration at `print_level 0`, exactly ONE table is written -- the
    // parent's. A sub-driver that built a console of its own would write a
    // second header and its own rows into the same `stdout`.
    CircleAndFarLineModel model;
    SqpOptions opts;
    opts.max_iter = 200;
    opts.common.print_level = 0;
    SqpDriver driver(opts);
    std::string written;
    SqpSolution sol;
    {
        StdoutCapture capture;
        EXPECT_TRUE(capture.active());
        sol = driver.solve(model);
        written = capture.text();
    }
    ASSERT_GE(sol.counters.restoration_iters, 1) << "fixture premise: restoration runs";

    const std::string head = sqp_iteration_table_head();
    ASSERT_FALSE(head.empty());
    const std::string first_line = head.substr(0, head.find('\n'));
    Index headers = 0;
    for (std::size_t at = written.find(first_line); at != std::string::npos;
         at = written.find(first_line, at + 1)) {
        ++headers;
    }
    EXPECT_EQ(headers, 1) << "the restoration sub-driver wrote a table of its own";
    // And the one table is the parent's whole output, byte for byte -- the same
    // statement `TheLiveConsoleEqualsFormatIterationTable` makes, kept here so
    // this test fails for the right reason if the sub-driver ever prints.
    EXPECT_EQ(written, format_iteration_table(sol));
}

TEST(SqpConsole, AttachTraceDuringASolveIsRefused) {
    // THE IN-FLIGHT RULE `set_options` uses, extended to the sink (M6 W5 T8.7):
    // the effective sink is fixed for the solve, so a mid-solve change could
    // not take effect in it. The refusal says so.
    HsProblem p = make_hs(38);
    SqpOptions opts;
    opts.max_iter = 60;
    SqpDriver driver(opts);
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
    driver.solve(*p.model);
    EXPECT_TRUE(threw) << "attach_trace inside a solve must be refused";
    // ... and it is legal again once the solve has returned.
    EXPECT_NO_THROW(driver.attach_trace(&json));
}

} // namespace
} // namespace hven::solvers
