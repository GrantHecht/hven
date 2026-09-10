// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/drivers/test_threads.cpp -- `common.threads` REACHES EVERY SQP FACTOR
// PATH, AND EVERY BACKEND CALL RESTORES THE CALLER'S SETTING (M6 W5 T8.8).
//
// `CommonOptions::threads` is shared surface, so it is pinned here rather than
// in either engine's suite. Its SQP default is 0 -- "leave the backend's own
// default alone" -- which is why the U0 replay at 0 is an identity BY
// CONSTRUCTION rather than a measurement: no thread scope is engaged anywhere,
// and the process-wide MKL_NUM_THREADS pin stays the reproducibility mechanism.
//
// WHAT A BOUNDARY CAN READ, AND WHAT IT CANNOT.
//
//   READ HERE. The three engines' persistent factors, through the accessors
//   this task adds -- `QpEngine::num_threads()` (the LIVE K0 factor, not the
//   carried int), `SsnEngine::num_threads()` (the live factor), and
//   `IpqpEngine::num_threads()` (the live KktFactorization session). The
//   DRIVER's own K0, through the hot handle its solve emits. And MKL's
//   thread-local state before and after a whole solve.
//
//   NOT READ HERE. The walk's per-solve, EQP-refine and verdict-refine
//   TEMPORARIES. They are constructed and destroyed inside one call, and a
//   callback running during the solve sees the CALLER's count because the
//   scope is per BACKEND CALL and is undone before any callback fires. They
//   are covered by CONSTRUCTION RULE instead: `detail::KktFactor`'s
//   constructor is the single point every walk/SSN-tier factor is built
//   through (pinned in tests/sqp/test_kkt_calls.cpp), and no library site
//   default-constructs one. An `HVEN_TESTING` construction observer in
//   `qp_engine.cpp` is REGISTERED for W6 if they are ever to be OBSERVED.
//   Where a counter exists, this file asserts that the temporary's PATH RAN as
//   a premise rather than hoping it did.
//
//   NOT READ ANYWHERE. The restoration sub-driver's own engines: the
//   sub-driver is constructed inside `SqpDriver::solve` from a private tag and
//   nothing about it escapes. What IS pinned end to end is that a solve which
//   RAN a restoration phase (asserted on `counters.restoration_iters`) leaves
//   the caller's thread-local override exactly as it found it -- which covers
//   every backend call the sub-driver made, at every tier.
//
// ACCELERATE IS UNOBSERVED. `SymmetricFactor::set_num_threads` on Accelerate
// STORES the count and applies it to nothing, so the accessor pins below hold
// there and the MKL state pins do not compile there. The SQP deliberately does
// NOT mirror the interior-point solver's driver-level
// `accelerate_set_num_threads()`, which is a process-wide call that is never
// restored -- see CommonOptions::threads. The Apple application is REGISTERED
// with the Mac increment. No Apple value is estimated here.
//
// TWO PINS THAT LIVE ELSEWHERE AND ARE CITED, NOT REWRITTEN:
//   * a changed thread count refuses a hot handle --
//     tests/sqp/test_warm_start.cpp, `WarmStart.AChangedThreadCountRefusesAHotHandle`
//     (the options fingerprint folds `threads`; T8.3 built it, T8.8 gives the
//     number teeth). Adoption also takes the SESSION's live count
//     (SymmetricFactor::adopt), so an adopted handle at a matching fingerprint
//     carries the right count by construction.
//   * a negative `common.threads` is refused on the SqpOptions, before any
//     engine is built -- tests/drivers/test_options.cpp. The constructor path
//     this task adds does not bypass it: `validate_sqp_options` runs in both
//     SqpDriver constructors and in set_options() before any engine is made.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>

// MKL's own thread state is the only way to check that a call-scoped count was
// undone. Guarded rather than gated in CMake so the backend-neutral half of
// this file (the accessors, the plumbing, the 0-vs-1 neutrality) still runs on
// Apple, where the MKL half has no meaning and no substitute.
#ifndef HVEN_USE_ACCELERATE_LAPACK
#include <mkl_service.h>
#endif

#include <hven/detail/kkt/kkt_calls.h>
#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/detail/qp/ssn_engine.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>
#include <hven/model/nlp_model.h>
#include <hven/qp/qp_types.h>

#include "sqp/support/hs_problems.h"

namespace {

using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using hven::solvers::BoundState;
using hven::solvers::IpqpEngine;
using hven::solvers::NlpModel;
using hven::solvers::QpEngine;
using hven::solvers::QpMode;
using hven::solvers::QpOptions;
using hven::solvers::QpProblem;
using hven::solvers::QpSolution;
using hven::solvers::QpStatus;
using hven::solvers::SolveOverrides;
using hven::solvers::SolveStatus;
using hven::solvers::SqpDriver;
using hven::solvers::SqpIterate;
using hven::solvers::SqpOptions;
using hven::solvers::SqpSolution;
using hven::solvers::SsnEngine;
using hven::solvers::StartLevel;

constexpr double kInfBound = std::numeric_limits<double>::infinity();

// The count this file asks for wherever it asks for a non-default one. TWO,
// not one: `MKL_NUM_THREADS=1` is the suite's environment pin, so a count of 1
// would be indistinguishable from the ambient setting at the MKL boundary.
constexpr int kAskedFor = 2;

// The caller's own thread-local override, distinct from both 0 and kAskedFor,
// so "restored" cannot be confused with "reset to the global default" -- the
// distinction MklThreadScope exists to keep (see thread_scope.h / the scope's
// own doc comment) and the one a restore-to-zero implementation would fail.
constexpr int kCallerOverride = 3;

// ---------------------------------------------------------------------------
// MKL thread-state helpers, no-ops on Accelerate.
// ---------------------------------------------------------------------------

// Sets a caller-side thread-local override and returns what was in force
// before it. On Accelerate: returns 0 and sets nothing (UNOBSERVED).
int set_caller_local(int n) {
#ifndef HVEN_USE_ACCELERATE_LAPACK
    return mkl_set_num_threads_local(n);
#else
    (void)n;
    return 0;
#endif
}

// Reads the thread-local override in force WITHOUT changing it. Zero on
// Accelerate.
int caller_local_in_force() {
#ifndef HVEN_USE_ACCELERATE_LAPACK
    // The setter returns the override it replaced, so setting the value back
    // immediately both reads it and leaves the thread where it was.
    const int in_force = mkl_set_num_threads_local(0);
    mkl_set_num_threads_local(in_force);
    return in_force;
#else
    return 0;
#endif
}

int max_threads_now() {
#ifndef HVEN_USE_ACCELERATE_LAPACK
    return mkl_get_max_threads();
#else
    return 0;
#endif
}

// ---------------------------------------------------------------------------
// FIXTURES
// ---------------------------------------------------------------------------

// A two-row inconsistent system whose linearization at (2, 2) is degenerate:
// the equality Jacobian rows are parallel while the two rows demand different
// right-hand sides, so the funnel cannot make progress and the RESTORATION
// PHASE runs. Transcribed from tests/sqp/test_sqp_restoration.cpp's
// InfeasibleCircleLineModel (that suite's fixture 1); this file needs a
// restoring solve and this directory links no engine fixtures.
//
//   min x0 + x1   s.t.  x0^2 + x1^2 = 1,  x0 + x1 = 2
class InfeasibleCircleLineModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 2; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override { return x(0) + x(1); }
    Vec eval_grad(const Vec &) const override { return Vec::Ones(2); }
    Vec eval_ce(const Vec &x) const override {
        Vec c(2);
        c << x(0) * x(0) + x(1) * x(1) - 1.0, x(0) + x(1) - 2.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double, const Vec &lambda_e, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 2.0 * lambda_e(0);
        h.insert(1, 1) = 2.0 * lambda_e(0);
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &x) const override {
        SpMatRM j(2, 2);
        j.insert(0, 0) = 2.0 * x(0);
        j.insert(0, 1) = 2.0 * x(1);
        j.insert(1, 0) = 1.0;
        j.insert(1, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -kInfBound);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kInfBound);
        return u;
    }
    Vec start_point() const override {
        Vec x(2);
        x << 2.0, 2.0;
        return x;
    }
};

// min 1/2(x0^2 + x1^2) - x0 - 2 x1  s.t. x0 + x1 <= 1, 0 <= x <= 10.
// tests/sqp/test_qp_engine.cpp's `simple_box_qp`, verbatim: the face
// {row 0 working, both variables free} is the fixture that reaches
// `QpEngine::refine_on_face`, whose one temporary factor this file asserts the
// PATH of.
QpProblem simple_box_qp() {
    QpProblem qp;
    const Eigen::MatrixXd Hd = Eigen::MatrixXd::Identity(2, 2);
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -1, -2;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << 1, 1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, 1.0);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 10.0);
    return qp;
}

QpSolution face_of(const Vec &x, std::vector<BoundState> bounds, std::vector<bool> rows) {
    QpSolution s;
    s.status = QpStatus::kOptimal;
    s.x = x;
    s.lambda_e = Vec(0);
    s.lambda_i = Vec::Zero(static_cast<Index>(rows.size()));
    s.z = Vec::Zero(x.size());
    s.bound_state = std::move(bounds);
    s.ineq_active = std::move(rows);
    s.tr_active.assign(static_cast<std::size_t>(x.size()), false);
    return s;
}

SqpOptions base_options(QpMode mode, int threads) {
    SqpOptions o;
    o.qp_mode = mode;
    o.common.threads = threads;
    // A hot handle is what carries the DRIVER's own K0 factor out to a reader,
    // and it is the only route by which a boundary can see the factor the
    // driver actually solved through.
    o.start_level = StartLevel::kHot;
    return o;
}

// The driver's OWN K0 factor's live thread count, read through the hot handle
// its solve emitted. `HotState::border` is the very `BorderState` the engine
// solved through -- qp_engine.h defines the type the opaque handle names.
int driver_k0_num_threads(const SqpSolution &sol) {
    EXPECT_NE(sol.warm_start.hot, nullptr) << "the driver emitted no hot handle to read";
    if (sol.warm_start.hot == nullptr) {
        return -1;
    }
    return sol.warm_start.hot->border->kkt.factor.num_threads();
}

// Field-by-field EXACT comparison of the deterministic scalars on one history
// row. `EXPECT_EQ` on a double is bit equality for every non-NaN value, which
// is the claim: the plumbing added by this task changes no number.
void expect_rows_identical(const SqpIterate &a, const SqpIterate &b, int i) {
    SCOPED_TRACE(::testing::Message() << "history row " << i);
    EXPECT_EQ(a.trial, b.trial);
    EXPECT_EQ(a.f, b.f);
    EXPECT_EQ(a.stationarity, b.stationarity);
    EXPECT_EQ(a.feasibility, b.feasibility);
    EXPECT_EQ(a.complementarity, b.complementarity);
    EXPECT_EQ(a.kkt_residual, b.kkt_residual);
    EXPECT_EQ(a.violation_l1, b.violation_l1);
    EXPECT_EQ(a.tr_radius, b.tr_radius);
    EXPECT_EQ(a.mu, b.mu);
    EXPECT_EQ(a.step_norm, b.step_norm);
    EXPECT_EQ(a.qp_solved, b.qp_solved);
    EXPECT_EQ(a.qp_status, b.qp_status);
    EXPECT_EQ(a.qp_minor_iters, b.qp_minor_iters);
    EXPECT_EQ(a.qp_factorizations, b.qp_factorizations);
    EXPECT_EQ(a.tr_binding, b.tr_binding);
    EXPECT_EQ(a.soc_applied, b.soc_applied);
    EXPECT_EQ(a.elastic_applied, b.elastic_applied);
    EXPECT_EQ(a.watchdog_restored, b.watchdog_restored);
}

} // namespace

// ===========================================================================
// (1) ZERO -- TODAY'S BEHAVIOUR, BIT FOR BIT
// ===========================================================================

// The SQP default is 0 on every tier and every construction site, which is what
// makes the U0 replay at 0 an identity BY CONSTRUCTION: no scope is engaged
// anywhere, so there is nothing for a thread count to change.
TEST(Threads, ZeroLeavesEveryFactorAtTheBackendDefault) {
    EXPECT_EQ(SqpOptions{}.common.threads, 0) << "the SQP's shipped default";

    const QpOptions qopts;
    const QpEngine walk(qopts);
    const SsnEngine ssn(qopts);
    const IpqpEngine ipqp(qopts);
    EXPECT_EQ(walk.num_threads(), 0) << "the walk tier's K0 factor";
    EXPECT_EQ(walk.carried_num_threads(), 0);
    EXPECT_EQ(ssn.num_threads(), 0) << "the SSN tier's one persistent factor";
    EXPECT_EQ(ipqp.num_threads(), 0) << "the IPQP tier's KKT factorization";

    const hven::solvers::detail::KktFactor defaulted;
    EXPECT_EQ(defaulted.factor.num_threads(), 0);

    // THE HOT HANDLE IS A WALK-TIER OBJECT. It carries the `BorderState` the
    // walk engine solved through, so it exists only on a solve that ran the
    // walk; a kSsn or kIpm solve emits none, and that is not a defect this test
    // could paper over -- there is simply no boundary route to those tiers'
    // factors from outside the driver. What every mode CAN be held to is that
    // the solve left MKL's thread state exactly as it found it.
    const int before = max_threads_now();
    for (const QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpm}) {
        SCOPED_TRACE(::testing::Message() << "qp_mode " << static_cast<int>(mode));
        hven::solvers::test_support::Hs76Model model;
        SqpDriver driver(base_options(mode, 0));
        const SqpSolution sol = driver.solve(model);
        ASSERT_EQ(sol.status, SolveStatus::kOptimal);
        if (mode == QpMode::kWalk) {
            EXPECT_EQ(driver_k0_num_threads(sol), 0) << "the driver's own K0 factor";
        }
        EXPECT_EQ(max_threads_now(), before) << "nothing was left behind";
    }

    // ... and through a restoration, whose sub-driver inherits `common` whole.
    InfeasibleCircleLineModel restoring;
    SqpDriver driver(base_options(QpMode::kWalk, 0));
    const SqpSolution sol = driver.solve(restoring, restoring.start_point());
    ASSERT_GE(sol.counters.restoration_iters, 1) << "fixture premise: the phase RAN";
    EXPECT_EQ(driver_k0_num_threads(sol), 0);
    EXPECT_EQ(max_threads_now(), before);
}

// ===========================================================================
// (2) NON-ZERO -- IT REACHES EVERY TIER
// ===========================================================================

// Behaviour change (4) of design section 2.7: a non-zero SQP thread count is
// applied where before it was only carried.
TEST(Threads, NonZeroReachesEveryFactorTier) {
    const QpOptions qopts;

    // --- the three engines' persistent factors, at the boundary -------------
    const QpEngine walk(qopts, kAskedFor);
    const SsnEngine ssn(qopts, kAskedFor);
    const IpqpEngine ipqp(qopts, kAskedFor);
    EXPECT_EQ(walk.num_threads(), kAskedFor) << "the LIVE K0 factor, not the carried int";
    EXPECT_EQ(walk.carried_num_threads(), kAskedFor)
        << "and the two agree -- the engine applies what it carries";
    EXPECT_EQ(ssn.num_threads(), kAskedFor);
    EXPECT_EQ(ipqp.num_threads(), kAskedFor);

    // --- the temporaries: construction rule, plus the PATH as a premise -----
    //
    // The EQP-refine temporary (QpEngine::refine_on_face). `factorizations`
    // counts exactly the temporary this call builds, so a count of 1 IS the
    // statement that the site ran; what count that temporary was built at is
    // covered by KktFactor's constructor (tests/sqp/test_kkt_calls.cpp) and by
    // the report's grep, because the object dies before this line runs.
    const QpProblem qp = simple_box_qp();
    Vec off(2);
    off << 0.10, 0.85;
    const QpSolution face =
        face_of(off, {BoundState::kFree, BoundState::kFree}, std::vector<bool>{true});
    QpSolution refined;
    ASSERT_TRUE(walk.refine_on_face(qp, face, SolveOverrides{}, refined));
    EXPECT_EQ(refined.counters.factorizations, 1)
        << "PREMISE: the EQP-refine temporary was actually constructed on this path";
    EXPECT_EQ(walk.num_threads(), kAskedFor)
        << "and the persistent K0 is untouched by a temporary's life";

    // --- the driver's own K0, end to end -----------------------------------
    const int before = max_threads_now();
    hven::solvers::test_support::Hs76Model model;
    SqpDriver driver(base_options(QpMode::kWalk, kAskedFor));
    const SqpSolution sol = driver.solve(model);
    ASSERT_EQ(sol.status, SolveStatus::kOptimal);
    EXPECT_GE(sol.counters.factorizations, 1)
        << "PREMISE: the walk's per-solve temporary factor path ran";
    EXPECT_EQ(driver_k0_num_threads(sol), kAskedFor)
        << "SqpOptions::common.threads reached the driver's own K0 factor";
    EXPECT_EQ(max_threads_now(), before) << "and the scope undid itself";

    // The verdict-refine fallback (`fresh`, qp_engine.cpp) has NO counter that
    // distinguishes it from the reused factor at that site, so its path is NOT
    // asserted here -- stated rather than hoped. It is covered by the same
    // construction rule as the other temporaries, and the W6 observer would
    // measure it.
}

// THE EIGHTH FACTOR PATH, and the only DENSE one: the Schur border's
// `DenseSymmetricFactor` over LAPACK dsytrf/dsytrs. Before the second commit of
// M6 W5 T8.8 it sat inside NO thread scope at all and ran at whatever MKL's
// process default was, which made "reaches EVERY factor path" false as written.
//
// It is observable at the boundary through the same hot handle the K0 factor
// is: `BorderState::schur` is the complement the walk solved through.
TEST(Threads, NonZeroReachesTheDenseBorderFactor) {
    const int before = max_threads_now();

    hven::solvers::test_support::Hs76Model model;
    SqpDriver driver(base_options(QpMode::kWalk, kAskedFor));
    const SqpSolution sol = driver.solve(model);
    ASSERT_EQ(sol.status, SolveStatus::kOptimal);
    ASSERT_NE(sol.warm_start.hot, nullptr);
    ASSERT_TRUE(sol.warm_start.hot->border->schur.has_value())
        << "PREMISE: this solve built a live Schur border";

    EXPECT_EQ(sol.warm_start.hot->border->schur->num_threads(), kAskedFor)
        << "the DENSE border factor's own count, read through to the factor";
    EXPECT_EQ(max_threads_now(), before) << "and its LAPACK calls undid the scope";
}

// The same at the default, which is the identity claim: the dense factor is at
// 0 and no scope engages, so the second commit changes no number either.
TEST(Threads, ZeroLeavesTheDenseBorderFactorAtTheBackendDefault) {
    hven::solvers::test_support::Hs76Model model;
    SqpDriver driver(base_options(QpMode::kWalk, 0));
    const SqpSolution sol = driver.solve(model);
    ASSERT_EQ(sol.status, SolveStatus::kOptimal);
    ASSERT_NE(sol.warm_start.hot, nullptr);
    ASSERT_TRUE(sol.warm_start.hot->border->schur.has_value());
    EXPECT_EQ(sol.warm_start.hot->border->schur->num_threads(), 0);
}

// ===========================================================================
// (3) RESTORED ON EVERY EXIT -- INCLUDING THE RESTORATION SUB-DRIVER'S CALLS
// ===========================================================================

// The other half of the call-scoped promise, at the DRIVER level: hven undoes
// exactly what it did, so a caller who had a thread-local override of their own
// still has it after a whole solve -- every tier, and the nested restoration
// sub-solve's calls too. Restoring a hardcoded 0 would pass every assertion in
// the test above while quietly resetting every other MKL user on this thread.
//
// On Accelerate this degenerates to a no-op comparison of two zeros, which is
// why the assertions are guarded rather than merely tolerant: an Apple value is
// UNOBSERVED, not zero.
TEST(Threads, TheCallersThreadSettingSurvivesEveryTierAndTheRestoration) {
#ifdef HVEN_USE_ACCELERATE_LAPACK
    GTEST_SKIP() << "Accelerate stores the count and applies it to nothing -- UNOBSERVED";
#else
    const int entry = caller_local_in_force();
    set_caller_local(kCallerOverride);
    ASSERT_EQ(mkl_get_max_threads(), kCallerOverride) << "the caller's own override is in force";

    for (const QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpm}) {
        SCOPED_TRACE(::testing::Message() << "qp_mode " << static_cast<int>(mode));
        hven::solvers::test_support::Hs76Model model;
        SqpDriver driver(base_options(mode, kAskedFor));
        const SqpSolution sol = driver.solve(model);
        ASSERT_EQ(sol.status, SolveStatus::kOptimal);
        EXPECT_EQ(mkl_get_max_threads(), kCallerOverride);
    }

    // The restoration sub-driver's engines are unreachable from outside the
    // driver, so what is pinned about them is this: a solve that RAN a
    // restoration phase left the caller's override exactly as it found it, and
    // every backend call that phase made is inside that statement. That the
    // sub-driver inherits the COUNT is construction rule -- `SqpOptions ropts =
    // opts_` copies `common` whole, and the seven overrides that follow
    // (enable_scaling, make_strategy, budget_mode, qp_mode, tr_init, tr_max,
    // max_iter) name no `common` field.
    InfeasibleCircleLineModel restoring;
    SqpDriver driver(base_options(QpMode::kWalk, kAskedFor));
    const SqpSolution sol = driver.solve(restoring, restoring.start_point());
    ASSERT_GE(sol.counters.restoration_iters, 1) << "fixture premise: the phase RAN";
    EXPECT_EQ(driver_k0_num_threads(sol), kAskedFor);

    EXPECT_EQ(mkl_get_max_threads(), kCallerOverride)
        << "hven's call-scoped thread count must restore the caller's own thread-local "
           "override, not reset the thread to MKL's global setting";
    EXPECT_EQ(mkl_set_num_threads_local(0), kCallerOverride)
        << "read the other way MKL exposes it: the setter returns what it replaced";

    // Leave the thread as this test found it.
    set_caller_local(entry);
#endif
}

// ===========================================================================
// (4) THE PLUMBING IS NEUTRAL -- 0 vs 1, EXACTLY
// ===========================================================================

// THE REPLAY THIS FILE STANDS IN FOR. The SQP corpus has no in-process thread
// lever (`MKL_NUM_THREADS=1` in the environment IS its pin), and this task does
// not add one -- so the threads = 1 arm the brief asks about is pinned HERE, at
// the driver, instead of in the harness.
//
// WHAT IT PROVES AND WHAT IT DOES NOT. Under the suite's `MKL_NUM_THREADS=1`
// environment, 0 and 1 are the same computation on the backend, so this is a
// PLUMBING neutrality proof: threading the count through six construction sites
// changes no number. It is NOT a claim about numerics at counts above 1 --
// MKL's kernels are not reproducible across thread counts without CNR, which
// stays IPM-only (design section 2.6), and no such claim is made anywhere in
// this task.
TEST(Threads, ZeroAndOneProduceIdenticalHistoriesAndCounters) {
    hven::solvers::test_support::Hs6Model hs6;
    hven::solvers::test_support::Hs7Model hs7;
    hven::solvers::test_support::Hs76Model hs76;
    NlpModel *models[] = {&hs6, &hs7, &hs76};
    const char *names[] = {"HS6", "HS7", "HS76"};

    for (int m = 0; m < 3; ++m) {
        SCOPED_TRACE(names[m]);
        SqpDriver zero(base_options(QpMode::kWalk, 0));
        SqpDriver one(base_options(QpMode::kWalk, 1));
        const SqpSolution a = zero.solve(*models[m]);
        const SqpSolution b = one.solve(*models[m]);

        ASSERT_EQ(a.status, b.status);
        ASSERT_EQ(a.history.size(), b.history.size());
        for (std::size_t i = 0; i < a.history.size(); ++i) {
            expect_rows_identical(a.history[i], b.history[i], static_cast<int>(i));
        }
        EXPECT_EQ(a.counters.major_iters, b.counters.major_iters);
        EXPECT_EQ(a.counters.factorizations, b.counters.factorizations);
        EXPECT_EQ(a.counters.symbolic_analyses, b.counters.symbolic_analyses);
        EXPECT_EQ(a.counters.qp_minor_iters, b.counters.qp_minor_iters);
        EXPECT_EQ(a.counters.steps_accepted, b.counters.steps_accepted);
        EXPECT_EQ(a.counters.restoration_iters, b.counters.restoration_iters);
        EXPECT_EQ(a.f, b.f);
        ASSERT_EQ(a.x.size(), b.x.size());
        for (Index i = 0; i < a.x.size(); ++i) {
            EXPECT_EQ(a.x(i), b.x(i)) << "x(" << i << ")";
        }
    }
}
