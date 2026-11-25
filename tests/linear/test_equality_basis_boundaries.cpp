// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_equality_basis_boundaries.cpp — the equality basis's boundary behaviour: the states
// it can be in before compute(), the refusals of compute() itself and of every operation on a
// basis that dropped a row, the rhs-only entry point, the group probes, the gap bands'
// non-finite arm, and EqualityRowSubsetModel's pass-through. The numerical-rank contract
// itself is tested in tests/sqp/test_equality_basis_unit.cpp; this file asserts the
// surrounding contract (hven/detail/sqp/equality_basis.h) and sits in tests/linear/ because
// every factorization the basis makes goes through hven::linear.

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Dense>
#include <gtest/gtest.h>

#include <hven/core/compiler.h>
#include <hven/detail/model/nlp_model.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/detail/sqp/equality_basis.h>

namespace hven::detail {
namespace {

SpMatRM to_sparse(const Eigen::MatrixXd &A) {
    SpMatRM S = A.sparseView();
    S.makeCompressed();
    return S;
}

Vec free_box(Index n) { return Vec::Constant(n, 1e20); }

// Runs f and expects std::invalid_argument whose message contains every fragment.
template <class F> void expect_refusal(F &&f, std::initializer_list<const char *> fragments) {
    try {
        f();
        ADD_FAILURE() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        const std::string message(e.what());
        for (const char *fragment : fragments) {
            EXPECT_NE(message.find(fragment), std::string::npos)
                << "missing '" << fragment << "' in: " << message;
        }
    }
}

// Four rows over five columns; row 2 = row 0 + row 1 exactly, so exactly one of rows {0,1,2}
// is dropped. Column 4 is touched only by row 3.
Eigen::MatrixXd one_dependence() {
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(4, 5);
    A.row(0) << 1.0, 2.0, 0.0, 0.0, 0.0;
    A.row(1) << 0.0, 1.0, 3.0, 0.0, 0.0;
    A.row(2) = A.row(0) + A.row(1);
    A.row(3) << 0.0, 0.0, 0.0, 1.0, 1.0;
    return A;
}

struct Computed {
    Eigen::MatrixXd A = one_dependence();
    SpMatRM Je = to_sparse(A);
    Vec lower = -free_box(5);
    Vec upper = free_box(5);
    EqualityBasis basis;
    Computed() { basis.compute(Je, lower, upper); }
};

TEST(EqualityBasisBoundaries, ADefaultBasisIsTheEmptyIdentity) {
    const EqualityBasis basis;
    EXPECT_EQ(basis.verdict(), EqualityBasisVerdict::kNotComputed);
    EXPECT_EQ(basis.refusal(), EqualityBasisRefusal::kNone);
    EXPECT_FALSE(basis.reduces());
    EXPECT_EQ(basis.me(), 0);
    EXPECT_TRUE(basis.retained().empty());
    EXPECT_TRUE(basis.dropped().empty());
    EXPECT_EQ(basis.fold(Vec(0)).size(), 0);
}

TEST(EqualityBasisBoundaries, ResetReturnsToTheIdentityOverMeRows) {
    Computed c;
    ASSERT_EQ(c.basis.verdict(), EqualityBasisVerdict::kReduced);
    ASSERT_TRUE(c.basis.reduces());

    c.basis.reset(4);
    EXPECT_EQ(c.basis.verdict(), EqualityBasisVerdict::kNotComputed);
    EXPECT_FALSE(c.basis.reduces());
    EXPECT_EQ(c.basis.me(), 4);
    EXPECT_EQ(c.basis.retained(), (std::vector<Index>{0, 1, 2, 3}));
    EXPECT_TRUE(c.basis.dropped().empty());
    EXPECT_EQ(c.basis.coefficients().rows(), 4);
    EXPECT_EQ(c.basis.coefficients().cols(), 0);

    // Every operation is the identity.
    const Vec lambda = Vec::LinSpaced(4, 1.0, 4.0);
    EXPECT_EQ(c.basis.fold(lambda), lambda);
    Vec out;
    double bound = -1.0;
    EXPECT_TRUE(c.basis.lift(lambda, out, &bound));
    EXPECT_EQ(out, lambda);
    EXPECT_EQ(bound, 0.0);
    Vec t;
    bound = -1.0;
    EXPECT_TRUE(c.basis.project_rhs(Vec(0), t, &bound));
    EXPECT_EQ(t, Vec::Zero(4));
    EXPECT_EQ(bound, 0.0);
    // Nothing dropped: verify() has nothing to check, whatever matrix it is handed.
    EXPECT_TRUE(c.basis.verify(SpMatRM(2, 2)));
}

TEST(EqualityBasisBoundaries, ComputeRefusesMismatchedBoundsByName) {
    const SpMatRM Je = to_sparse(one_dependence());
    EqualityBasis basis;
    expect_refusal([&] { basis.compute(Je, -free_box(4), free_box(5)); },
                   {"EqualityBasis::compute", "Je is 4x5", "sizes 4/5"});
    expect_refusal([&] { basis.compute(Je, -free_box(5), free_box(6)); }, {"sizes 5/6"});
    // A refused compute leaves the basis as it was.
    EXPECT_EQ(basis.verdict(), EqualityBasisVerdict::kNotComputed);
}

TEST(EqualityBasisBoundaries, ComputeRefusesEachBadToleranceByName) {
    const SpMatRM Je = to_sparse(one_dependence());
    const Vec lo = -free_box(5);
    const Vec hi = free_box(5);
    const auto refused = [&](const EqualityBasisTolerances &tol) {
        EqualityBasis basis;
        expect_refusal([&] { basis.compute(Je, lo, hi, tol); },
                       {"tolerances must satisfy pivot_factor > 0"});
    };
    EqualityBasisTolerances tol;
    tol.pivot_factor = 0.0;
    refused(tol);
    tol = {};
    tol.pivot_factor = std::numeric_limits<double>::quiet_NaN();
    refused(tol);
    tol = {};
    tol.gap_ratio = 0.5;
    refused(tol);
    tol = {};
    tol.coef_cap = 0.0;
    refused(tol);
    tol = {};
    tol.lift_forward_tol = -1.0;
    refused(tol);
    tol = {};
    tol.gram_pivot_perturb_exp = -1;
    refused(tol);

    // The boundary values themselves are legal.
    tol = {};
    tol.gap_ratio = 1.0;
    EqualityBasis ok;
    EXPECT_NO_THROW(ok.compute(Je, lo, hi, tol));
    tol = {};
    tol.gram_pivot_perturb_exp = 0;
    EXPECT_NO_THROW(ok.compute(Je, lo, hi, tol));
}

TEST(EqualityBasisBoundaries, ADroppedRowBasisRefusesEveryMismatchedSizeByName) {
    Computed c;
    ASSERT_EQ(c.basis.verdict(), EqualityBasisVerdict::kReduced);
    ASSERT_EQ(c.basis.dropped().size(), 1u);
    ASSERT_EQ(c.basis.retained().size(), 3u);

    Vec out;
    EXPECT_THROW(c.basis.lift(Vec::Zero(4), out), std::invalid_argument);
    expect_refusal([&] { c.basis.lift(Vec::Zero(4), out); }, {"lift", "expected 3", "|B|"});
    expect_refusal([&] { c.basis.fold(Vec::Zero(3)); }, {"fold", "expected 4", "me"});
    expect_refusal([&] { c.basis.project_rhs(Vec::Zero(2), out); },
                   {"project_rhs", "expected 1", "|D|"});
    expect_refusal([&] { c.basis.reduce_rhs(Vec::Zero(3), c.Je, c.lower, out); },
                   {"reduce_rhs", "disagree with the basis (4 x 5)"});
    expect_refusal([&] { c.basis.reduce_rhs(Vec::Zero(4), c.Je, Vec::Zero(4), out); },
                   {"reduce_rhs", "lower 4"});
    expect_refusal([&] { c.basis.reduce_rhs(Vec::Zero(4), SpMatRM(4, 4), c.lower, out); },
                   {"reduce_rhs", "Ae 4x4"});

    Vec gap;
    std::vector<EqualityGapBand> band;
    expect_refusal(
        [&] { c.basis.read_gaps(Vec::Zero(3), c.Je, Vec::Zero(5), 1e-8, gap, band); },
        {"read_gaps", "disagree with the basis (4 x 5)"});
    expect_refusal(
        [&] { c.basis.read_gaps(Vec::Zero(4), c.Je, Vec::Zero(4), 1e-8, gap, band); },
        {"read_gaps", "x 4"});

    expect_refusal([&] { c.basis.group_bytes(c.Je, 1); }, {"group_bytes", "dropped index 1"});
    expect_refusal([&] { c.basis.group_bytes(c.Je, -1); }, {"group_bytes", "dropped index -1"});
    expect_refusal([&] { c.basis.group_bytes(SpMatRM(3, 5), 0); }, {"group_bytes", "3 rows"});
    expect_refusal([&] { c.basis.group_hash(c.Je, 5); }, {"group_bytes"});
    expect_refusal([&] { c.basis.group_free_columns(c.Je, 1); },
                   {"group_free_columns", "dropped index 1"});
    expect_refusal([&] { c.basis.group_free_columns(SpMatRM(4, 4), 0); },
                   {"group_free_columns", "4x4", "expected 4x5"});

    QpProblem qp;
    qp.H = SpMatRM(5, 5);
    qp.g = Vec::Zero(5);
    qp.Ae = SpMatRM(3, 5);
    qp.be = Vec::Zero(3);
    qp.Ai = SpMatRM(0, 5);
    qp.bi = Vec(0);
    qp.lower = -free_box(5);
    qp.upper = free_box(5);
    expect_refusal([&] { c.basis.reduce(qp); }, {"EqualityBasis::reduce", "me=3 n=5"});
}

TEST(EqualityBasisBoundaries, ReduceRhsIsTheRowRestrictionReduceAppliesToItsQp) {
    Computed c;
    ASSERT_TRUE(c.basis.reduces());
    // An inconsistent rhs makes the correction t nonzero, so the comparison is not vacuous.
    Vec be(4);
    be << 1.0, 2.0, 7.0, -1.0;

    Vec reduced;
    double rhs_bound = -1.0;
    ASSERT_TRUE(c.basis.reduce_rhs(be, c.Je, c.lower, reduced, &rhs_bound));
    ASSERT_EQ(reduced.size(), 3);
    EXPECT_TRUE(std::isfinite(rhs_bound));
    EXPECT_GE(rhs_bound, 0.0);

    QpProblem qp;
    qp.H = SpMatRM(5, 5);
    qp.g = Vec::Zero(5);
    qp.Ae = c.Je;
    qp.be = be;
    qp.Ai = SpMatRM(0, 5);
    qp.bi = Vec(0);
    qp.lower = c.lower;
    qp.upper = c.upper;
    double qp_bound = -1.0;
    ASSERT_TRUE(c.basis.reduce(qp, &qp_bound));
    EXPECT_EQ(qp.me(), 3);
    EXPECT_EQ(qp.be, reduced);
    EXPECT_EQ(qp_bound, rhs_bound);

    // The retained rows of Ae are exactly the rows B of the original.
    const Eigen::MatrixXd Ae = qp.Ae;
    for (std::size_t k = 0; k < c.basis.retained().size(); ++k) {
        EXPECT_EQ(Ae.row(static_cast<Index>(k)), c.A.row(c.basis.retained()[k]));
    }
    // A consistent rhs is passed through unchanged on the retained rows.
    Vec consistent = c.A * Vec::LinSpaced(5, 1.0, 2.0);
    Vec out;
    ASSERT_TRUE(c.basis.reduce_rhs(consistent, c.Je, c.lower, out));
    for (std::size_t k = 0; k < c.basis.retained().size(); ++k) {
        EXPECT_NEAR(out(static_cast<Index>(k)), consistent(c.basis.retained()[k]), 1e-12);
    }
}

TEST(EqualityBasisBoundaries, ReduceCompressesAnUncompressedAeBeforeRestricting) {
    Computed c;
    ASSERT_TRUE(c.basis.reduces());
    QpProblem qp;
    qp.H = SpMatRM(5, 5);
    qp.g = Vec::Zero(5);
    qp.Ae = c.Je;
    // Reserve slack in each row so the matrix is stored uncompressed.
    qp.Ae.uncompress();
    ASSERT_FALSE(qp.Ae.isCompressed());
    qp.be = c.A * Vec::Ones(5);
    qp.Ai = SpMatRM(0, 5);
    qp.bi = Vec(0);
    qp.lower = c.lower;
    qp.upper = c.upper;
    ASSERT_TRUE(c.basis.reduce(qp));
    EXPECT_EQ(qp.me(), 3);
    EXPECT_TRUE(qp.Ae.isCompressed());
}

TEST(EqualityBasisBoundaries, AFullRankBasisReducesNothingAndReportsAZeroBound) {
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(2, 3);
    A.row(0) << 1.0, 0.0, 1.0;
    A.row(1) << 0.0, 1.0, 1.0;
    const SpMatRM Je = to_sparse(A);
    EqualityBasis basis;
    basis.compute(Je, -free_box(3), free_box(3));
    ASSERT_EQ(basis.verdict(), EqualityBasisVerdict::kFullRank);
    EXPECT_EQ(basis.refusal(), EqualityBasisRefusal::kNone);
    EXPECT_FALSE(basis.reduces());
    EXPECT_GT(basis.min_retained_pivot(), 0.0);
    EXPECT_EQ(basis.jacobian_hash(), EqualityBasis::hash_jacobian(Je));

    QpProblem qp;
    qp.H = SpMatRM(3, 3);
    qp.g = Vec::Zero(3);
    qp.Ae = Je;
    qp.be = Vec::Ones(2);
    qp.Ai = SpMatRM(0, 3);
    qp.bi = Vec(0);
    qp.lower = -free_box(3);
    qp.upper = free_box(3);
    double bound = -1.0;
    ASSERT_TRUE(basis.reduce(qp, &bound));
    EXPECT_EQ(bound, 0.0);
    EXPECT_EQ(qp.me(), 2);
    EXPECT_EQ(qp.be, Vec::Ones(2));

    // With nothing dropped even a wrong-sized qp is not inspected (the identity).
    QpProblem other = qp;
    other.Ae = SpMatRM(1, 3);
    other.be = Vec::Zero(1);
    EXPECT_TRUE(basis.reduce(other));
    EXPECT_EQ(other.me(), 1);

    Vec gap;
    std::vector<EqualityGapBand> band;
    basis.read_gaps(Vec::Zero(2), Je, Vec::Zero(3), 1e-8, gap, band);
    EXPECT_EQ(gap.size(), 0);
    EXPECT_TRUE(band.empty());
}

TEST(EqualityBasisBoundaries, ADenseColumnPastTheFillGuardIsRefusedBeforeAnyNumericWork) {
    // One column shared by every row makes the Gram matrix dense: sum_f c_f (c_f + 1) / 2 far
    // above the guard's limit. The split is refused (kFill), nothing is dropped, and no
    // factorization was taken.
    const Index m = 1600;
    std::vector<Eigen::Triplet<double>> t;
    for (Index i = 0; i < m; ++i) {
        t.emplace_back(static_cast<int>(i), 0, 1.0);
        t.emplace_back(static_cast<int>(i), static_cast<int>(i + 1), 1.0);
    }
    SpMatRM Je(m, m + 1);
    Je.setFromTriplets(t.begin(), t.end());
    Je.makeCompressed();
    EqualityBasis basis;
    basis.compute(Je, -free_box(m + 1), free_box(m + 1));
    EXPECT_EQ(basis.verdict(), EqualityBasisVerdict::kUndecidable);
    EXPECT_EQ(basis.refusal(), EqualityBasisRefusal::kFill);
    EXPECT_FALSE(basis.reduces());
    EXPECT_EQ(basis.retained().size(), static_cast<std::size_t>(m));
    EXPECT_EQ(basis.gram_factorizations(), 0);
    EXPECT_EQ(basis.max_coefficient(), 0.0);
    // The refused basis is the identity: every operation passes its argument through.
    const Vec lambda = Vec::Ones(m);
    EXPECT_EQ(basis.fold(lambda), lambda);
    Vec out;
    EXPECT_TRUE(basis.lift(lambda, out));
    EXPECT_EQ(out, lambda);
}

TEST(EqualityBasisBoundaries, ARowWhoseEntriesAreAllOnFixedColumnsIsDroppedWithAZeroCoefficient) {
    // Row 2 touches only column 1, which is fixed: on the free columns it is empty, so it is
    // dropped outright, with no support row.
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(3, 4);
    A.row(0) << 1.0, 0.0, 2.0, 0.0;
    A.row(1) << 0.0, 0.0, 1.0, 3.0;
    A.row(2) << 0.0, 5.0, 0.0, 0.0;
    const SpMatRM Je = to_sparse(A);
    Vec lower = -free_box(4);
    Vec upper = free_box(4);
    lower(1) = upper(1) = 0.5;
    EqualityBasis basis;
    basis.compute(Je, lower, upper);
    ASSERT_EQ(basis.verdict(), EqualityBasisVerdict::kReduced);
    EXPECT_EQ(basis.dropped(), (std::vector<Index>{2}));
    EXPECT_EQ(basis.retained(), (std::vector<Index>{0, 1}));
    EXPECT_EQ(basis.coefficients().nonZeros(), 0);
    EXPECT_TRUE(basis.verify(Je));
    EXPECT_TRUE(basis.group_free_columns(Je, 0).empty());

    // The dropped row's gap is its own residual at the fixed bound: 5 * (0.5 - x1) + ce(2).
    Vec gap;
    std::vector<EqualityGapBand> band;
    Vec x = Vec::Zero(4);
    x(1) = 0.5;
    basis.read_gaps(Vec::Zero(3), Je, x, 1e-8, gap, band);
    EXPECT_EQ(band[0], EqualityGapBand::kNoise);
    x(1) = 0.0;
    basis.read_gaps(Vec::Zero(3), Je, x, 1e-8, gap, band);
    EXPECT_EQ(band[0], EqualityGapBand::kInconsistent);
    EXPECT_NEAR(gap(0), 2.5, 1e-12);

    // Reducing leaves the retained rows' rhs untouched (the correction through an empty
    // coefficient column is zero) and keeps rows 0 and 1.
    Vec be(3);
    be << 1.0, 2.0, 7.0;
    Vec reduced;
    ASSERT_TRUE(basis.reduce_rhs(be, Je, lower, reduced));
    ASSERT_EQ(reduced.size(), 2);
    EXPECT_NEAR(reduced(0), 1.0, 1e-14);
    EXPECT_NEAR(reduced(1), 2.0, 1e-14);
}

TEST(EqualityBasisBoundaries, TheJacobianHashCoversShapePatternAndValues) {
    const Eigen::MatrixXd A = one_dependence();
    const SpMatRM Je = to_sparse(A);
    const std::uint64_t h = EqualityBasis::hash_jacobian(Je);
    EXPECT_EQ(h, EqualityBasis::hash_jacobian(to_sparse(A)));

    Eigen::MatrixXd valued = A;
    valued(0, 0) = 1.5;
    EXPECT_NE(h, EqualityBasis::hash_jacobian(to_sparse(valued)));

    // Same values, an extra stored zero: the pattern differs.
    SpMatRM patterned = Je;
    patterned.coeffRef(3, 0) = 0.0;
    patterned.makeCompressed();
    EXPECT_NE(h, EqualityBasis::hash_jacobian(patterned));

    // Same nonzeros, one more (empty) column: the shape differs.
    Eigen::MatrixXd wider(4, 6);
    wider << A, Eigen::VectorXd::Zero(4);
    EXPECT_NE(h, EqualityBasis::hash_jacobian(to_sparse(wider)));
}

TEST(EqualityBasisBoundaries, TheHashIsTheSameForAnUncompressedMatrixAndVerifyRefusesAnotherShape) {
    Computed c;
    SpMatRM loose = c.Je;
    loose.uncompress();
    ASSERT_FALSE(loose.isCompressed());
    EXPECT_EQ(EqualityBasis::hash_jacobian(loose), EqualityBasis::hash_jacobian(c.Je));
    EXPECT_EQ(c.basis.jacobian_hash(), EqualityBasis::hash_jacobian(c.Je));

    ASSERT_TRUE(c.basis.reduces());
    EXPECT_TRUE(c.basis.verify(c.Je));
    EXPECT_FALSE(c.basis.verify(SpMatRM(3, 5))) << "wrong row count";
    EXPECT_FALSE(c.basis.verify(SpMatRM(4, 6))) << "wrong column count";
    // A NaN in the matrix cannot be verified.
    Eigen::MatrixXd bad = c.A;
    bad(0, 0) = std::numeric_limits<double>::quiet_NaN();
    EXPECT_FALSE(c.basis.verify(to_sparse(bad)));
}

TEST(EqualityBasisBoundaries, GroupFreeColumnsAreTheSortedFreeUnionOfTheGroup) {
    // Fix column 1 (touched by rows 0, 1 and 2): it must vanish from every group, and the
    // group is the union over the dropped row and its support rows.
    const Eigen::MatrixXd A = one_dependence();
    const SpMatRM Je = to_sparse(A);
    Vec lower = -free_box(5);
    Vec upper = free_box(5);
    lower(1) = upper(1) = 0.25;
    EqualityBasis basis;
    basis.compute(Je, lower, upper);
    ASSERT_EQ(basis.verdict(), EqualityBasisVerdict::kReduced);
    ASSERT_EQ(basis.dropped().size(), 1u);

    const std::vector<Index> cols = basis.group_free_columns(Je, 0);
    EXPECT_FALSE(cols.empty());
    EXPECT_TRUE(std::is_sorted(cols.begin(), cols.end()));
    EXPECT_EQ(std::adjacent_find(cols.begin(), cols.end()), cols.end()) << "duplicates";
    for (Index col : cols) {
        EXPECT_NE(col, 1) << "a fixed column is not a free column";
        EXPECT_NE(col, 3);
        EXPECT_NE(col, 4); // only row 3 touches these, and row 3 is outside every group
    }
    // Rows 0 and 1 survive on columns 0 and 2, row 2 equals their sum there.
    EXPECT_EQ(cols, (std::vector<Index>{0, 2}));

    // Without the fixing, column 1 belongs to the group.
    Computed free;
    const std::vector<Index> all = free.basis.group_free_columns(free.Je, 0);
    EXPECT_EQ(all, (std::vector<Index>{0, 1, 2}));

    // Byte-identical groups hash equal; a value change in the group changes the hash.
    const std::uint64_t h = free.basis.group_hash(free.Je, 0);
    EXPECT_EQ(h, free.basis.group_hash(free.Je, 0));
    Eigen::MatrixXd moved = free.A;
    moved(3, 3) = 9.0; // outside the group
    EXPECT_EQ(h, free.basis.group_hash(to_sparse(moved), 0));
    Eigen::MatrixXd perturbed = free.A;
    perturbed(0, 0) = 1.0 + 1e-9;
    EXPECT_NE(h, free.basis.group_hash(to_sparse(perturbed), 0));
}

TEST(EqualityBasisBoundaries, ReadGapsClassifiesNoiseFeasibleInconsistentAndNonFinite) {
    Computed c;
    ASSERT_EQ(c.basis.dropped().size(), 1u);
    const Index r = c.basis.dropped()[0];
    const Vec x = Vec::Zero(5);
    Vec gap;
    std::vector<EqualityGapBand> band;

    // ce consistent with the dependence (ce = A y for some y): the gap is rounding noise.
    Vec ce = c.A * Vec::LinSpaced(5, 1.0, 2.0);
    c.basis.read_gaps(ce, c.Je, x, 1e-8, gap, band);
    ASSERT_EQ(band.size(), 1u);
    EXPECT_EQ(band[0], EqualityGapBand::kNoise);
    EXPECT_NEAR(gap(0), 0.0, 1e-13);

    // A gap above the noise floor but within feas_tol (1 + ||C(:,r)||_1) is feasible.
    Vec shifted = ce;
    shifted(r) += 5e-10;
    c.basis.read_gaps(shifted, c.Je, x, 1e-8, gap, band);
    EXPECT_EQ(band[0], EqualityGapBand::kFeasible);
    EXPECT_NEAR(std::abs(gap(0)), 5e-10, 1e-12);

    // The same gap against a tighter tolerance has no point satisfying the group.
    c.basis.read_gaps(shifted, c.Je, x, 1e-12, gap, band);
    EXPECT_EQ(band[0], EqualityGapBand::kInconsistent);
    shifted(r) += 1.0;
    c.basis.read_gaps(shifted, c.Je, x, 1e-8, gap, band);
    EXPECT_EQ(band[0], EqualityGapBand::kInconsistent);

    // A gap that is not a number is evidence of nothing.
    Vec bad = ce;
    bad(r) = std::numeric_limits<double>::quiet_NaN();
    c.basis.read_gaps(bad, c.Je, x, 1e-8, gap, band);
    EXPECT_EQ(band[0], EqualityGapBand::kNonFinite);
    EXPECT_TRUE(std::isnan(gap(0)));
    bad(r) = std::numeric_limits<double>::infinity();
    c.basis.read_gaps(bad, c.Je, x, 1e-8, gap, band);
    EXPECT_EQ(band[0], EqualityGapBand::kNonFinite);
}

TEST(EqualityBasisBoundaries, ReadGapsMovesFixedColumnsToTheirBoundBeforeReading) {
    // Row 2 = row 0 + row 1 on the FREE columns only; on column 1 (fixed at 0.25) it differs
    // by one. The gap is g = N^T w with w = ce + Je d, d = (l - x) on the fixed column, so a
    // ce consistent at x = l (y1 = 0 contributes nothing) reads as noise there and as a gap of
    // magnitude 0.25 once x(1) sits at 0.
    Eigen::MatrixXd A = one_dependence();
    A(2, 1) += 1.0;
    const SpMatRM Je = to_sparse(A);
    Vec lower = -free_box(5);
    Vec upper = free_box(5);
    lower(1) = upper(1) = 0.25;
    EqualityBasis basis;
    basis.compute(Je, lower, upper);
    ASSERT_EQ(basis.verdict(), EqualityBasisVerdict::kReduced);
    ASSERT_EQ(basis.dropped().size(), 1u);

    Vec y = Vec::LinSpaced(5, -1.0, 1.0);
    y(1) = 0.0;
    const Vec ce = A * y;
    Vec gap;
    std::vector<EqualityGapBand> band;
    Vec x = Vec::Zero(5);
    x(1) = 0.25;
    basis.read_gaps(ce, Je, x, 1e-8, gap, band);
    EXPECT_EQ(band[0], EqualityGapBand::kNoise);

    x(1) = 0.0;
    basis.read_gaps(ce, Je, x, 1e-8, gap, band);
    EXPECT_EQ(band[0], EqualityGapBand::kInconsistent);
    EXPECT_NEAR(std::abs(gap(0)), 0.25, 1e-12);
}

// A three-row, two-column model that records the multipliers eval_hess saw.
class RecordingModel final : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 3; }
    Index mi() const override { return 1; }
    double eval_f(const Vec &x) const override { return 0.5 * x.squaredNorm(); }
    Vec eval_grad(const Vec &x) const override { return x; }
    Vec eval_ce(const Vec &x) const override {
        Vec c(3);
        c << x(0), x(1), x(0) + x(1);
        return c;
    }
    Vec eval_ci(const Vec &x) const override { return Vec::Constant(1, x(0) - 1.0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &lambda_e,
                      const Vec &lambda_i) const override {
        last_lambda_e = lambda_e;
        last_lambda_i = lambda_i;
        last_obj_scale = obj_scale;
        SpMatRM H(2, 2);
        H.setIdentity();
        return H;
    }
    SpMatRM eval_jac_e(const Vec &) const override {
        Eigen::MatrixXd J(3, 2);
        J << 1.0, 0.0, 0.0, 1.0, 1.0, 1.0;
        return to_sparse(J);
    }
    SpMatRM eval_jac_i(const Vec &) const override {
        Eigen::MatrixXd J(1, 2);
        J << 1.0, 0.0;
        return to_sparse(J);
    }
    bool has_hessian() const override { return hessian_; }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Constant(2, 0.5); }

    mutable Vec last_lambda_e;
    mutable Vec last_lambda_i;
    mutable double last_obj_scale = 0.0;
    bool hessian_ = true;

  private:
    Vec lower_ = Vec::Constant(2, -3.0);
    Vec upper_ = Vec::Constant(2, 3.0);
};

TEST(EqualityRowSubsetModelBoundaries, ConstructionRefusesRowsOutOfRangeOrNotIncreasing) {
    const RecordingModel model;
    expect_refusal([&] { EqualityRowSubsetModel(model, {3}); },
                   {"EqualityRowSubsetModel", "row 3 at position 0", "[0, 3)"});
    expect_refusal([&] { EqualityRowSubsetModel(model, {-1}); }, {"row -1"});
    expect_refusal([&] { EqualityRowSubsetModel(model, {0, 2, 1}); }, {"row 1 at position 2"});
    expect_refusal([&] { EqualityRowSubsetModel(model, {1, 1}); }, {"row 1 at position 1"});
    // The empty subset and the full set are legal.
    EXPECT_EQ(EqualityRowSubsetModel(model, {}).me(), 0);
    EXPECT_EQ(EqualityRowSubsetModel(model, {0, 1, 2}).me(), 3);
}

TEST(EqualityRowSubsetModelBoundaries, EverythingButTheEqualityBlockPassesThrough) {
    RecordingModel model;
    const EqualityRowSubsetModel sub(model, {0, 2});
    const Vec x = Vec::LinSpaced(2, 0.25, 0.5);
    EXPECT_EQ(sub.n(), 2);
    EXPECT_EQ(sub.me(), 2);
    EXPECT_EQ(sub.mi(), 1);
    EXPECT_EQ(sub.eval_f(x), model.eval_f(x));
    EXPECT_EQ(sub.lower(), model.lower());
    EXPECT_EQ(sub.upper(), model.upper());
    EXPECT_EQ(sub.start_point(), model.start_point());
    EXPECT_TRUE(sub.has_hessian());
    model.hessian_ = false;
    EXPECT_FALSE(sub.has_hessian());

    Vec g, ci;
    sub.eval_grad_in_place(x, g);
    EXPECT_EQ(g, x);
    sub.eval_ci_in_place(x, ci);
    EXPECT_EQ(ci, model.eval_ci(x));
    SpMatRM Ji;
    sub.eval_jac_i_in_place(x, Ji);
    EXPECT_EQ(Eigen::MatrixXd(Ji), Eigen::MatrixXd(model.eval_jac_i(x)));

    // The deprecated by-value entry points are the same routes.
    HVEN_SUPPRESS_DEPRECATED_BEGIN
    EXPECT_EQ(sub.eval_grad(x), x);
    EXPECT_EQ(sub.eval_ci(x), model.eval_ci(x));
    EXPECT_EQ(Eigen::MatrixXd(sub.eval_jac_i(x)), Eigen::MatrixXd(model.eval_jac_i(x)));
    const Vec ce = sub.eval_ce(x);
    ASSERT_EQ(ce.size(), 2);
    EXPECT_EQ(ce(0), x(0));
    EXPECT_EQ(ce(1), x(0) + x(1));
    const SpMatRM Je = sub.eval_jac_e(x);
    HVEN_SUPPRESS_DEPRECATED_END
    ASSERT_EQ(Je.rows(), 2);
    const Eigen::MatrixXd dense = Je;
    EXPECT_EQ(dense.row(0), (Eigen::RowVector2d(1.0, 0.0)));
    EXPECT_EQ(dense.row(1), (Eigen::RowVector2d(1.0, 1.0)));
}

TEST(EqualityRowSubsetModelBoundaries, OmittedRowsCarryAZeroMultiplierIntoTheHessian) {
    RecordingModel model;
    const EqualityRowSubsetModel sub(model, {0, 2});
    const Vec x = Vec::Zero(2);
    const Vec lambda_i = Vec::Constant(1, 7.0);
    Vec lambda_e(2);
    lambda_e << 4.0, 5.0;

    SpMatRM H;
    sub.eval_hess_in_place(x, 2.5, lambda_e, lambda_i, H);
    EXPECT_EQ(model.last_lambda_e, (Vec(3) << 4.0, 0.0, 5.0).finished());
    EXPECT_EQ(model.last_lambda_i, lambda_i);
    EXPECT_EQ(model.last_obj_scale, 2.5);
    EXPECT_EQ(H.rows(), 2);

    HVEN_SUPPRESS_DEPRECATED_BEGIN
    const SpMatRM H2 = sub.eval_hess(x, 1.0, lambda_e, lambda_i);
    HVEN_SUPPRESS_DEPRECATED_END
    EXPECT_EQ(H2.rows(), 2);
    EXPECT_EQ(model.last_lambda_e(1), 0.0);

    expect_refusal([&] { sub.eval_hess_in_place(x, 1.0, Vec::Zero(3), lambda_i, H); },
                   {"EqualityRowSubsetModel::eval_hess", "lambda_e has size 3", "expected 2"});
}

TEST(EqualityRowSubsetModelBoundaries, RestrictKeepsTheNamedRowsInOrder) {
    const RecordingModel model;
    const EqualityRowSubsetModel sub(model, {1, 2});
    Vec full(3);
    full << 10.0, 20.0, 30.0;
    EXPECT_EQ(sub.restrict(full), (Vec(2) << 20.0, 30.0).finished());

    Eigen::MatrixXd J(3, 2);
    J << 1.0, 0.0, 0.0, 2.0, 3.0, 4.0;
    const SpMatRM R = sub.restrict(to_sparse(J));
    EXPECT_TRUE(R.isCompressed());
    const Eigen::MatrixXd dense = R;
    ASSERT_EQ(dense.rows(), 2);
    EXPECT_EQ(dense.row(0), J.row(1));
    EXPECT_EQ(dense.row(1), J.row(2));
}

} // namespace
} // namespace hven::detail
