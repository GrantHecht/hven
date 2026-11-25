// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M8 AP-4 G5: the dense structure helpers in model/nlp.h -- the order they fill, the
// counts, and every refusal path.

#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <hven/model/nlp.h>

namespace {

using hven::Index;
using hven::IndexVec;

TEST(DenseStructure, CountsAreTheDenseSizes) {
    EXPECT_EQ(hven::dense_jacobian_nonzeros(0, 3), 0);
    EXPECT_EQ(hven::dense_jacobian_nonzeros(2, 4), 8);
    EXPECT_EQ(hven::dense_jacobian_nonzeros(5, 1), 5);
    EXPECT_EQ(hven::dense_lower_hessian_nonzeros(1), 1);
    EXPECT_EQ(hven::dense_lower_hessian_nonzeros(4), 10);
    EXPECT_EQ(hven::dense_lower_hessian_nonzeros(7), 28);
}

TEST(DenseStructure, JacobianIsRowMajor) {
    IndexVec r(6), c(6);
    hven::dense_jacobian_structure(2, 3, r, c);
    const std::vector<Index> er{0, 0, 0, 1, 1, 1}, ec{0, 1, 2, 0, 1, 2};
    for (Index k = 0; k < 6; ++k) {
        EXPECT_EQ(r[k], er[k]) << k;
        EXPECT_EQ(c[k], ec[k]) << k;
    }
}

TEST(DenseStructure, ZeroRowsJacobianFillsNothing) {
    IndexVec r(0), c(0);
    EXPECT_NO_THROW(hven::dense_jacobian_structure(0, 4, r, c));
}

TEST(DenseStructure, HessianIsTheLowerTriangleRowByRow) {
    IndexVec r(10), c(10);
    hven::dense_lower_hessian_structure(4, r, c);
    const std::vector<Index> er{0, 1, 1, 2, 2, 2, 3, 3, 3, 3}, ec{0, 0, 1, 0, 1, 2, 0, 1, 2, 3};
    for (Index k = 0; k < 10; ++k) {
        EXPECT_EQ(r[k], er[k]) << k;
        EXPECT_EQ(c[k], ec[k]) << k;
        EXPECT_GE(r[k], c[k]);
    }
}

TEST(DenseStructure, SingleVariableHessianIsOneDiagonalEntry) {
    IndexVec r(1), c(1);
    hven::dense_lower_hessian_structure(1, r, c);
    EXPECT_EQ(r[0], 0);
    EXPECT_EQ(c[0], 0);
}

TEST(DenseStructure, BadSizesAreRefused) {
    EXPECT_THROW(hven::dense_jacobian_nonzeros(-1, 3), std::invalid_argument);
    EXPECT_THROW(hven::dense_jacobian_nonzeros(2, 0), std::invalid_argument);
    EXPECT_THROW(hven::dense_jacobian_nonzeros(2, -4), std::invalid_argument);
    EXPECT_THROW(hven::dense_jacobian_nonzeros(std::numeric_limits<Index>::max(), 2),
                 std::invalid_argument);
    EXPECT_THROW(hven::dense_lower_hessian_nonzeros(0), std::invalid_argument);
    EXPECT_THROW(hven::dense_lower_hessian_nonzeros(-3), std::invalid_argument);
    EXPECT_THROW(hven::dense_lower_hessian_nonzeros(std::numeric_limits<Index>::max()),
                 std::invalid_argument);
}

TEST(DenseStructure, WrongOutputLengthsAreRefusedAndNameTheCounts) {
    IndexVec good(6), shortv(5), longv(7);
    EXPECT_THROW(hven::dense_jacobian_structure(2, 3, shortv, good), std::invalid_argument);
    EXPECT_THROW(hven::dense_jacobian_structure(2, 3, good, longv), std::invalid_argument);
    try {
        hven::dense_jacobian_structure(2, 3, shortv, good);
        FAIL();
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("has 5 entries, the dense pattern has 6"),
                  std::string::npos)
            << e.what();
    }
    EXPECT_THROW(hven::dense_jacobian_structure(-1, 3, good, good), std::invalid_argument);
    EXPECT_THROW(hven::dense_jacobian_structure(2, 0, good, good), std::invalid_argument);

    IndexVec h3(3), h4(4), h6(6);
    EXPECT_THROW(hven::dense_lower_hessian_structure(3, h4, h6), std::invalid_argument);
    EXPECT_THROW(hven::dense_lower_hessian_structure(3, h6, h3), std::invalid_argument);
    EXPECT_THROW(hven::dense_lower_hessian_structure(0, h3, h3), std::invalid_argument);
}

TEST(DenseStructure, RefusalLeavesTheOutputsUntouched) {
    IndexVec r = IndexVec::Constant(5, -7), c = IndexVec::Constant(6, -7);
    EXPECT_THROW(hven::dense_jacobian_structure(2, 3, r, c), std::invalid_argument);
    EXPECT_EQ(r.minCoeff(), -7);
    EXPECT_EQ(c.maxCoeff(), -7);
}

} // namespace
