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

const char *spec_spelling(SqpStatus v) {
    switch (v) {
    case SqpStatus::kOptimal:
        return "\"optimal\"";
    case SqpStatus::kMaxIter:
        return "\"max_iter\"";
    case SqpStatus::kInfeasible:
        return "\"infeasible\"";
    case SqpStatus::kNumericalError:
        return "\"numerical_error\"";
    case SqpStatus::kBudgetExhausted:
        return "\"budget_exhausted\"";
    }
    return kUnspelled;
}

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
    static_assert(static_cast<int>(SqpStatus::kBudgetExhausted) == 5 - 1, "5 solve statuses");
    static_assert(static_cast<int>(WorkingSetLinearAlgebra::kSchurBorder) == 2 - 1,
                  "2 working-set algebras");
    static_assert(static_cast<int>(StartLevel::kHot) == 4 - 1, "4 start levels");
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
    const auto solve_status = [](SqpStatus st) {
        const SqpCounters counters;
        std::ostringstream os;
        JsonLinesTraceSink s(os);
        s.on_sqp_solve_end(SqpSolveEndTraceEvent{st, 0, counters});
        EXPECT_EQ(raw_field(os.str(), "status"), spec_spelling(st));
    };
    solve_status(SqpStatus::kOptimal);
    solve_status(SqpStatus::kMaxIter);
    solve_status(SqpStatus::kInfeasible);
    solve_status(SqpStatus::kNumericalError);
    solve_status(SqpStatus::kBudgetExhausted);

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
        s.on_sqp_solve_end(SqpSolveEndTraceEvent{SqpStatus::kOptimal, 0, counters});
        EXPECT_EQ(raw_field(os.str(), "start_level_used"), spec_spelling(lv));
    };
    start_level(StartLevel::kCold);
    start_level(StartLevel::kSeeded);
    start_level(StartLevel::kWarm);
    start_level(StartLevel::kHot);
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
    sink.on_sqp_solve_end(SqpSolveEndTraceEvent{SqpStatus::kOptimal, 0, counters});
    sink.on_ipqp_certify(IpqpTraceCertifyEvent{});
    sink.on_sqp_solve_end(SqpSolveEndTraceEvent{SqpStatus::kOptimal, 0, counters});
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
    sink.on_sqp_solve_end(SqpSolveEndTraceEvent{SqpStatus::kOptimal, 0, counters});
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

/// @brief A model whose variable 0 has a zero-width box.
///
/// BEHAVIOUR-FAITHFUL FOR THE PINNED CASE, from `PinnedVariableModel` in
/// tests/sqp/test_ipqp_dispatch.cpp:156 (which pins the decline itself). Not a
/// byte copy: the origin's `pin` constructor flag and its unpinned box are
/// dropped, since only the pinned case is wanted here. It is here because it is
/// the only in-tree population with `ipqp_declined_pinned > 0`, and that is the
/// term the entry identity subtracts. Neither that test nor its fixture was
/// modified.
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
// `w2_box_blocked_qp` (:8536) and `w2_antiparallel_eq_qp` (:9023) are file-local
// there and are copied here with BYTE-IDENTICAL bodies; that test and its
// fixture are not touched at all.
//
// `w2_escaped_evidence` is ADAPTED from `w2_escaped` (:8631), not copied:
// renamed, narrowed to the evidence block, and with the radius fixed at +inf
// rather than defaulted.

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
              "{\"v\":0,\"ev\":\"sqp.major\",\"seq\":1,\"depth\":0,\"trial\":4,\"f\":-2.5,\"station"
              "arity\":9.9999999999999995e-08,\"feasibility\":0.25,\"complementarity\":0.125,\"kkt_"
              "residual\":0.5,\"violation_l1\":1.5,\"tr_radius\":2,\"mu\":1e-08,\"step_norm\":0.75,"
              "\"qp_solved\":false,\"ipqp_least_infeasible_primal\":3.25,\"ipqp_farkas_corroborated"
              "\":true,\"qp_status\":\"max_iter\",\"qp_minor_iters\":9,\"qp_factorizations\":3,\"tr"
              "_binding\":false,\"verdict\":\"accept_f\",\"soc_applied\":true,\"elastic_applied\":f"
              "alse,\"elastic_rho0_ceiling_hit\":true,\"restoration_seed_used\":false,\"watchdog_re"
              "stored\":true,\"major\":4,\"mode\":\"walk\"}\n");
}

TEST(JsonLinesTraceSink, GoldenLineSqpMajorSecondBoolSignatureTheHardDoublesAndTheSsnMode) {
    EXPECT_EQ(major_line(golden_major_b(), 0, IpqpTraceQpMode::kSsn),
              "{\"v\":0,\"ev\":\"sqp.major\",\"seq\":1,\"depth\":0,\"trial\":0,\"f\":0.100000000000"
              "00001,\"stationarity\":-0,\"feasibility\":\"nan\",\"complementarity\":\"inf\",\"kkt_"
              "residual\":\"-inf\",\"violation_l1\":1e-300,\"tr_radius\":1.7976931348623157e+308,\""
              "mu\":4.9406564584124654e-324,\"step_norm\":0,\"qp_solved\":false,\"ipqp_least_infeas"
              "ible_primal\":0,\"ipqp_farkas_corroborated\":false,\"qp_status\":\"infeasible\",\"qp"
              "_minor_iters\":0,\"qp_factorizations\":0,\"tr_binding\":true,\"verdict\":\"restore\""
              ",\"soc_applied\":true,\"elastic_applied\":false,\"elastic_rho0_ceiling_hit\":false,"
              "\"restoration_seed_used\":true,\"watchdog_restored\":true,\"major\":0,\"mode\":\"ssn"
              "\"}\n");
}

TEST(JsonLinesTraceSink, GoldenLineSqpMajorThirdBoolSignatureAndTheIpqpMode) {
    EXPECT_EQ(major_line(golden_major_c(), 11, IpqpTraceQpMode::kIpqp),
              "{\"v\":0,\"ev\":\"sqp.major\",\"seq\":1,\"depth\":0,\"trial\":11,\"f\":1234.5,\"stat"
              "ionarity\":9.9999999999999998e-13,\"feasibility\":6.25,\"complementarity\":7.5,\"kkt"
              "_residual\":8.75,\"violation_l1\":9,\"tr_radius\":0.03125,\"mu\":9.9999999999999995e"
              "-07,\"step_norm\":12.5,\"qp_solved\":false,\"ipqp_least_infeasible_primal\":-1.5,\"i"
              "pqp_farkas_corroborated\":false,\"qp_status\":\"numerical_error\",\"qp_minor_iters\""
              ":21,\"qp_factorizations\":13,\"tr_binding\":false,\"verdict\":\"accept_h\",\"soc_app"
              "lied\":false,\"elastic_applied\":true,\"elastic_rho0_ceiling_hit\":true,\"restoratio"
              "n_seed_used\":true,\"watchdog_restored\":true,\"major\":11,\"mode\":\"ipqp\"}\n");
}

TEST(JsonLinesTraceSink, GoldenLineSqpMajorEveryBoolTrue) {
    EXPECT_EQ(
        major_line(golden_major_d(), 2, IpqpTraceQpMode::kWalk),
        "{\"v\":0,\"ev\":\"sqp.major\",\"seq\":1,\"depth\":0,\"trial\":2,\"f\":3,\"stationari"
        "ty\":4,\"feasibility\":5,\"complementarity\":6,\"kkt_residual\":7,\"violation_l1\":8"
        ",\"tr_radius\":9,\"mu\":10,\"step_norm\":11,\"qp_solved\":true,\"ipqp_least_infeasib"
        "le_primal\":12,\"ipqp_farkas_corroborated\":true,\"qp_status\":\"optimal\",\"qp_mino"
        "r_iters\":13,\"qp_factorizations\":14,\"tr_binding\":true,\"verdict\":\"reject\",\"s"
        "oc_applied\":true,\"elastic_applied\":true,\"elastic_rho0_ceiling_hit\":true,\"resto"
        "ration_seed_used\":true,\"watchdog_restored\":true,\"major\":2,\"mode\":\"walk\"}\n");
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
    static_assert(::hven::detail::kAggregateArity<SqpIterate> == 23,
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
    // dispatch's walk site never runs and writes no line.
    //
    // That is a real gap in the stream (registered for T5's doc), and it is
    // pinned here rather than papered over.
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
        EXPECT_EQ(at("\"walk\""), 0) << "the fallback's own walk is not a dispatch invocation";
        EXPECT_EQ(at("\"ssn\""), 0) << "the kSsn ARM did not run; the warm grade is not it";
    }
    {
        PinnedVariableModel model;
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
    EXPECT_EQ(kSqpCountersFieldCount, 33u);
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
    EXPECT_EQ(lines_inside,
              static_cast<Index>(first.history.size() + second.history.size() +
                                 first.counters.major_iters + second.counters.major_iters))
        << "one row and one walk qp.mode line per major, and nothing else on a walk cell";
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
        SqpStatus status;
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
        ASSERT_EQ(sol.status, SqpStatus::kOptimal);
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
        ASSERT_EQ(sol.status, SqpStatus::kMaxIter);
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
        ASSERT_EQ(sol.status, SqpStatus::kInfeasible);
        EXPECT_EQ(raw_field(only_line(os.str(), "sqp.solve.end"), "status"), "\"infeasible\"");
        EXPECT_EQ(json.depth(), 0);
    }
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

    ASSERT_EQ(sol.status, SqpStatus::kInfeasible);
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
    // THE 90 COUNTERS IN TABLE ORDER, each carrying its own number, so a
    // reorder, a duplicate or a dropped entry moves these bytes.
    //
    // Declared re-derivable while v0 is open (plan section 2 rule 6).
    const SqpCounters c = distinct_counters();
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_sqp_solve_end(SqpSolveEndTraceEvent{SqpStatus::kBudgetExhausted, 42, c});
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
              "d_bounds\":30,\"n_seeded\":31,\"seeded_clamped\":32,\"ip_activity_inferred\":33,\"ss"
              "n\":{\"ssn_iters\":34,\"ssn_bulk_flips\":35,\"ssn_backtracks\":36,\"ssn_prox_updates"
              "\":37,\"ssn_escapes\":38,\"ssn_uncertain_peak\":39,\"ssn_refinements\":40,\"ssn_refi"
              "ne_refused\":41,\"ssn_refine_factorizations\":42,\"ssn_refine_neg_duals\":43,\"ssn_s"
              "ign_swept\":44,\"ssn_sign_sweep_max\":45,\"ssn_escape_budget\":46,\"ssn_escape_singu"
              "lar\":47,\"ssn_escape_no_contraction\":48,\"ssn_escape_infeasible_suspect\":49,\"ssn"
              "_escape_indefinite\":50,\"ssn_escape_gate_refused\":51},\"ipqp\":{\"ipqp_iters\":52,"
              "\"ipqp_factorizations\":53,\"ipqp_symbolic_analyses\":54,\"ipqp_solves\":55,\"ipqp_p"
              "attern_verifies\":56,\"ipqp_rho_demanded_max\":57,\"ipqp_rho_demanded_last\":58,\"ip"
              "qp_inertia_retries\":59,\"ipqp_iters_at_elevated_rho\":60,\"ipqp_ladder_reclimbs\":6"
              "1,\"ipqp_pivot_reroute_primal\":62,\"ipqp_pivot_reroute_dual_fallback\":63,\"ipqp_it"
              "ers_ladder_armed_no_advance\":64,\"ipqp_final_inertia_read\":65,\"ipqp_reg_decreases"
              "\":66,\"ipqp_reg_increases\":67,\"ipqp_prox_center_updates\":68,\"ipqp_restart_repai"
              "rs\":69,\"ipqp_restart_shift_max\":70,\"ipqp_mu_adopted\":71,\"ipqp_warm_restart_aba"
              "ndoned\":72,\"ipqp_declined_pinned\":73,\"ipqp_tier_retired_after\":74,\"ipqp_face_u"
              "ncertain\":75,\"ipqp_refine_accepted\":76,\"ipqp_refine_refused\":77,\"ipqp_to_refin"
              "e\":78,\"ipqp_to_ssn\":79,\"ipqp_to_walk\":80,\"ipqp_escapes\":81,\"ipqp_escape_budg"
              "et\":82,\"ipqp_escape_stall\":83,\"ipqp_escape_indefinite\":84,\"ipqp_escape_numeric"
              "al\":85,\"ipqp_escape_infeasible_suspect\":86,\"ipqp_alpha_p_min\":87,\"ipqp_alpha_d"
              "_min\":88,\"ipqp_read_kept_tight_sides\":89,\"ipqp_read_barrier_noise_sides\":90}}}"
              "\n");
}

TEST(JsonLinesTraceSink, GoldenLineSqpSolveEndWritesTheAbsenceSentinelsAsNull) {
    // THE OTHER HALF OF RULE 5 (fix round 1, R7): a default-constructed
    // `SqpCounters` holds the three documented absence sentinels.
    //
    // `ipqp_alpha_p_min` and `ipqp_alpha_d_min` at `+infinity` ("no step
    // observed yet") and `ipqp_tier_retired_after` at 0 ("never retired") must
    // every one read `null`, not a value.
    const SqpCounters c;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_sqp_solve_end(SqpSolveEndTraceEvent{SqpStatus::kNumericalError, 0, c});
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
              "ded\":0,\"seeded_clamped\":0,\"ip_activity_inferred\":0,\"ssn\":{\"ssn_iters\":0,\"s"
              "sn_bulk_flips\":0,\"ssn_backtracks\":0,\"ssn_prox_updates\":0,\"ssn_escapes\":0,\"ss"
              "n_uncertain_peak\":0,\"ssn_refinements\":0,\"ssn_refine_refused\":0,\"ssn_refine_fac"
              "torizations\":0,\"ssn_refine_neg_duals\":0,\"ssn_sign_swept\":0,\"ssn_sign_sweep_max"
              "\":0,\"ssn_escape_budget\":0,\"ssn_escape_singular\":0,\"ssn_escape_no_contraction\""
              ":0,\"ssn_escape_infeasible_suspect\":0,\"ssn_escape_indefinite\":0,\"ssn_escape_gate"
              "_refused\":0},\"ipqp\":{\"ipqp_iters\":0,\"ipqp_factorizations\":0,\"ipqp_symbolic_a"
              "nalyses\":0,\"ipqp_solves\":0,\"ipqp_pattern_verifies\":0,\"ipqp_rho_demanded_max\":"
              "0,\"ipqp_rho_demanded_last\":0,\"ipqp_inertia_retries\":0,\"ipqp_iters_at_elevated_r"
              "ho\":0,\"ipqp_ladder_reclimbs\":0,\"ipqp_pivot_reroute_primal\":0,\"ipqp_pivot_rerou"
              "te_dual_fallback\":0,\"ipqp_iters_ladder_armed_no_advance\":0,\"ipqp_final_inertia_r"
              "ead\":0,\"ipqp_reg_decreases\":0,\"ipqp_reg_increases\":0,\"ipqp_prox_center_updates"
              "\":0,\"ipqp_restart_repairs\":0,\"ipqp_restart_shift_max\":0,\"ipqp_mu_adopted\":0,"
              "\"ipqp_warm_restart_abandoned\":0,\"ipqp_declined_pinned\":0,\"ipqp_tier_retired_aft"
              "er\":null,\"ipqp_face_uncertain\":0,\"ipqp_refine_accepted\":0,\"ipqp_refine_refused"
              "\":0,\"ipqp_to_refine\":0,\"ipqp_to_ssn\":0,\"ipqp_to_walk\":0,\"ipqp_escapes\":0,\""
              "ipqp_escape_budget\":0,\"ipqp_escape_stall\":0,\"ipqp_escape_indefinite\":0,\"ipqp_e"
              "scape_numerical\":0,\"ipqp_escape_infeasible_suspect\":0,\"ipqp_alpha_p_min\":null,"
              "\"ipqp_alpha_d_min\":null,\"ipqp_read_kept_tight_sides\":0,\"ipqp_read_barrier_noise"
              "_sides\":0}}}\n");
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
    sink.on_sqp_solve_end(SqpSolveEndTraceEvent{SqpStatus::kOptimal, 0, c});
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
    EXPECT_EQ(qp_mode_census(os.str()).count("\"ssn\""), 0u)
        << "and no ssn dispatch record exists: the two readings are different questions";
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

} // namespace
} // namespace hven::solvers
