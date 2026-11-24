// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_function_handles.cpp — ConstraintFunction and ObjectiveFunction (model/constraint_function.h,
// model/objective_function.h): a piece bound to its index map, called through the five
// constraint shapes and the three objective shapes, and split across threads. The handles only
// forward, so each shape is checked against the closed-form value of a two-application piece:
// the residual slots, the adjoint-gradient slots (J^T lambda), the Jacobian and Hessian entries
// scattered into a KKT matrix through the claim locations, and the accumulate-versus-assign
// discipline of each destination.

#include <mutex>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Sparse>
#include <gtest/gtest.h>

#include <hven/model/constraint_function.h>
#include <hven/model/local_blocks.h>
#include <hven/model/objective_function.h>
#include <hven/model/piece_interfaces.h>

namespace hven {
namespace {

using Mi = Eigen::MatrixXi;
using Vi = Eigen::VectorXi;
using Vd = Eigen::VectorXd;
using Kkt = Eigen::SparseMatrix<double, Eigen::RowMajor>;

struct Product { // c = v0 * v1; H(1,0) = lambda
    static constexpr Index kInputs = 2, kOutputs = 1;
    std::string name() const { return "product"; }
    void value(ConstVecRef v, VecRef out) const { out[0] = v[0] * v[1]; }
    void jacobian(ConstVecRef v, MatRef J) const { J << v[1], v[0]; }
    void hessian(ConstVecRef, ConstVecRef lambda, MatRef H) const { H(1, 0) = lambda[0]; }
};

struct Bowl { // f = |v|^2
    static constexpr Index kInputs = 2;
    double value(ConstVecRef v) const { return v.squaredNorm(); }
    void gradient(ConstVecRef v, VecRef g) const { g = 2.0 * v; }
    void hessian(ConstVecRef, MatRef H) const {
        H(0, 0) = 2.0;
        H(1, 1) = 2.0;
    }
};

int position(const Kkt &m, int r, int c) {
    for (int p = m.outerIndexPtr()[r]; p < m.outerIndexPtr()[r + 1]; ++p) {
        if (m.innerIndexPtr()[p] == c) {
            return p;
        }
    }
    ADD_FAILURE() << "(" << r << ", " << c << ") is not in the pattern";
    return 0;
}

// The claim pass the layout runs, for one function: slot arrays plus a zeroed KKT matrix over
// the claimed pattern and the locations of every slot in its value array.
template <class F> struct Laid {
    Vi rows, cols, locations;
    Vi clashes;
    std::vector<std::mutex> locks;
    Kkt kkt;
    int slots = 0;

    Laid(F &f, int dim, int con_offset, bool dojac, bool dohess) {
        const int n = f.num_kkt_elements(dojac, dohess);
        rows = Vi::Constant(n, -9);
        cols = Vi::Constant(n, -9);
        int free = 0;
        f.get_kkt_space(rows, cols, free, con_offset, dojac, dohess);
        slots = free;
        std::vector<Eigen::Triplet<double>> t;
        for (int s = 0; s < slots; ++s) {
            if (rows[s] >= 0) {
                t.emplace_back(rows[s], cols[s], 0.0);
            }
        }
        kkt = Kkt(dim, dim);
        kkt.setFromTriplets(t.begin(), t.end());
        kkt.makeCompressed();
        locations = Vi::Zero(slots);
        for (int s = 0; s < slots; ++s) {
            locations[s] = rows[s] >= 0 ? position(kkt, rows[s], cols[s]) : 0;
        }
        clashes = Vi::Constant(dim, -1);
    }
};

// Two applications of Product over three variables: v = (x0, x1) and (x1, x2), rows 0 and 1.
ConstraintFunction product_function() {
    return ConstraintFunction(ConstraintInterface(Product{}),
                              (Mi(2, 2) << 0, 1, 1, 2).finished(), (Mi(1, 2) << 0, 1).finished());
}

constexpr int kDim = 5; // three variables, two equality rows
constexpr int kConOffset = 3;

TEST(ConstraintFunctionHandle, ReportsTheCountsTheIndexMapDerives) {
    ConstraintFunction f = product_function();
    EXPECT_EQ(f.function.name(), "product");
    EXPECT_EQ(f.function.input_rows(), 2);
    EXPECT_EQ(f.function.output_rows(), 1);
    EXPECT_TRUE(f.function.thread_safe());
    EXPECT_EQ(f.num_con_eles(), 2);
    EXPECT_EQ(f.num_grad_eles(), 4);
    EXPECT_EQ(f.num_kkt_elements(true, true), 10);
    EXPECT_EQ(f.num_kkt_elements(true, false), 4);
    EXPECT_EQ(f.index_data.num_appl(), 2);
}

TEST(ConstraintFunctionHandle, EveryShapeWritesItsDestinationsFromTheClosedForm) {
    ConstraintFunction f = product_function();
    Vi fxrows = Vi::Zero(2), gxrows = Vi::Zero(4);
    int free = 0;
    f.get_constraint_space(fxrows, free);
    EXPECT_EQ(fxrows, (Vi(2) << 0, 1).finished());
    free = 0;
    f.get_gradient_space(gxrows, free);
    EXPECT_EQ(gxrows, (Vi(4) << 0, 1, 1, 2).finished());
    Laid<ConstraintFunction> laid(f, kDim, kConOffset, true, true);
    ASSERT_EQ(laid.slots, 10);

    const Vd x = (Vd(3) << 1.0, 2.0, 3.0).finished();
    const Vd lambda = (Vd(2) << 10.0, 100.0).finished();

    // constraints: residual slots only.
    Vd fx = Vd::Constant(2, -1.0);
    f.constraints(x, fx);
    EXPECT_EQ(fx, (Vd(2) << 2.0, 6.0).finished());

    // constraints_adjointgradient: residuals, and J^T lambda per gradient slot (accumulated).
    fx.setConstant(-1.0);
    Vd agx = Vd::Constant(4, 1000.0);
    f.constraints_adjointgradient(x, lambda, fx, agx);
    EXPECT_EQ(fx, (Vd(2) << 2.0, 6.0).finished());
    EXPECT_EQ(agx, (Vd(4) << 1020.0, 1010.0, 1300.0, 1200.0).finished()) << "adds into the slots";

    // constraints_jacobian: residuals and the Jacobian entries in the KKT matrix.
    fx.setConstant(-1.0);
    f.constraints_jacobian(x, fx, laid.kkt, laid.locations, laid.clashes, laid.locks);
    EXPECT_EQ(fx, (Vd(2) << 2.0, 6.0).finished());
    Eigen::MatrixXd K = laid.kkt;
    EXPECT_EQ(K(3, 0), 2.0);
    EXPECT_EQ(K(3, 1), 1.0);
    EXPECT_EQ(K(4, 1), 3.0);
    EXPECT_EQ(K(4, 2), 2.0);
    EXPECT_EQ(K(1, 0), 0.0) << "the Hessian entries are not written by this shape";
    EXPECT_EQ(K(2, 1), 0.0);

    // constraints_jacobian_adjointgradient: both of the above, KKT accumulated into.
    fx.setConstant(-1.0);
    agx.setZero();
    f.constraints_jacobian_adjointgradient(x, lambda, fx, agx, laid.kkt, laid.locations,
                                           laid.clashes, laid.locks);
    EXPECT_EQ(agx, (Vd(4) << 20.0, 10.0, 300.0, 200.0).finished());
    K = laid.kkt;
    EXPECT_EQ(K(3, 0), 4.0) << "a second shape adds its Jacobian to what the first left";
    EXPECT_EQ(K(4, 2), 4.0);

    // The full shape: Jacobian and the lambda-weighted lower Hessian.
    laid.kkt.coeffs().setZero();
    agx.setZero();
    f.constraints_jacobian_adjointgradient_adjointhessian(x, lambda, fx, agx, laid.kkt,
                                                          laid.locations, laid.clashes, laid.locks);
    K = laid.kkt;
    EXPECT_EQ(K(3, 0), 2.0);
    EXPECT_EQ(K(3, 1), 1.0);
    EXPECT_EQ(K(4, 1), 3.0);
    EXPECT_EQ(K(4, 2), 2.0);
    EXPECT_EQ(K(1, 0), 10.0);
    EXPECT_EQ(K(2, 1), 100.0);
    EXPECT_EQ(agx, (Vd(4) << 20.0, 10.0, 300.0, 200.0).finished());
    EXPECT_EQ(fx, (Vd(2) << 2.0, 6.0).finished());
}

TEST(ConstraintFunctionHandle, ASharedColumnIsLockedWhenItsClashMarkSaysSo) {
    // Two applications that both write column 1: with the sharing flag cleared the writes are
    // guarded by the lock the clash mark names; the sums are the same either way.
    ConstraintFunction f = product_function();
    f.index_data.unique_constraints = false;
    Vi fxrows = Vi::Zero(2), gxrows = Vi::Zero(4);
    int free = 0;
    f.get_constraint_space(fxrows, free);
    free = 0;
    f.get_gradient_space(gxrows, free);
    Laid<ConstraintFunction> laid(f, kDim, kConOffset, true, true);
    laid.clashes(1) = 0; // column 1 is contested, guarded by lock 0
    laid.locks = std::vector<std::mutex>(1);
    const Vd x = (Vd(3) << 1.0, 2.0, 3.0).finished();
    const Vd lambda = (Vd(2) << 10.0, 100.0).finished();
    Vd fx(2), agx(4);
    agx.setZero();
    f.constraints_jacobian_adjointgradient_adjointhessian(x, lambda, fx, agx, laid.kkt,
                                                          laid.locations, laid.clashes, laid.locks);
    const Eigen::MatrixXd K = laid.kkt;
    EXPECT_EQ(K(3, 1), 1.0);
    EXPECT_EQ(K(4, 1), 3.0);
    EXPECT_EQ(K(2, 1), 100.0);
    EXPECT_TRUE(laid.locks[0].try_lock()) << "every lock taken was released";
    laid.locks[0].unlock();
}

TEST(ConstraintFunctionHandle, ThreadSplitGivesEachThreadItsOwnApplicationsAndTheSamePiece) {
    ConstraintFunction f(ConstraintInterface(Product{}),
                         (Mi(2, 3) << 0, 1, 2, 1, 2, 3).finished(),
                         (Mi(1, 3) << 0, 1, 2).finished());
    const auto parts = f.thread_split(2); // 3 applications -> 2 + 1
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(parts[0].index_data.num_appl(), 2);
    EXPECT_EQ(parts[1].index_data.num_appl(), 1);
    EXPECT_EQ(parts[0].function.name(), "product");
    EXPECT_EQ(parts[1].function.output_rows(), 1);
    EXPECT_EQ(parts[1].index_data.get_v_index(), (Mi(2, 1) << 2, 3).finished());
    EXPECT_EQ(parts[1].index_data.get_c_index(), (Mi(1, 1) << 2).finished());

    // Evaluated separately, the slices cover the same residuals the whole function computes.
    const Vd x = (Vd(4) << 1.0, 2.0, 3.0, 4.0).finished();
    Vi rows = Vi::Zero(3);
    int free = 0;
    f.get_constraint_space(rows, free);
    Vd whole(3);
    f.constraints(x, whole);
    ConstraintFunction a = parts[0], b = parts[1];
    Vi ra = Vi::Zero(2), rb = Vi::Zero(1);
    free = 0;
    a.get_constraint_space(ra, free);
    free = 0;
    b.get_constraint_space(rb, free);
    Vd fa(2), fb(1);
    a.constraints(x, fa);
    b.constraints(x, fb);
    EXPECT_EQ(fa(0), whole(0));
    EXPECT_EQ(fa(1), whole(1));
    EXPECT_EQ(fb(0), whole(2));

    EXPECT_EQ(f.thread_split(1).size(), 1u);
    EXPECT_EQ(f.thread_split(7).size(), 3u) << "no empty slices past the application count";
    EXPECT_THROW(f.thread_split(0), std::invalid_argument);
}

TEST(ObjectiveFunctionHandle, EveryShapeAddsScaledValuesAndFillsItsSlots) {
    ObjectiveFunction f(ObjectiveInterface(Bowl{}), (Mi(2, 2) << 0, 1, 1, 2).finished());
    EXPECT_EQ(f.function.input_rows(), 2);
    EXPECT_EQ(f.function.output_rows(), 1) << "an objective is scalar by construction";
    EXPECT_EQ(f.num_grad_eles(), 4);
    EXPECT_EQ(f.num_kkt_elements(true, true), 6);
    EXPECT_EQ(f.num_kkt_elements(true, false), 0);

    Vi gxrows = Vi::Zero(4);
    int free = 0;
    f.get_gradient_space(gxrows, free);
    EXPECT_EQ(gxrows, (Vi(4) << 0, 1, 1, 2).finished());

    const Vd x = (Vd(3) << 1.0, 2.0, 3.0).finished();

    double val = 100.0;
    f.objective(3.0, x, val);
    EXPECT_DOUBLE_EQ(val, 100.0 + 3.0 * ((1 + 4) + (4 + 9)));

    val = 0.0;
    Vd gx = Vd::Constant(4, -1.0);
    f.objective_gradient(2.0, x, val, gx);
    EXPECT_DOUBLE_EQ(val, 2.0 * 18.0);
    EXPECT_EQ(gx, (Vd(4) << 4.0, 8.0, 8.0, 12.0).finished()) << "slot k holds scale * grad";

    Laid<ObjectiveFunction> laid(f, 3, 0, false, true);
    ASSERT_EQ(laid.slots, 6);
    val = 0.0;
    gx.setZero();
    f.objective_gradient_hessian(0.5, x, val, gx, laid.kkt, laid.locations, laid.clashes,
                                 laid.locks);
    EXPECT_DOUBLE_EQ(val, 9.0);
    EXPECT_EQ(gx, (Vd(4) << 1.0, 2.0, 2.0, 3.0).finished());
    const Eigen::MatrixXd K = laid.kkt;
    EXPECT_DOUBLE_EQ(K(0, 0), 1.0);
    EXPECT_DOUBLE_EQ(K(1, 1), 2.0) << "the two applications sum on their shared variable";
    EXPECT_DOUBLE_EQ(K(2, 2), 1.0);
    EXPECT_DOUBLE_EQ(K(1, 0), 0.0);
}

TEST(ObjectiveFunctionHandle, ThreadSplitCoversEveryApplicationExactlyOnce) {
    ObjectiveFunction f(ObjectiveInterface(Bowl{}),
                        (Mi(2, 5) << 0, 1, 2, 3, 4, 1, 2, 3, 4, 5).finished());
    const auto parts = f.thread_split(3); // 5 -> 2 + 2 + 1
    ASSERT_EQ(parts.size(), 3u);
    int total = 0;
    for (const auto &p : parts) {
        total += p.index_data.num_appl();
        EXPECT_EQ(p.function.input_rows(), 2);
    }
    EXPECT_EQ(total, 5);
    EXPECT_EQ(parts[2].index_data.get_v_index(), (Mi(2, 1) << 4, 5).finished());
    EXPECT_EQ(f.thread_split(9).size(), 5u);
    EXPECT_THROW(f.thread_split(-1), std::invalid_argument);
}

TEST(FunctionHandles, CopiesOfAHandleEvaluateIndependentlyOfTheOriginal) {
    ConstraintFunction f = product_function();
    ConstraintFunction copy = f;
    const Vd x = (Vd(3) << 1.0, 2.0, 3.0).finished();
    Vi rows = Vi::Zero(2);
    int free = 0;
    copy.get_constraint_space(rows, free);
    Vd fx(2);
    copy.constraints(x, fx);
    EXPECT_EQ(fx, (Vd(2) << 2.0, 6.0).finished());
    EXPECT_EQ(copy.function.name(), f.function.name());
    // Mutating the copy's map does not move the original's.
    copy.index_data.set_v_index((Mi(2, 1) << 0, 2).finished());
    EXPECT_EQ(f.index_data.num_appl(), 2);
    EXPECT_EQ(copy.index_data.num_appl(), 1);
}

} // namespace
} // namespace hven
