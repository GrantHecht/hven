// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The interior-point QP tier's KKT scatter plan (M6 W1 T3.b).
//
// Three claims are pinned here, and each is pinned so that it can FAIL:
//
//  1. The scattered values are what a plain setFromTriplets assembly of the
//     same system produces -- BYTE-identical, not merely close. The reference
//     below is written independently, in a DIFFERENT emission order, so an
//     off-by-one in the position map does not cancel out against itself.
//  2. The recorded diagonal slots address the entries they claim to. Checked
//     twice over: against the reference matrix's own coefficients, and --
//     the non-vacuity half -- by perturbing a recorded slot and watching the
//     FACTORIZATION's observed inertia move. A slot table that pointed
//     somewhere else would leave the inertia where it was.
//  3. Amendment E's re-entrancy assertion: predictor and corrector solves
//     against ONE numeric factorization, both residuals checked, so a backend
//     change that broke `KktFactorization::solve`'s constness fails loudly
//     here instead of silently corrupting a corrector step.

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include <Eigen/Core>
#include <Eigen/SparseCore>

#include "hven/core/types.h"
#include "hven/detail/interior/kkt_factorization.h"
#include "hven/detail/qp/ipqp_kkt_layout.h"

namespace {

using hven::Index;
using hven::SpMatRM;
using hven::solvers::IpqpKktLayout;
using hven::solvers::KktFactorization;

SpMatRM sparse_from(Index rows, Index cols, const std::vector<Eigen::Triplet<double>> &entries) {
    SpMatRM m(rows, cols);
    m.setFromTriplets(entries.begin(), entries.end());
    m.makeCompressed();
    return m;
}

// The oracle. Same system, same upper-triangle convention, assembled the
// obvious way -- one triplet vector, one setFromTriplets -- and emitted in an
// order deliberately unlike the layout's (blocks reversed, H rows walked
// backwards) so that agreement is evidence about the POSITION MAP rather than
// about two copies of one loop.
SpMatRM reference_kkt(const SpMatRM &H, const SpMatRM &Ae, const SpMatRM &Ai, Index n, Index me,
                      Index mi) {
    const Index dim = n + 2 * mi + me;
    const Index s_off = n;
    const Index e_off = n + mi;
    const Index i_off = n + mi + me;

    std::vector<Eigen::Triplet<double>> t;
    for (Index j = 0; j < mi; ++j) {
        t.emplace_back(i_off + j, i_off + j, 0.0);
    }
    for (Index j = mi - 1; j >= 0; --j) {
        t.emplace_back(s_off + j, i_off + j, -1.0);
        t.emplace_back(s_off + j, s_off + j, 0.0);
        for (SpMatRM::InnerIterator it(Ai, j); it; ++it) {
            t.emplace_back(it.col(), i_off + j, it.value());
        }
    }
    for (Index r = me - 1; r >= 0; --r) {
        t.emplace_back(e_off + r, e_off + r, 0.0);
        for (SpMatRM::InnerIterator it(Ae, r); it; ++it) {
            t.emplace_back(it.col(), e_off + r, it.value());
        }
    }
    for (Index i = n - 1; i >= 0; --i) {
        for (SpMatRM::InnerIterator it(H, i); it; ++it) {
            t.emplace_back(i, it.col(), it.value());
        }
        t.emplace_back(i, i, 0.0);
    }
    return sparse_from(dim, dim, t);
}

void expect_identical(const SpMatRM &got, const SpMatRM &want) {
    ASSERT_EQ(got.rows(), want.rows());
    ASSERT_EQ(got.cols(), want.cols());
    ASSERT_TRUE(got.isCompressed());
    ASSERT_TRUE(want.isCompressed());
    ASSERT_EQ(got.nonZeros(), want.nonZeros());
    for (Index r = 0; r <= got.rows(); ++r) {
        EXPECT_EQ(got.outerIndexPtr()[r], want.outerIndexPtr()[r]) << "outer index " << r;
    }
    for (Index t = 0; t < got.nonZeros(); ++t) {
        EXPECT_EQ(got.innerIndexPtr()[t], want.innerIndexPtr()[t]) << "inner index " << t;
        // Exact equality, not a tolerance: the claim is that the scatter
        // reproduces the reference's BYTES, and every value here is one
        // source datum copied (or summed with a structural 0.0) into place.
        EXPECT_EQ(got.valuePtr()[t], want.valuePtr()[t]) << "value " << t;
    }
}

// ---------------------------------------------------------------------------
// Fixtures. n = 3 throughout, with a shared-column pattern in H and in Ai (two
// rows both touching variable 1) so a position map that keyed on the column
// alone would collide.
// ---------------------------------------------------------------------------

struct Fixture {
    SpMatRM H, Ae, Ai;
    Index n = 0, me = 0, mi = 0;
};

Fixture general_fixture() {
    Fixture f;
    f.n = 3;
    f.me = 1;
    f.mi = 2;
    // Upper triangle only, with an off-diagonal and one variable (2) whose
    // diagonal is the only entry in its row.
    f.H = sparse_from(3, 3, {{0, 0, 2.0}, {0, 1, 0.5}, {1, 1, 3.0}, {2, 2, 4.0}});
    f.Ae = sparse_from(1, 3, {{0, 0, 1.0}, {0, 2, 2.0}});
    f.Ai = sparse_from(2, 3, {{0, 0, 1.0}, {0, 1, 1.0}, {1, 1, 3.0}, {1, 2, 1.0}});
    return f;
}

// A variable with NO diagonal entry in H: the structural diagonal the plan
// adds is then the only entry in that slot, and primal_diag_source() must
// report 0 for it.
Fixture missing_h_diagonal_fixture() {
    Fixture f = general_fixture();
    f.H = sparse_from(3, 3, {{0, 0, 2.0}, {0, 1, 0.5}, {2, 2, 4.0}});
    return f;
}

Fixture no_equalities_fixture() {
    Fixture f = general_fixture();
    f.me = 0;
    f.Ae = sparse_from(0, 3, {});
    return f;
}

Fixture no_inequalities_fixture() {
    Fixture f = general_fixture();
    f.mi = 0;
    f.Ai = sparse_from(0, 3, {});
    return f;
}

Fixture bound_only_fixture() {
    Fixture f = general_fixture();
    f.me = 0;
    f.mi = 0;
    f.Ae = sparse_from(0, 3, {});
    f.Ai = sparse_from(0, 3, {});
    return f;
}

// ---------------------------------------------------------------------------

TEST(IpqpKktLayoutTest, DimensionAndBlockBasesFollowTheKktVectorOrder) {
    const Fixture f = general_fixture();
    IpqpKktLayout layout;
    SpMatRM k;

    EXPECT_FALSE(layout.has_structure());
    EXPECT_TRUE(layout.sync(f.H, f.Ae, f.Ai, f.n, f.me, f.mi, k));

    // [primals | slacks | eq_lmults | iq_lmults], so dim = n + 2*mi + me.
    EXPECT_EQ(layout.dim(), f.n + 2 * f.mi + f.me);
    EXPECT_EQ(k.rows(), layout.dim());
    EXPECT_EQ(k.cols(), layout.dim());
    EXPECT_EQ(layout.primal_diag_base(), 0);
    EXPECT_EQ(layout.slack_diag_base(), f.n);
    EXPECT_EQ(layout.eq_pivot_base(), f.n + f.mi);
    EXPECT_EQ(layout.iq_pivot_base(), f.n + f.mi + f.me);
    EXPECT_EQ(static_cast<Index>(layout.diag_pos().size()), layout.dim());
    EXPECT_TRUE(layout.has_structure());
}

TEST(IpqpKktLayoutTest, TheLaidOutMatrixIsByteIdenticalToASetFromTripletsReference) {
    for (const Fixture &f :
         {general_fixture(), missing_h_diagonal_fixture(), no_equalities_fixture(),
          no_inequalities_fixture(), bound_only_fixture()}) {
        IpqpKktLayout layout;
        SpMatRM k;
        ASSERT_TRUE(layout.sync(f.H, f.Ae, f.Ai, f.n, f.me, f.mi, k));
        expect_identical(k, reference_kkt(f.H, f.Ae, f.Ai, f.n, f.me, f.mi));
    }
}

TEST(IpqpKktLayoutTest, AReusedPlanScattersNewValuesWithoutRelayingOutThePattern) {
    Fixture f = general_fixture();
    IpqpKktLayout layout;
    SpMatRM k;
    ASSERT_TRUE(layout.sync(f.H, f.Ae, f.Ai, f.n, f.me, f.mi, k));

    // The caller's per-iteration writes, which the next scatter must clear:
    // if the zero-fill were skipped these would survive and the comparison
    // below would fail.
    k.valuePtr()[layout.primal_diag_slot(0)] += 17.0;
    k.valuePtr()[layout.eq_pivot_slot(0)] = -3.0;
    k.valuePtr()[layout.slack_coupling_slot(1)] = -9.0;

    // Same pattern, different values -- what a new major with an unchanged
    // structure hands the tier.
    f.H = sparse_from(3, 3, {{0, 0, 7.0}, {0, 1, -1.5}, {1, 1, 11.0}, {2, 2, 0.25}});
    f.Ae = sparse_from(1, 3, {{0, 0, -4.0}, {0, 2, 6.0}});
    f.Ai = sparse_from(2, 3, {{0, 0, 2.0}, {0, 1, -2.0}, {1, 1, 8.0}, {1, 2, 5.0}});

    EXPECT_TRUE(layout.matches(f.H, f.Ae, f.Ai, f.n, f.me, f.mi));
    EXPECT_FALSE(layout.sync(f.H, f.Ae, f.Ai, f.n, f.me, f.mi, k));
    expect_identical(k, reference_kkt(f.H, f.Ae, f.Ai, f.n, f.me, f.mi));
}

TEST(IpqpKktLayoutTest, APatternChangeRelaysTheStructureOut) {
    Fixture f = general_fixture();
    IpqpKktLayout layout;
    SpMatRM k;
    ASSERT_TRUE(layout.sync(f.H, f.Ae, f.Ai, f.n, f.me, f.mi, k));

    // One entry added to Ai: same dimensions, different pattern.
    f.Ai = sparse_from(2, 3, {{0, 0, 1.0}, {0, 1, 1.0}, {1, 0, 0.5}, {1, 1, 3.0}, {1, 2, 1.0}});
    EXPECT_FALSE(layout.matches(f.H, f.Ae, f.Ai, f.n, f.me, f.mi));
    EXPECT_TRUE(layout.sync(f.H, f.Ae, f.Ai, f.n, f.me, f.mi, k));
    expect_identical(k, reference_kkt(f.H, f.Ae, f.Ai, f.n, f.me, f.mi));

    // And a dimension change, which the key also has to catch.
    const Fixture g = no_equalities_fixture();
    EXPECT_FALSE(layout.matches(g.H, g.Ae, g.Ai, g.n, g.me, g.mi));
    EXPECT_TRUE(layout.sync(g.H, g.Ae, g.Ai, g.n, g.me, g.mi, k));
    EXPECT_EQ(layout.dim(), g.n + 2 * g.mi + g.me);
}

TEST(IpqpKktLayoutTest, DiagonalSlotsAddressTheEntriesTheyClaimTo) {
    const Fixture f = general_fixture();
    IpqpKktLayout layout;
    SpMatRM k;
    ASSERT_TRUE(layout.sync(f.H, f.Ae, f.Ai, f.n, f.me, f.mi, k));

    // Write a distinguishable value through each slot, then read it back
    // through Eigen's own coordinate accessor -- an independent route to the
    // same entry.
    for (Index i = 0; i < f.n; ++i) {
        k.valuePtr()[layout.primal_diag_slot(i)] = 100.0 + static_cast<double>(i);
    }
    for (Index j = 0; j < f.mi; ++j) {
        k.valuePtr()[layout.slack_diag_slot(j)] = 200.0 + static_cast<double>(j);
        k.valuePtr()[layout.iq_pivot_slot(j)] = 400.0 + static_cast<double>(j);
        k.valuePtr()[layout.slack_coupling_slot(j)] = 500.0 + static_cast<double>(j);
    }
    for (Index r = 0; r < f.me; ++r) {
        k.valuePtr()[layout.eq_pivot_slot(r)] = 300.0 + static_cast<double>(r);
    }

    const Index s_off = f.n;
    const Index e_off = f.n + f.mi;
    const Index i_off = f.n + f.mi + f.me;
    for (Index i = 0; i < f.n; ++i) {
        EXPECT_EQ(k.coeff(i, i), 100.0 + static_cast<double>(i)) << "primal diag " << i;
    }
    for (Index j = 0; j < f.mi; ++j) {
        EXPECT_EQ(k.coeff(s_off + j, s_off + j), 200.0 + static_cast<double>(j));
        EXPECT_EQ(k.coeff(i_off + j, i_off + j), 400.0 + static_cast<double>(j));
        EXPECT_EQ(k.coeff(s_off + j, i_off + j), 500.0 + static_cast<double>(j));
    }
    for (Index r = 0; r < f.me; ++r) {
        EXPECT_EQ(k.coeff(e_off + r, e_off + r), 300.0 + static_cast<double>(r));
    }
}

TEST(IpqpKktLayoutTest, PrimalDiagSourceCarriesHsOwnDiagonalAndZeroWhereHHasNone) {
    const Fixture f = missing_h_diagonal_fixture();
    IpqpKktLayout layout;
    SpMatRM k;
    ASSERT_TRUE(layout.sync(f.H, f.Ae, f.Ai, f.n, f.me, f.mi, k));

    ASSERT_EQ(static_cast<Index>(layout.primal_diag_source().size()), f.n);
    EXPECT_EQ(layout.primal_diag_source()[0], 2.0);
    EXPECT_EQ(layout.primal_diag_source()[1], 0.0); // H stores no (1,1) entry
    EXPECT_EQ(layout.primal_diag_source()[2], 4.0);

    // It is refreshed by a reuse-scatter, not frozen at layout time.
    Fixture g = f;
    g.H = sparse_from(3, 3, {{0, 0, -6.0}, {0, 1, 0.5}, {2, 2, 9.0}});
    ASSERT_FALSE(layout.sync(g.H, g.Ae, g.Ai, g.n, g.me, g.mi, k));
    EXPECT_EQ(layout.primal_diag_source()[0], -6.0);
    EXPECT_EQ(layout.primal_diag_source()[1], 0.0);
    EXPECT_EQ(layout.primal_diag_source()[2], 9.0);
}

// ---------------------------------------------------------------------------
// The factorization-side pins: the assembled system's inertia, the
// mutation-non-vacuity cross-check on the diagonal slots, and Amendment E.
// ---------------------------------------------------------------------------

// The proximal and dual regularizations the fixtures factorize under. delta is
// deliberately O(1) rather than the small value a converged tier would carry:
// eliminating the -delta I blocks contributes B' (1/delta) B to the primal
// block, so a tiny delta makes that term dominate everything else and no
// perturbation of a primal diagonal short of 1/delta could move the inertia --
// which would make the non-vacuity pin below vacuous for the wrong reason.
constexpr double kRho = 8.0;
constexpr double kDelta = 1.0;

KktFactorization::Options factor_options() {
    KktFactorization::Options opts;
    opts.kind = hven::linear::FactorKind::kLDLT;
    opts.num_threads = 1;
    return opts;
}

// Fills the four diagonal families so the system is quasi-definite: the
// primal block H + rho I + Sigma positive definite, the slack block
// Lambda S^-1 positive, both multiplier blocks -delta. Section 4.1's inertia
// target for this layout is then (n + mi, me + mi, 0).
void install_quasidefinite_diagonals(const IpqpKktLayout &layout, const Fixture &f, double rho,
                                     double delta, SpMatRM &k) {
    double *const v = k.valuePtr();
    for (Index i = 0; i < f.n; ++i) {
        v[layout.primal_diag_slot(i)] = layout.primal_diag_source()[static_cast<std::size_t>(i)] +
                                        rho + 1.0 + static_cast<double>(i);
    }
    for (Index j = 0; j < f.mi; ++j) {
        v[layout.slack_diag_slot(j)] = 2.0 + static_cast<double>(j);
        v[layout.iq_pivot_slot(j)] = -delta;
    }
    for (Index r = 0; r < f.me; ++r) {
        v[layout.eq_pivot_slot(r)] = -delta;
    }
}

TEST(IpqpKktLayoutTest, TheAssembledSystemFactorizesToTheSpecifiedInertiaTarget) {
    const Fixture f = general_fixture();
    IpqpKktLayout layout;
    KktFactorization kkt(factor_options());
    ASSERT_TRUE(layout.sync(f.H, f.Ae, f.Ai, f.n, f.me, f.mi, kkt.matrix()));
    install_quasidefinite_diagonals(layout, f, kRho, kDelta, kkt.matrix());

    kkt.compute();

    ASSERT_EQ(kkt.info(), Eigen::Success);
    EXPECT_EQ(kkt.peigs(), f.n + f.mi);
    EXPECT_EQ(kkt.neigs(), f.me + f.mi);
    const hven::linear::InertiaEvidence &ev = kkt.inertia_evidence();
    EXPECT_EQ(ev.state, hven::linear::InertiaEvidence::State::kObserved);
    EXPECT_EQ(ev.n_zero, 0);
}

// MUTATION NON-VACUITY (W0.3 precedent) for the diagonal slot table: a value
// written through a recorded primal slot has to reach the factorization. Drive
// one primal diagonal strongly negative and the observed inertia must move by
// exactly one eigenvalue. A slot table pointing at the wrong entry -- an
// off-diagonal, another block's diagonal -- would not produce this.
TEST(IpqpKktLayoutTest, PerturbingARecordedDiagonalSlotMovesTheObservedInertia) {
    const Fixture f = general_fixture();
    IpqpKktLayout layout;
    KktFactorization kkt(factor_options());
    ASSERT_TRUE(layout.sync(f.H, f.Ae, f.Ai, f.n, f.me, f.mi, kkt.matrix()));
    install_quasidefinite_diagonals(layout, f, kRho, kDelta, kkt.matrix());
    kkt.compute();
    ASSERT_EQ(kkt.peigs(), f.n + f.mi);
    ASSERT_EQ(kkt.neigs(), f.me + f.mi);

    kkt.matrix().valuePtr()[layout.primal_diag_slot(2)] = -1e6;
    kkt.refactorize();

    EXPECT_EQ(kkt.peigs(), f.n + f.mi - 1);
    EXPECT_EQ(kkt.neigs(), f.me + f.mi + 1);

    // And the same for a slack-block slot, so the pin covers more than one
    // family of the table.
    install_quasidefinite_diagonals(layout, f, kRho, kDelta, kkt.matrix());
    kkt.matrix().valuePtr()[layout.slack_diag_slot(1)] = -1e6;
    kkt.refactorize();

    EXPECT_EQ(kkt.peigs(), f.n + f.mi - 1);
    EXPECT_EQ(kkt.neigs(), f.me + f.mi + 1);
}

// AMENDMENT E. Mehrotra's predictor and corrector share ONE numeric
// factorization: KktFactorization::solve is const and must not disturb the
// factor. Asserted, not assumed.
TEST(IpqpKktLayoutTest, PredictorAndCorrectorSolveAgainstOneFactorization) {
    const Fixture f = general_fixture();
    IpqpKktLayout layout;
    KktFactorization kkt(factor_options());
    ASSERT_TRUE(layout.sync(f.H, f.Ae, f.Ai, f.n, f.me, f.mi, kkt.matrix()));
    install_quasidefinite_diagonals(layout, f, kRho, kDelta, kkt.matrix());
    kkt.compute();
    ASSERT_EQ(kkt.info(), Eigen::Success);
    const auto factorizations_before = kkt.counters().factorize_count;

    const Index dim = layout.dim();
    Eigen::VectorXd affine_rhs(dim), corrector_rhs(dim);
    for (Index i = 0; i < dim; ++i) {
        affine_rhs[i] = 1.0 + 0.25 * static_cast<double>(i);
        // Deliberately unlike the first: a corrector RHS carries the
        // Delta S Delta Lambda term and the sigma*mu centering, so the two
        // solves must not be able to pass by returning the same vector.
        corrector_rhs[i] = -3.0 + std::sin(static_cast<double>(i));
    }

    Eigen::VectorXd affine(dim), corrector(dim);
    kkt.solve(affine_rhs, affine);
    kkt.solve(corrector_rhs, corrector);

    // ONE factorization served both.
    EXPECT_EQ(kkt.counters().factorize_count, factorizations_before);

    const Eigen::VectorXd affine_resid =
        kkt.matrix().selfadjointView<Eigen::Upper>() * affine - affine_rhs;
    const Eigen::VectorXd corrector_resid =
        kkt.matrix().selfadjointView<Eigen::Upper>() * corrector - corrector_rhs;
    EXPECT_LT(affine_resid.norm(), 1e-9 * (1.0 + affine_rhs.norm()));
    EXPECT_LT(corrector_resid.norm(), 1e-9 * (1.0 + corrector_rhs.norm()));

    // Non-vacuity: the two solves are genuinely different vectors, so the
    // second residual is not the first one measured twice.
    EXPECT_GT((affine - corrector).norm(), 1e-6);
}

// ---------------------------------------------------------------------------
// Boundary validation. Eigen's asserts are compiled out in Release, so each of
// these is the only guard against a malformed assembly.
// ---------------------------------------------------------------------------

TEST(IpqpKktLayoutTest, MalformedInputsAreRefusedAtTheBoundary) {
    const Fixture f = general_fixture();
    IpqpKktLayout layout;
    SpMatRM k;

    EXPECT_THROW(layout.sync(f.H, f.Ae, f.Ai, -1, f.me, f.mi, k), std::invalid_argument);
    EXPECT_THROW(layout.sync(f.H, f.Ae, f.Ai, 4, f.me, f.mi, k), std::invalid_argument);
    EXPECT_THROW(layout.sync(f.H, f.Ae, f.Ai, f.n, 2, f.mi, k), std::invalid_argument);
    EXPECT_THROW(layout.sync(f.H, f.Ae, f.Ai, f.n, f.me, 3, k), std::invalid_argument);

    // A lower-triangle entry in H would be emitted below the KKT diagonal and
    // rejected by the linear layer with a diagnostic naming the wrong layer.
    const SpMatRM lower = sparse_from(3, 3, {{0, 0, 2.0}, {1, 0, 0.5}, {1, 1, 3.0}, {2, 2, 4.0}});
    EXPECT_THROW(layout.sync(lower, f.Ae, f.Ai, f.n, f.me, f.mi, k), std::invalid_argument);
}

TEST(IpqpKktLayoutTest, ADiagonalIndexOutsideItsBlockIsRefused) {
    const Fixture f = general_fixture();
    IpqpKktLayout layout;
    SpMatRM k;
    ASSERT_TRUE(layout.sync(f.H, f.Ae, f.Ai, f.n, f.me, f.mi, k));

    EXPECT_THROW((void)layout.primal_diag_slot(f.n), std::out_of_range);
    EXPECT_THROW((void)layout.primal_diag_slot(-1), std::out_of_range);
    EXPECT_THROW((void)layout.slack_diag_slot(f.mi), std::out_of_range);
    EXPECT_THROW((void)layout.eq_pivot_slot(f.me), std::out_of_range);
    EXPECT_THROW((void)layout.iq_pivot_slot(f.mi), std::out_of_range);
    EXPECT_THROW((void)layout.slack_coupling_slot(f.mi), std::out_of_range);
}

} // namespace
