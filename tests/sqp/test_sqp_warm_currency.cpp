// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_sqp_warm_currency.cpp — SqpSolver::export_warm_start and the
// PAYLOAD route `solve(bridge, x0, const WarmStartData &, budget)`, the SQP
// engine's half of the warm-start currency.
//
// M6 W5 T8.5 REPLACED STAGING WITH AN ARGUMENT. `stage_warm_start(p);
// solve(b, x0)` is `solve(b, x0, p)`; the hand-over checks that ran at the
// staging call run at the public entry, with the same messages under the
// `SqpSolver::solve` name; and the against-the-problem checks (block lengths,
// then the declaration stamp) still fire at solve entry -- now IN EVERY
// `qp_mode`, kIpm included (M5 ruling 4's mode-local cold grade RETIRED for
// that case). Two families of test went with the staging state they were about
// -- the clear-first rule and the two-warm-sources refusal -- each retired in
// place with the argument that the hazard it guarded is unconstructible now,
// and each replaced by the positive statement of the new shape.
//
// Pinned here, in the order the surface is used:
//   * the no-completed-solve export refusal, and the round trip that follows a
//     real solve (blocks == the solution's, widths == the model's, stamp == the
//     bridge's key);
//   * the hand-over: finiteness and internal consistency refused THERE; block
//     sizes and the stamp refused at SOLVE ENTRY, against the problem that call
//     binds, in every qp_mode; a payload applying to its own call and no other
//     (the next solve is cold);
//   * overload selection by argument type -- an lvalue `SqpWarmStart` takes the
//     native route, an lvalue `WarmStartData` the payload one;
//   * the two application routes -- the "hven.ipm.polish.v1" crossover through
//     to_sqp_warm_start, and the core-only seed -- both entering at
//     StartLevel::kSeeded, and a foreign tag skipped silently;
//   * R5 determinism, instrumented on `SqpResult::history[0]` (the first
//     iterate's measured row, a bit-exact function of the ingested start state);
//   * the interior-point -> SQP composition end to end, natively: one declared
//     problem, one exported value, no conversion and no re-stamp anywhere, and
//     the declaration change that still refuses.
//
// Names carry a `Currency` prefix: this suite's TUs share a link unit, so
// file-scope names must not collide with the other files here.

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>
#include <gtest/gtest.h>

#include <hven/detail/model/nlp_adapter.h>
#include <hven/detail/warmstart/warm_start.h>
#include <hven/drivers/ipm_solver.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>
#include <hven/model/nlp_model.h>
#include <hven/model/nlp_model_assembly.h>
#include <hven/model/nlp_problem_model.h>
#include <hven/warmstart/ipm_polish_extension.h>
#include <hven/warmstart/warm_start_data.h>

using hven::ConstEigenRef;
using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using hven::solvers::declaration_key;
using hven::solvers::IpmPolishData;
using hven::solvers::kIpmPolishTag;
using hven::solvers::NlpModel;
using hven::solvers::NlpModelAssembly;
using hven::solvers::NlpProblemModel;
using hven::solvers::NlpTripletModel;
using hven::solvers::QpMode;
using hven::solvers::serialize_ipm_polish;
using hven::solvers::SolveStatus;
using hven::solvers::SqpCounters;
using hven::solvers::SqpOptions;
using hven::solvers::SqpResult;
using hven::solvers::SqpSolver;
using hven::solvers::SqpWarmStart;
using hven::solvers::StartLevel;
using hven::solvers::to_sqp_warm_start;
using hven::solvers::WarmExtension;
using hven::solvers::WarmStartData;

namespace {

constexpr double kCurrencyInf = std::numeric_limits<double>::infinity();

// The fixture problem, in NlpModel form.
//
//   min  0.5*(x0^2 + x1^2 + x2^2) - 2*x0 - 3*x1
//   s.t. cE:  x0 + x1 + x2 - 1 = 0
//        cI:  x0 + 0.5        <= 0
//        box: (-1, -1, 0) <= x <= (2, 2, 2)
//
// A strictly convex QP with linear constraints, so the solve is short and its
// answer is closed-form: x* = (-0.5, 1.5, 0), lambda_e = 1.5, lambda_i = 1,
// z = (0, 0, 1.5), f* = -2.25. Shaped for this file's purpose rather than
// borrowed from the HS battery: one strictly active inequality, one strictly
// active variable bound and one free variable, so a crossover hand-off through
// it has a real active set to infer and a real slack row to leave alone.
//
// `lower_x0_finite` is the one structural knob: dropping x0's finite lower
// bound moves the bound digest, and therefore the stamp, without moving n, me,
// mi or the solution (x0* = -0.5 is strictly inside [-1, 2] either way) -- the
// shape the solve-entry stamp check exists to catch and the size check cannot.
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

// The same problem widened to four variables: a different declared size, which
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

// The fixture problem with one more equality row and the same box: a genuinely
// different declared problem, which is what the staleness pin needs. The second
// row is x1 + x2 = 1.5, chosen only so the problem still has a solution;
// nothing below reads its answer.
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
struct CurrencyIpmProblem : NlpTripletModel {
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
std::shared_ptr<NlpModelAssembly> make_bridge(std::shared_ptr<const NlpModel> model) {
    return std::make_shared<NlpModelAssembly>(std::move(model));
}

// The solution the fixture converges to, as a warm-start value stamped for
// `bridge`. This is what a caller does with export_warm_start()'s output, so
// the tests below use export_warm_start() itself wherever they can and this
// helper only where a payload has to be BUILT (a corrupt one, a foreign-tagged
// one, a hand-made polish one).
WarmStartData core_payload(const SqpResult &sol, const NlpModelAssembly &bridge) {
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
IpmPolishData fixture_polish(const SqpResult &sol) {
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

// The R5 instrument. `history[0]` is the first measured iterate of a solve --
// evaluate_kkt at the ingested point, before any subproblem is built -- so every
// column in it is a deterministic function of the start state the ingest
// produced. Compared with EXPECT_EQ, not EXPECT_NEAR: R5 asks for bit-identical
// first iterates, and a near-comparison would pass on a start state that had
// drifted.
void expect_same_first_iterate(const SqpResult &a, const SqpResult &b) {
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
SqpResult solve_fixture_cold(const NlpModel &model) {
    SqpSolver driver{SqpOptions{}};
    return driver.solve(model);
}

} // namespace

// --- Export: the refusal, and the round trip ---

// A driver that has not solved cannot export. Never an empty payload: an empty
// payload stages cleanly against anything and then silently cold-starts, which
// is the wrong-but-plausible shape this refusal exists to rule out.
TEST(SqpWarmCurrency, AFreshDriverCannotExport) {
    SqpSolver driver{SqpOptions{}};
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

    SqpSolver driver{SqpOptions{}};
    EXPECT_THROW((void)driver.solve(model), std::invalid_argument);
    EXPECT_THROW((void)driver.export_warm_start(), std::logic_error);
}

// The other half of the same contract sentence. "Completed" means a public
// solve() that RETURNED, so a call that throws does not arm the export and,
// symmetrically, does not disarm one an earlier call armed -- the capture is
// simply never reached. Pinned because the opposite shape is the plausible one
// (it is what the retired staging entry's own clears-first rule did to staged
// state),
// and a consumer that solved, then hit a bad model, then exported would silently
// get a refusal instead of the payload it was entitled to.
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

    SqpSolver driver{SqpOptions{}};
    const SqpResult first = driver.solve(*bridge, good->start_point());
    ASSERT_EQ(first.status, SolveStatus::kOptimal);
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

// The round trip. The exported blocks are the solution's own vectors -- model
// space IS declared space on this engine, so they are equal bit-for-bit, not
// merely close -- at the model's declared widths, and the stamp is the key of
// the bridge the solve ran against.
TEST(SqpWarmCurrency, ExportCarriesTheSolutionAtDeclaredWidthsAndTheBridgesKey) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);

    SqpSolver driver{SqpOptions{}};
    const SqpResult sol = driver.solve(*bridge, model->start_point());
    ASSERT_EQ(sol.status, SolveStatus::kOptimal);

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

    SqpSolver driver{SqpOptions{}};
    ASSERT_EQ(driver.solve(*narrow).status, SolveStatus::kOptimal);
    ASSERT_EQ(driver.export_warm_start().primal_.size(), 3);

    ASSERT_EQ(driver.solve(*wide).status, SolveStatus::kOptimal);
    EXPECT_EQ(driver.export_warm_start().primal_.size(), 4);
}

// --- The hand-over: what is refused HERE, and what is not ---

TEST(SqpWarmCurrency, TheHandOverRefusesANonFiniteBlock) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpResult sol = solve_fixture_cold(*model);

    WarmStartData data = core_payload(sol, *bridge);
    data.iq_lmults_(0) = std::numeric_limits<double>::quiet_NaN();

    SqpSolver driver{SqpOptions{}};
    EXPECT_THROW((void)driver.solve(*bridge, model->start_point(), data), std::invalid_argument);

    // Refused, and nothing left behind: the next solve is cold, which the level
    // reading says outright.
    const SqpResult after = driver.solve(*bridge, model->start_point());
    EXPECT_EQ(after.counters.start_level_used, StartLevel::kCold);
}

// The one structural question answerable with no problem in hand: `primal_` and
// `bound_lmults_` are two readings of one space, so they are EITHER BOTH EMPTY
// (the multipliers-only seed) OR ONE LENGTH. This is the half-empty shape,
// which is neither.
TEST(SqpWarmCurrency, TheHandOverRefusesACoreThatDisagreesWithItself) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpResult sol = solve_fixture_cold(*model);

    WarmStartData data = core_payload(sol, *bridge);
    data.bound_lmults_ = Vec::Zero(2);

    SqpSolver driver{SqpOptions{}};
    EXPECT_THROW((void)driver.solve(*bridge, model->start_point(), data), std::invalid_argument);
}

// A CORRUPT payload under the KNOWN tag is refused at the hand-over, naming the
// tag.
// Corruption is not a foreign tag: a reader that skipped it would silently
// cold-start a solve the caller asked to cross over into.
TEST(SqpWarmCurrency, TheHandOverRefusesAMalformedPolishPayloadNamingTheTag) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpResult sol = solve_fixture_cold(*model);

    WarmStartData data = core_payload(sol, *bridge);
    WarmExtension ext = polish_extension(fixture_polish(sol));
    ext.payload_.resize(ext.payload_.size() - 4); // truncated mid-block
    data.extensions_.push_back(ext);

    SqpSolver driver{SqpOptions{}};
    try {
        (void)driver.solve(*bridge, model->start_point(), data);
        FAIL() << "a truncated payload under the known tag must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find(std::string(kIpmPolishTag)), std::string::npos)
            << "the refusal must name the tag it refused under: " << message;
        EXPECT_NE(message.find("SqpSolver::solve"), std::string::npos) << message;
    }
}

// THE SIGN REFUSAL, both blocks, in the same terms the IPM's hand-over uses.
// The extension states z_lower_/z_upper_ as prices -- non-negative at every
// coordinate -- so a negative entry is corruption, not a seed. Left standing
// it would reach from_interior_point, whose activity rule leaves a wrong-sign
// price FREE while z = z_lower - z_upper carries the corruption into the
// object regardless; the kSeeded clamp defends `lambda_i`, not these blocks.
// Refused here so the class is loud on both engines rather than half-absorbed
// on each (tests/interior/test_ipm_warm_start.cpp pins the IPM half).
TEST(SqpWarmCurrency, TheHandOverRefusesANegativeLowerBoundPriceNamingTheTagAndTheBlock) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpResult sol = solve_fixture_cold(*model);

    WarmStartData data = core_payload(sol, *bridge);
    IpmPolishData polish = fixture_polish(sol);
    ASSERT_GT(polish.z_lower_.size(), 0);
    polish.z_lower_(0) = -1.0;
    data.extensions_.push_back(polish_extension(polish));

    SqpSolver driver{SqpOptions{}};
    try {
        (void)driver.solve(*bridge, model->start_point(), data);
        FAIL() << "a negative price under the known tag must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find(std::string(kIpmPolishTag)), std::string::npos) << message;
        EXPECT_NE(message.find("SqpSolver::solve"), std::string::npos) << message;
        EXPECT_NE(message.find("lower-bound multiplier block"), std::string::npos) << message;
        EXPECT_NE(message.find("index 0"), std::string::npos) << message;
    }

    // Refused, and nothing left behind: the same driver's next solve, with no
    // payload argument, is cold.
    const SqpResult after = driver.solve(*bridge, model->start_point());
    EXPECT_EQ(after.counters.start_level_used, StartLevel::kCold);
}

TEST(SqpWarmCurrency, TheHandOverRefusesANegativeUpperBoundPriceNamingTheTagAndTheBlock) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpResult sol = solve_fixture_cold(*model);

    WarmStartData data = core_payload(sol, *bridge);
    IpmPolishData polish = fixture_polish(sol);
    ASSERT_GT(polish.z_upper_.size(), 1);
    polish.z_upper_(1) = -1e-12;
    data.extensions_.push_back(polish_extension(polish));

    SqpSolver driver{SqpOptions{}};
    try {
        (void)driver.solve(*bridge, model->start_point(), data);
        FAIL() << "a negative price under the known tag must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find(std::string(kIpmPolishTag)), std::string::npos) << message;
        EXPECT_NE(message.find("SqpSolver::solve"), std::string::npos) << message;
        EXPECT_NE(message.find("upper-bound multiplier block"), std::string::npos) << message;
        EXPECT_NE(message.find("index 1"), std::string::npos) << message;
    }

    // Refused, and nothing left behind: the same driver's next solve, with no
    // payload argument, is cold.
    const SqpResult after = driver.solve(*bridge, model->start_point());
    EXPECT_EQ(after.counters.start_level_used, StartLevel::kCold);
}

// The floor of the contract is INCLUSIVE: an all-zero price block is what an
// unpriced or absent side carries on the ordinary path, so it is accepted and
// the solve crosses over. The refusal above is about the SIGN, not about zero.
TEST(SqpWarmCurrency, AZeroValuedPriceBlockIsStillAccepted) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpResult sol = solve_fixture_cold(*model);

    WarmStartData data = core_payload(sol, *bridge);
    IpmPolishData polish = fixture_polish(sol);
    polish.z_lower_.setZero();
    polish.z_upper_.setZero();
    data.extensions_.push_back(polish_extension(polish));

    SqpSolver driver{SqpOptions{}};
    SqpResult out;
    ASSERT_NO_THROW(out = driver.solve(*bridge, model->start_point(), data));
    EXPECT_EQ(out.counters.start_level_used, StartLevel::kSeeded);
}

// A FOREIGN tag is skipped silently (R3) -- a capability downgrade, not an
// error. The value is accepted, applies, and the solve runs as a core-only warm
// one.
TEST(SqpWarmCurrency, AForeignExtensionTagIsSkippedSilently) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpResult sol = solve_fixture_cold(*model);

    WarmStartData data = core_payload(sol, *bridge);
    WarmExtension ext;
    ext.tag_ = "some.other.engine.v3";
    ext.payload_ = {std::byte{0xde}, std::byte{0xad}};
    data.extensions_.push_back(ext);

    SqpSolver driver{SqpOptions{}};
    SqpResult out;
    ASSERT_NO_THROW(out = driver.solve(*bridge, model->start_point(), data));
    EXPECT_EQ(out.status, SolveStatus::kOptimal);
    EXPECT_EQ(out.counters.start_level_used, StartLevel::kSeeded);
}

// THE CLEARS-FIRST RULE IS RETIRED WITH STAGING (M6 W5 T8.5), and this test is
// its replacement rather than its deletion. It pinned the hazard of a value
// SURVIVING a call: stage a good payload, get a refusal on a bad one, solve
// anyway, and silently warm-start off the value the caller had moved on from.
// A payload is an ARGUMENT now, so it lives exactly as long as the call it is
// passed to and there is no earlier value left standing to be found. What is
// pinned here is that positive fact, on a driver that really did run warm once.
TEST(SqpWarmCurrency, ARefusedPayloadLeavesNothingBehindForTheNextSolve) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpResult sol = solve_fixture_cold(*model);

    SqpSolver driver{SqpOptions{}};
    const SqpResult warm = driver.solve(*bridge, model->start_point(), core_payload(sol, *bridge));
    ASSERT_EQ(warm.counters.start_level_used, StartLevel::kSeeded);

    WarmStartData bad = core_payload(sol, *bridge);
    bad.primal_(0) = std::numeric_limits<double>::infinity();
    EXPECT_THROW((void)driver.solve(*bridge, model->start_point(), bad), std::invalid_argument);

    const SqpResult out = driver.solve(*bridge, model->start_point());
    EXPECT_EQ(out.counters.start_level_used, StartLevel::kCold)
        << "a solve with no payload argument carries nothing from an earlier one";
}

// --- Solve entry: the two checks that need a bound problem ---

TEST(SqpWarmCurrency, SolveEntryRefusesAPayloadAtTheWrongSizes) {
    const auto narrow = std::make_shared<CurrencyModel>();
    const auto narrow_bridge = make_bridge(narrow);
    const auto wide = std::make_shared<CurrencyWiderModel>();
    const auto wide_bridge = make_bridge(wide);
    const SqpResult sol = solve_fixture_cold(*narrow);

    SqpSolver driver{SqpOptions{}};
    // The HAND-OVER accepts it -- nothing about the payload is internally wrong
    // -- and the SOLVE is where the destination's width becomes knowable.
    try {
        (void)driver.solve(*wide_bridge, wide->start_point(), core_payload(sol, *narrow_bridge));
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
    const SqpResult after = driver.solve(*wide_bridge, wide->start_point());
    EXPECT_EQ(after.counters.start_level_used, StartLevel::kCold);
}

// Same sizes, different declared problem. The declared bound STRUCTURE moved --
// one variable's lower side went from finite to infinite -- which the stamp's
// bound conjunct carries alone. Structure, not value: moving a finite bound to
// another finite value would not re-key (that is the continuation flow), which
// is why this fixture changes finiteness rather than a number. The size check
// cannot see it; only the stamp can, at solve entry, against the problem that
// call binds. This is also the pin for the refusal naming both key digests.
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

    const SqpResult sol = solve_fixture_cold(*original);
    SqpSolver driver{SqpOptions{}};
    try {
        (void)driver.solve(*rekeyed_bridge, rekeyed->start_point(),
                           core_payload(sol, *original_bridge));
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

    const SqpResult after = driver.solve(*rekeyed_bridge, rekeyed->start_point());
    EXPECT_EQ(after.counters.start_level_used, StartLevel::kCold);
}

// --- One-shot consumption, and the two-sources refusal ---

TEST(SqpWarmCurrency, APayloadAppliesToItsOwnCallAndTheOneAfterIsCold) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpResult sol = solve_fixture_cold(*model);

    SqpSolver driver{SqpOptions{}};
    const SqpResult first = driver.solve(*bridge, model->start_point(), core_payload(sol, *bridge));
    EXPECT_EQ(first.status, SolveStatus::kOptimal);
    EXPECT_EQ(first.counters.start_level_used, StartLevel::kSeeded);
    EXPECT_EQ(first.counters.n_seeded, 1);

    const SqpResult second = driver.solve(*bridge, model->start_point());
    EXPECT_EQ(second.counters.start_level_used, StartLevel::kCold)
        << "a payload applies to the call it is an argument to and to no other: the "
           "second solve gets nothing";
    EXPECT_EQ(second.counters.n_seeded, 0);
}

// THE TWO-SOURCES REFUSAL IS RETIRED (M6 W5 T8.5) and so is the test that
// pinned it, `AnExplicitWarmArgumentAndAStagedValueAreRefusedTogether`. It
// existed because a driver could hold a STAGED payload while a call passed a
// native `SqpWarmStart` argument -- two warm-start sources for one solve, neither
// of which could silently win. With staging gone a call's warm-start source is
// exactly its own argument, and `refuse_two_warm_sources()` went with it.
//
// WHAT REPLACES IT is a statement about OVERLOAD RESOLUTION, which is where the
// "which source wins" question now lives: an lvalue of `SqpWarmStart` selects
// the native overload and an lvalue of `WarmStartData` selects the payload one,
// unambiguously, because neither type converts to the other. (The one ambiguous
// SPELLING is a braced third argument -- `solve(bridge, x0, {})` would match
// `SolveBudget`, `WarmStartData` and `SqpWarmStart` alike -- and it is a
// compile error, not a silent precedence; the migration guide says to spell the
// type. It cannot be pinned by a test that has to compile.)
TEST(SqpWarmCurrency, TheNativeAndPayloadOverloadsAreSelectedByArgumentType) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpResult sol = solve_fixture_cold(*model);

    // THE NATIVE ROUTE: a SqpWarmStart from a prior solve of the same model
    // carries a real structure hash, so it can reach kWarm.
    SqpSolver native{SqpOptions{}};
    const SqpResult by_native = native.solve(*bridge, model->start_point(), sol.warm_start);
    EXPECT_EQ(by_native.status, SolveStatus::kOptimal);
    EXPECT_GE(static_cast<int>(by_native.counters.start_level_used),
              static_cast<int>(StartLevel::kSeeded));

    // THE PAYLOAD ROUTE: the same solve's value as declared-space currency
    // carries hash 0 by construction, so it caps at kSeeded. Same driver class,
    // same problem, same starting point -- only the argument's TYPE differs,
    // and that is what selects the route.
    SqpSolver payload{SqpOptions{}};
    const SqpResult by_payload =
        payload.solve(*bridge, model->start_point(), core_payload(sol, *bridge));
    EXPECT_EQ(by_payload.status, SolveStatus::kOptimal);
    EXPECT_EQ(by_payload.counters.start_level_used, StartLevel::kSeeded);
}

// --- The two application routes, and the level they enter at ---

// CORE-ONLY: accepted (R3) and entering at StartLevel::kSeeded -- the level the
// currency's core can justify, since the kWarm/kHot gate is a fingerprint of
// assembled matrices the value does not carry.
TEST(SqpWarmCurrency, ACoreOnlyValueEntersAtTheSeededLevel) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpResult sol = solve_fixture_cold(*model);

    SqpSolver driver{SqpOptions{}};

    // x0 is deliberately NOWHERE NEAR the solution: if the payload's primal did
    // not replace it, the first iterate could not be the solved point.
    const SqpResult out = driver.solve(*bridge, Vec::Constant(3, 1.75), core_payload(sol, *bridge));

    EXPECT_EQ(out.status, SolveStatus::kOptimal);
    EXPECT_EQ(out.counters.start_level_used, StartLevel::kSeeded);
    // The payload's primal replaced x0, through the ingest rule this class
    // already documents ("x0 is the cold fallback"): the first measured iterate
    // is the exported point, not the argument.
    ASSERT_FALSE(out.history.empty());
    EXPECT_NEAR(out.history.front().f, sol.f, 1e-12);
    // Warm from a KKT point of the same problem: certified without spending a
    // major. A margin, not an exact count -- what is asserted is that the
    // ingest reached the convergence test, not a particular trajectory.
    EXPECT_LE(out.counters.major_iters, sol.counters.major_iters);
}

// WITH THE POLISH TAG: the value is routed through to_sqp_warm_start and
// nothing is re-implemented. Pinned by EQUIVALENCE -- handing the payload to
// solve must produce the same solve as calling the bridge by hand and passing
// its output through the NATIVE overload.
TEST(SqpWarmCurrency, APolishTaggedValueRoutesThroughTheCrossoverBridge) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpResult sol = solve_fixture_cold(*model);

    WarmStartData data = core_payload(sol, *bridge);
    const IpmPolishData polish = fixture_polish(sol);
    data.extensions_.push_back(polish_extension(polish));

    // (a) through the payload route.
    SqpSolver staged_driver{SqpOptions{}};
    const SqpResult staged = staged_driver.solve(*bridge, model->start_point(), data);

    // (b) through the bridge called by hand, on a fresh driver.
    const SqpWarmStart crossover = to_sqp_warm_start(data, model->lower(), model->upper(),
                                                     declaration_key(bridge->declaration()));
    SqpSolver explicit_driver{SqpOptions{}};
    const SqpResult direct = explicit_driver.solve(*bridge, model->start_point(), crossover);

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

// The same staged payload, from cold, twice: bit-identical first iterates on
// the R5 instrument above, so a drift anywhere in the ingest shows up here.
TEST(SqpWarmCurrency, PassingTheSameValueTwiceFromColdIsBitIdentical) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpResult sol = solve_fixture_cold(*model);
    const WarmStartData data = core_payload(sol, *bridge);

    SqpSolver first_driver{SqpOptions{}};
    const SqpResult first = first_driver.solve(*bridge, model->start_point(), data);

    SqpSolver second_driver{SqpOptions{}};
    const SqpResult second = second_driver.solve(*bridge, model->start_point(), data);

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

// The crossover as one composition: an interior-point solve, its exported
// value, and an SQP solve that finishes from it -- no re-stamp, no conversion,
// nothing in between.
//
// The first two assertions below are why the stamp is the DECLARATION key
// rather than a layout digest. The two engines lay one declared model's
// (row, column) claim SET in different orders -- the interior-point program
// lays the constraint Jacobian's claims before the Hessian's, NlpModelAssembly
// the Hessian's first -- and a claim digest is order-sensitive by design. So
// the layout keys still differ, correctly for the question that key answers,
// and the stamps agree.
TEST(SqpWarmCurrency, InteriorPointExportCrossesOverIntoTheSqpEngine) {
    const auto problem = std::make_shared<CurrencyIpmProblem>();

    const auto ipm_program = hven::solvers::make_nlp_program(problem);
    hven::solvers::IpmSolver ipm;
    {
        auto o = ipm.options();
        o.common.print_level = 10;
        ipm.set_options(std::move(o));
    }
    Eigen::VectorXd x0(3);
    x0 << 0.0, 0.0, 0.5;
    const hven::solvers::IpmResult ipm_result = ipm.solve(*ipm_program, x0);
    ASSERT_EQ(ipm_result.status, hven::solvers::SolveStatus::kOptimal);

    // THE EXPORT, OFF THE RESULT (M6 W5 T8.5): the solver-side
    // export_warm_start() is gone and the snapshot travels on the returned
    // value.
    const std::optional<WarmStartData> snapshot = ipm_result.export_warm_start();
    ASSERT_TRUE(snapshot.has_value());
    const WarmStartData exported = *snapshot;
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
    EXPECT_NE(ipm_program->model_structure_key().claim_digest_,
              bridge->model_structure_key().claim_digest_)
        << "the two engines really do lay this declaration's claims differently; "
           "if they ever stop, this pin is the note that says the layout keys "
           "converged, not that anything broke";
    EXPECT_TRUE(declaration_key(ipm_program->declaration()) ==
                declaration_key(bridge->declaration()))
        << "one declared problem must key the same on both engines -- that is "
           "the whole content of the declaration-identity ruling";
    EXPECT_TRUE(exported.structure_key_ == declaration_key(bridge->declaration()))
        << "and the exported value carries that key, so it stages here as-is";

    // THE COMPOSITION, with the value exactly as the other engine handed it
    // over.
    SqpSolver sqp{SqpOptions{}};
    const SqpResult out = sqp.solve(*bridge, model->start_point(), exported);

    EXPECT_EQ(out.status, SolveStatus::kOptimal);
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
    const SqpResult cold = solve_fixture_cold(*model);
    ASSERT_EQ(cold.status, SolveStatus::kOptimal);
    EXPECT_LT(out.counters.major_iters, cold.counters.major_iters)
        << "the crossover must save majors against a cold solve of the same problem";

    // And the SQP engine can hand the result straight back, under the same
    // stamp -- so the composition closes rather than ending in a value only one
    // side can read.
    const WarmStartData reexported = sqp.export_warm_start();
    EXPECT_TRUE(reexported.structure_key_ == exported.structure_key_);
    EXPECT_EQ(reexported.primal_, out.x);
}

// Genuine staleness: the caller transcribed a different problem -- one more
// equality row -- and the value taken on the old one is refused.
//
// A row change moves a BLOCK LENGTH as well as the key, so the size check is
// what fires (it runs first, deliberately: "eq_lmults_ holds 1, this problem
// declares 2" is the more actionable of the two diagnostics when both are
// true). The keys are asserted apart directly below, so the pin says what it
// means. The same-size case, where the stamp is the only thing that can catch
// the change, is the bound-structure test further up.
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

    const SqpResult sol = solve_fixture_cold(*original);
    SqpSolver driver{SqpOptions{}};
    try {
        (void)driver.solve(*extra_bridge, extra->start_point(),
                           core_payload(sol, *original_bridge));
        FAIL() << "a value taken on a one-row-smaller declaration must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("eq_lmults_"), std::string::npos) << message;
    }

    // Consumed by the refusal, like every other solve-entry refusal.
    const SqpResult after = driver.solve(*extra_bridge, extra->start_point());
    EXPECT_EQ(after.counters.start_level_used, StartLevel::kCold);
}

// --- R6: cold-vs-hot is never answer-observable ---

namespace {

// BITWISE, and it says so by reading the bits. EXPECT_EQ on two doubles is
// `==`, which calls -0.0 and +0.0 equal though their bits differ and would
// call NaN unequal to itself; R6 asks for bit identity, so the comparison is
// on the bit patterns -- the same idiom the interior engine's R6 pin uses.
void expect_same_bits(double a, double b, const std::string &what) {
    EXPECT_EQ(std::bit_cast<std::uint64_t>(a), std::bit_cast<std::uint64_t>(b))
        << what << ": " << a << " vs " << b;
}

// Every answer an SqpResult reports -- the point, its prices, the objective,
// the terminal KKT measurement and the verdict -- compared bitwise: R6 asks for
// exact identity, not a neighbourhood of it. Wall time is deliberately absent:
// it may differ freely, and it is informational, never asserted.
void expect_same_answer(const SqpResult &hot, const SqpResult &cold) {
    EXPECT_EQ(hot.status, cold.status);
    EXPECT_EQ(hot.infeasibility_certified, cold.infeasibility_certified);
    expect_same_bits(hot.f, cold.f, "objective");
    expect_same_bits(hot.sqp_stationarity, cold.sqp_stationarity, "stationarity");
    expect_same_bits(hot.sqp_feasibility, cold.sqp_feasibility, "feasibility");
    expect_same_bits(hot.sqp_complementarity, cold.sqp_complementarity, "complementarity");
    expect_same_bits(hot.kkt_residual, cold.kkt_residual, "kkt_residual");

    ASSERT_EQ(hot.x.size(), cold.x.size());
    for (Index i = 0; i < hot.x.size(); ++i) {
        expect_same_bits(hot.x(i), cold.x(i), "primal " + std::to_string(i));
        expect_same_bits(hot.z(i), cold.z(i), "bound multiplier " + std::to_string(i));
    }
    ASSERT_EQ(hot.lambda_e.size(), cold.lambda_e.size());
    for (Index i = 0; i < hot.lambda_e.size(); ++i) {
        expect_same_bits(hot.lambda_e(i), cold.lambda_e(i),
                         "equality multiplier " + std::to_string(i));
    }
    ASSERT_EQ(hot.lambda_i.size(), cold.lambda_i.size());
    for (Index i = 0; i < hot.lambda_i.size(); ++i) {
        expect_same_bits(hot.lambda_i(i), cold.lambda_i(i),
                         "inequality multiplier " + std::to_string(i));
    }

    // THE PATH, not only the endpoint: the two solves visited the same
    // iterates, in the same order, measuring the same residuals at each. A
    // divergence anywhere in the loop that the endpoint happened to absorb
    // still fails here.
    ASSERT_EQ(hot.history.size(), cold.history.size());
    for (std::size_t k = 0; k < hot.history.size(); ++k) {
        const auto &a = hot.history[k];
        const auto &b = cold.history[k];
        const std::string row = "row " + std::to_string(k) + " ";
        expect_same_bits(a.f, b.f, row + "f");
        expect_same_bits(a.stationarity, b.stationarity, row + "stationarity");
        expect_same_bits(a.feasibility, b.feasibility, row + "feasibility");
        expect_same_bits(a.complementarity, b.complementarity, row + "complementarity");
        expect_same_bits(a.kkt_residual, b.kkt_residual, row + "kkt_residual");
        expect_same_bits(a.violation_l1, b.violation_l1, row + "violation_l1");
        expect_same_bits(a.tr_radius, b.tr_radius, row + "tr_radius");
        expect_same_bits(a.step_norm, b.step_norm, row + "step_norm");
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

// M5 R6: hot-state reuse is never observable in answers -- the SQP side.
//
// SqpSolver carries no cross-solve NUMERIC state of its own; the one piece in
// the picture is its QpEngine's border cache, consulted through the gate whose
// conjuncts are qp_engine.h's HOT-START REUSE conditions (a)-(e). A grant means
// the matrix that WOULD have been assembled and factorized is bit-for-bit the
// one already in hand, and the fast path skips only that assembly and
// factorization -- never sync_borders(), which reconciles unconditionally.
//
// This is that argument turned into a measurement: two solves on ONE driver,
// against a fresh driver's single solve of the same problem from the same start
// over the same bridge, so the driver's own carried state is the only thing
// that differs. Exact equality, no margins; wall time is exempt and not read.
TEST(SqpWarmCurrency, ASecondSolveOnOneDriverAnswersExactlyWhatAFreshDriverAnswers) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const Vec x0 = model->start_point();

    SqpSolver reused{SqpOptions{}};
    const SqpResult first = reused.solve(*bridge, x0);
    ASSERT_EQ(first.status, SolveStatus::kOptimal);
    // The hot solve. Nothing was staged between the two calls, so this call
    // differs from the first only in what the driver and its engine carried
    // out of it.
    const SqpResult second = reused.solve(*bridge, x0);
    ASSERT_EQ(second.status, SolveStatus::kOptimal);

    SqpSolver fresh{SqpOptions{}};
    const SqpResult cold = fresh.solve(*bridge, x0);
    ASSERT_EQ(cold.status, SolveStatus::kOptimal);

    expect_same_answer(second, cold);
    expect_same_counters(second.counters, cold.counters);
    // The first solve is the control: a driver's FIRST solve is a cold one, so
    // all three of these agree, and the second-vs-first comparison is what
    // shows the carried state changed nothing on this instance either.
    expect_same_answer(second, first);
    expect_same_counters(second.counters, first.counters);
}

// The same question with the second solve warm-started: the test above
// re-solves from the same cold start, this one stages the exported solution
// into BOTH sides first, so the warm ingest is held constant and the engine's
// carried cache is again the only difference between them.
//
// Neither test claims a granted reuse. Measured on this fixture, no
// factorization was saved -- the reused driver pays exactly the factorizations
// and symbolic analyses the fresh one pays -- and qp_engine.h notes its five
// conditions are necessary but not sufficient for `factorizations == 0`, so
// equal counts are consistent with a granted reuse that border_candidate then
// rebuilt anyway. What these two pin is that whatever the driver carried out of
// an earlier solve changed nothing. The case where the fast path demonstrably
// DOES skip a factorization and the answer is still bit-identical is pinned by
// tests/sqp/test_warm_start.cpp's SqpWarmStart.HotReuseIsNeverAnswerObservable.
TEST(SqpWarmCurrency, AWarmResolveOnAUsedDriverAnswersExactlyWhatAFreshDriverAnswers) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const Vec x0 = model->start_point();

    SqpSolver reused{SqpOptions{}};
    const SqpResult first = reused.solve(*bridge, x0);
    ASSERT_EQ(first.status, SolveStatus::kOptimal);
    const WarmStartData payload = reused.export_warm_start();

    const SqpResult hot = reused.solve(*bridge, x0, payload);
    ASSERT_EQ(hot.status, SolveStatus::kOptimal);

    SqpSolver fresh{SqpOptions{}};
    const SqpResult cold = fresh.solve(*bridge, x0, payload);
    ASSERT_EQ(cold.status, SolveStatus::kOptimal);

    // Both staged the same value, so both resolve at the same level -- kSeeded,
    // never kWarm or kHot: a currency-borne value carries no structure hash,
    // which is what those two levels are gated on. Asserted here too because
    // R6's other half is that a reuse form which cannot argue answer-neutrality
    // is not silently enabled, and a level climbing to kHot on the reused driver
    // would be exactly that.
    EXPECT_EQ(hot.counters.start_level_used, StartLevel::kSeeded);
    EXPECT_EQ(cold.counters.start_level_used, StartLevel::kSeeded);

    expect_same_answer(hot, cold);
    expect_same_counters(hot.counters, cold.counters);
}

// ===========================================================================
// M6 W1 TASK 7 -- the preserved-seed ingest and the mode-local cold degrade
// ===========================================================================
//
// PLAN RULING 4, HALF RETIRED. Under `QpMode::kIpm` the interior-point tier's
// MAIN-subproblem seed is still built here, from the validated payload and its
// polish extension, WITHOUT the signed-z flattening the crossover necessarily
// performs. That half stands.
//
// WHAT WENT (M6 W5 T8.5, owner ruling; design 2.4): the MODE-LOCAL COLD DEGRADE.
// M5 ruling 4 made a stamp or dimension mismatch a cold grade under kIpm --
// spec section 5.4 lists both as the cold grade -- where kWalk and kSsn threw.
// The ruling is RETIRED FOR THAT CASE: identity mismatch REFUSES IN EVERY MODE.
// The reason is that the old shape made an IDENTITY answer depend on which QP
// KERNEL the driver happened to be configured for, so the same payload against
// the same problem was refused or silently discarded according to something
// identity has nothing to do with. Pattern and value defects still degrade, in
// every mode, exactly as before.
//
// The two tests below are DECLARED FLIPS of the two that pinned the old
// behaviour (`KIpmDegradesAWrongSizedStagedValueColdWhereKWalkThrows` and
// `KIpmDegradesAStampMismatchColdWhereKWalkThrows`): same fixtures, same
// payloads, the kIpm arm's expectation inverted from a cold grade to the same
// refusal kWalk gives.

namespace {

SqpOptions ipm_currency_options() {
    SqpOptions o;
    o.qp_mode = QpMode::kIpm;
    return o;
}

// The fixture's own converged value, moved off the solution so the next solve
// has to build at least one subproblem for the tier to enter.
WarmStartData perturbed_core(const SqpResult &sol, const NlpModelAssembly &bridge) {
    WarmStartData data = core_payload(sol, bridge);
    data.primal_(0) += 0.5;
    return data;
}

} // namespace

TEST(SqpWarmCurrency, KIpmRefusesAWrongSizedPayloadLikeKWalk) {
    const auto narrow = std::make_shared<CurrencyModel>();
    const auto narrow_bridge = make_bridge(narrow);
    const auto wide = std::make_shared<CurrencyWiderModel>();
    const auto wide_bridge = make_bridge(wide);
    const SqpResult sol = solve_fixture_cold(*narrow);
    const WarmStartData payload = core_payload(sol, *narrow_bridge);

    // kWalk: unchanged. The 3-wide payload is refused against the 4-variable
    // problem, loudly, naming the block.
    SqpSolver walker{SqpOptions{}};
    EXPECT_THROW((void)walker.solve(*wide_bridge, wide->start_point(), payload),
                 std::invalid_argument);

    // kIpm: THE FLIP. The same value on the same problem is refused the same
    // way. It used to be cold-graded and the solve ran.
    SqpSolver ipm{ipm_currency_options()};
    EXPECT_THROW((void)ipm.solve(*wide_bridge, wide->start_point(), payload),
                 std::invalid_argument);

    // And the two refusals are the SAME refusal, not merely the same type:
    // one identity check, one message, whatever the kernel.
    std::string walk_message;
    std::string ipm_message;
    try {
        SqpSolver w{SqpOptions{}};
        (void)w.solve(*wide_bridge, wide->start_point(), payload);
    } catch (const std::invalid_argument &error) {
        walk_message = error.what();
    }
    try {
        SqpSolver i{ipm_currency_options()};
        (void)i.solve(*wide_bridge, wide->start_point(), payload);
    } catch (const std::invalid_argument &error) {
        ipm_message = error.what();
    }
    EXPECT_FALSE(walk_message.empty());
    EXPECT_EQ(walk_message, ipm_message);
}

TEST(SqpWarmCurrency, KIpmRefusesAStampMismatchLikeKWalk) {
    const auto original = std::make_shared<CurrencyModel>(/*lower_x0_finite=*/true);
    const auto original_bridge = make_bridge(original);
    const auto rekeyed = std::make_shared<CurrencyModel>(/*lower_x0_finite=*/false);
    const auto rekeyed_bridge = make_bridge(rekeyed);
    ASSERT_EQ(original->n(), rekeyed->n()) << "same sizes, so only the stamp can refuse";

    const SqpResult sol = solve_fixture_cold(*original);
    const WarmStartData payload = core_payload(sol, *original_bridge);

    SqpSolver walker{SqpOptions{}};
    EXPECT_THROW((void)walker.solve(*rekeyed_bridge, rekeyed->start_point(), payload),
                 std::invalid_argument);

    // THE FLIP: kIpm refuses a stamp mismatch too, where it used to cold-grade.
    SqpSolver ipm{ipm_currency_options()};
    try {
        (void)ipm.solve(*rekeyed_bridge, rekeyed->start_point(), payload);
        FAIL() << "a stamp mismatch must refuse under kIpm exactly as it does under kWalk";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        // The stamp refusal, not some other one: it names both keys.
        EXPECT_NE(message.find("declaration key"), std::string::npos) << message;
    }
}

// PLAN SECTION 7 NOTE (f). Staging finiteness-checks the extension's three
// VECTORS but not `mu_`, and the decode round-trips NaN bit-exactly -- so the
// check belongs at the tier's own ingest, where a non-finite `mu_` marks the
// extension MALFORMED and the tier degrades COLD. The core staging throw is
// unchanged: the value still stages in every mode.
//
// THREE SIGNATURES, ONE FIXTURE. Cold pays no repair and adopts no mu; the
// base grade repairs (the re-split leaves exact zeros) and adopts nothing;
// the full grade does both. That is what separates "degraded cold" from
// "degraded to base" from "used".
TEST(SqpWarmCurrency, ANonFiniteExtensionMuDegradesTheTierColdAndStagesInEveryMode) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpResult sol = solve_fixture_cold(*model);

    IpmPolishData usable = fixture_polish(sol);
    usable.mu_ = 0.1; // binds the section 5.3 clamp from above, so adoption fires
    IpmPolishData malformed = usable;
    malformed.mu_ = std::numeric_limits<double>::quiet_NaN();

    const auto run = [&](const IpmPolishData *polish) {
        WarmStartData data = perturbed_core(sol, *bridge);
        if (polish != nullptr) {
            data.extensions_.push_back(polish_extension(*polish));
        }
        SqpSolver driver{ipm_currency_options()};
        // THE STAGING CONTRACT IS UNCHANGED IN EVERY MODE, malformed `mu_`
        // included -- that is what note (f) means by "the core staging throw is
        // unchanged".
        return driver.solve(*bridge, data.primal_, data);
    };

    const SqpResult full = run(&usable);
    const SqpResult base = run(nullptr);
    const SqpResult cold_tier = run(&malformed);

    ASSERT_EQ(full.status, SolveStatus::kOptimal);
    ASSERT_EQ(base.status, SolveStatus::kOptimal);
    ASSERT_EQ(cold_tier.status, SolveStatus::kOptimal);

    EXPECT_GT(full.counters.ipqp.ipqp_mu_adopted, 0) << "the full grade reads the payload mu";
    EXPECT_GT(full.counters.ipqp.ipqp_restart_repairs, 0);

    EXPECT_EQ(base.counters.ipqp.ipqp_mu_adopted, 0) << "the base grade has no payload mu";
    EXPECT_GT(base.counters.ipqp.ipqp_restart_repairs, 0)
        << "but it is still a WARM restart -- the re-split's exact zeros are repaired";

    // T9 HARDENING (T7 registered it): the `> 0` arms above are satisfiable by
    // any later cross-major carry that happens to repair once, so the two
    // discriminating counts are pinned EXACTLY as well.
#ifdef USE_ACCELERATE_SPARSE
    RecordProperty("three_signature_accelerate", "UNOBSERVED -- the exact counts are MKL-only");
#else
    EXPECT_EQ(full.counters.ipqp.ipqp_restart_repairs, 1);
    EXPECT_EQ(full.counters.ipqp.ipqp_mu_adopted, 1);
    EXPECT_EQ(base.counters.ipqp.ipqp_restart_repairs, 1);
#endif

    EXPECT_EQ(cold_tier.counters.ipqp.ipqp_mu_adopted, 0);
    EXPECT_EQ(cold_tier.counters.ipqp.ipqp_restart_repairs, 0)
        << "a malformed extension degrades the tier COLD, not to the base grade: a cold first "
           "subproblem pays no repair at all";

    // The solve-level ingest is untouched by the malformed extension -- flow
    // (a) never reads `mu_`, so all three resolve at the same level.
    EXPECT_EQ(full.counters.start_level_used, StartLevel::kSeeded);
    EXPECT_EQ(base.counters.start_level_used, StartLevel::kSeeded);
    EXPECT_EQ(cold_tier.counters.start_level_used, StartLevel::kSeeded);
}

// M5 R5, RESTATED FOR THE TIER (fixture A8's determinism half): the same
// payload staged twice from cold produces bit-identical first iterates AND
// bit-identical tier counters. Compared with EXPECT_EQ throughout -- a
// near-comparison would pass on a seed that had drifted.
TEST(SqpWarmCurrency, TheSamePayloadStagedTwiceGivesBitIdenticalKIpmSolves) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpResult sol = solve_fixture_cold(*model);

    WarmStartData data = perturbed_core(sol, *bridge);
    data.extensions_.push_back(polish_extension(fixture_polish(sol)));

    SqpSolver a{ipm_currency_options()};
    const SqpResult first = a.solve(*bridge, data.primal_, data);

    SqpSolver b{ipm_currency_options()};
    const SqpResult second = b.solve(*bridge, data.primal_, data);

    ASSERT_EQ(first.status, SolveStatus::kOptimal);
    expect_same_first_iterate(first, second);
    EXPECT_EQ(first.x, second.x);
    EXPECT_EQ(first.counters.ipqp.ipqp_iters, second.counters.ipqp.ipqp_iters);
    EXPECT_EQ(first.counters.ipqp.ipqp_factorizations, second.counters.ipqp.ipqp_factorizations);
    EXPECT_EQ(first.counters.ipqp.ipqp_restart_repairs, second.counters.ipqp.ipqp_restart_repairs);
    EXPECT_EQ(first.counters.ipqp.ipqp_restart_shift_max,
              second.counters.ipqp.ipqp_restart_shift_max);
    EXPECT_EQ(first.counters.ipqp.ipqp_mu_adopted, second.counters.ipqp.ipqp_mu_adopted);
}

// FIX ROUND 1, F1: the tier seed is cleared on EVERY solve entry, not only the
// path that arms it -- here a staged point that IS the solution, so no
// subproblem is ever built. `.superpowers/w1-t7-report.md` FIX ROUND 2.
TEST(SqpWarmCurrency, AStagedTierSeedDoesNotSurviveIntoTheNextSolve) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpResult sol = solve_fixture_cold(*model);

    WarmStartData data = core_payload(sol, *bridge); // AT the solution, unperturbed
    IpmPolishData polish = fixture_polish(sol);
    polish.mu_ = 0.1; // binds the section 5.3 clamp, so a live seed is visible
    data.extensions_.push_back(polish_extension(polish));

    SqpSolver driver{ipm_currency_options()};
    const SqpResult stalled = driver.solve(*bridge, data.primal_, data);
    ASSERT_EQ(stalled.status, SolveStatus::kOptimal);
    ASSERT_EQ(stalled.counters.ipqp.ipqp_symbolic_analyses, 0)
        << "the fixture's premise: this solve entered the tier not at all, so the seed it armed "
           "was never spent";

    // THE NEXT SOLVE, from a perturbed point and through the overload that
    // never consults staged state. It does build subproblems, so a leaked seed
    // would reach the tier and its payload `mu` would bind the clamp.
    Vec moved = data.primal_;
    moved(0) += 0.5;
    const SqpResult after =
        driver.solve(*bridge, moved, SqpWarmStart{}, hven::solvers::SolveBudget{});
    ASSERT_GT(after.counters.ipqp.ipqp_symbolic_analyses, 0)
        << "and this one DID enter the tier, or the assertion below is vacuous";
    EXPECT_EQ(after.counters.ipqp.ipqp_mu_adopted, 0)
        << "a tier seed armed by an earlier call must not reach the next solve's first "
           "subproblem";
}

// CODEX 8: the direct-construction zL/zU pin cannot see a flattening
// introduced in the PAYLOAD path; this is that half, asserted through the
// payload overload itself. `.superpowers/w1-t7-report.md` FIX ROUND 2.
TEST(SqpWarmCurrency, ThePayloadPathDeliversThePolishSplitWithoutFlattening) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpResult sol = solve_fixture_cold(*model);

    // BOTH sides priced on one variable -- the configuration a signed `z`
    // cannot represent, and the only one that can tell the two paths apart.
    IpmPolishData polish = fixture_polish(sol);
    polish.z_lower_ = Vec::Constant(model->n(), 0.06);
    polish.z_upper_ = Vec::Constant(model->n(), 0.04);
    polish.mu_ = 0.1;

    // A hand-rolled perturbation, not `perturbed_core`: both offsets avoid a
    // structural repair floor (the inequality's slack, x2's own lower bound)
    // that would fire regardless of the payload. `.superpowers/w1-t7-report.md`
    // FIX ROUND 2.
    const auto run = [&](bool with_extension) {
        WarmStartData data = core_payload(sol, *bridge);
        data.primal_(0) -= 0.2;
        data.primal_(2) += 0.3;
        if (with_extension) {
            data.extensions_.push_back(polish_extension(polish));
        }
        // THE CEILING IS THE FIXTURE'S OWN, not the shipped one: T10b's default
        // (1e-2) sits BELOW this payload's `mu_`, so the section 5.3 clamp would
        // cap the adoption out of existence and the pin below would go vacuous.
        SqpOptions o = ipm_currency_options();
        o.ipqp.ipqp_init_mu = polish.mu_;
        SqpSolver driver{o};
        return driver.solve(*bridge, data.primal_, data);
    };

    const SqpResult full = run(true);
    const SqpResult base = run(false);
    ASSERT_EQ(full.status, SolveStatus::kOptimal);
    ASSERT_EQ(base.status, SolveStatus::kOptimal);

    // Precondition for the zero-repair proof below: no absent side (absent
    // sides are zeroed without touching the counters). T7 fix round 3 report.
    ASSERT_TRUE(model->lower().array().isFinite().all());
    ASSERT_TRUE(model->upper().array().isFinite().all());

    // With every present-side movement counted (fix round 1, F4), zero
    // repairs and zero shift prove the full grade ingested the split unmoved.
    EXPECT_EQ(full.counters.ipqp.ipqp_restart_repairs, 0);
    EXPECT_DOUBLE_EQ(full.counters.ipqp.ipqp_restart_shift_max, 0.0);
    EXPECT_GT(base.counters.ipqp.ipqp_restart_repairs, 0);
    EXPECT_GT(full.counters.ipqp.ipqp_mu_adopted, 0);
    EXPECT_EQ(base.counters.ipqp.ipqp_mu_adopted, 0);
    EXPECT_NE(full.counters.ipqp.ipqp_iters, 0);
}

// FIX ROUND 1, tycho F1(a): SECTION 5.3'S CEILING READ OFF THE ARITHMETIC, not off a fixture's
// own knob -- one staged payload, two ceilings, and the ceiling is the ONLY variable.
// `.superpowers/w1-t10b-fix1-report.md` item C.
TEST(SqpWarmCurrency, APolishMuAboveTheShippedCeilingIsClampedOutOfAdoption) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpResult sol = solve_fixture_cold(*model);

    IpmPolishData polish = fixture_polish(sol);
    polish.mu_ = 0.1;
    ASSERT_GT(polish.mu_, SqpOptions{}.ipqp.ipqp_init_mu) << "the premise: ABOVE the ceiling";

    const auto run = [&](double ceiling) {
        WarmStartData data = core_payload(sol, *bridge);
        data.primal_(0) -= 0.2;
        data.primal_(2) += 0.3;
        data.extensions_.push_back(polish_extension(polish));
        SqpOptions o = ipm_currency_options();
        o.ipqp.ipqp_init_mu = ceiling;
        SqpSolver driver{o};
        return driver.solve(*bridge, data.primal_, data);
    };

    const SqpResult shipped = run(SqpOptions{}.ipqp.ipqp_init_mu);
    const SqpResult raised = run(polish.mu_);
    ASSERT_EQ(shipped.status, SolveStatus::kOptimal);
    ASSERT_EQ(raised.status, SolveStatus::kOptimal);
    // Non-vacuity: both arms reached the tier, so the counter below reads a clamp and not
    // the absence of a subproblem to clamp.
    ASSERT_GT(shipped.counters.ipqp.ipqp_symbolic_analyses, 0);
    ASSERT_GT(raised.counters.ipqp.ipqp_symbolic_analyses, 0);

    EXPECT_GT(raised.counters.ipqp.ipqp_mu_adopted, 0);
    EXPECT_EQ(shipped.counters.ipqp.ipqp_mu_adopted, 0);
}

// FIX ROUND 1, tycho F1(b): AND THE CEILING IS NOT A BLANKET REFUSAL -- a payload two decades
// under it is adopted at the SHIPPED default, no fixture-local knob. The seed moves 1e-6, not
// 0.2, so its own measured complementarity sits BELOW the payload; item C measures both.
TEST(SqpWarmCurrency, APolishMuUnderTheShippedCeilingIsStillAdopted) {
    const auto model = std::make_shared<CurrencyModel>();
    const auto bridge = make_bridge(model);
    const SqpResult sol = solve_fixture_cold(*model);

    IpmPolishData polish = fixture_polish(sol);
    polish.mu_ = 1e-4;
    ASSERT_LT(polish.mu_, SqpOptions{}.ipqp.ipqp_init_mu) << "the premise: UNDER the ceiling";

    WarmStartData data = core_payload(sol, *bridge);
    data.primal_(0) -= 1e-6;
    data.primal_(2) += 1.5e-6;
    data.extensions_.push_back(polish_extension(polish));

    SqpSolver driver{ipm_currency_options()};
    const SqpResult r = driver.solve(*bridge, data.primal_, data);
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    ASSERT_GT(r.counters.ipqp.ipqp_symbolic_analyses, 0) << "or the adoption pin is vacuous";
    EXPECT_GT(r.counters.ipqp.ipqp_mu_adopted, 0);
}
