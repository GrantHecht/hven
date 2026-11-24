// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_nlp_model_assembly_boundaries.cpp — NlpModelAssembly (the claim-stream bridge over an
// NlpModel) against a model that breaks its contract: every construction refusal, the lay-time
// shape checks on the three matrices and the bound vectors, the renegotiation and thread-count
// rules, the failed-lay-leaves-the-layout-alone rule, and the evaluation-time checks that a
// pattern which drifted after the claim pass, or a block the model sized wrongly, is refused by
// name. The well-behaved path is covered by test_nlp_model_assembly.cpp.

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#include <Eigen/Sparse>
#include <gtest/gtest.h>

#include <hven/detail/model/nlp_model.h>
#include <hven/detail/model/nlp_model_assembly.h>
#include <hven/model/candidate_point.h>

namespace hven::detail {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

template <class F> void expect_refusal(F &&f, std::initializer_list<const char *> fragments) {
    try {
        f();
        ADD_FAILURE() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        const std::string message(e.what());
        for (const char *fragment : fragments) {
            EXPECT_NE(message.find(fragment), std::string::npos)
                << "missing '" << fragment << "' in: " << message;
        }
    }
}

SpMatRM sparse_of(const Eigen::MatrixXd &M) {
    SpMatRM S = M.sparseView();
    S.makeCompressed();
    return S;
}

// n = 2 variables, one equality and one inequality row, every contract-breaking knob public.
class Knobs final : public NlpModel {
  public:
    Index n_ = 2, me_ = 1, mi_ = 1;
    Vec lower_ = Vec::Constant(2, -5.0);
    Vec upper_ = Vec::Constant(2, 5.0);
    Vec start_ = Vec::Zero(2);
    bool hessian_ = true;
    bool hess_lower_triangle = false;  // returns an entry below the diagonal
    bool wrong_hess_shape = false;
    bool wrong_jac_e_shape = false;
    bool wrong_jac_i_shape = false;
    mutable bool drift_jac_e = false;   // after the claim pass, the pattern shrinks
    mutable bool short_gradient = false;
    mutable bool short_ce = false;

    Index n() const override { return n_; }
    Index me() const override { return me_; }
    Index mi() const override { return mi_; }
    double eval_f(const Vec &x) const override { return 0.5 * x.squaredNorm(); }
    Vec eval_grad(const Vec &x) const override {
        return short_gradient ? Vec(x.head(1)) : x;
    }
    Vec eval_ce(const Vec &x) const override {
        return short_ce ? Vec(0) : Vec::Constant(me_, x.sum());
    }
    Vec eval_ci(const Vec &x) const override { return Vec::Constant(mi_, x(0)); }
    SpMatRM eval_hess(const Vec &, double scale, const Vec &, const Vec &) const override {
        if (wrong_hess_shape) {
            return sparse_of(Eigen::MatrixXd::Identity(3, 3));
        }
        // The contract is the upper triangle only, with a pattern that does not depend on the
        // scale or the multipliers (a structural zero stays stored).
        SpMatRM S(2, 2);
        S.insert(0, 0) = scale;
        S.insert(1, 1) = scale;
        if (hess_lower_triangle) {
            S.insert(1, 0) = 1.0;
        }
        S.makeCompressed();
        return S;
    }
    SpMatRM eval_jac_e(const Vec &) const override {
        if (wrong_jac_e_shape) {
            return sparse_of(Eigen::MatrixXd::Ones(me_ + 1, 2));
        }
        SpMatRM S(me_, 2);
        for (Index r = 0; r < me_; ++r) {
            S.insert(r, 0) = 1.0;
            if (!drift_jac_e) {
                S.insert(r, 1) = 1.0;
            }
        }
        S.makeCompressed();
        return S;
    }
    SpMatRM eval_jac_i(const Vec &) const override {
        if (wrong_jac_i_shape) {
            return sparse_of(Eigen::MatrixXd::Ones(mi_, 3));
        }
        SpMatRM S(mi_, 2);
        for (Index r = 0; r < mi_; ++r) {
            S.insert(r, 0) = 1.0;
            S.insert(r, 1) = 1.0;
        }
        S.makeCompressed();
        return S;
    }
    bool has_hessian() const override { return hessian_; }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return start_; }
};

std::shared_ptr<Knobs> knobs() { return std::make_shared<Knobs>(); }

TEST(NlpModelAssemblyBoundary, AWellBehavedModelLaysTheDocumentedClaimStream) {
    NlpModelAssembly bridge(knobs());
    EXPECT_EQ(bridge.kkt_dimension(), 4);
    EXPECT_EQ(bridge.hessian_claims().count, 2);
    EXPECT_EQ(bridge.equality_jacobian_claims().count, 2);
    EXPECT_EQ(bridge.inequality_jacobian_claims().count, 2);
    EXPECT_EQ(bridge.kkt_claim_rows().size(), 6);
    EXPECT_EQ(bridge.declaration().primal_vars, 2);
    EXPECT_EQ(bridge.declaration().equality_rows, 1);
    EXPECT_EQ(bridge.declaration().inequality_rows, 1);
    EXPECT_EQ(bridge.evaluation_threads(), 1);
    EXPECT_TRUE(has_capability(bridge.capabilities(), AssemblyCapability::kValuesFastPath));
    EXPECT_FALSE(has_capability(bridge.capabilities(), AssemblyCapability::kDirectScatter));
    EXPECT_EQ(bridge.absent_bounds(), 0);
    EXPECT_DOUBLE_EQ(bridge.infinite_bound(), 1e20);
}

TEST(NlpModelAssemblyBoundary, ConstructionRefusesANullModelAndABadThresholdByName) {
    expect_refusal([] { NlpModelAssembly b(nullptr); }, {"NlpModelAssembly", "model is null"});
    for (const double bad : {0.0, -1.0, std::nan("")}) {
        expect_refusal([&] { NlpModelAssembly b(knobs(), bad); },
                       {"infinite_bound must be > 0", "kInfinity is legal"});
    }
    // +infinity is the documented legal threshold: nothing reads as absent.
    auto m = knobs();
    m->upper_(0) = 1e30;
    NlpModelAssembly ok(m, kInf);
    EXPECT_EQ(ok.absent_bounds(), 0);
}

TEST(NlpModelAssemblyBoundary, DimensionsThatAreNotCountsAreRefusedByName) {
    auto negative = knobs();
    negative->me_ = -2;
    expect_refusal([&] { NlpModelAssembly b(negative); },
                   {"the model reports me() = -2", "not a count this declaration can carry"});
    auto huge = knobs();
    huge->mi_ = Index{1} << 40;
    expect_refusal([&] { NlpModelAssembly b(huge); }, {"mi() = 1099511627776"});
    auto empty = knobs();
    empty->n_ = 0;
    expect_refusal([&] { NlpModelAssembly b(empty); },
                   {"n() = 0", "no primal variables is not a problem"});
    auto bad_n = knobs();
    bad_n->n_ = -1;
    expect_refusal([&] { NlpModelAssembly b(bad_n); }, {"n() = -1"});
}

TEST(NlpModelAssemblyBoundary, BoundAndStartVectorsMustHaveTheDeclaredLength) {
    auto m = knobs();
    m->lower_ = Vec::Constant(3, -1.0);
    expect_refusal([&] { NlpModelAssembly b(m); },
                   {"returned lower() with 3 rows", "declares 2 of them"});
    m = knobs();
    m->upper_ = Vec::Constant(1, 1.0);
    expect_refusal([&] { NlpModelAssembly b(m); }, {"upper() with 1 rows"});
    m = knobs();
    m->start_ = Vec::Zero(5);
    expect_refusal([&] { NlpModelAssembly b(m); }, {"start_point() with 5 rows"});
}

TEST(NlpModelAssemblyBoundary, ABoundPairThatReadsAsNoIntervalIsRefusedWithTheAbsentBoundClause) {
    auto m = knobs();
    m->lower_(1) = 1e21; // reads as +inf under the default threshold
    m->upper_(1) = 5.0;
    expect_refusal([&] { NlpModelAssembly b(m); },
                   {"variable 1 has no interval", "lower=1e+21", "read as [inf, 5]",
                    "infinite_bound = 1e+20"});
    m = knobs();
    m->lower_(0) = m->upper_(0) = 1e21; // fixed at an infinity
    expect_refusal([&] { NlpModelAssembly b(m); }, {"variable 0 has no interval"});
    // The same pair is fine under a larger threshold: it is an ordinary (huge) fixed value.
    m = knobs();
    m->lower_(0) = m->upper_(0) = 1e21;
    EXPECT_NO_THROW(NlpModelAssembly(m, 1e30));
}

TEST(NlpModelAssemblyBoundary, AbsentBoundsAreCountedAndLaidAsInfinities) {
    auto m = knobs();
    m->lower_(0) = -1e25;
    m->upper_(1) = 3e22;
    NlpModelAssembly b(m);
    EXPECT_EQ(b.absent_bounds(), 2);
    const auto &records = b.declaration().variable_bounds;
    ASSERT_EQ(records.size(), 2u);
    EXPECT_EQ(records[0].lower, -kInf);
    EXPECT_EQ(records[1].upper, kInf);
    EXPECT_EQ(records[1].lower, -5.0);
}

TEST(NlpModelAssemblyBoundary, ANanBoundIsRefusedByTheDeclarationsOwnValidation) {
    auto m = knobs();
    m->lower_(1) = std::nan("");
    EXPECT_THROW(NlpModelAssembly b(m), std::invalid_argument);
}

TEST(NlpModelAssemblyBoundary, MatricesOfTheWrongShapeAreRefusedBeforeAnyClaimIsLaid) {
    auto h = knobs();
    h->wrong_hess_shape = true;
    expect_refusal([&] { NlpModelAssembly b(h); },
                   {"eval_hess returned a 3 x 3 matrix", "declares that block as 2 x 2"});
    auto je = knobs();
    je->wrong_jac_e_shape = true;
    expect_refusal([&] { NlpModelAssembly b(je); },
                   {"eval_jac_e returned a 2 x 2 matrix", "declares that block as 1 x 2"});
    auto ji = knobs();
    ji->wrong_jac_i_shape = true;
    expect_refusal([&] { NlpModelAssembly b(ji); },
                   {"eval_jac_i returned a 1 x 3 matrix", "declares that block as 1 x 2"});
}

TEST(NlpModelAssemblyBoundary, AHessianEntryBelowTheDiagonalIsRefusedAtTheClaimPass) {
    auto m = knobs();
    m->hess_lower_triangle = true;
    expect_refusal([&] { NlpModelAssembly b(m); },
                   {"eval_hess stored an entry at (row 1, column 0)", "below the diagonal",
                    "upper triangle only"});
}

TEST(NlpModelAssemblyBoundary, AModelWithoutAHessianLaysNoHessianClaimAndIsNeverAsked) {
    auto m = knobs();
    m->hessian_ = false;
    m->wrong_hess_shape = true; // would be refused if it were read
    NlpModelAssembly b(m);
    EXPECT_EQ(b.hessian_claims().count, 0);
    EXPECT_EQ(b.kkt_claim_rows().size(), 4);
}

TEST(NlpModelAssemblyBoundary, ABlockWithNoRowsIsNotEvaluatedAndLaysNoClaims) {
    auto m = knobs();
    m->me_ = 0;
    m->mi_ = 0;
    m->wrong_jac_e_shape = true; // never read: a block with no rows is skipped
    m->wrong_jac_i_shape = true;
    NlpModelAssembly b(m);
    EXPECT_EQ(b.kkt_dimension(), 2);
    EXPECT_EQ(b.equality_jacobian_claims().count, 0);
    EXPECT_EQ(b.inequality_jacobian_claims().count, 0);
}

TEST(NlpModelAssemblyBoundary, PartitionAndThreadCountsAreCheckedAndRenegotiationIsAStructuralEvent) {
    NlpModelAssembly b(knobs());
    expect_refusal([&] { b.negotiate_partition_count(0); },
                   {"a partition count must be at least 1 (got 0)"});
    expect_refusal([&] { b.negotiate_partition_count(-3); }, {"got -3"});
    expect_refusal([&] { b.set_evaluation_threads(0); },
                   {"an evaluation thread count must be at least 1 (got 0)"});
    EXPECT_NO_THROW(b.set_evaluation_threads(4));
    EXPECT_EQ(b.evaluation_threads(), 1) << "the serial bridge always reports one thread";

    const StructureEpoch before = b.structure_epoch();
    EXPECT_EQ(b.negotiate_partition_count(8), 1) << "one serial piece adopts one partition";
    EXPECT_NE(b.structure_epoch(), before) << "renegotiating re-lays and so moves the epoch";
    EXPECT_EQ(b.model_structure_key().partition_count, 1);
}

// ---------------------------------------------------------------------------
// Evaluation-time checks.
// ---------------------------------------------------------------------------

TEST(NlpModelAssemblyBoundary, APatternThatShrankAfterTheClaimPassIsRefusedAtFirstOrderEvaluation) {
    auto m = knobs();
    NlpModelAssembly b(m);
    Vec x = Vec::Zero(2), le = Vec::Zero(1), li = Vec::Zero(1);
    double f = 0.0;
    Vec eq(1), iq(1), g(2), a(2);
    CandidateFirstOrder out{{f, eq, iq}, g, a};
    ASSERT_NO_THROW(b.evaluate_candidate_first_order(CandidatePoint{x, le, li, 1.0}, out));

    m->drift_jac_e = true;
    expect_refusal([&] { b.evaluate_candidate_first_order(CandidatePoint{x, le, li, 1.0}, out); },
                   {"the model returned eval_jac_e with 1 stored elements",
                    "claim pass laid 2 slots", "sparsity pattern is not invariant"});
}

TEST(NlpModelAssemblyBoundary, AGradientOrResidualBlockOfTheWrongLengthIsRefusedByName) {
    auto m = knobs();
    NlpModelAssembly b(m);
    Vec x = Vec::Zero(2), le = Vec::Zero(1), li = Vec::Zero(1);
    double f = 0.0;
    Vec eq(1), iq(1), g(2), a(2);
    CandidateFirstOrder out{{f, eq, iq}, g, a};

    m->short_gradient = true;
    expect_refusal([&] { b.evaluate_candidate_first_order(CandidatePoint{x, le, li, 1.0}, out); },
                   {"returned eval_grad with 1 rows", "declares 2 of them"});
    m->short_gradient = false;

    m->short_ce = true;
    CandidateValues values{f, eq, iq};
    expect_refusal([&] { b.evaluate_candidate_values(CandidatePoint{x, le, li, 1.0}, values); },
                   {"returned eval_values' cE block with 0 rows", "declares 1 of them"});
}

TEST(NlpModelAssemblyBoundary, CandidateValuesAssignAndScaleTheObjective) {
    NlpModelAssembly b(knobs());
    Vec x(2);
    x << 1.0, 2.0;
    const Vec none(0);
    double f = 99.0;
    Vec eq(1), iq(1);
    CandidateValues values{f, eq, iq};
    b.evaluate_candidate_values(CandidatePoint{x, none, none, 2.0}, values);
    EXPECT_DOUBLE_EQ(f, 2.0 * 2.5);
    EXPECT_DOUBLE_EQ(eq(0), 3.0);
    EXPECT_DOUBLE_EQ(iq(0), 1.0);
    // Assigned, not accumulated: a second call overwrites.
    b.evaluate_candidate_values(CandidatePoint{x, none, none, 1.0}, values);
    EXPECT_DOUBLE_EQ(f, 2.5);
}

TEST(NlpModelAssemblyBoundary, TheIdentityProbeIsStableAndCarriesTheEpoch) {
    NlpModelAssembly b(knobs());
    Vec x(2);
    x << 0.5, -0.25;
    const IdentityProbe first = b.probe_identity(x);
    const IdentityProbe second = b.probe_identity(x);
    EXPECT_EQ(first.epoch, b.structure_epoch());
    EXPECT_EQ(first.value_digest, second.value_digest);
    Vec y = x;
    y(0) += 1.0;
    EXPECT_NE(b.probe_identity(y).value_digest, first.value_digest);
}

} // namespace
} // namespace hven::detail
