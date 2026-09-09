// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/drivers/test_warm_protocol.cpp — THE WARM MATRIX (M6 W5 T8.5).
//
// One protocol, two identities (design 2.4). This file pins the PROTOCOL
// itself, across BOTH engines and both routes, which is why it lives in the
// shared drivers/ suite rather than in either engine's:
//
//   THE PAYLOAD ROUTE -- `solve(model, x0, const WarmStartData &, budget)` on
//   the interior-point solver and on the SQP driver. `WarmStartData` is the
//   declared-space currency: it carries a `DeclarationKey` stamp, it crosses
//   between engines, and it is the value `SolveResult::export_warm_start()`
//   hands back. Its rule is
//
//       IDENTITY MISMATCH REFUSES -- block lengths, then the stamp, both
//       `std::invalid_argument`, on both engines, and on the SQP in EVERY
//       `qp_mode` including kIpm (M5 ruling 4's mode-local cold grade retired).
//       PATTERN MISMATCH DEGRADES -- a payload never saw a model, so it carries
//       structure hash 0 and caps at kSeeded by construction.
//       VALUE DEFECTS DEGRADE -- the SQP's seeded clamp band, the IPM's
//       floor/cap, counted as they always were.
//
//   THE NATIVE ROUTE -- `solve(bridge, x0, const SqpWarmStart &, budget)`, the
//   LABELLED SQP-ONLY entry. It carries no stamp, so nothing on it is ever
//   refused for identity; every defect DEGRADES, exactly as before T8.5.
//
// AND THE MULTIPLIERS-ONLY SEED, the payload form whose `primal_` is EMPTY:
// `x0` is the start, the multipliers travel, a polish extension is IGNORED and
// COUNTED, and an empty `primal_` beside a populated `bound_lmults_` is refused
// at the hand-over.
//
// The engine-specific halves of the currency -- the export's declared-space
// mapping, the polish extension's own codec, the IPM's bound-dual seeding --
// stay in tests/interior/test_ipm_warm_start.cpp and
// tests/sqp/test_sqp_warm_currency.cpp. What is here is only what has to be
// true of BOTH.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>

#include <hven/core/start_level.h>
#include <hven/detail/warmstart/warm_start.h>
#include <hven/drivers/interior_point_solver.h>
#include <hven/drivers/ipm_solver_types.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>
#include <hven/model/nlp_model_aggregate.h>
#include <hven/model/nlp_problem_model.h>
#include <hven/model/nlp_solver.h>
#include <hven/warmstart/ipm_polish_extension.h>
#include <hven/warmstart/seeding.h>
#include <hven/warmstart/sqp_warm_start.h>
#include <hven/warmstart/warm_start_data.h>

#include "support/hs071_problem.h"

namespace {

using hven::solvers::declaration_key;
using hven::solvers::kIpmPolishTag;
using hven::solvers::NlpModelAggregate;
using hven::solvers::NlpProblemModel;
using hven::solvers::NLPSolver;
using hven::solvers::SolveStatus;
using hven::solvers::SqpDriver;
using hven::solvers::SqpOptions;
using hven::solvers::SqpResult;
using hven::solvers::SqpWarmStart;
using hven::solvers::StartLevel;
using hven::solvers::WarmStartData;
using Vec = Eigen::VectorXd;

// A SECOND declared problem, deliberately UNLIKE HS071 in its declared
// dimensions: three variables, one equality row, no inequality rows. Handing
// HS071's payload to this one is the identity mismatch every REFUSES pin below
// is built on -- and it is a mismatch in the block lengths AND in the stamp, so
// the two checks are exercised in the order the engines run them.
struct ThreeVarProblem final : hven::solvers::NLPProblem {
    int num_vars() const override { return 3; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 3; }
    int num_hess_nonzeros() const override { return 3; }

    void bounds(Eigen::Ref<Vec> xl, Eigen::Ref<Vec> xu, Eigen::Ref<Vec> gl,
                Eigen::Ref<Vec> gu) const override {
        xl << -10.0, -10.0, -10.0;
        xu << 10.0, 10.0, 10.0;
        gl << 3.0;
        gu << 3.0;
    }
    void eval_f(hven::ConstEigenRef<Vec> x, double &f) const override { f = 0.5 * x.squaredNorm(); }
    void eval_grad_f(hven::ConstEigenRef<Vec> x, Eigen::Ref<Vec> g) const override { g = x; }
    void eval_g(hven::ConstEigenRef<Vec> x, Eigen::Ref<Vec> g) const override { g[0] = x.sum(); }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0, 0;
        c << 0, 1, 2;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 2;
        c << 0, 1, 2;
    }
    void eval_jac(hven::ConstEigenRef<Vec>, Eigen::Ref<Vec> v) const override { v.setOnes(); }
    void eval_hess(hven::ConstEigenRef<Vec>, double obj_factor, hven::ConstEigenRef<Vec>,
                   Eigen::Ref<Vec> v) const override {
        v.setConstant(obj_factor);
    }
    std::string name() const override { return "WarmProtocolThreeVar"; }
};

Vec hs071_start() {
    Vec x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    return x0;
}

SqpOptions quiet_sqp(hven::solvers::QpMode mode = hven::solvers::QpMode::kWalk) {
    SqpOptions o;
    o.common.print_level = 10;
    o.qp_mode = mode;
    return o;
}

hven::solvers::IpmOptions quiet_ipm(const NLPSolver &solver) {
    hven::solvers::IpmOptions o = solver.optimizer_->options();
    o.common.print_level = 10;
    return o;
}

// One converged interior-point solve of HS071, and the payload it exports.
// Every pin below that needs "a real, well-formed payload for HS071" takes it
// from here, so no test hand-builds a value the engines would never have
// produced.
struct Hs071Export {
    std::shared_ptr<hven::solvers::NLPProblem> problem;
    NLPSolver ipm;
    WarmStartData payload;

    Hs071Export() : problem(std::make_shared<hven_drivers_tests::Hs071Problem>()), ipm(problem) {
        ipm.optimizer_->set_options(quiet_ipm(ipm));
        ipm.transcribe();
        const hven::solvers::IpmResult r = ipm.optimizer_->solve(*ipm.nlp_, hs071_start());
        EXPECT_EQ(r.status, SolveStatus::kOptimal);
        const std::optional<WarmStartData> snapshot = r.export_warm_start();
        EXPECT_TRUE(snapshot.has_value());
        payload = snapshot.value_or(WarmStartData{});
    }
};

// The multipliers-only form of a payload: the two row blocks and the stamp
// kept, the point and the bound prices dropped.
WarmStartData seed_form(const WarmStartData &full) {
    WarmStartData seed;
    seed.eq_lmults_ = full.eq_lmults_;
    seed.iq_lmults_ = full.iq_lmults_;
    seed.structure_key_ = full.structure_key_;
    return seed;
}

// A SQP-side view of one NLPProblem, kept alive with its bridge.
struct SqpView {
    std::shared_ptr<NlpProblemModel> model;
    std::shared_ptr<NlpModelAggregate> bridge;

    explicit SqpView(std::shared_ptr<hven::solvers::NLPProblem> problem)
        : model(std::make_shared<NlpProblemModel>(std::move(problem))),
          bridge(std::make_shared<NlpModelAggregate>(model)) {}
};

} // namespace

// ===========================================================================
// IDENTITY REFUSES -- on both engines, and on the SQP in every qp_mode
// ===========================================================================

TEST(WarmProtocol, PayloadStampMismatchRefusesOnBothEngines) {
    const Hs071Export exported;
    ASSERT_EQ(exported.payload.primal_.size(), 4);

    // --- The interior-point engine ---
    const auto three = std::make_shared<ThreeVarProblem>();
    NLPSolver ipm(three);
    ipm.optimizer_->set_options(quiet_ipm(ipm));
    ipm.transcribe();
    EXPECT_THROW((void)ipm.optimizer_->solve(*ipm.nlp_, Vec::Zero(3), exported.payload),
                 std::invalid_argument);

    // --- The SQP engine, in EVERY mode ---
    //
    // THE POINT OF THE LOOP (M6 W5 T8.5, owner ruling). Before this task kIpm
    // COLD-GRADED exactly this case where kWalk and kSsn threw, so the answer to
    // "is this payload this problem's?" depended on which QP kernel the driver
    // was configured for. It does not any more.
    SqpView view(three);
    for (const hven::solvers::QpMode mode :
         {hven::solvers::QpMode::kWalk, hven::solvers::QpMode::kSsn, hven::solvers::QpMode::kIpm}) {
        SqpDriver driver{quiet_sqp(mode)};
        EXPECT_THROW((void)driver.solve(*view.bridge, view.model->start_point(), exported.payload),
                     std::invalid_argument)
            << "qp_mode ordinal " << static_cast<int>(mode);
    }
}

// The SAME refusal, not merely the same exception type: one identity check, one
// message, whatever the kernel. Stated separately from the pin above because
// "all three throw" would be satisfied by three different diagnostics, and the
// ruling is that there is now ONE.
TEST(WarmProtocol, KIpmModeNoLongerColdGradesAMismatch) {
    const Hs071Export exported;
    const auto three = std::make_shared<ThreeVarProblem>();
    SqpView view(three);

    const auto message_for = [&](hven::solvers::QpMode mode) {
        SqpDriver driver{quiet_sqp(mode)};
        try {
            (void)driver.solve(*view.bridge, view.model->start_point(), exported.payload);
        } catch (const std::invalid_argument &error) {
            return std::string(error.what());
        }
        return std::string{};
    };

    const std::string walk = message_for(hven::solvers::QpMode::kWalk);
    const std::string ssn = message_for(hven::solvers::QpMode::kSsn);
    const std::string ipm = message_for(hven::solvers::QpMode::kIpm);

    ASSERT_FALSE(walk.empty()) << "the kWalk refusal is the reference, and it must fire";
    EXPECT_EQ(walk, ssn);
    EXPECT_EQ(walk, ipm) << "M5 ruling 4's mode-local cold grade is RETIRED: kIpm refuses an "
                            "identity mismatch exactly as kWalk does";
    EXPECT_NE(walk.find("SqpDriver::solve"), std::string::npos) << walk;
}

// ===========================================================================
// THE MULTIPLIERS-ONLY SEED
// ===========================================================================

TEST(WarmProtocol, MultipliersOnlySeedResolvesSeededOnSqp) {
    const Hs071Export exported;
    const WarmStartData seed = seed_form(exported.payload);
    ASSERT_EQ(seed.primal_.size(), 0);
    ASSERT_EQ(seed.bound_lmults_.size(), 0);
    ASSERT_EQ(seed.eq_lmults_.size(), 1);
    ASSERT_EQ(seed.iq_lmults_.size(), 1);

    SqpView view(exported.problem);
    SqpDriver driver{quiet_sqp()};
    const Vec x0 = hs071_start();
    const SqpResult out = driver.solve(*view.bridge, x0, seed);

    // THE BEHAVIOUR CHANGE (design 2.7 item (5)): before T8.5 an empty `primal_`
    // failed prepare_solve's plausibility gate, the object resolved kCold and
    // the multipliers were DROPPED. It resolves kSeeded now.
    EXPECT_EQ(out.status, SolveStatus::kOptimal);
    EXPECT_EQ(out.counters.start_level_used, StartLevel::kSeeded);
    EXPECT_EQ(out.counters.n_seeded, 1);

    // AND x STARTS AT x0, which is what "multipliers only" means: the payload
    // named no point, so the call's own start point is the start. The first
    // measured iterate is evaluate_kkt at the ingested point, before any
    // subproblem is built, so its objective is f(x0).
    ASSERT_FALSE(out.history.empty());
    EXPECT_DOUBLE_EQ(out.history.front().f, view.model->eval_f(x0))
        << "the seed form starts at x0, not at the exporter's point";
}

TEST(WarmProtocol, MultipliersOnlySeedIgnoresAPolishExtension) {
    const Hs071Export exported;
    // HS071 declares a box on every variable, so the interior-point export
    // really does carry the polish extension -- without which this pin would
    // pass vacuously.
    ASSERT_NE(hven::solvers::find_ipm_polish(exported.payload), nullptr);

    WarmStartData seed = seed_form(exported.payload);
    seed.extensions_ = exported.payload.extensions_;
    ASSERT_EQ(seed.extensions_.size(), 1u);
    ASSERT_EQ(seed.extensions_[0].tag_, std::string(kIpmPolishTag));

    // --- The SQP: the extension is dropped and COUNTED ---
    //
    // Its bound duals and inequality values are stated at the EXPORTER's point,
    // and this solve stands at x0; feeding them to the crossover's activity rule
    // would attribute activity nothing measured here.
    {
        SqpView view(exported.problem);
        SqpDriver driver{quiet_sqp()};
        const SqpResult out = driver.solve(*view.bridge, hs071_start(), seed);
        EXPECT_EQ(out.status, SolveStatus::kOptimal);
        EXPECT_EQ(out.counters.start_level_used, StartLevel::kSeeded);
        EXPECT_EQ(out.counters.polish_ignored, 1);
    }

    // NON-VACUITY: the same seed WITHOUT the extension counts zero, so the 1
    // above is about the extension and not about the seed form.
    {
        SqpView view(exported.problem);
        SqpDriver driver{quiet_sqp()};
        const SqpResult out =
            driver.solve(*view.bridge, hs071_start(), seed_form(exported.payload));
        EXPECT_EQ(out.counters.polish_ignored, 0);
    }

    // And a FULL payload's extension IS consumed -- the counter marks the drop,
    // not the presence.
    {
        SqpView view(exported.problem);
        SqpDriver driver{quiet_sqp()};
        const SqpResult out = driver.solve(*view.bridge, hs071_start(), exported.payload);
        EXPECT_EQ(out.counters.polish_ignored, 0);
    }

    // --- The interior-point engine: the same drop, its own counter ---
    //
    // The seed's multipliers go through the floor/cap install site, and NO `z`
    // is installed: the core's signed bound block does not invert into the
    // (z_lower, z_upper) pair the barrier holds, and the pair that would have
    // travelled in the extension describes the exporter's point.
    {
        NLPSolver ipm(exported.problem);
        ipm.optimizer_->set_options(quiet_ipm(ipm));
        ipm.transcribe();
        const hven::solvers::IpmResult r = ipm.optimizer_->solve(*ipm.nlp_, hs071_start(), seed);
        EXPECT_EQ(r.status, SolveStatus::kOptimal);
        EXPECT_EQ(r.polish_ignored, 1);
        EXPECT_EQ(r.payload_ignored, 0) << "the seed was applied, not ignored";
    }
    {
        NLPSolver ipm(exported.problem);
        ipm.optimizer_->set_options(quiet_ipm(ipm));
        ipm.transcribe();
        const hven::solvers::IpmResult r =
            ipm.optimizer_->solve(*ipm.nlp_, hs071_start(), exported.payload);
        EXPECT_EQ(r.polish_ignored, 0) << "a full payload's extension IS consumed";
    }
}

TEST(WarmProtocol, EmptyPrimalWithNonEmptyBoundDualsIsRefusedAtHandOver) {
    const Hs071Export exported;

    WarmStartData half_empty = exported.payload;
    half_empty.primal_.resize(0);
    half_empty.extensions_.clear();
    ASSERT_EQ(half_empty.bound_lmults_.size(), 4);

    // NEITHER FORM. `primal_` and `bound_lmults_` are two readings of ONE space,
    // so they are either both empty (the seed) or one length (a point and its
    // prices); an empty point beside populated bound prices claims prices
    // somewhere the payload does not name. Refused where the caller is still
    // standing, on both engines, before either runs anything.
    {
        SqpView view(exported.problem);
        SqpDriver driver{quiet_sqp()};
        try {
            (void)driver.solve(*view.bridge, hs071_start(), half_empty);
            FAIL() << "an empty primal_ beside populated bound prices must refuse";
        } catch (const std::invalid_argument &error) {
            const std::string message = error.what();
            EXPECT_NE(message.find("primal_"), std::string::npos) << message;
            EXPECT_NE(message.find("bound_lmults_"), std::string::npos) << message;
            EXPECT_NE(message.find("BOTH EMPTY"), std::string::npos) << message;
        }
    }
    {
        NLPSolver ipm(exported.problem);
        ipm.optimizer_->set_options(quiet_ipm(ipm));
        ipm.transcribe();
        try {
            (void)ipm.optimizer_->solve(*ipm.nlp_, hs071_start(), half_empty);
            FAIL() << "an empty primal_ beside populated bound prices must refuse";
        } catch (const std::invalid_argument &error) {
            const std::string message = error.what();
            EXPECT_NE(message.find("primal_"), std::string::npos) << message;
            EXPECT_NE(message.find("BOTH EMPTY"), std::string::npos) << message;
        }
    }

    // AND THE REVERSE HALF, for completeness: a point with no prices beside it
    // is refused by the same rule, in the same place.
    WarmStartData no_prices = exported.payload;
    no_prices.bound_lmults_.resize(0);
    no_prices.extensions_.clear();
    SqpView view(exported.problem);
    SqpDriver driver{quiet_sqp()};
    EXPECT_THROW((void)driver.solve(*view.bridge, hs071_start(), no_prices), std::invalid_argument);
}

// ===========================================================================
// THE NATIVE ROUTE: everything DEGRADES, nothing is refused for identity
// ===========================================================================

TEST(WarmProtocol, NativePatternMismatchStillDegradesToSeeded) {
    const auto problem = std::make_shared<hven_drivers_tests::Hs071Problem>();
    SqpView view(problem);

    SqpDriver producer{quiet_sqp()};
    const SqpResult first = producer.solve(*view.bridge, hs071_start());
    ASSERT_EQ(first.status, SolveStatus::kOptimal);
    ASSERT_NE(first.warm_start.structure_hash, 0u)
        << "a native object from a real solve carries a real hash, or this pin measures nothing";

    // A DELIBERATELY WRONG HASH on an otherwise perfect object, against the very
    // problem it came from: the values are usable, the provenance claim is not.
    // The native route has no stamp to refuse against, so the ladder does the
    // work -- kSeeded, never a throw.
    SqpWarmStart stale = first.warm_start;
    stale.structure_hash = 0x1;
    stale.hot.reset();

    SqpDriver consumer{quiet_sqp()};
    SqpResult out;
    ASSERT_NO_THROW(out = consumer.solve(*view.bridge, hs071_start(), stale));
    EXPECT_EQ(out.status, SolveStatus::kOptimal);
    EXPECT_EQ(out.counters.start_level_used, StartLevel::kSeeded);

    // AND THE MATCHING OBJECT REACHES kWarm, so "kSeeded" above is the
    // degradation and not the ceiling this route always hits.
    SqpWarmStart matching = first.warm_start;
    matching.hot.reset();
    SqpDriver warm_consumer{quiet_sqp()};
    const SqpResult warm = warm_consumer.solve(*view.bridge, hs071_start(), matching);
    EXPECT_EQ(warm.counters.start_level_used, StartLevel::kWarm);
}

TEST(WarmProtocol, NativeNonFiniteResolvesCold) {
    const auto problem = std::make_shared<hven_drivers_tests::Hs071Problem>();
    SqpView view(problem);

    SqpDriver producer{quiet_sqp()};
    const SqpResult first = producer.solve(*view.bridge, hs071_start());
    ASSERT_EQ(first.status, SolveStatus::kOptimal);

    SqpWarmStart poisoned = first.warm_start;
    poisoned.hot.reset();
    poisoned.x(0) = std::numeric_limits<double>::quiet_NaN();

    // NOT A THROW. The native route's contract is that a defect resolves kCold,
    // and it does so with the hash still MATCHING -- which is what makes the
    // finiteness gate, and not the pattern gate, the thing being read.
    SqpDriver consumer{quiet_sqp()};
    SqpResult out;
    ASSERT_NO_THROW(out = consumer.solve(*view.bridge, hs071_start(), poisoned));
    EXPECT_EQ(out.status, SolveStatus::kOptimal);
    EXPECT_EQ(out.counters.start_level_used, StartLevel::kCold);
    EXPECT_EQ(out.counters.n_seeded, 0);
}

// THE VALUE-DEFECT RUNG, straddling the band. Inside it the price is a rounding
// artefact and is clamped and counted; outside it the object is malformed and
// the whole ingest is unwound to a genuine cold solve. Neither is a refusal.
TEST(WarmProtocol, SeededClampBandStillDegradesNotRefuses) {
    const auto problem = std::make_shared<hven_drivers_tests::Hs071Problem>();
    SqpView view(problem);

    SqpDriver producer{quiet_sqp()};
    const SqpResult first = producer.solve(*view.bridge, hs071_start());
    ASSERT_EQ(first.status, SolveStatus::kOptimal);
    ASSERT_GT(first.warm_start.lambda_i.size(), 0);

    const auto run_with_price = [&](double price) {
        SqpWarmStart w = first.warm_start;
        w.hot.reset();
        // Hash 0: the seeded ladder is where the clamp lives, so the object has
        // to resolve kSeeded before the band can be read at all.
        w.structure_hash = 0;
        w.lambda_i(0) = price;
        SqpDriver driver{quiet_sqp()};
        return driver.solve(*view.bridge, hs071_start(), w);
    };

    // JUST OUTSIDE the band: degraded to cold, and the clamp counter reads 0 --
    // the degradation unwinds the ingest completely, counters included.
    const SqpResult outside = run_with_price(-2.0 * hven::solvers::kSeededDualClampTol);
    EXPECT_EQ(outside.status, SolveStatus::kOptimal);
    EXPECT_EQ(outside.counters.start_level_used, StartLevel::kCold);
    EXPECT_EQ(outside.counters.seeded_clamped, 0);

    // JUST INSIDE: clamped to zero, counted, and the object stays kSeeded.
    const SqpResult inside = run_with_price(-0.5 * hven::solvers::kSeededDualClampTol);
    EXPECT_EQ(inside.status, SolveStatus::kOptimal);
    EXPECT_EQ(inside.counters.start_level_used, StartLevel::kSeeded);
    EXPECT_EQ(inside.counters.seeded_clamped, 1);
}

// ===========================================================================
// THE CEILING: common.start_level on the interior-point payload route
// ===========================================================================

// kCold IGNORES THE PAYLOAD AND COUNTS IT. The call is then the solve it would
// have been with no payload at all -- asserted here as bitwise equality of every
// reported number against a genuine no-payload solve, with `payload_ignored` as
// the ONE declared difference between the two results.
//
// The lengths and the stamp are still checked at this rung: a ceiling says what
// of a payload to APPLY, never that a payload describing a different problem is
// acceptable. That half is the second block below.
TEST(WarmProtocol, ColdCeilingIgnoresAndCountsThePayload) {
    const Hs071Export exported;

    const auto solve_at = [&](StartLevel ceiling, const WarmStartData *payload) {
        NLPSolver ipm(exported.problem);
        hven::solvers::IpmOptions o = quiet_ipm(ipm);
        o.common.start_level = ceiling;
        ipm.optimizer_->set_options(std::move(o));
        ipm.transcribe();
        return payload != nullptr ? ipm.optimizer_->solve(*ipm.nlp_, hs071_start(), *payload)
                                  : ipm.optimizer_->solve(*ipm.nlp_, hs071_start());
    };

    const hven::solvers::IpmResult no_payload = solve_at(StartLevel::kCold, nullptr);
    const hven::solvers::IpmResult ignored = solve_at(StartLevel::kCold, &exported.payload);

    ASSERT_EQ(no_payload.status, SolveStatus::kOptimal);
    ASSERT_EQ(ignored.status, SolveStatus::kOptimal);

    // THE COUNT, which is the whole point of not ignoring it silently.
    EXPECT_EQ(ignored.payload_ignored, 1);
    EXPECT_EQ(no_payload.payload_ignored, 0);
    // Not `polish_ignored`: nothing of the payload was READ at this rung, and
    // payload_ignored is this call's answer.
    EXPECT_EQ(ignored.polish_ignored, 0);

    // AND THE SOLVE IS THE SAME SOLVE, bit for bit in every reported number.
    EXPECT_EQ(ignored.iterations, no_payload.iterations);
    ASSERT_EQ(ignored.x.size(), no_payload.x.size());
    for (Eigen::Index i = 0; i < ignored.x.size(); i++) {
        EXPECT_EQ(ignored.x[i], no_payload.x[i]) << "primal entry " << i;
    }
    ASSERT_EQ(ignored.lambda_i.size(), no_payload.lambda_i.size());
    for (Eigen::Index i = 0; i < ignored.lambda_i.size(); i++) {
        EXPECT_EQ(ignored.lambda_i[i], no_payload.lambda_i[i]) << "iq multiplier " << i;
    }
    EXPECT_EQ(ignored.f, no_payload.f);
    EXPECT_EQ(ignored.kkt_inf, no_payload.kkt_inf);

    // IDENTITY IS STILL CHECKED AT THIS RUNG. A foreign payload under a kCold
    // ceiling is refused, not quietly discarded.
    const auto three = std::make_shared<ThreeVarProblem>();
    NLPSolver other(three);
    hven::solvers::IpmOptions o = quiet_ipm(other);
    o.common.start_level = StartLevel::kCold;
    other.optimizer_->set_options(std::move(o));
    other.transcribe();
    EXPECT_THROW((void)other.optimizer_->solve(*other.nlp_, Vec::Zero(3), exported.payload),
                 std::invalid_argument);
}

// The rungs ABOVE kCold, which the pin above needs as its contrast: at kSeeded
// only the multipliers are applied and the polish is dropped and counted; at
// kWarm the whole payload applies; kHot is documented as identical to kWarm,
// this engine having no hot handle to adopt.
TEST(WarmProtocol, TheIpmCeilingHasFourRungs) {
    const Hs071Export exported;
    ASSERT_NE(hven::solvers::find_ipm_polish(exported.payload), nullptr);

    const auto solve_at = [&](StartLevel ceiling) {
        NLPSolver ipm(exported.problem);
        hven::solvers::IpmOptions o = quiet_ipm(ipm);
        o.common.start_level = ceiling;
        ipm.optimizer_->set_options(std::move(o));
        ipm.transcribe();
        return ipm.optimizer_->solve(*ipm.nlp_, hs071_start(), exported.payload);
    };

    const hven::solvers::IpmResult seeded = solve_at(StartLevel::kSeeded);
    EXPECT_EQ(seeded.status, SolveStatus::kOptimal);
    EXPECT_EQ(seeded.payload_ignored, 0);
    EXPECT_EQ(seeded.polish_ignored, 1) << "at kSeeded the point and the polish are dropped";

    const hven::solvers::IpmResult warm = solve_at(StartLevel::kWarm);
    EXPECT_EQ(warm.status, SolveStatus::kOptimal);
    EXPECT_EQ(warm.payload_ignored, 0);
    EXPECT_EQ(warm.polish_ignored, 0) << "at kWarm the whole payload applies";

    const hven::solvers::IpmResult hot = solve_at(StartLevel::kHot);
    EXPECT_EQ(hot.status, SolveStatus::kOptimal);
    EXPECT_EQ(hot.payload_ignored, 0);
    EXPECT_EQ(hot.polish_ignored, 0);

    // kHot IS kWarm on this engine, asserted rather than merely documented:
    // there is no hot handle to adopt, so the two rungs must agree in every
    // reported number.
    EXPECT_EQ(hot.iterations, warm.iterations);
    ASSERT_EQ(hot.x.size(), warm.x.size());
    for (Eigen::Index i = 0; i < hot.x.size(); i++) {
        EXPECT_EQ(hot.x[i], warm.x[i]) << "primal entry " << i;
    }

    // AND THE WHOLE PAYLOAD IS WORTH SOMETHING: kWarm restarts the converged
    // point, so it cannot take more iterations than the rung that throws the
    // point away.
    EXPECT_LE(warm.iterations, seeded.iterations);
}

// ===========================================================================
// THE ROUND TRIP: interior-point -> SQP -> interior-point, through arguments
// ===========================================================================

TEST(WarmProtocol, RoundTripIpmToSqpToIpmStillWorks) {
    const Hs071Export exported;

    // --- IPM -> SQP, the crossover the bench's leg (d) runs ---
    SqpView view(exported.problem);
    SqpDriver sqp{quiet_sqp()};
    const SqpResult crossed = sqp.solve(*view.bridge, hs071_start(), exported.payload);
    EXPECT_EQ(crossed.status, SolveStatus::kOptimal);
    // kSeeded, and it cannot be more: a payload carries structure hash 0 by
    // construction, which is the ladder's own ceiling for this route.
    EXPECT_EQ(crossed.counters.start_level_used, StartLevel::kSeeded);

    // --- SQP -> IPM, back the other way through the same currency ---
    const std::optional<WarmStartData> back = crossed.export_warm_start();
    ASSERT_TRUE(back.has_value());
    EXPECT_TRUE(back->structure_key_ == declaration_key(view.bridge->declaration()))
        << "one declared problem keys the same on both engines";
    // The SQP produces no extensions, so this is the core-only shape.
    EXPECT_TRUE(back->extensions_.empty());

    NLPSolver ipm(exported.problem);
    ipm.optimizer_->set_options(quiet_ipm(ipm));
    ipm.transcribe();
    const hven::solvers::IpmResult returned =
        ipm.optimizer_->solve(*ipm.nlp_, hs071_start(), *back);
    EXPECT_EQ(returned.status, SolveStatus::kOptimal);
    EXPECT_EQ(returned.payload_ignored, 0);
    // No extension came back, so there was none to drop.
    EXPECT_EQ(returned.polish_ignored, 0);

    // And the chain really did go somewhere: all three legs agree on the
    // objective, to the loosest of the tolerances involved.
    EXPECT_NEAR(returned.f, 17.0140173, 1e-5);
    EXPECT_NEAR(crossed.f, 17.0140173, 1e-5);
}
