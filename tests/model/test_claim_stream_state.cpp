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

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <Eigen/Core>
#include <Eigen/Sparse>

#include "hven/detail/model/claim_restatement.h"
#include "hven/model/aggregate_declaration.h"
#include "hven/model/candidate_point.h"
#include "hven/model/claim_space.h"
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

/// Whether the piece below refuses its next claim pass. A file-scope flag rather
/// than piece state because the partitioner hands every piece out by value, so
/// nothing written on the instance the test holds reaches the copies that are
/// actually asked to claim.
bool g_refuse_next_claim_pass = false;

/// A piece that lays perfectly well and then, once armed, THROWS FROM INSIDE
/// get_mat_space -- the region between the start of a re-lay and the point the
/// old ordering cleared the analysed-destination capture at.
///
/// It stands in for the failure the ordering fix is actually about: a bad_alloc
/// out of one of the six array resizes, the clash matrix, or the mutex vector.
/// Those are unreachable on demand and a piece refusal is reachable, and both
/// arrive at the same place -- a throw out of a re-lay that has already replaced
/// the arrays the previous analysis was taken against.
struct RelayRefusingPiece {
    std::string name() const { return "relay_refusing"; }
    int input_rows() const { return 2; }
    int output_rows() const { return 1; }
    bool thread_safe() const { return true; }

    int num_kkt_elements(bool dojac, bool) const { return dojac ? 2 : 0; }

    void get_kkt_space(Eigen::Ref<Eigen::VectorXi> rows, Eigen::Ref<Eigen::VectorXi> cols,
                       int &freeloc, int conoffset, bool dojac, bool,
                       hven::solvers::SolverIndexingData &data) {
        if (g_refuse_next_claim_pass) {
            throw std::runtime_error("RelayRefusingPiece: refusing this claim pass");
        }
        data.inner_kkt_starts_.resize(data.num_appl());
        for (int appl = 0; appl < data.num_appl(); appl++) {
            data.inner_kkt_starts_[appl] = freeloc;
            if (!dojac) {
                continue;
            }
            const int row = data.c_loc(0, appl) + conoffset;
            rows[freeloc] = row;
            cols[freeloc] = data.v_scatter_loc(0, appl);
            freeloc++;
            rows[freeloc] = row;
            cols[freeloc] = data.v_scatter_loc(1, appl);
            freeloc++;
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
struct SolverInterfaceAdapter<RelayRefusingPiece> : DirectFunctionModel<RelayRefusingPiece> {};
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

    // THE TERM: the restatement writes the SPARE arena and commits by swapping
    // it with the live one, so a refusal touches nothing that was published --
    // and the epoch was not bumped either. What a consumer holds still reads.
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

/// Builds into a fresh arena and returns it, so the refusal cases below read as
/// one call. The production caller passes its SPARE arena instead, which is what
/// makes an equal-width rebuild allocation-free -- pinned separately.
hven::solvers::detail::ClaimArena restated(const RawClaimLayout &raw,
                                           const ClaimDomainCounts &counts) {
    hven::solvers::detail::ClaimArena arena;
    hven::solvers::detail::restate_claim_stream(raw, counts, arena);
    return arena;
}

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
    auto arena = restated(fixture.layout(), RefusalFixture::counts());
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
        (void)restated(fixture.layout(), RefusalFixture::counts());
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
        (void)restated(fixture.layout(), RefusalFixture::counts());
        FAIL() << "a negative coordinate must be refused";
    } catch (const std::invalid_argument &error) {
        EXPECT_NE(std::string(error.what()).find("elimination"), std::string::npos) << error.what();
    }
}

TEST(ClaimStreamRefusals, AGradientRowOutsideTheDeclaredVariablesIsRefused) {
    RefusalFixture fixture;
    fixture.gradient_[1] = 2; // one past the two declared variables
    try {
        (void)restated(fixture.layout(), RefusalFixture::counts());
        FAIL() << "a gradient row outside the declared variables must be refused";
    } catch (const std::invalid_argument &error) {
        EXPECT_NE(std::string(error.what()).find("objective-gradient"), std::string::npos)
            << error.what();
    }
}

TEST(ClaimStreamRefusals, AColumnOutsideTheDeclaredVariablesIsRefused) {
    RefusalFixture fixture;
    fixture.cols_[0] = 2;
    EXPECT_THROW((void)restated(fixture.layout(), RefusalFixture::counts()), std::invalid_argument);
}

TEST(ClaimStreamRefusals, AConstraintRowOutsideItsOwnDomainBandIsRefused) {
    RefusalFixture fixture;
    // The equality segment's slot restated into the INEQUALITY band: the layout
    // and the piece list that claimed it disagree about which domain it is.
    fixture.rows_[1] = 4;
    try {
        (void)restated(fixture.layout(), RefusalFixture::counts());
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
        EXPECT_THROW((void)restated(fixture.layout(), RefusalFixture::counts()),
                     std::invalid_argument);
    }
    {
        RefusalFixture fixture;
        fixture.marks_ << 0, 2, 1, 2; // backwards
        EXPECT_THROW((void)restated(fixture.layout(), RefusalFixture::counts()),
                     std::invalid_argument);
    }
    {
        // A table that does not OPEN at zero is malformed, and must not be
        // reported as an additivity failure: nothing about the element counts is
        // in question here.
        RefusalFixture fixture;
        fixture.marks_ << 1, 1, 2, 2;
        try {
            (void)restated(fixture.layout(), RefusalFixture::counts());
            FAIL() << "a mark table that does not open at zero must be refused";
        } catch (const std::invalid_argument &error) {
            const std::string message = error.what();
            EXPECT_NE(message.find("open at"), std::string::npos) << message;
            EXPECT_EQ(message.find("additive"), std::string::npos) << message;
        }
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
            (void)restated(raw, ClaimDomainCounts{2, 1, 0});
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

    auto arena = restated(fixture.layout(2), RefusalFixture::counts());
    ASSERT_EQ(arena.hessian_offsets().size(), static_cast<Eigen::Index>(3));
    EXPECT_EQ(arena.hessian_offsets()[0], 0);
    EXPECT_EQ(arena.hessian_offsets()[1], 1);
    EXPECT_EQ(arena.hessian_offsets()[2], 1);
    EXPECT_EQ(arena.inequality_offsets()[0], 0);
    EXPECT_EQ(arena.inequality_offsets()[1], 0);
    EXPECT_EQ(arena.inequality_offsets()[2], 0);
}

// ---------------------------------------------------------------------------
// Retain-and-reuse, and the routes that must not retain
// ---------------------------------------------------------------------------

TEST(ClaimStreamState, AnEqualWidthRebuildAllocatesNothing) {
    // TWO BUFFERS, ALTERNATING. A rebuild builds into the spare and swaps, so
    // the buffer that was live comes back as the next rebuild's spare: over four
    // rebuilds at one claim structure the published storage alternates A, B, A, B
    // and no third address ever appears. A design that allocated a fresh arena
    // per rebuild could not produce that pattern except by accident, four times
    // running.
    CorpusCase fixture = state_case();
    fixture.fixed_variables_.clear();
    auto nlp = build_corpus(fixture);

    std::vector<const int *> storage;
    storage.push_back(nlp->kkt_claim_rows().data());
    for (int lay = 0; lay < 3; lay++) {
        nlp->make_nlp(nlp->primal_vars_, nlp->user_equal_cons_, nlp->inequal_cons_);
        storage.push_back(nlp->kkt_claim_rows().data());
    }

    ASSERT_EQ(storage.size(), 4u);
    EXPECT_NE(storage[0], storage[1]);
    EXPECT_EQ(storage[0], storage[2]);
    EXPECT_EQ(storage[1], storage[3]);
}

TEST(ClaimStreamState, APieceWrittenIntoAMasterListWithoutARelayIsNotRetainedAcross) {
    // THE FOOTGUN THIS CLOSES. The three master piece lists are public. A caller
    // that writes one and then reaches rebuild_structures WITHOUT make_nlp --
    // through a treatment or a partition renegotiation -- would, on a stamp
    // compare alone, keep a stream describing the pieces that were laid: same
    // dimensions, same claim counts, same gradient count, different columns.
    CorpusCase fixture = state_case();
    fixture.fixed_variables_.clear();
    auto nlp = build_corpus(fixture);

    const Eigen::VectorXi cols_before = copy_of(nlp->kkt_claim_cols());
    const StructureEpoch claim_before = nlp->claim_stream_epoch();
    const int kkt_before = nlp->num_user_kkt_elems_;

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
    swapped.kind_ = "corpus_smuggled";
    nlp->equality_constraints_[0] = hven::solvers::ConstraintFunction(
        hven::solvers::ConstraintInterface(std::move(swapped)), v_index, c_index);

    // NO make_nlp. A renegotiation to the count already in force re-lays
    // unconditionally, which is the shortest route to rebuild_structures.
    ASSERT_EQ(nlp->negotiate_partition_count(nlp->num_partitions_), nlp->num_partitions_);

    ASSERT_EQ(nlp->num_user_kkt_elems_, kkt_before);
    EXPECT_NE(nlp->claim_stream_epoch(), claim_before);
    EXPECT_NE(copy_of(nlp->kkt_claim_cols()), cols_before);

    const hven::model_tests::ReferenceStream want = reference_restatement(*nlp);
    ASSERT_EQ(nlp->kkt_claim_cols().size(), static_cast<Eigen::Index>(want.cols_.size()));
    for (Eigen::Index slot = 0; slot < nlp->kkt_claim_cols().size(); slot++) {
        ASSERT_EQ(nlp->kkt_claim_cols()[slot], want.cols_[static_cast<std::size_t>(slot)]);
    }
}

TEST(ClaimStreamState, AFirstLayThatCannotRestateSaysThatAndNotSomethingAboutElimination) {
    // The message a refused FIRST lay leaves behind. Nothing here is eliminated
    // and nothing was ever published, so an answer about a reduced layout would
    // be a confident lie about the wrong thing.
    auto nlp = std::make_shared<NonLinearProgram>(1);
    constexpr int kApplications = 6;
    Eigen::MatrixXi v_index(2, kApplications);
    Eigen::MatrixXi c_index(1, kApplications);
    for (int appl = 0; appl < kApplications; appl++) {
        v_index(0, appl) = 2 * appl;
        v_index(1, appl) = 2 * appl + 1;
        c_index(0, appl) = appl;
    }
    nlp->equality_constraints_.push_back(hven::solvers::ConstraintFunction(
        hven::solvers::ConstraintInterface(NonAdditivePiece{}), v_index, c_index));
    for (int i = 0; i < 2 * kApplications; i++) {
        nlp->set_variable_bound(i, -1.0, 1.0);
    }

    EXPECT_THROW(nlp->make_nlp(2 * kApplications, kApplications, 0), std::invalid_argument);
    ASSERT_FALSE(nlp->is_reduced());

    try {
        (void)nlp->kkt_claim_rows();
        FAIL() << "a layout whose first restatement was refused publishes nothing";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("restating"), std::string::npos) << message;
        EXPECT_EQ(message.find("eliminated"), std::string::npos) << message;
    }
}

TEST(ClaimStreamState, ARefusedRelayLeavesTheLayoutReportingItselfUnanalysed) {
    // THE OTHER HALF OF THE EXCEPTION-SAFETY TERM, and the half that is about
    // the ENGINE's surface rather than the claim stream's. A re-lay clears the
    // analysed-destination capture with its first statements, before anything
    // that can throw: so a lay that is refused part-way -- here, by a
    // restatement that cannot describe the slots it was handed -- leaves a
    // layout that says it has not been analysed, and refuses a KKT-bearing
    // assemble BY NAME. The failure this closes is the opposite: a layout whose
    // arrays have been replaced while its location table still claims to be
    // bound to the destination the previous layout was analysed against, which
    // Release builds would scatter through in silence.
    constexpr int kApplications = 8;
    Eigen::MatrixXi v_index(2, kApplications);
    Eigen::MatrixXi c_index(1, kApplications);
    for (int appl = 0; appl < kApplications; appl++) {
        v_index(0, appl) = 2 * appl;
        v_index(1, appl) = 2 * appl + 1;
        c_index(0, appl) = appl;
    }

    g_refuse_next_claim_pass = false;
    auto nlp = std::make_shared<NonLinearProgram>(1);
    nlp->equality_constraints_.push_back(hven::solvers::ConstraintFunction(
        hven::solvers::ConstraintInterface(RelayRefusingPiece{}), v_index, c_index));
    for (int i = 0; i < 2 * kApplications; i++) {
        nlp->set_variable_bound(i, -1.0, 1.0);
    }
    nlp->make_nlp(2 * kApplications, kApplications, 0);

    Eigen::SparseMatrix<double, Eigen::RowMajor> kkt;
    nlp->analyze_sparsity(kkt);
    ASSERT_NE(nlp->bound_kkt_destination(), nullptr);

    // The re-lay now fails INSIDE get_mat_space, after set_mat_dimensions has
    // already replaced the arrays the analysis above was taken against.
    g_refuse_next_claim_pass = true;
    EXPECT_THROW(nlp->make_nlp(2 * kApplications, kApplications, 0), std::runtime_error);
    g_refuse_next_claim_pass = false;

    // The capture is gone, and the entry that reads it says so.
    EXPECT_EQ(nlp->bound_kkt_destination(), nullptr);

    std::vector<double> values(static_cast<std::size_t>(std::max(nlp->num_user_kkt_elems_, 1)),
                               0.0);
    const hven::solvers::KktScatterView kkt_view{values.data(), static_cast<int>(values.size()),
                                                 &nlp->kkt_location_table()};
    const hven::Vec x = hven::Vec::Zero(nlp->primal_vars_);
    const hven::Vec no_multipliers = hven::Vec::Zero(0);
    const hven::solvers::CandidatePoint point{x, no_multipliers, no_multipliers, 1.0};

    try {
        nlp->assemble(point, hven::solvers::EvalRequest::kConstraintJacobian, kkt_view,
                      hven::solvers::RhsScatterView{});
        FAIL() << "a KKT-bearing assemble against an un-analysed layout must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("has not been laid against any destination"), std::string::npos)
            << message;
    }
}
