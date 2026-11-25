// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// SqpSolver::solve's refusal of a corrupt "hven.ipm.polish.v1" extension at ingest. The warm
// start is produced by a real interior-point solve (so every block is well formed), then ONE thing
// is broken per case, and the solve must refuse BEFORE iterating, with a message that names the
// staging call and the broken block. A corrupt payload that were absorbed would seed the
// crossover with prices it cannot judge, so each class is pinned separately:
//
//   * the extension appears twice (ambiguous: neither is arbitrated);
//   * its bytes do not decode;
//   * each of the three blocks has the wrong length against the core beside it;
//   * a non-finite entry in each of the three blocks;
//   * a negative price in either bound block.
//
// The intact value is solved from as the control, so a refusal cannot be a side effect of the
// harness.

#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/warmstart/warm_start_access.h>
#include <hven/ipm/ipm_solver.h>
#include <hven/ipm/ipm_solver_types.h>
#include <hven/model/problem.h>
#include <hven/sqp/sqp_solver.h>
#include <hven/sqp/sqp_solver_types.h>
#include <hven/warmstart/ipm_polish_extension.h>

#include "model/support/hs071_problem.h"

namespace {

using hven::IpmOptions;
using hven::IpmPolishData;
using hven::IpmSolver;
using hven::Problem;
using hven::SolveStatus;
using hven::SqpOptions;
using hven::SqpSolver;
using hven::Verbosity;
using hven::WarmStart;
using hven::detail::WarmStartAccess;
using hven_model_tests::hs071_start;
using hven_model_tests::Hs071Problem;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

WarmStart exported_value() {
    Problem problem(std::make_shared<Hs071Problem>());
    IpmOptions o;
    o.common.verbosity = Verbosity::kSilent;
    IpmSolver solver(o);
    const auto r = solver.solve(problem, hs071_start());
    EXPECT_EQ(r.status, SolveStatus::kOptimal);
    return r.warm_start;
}

// The refusal message of an SQP solve from `warm`, or "" when it was accepted.
std::string sqp_refusal(const WarmStart &warm) {
    Problem problem(std::make_shared<Hs071Problem>());
    SqpOptions o;
    o.common.verbosity = Verbosity::kSilent;
    SqpSolver solver(o);
    try {
        (void)solver.solve(problem, hs071_start(), warm);
    } catch (const std::invalid_argument &e) {
        return e.what();
    }
    return "";
}

// Decode the extension, let `edit` break it, write it back.
template <class Edit> WarmStart with_edited_polish(Edit &&edit) {
    WarmStart w = exported_value();
    auto &exts = WarmStartAccess::extensions(w);
    EXPECT_EQ(exts.size(), 1u);
    IpmPolishData d = hven::deserialize_ipm_polish(exts.at(0).payload);
    edit(d);
    exts.at(0).payload = hven::serialize_ipm_polish(d);
    return w;
}

bool has(const std::string &hay, const std::string &needle) {
    return hay.find(needle) != std::string::npos;
}

TEST(PolishIngestRefusals, TheIntactValueIsAcceptedAsTheControl) {
    EXPECT_EQ(sqp_refusal(exported_value()), "");
}

TEST(PolishIngestRefusals, ADuplicatedExtensionIsRefusedNotArbitrated) {
    WarmStart w = exported_value();
    auto &exts = WarmStartAccess::extensions(w);
    ASSERT_EQ(exts.size(), 1u);
    exts.push_back(exts.front());
    const std::string msg = sqp_refusal(w);
    EXPECT_TRUE(has(msg, "SqpSolver::solve: the warm-start payload's extension list could not be "
                         "read"))
        << msg;
    EXPECT_TRUE(has(msg, "more than once")) << msg;
}

TEST(PolishIngestRefusals, AnUndecodablePayloadIsRefusedNamingTheTag) {
    WarmStart w = exported_value();
    auto &payload = WarmStartAccess::extensions(w).at(0).payload;
    ASSERT_GT(payload.size(), 8u);
    payload.resize(payload.size() / 2); // truncated mid-block
    const std::string msg = sqp_refusal(w);
    EXPECT_TRUE(has(msg, "extension whose payload could not be read")) << msg;
    EXPECT_TRUE(has(msg, std::string(hven::kIpmPolishTag))) << msg;
}

TEST(PolishIngestRefusals, ABlockOfTheWrongLengthIsRefusedAgainstTheCore) {
    struct Case {
        const char *block;
        void (*edit)(IpmPolishData &);
    };
    const Case cases[] = {
        {"the lower-bound multiplier block",
         [](IpmPolishData &d) { d.z_lower.conservativeResize(d.z_lower.size() + 1); }},
        {"the upper-bound multiplier block",
         [](IpmPolishData &d) { d.z_upper.conservativeResize(d.z_upper.size() + 1); }},
        {"the inequality-value block",
         [](IpmPolishData &d) { d.iq_values.conservativeResize(d.iq_values.size() + 1); }},
    };
    for (const Case &c : cases) {
        SCOPED_TRACE(c.block);
        // conservativeResize leaves the new entry uninitialised: zero it so the only defect is
        // the length.
        const WarmStart w = with_edited_polish([&](IpmPolishData &d) {
            c.edit(d);
            d.z_lower.tail(1).setZero();
            d.z_upper.tail(1).setZero();
            d.iq_values.tail(1).setZero();
        });
        const std::string msg = sqp_refusal(w);
        EXPECT_TRUE(has(msg, std::string("extension holds ")) && has(msg, c.block)) << msg;
        EXPECT_TRUE(has(msg, "over the same DECLARED problem")) << msg;
    }
}

TEST(PolishIngestRefusals, ANonFiniteEntryIsRefusedInEachBlock) {
    struct Case {
        const char *block;
        void (*edit)(IpmPolishData &);
    };
    const Case cases[] = {
        {"the lower-bound multiplier block", [](IpmPolishData &d) { d.z_lower(0) = kNaN; }},
        {"the upper-bound multiplier block", [](IpmPolishData &d) { d.z_upper(0) = kNaN; }},
        {"the inequality-value block", [](IpmPolishData &d) { d.iq_values(0) = kNaN; }},
    };
    for (const Case &c : cases) {
        SCOPED_TRACE(c.block);
        const std::string msg = sqp_refusal(with_edited_polish(c.edit));
        EXPECT_TRUE(has(msg, "holds a non-finite value in") && has(msg, c.block)) << msg;
    }
}

TEST(PolishIngestRefusals, ANegativePriceIsRefusedInEitherBoundBlock) {
    const std::string lower = sqp_refusal(with_edited_polish([](IpmPolishData &d) {
        d.z_lower(0) = -1.0;
    }));
    EXPECT_FALSE(lower.empty());
    EXPECT_TRUE(has(lower, "the lower-bound multiplier block")) << lower;
    const std::string upper = sqp_refusal(with_edited_polish([](IpmPolishData &d) {
        d.z_upper(1) = -1.0;
    }));
    EXPECT_FALSE(upper.empty());
    EXPECT_TRUE(has(upper, "the upper-bound multiplier block")) << upper;
}

// WarmStart::lambda re-reads the multiplier blocks against the problem's row counts: a value
// whose blocks were resized after export (a corrupt or hand-edited one) must be refused with the
// counts named, never mapped through compose_rows at the wrong width.
TEST(PolishIngestRefusals, LambdaRefusesMultiplierBlocksOfTheWrongWidth) {
    Problem problem(std::make_shared<Hs071Problem>());
    const WarmStart intact = exported_value();
    EXPECT_NO_THROW((void)intact.lambda(problem));
    EXPECT_EQ(intact.lambda(problem).size(), problem.num_constraints());

    WarmStart grown_eq = intact;
    WarmStartAccess::eq(grown_eq).conservativeResize(WarmStartAccess::eq(grown_eq).size() + 1);
    WarmStartAccess::eq(grown_eq).tail(1).setZero();
    try {
        (void)grown_eq.lambda(problem);
        FAIL() << "a widened equality block must be refused";
    } catch (const std::invalid_argument &e) {
        EXPECT_TRUE(has(e.what(), "the multiplier blocks hold 2 equality and 1 inequality entries"))
            << e.what();
        EXPECT_TRUE(has(e.what(), "but the problem has 1 equality and 1 inequality rows"))
            << e.what();
    }

    WarmStart grown_iq = intact;
    WarmStartAccess::iq(grown_iq).conservativeResize(WarmStartAccess::iq(grown_iq).size() + 2);
    WarmStartAccess::iq(grown_iq).tail(2).setZero();
    EXPECT_THROW((void)grown_iq.lambda(problem), std::invalid_argument);
}

} // namespace
