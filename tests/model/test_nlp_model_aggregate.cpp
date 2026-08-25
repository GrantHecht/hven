// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The NlpModel bridge: the contract's second provider.
//
// What is pinned here, in the order the file runs: the declaration the bridge
// builds from a model; the claim pass and its layout determinism; the structural
// key and the epoch's live behaviour against a real provider; the capability
// declaration and the routing that has to be true for it; the model evaluators
// each of the eight request shapes runs, exactly and no more; the assembled KKT
// and right-hand-side values against a hand composition of the same model; and
// the boundary refusals a bridge owes on destinations and on a model that breaks
// its own sparsity precondition.
//
// The equivalence pins run against an HS transcription rather than a synthetic
// model, so what they compare is the bridge's decomposition of a model somebody
// else wrote. hs_problems.h is header-only test support (nlp_model.h plus Eigen
// and fmt); it is named rooted at the tests tree, which this target carries on
// its include path.

#include <cstddef>
#include <limits>
#include <memory>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>

#include <gtest/gtest.h>

#include "hven/model/claim_space.h"
#include "hven/model/nlp_aggregate.h"
#include "hven/model/nlp_model.h"
#include "hven/model/nlp_model_aggregate.h"
#include "hven/model/structure_identity.h"

#include "sqp/support/hs_problems.h"

using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using hven::solvers::AggregateCapability;
using hven::solvers::CandidateFirstOrder;
using hven::solvers::CandidatePoint;
using hven::solvers::CandidateValues;
using hven::solvers::ClaimBlock;
using hven::solvers::EvalRequest;
using hven::solvers::has_capability;
using hven::solvers::IdentityProbe;
using hven::solvers::KktLocationTable;
using hven::solvers::KktScatterView;
using hven::solvers::kRequestConstraintJacobiansOnly;
using hven::solvers::kRequestConstraintKkt;
using hven::solvers::kRequestConstraintResidualsAndJacobian;
using hven::solvers::kRequestFirstOrderKkt;
using hven::solvers::kRequestFirstOrderRhs;
using hven::solvers::kRequestFullKkt;
using hven::solvers::kRequestGradientAndJacobians;
using hven::solvers::kRequestLagrangianHessian;
using hven::solvers::kRequestObjectiveAndConstraints;
using hven::solvers::kRequestObjectiveGradientAndConstraints;
using hven::solvers::kRequestObjectiveOnly;
using hven::solvers::NlpModel;
using hven::solvers::NlpModelAggregate;
using hven::solvers::RhsArenaView;
using hven::solvers::RhsLocationTable;
using hven::solvers::RhsScatterView;
using hven::solvers::StructureEpoch;

namespace {

// ---------------------------------------------------------------------------
// A model that counts what is asked of it
// ---------------------------------------------------------------------------

/// How many times each evaluator ran.
struct BridgeEvalCounts {
    int f_ = 0;
    int grad_ = 0;
    int ce_ = 0;
    int ci_ = 0;
    int jac_e_ = 0;
    int jac_i_ = 0;
    int hess_ = 0;
    int values_ = 0;

    friend bool operator==(const BridgeEvalCounts &, const BridgeEvalCounts &) = default;

    friend std::ostream &operator<<(std::ostream &out, const BridgeEvalCounts &counts) {
        return out << "{f=" << counts.f_ << " grad=" << counts.grad_ << " ce=" << counts.ce_
                   << " ci=" << counts.ci_ << " jac_e=" << counts.jac_e_
                   << " jac_i=" << counts.jac_i_ << " hess=" << counts.hess_
                   << " values=" << counts.values_ << "}";
    }
};

/// Elementwise comparison of two claim streams. Eigen's own operator== is
/// coefficient-wise, so a claim-stream equality is spelled out here once.
bool same_claim_stream(Eigen::Ref<const Eigen::VectorXi> left,
                       Eigen::Ref<const Eigen::VectorXi> right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (Eigen::Index slot = 0; slot < left.size(); ++slot) {
        if (left[slot] != right[slot]) {
            return false;
        }
    }
    return true;
}

/// A two-variable model with one equality and one inequality row, counting every
/// evaluator it is asked for.
///
/// Its eval_values override computes f, cE and cI directly rather than calling
/// eval_f/eval_ce/eval_ci, which is what the model contract permits an override
/// to do and what makes every count here unambiguous: a values evaluation that
/// went through the three separate evaluators would show up as three counts
/// instead of one.
///
///   f(x)  = x0^2 + 2 x1^2 + x0 x1
///   cE(x) = x0 + x1 - 1
///   cI(x) = x0^2 + x1 - 2
class BridgeCountingModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override {
        counts_.f_++;
        return objective_of(x);
    }

    Vec eval_grad(const Vec &x) const override {
        counts_.grad_++;
        return (Vec(2) << 2.0 * x(0) + x(1), 4.0 * x(1) + x(0)).finished();
    }

    Vec eval_ce(const Vec &x) const override {
        counts_.ce_++;
        return equality_of(x);
    }

    Vec eval_ci(const Vec &x) const override {
        counts_.ci_++;
        return inequality_of(x);
    }

    void eval_values(const Vec &x, double &f, Vec &cE, Vec &cI) const override {
        counts_.values_++;
        f = objective_of(x);
        cE = equality_of(x);
        cI = inequality_of(x);
    }

    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &lambda_e,
                      const Vec &lambda_i) const override {
        counts_.hess_++;
        last_hess_scale_ = obj_scale;
        last_hess_lambda_e_ = lambda_e;
        last_hess_lambda_i_ = lambda_i;
        const double li = lambda_i.size() > 0 ? lambda_i(0) : 0.0;
        // Every structural entry every call: the objective's three, plus the
        // inequality's (0, 0) contribution folded onto the same slot. The
        // equality is linear and contributes nothing but is not a slot of its
        // own either.
        return hven::solvers::test_support::detail::make_upper(
            2,
            {{0, 0, 2.0 * obj_scale + 2.0 * li}, {0, 1, 1.0 * obj_scale}, {1, 1, 4.0 * obj_scale}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        counts_.jac_e_++;
        return hven::solvers::test_support::detail::make_jac(1, 2, {{0, 0, 1.0}, {0, 1, 1.0}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        counts_.jac_i_++;
        return hven::solvers::test_support::detail::make_jac(1, 2,
                                                             {{0, 0, 2.0 * x(0)}, {0, 1, 1.0}});
    }

    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return (Vec(2) << 0.5, 0.25).finished(); }

    // ---- test hooks -------------------------------------------------------

    const BridgeEvalCounts &counts() const { return counts_; }
    void reset_counts() const { counts_ = BridgeEvalCounts{}; }
    double last_hess_scale() const { return last_hess_scale_; }
    const Vec &last_hess_lambda_e() const { return last_hess_lambda_e_; }
    const Vec &last_hess_lambda_i() const { return last_hess_lambda_i_; }

    static double objective_of(const Vec &x) {
        return x(0) * x(0) + 2.0 * x(1) * x(1) + x(0) * x(1);
    }
    static Vec equality_of(const Vec &x) {
        Vec c(1);
        c(0) = x(0) + x(1) - 1.0;
        return c;
    }
    static Vec inequality_of(const Vec &x) {
        Vec c(1);
        c(0) = x(0) * x(0) + x(1) - 2.0;
        return c;
    }

  protected:
    /// Derived fixtures override an evaluator and count it the same way.
    mutable BridgeEvalCounts counts_;

  private:
    Vec lower_ = (Vec(2) << 0.0, -1.0).finished();
    Vec upper_ = (Vec(2) << 10.0, 10.0).finished();

    mutable double last_hess_scale_ = -1.0;
    mutable Vec last_hess_lambda_e_;
    mutable Vec last_hess_lambda_i_;
};

/// A model whose equality row is a constant: it has one row, and its Jacobian is
/// all structural zeros, so the claim pass lays no claim for it. The evaluator
/// must still be called on a Jacobian-bearing request -- the row count decides
/// that, not the claim count.
///
///   cE(x) = 0.5, Je = the 1 x 2 empty pattern
class BridgeConstantEqualityModel : public BridgeCountingModel {
  public:
    Vec eval_ce(const Vec &) const override {
        counts_.ce_++;
        return constant_equality();
    }

    void eval_values(const Vec &x, double &f, Vec &cE, Vec &cI) const override {
        counts_.values_++;
        f = objective_of(x);
        cE = constant_equality();
        cI = inequality_of(x);
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        counts_.jac_e_++;
        return hven::solvers::test_support::detail::no_jac(1, 2);
    }

    static Vec constant_equality() {
        Vec c(1);
        c(0) = 0.5;
        return c;
    }
};

/// A model whose Jacobian pattern collapses at one point: the entry that
/// evaluates to zero is dropped instead of being emitted as a structural zero.
/// That is exactly what nlp_model.h forbids, and the bridge has to say so rather
/// than scatter through the wrong slots.
class BridgePatternDriftModel : public BridgeCountingModel {
  public:
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        if (x(0) == 0.0) {
            return hven::solvers::test_support::detail::make_jac(1, 2, {{0, 1, 1.0}});
        }
        return BridgeCountingModel::eval_jac_i(x);
    }
};

/// A model whose Jacobian pattern grows after the claim pass: it emits one entry
/// at the start point, where the claim pass reads it, and two everywhere else.
/// The other direction of the same violation the drift model exercises, and the
/// one a collapse-only pin would miss.
class BridgePatternGrowthModel : public BridgeCountingModel {
  public:
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        if (x(0) == 0.5) { // the start point: one entry claimed
            counts_.jac_i_++;
            return hven::solvers::test_support::detail::make_jac(1, 2, {{0, 1, 1.0}});
        }
        return BridgeCountingModel::eval_jac_i(x);
    }
};

/// A model that refuses to be evaluated once armed. Used to reach a lay that
/// fails after the structures on hand were already laid once.
class BridgeRefusingModel : public BridgeCountingModel {
  public:
    void arm() { armed_ = true; }

    SpMatRM eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                      const Vec &lambda_i) const override {
        if (armed_) {
            throw std::runtime_error("BridgeRefusingModel: armed");
        }
        return BridgeCountingModel::eval_hess(x, obj_scale, lambda_e, lambda_i);
    }

  private:
    bool armed_ = false;
};

// ---------------------------------------------------------------------------
// The consumer side: storage and the tables that address it
// ---------------------------------------------------------------------------

/// One arena's consumer-owned storage plus the identity table addressing it.
class BridgeArena {
  public:
    void lay(int claims) {
        rows_.resize(claims);
        for (int slot = 0; slot < claims; ++slot) {
            rows_[static_cast<std::size_t>(slot)] = slot;
        }
        values_.assign(static_cast<std::size_t>(claims), 0.0);
        table_ = RhsLocationTable(rows_.data(), claims);
    }

    RhsArenaView view() {
        if (values_.empty()) {
            return RhsArenaView{};
        }
        return RhsArenaView{values_.data(), static_cast<int>(values_.size()), &table_};
    }

    void zero() { values_.assign(values_.size(), 0.0); }
    const std::vector<double> &values() const { return values_; }

  private:
    std::vector<int> rows_;
    std::vector<double> values_;
    RhsLocationTable table_;
};

/// Which order a consumer's KKT location table puts the claims in.
///
/// The default is a genuine permutation, deliberately: with an identity table a
/// scatter that ignored the table and wrote its own slot index would be
/// indistinguishable from one that honoured it. Reversal is bijective, so no two
/// claims collide, and every assertion below reads a value back through the same
/// table the provider wrote it through.
enum class BridgeTableOrder { kPermuted, kIdentity };

/// Everything a consumer publishes for one assemble call against the bridge: the
/// KKT value array with its location table over the provider's claim stream, the
/// four arenas, and the scalar objective slot.
class BridgeDestinations {
  public:
    explicit BridgeDestinations(const NlpModelAggregate &aggregate,
                                BridgeTableOrder order = BridgeTableOrder::kPermuted) {
        const int claims = static_cast<int>(aggregate.kkt_claim_rows().size());
        kkt_locations_.resize(static_cast<std::size_t>(claims));
        for (int slot = 0; slot < claims; ++slot) {
            kkt_locations_[static_cast<std::size_t>(slot)] =
                order == BridgeTableOrder::kIdentity ? slot : claims - 1 - slot;
        }
        kkt_values_.assign(static_cast<std::size_t>(claims), 0.0);
        kkt_clashes_.assign(static_cast<std::size_t>(aggregate.kkt_dimension()), -1);
        kkt_table_ = KktLocationTable(kkt_locations_.data(), claims, kkt_clashes_.data(),
                                      static_cast<int>(kkt_clashes_.size()), &kkt_locks_);

        objective_gradient_.lay(static_cast<int>(aggregate.objective_gradient_claim_rows().size()));
        adjoint_gradient_.lay(
            static_cast<int>(aggregate.constraint_adjoint_gradient_claim_rows().size()));
        equality_residuals_.lay(static_cast<int>(aggregate.equality_residual_claim_rows().size()));
        inequality_residuals_.lay(
            static_cast<int>(aggregate.inequality_residual_claim_rows().size()));
    }

    BridgeDestinations(const BridgeDestinations &) = delete;
    BridgeDestinations &operator=(const BridgeDestinations &) = delete;

    KktScatterView kkt_view() {
        if (kkt_values_.empty()) {
            return KktScatterView{};
        }
        return KktScatterView{kkt_values_.data(), static_cast<int>(kkt_values_.size()),
                              &kkt_table_};
    }

    RhsScatterView rhs_view() {
        return RhsScatterView{objective_gradient_.view(), adjoint_gradient_.view(),
                              equality_residuals_.view(), inequality_residuals_.view(),
                              &objective_};
    }

    void zero() {
        kkt_values_.assign(kkt_values_.size(), 0.0);
        objective_ = 0.0;
        objective_gradient_.zero();
        adjoint_gradient_.zero();
        equality_residuals_.zero();
        inequality_residuals_.zero();
    }

    /// The assembled KKT values placed back at the coordinates the claims name.
    Eigen::MatrixXd dense_kkt(const NlpModelAggregate &aggregate) const {
        const int dim = aggregate.kkt_dimension();
        Eigen::MatrixXd dense = Eigen::MatrixXd::Zero(dim, dim);
        const auto rows = aggregate.kkt_claim_rows();
        const auto cols = aggregate.kkt_claim_cols();
        for (Eigen::Index slot = 0; slot < rows.size(); ++slot) {
            // Read back through the table the provider wrote through, so the
            // assertion is blind to which order the consumer chose.
            const std::size_t offset =
                static_cast<std::size_t>(kkt_locations_[static_cast<std::size_t>(slot)]);
            dense(rows[slot], cols[slot]) += kkt_values_[offset];
        }
        return dense;
    }

    double objective_ = 0.0;
    BridgeArena objective_gradient_;
    BridgeArena adjoint_gradient_;
    BridgeArena equality_residuals_;
    BridgeArena inequality_residuals_;
    std::vector<double> kkt_values_;

    /// Where claim slot @p slot was told to land.
    int kkt_location(int slot) const { return kkt_locations_[static_cast<std::size_t>(slot)]; }

  private:
    std::vector<int> kkt_locations_;
    std::vector<int> kkt_clashes_;
    std::vector<std::mutex> kkt_locks_;
    KktLocationTable kkt_table_;
};

/// The point every evaluation below runs at, with full multiplier blocks.
struct BridgePoint {
    Vec x_ = (Vec(2) << 0.75, -0.5).finished();
    Vec equality_ = (Vec(1) << 1.5).finished();
    Vec inequality_ = (Vec(1) << 0.25).finished();
    Vec empty_;

    CandidatePoint full(double scale = 1.0) const {
        return CandidatePoint{x_, equality_, inequality_, scale};
    }
    CandidatePoint values_only(double scale = 1.0) const {
        return CandidatePoint{x_, empty_, empty_, scale};
    }
};

std::shared_ptr<BridgeCountingModel> counting_model() {
    return std::make_shared<BridgeCountingModel>();
}

} // namespace

// ---------------------------------------------------------------------------
// The declaration the bridge builds
// ---------------------------------------------------------------------------

TEST(NlpModelAggregateDeclaration, CarriesTheModelsDimensionsAndOnePartition) {
    NlpModelAggregate aggregate(counting_model());
    const auto &declared = aggregate.declaration();

    EXPECT_EQ(declared.primal_vars_, 2);
    EXPECT_EQ(declared.equality_rows_, 1);
    EXPECT_EQ(declared.inequality_rows_, 1);
    EXPECT_EQ(declared.partition_count_, 1);
}

TEST(NlpModelAggregateDeclaration, RecordsOneBoundRecordPerVariableVerbatim) {
    NlpModelAggregate aggregate(counting_model());
    const auto &records = aggregate.declaration().variable_bounds_;

    ASSERT_EQ(records.size(), 2u);
    EXPECT_EQ(records[0].index_, 0);
    EXPECT_DOUBLE_EQ(records[0].lower_, 0.0);
    EXPECT_DOUBLE_EQ(records[0].upper_, 10.0);
    EXPECT_EQ(records[1].index_, 1);
    EXPECT_DOUBLE_EQ(records[1].lower_, -1.0);
    EXPECT_DOUBLE_EQ(records[1].upper_, 10.0);
}

TEST(NlpModelAggregateDeclaration, CarriesNoEnginePieces) {
    // The piece lists hold the partitioned engine's own handles. One serial
    // bridge is not a collection of them, and the declaration says so rather
    // than inventing a piece to fill the list with.
    NlpModelAggregate aggregate(counting_model());
    EXPECT_TRUE(aggregate.declaration().objectives_.empty());
    EXPECT_TRUE(aggregate.declaration().equality_constraints_.empty());
    EXPECT_TRUE(aggregate.declaration().inequality_constraints_.empty());
}

TEST(NlpModelAggregateDeclaration, PassesTheContractsOwnValidation) {
    // A constrained bridge declares rows of both kinds and no pieces. The
    // boundary is universal: the piece-sum conjunct is vacuous with empty piece
    // lists, and every other check applies to this declaration as to any other.
    NlpModelAggregate aggregate(counting_model());
    EXPECT_GT(aggregate.declaration().equality_rows_, 0);
    EXPECT_GT(aggregate.declaration().inequality_rows_, 0);
    EXPECT_NO_THROW(aggregate.declaration().validate());
}

TEST(NlpModelAggregateDeclaration, RefusesANullModel) {
    EXPECT_THROW({ NlpModelAggregate bridge(nullptr); }, std::invalid_argument);
}

// ---------------------------------------------------------------------------
// The claim pass
// ---------------------------------------------------------------------------

TEST(NlpModelAggregateClaims, SplitsTheModelsReturnsIntoThreeBlocks) {
    NlpModelAggregate aggregate(counting_model());

    // Three Hessian entries, two per Jacobian row.
    EXPECT_EQ(aggregate.hessian_claims(), (ClaimBlock{0, 3}));
    EXPECT_EQ(aggregate.equality_jacobian_claims(), (ClaimBlock{3, 2}));
    EXPECT_EQ(aggregate.inequality_jacobian_claims(), (ClaimBlock{5, 2}));
    EXPECT_EQ(aggregate.kkt_claim_rows().size(), 7);
    EXPECT_EQ(aggregate.kkt_dimension(), 4);
}

TEST(NlpModelAggregateClaims, NamesTheUpperTriangleAndTheOffsetConstraintRows) {
    NlpModelAggregate aggregate(counting_model());
    const auto rows = aggregate.kkt_claim_rows();
    const auto cols = aggregate.kkt_claim_cols();

    // Hessian: (0,0), (0,1), (1,1) -- the triangle the model returns, at
    // variable coordinates.
    EXPECT_EQ(rows[0], 0);
    EXPECT_EQ(cols[0], 0);
    EXPECT_EQ(rows[1], 0);
    EXPECT_EQ(cols[1], 1);
    EXPECT_EQ(rows[2], 1);
    EXPECT_EQ(cols[2], 1);
    // Equality Jacobian row 0 lands at n; inequality at n + me.
    EXPECT_EQ(rows[3], 2);
    EXPECT_EQ(rows[4], 2);
    EXPECT_EQ(rows[5], 3);
    EXPECT_EQ(rows[6], 3);
    for (int slot = 3; slot < 7; ++slot) {
        EXPECT_LT(cols[slot], 2) << "a Jacobian claim names a variable column";
    }
}

TEST(NlpModelAggregateClaims, ArenasAreClaimedWholeInRowOrder) {
    NlpModelAggregate aggregate(counting_model());

    EXPECT_EQ(aggregate.objective_gradient_claim_rows().size(), 2);
    EXPECT_EQ(aggregate.constraint_adjoint_gradient_claim_rows().size(), 2);
    EXPECT_EQ(aggregate.equality_residual_claim_rows().size(), 1);
    EXPECT_EQ(aggregate.inequality_residual_claim_rows().size(), 1);
    EXPECT_EQ(aggregate.objective_gradient_claim_rows()[0], 0);
    EXPECT_EQ(aggregate.objective_gradient_claim_rows()[1], 1);
}

TEST(NlpModelAggregateClaims, LayoutIsDeterministicAcrossSeparateBridges) {
    NlpModelAggregate first(counting_model());
    NlpModelAggregate second(counting_model());

    EXPECT_TRUE(same_claim_stream(first.kkt_claim_rows(), second.kkt_claim_rows()));
    EXPECT_TRUE(same_claim_stream(first.kkt_claim_cols(), second.kkt_claim_cols()));
    EXPECT_EQ(first.model_structure_key(), second.model_structure_key());
}

// ---------------------------------------------------------------------------
// The structural key and the epoch
// ---------------------------------------------------------------------------

TEST(NlpModelAggregateKey, IsTheThreeConjunctsAtOnePartition) {
    NlpModelAggregate aggregate(counting_model());
    const auto key = aggregate.model_structure_key();

    EXPECT_EQ(key.partition_count_, 1);
    EXPECT_EQ(key.claim_digest_, hven::solvers::claim_stream_digest(aggregate.declaration(),
                                                                    aggregate.kkt_claim_rows(),
                                                                    aggregate.kkt_claim_cols()));
    EXPECT_EQ(key.bound_digest_, hven::solvers::materialized_bound_digest(aggregate.declaration()));
}

TEST(NlpModelAggregateKey, DiffersFromAModelWithDifferentBoundStructure) {
    class FreeBoundsModel : public BridgeCountingModel {
      public:
        const Vec &lower() const override { return free_; }
        const Vec &upper() const override { return free_upper_; }

      private:
        Vec free_ = Vec::Constant(2, -std::numeric_limits<double>::infinity());
        Vec free_upper_ = Vec::Constant(2, std::numeric_limits<double>::infinity());
    };

    NlpModelAggregate bounded(counting_model());
    NlpModelAggregate unbounded(std::make_shared<FreeBoundsModel>());

    EXPECT_EQ(bounded.model_structure_key().claim_digest_,
              unbounded.model_structure_key().claim_digest_);
    EXPECT_NE(bounded.model_structure_key().bound_digest_,
              unbounded.model_structure_key().bound_digest_);
    EXPECT_FALSE(bounded.model_structure_key() == unbounded.model_structure_key());
}

TEST(NlpModelAggregateEpoch, TheFirstLayIsAStructuralEvent) {
    NlpModelAggregate aggregate(counting_model());
    EXPECT_EQ(aggregate.structure_epoch(), StructureEpoch(1));
}

TEST(NlpModelAggregateEpoch, ARenegotiationBumpsEvenAtTheCountAlreadyInForce) {
    NlpModelAggregate aggregate(counting_model());
    EXPECT_EQ(aggregate.negotiate_partition_count(1), 1);
    EXPECT_EQ(aggregate.structure_epoch(), StructureEpoch(2));
    EXPECT_EQ(aggregate.negotiate_partition_count(8), 1);
    EXPECT_EQ(aggregate.structure_epoch(), StructureEpoch(3));
}

TEST(NlpModelAggregateEpoch, EvaluationDoesNotBump) {
    NlpModelAggregate aggregate(counting_model());
    BridgeDestinations destinations(aggregate);
    const BridgePoint point;

    const StructureEpoch before = aggregate.structure_epoch();
    aggregate.assemble(point.full(), kRequestFullKkt, destinations.kkt_view(),
                       destinations.rhs_view());
    aggregate.probe_identity(point.x_);
    EXPECT_EQ(aggregate.structure_epoch(), before);
}

TEST(NlpModelAggregateEpoch, ARefusedPartitionRequestNeitherLaysNorBumps) {
    NlpModelAggregate aggregate(counting_model());
    const StructureEpoch before = aggregate.structure_epoch();
    const auto key_before = aggregate.model_structure_key();

    EXPECT_THROW(aggregate.negotiate_partition_count(0), std::invalid_argument);
    EXPECT_THROW(aggregate.negotiate_partition_count(-3), std::invalid_argument);
    EXPECT_EQ(aggregate.structure_epoch(), before);
    EXPECT_EQ(aggregate.model_structure_key(), key_before);
}

TEST(NlpModelAggregateEpoch, AFailedLayLeavesTheStructuresOnHandUntouched) {
    // The failure-restore rule's antecedent is never reached here, and that is
    // the strong form of honouring it: a lay is built whole and committed whole,
    // so a lay that throws leaves the previous structures in place. Nothing was
    // re-laid, so there is nothing to restore and no epoch event to report.
    auto model = std::make_shared<BridgeRefusingModel>();
    NlpModelAggregate aggregate(model);
    const StructureEpoch before = aggregate.structure_epoch();
    const auto key_before = aggregate.model_structure_key();
    const Eigen::VectorXi claims_before = aggregate.kkt_claim_rows();

    model->arm();
    EXPECT_THROW(aggregate.negotiate_partition_count(1), std::runtime_error);

    EXPECT_EQ(aggregate.structure_epoch(), before);
    EXPECT_EQ(aggregate.model_structure_key(), key_before);
    EXPECT_TRUE(same_claim_stream(aggregate.kkt_claim_rows(), claims_before));
}

TEST(NlpModelAggregatePartitions, CapsEveryPositiveRequestToOne) {
    NlpModelAggregate aggregate(counting_model());
    EXPECT_EQ(aggregate.negotiate_partition_count(1), 1);
    EXPECT_EQ(aggregate.negotiate_partition_count(2), 1);
    EXPECT_EQ(aggregate.negotiate_partition_count(64), 1);
    EXPECT_EQ(aggregate.model_structure_key().partition_count_, 1);
    EXPECT_EQ(aggregate.declaration().partition_count_, 1);
}

TEST(NlpModelAggregateThreads, ReportsOneAndRefusesANonPositiveRequest) {
    NlpModelAggregate aggregate(counting_model());
    EXPECT_EQ(aggregate.evaluation_threads(), 1);
    aggregate.set_evaluation_threads(4);
    EXPECT_EQ(aggregate.evaluation_threads(), 1);
    EXPECT_THROW(aggregate.set_evaluation_threads(0), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// The capability declaration, and the routing that has to hold for it
// ---------------------------------------------------------------------------

TEST(NlpModelAggregateCapabilities, DeclaresTheValuesFastPathAndNothingElse) {
    NlpModelAggregate aggregate(counting_model());
    EXPECT_TRUE(has_capability(aggregate.capabilities(), AggregateCapability::kValuesFastPath));
    EXPECT_FALSE(has_capability(aggregate.capabilities(), AggregateCapability::kDirectScatter));
    EXPECT_EQ(aggregate.capabilities(), AggregateCapability::kValuesFastPath);
}

TEST(NlpModelAggregateCapabilities, BindsNoKktDestination) {
    NlpModelAggregate aggregate(counting_model());
    EXPECT_EQ(aggregate.bound_kkt_destination(), nullptr);
}

TEST(NlpModelAggregateValuesPath, ReachesEvalValuesAndNoDerivativeEvaluator) {
    auto model = counting_model();
    NlpModelAggregate aggregate(model);
    const BridgePoint point;
    double objective = 0.0;
    Vec equality(1);
    Vec inequality(1);

    model->reset_counts();
    aggregate.evaluate_candidate_values(point.values_only(),
                                        CandidateValues{objective, equality, inequality});

    BridgeEvalCounts expected;
    expected.values_ = 1;
    EXPECT_EQ(model->counts(), expected);
}

TEST(NlpModelAggregateValuesPath, TheProbeIsTheValuesPathPlusAHash) {
    auto model = counting_model();
    NlpModelAggregate aggregate(model);
    const BridgePoint point;

    model->reset_counts();
    const IdentityProbe probe = aggregate.probe_identity(point.x_);

    BridgeEvalCounts expected;
    expected.values_ = 1;
    EXPECT_EQ(model->counts(), expected);
    EXPECT_EQ(probe.epoch_, aggregate.structure_epoch());
    EXPECT_EQ(probe, aggregate.probe_identity(point.x_));
}

TEST(NlpModelAggregateValuesPath, ReportsTheModelsValuesScaledByThePointsScale) {
    NlpModelAggregate aggregate(counting_model());
    const BridgePoint point;
    double objective = 0.0;
    Vec equality(1);
    Vec inequality(1);

    aggregate.evaluate_candidate_values(point.values_only(2.5),
                                        CandidateValues{objective, equality, inequality});

    EXPECT_DOUBLE_EQ(objective, 2.5 * BridgeCountingModel::objective_of(point.x_));
    EXPECT_DOUBLE_EQ(equality(0), BridgeCountingModel::equality_of(point.x_)(0));
    EXPECT_DOUBLE_EQ(inequality(0), BridgeCountingModel::inequality_of(point.x_)(0));
}

TEST(NlpModelAggregateValuesPath, TwiceAtOnePointAssignsRatherThanDoubles) {
    NlpModelAggregate aggregate(counting_model());
    const BridgePoint point;
    double objective = 0.0;
    Vec equality(1);
    Vec inequality(1);
    const CandidateValues out{objective, equality, inequality};

    aggregate.evaluate_candidate_values(point.values_only(), out);
    const double once = objective;
    const double equality_once = equality(0);
    aggregate.evaluate_candidate_values(point.values_only(), out);

    EXPECT_DOUBLE_EQ(objective, once);
    EXPECT_DOUBLE_EQ(equality(0), equality_once);
}

TEST(NlpModelAggregateFirstOrder, TwiceAtOnePointAssignsRatherThanDoubles) {
    NlpModelAggregate aggregate(counting_model());
    const BridgePoint point;
    double objective = 0.0;
    Vec equality(1);
    Vec inequality(1);
    Vec objective_gradient(2);
    Vec adjoint_gradient(2);
    const CandidateFirstOrder out{CandidateValues{objective, equality, inequality},
                                  objective_gradient, adjoint_gradient};

    aggregate.evaluate_candidate_first_order(point.full(), out);
    const Vec gradient_once = objective_gradient;
    const Vec adjoint_once = adjoint_gradient;
    aggregate.evaluate_candidate_first_order(point.full(), out);

    EXPECT_TRUE(objective_gradient.isApprox(gradient_once));
    EXPECT_TRUE(adjoint_gradient.isApprox(adjoint_once));
}

TEST(NlpModelAggregateFirstOrder, ComposesTheAdjointGradientFromBothJacobians) {
    NlpModelAggregate aggregate(counting_model());
    const BridgePoint point;
    double objective = 0.0;
    Vec equality(1);
    Vec inequality(1);
    Vec objective_gradient(2);
    Vec adjoint_gradient(2);

    aggregate.evaluate_candidate_first_order(
        point.full(), CandidateFirstOrder{CandidateValues{objective, equality, inequality},
                                          objective_gradient, adjoint_gradient});

    // Je^T lambda_e + Ji^T lambda_i, by hand: Je = [1, 1], Ji = [2 x0, 1].
    const double le = point.equality_(0);
    const double li = point.inequality_(0);
    EXPECT_DOUBLE_EQ(adjoint_gradient(0), 1.0 * le + 2.0 * point.x_(0) * li);
    EXPECT_DOUBLE_EQ(adjoint_gradient(1), 1.0 * le + 1.0 * li);
}

// ---------------------------------------------------------------------------
// The evaluator set each request shape runs
// ---------------------------------------------------------------------------

namespace {

BridgeEvalCounts counts_for(EvalRequest request, bool full_multipliers, double scale = 1.0) {
    auto model = counting_model();
    NlpModelAggregate aggregate(model);
    BridgeDestinations destinations(aggregate);
    const BridgePoint point;

    model->reset_counts();
    aggregate.assemble(full_multipliers ? point.full(scale) : point.values_only(scale), request,
                       destinations.kkt_view(), destinations.rhs_view());
    return model->counts();
}

} // namespace

TEST(NlpModelAggregateEvaluatorSets, ObjectiveOnlyRunsEvalFAlone) {
    BridgeEvalCounts expected;
    expected.f_ = 1;
    EXPECT_EQ(counts_for(kRequestObjectiveOnly, false), expected);
}

TEST(NlpModelAggregateEvaluatorSets, ObjectiveAndConstraintsRunsEvalValuesAlone) {
    BridgeEvalCounts expected;
    expected.values_ = 1;
    EXPECT_EQ(counts_for(kRequestObjectiveAndConstraints, false), expected);
}

TEST(NlpModelAggregateEvaluatorSets, ObjectiveGradientAndConstraintsAddsEvalGrad) {
    BridgeEvalCounts expected;
    expected.values_ = 1;
    expected.grad_ = 1;
    EXPECT_EQ(counts_for(kRequestObjectiveGradientAndConstraints, false), expected);
}

TEST(NlpModelAggregateEvaluatorSets, FirstOrderRhsAddsBothJacobiansAndNoHessian) {
    BridgeEvalCounts expected;
    expected.values_ = 1;
    expected.grad_ = 1;
    expected.jac_e_ = 1;
    expected.jac_i_ = 1;
    EXPECT_EQ(counts_for(kRequestFirstOrderRhs, true), expected);
}

TEST(NlpModelAggregateEvaluatorSets, ConstraintResidualsAndJacobianSkipsTheObjectiveEvaluators) {
    // The shape names no objective output, so eval_values is the wrong call: it
    // would compute f for a request that never asked for it.
    BridgeEvalCounts expected;
    expected.ce_ = 1;
    expected.ci_ = 1;
    expected.jac_e_ = 1;
    expected.jac_i_ = 1;
    EXPECT_EQ(counts_for(kRequestConstraintResidualsAndJacobian, false), expected);
}

TEST(NlpModelAggregateEvaluatorSets, FirstOrderKktRunsTheSameSetAsTheFirstOrderRhs) {
    BridgeEvalCounts expected;
    expected.values_ = 1;
    expected.grad_ = 1;
    expected.jac_e_ = 1;
    expected.jac_i_ = 1;
    EXPECT_EQ(counts_for(kRequestFirstOrderKkt, true), expected);
}

TEST(NlpModelAggregateEvaluatorSets, ConstraintKktAddsTheHessianAndStillNoObjective) {
    BridgeEvalCounts expected;
    expected.ce_ = 1;
    expected.ci_ = 1;
    expected.jac_e_ = 1;
    expected.jac_i_ = 1;
    expected.hess_ = 1;
    EXPECT_EQ(counts_for(kRequestConstraintKkt, true), expected);
}

TEST(NlpModelAggregateEvaluatorSets, FullKktRunsEveryEvaluatorExactlyOnce) {
    BridgeEvalCounts expected;
    expected.values_ = 1;
    expected.grad_ = 1;
    expected.jac_e_ = 1;
    expected.jac_i_ = 1;
    expected.hess_ = 1;
    EXPECT_EQ(counts_for(kRequestFullKkt, true), expected);
}

// The three SQP-owned shapes (rows 9-11): the model-call bill each one
// reproduces, pinned exactly rather than at-most. Full struct equality on
// BridgeEvalCounts is itself the falsification device -- if shape 9 also
// called eval_f, expected.f_ (0) would disagree with the real count and the
// EXPECT_EQ below would fail; the explicit zero checks make that property
// visible without relying on the reader to notice what the struct omits.

TEST(NlpModelAggregateEvaluatorSets, LagrangianHessianRunsEvalHessAloneAndNothingElse) {
    auto model = counting_model();
    NlpModelAggregate aggregate(model);
    BridgeDestinations destinations(aggregate);
    const BridgePoint point;

    model->reset_counts();
    aggregate.assemble(point.full(), kRequestLagrangianHessian, destinations.kkt_view(),
                       destinations.rhs_view());

    BridgeEvalCounts expected;
    expected.hess_ = 1;
    EXPECT_EQ(model->counts(), expected);
    EXPECT_EQ(model->counts().f_, 0) << "shape 9 must not evaluate the objective";
}

TEST(NlpModelAggregateEvaluatorSets, GradientAndJacobiansRunsGradPlusBothJacobiansAndNoValues) {
    auto model = counting_model();
    NlpModelAggregate aggregate(model);
    BridgeDestinations destinations(aggregate);
    const BridgePoint point;

    model->reset_counts();
    aggregate.assemble(point.values_only(), kRequestGradientAndJacobians, destinations.kkt_view(),
                       destinations.rhs_view());

    BridgeEvalCounts expected;
    expected.grad_ = 1;
    expected.jac_e_ = 1;
    expected.jac_i_ = 1;
    EXPECT_EQ(model->counts(), expected);
    EXPECT_EQ(model->counts().values_, 0)
        << "shape 10 must not re-evaluate the values its consumer already holds";
}

TEST(NlpModelAggregateEvaluatorSets, ConstraintJacobiansOnlyRunsBothJacobiansAloneAndNoGradient) {
    auto model = counting_model();
    NlpModelAggregate aggregate(model);
    BridgeDestinations destinations(aggregate);
    const BridgePoint point;

    model->reset_counts();
    aggregate.assemble(point.values_only(), kRequestConstraintJacobiansOnly,
                       destinations.kkt_view(), destinations.rhs_view());

    BridgeEvalCounts expected;
    expected.jac_e_ = 1;
    expected.jac_i_ = 1;
    EXPECT_EQ(model->counts(), expected);
    EXPECT_EQ(model->counts().grad_, 0) << "shape 11 names no objective output at all";
}

TEST(NlpModelAggregateEvaluatorSets, AConstantConstraintBlockStillReachesItsJacobianEvaluator) {
    // The equality row is a constant, so its Jacobian is all structural zeros
    // and the claim pass lays no claim for it. The row count is what decides
    // whether the callback runs, and it must: the request named a Jacobian.
    auto model = std::make_shared<BridgeConstantEqualityModel>();
    NlpModelAggregate aggregate(model);
    ASSERT_EQ(aggregate.equality_jacobian_claims().count_, 0)
        << "the fixture must claim nothing for its constant block";
    ASSERT_EQ(aggregate.declaration().equality_rows_, 1);

    BridgeDestinations destinations(aggregate);
    const BridgePoint point;
    model->reset_counts();
    aggregate.assemble(point.full(), kRequestFullKkt, destinations.kkt_view(),
                       destinations.rhs_view());

    BridgeEvalCounts expected;
    expected.values_ = 1;
    expected.grad_ = 1;
    expected.jac_e_ = 1;
    expected.jac_i_ = 1;
    expected.hess_ = 1;
    EXPECT_EQ(model->counts(), expected);

    // The block that claimed nothing scatters nothing, and its residual row is
    // still filled: claiming no Jacobian slot is not the same as having no row.
    EXPECT_DOUBLE_EQ(destinations.equality_residuals_.values()[0],
                     BridgeConstantEqualityModel::constant_equality()(0));
}

TEST(NlpModelAggregateEvaluatorSets, TheConstraintOnlyKktAsksForTheHessianAtZeroObjectiveScale) {
    auto model = counting_model();
    NlpModelAggregate aggregate(model);
    BridgeDestinations destinations(aggregate);
    const BridgePoint point;

    aggregate.assemble(point.full(3.0), kRequestConstraintKkt, destinations.kkt_view(),
                       destinations.rhs_view());

    EXPECT_DOUBLE_EQ(model->last_hess_scale(), 0.0);
    EXPECT_DOUBLE_EQ(model->last_hess_lambda_e()(0), point.equality_(0));
    EXPECT_DOUBLE_EQ(model->last_hess_lambda_i()(0), point.inequality_(0));
}

TEST(NlpModelAggregateEvaluatorSets, TheFullKktAsksForTheHessianAtThePointsObjectiveScale) {
    auto model = counting_model();
    NlpModelAggregate aggregate(model);
    BridgeDestinations destinations(aggregate);
    const BridgePoint point;

    aggregate.assemble(point.full(3.0), kRequestFullKkt, destinations.kkt_view(),
                       destinations.rhs_view());

    EXPECT_DOUBLE_EQ(model->last_hess_scale(), 3.0);
}

// ---------------------------------------------------------------------------
// The assembled values, against a hand composition of the same model
// ---------------------------------------------------------------------------

namespace {

/// The KKT system the bridge is supposed to assemble for @p model at @p x, built
/// independently of the bridge: the Hessian's upper triangle at variable
/// coordinates, the equality Jacobian at rows [n, n + me), the inequality
/// Jacobian at rows [n + me, n + me + mi).
Eigen::MatrixXd hand_composed_kkt(const NlpModel &model, const Vec &x, double scale, const Vec &le,
                                  const Vec &li, bool with_hessian) {
    const int n = static_cast<int>(model.n());
    const int me = static_cast<int>(model.me());
    const int mi = static_cast<int>(model.mi());
    Eigen::MatrixXd dense = Eigen::MatrixXd::Zero(n + me + mi, n + me + mi);

    if (with_hessian) {
        const SpMatRM hessian = model.eval_hess(x, scale, le, li);
        for (int outer = 0; outer < static_cast<int>(hessian.outerSize()); ++outer) {
            for (SpMatRM::InnerIterator it(hessian, outer); it; ++it) {
                dense(it.row(), it.col()) += it.value();
            }
        }
    }
    if (me > 0) {
        const SpMatRM jacobian = model.eval_jac_e(x);
        for (int outer = 0; outer < static_cast<int>(jacobian.outerSize()); ++outer) {
            for (SpMatRM::InnerIterator it(jacobian, outer); it; ++it) {
                dense(n + it.row(), it.col()) += it.value();
            }
        }
    }
    if (mi > 0) {
        const SpMatRM jacobian = model.eval_jac_i(x);
        for (int outer = 0; outer < static_cast<int>(jacobian.outerSize()); ++outer) {
            for (SpMatRM::InnerIterator it(jacobian, outer); it; ++it) {
                dense(n + me + it.row(), it.col()) += it.value();
            }
        }
    }
    return dense;
}

Vec hand_composed_adjoint(const NlpModel &model, const Vec &x, const Vec &le, const Vec &li) {
    Vec adjoint = Vec::Zero(model.n());
    if (model.me() > 0) {
        adjoint += Vec(model.eval_jac_e(x).transpose() * le);
    }
    if (model.mi() > 0) {
        adjoint += Vec(model.eval_jac_i(x).transpose() * li);
    }
    return adjoint;
}

/// Runs the full-KKT shape against @p model and compares every destination with
/// a hand composition. Used on more than one HS transcription, so the comparison
/// is written once.
void expect_full_kkt_matches_hand_composition(std::shared_ptr<NlpModel> model, const Vec &x,
                                              const Vec &le, const Vec &li, double scale) {
    NlpModelAggregate aggregate(model);
    BridgeDestinations destinations(aggregate);

    aggregate.assemble(CandidatePoint{x, le, li, scale}, kRequestFullKkt, destinations.kkt_view(),
                       destinations.rhs_view());

    EXPECT_TRUE(destinations.dense_kkt(aggregate).isApprox(
        hand_composed_kkt(*model, x, scale, le, li, true)))
        << "the assembled KKT block does not match the hand composition";

    EXPECT_DOUBLE_EQ(destinations.objective_, scale * model->eval_f(x));

    const Vec gradient = scale * model->eval_grad(x);
    for (int row = 0; row < gradient.size(); ++row) {
        EXPECT_DOUBLE_EQ(destinations.objective_gradient_.values()[static_cast<std::size_t>(row)],
                         gradient[row]);
    }

    const Vec adjoint = hand_composed_adjoint(*model, x, le, li);
    for (int row = 0; row < adjoint.size(); ++row) {
        EXPECT_DOUBLE_EQ(destinations.adjoint_gradient_.values()[static_cast<std::size_t>(row)],
                         adjoint[row]);
    }

    if (model->me() > 0) {
        const Vec residuals = model->eval_ce(x);
        for (int row = 0; row < residuals.size(); ++row) {
            EXPECT_DOUBLE_EQ(
                destinations.equality_residuals_.values()[static_cast<std::size_t>(row)],
                residuals[row]);
        }
    }
    if (model->mi() > 0) {
        const Vec residuals = model->eval_ci(x);
        for (int row = 0; row < residuals.size(); ++row) {
            EXPECT_DOUBLE_EQ(
                destinations.inequality_residuals_.values()[static_cast<std::size_t>(row)],
                residuals[row]);
        }
    }
}

} // namespace

TEST(NlpModelAggregateEquivalence, FullKktOnAModelWithBothConstraintKinds) {
    // HS14: two variables, one equality, one inequality, no finite bounds.
    auto model = std::make_shared<hven::solvers::test_support::Hs14Model>();
    const Vec x = (Vec(2) << 1.25, 0.75).finished();
    expect_full_kkt_matches_hand_composition(model, x, (Vec(1) << 0.5).finished(),
                                             (Vec(1) << 1.25).finished(), 1.0);
}

TEST(NlpModelAggregateEquivalence, FullKktOnAnInequalityOnlyModelAtANonUnitScale) {
    // HS76: four variables, three inequality rows, no equalities -- the
    // zero-declared-rows case on the equality side.
    auto model = std::make_shared<hven::solvers::test_support::Hs76Model>();
    const Vec x = (Vec(4) << 0.3, 0.2, 0.1, 0.4).finished();
    expect_full_kkt_matches_hand_composition(model, x, Vec(0),
                                             (Vec(3) << 0.5, 0.25, 0.75).finished(), 2.0);
}

TEST(NlpModelAggregateEquivalence, FullKktOnAnEqualityOnlyModel) {
    // HS6: two variables, one equality row, no inequalities.
    auto model = std::make_shared<hven::solvers::test_support::Hs6Model>();
    const Vec x = (Vec(2) << 0.6, 0.4).finished();
    expect_full_kkt_matches_hand_composition(model, x, (Vec(1) << 1.5).finished(), Vec(0), 1.0);
}

TEST(NlpModelAggregateEquivalence, ConstraintKktAssemblesTheAdjointHessianAlone) {
    auto model = std::make_shared<hven::solvers::test_support::Hs14Model>();
    const Vec x = (Vec(2) << 1.25, 0.75).finished();
    const Vec le = (Vec(1) << 0.5).finished();
    const Vec li = (Vec(1) << 1.25).finished();

    NlpModelAggregate aggregate(model);
    BridgeDestinations destinations(aggregate);
    aggregate.assemble(CandidatePoint{x, le, li, 3.0}, kRequestConstraintKkt,
                       destinations.kkt_view(), destinations.rhs_view());

    // The objective block is dropped: the hand composition uses obj_scale 0
    // whatever the point's own scale is.
    EXPECT_TRUE(destinations.dense_kkt(aggregate).isApprox(
        hand_composed_kkt(*model, x, 0.0, le, li, true)));
    EXPECT_DOUBLE_EQ(destinations.objective_, 0.0);
    for (double value : destinations.objective_gradient_.values()) {
        EXPECT_DOUBLE_EQ(value, 0.0) << "the shape names no objective gradient";
    }
}

TEST(NlpModelAggregateEquivalence, ConstraintResidualsAndJacobianWritesNoHessianSlot) {
    auto model = counting_model();
    NlpModelAggregate aggregate(model);
    BridgeDestinations destinations(aggregate);
    const BridgePoint point;

    aggregate.assemble(point.values_only(), kRequestConstraintResidualsAndJacobian,
                       destinations.kkt_view(), destinations.rhs_view());

    const ClaimBlock hessian = aggregate.hessian_claims();
    for (int slot = hessian.start_; slot < hessian.start_ + hessian.count_; ++slot) {
        EXPECT_DOUBLE_EQ(
            destinations.kkt_values_[static_cast<std::size_t>(destinations.kkt_location(slot))],
            0.0);
    }
    EXPECT_TRUE(destinations.dense_kkt(aggregate).isApprox(
        hand_composed_kkt(*model, point.x_, 0.0, point.equality_, point.inequality_, false)));
}

// The three SQP-owned shapes (rows 9-11), served by the bridge without throw
// and checked against the same hand compositions the interior-owned shapes
// use above.

TEST(NlpModelAggregateEquivalence, LagrangianHessianAloneMatchesEvalHessComposedLagrangian) {
    auto model = counting_model();
    NlpModelAggregate aggregate(model);
    BridgeDestinations destinations(aggregate);
    const BridgePoint point;
    const double scale = 2.0;

    EXPECT_NO_THROW(aggregate.assemble(point.full(scale), kRequestLagrangianHessian,
                                       destinations.kkt_view(), destinations.rhs_view()));

    // The composed Lagrangian Hessian, by hand: the model's own eval_hess at
    // this point's scale and multipliers, scattered through no Jacobian claim
    // at all -- shape 9 names the Hessian alone.
    Eigen::MatrixXd expected =
        Eigen::MatrixXd::Zero(aggregate.kkt_dimension(), aggregate.kkt_dimension());
    const SpMatRM hessian = model->eval_hess(point.x_, scale, point.equality_, point.inequality_);
    for (int outer = 0; outer < static_cast<int>(hessian.outerSize()); ++outer) {
        for (SpMatRM::InnerIterator it(hessian, outer); it; ++it) {
            expected(it.row(), it.col()) += it.value();
        }
    }
    EXPECT_TRUE(destinations.dense_kkt(aggregate).isApprox(expected))
        << "shape 9's assembled KKT block must equal the composed Lagrangian Hessian alone";

    // Neither value kind, and no gradient of either kind: this shape names
    // none of them.
    EXPECT_DOUBLE_EQ(destinations.objective_, 0.0);
    for (double value : destinations.objective_gradient_.values()) {
        EXPECT_DOUBLE_EQ(value, 0.0);
    }
    for (double value : destinations.equality_residuals_.values()) {
        EXPECT_DOUBLE_EQ(value, 0.0);
    }
    for (double value : destinations.inequality_residuals_.values()) {
        EXPECT_DOUBLE_EQ(value, 0.0);
    }
}

TEST(NlpModelAggregateEquivalence, GradientAndJacobiansFillsTheGradientArenaAndBothJacobianClaims) {
    auto model = counting_model();
    NlpModelAggregate aggregate(model);
    BridgeDestinations destinations(aggregate);
    const BridgePoint point;

    EXPECT_NO_THROW(aggregate.assemble(point.values_only(), kRequestGradientAndJacobians,
                                       destinations.kkt_view(), destinations.rhs_view()));

    const Vec gradient = model->eval_grad(point.x_);
    for (int row = 0; row < gradient.size(); ++row) {
        EXPECT_DOUBLE_EQ(destinations.objective_gradient_.values()[static_cast<std::size_t>(row)],
                         gradient[row]);
    }
    EXPECT_TRUE(destinations.dense_kkt(aggregate).isApprox(
        hand_composed_kkt(*model, point.x_, 0.0, point.equality_, point.inequality_, false)))
        << "shape 10's assembled KKT block must be the Jacobian pair alone";

    // Values not re-evaluated, and no adjoint gradient: this shape names
    // neither value kind and no constraint adjoint.
    EXPECT_DOUBLE_EQ(destinations.objective_, 0.0);
    for (double value : destinations.adjoint_gradient_.values()) {
        EXPECT_DOUBLE_EQ(value, 0.0);
    }
    for (double value : destinations.equality_residuals_.values()) {
        EXPECT_DOUBLE_EQ(value, 0.0);
    }
    for (double value : destinations.inequality_residuals_.values()) {
        EXPECT_DOUBLE_EQ(value, 0.0);
    }
}

TEST(NlpModelAggregateEquivalence,
     ConstraintJacobiansOnlyFillsTheJacobianClaimsAndLeavesTheGradientArenaUntouched) {
    auto model = counting_model();
    NlpModelAggregate aggregate(model);
    BridgeDestinations destinations(aggregate);
    const BridgePoint point;

    EXPECT_NO_THROW(aggregate.assemble(point.values_only(), kRequestConstraintJacobiansOnly,
                                       destinations.kkt_view(), destinations.rhs_view()));

    EXPECT_TRUE(destinations.dense_kkt(aggregate).isApprox(
        hand_composed_kkt(*model, point.x_, 0.0, point.equality_, point.inequality_, false)))
        << "shape 11's assembled KKT block must be the Jacobian pair alone";

    for (double value : destinations.objective_gradient_.values()) {
        EXPECT_DOUBLE_EQ(value, 0.0) << "shape 11 names no objective gradient";
    }
    EXPECT_DOUBLE_EQ(destinations.objective_, 0.0);
}

TEST(NlpModelAggregateEquivalence, ClaimsLandWhereTheTableSendsThemUnderEitherOrder) {
    // The companion of every pin above, which runs under a permuted table: with
    // an identity table each claim's value lands at its own slot index, so the
    // two orders together separate "wrote through the table" from "wrote its own
    // slot index".
    auto model = counting_model();
    NlpModelAggregate aggregate(model);
    const BridgePoint point;

    BridgeDestinations identity(aggregate, BridgeTableOrder::kIdentity);
    aggregate.assemble(point.full(), kRequestFullKkt, identity.kkt_view(), identity.rhs_view());

    BridgeDestinations permuted(aggregate, BridgeTableOrder::kPermuted);
    aggregate.assemble(point.full(), kRequestFullKkt, permuted.kkt_view(), permuted.rhs_view());

    const int claims = static_cast<int>(aggregate.kkt_claim_rows().size());
    ASSERT_GT(claims, 1);
    for (int slot = 0; slot < claims; ++slot) {
        EXPECT_EQ(identity.kkt_location(slot), slot);
        EXPECT_EQ(permuted.kkt_location(slot), claims - 1 - slot);
        EXPECT_DOUBLE_EQ(
            permuted.kkt_values_[static_cast<std::size_t>(permuted.kkt_location(slot))],
            identity.kkt_values_[static_cast<std::size_t>(slot)]);
    }
    EXPECT_TRUE(permuted.dense_kkt(aggregate).isApprox(identity.dense_kkt(aggregate)));
}

TEST(NlpModelAggregateEquivalence, AssembleAccumulatesRatherThanAssigns) {
    auto model = counting_model();
    NlpModelAggregate aggregate(model);
    BridgeDestinations destinations(aggregate);
    const BridgePoint point;

    aggregate.assemble(point.full(), kRequestFullKkt, destinations.kkt_view(),
                       destinations.rhs_view());
    const std::vector<double> once = destinations.kkt_values_;
    const double objective_once = destinations.objective_;
    aggregate.assemble(point.full(), kRequestFullKkt, destinations.kkt_view(),
                       destinations.rhs_view());

    EXPECT_DOUBLE_EQ(destinations.objective_, 2.0 * objective_once);
    for (std::size_t slot = 0; slot < once.size(); ++slot) {
        EXPECT_DOUBLE_EQ(destinations.kkt_values_[slot], 2.0 * once[slot]);
    }
}

// ---------------------------------------------------------------------------
// The boundary refusals the bridge owns
// ---------------------------------------------------------------------------

TEST(NlpModelAggregateBoundary, RefusesAGradientArenaSizedOtherThanTheLaidWidth) {
    NlpModelAggregate aggregate(counting_model());
    BridgeDestinations destinations(aggregate);
    const BridgePoint point;

    RhsScatterView rhs = destinations.rhs_view();
    rhs.objective_gradient_.size_ = 1; // present, but one row short of the laid width
    EXPECT_THROW(aggregate.assemble(point.values_only(), kRequestObjectiveGradientAndConstraints,
                                    destinations.kkt_view(), rhs),
                 std::invalid_argument);
}

TEST(NlpModelAggregateBoundary, RefusesAKktTableThatDoesNotCoverTheClaimStream) {
    NlpModelAggregate aggregate(counting_model());
    BridgeDestinations destinations(aggregate);
    const BridgePoint point;

    std::vector<int> short_locations{0, 1};
    std::vector<int> clashes(static_cast<std::size_t>(aggregate.kkt_dimension()), -1);
    std::vector<std::mutex> locks;
    KktLocationTable short_table(short_locations.data(), 2, clashes.data(),
                                 static_cast<int>(clashes.size()), &locks);
    KktScatterView kkt{destinations.kkt_values_.data(),
                       static_cast<int>(destinations.kkt_values_.size()), &short_table};

    EXPECT_THROW(aggregate.assemble(point.full(), kRequestFullKkt, kkt, destinations.rhs_view()),
                 std::invalid_argument);
}

TEST(NlpModelAggregateBoundary, RefusesAModelWhoseSparsityPatternCollapses) {
    auto model = std::make_shared<BridgePatternDriftModel>();
    NlpModelAggregate aggregate(model);
    BridgeDestinations destinations(aggregate);

    // The claim pass ran at the model's start point, where the pattern is whole.
    // At x0 = 0 this model drops an entry, which is the violation nlp_model.h
    // names -- and the bridge says so instead of scattering through the wrong
    // slots.
    const Vec x = (Vec(2) << 0.0, 1.0).finished();
    const Vec le = (Vec(1) << 1.0).finished();
    const Vec li = (Vec(1) << 1.0).finished();
    EXPECT_THROW(aggregate.assemble(CandidatePoint{x, le, li}, kRequestFullKkt,
                                    destinations.kkt_view(), destinations.rhs_view()),
                 std::invalid_argument);
}

namespace {

/// A model whose inequality Jacobian keeps its nonzero COUNT and moves the
/// coordinate: one stored element, at (0, 0) until `drift()` is called and at
/// (0, 1) after. The count check cannot see this -- one entry before, one
/// entry after -- so it is the coordinate comparison in the scatter or nothing.
class BridgeSameCountDriftModel : public BridgeCountingModel {
  public:
    void drift() { drifted_ = true; }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        counts_.jac_i_++;
        return hven::solvers::test_support::detail::make_jac(1, 2, {{0, drifted_ ? 1 : 0, 3.0}});
    }

  private:
    bool drifted_ = false;
};

/// A model that presents the same two stored elements in the other order once
/// `reverse()` is called. The set is invariant, the count is invariant, and the
/// pairing of the nth element with the nth claim slot is not: entry 0 of row 0
/// now carries the coordinate slot 1 was claimed at. Built by exchanging the
/// two stored entries in place, which is how a return assembled outside
/// setFromTriplets can legally reach this state.
class BridgeStorageOrderModel : public BridgeCountingModel {
  public:
    void reverse() { reversed_ = true; }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> jacobian = BridgeCountingModel::eval_jac_i(x);
        if (reversed_) {
            jacobian.makeCompressed();
            std::swap(jacobian.innerIndexPtr()[0], jacobian.innerIndexPtr()[1]);
            std::swap(jacobian.valuePtr()[0], jacobian.valuePtr()[1]);
        }
        return jacobian;
    }

  private:
    bool reversed_ = false;
};

} // namespace

// ---------------------------------------------------------------------------
// The scatter's own precondition: the nth stored element is the nth claim slot
// ---------------------------------------------------------------------------
//
// Both pins below are bidirectional. The conforming state is asserted to be
// SERVED first -- otherwise a scatter that refused everything would pass them --
// and the violating state is then asserted to be refused by name. Neither
// failure moves a nonzero count, so require_claimed_nonzeros sees nothing in
// either case; the coordinate comparison in scatter_matrix is the only thing
// standing between these returns and a value summed into a location laid for a
// different coordinate.

TEST(NlpModelAggregateBoundary, RefusesAReturnWhosePatternMovesAtTheSameCount) {
    auto model = std::make_shared<BridgeSameCountDriftModel>();
    NlpModelAggregate aggregate(model);
    BridgeDestinations destinations(aggregate);
    const BridgePoint point;

    // Conforming: the coordinate the claim pass recorded is the one presented.
    EXPECT_NO_THROW(aggregate.assemble(point.full(), kRequestFullKkt, destinations.kkt_view(),
                                       destinations.rhs_view()));

    model->drift();
    destinations.zero();
    try {
        aggregate.assemble(point.full(), kRequestFullKkt, destinations.kkt_view(),
                           destinations.rhs_view());
        FAIL() << "a moved coordinate at an unchanged count must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("eval_jac_i"), std::string::npos) << message;
        EXPECT_NE(message.find("presented (0, 1)"), std::string::npos) << message;
        EXPECT_NE(message.find("laid at (0, 0)"), std::string::npos) << message;
    }
}

TEST(NlpModelAggregateBoundary, RefusesAReturnThatPresentsItsElementsInAnotherOrder) {
    auto model = std::make_shared<BridgeStorageOrderModel>();
    NlpModelAggregate aggregate(model);
    BridgeDestinations destinations(aggregate);
    const BridgePoint point;

    EXPECT_NO_THROW(aggregate.assemble(point.full(), kRequestFullKkt, destinations.kkt_view(),
                                       destinations.rhs_view()));

    model->reverse();
    destinations.zero();
    try {
        aggregate.assemble(point.full(), kRequestFullKkt, destinations.kkt_view(),
                           destinations.rhs_view());
        FAIL() << "a reordered return must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("eval_jac_i"), std::string::npos) << message;
        // The FIRST offending entry, which is the leading one of the exchanged
        // pair: it carries the coordinate its neighbour's slot was claimed at.
        EXPECT_NE(message.find("presented (0, 1)"), std::string::npos) << message;
        EXPECT_NE(message.find("laid at (0, 0)"), std::string::npos) << message;
    }
}

namespace {

/// The message of the std::invalid_argument a bridge construction must throw, so
/// a test can assert it names the callback and both dimensions.
std::string bridge_construction_message(std::shared_ptr<NlpModel> model) {
    try {
        NlpModelAggregate bridge(std::move(model));
    } catch (const std::invalid_argument &error) {
        return error.what();
    }
    return {};
}

} // namespace

TEST(NlpModelAggregateBoundary, RefusesAModelWhoseSparseBlockIsNotTheShapeItDeclares) {
    // Eigen's own asserts are compiled out under NDEBUG, so nothing but this
    // check stands between a dimension-lying model and claims laid outside the
    // assembled space. One case per callback, and each message must name the
    // callback and both the returned and the declared shape.
    class WideHessianModel : public BridgeCountingModel {
      public:
        SpMatRM eval_hess(const Vec &, double, const Vec &, const Vec &) const override {
            return hven::solvers::test_support::detail::make_upper(3, {{0, 0, 1.0}});
        }
    };
    class TallEqualityJacobianModel : public BridgeCountingModel {
      public:
        Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
            return hven::solvers::test_support::detail::make_jac(4, 2, {{0, 0, 1.0}});
        }
    };
    class NarrowInequalityJacobianModel : public BridgeCountingModel {
      public:
        Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
            return hven::solvers::test_support::detail::make_jac(1, 5, {{0, 0, 1.0}});
        }
    };

    const std::string hessian = bridge_construction_message(std::make_shared<WideHessianModel>());
    EXPECT_NE(hessian.find("eval_hess"), std::string::npos) << hessian;
    EXPECT_NE(hessian.find('3'), std::string::npos) << hessian;
    EXPECT_NE(hessian.find('2'), std::string::npos) << hessian;

    const std::string equality =
        bridge_construction_message(std::make_shared<TallEqualityJacobianModel>());
    EXPECT_NE(equality.find("eval_jac_e"), std::string::npos) << equality;
    EXPECT_NE(equality.find('4'), std::string::npos) << equality;
    EXPECT_NE(equality.find('1'), std::string::npos) << equality;

    const std::string inequality =
        bridge_construction_message(std::make_shared<NarrowInequalityJacobianModel>());
    EXPECT_NE(inequality.find("eval_jac_i"), std::string::npos) << inequality;
    EXPECT_NE(inequality.find('5'), std::string::npos) << inequality;
    EXPECT_NE(inequality.find('2'), std::string::npos) << inequality;
}

TEST(NlpModelAggregateBoundary, RefusesADimensionThatChangesAfterTheClaimPass) {
    // The per-call half of the same check. This model is honest at the start
    // point, where the claim pass reads it, and lies everywhere else -- so the
    // claim-time check cannot catch it and the per-call one must.
    class LateWideJacobianModel : public BridgeCountingModel {
      public:
        Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
            if (x(0) == 0.5) {
                return BridgeCountingModel::eval_jac_i(x);
            }
            return hven::solvers::test_support::detail::make_jac(1, 7, {{0, 0, 1.0}, {0, 1, 1.0}});
        }
    };

    auto model = std::make_shared<LateWideJacobianModel>();
    NlpModelAggregate aggregate(model);
    BridgeDestinations destinations(aggregate);
    const BridgePoint point;

    try {
        aggregate.assemble(point.full(), kRequestFullKkt, destinations.kkt_view(),
                           destinations.rhs_view());
        FAIL() << "a Jacobian whose width contradicts the declaration must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("eval_jac_i"), std::string::npos) << message;
        EXPECT_NE(message.find('7'), std::string::npos) << message;
    }
}

TEST(NlpModelAggregateBoundary, RefusesASparsityPatternThatGrowsAfterTheClaimPass) {
    // The companion of the collapse pin above. The claim pass reads one entry at
    // the start point; a later evaluation returns two, so the extra value has no
    // slot to land in and the count check says so.
    auto model = std::make_shared<BridgePatternGrowthModel>();
    NlpModelAggregate aggregate(model);
    ASSERT_EQ(aggregate.inequality_jacobian_claims().count_, 1)
        << "the fixture must claim one slot at the start point";

    BridgeDestinations destinations(aggregate);
    const BridgePoint point;
    try {
        aggregate.assemble(point.full(), kRequestFullKkt, destinations.kkt_view(),
                           destinations.rhs_view());
        FAIL() << "a sparsity pattern that grew after the claim pass must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("eval_jac_i"), std::string::npos) << message;
        EXPECT_NE(message.find('2'), std::string::npos) << message;
        EXPECT_NE(message.find('1'), std::string::npos) << message;
    }
}

TEST(NlpModelAggregateBoundary, RefusesAHessianEntryBelowTheDiagonal) {
    class LowerTriangleHessianModel : public BridgeCountingModel {
      public:
        SpMatRM eval_hess(const Vec &, double, const Vec &, const Vec &) const override {
            // (1, 0) is below the diagonal, which nlp_model.h's upper-triangle
            // return convention does not admit. The claims record the triangle
            // verbatim, so this is caught at claim time rather than trusted.
            return hven::solvers::test_support::detail::make_upper(
                2, {{0, 0, 2.0}, {1, 0, 1.0}, {1, 1, 4.0}});
        }
    };

    EXPECT_THROW(
        { NlpModelAggregate bridge(std::make_shared<LowerTriangleHessianModel>()); },
        std::invalid_argument);
}

TEST(NlpModelAggregateBoundary, RefusesADeclarationWhoseBoundsIntersectToNothing) {
    class InvertedBoundsModel : public BridgeCountingModel {
      public:
        const Vec &lower() const override { return lower_; }
        const Vec &upper() const override { return upper_; }

      private:
        Vec lower_ = (Vec(2) << 1.0, 0.0).finished();
        Vec upper_ = (Vec(2) << -1.0, 1.0).finished();
    };

    EXPECT_THROW(
        { NlpModelAggregate bridge(std::make_shared<InvertedBoundsModel>()); },
        std::invalid_argument);
}
