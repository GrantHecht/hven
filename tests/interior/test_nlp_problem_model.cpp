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
    Eigen::VectorXd xl_{{-1e20, -kNpmInf}}, xu_{{1e20, kNpmInf}};

    int num_vars() const override { return 2; }
    int num_cons() const override { return 2; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 1; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl = xl_;
        xu = xu_;
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

namespace {

/// A bare NlpModel -- not the triplet conversion -- whose inequality Jacobian
/// and Hessian patterns move once armed. n = 3, no equality rows, one
/// inequality row.
struct NpmDriftingModel : hven::solvers::NlpModel {
    mutable bool drifted_ = false;
    Eigen::VectorXd lower_{{-kNpmInf, -kNpmInf, -kNpmInf}};
    Eigen::VectorXd upper_{{kNpmInf, kNpmInf, kNpmInf}};

    hven::Index n() const override { return 3; }
    hven::Index me() const override { return 0; }
    hven::Index mi() const override { return 1; }

    double eval_f(const Eigen::VectorXd &x) const override { return x.squaredNorm(); }
    Eigen::VectorXd eval_grad(const Eigen::VectorXd &x) const override { return 2.0 * x; }
    Eigen::VectorXd eval_ce(const Eigen::VectorXd &) const override {
        return Eigen::VectorXd::Zero(0);
    }
    Eigen::VectorXd eval_ci(const Eigen::VectorXd &x) const override {
        Eigen::VectorXd ci(1);
        ci[0] = x[0] + x[2] - 1.0;
        return ci;
    }
    hven::SpMatRM eval_jac_e(const Eigen::VectorXd &) const override { return hven::SpMatRM(0, 3); }
    hven::SpMatRM eval_jac_i(const Eigen::VectorXd &) const override {
        hven::SpMatRM j(1, 3);
        j.insert(0, drifted_ ? 1 : 0) = 1.0;
        j.insert(0, 2) = 1.0;
        j.makeCompressed();
        return j;
    }
    hven::SpMatRM eval_hess(const Eigen::VectorXd &, double obj_scale, const Eigen::VectorXd &,
                            const Eigen::VectorXd &) const override {
        hven::SpMatRM h(3, 3);
        h.insert(0, 0) = 2.0 * obj_scale;
        h.insert(drifted_ ? 2 : 1, drifted_ ? 2 : 1) = 2.0 * obj_scale;
        h.makeCompressed();
        return h;
    }
    const Eigen::VectorXd &lower() const override { return lower_; }
    const Eigen::VectorXd &upper() const override { return upper_; }
    Eigen::VectorXd start_point() const override { return Eigen::VectorXd::Zero(3); }
};

} // namespace

TEST(NlpAdapterHostTest, AModelWhosePatternDriftsAfterTranscriptionIsRefused) {
    // The host's claims were laid over the pattern it saw at transcription. A
    // model that presents a different one afterwards must be refused on the
    // host's own path, not merely by the guard called in isolation.
    auto model = std::make_shared<NpmDriftingModel>();
    hven::solvers::NLPAdapterCore core(model, "NpmDriftingModel");

    Eigen::VectorXd x(3);
    x << 0.5, -0.25, 1.5;
    ASSERT_NO_THROW(core.refresh_jacobians(x));
    ASSERT_NO_THROW(core.eval_hessian_values(x, 1.0, Eigen::VectorXd(), Eigen::VectorXd(1)));

    model->drifted_ = true;
    Eigen::VectorXd x2(3);
    x2 << 0.6, -0.35, 1.4;
    EXPECT_THROW(core.refresh_jacobians(x2), std::invalid_argument);

    Eigen::VectorXd x3(3);
    x3 << 0.7, -0.45, 1.3;
    Eigen::VectorXd li(1);
    li << 0.0;
    EXPECT_THROW(core.eval_hessian_values(x3, 1.0, Eigen::VectorXd(), li), std::invalid_argument);

    // And through a full assembly, which is the path a solve takes.
    auto fresh = std::make_shared<NpmDriftingModel>();
    auto fresh_core = std::make_shared<hven::solvers::NLPAdapterCore>(fresh, "NpmDriftingModel");
    auto nlp = hven::solvers::make_nlp_program(fresh_core);
    Eigen::SparseMatrix<double, Eigen::RowMajor> kkt(nlp->kkt_dim_, nlp->kkt_dim_);
    nlp->analyze_sparsity(kkt);
    Eigen::Map<Eigen::VectorXd>(kkt.valuePtr(), kkt.nonZeros()).setZero();
    double val = 0.0;
    Eigen::VectorXd PGX = Eigen::VectorXd::Zero(nlp->primal_vars_);
    Eigen::VectorXd AGX = Eigen::VectorXd::Zero(nlp->primal_vars_);
    Eigen::VectorXd FXE = Eigen::VectorXd::Zero(nlp->equal_cons_);
    Eigen::VectorXd FXI = Eigen::VectorXd::Zero(nlp->inequal_cons_);
    Eigen::VectorXd LE(0), LI = Eigen::VectorXd::Zero(nlp->inequal_cons_);
    ASSERT_NO_THROW(nlp->eval_kkt(1.0, x, LE, LI, val, PGX, AGX, FXE, FXI, kkt));
    fresh->drifted_ = true;
    EXPECT_THROW(nlp->eval_kkt(1.0, x2, LE, LI, val, PGX, AGX, FXE, FXI, kkt),
                 std::invalid_argument);
}

TEST(NlpProblemModelTest, VariableBoundsAreValidatedAtConstruction) {
    // All three refusals belong here rather than only in a consumer: every
    // consumer reads the bounds through this model, and start_point() projects
    // onto them.
    auto bad = [](auto mutate) {
        auto problem = std::make_shared<NpmLargeBoundProblem>();
        mutate(*problem);
        EXPECT_THROW(NlpProblemModel{problem}, std::invalid_argument);
    };
    bad([](NpmLargeBoundProblem &p) { p.xl_[0] = std::numeric_limits<double>::quiet_NaN(); });
    bad([](NpmLargeBoundProblem &p) { p.xu_[1] = std::numeric_limits<double>::quiet_NaN(); });
    bad([](NpmLargeBoundProblem &p) {
        p.xl_[0] = 1.0;
        p.xu_[0] = 0.0;
    });
    bad([](NpmLargeBoundProblem &p) {
        p.xl_[1] = kNpmInf;
        p.xu_[1] = kNpmInf;
    });
    bad([](NpmLargeBoundProblem &p) {
        p.xl_[1] = -kNpmInf;
        p.xu_[1] = -kNpmInf;
    });

    // The controls: a variable fixed at a finite value is the ordinary way to
    // fix one, and an ordinary free variable has two different infinities.
    auto fixed_finite = std::make_shared<NpmLargeBoundProblem>();
    fixed_finite->xl_[0] = 2.0;
    fixed_finite->xu_[0] = 2.0;
    EXPECT_NO_THROW(NlpProblemModel{fixed_finite});
    EXPECT_NO_THROW(NlpProblemModel{std::make_shared<NpmLargeBoundProblem>()});
}

namespace {

/// A bare NlpModel that can return an uncompressed matrix from whichever of
/// the three matrix entry points is selected. n = 2, one equality row, one
/// inequality row.
struct NpmUncompressedModel : hven::solvers::NlpModel {
    enum class Loose { None, JacE, JacI, Hess };
    mutable Loose loose_ = Loose::None;
    Eigen::VectorXd lower_{{-kNpmInf, -kNpmInf}};
    Eigen::VectorXd upper_{{kNpmInf, kNpmInf}};

    hven::Index n() const override { return 2; }
    hven::Index me() const override { return 1; }
    hven::Index mi() const override { return 1; }

    double eval_f(const Eigen::VectorXd &x) const override { return x.squaredNorm(); }
    Eigen::VectorXd eval_grad(const Eigen::VectorXd &x) const override { return 2.0 * x; }
    Eigen::VectorXd eval_ce(const Eigen::VectorXd &x) const override {
        Eigen::VectorXd ce(1);
        ce[0] = x[0] + x[1] - 1.0;
        return ce;
    }
    Eigen::VectorXd eval_ci(const Eigen::VectorXd &x) const override {
        Eigen::VectorXd ci(1);
        ci[0] = x[0] - x[1];
        return ci;
    }

    /// Two entries in row 0, left uncompressed when @p loose is selected.
    static hven::SpMatRM row_matrix(double a, double b, bool loose) {
        hven::SpMatRM m(1, 2);
        m.reserve(Eigen::VectorXi::Constant(1, 4)); // room to spare, so gaps remain
        m.insert(0, 0) = a;
        m.insert(0, 1) = b;
        if (!loose) {
            m.makeCompressed();
        }
        return m;
    }
    hven::SpMatRM eval_jac_e(const Eigen::VectorXd &) const override {
        return row_matrix(1.0, 1.0, loose_ == Loose::JacE);
    }
    hven::SpMatRM eval_jac_i(const Eigen::VectorXd &) const override {
        return row_matrix(1.0, -1.0, loose_ == Loose::JacI);
    }
    hven::SpMatRM eval_hess(const Eigen::VectorXd &, double obj_scale, const Eigen::VectorXd &,
                            const Eigen::VectorXd &) const override {
        hven::SpMatRM h(2, 2);
        h.reserve(Eigen::VectorXi::Constant(2, 4));
        h.insert(0, 0) = 2.0 * obj_scale;
        h.insert(1, 1) = 2.0 * obj_scale;
        if (loose_ != Loose::Hess) {
            h.makeCompressed();
        }
        return h;
    }
    const Eigen::VectorXd &lower() const override { return lower_; }
    const Eigen::VectorXd &upper() const override { return upper_; }
    Eigen::VectorXd start_point() const override { return Eigen::VectorXd::Zero(2); }
};

} // namespace

TEST(NlpAdapterHostTest, AnUncompressedMatrixReturnIsRefusedFromEachEntryPoint) {
    // Uncompressed storage names every claimed coordinate and still hands the
    // wrong value for each: the value array carries gaps, so pairing stored
    // value k with recorded coordinate k reads a different element. The guard
    // refuses it before any value is read.
    Eigen::VectorXd x(2), le(1), li(1);
    x << 0.4, 0.6;
    le << 0.0;
    li << 0.0;

    auto host_for = [](NpmUncompressedModel::Loose loose) {
        auto model = std::make_shared<NpmUncompressedModel>();
        model->loose_ = loose;
        return std::make_pair(
            model, std::make_shared<hven::solvers::NLPAdapterCore>(model, "NpmUncompressedModel"));
    };

    // The control: compressed throughout, nothing refused.
    {
        auto [model, core] = host_for(NpmUncompressedModel::Loose::None);
        EXPECT_NO_THROW(core->refresh_jacobians(x));
        EXPECT_NO_THROW(core->eval_hessian_values(x, 1.0, le, li));
    }

    // Each of the three entry points, one at a time, and at both moments the
    // host reads a matrix. Loose from the start: the claim walk refuses it, so
    // the host is never built over a pattern it could not have recorded
    // correctly. Loose only afterwards: the claim walk saw compressed storage,
    // and the evaluation is what refuses.
    for (auto loose : {NpmUncompressedModel::Loose::JacE, NpmUncompressedModel::Loose::JacI,
                       NpmUncompressedModel::Loose::Hess}) {
        auto from_the_start = std::make_shared<NpmUncompressedModel>();
        from_the_start->loose_ = loose;
        EXPECT_THROW(hven::solvers::NLPAdapterCore(from_the_start, "NpmUncompressedModel"),
                     std::invalid_argument);

        auto model = std::make_shared<NpmUncompressedModel>();
        std::shared_ptr<hven::solvers::NLPAdapterCore> core;
        ASSERT_NO_THROW(
            core = std::make_shared<hven::solvers::NLPAdapterCore>(model, "NpmUncompressedModel"));
        model->loose_ = loose;
        if (loose == NpmUncompressedModel::Loose::Hess) {
            EXPECT_THROW(core->eval_hessian_values(x, 1.0, le, li), std::invalid_argument);
        } else {
            EXPECT_THROW(core->refresh_jacobians(x), std::invalid_argument);
        }
    }
}

namespace {

/// The same model reached through the base class's DEFAULT in-place
/// implementations: it forwards only the by-value methods and overrides none
/// of the in-place ones, so every in-place call on it takes NlpModel's
/// delegation.
struct NpmByValueOnlyModel : hven::solvers::NlpModel {
    std::shared_ptr<NlpProblemModel> inner_;

    explicit NpmByValueOnlyModel(std::shared_ptr<NlpProblemModel> inner)
        : inner_(std::move(inner)) {}

    hven::Index n() const override { return inner_->n(); }
    hven::Index me() const override { return inner_->me(); }
    hven::Index mi() const override { return inner_->mi(); }
    double eval_f(const Eigen::VectorXd &x) const override { return inner_->eval_f(x); }
    Eigen::VectorXd eval_grad(const Eigen::VectorXd &x) const override {
        return inner_->eval_grad(x);
    }
    Eigen::VectorXd eval_ce(const Eigen::VectorXd &x) const override { return inner_->eval_ce(x); }
    Eigen::VectorXd eval_ci(const Eigen::VectorXd &x) const override { return inner_->eval_ci(x); }
    hven::SpMatRM eval_hess(const Eigen::VectorXd &x, double obj_scale, const Eigen::VectorXd &le,
                            const Eigen::VectorXd &li) const override {
        return inner_->eval_hess(x, obj_scale, le, li);
    }
    hven::SpMatRM eval_jac_e(const Eigen::VectorXd &x) const override {
        return inner_->eval_jac_e(x);
    }
    hven::SpMatRM eval_jac_i(const Eigen::VectorXd &x) const override {
        return inner_->eval_jac_i(x);
    }
    const Eigen::VectorXd &lower() const override { return inner_->lower(); }
    const Eigen::VectorXd &upper() const override { return inner_->upper(); }
    Eigen::VectorXd start_point() const override { return inner_->start_point(); }
};

/// Bit-for-bit, not near: the two paths must agree exactly.
void npm_expect_identical(const Eigen::VectorXd &a, const Eigen::VectorXd &b, const char *what) {
    ASSERT_EQ(a.size(), b.size()) << what;
    for (Eigen::Index k = 0; k < a.size(); k++) {
        EXPECT_EQ(a[k], b[k]) << what << " entry " << k;
    }
}

void npm_expect_identical(const hven::SpMatRM &a, const hven::SpMatRM &b, const char *what) {
    ASSERT_EQ(a.rows(), b.rows()) << what;
    ASSERT_EQ(a.cols(), b.cols()) << what;
    ASSERT_EQ(a.nonZeros(), b.nonZeros()) << what;
    ASSERT_EQ(a.isCompressed(), b.isCompressed()) << what;
    for (Eigen::Index k = 0; k <= a.outerSize(); k++) {
        EXPECT_EQ(a.outerIndexPtr()[k], b.outerIndexPtr()[k]) << what << " outer " << k;
    }
    for (Eigen::Index k = 0; k < a.nonZeros(); k++) {
        EXPECT_EQ(a.innerIndexPtr()[k], b.innerIndexPtr()[k]) << what << " inner " << k;
        EXPECT_EQ(a.valuePtr()[k], b.valuePtr()[k]) << what << " value " << k;
    }
}

} // namespace

// An override and the base class's default must be interchangeable. The
// default calls the by-value counterpart and assigns; an override fills the
// destination directly. Every consumer calls the in-place form
// unconditionally, so the two have to produce the same numbers -- exactly,
// not nearly.
TEST(NlpProblemModelTest, TheInPlaceOverrideAndTheDefaultDelegationAgreeExactly) {
    auto problem = std::make_shared<NpmKindsProblem>();
    auto overriding = std::make_shared<NlpProblemModel>(problem);
    NpmByValueOnlyModel defaulting(std::make_shared<NlpProblemModel>(problem));

    Eigen::VectorXd le(1), li(4);
    le << 0.5;
    li << 1.0, 2.0, 3.0, 4.0;

    Eigen::VectorXd grad_a, grad_b, ce_a, ce_b, ci_a, ci_b;
    hven::SpMatRM je_a, je_b, ji_a, ji_b, h_a, h_b;

    // Three iterates through one pair of destinations, so both the first call
    // (which lays the destination's pattern) and every steady-state call after
    // it are covered.
    for (double t : {0.0, 1.5, -2.25}) {
        Eigen::VectorXd x(2);
        x << t, 0.5 * t - 1.0;

        overriding->eval_grad_in_place(x, grad_a);
        defaulting.eval_grad_in_place(x, grad_b);
        npm_expect_identical(grad_a, grad_b, "eval_grad_in_place");

        overriding->eval_ce_in_place(x, ce_a);
        defaulting.eval_ce_in_place(x, ce_b);
        npm_expect_identical(ce_a, ce_b, "eval_ce_in_place");

        overriding->eval_ci_in_place(x, ci_a);
        defaulting.eval_ci_in_place(x, ci_b);
        npm_expect_identical(ci_a, ci_b, "eval_ci_in_place");

        overriding->eval_jac_e_in_place(x, je_a);
        defaulting.eval_jac_e_in_place(x, je_b);
        npm_expect_identical(je_a, je_b, "eval_jac_e_in_place");

        overriding->eval_jac_i_in_place(x, ji_a);
        defaulting.eval_jac_i_in_place(x, ji_b);
        npm_expect_identical(ji_a, ji_b, "eval_jac_i_in_place");

        overriding->eval_hess_in_place(x, 2.0, le, li, h_a);
        defaulting.eval_hess_in_place(x, 2.0, le, li, h_b);
        npm_expect_identical(h_a, h_b, "eval_hess_in_place");

        // ...and each agrees with its own by-value counterpart, which is what
        // the defaults are defined in terms of.
        npm_expect_identical(overriding->eval_grad(x), grad_a, "eval_grad");
        npm_expect_identical(overriding->eval_ce(x), ce_a, "eval_ce");
        npm_expect_identical(overriding->eval_ci(x), ci_a, "eval_ci");
        npm_expect_identical(overriding->eval_jac_e(x), je_a, "eval_jac_e");
        npm_expect_identical(overriding->eval_jac_i(x), ji_a, "eval_jac_i");
        npm_expect_identical(overriding->eval_hess(x, 2.0, le, li), h_a, "eval_hess");
    }
}

namespace {

/// Counts every evaluation the host asks of it. Implements only the by-value
/// methods, so the in-place calls the host makes arrive here through
/// NlpModel's defaults and one host call is one count. n = 3, one equality
/// row, one inequality row.
struct NpmCountingModel : hven::solvers::NlpModel {
    mutable int n_f_ = 0, n_grad_ = 0, n_ce_ = 0, n_ci_ = 0;
    mutable int n_jac_e_ = 0, n_jac_i_ = 0, n_hess_ = 0, n_start_point_ = 0;
    Eigen::VectorXd lower_{{-kNpmInf, -kNpmInf, -kNpmInf}};
    Eigen::VectorXd upper_{{kNpmInf, kNpmInf, kNpmInf}};

    void reset() {
        n_f_ = n_grad_ = n_ce_ = n_ci_ = 0;
        n_jac_e_ = n_jac_i_ = n_hess_ = n_start_point_ = 0;
    }

    hven::Index n() const override { return 3; }
    hven::Index me() const override { return 1; }
    hven::Index mi() const override { return 1; }

    double eval_f(const Eigen::VectorXd &x) const override {
        n_f_++;
        return x.squaredNorm();
    }
    Eigen::VectorXd eval_grad(const Eigen::VectorXd &x) const override {
        n_grad_++;
        return 2.0 * x;
    }
    Eigen::VectorXd eval_ce(const Eigen::VectorXd &x) const override {
        n_ce_++;
        Eigen::VectorXd ce(1);
        ce[0] = x[0] + x[1] - 1.0;
        return ce;
    }
    Eigen::VectorXd eval_ci(const Eigen::VectorXd &x) const override {
        n_ci_++;
        Eigen::VectorXd ci(1);
        ci[0] = x[2] - 2.0;
        return ci;
    }
    hven::SpMatRM eval_jac_e(const Eigen::VectorXd &) const override {
        n_jac_e_++;
        hven::SpMatRM j(1, 3);
        j.insert(0, 0) = 1.0;
        j.insert(0, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    hven::SpMatRM eval_jac_i(const Eigen::VectorXd &) const override {
        n_jac_i_++;
        hven::SpMatRM j(1, 3);
        j.insert(0, 2) = 1.0;
        j.makeCompressed();
        return j;
    }
    hven::SpMatRM eval_hess(const Eigen::VectorXd &, double obj_scale, const Eigen::VectorXd &,
                            const Eigen::VectorXd &) const override {
        n_hess_++;
        hven::SpMatRM h(3, 3);
        for (int i = 0; i < 3; i++) {
            h.insert(i, i) = 2.0 * obj_scale;
        }
        h.makeCompressed();
        return h;
    }
    const Eigen::VectorXd &lower() const override { return lower_; }
    const Eigen::VectorXd &upper() const override { return upper_; }
    Eigen::VectorXd start_point() const override {
        n_start_point_++;
        return Eigen::VectorXd::Zero(3);
    }
};

} // namespace

// Transcription's exact evaluation bill, once and for all: one start_point,
// one eval_hess, one eval_jac_e, one eval_jac_i, and nothing else -- no
// eval_f, no eval_grad, no eval_ce, no eval_ci. The three derivative calls are
// what the sparsity patterns are read from; the value-only methods have
// nothing setup needs.
TEST(NlpAdapterHostTest, TranscriptionSpendsOneStartPointAndThreeDerivativeCalls) {
    auto model = std::make_shared<NpmCountingModel>();
    hven::solvers::NLPAdapterCore core(model, "NpmCountingModel");

    EXPECT_EQ(model->n_start_point_, 1);
    EXPECT_EQ(model->n_hess_, 1);
    EXPECT_EQ(model->n_jac_e_, 1);
    EXPECT_EQ(model->n_jac_i_, 1);
    EXPECT_EQ(model->n_f_, 0);
    EXPECT_EQ(model->n_grad_, 0);
    EXPECT_EQ(model->n_ce_, 0);
    EXPECT_EQ(model->n_ci_, 0);

    // ...and a later evaluation adds no setup work of its own: it spends one
    // call of each kind it needs and never asks for the start point again.
    model->reset();
    Eigen::VectorXd x(3), le(1), li(1);
    x << 0.25, -0.5, 1.75;
    le << 0.3;
    li << 0.7;
    core.refresh_gradient(x);
    core.refresh_residuals(x);
    core.refresh_jacobians(x);
    core.eval_hessian_values(x, 1.0, le, li);
    EXPECT_EQ(model->n_start_point_, 0);
    EXPECT_EQ(model->n_grad_, 1);
    EXPECT_EQ(model->n_ce_, 1);
    EXPECT_EQ(model->n_ci_, 1);
    EXPECT_EQ(model->n_jac_e_, 1);
    EXPECT_EQ(model->n_jac_i_, 1);
    EXPECT_EQ(model->n_hess_, 1);
}

namespace {

/// A bare NlpModel whose gradient is one entry longer than it declares.
/// Everything else is well formed, so it survives the host's claim pass and
/// the refusal has to come from the evaluation.
struct NpmWrongGradientModel : hven::solvers::NlpModel {
    Eigen::VectorXd lower_{{-kNpmInf, -kNpmInf}};
    Eigen::VectorXd upper_{{kNpmInf, kNpmInf}};

    hven::Index n() const override { return 2; }
    hven::Index me() const override { return 0; }
    hven::Index mi() const override { return 0; }

    double eval_f(const Eigen::VectorXd &x) const override { return x.squaredNorm(); }
    Eigen::VectorXd eval_grad(const Eigen::VectorXd &) const override {
        return Eigen::VectorXd::Zero(3); // one too many
    }
    Eigen::VectorXd eval_ce(const Eigen::VectorXd &) const override {
        return Eigen::VectorXd::Zero(0);
    }
    Eigen::VectorXd eval_ci(const Eigen::VectorXd &) const override {
        return Eigen::VectorXd::Zero(0);
    }
    hven::SpMatRM eval_jac_e(const Eigen::VectorXd &) const override { return hven::SpMatRM(0, 2); }
    hven::SpMatRM eval_jac_i(const Eigen::VectorXd &) const override { return hven::SpMatRM(0, 2); }
    hven::SpMatRM eval_hess(const Eigen::VectorXd &, double obj_scale, const Eigen::VectorXd &,
                            const Eigen::VectorXd &) const override {
        hven::SpMatRM h(2, 2);
        h.insert(0, 0) = 2.0 * obj_scale;
        h.insert(1, 1) = 2.0 * obj_scale;
        h.makeCompressed();
        return h;
    }
    const Eigen::VectorXd &lower() const override { return lower_; }
    const Eigen::VectorXd &upper() const override { return upper_; }
    Eigen::VectorXd start_point() const override { return Eigen::VectorXd::Zero(2); }
};

} // namespace

TEST(NlpAdapterHostTest, AWrongLengthGradientIsRefusedAndNamesBothCounts) {
    // The gradient lands in a fixed segment of the solver's arena, so a return
    // of any other length would be written past the rows laid for it. Every
    // other by-value result on this host is length-checked; this one is too.
    auto model = std::make_shared<NpmWrongGradientModel>();
    hven::solvers::NLPAdapterCore core(model, "NpmWrongGradientModel");

    Eigen::VectorXd x(2);
    x << 0.5, -0.25;
    try {
        core.refresh_gradient(x);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        const std::string message(e.what());
        EXPECT_NE(message.find("eval_grad"), std::string::npos) << message;
        EXPECT_NE(message.find("3"), std::string::npos) << message; // what came back
        EXPECT_NE(message.find("2"), std::string::npos) << message; // what n() declares
        EXPECT_NE(message.find("NpmWrongGradientModel"), std::string::npos) << message;
    }
}

TEST(NlpAdapterHostTest, AShortEqualityMultiplierBlockIsRefusedBeforeItIsSliced) {
    // The engine's block may be longer than this host's rows -- its own
    // appended rows sit after them, so the host takes the head. Shorter has no
    // head to take, and the slice would read past the end, which under NDEBUG
    // is silent. The refusal comes before the slice.
    auto model = std::make_shared<NpmCountingModel>(); // one equality row
    hven::solvers::NLPAdapterCore core(model, "NpmCountingModel");
    ASSERT_EQ(core.num_eq_, 1);

    Eigen::VectorXd empty(0);
    try {
        core.record_equality_multipliers(empty);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        const std::string message(e.what());
        EXPECT_NE(message.find("0 equality multipliers"), std::string::npos) << message;
        EXPECT_NE(message.find("hosts 1 equality rows"), std::string::npos) << message;
        EXPECT_NE(message.find("NpmCountingModel"), std::string::npos) << message;
        // Named site: this is the record's own refusal, not the piece's first
        // read of the same block.
        EXPECT_NE(message.find("at the record"), std::string::npos) << message;
    }
    EXPECT_FALSE(core.le_recorded_);

    // Exactly as long as, and longer than, this host's rows are both accepted,
    // and the head is what lands in the record.
    Eigen::VectorXd exact(1), longer(3);
    exact << 0.25;
    longer << 0.5, -7.0, 9.0;
    EXPECT_NO_THROW(core.record_equality_multipliers(exact));
    EXPECT_DOUBLE_EQ(core.le_record_[0], 0.25);
    EXPECT_NO_THROW(core.record_equality_multipliers(longer));
    EXPECT_DOUBLE_EQ(core.le_record_[0], 0.5);
    EXPECT_TRUE(core.le_recorded_);
}

TEST(NlpAdapterHostTest, AnEmptyMultiplierBlockIsNotReadAsAllZeroAtTheHessianOwner) {
    // One rule for every block this host reads: shorter than the rows it is
    // checked against is refused, and an empty block is not a shape that means
    // all-zero. The engine hands down full-length blocks on every path, so a
    // block that arrives empty where rows are hosted is a defect in the chain.
    auto model = std::make_shared<NpmCountingModel>(); // one equality, one inequality row
    hven::solvers::NLPAdapterCore core(model, "NpmCountingModel");
    ASSERT_EQ(core.num_eq_, 1);
    ASSERT_EQ(core.num_iq_, 1);

    Eigen::VectorXd x(3);
    x << 0.5, -0.25, 1.5;
    Eigen::VectorXd le(1), li(1);
    le << 0.75;
    li << -0.5;

    try {
        core.eval_hessian_values(x, 1.0, Eigen::VectorXd(), li);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        const std::string message(e.what());
        EXPECT_NE(message.find("0 equality multipliers"), std::string::npos) << message;
        EXPECT_NE(message.find("hosts 1 equality rows"), std::string::npos) << message;
        EXPECT_NE(message.find("at the Hessian owner"), std::string::npos) << message;
    }
    try {
        core.eval_hessian_values(x, 1.0, le, Eigen::VectorXd());
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        const std::string message(e.what());
        EXPECT_NE(message.find("0 inequality multipliers"), std::string::npos) << message;
        EXPECT_NE(message.find("hosts 1 inequality rows"), std::string::npos) << message;
        EXPECT_NE(message.find("at the Hessian owner"), std::string::npos) << message;
    }

    // Full-length blocks, and blocks longer than this host's rows, both stand:
    // the host takes the head.
    EXPECT_NO_THROW(core.eval_hessian_values(x, 1.0, le, li));
    Eigen::VectorXd longer_e(3), longer_i(2);
    longer_e << 0.75, 4.0, -4.0;
    longer_i << -0.5, 8.0;
    EXPECT_NO_THROW(core.eval_hessian_values(x, 1.0, longer_e, longer_i));
}
