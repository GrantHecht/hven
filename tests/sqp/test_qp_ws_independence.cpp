// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M7 W2 E2 step 1: the working-set dependency test and the entry detector
// (qp/ws_independence.h), on constructed QPs, in both ws_algebra modes.

#include <cmath>
#include <initializer_list>
#include <optional>
#include <utility>
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
    (void)solve_eqp(qp, ws, Vec::Zero(qp.n()), kkt, o);
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
    (void)solve_eqp(qp, ws, Vec::Zero(qp.n()), kkt, o);
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
    EXPECT_FALSE(d.uncertified_fallback);
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
    EXPECT_TRUE(d.uncertified_fallback);
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
    EXPECT_EQ(s.counters.dep_uncertified_fallbacks, 0);
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
            (void)solve_eqp(qp, ws, Vec::Zero(qp.n()), kkt, o);
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

// ---------------------------------------------------------------------------
// E2 fix2 (settler S23, S24): the dependency test's landed contract, on both
// algebras. `rows_view` factors K0 over `k0_rows` and borders `border_rows`
// onto it (the Schur path), or factors the eliminated KKT over every row.
// ---------------------------------------------------------------------------

struct RowsView {
    QpProblem qp;
    QpOptions o;
    WorkingSet ws;
    bool bordered = false;
    detail::KktFactor kkt; // eliminated path, or K0 on the border path
    KktAssembly k0;
    std::vector<Index> k0_rows;
    std::vector<BorderLedgerEntry> ledger;
    std::optional<SchurComplement> schur;

    RowsView(QpProblem problem, double mu, const std::vector<Index> &k0_in,
             const std::vector<Index> &border_rows, bool on_border)
        : qp(std::move(problem)), ws(qp.n(), qp.mi()), bordered(on_border) {
        o.dual_mu = mu;
        for (const Index r : k0_in) {
            ws.add_ineq(r);
        }
        if (!bordered) {
            for (const Index r : border_rows) {
                ws.add_ineq(r);
            }
            (void)solve_eqp(qp, ws, Vec::Zero(qp.n()), kkt, o);
            return;
        }
        k0 = assemble_kkt_full(qp, ws, o);
        k0_rows = ws.active_ineq();
        detail::factorize_checked(kkt, k0.K);
        schur.emplace(kkt, o);
        for (const Index r : border_rows) {
            schur->add_border(BorderOps::add_ineq_row(qp, r, k0.K.rows()), -o.dual_mu);
            ledger.push_back({BorderLedgerEntry::Kind::kIneqRow, r});
            ws.add_ineq(r);
        }
    }

    detail::DepFactorView view() const {
        detail::DepFactorView v;
        if (bordered) {
            v.k0 = &k0;
            v.k0_rows = &k0_rows;
            v.ledger = &ledger;
            v.schur = &*schur;
        } else {
            v.eliminated = &kkt;
        }
        return v;
    }

    detail::DepTest test(const Vec &a) const {
        return detail::dependency_test(qp, ws, view(), a, o);
    }

    // R-E's re-read (E2 fix6): the refinement run to its end on the same factor.
    detail::DepTest reread(const Vec &a) const {
        return detail::certificate_coefficients(qp, ws, view(), a, o);
    }
};

Vec vec_of(std::initializer_list<double> v) {
    Vec out(static_cast<Index>(v.size()));
    Index i = 0;
    for (const double x : v) {
        out(i++) = x;
    }
    return out;
}

// S23: calibration at a SAWPATH-scale row ratio, both rows IN the working set.
// Row 0 = s (1, 1, 0, 0) with s = 1e10, row 1 = (0, 0, 1, 0), n = 4, H = I.
// The candidates are exact combinations or exact complements, so the verdicts
// are properties of the data, not of the factor:
//   (2, 2, 3, 0) = (2/s) row0 + 3 row1        -> dependent, v = (2/s, 3)
//   (s, s, 1, 0) = row0 + row1                 -> dependent, v = (1, 1)
//   (1, 0, 0, 0): component (1/2, -1/2, 0, 0)  -> independent, rho = 1/sqrt2
//   (0, 0, 0, 1): orthogonal to both rows      -> independent, rho = 1
// Row 0 is in K0 or bordered onto a K0 holding row 1 (the border path), and
// the eliminated path factors both.
QpProblem ratio_rows_qp(double s) {
    Eigen::MatrixXd Ai(2, 4);
    Ai << s, s, 0, 0, 0, 0, 1, 0;
    return make_qp(Eigen::MatrixXd::Identity(4, 4), Vec::Zero(4), Eigen::MatrixXd(0, 4), Vec(0), Ai,
                   vec_of({s, 1.0}), -10.0, 10.0);
}

TEST(WsIndependenceCalibration, RefinedStatisticSeparatesAtARowRatioOf1e10BothAlgebras) {
    constexpr double kS = 1e10;
    for (const bool border : {false, true}) {
        for (const double mu : {1e-8, 1e-10, 1e-12}) {
            SCOPED_TRACE(::testing::Message()
                         << (border ? "border" : "eliminated") << " mu " << mu);
            const RowsView rv(ratio_rows_qp(kS), mu, {1}, {0}, border);
            const detail::DepTest d1 = rv.test(vec_of({2.0, 2.0, 3.0, 0.0}));
            EXPECT_EQ(d1.verdict, detail::DepVerdict::kDependent);
            EXPECT_LT(d1.rho, 1e-8);
            EXPECT_FALSE(d1.stall_admit);
            EXPECT_NEAR(d1.v_w(0) * kS, 2.0, 1e-6);
            EXPECT_NEAR(d1.v_w(1), 3.0, 1e-6);
            EXPECT_LE(d1.solves, 1 + detail::kDepRefineSteps);

            const detail::DepTest d2 = rv.test(vec_of({kS, kS, 1.0, 0.0}));
            EXPECT_EQ(d2.verdict, detail::DepVerdict::kDependent);
            EXPECT_LT(d2.rho, 1e-8);
            EXPECT_NEAR(d2.v_w(0), 1.0, 1e-6);
            EXPECT_NEAR(d2.v_w(1), 1.0, 1e-6);

            // Observed on MKL, both algebras, every mu: this candidate's refinement
            // stalls (the 1e10 row puts the residual's rounding floor above the
            // unscaled target), so it is admitted through the stall branch and
            // would count dep_stall_admits. The verdict and rho are exact either
            // way; the flag is not pinned (a backend's rounding decides it).
            const detail::DepTest i1 = rv.test(vec_of({1.0, 0.0, 0.0, 0.0}));
            EXPECT_EQ(i1.verdict, detail::DepVerdict::kIndependent);
            EXPECT_NEAR(i1.rho, std::sqrt(0.5), 1e-6);

            const detail::DepTest i2 = rv.test(vec_of({0.0, 0.0, 0.0, 1.0}));
            EXPECT_EQ(i2.verdict, detail::DepVerdict::kIndependent);
            EXPECT_NEAR(i2.rho, 1.0, 1e-6);
            EXPECT_FALSE(i2.stall_admit);
        }
    }
}

// S23: a DELIBERATELY STALLED refinement with rho > kDepUndecidable. The
// working rows (1, 0, 0) and (1, sigma, 0), sigma = 1e-5, have sigma_min^2 ~
// 5e-11 < mu = 1e-8, so the mu-regularized solve contracts the refinement by
// only ~1 - sigma^2/mu per step and kDepRefineSteps steps cannot reach the
// target. The candidate (0, 1, 1) needs the coefficient 1/sigma on the weak
// direction; its e2 part is outside the rows' span, so rho >= 1/sqrt2 > 1e-3
// whatever the coefficients: ADMITTED as independent, flagged stall_admit.
QpProblem stalled_rows_qp() {
    constexpr double kSigma = 1e-5;
    Eigen::MatrixXd Ai(3, 3);
    Ai << 1, 0, 0, 1, kSigma, 0, 0, 1, 1;
    Vec g(3);
    g << -2.0, -(1.0 + kSigma), -2.0;
    return make_qp(Eigen::MatrixXd::Identity(3, 3), g, Eigen::MatrixXd(0, 3), Vec(0), Ai,
                   vec_of({0.0, 0.0, 1.0}), -10.0, 10.0);
}

TEST(WsIndependenceCalibration, AStalledRefinementAboveTheUndecidableBandIsAdmittedAndFlagged) {
    for (const bool border : {false, true}) {
        SCOPED_TRACE(border ? "border" : "eliminated");
        const RowsView rv(stalled_rows_qp(), 1e-8, {0}, {1}, border);
        const detail::DepTest t = rv.test(vec_of({0.0, 1.0, 1.0}));
        EXPECT_EQ(t.verdict, detail::DepVerdict::kIndependent);
        EXPECT_TRUE(t.stall_admit) << "the refinement converged: the fixture no longer stalls";
        EXPECT_GE(t.rho, detail::kDepUndecidable);
        EXPECT_EQ(t.solves, 1 + detail::kDepRefineSteps);
    }
}

// S23 end to end: seeded with rows 0 and 1 active at x = 0, the step on their
// face runs along e2 and row 2 = (0, 1, 1) blocks at x2 = 1. Its test stalls
// (above) with rho >= 1e-3: admitted and counted. The answer is the vertex
// (0, 0, 1) with lambda = (1, 1, 1) (g was chosen for it), checked here from the
// data.
TEST_P(WsIndependence, AStalledTestAboveTheUndecidableBandIsAdmittedAndCounted) {
    const QpProblem qp = stalled_rows_qp();
    QpSolution seed;
    seed.x = Vec::Zero(3);
    seed.bound_state.assign(3, BoundState::kFree);
    seed.ineq_active = {true, true, false};
    const QpEngine on(opts_for(GetParam()));
    const QpSolution s = on.solve(qp, seed);
    ASSERT_EQ(s.status, QpStatus::kOptimal);
    EXPECT_GE(s.counters.dep_stall_admits, 1);
    EXPECT_EQ(s.counters.dep_undecidable, 0);
    EXPECT_TRUE(s.ineq_active[2]);
    EXPECT_NEAR(s.x(0), 0.0, 1e-9);
    EXPECT_NEAR(s.x(1), 0.0, 1e-9);
    EXPECT_NEAR(s.x(2), 1.0, 1e-9);
    const Vec r = qp.H.selfadjointView<Eigen::Upper>() * s.x + qp.g +
                  Eigen::MatrixXd(qp.Ai).transpose() * s.lambda_i;
    EXPECT_LE(r.lpNorm<Eigen::Infinity>(), 1e-6);
    EXPECT_GE(s.lambda_i.minCoeff(), -1e-9);
    EXPECT_LE((Eigen::MatrixXd(qp.Ai) * s.x - qp.bi).maxCoeff(), 1e-9);
}

// S24: kDepRemovalTol is NOT the candidate test's boundary. A CONVERGED
// candidate with kDepTol < rho < kDepUndecidable is independent. Working row
// (1, 0, 0); candidates (1, e, 0) have rho = e / sqrt(1 + e^2) exactly.
TEST(WsIndependenceCalibration, AConvergedRhoBetweenKDepTolAndKDepUndecidableIsIndependent) {
    Eigen::MatrixXd Ai(2, 3);
    Ai << 1, 0, 0, 0, 1, 0;
    const QpProblem qp =
        make_qp(Eigen::MatrixXd::Identity(3, 3), Vec::Zero(3), Eigen::MatrixXd(0, 3), Vec(0), Ai,
                vec_of({1.0, 1.0}), -10.0, 10.0);
    for (const bool border : {false, true}) {
        const RowsView rv(qp, 1e-8, {}, {0}, border);
        for (const double e : {1e-5, 1e-4, 5e-4}) {
            SCOPED_TRACE(::testing::Message() << (border ? "border" : "eliminated") << " e " << e);
            const detail::DepTest t = rv.test(vec_of({1.0, e, 0.0}));
            EXPECT_EQ(t.verdict, detail::DepVerdict::kIndependent);
            EXPECT_FALSE(t.stall_admit) << "converged";
            EXPECT_GT(t.rho, detail::kDepTol);
            EXPECT_LT(t.rho, detail::kDepUndecidable);
            EXPECT_NEAR(t.rho, e / std::sqrt(1.0 + e * e), 1e-9);
        }
    }
}

// R-E (E2 fix6, settler S33): the grey re-read never flips a candidate farther than kDepTol from
// the span, and it certifies a grey dependent. For EVERY coefficient vector v, rho(v) = ||(a -
// A_W^T v)_free|| / ||a|| >= the true relative distance, so the re-read rho' (plus its rounding
// floor, kDepRereadKappa) <= kDepTol is a certificate. (i) S24's candidates (1, e, 0) at exact
// relative distance e / sqrt(1 + e^2) >= 1e-5 re-read to that distance and stay independent.
// (ii) A candidate that IS a multiple of the one working row, on a stiff face (H = 1e4 I,
// mu = 1e-8): the regularized solve leaves A_W u = mu v, whose residual sits below the target
// after the first solve, so the test "converges" at once -- but rho reads (H + delta) u ~ 1e4 mu
// and lands in the grey band. Run to its end the refinement contracts u by ~mu per step and
// rho' falls below 1e-12: dependent, certified.
TEST(WsIndependenceCalibration, AGreyReReadNeverFlipsACandidateAboveKDepTol) {
    Eigen::MatrixXd Ai(2, 3);
    Ai << 1, 0, 0, 0, 1, 0;
    const QpProblem qp =
        make_qp(Eigen::MatrixXd::Identity(3, 3), Vec::Zero(3), Eigen::MatrixXd(0, 3), Vec(0), Ai,
                vec_of({1.0, 1.0}), -10.0, 10.0);
    Eigen::MatrixXd Ai_stiff(2, 3);
    Ai_stiff << 1, 1, 0, 0, 0, 1;
    const QpProblem stiff =
        make_qp(1e4 * Eigen::MatrixXd::Identity(3, 3), Vec::Zero(3), Eigen::MatrixXd(0, 3), Vec(0),
                Ai_stiff, vec_of({1.0, 1.0}), -10.0, 10.0);
    for (const bool border : {false, true}) {
        const RowsView rv(qp, 1e-8, {}, {0}, border);
        for (const double e : {1e-5, 1e-4, 5e-4}) {
            SCOPED_TRACE(::testing::Message() << (border ? "border" : "eliminated") << " e " << e);
            const Vec a = vec_of({1.0, e, 0.0});
            const detail::DepTest r = rv.reread(a);
            const double dist = e / std::sqrt(1.0 + e * e);
            EXPECT_GE(r.rho, dist * (1.0 - 1e-9));
            EXPECT_GT(r.rho + detail::dependency_rounding_floor(rv.qp, rv.ws, r, a),
                      detail::kDepTol);
        }
        SCOPED_TRACE(border ? "stiff border" : "stiff eliminated");
        const RowsView sv(stiff, 1e-8, {}, {0}, border);
        const Vec a = vec_of({3.0, 3.0, 0.0});
        const detail::DepTest t = sv.test(a);
        EXPECT_EQ(t.verdict, detail::DepVerdict::kIndependent);
        EXPECT_FALSE(t.stall_admit) << "converged";
        EXPECT_GT(t.rho, detail::kDepTol);
        EXPECT_LT(t.rho, detail::kDepUndecidable);
        const detail::DepTest r = sv.reread(a);
        EXPECT_LE(r.rho, 1e-12);
        EXPECT_LE(r.rho + detail::dependency_rounding_floor(sv.qp, sv.ws, r, a), detail::kDepTol);
        EXPECT_NEAR(r.v_w(0), 3.0, 1e-9);
        EXPECT_LE(r.solves, 1 + detail::kDepRefineSteps);
    }
}

// S24 end to end: THE HEURISTIC FALLBACK. near_dependent_rows_qp as mandatory
// equalities with x2's lower pin seeded: the pin is the only removable member
// and y_hat does not certify it, so the uncertified pin is offered alone and
// removed (dep_uncertified_fallbacks). The QP's optimum is (1, 0, 0) with
// lambda_e = (4, 0), z_2 = 1 + 4e-5 (x2 = 0 is optimal: x1 = 200 x2 on the
// equalities, and d f / d x2 > 0 there). The walk must still reach it: the
// status and the first-order residual are recomputed from the data.
TEST_P(WsIndependence, TheUncertifiedFallbackStillEndsAtAFirstOrderPoint) {
    const QpProblem qp = near_dependent_rows_qp(/*as_equalities=*/true);
    QpSolution seed;
    seed.x = Vec::Zero(3);
    seed.bound_state = {BoundState::kFree, BoundState::kFree, BoundState::kAtLower};
    const QpEngine on(opts_for(GetParam()));
    const QpSolution s = on.solve(qp, seed);
    EXPECT_GE(s.counters.dep_uncertified_fallbacks, 1);
    ASSERT_EQ(s.status, QpStatus::kOptimal);
    // x1 is fixed only through the 1e-7 coefficient, so it carries rounding / 1e-7.
    EXPECT_NEAR(s.x(0), 1.0, 1e-9);
    EXPECT_NEAR(s.x(1), 0.0, 1e-6);
    EXPECT_NEAR(s.x(2), 0.0, 1e-9);
    // H x + g + Ae^T lambda_e - z = 0, z_2 >= 0 at the lower bound, Ae x = be.
    const Vec r = qp.H.selfadjointView<Eigen::Upper>() * s.x + qp.g +
                  Eigen::MatrixXd(qp.Ae).transpose() * s.lambda_e - s.z;
    EXPECT_LE(r.lpNorm<Eigen::Infinity>(), 1e-6);
    EXPECT_LE((Eigen::MatrixXd(qp.Ae) * s.x - qp.be).lpNorm<Eigen::Infinity>(), 1e-9);
    EXPECT_EQ(s.z(0), 0.0);
    EXPECT_EQ(s.z(1), 0.0);
    EXPECT_GE(s.z(2), 0.0);
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
    (void)solve_eqp(b.qp, b.ws, Vec::Zero(b.qp.n()), kkt, b.o);
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

// ===========================================================================
// M7 W2 E2 FIX ROUND 7 (settler E4-37(b)): AN UN-RUN TEST IS NOT A VERDICT OF
// DEPENDENCE. A pass whose factor does not hold its working set gives the test
// nothing to solve with -- no factor, no solve, rho at its NaN default -- and
// the verdict is then the integer screen's alone. `DepTest::unrun` says so, and
// `QpCounters::dep_unrun_tests` counts it at every growth site.
//
// Before this round the screen-negative half returned kUndecidable, which
// `DepTest::dependent()` reads as dependent, so a blocker was charged the
// deferred-add path (a re-check pass, and a known-dependent mark if that
// re-check removed it) on the strength of a rho that had never been computed.
// The walk-level consequence is the ACOPR118 fixture in
// tests/qp/test_qp_unrun_dependency_test.cpp.
// ===========================================================================

// 4 variables, one working row, nothing pinned: me + |W| + 1 = 2 <= n = 4, so the
// screen proves nothing, and with no usable factor NOTHING IS COMPUTED.
TEST(WsIndependenceUnrun, NoFactorAndASilentScreenIsINDEPENDENT) {
    Eigen::MatrixXd Ai(1, 4);
    Ai << 1, 1, 0, 0;
    Vec bi(1);
    bi << 1.0;
    const QpProblem qp = make_qp(Eigen::MatrixXd::Identity(4, 4), Vec::Zero(4),
                                 Eigen::MatrixXd(0, 4), Vec(0), Ai, bi, -10.0, 10.0);
    QpOptions o;
    WorkingSet ws(4, 1);
    ws.add_ineq(0);
    const detail::DepFactorView none; // the pass holds no factor for `ws`
    ASSERT_FALSE(none.usable());

    // The candidate is EXACTLY the working row -- truly dependent, and the test still may not
    // say so, because it did not look. A verdict this test cannot support is not the safe
    // direction here: it is a fabricated one.
    Vec a(4);
    a << 1.0, 1.0, 0.0, 0.0;
    const detail::DepTest t = detail::dependency_test(qp, ws, none, a, o);
    EXPECT_EQ(t.verdict, detail::DepVerdict::kIndependent);
    EXPECT_FALSE(t.dependent());
    EXPECT_TRUE(t.unrun);
    EXPECT_FALSE(t.screen);
    EXPECT_TRUE(std::isnan(t.rho)) << "rho is ABSENT, never a substituted zero";
    EXPECT_EQ(t.solves, 0);
    EXPECT_FALSE(t.stall_admit);
    // certificate_coefficients is the same body with the refinement run out: same answer.
    const detail::DepTest c = detail::certificate_coefficients(qp, ws, none, a, o);
    EXPECT_EQ(c.verdict, detail::DepVerdict::kIndependent);
    EXPECT_TRUE(c.unrun);
    EXPECT_EQ(c.solves, 0);
}

// The same, with the screen POSITIVE: me + |W| + 1 = 2 + 2 + 1 = 5 > n = 4 is an exact
// integer proof, so it decides even though nothing was solved. `unrun` is still true -- it
// records that no ARITHMETIC ran, not that no verdict was reached.
TEST(WsIndependenceUnrun, NoFactorAndAFiringScreenIsSTILLDEPENDENT) {
    Eigen::MatrixXd Ae(2, 4);
    Ae << 1, 0, 0, 0, 0, 1, 0, 0;
    Vec be(2);
    be << 0.0, 0.0;
    Eigen::MatrixXd Ai(2, 4);
    Ai << 0, 0, 1, 0, 0, 0, 0, 1;
    Vec bi(2);
    bi << 1.0, 1.0;
    const QpProblem qp =
        make_qp(Eigen::MatrixXd::Identity(4, 4), Vec::Zero(4), Ae, be, Ai, bi, -10.0, 10.0);
    QpOptions o;
    WorkingSet ws(4, 2);
    ws.add_ineq(0);
    ws.add_ineq(1);
    ASSERT_EQ(ws.num_free(), 4) << "no pin, so the no-free-column branch above is not the one";
    const detail::DepFactorView none;
    Vec a(4);
    a << 0.0, 0.0, 0.0, 1.0;
    const detail::DepTest t = detail::dependency_test(qp, ws, none, a, o);
    EXPECT_EQ(t.verdict, detail::DepVerdict::kDependent);
    EXPECT_TRUE(t.dependent());
    EXPECT_TRUE(t.screen);
    EXPECT_TRUE(t.unrun);
    EXPECT_TRUE(std::isnan(t.rho));
    EXPECT_EQ(t.solves, 0);
}

// With a factor, nothing about this changes: `unrun` is false and rho is a measurement. The
// guard against a flag that leaks into the ordinary path.
TEST_P(WsIndependence, AUsableFactorIsNeverAnUnrunTest) {
    Eigen::MatrixXd Ai(2, 3);
    Ai << 1, 1, 0, 0, 0, 1;
    Vec bi(2);
    bi << 1.0, 1.0;
    const QpProblem qp = make_qp(Eigen::MatrixXd::Identity(3, 3), Vec::Zero(3),
                                 Eigen::MatrixXd(0, 3), Vec(0), Ai, bi, -10.0, 10.0);
    const QpOptions o = opts_for(GetParam());
    WorkingSet ws(3, 2);
    ws.add_ineq(0);
    detail::KktFactor kkt;
    (void)solve_eqp(qp, ws, Vec::Zero(qp.n()), kkt, o);
    detail::DepFactorView v;
    v.eliminated = &kkt;
    Vec a(3);
    a << 1.0, 1.0, 0.0;
    const detail::DepTest t = detail::dependency_test(qp, ws, v, a, o);
    EXPECT_FALSE(t.unrun);
    EXPECT_GE(t.solves, 1);
    EXPECT_FALSE(std::isnan(t.rho));
    EXPECT_EQ(t.verdict, detail::DepVerdict::kDependent);
}

// ===========================================================================
// THE DEAD-END RANK REPAIR's analysis (M7 W2 RDV; ws_independence.h, rank_repair), on constructed
// working sets: the release set, its orientation on an inconsistent face, the mandatory rows'
// own dependence, the stuck case, and the block path past kRankDenseRows rows.
// ===========================================================================

using RankOutcome = detail::RankRepair::Outcome;

bool released(const detail::RankRepair &r, detail::WsMember::Kind kind, Index idx) {
    for (const detail::WsMember &m : r.release) {
        if (m.kind == kind && m.idx == idx) {
            return true;
        }
    }
    return false;
}

// x0 + x1 = 1 (equality) beside the working row x0 + x1 <= 2: dependent and INCONSISTENT; on
// the face without the row x0 + x1 = 1 < 2, so the row is released.
TEST(WsRankRepair, InconsistentRowIsReleasedByItsOrientation) {
    Eigen::MatrixXd Ae(1, 2);
    Ae << 1, 1;
    Eigen::MatrixXd Ai(1, 2);
    Ai << 1, 1;
    const QpProblem qp = make_qp(Eigen::MatrixXd::Identity(2, 2), Vec::Zero(2), Ae,
                                 Vec::Constant(1, 1.0), Ai, Vec::Constant(1, 2.0), -10.0, 10.0);
    WorkingSet ws(2, 1);
    ws.add_ineq(0);
    const detail::RankRepair r = detail::rank_repair(qp, ws, Vec::Zero(1), QpOptions{}, 0);
    EXPECT_EQ(r.outcome, RankOutcome::kRelease);
    EXPECT_EQ(r.deficiency, 1);
    EXPECT_EQ(r.inconsistent_steps, 1);
    ASSERT_EQ(r.release.size(), 1u);
    EXPECT_TRUE(released(r, detail::WsMember::Kind::kRow, 0));
    ASSERT_EQ(r.release_consistent.size(), 1u);
    EXPECT_EQ(r.release_consistent[0], 0);
}

// The consistency tolerance is scaled by the members that PARTICIPATE in the direction: x2 pinned
// at the raw bound 1e4 takes no part in the dependence x0 + x1 = 1 vs x0 + x1 <= 1.1 (gamma =
// 0.05), so it cannot inflate the tolerance (scaled by ||rhs||_inf it was 10 * 1e-5 * 1e4 = 1:
// the inconsistent direction read consistent).
TEST(WsRankRepair, ALargePinnedBoundElsewhereDoesNotInflateTheConsistencyScale) {
    Eigen::MatrixXd Ae(1, 3);
    Ae << 1, 1, 0;
    Eigen::MatrixXd Ai(1, 3);
    Ai << 1, 1, 0;
    QpProblem qp = make_qp(Eigen::MatrixXd::Identity(3, 3), Vec::Zero(3), Ae, Vec::Constant(1, 1.0),
                           Ai, Vec::Constant(1, 1.1), -10.0, 10.0);
    qp.lower(2) = 1e4;
    qp.upper(2) = 2e4;
    QpOptions o;
    o.feas_tol = 1e-5;
    WorkingSet ws(3, 1);
    ws.add_ineq(0);
    ws.bound_state()[2] = BoundState::kAtLower;
    const detail::RankRepair r = detail::rank_repair(qp, ws, Vec::Zero(1), o, 0);
    EXPECT_EQ(r.outcome, RankOutcome::kRelease);
    EXPECT_EQ(r.deficiency, 1);
    EXPECT_EQ(r.inconsistent_steps, 1) << "gamma = 0.05 reads inconsistent";
    ASSERT_EQ(r.release.size(), 1u);
    EXPECT_TRUE(released(r, detail::WsMember::Kind::kRow, 0));
}

// 1050 disjoint equality rows x_2i + x_2i+1 = 1 plus nine working rows x_2c + x_2c+1 <= 2
// (c < 9), each duplicating an equality row inconsistently: nine null directions on the block
// path (1059 > kRankDenseRows rows).
QpProblem nine_null_directions(WorkingSet &ws) {
    const Index me = 1050;
    const Index n = 2 * me;
    std::vector<Eigen::Triplet<double>> te;
    for (Index i = 0; i < me; ++i) {
        te.emplace_back(i, 2 * i, 1.0);
        te.emplace_back(i, 2 * i + 1, 1.0);
    }
    std::vector<Eigen::Triplet<double>> ti;
    for (Index c = 0; c < 9; ++c) {
        ti.emplace_back(c, 2 * c, 1.0);
        ti.emplace_back(c, 2 * c + 1, 1.0);
    }
    QpProblem qp;
    qp.H.resize(n, n);
    qp.H.setIdentity();
    qp.g = Vec::Zero(n);
    qp.Ae.resize(me, n);
    qp.Ae.setFromTriplets(te.begin(), te.end());
    qp.be = Vec::Ones(me);
    qp.Ai.resize(9, n);
    qp.Ai.setFromTriplets(ti.begin(), ti.end());
    qp.bi = Vec::Constant(9, 2.0);
    qp.lower = Vec::Zero(n);
    qp.upper = Vec::Constant(n, 10.0);
    ws = WorkingSet(n, 9);
    for (Index c = 0; c < 9; ++c) {
        ws.add_ineq(c);
    }
    return qp;
}

// Nine null directions: the first block (kRankBlockStart = 8 columns) cannot hold them all, so
// the block DOUBLES before every direction is resolved.
TEST(WsRankRepair, NineNullDirectionsDoubleTheBlock) {
    static_assert(9 > detail::kRankBlockStart);
    WorkingSet ws(1, 0);
    const QpProblem qp = nine_null_directions(ws);
    ASSERT_GT(qp.me() + 9, detail::kRankDenseRows);
    const detail::RankRepair r = detail::rank_repair(qp, ws, Vec::Zero(9), QpOptions{}, 0);
    EXPECT_EQ(r.outcome, RankOutcome::kRelease);
    EXPECT_EQ(r.deficiency, 9);
    EXPECT_EQ(r.inconsistent_steps, 9);
    ASSERT_EQ(r.release.size(), 9u);
    for (Index c = 0; c < 9; ++c) {
        EXPECT_TRUE(released(r, detail::WsMember::Kind::kRow, c)) << c;
    }
}

// A block past the cap REFUSES and releases nothing. The cap is a parameter only so the refusal
// need not allocate a multi-gigabyte block: here the doubled block (16 columns) exceeds it.
TEST(WsRankRepair, ABlockPastTheCapRefuses) {
    WorkingSet ws(1, 0);
    const QpProblem qp = nine_null_directions(ws);
    const double rows = static_cast<double>(qp.me() + 9);
    const detail::RankRepair r =
        detail::rank_repair(qp, ws, Vec::Zero(9), QpOptions{}, 0, rows * 12.0);
    EXPECT_EQ(r.outcome, RankOutcome::kRefused);
    EXPECT_TRUE(r.release.empty());
}

// The same pair with the row x0 + x1 <= 1/2: the face without it violates it, and no other member
// is releasable -- STUCK, nothing released (the QP is infeasible).
TEST(WsRankRepair, InconsistentWithNoAdmissibleMemberIsStuck) {
    Eigen::MatrixXd Ae(1, 2);
    Ae << 1, 1;
    Eigen::MatrixXd Ai(1, 2);
    Ai << 1, 1;
    const QpProblem qp = make_qp(Eigen::MatrixXd::Identity(2, 2), Vec::Zero(2), Ae,
                                 Vec::Constant(1, 1.0), Ai, Vec::Constant(1, 0.5), -10.0, 10.0);
    WorkingSet ws(2, 1);
    ws.add_ineq(0);
    const detail::RankRepair r = detail::rank_repair(qp, ws, Vec::Zero(1), QpOptions{}, 0);
    EXPECT_EQ(r.outcome, RankOutcome::kStuck);
    EXPECT_TRUE(r.release.empty());
}

// A consistent duplicate row is released and the face does not change; a shift-driven one is
// mandatory (not releasable), leaving only the rows' own dependence.
TEST(WsRankRepair, ConsistentRowIsReleasedUnlessShiftDriven) {
    Eigen::MatrixXd Ae(1, 2);
    Ae << 1, 1;
    Eigen::MatrixXd Ai(1, 2);
    Ai << 2, 2;
    const QpProblem qp = make_qp(Eigen::MatrixXd::Identity(2, 2), Vec::Zero(2), Ae,
                                 Vec::Constant(1, 1.0), Ai, Vec::Constant(1, 2.0), -10.0, 10.0);
    WorkingSet ws(2, 1);
    ws.add_ineq(0);
    const detail::RankRepair r = detail::rank_repair(qp, ws, Vec::Zero(1), QpOptions{}, 0);
    EXPECT_EQ(r.outcome, RankOutcome::kRelease);
    EXPECT_EQ(r.inconsistent_steps, 0);
    EXPECT_TRUE(released(r, detail::WsMember::Kind::kRow, 0));
    ASSERT_EQ(r.release_consistent.size(), 1u);
    EXPECT_EQ(r.release_consistent[0], 1) << "a consistent-case release is marked as such";
    const detail::RankRepair s = detail::rank_repair(qp, ws, Vec::Constant(1, 0.5), QpOptions{}, 0);
    EXPECT_EQ(s.outcome, RankOutcome::kFullRank);
    EXPECT_EQ(s.deficiency, 1);
    EXPECT_TRUE(s.release.empty());
}

// x0 - x1 = 0 with x0 pinned at its lower bound 0 and x1 at its upper bound 1: no free column, the
// row empty on F, inconsistent. Both pins have the right orientation (releasing either moves its
// variable strictly inside its box on the new face); the tie goes to the first, x0.
TEST(WsRankRepair, PinsCarryTheRowsDependencyAndItsOrientation) {
    Eigen::MatrixXd Ae(1, 2);
    Ae << 1, -1;
    const QpProblem qp = make_qp(Eigen::MatrixXd::Identity(2, 2), Vec::Zero(2), Ae, Vec::Zero(1),
                                 Eigen::MatrixXd(0, 2), Vec(0), 0.0, 1.0);
    WorkingSet ws(2, 0);
    ws.bound_state()[0] = BoundState::kAtLower;
    ws.bound_state()[1] = BoundState::kAtUpper;
    const detail::RankRepair r = detail::rank_repair(qp, ws, Vec(0), QpOptions{}, 0);
    EXPECT_EQ(r.outcome, RankOutcome::kRelease);
    EXPECT_EQ(r.inconsistent_steps, 1);
    ASSERT_EQ(r.release.size(), 1u);
    EXPECT_TRUE(released(r, detail::WsMember::Kind::kPin, 0));
}

// The equality rows' own consistent dependence (x0 + x1 = 1 twice over) is left to the dual
// regularization: full rank as far as the repair is concerned.
TEST(WsRankRepair, MandatoryOnlyDependenceReleasesNothing) {
    Eigen::MatrixXd Ae(2, 2);
    Ae << 1, 1, 2, 2;
    Vec be(2);
    be << 1, 2;
    const QpProblem qp = make_qp(Eigen::MatrixXd::Identity(2, 2), Vec::Zero(2), Ae, be,
                                 Eigen::MatrixXd(0, 2), Vec(0), -10.0, 10.0);
    WorkingSet ws(2, 0);
    const detail::RankRepair r = detail::rank_repair(qp, ws, Vec(0), QpOptions{}, 0);
    EXPECT_EQ(r.outcome, RankOutcome::kFullRank);
    EXPECT_EQ(r.deficiency, 1);
}

// Past kRankDenseRows rows the block inverse iteration decides. 1050 disjoint equality rows
// x_2i + x_2i+1 = 1, a working row x0 + x1 <= 2 (inconsistent with row 0) and both variables of
// row 1 pinned at their lower bound 0 (row 1 empty on the free columns, inconsistent): two null
// directions, each released by its orientation.
TEST(WsRankRepair, BlockPathFindsTwoInconsistentDirections) {
    const Index me = 1050;
    const Index n = 2 * me;
    ASSERT_GT(me + 1, detail::kRankDenseRows);
    std::vector<Eigen::Triplet<double>> te;
    for (Index i = 0; i < me; ++i) {
        te.emplace_back(i, 2 * i, 1.0);
        te.emplace_back(i, 2 * i + 1, 1.0);
    }
    QpProblem qp;
    qp.H.resize(n, n);
    qp.H.setIdentity();
    qp.g = Vec::Zero(n);
    qp.Ae.resize(me, n);
    qp.Ae.setFromTriplets(te.begin(), te.end());
    qp.be = Vec::Ones(me);
    std::vector<Eigen::Triplet<double>> ti{{0, 0, 1.0}, {0, 1, 1.0}};
    qp.Ai.resize(1, n);
    qp.Ai.setFromTriplets(ti.begin(), ti.end());
    qp.bi = Vec::Constant(1, 2.0);
    qp.lower = Vec::Zero(n);
    qp.upper = Vec::Constant(n, 10.0);
    WorkingSet ws(n, 1);
    ws.add_ineq(0);
    ws.bound_state()[2] = BoundState::kAtLower;
    ws.bound_state()[3] = BoundState::kAtLower;
    const detail::RankRepair r = detail::rank_repair(qp, ws, Vec::Zero(1), QpOptions{}, 0);
    EXPECT_EQ(r.outcome, RankOutcome::kRelease);
    EXPECT_EQ(r.deficiency, 2);
    EXPECT_EQ(r.inconsistent_steps, 2);
    ASSERT_EQ(r.release.size(), 2u);
    EXPECT_TRUE(released(r, detail::WsMember::Kind::kRow, 0));
    EXPECT_TRUE(released(r, detail::WsMember::Kind::kPin, 2) ||
                released(r, detail::WsMember::Kind::kPin, 3));
}

} // namespace
