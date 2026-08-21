// The rest of the contract surface: the piece concepts, the declaration's
// boundary validation and bound materialization, the published location tables,
// the scatter views, and the rules a request carries -- only the eight mapped
// shapes are legal, only what the request names is written, destinations are
// accumulated into, multipliers are required where they are read, and masking a
// request never moves layout, digest or epoch.

#include <limits>
#include <mutex>
#include <stdexcept>
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

// The objective-kind concept is NOT weaker than the seam that stores an
// objective: that seam forwards the constraint surface too, so a type carrying
// only the three scalar methods is not storable however objective-shaped it
// looks. A constraint piece has the constraint surface and not the objective
// one, and must fail the objective-kind concept for that reason alone.
static_assert(
    !ObjectiveAggregatePiece<hven::solvers::NLPConstraintPiece, hven::solvers::SolverIndexingData>);
static_assert(!hven::solvers::ObjectiveAggregateSurface<hven::solvers::NLPConstraintPiece,
                                                        hven::solvers::SolverIndexingData>);
static_assert(!ObjectiveAggregatePiece<ContractProbeMinimalPiece, ContractProbeIndexData>);

/// The scalar objective surface alone, with no constraint surface behind it:
/// objective-shaped, and still not storable as an objective.
struct ContractProbeObjectiveSurfaceOnly {
    std::string name() const { return "objective-surface-only"; }
    int input_rows() const { return 2; }
    int output_rows() const { return 1; }
    bool thread_safe() const { return true; }

    void get_kkt_space(Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>, int &, int, bool,
                       bool, ContractProbeIndexData &) {}
    int num_kkt_elements(bool, bool) const { return 0; }

    void objective(double, const Eigen::Ref<const Eigen::VectorXd> &, double &,
                   const ContractProbeIndexData &) const {}
    void objective_gradient(double, const Eigen::Ref<const Eigen::VectorXd> &, double &,
                            Eigen::Ref<Eigen::VectorXd>, const ContractProbeIndexData &) const {}
    void objective_gradient_hessian(double, const Eigen::Ref<const Eigen::VectorXd> &, double &,
                                    Eigen::Ref<Eigen::VectorXd>,
                                    Eigen::SparseMatrix<double, Eigen::RowMajor> &,
                                    Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>,
                                    std::vector<std::mutex> &,
                                    const ContractProbeIndexData &) const {}
};

static_assert(hven::solvers::ObjectiveAggregateSurface<ContractProbeObjectiveSurfaceOnly,
                                                       ContractProbeIndexData>);
static_assert(!ObjectiveAggregatePiece<ContractProbeObjectiveSurfaceOnly, ContractProbeIndexData>,
              "the objective-kind concept must be no weaker than the seam that stores one");

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

TEST(AggregatePieceConcept, TheObjectiveKindIsNoWeakerThanTheSeamThatStoresOne) {
    EXPECT_TRUE((ObjectiveAggregatePiece<hven::solvers::NLPObjectivePiece,
                                         hven::solvers::SolverIndexingData>));
    EXPECT_TRUE((hven::solvers::ObjectiveAggregateSurface<ContractProbeObjectiveSurfaceOnly,
                                                          ContractProbeIndexData>));
    EXPECT_FALSE(
        (ObjectiveAggregatePiece<ContractProbeObjectiveSurfaceOnly, ContractProbeIndexData>));
    EXPECT_FALSE((ObjectiveAggregatePiece<hven::solvers::NLPConstraintPiece,
                                          hven::solvers::SolverIndexingData>));
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

TEST(AggregateDeclarationTest, RejectsAnInvertedBoundRecord) {
    AggregateDeclaration declaration = consistent_declaration();
    declaration.variable_bounds_.push_back(VariableBound{2, 4.0, 1.0});
    const std::string message = invalid_argument_message(declaration);
    EXPECT_NE(message.find("inverted"), std::string::npos) << message;
    EXPECT_NE(message.find('4'), std::string::npos) << message;
    EXPECT_NE(message.find('1'), std::string::npos) << message;
}

TEST(AggregateDeclarationTest, RejectsABoundHistoryWhoseIntersectionIsEmpty) {
    // Each record is fine on its own; together they leave the variable nothing.
    AggregateDeclaration declaration = consistent_declaration();
    declaration.variable_bounds_.push_back(VariableBound{2, 0.0, 1.0});
    declaration.variable_bounds_.push_back(VariableBound{2, 3.0, 4.0});
    const std::string message = invalid_argument_message(declaration);
    EXPECT_NE(message.find("empty"), std::string::npos) << message;
    EXPECT_NE(message.find('2'), std::string::npos) << message;
}

TEST(AggregateDeclarationTest, AcceptsABoundHistoryThatMerelyNarrows) {
    AggregateDeclaration declaration = consistent_declaration();
    declaration.variable_bounds_.push_back(VariableBound{2, 0.0, 4.0});
    declaration.variable_bounds_.push_back(VariableBound{2, 1.0, 3.0});
    EXPECT_NO_THROW(declaration.validate());
}

TEST(AggregateDeclarationTest, MaterializesOneRecordPerVariableTightestWins) {
    AggregateDeclaration declaration = consistent_declaration();
    declaration.variable_bounds_.push_back(VariableBound{1, 0.0, 4.0});
    declaration.variable_bounds_.push_back(VariableBound{1, 1.0, 3.0});

    const std::vector<VariableBound> materialized = declaration.materialize_variable_bounds();
    ASSERT_EQ(materialized.size(), static_cast<std::size_t>(declaration.primal_vars_));
    for (std::size_t index = 0; index < materialized.size(); ++index) {
        EXPECT_EQ(materialized[index].index_, static_cast<int>(index));
    }
    EXPECT_DOUBLE_EQ(materialized[1].lower_, 1.0);
    EXPECT_DOUBLE_EQ(materialized[1].upper_, 3.0);
    // An undeclared variable materializes unbounded on both sides.
    EXPECT_EQ(materialized[0].lower_, -std::numeric_limits<double>::infinity());
    EXPECT_EQ(materialized[0].upper_, std::numeric_limits<double>::infinity());
}

TEST(AggregateDeclarationTest, MaterializationRejectsWhatValidationRejects) {
    AggregateDeclaration declaration = consistent_declaration();
    declaration.variable_bounds_.push_back(VariableBound{9, 0.0, 1.0});
    EXPECT_THROW(declaration.materialize_variable_bounds(), std::invalid_argument);
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

TEST(CandidateStorageValidation, MultipliersMustBeFullLengthWhereTheyAreRead) {
    Vec x(4);
    Vec none;
    Vec equality(2);
    Vec inequality(3);
    Vec short_inequality(1);

    EXPECT_NO_THROW(
        hven::solvers::validate_full_multipliers(CandidatePoint{x, equality, inequality}, 2, 3));
    // Empty is legal where they are NOT read, and a missing input where they
    // are: this checker is only ever reached on the paths that read them.
    EXPECT_THROW(
        hven::solvers::validate_full_multipliers(CandidatePoint{x, none, inequality}, 2, 3),
        std::invalid_argument);
    EXPECT_THROW(hven::solvers::validate_full_multipliers(CandidatePoint{x, equality, none}, 2, 3),
                 std::invalid_argument);
    EXPECT_THROW(hven::solvers::validate_full_multipliers(
                     CandidatePoint{x, equality, short_inequality}, 2, 3),
                 std::invalid_argument);
}

TEST(CandidateStorageValidation, TheFullMultiplierMessageNamesTheBlockAndBothSizes) {
    Vec x(4);
    Vec none;
    Vec inequality(3);
    try {
        hven::solvers::validate_full_multipliers(CandidatePoint{x, none, inequality}, 2, 3);
        FAIL() << "an empty multiplier block must be rejected where the multipliers are read";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("equality-multiplier"), std::string::npos) << message;
        EXPECT_NE(message.find('0'), std::string::npos) << message;
        EXPECT_NE(message.find('2'), std::string::npos) << message;
    }
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

TEST(KktLocationTableTest, RejectsAClashMarkBelowTheSentinel) {
    // -1 is the only legal negative. A mark like -3 passes any upper-bound
    // check, reaches lock() through a scatter that tested `mark != -1`, and
    // indexes the mutex vector out of bounds -- which is exactly what this
    // constructor exists to make unreachable.
    std::vector<int> locations = {0, 1, 2};
    std::vector<int> clashes = {-1, -3, -1};
    std::vector<std::mutex> locks(1);
    try {
        KktLocationTable table(locations.data(), 3, clashes.data(), 3, &locks);
        FAIL() << "a clash mark below the sentinel must be rejected";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("-3"), std::string::npos) << message;
        EXPECT_NE(message.find("-1"), std::string::npos) << message;
    }
}

TEST(RhsLocationTableTest, RejectsARowBelowTheSentinel) {
    const std::vector<int> rows = {0, -2, 2};
    EXPECT_THROW(RhsLocationTable(rows.data(), static_cast<int>(rows.size())),
                 std::invalid_argument);
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
///
/// WARNING to anyone copying this fixture: `kkt_clashes` is sized at kKktSlots
/// and published with that as its clash count, which reads as though the two
/// were one number. They are not. A location array is indexed by CLAIM SLOT; a
/// clash array is indexed by CANONICAL COLUMN of the assembled matrix. In a
/// real provider those dimensions are unrelated -- a problem has far more
/// claims than columns -- and sizing one from the other would publish a table
/// whose clash lookups run off the end. They coincide here only because this
/// fixture is small enough for a one-to-one slot/column toy layout, and the
/// table's own constructor validates the pair rather than trusting it.
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

/// Named multiplier blocks at full length, for the requests that read them. A
/// point's blocks must outlive the call, so these are named objects rather than
/// temporaries at the call site.
struct ContractProbeMultipliers {
    Vec equality = Vec::Constant(FakeAggregate::kEqualityRows, 0.25);
    Vec inequality = Vec::Constant(FakeAggregate::kInequalityRows, 0.5);
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
    ContractProbeMultipliers multipliers;
    Vec x = Vec::Zero(FakeAggregate::kPrimalVars);

    aggregate.assemble(CandidatePoint{x, multipliers.equality, multipliers.inequality},
                       hven::solvers::kRequestConstraintKkt, out.kkt_view(), out.rhs_view());

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

TEST(AggregateAssembleContract, AnArenaTheRequestDoesNameMayNotBeLeftEmpty) {
    // The complement of the permission above, and the half that makes the
    // type-level guarantee two-sided: a request the entry ACCEPTS has a
    // destination for everything it names. The fake checks nothing of its own,
    // so this refusal can only be the entry's -- and it arrives before the
    // hook, so nothing is half-written.
    FakeAggregate aggregate;
    ContractProbeDestinations out;
    Vec x = Vec::Zero(FakeAggregate::kPrimalVars);
    Vec none;

    RhsScatterView rhs = out.rhs_view();
    rhs.equality_residuals_ = hven::solvers::RhsArenaView{};

    try {
        aggregate.assemble(CandidatePoint{x, none, none},
                           hven::solvers::kRequestObjectiveAndConstraints, out.kkt_view(), rhs);
        FAIL() << "a request naming an arena whose view is empty must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("equality residual"), std::string::npos) << message;
    }
    EXPECT_EQ(out.objective, kUntouchedSentinel);
    EXPECT_TRUE(ContractProbeDestinations::untouched(out.inequality_residuals));
}

TEST(AggregateAssembleContract, AZeroLengthGradientViewIsRefusedEvenWithStorage) {
    // The case a size-match rule would have let through. A residual block may
    // legitimately have zero declared rows, so a zero-length residual view is
    // accepted; a gradient arena has no such case -- an aggregate with no
    // primal variables is not a problem -- so a zero-length view under a
    // request that NAMES the arena is incoherent, and a filler trusting its
    // non-null storage would write past it.
    FakeAggregate aggregate;
    ContractProbeDestinations out;
    Vec x = Vec::Zero(FakeAggregate::kPrimalVars);
    Vec none;

    RhsScatterView rhs = out.rhs_view();
    rhs.objective_gradient_ =
        hven::solvers::RhsArenaView{out.objective_gradient.data(), 0, &out.gradient_table};

    try {
        aggregate.assemble(CandidatePoint{x, none, none},
                           hven::solvers::kRequestObjectiveGradientAndConstraints, out.kkt_view(),
                           rhs);
        FAIL() << "a named gradient arena of zero length must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("objective gradient"), std::string::npos) << message;
        EXPECT_NE(message.find("zero"), std::string::npos) << message;
    }
    EXPECT_TRUE(ContractProbeDestinations::untouched(out.objective_gradient));
}

TEST(AggregateAssembleContract, AKktBearingRequestMayNotBeGivenAnEmptyKktView) {
    // The three KKT-bearing flags share one destination, so any of them names
    // the KKT view. The objective-only shape may leave it empty (above); this
    // shape may not.
    FakeAggregate aggregate;
    ContractProbeDestinations out;
    ContractProbeMultipliers multipliers;
    Vec x = Vec::Zero(FakeAggregate::kPrimalVars);

    EXPECT_THROW(aggregate.assemble(CandidatePoint{x, multipliers.equality, multipliers.inequality},
                                    hven::solvers::kRequestConstraintJacobianOnly, KktScatterView{},
                                    out.rhs_view()),
                 std::invalid_argument);
    EXPECT_TRUE(ContractProbeDestinations::untouched(out.equality_residuals));
}

TEST(AggregateAssembleContract, ABoundProviderRefusesAViewNamingAnotherDestination) {
    // A provider whose location tables are offsets into one particular value
    // array says so, and the entry then holds a view to that array. One pointer
    // comparison, identity only -- it says nothing about whether either array
    // is still alive -- and the refusal names the remedy, because the case it
    // really catches is a consumer whose matrix moved.
    FakeAggregate aggregate;
    ContractProbeDestinations out;
    ContractProbeMultipliers multipliers;
    std::vector<double> elsewhere(ContractProbeDestinations::kKktSlots, 0.0);
    Vec x = Vec::Zero(FakeAggregate::kPrimalVars);

    aggregate.bind_kkt_destination(out.kkt_values.data());
    EXPECT_NO_THROW(aggregate.assemble(
        CandidatePoint{x, multipliers.equality, multipliers.inequality},
        hven::solvers::kRequestConstraintJacobianOnly, out.kkt_view(), out.rhs_view()));

    KktScatterView wrong = out.kkt_view();
    wrong.values_ = elsewhere.data();
    try {
        aggregate.assemble(CandidatePoint{x, multipliers.equality, multipliers.inequality},
                           hven::solvers::kRequestConstraintJacobianOnly, wrong, out.rhs_view());
        FAIL() << "a bound provider must refuse a view naming a different destination";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("re-run the analysis"), std::string::npos) << message;
    }
    EXPECT_EQ(elsewhere[0], 0.0);
}

TEST(AggregateAssembleContract, AnUnboundProviderAcceptsWhateverDestinationItIsGiven) {
    // The default, and what the candidate surface is always in: a provider that
    // binds nothing is never checked here, so nothing in this contract imposes
    // an analysis step on an implementation that does not need one.
    FakeAggregate aggregate;
    ContractProbeDestinations out;
    ContractProbeMultipliers multipliers;
    std::vector<double> elsewhere(ContractProbeDestinations::kKktSlots, 0.0);
    Vec x = Vec::Zero(FakeAggregate::kPrimalVars);

    KktScatterView anywhere = out.kkt_view();
    anywhere.values_ = elsewhere.data();
    EXPECT_NO_THROW(aggregate.assemble(
        CandidatePoint{x, multipliers.equality, multipliers.inequality},
        hven::solvers::kRequestConstraintJacobianOnly, anywhere, out.rhs_view()));
    EXPECT_NE(elsewhere[0], 0.0);
}

TEST(AggregateCandidateContract, TheCandidateEntriesAssignRatherThanAccumulate) {
    // Deliberately the opposite of what assemble does to an arena. Twice at one
    // point returns the values AT that point, not twice them: the caller is a
    // scorer holding a scratch buffer, and requiring it to pre-zero would be an
    // obligation with nothing to buy it -- while a forgotten zero would quietly
    // return the sum of two evaluations.
    FakeAggregate aggregate;
    ContractProbeMultipliers multipliers;
    Vec x = Vec::Constant(FakeAggregate::kPrimalVars, 0.75);
    Vec none;

    double objective = 0.0;
    Vec equality = Vec::Constant(FakeAggregate::kEqualityRows, 99.0);
    Vec inequality = Vec::Constant(FakeAggregate::kInequalityRows, 99.0);
    CandidateValues values{objective, equality, inequality};

    aggregate.evaluate_candidate_values(CandidatePoint{x, none, none}, values);
    const double first_objective = objective;
    const Vec first_equality = equality;

    aggregate.evaluate_candidate_values(CandidatePoint{x, none, none}, values);
    EXPECT_DOUBLE_EQ(objective, first_objective);
    EXPECT_EQ(equality, first_equality);

    Vec objective_gradient = Vec::Constant(FakeAggregate::kPrimalVars, 99.0);
    Vec adjoint_gradient = Vec::Constant(FakeAggregate::kPrimalVars, 99.0);
    CandidateFirstOrder first_order{values, objective_gradient, adjoint_gradient};

    aggregate.evaluate_candidate_first_order(
        CandidatePoint{x, multipliers.equality, multipliers.inequality}, first_order);
    const Vec first_gradient = objective_gradient;

    aggregate.evaluate_candidate_first_order(
        CandidatePoint{x, multipliers.equality, multipliers.inequality}, first_order);
    EXPECT_EQ(objective_gradient, first_gradient);
}

TEST(AggregateAssembleContract, RequestMaskingMovesNeitherLayoutNorDigestNorEpoch) {
    FakeAggregate aggregate;
    ContractProbeDestinations out;
    ContractProbeMultipliers multipliers;
    Vec x = Vec::Zero(FakeAggregate::kPrimalVars);
    Vec none;

    const auto key_before = aggregate.model_structure_key();
    const auto epoch_before = aggregate.structure_epoch();
    const int layout_before = aggregate.layout_serial();

    aggregate.assemble(CandidatePoint{x, none, none}, hven::solvers::kRequestObjectiveOnly,
                       out.kkt_view(), out.rhs_view());
    aggregate.assemble(CandidatePoint{x, multipliers.equality, multipliers.inequality},
                       hven::solvers::kRequestFullKkt, out.kkt_view(), out.rhs_view());

    EXPECT_EQ(aggregate.model_structure_key(), key_before);
    EXPECT_EQ(aggregate.structure_epoch(), epoch_before);
    EXPECT_EQ(aggregate.layout_serial(), layout_before);
}

TEST(AggregateAssembleContract, SuccessiveCallsAccumulateIntoOneDestination) {
    // The provider accumulates and never assigns: the consumer owns its storage
    // AND its initial state. Two identical calls against one destination must
    // therefore land twice, which is also what makes a fan-out over partitions
    // compose.
    FakeAggregate aggregate;
    ContractProbeDestinations out;
    Vec x = Vec::Zero(FakeAggregate::kPrimalVars);
    Vec none;

    aggregate.assemble(CandidatePoint{x, none, none}, hven::solvers::kRequestObjectiveOnly,
                       out.kkt_view(), out.rhs_view());
    const double after_one = out.objective;
    aggregate.assemble(CandidatePoint{x, none, none}, hven::solvers::kRequestObjectiveOnly,
                       out.kkt_view(), out.rhs_view());

    EXPECT_DOUBLE_EQ(out.objective - after_one, after_one - kUntouchedSentinel);
    EXPECT_NE(out.objective, after_one);
}

TEST(AggregateAssembleContract, AnRhsArenaIsAccumulatedIntoNotAssigned) {
    // The same discipline one level in from the scalar slot: a claim landing in
    // an RHS arena adds to whatever the consumer left there.
    FakeAggregate aggregate;
    ContractProbeDestinations out;
    Vec x = Vec::Zero(FakeAggregate::kPrimalVars);
    Vec none;

    aggregate.assemble(CandidatePoint{x, none, none},
                       hven::solvers::kRequestObjectiveGradientAndConstraints, out.kkt_view(),
                       out.rhs_view());
    const Vec gradient_after_one = out.objective_gradient;
    const Vec residuals_after_one = out.equality_residuals;
    ASSERT_FALSE(ContractProbeDestinations::untouched(out.objective_gradient));

    aggregate.assemble(CandidatePoint{x, none, none},
                       hven::solvers::kRequestObjectiveGradientAndConstraints, out.kkt_view(),
                       out.rhs_view());

    // The second pass added the same increment again, entry for entry.
    const Vec gradient_increment =
        gradient_after_one - Vec::Constant(FakeAggregate::kPrimalVars, kUntouchedSentinel);
    EXPECT_TRUE(out.objective_gradient.isApprox(gradient_after_one + gradient_increment));
    const Vec residual_increment =
        residuals_after_one - Vec::Constant(FakeAggregate::kEqualityRows, kUntouchedSentinel);
    EXPECT_TRUE(out.equality_residuals.isApprox(residuals_after_one + residual_increment));
    EXPECT_GT(gradient_increment.cwiseAbs().minCoeff(), 0.0);
}

TEST(AggregateAssembleContract, AKktArenaClaimIsAccumulatedIntoNotAssigned) {
    // And through the KKT location table, which is the one that matters most:
    // several pieces claim slots in one arena and each sums its own
    // contribution in, so a fill that assigned would silently keep only the
    // last writer's value.
    FakeAggregate aggregate;
    ContractProbeDestinations out;
    ContractProbeMultipliers multipliers;
    Vec x = Vec::Zero(FakeAggregate::kPrimalVars);

    aggregate.assemble(CandidatePoint{x, multipliers.equality, multipliers.inequality},
                       hven::solvers::kRequestFullKkt, out.kkt_view(), out.rhs_view());
    ASSERT_FALSE(out.kkt_untouched());
    const std::vector<double> after_one = out.kkt_values;

    aggregate.assemble(CandidatePoint{x, multipliers.equality, multipliers.inequality},
                       hven::solvers::kRequestFullKkt, out.kkt_view(), out.rhs_view());

    for (std::size_t slot = 0; slot < out.kkt_values.size(); ++slot) {
        const double increment = after_one[slot] - kUntouchedSentinel;
        EXPECT_NE(increment, 0.0) << "KKT slot " << slot << " was never written";
        EXPECT_DOUBLE_EQ(out.kkt_values[slot], after_one[slot] + increment)
            << "KKT slot " << slot << " was assigned rather than accumulated into";
    }
}

TEST(AggregateNonVirtualEntryContract, ValidationIsAPropertyOfTheTypeNotOfTheImplementation) {
    // The fake performs NO validation of its own -- its hooks are pure work.
    // Every refusal below therefore comes from the entry, which is the
    // guarantee the non-virtual split buys: a consumer holding any
    // NlpAggregate& knows an unmapped request and a mis-sized point were
    // refused before a value moved, whoever wrote the implementation.
    FakeAggregate aggregate;
    hven::solvers::NlpAggregate &surface = aggregate;
    ContractProbeDestinations out;
    Vec x = Vec::Zero(FakeAggregate::kPrimalVars);
    Vec short_x = Vec::Zero(FakeAggregate::kPrimalVars - 1);
    Vec none;

    EXPECT_THROW(surface.assemble(CandidatePoint{x, none, none},
                                  EvalRequest::kConstraintJacobian | EvalRequest::kObjectiveHessian,
                                  out.kkt_view(), out.rhs_view()),
                 std::invalid_argument);
    EXPECT_THROW(surface.assemble(CandidatePoint{short_x, none, none},
                                  hven::solvers::kRequestObjectiveOnly, out.kkt_view(),
                                  out.rhs_view()),
                 std::invalid_argument);
    EXPECT_TRUE(out.kkt_untouched());
    EXPECT_EQ(out.objective, kUntouchedSentinel);

    double objective = 0.0;
    Vec equality(FakeAggregate::kEqualityRows);
    Vec inequality(FakeAggregate::kInequalityRows);
    Vec wrong_equality(FakeAggregate::kEqualityRows + 1);
    EXPECT_THROW(
        surface.evaluate_candidate_values(CandidatePoint{short_x, none, none},
                                          CandidateValues{objective, equality, inequality}),
        std::invalid_argument);
    EXPECT_THROW(
        surface.evaluate_candidate_values(CandidatePoint{x, none, none},
                                          CandidateValues{objective, wrong_equality, inequality}),
        std::invalid_argument);
}

TEST(AggregateNonVirtualEntryContract, TheProbeInheritsTheValuesEntrysValidation) {
    // A probe is a values evaluation plus a hash, so routing it through the
    // public entry is what gives it that entry's checks rather than a second
    // copy of them.
    FakeAggregate aggregate;
    Vec short_x = Vec::Zero(FakeAggregate::kPrimalVars - 1);
    EXPECT_THROW(aggregate.probe_identity(short_x), std::invalid_argument);
}

TEST(AggregateAssembleContract, AnUnmappedRequestIsRefusedBeforeAnythingIsWritten) {
    FakeAggregate aggregate;
    ContractProbeDestinations out;
    Vec x = Vec::Zero(FakeAggregate::kPrimalVars);
    Vec none;

    const EvalRequest unmapped =
        EvalRequest::kObjectiveValue | EvalRequest::kConstraintAdjointHessian;
    EXPECT_THROW(
        aggregate.assemble(CandidatePoint{x, none, none}, unmapped, out.kkt_view(), out.rhs_view()),
        std::invalid_argument);

    EXPECT_EQ(out.objective, kUntouchedSentinel);
    EXPECT_TRUE(out.kkt_untouched());
}

TEST(AggregateAssembleContract, AnAdjointRequestWithEmptyMultipliersIsRefused) {
    FakeAggregate aggregate;
    ContractProbeDestinations out;
    Vec x = Vec::Zero(FakeAggregate::kPrimalVars);
    Vec none;

    // Legal shape, missing input: the adjoint gradient contracts a derivative
    // against the multipliers, so empty is a forgotten block rather than a zero
    // vector, and serving it would return a wrong adjoint with no diagnostic.
    EXPECT_THROW(aggregate.assemble(CandidatePoint{x, none, none},
                                    hven::solvers::kRequestFirstOrderRhs, out.kkt_view(),
                                    out.rhs_view()),
                 std::invalid_argument);
    EXPECT_TRUE(ContractProbeDestinations::untouched(out.adjoint_gradient));
}

TEST(AggregateAssembleContract, AValuesOnlyRequestStillAcceptsEmptyMultipliers) {
    FakeAggregate aggregate;
    ContractProbeDestinations out;
    Vec x = Vec::Zero(FakeAggregate::kPrimalVars);
    Vec none;

    EXPECT_NO_THROW(aggregate.assemble(CandidatePoint{x, none, none},
                                       hven::solvers::kRequestObjectiveAndConstraints,
                                       out.kkt_view(), out.rhs_view()));
    EXPECT_NO_THROW(aggregate.assemble(CandidatePoint{x, none, none},
                                       hven::solvers::kRequestConstraintJacobianOnly,
                                       out.kkt_view(), out.rhs_view()));
}

TEST(AggregateCandidateContract, FirstOrderAlwaysRequiresFullMultipliers) {
    FakeAggregate aggregate;
    ContractProbeMultipliers multipliers;
    Vec x = Vec::Zero(FakeAggregate::kPrimalVars);
    Vec none;

    double objective = 0.0;
    Vec equality(FakeAggregate::kEqualityRows);
    Vec inequality(FakeAggregate::kInequalityRows);
    Vec objective_gradient(FakeAggregate::kPrimalVars);
    Vec adjoint_gradient(FakeAggregate::kPrimalVars);
    CandidateFirstOrder storage{CandidateValues{objective, equality, inequality},
                                objective_gradient, adjoint_gradient};

    EXPECT_THROW(aggregate.evaluate_candidate_first_order(CandidatePoint{x, none, none}, storage),
                 std::invalid_argument);
    EXPECT_NO_THROW(aggregate.evaluate_candidate_first_order(
        CandidatePoint{x, multipliers.equality, multipliers.inequality}, storage));
}

TEST(AggregateCandidateContract, TheValuesPathStillAcceptsEmptyMultipliers) {
    FakeAggregate aggregate;
    Vec x = Vec::Zero(FakeAggregate::kPrimalVars);
    Vec none;

    double objective = 0.0;
    Vec equality(FakeAggregate::kEqualityRows);
    Vec inequality(FakeAggregate::kInequalityRows);
    CandidateValues storage{objective, equality, inequality};

    EXPECT_NO_THROW(aggregate.evaluate_candidate_values(CandidatePoint{x, none, none}, storage));
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
