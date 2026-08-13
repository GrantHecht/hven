#include <gtest/gtest.h>
#include <hven/detail/sqp/kkt_assembly.h>
using namespace hven::solvers;

TEST(KktAssembly, DimensionsAndRegularization) {
    QpProblem qp; // 3 vars, 1 equality, 2 inequalities (build as in earlier tests)
    qp.H =
        Eigen::MatrixXd::Identity(3, 3).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(3);
    Eigen::MatrixXd Aed(1, 3);
    Aed << 1, 1, 1;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Zero(1);
    Eigen::MatrixXd Aid(2, 3);
    Aid << 1, 0, 0, 0, 1, 0;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Zero(2);
    qp.lower = Vec::Constant(3, -1e20);
    qp.upper = Vec::Constant(3, 1e20);

    WorkingSet ws(3, 2);
    ws.bound_state()[2] = BoundState::kAtLower; // variable 2 fixed at its bound
    ws.add_ineq(0);                             // inequality row 0 in working set

    QpOptions opts;
    auto asm_ = assemble_kkt(qp, ws, opts);
    // free vars = {0,1}; rows = 2 free + 1 eq + 1 working ineq = 4
    EXPECT_EQ(asm_.K.rows(), 4);
    Eigen::MatrixXd Kd = Eigen::MatrixXd(asm_.K).selfadjointView<Eigen::Upper>();
    EXPECT_DOUBLE_EQ(Kd(0, 0), 1.0 + opts.primal_delta);
    EXPECT_DOUBLE_EQ(Kd(2, 2), -opts.dual_mu); // eq row regularized
    EXPECT_DOUBLE_EQ(Kd(3, 3), -opts.dual_mu); // working ineq row regularized
    EXPECT_DOUBLE_EQ(Kd(0, 2), 1.0);           // Ae entry for free var 0
    EXPECT_EQ(asm_.free_of_full.size(), 2u);
}

// H couples a free variable (0) and a fixed variable (1) through an
// off-diagonal upper entry; the fixed variable's substitution value must
// show up as exactly that coupling term in rhs_shift's Hessian row, and
// nowhere else (K is 1x1 since var 1 is eliminated and there are no
// constraints).
TEST(KktAssembly, HessianEliminationShift) {
    QpProblem qp;
    Eigen::MatrixXd Hd(2, 2);
    Hd << 2, 3, 0, 5; // upper triangle: H(0,0)=2, H(0,1)=3, H(1,1)=5
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    qp.Ae = Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    qp.be = Vec(0);
    qp.Ai = Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    qp.upper(1) = 4.0; // variable 1's active bound value

    WorkingSet ws(2, 0);
    ws.bound_state()[1] = BoundState::kAtUpper; // variable 1 fixed at upper = 4.0

    QpOptions opts;
    auto asm_ = assemble_kkt(qp, ws, opts);

    ASSERT_EQ(asm_.free_of_full.size(), 1u);
    EXPECT_EQ(asm_.free_of_full[0], 0);
    ASSERT_EQ(asm_.K.rows(), 1);
    Eigen::MatrixXd Kd = Eigen::MatrixXd(asm_.K).selfadjointView<Eigen::Upper>();
    EXPECT_DOUBLE_EQ(Kd(0, 0), 2.0 + opts.primal_delta); // H(0,0) + delta only; no fixed-fixed term

    ASSERT_EQ(asm_.rhs_shift.size(), 1);
    // rhs_shift(0) = H(0,1) * x_fixed(1) = 3 * 4 = 12.
    EXPECT_DOUBLE_EQ(asm_.rhs_shift(0), 12.0);
}

// A WorkingSet sized for a different problem (mi = 5) than the QpProblem it
// is paired with (mi = 2) must be rejected up front: an active row index
// valid for the WorkingSet but beyond qp.Ai's rows would otherwise read past
// the end of Ai's storage inside the assembly loop.
TEST(KktAssembly, MismatchedWorkingSetThrows) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    qp.Ae = Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 2);
    Aid << 1, 0, 0, 1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Zero(2);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);

    WorkingSet ws(2, 5); // mi = 5 disagrees with qp.mi() = 2
    ws.add_ineq(4);      // valid for ws, out of range for qp.Ai

    EXPECT_THROW(assemble_kkt(qp, ws, QpOptions{}), std::invalid_argument);
}

// Mirror of HessianEliminationShift: here the FIXED variable has the SMALLER
// full index, so the coupling entry's upper-triangle storage is
// (row = fixed 0, col = free 1) and the shift must be gathered through the
// symmetrized branch that attributes the stored value to the free column.
//
// H = [2 3; 3 5] (stored upper: (0,0)=2, (0,1)=3, (1,1)=5); fix x0 at
// lower = 7. Free set = {1}, so K = [H(1,1) + delta] and the Hessian row of
// rhs_shift is
//     rhs_shift(0) = H(1,0) * x_fixed(0) = 3 * 7 = 21,
// sourced ONLY from the stored (0,1) entry via symmetry (H(1,0) == H(0,1)).
// The (0,0) entry is fixed-fixed and contributes nowhere.
TEST(KktAssembly, MirroredHessianEliminationShift) {
    QpProblem qp;
    Eigen::MatrixXd Hd(2, 2);
    Hd << 2, 3, 0, 5;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    qp.Ae = Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    qp.be = Vec(0);
    qp.Ai = Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    qp.lower(0) = 7.0; // variable 0's active bound value

    WorkingSet ws(2, 0);
    ws.bound_state()[0] = BoundState::kAtLower; // variable 0 fixed at lower = 7.0

    QpOptions opts;
    auto asm_ = assemble_kkt(qp, ws, opts);

    ASSERT_EQ(asm_.free_of_full.size(), 1u);
    EXPECT_EQ(asm_.free_of_full[0], 1);
    ASSERT_EQ(asm_.K.rows(), 1);
    Eigen::MatrixXd Kd = Eigen::MatrixXd(asm_.K).selfadjointView<Eigen::Upper>();
    EXPECT_DOUBLE_EQ(Kd(0, 0), 5.0 + opts.primal_delta); // H(1,1) + delta only

    ASSERT_EQ(asm_.rhs_shift.size(), 1);
    EXPECT_DOUBLE_EQ(asm_.rhs_shift(0), 21.0);
}

TEST(WorkingSet, AddDropThrowAndSort) {
    WorkingSet ws(4, 5);
    EXPECT_EQ(ws.num_free(), 4);

    ws.add_ineq(3);
    ws.add_ineq(1);
    ws.add_ineq(2);
    EXPECT_EQ(ws.active_ineq(), (std::vector<Index>{1, 2, 3}));

    EXPECT_THROW(ws.add_ineq(2), std::invalid_argument);  // already present
    EXPECT_THROW(ws.drop_ineq(4), std::invalid_argument); // never present
    EXPECT_THROW(ws.add_ineq(5), std::invalid_argument);  // out of range

    ws.drop_ineq(2);
    EXPECT_EQ(ws.active_ineq(), (std::vector<Index>{1, 3}));
    EXPECT_THROW(ws.drop_ineq(2), std::invalid_argument); // no longer present

    ws.bound_state()[0] = BoundState::kAtLower;
    ws.bound_state()[3] = BoundState::kFixed;
    EXPECT_EQ(ws.num_free(), 2);
}
