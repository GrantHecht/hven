#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <vector>

#include "hven/detail/model/nlp_adapter.h"
#include "hven/model/nlp_problem_model.h"

// UNITY-BUILD NOTE: this suite is compiled with UNITY_BUILD ON, so an anonymous
// namespace does not isolate these helpers from the other test TUs merged into
// the same batch. Every name below carries an npm_/Npm prefix for that reason.

namespace {
constexpr double kNpmInf = std::numeric_limits<double>::infinity();
} // namespace

using hven::ConstEigenRef;
using hven::solvers::NLPAdapterCore;
using hven::solvers::NLPCoordinate;
using hven::solvers::NLPProblem;
using hven::solvers::NlpProblemModel;
using hven::solvers::NLPRowKind;

namespace {

/// n = 2, one constraint row of every kind, and one Jacobian slot per row in
/// column 0 whose value is the row index plus one. g_r(x) = (r + 1) * x0, so
/// every residual and every Jacobian entry below is a hand-checkable number.
struct NpmKindsProblem : NLPProblem {
    mutable int n_eval_g_ = 0, n_eval_jac_ = 0;

    int num_vars() const override { return 2; }
    int num_cons() const override { return 5; }
    int num_jac_nonzeros() const override { return 5; }
    int num_hess_nonzeros() const override { return 1; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kNpmInf, -kNpmInf;
        xu << kNpmInf, kNpmInf;
        gl << 3.0, -kNpmInf, 1.0, 1.0, -kNpmInf;
        gu << 3.0, 2.0, kNpmInf, 4.0, kNpmInf;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override { f = x[0] * x[0]; }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = 2.0 * x[0];
        g[1] = 0.0;
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        n_eval_g_++;
        for (int r = 0; r < 5; r++) {
            g[r] = (r + 1) * x[0];
        }
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 2, 3, 4;
        c << 0, 0, 0, 0, 0;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0;
        c << 0;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        n_eval_jac_++;
        v << 1.0, 2.0, 3.0, 4.0, 5.0;
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = obj_factor + lambda.sum();
    }
    std::string name() const override { return "NpmKindsProblem"; }
};

/// Unconstrained, n = 2, and a declared Hessian structure that is genuinely
/// lower-triangular and carries a duplicate slot.
struct NpmHessProblem : NLPProblem {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 0; }
    int num_jac_nonzeros() const override { return 0; }
    int num_hess_nonzeros() const override { return 4; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>) const override {
        xl << -kNpmInf, -kNpmInf;
        xu << kNpmInf, kNpmInf;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override { f = x.squaredNorm(); }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g = 2.0 * x;
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>) const override {}
    void jac_structure(Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>) const override {}
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 1, 1; // (1, 0) is declared twice: duplicates are legal and sum
        c << 0, 0, 1, 0;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>) const override {}
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v << 2.0 * obj_factor, 0.25, 4.0 * obj_factor, 0.75;
    }
    std::string name() const override { return "NpmHessProblem"; }
};

/// Bounds chosen to separate "unbounded" from "a large finite bound": variable
/// 0 is bounded at +/-1e20, variable 1 is genuinely free, row 0 is an equality
/// at 1e20 and row 1 is a range between -1e20 and 1e20.
struct NpmLargeBoundProblem : NLPProblem {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 2; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 1; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -1e20, -kNpmInf;
        xu << 1e20, kNpmInf;
        gl << 1e20, -1e20;
        gu << 1e20, 1e20;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override { f = x[0]; }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> g) const override {
        g << 1.0, 0.0;
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g << x[0], x[1];
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0;
        c << 0;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v << 1.0, 1.0;
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double, ConstEigenRef<Eigen::VectorXd>,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 0.0;
    }
    std::string name() const override { return "NpmLargeBoundProblem"; }
};

/// Variable bounds that leave the origin outside the box on two of three
/// variables, so a projected start point is distinguishable from the origin.
struct NpmBoxedProblem : NpmHessProblem {
    int num_vars() const override { return 3; }
    int num_hess_nonzeros() const override { return 3; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>) const override {
        xl << 1.0, -kNpmInf, -kNpmInf;
        xu << 5.0, -2.0, kNpmInf;
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g = 2.0 * x;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 2;
        c << 0, 1, 2;
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v.setConstant(2.0 * obj_factor);
    }
    std::string name() const override { return "NpmBoxedProblem"; }
};

} // namespace

TEST(NlpProblemModelTest, RowKindsDecideTheNativeBlocksAndTheirSigns) {
    NlpProblemModel model(std::make_shared<NpmKindsProblem>());
    EXPECT_EQ(model.n(), 2);
    EXPECT_EQ(model.me(), 1);
    EXPECT_EQ(model.mi(), 4);
    EXPECT_EQ(model.num_declared_rows(), 5);
    EXPECT_EQ(model.rows().kinds_[0], NLPRowKind::Equality);
    EXPECT_EQ(model.rows().kinds_[1], NLPRowKind::UpperBounded);
    EXPECT_EQ(model.rows().kinds_[2], NLPRowKind::LowerBounded);
    EXPECT_EQ(model.rows().kinds_[3], NLPRowKind::Range);
    EXPECT_EQ(model.rows().kinds_[4], NLPRowKind::Free);

    Eigen::VectorXd x(2);
    x << 2.0, 0.0;

    const Eigen::VectorXd ce = model.eval_ce(x);
    ASSERT_EQ(ce.size(), 1);
    EXPECT_DOUBLE_EQ(ce[0], 1.0 * 2.0 - 3.0); // g - gl

    const Eigen::VectorXd ci = model.eval_ci(x);
    ASSERT_EQ(ci.size(), 4);
    EXPECT_DOUBLE_EQ(ci[0], 2.0 * 2.0 - 2.0); // upper-bounded: g - gu
    EXPECT_DOUBLE_EQ(ci[1], 1.0 - 3.0 * 2.0); // lower-bounded: gl - g
    EXPECT_DOUBLE_EQ(ci[2], 4.0 * 2.0 - 4.0); // range, upper part
    EXPECT_DOUBLE_EQ(ci[3], 1.0 - 4.0 * 2.0); // range, lower part

    const Eigen::SparseMatrix<double, Eigen::RowMajor> je = model.eval_jac_e(x);
    ASSERT_EQ(je.rows(), 1);
    ASSERT_EQ(je.cols(), 2);
    EXPECT_DOUBLE_EQ(je.coeff(0, 0), 1.0);

    const Eigen::SparseMatrix<double, Eigen::RowMajor> ji = model.eval_jac_i(x);
    ASSERT_EQ(ji.rows(), 4);
    EXPECT_DOUBLE_EQ(ji.coeff(0, 0), 2.0);  // upper-bounded keeps the sign
    EXPECT_DOUBLE_EQ(ji.coeff(1, 0), -3.0); // lower-bounded negates
    EXPECT_DOUBLE_EQ(ji.coeff(2, 0), 4.0);  // range: upper part, then
    EXPECT_DOUBLE_EQ(ji.coeff(3, 0), -4.0); // the negated lower part
}

TEST(NlpProblemModelTest, MultiplierShapesMapBothWays) {
    NlpProblemModel model(std::make_shared<NpmKindsProblem>());

    Eigen::VectorXd le(1), li(4);
    le << 0.5;
    li << 1.0, 2.0, 3.0, 4.0;
    const Eigen::VectorXd lam = model.compose_user_multipliers(le, li);
    ASSERT_EQ(lam.size(), 5);
    EXPECT_DOUBLE_EQ(lam[0], 0.5);  // equality passes through
    EXPECT_DOUBLE_EQ(lam[1], 1.0);  // upper-bounded passes through
    EXPECT_DOUBLE_EQ(lam[2], -2.0); // lower-bounded negates
    EXPECT_DOUBLE_EQ(lam[3], -1.0); // range: upper minus lower
    EXPECT_DOUBLE_EQ(lam[4], 0.0);  // a dropped row carries no multiplier

    // Empty blocks mean all-zero, which is what a chain that never produced one
    // hands over.
    const Eigen::VectorXd none =
        model.compose_user_multipliers(Eigen::VectorXd(), Eigen::VectorXd());
    EXPECT_EQ(none.size(), 5);
    EXPECT_DOUBLE_EQ(none.norm(), 0.0);

    // Back the other way: a range row's signed value splits into two
    // non-negative parts, and the dropped row's value goes nowhere.
    Eigen::VectorXd declared(5), back_e, back_i;
    declared << 0.5, 1.0, -2.0, -1.0, 7.0;
    model.split_user_multipliers(declared, back_e, back_i);
    ASSERT_EQ(back_e.size(), 1);
    ASSERT_EQ(back_i.size(), 4);
    EXPECT_DOUBLE_EQ(back_e[0], 0.5);
    EXPECT_DOUBLE_EQ(back_i[0], 1.0);
    EXPECT_DOUBLE_EQ(back_i[1], 2.0);
    EXPECT_DOUBLE_EQ(back_i[2], 0.0);
    EXPECT_DOUBLE_EQ(back_i[3], 1.0);

    // compose(split(l)) == l for every declared l whose Free rows are zero.
    declared[4] = 0.0;
    model.split_user_multipliers(declared, back_e, back_i);
    const Eigen::VectorXd round_trip = model.compose_user_multipliers(back_e, back_i);
    EXPECT_TRUE(round_trip.isApprox(declared));

    // ...and a nonzero Free-row value is the one thing that does not survive:
    // it has no native image to come back from.
    declared[4] = 7.0;
    model.split_user_multipliers(declared, back_e, back_i);
    EXPECT_DOUBLE_EQ(model.compose_user_multipliers(back_e, back_i)[4], 0.0);

    // The other order is NOT an identity. compose is not injective on native
    // pairs: a Range row carrying (upper, lower) = (3, 4) composes to -1 and
    // splits back to (0, 1), which is a different pair with the same image.
    Eigen::VectorXd native_e(1), native_i(4), again_e, again_i;
    native_e << 0.0;
    native_i << 0.0, 0.0, 3.0, 4.0;
    const Eigen::VectorXd composed = model.compose_user_multipliers(native_e, native_i);
    EXPECT_DOUBLE_EQ(composed[3], -1.0);
    model.split_user_multipliers(composed, again_e, again_i);
    EXPECT_DOUBLE_EQ(again_i[2], 0.0);
    EXPECT_DOUBLE_EQ(again_i[3], 1.0);
    EXPECT_FALSE(again_i.isApprox(native_i));
    // It does come back where a Range row has at most one nonzero side.
    native_i << 0.0, 0.0, 3.0, 0.0;
    model.split_user_multipliers(model.compose_user_multipliers(native_e, native_i), again_e,
                                 again_i);
    EXPECT_TRUE(again_i.isApprox(native_i));

    Eigen::VectorXd wrong(4);
    wrong.setZero();
    EXPECT_THROW(model.split_user_multipliers(wrong, back_e, back_i), std::invalid_argument);
}

TEST(NlpProblemModelTest, TheDeclaredLowerTriangleBecomesTheNativeUpperTriangle) {
    NlpProblemModel model(std::make_shared<NpmHessProblem>());
    Eigen::VectorXd x(2);
    x << 0.3, -0.7;

    const Eigen::SparseMatrix<double, Eigen::RowMajor> h =
        model.eval_hess(x, 2.0, Eigen::VectorXd(), Eigen::VectorXd());
    ASSERT_EQ(h.rows(), 2);
    ASSERT_EQ(h.cols(), 2);
    // Three distinct coordinates from four declared slots: (1, 0) was declared
    // twice and its two values are summed into the one native entry.
    EXPECT_EQ(h.nonZeros(), 3);
    EXPECT_DOUBLE_EQ(h.coeff(0, 0), 4.0); // 2 * obj_factor
    EXPECT_DOUBLE_EQ(h.coeff(0, 1), 1.0); // 0.25 + 0.75, mirrored from (1, 0)
    EXPECT_DOUBLE_EQ(h.coeff(1, 1), 8.0); // 4 * obj_factor
    EXPECT_DOUBLE_EQ(h.coeff(1, 0), 0.0); // nothing below the diagonal

    for (int outer = 0; outer < h.outerSize(); outer++) {
        for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(h, outer); it; ++it) {
            EXPECT_LE(it.row(), it.col());
        }
    }
}

TEST(NlpProblemModelTest, LargeFiniteBoundsStayBoundsAndInfinitiesStayUnbounded) {
    auto problem = std::make_shared<NpmLargeBoundProblem>();
    NlpProblemModel model(problem);

    // Variable bounds, verbatim in both directions.
    EXPECT_DOUBLE_EQ(model.lower()[0], -1e20);
    EXPECT_DOUBLE_EQ(model.upper()[0], 1e20);
    EXPECT_TRUE(std::isfinite(model.lower()[0]));
    EXPECT_TRUE(std::isfinite(model.upper()[0]));
    EXPECT_EQ(model.lower()[1], -kNpmInf);
    EXPECT_EQ(model.upper()[1], kNpmInf);

    // A row declared at 1e20 on both sides is an equality AT 1e20, not a free
    // row and not an equality at infinity; a row between -1e20 and 1e20 is a
    // range, not a free row.
    EXPECT_EQ(model.rows().kinds_[0], NLPRowKind::Equality);
    EXPECT_EQ(model.rows().kinds_[1], NLPRowKind::Range);
    EXPECT_EQ(model.me(), 1);
    EXPECT_EQ(model.mi(), 2);

    Eigen::VectorXd x(2);
    x << 1.0, 2.0;
    EXPECT_DOUBLE_EQ(model.eval_ce(x)[0], 1.0 - 1e20);
    const Eigen::VectorXd ci = model.eval_ci(x);
    EXPECT_DOUBLE_EQ(ci[0], 2.0 - 1e20);
    EXPECT_DOUBLE_EQ(ci[1], -1e20 - 2.0);

    // And the host stages variable 0's bounds while leaving variable 1 free.
    auto core = std::make_shared<NLPAdapterCore>(std::make_shared<NlpProblemModel>(problem),
                                                 problem->name());
    EXPECT_DOUBLE_EQ(core->x_lower_[0], -1e20);
    EXPECT_DOUBLE_EQ(core->x_upper_[0], 1e20);
    EXPECT_EQ(core->x_lower_[1], -kNpmInf);
    EXPECT_EQ(core->x_upper_[1], kNpmInf);
}

TEST(NlpProblemModelTest, TheTwoBlocksShareOneCallbackPerIterate) {
    auto problem = std::make_shared<NpmKindsProblem>();
    NlpProblemModel model(problem);
    problem->n_eval_g_ = 0;
    problem->n_eval_jac_ = 0;

    Eigen::VectorXd x(2);
    x << 1.5, 0.0;
    model.eval_ce(x);
    model.eval_ci(x);
    model.eval_jac_e(x);
    model.eval_jac_i(x);
    EXPECT_EQ(problem->n_eval_g_, 1);
    EXPECT_EQ(problem->n_eval_jac_, 1);

    // A new iterate is a new evaluation.
    x << 2.5, 0.0;
    model.eval_ce(x);
    model.eval_jac_e(x);
    EXPECT_EQ(problem->n_eval_g_, 2);
    EXPECT_EQ(problem->n_eval_jac_, 2);

    Eigen::VectorXd wrong(3);
    wrong.setZero();
    EXPECT_THROW(model.eval_ce(wrong), std::invalid_argument);
}

TEST(NlpProblemModelTest, TheStartPointIsTheOriginProjectedOntoTheVariableBounds) {
    NlpProblemModel model(std::make_shared<NpmBoxedProblem>());
    const Eigen::VectorXd x0 = model.start_point();
    ASSERT_EQ(x0.size(), 3);
    EXPECT_DOUBLE_EQ(x0[0], 1.0);  // below its lower bound, so pushed up
    EXPECT_DOUBLE_EQ(x0[1], -2.0); // above its upper bound, so pushed down
    EXPECT_DOUBLE_EQ(x0[2], 0.0);  // free
}

TEST(NlpAdapterHostTest, APatternThatShiftsBetweenEvaluationsIsRefusedByName) {
    // The claim pass records coordinates once; every later evaluation is paired
    // with those slots positionally, so a return that presents a different
    // pattern must be refused rather than summed into another coordinate's slot.
    Eigen::SparseMatrix<double, Eigen::RowMajor> claimed(2, 2);
    claimed.insert(0, 0) = 1.0;
    claimed.insert(1, 1) = 1.0;
    claimed.makeCompressed();
    const std::vector<NLPCoordinate> recorded{{0, 0}, {1, 1}};
    EXPECT_NO_THROW(
        hven::solvers::nlp_require_claimed_pattern(claimed, recorded, "eval_jac_e", "NpmProbe"));

    // Same entry count, one coordinate moved.
    Eigen::SparseMatrix<double, Eigen::RowMajor> moved(2, 2);
    moved.insert(0, 1) = 1.0;
    moved.insert(1, 1) = 1.0;
    moved.makeCompressed();
    EXPECT_THROW(
        hven::solvers::nlp_require_claimed_pattern(moved, recorded, "eval_jac_e", "NpmProbe"),
        std::invalid_argument);

    // A different entry count is refused before any coordinate is compared.
    Eigen::SparseMatrix<double, Eigen::RowMajor> shrunk(2, 2);
    shrunk.insert(0, 0) = 1.0;
    shrunk.makeCompressed();
    EXPECT_THROW(
        hven::solvers::nlp_require_claimed_pattern(shrunk, recorded, "eval_jac_e", "NpmProbe"),
        std::invalid_argument);
}
