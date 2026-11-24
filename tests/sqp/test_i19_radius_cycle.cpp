// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_i19_radius_cycle.cpp -- M8 I19-2a: the r -> 2r -> r trust-radius cycle of issue #19,
// reproduced in-repo on four fixtures (tests/sqp/support/i19_radius_fixtures.h).
//
//   F-CYC  pure f-type cycle against the initial funnel width
//   F-H    h-type acceptances whose violation rose, grown on the objective ratio
//   F-SOC  SOC-rescued acceptances grown, then rejected at the doubled radius
//   F-CAP  cap-only growth, then idle halvings of a radius the step never touched
//
// The DEFAULT readings below were pinned at origin/m8 afb65b804, before any I19-2a lever
// existed: they are the baseline every lever is measured against and must not move while the
// levers stay at their defaults. Exact pins under HVEN_TEST_ROUTE_REFERENCE; elsewhere the
// mechanism's lower bounds and the optimal exit.

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/detail/sqp/globalization/funnel.h>
#include <hven/detail/sqp/globalization/trust_region.h>

#include "sqp/support/i19_radius_fixtures.h"
#include "support/route_reference.h"

namespace hven {
namespace {

using test_support::I19Fixture;
using test_support::I19RadiusReading;

struct DefaultPin {
    int majors;
    int growths;
    int grow_then_reject;
    int triplets;
    int rejected;
    int funnel_ceiling;
    int h_increase_growths;
    int soc_grow_then_reject;
    int cap_only_growths;
    int unbound_rejections;
    int identical_resolves;
};

std::string describe(const I19RadiusReading &q, const SqpResult &r) {
    return fmt::format("majors={} growths={} gtr={} trip={} rej={} rfc={} hinc={} socgtr={} "
                       "caponly={} unbound={} ident={}",
                       q.majors, q.growths, q.grow_then_reject, q.triplets, q.ceiling_like,
                       r.counters.reject_funnel_ceiling, q.h_increase_growths,
                       q.soc_grow_then_reject, q.cap_only_growths, q.unbound_rejections,
                       q.identical_resolves);
}

void expect_pin(const char *key, const I19Fixture &f, const DefaultPin &pin) {
    const SqpResult r = test_support::i19_solve(f);
    const I19RadiusReading q = test_support::i19_read(r);
    ASSERT_EQ(r.status, SolveStatus::kOptimal) << describe(q, r);
    EXPECT_EQ(q.ceiling_like, r.counters.rejected_steps) << "every depth-0 rejection is a row";
#if HVEN_TEST_ROUTE_REFERENCE
    EXPECT_EQ(q.majors, pin.majors) << describe(q, r);
    EXPECT_EQ(q.growths, pin.growths) << describe(q, r);
    EXPECT_EQ(q.grow_then_reject, pin.grow_then_reject) << describe(q, r);
    EXPECT_EQ(q.triplets, pin.triplets) << describe(q, r);
    EXPECT_EQ(q.ceiling_like, pin.rejected) << describe(q, r);
    EXPECT_EQ(r.counters.reject_funnel_ceiling, pin.funnel_ceiling) << describe(q, r);
    EXPECT_EQ(q.h_increase_growths, pin.h_increase_growths) << describe(q, r);
    EXPECT_EQ(q.soc_grow_then_reject, pin.soc_grow_then_reject) << describe(q, r);
    EXPECT_EQ(q.cap_only_growths, pin.cap_only_growths) << describe(q, r);
    EXPECT_EQ(q.unbound_rejections, pin.unbound_rejections) << describe(q, r);
    EXPECT_EQ(q.identical_resolves, pin.identical_resolves) << describe(q, r);
#else
    test_support::record_off_reference_route(key, describe(q, r));
    (void)pin;
#endif
}

// The mechanism every fixture exists for, on every route.
void expect_cycle(const I19Fixture &f) {
    const SqpResult r = test_support::i19_solve(f);
    const I19RadiusReading q = test_support::i19_read(r);
    ASSERT_EQ(r.status, SolveStatus::kOptimal) << describe(q, r);
    EXPECT_GE(q.grow_then_reject, 5) << describe(q, r);
    EXPECT_GE(q.triplets, 3) << describe(q, r);
}

constexpr SqpHessianMode kExact = SqpHessianMode::kExact;
constexpr SqpHessianMode kQn = SqpHessianMode::kLbfgsDamped;

// ---- F-CYC --------------------------------------------------------------------------------------

TEST(I19RadiusCycle, FCycDefaultReadingExact) {
    const I19Fixture f = test_support::f_cyc(kExact);
    expect_cycle(f);
    expect_pin("i19_fcyc_exact", f, {44, 22, 17, 17, 18, 18, 0, 0, 0, 1, 0});
}

TEST(I19RadiusCycle, FCycDefaultReadingQn) {
    const I19Fixture f = test_support::f_cyc(kQn);
    expect_cycle(f);
    expect_pin("i19_fcyc_qn", f, {47, 22, 16, 16, 18, 18, 0, 0, 0, 1, 0});
}

TEST(I19RadiusCycle, FCycEveryCycleIsFType) {
    // The width is never touched: no h-type acceptance, no SOC.
    for (const SqpHessianMode mode : {kExact, kQn}) {
        const SqpResult r = test_support::i19_solve(test_support::f_cyc(mode));
        const I19RadiusReading q = test_support::i19_read(r);
        EXPECT_EQ(r.counters.soc_steps, 0);
        int f_growths_rejected = 0;
        for (std::size_t k = 0; k + 1 < r.history.size(); ++k) {
            const auto &h = r.history[k];
            if (h.sqp && h.sqp->verdict == StepVerdict::kAcceptObjectiveStep &&
                r.history[k + 1].sqp->trust_radius > h.sqp->trust_radius &&
                r.history[k + 1].sqp->verdict == StepVerdict::kReject) {
                ++f_growths_rejected;
            }
        }
        EXPECT_EQ(f_growths_rejected, q.grow_then_reject) << "every grown-then-rejected step "
                                                             "was an f-type acceptance";
    }
}

// ---- F-H ----------------------------------------------------------------------------------------

TEST(I19RadiusCycle, FHDefaultReadingExact) {
    const I19Fixture f = test_support::f_h(kExact);
    expect_cycle(f);
    expect_pin("i19_fh_exact", f, {36, 17, 14, 14, 15, 15, 5, 0, 0, 1, 0});
}

TEST(I19RadiusCycle, FHDefaultReadingQn) {
    const I19Fixture f = test_support::f_h(kQn);
    expect_cycle(f);
    expect_pin("i19_fh_qn", f, {38, 17, 14, 14, 15, 15, 5, 0, 0, 1, 0});
}

TEST(I19RadiusCycle, FHGrowsOnHTypeStepsWhoseViolationRose) {
    for (const SqpHessianMode mode : {kExact, kQn}) {
        const SqpResult r = test_support::i19_solve(test_support::f_h(mode));
        const I19RadiusReading q = test_support::i19_read(r);
        EXPECT_EQ(r.counters.soc_steps, 0);
        EXPECT_GE(q.h_increase_growths, 3) << describe(q, r);
    }
}

// ---- F-SOC --------------------------------------------------------------------------------------

TEST(I19RadiusCycle, FSocDefaultReadingExact) {
    const I19Fixture f = test_support::f_soc(kExact);
    expect_cycle(f);
    expect_pin("i19_fsoc_exact", f, {36, 18, 12, 12, 14, 14, 1, 12, 0, 0, 0});
}

TEST(I19RadiusCycle, FSocDefaultReadingQn) {
    const I19Fixture f = test_support::f_soc(kQn);
    expect_cycle(f);
    expect_pin("i19_fsoc_qn", f, {38, 19, 13, 13, 15, 15, 1, 13, 0, 1, 0});
}

TEST(I19RadiusCycle, FSocGrowsOnSocRescuedAcceptancesAndIsThenRejected) {
    for (const SqpHessianMode mode : {kExact, kQn}) {
        const SqpResult r = test_support::i19_solve(test_support::f_soc(mode));
        const I19RadiusReading q = test_support::i19_read(r);
        EXPECT_GE(q.soc_grow_then_reject, 3) << describe(q, r);
    }
}

// ---- F-CAP --------------------------------------------------------------------------------------

TEST(I19RadiusCycle, FCapDefaultReadingExact) {
    const I19Fixture f = test_support::f_cap(kExact);
    expect_pin("i19_fcap_exact", f, {126, 70, 5, 5, 36, 36, 7, 5, 64, 36, 30});
}

TEST(I19RadiusCycle, FCapDefaultReadingQn) {
    const I19Fixture f = test_support::f_cap(kQn);
    expect_pin("i19_fcap_qn", f, {127, 70, 5, 5, 36, 36, 5, 5, 64, 36, 30});
}

TEST(I19RadiusCycle, FCapGrowsOnCapOnlyStepsAndResolvesTheIdenticalQp) {
    for (const SqpHessianMode mode : {kExact, kQn}) {
        const SqpResult r = test_support::i19_solve(test_support::f_cap(mode));
        const I19RadiusReading q = test_support::i19_read(r);
        ASSERT_EQ(r.status, SolveStatus::kOptimal) << describe(q, r);
        EXPECT_GE(q.cap_only_growths, 3) << describe(q, r);
        EXPECT_GE(q.unbound_rejections, 3) << describe(q, r);
        EXPECT_GE(q.identical_resolves, 1) << describe(q, r);
    }
}

// =================================================================================================
// The levers in the driver (I19-2a). Each lever reaches the driver, moves its own fixture's
// signature, keeps the optimal exit and costs no more majors than the default there. The exact
// readings are pinned under HVEN_TEST_ROUTE_REFERENCE; the counter identities hold everywhere.
// =================================================================================================

enum class Lever { kNone, kG1, kG2, kG3, kG4, kG5, kG6, kG7 };

const char *lever_name(Lever l) {
    switch (l) {
    case Lever::kNone:
        return "none";
    case Lever::kG1:
        return "g1";
    case Lever::kG2:
        return "g2";
    case Lever::kG3:
        return "g3";
    case Lever::kG4:
        return "g4";
    case Lever::kG5:
        return "g5";
    case Lever::kG6:
        return "g6";
    case Lever::kG7:
        return "g7";
    }
    return "?";
}

// The declared arm values (DECLARATION-I19-2a section 3).
void set_lever(SqpOptions &o, Lever l) {
    switch (l) {
    case Lever::kNone:
        break;
    case Lever::kG1:
        o.advanced.trust_region_growth = SqpTrustRegionGrowth::kStepTypeRatio;
        break;
    case Lever::kG2:
        o.advanced.trust_region_growth_after_soc = false;
        break;
    case Lever::kG3:
        o.advanced.trust_region_rejection_memory = 3;
        break;
    case Lever::kG4:
        o.advanced.trust_region_ratio_ceiling = 3.0;
        break;
    case Lever::kG5:
        o.advanced.trust_region_shrink_to_step = true;
        break;
    case Lever::kG6:
        o.advanced.trust_region_growth_on_cap_pins = false;
        break;
    case Lever::kG7:
        o.advanced.trust_region_funnel_headroom = true;
        break;
    }
}

struct LeverReading {
    int majors;
    int tr_growths;
    int tr_growth_refusals;
    int tr_shrinks_to_step;
    int tr_grow_then_reject;
    int tr_unbound_rejections;
    int tr_cap_binding_majors;
};

LeverReading lever_reading(const SqpResult &r) {
    const I19RadiusReading q = test_support::i19_read(r);
    const auto &c = r.counters;
    return {q.majors,
            static_cast<int>(c.tr_growths),
            static_cast<int>(c.tr_growth_refusals),
            static_cast<int>(c.tr_shrinks_to_step),
            static_cast<int>(c.tr_grow_then_reject),
            static_cast<int>(c.tr_unbound_rejections),
            static_cast<int>(c.tr_cap_binding_majors)};
}

std::string describe(const LeverReading &v) {
    return fmt::format("{{{}, {}, {}, {}, {}, {}, {}}}", v.majors, v.tr_growths,
                       v.tr_growth_refusals, v.tr_shrinks_to_step, v.tr_grow_then_reject,
                       v.tr_unbound_rejections, v.tr_cap_binding_majors);
}

bool operator==(const LeverReading &a, const LeverReading &b) {
    return a.majors == b.majors && a.tr_growths == b.tr_growths &&
           a.tr_growth_refusals == b.tr_growth_refusals &&
           a.tr_shrinks_to_step == b.tr_shrinks_to_step &&
           a.tr_grow_then_reject == b.tr_grow_then_reject &&
           a.tr_unbound_rejections == b.tr_unbound_rejections &&
           a.tr_cap_binding_majors == b.tr_cap_binding_majors;
}

// The new counters against the history they summarize. Depth-0 rows only: a restoration
// sub-solve's radius is its own and is not folded.
void expect_counter_identities(const SqpResult &r) {
    const I19RadiusReading q = test_support::i19_read(r);
    EXPECT_EQ(r.counters.tr_growths, q.growths);
    EXPECT_EQ(r.counters.tr_grow_then_reject, q.grow_then_reject);
    EXPECT_EQ(r.counters.tr_unbound_rejections, q.unbound_rejections);
    Index cap_rows = 0;
    Index refused_rows = 0;
    for (const IterationRecord &h : r.history) {
        if (!h.sqp || h.sqp->depth != 0) {
            continue;
        }
        cap_rows += h.sqp->cap_binding ? 1 : 0;
        refused_rows += h.sqp->tr_growth_refusal != 0 ? 1 : 0;
        if (h.sqp->tr_growth_refusal != 0) {
            EXPECT_TRUE(h.sqp->verdict == StepVerdict::kAcceptObjectiveStep ||
                        h.sqp->verdict == StepVerdict::kAcceptFeasibilityStep)
                << "a refusal is recorded on an accepted row only";
        }
    }
    EXPECT_EQ(r.counters.tr_cap_binding_majors, cap_rows);
    EXPECT_EQ(r.counters.tr_growth_refusals, refused_rows);
}

constexpr Lever kLevers[] = {Lever::kNone, Lever::kG1, Lever::kG2, Lever::kG3,
                             Lever::kG4,   Lever::kG5, Lever::kG6, Lever::kG7};

struct FixtureRow {
    const char *fixture;
    I19Fixture (*make)(SqpHessianMode);
    SqpHessianMode mode;
    // {majors, tr_growths, refusals, shrinks_to_step, grow_then_reject, unbound_rejections,
    //  cap_binding_majors}, one per lever in kLevers' order
    LeverReading pin[8];
};

// clang-format off
const FixtureRow kLeverPins[] = {
    {"fcyc", test_support::f_cyc, kExact, {{44, 22, 0, 0, 17, 1, 0}, {44, 21, 1, 0, 17, 1, 0}, {44, 22, 0, 0, 17, 1, 0}, {32, 9, 13, 0, 5, 0, 0}, {44, 22, 0, 0, 17, 1, 0}, {44, 22, 0, 1, 17, 1, 0}, {44, 22, 0, 0, 17, 1, 0}, {27, 4, 18, 0, 0, 0, 0}}},
    {"fcyc", test_support::f_cyc, kQn, {{47, 22, 0, 0, 16, 1, 0}, {47, 21, 1, 0, 16, 1, 0}, {47, 22, 0, 0, 16, 1, 0}, {35, 10, 13, 0, 5, 0, 0}, {47, 21, 1, 0, 16, 1, 0}, {46, 22, 0, 1, 16, 1, 0}, {47, 22, 0, 0, 16, 1, 0}, {29, 4, 18, 0, 0, 0, 0}}},
    {"fh", test_support::f_h, kExact, {{36, 17, 0, 0, 14, 1, 0}, {22, 2, 15, 0, 0, 0, 0}, {36, 17, 0, 0, 14, 1, 0}, {26, 6, 11, 0, 4, 0, 0}, {36, 17, 0, 0, 14, 1, 0}, {36, 17, 0, 1, 14, 1, 0}, {36, 17, 0, 0, 14, 1, 0}, {22, 2, 15, 0, 0, 0, 0}}},
    {"fh", test_support::f_h, kQn, {{38, 17, 0, 0, 14, 1, 0}, {24, 2, 15, 0, 0, 0, 0}, {38, 17, 0, 0, 14, 1, 0}, {28, 6, 11, 0, 4, 0, 0}, {38, 17, 0, 0, 14, 1, 0}, {39, 17, 0, 1, 14, 1, 0}, {38, 17, 0, 0, 14, 1, 0}, {24, 2, 15, 0, 0, 0, 0}}},
    {"fsoc", test_support::f_soc, kExact, {{36, 18, 0, 0, 12, 0, 0}, {43, 15, 12, 0, 11, 0, 0}, {32, 3, 25, 0, 0, 0, 0}, {29, 10, 9, 0, 4, 0, 0}, {36, 18, 0, 0, 12, 0, 0}, {36, 18, 0, 0, 12, 0, 0}, {36, 18, 0, 0, 12, 0, 0}, {40, 2, 34, 0, 0, 0, 0}}},
    {"fsoc", test_support::f_soc, kQn, {{38, 19, 0, 0, 13, 1, 0}, {44, 15, 12, 0, 11, 0, 0}, {37, 3, 26, 0, 0, 0, 0}, {29, 10, 9, 0, 4, 0, 0}, {38, 19, 0, 0, 13, 1, 0}, {38, 19, 0, 1, 13, 1, 0}, {38, 19, 0, 0, 13, 1, 0}, {43, 2, 34, 0, 0, 0, 0}}},
    {"fcap", test_support::f_cap, kExact, {{126, 70, 0, 0, 5, 36, 117}, {124, 53, 31, 0, 3, 34, 115}, {122, 58, 30, 0, 0, 31, 108}, {123, 67, 6, 0, 2, 33, 112}, {126, 70, 0, 0, 5, 36, 117}, {105, 76, 0, 4, 10, 4, 84}, {98, 10, 78, 0, 6, 7, 85}, {118, 29, 59, 0, 0, 27, 104}}},
    {"fcap", test_support::f_cap, kQn, {{127, 70, 0, 0, 5, 36, 116}, {125, 53, 29, 0, 3, 34, 114}, {135, 55, 43, 0, 0, 30, 99}, {124, 67, 6, 0, 2, 33, 111}, {127, 70, 0, 0, 5, 36, 116}, {108, 77, 0, 5, 10, 5, 83}, {101, 11, 76, 0, 6, 8, 84}, {132, 29, 69, 0, 0, 27, 96}}},
    {"fhs", test_support::f_h_scaled, kExact, {{23, 10, 0, 0, 6, 1, 0}, {16, 3, 8, 0, 0, 0, 0}, {23, 10, 0, 0, 6, 1, 0}, {19, 6, 4, 0, 2, 0, 0}, {23, 10, 0, 0, 6, 1, 0}, {23, 10, 0, 1, 6, 1, 0}, {23, 10, 0, 0, 6, 1, 0}, {17, 3, 7, 0, 0, 0, 0}}},
    {"fhs", test_support::f_h_scaled, kQn, {{29, 11, 0, 0, 7, 1, 0}, {18, 3, 8, 0, 0, 0, 0}, {29, 11, 0, 0, 7, 1, 0}, {25, 6, 5, 0, 3, 0, 0}, {29, 11, 0, 0, 7, 1, 0}, {31, 11, 0, 1, 7, 1, 0}, {29, 11, 0, 0, 7, 1, 0}, {22, 3, 8, 0, 0, 0, 0}}},
};
// clang-format on

TEST(I19RadiusCycle, EveryLeverOnEveryFixtureReadsItsPinAndKeepsTheIdentities) {
    for (const FixtureRow &row : kLeverPins) {
        for (std::size_t li = 0; li < 8; ++li) {
            struct {
                const char *fixture;
                SqpHessianMode mode;
                Lever lever;
                LeverReading pin;
            } const c{row.fixture, row.mode, kLevers[li], row.pin[li]};
            SCOPED_TRACE(fmt::format("{} {} {}", c.fixture, c.mode == kExact ? "exact" : "qn",
                                     lever_name(c.lever)));
            I19Fixture f = row.make(c.mode);
            set_lever(f.options, c.lever);
            const SqpResult r = test_support::i19_solve(f);
            ASSERT_EQ(r.status, SolveStatus::kOptimal);
            expect_counter_identities(r);
            if (c.lever == Lever::kNone) {
                EXPECT_EQ(r.counters.tr_growth_refusals, 0) << "no lever, no refusal";
                EXPECT_EQ(r.counters.tr_shrinks_to_step, 0);
            }
            const LeverReading got = lever_reading(r);
#if HVEN_TEST_ROUTE_REFERENCE
            EXPECT_TRUE(got == c.pin) << "read " << describe(got) << " pinned " << describe(c.pin);
#else
            test_support::record_off_reference_route(fmt::format("i19_lever_{}_{}_{}", c.fixture,
                                                                 c.mode == kExact ? "x" : "q",
                                                                 lever_name(c.lever)),
                                                     describe(got));
#endif
        }
    }
}

// Each lever moves its own fixture's signature on every route, at no extra majors.
struct LeverEffect {
    std::function<I19Fixture(SqpHessianMode)> make;
    Lever lever;
    // the signature the lever exists to remove, read off a solve
    std::function<Index(const SqpResult &)> signature;
};

TEST(I19RadiusCycle, EachLeverReducesItsFixturesSignatureAtNoExtraMajors) {
    const auto gtr = [](const SqpResult &r) { return r.counters.tr_grow_then_reject; };
    const auto unbound = [](const SqpResult &r) { return r.counters.tr_unbound_rejections; };
    const std::vector<LeverEffect> effects = {
        {test_support::f_h, Lever::kG1, gtr},       {test_support::f_soc, Lever::kG2, gtr},
        {test_support::f_cyc, Lever::kG3, gtr},     {test_support::f_cap, Lever::kG5, unbound},
        {test_support::f_cap, Lever::kG6, unbound}, {test_support::f_cyc, Lever::kG7, gtr},
    };
    for (const LeverEffect &e : effects) {
        for (const SqpHessianMode mode : {kExact, kQn}) {
            SCOPED_TRACE(fmt::format("{} {}", lever_name(e.lever), mode == kExact ? "x" : "q"));
            const SqpResult base = test_support::i19_solve(e.make(mode));
            I19Fixture f = e.make(mode);
            set_lever(f.options, e.lever);
            const SqpResult with = test_support::i19_solve(f);
            ASSERT_EQ(with.status, SolveStatus::kOptimal);
            EXPECT_LT(2 * e.signature(with), e.signature(base)) << "at least halved";
            EXPECT_LE(with.counters.major_iters, base.counters.major_iters);
        }
    }
}

TEST(I19RadiusCycle, TheRatioCeilingReachesTheDriver) {
    // No fixture grows on a ratio above 3 often; F-CYC under QN does once. A low ceiling shows
    // the lever acting on every route.
    for (const SqpHessianMode mode : {kExact, kQn}) {
        I19Fixture f = test_support::f_cyc(mode);
        f.options.advanced.trust_region_ratio_ceiling = 1.05;
        const SqpResult r = test_support::i19_solve(f);
        ASSERT_EQ(r.status, SolveStatus::kOptimal);
        EXPECT_GT(r.counters.tr_growth_refusals, 0);
        for (const IterationRecord &h : r.history) {
            if (h.sqp && h.sqp->tr_growth_refusal != 0) {
                EXPECT_EQ(h.sqp->tr_growth_refusal, detail::kTrRefusalRatioCeiling);
            }
        }
    }
}

TEST(I19RadiusCycle, TheRefusalBitsNameTheRefusingLever) {
    const auto bits = [](const SqpResult &r) {
        Index all = 0;
        for (const IterationRecord &h : r.history) {
            if (h.sqp) {
                all |= h.sqp->tr_growth_refusal;
            }
        }
        return all;
    };
    const std::vector<std::pair<Lever, Index>> want = {
        {Lever::kG1, detail::kTrRefusalViolationRatio}, {Lever::kG2, detail::kTrRefusalSoc},
        {Lever::kG3, detail::kTrRefusalMemory},         {Lever::kG6, detail::kTrRefusalCapPins},
        {Lever::kG7, detail::kTrRefusalFunnelHeadroom},
    };
    for (const auto &[lever, bit] : want) {
        I19Fixture f = test_support::f_cap(kExact); // every one of these refuses here
        set_lever(f.options, lever);
        EXPECT_EQ(bits(test_support::i19_solve(f)), bit) << lever_name(lever);
    }
}

TEST(I19RadiusCycle, RemedyLeversAtDefaultLeaveTheFixturesUnmoved) {
    // Every lever written at its default value, explicitly: the fixtures read their R pins.
    for (const auto &make :
         {test_support::f_cyc, test_support::f_h, test_support::f_soc, test_support::f_cap}) {
        for (const SqpHessianMode mode : {kExact, kQn}) {
            const SqpResult base = test_support::i19_solve(make(mode));
            I19Fixture f = make(mode);
            f.options.advanced.trust_region_growth = SqpTrustRegionGrowth::kRatio;
            f.options.advanced.trust_region_growth_after_soc = true;
            f.options.advanced.trust_region_rejection_memory = 0;
            f.options.advanced.trust_region_ratio_ceiling = std::numeric_limits<double>::infinity();
            f.options.advanced.trust_region_shrink_to_step = false;
            f.options.advanced.trust_region_growth_on_cap_pins = true;
            f.options.advanced.trust_region_funnel_headroom = false;
            const SqpResult r = test_support::i19_solve(f);
            EXPECT_EQ(r.counters.major_iters, base.counters.major_iters);
            EXPECT_EQ(r.counters.tr_growth_refusals, 0);
            EXPECT_EQ(r.counters.tr_shrinks_to_step, 0);
            ASSERT_EQ(r.history.size(), base.history.size());
            for (std::size_t k = 0; k < r.history.size(); ++k) {
                EXPECT_EQ(r.history[k].sqp->trust_radius, base.history[k].sqp->trust_radius);
            }
        }
    }
}

// ---- scripted verdicts: the memory's wiring, independent of any fixture's trajectory -----------

// Verdicts from a script, then the last entry forever. Never mutated by a second judgement:
// SOC is off in every solve that uses it.
class ScriptedStrategy final : public detail::GlobalizationStrategy {
  public:
    explicit ScriptedStrategy(std::vector<StepVerdict> script) : script_(std::move(script)) {}
    StepVerdict judge(const detail::StepContext &) override {
        const std::size_t k = std::min(calls_, script_.size() - 1);
        ++calls_;
        return script_[k];
    }
    void reset(double) override {}
    void resume_from_restoration(double) override {}

  private:
    std::vector<StepVerdict> script_;
    std::size_t calls_ = 0;
};

constexpr StepVerdict kA = StepVerdict::kAcceptObjectiveStep;
constexpr StepVerdict kR = StepVerdict::kReject;
constexpr StepVerdict kX = StepVerdict::kRestore;

// f = sum x_j on a far sphere: every step is radius-bound. kAnyAccepted grows on every accepted
// radius-bound step, so the radius sequence is the script's alone.
SqpResult scripted(std::vector<StepVerdict> script, Index memory, Index max_iter) {
    auto m = std::make_shared<test_support::I19Sphere>(Vec{{0.0, 1.0}}, 0.0, 1000.0);
    SqpOptions o;
    o.common.verbosity = Verbosity::kSilent;
    o.common.max_iterations = max_iter;
    o.advanced.second_order_correction = false;
    o.advanced.trust_region_growth = SqpTrustRegionGrowth::kAnyAccepted;
    o.advanced.trust_region_rejection_memory = memory;
    o.advanced.make_strategy = [script] { return std::make_unique<ScriptedStrategy>(script); };
    Problem p(m);
    return SqpSolver(o).solve(p, Vec{{1000.0, 0.0}});
}

// The radii of the first `n` depth-0 rows.
std::vector<double> radii(const SqpResult &r, std::size_t n) {
    std::vector<double> v;
    for (const IterationRecord &h : r.history) {
        if (h.sqp && h.sqp->depth == 0 && v.size() < n) {
            v.push_back(h.sqp->trust_radius);
        }
    }
    return v;
}

TEST(I19RadiusCycle, ScriptedMemoryRefusesTheRejectedRadiusUntilNAcceptancesPass) {
    // accept (1 -> 2), reject at the grown 2 (cap 2; -> 1), then accepts: the first three cannot
    // reach 2, the clean count clears after the third, the fourth grows.
    const SqpResult r = scripted({kA, kR, kA}, 3, 8);
    const std::vector<double> want = {1.0, 2.0, 1.0, 1.0, 1.0, 1.0, 2.0, 4.0};
    EXPECT_EQ(radii(r, want.size()), want);
    EXPECT_EQ(r.counters.tr_grow_then_reject, 1);
    EXPECT_EQ(r.counters.tr_growth_refusals, 3);
    // Off: the radius doubles straight back.
    const std::vector<double> off = {1.0, 2.0, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0};
    EXPECT_EQ(radii(scripted({kA, kR, kA}, 0, 8), off.size()), off);
}

TEST(I19RadiusCycle, ScriptedMemoryRecordsTheRejectedRadiusNotTheShrunkOne) {
    // 1 -> 2 -> 4, reject at 4 (cap 4; -> 2): 2 -> 4 is refused, so the radius stays at 2.
    const SqpResult r = scripted({kA, kA, kR, kA}, 3, 6);
    const std::vector<double> want = {1.0, 2.0, 4.0, 2.0, 2.0, 2.0};
    EXPECT_EQ(radii(r, want.size()), want);
}

TEST(I19RadiusCycle, ScriptedMemoryIgnoresARejectionAtARadiusNoGrowthSet) {
    // accept (1 -> 2), reject at 2 (cap 2; -> 1), reject at 1 (not from a growth: no record, the
    // clean count is not restarted); the accepts then clear after three.
    const SqpResult r = scripted({kA, kR, kR, kA}, 3, 8);
    const std::vector<double> want = {1.0, 2.0, 1.0, 0.5, 1.0, 1.0, 1.0, 2.0};
    EXPECT_EQ(radii(r, want.size()), want);
    EXPECT_EQ(r.counters.tr_grow_then_reject, 1);
    EXPECT_EQ(r.counters.tr_unbound_rejections, 0);
}

TEST(I19RadiusCycle, ScriptedMemoryIsClearedAtARestorationResume) {
    // Off-manifold start (h > 0) so restoration has work. accept (1 -> 2), reject at 2 (cap 2),
    // then the restoration signature; after the resume every accepted step grows freely.
    auto m = std::make_shared<test_support::I19Sphere>(Vec{{0.0, 1.0}}, 0.0, 1000.0);
    SqpOptions o;
    o.common.verbosity = Verbosity::kSilent;
    o.common.max_iterations = 40;
    o.advanced.second_order_correction = false;
    o.advanced.trust_region_growth = SqpTrustRegionGrowth::kAnyAccepted;
    // A long window: only the resume can clear the cap before the radius regrows past it.
    o.advanced.trust_region_rejection_memory = 50;
    o.advanced.make_strategy = [] {
        return std::make_unique<ScriptedStrategy>(std::vector<StepVerdict>{kA, kR, kX, kA});
    };
    Problem p(m);
    const SqpResult r = SqpSolver(o).solve(p, Vec{{1010.0, 0.0}});
    ASSERT_GE(r.counters.restoration_entries, 1);
    EXPECT_EQ(r.counters.tr_grow_then_reject, 1);
    EXPECT_EQ(r.counters.tr_growth_refusals, 0) << "the cap did not survive the resume";
    // Premise: after the resume the radius regrew past the rejected 2.
    double peak_after = 0.0;
    bool resumed = false;
    for (const IterationRecord &h : r.history) {
        if (!h.sqp || h.sqp->depth != 0) {
            continue;
        }
        resumed = resumed || h.sqp->verdict == kX;
        if (resumed) {
            peak_after = std::max(peak_after, h.sqp->trust_radius);
        }
    }
    EXPECT_GT(peak_after, 2.0);
    expect_counter_identities(r);
}

// ---- G5's shrink and the elastic tier's two flags ----------------------------------------------

TEST(I19RadiusCycle, ShrinkToStepSetsTheRadiusFromTheRejectedStepAlone) {
    // F-CAP and F-SOC: after each rejection whose step the radius did not hold, the next row's
    // radius is half the rejected row's step -- the original trial's, not a SOC re-solve's (F-SOC
    // under QN has such a row whose SOC re-solve solved: soc_h is present on it).
    int soc_rows = 0;
    for (const auto &make : {test_support::f_cap, test_support::f_soc}) {
        for (const SqpHessianMode mode : {kExact, kQn}) {
            I19Fixture f = make(mode);
            f.options.advanced.trust_region_shrink_to_step = true;
            const SqpResult r = test_support::i19_solve(f);
            ASSERT_EQ(r.status, SolveStatus::kOptimal);
            int checked = 0;
            for (std::size_t k = 0; k + 1 < r.history.size(); ++k) {
                const auto &h = r.history[k];
                if (!h.sqp || h.sqp->verdict != StepVerdict::kReject || h.sqp->tr_binding) {
                    continue;
                }
                const double want =
                    std::min(0.5 * h.sqp->trust_radius, 0.5 * h.step_norm.value_or(0.0));
                EXPECT_EQ(r.history[k + 1].sqp->trust_radius, want) << "row " << k;
                ++checked;
                soc_rows += h.sqp->soc_h.has_value() ? 1 : 0;
            }
            EXPECT_EQ(r.counters.tr_shrinks_to_step, checked);
        }
    }
    EXPECT_GE(soc_rows, 1) << "premise: a SOC re-solve solved on an unbound rejected row";
}

// min |x - c|^2 / 2 with no constraints and c far away.
struct FarQuadratic final : Nlp {
    NlpDimensions dimensions() const override {
        return {.n = 2, .m = 0, .jacobian_nonzeros = 0, .hessian_nonzeros = 2};
    }
    void bounds(VecRef l, VecRef u, VecRef, VecRef) const override {
        l.setConstant(-kInfinity);
        u.setConstant(kInfinity);
    }
    double eval_f(ConstVecRef x) const override {
        return 0.5 * ((x[0] - 1e3) * (x[0] - 1e3) + (x[1] - 1e3) * (x[1] - 1e3));
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g[0] = x[0] - 1e3;
        g[1] = x[1] - 1e3;
    }
    void eval_g(ConstVecRef, VecRef) const override {}
    void jacobian_structure(IndexVecRef, IndexVecRef) const override {}
    void eval_jacobian(ConstVecRef, VecRef) const override {}
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r[0] = 0;
        c[0] = 0;
        r[1] = 1;
        c[1] = 1;
    }
    void eval_hessian(ConstVecRef, double s, ConstVecRef, VecRef v) const override {
        v[0] = s;
        v[1] = s;
    }
};

TEST(I19RadiusCycle, AMixedStepStaysGrowthEligibleWithoutCapPinGrowth) {
    // From (0, 100) with caller_step_limit 0.5 and radius 2: x0's cap is 0.5 (it binds), x1's is
    // 50.5, so the radius holds x1 -- a step held by both kinds of pin. G6 refuses growth only to
    // a step the cap alone held, so this one doubles the radius.
    SqpOptions o;
    o.common.verbosity = Verbosity::kSilent;
    o.common.max_iterations = 3;
    o.trust_region_initial = 2.0;
    o.caller_step_limit = 0.5;
    o.advanced.trust_region_growth_on_cap_pins = false;
    Problem p(std::make_shared<FarQuadratic>());
    const SqpResult r = SqpSolver(o).solve(p, Vec{{0.0, 100.0}});
    ASSERT_GE(r.history.size(), 2u);
    const auto &h0 = r.history[0];
    ASSERT_TRUE(h0.sqp->tr_binding && h0.sqp->cap_binding) << "premise: a mixed step";
    ASSERT_EQ(h0.sqp->verdict, StepVerdict::kAcceptObjectiveStep);
    EXPECT_EQ(r.history[1].sqp->trust_radius, 4.0);
    EXPECT_EQ(h0.sqp->tr_growth_refusal, 0);
}

// min 1/2 |x - c|^2 s.t. x0 = 0 and x0 = 1 (inconsistent): with the equality basis off every
// main QP is infeasible and the elastic ladder supplies the step; c is far along x1, so the step
// there is limited.
struct InconsistentPair final : Nlp {
    NlpDimensions dimensions() const override {
        return {.n = 2, .m = 2, .jacobian_nonzeros = 2, .hessian_nonzeros = 2};
    }
    void bounds(VecRef l, VecRef u, VecRef gl, VecRef gu) const override {
        l.setConstant(-kInfinity);
        u.setConstant(kInfinity);
        gl[0] = gu[0] = 0.0;
        gl[1] = gu[1] = 1.0;
    }
    double eval_f(ConstVecRef x) const override {
        return 0.5 * (x[0] * x[0] + (x[1] - 100.0) * (x[1] - 100.0));
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g[0] = x[0];
        g[1] = x[1] - 100.0;
    }
    void eval_g(ConstVecRef x, VecRef g) const override {
        g[0] = x[0];
        g[1] = x[0];
    }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r[0] = 0;
        c[0] = 0;
        r[1] = 1;
        c[1] = 0;
    }
    void eval_jacobian(ConstVecRef, VecRef v) const override {
        v[0] = 1.0;
        v[1] = 1.0;
    }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r[0] = 0;
        c[0] = 0;
        r[1] = 1;
        c[1] = 1;
    }
    void eval_hessian(ConstVecRef, double s, ConstVecRef, VecRef v) const override {
        v[0] = s;
        v[1] = s;
    }
};

TEST(I19RadiusCycle, OnElasticMajorsTheRadiusAndTheCapStaySeparated) {
    const auto run = [](double radius, double cap) {
        SqpOptions o;
        o.common.verbosity = Verbosity::kSilent;
        o.common.max_iterations = 6;
        o.trust_region_initial = radius;
        o.trust_region_max = 1e4;
        o.caller_step_limit = cap;
        o.advanced.equality_basis = false;
        Problem p(std::make_shared<InconsistentPair>());
        return SqpSolver(o).solve(p, Vec{{0.5, 0.0}});
    };
    // The cap holds x1 (|dx1| <= 0.1 (1 + |x1|)); the radius (1e3) holds nothing.
    const SqpResult capped = run(1e3, 0.1);
    int elastic_rows = 0;
    for (const IterationRecord &h : capped.history) {
        if (h.sqp && h.sqp->elastic && h.sqp->qp_solved) {
            EXPECT_TRUE(h.sqp->cap_binding) << "row " << h.iteration;
            EXPECT_FALSE(h.sqp->tr_binding) << "row " << h.iteration;
            ++elastic_rows;
        }
    }
    EXPECT_GE(elastic_rows, 1);
    // The radius (0.5) holds x1; no cap.
    const SqpResult radius = run(0.5, 0.0);
    elastic_rows = 0;
    for (const IterationRecord &h : radius.history) {
        if (h.sqp && h.sqp->elastic && h.sqp->qp_solved) {
            EXPECT_TRUE(h.sqp->tr_binding) << "row " << h.iteration;
            EXPECT_FALSE(h.sqp->cap_binding) << "row " << h.iteration;
            ++elastic_rows;
        }
    }
    EXPECT_GE(elastic_rows, 1);
    // And G6 reads the radius's flag there: a cap-held elastic step does not grow the radius.
    {
        SqpOptions o;
        o.common.verbosity = Verbosity::kSilent;
        o.common.max_iterations = 6;
        o.trust_region_initial = 1e3;
        o.trust_region_max = 1e4;
        o.caller_step_limit = 0.1;
        o.advanced.trust_region_growth = SqpTrustRegionGrowth::kAnyAccepted;
        o.advanced.trust_region_growth_on_cap_pins = false;
        o.advanced.equality_basis = false;
        Problem p(std::make_shared<InconsistentPair>());
        const SqpResult r = SqpSolver(o).solve(p, Vec{{0.5, 0.0}});
        EXPECT_EQ(r.counters.tr_growths, 0);
        EXPECT_GE(r.counters.tr_growth_refusals, 1);
    }
}

} // namespace
} // namespace hven
