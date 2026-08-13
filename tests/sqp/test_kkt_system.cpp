#include <gtest/gtest.h>
#include <hven/detail/sqp/kkt_system.h>
using namespace hven::solvers;

// K = [H Aᵀ; A 0] with H = diag(2,3), A = [1 1]. Saddle point: inertia (2,1,0).
static SpMatU make_kkt3() {
    Eigen::MatrixXd D(3, 3);
    D << 2, 0, 1, 0, 3, 1, 1, 1, 0;
    SpMatU K = D.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    K.makeCompressed();
    return K;
}

TEST(KktSystem, FactorSolveInertia) {
    QpOptions opts;
    KktSystem kkt(opts);
    SpMatU K = make_kkt3();
    kkt.factorize(K);
    EXPECT_EQ(kkt.num_pos_eigs(), 2);
    EXPECT_EQ(kkt.num_neg_eigs(), 1);
    Vec b(3);
    b << 1.0, 2.0, 0.5;
    Vec x = kkt.solve(b);
    Eigen::MatrixXd Kd = Eigen::MatrixXd(K).selfadjointView<Eigen::Upper>();
    EXPECT_LT((Kd * x - b).norm(), 1e-10);
}

TEST(KktSystem, PatternHashDetectsChange) {
    QpOptions opts;
    KktSystem kkt(opts);
    SpMatU K = make_kkt3();
    kkt.factorize(K);
    EXPECT_TRUE(kkt.pattern_matches(K));
    // Different pattern: drop the (0,2) coupling.
    Eigen::MatrixXd D(3, 3);
    D << 2, 0, 0, 0, 3, 1, 0, 1, 0;
    SpMatU K2 = D.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    K2.makeCompressed();
    EXPECT_FALSE(kkt.pattern_matches(K2));
    kkt.factorize(K2); // must re-analyze internally, not corrupt state
    EXPECT_EQ(kkt.num_neg_eigs(), 1);
}
