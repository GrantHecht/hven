// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M6 W5 T8.3: options as values on both engines.
//
// What this file pins:
//   * validate(const IpmOptions &) refuses everything
//     InteriorPointSolver::Settings::validate() refused -- one representative
//     per refusal class, enumerated from the old body.
//   * set_options() is TRANSACTIONAL on the interior-point engine: a rejected
//     replacement leaves the previous value in force and the solver usable.
//   * set_options() during a solve throws std::logic_error, leaves options()
//     unchanged, and clears the guard so a later solve still works.
//   * ipm_preset() returns a FULL validated value; an unknown name is refused.
//   * The ONE rule for the twelve BACKEND-CONFIGURATION fields, which are read
//     once when the program is attached: changing one on an attached solver is
//     refused by name rather than silently ignored.
//
// The SQP half of the same surface is pinned below it, and the hot-handle
// fingerprint lives in tests/sqp/test_warm_start.cpp beside the kHot chain it
// belongs to.

#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "hven/detail/drivers/interior_point_solver_presets.h"
#include "hven/drivers/common_options.h"
#include "hven/drivers/interior_point_solver.h"
#include "hven/drivers/ipm_solver_types.h"
#include "hven/drivers/sqp_driver.h"
#include "hven/drivers/sqp_types.h"
#include "hven/model/nlp_solver.h"
#include "hven/qp/qp_types.h"

#include "sqp/support/hs_problems.h"

namespace {

using hven::solvers::AcceptanceStrategies;
using hven::solvers::BarrierGovernors;
using hven::solvers::CommonOptions;
using hven::solvers::FixedVariableTreatments;
using hven::solvers::InteriorPointSolver;
using hven::solvers::IpmOptions;

constexpr double kInf = std::numeric_limits<double>::infinity();

// The canonical HS071, copied from tests/interior/test_nlp_solver.cpp:33 -- the
// live solves below need a program and this suite links no engine fixtures.
struct Hs071Problem final : hven::solvers::NLPProblem {
    int num_vars() const override { return 4; }
    int num_cons() const override { return 2; }
    int num_jac_nonzeros() const override { return 8; }
    int num_hess_nonzeros() const override { return 10; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << 1.0, 1.0, 1.0, 1.0;
        xu << 5.0, 5.0, 5.0, 5.0;
        gl << 25.0, 40.0;
        gu << kInf, 40.0;
    }
    void eval_f(hven::ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = x[0] * x[3] * (x[0] + x[1] + x[2]) + x[2];
    }
    void eval_grad_f(hven::ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[3] * (2.0 * x[0] + x[1] + x[2]);
        g[1] = x[0] * x[3];
        g[2] = x[0] * x[3] + 1.0;
        g[3] = x[0] * (x[0] + x[1] + x[2]);
    }
    void eval_g(hven::ConstEigenRef<Eigen::VectorXd> x,
                Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] * x[1] * x[2] * x[3];
        g[1] = x[0] * x[0] + x[1] * x[1] + x[2] * x[2] + x[3] * x[3];
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0, 0, 0, 1, 1, 1, 1;
        c << 0, 1, 2, 3, 0, 1, 2, 3;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 1, 2, 2, 2, 3, 3, 3, 3;
        c << 0, 0, 1, 0, 1, 2, 0, 1, 2, 3;
    }
    void eval_jac(hven::ConstEigenRef<Eigen::VectorXd> x,
                  Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = x[1] * x[2] * x[3];
        v[1] = x[0] * x[2] * x[3];
        v[2] = x[0] * x[1] * x[3];
        v[3] = x[0] * x[1] * x[2];
        v[4] = 2.0 * x[0];
        v[5] = 2.0 * x[1];
        v[6] = 2.0 * x[2];
        v[7] = 2.0 * x[3];
    }
    void eval_hess(hven::ConstEigenRef<Eigen::VectorXd> x, double obj_factor,
                   hven::ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        const double x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3];
        v[0] = obj_factor * 2 * x3 + lambda[1] * 2;
        v[1] = obj_factor * x3 + lambda[0] * x2 * x3;
        v[2] = lambda[1] * 2;
        v[3] = obj_factor * x3 + lambda[0] * x1 * x3;
        v[4] = lambda[0] * x0 * x3;
        v[5] = lambda[1] * 2;
        v[6] = obj_factor * (2 * x0 + x1 + x2) + lambda[0] * x1 * x2;
        v[7] = obj_factor * x0 + lambda[0] * x0 * x2;
        v[8] = obj_factor * x0 + lambda[0] * x0 * x1;
        v[9] = lambda[1] * 2;
    }
    std::string name() const override { return "Hs071Problem"; }
};

Eigen::VectorXd hs071_start() {
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    return x0;
}

// Silent, and otherwise default.
IpmOptions quiet() {
    IpmOptions o;
    o.common.print_level = 10;
    return o;
}

// A value validate() rejects, whichever field the caller is not looking at.
void expect_refused(const IpmOptions &o, const char *field) {
    try {
        hven::solvers::validate(o);
        FAIL() << "validate() accepted an options value it must refuse (" << field << ")";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find(field), std::string::npos)
            << "the refusal names the wrong field: " << e.what();
    }
}

} // namespace

// ---------------------------------------------------------------------------
// validate(const IpmOptions &): one representative per refusal class of the old
// Settings::validate() body, in that body's own order.
// ---------------------------------------------------------------------------

TEST(Options, IpmValidateRefusesWhatSettingsValidateRefused) {
    {
        // pos_int
        IpmOptions o;
        o.max_iters = 0;
        expect_refused(o, "max_iters");
    }
    {
        // the non-negative family (max_refac / max_ls_iters / max_soc /
        // ls_extended_iters / max_feas_rest share one condition and message)
        IpmOptions o;
        o.max_feas_rest = -1;
        expect_refused(o, "max_feas_rest");
    }
    {
        // combination guard 1: funnel/filter above an adaptive-only governor
        IpmOptions o;
        o.acceptance_strategy = AcceptanceStrategies::funnel;
        o.barrier_governor = BarrierGovernors::classic_adaptive;
        o.never_monotone = false;
        expect_refused(o, "acceptance_strategy");
    }
    {
        // combination guard 2: never_monotone against the monitored governor
        IpmOptions o;
        o.never_monotone = true;
        o.barrier_governor = BarrierGovernors::monitored;
        expect_refused(o, "never_monotone");
    }
    {
        // pos_finite
        IpmOptions o;
        o.kkt_tol = std::numeric_limits<double>::quiet_NaN();
        expect_refused(o, "kkt_tol");
    }
    {
        // cross-field: convergence tol above its acceptable tol
        IpmOptions o;
        o.acc_econ_tol = o.econ_tol / 10.0;
        expect_refused(o, "econ_tol");
    }
    {
        // cross-field: acceptable tol above its divergence tol
        IpmOptions o;
        o.div_kkt_tol = o.acc_kkt_tol / 10.0;
        expect_refused(o, "acc_kkt_tol");
    }
    {
        // barrier ordering: min_mu above max_mu
        IpmOptions o;
        o.min_mu = 10.0 * o.max_mu;
        expect_refused(o, "min_mu");
    }
    {
        // barrier ordering: init_mu outside [min_mu, max_mu]
        IpmOptions o;
        o.init_mu = 10.0 * o.max_mu;
        expect_refused(o, "init_mu");
    }
    {
        // in_open_unit
        IpmOptions o;
        o.bound_fraction = 1.0;
        expect_refused(o, "bound_fraction");
    }
    {
        // greater_than
        IpmOptions o;
        o.alpha_red = 1.0;
        expect_refused(o, "alpha_red");
    }
    {
        // in_open_interval
        IpmOptions o;
        o.bound_interval_push = 0.5;
        expect_refused(o, "bound_interval_push");
    }
    {
        // in_closed_interval
        IpmOptions o;
        o.bound_relax_factor = 2.0 * hven::solvers::kMaxBoundRelaxFactor;
        expect_refused(o, "bound_relax_factor");
    }
    {
        // check_fixed_variable_treatment (a closed-set enum still reaches the
        // struct as a plain field)
        IpmOptions o;
        o.fixed_variable_treatment = static_cast<FixedVariableTreatments>(42);
        expect_refused(o, "fixed_variable_treatment");
    }
    {
        // the QP integer family: non-negative
        IpmOptions o;
        o.qp_ref_steps = -1;
        expect_refused(o, "qp_ref_steps");
    }
    {
        // the QP integer family: exactly 0 or 1
        IpmOptions o;
        o.qp_matching = 2;
        expect_refused(o, "qp_matching");
    }
    {
        // objective scale
        IpmOptions o;
        o.obj_scale = -1.0;
        expect_refused(o, "obj_scale");
    }
    {
        // the two fields that MOVED into CommonOptions keep the conditions
        // qp_threads_ and print_level_ faced.
        IpmOptions o;
        o.common.threads = 0;
        expect_refused(o, "common.threads");
    }
    {
        IpmOptions o;
        o.common.print_level = -1;
        expect_refused(o, "common.print_level");
    }
    // And the shipped default is accepted.
    EXPECT_NO_THROW(hven::solvers::validate(IpmOptions{}));
}

// The defaults each engine's `common` carries. The interior-point engine keeps
// the values Settings::qp_threads_ and Settings::print_level_ had; the struct's
// own defaults (0 / 3) are the SQP engine's, pinned beside SqpOptions.
TEST(Options, CommonDefaultsAreEachEnginesOwn) {
    EXPECT_EQ(CommonOptions{}.threads, 0);
    EXPECT_EQ(CommonOptions{}.print_level, 3);
    EXPECT_EQ(CommonOptions{}.start_level, hven::solvers::StartLevel::kWarm);

    EXPECT_EQ(IpmOptions{}.common.threads, HVEN_DEFAULT_QP_THREADS);
    EXPECT_EQ(IpmOptions{}.common.print_level, 0);
    EXPECT_EQ(IpmOptions{}.common.start_level, hven::solvers::StartLevel::kWarm);
}

// ---------------------------------------------------------------------------
// set_options() on the interior-point engine
// ---------------------------------------------------------------------------

TEST(Options, IpmSetOptionsIsTransactional) {
    InteriorPointSolver solver(quiet());
    const int good_iters = 123;
    {
        IpmOptions o = solver.options();
        o.max_iters = good_iters;
        solver.set_options(std::move(o));
    }
    ASSERT_EQ(solver.options().max_iters, good_iters);

    IpmOptions bad = solver.options();
    bad.max_iters = 0;
    bad.kkt_tol = 1.0e-9;
    EXPECT_THROW(solver.set_options(bad), std::invalid_argument);

    // Neither field moved: the value is taken whole or not at all.
    EXPECT_EQ(solver.options().max_iters, good_iters);
    EXPECT_EQ(solver.options().kkt_tol, IpmOptions{}.kkt_tol);
}

// The twelve BACKEND-CONFIGURATION fields are read exactly once, inside
// set_qp_params(), which runs when the program is attached. Changing one on an
// attached solver would have been silently inert; it is refused by name.
TEST(Options, IpmSetOptionsRefusesABackendFieldOnceAProgramIsAttached) {
    hven::solvers::NLPSolver solver(std::make_shared<Hs071Problem>());
    {
        IpmOptions o = solver.optimizer_->options();
        o.common.print_level = 10;
        solver.optimizer_->set_options(std::move(o));
    }
    // Not attached yet (NLPSolver transcribes lazily), so the field moves.
    {
        IpmOptions o = solver.optimizer_->options();
        o.qp_ref_steps = 1;
        EXPECT_NO_THROW(solver.optimizer_->set_options(std::move(o)));
    }
    solver.transcribe();

    IpmOptions changed = solver.optimizer_->options();
    changed.qp_ref_steps = 2;
    try {
        solver.optimizer_->set_options(changed);
        FAIL() << "a backend-configuration change on an attached solver must be refused";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("qp_ref_steps"), std::string::npos);
    }
    EXPECT_EQ(solver.optimizer_->options().qp_ref_steps, 1);

    // Everything else still takes effect on the next solve while attached --
    // fixed_variable_treatment included, which run_phase_sequence() re-applies
    // through configure_variable_treatment() at every entry.
    IpmOptions ok = solver.optimizer_->options();
    ok.fixed_variable_treatment = FixedVariableTreatments::MakeConstraint;
    ok.max_iters = 77;
    EXPECT_NO_THROW(solver.optimizer_->set_options(std::move(ok)));
    EXPECT_EQ(solver.optimizer_->options().fixed_variable_treatment,
              FixedVariableTreatments::MakeConstraint);
    EXPECT_EQ(solver.optimizer_->options().max_iters, 77);
}

// A replacement from inside the solve is a logic_error, the options do not
// move, and the guard clears on the unwind so the next solve still runs.
TEST(Options, IpmSetOptionsDuringASolveThrowsLogicError) {
    hven::solvers::NLPSolver solver(std::make_shared<Hs071Problem>());
    {
        IpmOptions o = solver.optimizer_->options();
        o.common.print_level = 10;
        solver.optimizer_->set_options(std::move(o));
    }
    const IpmOptions before = solver.optimizer_->options();

    int attempts = 0;
    bool saw_logic_error = false;
    InteriorPointSolver *engine = solver.optimizer_.get();
    engine->set_late_callback([&](const hven::solvers::IterateInfo &,
                                  hven::ConstEigenRef<Eigen::VectorXd>,
                                  hven::ConstEigenRef<Eigen::VectorXd>) {
        ++attempts;
        IpmOptions o = engine->options();
        o.max_iters = 3;
        try {
            engine->set_options(std::move(o));
        } catch (const std::logic_error &) {
            saw_logic_error = true;
        }
        return 0;
    });

    ASSERT_EQ(solver.optimize(hs071_start()), hven::ConvergenceFlags::CONVERGED);
    ASSERT_GT(attempts, 0);
    EXPECT_TRUE(saw_logic_error);
    EXPECT_EQ(solver.optimizer_->options().max_iters, before.max_iters);

    // The guard cleared on the way out: a replacement between calls works, and
    // so does the next solve.
    IpmOptions after = solver.optimizer_->options();
    after.max_iters = 400;
    EXPECT_NO_THROW(solver.optimizer_->set_options(std::move(after)));
    EXPECT_EQ(solver.optimizer_->options().max_iters, 400);
    engine->disable_late_callback();
    EXPECT_EQ(solver.optimize(hs071_start()), hven::ConvergenceFlags::CONVERGED);
}

// ---------------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------------

TEST(Options, PresetsReturnFullValues) {
    // Every shipped preset name produces a value validate() accepts, and the
    // value is FULL -- every field outside the preset's nine is the default.
    for (const auto &entry : hven::solvers::kInteriorPointSolverPresets) {
        const IpmOptions o = hven::solvers::ipm_preset(entry.name_);
        EXPECT_NO_THROW(hven::solvers::validate(o)) << "preset " << entry.name_;
        EXPECT_EQ(o.acceptance_strategy, entry.fields_.acceptance_strategy_) << entry.name_;
        EXPECT_EQ(o.merit_penalty_rule, entry.fields_.merit_penalty_rule_) << entry.name_;
        EXPECT_EQ(o.barrier_governor, entry.fields_.barrier_governor_) << entry.name_;
        EXPECT_EQ(o.never_monotone, entry.fields_.never_monotone_) << entry.name_;
        EXPECT_EQ(o.restoration_mode, entry.fields_.restoration_mode_) << entry.name_;
        EXPECT_EQ(o.inertia_mode, entry.fields_.inertia_mode_) << entry.name_;
        EXPECT_EQ(o.max_soc, entry.fields_.max_soc_) << entry.name_;
        EXPECT_EQ(o.ls_extended_iters, entry.fields_.ls_extended_iters_) << entry.name_;
        EXPECT_EQ(o.watchdog, entry.fields_.watchdog_) << entry.name_;
        // Outside the nine, a default value.
        EXPECT_EQ(o.max_iters, IpmOptions{}.max_iters) << entry.name_;
        EXPECT_EQ(o.common.print_level, IpmOptions{}.common.print_level) << entry.name_;
    }
}

TEST(Options, AnUnknownIpmPresetIsRefusedAndListsTheValidNames) {
    try {
        hven::solvers::ipm_preset("no-such-preset");
        FAIL() << "an unknown preset name must be refused";
    } catch (const std::invalid_argument &e) {
        const std::string what(e.what());
        EXPECT_NE(what.find("no-such-preset"), std::string::npos);
        EXPECT_NE(what.find(hven::solvers::kInteriorPointSolverPresets[0].name_),
                  std::string::npos);
    }
}

// ---------------------------------------------------------------------------
// The SQP engine's half of the same surface
// ---------------------------------------------------------------------------

namespace {

using hven::solvers::SqpDriver;
using hven::solvers::SqpOptions;

// Whatever the driver reads at construction, at the SQP engine's own defaults.
SqpOptions sqp_default() { return SqpOptions{}; }

} // namespace

// The SQP engine's `common` defaults are the struct's own -- 0 threads (leave
// the backend alone) and print_level 3 (silent) -- and neither is read in T8.3.
// `SqpOptions::start_level` is still the field the driver caps a warm start
// with; `common.start_level` is carried beside it until T8.10 folds the two.
TEST(Options, SqpCommonDefaultsAreTheStructsOwn) {
    const SqpOptions o;
    EXPECT_EQ(o.common.threads, 0);
    EXPECT_EQ(o.common.print_level, 3);
    EXPECT_EQ(o.common.start_level, o.start_level);
}

TEST(Options, SqpValidateRefusesTheCommonFieldsAndWhatItAlwaysRefused) {
    {
        SqpOptions o;
        o.max_iter = -1;
        EXPECT_THROW(hven::solvers::validate(o), std::invalid_argument);
        // The pre-T8.3 name is a forwarder onto the same body.
        EXPECT_THROW(hven::solvers::validate_sqp_options(o), std::invalid_argument);
    }
    {
        SqpOptions o;
        o.common.threads = -1;
        try {
            hven::solvers::validate(o);
            FAIL() << "a negative thread count must be refused";
        } catch (const std::invalid_argument &e) {
            EXPECT_NE(std::string(e.what()).find("common.threads"), std::string::npos);
        }
    }
    {
        SqpOptions o;
        o.common.print_level = -1;
        try {
            hven::solvers::validate(o);
            FAIL() << "a negative print level must be refused";
        } catch (const std::invalid_argument &e) {
            EXPECT_NE(std::string(e.what()).find("common.print_level"), std::string::npos);
        }
    }
    EXPECT_NO_THROW(hven::solvers::validate(SqpOptions{}));
}

TEST(Options, SqpPresetsReturnFullValues) {
    const SqpOptions d = hven::solvers::sqp_preset("default");
    EXPECT_NO_THROW(hven::solvers::validate(d));
    EXPECT_EQ(d.max_iter, SqpOptions{}.max_iter);
    EXPECT_EQ(d.kkt_tol, SqpOptions{}.kkt_tol);
    EXPECT_EQ(d.common.print_level, SqpOptions{}.common.print_level);
    try {
        hven::solvers::sqp_preset("no-such-preset");
        FAIL() << "an unknown preset name must be refused";
    } catch (const std::invalid_argument &e) {
        const std::string what(e.what());
        EXPECT_NE(what.find("no-such-preset"), std::string::npos);
        EXPECT_NE(what.find("default"), std::string::npos);
    }
}

TEST(Options, SqpSetOptionsIsTransactional) {
    SqpDriver d(sqp_default());
    {
        SqpOptions o = d.options();
        o.max_iter = 42;
        d.set_options(std::move(o));
    }
    ASSERT_EQ(d.options().max_iter, 42);

    SqpOptions bad = d.options();
    bad.max_iter = -1;
    bad.kkt_tol = 1.0e-9;
    EXPECT_THROW(d.set_options(bad), std::invalid_argument);
    EXPECT_EQ(d.options().max_iter, 42);
    EXPECT_EQ(d.options().kkt_tol, SqpOptions{}.kkt_tol);

    // The old engines are still in force too: the driver solves.
    const auto p = hven::solvers::test_support::make_hs(7);
    EXPECT_NO_THROW(d.solve(*p.model));
}

// The replacement is refused while a solve is in flight -- reached through
// SqpOptions::make_strategy, which the driver calls once per solve from inside
// solve_impl. The options do not move and the guard clears on the way out.
TEST(Options, SqpSetOptionsDuringASolveThrowsLogicError) {
    SqpDriver d(sqp_default());
    bool attempted = false;
    bool refused = false;
    {
        SqpOptions o = d.options();
        o.make_strategy = [&]() -> std::unique_ptr<hven::solvers::GlobalizationStrategy> {
            attempted = true;
            SqpOptions replacement = d.options();
            replacement.max_iter = 5;
            try {
                d.set_options(std::move(replacement));
            } catch (const std::logic_error &) {
                refused = true;
            }
            return std::make_unique<hven::solvers::FunnelStrategy>();
        };
        d.set_options(std::move(o));
    }
    const auto p = hven::solvers::test_support::make_hs(7);
    const auto sol = d.solve(*p.model);
    (void)sol;
    ASSERT_TRUE(attempted) << "the strategy factory never ran";
    EXPECT_TRUE(refused) << "a replacement under an in-flight solve must be a logic_error";
    EXPECT_EQ(d.options().max_iter, SqpOptions{}.max_iter);

    // The guard cleared: a replacement between calls goes through.
    SqpOptions after = d.options();
    after.make_strategy = {};
    after.max_iter = 11;
    EXPECT_NO_THROW(d.set_options(std::move(after)));
    EXPECT_EQ(d.options().max_iter, 11);
}

// ---------------------------------------------------------------------------
// The hot-handle options fingerprint (M6 W5 T8.3)
// ---------------------------------------------------------------------------

// EVERY field of QpOptions, plus the thread count, must move the hash. A field
// left out is a hole in the hot-handle reuse gate: an engine built under one
// value would adopt a K0 factorized under another. This test flips each of the
// nine fields ALONE from a default value and requires the fingerprint to move;
// add a field to QpOptions and this test is where the omission surfaces.
TEST(Options, TheOptionsFingerprintCoversEveryQpOptionsField) {
    using hven::solvers::options_fingerprint;
    using hven::solvers::QpOptions;
    using hven::solvers::WorkingSetLinearAlgebra;

    const QpOptions base;
    const std::uint64_t h0 = options_fingerprint(base, 0);

    // Stable: the same value hashes the same, every time and from an
    // independently constructed struct.
    EXPECT_EQ(options_fingerprint(base, 0), h0);
    EXPECT_EQ(options_fingerprint(QpOptions{}, 0), h0);

    const auto moved = [&](auto edit) {
        QpOptions o = base;
        edit(o);
        return options_fingerprint(o, 0) != h0;
    };
    EXPECT_TRUE(moved([](QpOptions &o) { o.primal_delta *= 2.0; })) << "primal_delta";
    EXPECT_TRUE(moved([](QpOptions &o) { o.dual_mu *= 2.0; })) << "dual_mu";
    EXPECT_TRUE(moved([](QpOptions &o) { o.feas_tol *= 2.0; })) << "feas_tol";
    EXPECT_TRUE(moved([](QpOptions &o) { o.opt_tol *= 2.0; })) << "opt_tol";
    EXPECT_TRUE(moved([](QpOptions &o) { o.max_iter += 1; })) << "max_iter";
    EXPECT_TRUE(moved([](QpOptions &o) { o.schur_cap += 1; })) << "schur_cap";
    EXPECT_TRUE(moved([](QpOptions &o) { o.schur_cond_max *= 2.0; })) << "schur_cond_max";
    EXPECT_TRUE(moved([](QpOptions &o) { o.ws_algebra = WorkingSetLinearAlgebra::kRefactorize; }))
        << "ws_algebra";
    EXPECT_TRUE(moved([](QpOptions &o) { o.tr_radius = 1.0; })) << "tr_radius";

    // And the thread count, which is not a QpOptions field but IS part of what a
    // factorization was built under. Carried and not applied by the SQP engine
    // until T8.8; hashed from T8.3 so this pin's meaning does not change then.
    EXPECT_NE(options_fingerprint(base, 1), h0) << "threads";
    EXPECT_NE(options_fingerprint(base, 2), options_fingerprint(base, 1)) << "threads";
}
