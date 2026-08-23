// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_aggregate_eval_seam.cpp — the consumer-side binding's pins.
//
// WHAT IS BEING PINNED. AggregateEvalSeam
// (include/hven/detail/drivers/aggregate_eval_seam.h) reproduces the driver's
// evaluation moments against an NlpAggregate instead of an NlpModel. The
// preservation bar is BIT-IDENTITY: every NlpEval field and every QpProblem
// block the seam produces must equal, bit for bit, what the free functions in
// drivers/sqp_driver.h produce from the same model at the same point, with
// identical sparse structure arrays. Everything downstream -- the KKT residual,
// the funnel, the structural hash, every pinned counter -- reads those two
// objects and nothing else, so their bit-identity is the whole preservation
// argument.
//
// FIVE BATTERIES:
//   AggregateEvalSeamIdentity.*    -- the five moments against the five free
//                                     functions, over four fixtures and every
//                                     point in each.
//   AggregateEvalSeamFalsification.* -- the identity pin CAN fail: one entry of
//                                     the seam's claim-slot -> arena-offset
//                                     permutation is swapped with another
//                                     through a friend hook that exists only in
//                                     this file, and the same comparison then
//                                     reports a mismatch.
//   AggregateEvalSeamEpoch.*       -- a partition renegotiation bumps the
//                                     aggregate's structure epoch; the next
//                                     evaluation re-lays and still agrees.
//   AggregateEvalSeamArena.*       -- consecutive evaluations at different
//                                     points do not accumulate into each other.
//   SqpDriverEntryEquivalence.*    -- one level up: the driver's aggregate-taking
//                                     solve() and its model-taking wrapper run
//                                     the same solve, counters and hand-off
//                                     included, cold and warm.
//
// THE NEGATIVE-ZERO FIXTURE IS THE POINT OF SeamCouplingModel. The provider's
// assemble ACCUMULATES, so the seam seeds its arena before every call, and a
// +0.0 seed would turn a model's -0.0 into +0.0 (IEEE: (+0.0) + (-0.0) ==
// +0.0). That model's gradient and inequality Jacobian both carry a genuine
// -0.0 at its first test point, so the bitwise comparisons below fail unless
// the seed is the negative zero the seam actually uses.

#include <bit>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <hven/drivers/sqp_driver.h>

#include <hven/detail/drivers/aggregate_eval_seam.h>
#include <hven/model/nlp_model.h>
#include <hven/model/nlp_model_aggregate.h>

#include "support/hs_problems.h"
#include "support/parametric_families.h"

namespace hven::solvers {

/// The permutation-corruption hook the falsification battery needs, declared as
/// a friend by the seam and defined ONLY here. Nothing in the shipped class can
/// mutate a location table.
struct AggregateEvalSeamTestAccess {
    static int hessian_claim_count(const AggregateEvalSeam &seam) { return seam.hessian_.count_; }

    static int location(const AggregateEvalSeam &seam, int claim_slot) {
        return seam.kkt_locations_[static_cast<std::size_t>(claim_slot)];
    }

    /// Swaps two claim slots' arena offsets. A SWAP rather than an arbitrary
    /// write, deliberately: it keeps every offset in range, so what the pin
    /// catches is a wrong PERMUTATION and never an out-of-bounds access that
    /// would have been caught by something else.
    static void swap_locations(AggregateEvalSeam &seam, int left_slot, int right_slot) {
        std::swap(seam.kkt_locations_[static_cast<std::size_t>(left_slot)],
                  seam.kkt_locations_[static_cast<std::size_t>(right_slot)]);
    }

    /// Overwrites the epoch the seam believes it laid against, WITHOUT touching
    /// the structures. Used to construct the two states a lay that throws could
    /// leave behind -- structures partial, epoch either committed or stale --
    /// so the recovery rule can be pinned in both directions.
    static void adopt_epoch(AggregateEvalSeam &seam, hven::solvers::StructureEpoch epoch) {
        seam.epoch_at_lay_ = epoch;
    }
};

} // namespace hven::solvers

namespace {

using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using hven::solvers::AggregateEvalSeam;
using hven::solvers::AggregateEvalSeamTestAccess;
using hven::solvers::build_subproblem;
using hven::solvers::eval_nlp;
using hven::solvers::eval_nlp_values;
using hven::solvers::NlpEval;
using hven::solvers::NlpModel;
using hven::solvers::NlpModelAggregate;
using hven::solvers::QpProblem;
using hven::solvers::upgrade_to_full;
// The driver-entry battery at the end of this file.
using hven::solvers::SqpCounters;
using hven::solvers::SqpDriver;
using hven::solvers::SqpIterate;
using hven::solvers::SqpOptions;
using hven::solvers::SqpSolution;
using hven::solvers::SqpStatus;
using hven::solvers::StartLevel;
using hven::solvers::WarmStart;
using hven::solvers::test_support::detail::kInf;
using hven::solvers::test_support::detail::make_jac;
using hven::solvers::test_support::detail::make_upper;

// ---------------------------------------------------------------------------
// The coupling fixture: me > 0, mi > 0, a genuinely non-diagonal Hessian, a
// structural zero in each Jacobian row, and a NEGATIVE ZERO at its first test
// point in both the gradient and the inequality Jacobian.
//
//   f(x)  = -x0^2 + 2 x1^2 + 3 x2^2 + x1 x2
//   cE(x) = x0 + x1 + x2 - 1
//   cI(x) = ( x0^2 + x1 - 2 ,  x1 - 2 x2^2 - 0.5 )
//
// grad f = (-2 x0, 4 x1 + x2, 6 x2 + x1), so grad f (0) is -0.0 wherever
// x0 == 0; d(cI1)/dx2 = -4 x2 is -0.0 wherever x2 == 0. No HS problem in
// tests/sqp/support/hs_problems.h has both constraint kinds AND an off-diagonal
// Hessian entry, which is why this one exists rather than being reused.
// ---------------------------------------------------------------------------
class SeamCouplingModel : public NlpModel {
  public:
    Index n() const override { return 3; }
    Index me() const override { return 1; }
    Index mi() const override { return 2; }

    double eval_f(const Vec &x) const override {
        return -x(0) * x(0) + 2.0 * x(1) * x(1) + 3.0 * x(2) * x(2) + x(1) * x(2);
    }

    Vec eval_grad(const Vec &x) const override {
        return (Vec(3) << -2.0 * x(0), 4.0 * x(1) + x(2), 6.0 * x(2) + x(1)).finished();
    }

    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c(0) = x(0) + x(1) + x(2) - 1.0;
        return c;
    }

    Vec eval_ci(const Vec &x) const override {
        Vec c(2);
        c(0) = x(0) * x(0) + x(1) - 2.0;
        c(1) = x(1) - 2.0 * x(2) * x(2) - 0.5;
        return c;
    }

    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &,
                      const Vec &lambda_i) const override {
        // Every structural entry every call, whatever obj_scale and the
        // multipliers are (nlp_model.h's pattern-invariance precondition).
        return make_upper(3, {{0, 0, -2.0 * obj_scale + 2.0 * lambda_i(0)},
                              {1, 1, 4.0 * obj_scale},
                              {1, 2, 1.0 * obj_scale},
                              {2, 2, 6.0 * obj_scale - 4.0 * lambda_i(1)}});
    }

    SpMatRM eval_jac_e(const Vec &) const override {
        return make_jac(1, 3, {{0, 0, 1.0}, {0, 1, 1.0}, {0, 2, 1.0}});
    }

    SpMatRM eval_jac_i(const Vec &x) const override {
        // (0, 2) and (1, 0) are structural zeros, emitted every call.
        return make_jac(2, 3,
                        {{0, 0, 2.0 * x(0)},
                         {0, 1, 1.0},
                         {0, 2, 0.0},
                         {1, 0, 0.0},
                         {1, 1, 1.0},
                         {1, 2, -4.0 * x(2)}});
    }

    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return (Vec(3) << 0.0, 0.25, 0.0).finished(); }

  private:
    Vec lower_ = (Vec(3) << -1.0, -kInf, -3.0).finished();
    Vec upper_ = (Vec(3) << kInf, 5.0, 6.0).finished();
};

// ---------------------------------------------------------------------------
// Fixtures and comparison helpers
// ---------------------------------------------------------------------------

struct SeamFixture {
    std::string name_;
    std::shared_ptr<NlpModel> model_;
    std::vector<Vec> points_;
};

std::vector<SeamFixture> fixtures() {
    std::vector<SeamFixture> all;
    all.push_back({"SeamCoupling",
                   std::make_shared<SeamCouplingModel>(),
                   {(Vec(3) << 0.0, 0.25, 0.0).finished(), (Vec(3) << 0.7, -0.3, 1.25).finished(),
                    (Vec(3) << -1.5, 2.0, 0.5).finished()}});
    all.push_back({"HS14",
                   std::make_shared<hven::solvers::test_support::Hs14Model>(),
                   {(Vec(2) << 2.0, 2.0).finished(), (Vec(2) << 1.5, -0.25).finished(),
                    (Vec(2) << 0.0, 0.0).finished()}});
    all.push_back({"HS15",
                   std::make_shared<hven::solvers::test_support::Hs15Model>(),
                   {(Vec(2) << -2.0, 1.0).finished(), (Vec(2) << 0.5, 2.0).finished(),
                    (Vec(2) << 0.0, 0.0).finished()}});
    all.push_back(
        {"HS76",
         std::make_shared<hven::solvers::test_support::Hs76Model>(),
         {(Vec(4) << 0.5, 0.5, 0.5, 0.5).finished(), (Vec(4) << 0.0, 1.25, -0.5, 2.0).finished()}});
    return all;
}

/// Multipliers keyed off the point so that successive calls in one battery do
/// not accidentally agree by carrying the same numbers.
Vec multipliers(Index rows, double seed) {
    Vec lambda(rows);
    for (Index row = 0; row < rows; ++row) {
        lambda[row] = seed + 0.375 * static_cast<double>(row + 1);
    }
    return lambda;
}

/// BITWISE equality, which is strictly stronger than `==`: it separates +0.0
/// from -0.0, which is exactly the discrepancy an accumulate-onto-+0.0 seed
/// would introduce, and it is what "bit-identical" is being claimed here.
bool same_bits(double left, double right) {
    return std::bit_cast<std::uint64_t>(left) == std::bit_cast<std::uint64_t>(right);
}

void expect_same_vector(const Vec &actual, const Vec &expected, const char *what) {
    SCOPED_TRACE(what);
    ASSERT_EQ(actual.size(), expected.size());
    for (Index row = 0; row < expected.size(); ++row) {
        // Both readings: `==` is what the brief pins, `same_bits` is the
        // stronger statement the seed discipline actually buys.
        EXPECT_EQ(actual[row], expected[row]) << "row " << row;
        EXPECT_TRUE(same_bits(actual[row], expected[row])) << "row " << row;
    }
}

/// Structure and values, compared as the CSR CONTENT every downstream reader
/// actually sees: dimensions, nonzero count, and the (row, column, value)
/// sequence the row-major iteration produces. That sequence is what
/// InnerIterator walks (the SOC/elastic shift), what a sparse transpose-product
/// accumulates in (evaluate_kkt's grad_lag), and what makeCompressed() lays out
/// (every QpProblem block) -- so two matrices agreeing on it agree on every
/// float any consumer computes from them, in the same order.
///
/// AND, WHERE BOTH SIDES ARE COMPRESSED, THE RAW ARRAYS TOO. That is the
/// stronger reading of "identical structure arrays", and it holds for every
/// QpProblem block unconditionally, because build_subproblem compresses. It
/// does NOT hold for an NlpEval Jacobian on a model that returns an
/// UNCOMPRESSED matrix -- tests/sqp/support/hs_problems.h's HS76 builds its
/// inequality Jacobian with insert() and never compresses it, so the free
/// function's NlpEval carries an uncompressed matrix where the seam's carries a
/// compressed one holding the same content. That divergence is pinned
/// explicitly by PublishesCompressedJacobiansWhereAModelReturnMayNotBe below,
/// rather than left for this comparator to hide.
void expect_same_matrix(const SpMatRM &actual, const SpMatRM &expected, const char *what) {
    SCOPED_TRACE(what);
    ASSERT_EQ(actual.rows(), expected.rows());
    ASSERT_EQ(actual.cols(), expected.cols());
    ASSERT_EQ(actual.nonZeros(), expected.nonZeros());

    for (Index outer = 0; outer < expected.outerSize(); ++outer) {
        SpMatRM::InnerIterator mine(actual, outer);
        SpMatRM::InnerIterator theirs(expected, outer);
        for (; mine && theirs; ++mine, ++theirs) {
            ASSERT_EQ(mine.row(), theirs.row()) << "outer " << outer;
            ASSERT_EQ(mine.col(), theirs.col()) << "outer " << outer;
            // Both readings: `==` is what the brief pins, `same_bits` is the
            // stronger statement the arena's seed discipline actually buys.
            EXPECT_EQ(mine.value(), theirs.value())
                << "outer " << outer << ", col " << theirs.col();
            EXPECT_TRUE(same_bits(mine.value(), theirs.value()))
                << "outer " << outer << ", col " << theirs.col();
        }
        ASSERT_FALSE(static_cast<bool>(mine)) << "actual has extra entries in outer " << outer;
        ASSERT_FALSE(static_cast<bool>(theirs)) << "actual is short in outer " << outer;
    }

    if (actual.isCompressed() && expected.isCompressed()) {
        for (Index outer = 0; outer <= actual.outerSize(); ++outer) {
            ASSERT_EQ(actual.outerIndexPtr()[outer], expected.outerIndexPtr()[outer])
                << "outer index " << outer;
        }
        for (Index entry = 0; entry < expected.nonZeros(); ++entry) {
            ASSERT_EQ(actual.innerIndexPtr()[entry], expected.innerIndexPtr()[entry])
                << "inner index " << entry;
            EXPECT_TRUE(same_bits(actual.valuePtr()[entry], expected.valuePtr()[entry]))
                << "value " << entry;
        }
    }
}

void expect_same_eval(const NlpEval &actual, const NlpEval &expected) {
    EXPECT_EQ(actual.f, expected.f);
    EXPECT_TRUE(same_bits(actual.f, expected.f));
    EXPECT_EQ(actual.all_finite, expected.all_finite);
    expect_same_vector(actual.grad, expected.grad, "grad");
    expect_same_vector(actual.ce, expected.ce, "ce");
    expect_same_vector(actual.ci, expected.ci, "ci");
    expect_same_matrix(actual.Je, expected.Je, "Je");
    expect_same_matrix(actual.Ji, expected.Ji, "Ji");
}

void expect_same_qp(const QpProblem &actual, const QpProblem &expected) {
    expect_same_matrix(actual.H, expected.H, "H");
    expect_same_vector(actual.g, expected.g, "g");
    expect_same_matrix(actual.Ae, expected.Ae, "Ae");
    expect_same_vector(actual.be, expected.be, "be");
    expect_same_matrix(actual.Ai, expected.Ai, "Ai");
    expect_same_vector(actual.bi, expected.bi, "bi");
    expect_same_vector(actual.lower, expected.lower, "lower");
    expect_same_vector(actual.upper, expected.upper, "upper");
}

/// True iff the two Hessians disagree anywhere -- the falsification battery's
/// reading of the same comparison the identity battery makes.
bool hessians_differ(const SpMatRM &left, const SpMatRM &right) {
    if (left.nonZeros() != right.nonZeros()) {
        return true;
    }
    for (Index entry = 0; entry < left.nonZeros(); ++entry) {
        if (!same_bits(left.valuePtr()[entry], right.valuePtr()[entry])) {
            return true;
        }
    }
    return false;
}

} // namespace

// ---------------------------------------------------------------------------
// Battery 1: bit-identity against the free functions
// ---------------------------------------------------------------------------

TEST(AggregateEvalSeamIdentity, EvalNlpReproducesTheFreeFunction) {
    for (const SeamFixture &fixture : fixtures()) {
        NlpModelAggregate aggregate(fixture.model_);
        AggregateEvalSeam seam(aggregate);
        for (const Vec &x : fixture.points_) {
            SCOPED_TRACE(fixture.name_ + " at " + std::to_string(x[0]));
            const Vec lambda_e = multipliers(fixture.model_->me(), 0.5);
            const Vec lambda_i = multipliers(fixture.model_->mi(), 0.25);
            expect_same_eval(seam.eval_nlp(x, lambda_e, lambda_i), eval_nlp(*fixture.model_, x));
        }
    }
}

TEST(AggregateEvalSeamIdentity, EvalNlpValuesReproducesTheFreeFunction) {
    for (const SeamFixture &fixture : fixtures()) {
        NlpModelAggregate aggregate(fixture.model_);
        AggregateEvalSeam seam(aggregate);
        for (const Vec &x : fixture.points_) {
            SCOPED_TRACE(fixture.name_ + " at " + std::to_string(x[0]));
            expect_same_eval(seam.eval_nlp_values(x), eval_nlp_values(*fixture.model_, x));
        }
    }
}

TEST(AggregateEvalSeamIdentity, RefreshDerivativesReproducesUpgradeToFull) {
    for (const SeamFixture &fixture : fixtures()) {
        NlpModelAggregate aggregate(fixture.model_);
        AggregateEvalSeam seam(aggregate);
        for (const Vec &x : fixture.points_) {
            SCOPED_TRACE(fixture.name_ + " at " + std::to_string(x[0]));
            NlpEval seam_ev = seam.eval_nlp_values(x);
            NlpEval free_ev = eval_nlp_values(*fixture.model_, x);
            seam.refresh_derivatives(seam_ev, x);
            upgrade_to_full(*fixture.model_, x, free_ev);
            expect_same_eval(seam_ev, free_ev);
            // The upgraded bundle is also the fresh full one, which is what
            // upgrade_to_full's own contract claims.
            expect_same_eval(seam_ev, eval_nlp(*fixture.model_, x));
        }
    }
}

TEST(AggregateEvalSeamIdentity, JacobiansOnlyReproducesTheProbeFetch) {
    for (const SeamFixture &fixture : fixtures()) {
        NlpModelAggregate aggregate(fixture.model_);
        AggregateEvalSeam seam(aggregate);
        for (const Vec &x : fixture.points_) {
            SCOPED_TRACE(fixture.name_ + " at " + std::to_string(x[0]));
            NlpEval seam_ev = seam.eval_nlp_values(x);
            NlpEval free_ev = eval_nlp_values(*fixture.model_, x);
            seam.jacobians_only(seam_ev, x);
            // The probe's own fetch, verbatim from the driver's make_warm_start.
            if (fixture.model_->me() > 0) {
                free_ev.Je = fixture.model_->eval_jac_e(x);
            }
            if (fixture.model_->mi() > 0) {
                free_ev.Ji = fixture.model_->eval_jac_i(x);
            }
            expect_same_eval(seam_ev, free_ev);
        }
    }
}

TEST(AggregateEvalSeamIdentity, BuildSubproblemReproducesTheFreeFunction) {
    for (const double obj_scale : {1.0, 0.75, 0.0}) {
        for (const SeamFixture &fixture : fixtures()) {
            NlpModelAggregate aggregate(fixture.model_);
            AggregateEvalSeam seam(aggregate);
            for (const Vec &x : fixture.points_) {
                SCOPED_TRACE(fixture.name_ + " at " + std::to_string(x[0]) + ", obj_scale " +
                             std::to_string(obj_scale));
                const Vec lambda_e = multipliers(fixture.model_->me(), 0.5);
                const Vec lambda_i = multipliers(fixture.model_->mi(), 0.25);
                const NlpEval seam_ev = seam.eval_nlp(x, lambda_e, lambda_i);
                const NlpEval free_ev = eval_nlp(*fixture.model_, x);
                expect_same_qp(
                    seam.build_subproblem(seam_ev, x, lambda_e, lambda_i, obj_scale),
                    build_subproblem(*fixture.model_, free_ev, x, lambda_e, lambda_i, obj_scale));
            }
        }
    }
}

TEST(AggregateEvalSeamIdentity, LaysTheDeclarationsDimensionsAndBounds) {
    for (const SeamFixture &fixture : fixtures()) {
        SCOPED_TRACE(fixture.name_);
        NlpModelAggregate aggregate(fixture.model_);
        AggregateEvalSeam seam(aggregate);
        EXPECT_EQ(seam.n(), fixture.model_->n());
        EXPECT_EQ(seam.me(), fixture.model_->me());
        EXPECT_EQ(seam.mi(), fixture.model_->mi());
        expect_same_vector(seam.lower(), fixture.model_->lower(), "lower");
        expect_same_vector(seam.upper(), fixture.model_->upper(), "upper");
        EXPECT_EQ(&seam.aggregate(), &aggregate);
        EXPECT_EQ(seam.epoch(), aggregate.structure_epoch());
    }
}

// THE ONE DECLARED DIVERGENCE, pinned rather than absorbed into a lenient
// comparator: the seam always publishes COMPRESSED matrices, because it copies
// its own laid pattern, while the free functions publish whatever storage mode
// the model's return happened to be in. HS76 builds its inequality Jacobian
// with insert() and never compresses it, so it is the fixture where the two
// modes differ.
//
// WHY IT IS CONTENT-NEUTRAL, and therefore not a break of the preservation bar:
// the (row, column, value) sequence a row-major iteration produces is identical
// -- which is what InnerIterator walks, what a sparse transpose-product
// accumulates in, and what makeCompressed() lays out -- so every float any
// consumer computes is identical, in the same order. The QpProblem blocks that
// carry these matrices downstream are compressed on BOTH sides, because
// build_subproblem compresses, and their raw arrays are asserted equal below.
TEST(AggregateEvalSeamIdentity, PublishesCompressedJacobiansWhereAModelReturnMayNotBe) {
    const auto model = std::make_shared<hven::solvers::test_support::Hs76Model>();
    const Vec x = (Vec(4) << 0.5, 0.5, 0.5, 0.5).finished();
    const Vec lambda_e = multipliers(0, 0.5);
    const Vec lambda_i = multipliers(3, 0.25);

    // The fixture's own property this pin depends on. If hs_problems.h ever
    // compresses this return, the divergence disappears and this line says so.
    ASSERT_FALSE(model->eval_jac_i(x).isCompressed());

    NlpModelAggregate aggregate(model);
    AggregateEvalSeam seam(aggregate);
    const NlpEval seam_ev = seam.eval_nlp(x, lambda_e, lambda_i);
    const NlpEval free_ev = eval_nlp(*model, x);

    EXPECT_FALSE(free_ev.Ji.isCompressed());
    EXPECT_TRUE(seam_ev.Ji.isCompressed());
    EXPECT_TRUE(seam_ev.Je.isCompressed());
    // Same content in the same order, which is the whole of what a consumer
    // reads off an NlpEval Jacobian.
    expect_same_matrix(seam_ev.Ji, free_ev.Ji, "Ji content");

    // And the block that actually travels downstream is compressed on both
    // sides, with the raw arrays equal -- expect_same_qp asserts them.
    const QpProblem seam_qp = seam.build_subproblem(seam_ev, x, lambda_e, lambda_i, 1.0);
    const QpProblem free_qp = build_subproblem(*model, free_ev, x, lambda_e, lambda_i, 1.0);
    ASSERT_TRUE(seam_qp.Ai.isCompressed());
    ASSERT_TRUE(free_qp.Ai.isCompressed());
    expect_same_qp(seam_qp, free_qp);
}

// ---------------------------------------------------------------------------
// Battery 2: the pin can fail
// ---------------------------------------------------------------------------

TEST(AggregateEvalSeamFalsification, ASwappedLocationBreaksTheHessianComparison) {
    const auto model = std::make_shared<SeamCouplingModel>();
    NlpModelAggregate aggregate(model);
    AggregateEvalSeam seam(aggregate);

    const Vec x = (Vec(3) << 0.7, -0.3, 1.25).finished();
    const Vec lambda_e = multipliers(1, 0.5);
    const Vec lambda_i = multipliers(2, 0.25);
    const NlpEval seam_ev = seam.eval_nlp(x, lambda_e, lambda_i);
    const NlpEval free_ev = eval_nlp(*model, x);
    const QpProblem reference = build_subproblem(*model, free_ev, x, lambda_e, lambda_i, 1.0);

    // The intact seam agrees, which is what makes the corrupted reading below a
    // statement about the corruption rather than about the fixture.
    ASSERT_FALSE(
        hessians_differ(seam.build_subproblem(seam_ev, x, lambda_e, lambda_i, 1.0).H, reference.H));

    // Two Hessian claim slots whose values differ at this point, so a swapped
    // permutation is observable at all.
    ASSERT_GE(AggregateEvalSeamTestAccess::hessian_claim_count(seam), 2);
    ASSERT_NE(reference.H.valuePtr()[0], reference.H.valuePtr()[1]);
    ASSERT_NE(AggregateEvalSeamTestAccess::location(seam, 0),
              AggregateEvalSeamTestAccess::location(seam, 1));

    AggregateEvalSeamTestAccess::swap_locations(seam, 0, 1);
    EXPECT_TRUE(
        hessians_differ(seam.build_subproblem(seam_ev, x, lambda_e, lambda_i, 1.0).H, reference.H));
}

// ---------------------------------------------------------------------------
// Battery 3: the structure epoch
// ---------------------------------------------------------------------------

TEST(AggregateEvalSeamEpoch, ARenegotiationForcesARelayAndTheOutputsStillAgree) {
    const auto model = std::make_shared<SeamCouplingModel>();
    NlpModelAggregate aggregate(model);
    AggregateEvalSeam seam(aggregate);

    const Vec x = (Vec(3) << 0.7, -0.3, 1.25).finished();
    const Vec lambda_e = multipliers(1, 0.5);
    const Vec lambda_i = multipliers(2, 0.25);
    const hven::solvers::StructureEpoch before = seam.epoch();

    // A renegotiation is a structural event by contract, even at the count
    // already in force: claim ORDER moved, so slot-indexed state is stale.
    EXPECT_EQ(aggregate.negotiate_partition_count(1), 1);
    EXPECT_NE(aggregate.structure_epoch(), before);
    EXPECT_EQ(seam.epoch(), before) << "the seam must not re-lay before it is next used";

    const NlpEval seam_ev = seam.eval_nlp(x, lambda_e, lambda_i);
    EXPECT_EQ(seam.epoch(), aggregate.structure_epoch());
    EXPECT_NE(seam.epoch(), before);
    expect_same_eval(seam_ev, eval_nlp(*model, x));
    expect_same_qp(seam.build_subproblem(seam_ev, x, lambda_e, lambda_i, 1.0),
                   build_subproblem(*model, eval_nlp(*model, x), x, lambda_e, lambda_i, 1.0));
}

namespace {

/// SeamCouplingModel with one structural zero it can drop: (0, 2) of the
/// inequality Jacobian, emitted every call until `drop_structural_zero()` is
/// called and never after. Dropping it is a genuine structural change -- the
/// claim stream is one slot shorter and every claim after it moves -- so a
/// re-lay that did nothing would be caught rather than papered over. The model
/// stays pattern-invariant on either side of the flip, which is what keeps this
/// a re-lay test rather than a violation test.
class SeamShrinkingJacobianModel : public SeamCouplingModel {
  public:
    void drop_structural_zero() { dropped_ = true; }

    SpMatRM eval_jac_i(const Vec &x) const override {
        if (!dropped_) {
            return SeamCouplingModel::eval_jac_i(x);
        }
        return make_jac(
            2, 3, {{0, 0, 2.0 * x(0)}, {0, 1, 1.0}, {1, 0, 0.0}, {1, 1, 1.0}, {1, 2, -4.0 * x(2)}});
    }

  private:
    bool dropped_ = false;
};

} // namespace

TEST(AggregateEvalSeamEpoch, ARelayPicksUpAClaimStreamThatActuallyChanged) {
    // The companion to the test above, and the one a no-op re-lay could not
    // pass: there the model's claims are identical before and after, so a seam
    // that merely re-read the epoch and rebuilt nothing would still agree with
    // the free functions. Here the claim stream genuinely moves.
    const auto model = std::make_shared<SeamShrinkingJacobianModel>();
    NlpModelAggregate aggregate(model);
    AggregateEvalSeam seam(aggregate);

    const Vec x = (Vec(3) << 0.7, -0.3, 1.25).finished();
    const Vec lambda_e = multipliers(1, 0.5);
    const Vec lambda_i = multipliers(2, 0.25);

    const NlpEval before = seam.eval_nlp(x, lambda_e, lambda_i);
    ASSERT_EQ(before.Ji.nonZeros(), 6);
    const Eigen::Index claims_before = aggregate.kkt_claim_rows().size();

    model->drop_structural_zero();
    ASSERT_EQ(aggregate.negotiate_partition_count(1), 1);
    ASSERT_EQ(aggregate.kkt_claim_rows().size(), claims_before - 1)
        << "the fixture must actually move the claim stream";

    const NlpEval after = seam.eval_nlp(x, lambda_e, lambda_i);
    EXPECT_EQ(seam.epoch(), aggregate.structure_epoch());
    EXPECT_EQ(after.Ji.nonZeros(), 5) << "the seam is still publishing the old pattern";
    expect_same_eval(after, eval_nlp(*model, x));
    expect_same_qp(seam.build_subproblem(after, x, lambda_e, lambda_i, 1.0),
                   build_subproblem(*model, eval_nlp(*model, x), x, lambda_e, lambda_i, 1.0));
}

TEST(AggregateEvalSeamEpoch, AStaleEpochOverBrokenStructuresHealsAtTheNextMoment) {
    // The commit point, pinned through the state it decides. A lay that throws
    // part-way leaves this seam's structures partial -- they are rebuilt in
    // place, not built-then-committed -- so the only thing standing between that
    // and a permanently wrong seam is WHEN the epoch is written. Committed last,
    // a failed lay leaves the epoch stale and the next moment re-lays; committed
    // first, the epoch would already say "current" and the partial structures
    // would never be rebuilt.
    //
    // The broken state is CONSTRUCTED rather than injected: no live
    // NlpModelAggregate can make this seam's lay throw (every refusal in it is
    // one the bridge has already made against its own claims), so the pin
    // reproduces the two states a throw would leave and asserts what each does
    // next.
    const auto model = std::make_shared<SeamCouplingModel>();
    NlpModelAggregate aggregate(model);
    AggregateEvalSeam seam(aggregate);

    const Vec x = (Vec(3) << 0.7, -0.3, 1.25).finished();
    const Vec lambda_e = multipliers(1, 0.5);
    const Vec lambda_i = multipliers(2, 0.25);
    const NlpEval free_ev = eval_nlp(*model, x);
    const QpProblem reference = build_subproblem(*model, free_ev, x, lambda_e, lambda_i, 1.0);
    const hven::solvers::StructureEpoch current = seam.epoch();

    // A seam whose structures are wrong, standing at the current epoch: this is
    // what a commit-first ordering would leave, and it stays wrong.
    ASSERT_GE(AggregateEvalSeamTestAccess::hessian_claim_count(seam), 2);
    AggregateEvalSeamTestAccess::swap_locations(seam, 0, 1);
    {
        const NlpEval broken = seam.eval_nlp(x, lambda_e, lambda_i);
        ASSERT_EQ(seam.epoch(), current);
        EXPECT_TRUE(hessians_differ(seam.build_subproblem(broken, x, lambda_e, lambda_i, 1.0).H,
                                    reference.H))
            << "the fixture must actually be broken for the recovery arm to mean anything";
    }

    // The same broken structures, standing at a STALE epoch: this is what the
    // commit-last ordering leaves, and the next moment repairs it.
    AggregateEvalSeamTestAccess::adopt_epoch(seam, hven::solvers::StructureEpoch{});
    ASSERT_NE(seam.epoch(), aggregate.structure_epoch());

    const NlpEval healed = seam.eval_nlp(x, lambda_e, lambda_i);
    EXPECT_EQ(seam.epoch(), aggregate.structure_epoch());
    expect_same_eval(healed, free_ev);
    expect_same_qp(seam.build_subproblem(healed, x, lambda_e, lambda_i, 1.0), reference);
}

// ---------------------------------------------------------------------------
// Battery 4: the arena carries nothing between calls
// ---------------------------------------------------------------------------

TEST(AggregateEvalSeamArena, ConsecutiveEvaluationsDoNotAccumulate) {
    for (const SeamFixture &fixture : fixtures()) {
        NlpModelAggregate aggregate(fixture.model_);
        AggregateEvalSeam seam(aggregate);
        const Vec lambda_e = multipliers(fixture.model_->me(), 0.5);
        const Vec lambda_i = multipliers(fixture.model_->mi(), 0.25);

        // Every point in turn, then the first one again: a leaked arena would
        // show up either as a second reading that carries the first, or as a
        // repeat of the first that no longer matches itself.
        for (const Vec &x : fixture.points_) {
            SCOPED_TRACE(fixture.name_ + " at " + std::to_string(x[0]));
            const NlpEval seam_ev = seam.eval_nlp(x, lambda_e, lambda_i);
            const NlpEval free_ev = eval_nlp(*fixture.model_, x);
            expect_same_eval(seam_ev, free_ev);
            expect_same_qp(seam.build_subproblem(seam_ev, x, lambda_e, lambda_i, 1.0),
                           build_subproblem(*fixture.model_, free_ev, x, lambda_e, lambda_i, 1.0));
        }
        const Vec &first = fixture.points_.front();
        SCOPED_TRACE(fixture.name_ + " revisiting the first point");
        const NlpEval seam_ev = seam.eval_nlp(first, lambda_e, lambda_i);
        const NlpEval free_ev = eval_nlp(*fixture.model_, first);
        expect_same_eval(seam_ev, free_ev);
        expect_same_qp(seam.build_subproblem(seam_ev, first, lambda_e, lambda_i, 1.0),
                       build_subproblem(*fixture.model_, free_ev, first, lambda_e, lambda_i, 1.0));
    }
}

// ---------------------------------------------------------------------------
// SqpDriverEntryEquivalence -- the two doors into one solve
// ---------------------------------------------------------------------------
//
// WHAT IS BEING PINNED, and why it is a different claim from the four batteries
// above. Those pin that the SEAM reproduces the free functions bit for bit at
// one point. This one pins the consequence at the level a caller sees: that
// SqpDriver::solve(NlpModelAggregate &, ...) -- the primary path -- and
// SqpDriver::solve(const NlpModel &, ...) -- the convenience wrapper, which
// borrows the model into a bridge of its own for the duration of the call --
// run THE SAME SOLVE. Same status, bit-equal iterate, every counter equal,
// and a WarmStart whose emitted fields agree field by field, on both a cold
// and a warm arm of two battery families.
//
// THE ASYMMETRY IS DELIBERATE AND IS THE INTERESTING PART. The bridge arm
// builds ONE NlpModelAggregate outside both of its solves; the model arm
// builds one PER CALL. So on the warm arm the two sides do not perform the
// same number of bridge lays -- two against one -- and the driver's counters
// must still agree to the integer, because a lay is a structural walk of the
// model's three derivative patterns and is invisible to every counter the
// driver keeps (include/hven/model/nlp_model_aggregate.h's constructor doc;
// SqpDriverContract.CallCountPerMajorIsBounded is where that cost IS pinned,
// against a counting model). PLAIN MODELS ARE USED HERE FOR EXACTLY THAT
// REASON: a CountingModel would make the two arms differ by one lay's worth of
// derivative calls, which is a fact about the entry points rather than about
// the solve, and pinning it here would only restate what that other test
// already says.
//
// F1 AND F3n50 ARE THE BATTERY'S OWN TWO SMALL FAMILIES (tests/sqp/
// test_warm_start_battery.cpp builds its table from these constructors): F1 is
// a 2-variable box QP whose single subproblem is the whole answer, F3n50 a
// 50-variable spring chain that takes several majors and activates a real
// constraint. Between them the arms cover a solve that converges immediately
// and one that iterates.
namespace {

/// Every field of SqpCounters, including the nested SSN block. Written out
/// rather than looped because there is no reflection here and a missed field is
/// a silent hole: this is the assertion the whole equivalence claim rests on.
void expect_same_counters(const SqpCounters &bridge, const SqpCounters &model,
                          const std::string &tag) {
    SCOPED_TRACE(tag);
    EXPECT_EQ(bridge.major_iters, model.major_iters);
    EXPECT_EQ(bridge.qp_minor_iters, model.qp_minor_iters);
    EXPECT_EQ(bridge.factorizations, model.factorizations);
    EXPECT_EQ(bridge.steps_accepted, model.steps_accepted);
    EXPECT_EQ(bridge.rejected_steps, model.rejected_steps);
    EXPECT_EQ(bridge.soc_steps, model.soc_steps);
    EXPECT_EQ(bridge.soc_applied, model.soc_applied);
    EXPECT_EQ(bridge.soc_qp_infeasible, model.soc_qp_infeasible);
    EXPECT_EQ(bridge.soc_rejected, model.soc_rejected);
    EXPECT_EQ(bridge.elastic_activations, model.elastic_activations);
    EXPECT_EQ(bridge.elastic_escalations, model.elastic_escalations);
    EXPECT_EQ(bridge.restoration_iters, model.restoration_iters);
    EXPECT_EQ(bridge.eqp_refine_steps, model.eqp_refine_steps);
    EXPECT_EQ(bridge.border_refine_steps, model.border_refine_steps);
    EXPECT_EQ(bridge.suspect_escalations, model.suspect_escalations);
    EXPECT_EQ(bridge.symbolic_analyses, model.symbolic_analyses);
    EXPECT_EQ(bridge.start_level_used, model.start_level_used);
    EXPECT_EQ(bridge.full_step_majors, model.full_step_majors);
    EXPECT_EQ(bridge.watchdog_restores, model.watchdog_restores);
    EXPECT_EQ(bridge.evals_full, model.evals_full);
    EXPECT_EQ(bridge.evals_values, model.evals_values);
    EXPECT_EQ(bridge.probe_budget_stops, model.probe_budget_stops);
    EXPECT_EQ(bridge.crash_seeded_rows, model.crash_seeded_rows);
    EXPECT_EQ(bridge.crash_seeded_bounds, model.crash_seeded_bounds);
    EXPECT_EQ(bridge.n_seeded, model.n_seeded);
    EXPECT_EQ(bridge.seeded_clamped, model.seeded_clamped);
    EXPECT_EQ(bridge.ip_activity_inferred, model.ip_activity_inferred);
    EXPECT_EQ(bridge.ssn.ssn_iters, model.ssn.ssn_iters);
    EXPECT_EQ(bridge.ssn.ssn_bulk_flips, model.ssn.ssn_bulk_flips);
    EXPECT_EQ(bridge.ssn.ssn_backtracks, model.ssn.ssn_backtracks);
    EXPECT_EQ(bridge.ssn.ssn_prox_updates, model.ssn.ssn_prox_updates);
    EXPECT_EQ(bridge.ssn.ssn_escapes, model.ssn.ssn_escapes);
    EXPECT_EQ(bridge.ssn.ssn_uncertain_peak, model.ssn.ssn_uncertain_peak);
}

/// Bit-equality, not near-equality: two vectors of the same length whose
/// doubles compare bitwise identical. NaN would compare unequal under ==, and
/// -0.0 == +0.0 would compare EQUAL under it, so neither is left to chance.
void expect_bit_equal(const Vec &bridge, const Vec &model, const std::string &what) {
    SCOPED_TRACE(what);
    ASSERT_EQ(bridge.size(), model.size());
    for (Index i = 0; i < bridge.size(); ++i) {
        EXPECT_EQ(std::bit_cast<std::uint64_t>(bridge(i)), std::bit_cast<std::uint64_t>(model(i)))
            << "entry " << i << ": " << bridge(i) << " vs " << model(i);
    }
}

/// The emitted hand-off, field by field. `hot` is compared by PRESENCE rather
/// than by address: it is a handle onto one driver's own engine state, so two
/// drivers cannot share one and equality of the pointers is not the claim.
void expect_same_warm_start(const WarmStart &bridge, const WarmStart &model,
                            const std::string &tag) {
    SCOPED_TRACE(tag);
    EXPECT_EQ(bridge.valid, model.valid);
    EXPECT_EQ(bridge.structure_hash, model.structure_hash);
    expect_bit_equal(bridge.x, model.x, "warm.x");
    expect_bit_equal(bridge.lambda_e, model.lambda_e, "warm.lambda_e");
    expect_bit_equal(bridge.lambda_i, model.lambda_i, "warm.lambda_i");
    expect_bit_equal(bridge.z, model.z, "warm.z");
    EXPECT_EQ(bridge.ineq_active, model.ineq_active);
    EXPECT_EQ(bridge.bound_active, model.bound_active);
    EXPECT_EQ(bridge.qp_working_set.n(), model.qp_working_set.n());
    EXPECT_EQ(bridge.qp_working_set.mi(), model.qp_working_set.mi());
    EXPECT_EQ(bridge.qp_working_set.active_ineq(), model.qp_working_set.active_ineq());
    EXPECT_EQ(bridge.qp_working_set.bound_state(), model.qp_working_set.bound_state());
    EXPECT_EQ(bridge.funnel_width, model.funnel_width);
    EXPECT_EQ(bridge.tr_radius, model.tr_radius);
    EXPECT_EQ(bridge.primal_delta, model.primal_delta);
    EXPECT_EQ(bridge.dual_mu, model.dual_mu);
    EXPECT_EQ(bridge.has_prox_center, model.has_prox_center);
    EXPECT_EQ(bridge.prox_sigma, model.prox_sigma);
    expect_bit_equal(bridge.prox_center_x, model.prox_center_x, "warm.prox_center_x");
    expect_bit_equal(bridge.prox_center_lambda, model.prox_center_lambda,
                     "warm.prox_center_lambda");
    EXPECT_EQ(bridge.hot != nullptr, model.hot != nullptr);
}

/// Every field of every history row, floats compared bit for bit.
///
/// Length alone is not the claim. Two solves can take the same number of majors
/// down different paths -- a different trust-region trajectory, a rejected step
/// where the other accepted, an SOC that fired on one side only -- and a
/// length-only comparison would call those equal. The rows carry exactly the
/// per-major record the diagnostics contract publishes, so comparing them is
/// comparing the two paths and not just their endpoints.
void expect_same_history(const std::vector<SqpIterate> &bridge,
                         const std::vector<SqpIterate> &model, const std::string &tag) {
    SCOPED_TRACE(tag);
    ASSERT_EQ(bridge.size(), model.size());
    for (std::size_t k = 0; k < model.size(); ++k) {
        SCOPED_TRACE("history row " + std::to_string(k));
        const SqpIterate &b = bridge[k];
        const SqpIterate &m = model[k];
        EXPECT_EQ(b.trial, m.trial);
        EXPECT_EQ(std::bit_cast<std::uint64_t>(b.f), std::bit_cast<std::uint64_t>(m.f));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(b.stationarity),
                  std::bit_cast<std::uint64_t>(m.stationarity));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(b.feasibility),
                  std::bit_cast<std::uint64_t>(m.feasibility));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(b.complementarity),
                  std::bit_cast<std::uint64_t>(m.complementarity));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(b.kkt_residual),
                  std::bit_cast<std::uint64_t>(m.kkt_residual));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(b.violation_l1),
                  std::bit_cast<std::uint64_t>(m.violation_l1));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(b.tr_radius),
                  std::bit_cast<std::uint64_t>(m.tr_radius));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(b.mu), std::bit_cast<std::uint64_t>(m.mu));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(b.step_norm),
                  std::bit_cast<std::uint64_t>(m.step_norm));
        EXPECT_EQ(b.qp_solved, m.qp_solved);
        EXPECT_EQ(b.qp_status, m.qp_status);
        EXPECT_EQ(b.qp_minor_iters, m.qp_minor_iters);
        EXPECT_EQ(b.qp_factorizations, m.qp_factorizations);
        EXPECT_EQ(b.tr_binding, m.tr_binding);
        EXPECT_EQ(b.verdict, m.verdict);
        EXPECT_EQ(b.soc_applied, m.soc_applied);
        EXPECT_EQ(b.elastic_applied, m.elastic_applied);
        EXPECT_EQ(b.watchdog_restored, m.watchdog_restored);
    }
}

/// The whole comparison for one solve pair.
void expect_same_solution(const SqpSolution &bridge, const SqpSolution &model,
                          const std::string &tag) {
    SCOPED_TRACE(tag);
    ASSERT_EQ(bridge.status, model.status);
    EXPECT_EQ(bridge.infeasibility_certified, model.infeasibility_certified);
    EXPECT_EQ(std::bit_cast<std::uint64_t>(bridge.f), std::bit_cast<std::uint64_t>(model.f));
    expect_bit_equal(bridge.x, model.x, "x");
    expect_bit_equal(bridge.lambda_e, model.lambda_e, "lambda_e");
    expect_bit_equal(bridge.lambda_i, model.lambda_i, "lambda_i");
    expect_bit_equal(bridge.z, model.z, "z");
    expect_same_history(bridge.history, model.history, tag + " history");
    expect_same_counters(bridge.counters, model.counters, tag + " counters");
    expect_same_warm_start(bridge.warm_start, model.warm_start, tag + " warm_start");
}

/// One family, both entries, cold arm. The two models are separate instances of
/// the same family at the same parameter, so neither solve can see the other's
/// state.
template <typename Make> void check_cold_arm(Make make, const std::string &tag) {
    const auto model_side = std::make_shared<decltype(make())>(make());
    const auto bridge_side = std::make_shared<decltype(make())>(make());
    const Vec x0 = model_side->start_point();

    SqpOptions opts;
    SqpDriver model_driver{opts};
    const SqpSolution via_model = model_driver.solve(*model_side, x0);

    // ONE bridge, built outside the solve -- the hot-loop shape the driver
    // header's model-taking overloads point callers at.
    NlpModelAggregate bridge{bridge_side};
    SqpDriver bridge_driver{opts};
    const SqpSolution via_bridge = bridge_driver.solve(bridge, x0);

    ASSERT_EQ(via_model.status, SqpStatus::kOptimal) << tag;
    expect_same_solution(via_bridge, via_model, tag + " cold");
}

/// One family, both entries, warm arm: each side seeds itself from its OWN cold
/// solve on its own driver, then re-solves through the same entry with that
/// hand-off. The bridge side reuses its single aggregate across both solves,
/// which is the asymmetry this battery exists to license.
template <typename Make> void check_warm_arm(Make make, const std::string &tag) {
    const auto model_side = std::make_shared<decltype(make())>(make());
    const auto bridge_side = std::make_shared<decltype(make())>(make());
    const Vec x0 = model_side->start_point();

    SqpOptions opts;
    SqpDriver model_driver{opts};
    const SqpSolution model_seed = model_driver.solve(*model_side, x0);
    ASSERT_TRUE(model_seed.warm_start.valid) << tag;
    const SqpSolution via_model = model_driver.solve(*model_side, x0, model_seed.warm_start);

    NlpModelAggregate bridge{bridge_side};
    SqpDriver bridge_driver{opts};
    const SqpSolution bridge_seed = bridge_driver.solve(bridge, x0);
    ASSERT_TRUE(bridge_seed.warm_start.valid) << tag;
    const SqpSolution via_bridge = bridge_driver.solve(bridge, x0, bridge_seed.warm_start);

    // THE SEEDING SOLVES ARE COMPARED IN FULL, not just their hand-offs, and
    // that is not redundant with the cold battery: those two solves are what
    // put each driver's ENGINE into the state the warm solve then ingests
    // from, so a divergence confined to them would otherwise be invisible here
    // -- the warm solves would agree with each other while having been reached
    // from different places, and the hot handle each offers would be the only
    // trace, compared here by presence rather than by content.
    expect_same_solution(bridge_seed, model_seed, tag + " seed solve");
    expect_same_warm_start(bridge_seed.warm_start, model_seed.warm_start, tag + " seed hand-off");
    // AND THE WARM ARM MUST ACTUALLY INGEST, or this is a second cold pin.
    ASSERT_NE(via_model.counters.start_level_used, StartLevel::kCold) << tag;
    expect_same_solution(via_bridge, via_model, tag + " warm");
}

} // namespace

TEST(SqpDriverEntryEquivalence, ColdArmAgreesOnF1AndF3n50) {
    check_cold_arm([] { return hven::solvers::test_support::F1BoxQp(0.0); }, "F1");
    check_cold_arm([] { return hven::solvers::test_support::F3SpringChain(50, 0.5, 0.25); },
                   "F3n50");
}

TEST(SqpDriverEntryEquivalence, WarmArmAgreesOnF1AndF3n50) {
    check_warm_arm([] { return hven::solvers::test_support::F1BoxQp(0.0); }, "F1");
    check_warm_arm([] { return hven::solvers::test_support::F3SpringChain(50, 0.5, 0.25); },
                   "F3n50");
}
