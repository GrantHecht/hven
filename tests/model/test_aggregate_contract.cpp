// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The rest of the contract surface: the piece concepts, the declaration's
// boundary validation and bound materialization, the published location tables,
// the scatter views, and the rules a request carries -- only the eight mapped
// shapes are legal, only what the request names is written, destinations are
// accumulated into, multipliers are required where they are read, and masking a
// request never moves layout, digest or epoch.

#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "hven/detail/interior/fixed_variable_row.h"
#include "hven/detail/interior/indexing_data.h"
#include "hven/detail/model/nlp_adapter.h"
#include "hven/model/aggregate_declaration.h"
#include "hven/model/candidate_point.h"
#include "hven/model/claim_space.h"
#include "hven/model/nlp_aggregate.h"
#include "hven/model/non_linear_program.h"
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

TEST(AggregateDeclarationTest, AcceptsRowCountsWhenNoPiecesAreDeclared) {
    // The piece-sum conjunct is the one conditional check, and it is conditional
    // on there being pieces. A provider that is not a piece collection -- a
    // bridge over a single model -- declares rows and no pieces, and there is
    // then no sum for the row counts to disagree with. Every other check still
    // applies to such a declaration. The counterpart, where the pieces exist and
    // the sum is wrong, is pinned against a real piece-sourced declaration in the
    // engine's own aggregate suite.
    AggregateDeclaration declaration = consistent_declaration();
    declaration.equality_rows_ = 3;
    declaration.inequality_rows_ = 5;
    EXPECT_NO_THROW(declaration.validate());
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
                                    hven::solvers::kRequestConstraintResidualsAndJacobian,
                                    KktScatterView{}, out.rhs_view()),
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
        hven::solvers::kRequestConstraintResidualsAndJacobian, out.kkt_view(), out.rhs_view()));

    KktScatterView wrong = out.kkt_view();
    wrong.values_ = elsewhere.data();
    try {
        aggregate.assemble(CandidatePoint{x, multipliers.equality, multipliers.inequality},
                           hven::solvers::kRequestConstraintResidualsAndJacobian, wrong,
                           out.rhs_view());
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
        hven::solvers::kRequestConstraintResidualsAndJacobian, anywhere, out.rhs_view()));
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
                                       hven::solvers::kRequestConstraintResidualsAndJacobian,
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

/// A scalar objective over one variable per application, written out here
/// because the declaration round trip has to carry an OBJECTIVE list as well
/// as the two constraint lists, and the pieces this suite otherwise builds are
/// constraints.
///
/// Structurally linear, so it claims no KKT slot -- the objective claim pass
/// asks for Hessian space only, and a linear objective has none. Nothing here
/// is evaluated: these tests lay layouts and read declarations.
///
/// At namespace scope rather than in the anonymous block below because the
/// erasure seam's adapter has to be specialized on it, and a specialization
/// belongs at the namespace scope of the template it specializes.
struct AdoptDeclarationObjectivePiece {
    std::string name() const { return "AdoptDeclarationObjective"; }
    int input_rows() const { return 1; }
    int output_rows() const { return 1; }
    bool thread_safe() const { return true; }

    int num_kkt_elements(bool dojac, bool) const { return dojac ? 1 : 0; }

    void get_kkt_space(Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>, int &freeloc, int,
                       bool dojac, bool, hven::solvers::SolverIndexingData &data) {
        data.inner_kkt_starts_.resize(data.num_appl());
        for (int v = 0; v < data.num_appl(); v++) {
            data.inner_kkt_starts_[v] = freeloc;
            if (dojac) {
                freeloc++;
            }
        }
    }

    void constraints(const Eigen::Ref<const Eigen::VectorXd> &, Eigen::Ref<Eigen::VectorXd>,
                     const hven::solvers::SolverIndexingData &) const {}
    void constraints_adjointgradient(const Eigen::Ref<const Eigen::VectorXd> &,
                                     const Eigen::Ref<const Eigen::VectorXd> &,
                                     Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>,
                                     const hven::solvers::SolverIndexingData &) const {}
    void constraints_jacobian(const Eigen::Ref<const Eigen::VectorXd> &,
                              Eigen::Ref<Eigen::VectorXd>,
                              Eigen::SparseMatrix<double, Eigen::RowMajor> &,
                              Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>,
                              std::vector<std::mutex> &,
                              const hven::solvers::SolverIndexingData &) const {}
    void constraints_jacobian_adjointgradient(
        const Eigen::Ref<const Eigen::VectorXd> &, const Eigen::Ref<const Eigen::VectorXd> &,
        Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &, Eigen::Ref<Eigen::VectorXi>,
        Eigen::Ref<Eigen::VectorXi>, std::vector<std::mutex> &,
        const hven::solvers::SolverIndexingData &) const {}
    void constraints_jacobian_adjointgradient_adjointhessian(
        const Eigen::Ref<const Eigen::VectorXd> &, const Eigen::Ref<const Eigen::VectorXd> &,
        Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &, Eigen::Ref<Eigen::VectorXi>,
        Eigen::Ref<Eigen::VectorXi>, std::vector<std::mutex> &,
        const hven::solvers::SolverIndexingData &) const {}

    void objective(double, const Eigen::Ref<const Eigen::VectorXd> &, double &,
                   const hven::solvers::SolverIndexingData &) const {}
    void objective_gradient(double, const Eigen::Ref<const Eigen::VectorXd> &, double &,
                            Eigen::Ref<Eigen::VectorXd>,
                            const hven::solvers::SolverIndexingData &) const {}
    void objective_gradient_hessian(double, const Eigen::Ref<const Eigen::VectorXd> &, double &,
                                    Eigen::Ref<Eigen::VectorXd>,
                                    Eigen::SparseMatrix<double, Eigen::RowMajor> &,
                                    Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>,
                                    std::vector<std::mutex> &,
                                    const hven::solvers::SolverIndexingData &) const {}
};

namespace hven::solvers {
template <>
struct SolverInterfaceAdapter<AdoptDeclarationObjectivePiece>
    : DirectFunctionModel<AdoptDeclarationObjectivePiece> {};
} // namespace hven::solvers

// ---------------------------------------------------------------------------
// Adopting a declaration
//
// The other direction of the declaration: a provider hands one out through
// declaration(), and this is the entry that lays a problem FROM one. The pins
// below are what makes the pair a round trip -- an adopted declaration lays the
// same structures the declaration was read off, fixing rows included -- and
// what makes the per-piece thread mode declaration data rather than a field a
// caller writes whenever it likes.
// ---------------------------------------------------------------------------

namespace {

using hven::solvers::ConstraintFunction;
using hven::solvers::ConstraintInterface;
using hven::solvers::FixedVariableRow;
using hven::solvers::FixedVariableTreatments;
using hven::solvers::ModelStructureKey;
using hven::solvers::NonLinearProgram;
using hven::solvers::ObjectiveFunction;
using hven::solvers::ObjectiveInterface;
using hven::solvers::ThreadingFlags;

/// Applications per piece, and pieces. The product is the KKT element count,
/// which has to clear kMinKktElementsPerPartition per partition or the lay's
/// cap collapses the problem to one partition and no thread mode can move a
/// claim anywhere.
constexpr int kAdoptApplications = 1050;
constexpr int kAdoptPieces = 3;
constexpr int kAdoptRows = kAdoptApplications * kAdoptPieces;

/// One equality piece pinning kAdoptApplications consecutive variables, one
/// application per row. Cheap to lay and it claims exactly one KKT slot per
/// application, which is what makes the claim stream easy to read.
ConstraintFunction adopt_pin_piece(int first_index) {
    Eigen::MatrixXi v_index(1, kAdoptApplications);
    Eigen::MatrixXi c_index(1, kAdoptApplications);
    for (int k = 0; k < kAdoptApplications; k++) {
        v_index(0, k) = first_index + k;
        c_index(0, k) = first_index + k;
    }
    return ConstraintFunction(ConstraintInterface(FixedVariableRow(0.25)), v_index, c_index);
}

/// Which of the three piece lists a fixture populates. The equality-only shape
/// is what the claim-order tests read, because its stream is one block per
/// piece; the three-list shape is what the round trip needs, since a
/// declaration carries all three lists and each piece's thread mode.
enum class AdoptPieceKinds { kEqualitiesOnly, kAllThreeLists };

/// Applications in the inequality piece of the three-list shape.
constexpr int kAdoptInequalityRows = 64;
/// Applications in the objective piece of the three-list shape.
constexpr int kAdoptObjectiveApplications = 16;

/// One objective piece over the first kAdoptObjectiveApplications variables,
/// one variable per application.
ObjectiveFunction adopt_objective_piece() {
    Eigen::MatrixXi v_index(1, kAdoptObjectiveApplications);
    for (int k = 0; k < kAdoptObjectiveApplications; k++) {
        v_index(0, k) = k;
    }
    return ObjectiveFunction(ObjectiveInterface(AdoptDeclarationObjectivePiece()), v_index);
}

/// One inequality piece over the first kAdoptInequalityRows variables.
ConstraintFunction adopt_inequality_piece() {
    Eigen::MatrixXi v_index(1, kAdoptInequalityRows);
    Eigen::MatrixXi c_index(1, kAdoptInequalityRows);
    for (int k = 0; k < kAdoptInequalityRows; k++) {
        v_index(0, k) = k;
        c_index(0, k) = k;
    }
    return ConstraintFunction(ConstraintInterface(FixedVariableRow(-0.5)), v_index, c_index);
}

/// A laid problem of kAdoptPieces equality pieces, at the given thread modes and
/// requested partition count, with a two-sided bound on every variable. When
/// @p fix_one is true the bounds on one variable are narrowed to a point, which
/// is what a fixed-variable treatment turns into an internal fixing row.
std::shared_ptr<NonLinearProgram> adopt_build(const std::vector<ThreadingFlags> &modes,
                                              int requested_partitions, bool fix_one,
                                              AdoptPieceKinds kinds) {
    auto nlp = std::make_shared<NonLinearProgram>(requested_partitions);
    for (int p = 0; p < kAdoptPieces; p++) {
        ConstraintFunction piece = adopt_pin_piece(p * kAdoptApplications);
        piece.set_thread_mode(modes[static_cast<std::size_t>(p)]);
        nlp->equality_constraints_.push_back(std::move(piece));
    }

    int inequality_rows = 0;
    if (kinds == AdoptPieceKinds::kAllThreeLists) {
        ObjectiveFunction objective = adopt_objective_piece();
        objective.set_thread_mode(ThreadingFlags::MainThread);
        nlp->objectives_.push_back(std::move(objective));

        ConstraintFunction inequality = adopt_inequality_piece();
        inequality.set_thread_mode(ThreadingFlags::RoundRobin);
        nlp->inequality_constraints_.push_back(std::move(inequality));
        inequality_rows = kAdoptInequalityRows;
    }

    for (int i = 0; i < kAdoptRows; i++) {
        nlp->set_variable_bound(i, -1.0, 2.0);
    }
    if (fix_one) {
        nlp->set_variable_bound(7, 0.25, 0.25);
    }
    nlp->make_nlp(kAdoptRows, kAdoptRows, inequality_rows);
    return nlp;
}

/// The equality-only shape, which is what most of these tests read.
std::shared_ptr<NonLinearProgram> adopt_build(const std::vector<ThreadingFlags> &modes,
                                              int requested_partitions, bool fix_one) {
    return adopt_build(modes, requested_partitions, fix_one, AdoptPieceKinds::kEqualitiesOnly);
}

/// Whether the lay marker's two entries can be reached from here -- which is
/// to say, from anywhere that is not the type that lays layouts.
///
/// Access is part of what makes a requires-expression satisfied, so these
/// answer "is it accessible?", not "does it exist?". They must be FALSE: a
/// reachable pair would make the post-lay refusal advisory, since a thaw
/// followed by a write moves a laid piece's mode while the structural key, the
/// structure epoch and the claim arrays all stand still, and the size-only
/// guard on the declaration readers would not see the difference.
///
/// A negative compile-time pin is spelling-sensitive by nature: were an entry
/// renamed in the piece types, these would go on passing for the wrong reason.
/// The setter check below is the control that the expression FORM is right;
/// the names themselves are held in step by the library, which calls them.
template <class Piece>
concept AdoptCanFreezeThreadMode = requires(Piece &piece) { piece.mark_thread_mode_laid(); };

template <class Piece>
concept AdoptCanThawThreadMode = requires(Piece &piece) { piece.clear_thread_mode_laid(); };

template <class Piece>
concept AdoptCanSetThreadMode =
    requires(Piece &piece) { piece.set_thread_mode(ThreadingFlags::MainThread); };

static_assert(!AdoptCanFreezeThreadMode<ConstraintFunction>);
static_assert(!AdoptCanFreezeThreadMode<ObjectiveFunction>);
static_assert(!AdoptCanThawThreadMode<ConstraintFunction>);
static_assert(!AdoptCanThawThreadMode<ObjectiveFunction>);

// The control: a member call of exactly this shape IS satisfiable on the same
// types, so the four assertions above are about access and not about a
// requires-expression that could never be satisfied by anything.
static_assert(AdoptCanSetThreadMode<ConstraintFunction>);
static_assert(AdoptCanSetThreadMode<ObjectiveFunction>);

std::vector<ThreadingFlags> adopt_default_modes() {
    return {ThreadingFlags::Thread0, ThreadingFlags::Thread1, ThreadingFlags::MainThread};
}

/// Every field of a declaration a round trip has to reproduce: the dimensions,
/// the fixing-row count, the adopted partition count, the bound records
/// verbatim, and the three piece lists with each piece's thread mode.
void adopt_expect_same_declaration(const AggregateDeclaration &adopted,
                                   const AggregateDeclaration &source) {
    EXPECT_EQ(adopted.primal_vars_, source.primal_vars_);
    EXPECT_EQ(adopted.equality_rows_, source.equality_rows_);
    EXPECT_EQ(adopted.inequality_rows_, source.inequality_rows_);
    EXPECT_EQ(adopted.fixing_rows_, source.fixing_rows_);
    EXPECT_EQ(adopted.partition_count_, source.partition_count_);
    EXPECT_EQ(adopted.variable_bounds_, source.variable_bounds_);
    ASSERT_EQ(adopted.objectives_.size(), source.objectives_.size());
    ASSERT_EQ(adopted.equality_constraints_.size(), source.equality_constraints_.size());
    ASSERT_EQ(adopted.inequality_constraints_.size(), source.inequality_constraints_.size());
    for (std::size_t k = 0; k < source.equality_constraints_.size(); k++) {
        EXPECT_EQ(adopted.equality_constraints_[k].get_thread_mode(),
                  source.equality_constraints_[k].get_thread_mode())
            << "equality piece " << k;
    }
    for (std::size_t k = 0; k < source.objectives_.size(); k++) {
        EXPECT_EQ(adopted.objectives_[k].get_thread_mode(), source.objectives_[k].get_thread_mode())
            << "objective piece " << k;
    }
    for (std::size_t k = 0; k < source.inequality_constraints_.size(); k++) {
        EXPECT_EQ(adopted.inequality_constraints_[k].get_thread_mode(),
                  source.inequality_constraints_[k].get_thread_mode())
            << "inequality piece " << k;
    }
}

/// The claim stream as laid: the two index arrays over the user elements, in
/// claim order.
std::vector<int> adopt_claim_stream(const NonLinearProgram &nlp) {
    std::vector<int> stream;
    stream.reserve(static_cast<std::size_t>(2 * nlp.num_user_kkt_elems_));
    for (int i = 0; i < nlp.num_user_kkt_elems_; i++) {
        stream.push_back(nlp.kkt_coeff_rows_[i]);
        stream.push_back(nlp.kkt_coeff_cols_[i]);
    }
    return stream;
}

} // namespace

TEST(AdoptDeclaration, LaysWhatAHandAssembledProblemLays) {
    auto assembled = adopt_build(adopt_default_modes(), 8, false);
    const AggregateDeclaration declaration = assembled->declaration();

    NonLinearProgram adopted(1);
    adopted.adopt_declaration(declaration);

    EXPECT_EQ(adopted.model_structure_key(), assembled->model_structure_key());
    EXPECT_EQ(adopted.num_partitions_, assembled->num_partitions_);
    EXPECT_EQ(adopt_claim_stream(adopted), adopt_claim_stream(*assembled));

    adopt_expect_same_declaration(adopted.declaration(), declaration);
}

TEST(AdoptDeclaration, RoundTripsExactlyWithFixingRowsInstalled) {
    // All three lists populated, so the round trip is over a declaration that
    // carries objective, equality and inequality pieces with their own thread
    // modes -- not over the equality list alone.
    auto source = adopt_build(adopt_default_modes(), 8, true, AdoptPieceKinds::kAllThreeLists);
    ASSERT_TRUE(source->configure_variable_treatment(FixedVariableTreatments::MakeConstraint, 0.0));
    ASSERT_EQ(source->internal_fixed_constraints(), 1);

    const AggregateDeclaration declaration = source->declaration();
    // The declaration is self-describing: the row count is the row space AS
    // LAID and the fixing-row count says how much of it is internal.
    ASSERT_EQ(declaration.equality_rows_, kAdoptRows + 1);
    ASSERT_EQ(declaration.inequality_rows_, kAdoptInequalityRows);
    ASSERT_EQ(declaration.fixing_rows_, 1);
    ASSERT_EQ(declaration.objectives_.size(), 1u);
    ASSERT_EQ(declaration.inequality_constraints_.size(), 1u);

    NonLinearProgram adopted(1);
    adopted.adopt_declaration(declaration);

    EXPECT_EQ(adopted.model_structure_key(), source->model_structure_key());
    EXPECT_EQ(adopt_claim_stream(adopted), adopt_claim_stream(*source));
    EXPECT_EQ(adopted.equal_cons_, source->equal_cons_);
    EXPECT_EQ(adopted.internal_fixed_constraints(), source->internal_fixed_constraints());
    adopt_expect_same_declaration(adopted.declaration(), declaration);

    // And the trip closes: what the ADOPTER declares is itself adoptable, and
    // lays the same thing again. That is the property the self-describing
    // fixing-row count buys -- a declaration read off a provider that never
    // ran a treatment of its own still says how much of its row space is
    // internal.
    const AggregateDeclaration readopted_declaration = adopted.declaration();
    NonLinearProgram readopted(1);
    readopted.adopt_declaration(readopted_declaration);
    EXPECT_EQ(readopted.model_structure_key(), source->model_structure_key());
    EXPECT_EQ(adopt_claim_stream(readopted), adopt_claim_stream(*source));
    EXPECT_EQ(readopted.internal_fixed_constraints(), source->internal_fixed_constraints());
    adopt_expect_same_declaration(readopted.declaration(), declaration);
}

TEST(AdoptDeclaration, ReportsTheAdoptedPartitionCountNotTheRequestedOne) {
    auto assembled = adopt_build(adopt_default_modes(), 1, false);
    AggregateDeclaration declaration = assembled->declaration();
    declaration.partition_count_ = 64;

    NonLinearProgram adopted(1);
    adopted.adopt_declaration(std::move(declaration));

    // 64 partitions is far more work than the problem has to hand out, so the
    // lay's cap brings the count down -- and it is the ADOPTED count that
    // reaches the declaration and the key.
    EXPECT_LT(adopted.declaration().partition_count_, 64);
    EXPECT_EQ(adopted.declaration().partition_count_, adopted.num_partitions_);
    EXPECT_EQ(adopted.model_structure_key().partition_count_, adopted.num_partitions_);
}

TEST(AdoptDeclaration, ClaimOrderIsAFunctionOfTheDeclaredThreadModes) {
    auto assembled = adopt_build(adopt_default_modes(), 8, false);
    const AggregateDeclaration declaration = assembled->declaration();

    NonLinearProgram first(1);
    first.adopt_declaration(declaration);
    NonLinearProgram second(1);
    second.adopt_declaration(declaration);
    EXPECT_EQ(adopt_claim_stream(first), adopt_claim_stream(second));
    EXPECT_EQ(first.model_structure_key(), second.model_structure_key());

    // The same pieces, the same bounds, the same partition count -- and the
    // LAST piece declared onto the FIRST partition, so it claims ahead of the
    // piece that used to sit between them. Claims are handed out partition by
    // partition, so that is what a mode change moves: a mode that keeps a
    // piece's position in the partition order leaves the stream alone.
    AggregateDeclaration moved = declaration;
    moved.equality_constraints_[kAdoptPieces - 1].set_thread_mode(ThreadingFlags::Thread0);

    const hven::solvers::StructureEpoch before = second.structure_epoch();
    second.adopt_declaration(std::move(moved));
    EXPECT_FALSE(second.structure_epoch() == before);
    EXPECT_FALSE(second.model_structure_key() == first.model_structure_key());

    // WHICH claims moved, and not merely that some did. Each piece claims one
    // slot per application, so the stream is three equal blocks in partition
    // order: the piece whose mode did not change claims where it did, and the
    // one that moved claims where the piece between them used to.
    const std::vector<int> unchanged = adopt_claim_stream(first);
    const std::vector<int> reordered = adopt_claim_stream(second);
    ASSERT_EQ(reordered.size(), unchanged.size());
    const std::size_t block_size = 2 * static_cast<std::size_t>(kAdoptApplications);
    ASSERT_EQ(unchanged.size(), block_size * kAdoptPieces);
    auto block = [&](const std::vector<int> &stream, std::size_t index) {
        return std::vector<int>(stream.begin() + static_cast<std::ptrdiff_t>(index * block_size),
                                stream.begin() +
                                    static_cast<std::ptrdiff_t>((index + 1) * block_size));
    };
    EXPECT_EQ(block(reordered, 0), block(unchanged, 0));
    EXPECT_EQ(block(reordered, 1), block(unchanged, 2));
    EXPECT_EQ(block(reordered, 2), block(unchanged, 1));
}

TEST(AdoptDeclaration, AThreadModeWriteAfterTheLayIsRefused) {
    auto assembled = adopt_build(adopt_default_modes(), 8, false);

    try {
        assembled->equality_constraints_[0].set_thread_mode(ThreadingFlags::RoundRobin);
        FAIL() << "a thread-mode write on a laid piece must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("FixedVariableRow"), std::string::npos) << message;
        EXPECT_NE(message.find("set_thread_mode"), std::string::npos) << message;
    }

    // The partition copies too. They are pieces of the same laid layout, and
    // they are frozen after the partitioning rather than inheriting whatever
    // marker their master carried when the copy was taken -- so the answer does
    // not depend on whether this was a first lay or a re-lay.
    ASSERT_FALSE(assembled->part_eq_.empty());
    ASSERT_FALSE(assembled->part_eq_[0].empty());
    EXPECT_THROW(assembled->part_eq_[0][0].set_thread_mode(ThreadingFlags::RoundRobin),
                 std::invalid_argument);

    // The copy a declaration hands out is declaration data, not part of a
    // layout, so it takes a new mode -- which is the one route to changing one.
    AggregateDeclaration declaration = assembled->declaration();
    EXPECT_NO_THROW(
        declaration.equality_constraints_[0].set_thread_mode(ThreadingFlags::RoundRobin));
}

TEST(AdoptDeclaration, ARefusedAdoptionLeavesTheProblemUnchanged) {
    auto target = adopt_build(adopt_default_modes(), 8, false);
    const AggregateDeclaration before_declaration = target->declaration();
    const ModelStructureKey before_key = target->model_structure_key();
    const std::vector<int> before_stream = adopt_claim_stream(*target);
    const hven::solvers::StructureEpoch before_epoch = target->structure_epoch();
    const int before_equality_rows = target->equal_cons_;
    const int before_internal = target->internal_fixed_constraints();
    const int before_partitions = target->num_partitions_;

    // A fixing-row count the equality tail cannot supply: a fixing row is one
    // piece claiming one row, and this declaration's last piece claims every
    // one of its many.
    AggregateDeclaration malformed = before_declaration;
    malformed.fixing_rows_ = 1;
    // Same three list SIZES as the layout on hand, different CONTENTS: the
    // size-only guard on the declaration readers cannot tell these lists apart
    // from the laid ones, so if the refusal came after the move the target
    // would report this mode under the old key and the old epoch.
    malformed.equality_constraints_[0].set_thread_mode(ThreadingFlags::MainThread);
    try {
        target->adopt_declaration(malformed);
        FAIL() << "a fixing-row count the equality tail cannot supply must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("fixing row"), std::string::npos) << message;
        EXPECT_NE(message.find(std::to_string(kAdoptApplications)), std::string::npos) << message;
    }

    // The refusal is the DECLARATION's, made before anything moved, so the
    // problem on hand is what it was -- down to its layout and its epoch.
    adopt_expect_same_declaration(target->declaration(), before_declaration);
    EXPECT_EQ(target->model_structure_key(), before_key);
    EXPECT_EQ(adopt_claim_stream(*target), before_stream);
    EXPECT_TRUE(target->structure_epoch() == before_epoch);
    EXPECT_EQ(target->equal_cons_, before_equality_rows);
    EXPECT_EQ(target->internal_fixed_constraints(), before_internal);
    EXPECT_EQ(target->num_partitions_, before_partitions);
}

TEST(AdoptDeclaration, AModeSetOnTheDeclarationSurvivesTheLayAndIsFrozenAgain) {
    auto provider = adopt_build(adopt_default_modes(), 8, false);

    // Read the declaration, write a mode on its copy -- which is the one route
    // to changing one -- and adopt it back.
    AggregateDeclaration redeclared = provider->declaration();
    ASSERT_EQ(redeclared.equality_constraints_[0].get_thread_mode(), ThreadingFlags::Thread0);
    redeclared.equality_constraints_[0].set_thread_mode(ThreadingFlags::Thread1);
    provider->adopt_declaration(std::move(redeclared));

    // The lay carries the declared mode, and reports it back.
    EXPECT_EQ(provider->equality_constraints_[0].get_thread_mode(), ThreadingFlags::Thread1);
    EXPECT_EQ(provider->declaration().equality_constraints_[0].get_thread_mode(),
              ThreadingFlags::Thread1);

    // And freezes it again: the piece the layout holds refuses a write, while
    // the copy the declaration hands out still takes one.
    EXPECT_THROW(provider->equality_constraints_[0].set_thread_mode(ThreadingFlags::MainThread),
                 std::invalid_argument);
    AggregateDeclaration again = provider->declaration();
    EXPECT_NO_THROW(again.equality_constraints_[0].set_thread_mode(ThreadingFlags::MainThread));
}

TEST(AdoptDeclaration, RefusesADeclarationWithRowsAndNoPiecesToClaimThem) {
    auto target = adopt_build(adopt_default_modes(), 8, false);
    const ModelStructureKey before_key = target->model_structure_key();

    // What a provider that is not a piece collection declares: dimensions with
    // no pieces behind them. The declaration TYPE accepts it -- its piece-sum
    // conjunct has no sum to check -- and this entry must not, because it lays
    // the problem out of the pieces.
    AggregateDeclaration piece_less;
    piece_less.primal_vars_ = 4;
    piece_less.equality_rows_ = 2;
    piece_less.inequality_rows_ = 1;
    EXPECT_NO_THROW(piece_less.validate());

    try {
        target->adopt_declaration(piece_less);
        FAIL() << "a declaration with rows and no pieces must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("adopt_declaration"), std::string::npos) << message;
        EXPECT_NE(message.find("no pieces"), std::string::npos) << message;
    }
    EXPECT_EQ(target->model_structure_key(), before_key);

    // Rows and pieces both absent is a different thing and is adoptable: there
    // is nothing to claim and nothing claiming it.
    AggregateDeclaration empty;
    empty.primal_vars_ = 4;
    EXPECT_NO_THROW(target->adopt_declaration(empty));
}

TEST(AdoptDeclaration, RefusesAFixingRowThatWritesOutsideTheInternalBand) {
    auto target = adopt_build(adopt_default_modes(), 8, false);
    const AggregateDeclaration before_declaration = target->declaration();
    const ModelStructureKey before_key = target->model_structure_key();

    // A tail piece of exactly the shape a fixing row has -- one piece, one row
    // -- but writing a row the transcription declared. The declared count is
    // trusted, so the shape check cannot tell this from a real fixing row; the
    // row it writes can.
    AggregateDeclaration mislabelled = before_declaration;
    mislabelled.equality_constraints_.push_back(hven::solvers::make_fixed_variable_row(0, 0.0, 0));
    mislabelled.equality_rows_ = kAdoptRows + 1;
    mislabelled.fixing_rows_ = 1;
    EXPECT_NO_THROW(mislabelled.validate());

    try {
        target->adopt_declaration(mislabelled);
        FAIL() << "a declared fixing row writing a user row must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("adopt_declaration"), std::string::npos) << message;
        EXPECT_NE(message.find("band"), std::string::npos) << message;
    }

    adopt_expect_same_declaration(target->declaration(), before_declaration);
    EXPECT_EQ(target->model_structure_key(), before_key);
}

TEST(AdoptDeclaration, TheNextTreatmentRederivesTheFixingRowsTheAdoptionCarried) {
    auto source = adopt_build(adopt_default_modes(), 8, true, AdoptPieceKinds::kAllThreeLists);
    ASSERT_TRUE(source->configure_variable_treatment(FixedVariableTreatments::MakeConstraint, 0.0));
    ASSERT_EQ(source->internal_fixed_constraints(), 1);

    NonLinearProgram adopted(1);
    adopted.adopt_declaration(source->declaration());
    ASSERT_EQ(adopted.internal_fixed_constraints(), 1);
    const std::size_t equality_pieces = adopted.equality_constraints_.size();

    // The rows came across; the classification they were derived from did not.
    EXPECT_EQ(adopted.fixed_variable_indices().size(), 0);

    // The next configuration discards them and derives its own from the bounds
    // the adoption replayed -- landing on the same row space, with the user
    // pieces untouched.
    EXPECT_TRUE(adopted.configure_variable_treatment(FixedVariableTreatments::MakeConstraint, 0.0));
    EXPECT_EQ(adopted.internal_fixed_constraints(), 1);
    ASSERT_EQ(adopted.fixed_variable_indices().size(), 1);
    EXPECT_EQ(adopted.fixed_variable_indices()[0], 7);
    EXPECT_EQ(adopted.equality_constraints_.size(), equality_pieces);
    EXPECT_EQ(adopted.user_equal_cons_, kAdoptRows);
    EXPECT_EQ(adopted.equal_cons_, source->equal_cons_);
    EXPECT_EQ(adopted.model_structure_key(), source->model_structure_key());
}

TEST(AdoptDeclaration, TheLayMarkerEntriesAreNotReachableFromOutsideTheLay) {
    // The substance is in the static_asserts beside adopt_default_modes(); this
    // case exists so a regression in them is reported as a named failure rather
    // than only as a build break -- and because what they rule out is the one
    // route by which a laid piece's mode could move with no diagnostic
    // anywhere: thaw the marker, then write the mode.
    EXPECT_FALSE(AdoptCanFreezeThreadMode<ConstraintFunction>);
    EXPECT_FALSE(AdoptCanFreezeThreadMode<ObjectiveFunction>);
    EXPECT_FALSE(AdoptCanThawThreadMode<ConstraintFunction>);
    EXPECT_FALSE(AdoptCanThawThreadMode<ObjectiveFunction>);
    EXPECT_TRUE(AdoptCanSetThreadMode<ConstraintFunction>);
    EXPECT_TRUE(AdoptCanSetThreadMode<ObjectiveFunction>);
}

TEST(AggregateDeclarationValidation, RefusesAFixingRowCountThatIsNotPartOfTheEqualityRows) {
    AggregateDeclaration declaration;
    declaration.primal_vars_ = 2;
    declaration.equality_rows_ = 3;
    declaration.fixing_rows_ = 4;
    EXPECT_THROW(declaration.validate(), std::invalid_argument);

    declaration.fixing_rows_ = -1;
    EXPECT_THROW(declaration.validate(), std::invalid_argument);

    // In range and still refused: a positive count says there are specific
    // pieces at the equality tail to be lifted off, and this declaration
    // carries none.
    declaration.fixing_rows_ = 3;
    EXPECT_THROW(declaration.validate(), std::invalid_argument);

    declaration.fixing_rows_ = 0;
    EXPECT_NO_THROW(declaration.validate());
}
