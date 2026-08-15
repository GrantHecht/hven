// tests/test_hs_sweeps.cpp — PHASE-5 TASK 7: THE NONCONVEX SWEEP CORPUS and
// the policy adjudications it exists to produce.
//
// STEP 1 (this file's first half) is the TRANSCRIPTION GUARD on
// tests/sqp/support/hs_sweeps.h: derivative checks and pattern-invariance checks
// on every corpus member at several parameter values, plus the T6 contract
// checks on set_parameters. Nothing in the second half means anything if a
// wrapper reports a gradient its own objective does not have.
//
// STEP 2 (the second half) runs the corpus and pins what it measures. The
// numbers it pins are the ones
// docs/notes/2026-07-31-nonconvex-sweep-adjudications.md reports.
//
// =====================================================================
// WHAT THIS CORPUS FOUND FIRST (B-1), AND WHAT PHASE-5 TASK 7b DID ABOUT IT
//
// AS SHIPPED IN TASK 7: **A WARM-STARTED SOLVE COULD REPORT kOptimal, IN ZERO
// MAJORS, AT A POINT THAT IS NOT A KKT POINT OF THE PROBLEM IT WAS ASKED TO
// SOLVE.** Three of the six corpus problems reproduced it, and this file pinned
// the wrong answers exactly as observed (the Phase-3 HS26 precedent) so that a
// repair would FAIL these tests and have to come back. TASK 7b IS THAT REPAIR
// AND THIS IS THAT COMING BACK: `kDefectiveWarmArms` is now EMPTY and all six
// problems are in `kSoundWarmArms`.
//
// THE MECHANISM IN FOUR LINES, all of them citations rather than claims:
//   1. sqp_driver.h line ~3126 seeds `lambda_e`/`lambda_i` FROM THE WARM START
//      when a warm ingest resolves -- i.e. from a solve of a DIFFERENT problem.
//   2. The convergence test (line ~3549) is
//      `kkt.stationarity <= kkt_tol && kkt.feasibility <= feas_tol`. NLP
//      complementarity is measured but NOT gated.
//   3. sqp_driver.h's CONVERGENCE TEST note justified (2) by
//      "|lambda_i(j) cI_j| = |lambda_i(j) (Ji p)(j)| = O(||lambda_i|| ||p||),
//      which goes to zero with the step". THAT ARGUMENT PRESUPPOSES A
//      SUBPROBLEM WAS SOLVED IN THIS SOLVE -- it is an identity about the
//      subproblem's OWN complementarity -- and the test at (2) runs at the TOP
//      of the major loop, before any subproblem exists.
//   4. So when a previously ACTIVE inequality went STRICTLY SLACK at the new
//      parameter while its stale, strictly positive multiplier still zeroed
//      the Lagrangian gradient at the old point, the solve exited kOptimal
//      having done nothing.
//
// THE REPAIR (sqp_driver.h's THE INGESTED MULTIPLIERS ARE MADE COMPLEMENTARY):
// on a warm or hot ingest, any ingested `lambda_i(j)` whose row is not
// GEOMETRICALLY ACTIVE at the ingested x (`cI_j(x) < -feas_tol`) is set to
// zero before the first convergence test reads it -- the same distance test,
// with the same tolerance, that the reduced stationarity measure already
// applies to BOUNDS. Step (3)'s argument is thereby made true again rather than
// patched around: the ingested multipliers are complementary by construction,
// so (stationarity, feasibility) is once more a COMPLETE KKT test where it
// runs. THE REGRESSION CORPUS FOR THE REPAIR IS tests/test_b1_gate.cpp; this
// file carries element 1 of it (these three specs flipping to sound).
//
// THE CRITERION FOR WHAT WAS EXPOSED -- READ THE NOTE'S §1.4, NOT THIS
// CORPUS'S OWN SPLIT. A warm ingest was exposed exactly when moving p moved NO
// quantity the convergence test GATES (neither stationarity nor feasibility at
// the ingested point) while it did move one the test only RECORDS (general-
// inequality complementarity).
//
// THAT "NO" IS A DISJUNCTION OVER EVERY CHANNEL p ENTERS THROUGH, and it is the
// trap a corpus designer falls into: ONE COUPLED GATE RESCUES A MODEL WHOSE
// INEQUALITY ROW IS OTHERWISE EXPOSED, so a family can contain a perfectly
// B-1-shaped relaxing inequality and still be immune. F4 of
// tests/sqp/support/parametric_families.h is exactly that near-miss -- its cI row
// IS exposed in isolation, and it survives only because the same scalar a(p)
// simultaneously moves an equality row and a bound (note §1.4). CONSEQUENCE FOR
// ANY REGRESSION CORPUS BUILT AGAINST B-1: it must contain a problem where a
// RELAXING INEQUALITY IS THE SOLE THING p TOUCHES. An F4-shaped family passes a
// repair and a non-repair alike and so demonstrates nothing. THIS CORPUS DOES
// NOT CONTAIN SUCH A PROBLEM EITHER -- HS10/HS15/HS33 are exposed because their
// specs carry `tilt == vec0` and a pure `ci_shift`, which is a property of the
// SPEC and not of the model -- which is exactly why test_b1_gate.cpp exists and
// why the repair is not regression-tested from here alone.
//
// CONVEXITY IS IRRELEVANT, and the me-vs-mi split this file's own
// kSoundWarmArms/kDefectiveWarmArms encoded is a CONSEQUENCE in one direction
// only, not the criterion -- see those constants' own note. The Task-7 review
// reproduced the defect on a convex n = 3 problem of its own AND on
// F6PathBoundQuadrature, a CONVEX family already shipped in
// tests/sqp/support/parametric_families.h, whose p is a pure constant shift of cI.
// An earlier version of this banner and of the note scoped the defect by
// convexity; that was wrong and is corrected in the note's §1.4.
//
// THE CONSEQUENCE FOR THE ADJUDICATIONS. Task 7 delivered (a), the full-step
// (Kungurtsev-Diehl) lever, as a PARTIAL result, because the three problems
// with inequality constraints -- which are also the three where the funnel
// fights hardest -- performed no warm solves at all under the defect. THAT GAP
// IS NOW CLOSED: all six warm arms do real work, HS10's carries 8 elastic
// activations and 48 rho escalations and HS15's carries a rejected trial, and
// the lever STILL moves nothing. Adjudication (b), kappa_soc, was never
// contaminated and its outcome partition is unchanged; only its per-corpus
// minor-iteration denominator moved. See the note's §3 and §4.
//
// ONE THING THE REPAIR EXPOSED THAT THE DEFECT HAD HIDDEN: HS15's warm arm
// does NOT traverse its whole grid, and that is neither B-1 nor a new defect.
// See kTruncatedWarmArms below and section (1).

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <gtest/gtest.h>

#include <hven/core/ledger.h>
#include <hven/detail/sqp/globalization.h>
#include <hven/detail/sqp/sqp_driver.h>
#include <hven/detail/warmstart/continuation.h>
#include <hven/drivers/sqp_types.h>
#include <hven/model/nlp_model.h>
#include <hven/qp/qp_types.h>

#include "support/derivative_check.h"
#include "support/hs_sweeps.h"
#include "support/nlp_kkt_check.h"

namespace hven::solvers {
namespace {

using test_support::assert_gradient;
using test_support::assert_hessian;
using test_support::assert_jacobians;
using test_support::hs_sweep_spec;
using test_support::hs_sweep_specs;
using test_support::HsSweep;
using test_support::HsSweepSpec;
using test_support::make_hs_sweep;

// The Phase-3 battery's tolerance, for the same reason: derivative_check.h's
// comparison is mixed absolute/relative with a floor of 1, so 1e-6 is a
// relative bar on the large entries (HS10 at (-10, 10) has |hess cI| ~ 6) and
// an absolute one on the small.
constexpr double kDerivTol = 1e-6;

// A second evaluation point per problem, off the published start: a
// derivative check at ONE point can be passed by a model that is wrong
// everywhere else, which is why hs_problems.h's own checks use two. The
// perturbation is deliberately asymmetric per coordinate so it cannot cancel
// a sign error that happens to be symmetric.
Vec perturbed(const Vec &x0) {
    Vec x = x0;
    for (Index i = 0; i < x.size(); ++i) {
        x(i) += 0.3 + 0.11 * static_cast<double>(i);
    }
    return x;
}

// Multipliers for assert_hessian. NONZERO ON EVERY ROW, because the Lagrangian
// Hessian's constraint blocks are exactly what a zero multiplier would hide --
// and hiding them is the failure mode nlp_model.h's Phase-5 Task 0 clause was
// added for.
Vec ramp(Index m, double first) {
    Vec v(m);
    for (Index i = 0; i < m; ++i) {
        v(i) = first + 0.37 * static_cast<double>(i);
    }
    return v;
}

// The (row, col) index arrays of the three structural matrices, flattened into
// a string. Compared across x and p; see hs_sweeps.h's STRUCTURAL PATTERN
// INVARIANCE paragraph for the obligation this checks.
//
// IT COMPARES INDEX ARRAYS, NOT VALUES. That is the whole point: two matrices
// with the same pattern and different numbers must compare EQUAL here, or the
// check would fail on every model the moment p moved a value, and would
// therefore be pinning the opposite of the contract.
std::string pattern_signature(const NlpModel &model, const Vec &x, const Vec &le, const Vec &li) {
    std::string s;
    const auto add_sparse = [&s](const auto &m) {
        s += fmt::format("[{}x{}", m.rows(), m.cols());
        for (int k = 0; k < m.outerSize(); ++k) {
            for (typename std::decay_t<decltype(m)>::InnerIterator it(m, k); it; ++it) {
                s += fmt::format(" {},{}", it.row(), it.col());
            }
        }
        s += "]";
    };
    add_sparse(model.eval_hess(x, 1.0, le, li));
    add_sparse(model.eval_jac_e(x));
    add_sparse(model.eval_jac_i(x));
    return s;
}

// ---------------------------------------------------------------------
// STEP 1 -- THE TRANSCRIPTION GUARD
// ---------------------------------------------------------------------

TEST(HsSweepModels, DerivativesAgreeWithFiniteDifferencesAtEveryParameterValue) {
    for (const HsSweepSpec &spec : hs_sweep_specs()) {
        auto model = make_hs_sweep(spec.number);
        const Vec x0 = model->start_point();
        const Vec x1 = perturbed(x0);
        const Vec le = ramp(model->me(), 0.7);
        const Vec li = ramp(model->mi(), 0.4);

        // p0, the midpoint and p1: the endpoints are what the sweeps actually
        // start and finish at, and the midpoint catches a term that is right
        // at both ends by accident (an even function of p - (p0+p1)/2, say).
        const double mid = 0.5 * (spec.p0 + spec.p1);
        for (double p : {spec.p0, mid, spec.p1}) {
            model->set_parameters(Vec::Constant(1, p));
            for (const Vec &x : {x0, x1}) {
                SCOPED_TRACE(fmt::format(
                    "{} at p = {}, x = ({})", spec.name, p,
                    fmt::join(std::vector<double>(x.data(), x.data() + x.size()), ", ")));
                EXPECT_TRUE(assert_gradient(*model, x, kDerivTol));
                EXPECT_TRUE(assert_jacobians(*model, x, kDerivTol));
                EXPECT_TRUE(assert_hessian(*model, x, le, li, kDerivTol));
            }
        }
    }
}

TEST(HsSweepModels, SparsityPatternIsIndependentOfBothXAndP) {
    for (const HsSweepSpec &spec : hs_sweep_specs()) {
        auto model = make_hs_sweep(spec.number);
        const Vec x0 = model->start_point();
        const Vec x1 = perturbed(x0);
        const Vec le = ramp(model->me(), 0.7);
        const Vec li = ramp(model->mi(), 0.4);
        const Vec zero_e = Vec::Zero(model->me());
        const Vec zero_i = Vec::Zero(model->mi());

        model->set_parameters(Vec::Constant(1, spec.p0));
        const std::string reference = pattern_signature(*model, x0, le, li);

        // Same p, different x.
        EXPECT_EQ(reference, pattern_signature(*model, x1, le, li))
            << spec.name << ": x-dependence";
        // Same x and p, ZERO multipliers and a zero objective scale -- the
        // clause nlp_model.h added in Phase-5 Task 0, and the one the driver's
        // warm-start ingest/emission probes rely on.
        EXPECT_EQ(reference, pattern_signature(*model, x0, zero_e, zero_i))
            << spec.name << ": lambda-dependence";
        {
            std::string s;
            const auto h = model->eval_hess(x0, 0.0, zero_e, zero_i);
            const auto h_ref = model->eval_hess(x0, 1.0, le, li);
            EXPECT_EQ(h.nonZeros(), h_ref.nonZeros()) << spec.name << ": obj_scale-dependence";
        }
        // Different p, both x.
        model->set_parameters(Vec::Constant(1, spec.p1));
        EXPECT_EQ(reference, pattern_signature(*model, x0, le, li))
            << spec.name << ": p-dependence";
        EXPECT_EQ(reference, pattern_signature(*model, x1, le, li))
            << spec.name << ": p-and-x-dependence";
    }
}

TEST(HsSweepModels, AtPZeroTheWrapperIsThePublishedProblem) {
    for (const HsSweepSpec &spec : hs_sweep_specs()) {
        auto sweep = make_hs_sweep(spec.number);
        const auto base = test_support::make_hs(spec.number).model;
        // Every corpus member's p0 is 0 by construction (hs_sweeps.h's spec
        // table); this test would be vacuous otherwise, so it says so.
        ASSERT_EQ(0.0, spec.p0) << spec.name;
        sweep->set_parameters(Vec::Constant(1, 0.0));

        const Vec x = perturbed(sweep->start_point());
        EXPECT_EQ(base->n(), sweep->n()) << spec.name;
        EXPECT_EQ(base->me(), sweep->me()) << spec.name;
        EXPECT_EQ(base->mi(), sweep->mi()) << spec.name;
        EXPECT_DOUBLE_EQ(base->eval_f(x), sweep->eval_f(x)) << spec.name;
        EXPECT_EQ(base->start_point(), sweep->start_point()) << spec.name;
        EXPECT_EQ(base->eval_grad(x), sweep->eval_grad(x)) << spec.name;
        EXPECT_EQ(base->eval_ce(x), sweep->eval_ce(x)) << spec.name;
        EXPECT_EQ(base->eval_ci(x), sweep->eval_ci(x)) << spec.name;
    }
}

TEST(HsSweepModels, MovingPMovesExactlyTheShiftedRowsAndTheTiltedObjective) {
    for (const HsSweepSpec &spec : hs_sweep_specs()) {
        auto model = make_hs_sweep(spec.number);
        const Vec x = perturbed(model->start_point());
        model->set_parameters(Vec::Constant(1, 0.0));
        const double f0 = model->eval_f(x);
        const Vec g0 = model->eval_grad(x);
        const Vec ce0 = model->eval_ce(x);
        const Vec ci0 = model->eval_ci(x);

        const double p = spec.p1;
        model->set_parameters(Vec::Constant(1, p));
        // The construction is affine in p and this is the statement of it:
        // f moves by p * t^T x, grad by p * t, cE by -p * sE, cI by -p * sI.
        EXPECT_NEAR(f0 + p * spec.tilt.dot(x), model->eval_f(x), 1e-12 * (1.0 + std::abs(f0)))
            << spec.name;
        EXPECT_LT((g0 + p * spec.tilt - model->eval_grad(x)).lpNorm<Eigen::Infinity>(), 1e-12)
            << spec.name;
        if (model->me() > 0) {
            EXPECT_LT((ce0 - p * spec.ce_shift - model->eval_ce(x)).lpNorm<Eigen::Infinity>(),
                      1e-12)
                << spec.name;
        }
        if (model->mi() > 0) {
            EXPECT_LT((ci0 - p * spec.ci_shift - model->eval_ci(x)).lpNorm<Eigen::Infinity>(),
                      1e-12)
                << spec.name;
        }
    }
}

TEST(HsSweepModels, ParameterContractViolationsThrowWithTheirSizesInTheMessage) {
    auto model = make_hs_sweep(77);
    EXPECT_EQ(1, model->parameter_dim());
    EXPECT_THROW(model->set_parameters(Vec::Zero(2)), std::invalid_argument);
    EXPECT_THROW(model->set_parameters(Vec::Zero(0)), std::invalid_argument);
    EXPECT_THROW(model->set_parameters(Vec::Constant(1, std::nan(""))), std::invalid_argument);
    // A mis-sized shift is a CONSTRUCTION error, not a solve-time surprise.
    EXPECT_THROW(HsSweep(77, Vec::Zero(4), Vec::Zero(2), Vec::Zero(0), 0.0), std::invalid_argument);
    EXPECT_THROW(HsSweep(77, Vec::Zero(5), Vec::Zero(1), Vec::Zero(0), 0.0), std::invalid_argument);
    EXPECT_THROW(hs_sweep_spec(99), std::invalid_argument);
    // The corpus is what hs_sweeps.h's THE SIX PROBLEMS note says it is.
    ASSERT_EQ(6u, hs_sweep_specs().size());
}

// ---------------------------------------------------------------------
// STEP 2 -- THE CORPUS RUN
// ---------------------------------------------------------------------

// Everything one cell reports. Counters are LEDGER SUMS, exactly as
// test_warm_start_battery.cpp's CellStats are and for the same reason: they
// are then the same numbers a later comparison reads, rather than an ad-hoc
// tally maintained beside the ledger and liable to drift from it.
struct SweepCell {
    Index majors = 0, minors = 0, factorizations = 0;
    Index accepted = 0, rejected = 0;
    Index soc_steps = 0, soc_applied = 0, soc_qp_infeasible = 0, soc_rejected = 0;
    Index elastic_activations = 0, elastic_escalations = 0, restoration_iters = 0;
    Index full_step_majors = 0, watchdog = 0;
    Index n_cold = 0, n_warm = 0, n_hot = 0;
    Index status_failures = 0;
    Index solves = 0;
    // The worst KKT residual seen over the cell's converged points, recomputed
    // FROM THE MODEL (support/nlp_kkt_check.h) rather than read off the driver.
    // The cold arm holds whole SqpSolutions and runs the full quadruple; the
    // warm arm gets what ContinuationStep carries (see run_warm_sweep).
    double worst_stationarity = 0.0, worst_primal = 0.0, worst_complementarity = 0.0;
    std::vector<double> pgrid;
    // The returned objective at each grid point, in the same order as `pgrid`.
    // THIS IS THE ANSWER, as opposed to the cost of getting it, and it is what
    // check_warm_arm_objective_against_the_cold_arm() reads.
    std::vector<double> fvals;
    bool reached_p1 = false;
    double seconds = 0.0;
};

// The PRIMAL half of nlp_kkt_check.h's quadruple, recomputed FROM THE MODEL:
// max over the equality rows, the violated inequality rows and the bounds.
// The warm arm needs it on its own because ContinuationStep carries x but no
// multipliers, so the full self_check_kkt is not available there; the cold
// arm, which holds whole SqpSolutions, runs the full check instead.
double primal_violation(const NlpModel &model, const Vec &x) {
    double v = 0.0;
    if (model.me() > 0) {
        v = std::max(v, model.eval_ce(x).lpNorm<Eigen::Infinity>());
    }
    if (model.mi() > 0) {
        v = std::max(v, model.eval_ci(x).maxCoeff());
    }
    for (Index i = 0; i < model.n(); ++i) {
        v = std::max(v, std::max(model.lower()(i) - x(i), x(i) - model.upper()(i)));
    }
    return std::max(v, 0.0);
}

void fold(SweepCell &c, const SqpSolveRecord &r) {
    c.majors += r.counters.major_iters;
    c.minors += r.counters.qp_minor_iters;
    c.factorizations += r.counters.factorizations;
    c.accepted += r.counters.steps_accepted;
    c.rejected += r.counters.rejected_steps;
    c.soc_steps += r.counters.soc_steps;
    c.soc_applied += r.counters.soc_applied;
    c.soc_qp_infeasible += r.counters.soc_qp_infeasible;
    c.soc_rejected += r.counters.soc_rejected;
    c.elastic_activations += r.counters.elastic_activations;
    c.elastic_escalations += r.counters.elastic_escalations;
    c.restoration_iters += r.counters.restoration_iters;
    c.full_step_majors += r.full_step_majors;
    c.watchdog += r.watchdog_restores;
    switch (r.start_level_used) {
    case StartLevel::kCold:
        ++c.n_cold;
        break;
    // PHASE-6 TASK 5. Folded into n_warm, for the same reason
    // test_warm_start_battery.cpp's own aggregator does: every warm arm here
    // chains hand-offs from the SAME model object, so the hash matches and
    // this case is unreachable -- and folding rather than dropping keeps the
    // "was this ingested" accounting honest if it ever fires.
    case StartLevel::kSeeded:
        ++c.n_warm;
        break;
    case StartLevel::kWarm:
        ++c.n_warm;
        break;
    case StartLevel::kHot:
        ++c.n_hot;
        break;
    }
    if (r.status != SqpStatus::kOptimal) {
        ++c.status_failures;
    }
    ++c.solves;
}

// The one SqpOptions every cell shares apart from the two axis fields.
//
// adaptive_mu IS OFF, for the reason test_warm_start_battery.cpp's
// battery_options() gives: the Task-10 schedule drives dual_mu off the KKT
// residual, which differs between two consecutive solves, and qp_engine.h's
// hot-start reuse condition (d) requires the effective (primal_delta,
// dual_mu) pair to be byte-identical across them. Holding it off on the cold
// arm too is what makes the arms differ in exactly one thing.
//
// THE TOLERANCES ARE THE PHASE-3 BATTERY'S (1e-6), not the Phase-4 battery's
// 1e-8. These are the published Hock-Schittkowski problems, two of which
// (HS26's degenerate quartic, HS30's weakly-active row) the Phase-3 battery
// measured as tolerance-limited at 1e-6 already; asking for 1e-8 would be
// asking the corpus to spend its majors on a convergence tail rather than on
// the globalization behaviour the adjudications are about.
SqpOptions sweep_options(StartLevel level, bool full_step, bool enable_soc, Index max_iter) {
    SqpOptions opts;
    opts.kkt_tol = 1e-6;
    opts.feas_tol = 1e-6;
    opts.max_iter = max_iter;
    opts.adaptive_mu = false;
    opts.start_level = level;
    opts.warm_full_step = full_step;
    opts.enable_soc = enable_soc;
    return opts;
}

// The PINNED-GRID continuation options -- see hs_sweeps.h's HsSweepSpec note
// for why the step controller is frozen rather than adaptive. With
// dp_init == dp_min == dp_max and target_majors = 0 the grid is a CONSTANT of
// the spec (p0, p0 + dp, ..., p1) and cannot be re-gridded by a lever that
// changes step acceptance, which is what makes every cross-arm comparison
// below a matched one. check_every_arm_visited_the_same_grid() asserts it
// rather than trusting this paragraph.
ContinuationOptions pinned_grid_options(const HsSweepSpec &spec) {
    ContinuationOptions copts;
    copts.dp_init = spec.dp;
    copts.dp_min = spec.dp;
    copts.dp_max = spec.dp;
    copts.target_majors = 0; // never grow
    copts.use_predictor = false;
    return copts;
}

// THE GRID THE SPEC DEFINES, built from (p0, p1, dp) alone and with no
// reference to anything any driver did. hs_sweeps.h's HsSweepSpec note is what
// makes this a CONSTANT of the spec rather than a description of a run:
// dp_init == dp_min == dp_max and target_majors = 0, so the controller can
// never re-grid.
std::vector<double> spec_grid(const HsSweepSpec &spec) {
    std::vector<double> g;
    for (double v = spec.p0; v <= spec.p1 + 1e-12; v += spec.dp) {
        g.push_back(v);
    }
    return g;
}

// The warm arm: one run_continuation sweep on one driver, so the hand-off
// chain is the thing under test.
SweepCell run_warm_sweep(const HsSweepSpec &spec, bool full_step, bool enable_soc) {
    auto model = make_hs_sweep(spec.number);
    SqpDriver driver(sweep_options(StartLevel::kWarm, full_step, enable_soc, spec.max_iter));
    Ledger ledger;
    driver.attach_ledger(&ledger, "sweep");
    const auto t0 = std::chrono::steady_clock::now();
    const ContinuationResult res =
        run_continuation(*model, Vec::Constant(1, spec.p0), Vec::Constant(1, spec.p1), driver,
                         pinned_grid_options(spec));
    const auto t1 = std::chrono::steady_clock::now();

    SweepCell c;
    for (const SqpSolveRecord &r : ledger.sqp_records()) {
        fold(c, r);
    }
    c.reached_p1 = res.reached_p1;
    c.seconds = std::chrono::duration<double>(t1 - t0).count();
    for (const ContinuationStep &st : res.steps) {
        c.pgrid.push_back(st.p(0));
        // Re-posing per step is required: run_continuation leaves the model at
        // p1, and every quantity below is a property of the model AT st.p.
        model->set_parameters(st.p);
        c.fvals.push_back(model->eval_f(st.x));
        if (st.status == SqpStatus::kOptimal) {
            c.worst_primal = std::max(c.worst_primal, primal_violation(*model, st.x));
        }
    }
    return c;
}

// THE DIRECT WARM CHAIN -- what section (3a) needs and what the sweep vehicle
// cannot supply. `ContinuationStep` carries x and status but NO MULTIPLIERS
// (the note's §1.3 footnote), so a run_continuation arm cannot be asked
// whether its points satisfy COMPLEMENTARITY -- which is precisely the term
// B-1 destroyed and the repair restores. This runs the same grid as two-solve
// links chained by hand, keeping whole SqpSolutions, so the full KKT quadruple
// is recomputable from the model at every point.
//
// It is the SAME hand-off path the sweep uses (solve(model, prev.x,
// prev.warm_start) with start_level = kWarm), just with the results retained;
// it is not a second mechanism being tested in place of the first.
struct ChainPoint {
    double p = 0.0;
    SqpStatus status = SqpStatus::kOptimal;
    StartLevel level = StartLevel::kCold;
    Index majors = 0;
    double f = 0.0;
    test_support::NlpKktResidual kkt;
};

std::vector<ChainPoint> run_warm_chain(const HsSweepSpec &spec) {
    auto model = make_hs_sweep(spec.number);
    std::vector<ChainPoint> out;
    SqpSolution prev;
    bool have_prev = false;
    for (double p = spec.p0; p <= spec.p1 + 1e-12; p += spec.dp) {
        model->set_parameters(Vec::Constant(1, p));
        const StartLevel level = have_prev ? StartLevel::kWarm : StartLevel::kCold;
        SqpDriver driver(sweep_options(level, true, true, spec.max_iter));
        const SqpSolution sol = have_prev ? driver.solve(*model, prev.x, prev.warm_start)
                                          : driver.solve(*model, model->start_point());
        ChainPoint pt;
        pt.p = p;
        pt.status = sol.status;
        pt.level = sol.counters.start_level_used;
        pt.majors = sol.counters.major_iters;
        pt.f = sol.f;
        if (sol.status == SqpStatus::kOptimal) {
            pt.kkt = test_support::self_check_kkt(*model, sol, 1e-6);
        }
        out.push_back(pt);
        prev = sol;
        have_prev = sol.status == SqpStatus::kOptimal;
        if (!have_prev) {
            break;
        }
    }
    return out;
}

const std::vector<std::vector<ChainPoint>> &warm_chains() {
    static const std::vector<std::vector<ChainPoint>> chains = [] {
        std::vector<std::vector<ChainPoint>> c;
        for (const HsSweepSpec &spec : hs_sweep_specs()) {
            c.push_back(run_warm_chain(spec));
        }
        return c;
    }();
    return chains;
}

// The cold arm: the SAME parameter values, each solved from scratch on its own
// driver from the model's published start point.
SweepCell run_cold_grid(const HsSweepSpec &spec, const std::vector<double> &grid, bool full_step,
                        bool enable_soc) {
    auto model = make_hs_sweep(spec.number);
    Ledger ledger;
    SweepCell c;
    bool all_optimal = true;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < grid.size(); ++i) {
        SqpDriver driver(sweep_options(StartLevel::kCold, full_step, enable_soc, spec.max_iter));
        driver.attach_ledger(&ledger, fmt::format("cold{}", i));
        model->set_parameters(Vec::Constant(1, grid[i]));
        const SqpSolution sol = driver.solve(*model, model->start_point());
        all_optimal = all_optimal && sol.status == SqpStatus::kOptimal;
        if (sol.status == SqpStatus::kOptimal) {
            const test_support::NlpKktResidual r = test_support::self_check_kkt(*model, sol, 1e-6);
            c.worst_stationarity = std::max(c.worst_stationarity, r.stationarity);
            c.worst_primal = std::max(c.worst_primal, r.primal);
            c.worst_complementarity = std::max(c.worst_complementarity, r.complementarity);
        }
        c.pgrid.push_back(grid[i]);
        c.fvals.push_back(sol.f);
    }
    const auto t1 = std::chrono::steady_clock::now();
    for (const SqpSolveRecord &r : ledger.sqp_records()) {
        fold(c, r);
    }
    // "Reached p1" for this arm means "solved every grid point": there is no
    // path being followed, so nothing else it could mean.
    c.reached_p1 = all_optimal;
    c.seconds = std::chrono::duration<double>(t1 - t0).count();
    return c;
}

enum ArmIdx { kArmCold = 0, kArmWarm = 1, kArmCount = 2 };
enum LeverIdx { kOn = 0, kOff = 1 };

const char *arm_name(int a) { return a == kArmCold ? "cold" : "warm"; }

struct ProblemCells {
    std::string name;
    int number = 0;
    // [arm][full_step: kOn/kOff][soc: kOn/kOff]
    SweepCell cell[kArmCount][2][2];
};

// THE WHOLE CORPUS, computed exactly once per process -- the same
// one-TEST-one-process argument test_warm_start_battery.cpp's THE RUNTIME
// BUDGET note makes: gtest_discover_tests registers one CTest entry per TEST
// and therefore one PROCESS per TEST, so assertions split across a dozen TESTs
// would recompute the grid a dozen times. Each section is a check_*() function
// called in order from TEST(HsSweepCorpus, Corpus); a gtest ASSERT_ inside one
// returns from that section only, so a section that cannot proceed does not
// silence the rest.
//
// COST: 24 sweeps + 24 matched cold grids, plus section (8)'s 34 decorated
// cold solves and their 34 undecorated controls -- all on problems with
// n <= 5. Measured 0.54 s Release / 1.58 s Debug for the whole TEST (clang++,
// MKL, MKL_NUM_THREADS=1, this development machine), so it needs no
// ScaleF7Slow-style exclusion and adds nothing to the phase-gate direct-run
// list.
const std::vector<ProblemCells> &corpus() {
    static const std::vector<ProblemCells> table = [] {
        std::vector<ProblemCells> t;
        for (const HsSweepSpec &spec : hs_sweep_specs()) {
            ProblemCells pc;
            pc.name = spec.name;
            pc.number = spec.number;
            for (int f = 0; f < 2; ++f) {
                for (int s = 0; s < 2; ++s) {
                    const bool full_step = (f == kOn);
                    const bool soc = (s == kOn);
                    pc.cell[kArmWarm][f][s] = run_warm_sweep(spec, full_step, soc);
                    // THE COLD ARM GETS THE SPEC'S OWN GRID, not the matched
                    // warm cell's (Task-7b review, I-3). It used to be handed
                    // the warm cell's `pgrid`, which was harmless while every
                    // warm arm traversed its whole range and became a real
                    // coupling the moment one did not: HS15's cold arm shrank
                    // 7 -> 5 solves purely BECAUSE ITS WARM ARM DIED, and
                    // adjudication (b)'s SOC population is counted off the cold
                    // arm. A ratification artifact must not depend on where an
                    // unrelated arm stopped. Decoupling also makes section (2)'s
                    // cold rows load-bearing instead of tautological, which is
                    // the Task-7 review's own M-3 disclosure closed.
                    pc.cell[kArmCold][f][s] = run_cold_grid(spec, spec_grid(spec), full_step, soc);
                }
            }
            t.push_back(std::move(pc));
        }
        return t;
    }();
    return table;
}

const ProblemCells &problem(const char *name) {
    for (const ProblemCells &p : corpus()) {
        if (p.name == name) {
            return p;
        }
    }
    // FATAL rather than a fall-through to corpus().front(), which would
    // silently compare everything against HS10 and bury the real failure under
    // a cascade of misleading diffs (project rule T6: the name it could not
    // find is in the message).
    throw std::invalid_argument(fmt::format("HsSweeps: no problem named '{}' in the corpus", name));
}

// A whole-corpus dump, attached to the pinned assertions so a re-measurement
// (a backend change, a repair of the defect above) reads the new table off the
// failure message instead of needing a debugger, and emitted once at the end of
// the corpus test as the results note's machine-produced data source.
std::string corpus_table() {
    std::string s = "\n";
    s += fmt::format("{:<6} {:<5} {:<4} {:<4} {:<6} {:<6} {:<6} {:<6} {:<8} {:<13} {:<9} {:<6} "
                     "{:<6} {:<3} {:<8} {}\n",
                     "prob", "arm", "KD", "SOC", "solves", "majors", "minors", "fact", "acc/rej",
                     "soc a/i/r/n", "elas a/e", "restor", "fsMaj", "wd", "lvl c/w/h", "fail");
    for (const ProblemCells &p : corpus()) {
        for (int a = 0; a < kArmCount; ++a) {
            for (int f = 0; f < 2; ++f) {
                for (int so = 0; so < 2; ++so) {
                    const SweepCell &c = p.cell[a][f][so];
                    s += fmt::format(
                        "{:<6} {:<5} {:<4} {:<4} {:<6} {:<6} {:<6} {:<6} {:<8} "
                        "{:<13} {:<9} {:<6} {:<6} {:<3} {:<8} {}\n",
                        p.name, arm_name(a), f == kOn ? "on" : "off", so == kOn ? "on" : "off",
                        c.solves, c.majors, c.minors, c.factorizations,
                        fmt::format("{}/{}", c.accepted, c.rejected),
                        fmt::format("{}/{}/{}/{}", c.soc_applied, c.soc_qp_infeasible,
                                    c.soc_rejected, c.soc_steps),
                        fmt::format("{}/{}", c.elastic_activations, c.elastic_escalations),
                        c.restoration_iters, c.full_step_majors, c.watchdog,
                        fmt::format("{}/{}/{}", c.n_cold, c.n_warm, c.n_hot), c.status_failures);
                }
            }
        }
    }
    return s;
}

// EVERY corpus member's warm arm is sound as of Phase-5 Task 7b's B-1 repair.
// kDefectiveWarmArms is kept, empty, rather than deleted: it is the shape a
// future regression would be recorded in, and an empty list is a stronger
// statement than a missing one.
//
// THESE ARE OBSERVED LABELS FOR THIS CORPUS, NOT A STATEMENT OF THE CRITERION,
// and an earlier version of this comment got that wrong by asserting the split
// was "exactly inequality-vs-equality, and not a coincidence". Half of that
// held and half was a confound (Task-7 review, I-1):
//
//   IMMUNITY was rigorous for the three that never broke. An equality-only KKT
//   system has NO COMPLEMENTARITY CONDITION AT ALL, so stationarity +
//   feasibility IS the complete KKT test there -- a stronger guarantee than
//   "the violation enters the feasibility measure", since even a stale
//   lambda_e that happens to zero gL still leaves a genuine KKT point. THAT IS
//   ALSO WHY THEY ARE THE REPAIR'S CONTROL SET: the repair touches lambda_i
//   only, and on a model with mi() == 0 its loop does not execute at all, so
//   these three must be bit-identical across it (asserted in
//   tests/test_b1_gate.cpp, against values measured at the pre-repair commit).
//
//   BREAKAGE was confounded. HS10/HS15/HS33 are ALSO exactly the three specs
//   with tilt == vec0 and a pure ci_shift, while HS26/HS40/HS77 all carry an
//   ce_shift -- so this corpus cannot distinguish "inequalities break" from "a
//   parameter that moves only cI's constant term breaks". The review's F6
//   reproduction settles it in favour of the second: an inequality-constrained
//   member carrying an objective tilt moves grad f, fails the stationarity
//   gate, and does NOT break. The operative criterion is in this file's THE
//   CRITERION FOR WHAT WAS EXPOSED banner and, authoritatively, the note's
//   §1.4.
const std::vector<const char *> kDefectiveWarmArms = {};
const std::vector<const char *> kSoundWarmArms = {"HS10", "HS15", "HS26", "HS33", "HS40", "HS77"};

bool is_sound_warm_arm(const std::string &name) {
    for (const char *s : kSoundWarmArms) {
        if (name == s) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------
// HS15'S WARM ARM STOPS SHORT, AND IT IS NEITHER B-1 NOR A NEW DEFECT.
//
// Under the defect HS15's warm arm did no work past its first point, so the
// spec's own range rule ("the widest range on which every arm still converges
// at every grid point", hs_sweeps.h) was never actually tested on it. With the
// repair the arm follows a real path, and THAT PATH HAS A SINGULARITY EXACTLY
// AT p = 1, which is on the grid.
//
// THE ARGUMENT, from the model rather than from the observation. The swept
// rows are cI1 = 1 - x1 x2 - p and cI2 = -x1 - x2^2. The branch the
// continuation tracks sits on BOTH (measured: at p = 0.75 it returns
// x = (-0.39685, -0.62996), where both rows read 0 to 1e-11), so it follows
// the curve x1 x2 = 1 - p intersected with x1 = -x2^2, i.e.
// -x2^3 = 1 - p: as p -> 1 the vertex goes to THE ORIGIN. At the origin
// grad cI1 = (-x2, -x1) = (0, 0) EXACTLY while grad cI2 = (-1, -2 x2) =
// (-1, 0), so two rows are active with a rank-1 Jacobian and LICQ fails. The
// solve there returns kNumericalError -- an honest refusal at a point where
// the constraint qualification does not hold, which is the OPPOSITE of B-1's
// failure mode (a confident wrong answer). HsSweepModels.Hs15BranchEndsAtA-
// PointWhereLicqFails pins the model-level half of this argument so it is not
// resting on the observation.
//
// THE COLD ARM DOES NOT MEET THE SAME WALL because it does not follow the
// branch: it restarts from the published start point at every p and, from
// p = 0.75 on, lands on a DIFFERENT connected component of the feasible set
// (HS15's set is disconnected -- that is why it is in the corpus). Both arms
// return genuine KKT points; they are simply not the same one. Section (3b)
// states that as an assertion rather than an excuse.
//
// WHAT IS PINNED, rather than tolerated: the arm stops at EXACTLY p = 1.0,
// with EXACTLY one status failure, IDENTICALLY under both settings of the
// full-step lever -- so every cross-lever comparison below is still matched.
// The spec is deliberately NOT re-gridded: shortening HS15's range would
// remove the p = 1 cold solve, which is where two of adjudication (b)'s nine
// SOC attempts come from, i.e. it would pay for a tidier section (1) with a
// hole in a ratified adjudication.
//
// AND THE TRUNCATION DOES NOT REACH THE COLD ARM (Task-7b review, I-3). It
// used to: run_cold_grid was HANDED its matched warm cell's pgrid, so HS15's
// cold arm shrank 7 -> 5 solves purely BECAUSE THE WARM ARM DIED -- and
// adjudication (b)'s SOC population is counted off the cold arm, so a closure
// was resting on where an unrelated arm happened to stop. corpus() now hands
// the cold arm spec_grid(spec). The consequence is asserted in section (2): a
// cold cell shorter than the full spec grid is a FAILURE.
const std::vector<const char *> kTruncatedWarmArms = {"HS15"};
constexpr double kHs15WarmArmStopsAt = 1.0;

bool is_truncated_warm_arm(const std::string &name) {
    for (const char *s : kTruncatedWarmArms) {
        if (name == s) {
            return true;
        }
    }
    return false;
}

// =====================================================================
// THE SECTIONS
// =====================================================================

// ---------------------------------------------------------------------
// (1) NOTHING FAILED ANYWHERE EXCEPT WHERE IT IS PINNED TO. Every cell
// traversed its whole grid and every solve in it reported kOptimal -- with the
// single exception argued in full at kTruncatedWarmArms above, HS15's warm arm,
// whose branch runs into an LICQ failure at p = 1. This says nothing about
// whether the ANSWERS are right (section (3) is where that is tested), but a
// cell that failed to traverse UNEXPECTEDLY would make every count below
// incomparable, so it is checked first.
// ---------------------------------------------------------------------
void check_every_cell_traversed_its_grid() {
    for (const ProblemCells &p : corpus()) {
        const bool truncated = is_truncated_warm_arm(p.name);
        for (int a = 0; a < kArmCount; ++a) {
            for (int f = 0; f < 2; ++f) {
                for (int s = 0; s < 2; ++s) {
                    const SweepCell &c = p.cell[a][f][s];
                    SCOPED_TRACE(fmt::format("{}/{} KD={} SOC={}", p.name, arm_name(a), f, s));
                    if (truncated && a == kArmWarm) {
                        // EXACTLY ONE failure, at EXACTLY the pinned parameter
                        // value, on EVERY lever setting -- so the truncation is
                        // a property of the path and not of anything under A/B.
                        EXPECT_FALSE(c.reached_p1);
                        EXPECT_EQ(1, c.status_failures);
                        ASSERT_FALSE(c.pgrid.empty());
                        EXPECT_NEAR(kHs15WarmArmStopsAt, c.pgrid.back(), 1e-12);
                    } else {
                        EXPECT_TRUE(c.reached_p1);
                        EXPECT_EQ(0, c.status_failures);
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------------
// (2) EVERY ARM VISITED THE SAME PARAMETER VALUES -- the O-7 discipline's
// requirement, asserted rather than assumed. Every cross-arm and cross-lever
// number below is a difference between cells; if two cells swept different
// grids, that difference would be measuring the grid instead of the lever.
// The pinned grid (hs_sweeps.h's HsSweepSpec note) is what makes this true by
// construction, so a failure here means the step controller re-gridded despite
// dp_init == dp_min == dp_max.
//
// WHAT IS LOAD-BEARING HERE. **EVERY CELL, warm and cold, is now compared
// against a grid rebuilt from the spec by spec_grid() with no reference to
// anything any driver did.** Phase-5 Task 7b's I-3 decoupling is what makes
// that true of the cold rows: through Task 7 run_cold_grid was HANDED its
// matched warm cell's pgrid, so the cold rows could not disagree and this
// check was narrower than its name (Task-7 review, M-3, which disclosed
// exactly that). They are load-bearing now.
//
// A TRUNCATED warm arm (kTruncatedWarmArms) walks a PREFIX of the spec grid,
// and the prefix is checked value-by-value exactly as the whole grid is. What
// a truncation may never do is RE-GRID the points it did visit, which is the
// property every cross-arm difference below rests on -- and it may no longer
// shorten the cold arm either, which is the property adjudication (b) rests on.
// ---------------------------------------------------------------------
void check_every_arm_visited_the_same_grid() {
    for (const ProblemCells &p : corpus()) {
        const std::vector<double> full = spec_grid(hs_sweep_spec(p.number));
        for (int a = 0; a < kArmCount; ++a) {
            for (int f = 0; f < 2; ++f) {
                for (int s = 0; s < 2; ++s) {
                    const std::vector<double> &got = p.cell[a][f][s].pgrid;
                    SCOPED_TRACE(fmt::format("{}/{} KD={} SOC={}", p.name, arm_name(a), f, s));
                    // Only a truncated WARM arm may be short, and only ever a
                    // prefix. The cold arm is never allowed to be short at all.
                    if (a == kArmWarm && is_truncated_warm_arm(p.name)) {
                        ASSERT_LT(got.size(), full.size());
                    } else {
                        ASSERT_EQ(full.size(), got.size());
                    }
                    for (std::size_t i = 0; i < got.size(); ++i) {
                        EXPECT_NEAR(full[i], got[i], 1e-12) << "step " << i;
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------------
// (3a) THE B-1 REGRESSION, AT CORPUS LEVEL, STATED ON THE TERM THE DEFECT
// ACTUALLY VIOLATED. **EVERY POINT A WARM SOLVE CERTIFIES MUST BE A GENUINE
// KKT POINT OF THE MODEL AT ITS OWN p, COMPLEMENTARITY INCLUDED.** That is the
// property B-1 broke -- its signature is an O(1) complementarity against
// stationarity and feasibility at roundoff -- and it is checked here by
// support/nlp_kkt_check.h, which recomputes the whole quadruple FROM THE MODEL
// and never reads the driver's own residual, so a driver that mismeasured
// cannot certify itself.
//
// IT RUNS ON THE DIRECT WARM CHAIN, not on the sweep cells, for the reason
// run_warm_chain's own note gives: ContinuationStep carries no multipliers, so
// the sweep vehicle CANNOT compute a complementarity at all. Task 7 hit that
// same wall and reported the defect's decimals from a hand-built chain for
// exactly this reason (the note's §1.3 footnote); this section makes that
// vehicle a permanent assertion instead of a one-off diagnostic.
//
// PRE-REPAIR THIS SECTION FAILS ON THREE PROBLEMS, with worst complementarity
// 1.50 (HS10), 716 (HS15) and 0.500 (HS33) against a bar of 1e-6.
// ---------------------------------------------------------------------
void check_every_warm_solve_is_a_genuine_kkt_point() {
    const std::vector<HsSweepSpec> &specs = hs_sweep_specs();
    ASSERT_EQ(specs.size(), warm_chains().size());
    for (std::size_t k = 0; k < specs.size(); ++k) {
        const std::vector<ChainPoint> &chain = warm_chains()[k];
        ASSERT_FALSE(chain.empty()) << specs[k].name;
        Index warm_points = 0;
        for (const ChainPoint &pt : chain) {
            SCOPED_TRACE(fmt::format("{} at p = {}", specs[k].name, pt.p));
            if (pt.status != SqpStatus::kOptimal) {
                // Only kTruncatedWarmArms may hold a non-certified point, and
                // section (1) pins WHERE. A point that was not certified makes
                // no claim, so nothing is asserted about its residuals.
                EXPECT_TRUE(is_truncated_warm_arm(specs[k].name)) << to_string(pt.status);
                continue;
            }
            if (pt.level != StartLevel::kCold) {
                ++warm_points;
            }
            EXPECT_LT(pt.kkt.stationarity, 1e-5);
            EXPECT_LT(pt.kkt.primal, 1e-5);
            EXPECT_LT(pt.kkt.dual_sign, 1e-9);
            EXPECT_LT(pt.kkt.complementarity, 1e-6)
                << "THE B-1 TERM. Pre-repair this read 1.50 (HS10), 716 (HS15), 0.500 (HS33)";
        }
        // THE CHAIN REALLY IS WARM -- without this the section above would be
        // the trivial observation that cold solves are clean (section (4)).
        EXPECT_GT(warm_points, 0) << specs[k].name << ": no warm ingest resolved at all";
    }
}

// ---------------------------------------------------------------------
// (3b) THE WARM ARM AGAINST THE COLD ARM, ON THE ANSWER. The cold arm never
// ingests a multiplier, so it could not reach B-1, and its points are checked
// against the model's own KKT conditions in section (4).
//
// **THIS IS A WEAKER STATEMENT THAN (3a) AND IT IS DELIBERATELY SCOPED.** Two
// arms agreeing on the objective is evidence only when the problem HAS a
// unique answer to agree on. HS15's feasible set is DISCONNECTED -- that is
// why it is in the corpus (hs_sweeps.h's THE SIX PROBLEMS) -- and the two arms
// are asking different questions of it: the warm arm FOLLOWS one connected
// branch by continuation, while the cold arm RESTARTS from the published start
// point at every p and may land on another component. Both return genuine KKT
// points; they are simply not the same one, and calling that a defect would be
// calling nonconvexity a defect.
//
// So HS15 is excluded from the agreement bar and gets its own pin instead: the
// two arms are asserted to agree while they are on the same branch and to
// DIVERGE, with both points certified, once they are not.
//
// UNTIL TASK 7b THIS SECTION WAS THE DEFECT PIN, holding the three broken arms
// to a WRONG answer exactly as observed (the Phase-3 HS26 precedent,
// docs/notes/2026-07-29-hs-battery-results.md's "Deviations from the brief"
// item 4) so that a repair would fail it and have to come back. It did, and
// this is the updated form.
// ---------------------------------------------------------------------
void check_warm_arm_objective_against_the_cold_arm() {
    ASSERT_TRUE(kDefectiveWarmArms.empty())
        << "B-1 is repaired; a non-empty list here needs a new pin, not a re-run";
    for (const ProblemCells &p : corpus()) {
        const SweepCell &warm = p.cell[kArmWarm][kOn][kOn];
        const SweepCell &cold = p.cell[kArmCold][kOn][kOn];
        // The cold arm sweeps the WHOLE spec grid (I-3's decoupling), so a
        // truncated warm arm is compared over its own PREFIX of it -- the grid
        // values are asserted equal point-by-point in section (2), so index i
        // is the same parameter value on both arms.
        ASSERT_LE(warm.fvals.size(), cold.fvals.size()) << p.name;
        if (is_truncated_warm_arm(p.name)) {
            continue; // pinned separately, just below
        }
        ASSERT_EQ(warm.fvals.size(), cold.fvals.size()) << p.name;
        double worst_rel = 0.0;
        for (std::size_t i = 0; i < warm.fvals.size(); ++i) {
            worst_rel = std::max(worst_rel, std::abs(warm.fvals[i] - cold.fvals[i]) /
                                                std::max(1.0, std::abs(cold.fvals[i])));
        }
        EXPECT_TRUE(is_sound_warm_arm(p.name)) << p.name;
        EXPECT_LT(worst_rel, 1e-6)
            << p.name << ": the warm arm must agree with the cold arm on the ANSWER; "
            << "worst relative objective gap " << worst_rel << corpus_table();
    }

    // HS15, THE DISCONNECTED CASE, pinned rather than excused.
    const ProblemCells &hs15 = problem("HS15");
    const SweepCell &warm = hs15.cell[kArmWarm][kOn][kOn];
    const SweepCell &cold = hs15.cell[kArmCold][kOn][kOn];
    ASSERT_GE(warm.fvals.size(), 4u);
    // p = 0 and p = 0.25: both arms are on the SAME branch and agree exactly.
    for (std::size_t i = 0; i < 2; ++i) {
        EXPECT_LT(std::abs(warm.fvals[i] - cold.fvals[i]) / std::max(1.0, std::abs(cold.fvals[i])),
                  1e-6)
            << "HS15 step " << i << ": the arms start on one branch" << corpus_table();
    }
    // p = 0.5 and p = 0.75: the cold arm has hopped to the other component. The
    // values are pinned so a future change that moves EITHER arm says so here.
    // The warm arm's points are certified KKT points by (3a); the cold arm's by
    // (4). The cold arm's are the LOWER objective, which is worth recording:
    // continuation TRACKS A BRANCH, it does not search for a global minimum.
    EXPECT_NEAR(144.39418, warm.fvals[2], 1e-4) << corpus_table();
    EXPECT_NEAR(56.5, cold.fvals[2], 1e-4) << corpus_table();
    EXPECT_NEAR(63.959044, warm.fvals[3], 1e-4) << corpus_table();
    EXPECT_NEAR(6.5, cold.fvals[3], 1e-4) << corpus_table();
    EXPECT_LT(cold.fvals[2], warm.fvals[2]) << "the restart finds the better component";
    EXPECT_LT(cold.fvals[3], warm.fvals[3]) << "the restart finds the better component";
}

// ---------------------------------------------------------------------
// (4) THE COLD ARM IS A CLEAN ORACLE. Every converged cold solve is checked
// against the model's own KKT conditions by support/nlp_kkt_check.h, which
// recomputes the whole quadruple from the model and never reads the driver's
// own residual -- so a driver that mismeasured cannot certify itself. THIS IS
// WHAT ENTITLES SECTION (3) TO TREAT THE COLD ARM AS THE ANSWER.
//
// COMPLEMENTARITY IS CHECKED HERE and it is the term the defect violates.
// On the cold arm it is at roundoff everywhere; on the warm arm the defective
// problems reach O(1) (measured up to 716 on HS15 -- see the note's §1.3).
// ---------------------------------------------------------------------
void check_the_cold_arm_is_a_clean_kkt_oracle() {
    for (const ProblemCells &p : corpus()) {
        for (int f = 0; f < 2; ++f) {
            for (int s = 0; s < 2; ++s) {
                const SweepCell &c = p.cell[kArmCold][f][s];
                EXPECT_LT(c.worst_stationarity, 1e-5) << p.name << " KD=" << f << " SOC=" << s;
                EXPECT_LT(c.worst_primal, 1e-5) << p.name << " KD=" << f << " SOC=" << s;
                EXPECT_LT(c.worst_complementarity, 1e-6) << p.name << " KD=" << f << " SOC=" << s;
            }
        }
    }
}

// ---------------------------------------------------------------------
// (5) THE FULL-STEP (KUNGURTSEV-DIEHL) A/B -- ADJUDICATION (a)'s DATA, NOW ON
// THE WHOLE CORPUS (Phase-5 Task 7b's re-adjudication).
//
// WHAT IS MEASURED: on every one of the six problems, the lever is flipped with
// the grid pinned and every ledger count compared. The result REPLICATES THE
// PHASE-4 BATTERY'S NULL and Task 7's own partial one: the mode ENGAGES
// (41-75 % of majors) and its watchdog NEVER fires, and NOT ONE OBSERVED COUNT
// MOVES.
//
// WHY THIS RUN IS THE ONE THAT SETTLES IT. Task 7 could only measure the three
// equality-constrained members, because B-1 left the other three doing no warm
// solves at all -- and it named that as the gap, precisely because both of
// Task-5-P4's arguments for and against the rule (mispriced duals; misleading
// linearizations) live where elastic activations and rejected trials live,
// i.e. in the three problems the defect had removed. THOSE THREE ARE NOW
// MEASURED: HS10's warm arm carries 8 elastic activations and 48 rho
// escalations, HS15's carries an elastic activation AND a rejected trial, and
// the lever still changes nothing on either.
// ---------------------------------------------------------------------
void check_full_step_lever_on_the_sound_warm_arms() {
    ASSERT_EQ(6u, kSoundWarmArms.size()) << "the whole corpus is in scope after the B-1 repair";
    for (const char *name : kSoundWarmArms) {
        const ProblemCells &p = problem(name);
        const SweepCell &on = p.cell[kArmWarm][kOn][kOn];
        const SweepCell &off = p.cell[kArmWarm][kOff][kOn];
        // THE MODE GENUINELY ENGAGED with the lever on, and genuinely did not
        // with it off. Without this the neutrality below would be the trivial
        // observation that an inert lever is inert.
        EXPECT_GT(on.full_step_majors, 0) << name;
        EXPECT_EQ(0, off.full_step_majors) << name;
        EXPECT_LE(on.full_step_majors, on.majors) << name;
        // NEUTRAL IN EVERY OBSERVED COUNT.
        EXPECT_EQ(on.majors, off.majors) << name << corpus_table();
        EXPECT_EQ(on.minors, off.minors) << name << corpus_table();
        EXPECT_EQ(on.factorizations, off.factorizations) << name << corpus_table();
        EXPECT_EQ(on.rejected, off.rejected) << name << corpus_table();
        EXPECT_EQ(on.n_warm, off.n_warm) << name << corpus_table();
        EXPECT_EQ(on.solves, off.solves) << name << corpus_table();
        EXPECT_EQ(on.status_failures, off.status_failures) << name << corpus_table();
        EXPECT_EQ(on.elastic_activations, off.elastic_activations) << name << corpus_table();
        EXPECT_EQ(on.elastic_escalations, off.elastic_escalations) << name << corpus_table();
        EXPECT_EQ(on.fvals, off.fvals)
            << name << ": the ANSWER is lever-independent too" << corpus_table();
        // THE WATCHDOG NEVER FIRED ANYWHERE IN THE CORPUS -- the same reading
        // the Phase-4 battery got, now on nonconvex problems.
        EXPECT_EQ(0, on.watchdog) << name;
        EXPECT_EQ(0, off.watchdog) << name;
    }
    // The engagement rates the note quotes, pinned so "the mode engaged" is a
    // measured claim rather than a remembered one. RE-MEASURED, Phase-5 Task 7b
    // (the B-1 repair): HS10/HS15/HS33 are new rows -- under the defect their
    // warm arms performed no majors past their first point and so had no
    // engagement to report -- and HS26/HS40/HS77 are UNCHANGED, which is the
    // control saying the repair did not disturb the equality-only third.
    EXPECT_EQ(26, problem("HS10").cell[kArmWarm][kOn][kOn].full_step_majors);
    EXPECT_EQ(40, problem("HS10").cell[kArmWarm][kOn][kOn].majors);
    EXPECT_EQ(21, problem("HS15").cell[kArmWarm][kOn][kOn].full_step_majors);
    EXPECT_EQ(28, problem("HS15").cell[kArmWarm][kOn][kOn].majors);
    EXPECT_EQ(12, problem("HS26").cell[kArmWarm][kOn][kOn].full_step_majors);
    EXPECT_EQ(29, problem("HS26").cell[kArmWarm][kOn][kOn].majors);
    EXPECT_EQ(12, problem("HS33").cell[kArmWarm][kOn][kOn].full_step_majors);
    EXPECT_EQ(16, problem("HS33").cell[kArmWarm][kOn][kOn].majors);
    EXPECT_EQ(12, problem("HS40").cell[kArmWarm][kOn][kOn].full_step_majors);
    EXPECT_EQ(16, problem("HS40").cell[kArmWarm][kOn][kOn].majors);
    EXPECT_EQ(13, problem("HS77").cell[kArmWarm][kOn][kOn].full_step_majors);
    EXPECT_EQ(25, problem("HS77").cell[kArmWarm][kOn][kOn].majors);
    // THE GAP TASK 7 NAMED IS CLOSED: the lever is now measured on warm arms
    // where the ELASTIC TIER runs and where a trial is REJECTED, which is where
    // both of the rule's competing mechanisms were argued to live.
    EXPECT_GT(problem("HS10").cell[kArmWarm][kOn][kOn].elastic_activations, 0);
    EXPECT_GT(problem("HS15").cell[kArmWarm][kOn][kOn].elastic_activations, 0);
    EXPECT_GT(problem("HS15").cell[kArmWarm][kOn][kOn].rejected, 0);
    // On a COLD arm the mode can never arm (there is no warm ingest), which is
    // the control that says full_step_majors is reporting the mode and not
    // some other property of the solve.
    for (const ProblemCells &p : corpus()) {
        EXPECT_EQ(0, p.cell[kArmCold][kOn][kOn].full_step_majors) << p.name;
    }
}

// ---------------------------------------------------------------------
// (6) THE SECOND-ORDER-CORRECTION OUTCOMES -- ADJUDICATION (b)'s DATA.
//
// The cells read here are the whole cold arm plus every warm arm. UNDER B-1
// THIS SECTION READ ONLY THE COLD ARM AND THE THREE SOUND WARM ARMS, because
// the three defective warm arms performed no majors past their first solve and
// so could not attempt a correction. The B-1 repair (Phase-5 Task 7b) makes
// all six warm arms real, and THE OUTCOME PARTITION DID NOT MOVE: the three
// repaired arms attempt no correction either (a correction is attempted only
// on a rejected trial that INCREASED the violation, and HS10/HS33's warm arms
// reject nothing while HS15's one rejection does not qualify), so adjudication
// (b)'s population is still 9 attempts and still the same 9.
//
// THAT POPULATION IS NO LONGER CONTINGENT ON HS15's TRUNCATION POINT, and it
// briefly was (Task-7b review, I-3). Two of the nine attempts are HS15 cold
// solves, and while run_cold_grid was handed the warm cell's grid they survived
// only because they sit at p <= 1, i.e. at exactly where the warm arm dies. The
// cold arm is now on the spec's own grid (corpus()), so a warm arm that stopped
// earlier -- on another backend, say -- can no longer shorten this population.
//
// THE PHASE-3 PRIOR IS CONTRADICTED, and this is the section that does it.
// docs/notes/2026-07-29-hs-battery-results.md carried forward a "~70 % of SOC
// re-solves come back kInfeasible" figure from Phase-3 Task 7's own fixtures
// (sqp_driver.h's own note puts it at 79 % of 19). ZERO of this corpus's
// attempts return kInfeasible: every one of them re-solves to kOptimal and is
// then REJECTED BY THE FUNNEL on the corrected point.
// ---------------------------------------------------------------------
void check_soc_outcome_partition() {
    Index attempts = 0, applied = 0, infeasible = 0, rejected = 0;
    const auto tally = [&](const SweepCell &c) {
        attempts += c.soc_steps;
        applied += c.soc_applied;
        infeasible += c.soc_qp_infeasible;
        rejected += c.soc_rejected;
        // sqp_types.h's own stated invariant on the partition, re-checked here
        // per cell rather than only in aggregate.
        EXPECT_EQ(c.soc_steps, c.soc_applied + c.soc_qp_infeasible + c.soc_rejected);
    };
    for (const ProblemCells &p : corpus()) {
        tally(p.cell[kArmCold][kOn][kOn]);
        tally(p.cell[kArmWarm][kOn][kOn]);
        // SOC OFF MUST MEAN NO ATTEMPTS, everywhere -- the control on the axis.
        for (int a = 0; a < kArmCount; ++a) {
            EXPECT_EQ(0, p.cell[a][kOn][kOff].soc_steps) << p.name << "/" << arm_name(a);
            EXPECT_EQ(0, p.cell[a][kOff][kOff].soc_steps) << p.name << "/" << arm_name(a);
        }
    }
    // THE HEADLINE PARTITION.
    EXPECT_EQ(9, attempts) << corpus_table();
    EXPECT_EQ(0, applied) << corpus_table();
    EXPECT_EQ(0, infeasible) << "the Phase-3 ~70-79 % kInfeasible prior does NOT reproduce here"
                             << corpus_table();
    EXPECT_EQ(9, rejected) << corpus_table();
    // WHERE THEY COME FROM: two problems only, exactly the two the Phase-3
    // battery's own SOC signal pointed at (HS77) plus the one whose start point
    // is infeasible for a product row (HS15).
    EXPECT_EQ(2, problem("HS15").cell[kArmCold][kOn][kOn].soc_steps);
    EXPECT_EQ(5, problem("HS77").cell[kArmCold][kOn][kOn].soc_steps);
    EXPECT_EQ(2, problem("HS77").cell[kArmWarm][kOn][kOn].soc_steps);
    for (const ProblemCells &p : corpus()) {
        if (p.name != "HS15" && p.name != "HS77") {
            EXPECT_EQ(0, p.cell[kArmCold][kOn][kOn].soc_steps) << p.name;
        }
    }
}

// ---------------------------------------------------------------------
// (7) WHAT SOC COSTS -- the enable_soc A/B, which is what prices adjudication
// (b)'s "waste". Turning the lever off is the MAXIMALLY AGGRESSIVE gate (it
// blocks every attempt), so the difference between the two cells is exactly
// the total price of the always-attempt policy on this corpus, with no
// modelling assumption in between.
//
// THE MAJORS ARE IDENTICAL IN EVERY CELL, which is the substance of "zero
// benefit observed": across 9 attempts not one changed the trajectory, because
// not one was applied. What SOC bought was 23 extra QP minor iterations and 4
// extra factorizations.
// ---------------------------------------------------------------------
void check_soc_costs_what_it_costs() {
    Index extra_minors = 0, extra_factorizations = 0, total_minors = 0;
    for (const ProblemCells &p : corpus()) {
        for (int a = 0; a < kArmCount; ++a) {
            const SweepCell &on = p.cell[a][kOn][kOn];
            const SweepCell &off = p.cell[a][kOn][kOff];
            EXPECT_EQ(on.majors, off.majors) << p.name << "/" << arm_name(a)
                                             << ": SOC changed the major count" << corpus_table();
            EXPECT_GE(on.minors, off.minors) << p.name << "/" << arm_name(a);
            EXPECT_GE(on.factorizations, off.factorizations) << p.name << "/" << arm_name(a);
            extra_minors += on.minors - off.minors;
            extra_factorizations += on.factorizations - off.factorizations;
            total_minors += on.minors;
        }
    }
    // UNCHANGED BY THE B-1 REPAIR (Phase-5 Task 7b), and that is a result
    // rather than an accident: every SOC attempt in this corpus is on a cell
    // the repair does not touch (the cold arm) or on HS77's warm arm, which was
    // sound all along.
    EXPECT_EQ(23, extra_minors) << corpus_table();
    EXPECT_EQ(4, extra_factorizations) << corpus_table();
    // The denominator the note's percentage is computed against, pinned so the
    // percentage is reproducible rather than recomputed by hand.
    //
    // RE-MEASURED, Phase-5 Task 7b (the B-1 repair): 3571 -> 3719. The three
    // repaired warm arms now perform real work. The increment is +148, and it
    // is what each arm ADDED rather than what it now totals: HS10 218 -> 271
    // (+53), HS15 37 -> 92 (+55), HS33 16 -> 56 (+40), and 53 + 55 + 40 = 148
    // is what closes 3571 -> 3719. (An earlier version of this comment quoted
    // the three TOTALS as though they were the increments -- Task-7b review
    // round 2, N-3.) HS15's cold arm is UNCHANGED at 1160 because the I-3
    // decoupling (see corpus()) keeps it on the spec's own grid rather than on
    // the warm arm's truncated one. THE SOC NUMERATOR ABOVE DOES NOT MOVE AT
    // ALL. 23/3719 = 0.62 %.
    //
    // (An intermediate reading of 3654 exists in this task's own history, taken
    // before the decoupling, when HS15's cold arm was still shortened to 1095
    // minors by its warm arm's truncation. It is superseded and appears only in
    // the note's revision history.)
#ifdef USE_ACCELERATE_SPARSE
    // Origin divergence entry D17. The note that carried it
    // (docs/notes/2026-07-31-accelerate-second-pass-results.md) did NOT
    // migrate into hven, and the register that succeeds it here
    // (docs/notes/2026-08-14-accelerate-divergence-register.md, "Why this
    // file exists at this path") has no row for D17 -- the observation is
    // quoted in full below, and this comment is its citable record here.
    // What was observed: this is
    // the corpus's ONLY failing assertion on Accelerate. Every abridged row
    // is byte-identical to the Linux measurement except HS26, which forks by
    // exactly one minor in each of its two SOC-on cells (cold 88/190/88 ->
    // 88/191/88 on MKL, warm 29/60/29 -> 29/61/29 on MKL) -- the same single
    // HS26 trajectory fork that dominates origin entry D16 (quoted at
    // tests/sqp/test_b1_gate.cpp's own Accelerate arm, same not-migrated
    // origin). Majors and factorizations are
    // unchanged in both cells, and the SOC numerator this test also pins
    // (extra_minors == 23, extra_factorizations == 4, two lines above) holds
    // EXACTLY on this backend -- the fork is confined to the denominator.
    // The origin's quoted 23/3719 = 0.62 % is unaffected at its own precision
    // by the -2 total here (23/3717 = 0.62 % also).
    EXPECT_EQ(3717, total_minors) << corpus_table();
#else
    EXPECT_EQ(3719, total_minors) << corpus_table();
#endif
}

// ---------------------------------------------------------------------
// (8) THE SOC TRIGGER RATIOS -- the variable a kappa_soc gate would test, and
// the only thing that can turn adjudication (b) from a rate into a THRESHOLD.
//
// The gate sqp_driver.h names (A NAMED CANDIDATE FOR TASK 11) is
// `h_raw <= kappa_soc * h_old`, so what has to be measured is h_raw/h_old at
// each attempt. Neither number is in SqpCounters, but BOTH are in the
// StepContext the globalization strategy is handed, so a RECORDING DECORATOR
// on SqpOptions::make_strategy reads them without touching the engine.
//
// TWO THINGS MAKE THE READING TRUSTWORTHY:
//   (a) THE DECORATOR IS RUN ON THE COLD ARM ONLY. sqp_driver.h reaches for
//       `dynamic_cast<FunnelStrategy *>` in THREE places, and a decorator is
//       not a FunnelStrategy, so each is silently skipped when one is
//       installed:
//         :3895  the warm funnel-width re-basing   } warm-ingest only, so
//         :3908  the full-step arming              } unreachable on a cold solve
//         :5109  make_warm_start's funnel->width() read, which runs on EVERY
//                exit INCLUDING COLD -- so a decorated cold solve emits a
//                WarmStart whose funnel_width stays at the unset sentinel.
//       (Line numbers as of commit d83a8ed; sqp_driver.h has grown before and
//       will again -- find the three sites by `dynamic_cast<.*FunnelStrategy`
//       if these have drifted.)
//       An earlier version of this comment said "exactly two places" and
//       missed the third (Task-7 review, I-2; it is spelled
//       `dynamic_cast<const FunnelStrategy *>`, which is why a grep for the
//       non-const form found only two). IT DOES NOT AFFECT THIS MEASUREMENT --
//       the decorated solves' warm objects are never consumed and every solve
//       here starts cold from the model's own start_point() -- but the
//       technique WOULD silently break if reused on a CHAIN of solves, which
//       is exactly what a later reader would try.
//   (b) IT IS CHECKED, NOT ARGUED: every solve below is run twice, once
//       decorated and once not, and the counters are asserted equal. (b), not
//       (a), is what actually establishes inertness; (a) explains why.
//
// PAIRING AN ATTEMPT TO ITS CORRECTED JUDGE CALL uses pred_df: a SOC-corrected
// judge() reuses the REJECTED trial's own pred_df unchanged (sqp_driver.h's
// SECOND-ORDER CORRECTION note, pinned exactly by
// SqpDriverSoc.SocDefeatsMaratos), while a shrink-and-retry at the same
// iterate solves a new QP against a different one.
// ---------------------------------------------------------------------
// THE THIRD NEAR-IDENTICAL RECORDING STRATEGY IN THE TEST TREE
// (test_sqp_driver.cpp's RecordingStrategy, test_sqp_restoration.cpp's), and
// the Task-7 review is right that it belongs in tests/sqp/support/ (M-6). It is
// left here deliberately: hoisting it means editing two other test files, which
// is a refactor rather than a fix and does not belong in this task's fix round.
// Carried in the note's §5. It compiles cleanly as it stands -- all three live
// in anonymous namespaces in separate TUs.
struct JudgeLog {
    std::vector<StepContext> ctx;
    std::vector<StepVerdict> verdict;
};

class RecordingFunnel final : public GlobalizationStrategy {
  public:
    explicit RecordingFunnel(JudgeLog *log) : log_(log) {}
    void reset(double h0) override { inner_.reset(h0); }
    void resume_from_restoration(double h) override { inner_.resume_from_restoration(h); }
    StepVerdict judge(const StepContext &c) override {
        const StepVerdict v = inner_.judge(c);
        log_->ctx.push_back(c);
        log_->verdict.push_back(v);
        return v;
    }

  private:
    FunnelStrategy inner_;
    JudgeLog *log_;
};

void check_soc_trigger_ratios() {
    std::vector<double> ratios;
    for (const HsSweepSpec &spec : hs_sweep_specs()) {
        auto model = make_hs_sweep(spec.number);
        for (double p = spec.p0; p <= spec.p1 + 1e-12; p += spec.dp) {
            model->set_parameters(Vec::Constant(1, p));
            JudgeLog log;
            SqpOptions opts = sweep_options(StartLevel::kCold, true, true, spec.max_iter);
            opts.make_strategy = [&log]() -> std::unique_ptr<GlobalizationStrategy> {
                return std::make_unique<RecordingFunnel>(&log);
            };
            SqpDriver decorated(opts);
            const SqpSolution sd = decorated.solve(*model, model->start_point());

            // (b): the decorator is inert on a cold solve. Asserted, not argued.
            SqpDriver plain(sweep_options(StartLevel::kCold, true, true, spec.max_iter));
            const SqpSolution sp = plain.solve(*model, model->start_point());
            ASSERT_EQ(sp.counters.major_iters, sd.counters.major_iters) << spec.name << " p=" << p;
            ASSERT_EQ(sp.counters.qp_minor_iters, sd.counters.qp_minor_iters)
                << spec.name << " p=" << p;
            ASSERT_EQ(sp.counters.soc_steps, sd.counters.soc_steps) << spec.name << " p=" << p;

            for (std::size_t i = 0; i + 1 < log.ctx.size(); ++i) {
                const StepContext &trial = log.ctx[i];
                if (log.verdict[i] != StepVerdict::kReject || trial.h_new <= trial.h_old) {
                    continue; // not a SOC-qualifying rejection
                }
                if (log.ctx[i + 1].pred_df != trial.pred_df ||
                    log.ctx[i + 1].h_old != trial.h_old) {
                    continue; // a shrink-and-retry, not a correction
                }
                ratios.push_back(trial.h_new / trial.h_old);
            }
        }
    }
    std::sort(ratios.begin(), ratios.end());
    // THE MEASURED SET, pinned to four significant figures. Its size (8)
    // EXCEEDS the cold arm's 7 SOC attempts BY ONE: HS15 at p = 1 rejects three
    // consecutive trials at one iterate, and the pred_df pairing rule cannot
    // tell which two of the three were the SOC attempts the counters report,
    // because at that iterate all three share both pred_df and h_old. The
    // note's §4.3 states that attribution limit rather than hiding it, and the
    // ADJUDICATION DOES NOT DEPEND ON IT: the extra entry (2.818) duplicates a
    // ratio already in the set, and the two ratios a threshold would actually
    // be set against -- the largest wasted one below the one known success and
    // the smallest one above it -- are both unambiguous HS15/HS77 readings.
    ASSERT_EQ(8u, ratios.size()) << fmt::format("{:.4g}", fmt::join(ratios, ", "));
    const std::vector<double> expected = {2.198, 2.198, 2.818, 2.818, 8.461, 28.01, 28.62, 29.36};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_NEAR(expected[i], ratios[i], 5e-3 * expected[i])
            << "ratio " << i << " of " << fmt::format("{:.4g}", fmt::join(ratios, ", "));
    }
    // THE DERIVATION'S LOAD-BEARING NUMBER: the largest ratio at which a SOC
    // attempt was WASTED and still sits BELOW the one ratio at which SOC has
    // ever been APPLIED anywhere in this project (9.115, measured on
    // SqpDriverSoc.SocDefeatsMaratos's Maratos fixture: h_old = 6.229194e-4,
    // h_raw = 5.678149e-3). Any kappa_soc large enough to preserve that rescue
    // is also large enough to admit this waste -- which is the whole
    // adjudication. See the note's §4.3.
    EXPECT_LT(ratios[4], 9.115) << "the gate cannot separate the one success from this waste";
    EXPECT_GT(ratios[5], 9.115) << "only the three largest ratios could ever be gated out";
}

TEST(HsSweepCorpus, Corpus) {
    check_every_cell_traversed_its_grid();
    check_every_arm_visited_the_same_grid();
    check_every_warm_solve_is_a_genuine_kkt_point();
    check_warm_arm_objective_against_the_cold_arm();
    check_the_cold_arm_is_a_clean_kkt_oracle();
    check_full_step_lever_on_the_sound_warm_arms();
    check_soc_outcome_partition();
    check_soc_costs_what_it_costs();
    check_soc_trigger_ratios();
    std::cout << corpus_table() << std::endl;
}

// ---------------------------------------------------------------------
// THE MINIMAL REPRODUCTION of the defect this file's WHAT THIS CORPUS FOUND
// FIRST note describes, NOW ASSERTING ITS REPAIR: TWO SOLVES, no continuation
// driver, no ledger, no corpus. HS10 is the cleanest instance in the corpus and
// the reason is exact arithmetic rather than luck:
//
//     f(x) = x1 - x2,  cI(x) = 3x1^2 - 2x1x2 + x2^2 - 1 - p <= 0
//
// grad f = (1, -1) is CONSTANT, and at x = (0, 1) the constraint gradient is
// Ji = (-2x2, -2x1 + 2x2)|_(0,1) = (-2, 2), so lambda_i = 1/2 satisfies
// stationarity EXACTLY, FOR EVERY p. Raising p makes the row strictly slack at
// that point without disturbing stationarity at all.
//
// **UNTIL PHASE-5 TASK 7b THIS TEST PINNED THE BUG**, as observed, with an
// explicit marker (the Phase-3 HS26 precedent): the p = 0.5 warm solve returned
// kOptimal in ZERO majors at the p = 0 solution, with stationarity 4.19e-08,
// primal 0 and COMPLEMENTARITY 0.25 -- an 18 % objective error reported as
// success -- and the whole sweep then froze from f = -1 at p = 0 to a claimed
// f = -1 at p = 3, where the truth is f = -2.
//
// IT NOW PINS THE REPAIR, on the same two solves, and the four assertions that
// used to record the defect are the four that record the fix: the row is
// strictly slack at the ingested point, so its stale price is cleared, the
// cleared price no longer cancels grad f, the solve does real work, and the
// point it certifies is the cold arm's.
// ---------------------------------------------------------------------
TEST(HsSweepRepair, WarmSolveOnAReleasedRowNoLongerCertifiesTheOldPoint) {
    auto model = make_hs_sweep(10);
    SqpOptions opts;
    opts.kkt_tol = 1e-6;
    opts.feas_tol = 1e-6;
    opts.max_iter = 60;
    opts.adaptive_mu = false;
    opts.start_level = StartLevel::kWarm;

    // --- solve at p = 0: the published HS10, solved from its own start point.
    model->set_parameters(Vec::Constant(1, 0.0));
    SqpDriver d0(opts);
    const SqpSolution s0 = d0.solve(*model, model->start_point());
    ASSERT_EQ(SqpStatus::kOptimal, s0.status);
    EXPECT_NEAR(-1.0, s0.f, 1e-6);
    EXPECT_NEAR(1.0, s0.x(1), 1e-6);
    ASSERT_EQ(1, s0.lambda_i.size());
    EXPECT_NEAR(0.5, s0.lambda_i(0), 1e-6) << "the exact multiplier derived above";

    // THE PRECONDITION OF THE WHOLE MECHANISM, checked from the model: at
    // p = 0.5 the row is strictly slack AT THE OLD POINT, by far more than
    // feas_tol, while the stale price is strictly positive.
    model->set_parameters(Vec::Constant(1, 0.5));
    EXPECT_NEAR(-0.5, model->eval_ci(s0.x)(0), 1e-6) << "released, by 5e5 x feas_tol";
    EXPECT_GT(s0.lambda_i(0), 0.0);

    // --- the SAME model at p = 0.5, warm-started from that solve.
    SqpDriver d1(opts);
    const SqpSolution s1 = d1.solve(*model, s0.x, s0.warm_start);

    // The warm start WAS ingested -- this is not a silently-cold solve, so the
    // repair is being exercised rather than bypassed.
    EXPECT_EQ(StartLevel::kWarm, s1.counters.start_level_used);
    // THE REPAIR, in four assertions -- the same four that used to record the
    // defect, inverted.
    EXPECT_EQ(SqpStatus::kOptimal, s1.status) << "reports success ...";
    EXPECT_GT(s1.counters.major_iters, 0) << "... having actually solved something ...";
    EXPECT_NEAR(-std::sqrt(1.5), s1.f, 1e-6) << "... at the NEW solution ...";
    const test_support::NlpKktResidual r = test_support::self_check_kkt(*model, s1, 1e-6);
    EXPECT_LT(r.stationarity, 1e-6);
    EXPECT_LT(r.primal, 1e-6);
    EXPECT_LT(r.complementarity, 1e-6)
        << "... whose COMPLEMENTARITY is now at tolerance. Pre-repair this read 0.25: "
           "lambda_i = 0.5 priced against cI = -0.5, on a term the convergence test "
           "does not gate. The repair does not gate it either -- it makes the ingested "
           "multipliers complementary BY CONSTRUCTION (sqp_driver.h's THE INGESTED "
           "MULTIPLIERS ARE MADE COMPLEMENTARY), which is what restores the "
           "stationarity/feasibility pair to a complete KKT test at that point.";

    // THE TRUTH, from a cold solve of the same problem: x*(0.5) = (0, sqrt(1.5)).
    SqpDriver d2(sweep_options(StartLevel::kCold, true, true, 60));
    const SqpSolution s2 = d2.solve(*model, model->start_point());
    ASSERT_EQ(SqpStatus::kOptimal, s2.status);
    EXPECT_NEAR(-std::sqrt(1.5), s2.f, 1e-6);
    const test_support::NlpKktResidual rc = test_support::self_check_kkt(*model, s2, 1e-6);
    EXPECT_LT(rc.complementarity, 1e-6);
    // The 18 % relative objective error is gone; the two arms now agree.
    EXPECT_LT(std::abs(s1.f - s2.f) / std::abs(s2.f), 1e-6) << "pre-repair this read 0.1835";
}

// ---------------------------------------------------------------------
// THE MODEL-LEVEL HALF of kTruncatedWarmArms' argument, so that section (1)'s
// pin rests on a property of HS15 rather than on the observation it explains.
//
// The branch HS15's continuation tracks sits on BOTH swept rows, so it follows
// x1 x2 = 1 - p intersected with x1 = -x2^2, and that curve reaches THE ORIGIN
// exactly at p = 1. There, two rows are active and the active-constraint
// Jacobian has rank 1 in R^2 -- LICQ fails -- because grad cI1 = (-x2, -x1)
// VANISHES IDENTICALLY at the origin. A solve asked to certify a point at which
// the constraint qualification does not hold has nothing to certify with, and
// kNumericalError is the honest answer. Nothing here is about warm starting.
// ---------------------------------------------------------------------
TEST(HsSweepModels, Hs15BranchEndsAtAPointWhereLicqFails) {
    auto model = make_hs_sweep(15);
    model->set_parameters(Vec::Constant(1, 1.0));
    const Vec origin = Vec::Zero(2);

    // BOTH rows are active at the origin when p = 1.
    const Vec ci = model->eval_ci(origin);
    ASSERT_EQ(2, ci.size());
    EXPECT_NEAR(0.0, ci(0), 1e-15) << "cI1 = 1 - x1 x2 - p = 0 at the origin for p = 1";
    EXPECT_NEAR(0.0, ci(1), 1e-15) << "cI2 = -x1 - x2^2 = 0 at the origin";

    // And the active-set Jacobian is rank 1: row 0 vanishes identically.
    const Eigen::MatrixXd ji(model->eval_jac_i(origin));
    EXPECT_EQ(0.0, ji.row(0).cwiseAbs().maxCoeff())
        << "grad cI1 = (-x2, -x1) is exactly zero at the origin -- LICQ fails";
    EXPECT_GT(ji.row(1).cwiseAbs().maxCoeff(), 0.0);
}

} // namespace
} // namespace hven::solvers
