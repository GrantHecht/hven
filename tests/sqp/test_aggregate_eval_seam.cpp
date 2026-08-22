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
// FOUR BATTERIES:
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
