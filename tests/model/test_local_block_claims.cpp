// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_local_block_claims.cpp — the local-block layer's KKT claim pass, called directly
// (model/local_blocks.h): which slots a constraint or objective block claims, in what order,
// with which coordinates; how an eliminated variable is claimed (-1 on both axes); the rule
// that a constraint block with second derivatives claims its Jacobian and Hessian domains
// together; the element counts; and the default name. test_pieces.cpp drives the same layer
// through a solve; here the claim stream itself is the thing asserted.

#include <array>
#include <stdexcept>
#include <string>

#include <Eigen/Core>
#include <gtest/gtest.h>

#include <hven/model/local_blocks.h>
#include <hven/model/solver_indexing_data.h>

namespace hven {
namespace {

using Mi = Eigen::MatrixXi;
using Vi = Eigen::VectorXi;

struct Curved { // one row, two inputs, dense Jacobian and full lower Hessian
    static constexpr Index kInputs = 2, kOutputs = 1;
    std::string name() const { return "curved"; }
    void value(ConstVecRef v, VecRef out) const { out[0] = v[0] * v[1]; }
    void jacobian(ConstVecRef v, MatRef J) const { J << v[1], v[0]; }
    void hessian(ConstVecRef, ConstVecRef lambda, MatRef H) const { H(1, 0) = lambda[0]; }
};

struct Straight { // one row, two inputs, one Jacobian entry, no second derivatives
    static constexpr Index kInputs = 2, kOutputs = 1;
    static constexpr std::array<BlockEntry, 1> kJacobianPattern{{{0, 1}}};
    static constexpr auto kHessianPattern = hven::kNoHessian;
    void value(ConstVecRef v, VecRef out) const { out[0] = 3.0 * v[1]; }
    void jacobian(ConstVecRef, MatRef J) const { J(0, 1) = 3.0; }
};

struct Bowl { // an objective term over two inputs: full lower Hessian
    static constexpr Index kInputs = 2;
    double value(ConstVecRef v) const { return v.squaredNorm(); }
    void gradient(ConstVecRef v, VecRef g) const { g = 2.0 * v; }
    void hessian(ConstVecRef, MatRef H) const {
        H(0, 0) = 2.0;
        H(1, 1) = 2.0;
    }
};

struct Slope { // a linear objective: no second derivatives
    static constexpr Index kInputs = 2;
    static constexpr auto kHessianPattern = hven::kNoHessian;
    double value(ConstVecRef v) const { return v[0] + v[1]; }
    void gradient(ConstVecRef, VecRef g) const { g.setOnes(); }
};

SolverIndexingData one_application(int row, int var0, int var1) {
    return SolverIndexingData(2, 1, (Mi(2, 1) << var0, var1).finished(),
                              (Mi(1, 1) << row).finished());
}

TEST(LocalBlockClaims, ACoupledClaimPutsTheJacobianThenTheLowerHessianOfEachApplication) {
    LocalConstraintBlock<Curved> block{Curved{}};
    SolverIndexingData d(2, 1, (Mi(2, 2) << 3, 6, 5, 7).finished(), (Mi(1, 2) << 4, 8).finished());
    Vi rows = Vi::Constant(12, -9), cols = Vi::Constant(12, -9);
    int free = 0;
    block.get_kkt_space(rows, cols, free, 10, true, true, d);
    ASSERT_EQ(free, 10) << "(2 Jacobian + 3 Hessian entries) per application";
    EXPECT_EQ(d.inner_kkt_starts, (Vi(2) << 0, 5).finished());
    // Application 0: row 4 + offset 10 against columns 3 and 5, then the lower triangle.
    EXPECT_EQ(rows.segment(0, 5), (Vi(5) << 14, 14, 3, 5, 5).finished());
    EXPECT_EQ(cols.segment(0, 5), (Vi(5) << 3, 5, 3, 3, 5).finished());
    // Application 1.
    EXPECT_EQ(rows.segment(5, 5), (Vi(5) << 18, 18, 6, 7, 7).finished());
    EXPECT_EQ(cols.segment(5, 5), (Vi(5) << 6, 7, 6, 6, 7).finished());
    EXPECT_EQ(rows(10), -9) << "nothing past the claimed slots is written";
    EXPECT_EQ(block.num_kkt_elements(true, true), 5);
}

TEST(LocalBlockClaims, ABlockWithSecondDerivativesRefusesToClaimOneDomainWithoutTheOther) {
    LocalConstraintBlock<Curved> block{Curved{}};
    SolverIndexingData d = one_application(0, 0, 1);
    Vi rows = Vi::Zero(8), cols = Vi::Zero(8);
    int free = 2;
    try {
        block.get_kkt_space(rows, cols, free, 0, true, false, d);
        ADD_FAILURE() << "expected std::logic_error";
    } catch (const std::logic_error &e) {
        const std::string message(e.what());
        EXPECT_NE(message.find("local constraint block 'curved'"), std::string::npos) << message;
        EXPECT_NE(message.find("without the other"), std::string::npos) << message;
        EXPECT_NE(message.find("claims the two together"), std::string::npos) << message;
    }
    EXPECT_EQ(free, 2) << "a refused claim advances nothing";
    EXPECT_THROW(block.get_kkt_space(rows, cols, free, 0, false, true, d), std::logic_error);
    EXPECT_EQ(free, 2);
    // Neither domain is a legal (empty) claim.
    block.get_kkt_space(rows, cols, free, 0, false, false, d);
    EXPECT_EQ(free, 2);
    EXPECT_EQ(d.inner_kkt_starts(0), 2);
}

TEST(LocalBlockClaims, ABlockWithNoSecondDerivativesMayClaimTheJacobianAlone) {
    LocalConstraintBlock<Straight> block{Straight{}};
    SolverIndexingData d = one_application(2, 4, 9);
    Vi rows = Vi::Zero(4), cols = Vi::Zero(4);
    int free = 0;
    EXPECT_NO_THROW(block.get_kkt_space(rows, cols, free, 20, true, false, d));
    EXPECT_EQ(free, 1);
    EXPECT_EQ(rows(0), 22);
    EXPECT_EQ(cols(0), 9) << "the pattern's one entry reads input 1";
    EXPECT_EQ(block.num_kkt_elements(true, false), 1);
    EXPECT_EQ(block.num_kkt_elements(true, true), 1) << "kNoHessian adds no Hessian slots";
    EXPECT_EQ(block.num_kkt_elements(false, true), 0);
    // And the Hessian-only claim is likewise legal (it claims nothing).
    free = 0;
    EXPECT_NO_THROW(block.get_kkt_space(rows, cols, free, 20, false, true, d));
    EXPECT_EQ(free, 0);
}

TEST(LocalBlockClaims, AnEliminatedVariableIsClaimedAsMinusOneOnBothAxes) {
    LocalConstraintBlock<Curved> block{Curved{}};
    SolverIndexingData d = one_application(1, 3, 5);
    // The scatter map sends input 0 to no column (an eliminated variable) and input 1 to 5.
    d.set_output_v_index((Mi(2, 1) << -1, 5).finished());
    Vi rows = Vi::Constant(5, -9), cols = Vi::Constant(5, -9);
    int free = 0;
    block.get_kkt_space(rows, cols, free, 10, true, true, d);
    ASSERT_EQ(free, 5);
    // Jacobian: input 0's entry is eliminated, input 1's lands at (row 1 + 10, col 5).
    EXPECT_EQ(rows.segment(0, 2), (Vi(2) << -1, 11).finished());
    EXPECT_EQ(cols.segment(0, 2), (Vi(2) << -1, 5).finished());
    // Hessian lower triangle (0,0), (1,0), (1,1): any entry touching input 0 is eliminated.
    EXPECT_EQ(rows.segment(2, 3), (Vi(3) << -1, -1, 5).finished());
    EXPECT_EQ(cols.segment(2, 3), (Vi(3) << -1, -1, 5).finished());
}

TEST(LocalBlockClaims, AnObjectiveBlockClaimsOnlyItsHessianAndIgnoresTheJacobianFlag) {
    LocalObjectiveBlock<Bowl> block{Bowl{}};
    SolverIndexingData d(2, (Mi(2, 2) << 0, 2, 1, 3).finished());
    Vi rows = Vi::Constant(8, -9), cols = Vi::Constant(8, -9);
    int free = 1;
    block.get_kkt_space(rows, cols, free, 99, true, true, d);
    ASSERT_EQ(free, 7);
    EXPECT_EQ(d.inner_kkt_starts, (Vi(2) << 1, 4).finished());
    EXPECT_EQ(rows.segment(1, 6), (Vi(6) << 0, 1, 1, 2, 3, 3).finished());
    EXPECT_EQ(cols.segment(1, 6), (Vi(6) << 0, 0, 1, 2, 2, 3).finished());
    EXPECT_EQ(block.num_kkt_elements(true, true), 3);
    EXPECT_EQ(block.num_kkt_elements(true, false), 0);
    EXPECT_EQ(block.num_kkt_elements(false, true), 3);

    // Without the Hessian domain the claim is empty, and the starts still record the cursor.
    free = 3;
    block.get_kkt_space(rows, cols, free, 0, true, false, d);
    EXPECT_EQ(free, 3);
    EXPECT_EQ(d.inner_kkt_starts, (Vi(2) << 3, 3).finished());
}

TEST(LocalBlockClaims, ALinearObjectiveBlockClaimsNothingEvenWithTheHessianDomain) {
    LocalObjectiveBlock<Slope> block{Slope{}};
    SolverIndexingData d(2, (Mi(2, 1) << 0, 1).finished());
    Vi rows = Vi::Zero(2), cols = Vi::Zero(2);
    int free = 0;
    block.get_kkt_space(rows, cols, free, 0, true, true, d);
    EXPECT_EQ(free, 0);
    EXPECT_EQ(block.num_kkt_elements(true, true), 0);
}

TEST(LocalBlockClaims, TheDefaultNameCarriesTheSizesAndAPieceNameOverridesIt) {
    EXPECT_EQ(LocalConstraintBlock<Curved>{Curved{}}.name(), "curved");
    EXPECT_EQ(LocalConstraintBlock<Straight>{Straight{}}.name(),
              "local constraint block (kInputs 2, kOutputs 1)");
    EXPECT_EQ(LocalConstraintBlock<Straight>{Straight{}}.input_rows(), 2);
    EXPECT_EQ(LocalConstraintBlock<Straight>{Straight{}}.output_rows(), 1);
    EXPECT_TRUE(LocalConstraintBlock<Straight>{Straight{}}.thread_safe());
    EXPECT_EQ(LocalObjectiveBlock<Bowl>{Bowl{}}.input_rows(), 2);
    EXPECT_EQ(LocalObjectiveBlock<Bowl>{Bowl{}}.name(), "local objective block (kInputs 2)");
    EXPECT_TRUE(LocalObjectiveBlock<Bowl>{Bowl{}}.thread_safe());
}

} // namespace
} // namespace hven
