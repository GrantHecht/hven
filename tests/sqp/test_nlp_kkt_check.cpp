// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_nlp_kkt_check.cpp — M6 W0.4. The pins on
// tests/sqp/support/nlp_kkt_check.h's `self_check_kkt` NON-FINITE CONTRACT.
//
// self_check_kkt is the primary correctness guard of the HS battery, the B1
// gate, the parametric families and every corpus row (bench/corpus_cells.h's
// record_kkt_check calls it on EVERY row of EVERY engine), so what it does with
// garbage decides whether those gates can be believed. Until W0.4 the answer
// was: it dropped it. Every accumulation in that function is a max, every
// one-sided violation term is a `std::max(0.0, raw)` ">= 0" applicability
// clamp, and both hand back their finite argument when handed a NaN -- so a NaN
// gradient, residual, multiplier or bound scored all four residuals at 0.0, an
// exactly-optimal reading computed from nothing. That hole is why M4 Task 5's
// scorer-vs-recorded verdict equality had to be cited "over finite rows"
// (docs/notes/2026-08-22-m4-task5-close.md §F) and is the registration this
// file closes.
//
// The contract now: any non-finite input scores ALL FOUR residuals +inf --
// bench/model_surface_kkt.h's semantics exactly, because the two checkers are
// compared cell by cell and must agree on poisoned rows as well as clean ones.
// An INFINITE BOUND stays ordinary and legal; the control below pins that,
// since a fix that made every unbounded model score +inf would take the
// battery's 14 all-infinite-bound problems with it.
//
// EVERY TEST HERE IS ARITHMETIC ON A HAND-BUILT TWO-VARIABLE MODEL. Nothing
// solves, nothing factorizes, no backend is touched -- the poison is injected
// at the model surface and at the returned quadruple, which is where a real
// poisoned row's poison comes from.

#include <cmath>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include <Eigen/SparseCore>

#include <hven/core/types.h>
#include <hven/drivers/sqp_types.h>
#include <hven/model/nlp_model.h>

#include "support/nlp_kkt_check.h"

namespace {

using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using hven::solvers::NlpModel;
using hven::solvers::SqpSolution;
using hven::solvers::test_support::NlpKktResidual;
using hven::solvers::test_support::self_check_kkt;

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// min 0.5 x'x  s.t.  x0 + x1 - 2 = 0,  x0 - 5 <= 0,  0 <= x <= 10.
//
// Its KKT point is x* = (1, 1), lambda_e = -1, lambda_i = 0, z = 0: the
// inequality is slack (cI = -4), both bounds are slack, and the equality
// residual is exactly 0 -- so a clean self_check_kkt reads four zeros and any
// nonzero reading is the poison talking.
//
// Every quantity self_check_kkt reads off the MODEL carries an additive poison
// knob, so a NaN or an infinity can be placed at exactly one site at a time.
// Poison at the SOLUTION side needs no knob: the caller owns that struct.
class PoisonableNlp : public NlpModel {
  public:
    PoisonableNlp() : lower_(Vec::Zero(2)), upper_(Vec::Constant(2, 10.0)) {}

    double grad_poison_ = 0.0; // added to grad(0)
    double ce_poison_ = 0.0;   // added to cE(0)
    double ci_poison_ = 0.0;   // added to cI(0)

    Vec lower_;
    Vec upper_;

    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override { return 0.5 * x.squaredNorm(); }

    Vec eval_grad(const Vec &x) const override {
        Vec g = x;
        g(0) += grad_poison_;
        return g;
    }

    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c(0) = x(0) + x(1) - 2.0 + ce_poison_;
        return c;
    }

    Vec eval_ci(const Vec &x) const override {
        Vec c(1);
        c(0) = x(0) - 5.0 + ci_poison_;
        return c;
    }

    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        // Upper triangle of obj_scale * I; the constraints are affine, so they
        // contribute nothing. Both diagonal entries are emitted whatever
        // obj_scale is, per nlp_model.h's structural-pattern invariance.
        std::vector<Eigen::Triplet<double>> t{{0, 0, obj_scale}, {1, 1, obj_scale}};
        SpMatRM h(2, 2);
        h.setFromTriplets(t.begin(), t.end());
        h.makeCompressed();
        return h;
    }

    SpMatRM eval_jac_e(const Vec &) const override {
        std::vector<Eigen::Triplet<double>> t{{0, 0, 1.0}, {0, 1, 1.0}};
        SpMatRM j(1, 2);
        j.setFromTriplets(t.begin(), t.end());
        j.makeCompressed();
        return j;
    }

    SpMatRM eval_jac_i(const Vec &) const override {
        // The (0, 1) entry is a STRUCTURAL ZERO, emitted rather than skipped --
        // nlp_model.h requires the pattern to be x-independent.
        std::vector<Eigen::Triplet<double>> t{{0, 0, 1.0}, {0, 1, 0.0}};
        SpMatRM j(1, 2);
        j.setFromTriplets(t.begin(), t.end());
        j.makeCompressed();
        return j;
    }

    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Zero(2); }
};

// The KKT point above, as a driver would have returned it.
SqpSolution optimal_solution() {
    SqpSolution sol;
    sol.status = hven::solvers::SqpStatus::kOptimal;
    sol.x = Vec(2);
    sol.x << 1.0, 1.0;
    sol.lambda_e = Vec::Constant(1, -1.0);
    sol.lambda_i = Vec::Zero(1);
    sol.z = Vec::Zero(2);
    return sol;
}

constexpr double kBoundTol = 1e-8;

bool all_four_are_positive_infinity(const NlpKktResidual &r) {
    return r.stationarity == kInf && r.primal == kInf && r.dual_sign == kInf &&
           r.complementarity == kInf;
}

// The pre-fix silent-drop SHAPE, named so it can be refused by name: four
// residuals at (or indistinguishable from) zero, which is what the max-based
// fold used to report for every poison site swept below.
bool reads_as_a_clean_optimum(const NlpKktResidual &r) {
    return r.stationarity < 1e-12 && r.primal < 1e-12 && r.dual_sign < 1e-12 &&
           r.complementarity < 1e-12;
}

// ---------------------------------------------------------------------------
// The control: finite data still scores finite, and an INFINITE BOUND is not
// poison. Without this the fix could pass its own pins by declaring everything
// broken.
// ---------------------------------------------------------------------------
TEST(NlpKktSelfCheck, CleanPointScoresFourZerosAndInfiniteBoundsStayLegal) {
    PoisonableNlp model;
    const SqpSolution sol = optimal_solution();

    const NlpKktResidual boxed = self_check_kkt(model, sol, kBoundTol);
    EXPECT_TRUE(reads_as_a_clean_optimum(boxed));
    EXPECT_NEAR(boxed.stationarity, 0.0, 1e-14);
    EXPECT_NEAR(boxed.primal, 0.0, 1e-14);
    EXPECT_NEAR(boxed.dual_sign, 0.0, 1e-14);
    EXPECT_NEAR(boxed.complementarity, 0.0, 1e-14);

    // Free on both sides -- the shape 14 of the HS battery's 27 problems have.
    // -inf/+inf are BOUNDS, not poison, and the reading must not move.
    model.lower_ = Vec::Constant(2, -kInf);
    model.upper_ = Vec::Constant(2, kInf);
    const NlpKktResidual unbounded = self_check_kkt(model, sol, kBoundTol);
    EXPECT_TRUE(reads_as_a_clean_optimum(unbounded));
    EXPECT_TRUE(std::isfinite(unbounded.stationarity));
    EXPECT_TRUE(std::isfinite(unbounded.primal));
    EXPECT_TRUE(std::isfinite(unbounded.dual_sign));
    EXPECT_TRUE(std::isfinite(unbounded.complementarity));

    // One finite side, one infinite -- the mixed case, likewise ordinary.
    model.lower_ = Vec::Zero(2);
    model.upper_ = Vec::Constant(2, kInf);
    const NlpKktResidual half = self_check_kkt(model, sol, kBoundTol);
    EXPECT_TRUE(reads_as_a_clean_optimum(half));
    EXPECT_TRUE(std::isfinite(half.complementarity));
}

// ---------------------------------------------------------------------------
// A NaN COMPONENT SURFACES IN THE REPORTED VALUE.
//
// The inequality residual is the sharpest demonstration of the old defect
// because BOTH of the terms it feeds dropped it: `std::max(0.0, NaN)` returns
// 0.0 (the ">= 0" clamp), and `std::max(0.0, |lambda_i * NaN|)` returns 0.0
// again (the fold). Pre-fix this scored four zeros -- a perfect KKT point on a
// model that had just returned NaN.
// ---------------------------------------------------------------------------
TEST(NlpKktSelfCheck, NanInequalityResidualSurfacesAsInfiniteResiduals) {
    PoisonableNlp model;
    model.ci_poison_ = kNan;
    const NlpKktResidual r = self_check_kkt(model, optimal_solution(), kBoundTol);

    EXPECT_FALSE(reads_as_a_clean_optimum(r)) << "the NaN was silently dropped";
    EXPECT_FALSE(std::isfinite(r.primal));
    EXPECT_FALSE(std::isfinite(r.complementarity));
    EXPECT_TRUE(all_four_are_positive_infinity(r))
        << "the contract is +inf on ALL FOUR, matching bench/model_surface_kkt.h";
}

// ---------------------------------------------------------------------------
// AN INF COMPONENT SURFACES.
//
// An infinity survives a max on its own, so the interesting site is the one
// where an infinity BECOMES a dropped NaN: |lambda_i * cI| with lambda_i
// exactly 0 and cI infinite is 0 * inf = NaN, which the fold then dropped. Pre-
// fix `complementarity` and `dual_sign` read 0.0 here while `primal` read +inf
// -- one poisoned point, two different verdicts, which is exactly the
// inconsistency the all-four rule removes.
// ---------------------------------------------------------------------------
TEST(NlpKktSelfCheck, InfiniteInequalityResidualSurfacesInEveryResidual) {
    PoisonableNlp model;
    model.ci_poison_ = kInf;
    const NlpKktResidual r = self_check_kkt(model, optimal_solution(), kBoundTol);

    EXPECT_FALSE(std::isfinite(r.complementarity))
        << "0 * inf became a NaN and the fold dropped it";
    EXPECT_FALSE(std::isfinite(r.dual_sign));
    EXPECT_TRUE(all_four_are_positive_infinity(r));
}

// ---------------------------------------------------------------------------
// THE ">= 0 APPLICABILITY" LEAK, AT THE BOUND SITE.
//
// A NaN lower bound used to leak through THREE conditions at once: both
// `std::max(0.0, NaN - x)` violation clamps returned 0.0, the
// at_lower/at_upper classification read false-and-false and so filed the
// variable as FREE, and the complementarity term's own dist went NaN and was
// dropped by the fold. A model with a corrupt box scored as a clean optimum.
// ---------------------------------------------------------------------------
TEST(NlpKktSelfCheck, NanBoundIsNotReadAsAFreeVariable) {
    PoisonableNlp model;
    model.lower_(0) = kNan;
    const NlpKktResidual lower_poisoned = self_check_kkt(model, optimal_solution(), kBoundTol);
    EXPECT_FALSE(reads_as_a_clean_optimum(lower_poisoned));
    EXPECT_TRUE(all_four_are_positive_infinity(lower_poisoned));

    PoisonableNlp upper_model;
    upper_model.upper_(1) = kNan;
    const NlpKktResidual upper_poisoned =
        self_check_kkt(upper_model, optimal_solution(), kBoundTol);
    EXPECT_TRUE(all_four_are_positive_infinity(upper_poisoned));
}

// ---------------------------------------------------------------------------
// THE PRE-FIX SHAPE IS REFUSED AT EVERY SITE.
//
// The three tests above pin the two named component classes; this one is the
// regression guard proper. It walks every quantity self_check_kkt consumes --
// the returned quadruple, the model's gradient and both residual blocks, both
// bound vectors, and the activity tolerance itself -- and asserts that NONE of
// them can be poisoned into the clean-optimum shape. A future edit that
// re-introduces a max-based fold at any one of these sites fails here rather
// than silently certifying a garbage row somewhere in the HS battery.
// ---------------------------------------------------------------------------
TEST(NlpKktSelfCheck, NoPoisonedInputEverScoresAsACleanOptimum) {
    struct Site {
        const char *name;
        void (*poison)(PoisonableNlp &, SqpSolution &, double &);
    };
    const Site sites[] = {
        {"x", [](PoisonableNlp &, SqpSolution &s, double &) { s.x(0) = kNan; }},
        {"x infinite", [](PoisonableNlp &, SqpSolution &s, double &) { s.x(1) = kInf; }},
        {"z", [](PoisonableNlp &, SqpSolution &s, double &) { s.z(1) = kNan; }},
        {"z infinite", [](PoisonableNlp &, SqpSolution &s, double &) { s.z(0) = kInf; }},
        {"lambda_e", [](PoisonableNlp &, SqpSolution &s, double &) { s.lambda_e(0) = kNan; }},
        {"lambda_i", [](PoisonableNlp &, SqpSolution &s, double &) { s.lambda_i(0) = kNan; }},
        {"gradient", [](PoisonableNlp &m, SqpSolution &, double &) { m.grad_poison_ = kNan; }},
        {"gradient infinite",
         [](PoisonableNlp &m, SqpSolution &, double &) { m.grad_poison_ = kInf; }},
        {"cE", [](PoisonableNlp &m, SqpSolution &, double &) { m.ce_poison_ = kNan; }},
        {"cI", [](PoisonableNlp &m, SqpSolution &, double &) { m.ci_poison_ = kNan; }},
        {"lower bound", [](PoisonableNlp &m, SqpSolution &, double &) { m.lower_(1) = kNan; }},
        {"upper bound", [](PoisonableNlp &m, SqpSolution &, double &) { m.upper_(0) = kNan; }},
        {"bound_tol", [](PoisonableNlp &, SqpSolution &, double &t) { t = kNan; }},
    };

    for (const Site &site : sites) {
        PoisonableNlp model;
        SqpSolution sol = optimal_solution();
        double tol = kBoundTol;
        site.poison(model, sol, tol);

        const NlpKktResidual r = self_check_kkt(model, sol, tol);
        EXPECT_FALSE(reads_as_a_clean_optimum(r))
            << "poison at '" << site.name << "' scored as a clean optimum";
        EXPECT_TRUE(all_four_are_positive_infinity(r)) << "poison at '" << site.name << "'";
    }
}

} // namespace
