#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include <tycho_sqp/border_ops.h>
#include <tycho_sqp/eqp_solve.h>
#include <tycho_sqp/kkt_assembly.h>
#include <tycho_sqp/kkt_system.h>
#include <tycho_sqp/schur_complement.h>
#include <tycho_sqp/working_set.h>

#include "support/border_test_utils.h"

using namespace tycho::sqp;

namespace {

// Repeated verbatim from tests/test_qp_engine.cpp's simple_box_qp() (also
// duplicated in test_qp_engine_border.cpp, following that file's own
// precedent of copying rather than sharing this tiny fixture).
QpProblem simple_box_qp() {
    // min 1/2(x0^2 + x1^2) - x0 - 2 x1  s.t. x0 + x1 <= 1, 0 <= x <= 10.
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Identity(2, 2);
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -1, -2;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << 1, 1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, 1.0);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 10.0);
    return qp;
}

// n=2, me=1, mi=2: exercises add_ineq_row/delete_k0_row against a K0 whose
// constraint-row block mixes an equality row with (potential) working rows,
// so `var_count`/`me` offsets are non-trivial (unlike simple_box_qp, whose
// me == 0 leaves that arithmetic degenerate).
QpProblem eq_and_two_ineq_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Identity(2, 2);
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -1, -2;
    Eigen::MatrixXd Aed(1, 2);
    Aed << 1, -1;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Zero(1);
    Eigen::MatrixXd Aid(2, 2);
    Aid << 1, 1, 1, -1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << 1, 5;
    qp.lower = Vec::Constant(2, -10.0);
    qp.upper = Vec::Constant(2, 10.0);
    return qp;
}

// n=2, me=1, mi=2, chosen so Ae, Ai.row(0), and Ai.row(1) are pairwise
// non-parallel (unlike eq_and_two_ineq_qp, where Ae and Ai.row(1) are
// IDENTICAL directions -- fine for the permutation-equivalence tests above,
// which compare against a dense factorization of the literal same augmented
// matrix, but it makes the equality/row-1 sub-system nearly rank-deficient,
// which is exactly wrong for ComposedDeleteAndAddBorders: that test compares
// two DIFFERENT computational paths (a multi-border Schur solve vs. a
// from-scratch dense LU) that are only mathematically equivalent, not
// literally the same matrix, so a near-singular shared sub-system amplifies
// ordinary floating-point rounding differently on each path -- observed as
// an O(1) absolute disagreement in the equality/row-1 duals (whose true
// magnitude was ~1e8) despite ~1e-8 RELATIVE agreement, well within
// dual_mu's own regularization scale. A well-conditioned fixture keeps every
// dual O(1) so the 1e-9 ABSOLUTE tolerance below is meaningful.
QpProblem well_conditioned_eq_and_two_ineq_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Identity(2, 2);
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -1, -2;
    Eigen::MatrixXd Aed(1, 2);
    Aed << 1, 0;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Zero(1);
    Eigen::MatrixXd Aid(2, 2);
    Aid << 0, 1, 1, 1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << 1, 1;
    qp.lower = Vec::Constant(2, -10.0);
    qp.upper = Vec::Constant(2, 10.0);
    return qp;
}

} // namespace

// (a) Activating a row that is NOT one of K0's initial working rows: build
// K0 with an empty working set, border it with BorderOps::add_ineq_row for
// row 0, and check the bordered solve against a dense factorization of the
// IDENTICAL augmented system (K0 padded with the same v/d at row/col 3) --
// the pattern of test_schur.cpp's BorderedSolveMatchesDirectFactorization,
// just with `v` built by BorderOps instead of by hand.
TEST(BorderOps, AddIneqRowMatchesDirect) {
    QpProblem qp = eq_and_two_ineq_qp();
    QpOptions opts;

    WorkingSet ws0(qp.n(), qp.mi());
    KktAssembly full = assemble_kkt_full(qp, ws0, opts);
    ASSERT_EQ(full.K.rows(), qp.n() + qp.me()); // n_working == 0

    KktSystem kkt0(opts);
    kkt0.factorize(full.K);
    SchurComplement schur(kkt0, opts);

    const Index k0_rows = full.K.rows();
    Vec v = BorderOps::add_ineq_row(qp, /*j=*/0, k0_rows);

    // v must equal Ai.row(0)^T scattered over the variable block and zero
    // over the equality block.
    Vec v_expected = Vec::Zero(k0_rows);
    v_expected(0) = 1.0;
    v_expected(1) = 1.0;
    EXPECT_DOUBLE_EQ((v - v_expected).norm(), 0.0);

    schur.add_border(v, -opts.dual_mu);

    Vec rhs(k0_rows + 1);
    rhs.head(qp.n()) = -qp.g;
    rhs(qp.n()) = qp.be(0);
    rhs(k0_rows) = qp.bi(0);

    Vec sol = schur.solve(rhs);

    Eigen::MatrixXd K4 = Eigen::MatrixXd::Zero(k0_rows + 1, k0_rows + 1);
    K4.topLeftCorner(k0_rows, k0_rows) = Eigen::MatrixXd(full.K).selfadjointView<Eigen::Upper>();
    K4.block(0, k0_rows, k0_rows, 1) = v;
    K4.block(k0_rows, 0, 1, k0_rows) = v.transpose();
    K4(k0_rows, k0_rows) = -opts.dual_mu;
    Vec x_ref = K4.fullPivLu().solve(rhs);

    EXPECT_LT((sol - x_ref).norm(), 1e-9);
}

// (b) Deactivating a row that WAS assembled into K0 as an initial working
// row: assemble K0 with inequality row 0 active, deactivate it via
// BorderOps::delete_k0_row, and check (i) the recovered x agrees with a
// dense reference that has that row/column removed entirely, and (ii) the
// deleted row's own dual (K0's unknown at var_count + k, NOT the border's
// own new y) comes back ~0 -- the actual deactivation mechanism, see
// border_ops.h's delete_k0_row comment for why d = 0 makes this exact.
TEST(BorderOps, DeleteK0RowMatchesDirect) {
    QpProblem qp = simple_box_qp();
    QpOptions opts;

    WorkingSet ws1(qp.n(), qp.mi());
    ws1.add_ineq(0);
    KktAssembly full = assemble_kkt_full(qp, ws1, opts);
    const Index var_count = qp.n();
    const Index me = qp.me();
    ASSERT_EQ(full.K.rows(), var_count + me + 1); // n_working == 1

    KktSystem kkt1(opts);
    kkt1.factorize(full.K);
    SchurComplement schur(kkt1, opts);

    const Index k0_rows = full.K.rows();
    Vec v = BorderOps::delete_k0_row(/*k=*/0, me, var_count, k0_rows);

    Vec v_expected = Vec::Zero(k0_rows);
    v_expected(var_count) = 1.0; // var_count + k, k == 0
    EXPECT_DOUBLE_EQ((v - v_expected).norm(), 0.0);

    schur.add_border(v, 0.0); // d = 0 -- see delete_k0_row's comment

    Vec rhs(k0_rows + 1);
    rhs.head(qp.n()) = -qp.g;
    rhs(var_count) = qp.bi(0); // K0's own row rhs -- inert, see comment above
    rhs(k0_rows) = 0.0;        // border rhs: pin the deleted row's dual to 0

    Vec sol = schur.solve(rhs);

    EXPECT_NEAR(sol(var_count), 0.0, 1e-9); // deactivated row's own dual

    // Dense reference: K0 with row/column var_count (the deactivated row)
    // removed entirely -- just the Hessian block here, since me == 0 and
    // there is only the one (now-removed) working row.
    Eigen::MatrixXd K0d = Eigen::MatrixXd(full.K).selfadjointView<Eigen::Upper>();
    Eigen::MatrixXd A = K0d.topLeftCorner(var_count, var_count);
    Vec rhs_ref = -qp.g;
    Vec x_ref = A.fullPivLu().solve(rhs_ref);

    EXPECT_LT((sol.head(var_count) - x_ref).norm(), 1e-9);
}

// delete_k0_row must reject an attempt to delete one of K0's EQUALITY rows
// (k < me): equalities are never deactivated this way, only rows that were
// themselves part of the initial working set.
TEST(BorderOps, DeleteK0RowRejectsEqualityRow) {
    QpProblem qp = eq_and_two_ineq_qp();
    QpOptions opts;
    WorkingSet ws(qp.n(), qp.mi());
    ws.add_ineq(0);
    KktAssembly full = assemble_kkt_full(qp, ws, opts);
    const Index var_count = qp.n();
    const Index me = qp.me();
    EXPECT_THROW(BorderOps::delete_k0_row(/*k=*/0, me, var_count, full.K.rows()),
                 std::invalid_argument);
}

// delete_k0_row must also reject k past K0's last constraint row.
TEST(BorderOps, DeleteK0RowRejectsOutOfRange) {
    QpProblem qp = simple_box_qp();
    QpOptions opts;
    WorkingSet ws(qp.n(), qp.mi());
    ws.add_ineq(0);
    KktAssembly full = assemble_kkt_full(qp, ws, opts);
    EXPECT_THROW(BorderOps::delete_k0_row(/*k=*/1, qp.me(), qp.n(), full.K.rows()),
                 std::invalid_argument);
}

// (c) Pin-by-border (via BorderOps::pin_variable) vs. pin-by-elimination
// (solve_eqp), routing Task 1's BorderPinEquivalence through BorderOps.
TEST(BorderOps, PinVariableMatchesEliminated) {
    QpProblem qp = simple_box_qp();
    QpOptions opts;

    WorkingSet ws_empty(qp.n(), qp.mi());
    KktAssembly full = assemble_kkt_full(qp, ws_empty, opts);

    KktSystem kkt0(opts);
    kkt0.factorize(full.K);
    SchurComplement schur(kkt0, opts);

    Vec v = BorderOps::pin_variable(/*i=*/0, full.K.rows());
    Vec v_expected = Vec::Zero(full.K.rows());
    v_expected(0) = 1.0;
    EXPECT_DOUBLE_EQ((v - v_expected).norm(), 0.0);

    schur.add_border(v, -opts.dual_mu);

    Vec rhs(full.K.rows() + schur.dim());
    rhs.head(qp.n()) = -qp.g;
    rhs(qp.n()) = qp.lower(0); // border rhs carries the bound VALUE

    Vec sol = schur.solve(rhs);

    std::vector<Vec> border_v{v};
    std::vector<double> border_d{-opts.dual_mu};
    sol = test_support::refine_bordered_solve(full.K, qp.n(), border_v, border_d, schur, rhs, sol,
                                              opts);
    Vec x_border = sol.head(qp.n());

    WorkingSet ws_pin(qp.n(), qp.mi());
    ws_pin.bound_state()[0] = BoundState::kAtLower;
    KktSystem kkt1(opts);
    EqpResult eqp = solve_eqp(qp, ws_pin, kkt1, opts);

    ASSERT_EQ(x_border.size(), eqp.x.size());
    EXPECT_LT((x_border - eqp.x).norm(), 1e-9);
}

// (d) add_border then drop_border returns exactly to the pre-border solve
// (schur.dim() == 0 again, and the same K0 rhs reproduces the same answer).
TEST(BorderOps, AddThenDropRoundTrip) {
    QpProblem qp = eq_and_two_ineq_qp();
    QpOptions opts;

    WorkingSet ws0(qp.n(), qp.mi());
    KktAssembly full = assemble_kkt_full(qp, ws0, opts);

    KktSystem kkt0(opts);
    kkt0.factorize(full.K);
    SchurComplement schur(kkt0, opts);

    Vec rhs0(full.K.rows());
    rhs0.head(qp.n()) = -qp.g;
    rhs0(qp.n()) = qp.be(0);

    Vec x_before = kkt0.solve(rhs0);

    Vec v = BorderOps::add_ineq_row(qp, /*j=*/0, full.K.rows());
    schur.add_border(v, -opts.dual_mu);
    ASSERT_EQ(schur.dim(), 1);

    schur.drop_border(0);
    ASSERT_EQ(schur.dim(), 0);

    Vec x_after = schur.solve(rhs0);
    EXPECT_LT((x_after - x_before).norm(), 1e-9);
}

// Composes a delete_k0_row border with an add_ineq_row border LIVE AT THE
// SAME TIME (schur.dim() == 2) -- exactly the pattern a border-mode engine
// runs continuously (deactivate one row, activate another, in the same
// working-set step). This exercises index-overlap risk specific to
// BorderOps (as opposed to test_schur.cpp's generic multi-border coverage,
// which never composes THESE two particular vector shapes): delete_k0_row's
// v is a unit vector INSIDE K0's own block, while add_ineq_row's v spans
// the variable block -- both must resolve to the correct absolute
// coordinates, and the two borders' own y-values (schur's NEW unknowns,
// appended after k0_rows in add_border call order) must not get swapped.
TEST(BorderOps, ComposedDeleteAndAddBorders) {
    QpProblem qp = well_conditioned_eq_and_two_ineq_qp(); // n=2, me=1, mi=2
    QpOptions opts;

    // K0: inequality row 0 assembled in as the initial (sole) working row.
    WorkingSet ws1(qp.n(), qp.mi());
    ws1.add_ineq(0);
    KktAssembly full = assemble_kkt_full(qp, ws1, opts);
    const Index var_count = qp.n();
    const Index me = qp.me();
    const Index k0_rows = full.K.rows();
    ASSERT_EQ(k0_rows, var_count + me + 1); // n_working == 1

    KktSystem kkt(opts);
    kkt.factorize(full.K);
    SchurComplement schur(kkt, opts);

    // Border 0: deactivate row 0 (K0's constraint row k == me, the sole
    // working row, absolute position var_count + me).
    Vec v_del = BorderOps::delete_k0_row(/*k=*/me, me, var_count, k0_rows);
    schur.add_border(v_del, 0.0);

    // Border 1: activate row 1 (not part of K0 at all).
    Vec v_add = BorderOps::add_ineq_row(qp, /*j=*/1, k0_rows);
    schur.add_border(v_add, -opts.dual_mu);

    ASSERT_EQ(schur.dim(), 2);

    Vec rhs(k0_rows + 2);
    rhs.head(qp.n()) = -qp.g;
    rhs(var_count) = qp.be(0);
    rhs(var_count + me) = qp.bi(0); // K0's own row-0 rhs -- inert, see delete_k0_row's comment
    rhs(k0_rows) = 0.0;             // border 0 (delete): pin row 0's dual to 0
    rhs(k0_rows + 1) = qp.bi(1);    // border 1 (add): row 1's rhs

    Vec sol = schur.solve(rhs);

    EXPECT_NEAR(sol(var_count + me), 0.0, 1e-9); // deactivated row 0's own dual

    // Dense reference: the mathematically equivalent system with row 1
    // active (assembled into K0 the ordinary way) and row 0 absent entirely.
    WorkingSet ws2(qp.n(), qp.mi());
    ws2.add_ineq(1);
    KktAssembly full2 = assemble_kkt_full(qp, ws2, opts);
    ASSERT_EQ(full2.K.rows(), var_count + me + 1);

    Eigen::MatrixXd K2 = Eigen::MatrixXd(full2.K).selfadjointView<Eigen::Upper>();
    Vec rhs2(full2.K.rows());
    rhs2.head(qp.n()) = -qp.g;
    rhs2(var_count) = qp.be(0);
    rhs2(var_count + me) = qp.bi(1);
    Vec x2 = K2.fullPivLu().solve(rhs2);

    EXPECT_LT((sol.head(var_count) - x2.head(var_count)).norm(), 1e-9);
    EXPECT_LT(std::abs(sol(var_count) - x2(var_count)), 1e-9); // equality dual agrees too

    // The y-block-ordering check: border 1's OWN new dual (schur's second
    // appended unknown, sol(k0_rows + 1)) must equal row 1's dual in the
    // dense reference (x2's working-row entry), not border 0's.
    EXPECT_LT(std::abs(sol(k0_rows + 1) - x2(var_count + me)), 1e-9);
}
