// The rest of the contract surface: the piece concept, the declaration's
// boundary validation, the published location tables, the scatter views, and
// the two rules the request carries -- only what the request names is written,
// and masking a request never moves layout, digest or epoch.

#include <mutex>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "hven/detail/interior/indexing_data.h"
#include "hven/detail/model/nlp_adapter.h"
#include "hven/model/aggregate_declaration.h"
#include "hven/model/candidate_point.h"
#include "hven/model/claim_space.h"
#include "hven/model/nlp_aggregate.h"
#include "support/fake_aggregate.h"

using hven::Vec;
using hven::solvers::AggregateDeclaration;
using hven::solvers::AggregatePiece;
using hven::solvers::CandidateFirstOrder;
using hven::solvers::CandidatePoint;
using hven::solvers::CandidateValues;
using hven::solvers::ConstraintAggregatePiece;
using hven::solvers::EvalRequest;
using hven::solvers::KktLocationTable;
using hven::solvers::KktScatterView;
using hven::solvers::ObjectiveAggregatePiece;
using hven::solvers::RhsArenaView;
using hven::solvers::RhsLocationTable;
using hven::solvers::RhsScatterView;
using hven::solvers::VariableBound;

// ---------------------------------------------------------------------------
// The piece concept
// ---------------------------------------------------------------------------

namespace {

/// Stands in for whatever per-piece indexing state a provider threads through
/// its claim and evaluation calls. The concept is stated over it rather than
/// over a concrete type so the contract names no provider's internals.
struct ContractProbeIndexData {};

/// The smallest thing that is a piece: the sizing surface plus the claim pass.
struct ContractProbeMinimalPiece {
    std::string name() const { return "probe"; }
    int input_rows() const { return 2; }
    int output_rows() const { return 1; }
    bool thread_safe() const { return true; }

    void get_kkt_space(Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>, int &freeloc, int,
                       bool, bool, ContractProbeIndexData &) {
        freeloc += 0;
    }
    int num_kkt_elements(bool, bool) const { return 0; }
};

/// A piece missing the claim pass is not a piece.
struct ContractProbeSizedOnly {
    std::string name() const { return "sized-only"; }
    int input_rows() const { return 2; }
    int output_rows() const { return 1; }
    bool thread_safe() const { return true; }
};

static_assert(AggregatePiece<ContractProbeMinimalPiece, ContractProbeIndexData>,
              "the minimal piece must satisfy the piece concept");
static_assert(!AggregatePiece<ContractProbeSizedOnly, ContractProbeIndexData>,
              "a type without the claim pass must not satisfy the piece concept");

// The pieces that exist today satisfy the concept unchanged -- the concept
// states the contract they already meet, it does not impose a new one.
static_assert(AggregatePiece<hven::solvers::NLPObjectivePiece, hven::solvers::SolverIndexingData>);
static_assert(AggregatePiece<hven::solvers::NLPConstraintPiece, hven::solvers::SolverIndexingData>);
static_assert(
    ObjectiveAggregatePiece<hven::solvers::NLPObjectivePiece, hven::solvers::SolverIndexingData>);
static_assert(
    ConstraintAggregatePiece<hven::solvers::NLPConstraintPiece, hven::solvers::SolverIndexingData>);

} // namespace

TEST(AggregatePieceConcept, IsSatisfiedByAMinimalPieceAndByTheExistingPieces) {
    // The substance is in the static_asserts above; this case exists so a
    // regression in them is reported as a named failure rather than only as a
    // build break.
    EXPECT_TRUE((AggregatePiece<ContractProbeMinimalPiece, ContractProbeIndexData>));
    EXPECT_TRUE(
        (AggregatePiece<hven::solvers::NLPObjectivePiece, hven::solvers::SolverIndexingData>));
    EXPECT_TRUE(
        (AggregatePiece<hven::solvers::NLPConstraintPiece, hven::solvers::SolverIndexingData>));
}

// ---------------------------------------------------------------------------
// AggregateDeclaration validation
// ---------------------------------------------------------------------------

namespace {

AggregateDeclaration consistent_declaration() {
    AggregateDeclaration declaration;
    declaration.primal_vars_ = 4;
    declaration.equality_rows_ = 0;
    declaration.inequality_rows_ = 0;
    declaration.partition_count_ = 2;
    return declaration;
}

/// Runs the call and returns the message of the std::invalid_argument it must
/// throw, so a test can assert the message names the offender and both numbers.
std::string invalid_argument_message(const AggregateDeclaration &declaration) {
    try {
        declaration.validate();
    } catch (const std::invalid_argument &error) {
        return error.what();
    }
    return {};
}

} // namespace

TEST(AggregateDeclarationTest, AcceptsAConsistentDeclaration) {
    EXPECT_NO_THROW(consistent_declaration().validate());
}

TEST(AggregateDeclarationTest, RejectsAnEqualityRowCountNoPieceClaims) {
    AggregateDeclaration declaration = consistent_declaration();
    declaration.equality_rows_ = 3;
    const std::string message = invalid_argument_message(declaration);
    EXPECT_NE(message.find("equality"), std::string::npos) << message;
    EXPECT_NE(message.find('3'), std::string::npos) << message;
    EXPECT_NE(message.find('0'), std::string::npos) << message;
}

TEST(AggregateDeclarationTest, RejectsAnInequalityRowCountNoPieceClaims) {
    AggregateDeclaration declaration = consistent_declaration();
    declaration.inequality_rows_ = 5;
    const std::string message = invalid_argument_message(declaration);
    EXPECT_NE(message.find("inequality"), std::string::npos) << message;
    EXPECT_NE(message.find('5'), std::string::npos) << message;
}

TEST(AggregateDeclarationTest, RejectsANonPositivePartitionCount) {
    AggregateDeclaration declaration = consistent_declaration();
    declaration.partition_count_ = 0;
    EXPECT_THROW(declaration.validate(), std::invalid_argument);
    declaration.partition_count_ = -2;
    EXPECT_THROW(declaration.validate(), std::invalid_argument);
}

TEST(AggregateDeclarationTest, RejectsANegativeDimension) {
    AggregateDeclaration declaration = consistent_declaration();
    declaration.primal_vars_ = -1;
    EXPECT_THROW(declaration.validate(), std::invalid_argument);
}

TEST(AggregateDeclarationTest, RejectsABoundOutsideTheVariableRange) {
    AggregateDeclaration declaration = consistent_declaration();
    declaration.variable_bounds_.push_back(VariableBound{7, 0.0, 1.0});
    const std::string message = invalid_argument_message(declaration);
    EXPECT_NE(message.find('7'), std::string::npos) << message;
    EXPECT_NE(message.find('4'), std::string::npos) << message;
}

TEST(AggregateDeclarationTest, RejectsANotANumberBound) {
    AggregateDeclaration declaration = consistent_declaration();
    declaration.variable_bounds_.push_back(
        VariableBound{1, std::numeric_limits<double>::quiet_NaN(), 1.0});
    EXPECT_THROW(declaration.validate(), std::invalid_argument);
}

TEST(AggregateDeclarationTest, AcceptsAnInRangeBound) {
    AggregateDeclaration declaration = consistent_declaration();
    declaration.variable_bounds_.push_back(VariableBound{3, 0.0, 1.0});
    EXPECT_NO_THROW(declaration.validate());
}

// ---------------------------------------------------------------------------
// Candidate storage validation
// ---------------------------------------------------------------------------

TEST(CandidateStorageValidation, ValuesBlocksMustMatchTheirDimensions) {
    double objective = 0.0;
    Vec equality(2);
    Vec inequality(3);
    CandidateValues values{objective, equality, inequality};

    EXPECT_NO_THROW(hven::solvers::validate_candidate_values(values, 2, 3));
    EXPECT_THROW(hven::solvers::validate_candidate_values(values, 4, 3), std::invalid_argument);
    EXPECT_THROW(hven::solvers::validate_candidate_values(values, 2, 1), std::invalid_argument);
}

TEST(CandidateStorageValidation, TheMessageNamesTheBlockAndBothNumbers) {
    double objective = 0.0;
    Vec equality(2);
    Vec inequality(3);
    CandidateValues values{objective, equality, inequality};
    try {
        hven::solvers::validate_candidate_values(values, 4, 3);
        FAIL() << "a mis-sized equality block must be rejected";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("equality"), std::string::npos) << message;
        EXPECT_NE(message.find('2'), std::string::npos) << message;
        EXPECT_NE(message.find('4'), std::string::npos) << message;
    }
}

TEST(CandidateStorageValidation, FirstOrderBlocksMustMatchTheirDimensions) {
    double objective = 0.0;
    Vec equality(2);
    Vec inequality(3);
    Vec objective_gradient(4);
    Vec adjoint_gradient(4);
    CandidateFirstOrder out{CandidateValues{objective, equality, inequality}, objective_gradient,
                            adjoint_gradient};

    EXPECT_NO_THROW(hven::solvers::validate_candidate_first_order(out, 4, 2, 3));
    EXPECT_THROW(hven::solvers::validate_candidate_first_order(out, 5, 2, 3),
                 std::invalid_argument);
}

TEST(CandidateStorageValidation, MultiplierBlocksAreEitherEmptyOrExact) {
    Vec x(4);
    Vec exact_equality(2);
    Vec wrong_equality(9);
    Vec none;

    EXPECT_NO_THROW(
        hven::solvers::validate_candidate_point(CandidatePoint{x, exact_equality, none}, 4, 2, 3));
    EXPECT_NO_THROW(
        hven::solvers::validate_candidate_point(CandidatePoint{x, none, none}, 4, 2, 3));
    EXPECT_THROW(
        hven::solvers::validate_candidate_point(CandidatePoint{x, wrong_equality, none}, 4, 2, 3),
        std::invalid_argument);
}

TEST(CandidateStorageValidation, ThePrimalBlockIsNeverOptional) {
    Vec x(3);
    Vec none;
    EXPECT_THROW(hven::solvers::validate_candidate_point(CandidatePoint{x, none, none}, 4, 2, 3),
                 std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Location tables and scatter views
// ---------------------------------------------------------------------------

namespace {

/// A published KKT table over `size` slots, mapping slot k to offset k, with
/// `contested` marking one column as contested.
struct ContractProbeKktTable {
    std::vector<int> locations;
    std::vector<int> clashes;
    std::vector<std::mutex> locks;

    ContractProbeKktTable(int size, int contested_column, int lock_count)
        : locations(static_cast<std::size_t>(size)), clashes(static_cast<std::size_t>(size), -1),
          locks(static_cast<std::size_t>(lock_count)) {
        for (int k = 0; k < size; ++k) {
            locations[static_cast<std::size_t>(k)] = k;
        }
        if (contested_column >= 0) {
            clashes[static_cast<std::size_t>(contested_column)] = 0;
        }
    }

    KktLocationTable table() {
        return KktLocationTable(locations.data(), static_cast<int>(locations.size()),
                                clashes.data(), static_cast<int>(clashes.size()), &locks);
    }
};

} // namespace

TEST(KktLocationTableTest, PublishesLocationsClashMarksAndLocksTogether) {
    ContractProbeKktTable fixture(4, 2, 1);
    const KktLocationTable table = fixture.table();

    EXPECT_EQ(table.size(), 4);
    EXPECT_EQ(table.location(3), 3);
    EXPECT_EQ(table.clash_lock(1), -1);
    EXPECT_EQ(table.clash_lock(2), 0);
    // The mutex the mark indexes is reachable from the same table, which is
    // what makes the shared lock keying structural rather than conventional.
    std::mutex &lock = table.lock(table.clash_lock(2));
    lock.lock();
    lock.unlock();
}

TEST(KktLocationTableTest, IsEmptyWhenNothingWasClaimed) {
    const KktLocationTable table;
    EXPECT_EQ(table.size(), 0);
    EXPECT_TRUE(table.empty());
}

TEST(KktLocationTableTest, RejectsAClashMarkOutsideTheLockVector) {
    ContractProbeKktTable fixture(4, 2, 0); // marks column 2 contested, supplies no locks
    EXPECT_THROW(fixture.table(), std::invalid_argument);
}

TEST(KktLocationTableTest, RejectsAMissingIndexArray) {
    std::vector<std::mutex> locks(1);
    EXPECT_THROW(KktLocationTable(nullptr, 3, nullptr, 3, &locks), std::invalid_argument);
}

TEST(RhsLocationTableTest, MapsClaimSlotsToArenaRows) {
    const std::vector<int> rows = {0, 2, 4};
    const RhsLocationTable table(rows.data(), static_cast<int>(rows.size()));
    EXPECT_EQ(table.size(), 3);
    EXPECT_EQ(table.location(1), 2);
}

TEST(RhsLocationTableTest, ADroppedRowIsSpelledAsNegativeOne) {
    // An eliminated variable's gradient row is not part of the reduced
    // problem's residual: the claim survives, the destination does not.
    const std::vector<int> rows = {0, -1, 2};
    const RhsLocationTable table(rows.data(), static_cast<int>(rows.size()));
    EXPECT_EQ(table.location(1), -1);
}

TEST(ScatterViewTest, DefaultConstructedViewsAreEmpty) {
    const KktScatterView kkt;
    EXPECT_TRUE(kkt.empty());
    const RhsArenaView arena;
    EXPECT_TRUE(arena.empty());
    const RhsScatterView rhs;
    EXPECT_TRUE(rhs.objective_gradient_.empty());
    EXPECT_TRUE(rhs.constraint_adjoint_gradient_.empty());
    EXPECT_TRUE(rhs.equality_residuals_.empty());
    EXPECT_TRUE(rhs.inequality_residuals_.empty());
    EXPECT_EQ(rhs.objective_, nullptr);
}

// ---------------------------------------------------------------------------
// The request rules, against the fake aggregate
// ---------------------------------------------------------------------------

namespace {

using hven::model_tests::FakeAggregate;
using hven::model_tests::kUntouchedSentinel;

/// Caller-owned destinations for one assemble call: every arena present, every
/// slot pre-filled with the sentinel, so "written" and "left alone" are
/// distinguishable per arena.
struct ContractProbeDestinations {
    static constexpr int kKktSlots = 5;

    double objective = kUntouchedSentinel;
    std::vector<double> kkt_values;
    std::vector<int> kkt_locations;
    std::vector<int> kkt_clashes;
    std::vector<std::mutex> kkt_locks;
    KktLocationTable kkt_table;

    Vec objective_gradient;
    Vec adjoint_gradient;
    Vec equality_residuals;
    Vec inequality_residuals;
    std::vector<int> gradient_rows;
    std::vector<int> equality_rows;
    std::vector<int> inequality_rows;
    RhsLocationTable gradient_table;
    RhsLocationTable equality_table;
    RhsLocationTable inequality_table;

    ContractProbeDestinations()
        : kkt_values(static_cast<std::size_t>(kKktSlots), kUntouchedSentinel),
          kkt_locations(static_cast<std::size_t>(kKktSlots)),
          kkt_clashes(static_cast<std::size_t>(kKktSlots), -1), kkt_locks(0),
          objective_gradient(Vec::Constant(FakeAggregate::kPrimalVars, kUntouchedSentinel)),
          adjoint_gradient(Vec::Constant(FakeAggregate::kPrimalVars, kUntouchedSentinel)),
          equality_residuals(Vec::Constant(FakeAggregate::kEqualityRows, kUntouchedSentinel)),
          inequality_residuals(Vec::Constant(FakeAggregate::kInequalityRows, kUntouchedSentinel)),
          gradient_rows(static_cast<std::size_t>(FakeAggregate::kPrimalVars)),
          equality_rows(static_cast<std::size_t>(FakeAggregate::kEqualityRows)),
          inequality_rows(static_cast<std::size_t>(FakeAggregate::kInequalityRows)) {
        for (int k = 0; k < kKktSlots; ++k) {
            kkt_locations[static_cast<std::size_t>(k)] = k;
        }
        for (int k = 0; k < FakeAggregate::kPrimalVars; ++k) {
            gradient_rows[static_cast<std::size_t>(k)] = k;
        }
        for (int k = 0; k < FakeAggregate::kEqualityRows; ++k) {
            equality_rows[static_cast<std::size_t>(k)] = k;
        }
        for (int k = 0; k < FakeAggregate::kInequalityRows; ++k) {
            inequality_rows[static_cast<std::size_t>(k)] = k;
        }
        kkt_table = KktLocationTable(kkt_locations.data(), kKktSlots, kkt_clashes.data(), kKktSlots,
                                     &kkt_locks);
        gradient_table = RhsLocationTable(gradient_rows.data(), FakeAggregate::kPrimalVars);
        equality_table = RhsLocationTable(equality_rows.data(), FakeAggregate::kEqualityRows);
        inequality_table = RhsLocationTable(inequality_rows.data(), FakeAggregate::kInequalityRows);
    }

    KktScatterView kkt_view() { return KktScatterView{kkt_values.data(), kKktSlots, &kkt_table}; }

    RhsScatterView rhs_view() {
        RhsScatterView rhs;
        rhs.objective_ = &objective;
        rhs.objective_gradient_ =
            RhsArenaView{objective_gradient.data(), FakeAggregate::kPrimalVars, &gradient_table};
        rhs.constraint_adjoint_gradient_ =
            RhsArenaView{adjoint_gradient.data(), FakeAggregate::kPrimalVars, &gradient_table};
        rhs.equality_residuals_ =
            RhsArenaView{equality_residuals.data(), FakeAggregate::kEqualityRows, &equality_table};
        rhs.inequality_residuals_ = RhsArenaView{inequality_residuals.data(),
                                                 FakeAggregate::kInequalityRows, &inequality_table};
        return rhs;
    }

    bool kkt_untouched() const {
        for (const double value : kkt_values) {
            if (value != kUntouchedSentinel) {
                return false;
            }
        }
        return true;
    }

    static bool untouched(const Vec &block) { return (block.array() == kUntouchedSentinel).all(); }
};

} // namespace

TEST(AggregateAssembleContract, AnObjectiveOnlyRequestWritesOnlyTheObjectiveSlot) {
    FakeAggregate aggregate;
    ContractProbeDestinations out;
    Vec x = Vec::Zero(FakeAggregate::kPrimalVars);
    Vec none;

    aggregate.assemble(CandidatePoint{x, none, none}, hven::solvers::kRequestObjectiveOnly,
                       out.kkt_view(), out.rhs_view());

    EXPECT_NE(out.objective, kUntouchedSentinel);
    EXPECT_TRUE(out.kkt_untouched());
    EXPECT_TRUE(ContractProbeDestinations::untouched(out.objective_gradient));
    EXPECT_TRUE(ContractProbeDestinations::untouched(out.adjoint_gradient));
    EXPECT_TRUE(ContractProbeDestinations::untouched(out.equality_residuals));
    EXPECT_TRUE(ContractProbeDestinations::untouched(out.inequality_residuals));
}

TEST(AggregateAssembleContract, TheConstraintKktShapeWritesNoObjectiveOutput) {
    FakeAggregate aggregate;
    ContractProbeDestinations out;
    Vec x = Vec::Zero(FakeAggregate::kPrimalVars);
    Vec none;

    aggregate.assemble(CandidatePoint{x, none, none}, hven::solvers::kRequestConstraintKkt,
                       out.kkt_view(), out.rhs_view());

    EXPECT_EQ(out.objective, kUntouchedSentinel);
    EXPECT_TRUE(ContractProbeDestinations::untouched(out.objective_gradient));
    EXPECT_FALSE(ContractProbeDestinations::untouched(out.adjoint_gradient));
    EXPECT_FALSE(ContractProbeDestinations::untouched(out.equality_residuals));
    EXPECT_FALSE(ContractProbeDestinations::untouched(out.inequality_residuals));
    EXPECT_FALSE(out.kkt_untouched());
}

TEST(AggregateAssembleContract, AnArenaTheRequestDoesNotNameMayBeLeftEmpty) {
    FakeAggregate aggregate;
    ContractProbeDestinations out;
    Vec x = Vec::Zero(FakeAggregate::kPrimalVars);
    Vec none;

    // The objective-only shape touches no arena, so an entirely empty KKT view
    // and an RHS view carrying only the scalar out slot are legal.
    RhsScatterView rhs;
    rhs.objective_ = &out.objective;
    EXPECT_NO_THROW(aggregate.assemble(CandidatePoint{x, none, none},
                                       hven::solvers::kRequestObjectiveOnly, KktScatterView{},
                                       rhs));
    EXPECT_NE(out.objective, kUntouchedSentinel);
}

TEST(AggregateAssembleContract, RequestMaskingMovesNeitherLayoutNorDigestNorEpoch) {
    FakeAggregate aggregate;
    ContractProbeDestinations out;
    Vec x = Vec::Zero(FakeAggregate::kPrimalVars);
    Vec none;

    const auto key_before = aggregate.model_structure_key();
    const auto epoch_before = aggregate.structure_epoch();
    const int layout_before = aggregate.layout_serial();

    aggregate.assemble(CandidatePoint{x, none, none}, hven::solvers::kRequestObjectiveOnly,
                       out.kkt_view(), out.rhs_view());
    aggregate.assemble(CandidatePoint{x, none, none}, hven::solvers::kRequestFullKkt,
                       out.kkt_view(), out.rhs_view());

    EXPECT_EQ(aggregate.model_structure_key(), key_before);
    EXPECT_EQ(aggregate.structure_epoch(), epoch_before);
    EXPECT_EQ(aggregate.layout_serial(), layout_before);
}

TEST(AggregateCapabilityContract, AnAggregateDeclaresNothingUnlessItSaysOtherwise) {
    FakeAggregate aggregate;
    EXPECT_EQ(aggregate.capabilities(), hven::solvers::AggregateCapability::kNone);
    EXPECT_FALSE(has_capability(aggregate.capabilities(),
                                hven::solvers::AggregateCapability::kValuesFastPath));

    aggregate.set_capabilities(hven::solvers::AggregateCapability::kValuesFastPath);
    EXPECT_TRUE(has_capability(aggregate.capabilities(),
                               hven::solvers::AggregateCapability::kValuesFastPath));
    EXPECT_FALSE(has_capability(aggregate.capabilities(),
                                hven::solvers::AggregateCapability::kDirectScatter));
}

TEST(AggregateContract, TheStructureKeyIsReachableThroughTheFreeFunctionSpelling) {
    FakeAggregate aggregate;
    const hven::solvers::NlpAggregate &surface = aggregate;
    EXPECT_EQ(hven::solvers::model_structure_key(surface), aggregate.model_structure_key());
}

TEST(AggregateContract, TheAdoptedPartitionCountIsTheKeysPartitionConjunct) {
    FakeAggregate aggregate;
    aggregate.negotiate_partition_count(FakeAggregate::kMaxPartitions + 2);
    EXPECT_EQ(aggregate.model_structure_key().partition_count_, FakeAggregate::kMaxPartitions);
}
