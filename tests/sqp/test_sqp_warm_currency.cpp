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
//   * the interior-point -> SQP composition end to end, which as of this commit
//     is BLOCKED CROSS-ENGINE by a stamp defect the last test in this file
//     documents and pins.
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
using hven::solvers::IpmPolishData;
using hven::solvers::kIpmPolishTag;
using hven::solvers::ModelStructureKey;
using hven::solvers::NlpModel;
using hven::solvers::NlpModelAggregate;
using hven::solvers::NLPProblem;
using hven::solvers::NlpProblemModel;
using hven::solvers::NLPSolver;
using hven::solvers::serialize_ipm_polish;
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
    data.structure_key_ = bridge.model_structure_key();
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

    EXPECT_TRUE(warm.structure_key_ == bridge->model_structure_key());
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

// SAME SIZES, DIFFERENT STRUCTURE. The size check cannot see this one: only the
// stamp can, and only at solve entry, against the problem that call binds.
TEST(SqpWarmCurrency, SolveEntryRefusesAStagedValueUnderAnotherStampNamingBothDigests) {
    const auto original = std::make_shared<CurrencyModel>(/*lower_x0_finite=*/true);
    const auto original_bridge = make_bridge(original);
    const auto rekeyed = std::make_shared<CurrencyModel>(/*lower_x0_finite=*/false);
    const auto rekeyed_bridge = make_bridge(rekeyed);

    // The two problems are the same shape and have the same solution; only the
    // BOUND STRUCTURE differs, which is a conjunct of the key.
    ASSERT_EQ(original->n(), rekeyed->n());
    ASSERT_FALSE(original_bridge->model_structure_key() == rekeyed_bridge->model_structure_key());

    const SqpSolution sol = solve_fixture_cold(*original);
    SqpDriver driver{SqpOptions{}};
    ASSERT_NO_THROW(driver.stage_warm_start(core_payload(sol, *original_bridge)));

    try {
        (void)driver.solve(*rekeyed_bridge, rekeyed->start_point());
        FAIL() << "a payload under another declared structure must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(
            message.find(fmt::format("{:#x}", original_bridge->model_structure_key().digest())),
            std::string::npos)
            << "the refusal must name the payload's own key: " << message;
        EXPECT_NE(
            message.find(fmt::format("{:#x}", rekeyed_bridge->model_structure_key().digest())),
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

    // AND THE VALUE IS STILL STAGED. The caller's fix is to drop one source and
    // call again; nothing was judged and nothing was spent.
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
    const WarmStart crossover =
        to_sqp_warm_start(data, model->lower(), model->upper(), bridge->model_structure_key());
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
// value, and an SQP solve that finishes from it.
//
// AND THE DEFECT IT WALKS INTO, pinned here rather than left to be discovered.
// The two engines lay the SAME (row, column) claim SET for one declared model
// in a DIFFERENT ORDER -- the interior-point program lays the constraint
// Jacobian claims before the Hessian's, NlpModelAggregate lays the Hessian's
// first -- and claim_stream_digest is ORDER-SENSITIVE by design
// (model/structure_identity.h says so outright). So the CLAIM conjunct of the
// key never agrees across the two engines, for any problem, while the BOUND
// conjunct always does. A payload exported by one engine is therefore refused
// by the other's stamp check, and the refusal is correct given the key it is
// comparing: the defect is in the key's cross-engine meaning, not in either
// engine's check.
//
// This test pins BOTH halves: the refusal as it stands today (so a fix cannot
// land silently), and the rest of the composition -- the export's blocks, its
// polish extension, and the SQP solve that consumes them -- through a payload
// re-stamped with the destination bridge's own key. Everything except the
// digest comparison is exercised end to end.
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

    // HALF ONE -- the defect. Same declared model, same bound conjunct, and yet
    // the keys differ, because the claim streams are ordered differently.
    const ModelStructureKey ipm_key = ipm.nlp_->model_structure_key();
    const ModelStructureKey sqp_key = bridge->model_structure_key();
    EXPECT_EQ(ipm_key.bound_digest_, sqp_key.bound_digest_)
        << "the bound conjunct is a property of the declaration and does agree";
    EXPECT_NE(ipm_key.claim_digest_, sqp_key.claim_digest_)
        << "THE DEFECT: if this now passes, the cross-engine claim-order "
           "mismatch has been fixed -- drop the re-stamp below and stage "
           "`exported` directly, and tell the M5 ledger";
    {
        SqpDriver refusing{SqpOptions{}};
        refusing.stage_warm_start(exported);
        EXPECT_THROW((void)refusing.solve(*bridge, model->start_point()), std::invalid_argument);
    }

    // HALF TWO -- the rest of the composition, on a payload re-stamped for the
    // destination. Everything the crossover actually consumes (the point, the
    // duals, the polish pair, the inequality values) is the interior-point
    // engine's own export, unmodified.
    WarmStartData staged = exported;
    staged.structure_key_ = sqp_key;

    SqpDriver sqp{SqpOptions{}};
    sqp.stage_warm_start(staged);
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

    // And the SQP engine can hand the result on again, stamped for ITS bridge.
    const WarmStartData reexported = sqp.export_warm_start();
    EXPECT_TRUE(reexported.structure_key_ == sqp_key);
    EXPECT_EQ(reexported.primal_, out.x);
}
