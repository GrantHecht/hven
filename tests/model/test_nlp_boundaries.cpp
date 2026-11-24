// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_nlp_boundaries.cpp — the declared-problem boundary (model/nlp.h, model/problem.h): what
// Problem(shared_ptr<Nlp>) refuses at construction, by message, the dense-structure helpers'
// argument rules, and the base Nlp's defaults for the callbacks a problem without those
// entries does not override. The shapes that are accepted are asserted next to the ones
// that are not, so a refusal that widens past its rule is caught from the other side.

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#include <Eigen/Core>
#include <gtest/gtest.h>

#include <hven/model/nlp.h>
#include <hven/model/problem.h>

namespace hven {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

template <class E = std::invalid_argument, class F>
void expect_throw(F &&f, std::initializer_list<const char *> fragments) {
    try {
        f();
        ADD_FAILURE() << "expected an exception";
    } catch (const E &e) {
        const std::string message(e.what());
        for (const char *fragment : fragments) {
            EXPECT_NE(message.find(fragment), std::string::npos)
                << "missing '" << fragment << "' in: " << message;
        }
    }
}

// A problem whose declaration is whatever the test sets: n variables, m rows, dense Jacobian and
// lower Hessian patterns sized from the dimensions (or overridden), bounds from the vectors.
class Configurable final : public Nlp {
  public:
    NlpDimensions dims;
    Vec x_lower, x_upper, g_lower, g_upper;
    bool hessian = true;
    std::string label = "configurable";

    static std::shared_ptr<Configurable> make(Index n, Index m) {
        auto p = std::make_shared<Configurable>();
        p->dims.n = n;
        p->dims.m = m;
        p->dims.jacobian_nonzeros = m * n;
        p->dims.hessian_nonzeros = n * (n + 1) / 2;
        p->x_lower = Vec::Constant(n, -1.0);
        p->x_upper = Vec::Constant(n, 1.0);
        p->g_lower = Vec::Constant(m, 0.0);
        p->g_upper = Vec::Constant(m, 0.0);
        return p;
    }

    NlpDimensions dimensions() const override { return dims; }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl = x_lower;
        xu = x_upper;
        gl = g_lower;
        gu = g_upper;
    }
    double eval_f(ConstVecRef x) const override { return 0.5 * x.squaredNorm(); }
    void eval_grad_f(ConstVecRef x, VecRef grad) const override { grad = x; }
    void eval_g(ConstVecRef x, VecRef g) const override {
        for (Index i = 0; i < g.size(); ++i) {
            g(i) = x.sum();
        }
    }
    void jacobian_structure(IndexVecRef rows, IndexVecRef cols) const override {
        dense_jacobian_structure(dims.m, dims.n, rows, cols);
    }
    void eval_jacobian(ConstVecRef, VecRef values) const override { values.setOnes(); }
    bool has_hessian() const override { return hessian; }
    void hessian_structure(IndexVecRef rows, IndexVecRef cols) const override {
        dense_lower_hessian_structure(dims.n, rows, cols);
    }
    void eval_hessian(ConstVecRef, double obj, ConstVecRef, VecRef values) const override {
        Index k = 0;
        for (Index i = 0; i < dims.n; ++i) {
            for (Index j = 0; j <= i; ++j, ++k) {
                values(k) = i == j ? obj : 0.0;
            }
        }
    }
    std::string name() const override { return label; }
};

// ---------------------------------------------------------------------------
// Problem(shared_ptr<Nlp>)
// ---------------------------------------------------------------------------

TEST(NlpBoundary, AWellFormedDeclarationIsAcceptedAndCounted) {
    auto nlp = Configurable::make(3, 2);
    nlp->g_lower << 0.0, -kInf;
    nlp->g_upper << 0.0, 1.0;
    const Problem p(nlp);
    EXPECT_EQ(p.num_variables(), 3);
    EXPECT_EQ(p.num_constraints(), 2);
    EXPECT_EQ(p.statistics().variables, 3);
    EXPECT_EQ(p.statistics().boxed_variables, 3);
    EXPECT_EQ(p.statistics().equality_rows, 1);
    EXPECT_EQ(p.statistics().upper_bounded_rows, 1);
    EXPECT_EQ(p.num_partitions(), 1);
}

TEST(NlpBoundary, ANullNlpIsRefusedByName) {
    expect_throw([] { Problem p{std::shared_ptr<Nlp>()}; }, {"Problem", "Nlp pointer is null"});
}

TEST(NlpBoundary, DimensionsAreCheckedBeforeAnythingIsAllocated) {
    {
        auto nlp = Configurable::make(2, 0);
        nlp->dims.n = 0;
        nlp->label = "no_vars";
        expect_throw([&] { Problem p(nlp); },
                     {"no_vars", "dimensions().n must be positive, got 0"});
    }
    {
        auto nlp = Configurable::make(2, 1);
        nlp->dims.m = -1;
        expect_throw([&] { Problem p(nlp); }, {"must be non-negative", "m=-1"});
    }
    {
        auto nlp = Configurable::make(2, 1);
        nlp->dims.jacobian_nonzeros = -4;
        expect_throw([&] { Problem p(nlp); }, {"jacobian_nonzeros=-4"});
    }
    {
        auto nlp = Configurable::make(2, 1);
        nlp->dims.hessian_nonzeros = -2;
        expect_throw([&] { Problem p(nlp); }, {"hessian_nonzeros=-2"});
    }
    {
        // Sizes past a 32-bit index are refused before the transcription lays anything.
        auto nlp = Configurable::make(2, 0);
        nlp->dims.n = Index{1} << 31;
        expect_throw([&] { Problem p(nlp); }, {"must fit a 32-bit index", "2147483647 at most"});
        nlp = Configurable::make(2, 0);
        nlp->dims.hessian_nonzeros = Index{1} << 32;
        expect_throw([&] { Problem p(nlp); }, {"must fit a 32-bit index"});
    }
    {
        auto nlp = Configurable::make(2, 0);
        nlp->dims.jacobian_nonzeros = 3;
        nlp->label = "phantom_jac";
        expect_throw([&] { Problem p(nlp); },
                     {"phantom_jac", "3 Jacobian nonzeros declared for a problem with no constraints"});
    }
}

TEST(NlpBoundary, VariableBoundsRefuseNanInvertedAndFixedAtInfinity) {
    {
        auto nlp = Configurable::make(3, 0);
        nlp->x_lower(1) = std::nan("");
        expect_throw([&] { Problem p(nlp); }, {"variable 1 bound is NaN"});
    }
    {
        auto nlp = Configurable::make(3, 0);
        nlp->x_upper(2) = std::nan("");
        expect_throw([&] { Problem p(nlp); }, {"variable 2 bound is NaN"});
    }
    {
        auto nlp = Configurable::make(3, 0);
        nlp->x_lower(0) = 2.0;
        nlp->x_upper(0) = 1.0;
        expect_throw([&] { Problem p(nlp); }, {"variable 0 lower bound 2 exceeds upper bound 1"});
    }
    {
        auto nlp = Configurable::make(3, 0);
        nlp->x_lower(1) = nlp->x_upper(1) = kInf;
        expect_throw([&] { Problem p(nlp); },
                     {"variable 1", "both bounds are inf", "fixed at infinity"});
    }
    {
        auto nlp = Configurable::make(3, 0);
        nlp->x_lower(1) = nlp->x_upper(1) = -kInf;
        expect_throw([&] { Problem p(nlp); }, {"both bounds are -inf", "fixed at infinity"});
    }
    {
        // A variable fixed at a finite value is the ordinary way to fix one, and is counted so.
        auto nlp = Configurable::make(3, 0);
        nlp->x_lower(1) = nlp->x_upper(1) = 0.5;
        const Problem p(nlp);
        EXPECT_EQ(p.statistics().fixed_variables, 1);
        EXPECT_EQ(p.statistics().boxed_variables, 2);
    }
}

TEST(NlpBoundary, RowBoundsRefuseNanInvertedAndEqualityAtInfinity) {
    {
        auto nlp = Configurable::make(2, 2);
        nlp->g_lower(1) = std::nan("");
        expect_throw([&] { Problem p(nlp); }, {"constraint row 1", "NaN"});
    }
    {
        auto nlp = Configurable::make(2, 2);
        nlp->g_lower(0) = 3.0;
        nlp->g_upper(0) = 2.0;
        expect_throw([&] { Problem p(nlp); }, {"constraint row 0", "lower bound 3 exceeds upper bound 2"});
    }
    {
        auto nlp = Configurable::make(2, 1);
        nlp->g_lower(0) = nlp->g_upper(0) = kInf;
        expect_throw([&] { Problem p(nlp); }, {"constraint row 0", "an equality at infinity"});
    }
}

TEST(NlpBoundary, ARowWithNoBoundsAtAllIsAFreeRowAndCountedAsSuch) {
    auto nlp = Configurable::make(2, 3);
    nlp->g_lower << -kInf, 0.0, 1.0;
    nlp->g_upper << kInf, kInf, 4.0;
    const Problem p(nlp);
    EXPECT_EQ(p.statistics().free_rows, 1);
    EXPECT_EQ(p.statistics().lower_bounded_rows, 1);
    EXPECT_EQ(p.statistics().range_rows, 1);
    EXPECT_EQ(p.statistics().equality_rows, 0);
}

// ---------------------------------------------------------------------------
// The base Nlp's defaults serve an empty block and refuse a non-empty one by name.
// ---------------------------------------------------------------------------

class Bare final : public Nlp {
  public:
    NlpDimensions dimensions() const override { return {2, 0, 0, 0}; }
    void bounds(VecRef xl, VecRef xu, VecRef, VecRef) const override {
        xl.setConstant(-1.0);
        xu.setConstant(1.0);
    }
    double eval_f(ConstVecRef x) const override { return x.squaredNorm(); }
    void eval_grad_f(ConstVecRef x, VecRef grad) const override { grad = 2.0 * x; }
    std::string name() const override { return "bare"; }
};

TEST(NlpBoundary, TheDefaultCallbacksAcceptEmptyBlocksAndRefuseNonEmptyOnes) {
    const Bare bare;
    Vec x = Vec::Zero(2);
    Vec none(0);
    IndexVec no_index(0);
    EXPECT_NO_THROW(bare.eval_g(x, none));
    EXPECT_NO_THROW(bare.jacobian_structure(no_index, no_index));
    EXPECT_NO_THROW(bare.eval_jacobian(x, none));
    EXPECT_NO_THROW(bare.hessian_structure(no_index, no_index));
    EXPECT_NO_THROW(bare.eval_hessian(x, 1.0, none, none));
    EXPECT_TRUE(bare.has_hessian());

    Vec one = Vec::Zero(1);
    IndexVec one_index = IndexVec::Zero(1);
    expect_throw<std::logic_error>([&] { bare.eval_g(x, one); },
                                   {"Nlp 'bare'", "eval_g() is not overridden", "declares 1 entries"});
    expect_throw<std::logic_error>([&] { bare.jacobian_structure(one_index, one_index); },
                                   {"jacobian_structure() is not overridden"});
    expect_throw<std::logic_error>([&] { bare.eval_jacobian(x, one); },
                                   {"eval_jacobian() is not overridden"});
    expect_throw<std::logic_error>([&] { bare.hessian_structure(one_index, one_index); },
                                   {"hessian_structure() is not overridden"});
    expect_throw<std::logic_error>([&] { bare.eval_hessian(x, 1.0, none, one); },
                                   {"eval_hessian() is not overridden"});
}

TEST(NlpBoundary, TheDefaultNameIsNlpAndNamesAreCarriedIntoRefusals) {
    class Anonymous final : public Nlp {
      public:
        NlpDimensions dimensions() const override { return {0, 0, 0, 0}; }
        void bounds(VecRef, VecRef, VecRef, VecRef) const override {}
        double eval_f(ConstVecRef) const override { return 0.0; }
        void eval_grad_f(ConstVecRef, VecRef) const override {}
    };
    const Anonymous a;
    EXPECT_EQ(a.name(), "nlp");
    expect_throw([&] { Problem p(std::make_shared<Anonymous>()); },
                 {"nlp", "dimensions().n must be positive"});
}

// ---------------------------------------------------------------------------
// Dense-structure helpers.
// ---------------------------------------------------------------------------

TEST(DenseStructureBoundary, NonzeroCountsRefuseWhatHasNoMeaning) {
    expect_throw([] { dense_jacobian_nonzeros(-1, 3); },
                 {"dense_jacobian_nonzeros", "need m >= 0 and n >= 1", "m = -1, n = 3"});
    expect_throw([] { dense_jacobian_nonzeros(2, 0); }, {"n = 0"});
    expect_throw([] { dense_jacobian_nonzeros(std::numeric_limits<Index>::max(), 2); },
                 {"m * n overflows Index"});
    EXPECT_EQ(dense_jacobian_nonzeros(0, 5), 0);
    EXPECT_EQ(dense_jacobian_nonzeros(3, 4), 12);

    expect_throw([] { dense_lower_hessian_nonzeros(0); },
                 {"dense_lower_hessian_nonzeros", "need n >= 1", "n = 0"});
    expect_throw([] { dense_lower_hessian_nonzeros((Index{1} << 31) + 1); }, {"too large"});
    EXPECT_EQ(dense_lower_hessian_nonzeros(1), 1);
    EXPECT_EQ(dense_lower_hessian_nonzeros(4), 10);
}

TEST(DenseStructureBoundary, StructureWritersRefuseAWrongLengthOutputByName) {
    IndexVec rows = IndexVec::Zero(5), cols = IndexVec::Zero(6);
    expect_throw([&] { dense_jacobian_structure(2, 3, rows, cols); },
                 {"dense_jacobian_structure rows", "output has 5 entries", "dense pattern has 6"});
    IndexVec rows6 = IndexVec::Zero(6);
    expect_throw([&] { dense_jacobian_structure(2, 3, rows6, rows); },
                 {"dense_jacobian_structure cols", "output has 5 entries"});
    IndexVec h3 = IndexVec::Zero(3), h6 = IndexVec::Zero(6);
    expect_throw([&] { dense_lower_hessian_structure(3, h3, h6); },
                 {"dense_lower_hessian_structure rows", "output has 3 entries", "dense pattern has 6"});
    expect_throw([&] { dense_lower_hessian_structure(3, h6, h3); },
                 {"dense_lower_hessian_structure cols"});
}

TEST(DenseStructureBoundary, TheWritersEmitRowMajorFullAndLowerTrianglePatterns) {
    IndexVec rows(6), cols(6);
    dense_jacobian_structure(2, 3, rows, cols);
    EXPECT_EQ(rows, (IndexVec(6) << 0, 0, 0, 1, 1, 1).finished());
    EXPECT_EQ(cols, (IndexVec(6) << 0, 1, 2, 0, 1, 2).finished());
    IndexVec hr(6), hc(6);
    dense_lower_hessian_structure(3, hr, hc);
    EXPECT_EQ(hr, (IndexVec(6) << 0, 1, 1, 2, 2, 2).finished());
    EXPECT_EQ(hc, (IndexVec(6) << 0, 0, 1, 0, 1, 2).finished());
    IndexVec none(0);
    EXPECT_NO_THROW(dense_jacobian_structure(0, 3, none, none));
}

} // namespace
} // namespace hven
