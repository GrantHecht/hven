// Task 4 (M4-Task5 plan): bench/model_surface_kkt.h's engine-independent KKT
// residual scorer.
//
// THE INCLUDE LIST BELOW IS ITSELF A CLAIM, per brief item (d): no driver/,
// qp/, or kkt/ header appears anywhere in this translation unit. Every fixture
// here is a plain NlpModel transcription (nlp_model.h), reached through the
// NlpModelAggregate bridge (nlp_model_aggregate.h) -- both model/ headers, the
// same standing bench/model_surface_kkt.h's own banner claims for itself. If a
// later edit needs a driver type to make one of these tests pass, that is a
// sign the scorer stopped being engine-independent, not a reason to add the
// include.
//
// Two fixtures, chosen so their KKT point can be verified by hand rather than
// by re-solving:
//
//   ToyModel             min 0.5(x0-3)^2 + 0.5(x1-4)^2
//                        s.t. x0 + x1 - 5 = 0 (active)
//                             x0 - 10 <= 0    (inactive)
//                             0 <= x0, x1 free
//                        KKT point x* = (2, 3), lambda_e = 1, lambda_i = 0,
//                        z = 0 -- solved in closed form: stationarity
//                        d/dx0 = (x0-3) + lambda_e, d/dx1 = (x1-4) + lambda_e,
//                        set both to zero with x0+x1=5.
//
//   ToyModelWithFixedVar the same problem plus a third variable x2 declared
//                        FIXED (lower == upper == 3, no degree of freedom),
//                        whose eval_grad row is a deliberate, unrelated
//                        constant -- proving the scorer's declared-fixed
//                        exclusion (nlp_aggregate.h's own "WHAT A SCORER
//                        OWES") rather than reading whatever a provider
//                        leaves there.

#include <limits>
#include <memory>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>

#include <gtest/gtest.h>

#include "hven/model/nlp_aggregate.h"
#include "hven/model/nlp_model.h"
#include "hven/model/nlp_model_aggregate.h"

#include "../../bench/model_surface_kkt.h"

using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using hven::solvers::model_surface_kkt_residuals;
using hven::solvers::ModelSurfaceKktResiduals;
using hven::solvers::NlpModel;
using hven::solvers::NlpModelAggregate;

namespace {

Eigen::SparseMatrix<double, Eigen::RowMajor>
make_row(Index rows, Index cols, const std::vector<Eigen::Triplet<double>> &triplets) {
    Eigen::SparseMatrix<double, Eigen::RowMajor> m(rows, cols);
    m.setFromTriplets(triplets.begin(), triplets.end());
    return m;
}

/// n = 2, me = 1, mi = 1, no fixed variables. See the file banner for the
/// closed-form KKT point.
class ToyModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override {
        return 0.5 * (x(0) - 3.0) * (x(0) - 3.0) + 0.5 * (x(1) - 4.0) * (x(1) - 4.0);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g(0) = x(0) - 3.0;
        g(1) = x(1) - 4.0;
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c(0) = x(0) + x(1) - 5.0;
        return c;
    }
    Vec eval_ci(const Vec &x) const override {
        Vec c(1);
        c(0) = x(0) - 10.0;
        return c;
    }
    SpMatRM eval_hess(const Vec &, double, const Vec &, const Vec &) const override {
        return SpMatRM(2, 2); // never claimed via kObjectiveHessian in this suite
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return make_row(1, 2, {{0, 0, 1.0}, {0, 1, 1.0}});
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return make_row(1, 2, {{0, 0, 1.0}});
    }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override {
        Vec x(2);
        x << 0.0, 0.0;
        return x;
    }

  private:
    Vec lower_ = (Vec(2) << 0.0, -std::numeric_limits<double>::infinity()).finished();
    Vec upper_ =
        (Vec(2) << std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity())
            .finished();
};

/// ToyModel's own problem, plus a third variable x2 declared FIXED at 3
/// (lower == upper == 3). eval_grad's row 2 is a deliberate constant, wildly
/// far from x2's true partial derivative -- the exclusion test's whole point
/// is that a correct scorer never reads it.
class ToyModelWithFixedVar : public NlpModel {
  public:
    static constexpr double kGarbageGradientRow = 1.0e9;

    Index n() const override { return 3; }
    Index me() const override { return 1; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override {
        return 0.5 * (x(0) - 3.0) * (x(0) - 3.0) + 0.5 * (x(1) - 4.0) * (x(1) - 4.0) +
               0.5 * (x(2) - 100.0) * (x(2) - 100.0);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(3);
        g(0) = x(0) - 3.0;
        g(1) = x(1) - 4.0;
        g(2) = kGarbageGradientRow; // NOT x(2) - 100.0, deliberately.
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c(0) = x(0) + x(1) - 5.0;
        return c;
    }
    Vec eval_ci(const Vec &x) const override {
        Vec c(1);
        c(0) = x(0) - 10.0;
        return c;
    }
    SpMatRM eval_hess(const Vec &, double, const Vec &, const Vec &) const override {
        return SpMatRM(3, 3);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return make_row(1, 3, {{0, 0, 1.0}, {0, 1, 1.0}});
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return make_row(1, 3, {{0, 0, 1.0}});
    }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override {
        Vec x(3);
        x << 0.0, 0.0, 3.0;
        return x;
    }

  private:
    Vec lower_ = (Vec(3) << 0.0, -std::numeric_limits<double>::infinity(), 3.0).finished();
    Vec upper_ = (Vec(3) << std::numeric_limits<double>::infinity(),
                  std::numeric_limits<double>::infinity(), 3.0)
                     .finished();
};

constexpr double kUlpTol = 1.0e-11;

} // namespace

// (a) A known KKT point: every residual reads near-ulp of zero.
TEST(ModelSurfaceKkt, KnownKktPointIsNearUlpZero) {
    NlpModelAggregate aggregate(std::make_shared<ToyModel>());

    Vec x(2);
    x << 2.0, 3.0;
    Vec lambda_e(1);
    lambda_e << 1.0;
    Vec lambda_i(1);
    lambda_i << 0.0;
    const Vec z = Vec::Zero(2);

    const ModelSurfaceKktResiduals r =
        model_surface_kkt_residuals(aggregate, x, lambda_e, lambda_i, z);

    EXPECT_NEAR(r.stationarity_, 0.0, kUlpTol);
    EXPECT_NEAR(r.complementarity_, 0.0, kUlpTol);
    EXPECT_NEAR(r.primal_, 0.0, kUlpTol);
    EXPECT_DOUBLE_EQ(r.dual_scale_, 1.0); // max(1, |lambda_e|inf=1, |lambda_i|inf=0, |z|inf=0)
    EXPECT_DOUBLE_EQ(r.x_scale_, 3.0);    // max(1, |x|inf) = max(1, 3) = 3
}

// (b) A declared-fixed variable's garbage gradient row must not corrupt
// stationarity: if the exclusion were missing this would read ~1e9, not ~0.
TEST(ModelSurfaceKkt, DeclaredFixedVariableGarbageRowIsExcluded) {
    NlpModelAggregate aggregate(std::make_shared<ToyModelWithFixedVar>());

    Vec x(3);
    x << 2.0, 3.0, 3.0;
    Vec lambda_e(1);
    lambda_e << 1.0;
    Vec lambda_i(1);
    lambda_i << 0.0;
    Vec z(3);
    z << 0.0, 0.0, 42.0; // garbage on the fixed coordinate too: its dist-to-bound
                         // is exactly 0, so complementarity must ignore it as well.

    const ModelSurfaceKktResiduals r =
        model_surface_kkt_residuals(aggregate, x, lambda_e, lambda_i, z);

    EXPECT_NEAR(r.stationarity_, 0.0, kUlpTol);
    EXPECT_NEAR(r.complementarity_, 0.0, kUlpTol);
    EXPECT_NEAR(r.primal_, 0.0, kUlpTol);
}

// (c) Falsification: perturbing the point off the KKT point moves the
// residuals away from zero.
TEST(ModelSurfaceKkt, PerturbedPointResidualsMove) {
    NlpModelAggregate aggregate(std::make_shared<ToyModel>());

    Vec x(2);
    x << 2.5, 3.0; // x0 perturbed off the KKT point; equality and stationarity
                   // both break.
    Vec lambda_e(1);
    lambda_e << 1.0;
    Vec lambda_i(1);
    lambda_i << 0.0;
    const Vec z = Vec::Zero(2);

    const ModelSurfaceKktResiduals r =
        model_surface_kkt_residuals(aggregate, x, lambda_e, lambda_i, z);

    EXPECT_GT(r.stationarity_, 1.0e-3);
    EXPECT_GT(r.primal_, 1.0e-3);
}

// (d) The scorer runs against a bare NlpModelAggregate -- this file's own
// include list is the engine-independence proof; this test is its functional
// half.
TEST(ModelSurfaceKkt, RunsAgainstBareNlpModelAggregateWithNoEngineInTheLink) {
    NlpModelAggregate aggregate(std::make_shared<ToyModel>());

    Vec x(2);
    x << 2.0, 3.0;
    Vec lambda_e(1);
    lambda_e << 1.0;
    Vec lambda_i(1);
    lambda_i << 0.0;
    const Vec z = Vec::Zero(2);

    const ModelSurfaceKktResiduals r =
        model_surface_kkt_residuals(aggregate, x, lambda_e, lambda_i, z);
    EXPECT_NEAR(r.stationarity_, 0.0, kUlpTol);
}

// z is not part of evaluate_candidate_first_order's own contract, so this
// scorer checks its length itself rather than silently reading past the end
// (T6: never fabricate, never truncate silently).
TEST(ModelSurfaceKkt, MismatchedZSizeThrows) {
    NlpModelAggregate aggregate(std::make_shared<ToyModel>());

    Vec x(2);
    x << 2.0, 3.0;
    Vec lambda_e(1);
    lambda_e << 1.0;
    Vec lambda_i(1);
    lambda_i << 0.0;
    Vec bad_z(1);
    bad_z << 0.0;

    EXPECT_THROW(model_surface_kkt_residuals(aggregate, x, lambda_e, lambda_i, bad_z),
                 std::invalid_argument);
}
