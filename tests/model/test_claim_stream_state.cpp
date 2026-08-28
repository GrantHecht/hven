// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// WHEN THE PUBLISHED CLAIM STREAM IS RETAINED, WHEN IT IS REBUILT, AND WHEN IT
// IS REFUSED -- the state machine behind the views, and the refusals that keep
// a consumer from reading a stream that no longer describes anything.
//
// The question the layout asks at every re-lay is one comparison: does the stamp
// the published stream was built against still hold? The tests below drive every
// route that answers it differently -- an elimination that changes nothing
// declared, a partition renegotiation that changes the claim ORDER, a treatment
// that appends internal rows, an adoption that lays twice, a re-lay over swapped
// pieces at identical counts -- and pin the two epochs against each other, which
// is the only way a consumer can tell retention from rebuild.
//
// The restatement's own refusals are driven directly against the free function,
// because a refusal reachable only by corrupting a laid layout's public members
// is one no test could otherwise construct.

#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <Eigen/Core>

#include "hven/detail/model/claim_restatement.h"
#include "hven/model/aggregate_declaration.h"
#include "hven/model/non_linear_program.h"

#include "support/claim_corpus.h"

using hven::model_tests::build_corpus;
using hven::model_tests::CorpusCase;
using hven::model_tests::CorpusConstraintPiece;
using hven::model_tests::reference_restatement;
using hven::solvers::AggregateDeclaration;
using hven::solvers::ClaimBlock;
using hven::solvers::FixedVariableTreatments;
using hven::solvers::NonLinearProgram;
using hven::solvers::StructureEpoch;
using hven::solvers::ThreadingFlags;

namespace {

CorpusCase state_case() {
    CorpusCase fixture;
    fixture.name_ = "state fixture";
    fixture.objective_pieces_ = 1;
    fixture.equality_pieces_ = 2;
    fixture.inequality_pieces_ = 1;
    fixture.applications_ = 10;
    fixture.fixed_variables_ = {4, 9};
    return fixture;
}

/// Room for several partitions, which the cap otherwise refuses.
CorpusCase wide_case() {
    CorpusCase fixture;
    fixture.name_ = "wide state fixture";
    fixture.objective_pieces_ = 1;
    fixture.equality_pieces_ = 3;
    fixture.inequality_pieces_ = 2;
    fixture.applications_ = 600;
    fixture.requested_partitions_ = 1;
    fixture.equality_mode_ = ThreadingFlags::RoundRobin;
    fixture.inequality_mode_ = ThreadingFlags::RoundRobin;
    fixture.fixed_variables_ = {2};
    return fixture;
}

Eigen::VectorXi copy_of(Eigen::Ref<const Eigen::VectorXi> view) { return Eigen::VectorXi(view); }

} // namespace

// ---------------------------------------------------------------------------
// Retain
// ---------------------------------------------------------------------------

TEST(ClaimStreamState, AnEliminationOnlyRelayRetainsTheStreamAndItsEpoch) {
    auto nlp = build_corpus(state_case());

    const StructureEpoch claim_before = nlp->claim_stream_epoch();
    const StructureEpoch structure_before = nlp->structure_epoch();
    const Eigen::VectorXi rows_before = copy_of(nlp->kkt_claim_rows());
    const int *storage_before = nlp->kkt_claim_rows().data();

    ASSERT_TRUE(nlp->configure_variable_treatment(FixedVariableTreatments::MakeParameter, 0.0));

    // The structures WERE re-laid -- the engine's own signal moved -- and the
    // claim stream was not, because nothing it describes changed.
    EXPECT_NE(nlp->structure_epoch(), structure_before);
    EXPECT_EQ(nlp->claim_stream_epoch(), claim_before);
    EXPECT_EQ(nlp->kkt_claim_rows().data(), storage_before);
    EXPECT_EQ(copy_of(nlp->kkt_claim_rows()), rows_before);
}

TEST(ClaimStreamState, RestoringFromAnEliminationRetainsToo) {
    auto nlp = build_corpus(state_case());
    ASSERT_TRUE(nlp->configure_variable_treatment(FixedVariableTreatments::MakeParameter, 0.0));

    const StructureEpoch claim_before = nlp->claim_stream_epoch();
    const Eigen::VectorXi rows_before = copy_of(nlp->kkt_claim_rows());

    // RelaxBounds keeps every variable, so the reduction is dropped and the
    // layout goes back over the full space. The declared claim structure never
    // moved, so neither does the stream.
    ASSERT_TRUE(nlp->configure_variable_treatment(FixedVariableTreatments::RelaxBounds, 1e-8));
    ASSERT_FALSE(nlp->is_reduced());
    EXPECT_EQ(nlp->claim_stream_epoch(), claim_before);
    EXPECT_EQ(copy_of(nlp->kkt_claim_rows()), rows_before);
}

// ---------------------------------------------------------------------------
// Rebuild
// ---------------------------------------------------------------------------

TEST(ClaimStreamState, APartitionRenegotiationRebuildsAndBumpsBothEpochs) {
    auto nlp = build_corpus(wide_case());
    const StructureEpoch claim_before = nlp->claim_stream_epoch();
    const StructureEpoch structure_before = nlp->structure_epoch();

    ASSERT_EQ(nlp->negotiate_partition_count(3), 3);

    EXPECT_NE(nlp->structure_epoch(), structure_before);
    EXPECT_NE(nlp->claim_stream_epoch(), claim_before);
    EXPECT_EQ(nlp->hessian_claim_partition_offsets().size(), static_cast<Eigen::Index>(4));
}

TEST(ClaimStreamState, AMakeConstraintTreatmentRebuildsBecauseTheRowSpaceMoved) {
    auto nlp = build_corpus(state_case());
    const StructureEpoch claim_before = nlp->claim_stream_epoch();
    const int equality_claims_before = nlp->equality_jacobian_claims().count_;

    ASSERT_TRUE(nlp->configure_variable_treatment(FixedVariableTreatments::MakeConstraint, 0.0));

    EXPECT_NE(nlp->claim_stream_epoch(), claim_before);
    // One internal equality row per fixed variable, each claiming one Jacobian
    // slot: the equality domain grew by exactly the fixed count.
    EXPECT_EQ(nlp->equality_jacobian_claims().count_,
              equality_claims_before + static_cast<int>(state_case().fixed_variables_.size()));
}

TEST(ClaimStreamState, ASwitchFromMakeConstraintToMakeParameterRebuilds) {
    auto nlp = build_corpus(state_case());
    ASSERT_TRUE(nlp->configure_variable_treatment(FixedVariableTreatments::MakeConstraint, 0.0));
    const StructureEpoch claim_before = nlp->claim_stream_epoch();

    // The switch DROPS the internal rows, so the declared row space and the
    // claim counts both move: a rebuild here is correct, not a missed retention.
    ASSERT_TRUE(nlp->configure_variable_treatment(FixedVariableTreatments::MakeParameter, 0.0));
    EXPECT_NE(nlp->claim_stream_epoch(), claim_before);
}

TEST(ClaimStreamState, ARepeatedLayAtIdenticalCountsStillRebuilds) {
    auto nlp = build_corpus(state_case());
    const StructureEpoch claim_before = nlp->claim_stream_epoch();

    nlp->make_nlp(nlp->primal_vars_, nlp->user_equal_cons_, nlp->inequal_cons_);

    // Every count in the stamp is the one it was. The generation is not, and
    // that is the conjunct that makes this a rebuild -- a re-lay may be over
    // different pieces entirely.
    EXPECT_NE(nlp->claim_stream_epoch(), claim_before);
}

TEST(ClaimStreamState, APieceSwapAtIdenticalCountsRebuildsAndMovesTheStream) {
    // R7 in the spec of record. The two layouts agree on every dimension, on
    // num_user_kkt_elems_, on the gradient count and on the partition count;
    // they differ only in WHICH columns the pieces claim. Nothing but the
    // generation separates them, and a stream retained across this would name
    // coordinates no piece claims.
    CorpusCase fixture = state_case();
    fixture.fixed_variables_.clear();
    auto nlp = build_corpus(fixture);

    const Eigen::VectorXi cols_before = copy_of(nlp->kkt_claim_cols());
    const StructureEpoch claim_before = nlp->claim_stream_epoch();
    const int kkt_before = nlp->num_user_kkt_elems_;
    const int pgx_before = nlp->num_pgx_elems_;

    // The same piece SHAPE over a different, equally sized set of columns: same
    // application count, same element counts, different sparsity.
    const int applications = fixture.applications_;
    Eigen::MatrixXi v_index(2, applications);
    Eigen::MatrixXi c_index(1, applications);
    const int last_variable = nlp->primal_vars_ - 1;
    for (int appl = 0; appl < applications; appl++) {
        v_index(0, appl) = last_variable - 2 * appl;
        v_index(1, appl) = last_variable - 2 * appl - 1;
        c_index(0, appl) = appl;
    }
    CorpusConstraintPiece swapped;
    swapped.owns_hessian_ = true;
    swapped.kind_ = "corpus_swapped";
    nlp->equality_constraints_[0] = hven::solvers::ConstraintFunction(
        hven::solvers::ConstraintInterface(std::move(swapped)), v_index, c_index);

    nlp->make_nlp(nlp->primal_vars_, nlp->user_equal_cons_, nlp->inequal_cons_);

    ASSERT_EQ(nlp->num_user_kkt_elems_, kkt_before);
    ASSERT_EQ(nlp->num_pgx_elems_, pgx_before);
    EXPECT_NE(nlp->claim_stream_epoch(), claim_before);
    EXPECT_NE(copy_of(nlp->kkt_claim_cols()), cols_before);
}

TEST(ClaimStreamState, AdoptingADeclarationWithFixingRowsRestatesOnBothRebuilds) {
    CorpusCase fixture = state_case();
    auto source = build_corpus(fixture);
    ASSERT_TRUE(source->configure_variable_treatment(FixedVariableTreatments::MakeConstraint, 0.0));
    ASSERT_GT(source->internal_fixed_constraints(), 0);

    AggregateDeclaration declaration = source->declaration();
    ASSERT_GT(declaration.fixing_rows_, 0);

    auto target = std::make_shared<NonLinearProgram>(1);
    const StructureEpoch claim_before = target->claim_stream_epoch();
    target->adopt_declaration(std::move(declaration));

    // TWICE: make_nlp lays the user's row space, then the fixing rows are
    // spliced on and the layout is laid again over a row space that now has
    // more claims in it. Both differ from what was published before them, so
    // both restate -- charged honestly rather than hidden.
    EXPECT_EQ(target->claim_stream_epoch().value(), claim_before.value() + 2);
    EXPECT_EQ(target->equality_jacobian_claims().count_, source->equality_jacobian_claims().count_);
}

// ---------------------------------------------------------------------------
// Refuse
// ---------------------------------------------------------------------------

TEST(ClaimStreamState, AnUnlaidProblemHasNoClaimStreamAndSaysSo) {
    NonLinearProgram nlp(1);
    EXPECT_THROW(
        {
            try {
                (void)nlp.kkt_claim_rows();
            } catch (const std::invalid_argument &error) {
                EXPECT_NE(std::string(error.what()).find("has not been laid"), std::string::npos)
                    << error.what();
                throw;
            }
        },
        std::invalid_argument);
}

TEST(ClaimStreamState, AStampThatMovesWhileReducedDropsTheStreamRatherThanGuessing) {
    auto nlp = build_corpus(wide_case());
    ASSERT_TRUE(nlp->configure_variable_treatment(FixedVariableTreatments::MakeParameter, 0.0));
    ASSERT_TRUE(nlp->is_reduced());

    const StructureEpoch claim_before = nlp->claim_stream_epoch();
    ASSERT_NO_THROW((void)nlp->kkt_claim_rows());

    // A renegotiation moves the claim ORDER while the layout is reduced. There
    // is no restating a reduced layout into declaration space, so the stream is
    // dropped -- and the epoch moves, so a consumer polling only the epoch
    // cannot go on holding a view of a layout that is gone.
    ASSERT_EQ(nlp->negotiate_partition_count(3), 3);
    EXPECT_NE(nlp->claim_stream_epoch(), claim_before);

    EXPECT_THROW(
        {
            try {
                (void)nlp->kkt_claim_rows();
            } catch (const std::invalid_argument &error) {
                const std::string message = error.what();
                EXPECT_NE(message.find("eliminated"), std::string::npos) << message;
                throw;
            }
        },
        std::invalid_argument);
    EXPECT_THROW((void)nlp->hessian_claims(), std::invalid_argument);
    EXPECT_THROW((void)nlp->objective_gradient_claim_rows(), std::invalid_argument);
    EXPECT_THROW((void)nlp->hessian_claim_partition_offsets(), std::invalid_argument);

    // And it comes back the moment a layout it CAN describe is laid.
    ASSERT_TRUE(nlp->configure_variable_treatment(FixedVariableTreatments::RelaxBounds, 1e-8));
    ASSERT_NO_THROW((void)nlp->kkt_claim_rows());
    hven::model_tests::ReferenceStream want = reference_restatement(*nlp);
    ASSERT_EQ(nlp->kkt_claim_rows().size(), static_cast<Eigen::Index>(want.rows_.size()));
    for (Eigen::Index slot = 0; slot < nlp->kkt_claim_rows().size(); slot++) {
        ASSERT_EQ(nlp->kkt_claim_rows()[slot], want.rows_[static_cast<std::size_t>(slot)]);
    }
}

// ---------------------------------------------------------------------------
// The exception-safety term
// ---------------------------------------------------------------------------

/// A piece whose num_kkt_elements is NOT additive in its two flags: asked for
/// both, it reports -- and claims -- THREE slots, while the two single-flag
/// calls report one each. The total is therefore honest, so the raw lay is
/// correctly sized and succeeds; only the per-domain split is wrong, which is
/// exactly the failure the restatement has to catch rather than scatter around.
///
/// At namespace scope because the erasure seam's adapter is specialized on it.
struct NonAdditivePiece {
    std::string name() const { return "non_additive"; }
    int input_rows() const { return 2; }
    int output_rows() const { return 1; }
    bool thread_safe() const { return true; }

    int num_kkt_elements(bool dojac, bool dohess) const {
        if (dojac && dohess) {
            return 3;
        }
        return (dojac ? 1 : 0) + (dohess ? 1 : 0);
    }

    void get_kkt_space(Eigen::Ref<Eigen::VectorXi> rows, Eigen::Ref<Eigen::VectorXi> cols,
                       int &freeloc, int conoffset, bool dojac, bool dohess,
                       hven::solvers::SolverIndexingData &data) {
        data.inner_kkt_starts_.resize(data.num_appl());
        for (int appl = 0; appl < data.num_appl(); appl++) {
            data.inner_kkt_starts_[appl] = freeloc;
            const int first = data.v_scatter_loc(0, appl);
            const int second = data.v_scatter_loc(1, appl);
            if (dojac) {
                rows[freeloc] = data.c_loc(0, appl) + conoffset;
                cols[freeloc] = first;
                freeloc++;
            }
            if (dohess) {
                rows[freeloc] = first;
                cols[freeloc] = first;
                freeloc++;
                // THE EXTRA ONE, which the flag-split counts do not predict.
                if (dojac) {
                    rows[freeloc] = second;
                    cols[freeloc] = second;
                    freeloc++;
                }
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
};

namespace hven::solvers {
template <>
struct SolverInterfaceAdapter<NonAdditivePiece> : DirectFunctionModel<NonAdditivePiece> {};
} // namespace hven::solvers

TEST(ClaimStreamState, ARelayThatThrowsLeavesThePreviouslyPublishedViewsStanding) {
    CorpusCase fixture = state_case();
    fixture.fixed_variables_.clear();
    auto nlp = build_corpus(fixture);

    const Eigen::VectorXi rows_before = copy_of(nlp->kkt_claim_rows());
    const ClaimBlock hessian_before = nlp->hessian_claims();
    const StructureEpoch claim_before = nlp->claim_stream_epoch();

    const int applications = fixture.applications_;
    Eigen::MatrixXi v_index(2, applications);
    Eigen::MatrixXi c_index(1, applications);
    for (int appl = 0; appl < applications; appl++) {
        v_index(0, appl) = 2 * appl;
        v_index(1, appl) = 2 * appl + 1;
        c_index(0, appl) = appl;
    }
    nlp->equality_constraints_[0] = hven::solvers::ConstraintFunction(
        hven::solvers::ConstraintInterface(NonAdditivePiece{}), v_index, c_index);

    // The raw rebuild succeeds -- the piece claims a consistent set of slots --
    // and the restatement then refuses, because the counts it was sized from do
    // not describe them.
    EXPECT_THROW(nlp->make_nlp(nlp->primal_vars_, nlp->user_equal_cons_, nlp->inequal_cons_),
                 std::invalid_argument);

    // THE TERM: the arena was built as a local, so nothing was committed, and
    // the epoch was not bumped either. What a consumer holds still reads.
    EXPECT_EQ(nlp->claim_stream_epoch(), claim_before);
    EXPECT_EQ(copy_of(nlp->kkt_claim_rows()), rows_before);
    EXPECT_EQ(nlp->hessian_claims(), hessian_before);
}

// ---------------------------------------------------------------------------
// The restatement's own refusals, driven directly
// ---------------------------------------------------------------------------

namespace {

using hven::solvers::detail::ClaimDomainCounts;
using hven::solvers::detail::RawClaimLayout;

/// One partition, one objective slot and one equality Jacobian slot, in a space
/// of two primal variables, one slack, one equality row and one inequality row.
struct RefusalFixture {
    Eigen::VectorXi rows_{2};
    Eigen::VectorXi cols_{2};
    Eigen::VectorXi marks_{4};
    Eigen::VectorXi gradient_{2};

    RefusalFixture() {
        // slot 0: an objective Hessian claim on (1, 0), walk order.
        rows_[0] = 1;
        cols_[0] = 0;
        // slot 1: an equality Jacobian claim. The assembled equality base is
        // primal + slack == 3.
        rows_[1] = 3;
        cols_[1] = 1;
        marks_ << 0, 1, 2, 2;
        gradient_ << 0, 1;
    }

    RawClaimLayout layout(int partitions = 1) const {
        return RawClaimLayout{rows_, cols_, marks_, gradient_, 2, 1, 1, 1, partitions};
    }
    static ClaimDomainCounts counts() { return ClaimDomainCounts{1, 1, 0}; }
};

} // namespace

TEST(ClaimStreamRefusals, TheWellFormedFixtureRestates) {
    RefusalFixture fixture;
    auto arena =
        hven::solvers::detail::restate_claim_stream(fixture.layout(), RefusalFixture::counts());
    ASSERT_EQ(arena.rows().size(), static_cast<Eigen::Index>(2));
    EXPECT_EQ(arena.rows()[0], 0);
    EXPECT_EQ(arena.cols()[0], 1);
    // The claim row drops the slack width: assembled row 3 is claim row 2.
    EXPECT_EQ(arena.rows()[1], 2);
    EXPECT_EQ(arena.cols()[1], 1);
    EXPECT_EQ(arena.hessian(), (ClaimBlock{0, 1}));
    EXPECT_EQ(arena.equality_jacobian(), (ClaimBlock{1, 1}));
    EXPECT_EQ(arena.inequality_jacobian(), (ClaimBlock{2, 0}));
}

TEST(ClaimStreamRefusals, AClaimedSlackRowIsRefused) {
    RefusalFixture fixture;
    fixture.rows_[1] = 2; // the slack row, between the primal and equality blocks
    try {
        (void)hven::solvers::detail::restate_claim_stream(fixture.layout(),
                                                          RefusalFixture::counts());
        FAIL() << "a claimed slack row must be refused";
    } catch (const std::invalid_argument &error) {
        EXPECT_NE(std::string(error.what()).find("slack row"), std::string::npos) << error.what();
    }
}

TEST(ClaimStreamRefusals, ANegativeCoordinateInAnUnreducedLayoutIsRefused) {
    RefusalFixture fixture;
    fixture.rows_[1] = -1;
    fixture.cols_[1] = -1;
    try {
        (void)hven::solvers::detail::restate_claim_stream(fixture.layout(),
                                                          RefusalFixture::counts());
        FAIL() << "a negative coordinate must be refused";
    } catch (const std::invalid_argument &error) {
        EXPECT_NE(std::string(error.what()).find("elimination"), std::string::npos) << error.what();
    }
}

TEST(ClaimStreamRefusals, AGradientRowOutsideTheDeclaredVariablesIsRefused) {
    RefusalFixture fixture;
    fixture.gradient_[1] = 2; // one past the two declared variables
    try {
        (void)hven::solvers::detail::restate_claim_stream(fixture.layout(),
                                                          RefusalFixture::counts());
        FAIL() << "a gradient row outside the declared variables must be refused";
    } catch (const std::invalid_argument &error) {
        EXPECT_NE(std::string(error.what()).find("objective-gradient"), std::string::npos)
            << error.what();
    }
}

TEST(ClaimStreamRefusals, AColumnOutsideTheDeclaredVariablesIsRefused) {
    RefusalFixture fixture;
    fixture.cols_[0] = 2;
    EXPECT_THROW((void)hven::solvers::detail::restate_claim_stream(fixture.layout(),
                                                                   RefusalFixture::counts()),
                 std::invalid_argument);
}

TEST(ClaimStreamRefusals, AConstraintRowOutsideItsOwnDomainBandIsRefused) {
    RefusalFixture fixture;
    // The equality segment's slot restated into the INEQUALITY band: the layout
    // and the piece list that claimed it disagree about which domain it is.
    fixture.rows_[1] = 4;
    try {
        (void)hven::solvers::detail::restate_claim_stream(fixture.layout(),
                                                          RefusalFixture::counts());
        FAIL() << "a constraint row outside its domain band must be refused";
    } catch (const std::invalid_argument &error) {
        EXPECT_NE(std::string(error.what()).find("row band"), std::string::npos) << error.what();
    }
}

TEST(ClaimStreamRefusals, MalformedMarkTablesAreRefused) {
    {
        RefusalFixture fixture;
        fixture.marks_.resize(3);
        fixture.marks_ << 0, 1, 2;
        EXPECT_THROW((void)hven::solvers::detail::restate_claim_stream(fixture.layout(),
                                                                       RefusalFixture::counts()),
                     std::invalid_argument);
    }
    {
        RefusalFixture fixture;
        fixture.marks_ << 0, 2, 1, 2; // backwards
        EXPECT_THROW((void)hven::solvers::detail::restate_claim_stream(fixture.layout(),
                                                                       RefusalFixture::counts()),
                     std::invalid_argument);
    }
    {
        // THE ADDITIVITY DETECTOR. The lay handed out two slots -- the marks say
        // so -- while the per-domain counts predict three. Three raw slots are
        // supplied so the arrays are long enough for either number, which is
        // what makes the marks the thing that disagrees.
        Eigen::VectorXi rows(3);
        Eigen::VectorXi cols(3);
        Eigen::VectorXi marks(4);
        Eigen::VectorXi gradient(2);
        rows << 1, 3, 0;
        cols << 0, 1, 0;
        marks << 0, 1, 2, 2;
        gradient << 0, 1;
        const RawClaimLayout raw{rows, cols, marks, gradient, 2, 1, 1, 1, 1};
        try {
            (void)hven::solvers::detail::restate_claim_stream(raw, ClaimDomainCounts{2, 1, 0});
            FAIL() << "a count/mark disagreement must be refused";
        } catch (const std::invalid_argument &error) {
            EXPECT_NE(std::string(error.what()).find("additive"), std::string::npos)
                << error.what();
        }
    }
}

TEST(ClaimStreamRefusals, AnEmptyPartitionGivesEqualAdjacentOffsets) {
    // Two partitions, everything claimed by the first: the second's runs are
    // empty in all three domains, which is what equal adjacent offsets say.
    RefusalFixture fixture;
    fixture.marks_.resize(7);
    fixture.marks_ << 0, 1, 2, 2, 2, 2, 2;

    auto arena =
        hven::solvers::detail::restate_claim_stream(fixture.layout(2), RefusalFixture::counts());
    ASSERT_EQ(arena.hessian_offsets().size(), static_cast<Eigen::Index>(3));
    EXPECT_EQ(arena.hessian_offsets()[0], 0);
    EXPECT_EQ(arena.hessian_offsets()[1], 1);
    EXPECT_EQ(arena.hessian_offsets()[2], 1);
    EXPECT_EQ(arena.inequality_offsets()[0], 0);
    EXPECT_EQ(arena.inequality_offsets()[1], 0);
    EXPECT_EQ(arena.inequality_offsets()[2], 0);
}
