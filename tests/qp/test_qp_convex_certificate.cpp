// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_convex_certificate.cpp -- M7 W2 NC4: THE CONVEX CERTIFICATE (qp_engine.h section 4c),
// on the walk seam (docs/testing.md). A trusted-kWrong dead end on a QP whose Hessian is
// STRUCTURALLY convex -- every stored entry of H exactly 0 off the diagonal and >= 0 on it; an
// LP is H == 0 -- cannot be a saddle: every KKT matrix the walk factorizes is then
// quasi-definite and its true inertia IS the expectation, so the kWrong is a misread of the
// rounding-sized pivots a flat face carries. The certificate overrules the read and the point
// meets the ordinary walk's first-order tests.
//
// The dumps are the C10 census's restoration QPs (tests/qp/fixtures/e2-d4/C10_*; the `result`
// line is the in-cell outcome at m7 1f8d3118, `site=1335` the trusted-kWrong dead end): nine
// LPs whose walk stopped AT the optimum and exited kNumericalError (NC3 report section 1). The
// objectives are a dense oracle's (Clarabel, scratchpad venv, never vendored; NC3 report): the
// walk must now end kOptimal there, with the KKT battery, on both algebras.
//
// MINOR AND FACTORIZATION COUNTS ARE PRINTED, NEVER ASSERTED.

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

#include <Eigen/Dense>
#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>

#include "../common_support/route_reference.h"
#include "support/qp_dump.h"

using namespace hven::solvers;
using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using hven::test_support::QpDump;

namespace {

constexpr auto kBorder = WorkingSetLinearAlgebra::kSchurBorder;
constexpr auto kRefactorize = WorkingSetLinearAlgebra::kRefactorize;

const char *alg_name(WorkingSetLinearAlgebra alg) {
    return alg == kBorder ? "border" : "refactorize";
}

double objective(const QpProblem &qp, const Vec &x) {
    return 0.5 * x.dot(qp.H.selfadjointView<Eigen::Upper>() * x) + qp.g.dot(x);
}

QpDump load(const char *file) {
    return hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) + "/" + file);
}

// THE RESTORATION LPs. Premise (the dump's own result line and its H): in-cell kNumericalError
// at the trusted-kWrong dead end, on a QP whose H is identically zero. Expected: kOptimal at the
// oracle objective, the KKT battery, the certificate taken, no saddle search or ride.
// `route_certificates` / `route_rereads` >= 0 pin a fixture's measured route instead of the
// "a kWrong read => the certificate" implication (BRITGAS qp4 since M7 W2 QC-8; STEENBRB qp3
// pins certificates == 1 unconditionally, QC-8 rider 8).
void expect_certified(const char *file, double f_star, WorkingSetLinearAlgebra alg,
                      Index route_certificates = -1, Index route_rereads = -1) {
    const QpDump d = load(file);
    ASSERT_NE(d.result.find("status=3"), std::string::npos) << d.result;
    ASSERT_NE(d.result.find("site=1335"), std::string::npos) << d.result;
    ASSERT_EQ(Eigen::MatrixXd(d.qp.H).cwiseAbs().maxCoeff(), 0.0) << "the premise is an LP";
    ASSERT_TRUE(detail::hessian_is_nonneg_diagonal(d.qp.H));
    // The walk's in-cell final point already sat at the optimum: the in-cell failure was the
    // classification, not the walk.
    EXPECT_NEAR(objective(d.qp, d.x_final), f_star, 1e-8 * std::max(1.0, std::abs(f_star)));

    QpOptions o = d.opts;
    o.ws_algebra = alg;
    ASSERT_TRUE(o.independent_working_set) << "the dump is a `default`-label solve";
    const QpSolution s = hven::test_support::solve_qp_dump(d, o);
    const double f = objective(d.qp, s.x);
    const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
    std::printf(
        "[ nc4 ] %s %s status=%d minors=%ld facts=%ld f=%.12g f*=%.12g eq=%.2e box=%.2e "
        "stat=%.2e comp=%.2e convex_certificates=%ld saddle_exits=%ld misses=%ld "
        "rereads=%ld\n",
        file, alg_name(alg), static_cast<int>(s.status), static_cast<long>(s.counters.minor_iters),
        static_cast<long>(s.counters.factorizations), f, f_star, k.eq, k.box, k.stationarity,
        k.complementarity, static_cast<long>(s.counters.convex_certificates),
        static_cast<long>(s.counters.saddle_exits),
        static_cast<long>(s.counters.saddle_exit_misses),
        static_cast<long>(s.counters.dep_inertia_rereads));

    // The certificate is asserted whenever the walk read a kWrong at all (R-C' re-reads every
    // trusted kWrong before the certificate is consulted). MEASURED: under Debug's rounding
    // BRITGAS qp2's walk takes another path to the same optimum and reads no kWrong (rereads 0);
    // the outcome below is the contract on every build.
#if HVEN_TEST_ROUTE_REFERENCE
    if (route_certificates >= 0) {
        EXPECT_EQ(s.counters.convex_certificates, route_certificates);
        if (route_rereads >= 0) {
            EXPECT_EQ(s.counters.dep_inertia_rereads, route_rereads);
        }
    } else if (s.counters.dep_inertia_rereads > 0) {
        EXPECT_GE(s.counters.convex_certificates, 1);
    }
#else
    // Off the reference build (Windows MKL, Accelerate, ...) the exact route is not pinned (WX
    // design section 2.1): it is recorded. A call that pins no route keeps the file's contract
    // implication -- a kWrong read => the certificate. A call that DOES pin one (BRITGAS qp4,
    // STEENBRB qp3) gets no implication: BRITGAS qp4's own reference route reads one re-read
    // and takes no certificate (the QC-8 note above), so the implication is not portable there;
    // the status, objective and KKT battery below are its portable contract.
    hven::test_support::record_off_reference_route(
        std::string(file) + "_" + alg_name(alg),
        "convex_certificates=" + std::to_string(s.counters.convex_certificates) +
            " rereads=" + std::to_string(s.counters.dep_inertia_rereads) +
            " minors=" + std::to_string(s.counters.minor_iters) +
            " facts=" + std::to_string(s.counters.factorizations));
    if (route_certificates < 0 && s.counters.dep_inertia_rereads > 0) {
        EXPECT_GE(s.counters.convex_certificates, 1);
    }
#endif
    EXPECT_EQ(s.counters.saddle_exits, 0);
    EXPECT_EQ(s.counters.saddle_exit_misses, 0) << "the certificate precedes the saddle search";
    ASSERT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_NEAR(f, f_star, 1e-8 * std::max(1.0, std::abs(f_star)));
    EXPECT_LE(k.eq, 1e-8);
    EXPECT_LE(k.ineq, 1e-8);
    EXPECT_LE(k.box, 1e-9);
    EXPECT_LE(k.stationarity, 1e-8);
    EXPECT_LE(k.sign, 1e-9);
    EXPECT_LE(k.complementarity, 1e-8);
}

// The two-variable shapes of the structural test.
QpProblem diag_qp(double h00, double h01, double h11) {
    // Every upper entry STORED, zeros included: the stored pattern is what the test reads.
    QpProblem qp;
    qp.H.resize(2, 2);
    qp.H.insert(0, 0) = h00;
    qp.H.insert(0, 1) = h01;
    qp.H.insert(1, 1) = h11;
    qp.H.makeCompressed();
    return qp;
}

} // namespace

TEST(QpNc4ConvexCertificate, TheStructuralTestIsExactAndConservative) {
    EXPECT_TRUE(detail::hessian_is_nonneg_diagonal(diag_qp(0.0, 0.0, 0.0).H));
    EXPECT_TRUE(detail::hessian_is_nonneg_diagonal(diag_qp(2.0, 0.0, 0.0).H));
    EXPECT_FALSE(detail::hessian_is_nonneg_diagonal(diag_qp(-1e-300, 0.0, 1.0).H));
    EXPECT_FALSE(detail::hessian_is_nonneg_diagonal(diag_qp(1.0, 1e-300, 1.0).H))
        << "a PSD H that is not diagonal is not claimed: the test is structural, not spectral";
    EXPECT_FALSE(detail::hessian_is_nonneg_diagonal(
        diag_qp(std::numeric_limits<double>::quiet_NaN(), 0.0, 1.0).H));
    // An explicitly STORED zero is a zero.
    SpMatRM H(2, 2);
    H.insert(0, 0) = 0.0;
    H.insert(0, 1) = 0.0;
    H.insert(1, 1) = 3.0;
    H.makeCompressed();
    EXPECT_TRUE(detail::hessian_is_nonneg_diagonal(H));
}

// THE SADDLES ARE NOT CONVEX. The three elastic saddles NC3 rides off (and the main-phase QP
// whose curvature is below resolution) carry negative curvature or off-diagonal coupling, so the
// certificate can never reach them.
TEST(QpNc4ConvexCertificate, NoSaddleDumpQualifies) {
    for (const char *f : {"C10_STEENBRB_qp2.txt", "C10_STEENBRD_qp1.txt", "C10_STEENBRE_qp1.txt",
                          "C10_STEENBRE_qp2.txt"}) {
        EXPECT_FALSE(detail::hessian_is_nonneg_diagonal(load(f).qp.H)) << f;
    }
}

// THE OPTION-OFF ARM IS UNTOUCHED: no certificate is taken with the independent working set off.
TEST(QpNc4ConvexCertificate, TheOptionOffWalkTakesNoCertificate) {
    const QpDump d = load("C10_STEENBRB_qp3.txt");
    for (const auto alg : {kBorder, kRefactorize}) {
        QpOptions o = d.opts;
        o.ws_algebra = alg;
        o.independent_working_set = false;
        const QpSolution s = hven::test_support::solve_qp_dump(d, o);
        std::printf("[ nc4 ] option-off %s status=%d minors=%ld\n", alg_name(alg),
                    static_cast<int>(s.status), static_cast<long>(s.counters.minor_iters));
        EXPECT_EQ(s.counters.convex_certificates, 0);
    }
}

TEST(QpNc4ConvexCertificate, SteenbrbRestorationLpQp3) {
    // The route certificate, pinned unconditionally (not behind "a kWrong was read"): the NC4
    // certificate fires exactly once on this walk on both algebras.
    expect_certified("C10_STEENBRB_qp3.txt", -48.0, kBorder, /*route_certificates=*/1);
    expect_certified("C10_STEENBRB_qp3.txt", -48.0, kRefactorize, /*route_certificates=*/1);
}
TEST(QpNc4ConvexCertificate, SteenbrbRestorationLpQp4) {
    expect_certified("C10_STEENBRB_qp4.txt", -24.0, kBorder);
    expect_certified("C10_STEENBRB_qp4.txt", -24.0, kRefactorize);
}
TEST(QpNc4ConvexCertificate, SteenbrbRestorationLpQp5) {
    expect_certified("C10_STEENBRB_qp5.txt", -24.0, kBorder);
    expect_certified("C10_STEENBRB_qp5.txt", -24.0, kRefactorize);
}
TEST(QpNc4ConvexCertificate, SteenbrbRestorationLpQp6) {
    expect_certified("C10_STEENBRB_qp6.txt", -12.0, kBorder);
    expect_certified("C10_STEENBRB_qp6.txt", -12.0, kRefactorize);
}
TEST(QpNc4ConvexCertificate, SteenbrbRestorationLpQp7) {
    expect_certified("C10_STEENBRB_qp7.txt", -6.0, kBorder);
    expect_certified("C10_STEENBRB_qp7.txt", -6.0, kRefactorize);
}
TEST(QpNc4ConvexCertificate, SteenbrbRestorationLpQp8) {
    expect_certified("C10_STEENBRB_qp8.txt", -3.0, kBorder);
    expect_certified("C10_STEENBRB_qp8.txt", -3.0, kRefactorize);
}
// BRITGAS qp2: the census's "LP whose walk stops AT the optimum and still exits as a failure".
TEST(QpNc4ConvexCertificate, BritgasRestorationLpQp2) {
    expect_certified("C10_BRITGAS_qp2.txt", -67.8186110861, kBorder);
    expect_certified("C10_BRITGAS_qp2.txt", -67.8186110861, kRefactorize);
}
TEST(QpNc4ConvexCertificate, BritgasRestorationLpQp3) {
    expect_certified("C10_BRITGAS_qp3.txt", -35.4063659176, kBorder);
    expect_certified("C10_BRITGAS_qp3.txt", -35.4063659176, kRefactorize);
}
TEST(QpNc4ConvexCertificate, BritgasRestorationLpQp4) {
    // M7 W2 QC-8 (declared route mover; the outcome holds): the implied blocker passes over
    // dependent blockers this walk used to exchange, and it reaches the optimum by another
    // route -- Release border 950 minors / 968 factorizations / 3 kWrong re-reads / 1 convex
    // certificate -> 969 / 1080 / 1 / 0 (Debug 985 / 1096 / 1 / 0), the one kWrong it still
    // reads resolved by the re-read without the certificate. The route is pinned here in
    // place of "a re-read => a certificate"; the NC4 certificate stays exercised by
    // SteenbrbRestorationLpQp3..8 and BritgasRestorationLpQp3 (one certificate each, both
    // algebras, both configs) and BritgasRestorationLpQp2 (Release; Debug reads no kWrong
    // there, as its own note above says).
    expect_certified("C10_BRITGAS_qp4.txt", -19.1894367779, kBorder, /*route_certificates=*/0,
                     /*route_rereads=*/1);
    expect_certified("C10_BRITGAS_qp4.txt", -19.1894367779, kRefactorize,
                     /*route_certificates=*/0, /*route_rereads=*/1);
}
