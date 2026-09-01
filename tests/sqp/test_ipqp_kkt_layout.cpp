// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The interior-point QP tier's KKT scatter plan (M6 W1 T3.b). Three claims pinned so each can
// FAIL: byte-identity against an independently ordered setFromTriplets reference, the diagonal
// slot table (mutation non-vacuity), and Amendment E re-entrancy. `.superpowers/w1-t3-report.md`.

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
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

// The oracle. Same system and upper-triangle convention, assembled with one setFromTriplets and
// emitted in a deliberately different order (blocks reversed, H rows walked backwards), so
// agreement is evidence about the POSITION MAP. `.superpowers/w1-t3-report.md`.
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
    // BITWISE, not `==`: `==` equates +0.0 with -0.0, the pair a zero-fill-then-accumulate
    // could produce where single-triplet assembly kept a stored -0.0. It passes bitwise on
    // all five fixtures, both paths -- do not loosen. `.superpowers/w1-t3-report.md` C2.
    for (Index t = 0; t < got.nonZeros(); ++t) {
        EXPECT_EQ(got.innerIndexPtr()[t], want.innerIndexPtr()[t]) << "inner index " << t;
        EXPECT_EQ(std::bit_cast<std::uint64_t>(got.valuePtr()[t]),
                  std::bit_cast<std::uint64_t>(want.valuePtr()[t]))
            << "value " << t << " (got " << got.valuePtr()[t] << ", want " << want.valuePtr()[t]
            << ")";
    }
    // And the whole array in one comparison, so the claim is stated the way it
    // is worded: the two value arrays are the same bytes.
    EXPECT_EQ(std::memcmp(got.valuePtr(), want.valuePtr(),
                          static_cast<std::size_t>(got.nonZeros()) * sizeof(double)),
              0);
}

// Same pattern, different values: every stored entry scaled, so the sparsity structure is
// bit-for-bit the one the plan was laid out for while no value survives -- what a new major
// with an unchanged structure hands the tier, and what the reuse scatter has to reproduce.
SpMatRM revalued(const SpMatRM &m, double factor, double shift) {
    SpMatRM out = m;
    for (Index t = 0; t < out.nonZeros(); ++t) {
        out.valuePtr()[t] = out.valuePtr()[t] * factor + shift;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Fixtures. n = 3; H and Ai share a column so a column-keyed position map would collide.
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

// EVERY fixture goes through BOTH paths: the setFromTriplets layout, then the reuse scatter
// with fresh values. Running only the fully populated fixture on the reuse path would let a
// scatter that unconditionally touched an equality or coupling offset pass.
TEST(IpqpKktLayoutTest, BothAssemblyPathsAreByteIdenticalToASetFromTripletsReference) {
    int layouts = 0;
    int reuses = 0;
    for (const Fixture &f :
         {general_fixture(), missing_h_diagonal_fixture(), no_equalities_fixture(),
          no_inequalities_fixture(), bound_only_fixture()}) {
        IpqpKktLayout layout;
        SpMatRM k;

        ASSERT_TRUE(layout.sync(f.H, f.Ae, f.Ai, f.n, f.me, f.mi, k));
        ++layouts;
        expect_identical(k, reference_kkt(f.H, f.Ae, f.Ai, f.n, f.me, f.mi));

        // The caller's per-iteration diagonal writes, which the scatter's
        // zero-fill has to clear.
        for (Index i = 0; i < f.n; ++i) {
            k.valuePtr()[layout.primal_diag_slot(i)] += 17.0;
        }
        for (Index j = 0; j < f.mi; ++j) {
            k.valuePtr()[layout.slack_diag_slot(j)] = 5.0;
            k.valuePtr()[layout.slack_coupling_slot(j)] = -9.0;
        }
        for (Index r = 0; r < f.me; ++r) {
            k.valuePtr()[layout.eq_pivot_slot(r)] = -3.0;
        }

        const SpMatRM h2 = revalued(f.H, -1.5, 0.25);
        const SpMatRM ae2 = revalued(f.Ae, 2.0, -0.75);
        const SpMatRM ai2 = revalued(f.Ai, 0.5, 4.0);
        ASSERT_TRUE(layout.matches(h2, ae2, ai2, f.n, f.me, f.mi));
        ASSERT_FALSE(layout.sync(h2, ae2, ai2, f.n, f.me, f.mi, k));
        ++reuses;
        expect_identical(k, reference_kkt(h2, ae2, ai2, f.n, f.me, f.mi));
    }
    // Non-vacuity for the loop itself: five fixtures, both paths each.
    EXPECT_EQ(layouts, 5);
    EXPECT_EQ(reuses, 5);
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
// Factorization-side pins: inertia, diagonal-slot mutation non-vacuity, Amendment E.
// ---------------------------------------------------------------------------

// The regularizations the fixtures factorize under. delta is deliberately O(1): eliminating
// the -delta I blocks contributes B' (1/delta) B to the primal block, so a tiny delta would
// make the non-vacuity pin below vacuous. `.superpowers/w1-t3-report.md`.
constexpr double kRho = 8.0;
constexpr double kDelta = 1.0;

KktFactorization::Options factor_options() {
    KktFactorization::Options opts;
    opts.kind = hven::linear::FactorKind::kLDLT;
    opts.num_threads = 1;
    return opts;
}

// Fills the four diagonal families so the system is quasi-definite: primal H + rho I + Sigma
// positive definite, slack Lambda S^-1 positive, both multiplier blocks -delta. Spec section
// 4.1's inertia target for this layout is then (n + mi, me + mi, 0).
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

// MUTATION NON-VACUITY for the diagonal slot table: a value written through a recorded primal
// slot has to reach the factorization, so driving one primal diagonal strongly negative must
// move the observed inertia by exactly one eigenvalue. `.superpowers/w1-t3-report.md`.
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
// Boundary validation: in Release these are the only guard on a malformed assembly.
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

// I1. The slot accessors' range check is meaningful only if the tables behind it belong to
// those dimensions; before any sync there are no tables, so every accessor refuses with
// std::logic_error -- distinct from the std::out_of_range a wrong in-range index gets.
TEST(IpqpKktLayoutTest, AFreshLayoutRefusesEverySlotAccessor) {
    const IpqpKktLayout layout;

    EXPECT_FALSE(layout.has_structure());
    EXPECT_THROW((void)layout.diag_pos(), std::logic_error);
    EXPECT_THROW((void)layout.primal_diag_source(), std::logic_error);
    EXPECT_THROW((void)layout.primal_diag_slot(0), std::logic_error);
    EXPECT_THROW((void)layout.slack_diag_slot(0), std::logic_error);
    EXPECT_THROW((void)layout.eq_pivot_slot(0), std::logic_error);
    EXPECT_THROW((void)layout.iq_pivot_slot(0), std::logic_error);
    EXPECT_THROW((void)layout.slack_coupling_slot(0), std::logic_error);
}

// I1, the half with a reachable throw site: a failed sync leaves the object entirely on its
// previous plan -- the dimensions are committed WITH the tables, at the end, so an accessor
// can never bounds-check a new n against a stale table. `.superpowers/w1-t3-report.md` I1.
TEST(IpqpKktLayoutTest, AFailedResyncLeavesTheObjectOnItsPreviousPlan) {
    const Fixture f = general_fixture(); // n = 3, me = 1, mi = 2
    IpqpKktLayout layout;
    SpMatRM k;
    ASSERT_TRUE(layout.sync(f.H, f.Ae, f.Ai, f.n, f.me, f.mi, k));

    const Index dim_before = layout.dim();
    const Index nnz_before = k.nonZeros();
    std::vector<std::size_t> slots_before;
    for (Index i = 0; i < f.n; ++i) {
        slots_before.push_back(layout.primal_diag_slot(i));
    }

    // A re-sync at a LARGER n that throws at the deepest validation there is
    // -- the upper-triangle scan, which runs after every shape check has
    // passed.
    const SpMatRM big_h =
        sparse_from(6, 6, {{0, 0, 1.0}, {3, 1, 2.0}, {4, 4, 1.0}, {5, 5, 1.0}}); // (3,1) is lower
    const SpMatRM big_ae = sparse_from(1, 6, {{0, 0, 1.0}});
    const SpMatRM big_ai = sparse_from(2, 6, {{0, 0, 1.0}, {1, 5, 1.0}});
    EXPECT_THROW(layout.sync(big_h, big_ae, big_ai, 6, 1, 2, k), std::invalid_argument);

    // Everything still describes the n = 3 plan.
    EXPECT_TRUE(layout.has_structure());
    EXPECT_EQ(layout.dim(), dim_before);
    EXPECT_EQ(k.nonZeros(), nnz_before); // the caller's buffer was not touched
    ASSERT_EQ(static_cast<Index>(layout.primal_diag_source().size()), f.n);
    for (Index i = 0; i < f.n; ++i) {
        EXPECT_EQ(layout.primal_diag_slot(i), slots_before[static_cast<std::size_t>(i)]);
    }
    // And an index that would have been in range for the ATTEMPTED n is
    // refused rather than read out of bounds.
    EXPECT_THROW((void)layout.primal_diag_slot(3), std::out_of_range);
    EXPECT_THROW((void)layout.primal_diag_slot(5), std::out_of_range);

    // The object is still usable on its old plan: a reuse scatter succeeds and
    // still reproduces the reference.
    ASSERT_FALSE(layout.sync(f.H, f.Ae, f.Ai, f.n, f.me, f.mi, k));
    expect_identical(k, reference_kkt(f.H, f.Ae, f.Ai, f.n, f.me, f.mi));
}

// M7. The degenerate shape is legal here and rejected downstream; pinned so
// the behaviour is recorded rather than discovered.
TEST(IpqpKktLayoutTest, TheEmptyProblemLaysOutAnEmptyMatrix) {
    IpqpKktLayout layout;
    SpMatRM k;
    const SpMatRM h0 = sparse_from(0, 0, {});
    const SpMatRM a0 = sparse_from(0, 0, {});

    ASSERT_TRUE(layout.sync(h0, a0, a0, 0, 0, 0, k));
    EXPECT_EQ(layout.dim(), 0);
    EXPECT_EQ(k.rows(), 0);
    EXPECT_EQ(k.nonZeros(), 0);
    EXPECT_TRUE(layout.diag_pos().empty());
    EXPECT_TRUE(layout.primal_diag_source().empty());
    // Every block is empty, so every slot index is out of range.
    EXPECT_THROW((void)layout.primal_diag_slot(0), std::out_of_range);
    EXPECT_THROW((void)layout.slack_coupling_slot(0), std::out_of_range);
    // A reuse sync is a no-op that still reports "not relaid".
    EXPECT_FALSE(layout.sync(h0, a0, a0, 0, 0, 0, k));
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
