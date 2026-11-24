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

#include <string>

#include <fmt/format.h>
#include <gtest/gtest.h>

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

} // namespace
} // namespace hven
