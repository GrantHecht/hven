// tests/test_nlp_model.cpp — Task 3: NlpModel interface, derivative checker,
// and the first six Hock-Schittkowski problems.
//
// Two batteries:
//   NlpModelHs.*             -- per-problem dimension/consistency checks plus
//                                derivative checks (assert_gradient/
//                                assert_jacobians/assert_hessian) at the
//                                problem's start point AND at one interior
//                                random point, for every one of HS1, HS3,
//                                HS5, HS6, HS7, HS76.
//   DerivativeCheckSelfTest.* -- the checker's own mutation self-test: a
//                                deliberately-wrong-gradient model (built by
//                                wrapping a real HS model and mutating
//                                eval_grad via an inline lambda) must FAIL
//                                assert_gradient/assert_hessian, pinning that
//                                the checker actually checks rather than
//                                vacuously passing.

#include <cmath>
#include <memory>
#include <random>

#include <gtest/gtest.h>

#include <hven/model/nlp_model.h>

#include "support/derivative_check.h"
#include "support/hs_problems.h"

using hven::solvers::Index;
using hven::solvers::NlpModel;
using hven::solvers::SpMatU;
using hven::solvers::Vec;
using hven::solvers::test_support::assert_gradient;
using hven::solvers::test_support::assert_hessian;
using hven::solvers::test_support::assert_jacobians;
using hven::solvers::test_support::HsProblem;
using hven::solvers::test_support::make_hs;

namespace {

constexpr double kTol = 1e-6;

// Perturbs the problem's start point by a uniform offset in [-0.3, 0.3] per
// coordinate to get "one interior random point" per the brief. 0.3 is safe
// (stays strictly inside every finite bound with margin to spare) for all
// six problems Task 3 shipped: HS1/HS3's only finite bound sits >= 1.0 away
// from the perturbed coordinate's start value, HS5's box is [-1.5,4] x
// [-3,3] around a start point of (0,0), and HS76's x >= 0 bound sits 0.2 away
// from the perturbed range [0.2, 0.8] (start 0.5 per coordinate). HS6/HS7
// have no finite bounds at all.
//
// TASK 11: STAYING INSIDE THE BOX IS NOT THE REQUIREMENT, and three of the
// twenty-one problems added in that task leave it -- HS24 (x >= 0, start
// (1, 0.5), so x2 can perturb to -0.2), HS33 (x >= 0, start (0, 0, 3)) and
// HS45 (start (2,2,2,2,2) is ALREADY outside x1 <= 1). That is harmless
// here: a derivative check is a statement about the ANALYTIC formulas versus
// the model's own f/c, which every model above evaluates on all of R^n, and
// none of them is a partial function of x within a 0.3-ball of its start
// point. What DOES matter is the DOMAIN of the transcendental problems, and
// there the margin is real and was checked: HS25's (u_i - x2)^x3 needs
// u_i > x2, and min_i u_i = 25.63 against x2 = 12.5 +/- 0.3, a margin of
// 12.8; its x1 divisor is 100 +/- 0.3. HS77/HS79's sin/cos and all the
// integer powers are entire.
Vec perturbed_point(const NlpModel &model, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> unit(-1.0, 1.0);
    Vec x = model.start_point();
    for (Index i = 0; i < x.size(); ++i) {
        x(i) += 0.3 * unit(rng);
    }
    return x;
}

void CheckHsDimensions(int number) {
    SCOPED_TRACE(::testing::Message() << "HS" << number);
    HsProblem hs = make_hs(number);
    const NlpModel &model = *hs.model;
    const Index n = model.n();
    const Index me = model.me();
    const Index mi = model.mi();

    ASSERT_GT(n, 0);
    ASSERT_GE(me, 0);
    ASSERT_GE(mi, 0);
    EXPECT_EQ(model.start_point().size(), n);
    EXPECT_EQ(model.lower().size(), n);
    EXPECT_EQ(model.upper().size(), n);
    for (Index i = 0; i < n; ++i) {
        EXPECT_LE(model.lower()(i), model.upper()(i)) << "index " << i;
    }

    const Vec x = model.start_point();
    EXPECT_EQ(model.eval_grad(x).size(), n);
    EXPECT_EQ(model.eval_ce(x).size(), me);
    EXPECT_EQ(model.eval_ci(x).size(), mi);
    EXPECT_EQ(model.eval_jac_e(x).rows(), me);
    EXPECT_EQ(model.eval_jac_e(x).cols(), n);
    EXPECT_EQ(model.eval_jac_i(x).rows(), mi);
    EXPECT_EQ(model.eval_jac_i(x).cols(), n);

    const Vec lambda_e = Vec::Zero(me);
    const Vec lambda_i = Vec::Zero(mi);
    const SpMatU H = model.eval_hess(x, 1.0, lambda_e, lambda_i);
    EXPECT_EQ(H.rows(), n);
    EXPECT_EQ(H.cols(), n);
    // eval_hess must store only its upper triangle, same requirement as
    // qp_problem.h::H's validate().
    for (Index i = 0; i < H.outerSize(); ++i) {
        for (SpMatU::InnerIterator it(H, i); it; ++it) {
            EXPECT_LE(it.row(), it.col())
                << "eval_hess has a lower-triangle entry at (row=" << it.row()
                << ", col=" << it.col() << ")";
        }
    }

    EXPECT_TRUE(std::isfinite(hs.f_star));
    ASSERT_NE(hs.source, nullptr);
    EXPECT_GT(std::string(hs.source).size(), 0u);
}

void CheckHsDerivatives(int number, unsigned seed) {
    SCOPED_TRACE(::testing::Message() << "HS" << number);
    HsProblem hs = make_hs(number);
    const NlpModel &model = *hs.model;
    const Index me = model.me();
    const Index mi = model.mi();

    // Arbitrary nonzero multipliers so assert_hessian actually exercises the
    // lambda_e^T hess(cE) / lambda_i^T hess(cI) terms, not just hess(f).
    const Vec lambda_e = me > 0 ? Vec::Constant(me, 0.37) : Vec(0);
    const Vec lambda_i = mi > 0 ? Vec::Constant(mi, 0.61) : Vec(0);

    const Vec x_start = model.start_point();
    {
        SCOPED_TRACE("start point");
        EXPECT_TRUE(assert_gradient(model, x_start, kTol));
        EXPECT_TRUE(assert_jacobians(model, x_start, kTol));
        EXPECT_TRUE(assert_hessian(model, x_start, lambda_e, lambda_i, kTol));
    }

    const Vec x_interior = perturbed_point(model, seed);
    {
        SCOPED_TRACE("interior point");
        EXPECT_TRUE(assert_gradient(model, x_interior, kTol));
        EXPECT_TRUE(assert_jacobians(model, x_interior, kTol));
        EXPECT_TRUE(assert_hessian(model, x_interior, lambda_e, lambda_i, kTol));
    }
}

// Decorator that forwards every NlpModel method to an underlying model
// EXCEPT eval_grad, which is deliberately wrong: a uniform 10% overstatement
// applied via an inline lambda -- the mutation-style self-test for the
// checker itself. A multiplicative mutation (rather than an additive
// constant) is deliberate: assert_hessian finite-differences the GRADIENT of
// the Lagrangian, and an additive-constant error in eval_grad has zero
// derivative (it would cancel out of the central difference entirely,
// silently passing assert_hessian even though eval_grad is wrong) -- only a
// mutation that varies with x actually perturbs the finite-difference
// Hessian the way a genuinely wrong analytic gradient would.
class WrongGradientModel : public NlpModel {
  public:
    explicit WrongGradientModel(const NlpModel &base) : base_(base) {}

    Index n() const override { return base_.n(); }
    Index me() const override { return base_.me(); }
    Index mi() const override { return base_.mi(); }

    double eval_f(const Vec &x) const override { return base_.eval_f(x); }

    Vec eval_grad(const Vec &x) const override {
        auto mutate_wrong = [](const Vec &g) {
            Vec out = g;
            out.array() *= 1.1; // deliberately wrong, on purpose
            return out;
        };
        return mutate_wrong(base_.eval_grad(x));
    }

    Vec eval_ce(const Vec &x) const override { return base_.eval_ce(x); }
    Vec eval_ci(const Vec &x) const override { return base_.eval_ci(x); }

    SpMatU eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                     const Vec &lambda_i) const override {
        return base_.eval_hess(x, obj_scale, lambda_e, lambda_i);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        return base_.eval_jac_e(x);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        return base_.eval_jac_i(x);
    }

    const Vec &lower() const override { return base_.lower(); }
    const Vec &upper() const override { return base_.upper(); }
    Vec start_point() const override { return base_.start_point(); }

  private:
    const NlpModel &base_;
};

// TASK 8 (the eval-economics carry): a decorator that OVERRIDES eval_values
// with a value deliberately DIFFERENT from what eval_f/eval_ce/eval_ci would
// return, and does nothing else -- every other method (including eval_f/
// eval_ce/eval_ci themselves) forwards verbatim to the base. This is the
// dispatch check nlp_model.h's own default-impl note asks for: a caller that
// reads NlpModel::eval_values must see THIS override's numbers, never
// silently fall back to calling eval_f/eval_ce/eval_ci behind its back.
class WrongValuesModel : public NlpModel {
  public:
    explicit WrongValuesModel(const NlpModel &base) : base_(base) {}

    Index n() const override { return base_.n(); }
    Index me() const override { return base_.me(); }
    Index mi() const override { return base_.mi(); }

    double eval_f(const Vec &x) const override { return base_.eval_f(x); }
    Vec eval_grad(const Vec &x) const override { return base_.eval_grad(x); }
    Vec eval_ce(const Vec &x) const override { return base_.eval_ce(x); }
    Vec eval_ci(const Vec &x) const override { return base_.eval_ci(x); }
    SpMatU eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                     const Vec &lambda_i) const override {
        return base_.eval_hess(x, obj_scale, lambda_e, lambda_i);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        return base_.eval_jac_e(x);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        return base_.eval_jac_i(x);
    }
    const Vec &lower() const override { return base_.lower(); }
    const Vec &upper() const override { return base_.upper(); }
    Vec start_point() const override { return base_.start_point(); }

    // THE OVERRIDE: a uniform +1000 offset on f, and on every entry of cE/cI
    // -- large enough that it can never be mistaken for roundoff, and
    // structured so a bug that ignores mi()/me() (e.g. always sizing cE/cI
    // to n()) is also visible as a size mismatch rather than silently
    // comparing empty-to-empty.
    void eval_values(const Vec &x, double &f, Vec &cE, Vec &cI) const override {
        f = base_.eval_f(x) + 1000.0;
        cE = base_.me() > 0 ? Vec(base_.eval_ce(x).array() + 1000.0) : Vec(0);
        cI = base_.mi() > 0 ? Vec(base_.eval_ci(x).array() + 1000.0) : Vec(0);
    }

  private:
    const NlpModel &base_;
};

} // namespace

TEST(NlpModelHs, Hs1DimensionsAndDerivatives) {
    CheckHsDimensions(1);
    CheckHsDerivatives(1, 101);
}
TEST(NlpModelHs, Hs3DimensionsAndDerivatives) {
    CheckHsDimensions(3);
    CheckHsDerivatives(3, 103);
}
TEST(NlpModelHs, Hs5DimensionsAndDerivatives) {
    CheckHsDimensions(5);
    CheckHsDerivatives(5, 105);
}
TEST(NlpModelHs, Hs6DimensionsAndDerivatives) {
    CheckHsDimensions(6);
    CheckHsDerivatives(6, 106);
}
TEST(NlpModelHs, Hs7DimensionsAndDerivatives) {
    CheckHsDimensions(7);
    CheckHsDerivatives(7, 107);
}
TEST(NlpModelHs, Hs76DimensionsAndDerivatives) {
    CheckHsDimensions(76);
    CheckHsDerivatives(76, 176);
}

// --- Task 11: the twenty-one problems added for the HS battery. ---
//
// STEP 1 OF THE BATTERY'S BRIEF: each of these must pass the derivative
// checker BEFORE it is allowed into tests/test_hs_battery.cpp. They are
// spelled out one TEST per problem rather than looped, so a transcription
// error names the problem in the gtest failure line instead of stopping a
// loop at the first bad one and hiding the rest.
//
// THE SEED IS THE PROBLEM NUMBER + 100, arbitrarily but reproducibly, exactly
// as the Task-3 tests above do.
TEST(NlpModelHs, Hs10DimensionsAndDerivatives) {
    CheckHsDimensions(10);
    CheckHsDerivatives(10, 110);
}
TEST(NlpModelHs, Hs11DimensionsAndDerivatives) {
    CheckHsDimensions(11);
    CheckHsDerivatives(11, 111);
}
TEST(NlpModelHs, Hs12DimensionsAndDerivatives) {
    CheckHsDimensions(12);
    CheckHsDerivatives(12, 112);
}
TEST(NlpModelHs, Hs14DimensionsAndDerivatives) {
    CheckHsDimensions(14);
    CheckHsDerivatives(14, 114);
}
TEST(NlpModelHs, Hs15DimensionsAndDerivatives) {
    CheckHsDimensions(15);
    CheckHsDerivatives(15, 115);
}
TEST(NlpModelHs, Hs22DimensionsAndDerivatives) {
    CheckHsDimensions(22);
    CheckHsDerivatives(22, 122);
}
TEST(NlpModelHs, Hs24DimensionsAndDerivatives) {
    CheckHsDimensions(24);
    CheckHsDerivatives(24, 124);
}
TEST(NlpModelHs, Hs25DimensionsAndDerivatives) {
    CheckHsDimensions(25);
    CheckHsDerivatives(25, 125);
}
TEST(NlpModelHs, Hs26DimensionsAndDerivatives) {
    CheckHsDimensions(26);
    CheckHsDerivatives(26, 126);
}
TEST(NlpModelHs, Hs27DimensionsAndDerivatives) {
    CheckHsDimensions(27);
    CheckHsDerivatives(27, 127);
}
TEST(NlpModelHs, Hs28DimensionsAndDerivatives) {
    CheckHsDimensions(28);
    CheckHsDerivatives(28, 128);
}
TEST(NlpModelHs, Hs30DimensionsAndDerivatives) {
    CheckHsDimensions(30);
    CheckHsDerivatives(30, 130);
}
TEST(NlpModelHs, Hs33DimensionsAndDerivatives) {
    CheckHsDimensions(33);
    CheckHsDerivatives(33, 133);
}
TEST(NlpModelHs, Hs35DimensionsAndDerivatives) {
    CheckHsDimensions(35);
    CheckHsDerivatives(35, 135);
}
TEST(NlpModelHs, Hs38DimensionsAndDerivatives) {
    CheckHsDimensions(38);
    CheckHsDerivatives(38, 138);
}
TEST(NlpModelHs, Hs39DimensionsAndDerivatives) {
    CheckHsDimensions(39);
    CheckHsDerivatives(39, 139);
}
TEST(NlpModelHs, Hs40DimensionsAndDerivatives) {
    CheckHsDimensions(40);
    CheckHsDerivatives(40, 140);
}
TEST(NlpModelHs, Hs43DimensionsAndDerivatives) {
    CheckHsDimensions(43);
    CheckHsDerivatives(43, 143);
}
TEST(NlpModelHs, Hs45DimensionsAndDerivatives) {
    CheckHsDimensions(45);
    CheckHsDerivatives(45, 145);
}
TEST(NlpModelHs, Hs77DimensionsAndDerivatives) {
    CheckHsDimensions(77);
    CheckHsDerivatives(77, 177);
}
TEST(NlpModelHs, Hs79DimensionsAndDerivatives) {
    CheckHsDimensions(79);
    CheckHsDerivatives(79, 179);
}

// hs_numbers() is what the battery iterates, so it must agree with make_hs in
// BOTH directions or a problem can be shipped and never exercised (or listed
// and crash the battery). Checked here rather than in the battery so the
// failure names the list, not a solve.
TEST(NlpModelHs, EveryListedNumberIsConstructible) {
    ASSERT_EQ(hven::solvers::test_support::hs_numbers().size(), 27u);
    for (int number : hven::solvers::test_support::hs_numbers()) {
        SCOPED_TRACE(::testing::Message() << "HS" << number);
        HsProblem hs = make_hs(number);
        EXPECT_NE(hs.model, nullptr);
    }
}

TEST(NlpModelHs, MakeHsRejectsUnshippedNumber) {
    EXPECT_THROW(make_hs(2), std::invalid_argument);
    EXPECT_THROW(make_hs(4), std::invalid_argument);
}

// THE RELATIVE-TOLERANCE FIX (Task 11, carry item 1) IS PINNED BY THE PROBLEM
// THAT FORCED IT. HS38's start point has |hess L|inf ~ 1.1e4, which puts the
// central-difference roundoff floor above an ABSOLUTE 1e-6; under Task 3's
// absolute comparison this check failed with |diff| ~ 2.6e-6 against
// tol = 1e-6 and no error in the analytic Hessian at all. The assertion below
// is that the relative bound accepts it, and the companion assertion is that
// the bound is not simply loose: a 1e-14 tolerance (which no magnitude of
// entry can rescue, since it is still relative) must REJECT the same call.
TEST(DerivativeCheckSelfTest, RelativeToleranceAdmitsLargeMagnitudeDerivatives) {
    HsProblem hs = make_hs(38);
    const Vec x = hs.model->start_point();
    const Vec none(0);
    EXPECT_GT(hs.model->eval_grad(x).lpNorm<Eigen::Infinity>(), 1e4)
        << "fixture must actually have large derivatives or it proves nothing";
    EXPECT_TRUE(assert_hessian(*hs.model, x, none, none, kTol));
    EXPECT_FALSE(assert_hessian(*hs.model, x, none, none, 1e-14))
        << "the relative bound must still be a bound";
}

TEST(DerivativeCheckSelfTest, CorrectModelPassesAssertGradient) {
    HsProblem hs = make_hs(1);
    EXPECT_TRUE(assert_gradient(*hs.model, hs.model->start_point(), kTol));
}

TEST(DerivativeCheckSelfTest, WrongGradientFailsAssertGradient) {
    HsProblem hs = make_hs(1);
    WrongGradientModel wrong(*hs.model);
    EXPECT_FALSE(assert_gradient(wrong, hs.model->start_point(), kTol));
}

TEST(DerivativeCheckSelfTest, WrongGradientFailsAssertHessian) {
    // HS6 has an equality constraint, so a nonzero lambda_e exercises the
    // Je^T lambda_e term of the Lagrangian gradient this check finite-
    // differences -- not just eval_grad in isolation.
    HsProblem hs = make_hs(6);
    WrongGradientModel wrong(*hs.model);
    const Vec lambda_e = Vec::Constant(1, 0.5);
    const Vec lambda_i(0);
    EXPECT_FALSE(assert_hessian(wrong, hs.model->start_point(), lambda_e, lambda_i, kTol));
}

// =========================================================================
// NlpModelEvalValues -- Phase-5 Task 8: NlpModel::eval_values, the
// eval-economics carry's model-side half.
// =========================================================================

// THE DEFAULT-IMPL CONTRACT: on any model that does not override
// eval_values (every HS problem in this file's battery), it must return
// EXACTLY what eval_f/eval_ce/eval_ci would have, at both an interior point
// and the published start point, across me()==0/mi()==0 and me()>0/mi()>0
// shapes alike (HS1: 0/0, HS76: 0/3, HS40: 3/0) -- nlp_model.h's own note
// on this ("bit-identical to calling eval_f(x)/eval_ce(x)/eval_ci(x)
// directly").
void CheckEvalValuesMatchesDefaultImpl(int number, unsigned seed) {
    SCOPED_TRACE(::testing::Message() << "HS" << number);
    HsProblem hs = make_hs(number);
    const NlpModel &model = *hs.model;

    for (const Vec &x : {model.start_point(), perturbed_point(model, seed)}) {
        double f = 0.0;
        Vec cE, cI;
        model.eval_values(x, f, cE, cI);

        EXPECT_EQ(f, model.eval_f(x));
        ASSERT_EQ(cE.size(), model.me());
        ASSERT_EQ(cI.size(), model.mi());
        if (model.me() > 0) {
            EXPECT_EQ(cE, model.eval_ce(x));
        }
        if (model.mi() > 0) {
            EXPECT_EQ(cI, model.eval_ci(x));
        }
    }
}

TEST(NlpModelEvalValues, DefaultMatchesEvalFCeCiOnNoConstraints) {
    CheckEvalValuesMatchesDefaultImpl(1, 201); // HS1: me == 0, mi == 0
}
TEST(NlpModelEvalValues, DefaultMatchesEvalFCeCiOnInequalitiesOnly) {
    CheckEvalValuesMatchesDefaultImpl(76, 276); // HS76: me == 0, mi == 3
}
TEST(NlpModelEvalValues, DefaultMatchesEvalFCeCiOnEqualitiesOnly) {
    CheckEvalValuesMatchesDefaultImpl(40, 240); // HS40: me == 3, mi == 0
}

// THE OVERRIDE ACTUALLY DISPATCHES. WrongValuesModel overrides ONLY
// eval_values, to a value 1000 away from eval_f/eval_ce/eval_ci's own --
// so a caller reading NlpModel::eval_values on it must see the WRONG
// (overridden) number, not silently fall back to calling eval_f/eval_ce/
// eval_ci behind the interface. This is the mechanism the whole task rests
// on: sqp_driver.h's three call sites call model.eval_values(...) through
// a `const NlpModel &`, so if virtual dispatch here were somehow bypassed
// (a non-virtual eval_values, or a caller that memoized the base class's
// answer) every override in the tree -- including F7's -- would be dead
// code no test would catch without this dispatch check existing.
TEST(NlpModelEvalValues, OverrideActuallyDispatches) {
    HsProblem hs = make_hs(76); // me == 0, mi == 3: exercises the cI path too
    WrongValuesModel wrong(*hs.model);
    const Vec x = hs.model->start_point();

    double f = 0.0;
    Vec cE, cI;
    wrong.eval_values(x, f, cE, cI);

    EXPECT_NE(f, hs.model->eval_f(x));
    EXPECT_NEAR(f, hs.model->eval_f(x) + 1000.0, 1e-12);
    ASSERT_EQ(cI.size(), hs.model->mi());
    const Vec base_ci = hs.model->eval_ci(x);
    for (Index j = 0; j < cI.size(); ++j) {
        EXPECT_NEAR(cI(j), base_ci(j) + 1000.0, 1e-12) << "row " << j;
    }
}

// eval_values IS CALLABLE THROUGH A BASE-CLASS REFERENCE -- the exact shape
// every sqp_driver.h call site uses (const NlpModel&, never the concrete
// type) -- so a model whose override is only reachable through its own
// concrete type would still be a silent no-op from the driver's point of
// view. WrongValuesModel's DEFAULT-IMPL SIBLING check (base_'s own model,
// through a `const NlpModel&` binding a Hs76Model) is CheckEvalValuesMatch-
// esDefaultImpl above; this test is the same shape for the OVERRIDING model.
TEST(NlpModelEvalValues, OverrideDispatchesThroughBaseClassReference) {
    HsProblem hs = make_hs(76);
    WrongValuesModel wrong_concrete(*hs.model);
    const NlpModel &wrong = wrong_concrete; // the driver's own calling shape

    double f = 0.0;
    Vec cE, cI;
    wrong.eval_values(wrong.start_point(), f, cE, cI);
    EXPECT_NEAR(f, hs.model->eval_f(hs.model->start_point()) + 1000.0, 1e-12);
}
