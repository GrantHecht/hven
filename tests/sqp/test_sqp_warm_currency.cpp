// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_sqp_warm_currency.cpp — SqpDriver::export_warm_start /
// stage_warm_start, the SQP engine's half of the M5 warm-start currency
// (M5 W3 Task 4).
//
// WHAT IS PINNED HERE, in the order the surface is used:
//   * the no-completed-solve export refusal, and the round trip that follows a
//     real solve (blocks == the solution's, widths == the model's, stamp == the
//     bridge's key);
//   * staging: finiteness and internal consistency refused AT staging; block
//     sizes and the stamp refused at SOLVE ENTRY, against the problem that call
//     binds; one-shot consumption (the second solve is cold);
//   * the two-sources refusal when an explicit `warm` argument meets a staged
//     value, and the one thing that refusal deliberately does NOT do (consume);
//   * the two application routes -- the "hven.ipm.polish.v1" crossover through
//     to_sqp_warm_start, and the core-only seed -- both entering at
//     StartLevel::kSeeded, and a foreign tag skipped silently;
//   * R5 determinism, instrumented on `SqpSolution::history[0]` (the first
//     iterate's measured row, a bit-exact function of the ingested start state);
//   * the interior-point -> SQP composition end to end, natively: one declared
//     problem, one exported value, no conversion and no re-stamp anywhere.
//     That composition is what the DECLARATION-IDENTITY stamp bought (owner
//     ruling, 2026-08-25) and the last two tests here pin both halves of it --
//     the cross-engine hand-off that now works, and the declaration change
//     that still refuses.
//
// Names carry a `Currency` prefix: this suite's TUs share a link unit, so
// file-scope names must not collide with the other files here.

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>
#include <gtest/gtest.h>

#include <hven/detail/warmstart/warm_start.h>
#include <hven/drivers/interior_point_solver.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>
#include <hven/model/nlp_model.h>
#include <hven/model/nlp_model_aggregate.h>
#include <hven/model/nlp_problem_model.h>
#include <hven/model/nlp_solver.h>
#include <hven/warmstart/ipm_polish_extension.h>
#include <hven/warmstart/warm_start_data.h>

using hven::ConstEigenRef;
using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using hven::solvers::declaration_key;
using hven::solvers::DeclarationKey;
using hven::solvers::IpmPolishData;
using hven::solvers::kIpmPolishTag;
using hven::solvers::NlpModel;
using hven::solvers::NlpModelAggregate;
using hven::solvers::NLPProblem;
using hven::solvers::NlpProblemModel;
using hven::solvers::NLPSolver;
using hven::solvers::serialize_ipm_polish;
using hven::solvers::SqpCounters;
using hven::solvers::SqpDriver;
using hven::solvers::SqpOptions;
using hven::solvers::SqpSolution;
using hven::solvers::SqpStatus;
using hven::solvers::StartLevel;
using hven::solvers::to_sqp_warm_start;
using hven::solvers::WarmExtension;
using hven::solvers::WarmStart;
using hven::solvers::WarmStartData;

namespace {

constexpr double kCurrencyInf = std::numeric_limits<double>::infinity();

// THE FIXTURE PROBLEM, in NlpModel form.
//
//   min  0.5*(x0^2 + x1^2 + x2^2) - 2*x0 - 3*x1
//   s.t. cE:  x0 + x1 + x2 - 1 = 0
//        cI:  x0 + 0.5        <= 0
//        box: (-1, -1, 0) <= x <= (2, 2, 2)
//
// A strictly convex QP with linear constraints, so the solve is short and its
// answer is closed-form: x* = (-0.5, 1.5, 0), lambda_e = 1.5, lambda_i = 1,
// z = (0, 0, 1.5), f* = -2.25. It is shaped for this file's purpose rather
// than borrowed from the HS battery: it has ONE strictly active inequality,
// ONE strictly active variable bound and one free variable, so a crossover
// hand-off through it has a real active set to infer and a real slack row to
// leave alone -- which an HS problem with no bounds (HS7) or no rows could not
// exercise.
//
// `lower_x0_finite` is the ONE structural knob: dropping x0's finite lower
// bound moves the bound digest, and therefore the stamp, WITHOUT moving n, me,
// mi or the solution (x0* = -0.5 is strictly inside [-1, 2] either way). That
// is exactly the shape the solve-entry stamp check exists to catch and the
// size check cannot.
class CurrencyModel : public NlpModel {
  public:
    explicit CurrencyModel(bool lower_x0_finite = true) : lower_(3), upper_(Vec::Constant(3, 2.0)) {
        lower_ << (lower_x0_finite ? -1.0 : -kCurrencyInf), -1.0, 0.0;
    }

    Index n() const override { return 3; }
    Index me() const override { return 1; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override {
        return 0.5 * x.squaredNorm() - 2.0 * x(0) - 3.0 * x(1);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(3);
        g << x(0) - 2.0, x(1) - 3.0, x(2);
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c(0) = x(0) + x(1) + x(2) - 1.0;
        return c;
    }
    Vec eval_ci(const Vec &x) const override {
        Vec c(1);
        c(0) = x(0) + 0.5;
        return c;
    }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(3, 3);
        h.insert(0, 0) = obj_scale;
        h.insert(1, 1) = obj_scale;
        h.insert(2, 2) = obj_scale;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override {
        SpMatRM j(1, 3);
        j.insert(0, 0) = 1.0;
        j.insert(0, 1) = 1.0;
        j.insert(0, 2) = 1.0;
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override {
        SpMatRM j(1, 3);
        j.insert(0, 0) = 1.0;
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override {
        Vec x(3);
        x << 0.0, 0.0, 0.5;
        return x;
    }

  private:
    Vec lower_, upper_;
};

// The same problem widened to four variables: a DIFFERENT DECLARED SIZE, which
// is what the solve-entry block-size check answers before the stamp is even
// reached.
class CurrencyWiderModel : public NlpModel {
  public:
    CurrencyWiderModel() : lower_(Vec::Constant(4, -1.0)), upper_(Vec::Constant(4, 2.0)) {
        lower_(3) = 0.0;
    }

    Index n() const override { return 4; }
    Index me() const override { return 1; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override {
        return 0.5 * x.squaredNorm() - 2.0 * x(0) - 3.0 * x(1);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(4);
        g << x(0) - 2.0, x(1) - 3.0, x(2), x(3);
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c(0) = x.sum() - 1.0;
        return c;
    }
    Vec eval_ci(const Vec &x) const override {
        Vec c(1);
        c(0) = x(0) + 0.5;
        return c;
    }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(4, 4);
        for (int i = 0; i < 4; ++i) {
            h.insert(i, i) = obj_scale;
        }
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override {
        SpMatRM j(1, 4);
        for (int i = 0; i < 4; ++i) {
            j.insert(0, i) = 1.0;
        }
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override {
        SpMatRM j(1, 4);
        j.insert(0, 0) = 1.0;
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Constant(4, 0.25); }

  private:
    Vec lower_, upper_;
};

// The fixture problem with ONE MORE EQUALITY ROW and the same box: a genuinely
// different DECLARED PROBLEM at the same primal width, which is what the
// staleness pin needs. The second row is x1 + x2 = 1.5, chosen only so the
// problem still has a solution; nothing below reads its answer.
class CurrencyExtraRowModel : public NlpModel {
  public:
    CurrencyExtraRowModel() : lower_(3), upper_(Vec::Constant(3, 2.0)) {
        lower_ << -1.0, -1.0, 0.0;
    }

    Index n() const override { return 3; }
    Index me() const override { return 2; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override {
        return 0.5 * x.squaredNorm() - 2.0 * x(0) - 3.0 * x(1);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(3);
        g << x(0) - 2.0, x(1) - 3.0, x(2);
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(2);
        c << x(0) + x(1) + x(2) - 1.0, x(1) + x(2) - 1.5;
        return c;
    }
    Vec eval_ci(const Vec &x) const override {
        Vec c(1);
        c(0) = x(0) + 0.5;
        return c;
    }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(3, 3);
        for (int i = 0; i < 3; ++i) {
            h.insert(i, i) = obj_scale;
        }
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override {
        SpMatRM j(2, 3);
        j.insert(0, 0) = 1.0;
        j.insert(0, 1) = 1.0;
        j.insert(0, 2) = 1.0;
        j.insert(1, 1) = 1.0;
        j.insert(1, 2) = 1.0;
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override {
        SpMatRM j(1, 3);
        j.insert(0, 0) = 1.0;
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override {
        Vec x(3);
        x << 0.0, 0.0, 0.5;
        return x;
    }

  private:
    Vec lower_, upper_;
};

// The fixture problem again, in the triplet-shaped convenience form the
// interior-point engine's front end takes. Row 0 is the equality (gl == gu);
// row 1 is upper-bounded at -0.5, which NlpProblemModel converts to the single
// inequality cI = x0 - (-0.5) = x0 + 0.5 -- the same row CurrencyModel states
// directly, so the two declarations describe one problem.
struct CurrencyIpmProblem : NLPProblem {
    int num_vars() const override { return 3; }
    int num_cons() const override { return 2; }
    int num_jac_nonzeros() const override { return 4; }
    int num_hess_nonzeros() const override { return 3; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -1.0, -1.0, 0.0;
        xu << 2.0, 2.0, 2.0;
        gl << 1.0, -kCurrencyInf;
        gu << 1.0, -0.5;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = 0.5 * x.squaredNorm() - 2.0 * x[0] - 3.0 * x[1];
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] - 2.0;
        g[1] = x[1] - 3.0;
        g[2] = x[2];
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] + x[1] + x[2];
        g[1] = x[0];
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0, 0, 1;
        c << 0, 1, 2, 0;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 2;
        c << 0, 1, 2;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v.setOnes();
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v.setConstant(obj_factor);
    }
    std::string name() const override { return "CurrencyIpmProblem"; }
};

// A bridge over one model, kept alive for as long as the test needs its key.
std::shared_ptr<NlpModelAggregate> make_bridge(std::shared_ptr<const NlpModel> model) {
    return std::make_shared<NlpModelAggregate>(std::move(model));
}

// The solution the fixture converges to, as a warm-start value stamped for
// `bridge`. This is what a caller does with export_warm_start()'s output, so
// the tests below use export_warm_start() itself wherever they can and this
// helper only where a payload has to be BUILT (a corrupt one, a foreign-tagged
// one, a hand-made polish one).
WarmStartData core_payload(const SqpSolution &sol, const NlpModelAggregate &bridge) {
    WarmStartData data;
    data.primal_ = sol.x;
    data.eq_lmults_ = sol.lambda_e;
    data.iq_lmults_ = sol.lambda_i;
    data.bound_lmults_ = sol.z;
    data.structure_key_ = declaration_key(bridge.declaration());
    return data;
}

// The polish hand-off a converged interior-point solve of this fixture would
// carry, built by hand so the crossover tests do not depend on the other
// engine: the invertible (z_lower, z_upper) pair at declared width, the
// inequality VALUES in this project's convention (cI(x) <= 0 at a feasible
// point), and the barrier level. Signed z = z_lower - z_upper by construction,
// so the core and the extension agree.
IpmPolishData fixture_polish(const SqpSolution &sol) {
    IpmPolishData polish;
    polish.mu_ = 1e-8;
    polish.z_lower_ = sol.z.cwiseMax(0.0);
    polish.z_upper_ = (-sol.z).cwiseMax(0.0);
    polish.iq_values_ = Vec::Constant(1, -1e-9);
    return polish;
}

WarmExtension polish_extension(const IpmPolishData &polish) {
    WarmExtension ext;
    ext.tag_ = std::string(kIpmPolishTag);
    ext.payload_ = serialize_ipm_polish(polish);
    return ext;
}

// The R5 INSTRUMENT. `history[0]` is the FIRST measured iterate of a solve --
// evaluate_kkt at the ingested point, before any subproblem is built -- so
// every column in it is a deterministic function of the start state the ingest
// produced. Compared with EXPECT_EQ, not EXPECT_NEAR: R5 asks for bit-identical
// first iterates, and a near-comparison would pass on a start state that had
// drifted.
void expect_same_first_iterate(const SqpSolution &a, const SqpSolution &b) {
    ASSERT_FALSE(a.history.empty());
    ASSERT_FALSE(b.history.empty());
    const auto &ra = a.history.front();
    const auto &rb = b.history.front();
    EXPECT_EQ(ra.f, rb.f);
    EXPECT_EQ(ra.stationarity, rb.stationarity);
    EXPECT_EQ(ra.feasibility, rb.feasibility);
    EXPECT_EQ(ra.complementarity, rb.complementarity);
    EXPECT_EQ(ra.kkt_residual, rb.kkt_residual);
    EXPECT_EQ(ra.violation_l1, rb.violation_l1);
    EXPECT_EQ(ra.tr_radius, rb.tr_radius);
}

// A converged solve of the fixture, from cold, on its own driver.
SqpSolution solve_fixture_cold(const NlpModel &model) {
    SqpDriver driver{SqpOptions{}};
    return driver.solve(model);
}

} // namespace

// --- Export: the refusal, and the round trip ---

// A driver that has not solved cannot export. Never an empty payload: an empty
// payload stages cleanly against anything and then silently cold-starts, which
// is the wrong-but-plausible shape this refusal exists to rule out.
TEST(SqpWarmCurrency, AFreshDriverCannotExport) {
    SqpDriver driver{SqpOptions{}};
    EXPECT_THROW((void)driver.export_warm_start(), std::logic_error);
}

// A solve that THREW is not a completed solve. The throw here is the
// declared-box validation on the model-taking overload, which fires before the
// loop starts -- so nothing was captured and the refusal is unchanged.
TEST(SqpWarmCurrency, ASolveThatThrewIsNotACompletedSolve) {
    class ShortBoxModel : public CurrencyModel {
      public:
        const Vec &lower() const override {
            static const Vec l = Vec::Constant(2, -1.0);
            return l;
        }
    } model;

    SqpDriver driver{SqpOptions{}};
    EXPECT_THROW((void)driver.solve(model), std::invalid_argument);
    EXPECT_THROW((void)driver.export_warm_start(), std::logic_error);
}

// THE OTHER HALF OF THE SAME CONTRACT SENTENCE. "Completed" means a public
// solve() that RETURNED, so a call that throws does not arm the export -- and,
// symmetrically, it does not DISARM one an earlier call armed. A throw leaves
// the last completed solve's payload standing, untouched, because the capture
// is simply never reached.
//
// Worth its own pin rather than left to the implementation: the alternative
// shape -- a throw clearing the export -- is the plausible one (it is what
// stage_warm_start's own clears-first rule does to STAGED state), and a
// consumer that solved, then hit a bad model, then exported would silently get
// a refusal instead of the payload it was entitled to.
TEST(SqpWarmCurrency, ASolveThatThrewLeavesAnEarlierExportStanding) {
    class ShortBoxModel : public CurrencyModel {
      public:
        const Vec &lower() const override {
            static const Vec l = Vec::Constant(2, -1.0);
            return l;
        }
    } bad;

    const auto good = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(good);

    SqpDriver driver{SqpOptions{}};
    const SqpSolution first = driver.solve(*bridge, good->start_point());
    ASSERT_EQ(first.status, SqpStatus::kOptimal);
    const WarmStartData after_first = driver.export_warm_start();

    // A second solve that throws -- here at the declared-box validation, before
    // the loop is ever entered.
    EXPECT_THROW((void)driver.solve(bad), std::invalid_argument);

    // The first solve's payload is still there, and is still the SAME value:
    // not merely exportable, but unmodified.
    const WarmStartData after_throw = driver.export_warm_start();
    EXPECT_EQ(after_throw, after_first);
    EXPECT_EQ(after_throw.primal_, first.x);
    EXPECT_TRUE(after_throw.structure_key_ == declaration_key(bridge->declaration()));
}

// THE ROUND TRIP. The exported blocks are the solution's own vectors -- model
// space IS declared space on this engine, so they are equal bit-for-bit, not
// merely close -- at the model's declared widths, and the stamp is the key of
// the bridge the solve ran against.
TEST(SqpWarmCurrency, ExportCarriesTheSolutionAtDeclaredWidthsAndTheBridgesKey) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);

    SqpDriver driver{SqpOptions{}};
    const SqpSolution sol = driver.solve(*bridge, model->start_point());
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);

    const WarmStartData warm = driver.export_warm_start();

    EXPECT_TRUE(warm.structure_key_ == declaration_key(bridge->declaration()));
    EXPECT_EQ(warm.primal_.size(), model->n());
    EXPECT_EQ(warm.eq_lmults_.size(), model->me());
    EXPECT_EQ(warm.iq_lmults_.size(), model->mi());
    EXPECT_EQ(warm.bound_lmults_.size(), model->n());

    EXPECT_EQ(warm.primal_, sol.x);
    EXPECT_EQ(warm.eq_lmults_, sol.lambda_e);
    EXPECT_EQ(warm.iq_lmults_, sol.lambda_i);
    // z IS the currency's signed z = zL - zU already (warm_start.h's SIGN
    // CONVENTIONS): nothing is converted on the way out. The fixture's only
    // active bound is x2's LOWER one, so the signed block is non-negative there
    // and zero elsewhere -- which is what "z >= 0 at an active lower bound"
    // means concretely.
    EXPECT_EQ(warm.bound_lmults_, sol.z);
    EXPECT_NEAR(warm.bound_lmults_(2), 1.5, 1e-6);
    EXPECT_NEAR(warm.bound_lmults_(0), 0.0, 1e-6);

    // NO EXTENSIONS: the polish tag is the interior-point engine's own, and
    // this engine defines none of its own yet.
    EXPECT_TRUE(warm.extensions_.empty());
}

// The export is the LAST completed solve's, and a later solve replaces it.
TEST(SqpWarmCurrency, ExportTracksTheLastCompletedSolve) {
    const auto narrow = std::make_shared<CurrencyModel>();
    const auto wide = std::make_shared<CurrencyWiderModel>();

    SqpDriver driver{SqpOptions{}};
    ASSERT_EQ(driver.solve(*narrow).status, SqpStatus::kOptimal);
    ASSERT_EQ(driver.export_warm_start().primal_.size(), 3);

    ASSERT_EQ(driver.solve(*wide).status, SqpStatus::kOptimal);
    EXPECT_EQ(driver.export_warm_start().primal_.size(), 4);
}

// --- Staging: what is refused HERE, and what is not ---

TEST(SqpWarmCurrency, StagingRefusesANonFiniteBlock) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpSolution sol = solve_fixture_cold(*model);

    WarmStartData data = core_payload(sol, *bridge);
    data.iq_lmults_(0) = std::numeric_limits<double>::quiet_NaN();

    SqpDriver driver{SqpOptions{}};
    EXPECT_THROW(driver.stage_warm_start(data), std::invalid_argument);

    // Refused, and therefore not staged: the next solve is cold, which the
    // level reading says outright.
    const SqpSolution after = driver.solve(*bridge, model->start_point());
    EXPECT_EQ(after.counters.start_level_used, StartLevel::kCold);
}

// The one structural question answerable with no problem in hand: `primal_` and
// `bound_lmults_` are two readings of one space.
TEST(SqpWarmCurrency, StagingRefusesACoreThatDisagreesWithItself) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpSolution sol = solve_fixture_cold(*model);

    WarmStartData data = core_payload(sol, *bridge);
    data.bound_lmults_ = Vec::Zero(2);

    SqpDriver driver{SqpOptions{}};
    EXPECT_THROW(driver.stage_warm_start(data), std::invalid_argument);
}

// A CORRUPT payload under the KNOWN tag is refused at staging, naming the tag.
// Corruption is not a foreign tag: a reader that skipped it would silently
// cold-start a solve the caller asked to cross over into.
TEST(SqpWarmCurrency, StagingRefusesAMalformedPolishPayloadNamingTheTag) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpSolution sol = solve_fixture_cold(*model);

    WarmStartData data = core_payload(sol, *bridge);
    WarmExtension ext = polish_extension(fixture_polish(sol));
    ext.payload_.resize(ext.payload_.size() - 4); // truncated mid-block
    data.extensions_.push_back(ext);

    SqpDriver driver{SqpOptions{}};
    try {
        driver.stage_warm_start(data);
        FAIL() << "a truncated payload under the known tag must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find(std::string(kIpmPolishTag)), std::string::npos)
            << "the refusal must name the tag it refused under: " << message;
        EXPECT_NE(message.find("stage_warm_start"), std::string::npos) << message;
    }
}

// A FOREIGN tag is skipped silently (R3) -- a capability downgrade, not an
// error. The value stages, applies, and the solve runs as a core-only warm one.
TEST(SqpWarmCurrency, AForeignExtensionTagIsSkippedSilently) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpSolution sol = solve_fixture_cold(*model);

    WarmStartData data = core_payload(sol, *bridge);
    WarmExtension ext;
    ext.tag_ = "some.other.engine.v3";
    ext.payload_ = {std::byte{0xde}, std::byte{0xad}};
    data.extensions_.push_back(ext);

    SqpDriver driver{SqpOptions{}};
    ASSERT_NO_THROW(driver.stage_warm_start(data));
    const SqpSolution out = driver.solve(*bridge, model->start_point());
    EXPECT_EQ(out.status, SqpStatus::kOptimal);
    EXPECT_EQ(out.counters.start_level_used, StartLevel::kSeeded);
}

// CLEARS FIRST, whether it succeeds or refuses. A caller that stages a good
// value, then stages a bad one and solves anyway, must cold-start -- never
// warm-start off the value it has already moved on from.
TEST(SqpWarmCurrency, ARefusedStagingClearsTheValueStagedBeforeIt) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpSolution sol = solve_fixture_cold(*model);

    SqpDriver driver{SqpOptions{}};
    ASSERT_NO_THROW(driver.stage_warm_start(core_payload(sol, *bridge)));

    WarmStartData bad = core_payload(sol, *bridge);
    bad.primal_(0) = std::numeric_limits<double>::infinity();
    EXPECT_THROW(driver.stage_warm_start(bad), std::invalid_argument);

    const SqpSolution out = driver.solve(*bridge, model->start_point());
    EXPECT_EQ(out.counters.start_level_used, StartLevel::kCold)
        << "the refused staging must have cleared the good value staged before it";
}

// --- Solve entry: the two checks that had nowhere to stand at staging ---

TEST(SqpWarmCurrency, SolveEntryRefusesAStagedValueAtTheWrongSizes) {
    const auto narrow = std::make_shared<CurrencyModel>();
    const auto narrow_bridge = make_bridge(narrow);
    const auto wide = std::make_shared<CurrencyWiderModel>();
    const auto wide_bridge = make_bridge(wide);
    const SqpSolution sol = solve_fixture_cold(*narrow);

    SqpDriver driver{SqpOptions{}};
    // Staging is where the value is accepted: no problem is bound here, so
    // nothing about the destination's width is knowable yet.
    ASSERT_NO_THROW(driver.stage_warm_start(core_payload(sol, *narrow_bridge)));

    try {
        (void)driver.solve(*wide_bridge, wide->start_point());
        FAIL() << "a 3-wide payload must be refused against a 4-variable problem";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("primal_"), std::string::npos) << message;
        EXPECT_NE(message.find("3"), std::string::npos) << message;
        EXPECT_NE(message.find("4"), std::string::npos) << message;
    }

    // CONSUMED BY THE REFUSAL -- loud, then gone. A caller that logs it and
    // solves anyway cold-starts rather than warm-starting off a value this
    // engine has just rejected.
    const SqpSolution after = driver.solve(*wide_bridge, wide->start_point());
    EXPECT_EQ(after.counters.start_level_used, StartLevel::kCold);
}

// SAME SIZES, DIFFERENT DECLARED PROBLEM. The declared bound STRUCTURE moved --
// one variable's lower side went from finite to infinite -- which is a
// DECLARATION change carried by the stamp's bound conjunct alone. Note it is
// the STRUCTURE, not a value: moving a finite bound to another finite value
// would NOT re-key (that is the continuation flow), which is why this fixture
// changes finiteness rather than a number. The size check cannot see it: only
// the stamp can, and only at solve entry, against the problem that call binds.
// This is the pin for the refusal naming BOTH key digests.
TEST(SqpWarmCurrency, SolveEntryRefusesAStagedValueUnderAnotherStampNamingBothDigests) {
    const auto original = std::make_shared<CurrencyModel>(/*lower_x0_finite=*/true);
    const auto original_bridge = make_bridge(original);
    const auto rekeyed = std::make_shared<CurrencyModel>(/*lower_x0_finite=*/false);
    const auto rekeyed_bridge = make_bridge(rekeyed);

    // The two problems are the same shape and have the same solution; only the
    // BOUND STRUCTURE differs, which is one of the two conjuncts of the stamp --
    // and the declaration conjunct is IDENTICAL, so the bound one is carrying
    // the whole refusal.
    ASSERT_EQ(original->n(), rekeyed->n());
    ASSERT_FALSE(declaration_key(original_bridge->declaration()) ==
                 declaration_key(rekeyed_bridge->declaration()));
    EXPECT_EQ(declaration_key(original_bridge->declaration()).declaration_digest_,
              declaration_key(rekeyed_bridge->declaration()).declaration_digest_);
    EXPECT_NE(declaration_key(original_bridge->declaration()).bound_digest_,
              declaration_key(rekeyed_bridge->declaration()).bound_digest_);

    const SqpSolution sol = solve_fixture_cold(*original);
    SqpDriver driver{SqpOptions{}};
    ASSERT_NO_THROW(driver.stage_warm_start(core_payload(sol, *original_bridge)));

    try {
        (void)driver.solve(*rekeyed_bridge, rekeyed->start_point());
        FAIL() << "a payload under another declared structure must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find(fmt::format(
                      "{:#x}", declaration_key(original_bridge->declaration()).digest())),
                  std::string::npos)
            << "the refusal must name the payload's own key: " << message;
        EXPECT_NE(message.find(fmt::format(
                      "{:#x}", declaration_key(rekeyed_bridge->declaration()).digest())),
                  std::string::npos)
            << "the refusal must name the live key: " << message;
    }

    const SqpSolution after = driver.solve(*rekeyed_bridge, rekeyed->start_point());
    EXPECT_EQ(after.counters.start_level_used, StartLevel::kCold);
}

// --- One-shot consumption, and the two-sources refusal ---

TEST(SqpWarmCurrency, AStagedValueIsConsumedByTheNextSolveAndTheOneAfterIsCold) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpSolution sol = solve_fixture_cold(*model);

    SqpDriver driver{SqpOptions{}};
    driver.stage_warm_start(core_payload(sol, *bridge));

    const SqpSolution first = driver.solve(*bridge, model->start_point());
    EXPECT_EQ(first.status, SqpStatus::kOptimal);
    EXPECT_EQ(first.counters.start_level_used, StartLevel::kSeeded);
    EXPECT_EQ(first.counters.n_seeded, 1);

    const SqpSolution second = driver.solve(*bridge, model->start_point());
    EXPECT_EQ(second.counters.start_level_used, StartLevel::kCold)
        << "the staged value is ONE-SHOT: the second solve gets nothing";
    EXPECT_EQ(second.counters.n_seeded, 0);
}

// TWO WARM-START SOURCES FOR ONE SOLVE, refused naming both -- no silent
// precedence. The refusal deliberately does NOT consume: it judges the call's
// arguments, not the value, and the call binds no problem and runs nothing.
TEST(SqpWarmCurrency, AnExplicitWarmArgumentAndAStagedValueAreRefusedTogether) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpSolution sol = solve_fixture_cold(*model);

    SqpDriver driver{SqpOptions{}};
    driver.stage_warm_start(core_payload(sol, *bridge));

    try {
        (void)driver.solve(*bridge, model->start_point(), sol.warm_start);
        FAIL() << "an explicit warm argument must not silently outrank a staged value";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("WarmStart argument"), std::string::npos) << message;
        EXPECT_NE(message.find("stage_warm_start"), std::string::npos) << message;
    }

    // Even a DEFAULT-CONSTRUCTED (cold) argument is a source: it is this
    // class's documented way of asking for a cold solve, so it contradicts a
    // staged value just as loudly.
    EXPECT_THROW((void)driver.solve(*bridge, model->start_point(), WarmStart{}),
                 std::invalid_argument);

    // AND THE REFUSAL SAYS SO. A caller must not have to infer from silence
    // whether a ONE-SHOT value survived a refusal -- this is the one refusal on
    // the surface that retains it, so the message states it in words (settler
    // ruling, 2026-08-25).
    try {
        (void)driver.solve(*bridge, model->start_point(), WarmStart{});
        FAIL() << "the two-sources refusal must fire";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("RETAINED"), std::string::npos)
            << "the refusal must SAY the staged value survives: " << message;
        EXPECT_NE(message.find("still staged"), std::string::npos) << message;
        EXPECT_NE(message.find("consumed nothing"), std::string::npos) << message;
    }

    // AND THE VALUE IS STILL STAGED, which is what the message just promised.
    // The caller's fix is to drop one source and call again; nothing was judged
    // and nothing was spent.
    const SqpSolution out = driver.solve(*bridge, model->start_point());
    EXPECT_EQ(out.counters.start_level_used, StartLevel::kSeeded);
}

// --- The two application routes, and the level they enter at ---

// CORE-ONLY: accepted (R3) and entering at StartLevel::kSeeded -- the level the
// currency's core can justify, since the kWarm/kHot gate is a fingerprint of
// assembled matrices the value does not carry.
TEST(SqpWarmCurrency, ACoreOnlyValueEntersAtTheSeededLevel) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpSolution sol = solve_fixture_cold(*model);

    SqpDriver driver{SqpOptions{}};
    driver.stage_warm_start(core_payload(sol, *bridge));

    // x0 is deliberately NOWHERE NEAR the solution: if the staged primal did not
    // replace it, the first iterate could not be the solved point.
    const SqpSolution out = driver.solve(*bridge, Vec::Constant(3, 1.75));

    EXPECT_EQ(out.status, SqpStatus::kOptimal);
    EXPECT_EQ(out.counters.start_level_used, StartLevel::kSeeded);
    // THE STAGED PRIMAL REPLACED x0, through the ingest rule this class already
    // documents ("x0 is the cold fallback"): the first measured iterate is the
    // exported point, not the argument.
    ASSERT_FALSE(out.history.empty());
    EXPECT_NEAR(out.history.front().f, sol.f, 1e-12);
    // Warm from a KKT point of the same problem: certified without spending a
    // major. A margin, not an exact count -- what is asserted is that the
    // ingest reached the convergence test, not a particular trajectory.
    EXPECT_LE(out.counters.major_iters, sol.counters.major_iters);
}

// WITH THE POLISH TAG: the value is routed through to_sqp_warm_start and
// nothing is re-implemented. Pinned by EQUIVALENCE -- staging the payload and
// solving must produce the same solve as calling the bridge by hand and passing
// its output as an explicit argument.
TEST(SqpWarmCurrency, APolishTaggedValueRoutesThroughTheCrossoverBridge) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpSolution sol = solve_fixture_cold(*model);

    WarmStartData data = core_payload(sol, *bridge);
    const IpmPolishData polish = fixture_polish(sol);
    data.extensions_.push_back(polish_extension(polish));

    // (a) through the staged path.
    SqpDriver staged_driver{SqpOptions{}};
    staged_driver.stage_warm_start(data);
    const SqpSolution staged = staged_driver.solve(*bridge, model->start_point());

    // (b) through the bridge called by hand, on a fresh driver.
    const WarmStart crossover = to_sqp_warm_start(data, model->lower(), model->upper(),
                                                  declaration_key(bridge->declaration()));
    SqpDriver explicit_driver{SqpOptions{}};
    const SqpSolution direct = explicit_driver.solve(*bridge, model->start_point(), crossover);

    // FIELD-FOR-FIELD on the solution, at the resolution the instrument
    // supports: same status, same point, same objective, same level, same
    // trajectory length, and a bit-identical first iterate.
    EXPECT_EQ(staged.status, direct.status);
    EXPECT_EQ(staged.x, direct.x);
    EXPECT_EQ(staged.lambda_e, direct.lambda_e);
    EXPECT_EQ(staged.lambda_i, direct.lambda_i);
    EXPECT_EQ(staged.z, direct.z);
    EXPECT_EQ(staged.f, direct.f);
    EXPECT_EQ(staged.counters.major_iters, direct.counters.major_iters);
    EXPECT_EQ(staged.counters.start_level_used, direct.counters.start_level_used);
    EXPECT_EQ(staged.history.size(), direct.history.size());
    expect_same_first_iterate(staged, direct);

    // And the level is kSeeded on both: to_sqp_warm_start's object carries
    // structure_hash == 0 by construction, which is the hash gate kWarm needs.
    EXPECT_EQ(staged.counters.start_level_used, StartLevel::kSeeded);
}

// --- R5: determinism ---

// The same staged payload, from cold, twice: bit-identical first iterates.
// Instrument: SqpSolution::history[0], the first MEASURED iterate -- every
// column of it is a function of the start state the ingest produced, so a drift
// anywhere in the ingest shows up here.
TEST(SqpWarmCurrency, StagingTheSameValueTwiceFromColdIsBitIdentical) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpSolution sol = solve_fixture_cold(*model);
    const WarmStartData data = core_payload(sol, *bridge);

    SqpDriver first_driver{SqpOptions{}};
    first_driver.stage_warm_start(data);
    const SqpSolution first = first_driver.solve(*bridge, model->start_point());

    SqpDriver second_driver{SqpOptions{}};
    second_driver.stage_warm_start(data);
    const SqpSolution second = second_driver.solve(*bridge, model->start_point());

    expect_same_first_iterate(first, second);
    EXPECT_EQ(first.x, second.x);
    EXPECT_EQ(first.f, second.f);
    EXPECT_EQ(first.counters.major_iters, second.counters.major_iters);
    EXPECT_EQ(first.counters.start_level_used, second.counters.start_level_used);

    // NON-CONSUMING (R5): the argument was taken by const reference, so the
    // caller's own value is untouched and stageable again.
    EXPECT_EQ(data.primal_, sol.x);
}

// --- The interior-point -> SQP composition ---

// THE CROSSOVER AS ONE COMPOSITION: an interior-point solve, its exported
// value, and an SQP solve that finishes from it -- no re-stamp, no conversion,
// nothing in between.
//
// WHAT THIS TEST WATCHED FAIL, and what fixed it. Before the 2026-08-25
// declaration-identity ruling the currency stamped `ModelStructureKey`, a
// digest of the claim stream a provider actually laid. The two engines lay the
// same (row, column) claim SET for one declared model in a DIFFERENT ORDER --
// the interior-point program lays the constraint Jacobian's claims before the
// Hessian's, NlpModelAggregate lays the Hessian's first -- and that digest is
// order-sensitive by design, so the cross-engine stamp disagreed on EVERY
// problem and this hand-off was refused unconditionally. The stamp is now the
// DECLARATION key, which reads the declared problem and nothing a provider
// decided. The first two assertions below are that fact stated directly: the
// LAYOUT keys still differ (the claim orders are still different, and that is
// still correct for the question that key answers), and the STAMPS agree.
TEST(SqpWarmCurrency, InteriorPointExportCrossesOverIntoTheSqpEngine) {
    const auto problem = std::make_shared<CurrencyIpmProblem>();

    NLPSolver ipm(problem);
    ipm.optimizer_->set_print_level(10);
    ipm.transcribe();
    Eigen::VectorXd x0(3);
    x0 << 0.0, 0.0, 0.5;
    ipm.optimizer_->optimize(x0);
    ASSERT_EQ(ipm.optimizer_->result().converge_flag_, hven::ConvergenceFlags::CONVERGED);

    const WarmStartData exported = ipm.optimizer_->export_warm_start();
    ASSERT_EQ(exported.primal_.size(), 3);
    ASSERT_EQ(exported.eq_lmults_.size(), 1);
    ASSERT_EQ(exported.iq_lmults_.size(), 1);
    // The problem HAS finite variable bounds, so the hand-off carries the
    // interior-point polish extension beside its core.
    ASSERT_EQ(exported.extensions_.size(), 1u);
    ASSERT_EQ(exported.extensions_[0].tag_, std::string(kIpmPolishTag));
    // It really did solve the fixture problem.
    EXPECT_NEAR(exported.primal_(0), -0.5, 1e-5);
    EXPECT_NEAR(exported.primal_(1), 1.5, 1e-5);
    EXPECT_NEAR(exported.primal_(2), 0.0, 1e-5);

    // The SAME problem, declared to the SQP engine through the same conversion
    // the interior-point front end uses.
    const auto model = std::make_shared<NlpProblemModel>(problem);
    const auto bridge = make_bridge(model);

    // THE TWO KEYS, side by side. The layout keys differ -- the claim orders
    // are genuinely different -- and the stamps do not.
    EXPECT_NE(ipm.nlp_->model_structure_key().claim_digest_,
              bridge->model_structure_key().claim_digest_)
        << "the two engines really do lay this declaration's claims differently; "
           "if they ever stop, this pin is the note that says the layout keys "
           "converged, not that anything broke";
    EXPECT_TRUE(declaration_key(ipm.nlp_->declaration()) == declaration_key(bridge->declaration()))
        << "one declared problem must key the same on both engines -- that is "
           "the whole content of the declaration-identity ruling";
    EXPECT_TRUE(exported.structure_key_ == declaration_key(bridge->declaration()))
        << "and the exported value carries that key, so it stages here as-is";

    // THE COMPOSITION, with the value exactly as the other engine handed it
    // over.
    SqpDriver sqp{SqpOptions{}};
    sqp.stage_warm_start(exported);
    const SqpSolution out = sqp.solve(*bridge, model->start_point());

    EXPECT_EQ(out.status, SqpStatus::kOptimal);
    EXPECT_EQ(out.counters.start_level_used, StartLevel::kSeeded);
    // At the driver's own default kkt_tol, not tighter: the SQP engine
    // certifies the polished point rather than re-solving to machine
    // precision, which is the whole point of a crossover.
    EXPECT_NEAR(out.x(0), -0.5, 1e-6);
    EXPECT_NEAR(out.x(1), 1.5, 1e-6);
    EXPECT_NEAR(out.x(2), 0.0, 1e-6);
    // MARGIN FORM (CLAUDE.md section 7): the hand-off is judged by the work it
    // saves, not by an exact trajectory. A cold solve of the same problem costs
    // strictly more majors than the polished one.
    const SqpSolution cold = solve_fixture_cold(*model);
    ASSERT_EQ(cold.status, SqpStatus::kOptimal);
    EXPECT_LT(out.counters.major_iters, cold.counters.major_iters)
        << "the crossover must save majors against a cold solve of the same problem";

    // And the SQP engine can hand the result straight back, under the same
    // stamp -- so the composition closes rather than ending in a value only one
    // side can read.
    const WarmStartData reexported = sqp.export_warm_start();
    EXPECT_TRUE(reexported.structure_key_ == exported.structure_key_);
    EXPECT_EQ(reexported.primal_, out.x);
}

// GENUINE STALENESS: the caller transcribed a DIFFERENT PROBLEM -- one more
// equality row -- and the value taken on the old one is refused.
//
// A row change moves a BLOCK LENGTH as well as the key, so the size check is
// what fires (it runs first, deliberately, because "eq_lmults_ holds 1, this
// problem declares 2" is the more actionable of the two diagnostics when both
// are true). The keys are asserted apart directly, right here, so the pin says
// what it means: this is a declaration change, and the stamp sees it. The
// SAME-SIZE case -- where the stamp is the ONLY thing that can catch the change
// -- is the bound-structure test further up, which is the one that names both
// digests.
TEST(SqpWarmCurrency, ADeclarationWithAnExtraRowIsStaleAndIsRefused) {
    const auto original = std::make_shared<CurrencyModel>();
    const auto original_bridge = make_bridge(original);
    const auto extra = std::make_shared<CurrencyExtraRowModel>();
    const auto extra_bridge = make_bridge(extra);

    // The declaration really did change, and the stamp really does see it.
    EXPECT_FALSE(declaration_key(original_bridge->declaration()) ==
                 declaration_key(extra_bridge->declaration()));
    EXPECT_NE(declaration_key(original_bridge->declaration()).declaration_digest_,
              declaration_key(extra_bridge->declaration()).declaration_digest_);
    // The BOX is untouched, so the bound conjunct is the same on both: the row
    // change is carried entirely by the declaration conjunct, which is what
    // that conjunct is for.
    EXPECT_EQ(declaration_key(original_bridge->declaration()).bound_digest_,
              declaration_key(extra_bridge->declaration()).bound_digest_);

    const SqpSolution sol = solve_fixture_cold(*original);
    SqpDriver driver{SqpOptions{}};
    ASSERT_NO_THROW(driver.stage_warm_start(core_payload(sol, *original_bridge)));

    try {
        (void)driver.solve(*extra_bridge, extra->start_point());
        FAIL() << "a value taken on a one-row-smaller declaration must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("eq_lmults_"), std::string::npos) << message;
    }

    // Consumed by the refusal, like every other solve-entry refusal.
    const SqpSolution after = driver.solve(*extra_bridge, extra->start_point());
    EXPECT_EQ(after.counters.start_level_used, StartLevel::kCold);
}

// --- R6: cold-vs-hot is never answer-observable ---

namespace {

// Every ANSWER an SqpSolution reports -- the point, its prices, the objective,
// the terminal KKT measurement and the verdict -- compared BITWISE. R6 asks for
// exact identity, not a neighbourhood of it: a margin here would be a claim
// that reuse perturbs the answer slightly, which is the one thing R6 forbids.
//
// Wall time is deliberately absent: it may differ freely (skipping work is the
// point of reuse) and it is informational, never asserted.
void expect_same_answer(const SqpSolution &hot, const SqpSolution &cold) {
    EXPECT_EQ(hot.status, cold.status);
    EXPECT_EQ(hot.infeasibility_certified, cold.infeasibility_certified);
    EXPECT_EQ(hot.f, cold.f);
    EXPECT_EQ(hot.stationarity, cold.stationarity);
    EXPECT_EQ(hot.feasibility, cold.feasibility);
    EXPECT_EQ(hot.complementarity, cold.complementarity);
    EXPECT_EQ(hot.kkt_residual, cold.kkt_residual);

    ASSERT_EQ(hot.x.size(), cold.x.size());
    for (Index i = 0; i < hot.x.size(); ++i) {
        EXPECT_EQ(hot.x(i), cold.x(i)) << "primal " << i;
        EXPECT_EQ(hot.z(i), cold.z(i)) << "bound multiplier " << i;
    }
    ASSERT_EQ(hot.lambda_e.size(), cold.lambda_e.size());
    for (Index i = 0; i < hot.lambda_e.size(); ++i) {
        EXPECT_EQ(hot.lambda_e(i), cold.lambda_e(i)) << "equality multiplier " << i;
    }
    ASSERT_EQ(hot.lambda_i.size(), cold.lambda_i.size());
    for (Index i = 0; i < hot.lambda_i.size(); ++i) {
        EXPECT_EQ(hot.lambda_i(i), cold.lambda_i(i)) << "inequality multiplier " << i;
    }

    // THE PATH, not only the endpoint: the two solves visited the same
    // iterates, in the same order, measuring the same residuals at each. A
    // divergence anywhere in the loop that the endpoint happened to absorb
    // still fails here.
    ASSERT_EQ(hot.history.size(), cold.history.size());
    for (std::size_t k = 0; k < hot.history.size(); ++k) {
        const auto &a = hot.history[k];
        const auto &b = cold.history[k];
        EXPECT_EQ(a.f, b.f) << "row " << k;
        EXPECT_EQ(a.stationarity, b.stationarity) << "row " << k;
        EXPECT_EQ(a.feasibility, b.feasibility) << "row " << k;
        EXPECT_EQ(a.complementarity, b.complementarity) << "row " << k;
        EXPECT_EQ(a.kkt_residual, b.kkt_residual) << "row " << k;
        EXPECT_EQ(a.violation_l1, b.violation_l1) << "row " << k;
        EXPECT_EQ(a.tr_radius, b.tr_radius) << "row " << k;
        EXPECT_EQ(a.step_norm, b.step_norm) << "row " << k;
        EXPECT_EQ(a.verdict, b.verdict) << "row " << k;
        EXPECT_EQ(a.qp_solved, b.qp_solved) << "row " << k;
        EXPECT_EQ(a.qp_status, b.qp_status) << "row " << k;
        EXPECT_EQ(a.qp_minor_iters, b.qp_minor_iters) << "row " << k;
        EXPECT_EQ(a.qp_factorizations, b.qp_factorizations) << "row " << k;
    }
}

// The COUNTERS, which R6 covers alongside the values: work spent is part of
// what a consumer reports, and a reuse that changed the iteration path would
// show up here first.
void expect_same_counters(const SqpCounters &hot, const SqpCounters &cold) {
    EXPECT_EQ(hot.major_iters, cold.major_iters);
    EXPECT_EQ(hot.qp_minor_iters, cold.qp_minor_iters);
    EXPECT_EQ(hot.factorizations, cold.factorizations);
    EXPECT_EQ(hot.steps_accepted, cold.steps_accepted);
    EXPECT_EQ(hot.rejected_steps, cold.rejected_steps);
    EXPECT_EQ(hot.soc_steps, cold.soc_steps);
    EXPECT_EQ(hot.soc_applied, cold.soc_applied);
    EXPECT_EQ(hot.elastic_activations, cold.elastic_activations);
    EXPECT_EQ(hot.elastic_escalations, cold.elastic_escalations);
    EXPECT_EQ(hot.restoration_iters, cold.restoration_iters);
    EXPECT_EQ(hot.suspect_escalations, cold.suspect_escalations);
    EXPECT_EQ(hot.symbolic_analyses, cold.symbolic_analyses);
    EXPECT_EQ(hot.start_level_used, cold.start_level_used);
    EXPECT_EQ(hot.full_step_majors, cold.full_step_majors);
    EXPECT_EQ(hot.watchdog_restores, cold.watchdog_restores);
    EXPECT_EQ(hot.border_refine_steps, cold.border_refine_steps);
    EXPECT_EQ(hot.eqp_refine_steps, cold.eqp_refine_steps);
}

} // namespace

// M5 R6: HOT-STATE REUSE IS NEVER OBSERVABLE IN ANSWERS -- the SQP side.
//
// SqpDriver carries no cross-solve NUMERIC state of its own; the one piece of
// cross-solve state in the picture is its QpEngine's border cache, and that
// cache is consulted through a gate whose conjuncts include the K0 STRUCTURAL
// hash, the K0 VALUES hash, the effective (primal_delta, dual_mu) pair, the
// incoming working set and the factor's own live identity (qp_engine.h's
// HOT-START REUSE note, conditions (a)-(e)). A grant therefore means the
// matrix that WOULD have been assembled and factorized is bit-for-bit the one
// already in hand, and the fast path skips only that assembly and
// factorization -- never sync_borders(), which reconciles unconditionally on
// every iteration of every solve.
//
// This is that argument turned into a measurement: two solves on ONE driver,
// against a fresh driver's single solve of the same problem from the same
// start, over the same bridge -- so the driver's own carried state is the only
// thing that differs between them.
//
// EXACT EQUALITY, no margins. Wall time is exempt and is not read here.
TEST(SqpWarmCurrency, ASecondSolveOnOneDriverAnswersExactlyWhatAFreshDriverAnswers) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const Vec x0 = model->start_point();

    SqpDriver reused{SqpOptions{}};
    const SqpSolution first = reused.solve(*bridge, x0);
    ASSERT_EQ(first.status, SqpStatus::kOptimal);
    // THE HOT SOLVE. Nothing was staged between the two calls, so this call
    // differs from the first only in what the driver and its engine carried
    // out of it.
    const SqpSolution second = reused.solve(*bridge, x0);
    ASSERT_EQ(second.status, SqpStatus::kOptimal);

    SqpDriver fresh{SqpOptions{}};
    const SqpSolution cold = fresh.solve(*bridge, x0);
    ASSERT_EQ(cold.status, SqpStatus::kOptimal);

    expect_same_answer(second, cold);
    expect_same_counters(second.counters, cold.counters);
    // The first solve is the control: a driver's FIRST solve is a cold one, so
    // all three of these agree, and the second-vs-first comparison is what
    // shows the carried state changed nothing on this instance either.
    expect_same_answer(second, first);
    expect_same_counters(second.counters, first.counters);
}

// THE SAME QUESTION WITH THE SECOND SOLVE WARM-STARTED. The test above
// re-solves from the same cold start; this one stages the exported solution
// into BOTH sides first, so the warm ingest is held constant and the engine's
// carried cache is again the only difference between them.
//
// NEITHER TEST CLAIMS A GRANTED REUSE, and that is stated rather than glossed:
// measured on this fixture, the reused driver pays exactly the factorizations
// and symbolic analyses the fresh one pays, so the gate above was not passed
// on either. What these two pin is that whatever the driver carried out of an
// earlier solve changed nothing -- which is the question the brief asks of a
// re-solve. The GRANTED case, where the fast path really does skip a
// factorization and the answer is still bit-identical, is pinned separately by
// tests/sqp/test_warm_start.cpp's WarmStart.HotReuseIsNeverAnswerObservable.
TEST(SqpWarmCurrency, AWarmResolveOnAUsedDriverAnswersExactlyWhatAFreshDriverAnswers) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const Vec x0 = model->start_point();

    SqpDriver reused{SqpOptions{}};
    const SqpSolution first = reused.solve(*bridge, x0);
    ASSERT_EQ(first.status, SqpStatus::kOptimal);
    const WarmStartData payload = reused.export_warm_start();

    reused.stage_warm_start(payload);
    const SqpSolution hot = reused.solve(*bridge, x0);
    ASSERT_EQ(hot.status, SqpStatus::kOptimal);

    SqpDriver fresh{SqpOptions{}};
    fresh.stage_warm_start(payload);
    const SqpSolution cold = fresh.solve(*bridge, x0);
    ASSERT_EQ(cold.status, SqpStatus::kOptimal);

    // Both staged the same value, so both resolve at the same level -- and
    // that level is kSeeded, never kWarm or kHot: a currency-borne value
    // carries no structure hash, which is what those two levels are gated on.
    // Stated here as well as in the level pins above because R6's other half
    // is that a reuse form which cannot argue answer-neutrality is not
    // SILENTLY enabled, and a level silently climbing to kHot on the reused
    // driver would be exactly that.
    EXPECT_EQ(hot.counters.start_level_used, StartLevel::kSeeded);
    EXPECT_EQ(cold.counters.start_level_used, StartLevel::kSeeded);

    expect_same_answer(hot, cold);
    expect_same_counters(hot.counters, cold.counters);
}
