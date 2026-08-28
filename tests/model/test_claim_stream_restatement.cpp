// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// THE CLAIM STREAM A LAID NonLinearProgram PUBLISHES, against a reference
// restatement derived independently of it.
//
// WHAT MAKES THIS A TEST RATHER THAN A RESTATEMENT OF THE IMPLEMENTATION. The
// layout classifies each laid slot with the intra-partition cursor marks its own
// lay recorded: which of the three piece lists claimed this slot. The reference
// in support/claim_corpus.h asks a different question of the same slots -- which
// ROW BAND of the assembled space does this claim name -- and restates them
// straight from the convention in model/claim_stream_source.h. Neither
// derivation can borrow the other's answer, and the assertion is that they
// agree, element for element, on every shape in the corpus.
//
// The rest of this file is the other half of the claim: that publishing the
// stream moved NOTHING. The raw arrays, the location table, the claim digest and
// the structural key are what the interior-point engine consumes, and they are
// read here before and after the new surface is touched.

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <Eigen/Core>

#include "hven/model/non_linear_program.h"
#include "hven/model/structure_identity.h"

#include "support/claim_corpus.h"

using hven::model_tests::build_corpus;
using hven::model_tests::corpus_cases;
using hven::model_tests::CorpusCase;
using hven::model_tests::expand_partition_offsets;
using hven::model_tests::reference_restatement;
using hven::model_tests::ReferenceStream;
using hven::solvers::ClaimBlock;
using hven::solvers::FixedVariableTreatments;
using hven::solvers::NonLinearProgram;

namespace {

/// Every published array against the reference, one element at a time, plus the
/// three blocks and the partition attribution the offset tables carry.
void expect_stream_matches_reference(const NonLinearProgram &nlp, const ReferenceStream &want) {
    const auto rows = nlp.kkt_claim_rows();
    const auto cols = nlp.kkt_claim_cols();

    ASSERT_EQ(rows.size(), static_cast<Eigen::Index>(want.rows_.size()));
    ASSERT_EQ(cols.size(), static_cast<Eigen::Index>(want.cols_.size()));
    for (Eigen::Index slot = 0; slot < rows.size(); slot++) {
        ASSERT_EQ(rows[slot], want.rows_[static_cast<std::size_t>(slot)])
            << "claim row at slot " << slot;
        ASSERT_EQ(cols[slot], want.cols_[static_cast<std::size_t>(slot)])
            << "claim column at slot " << slot;
    }

    EXPECT_EQ(nlp.hessian_claims(), want.hessian_);
    EXPECT_EQ(nlp.equality_jacobian_claims(), want.equality_jacobian_);
    EXPECT_EQ(nlp.inequality_jacobian_claims(), want.inequality_jacobian_);

    const auto gradient = nlp.objective_gradient_claim_rows();
    ASSERT_EQ(gradient.size(), static_cast<Eigen::Index>(want.gradient_rows_.size()));
    for (Eigen::Index slot = 0; slot < gradient.size(); slot++) {
        ASSERT_EQ(gradient[slot], want.gradient_rows_[static_cast<std::size_t>(slot)])
            << "objective-gradient claim row at slot " << slot;
    }

    const std::vector<int> per_slot = expand_partition_offsets(nlp);
    ASSERT_EQ(per_slot.size(), want.partitions_.size());
    for (std::size_t slot = 0; slot < per_slot.size(); slot++) {
        ASSERT_EQ(per_slot[slot], want.partitions_[slot])
            << "partition attribution at published slot " << slot;
    }
}

/// The encoding's own invariants, stated over the published tables alone: one
/// entry per partition plus a close, opening at 0, never going backwards, and
/// closing at the domain's run length.
void expect_offset_table_well_formed(Eigen::Ref<const Eigen::VectorXi> offsets,
                                     const ClaimBlock &block, int partitions, const char *domain) {
    SCOPED_TRACE(domain);
    ASSERT_EQ(offsets.size(), partitions + 1);
    EXPECT_EQ(offsets[0], 0);
    EXPECT_EQ(offsets[partitions], block.count_);
    for (int at = 1; at <= partitions; at++) {
        EXPECT_LE(offsets[at - 1], offsets[at]) << "offset " << at << " goes backwards";
    }
}

} // namespace

// ---------------------------------------------------------------------------
// The gate: the published stream IS the reference restatement
// ---------------------------------------------------------------------------

TEST(ClaimStreamRestatement, ThePublishedStreamEqualsAnIndependentReference) {
    for (const CorpusCase &corpus_case : corpus_cases()) {
        SCOPED_TRACE(corpus_case.name_);
        auto nlp = build_corpus(corpus_case);
        expect_stream_matches_reference(*nlp, reference_restatement(*nlp));
    }
}

TEST(ClaimStreamRestatement, TheThreeBlocksPartitionTheArenaInDomainOrder) {
    for (const CorpusCase &corpus_case : corpus_cases()) {
        SCOPED_TRACE(corpus_case.name_);
        auto nlp = build_corpus(corpus_case);

        const ClaimBlock hessian = nlp->hessian_claims();
        const ClaimBlock equality = nlp->equality_jacobian_claims();
        const ClaimBlock inequality = nlp->inequality_jacobian_claims();

        EXPECT_EQ(hessian.start_, 0);
        EXPECT_EQ(equality.start_, hessian.start_ + hessian.count_);
        EXPECT_EQ(inequality.start_, equality.start_ + equality.count_);
        EXPECT_EQ(inequality.start_ + inequality.count_, nlp->kkt_claim_rows().size());
        EXPECT_EQ(nlp->kkt_claim_rows().size(), nlp->kkt_claim_cols().size());

        // Every published slot is one of the laid slots, and there are exactly
        // as many of them: the restatement is a permutation, not a filter.
        EXPECT_EQ(nlp->kkt_claim_rows().size(), nlp->num_user_kkt_elems_);
    }
}

TEST(ClaimStreamRestatement, TheOffsetTablesAreWellFormedOnEveryShape) {
    for (const CorpusCase &corpus_case : corpus_cases()) {
        SCOPED_TRACE(corpus_case.name_);
        auto nlp = build_corpus(corpus_case);
        const int partitions = nlp->num_partitions_;

        expect_offset_table_well_formed(nlp->hessian_claim_partition_offsets(),
                                        nlp->hessian_claims(), partitions, "Hessian");
        expect_offset_table_well_formed(nlp->equality_jacobian_claim_partition_offsets(),
                                        nlp->equality_jacobian_claims(), partitions,
                                        "equality Jacobian");
        expect_offset_table_well_formed(nlp->inequality_jacobian_claim_partition_offsets(),
                                        nlp->inequality_jacobian_claims(), partitions,
                                        "inequality Jacobian");
    }
}

TEST(ClaimStreamRestatement, TheClaimConventionHoldsSlotForSlot) {
    // The convention read back off the published stream itself, without the
    // reference: a Hessian claim is an upper-triangle pair of primal
    // coordinates, an equality Jacobian claim names a row of the equality band,
    // an inequality Jacobian claim a row of the inequality band, and no claim
    // anywhere names a slack row -- the space has none.
    for (const CorpusCase &corpus_case : corpus_cases()) {
        SCOPED_TRACE(corpus_case.name_);
        auto nlp = build_corpus(corpus_case);

        const auto rows = nlp->kkt_claim_rows();
        const auto cols = nlp->kkt_claim_cols();
        const int primal = nlp->primal_vars_;
        const int equality_end = primal + nlp->equal_cons_;
        const int claim_dim = equality_end + nlp->inequal_cons_;

        const ClaimBlock hessian = nlp->hessian_claims();
        for (int at = 0; at < hessian.count_; at++) {
            const int slot = hessian.start_ + at;
            EXPECT_GE(rows[slot], 0);
            EXPECT_LT(rows[slot], primal);
            EXPECT_LT(cols[slot], primal);
            EXPECT_LE(rows[slot], cols[slot]) << "Hessian slot " << slot << " is not upper";
        }
        const ClaimBlock equality = nlp->equality_jacobian_claims();
        for (int at = 0; at < equality.count_; at++) {
            const int slot = equality.start_ + at;
            EXPECT_GE(rows[slot], primal);
            EXPECT_LT(rows[slot], equality_end);
            EXPECT_GE(cols[slot], 0);
            EXPECT_LT(cols[slot], primal);
        }
        const ClaimBlock inequality = nlp->inequality_jacobian_claims();
        for (int at = 0; at < inequality.count_; at++) {
            const int slot = inequality.start_ + at;
            EXPECT_GE(rows[slot], equality_end);
            EXPECT_LT(rows[slot], claim_dim);
            EXPECT_GE(cols[slot], 0);
            EXPECT_LT(cols[slot], primal);
        }

        const auto gradient = nlp->objective_gradient_claim_rows();
        EXPECT_EQ(gradient.size(), nlp->num_pgx_elems_);
        for (Eigen::Index slot = 0; slot < gradient.size(); slot++) {
            EXPECT_GE(gradient[slot], 0);
            EXPECT_LT(gradient[slot], primal);
        }
    }
}

// ---------------------------------------------------------------------------
// The gate's other half: the raw surface the engine consumes did not move
// ---------------------------------------------------------------------------

TEST(ClaimStreamRestatement, PublishingTheStreamMovesNoRawArray) {
    for (const CorpusCase &corpus_case : corpus_cases()) {
        SCOPED_TRACE(corpus_case.name_);
        auto nlp = build_corpus(corpus_case);

        const Eigen::VectorXi rows_before = nlp->kkt_coeff_rows_;
        const Eigen::VectorXi cols_before = nlp->kkt_coeff_cols_;
        const Eigen::VectorXi parts_before = nlp->kkt_coeff_part_ids_;
        const Eigen::VectorXi locations_before = nlp->kkt_locations_;
        const Eigen::VectorXi rhs_rows_before = nlp->rhs_coeff_rows_;
        const std::uint64_t digest_before = nlp->model_structure_key().claim_digest_;

        // Everything the new surface offers, read once.
        (void)nlp->kkt_claim_rows();
        (void)nlp->kkt_claim_cols();
        (void)nlp->objective_gradient_claim_rows();
        (void)nlp->hessian_claims();
        (void)nlp->equality_jacobian_claims();
        (void)nlp->inequality_jacobian_claims();
        (void)nlp->hessian_claim_partition_offsets();
        (void)nlp->equality_jacobian_claim_partition_offsets();
        (void)nlp->inequality_jacobian_claim_partition_offsets();
        (void)nlp->claim_stream_epoch();

        EXPECT_EQ(rows_before, nlp->kkt_coeff_rows_);
        EXPECT_EQ(cols_before, nlp->kkt_coeff_cols_);
        EXPECT_EQ(parts_before, nlp->kkt_coeff_part_ids_);
        EXPECT_EQ(locations_before, nlp->kkt_locations_);
        EXPECT_EQ(rhs_rows_before, nlp->rhs_coeff_rows_);
        EXPECT_EQ(digest_before, nlp->model_structure_key().claim_digest_);
    }
}

TEST(ClaimStreamRestatement, TheClaimDigestStillHashesTheRawSlotsInEmissionOrder) {
    // R-7 in the spec of record, as an assertion rather than a promise: the
    // digest is a function of the RAW arrays in the order the pieces handed them
    // out, and the restated stream is never fed to it. Recomputed here through
    // the public builder over the raw arrays.
    for (const CorpusCase &corpus_case : corpus_cases()) {
        SCOPED_TRACE(corpus_case.name_);
        auto nlp = build_corpus(corpus_case);

        const std::uint64_t over_raw = hven::solvers::claim_stream_digest(
            nlp->declaration(), nlp->kkt_coeff_rows_.head(nlp->num_user_kkt_elems_),
            nlp->kkt_coeff_cols_.head(nlp->num_user_kkt_elems_));
        EXPECT_EQ(nlp->model_structure_key().claim_digest_, over_raw);

        // And the restated stream is a DIFFERENT sequence wherever the layout
        // actually permutes anything -- which is the reason the header sentence
        // had to name one of the two orders.
        const std::uint64_t over_restated = hven::solvers::claim_stream_digest(
            nlp->declaration(), nlp->kkt_claim_rows(), nlp->kkt_claim_cols());
        if (nlp->hessian_claims().count_ > 0 && nlp->equality_jacobian_claims().count_ > 0) {
            EXPECT_NE(over_raw, over_restated);
        }
    }
}

// ---------------------------------------------------------------------------
// The elimination fixtures
// ---------------------------------------------------------------------------

namespace {

CorpusCase elimination_case() {
    CorpusCase fixture;
    fixture.name_ = "elimination fixture";
    fixture.objective_pieces_ = 1;
    fixture.equality_pieces_ = 2;
    fixture.inequality_pieces_ = 1;
    fixture.applications_ = 9;
    fixture.fixed_variables_ = {5, 11};
    return fixture;
}

} // namespace

TEST(ClaimStreamRestatement, AnEliminationOnlyRelayLeavesTheDeclarationSpaceStreamStanding) {
    auto nlp = build_corpus(elimination_case());
    const ReferenceStream before = reference_restatement(*nlp);
    expect_stream_matches_reference(*nlp, before);

    const int *rows_data = nlp->kkt_claim_rows().data();
    ASSERT_TRUE(nlp->configure_variable_treatment(FixedVariableTreatments::MakeParameter, 0.0));
    ASSERT_TRUE(nlp->is_reduced());

    // The layout now names coordinates in the reduced space and carries the
    // dropped-row sentinel in its gradient rows -- and the published stream is
    // still the declaration-space one, byte for byte, in the same storage.
    expect_stream_matches_reference(*nlp, before);
    EXPECT_EQ(nlp->kkt_claim_rows().data(), rows_data);

    // The retained gradient rows are the whole reason the array is retained
    // rather than aliased: the layout's own rows have lost the eliminated ones.
    bool layout_dropped_a_gradient_row = false;
    for (int slot = 0; slot < nlp->num_pgx_elems_; slot++) {
        layout_dropped_a_gradient_row |= (nlp->rhs_coeff_rows_[nlp->pgx_data_start_ + slot] < 0);
    }
    EXPECT_TRUE(layout_dropped_a_gradient_row);
    for (Eigen::Index slot = 0; slot < nlp->objective_gradient_claim_rows().size(); slot++) {
        EXPECT_GE(nlp->objective_gradient_claim_rows()[slot], 0);
    }
}

TEST(ClaimStreamRestatement, AMakeConstraintSwitchRestatesAgainstTheNewRowSpace) {
    auto nlp = build_corpus(elimination_case());
    ASSERT_TRUE(nlp->configure_variable_treatment(FixedVariableTreatments::MakeParameter, 0.0));
    ASSERT_TRUE(nlp->configure_variable_treatment(FixedVariableTreatments::MakeConstraint, 0.0));
    ASSERT_FALSE(nlp->is_reduced());

    // The treatment appended one internal equality row per fixed variable, so
    // the row space -- and the claim structure stated over it -- is a different
    // one, and the stream describes THAT.
    expect_stream_matches_reference(*nlp, reference_restatement(*nlp));
}

TEST(ClaimStreamRestatement, APartitionRenegotiationRestatesAgainstTheNewOrder) {
    CorpusCase fixture;
    fixture.name_ = "renegotiation fixture";
    fixture.objective_pieces_ = 1;
    fixture.equality_pieces_ = 3;
    fixture.inequality_pieces_ = 2;
    fixture.applications_ = 600;
    fixture.requested_partitions_ = 1;
    fixture.equality_mode_ = hven::solvers::ThreadingFlags::RoundRobin;
    fixture.inequality_mode_ = hven::solvers::ThreadingFlags::RoundRobin;

    auto nlp = build_corpus(fixture);
    ASSERT_EQ(nlp->num_partitions_, 1);
    expect_stream_matches_reference(*nlp, reference_restatement(*nlp));

    ASSERT_EQ(nlp->negotiate_partition_count(3), 3);
    expect_stream_matches_reference(*nlp, reference_restatement(*nlp));
    EXPECT_EQ(nlp->hessian_claim_partition_offsets().size(), 4);
}

TEST(ClaimStreamRestatement, ARepeatedLayRestatesAgainstTheProblemAsItNowStands) {
    auto nlp = build_corpus(elimination_case());
    expect_stream_matches_reference(*nlp, reference_restatement(*nlp));

    // Re-laid at the same dimensions off the same pieces: the stream is rebuilt
    // and is right either way, which is the weaker half. The state-machine suite
    // carries the half that says a rebuild HAPPENED.
    nlp->make_nlp(nlp->primal_vars_, nlp->user_equal_cons_, nlp->inequal_cons_);
    expect_stream_matches_reference(*nlp, reference_restatement(*nlp));
}
