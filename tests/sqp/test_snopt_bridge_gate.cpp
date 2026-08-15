// tests/test_snopt_bridge_gate.cpp — PHASE-6 TASK 0. THE REFEREE GATE for the
// SNOPT bridge.
//
// WHAT THIS FILE IS FOR. Phase 6 compares this SQP engine against SNOPT on F7, and
// every one of those comparisons is worthless if the bridge is solving a
// DIFFERENT problem than our own solver is. A transcription defect in
// bench/snopt_f7_driver.h -- an off-by-one in the coordinate arrays, a
// dropped constant, a sign flip on a defect row, an equality bound attached
// to the wrong row -- would not announce itself: SNOPT would happily converge,
// report INFO = 1, and hand back a confident answer to the wrong question.
// This gate is the thing that makes that impossible to miss, and it is why
// F7 carries a manufactured solution at all.
//
// THE REFEREE IS F7'S ANALYTIC OPTIMUM, not our solver's answer.
// tests/sqp/support/scale_problems.h derives x*(p), f*(p), the multipliers and the
// active set in closed form (its prose was NumPy-verified), so the assertion
// below compares SNOPT against MATHEMATICS rather than against our engine.
// That ordering matters: if this gate compared the two solvers to each other,
// a shared misreading of F7 would pass it.
//
// WHY THIS IS REGISTERED CONDITIONALLY. SNOPT is commercial and not vendored
// (see the driver header's firewall banner). Machines without it must keep
// building and testing normally, so tests/CMakeLists.txt adds this file to the
// test binary ONLY when the SNOPT directory exists, and the whole translation
// unit is additionally inert without HVEN_SQP_HAVE_SNOPT. On a machine
// without SNOPT the suite is unchanged at 446 tests; with SNOPT it is 450.
//
// TOLERANCE POSTURE -- "RECORD, DON'T FIGHT", AND WHAT THAT TURNED OUT TO
// MEAN HERE. The brief's gate criterion is x_err_inf < 1e-6 "at SNOPT default
// tolerances", with the standing instruction that SNOPT's defaults are looser
// than ours and are to be recorded rather than fought. On F7 the two halves of
// that sentence turn out to be in tension, and this file resolves it the way
// the instruction's second half directs.
//
// AT SNOPT'S OWN RESOLVED DEFAULTS the bridge lands 3.6e-5 from the
// manufactured optimum -- not 1e-6. That is not a defect and it is not ours:
// SNOPT's Major optimality tolerance defaults to 2.0e-6 on the installed
// 7.7.7 (MEASURED from its print file; the 2008 guide says 1.0e-6) and, far
// more importantly, BOTH major tolerances are RELATIVE where ours are
// absolute -- guide section 7.7 eq. (7.1) normalizes the feasibility test by
// ||x|| and eq. (7.2) normalizes the optimality test by ||pi||. At N = 30,
// ||x|| is order 10, so a 1e-6 relative test simply does not ask for 1e-6 of
// absolute accuracy.
//
// THE PROOF THAT IT IS TOLERANCE AND NOT TRANSCRIPTION is the second test
// below, and it is the most load-bearing assertion in this file: the achieved
// error TRACKS THE REQUESTED TOLERANCE monotonically across four orders of
// magnitude (3.6e-5 at the defaults, then 2.5e-6 / 1.4e-7 / 2.3e-10 at
// 1e-6 / 1e-8 / 1e-10). A mapping defect -- a dropped constant, a shifted
// row, an off-by-one coordinate -- would put a FLOOR under that sequence,
// because no amount of extra convergence recovers a constraint that was
// transcribed wrongly. There is no floor. The bridge solves F7, and it solves
// it as accurately as it is asked to.
//
// So the gate asserts the brief's 1e-6 criterion at a tolerance where 1e-6 is
// a meaningful request, and separately RECORDS what the defaults give. It
// does NOT assert anything about SNOPT's iteration counts, which are its
// business and are measured, never pinned, by bench_snopt_f7.
//
// Guide citations throughout are to Gill, Murray and Saunders, "User's Guide
// for SNOPT Version 7", June 16 2008; the installed library is 7.7.7. See the
// driver header for why the guide is cited for mechanism and measurement for
// defaults.

#ifdef HVEN_SQP_HAVE_SNOPT

#include <algorithm>
#include <cmath>

#include <gtest/gtest.h>

#include <Eigen/Core>

#include <hven/qp/qp_types.h>

#include "../../bench/snopt_f7_driver.h"

#include "support/scale_problems.h"

namespace {

using hven::solvers::Index;
using hven::solvers::Vec;
using hven::solvers::snopt_bridge::SnoptF7Driver;
using hven::solvers::snopt_bridge::SnoptOptions;
using hven::solvers::snopt_bridge::SnoptResult;
using hven::solvers::test_support::F7CollocationChain;

// The brief's gate instance: N = 30 nodes, p = 0.85. n = 5N = 150 variables,
// me = 3N = 90 equality rows, mi = N = 30 path rows. p = 0.85 > R/2 = 0.5 puts
// this in the WIDE-WINDOW regime, so a nonempty set of path rows is genuinely
// active at the optimum -- a gate at an empty-window p would never exercise
// the inequality transcription at all.
constexpr Index kNodes = 30;
constexpr double kP = 0.85;

// One cold solve at a stated tolerance, from F7's own start point. A negative
// `tol` leaves every SNOPT tolerance UNSET, i.e. measures SNOPT at its own
// resolved defaults.
SnoptResult solve_cold_at(F7CollocationChain &model, double tol) {
    SnoptOptions opts;
    if (tol > 0.0) {
        opts.major_feas_tol = tol;
        opts.major_opt_tol = tol;
        opts.minor_feas_tol = tol;
    }
    SnoptF7Driver driver(model, opts);
    driver.set_start_point(model.start_point());
    return driver.solve(/*start=*/0);
}

} // namespace

// THE GATE. The brief's criterion -- the bridge lands on F7's manufactured
// optimum to better than 1e-6 in the inf norm -- asserted at a tolerance where
// asking for 1e-6 of ABSOLUTE accuracy is a meaningful request. See the
// banner: at SNOPT's relative defaults it is not one.
TEST(SnoptBridgeGate, ColdSolveLandsOnTheManufacturedOptimum) {
    F7CollocationChain model(kNodes, 3, 2, kP, 1.0);
    const SnoptResult r = solve_cold_at(model, 1e-8);

    // SNOPT's own verdict first -- INFO = 1 is "optimality conditions
    // satisfied" (guide section 3.4, p.19).
    EXPECT_EQ(r.info, 1) << "SNOPT INFO = " << r.info << " (" << r.status << ")";

    // The referee.
    EXPECT_LT(r.x_err_inf, 1e-6) << "SNOPT's x is " << r.x_err_inf
                                 << " from F7's manufactured optimum in the inf norm";

    // The objective, scored by OUR model at SNOPT's x against f*(p). A bridge
    // that mapped the objective row wrongly could still satisfy the x test if
    // the constraints alone pinned the solution, so this is a separate check.
    EXPECT_LT(r.f_err_rel, 1e-9) << "relative objective error against f_star";

    // An ABSOLUTE primal infeasibility, computed from F7's own cE/cI at
    // SNOPT's x. SNOPT never reports this: its Major feasibility tolerance is
    // normalized by ||x|| (guide section 7.7, eq. 7.1), so its self-report and
    // this number are different quantities.
    EXPECT_LT(r.prim_inf_abs, 1e-9) << "absolute max(|cE|, cI+) at SNOPT's x";

    // The counters are MEASURED, never pinned (see this file's banner). They
    // must merely be self-consistent: a solve that reported optimality must
    // have taken at least one major, and the monitor must have been called --
    // if snSTOP were never invoked the counters would silently read zero and
    // every SNOPT column in the phase would be zero with no error anywhere.
    EXPECT_GT(r.stop_calls, 0) << "snSTOP was never invoked, so the counters are not trustworthy";
    EXPECT_GE(r.majors, 1);
    EXPECT_GE(r.minors, r.majors);
}

// THE "IS IT TOLERANCE OR IS IT US" TEST, which is the reason the gate above
// can be trusted. A transcription defect puts a FLOOR under the achievable
// accuracy: if a constant were dropped or a row shifted, SNOPT would converge
// beautifully to the wrong point and tightening its tolerance would not move
// the error against the manufactured optimum at all. So the assertion is not
// that any single error is small -- it is that the error FOLLOWS the request,
// strictly, over four orders of magnitude.
//
// The default-tolerance solve is included as the first entry and is RECORDED
// rather than asserted tightly: 3.6e-5 is what SNOPT's own defaults buy on
// this instance, it is the honest "natural tolerance" datum the first-contact
// note reports, and pinning it hard would be pinning SNOPT's default, which is
// not ours to pin.
TEST(SnoptBridgeGate, AccuracyTracksTheRequestedToleranceWithNoFloor) {
    F7CollocationChain model(kNodes, 3, 2, kP, 1.0);

    const SnoptResult at_default = solve_cold_at(model, -1.0);
    ASSERT_EQ(at_default.info, 1) << at_default.status;
    // Recorded, deliberately loose: the point is the ORDER, not this number.
    EXPECT_LT(at_default.x_err_inf, 1e-3)
        << "SNOPT at its own defaults: x_err_inf = " << at_default.x_err_inf;

    double previous = at_default.x_err_inf;
    for (const double tol : {1e-6, 1e-8, 1e-10}) {
        const SnoptResult r = solve_cold_at(model, tol);
        ASSERT_EQ(r.info, 1) << "tol = " << tol << ": " << r.status;
        EXPECT_LT(r.x_err_inf, previous)
            << "tightening to " << tol << " did not improve the error against x_star ("
            << r.x_err_inf << " vs " << previous << "); an accuracy FLOOR is the signature of a "
            << "transcription defect, not of a tolerance";
        previous = r.x_err_inf;
    }
    // Four orders of tightening must buy at least four orders of accuracy --
    // i.e. the sequence is not merely decreasing but decreasing PROPORTIONALLY,
    // which is what "no floor" actually means.
    EXPECT_LT(previous, 1e-8) << "final error at tol = 1e-10 was " << previous;
}

// THE TRANSCRIPTION CHECK. The gate above proves the bridge reaches the right
// POINT; this proves it is describing the right PROBLEM at that point, by
// comparing SNOPT's own row values against F7's own evaluators at the
// identical x. These are the two halves of "same problem, both directions"
// and a coordinate off-by-one survives neither.
TEST(SnoptBridgeGate, RowTranscriptionMatchesTheModelAtTheSolution) {
    F7CollocationChain model(kNodes, 3, 2, kP, 1.0);

    SnoptOptions opts;
    opts.major_feas_tol = 1e-8;
    opts.major_opt_tol = 1e-8;
    opts.minor_feas_tol = 1e-8;
    SnoptF7Driver driver(model, opts);
    driver.set_start_point(model.start_point());
    const SnoptResult r = driver.solve(/*start=*/0);
    ASSERT_EQ(r.info, 1) << "gate precondition: " << r.status;

    const Vec x = driver.solution();
    const int me = static_cast<int>(model.me());
    const int mi = static_cast<int>(model.mi());

    // Row 0 is the objective. SNOPT's F(ObjRow) must be F7's eval_f.
    EXPECT_NEAR(driver.row_value(0), model.eval_f(x),
                1e-9 * std::max(1.0, std::abs(model.eval_f(x))))
        << "objective row mis-mapped";

    // Rows 1+me..nF-1 are the path rows, kept verbatim as cI_k (see the
    // driver's mapping banner), so they compare directly against eval_ci.
    const Vec ci = model.eval_ci(x);
    for (int k = 0; k < mi; ++k) {
        EXPECT_NEAR(driver.row_value(1 + me + k), ci(k), 1e-9) << "path row " << k << " mis-mapped";
    }

    // The structure counts are closed-form for F7, and asserting them here is
    // what catches a coordinate array that was built for the wrong shape:
    //   nF  = 1 + me + mi
    //   neA = nnz(Je)                        (every equality coefficient)
    //   neG = n + mi*ns                      (objective gradient + path rows)
    EXPECT_EQ(driver.nf(), 1 + me + mi);
    EXPECT_EQ(driver.nea(), static_cast<int>(model.eval_jac_e(x).nonZeros()));
    EXPECT_EQ(driver.neg(), static_cast<int>(model.n()) + mi * static_cast<int>(model.state_dim()));
}

// THE WARM-BASIS PATH. Start = 2 requires xstate/Fstate to be a valid exit
// state (guide section 3.4, pp.17-18); this exercises the whole hand-off --
// solve at p0, move the parameter, re-solve warm -- and asserts the SECOND
// solve also lands on the manufactured optimum AT THE NEW p. That is the
// property Task 6's warm columns depend on, and a hand-off that silently
// degraded to a cold crash would still converge, so correctness alone does
// not prove the path was taken: the state arrays being accepted by SNOPT at
// Start = 2 is what proves it, and an invalid state throws.
TEST(SnoptBridgeGate, WarmBasisReSolveLandsOnTheOptimumAtTheNewParameter) {
    constexpr double kP0 = 0.80;
    constexpr double kP1 = 0.85;

    F7CollocationChain model(kNodes, 3, 2, kP0, 1.0);
    SnoptOptions opts;
    // Same tightening as the gate above, for the same reason: at SNOPT's
    // relative defaults a 1e-6 ABSOLUTE criterion is not a meaningful request.
    opts.major_feas_tol = 1e-8;
    opts.major_opt_tol = 1e-8;
    opts.minor_feas_tol = 1e-8;
    SnoptF7Driver driver(model, opts);
    driver.set_start_point(model.start_point());

    const SnoptResult cold = driver.solve(/*start=*/0);
    ASSERT_EQ(cold.info, 1) << "warm-arm precondition (cold solve at p0): " << cold.status;
    ASSERT_LT(cold.x_err_inf, 1e-6);

    // Move p. Only the equality-row bounds change (see the driver banner); the
    // basis state, the primal iterate and the duals are deliberately NOT
    // reset -- that non-reset IS the warm start.
    driver.set_parameter(kP1);
    const SnoptResult warm = driver.solve(/*start=*/2);

    EXPECT_EQ(warm.info, 1) << "warm re-solve: " << warm.status;
    EXPECT_LT(warm.x_err_inf, 1e-6)
        << "warm re-solve is " << warm.x_err_inf << " from x_star(" << kP1 << ")";
    EXPECT_LT(warm.f_err_rel, 1e-9);
}

#endif // HVEN_SQP_HAVE_SNOPT
