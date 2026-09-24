// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M7 W2 E2 step 1: the working-set dependency test and the entry detector
// (qp/ws_independence.h), on constructed QPs, in both ws_algebra modes.

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/qp/eqp_solve.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/detail/qp/ws_independence.h>

using namespace hven::solvers;
using hven::Index;
using hven::SpMatRM;
using hven::Vec;

namespace {

SpMatRM dense_to_sparse(const Eigen::MatrixXd &m) { return m.sparseView(); }

QpProblem make_qp(const Eigen::MatrixXd &H, const Vec &g, const Eigen::MatrixXd &Ae, const Vec &be,
                  const Eigen::MatrixXd &Ai, const Vec &bi, double lo, double up) {
    QpProblem qp;
    const Index n = H.rows();
    qp.H = dense_to_sparse(H.triangularView<Eigen::Upper>().toDenseMatrix());
    qp.g = g;
    qp.Ae = dense_to_sparse(Ae);
    qp.be = be;
    qp.Ai = dense_to_sparse(Ai);
    qp.bi = bi;
    qp.lower = Vec::Constant(n, lo);
    qp.upper = Vec::Constant(n, up);
    return qp;
}

QpOptions opts_for(WorkingSetLinearAlgebra alg, bool on = true) {
    QpOptions o;
    o.ws_algebra = alg;
    o.independent_working_set = on;
    return o;
}

class WsIndependence : public ::testing::TestWithParam<WorkingSetLinearAlgebra> {};

// min 1/2||x||^2 - x0 - 2 x1 s.t. x0 + x1 <= 1, 2 x0 + 2 x1 <= 2 (a duplicate),
// 0 <= x <= 10; seeded with BOTH rows active. The detector removes the
// higher-index seed row, and the walk ends at the unique optimum (0, 1).
TEST_P(WsIndependence, EntryDetectorRemovesDuplicatedSeedRow) {
    Eigen::MatrixXd Ai(2, 2);
    Ai << 1, 1, 2, 2;
    Vec g(2);
    g << -1, -2;
    Vec bi(2);
    bi << 1, 2;
    const QpProblem qp = make_qp(Eigen::MatrixXd::Identity(2, 2), g, Eigen::MatrixXd(0, 2), Vec(0),
                                 Ai, bi, 0.0, 10.0);
    QpSolution seed;
    seed.x = Vec::Zero(2);
    seed.bound_state.assign(2, BoundState::kFree);
    seed.ineq_active = {true, true};

    const QpEngine on(opts_for(GetParam()));
    const QpSolution s = on.solve(qp, seed);
    ASSERT_EQ(s.status, QpStatus::kOptimal);
    EXPECT_EQ(s.counters.entry_dependency_removals, 1);
    EXPECT_EQ(s.counters.mandatory_dependent_entries, 0);
    EXPECT_GE(s.counters.dep_test_solves, 4);
    EXPECT_NEAR(s.x(0), 0.0, 1e-8);
    EXPECT_NEAR(s.x(1), 1.0, 1e-8);
    // Step 1 only COUNTS at the add site, so the ratio test can re-admit the
    // removed row at alpha = 0; that add must be flagged dependent. Step 2's
    // exchange turns this into EXPECT_FALSE(both active).
    if (s.ineq_active[1] && s.ineq_active[0]) {
        EXPECT_GE(s.counters.dep_dependent, 1);
    }

    const QpEngine off(opts_for(GetParam(), false));
    const QpSolution s0 = off.solve(qp, seed);
    EXPECT_EQ(s0.counters.dep_test_solves, 0);
    EXPECT_EQ(s0.counters.entry_dependency_removals, 0);
    EXPECT_EQ(s0.counters.dep_tests, 0);
}

// Three rows seeded active in R^2 (an integer over-count): removals run until
// the set is independent.
TEST_P(WsIndependence, EntryDetectorClearsOverCount) {
    Eigen::MatrixXd Ai(3, 2);
    Ai << 1, 1, 1, -1, 3, 1;
    Vec g(2);
    g << -1, -2;
    Vec bi(3);
    bi << 1, 0, 2;
    const QpProblem qp = make_qp(Eigen::MatrixXd::Identity(2, 2), g, Eigen::MatrixXd(0, 2), Vec(0),
                                 Ai, bi, -10.0, 10.0);
    QpSolution seed;
    seed.x = Vec::Constant(2, 0.5);
    seed.bound_state.assign(2, BoundState::kFree);
    seed.ineq_active = {true, true, true};

    const QpEngine on(opts_for(GetParam()));
    const QpSolution s = on.solve(qp, seed);
    ASSERT_EQ(s.status, QpStatus::kOptimal);
    EXPECT_EQ(s.counters.entry_dependency_removals, 1);
    const QpEngine off(opts_for(GetParam(), false));
    const QpSolution s0 = off.solve(qp, seed);
    ASSERT_EQ(s0.status, QpStatus::kOptimal);
    EXPECT_NEAR((s.x - s0.x).lpNorm<Eigen::Infinity>(), 0.0, 1e-7);
}

// A duplicated EQUALITY (mandatory): counted, never removed, solve proceeds.
TEST_P(WsIndependence, MandatoryDependencyIsCountedNotRemoved) {
    Eigen::MatrixXd Ae(2, 3);
    Ae << 1, 1, 1, 2, 2, 2;
    Vec be(2);
    be << 1, 2;
    const QpProblem qp = make_qp(Eigen::MatrixXd::Identity(3, 3), Vec::Zero(3), Ae, be,
                                 Eigen::MatrixXd(0, 3), Vec(0), -10.0, 10.0);
    const QpEngine on(opts_for(GetParam()));
    const QpSolution s = on.solve(qp);
    ASSERT_EQ(s.status, QpStatus::kOptimal);
    EXPECT_EQ(s.counters.mandatory_dependent_entries, 1);
    EXPECT_EQ(s.counters.entry_dependency_removals, 0);
    for (Index i = 0; i < 3; ++i) {
        EXPECT_NEAR(s.x(i), 1.0 / 3.0, 1e-7);
    }
}

// A clean start: the detector runs, finds nothing, removes nothing.
TEST_P(WsIndependence, CleanStartIsUntouched) {
    Eigen::MatrixXd Ai(1, 2);
    Ai << 1, 1;
    Vec g(2);
    g << -1, -2;
    Vec bi(1);
    bi << 1;
    const QpProblem qp = make_qp(Eigen::MatrixXd::Identity(2, 2), g, Eigen::MatrixXd(0, 2), Vec(0),
                                 Ai, bi, 0.0, 10.0);
    const QpEngine on(opts_for(GetParam()));
    const QpEngine off(opts_for(GetParam(), false));
    const QpSolution s = on.solve(qp);
    const QpSolution s0 = off.solve(qp);
    ASSERT_EQ(s.status, QpStatus::kOptimal);
    EXPECT_EQ(s.counters.entry_dependency_removals, 0);
    EXPECT_EQ(s.counters.mandatory_dependent_entries, 0);
    EXPECT_EQ(s.x, s0.x);
    EXPECT_EQ(s.counters.minor_iters, s0.counters.minor_iters);
}

// E2 fix1, D2: the equality chain x0 - x1 = 0, x1 - x2 = 0 with x0 FIXED at 0
// and x2's lower pin (optional) is dependent: eq0 + eq1 - e0 + e2 = 0. On the
// eliminated path the pins are not constraint components of the factor; the
// support must still name them (their coefficient is A_W^T y_hat at the pinned
// column), or the dependency is attributed to the two MANDATORY equalities and
// nothing is removed. Seeded, the entry detector removes x2's pin -- the fixed
// pin and the equalities are mandatory -- in both algebras (the border path's
// pins were always ledger components), and the walk ends at (0, 0, 0, 1) with
// O(1) multipliers.
QpProblem chain_qp() {
    Eigen::MatrixXd Ae(2, 4);
    Ae << 1, -1, 0, 0, 0, 1, -1, 0;
    Vec g(4);
    g << 0, 0, 1, -1;
    QpProblem qp = make_qp(Eigen::MatrixXd::Identity(4, 4), g, Ae, Vec::Zero(2),
                           Eigen::MatrixXd(0, 4), Vec(0), -1.0, 1.0);
    qp.lower(0) = 0.0;
    qp.upper(0) = 0.0;
    qp.lower(2) = 0.0;
    return qp;
}

TEST_P(WsIndependence, AnOptionalPinInADependentEqualityChainIsRemoved) {
    const QpProblem qp = chain_qp();
    QpSolution seed;
    seed.x = Vec::Zero(4);
    seed.bound_state = {BoundState::kFixed, BoundState::kFree, BoundState::kAtLower,
                        BoundState::kFree};
    const QpEngine on(opts_for(GetParam()));
    const QpSolution s = on.solve(qp, seed);
    ASSERT_EQ(s.status, QpStatus::kOptimal);
    EXPECT_EQ(s.counters.entry_dependency_removals, 1);
    EXPECT_EQ(s.counters.mandatory_dependent_entries, 0);
    EXPECT_EQ(s.bound_state[0], BoundState::kFixed);
    EXPECT_EQ(s.bound_state[2], BoundState::kFree);
    EXPECT_NEAR(s.x(0), 0.0, 1e-12);
    EXPECT_NEAR(s.x(1), 0.0, 1e-12);
    EXPECT_NEAR(s.x(2), 0.0, 1e-12);
    EXPECT_NEAR(s.x(3), 1.0, 1e-12);
    // No 1/mu growth: every multiplier is O(|g|).
    EXPECT_LE(s.lambda_e.lpNorm<Eigen::Infinity>(), 10.0);
    EXPECT_LE(s.z.lpNorm<Eigen::Infinity>(), 10.0);
}

TEST(WsIndependenceDetector, TheEliminatedSupportNamesThePinsTheDependencyUses) {
    const QpProblem qp = chain_qp();
    QpOptions o;
    WorkingSet ws(4, 0);
    ws.bound_state()[0] = BoundState::kFixed;
    ws.bound_state()[2] = BoundState::kAtLower;
    detail::KktFactor kkt;
    (void)solve_eqp(qp, ws, kkt, o);
    detail::DepFactorView v;
    v.eliminated = &kkt;
    const detail::EntryDetection d = detail::detect_dependency(qp, ws, v, o);
    ASSERT_TRUE(d.found);
    EXPECT_LE(d.null_residual, detail::kDepTol);
    bool eq0 = false, eq1 = false, pin0 = false, pin2 = false, other = false;
    for (const detail::WsMember &m : d.support) {
        using K = detail::WsMember::Kind;
        eq0 = eq0 || (m.kind == K::kEq && m.idx == 0);
        eq1 = eq1 || (m.kind == K::kEq && m.idx == 1);
        pin0 = pin0 || (m.kind == K::kPin && m.idx == 0);
        pin2 = pin2 || (m.kind == K::kPin && m.idx == 2);
        other = other || (m.kind == K::kPin && m.idx != 0 && m.idx != 2) || m.kind == K::kRow;
    }
    EXPECT_TRUE(eq0 && eq1);
    EXPECT_TRUE(pin0) << "the fixed pin carries a coefficient";
    EXPECT_TRUE(pin2) << "the optional pin that makes the set dependent is named";
    EXPECT_FALSE(other);
}

// E2 fix1b (E2-D2): THE REMOVAL CERTIFICATE. Two working rows on x0, x1 that
// are dependent to eta = 1e-7 on the free columns, each touching the pinned
// x2 with +-kEps = 1e-5. y_hat = (1, -1)/sqrt2 has residual ~eta/sqrt2 (a
// found dependency) and gives x2's pin the coefficient ~sqrt2 * kEps: above
// kDepCoeffTol of the largest, so it is in the dependency's support, but it
// proves the pin dependent only to rho ~ eta / (2 kEps) = 5e-3 > kDepRemovalTol.
// The f7_n800_path_warm shape: a pin in the support by a small coefficient,
// removed newest-first, re-blocked at zero motion and ended the solve.
QpProblem near_dependent_rows_qp(bool as_equalities, double b = 1.0) {
    constexpr double kEps = 1e-5;
    constexpr double kEta = 1e-7;
    Eigen::MatrixXd A(2, 3);
    A << 1, 0, kEps, 1, kEta, -kEps;
    Vec g(3);
    g << -5, 0, 1;
    const Eigen::MatrixXd none(0, 3);
    QpProblem qp = as_equalities ? make_qp(Eigen::MatrixXd::Identity(3, 3), g, A,
                                           Vec::Constant(2, b), none, Vec(0), -10.0, 10.0)
                                 : make_qp(Eigen::MatrixXd::Identity(3, 3), g, none, Vec(0), A,
                                           Vec::Constant(2, b), -10.0, 10.0);
    qp.lower(2) = 0.0;
    qp.upper(2) = 1.0;
    return qp;
}

detail::EntryDetection detect_on(const QpProblem &qp, const WorkingSet &ws) {
    QpOptions o;
    detail::KktFactor kkt;
    (void)solve_eqp(qp, ws, kkt, o);
    detail::DepFactorView v;
    v.eliminated = &kkt;
    return detail::detect_dependency(qp, ws, v, o);
}

TEST(WsIndependenceDetector, APinInTheSupportByASmallCoefficientIsNotOfferedForRemoval) {
    const QpProblem qp = near_dependent_rows_qp(/*as_equalities=*/false);
    WorkingSet ws(3, 2);
    ws.add_ineq(0);
    ws.add_ineq(1);
    ws.bound_state()[2] = BoundState::kAtLower;
    const detail::EntryDetection d = detect_on(qp, ws);
    ASSERT_TRUE(d.found);
    EXPECT_LE(d.null_residual, detail::kDepTol);
    bool row0 = false, row1 = false, pin2 = false;
    for (const detail::WsMember &m : d.support) {
        using K = detail::WsMember::Kind;
        row0 = row0 || (m.kind == K::kRow && m.idx == 0);
        row1 = row1 || (m.kind == K::kRow && m.idx == 1);
        pin2 = pin2 || (m.kind == K::kPin && m.idx == 2);
    }
    EXPECT_TRUE(row0 && row1) << "both rows are certified (rho ~ eta)";
    EXPECT_FALSE(pin2) << "the pin clears kDepCoeffTol but not the removal certificate";
    EXPECT_TRUE(d.uncertified_dropped);
    EXPECT_FALSE(d.best_certified_fallback);
}

TEST(WsIndependenceDetector, WithNoRemovableMemberCertifiedTheBestCertifiedOneIsOfferedAlone) {
    // The same rows as MANDATORY equalities: the pin is the only removable
    // member and is not certified, so it is offered alone (the E2 fix1 D2
    // behaviour: a dependency is never attributed to mandatory rows only).
    const QpProblem qp = near_dependent_rows_qp(/*as_equalities=*/true);
    WorkingSet ws(3, 0);
    ws.bound_state()[2] = BoundState::kAtLower;
    const detail::EntryDetection d = detect_on(qp, ws);
    ASSERT_TRUE(d.found);
    Index pins = 0, eqs = 0;
    for (const detail::WsMember &m : d.support) {
        pins += m.kind == detail::WsMember::Kind::kPin && m.idx == 2 ? 1 : 0;
        eqs += m.kind == detail::WsMember::Kind::kEq ? 1 : 0;
    }
    EXPECT_EQ(pins, 1);
    EXPECT_EQ(eqs, 2);
    EXPECT_TRUE(d.uncertified_dropped);
    EXPECT_TRUE(d.best_certified_fallback);
}

// The same rows, violated at the seed (b = -1, x = 0), so both enter as shift
// adds (entry key 2) while x2's seeded pin has key 1: without the removal
// certificate the entry detector removes the pin, which y_hat does not prove
// dependent. With it a certified row goes, the pin stays, and the walk ends at
// the optimum (-1, 0, 0) with O(1) multipliers: lambda_row = 6, z_2 = 1 + 6e-5.
TEST_P(WsIndependence, AnUncertifiedPinIsKeptAndACertifiedRowRemoved) {
    const QpProblem qp = near_dependent_rows_qp(/*as_equalities=*/false, -1.0);
    QpSolution seed;
    seed.x = Vec::Zero(3);
    seed.bound_state = {BoundState::kFree, BoundState::kFree, BoundState::kAtLower};
    const QpEngine on(opts_for(GetParam()));
    const QpSolution s = on.solve(qp, seed);
    ASSERT_EQ(s.status, QpStatus::kOptimal);
    EXPECT_EQ(s.counters.shift_adds, 2);
    EXPECT_EQ(s.counters.entry_dependency_removals, 1);
    EXPECT_EQ(s.counters.dep_uncertified_trims, 1);
    EXPECT_EQ(s.counters.dep_certified_fallbacks, 0);
    EXPECT_EQ(s.bound_state[2], BoundState::kAtLower) << "the uncertified pin was not removed";
    EXPECT_NEAR(s.x(0), -1.0, 1e-12);
    EXPECT_NEAR(s.x(1), 0.0, 1e-12);
    EXPECT_NEAR(s.x(2), 0.0, 1e-12);
    // First order, recomputed: H x + g + Ai^T lambda_i - z e_2 = 0, lambda, z >= 0.
    const Vec r = qp.H.selfadjointView<Eigen::Upper>() * s.x + qp.g +
                  Eigen::MatrixXd(qp.Ai).transpose() * s.lambda_i;
    EXPECT_NEAR(r(0), 0.0, 1e-9);
    EXPECT_NEAR(r(1), 0.0, 1e-9);
    EXPECT_NEAR(r(2), s.z(2), 1e-9);
    EXPECT_GE(s.lambda_i.minCoeff(), 0.0);
    EXPECT_GE(s.z(2), 0.0);
    EXPECT_LE(s.lambda_i.lpNorm<Eigen::Infinity>(), 10.0);
    EXPECT_LE(s.z.lpNorm<Eigen::Infinity>(), 10.0);
}

INSTANTIATE_TEST_SUITE_P(BothAlgebras, WsIndependence,
                         ::testing::Values(WorkingSetLinearAlgebra::kSchurBorder,
                                           WorkingSetLinearAlgebra::kRefactorize));

// F7 (eliminated view): the refined statistic separates dependent from
// independent candidates across mu and row scale, with the declared kDepTol.
TEST(WsIndependenceCalibration, RefinedStatisticSeparates) {
    for (const double mu : {1e-8, 1e-10, 1e-12}) {
        for (const double scale : {1.0, 1e5}) {
            Eigen::MatrixXd Ai(2, 3);
            Ai << scale, scale, 0, 0, 0, 1;
            Vec bi(2);
            bi << scale, 1.0;
            const QpProblem qp = make_qp(Eigen::MatrixXd::Identity(3, 3), Vec::Zero(3),
                                         Eigen::MatrixXd(0, 3), Vec(0), Ai, bi, -10.0, 10.0);
            QpOptions o;
            o.dual_mu = mu;
            WorkingSet ws(3, 2);
            ws.add_ineq(0);
            detail::KktFactor kkt;
            (void)solve_eqp(qp, ws, kkt, o);
            detail::DepFactorView v;
            v.eliminated = &kkt;

            Vec dep(3);
            dep << 2.0, 2.0, 0.0;
            const detail::DepTest td = detail::dependency_test(qp, ws, v, dep, o);
            EXPECT_EQ(td.verdict, detail::DepVerdict::kDependent) << mu << " " << scale;
            EXPECT_LT(td.rho, 1e-8) << mu << " " << scale;
            EXPECT_NEAR(td.v_w(0) * scale, 2.0, 1e-6);
            EXPECT_GE(td.solves, 1);
            EXPECT_LE(td.solves, 1 + detail::kDepRefineSteps);

            Vec ind(3);
            ind << 1.0, 0.0, 0.0;
            const detail::DepTest ti = detail::dependency_test(qp, ws, v, ind, o);
            EXPECT_EQ(ti.verdict, detail::DepVerdict::kIndependent) << mu << " " << scale;
            EXPECT_GT(ti.rho, 1e-2);
        }
    }
}

// A bordered view on min 1/2||x||^2 in R^3 with row 0 = scale*(1,1,0) bordered
// into an empty-row K0 (the border coefficient map), for F7 and F11.
struct BorderedView {
    QpProblem qp;
    QpOptions o;
    WorkingSet ws{3, 2};
    detail::KktFactor kkt0;
    KktAssembly k0;
    std::vector<Index> k0_rows;
    std::vector<BorderLedgerEntry> ledger;
    std::optional<SchurComplement> schur;

    BorderedView(double mu, double scale) {
        Eigen::MatrixXd Ai(2, 3);
        Ai << scale, scale, 0, 0, 0, 1;
        Vec bi(2);
        bi << scale, 1.0;
        qp = make_qp(Eigen::MatrixXd::Identity(3, 3), Vec::Zero(3), Eigen::MatrixXd(0, 3), Vec(0),
                     Ai, bi, -10.0, 10.0);
        o.dual_mu = mu;
        k0 = assemble_kkt_full(qp, ws, o);
        detail::factorize_checked(kkt0, k0.K);
        schur.emplace(kkt0, o);
        schur->add_border(BorderOps::add_ineq_row(qp, 0, k0.K.rows()), -o.dual_mu);
        ledger.push_back({BorderLedgerEntry::Kind::kIneqRow, 0});
        ws.add_ineq(0);
    }

    detail::DepFactorView view() const {
        detail::DepFactorView v;
        v.k0 = &k0;
        v.k0_rows = &k0_rows;
        v.ledger = &ledger;
        v.schur = &*schur;
        return v;
    }
};

// F7 (bordered view): the same separation through the Schur border stack.
TEST(WsIndependenceCalibration, RefinedStatisticSeparatesBordered) {
    for (const double mu : {1e-8, 1e-10, 1e-12}) {
        for (const double scale : {1.0, 1e5}) {
            const BorderedView b(mu, scale);
            Vec dep(3);
            dep << 2.0, 2.0, 0.0;
            const detail::DepTest td = detail::dependency_test(b.qp, b.ws, b.view(), dep, b.o);
            EXPECT_EQ(td.verdict, detail::DepVerdict::kDependent) << mu << " " << scale;
            EXPECT_LT(td.rho, 1e-8) << mu << " " << scale;
            EXPECT_NEAR(td.v_w(0) * scale, 2.0, 1e-6);
            EXPECT_LE(td.solves, 1 + detail::kDepRefineSteps);

            Vec ind(3);
            ind << 1.0, 0.0, 0.0;
            const detail::DepTest ti = detail::dependency_test(b.qp, b.ws, b.view(), ind, b.o);
            EXPECT_EQ(ti.verdict, detail::DepVerdict::kIndependent) << mu << " " << scale;
            EXPECT_GT(ti.rho, 1e-2);
        }
    }
}

// F11: the test and the detector are read-only on both views: factor identity,
// ledger and border dimension are unchanged.
TEST(WsIndependenceReadOnly, TestAndDetectorWriteNothing) {
    BorderedView b(1e-8, 1.0);
    const auto sid = b.kkt0.factor.session_id();
    const auto ep = b.kkt0.factor.epoch();
    const Index dim = b.schur->dim();
    const auto ledger = b.ledger;
    Vec a(3);
    a << 1.0, 1.0, 0.0;
    (void)detail::dependency_test(b.qp, b.ws, b.view(), a, b.o);
    (void)detail::detect_dependency(b.qp, b.ws, b.view(), b.o);
    EXPECT_EQ(b.kkt0.factor.session_id(), sid);
    EXPECT_EQ(b.kkt0.factor.epoch(), ep);
    EXPECT_EQ(b.schur->dim(), dim);
    EXPECT_EQ(b.ledger.size(), ledger.size());

    detail::KktFactor kkt;
    (void)solve_eqp(b.qp, b.ws, kkt, b.o);
    detail::DepFactorView v;
    v.eliminated = &kkt;
    const auto sid2 = kkt.factor.session_id();
    const auto ep2 = kkt.factor.epoch();
    (void)detail::dependency_test(b.qp, b.ws, v, a, b.o);
    (void)detail::detect_dependency(b.qp, b.ws, v, b.o);
    EXPECT_EQ(kkt.factor.session_id(), sid2);
    EXPECT_EQ(kkt.factor.epoch(), ep2);
}

// A shift add dependent on the set: the re-check removes it and refuses it,
// and the walk still ends. x0 + x1 <= 1 is seeded; the start (1, 1) violates
// the duplicate 2x0 + 2x1 <= 2, which the homotopy would admit.
TEST_P(WsIndependence, DependentShiftAddIsRefused) {
    Eigen::MatrixXd Ai(2, 2);
    Ai << 1, 1, 2, 2;
    Vec g(2);
    g << -1, -2;
    Vec bi(2);
    bi << 1, 2;
    const QpProblem qp = make_qp(Eigen::MatrixXd::Identity(2, 2), g, Eigen::MatrixXd(0, 2), Vec(0),
                                 Ai, bi, 0.0, 10.0);
    QpSolution seed;
    seed.x = Vec::Constant(2, 1.0);
    seed.bound_state.assign(2, BoundState::kFree);
    seed.ineq_active = {true, false};
    const QpEngine on(opts_for(GetParam()));
    const QpSolution s = on.solve(qp, seed);
    ASSERT_EQ(s.status, QpStatus::kOptimal);
    EXPECT_GE(s.counters.shift_adds, 1);
    EXPECT_GE(s.counters.entry_dependency_removals + s.counters.dep_recheck_removals, 1);
    EXPECT_NEAR(s.x(0), 0.0, 1e-8);
    EXPECT_NEAR(s.x(1), 1.0, 1e-8);
}

} // namespace
