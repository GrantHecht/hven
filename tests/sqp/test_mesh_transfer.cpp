// tests/test_mesh_transfer.cpp — Phase-4 Task 11: mesh_transfer.h's MeshTransfer,
// the synthetic (transcription-agnostic) map of a WarmStart between
// discretization meshes.
//
// THE FIXTURE IS F6PathBoundQuadrature (tests/sqp/support/parametric_families.h),
// the quadrature discretization of
//     min_y INTEGRAL_0^1 (cosh(y - sin(pi t)) - 1) dt  s.t.  y(t) <= p,
// whose analytic multipliers are lambda_i = w_i * nu(t_i) with the mesh-free
// density nu(t) = sinh(max(0, sin(pi t) - p)) -- see that family's banner for
// the derivation, which is the concrete instance of mesh_transfer.h's own
// costate argument.
//
// WHAT EACH TEST IS FOR:
//   (a) TransferredMultipliersMatchFineSolve -- solve on a coarse mesh,
//       transfer to the 2x mesh, compare against the FINE COLD SOLVE's own
//       multipliers, and pin BOTH the error and the RAW-COPY COUNTERFACTUAL.
//       The counterfactual is this task's reason to exist: raw copying (the
//       same interpolation with the unscale/rescale steps deleted) is the
//       known wrong answer, and the assertion that it is worse by a large
//       pinned factor is what makes the unscaling load-bearing rather than
//       decorative.
//   (a') InteriorErrorIsSecondOrder -- the O(h^2) claim, as a measured RATE
//       across three mesh pairs rather than a single pinned number.
//   (b) ActivityInheritanceRule -- the exact inherited vectors, including
//       every edge case the header's rule decides (junction at a shared node,
//       single-node islands, a destination coarser than the source).
//   (c) WarmSolveOnFineMeshBeatsCold -- majors pinned on both sides.
//   (c') SeededTransferRidesTheDualsAndCostsFewerMinors -- Phase-6 Task 5: the
//       same transfer fed in as a `warm` OBJECT rather than as an x0, which
//       StartLevel::kSeeded made possible; the duals and activity hint cut the
//       major's minor count 21 -> 6 against (c)'s x0-only arm.
//   (d) CoarseningTransfer -- the reverse direction, fine -> a NON-NESTED,
//       NON-UNIFORM coarse mesh, with its own counterfactual.
//   (e) the T6 validation battery.
//
// ACCELERATE STANDING RULE. THE ONLY BACKEND-SENSITIVE PINS IN THIS FILE ARE
// (c)'s TWO MAJOR COUNTS (kColdMajors/kWarmMajors) AND (c')'s THREE MINOR
// COUNTS, which were MEASURED ON MKL
// PARDISO, the only backend available in this session: which trial steps the
// funnel accepts can depend on the factorization backend, hence how many
// majors a solve costs. A first Accelerate run that lands on different counts
// is a RE-MEASUREMENT, not automatically a defect; what must hold on any
// backend is the INEQUALITY beside them (warm < cold) and everything else in
// this file, all of which is backend-independent by construction -- the error
// and counterfactual numbers of (a), (a') and (d) are properties of
// interpolation and quadrature weights, computed from converged multipliers
// that agree with F6's ANALYTIC ones to 1e-6 (asserted in (a)), and (b)/(e)
// touch no linear algebra at all. F6's K0 is diagonal-plus-identity and
// nonsingular at every point (its banner's K0 GEOMETRY note), so no solve here
// can reach the QP engine's suspect gate.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/warmstart/mesh_transfer.h>
#include <hven/detail/warmstart/warm_start.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>

#include "support/parametric_families.h"

using namespace hven::solvers;
using hven::solvers::test_support::F6PathBoundQuadrature;
using hven::solvers::test_support::trapezoid_weights;
using hven::solvers::test_support::uniform_nodes;

namespace {

constexpr double kBound = 0.6; // the path bound level p used throughout

// MEASURED major counts for test (c) on the 33-node fine mesh: the cold solve
// from F6's own flat start point, and the warm one from the transferred
// point. Named constants rather than inline literals so the two pins read as
// one paired measurement.
constexpr Index kColdMajors = 3;
constexpr Index kWarmMajors = 1;

Mesh uniform_mesh(Index count) {
    Mesh m;
    m.nodes = uniform_nodes(count);
    m.weights = trapezoid_weights(m.nodes);
    return m;
}

// A converged cold solve of F6 on `mesh`, returned as its WarmStart. Every
// test that needs "the answer on this mesh" goes through here so no test
// compares against an unconverged point by accident.
WarmStart cold_solve(const Mesh &mesh, SqpSolution *out_sol = nullptr) {
    F6PathBoundQuadrature model(mesh.nodes, mesh.weights, kBound);
    SqpOptions opts;
    SqpDriver driver(opts);
    SqpSolution sol = driver.solve(model);
    EXPECT_EQ(sol.status, SqpStatus::kOptimal);
    if (out_sol != nullptr) {
        *out_sol = sol;
    }
    return sol.warm_start;
}

// THE COUNTERFACTUAL: mesh_transfer.h's map with the two weight steps
// DELETED -- the source multipliers are interpolated as if they were samples
// of a mesh-free function. This is exactly the mutation Step 3 of the task
// applies to the header (skip the unscaling), written out here so the test
// measures it without needing the library to be broken first.
Vec raw_copy(const Vec &lambda_from, const Mesh &from, const Mesh &to) {
    Vec out(to.nodes.size());
    for (Index k = 0; k < to.nodes.size(); ++k) {
        const double s = to.nodes(k);
        if (s <= from.nodes(0)) {
            out(k) = lambda_from(0);
            continue;
        }
        if (s >= from.nodes(from.nodes.size() - 1)) {
            out(k) = lambda_from(lambda_from.size() - 1);
            continue;
        }
        Index j = 0;
        while (j + 2 < from.nodes.size() && from.nodes(j + 1) <= s) {
            ++j;
        }
        const double theta = (s - from.nodes(j)) / (from.nodes(j + 1) - from.nodes(j));
        out(k) = (1.0 - theta) * lambda_from(j) + theta * lambda_from(j + 1);
    }
    return out;
}

// max|a - b| / max|reference|. RELATIVE TO THE VECTOR'S OWN SCALE rather than
// entry-by-entry: a path multiplier goes continuously to zero at each
// activity junction, so an entrywise relative error is unbounded there for
// reasons that say nothing about the transfer. This is the standard
// normalization for a profile comparison and is the number every pin below
// reads.
double rel_error(const Vec &got, const Vec &reference) {
    return (got - reference).cwiseAbs().maxCoeff() / reference.cwiseAbs().maxCoeff();
}

// ============================================================ (a)
//
// COARSE (17 nodes) -> FINE (33 nodes), against the fine cold solve.
//
// The two pinned numbers and where they come from:
//   - the transferred error, measured over the WHOLE vector, is dominated by
//     the two cells straddling the activity junctions, where nu has a kink
//     and a linear interpolant is only first-order accurate (F6's banner
//     derives this). Measured 4.7e-2; pinned below 8e-2.
//   - the raw-copy error is ~1.0, i.e. 100% of the peak multiplier, and it
//     does NOT shrink with the mesh: the coarse weights are ~2x the fine
//     ones, so raw copying reports every multiplier about twice too large.
//     That is the whole point -- an O(1) error that refinement cannot fix.
TEST(MeshTransfer, TransferredMultipliersMatchFineSolve) {
    const Mesh coarse = uniform_mesh(17);
    const Mesh fine = uniform_mesh(33);

    const WarmStart coarse_ws = cold_solve(coarse);
    const WarmStart fine_ws = cold_solve(fine);

    const MeshTransfer transfer;
    const WarmStart moved = transfer.transfer(coarse_ws, coarse, fine);

    ASSERT_EQ(moved.lambda_i.size(), fine.nodes.size());
    ASSERT_EQ(moved.x.size(), fine.nodes.size());

    // The fine cold solve's multipliers ARE the analytic ones to solver
    // accuracy -- checked here so the comparison below is against a
    // reference this test has independently verified, not merely against
    // another run of the same code.
    const F6PathBoundQuadrature fine_model(fine.nodes, fine.weights, kBound);
    EXPECT_LT(rel_error(fine_ws.lambda_i, fine_model.lambda_i_star(kBound)), 1e-6);

    const double transferred_err = rel_error(moved.lambda_i, fine_ws.lambda_i);
    const double raw_err = rel_error(raw_copy(coarse_ws.lambda_i, coarse, fine), fine_ws.lambda_i);

    EXPECT_LT(transferred_err, 8e-2) << "transferred = " << transferred_err;
    EXPECT_GT(raw_err, 0.5) << "raw = " << raw_err;

    // THE LOAD-BEARING ASSERTION. Deleting the unscale/rescale steps from
    // mesh_transfer.h makes transfer() identical to raw_copy() above, so this
    // ratio collapses to exactly 1 and the test fails. Measured 21x; pinned
    // at 8x so the pin survives ordinary mesh/fixture drift while still being
    // an order of magnitude away from the collapsed value.
    EXPECT_GT(raw_err / transferred_err, 8.0)
        << "transferred = " << transferred_err << ", raw = " << raw_err;

    // The primal point transfers by plain linear interpolation (x is a
    // pointwise state value, not a weighted one). Its error is FIRST order
    // for the same reason the multiplier error above is: x*(t) = min(a(t), p)
    // has a kink at each activity junction, exactly where nu does, so a
    // linear interpolant straddling one is only O(h) accurate there. Measured
    // 3.7e-2 = 6% of the peak state, consistent with the h/4 * (slope jump)
    // bound at h = 1/16.
    EXPECT_LT(rel_error(moved.x, fine_ws.x), 6e-2);

    // TRANSFER SEMANTICS (mesh_transfer.h's own note): the output describes a
    // DIFFERENT problem size, so it can never claim a structural match.
    EXPECT_EQ(moved.structure_hash, 0u);
    EXPECT_EQ(moved.hot, nullptr);
    EXPECT_TRUE(moved.valid);
    EXPECT_LT(moved.funnel_width, 0.0);              // the "unset" sentinel: not transferred
    EXPECT_EQ(moved.tr_radius, coarse_ws.tr_radius); // x-units: transferred as is
    // The regularization constants ride along for the same reason tr_radius
    // does -- they are algorithmic, not mesh-scaled -- and the source values
    // are checked to be REAL here (off warm_start.h's -1 "never populated"
    // sentinel) so the equalities cannot pass by both sides being unset.
    ASSERT_GT(coarse_ws.primal_delta, 0.0);
    ASSERT_GT(coarse_ws.dual_mu, 0.0);
    EXPECT_EQ(moved.primal_delta, coarse_ws.primal_delta);
    EXPECT_EQ(moved.dual_mu, coarse_ws.dual_mu);
    EXPECT_EQ(moved.qp_working_set.n(), fine.nodes.size());
    EXPECT_EQ(moved.qp_working_set.mi(), fine.nodes.size());

    // NEVER MUTATES THE INPUT.
    const WarmStart untouched = cold_solve(coarse);
    EXPECT_EQ(coarse_ws.lambda_i, untouched.lambda_i);
    EXPECT_EQ(coarse_ws.x, untouched.x);
}

// (a') THE O(h^2) CLAIM, AS A RATE. Away from the two junction cells the
// interpolated density is second-order accurate, so halving the mesh spacing
// should quarter the error. Measured 4.8e-2 -> 1.3e-2 -> 3.2e-3, i.e. ratios
// 3.9 and 4.0.
TEST(MeshTransfer, InteriorErrorIsSecondOrder) {
    const MeshTransfer transfer;
    std::vector<double> errors;
    for (Index coarse_count : {9, 17, 33}) {
        const Mesh coarse = uniform_mesh(coarse_count);
        const Mesh fine = uniform_mesh(2 * coarse_count - 1);
        const WarmStart moved = transfer.transfer(cold_solve(coarse), coarse, fine);
        const WarmStart fine_ws = cold_solve(fine);

        // Exclude the cells straddling the two junctions: "interior" means at
        // least one COARSE spacing away from either, which is exactly the
        // region where the source data has no kink in it.
        const double h_coarse = 1.0 / static_cast<double>(coarse_count - 1);
        const double left = F6PathBoundQuadrature::junction_left(kBound);
        const double right = F6PathBoundQuadrature::junction_right(kBound);
        const double scale = fine_ws.lambda_i.cwiseAbs().maxCoeff();
        double worst = 0.0;
        for (Index k = 0; k < fine.nodes.size(); ++k) {
            const double t = fine.nodes(k);
            if (std::abs(t - left) <= h_coarse || std::abs(t - right) <= h_coarse) {
                continue;
            }
            worst = std::max(worst, std::abs(moved.lambda_i(k) - fine_ws.lambda_i(k)) / scale);
        }
        errors.push_back(worst);
    }
    ASSERT_EQ(errors.size(), 3u);
    for (std::size_t k = 0; k + 1 < errors.size(); ++k) {
        const double ratio = errors[k] / errors[k + 1];
        EXPECT_GT(ratio, 3.2) << "errors: " << errors[0] << ", " << errors[1] << ", " << errors[2];
        EXPECT_LT(ratio, 5.0) << "errors: " << errors[0] << ", " << errors[1] << ", " << errors[2];
    }
}

// ============================================================ (b)
//
// THE ACTIVITY INHERITANCE RULE, on hand-built activity vectors rather than
// solved ones, so the expected output is an EXACT vector derived from the
// rule and not from a solver's opinion. The rule (mesh_transfer.h): a
// destination node inherits a label iff every source node of the source
// interval(s) whose CLOSURE contains it carries that label; otherwise it is
// left free.
TEST(MeshTransfer, ActivityInheritanceRule) {
    // Source: 5 nodes at 0, .25, .5, .75, 1 with nodes 2 and 3 active, i.e.
    // ONE junction inside [t1, t2] and one inside [t3, t4].
    Mesh from;
    from.nodes = uniform_nodes(5);
    from.weights = trapezoid_weights(from.nodes);

    WarmStart ws;
    ws.valid = true;
    ws.x = Vec::Zero(5);
    ws.ineq_active = {0, 0, 1, 1, 0};
    ws.qp_working_set = WorkingSet(5, 5);

    // Destination: the 9-node 2x refinement, so nodes 0,2,4,6,8 coincide with
    // the source's and 1,3,5,7 are the new midpoints.
    Mesh to;
    to.nodes = uniform_nodes(9);
    to.weights = trapezoid_weights(to.nodes);

    const MeshTransfer transfer;
    const WarmStart moved = transfer.transfer(ws, from, to);

    // Derived node by node from the rule:
    //   k=0 (t=0.00) at source node 0; stencil {0,1} = {0,0}      -> 0
    //   k=1 (t=0.125) inside (s0,s1); stencil {0,1} = {0,0}       -> 0
    //   k=2 (t=0.25) at source node 1; stencil {0,1,2} = {0,0,1}  -> 0 (junction node)
    //   k=3 (t=0.375) inside (s1,s2); stencil {1,2} = {0,1}       -> 0 (junction cell)
    //   k=4 (t=0.50) at source node 2; stencil {1,2,3} = {0,1,1}  -> 0 (junction node)
    //   k=5 (t=0.625) inside (s2,s3); stencil {2,3} = {1,1}       -> 1 INTERIOR
    //   k=6 (t=0.75) at source node 3; stencil {2,3,4} = {1,1,0}  -> 0 (junction node)
    //   k=7 (t=0.875) inside (s3,s4); stencil {3,4} = {1,0}       -> 0 (junction cell)
    //   k=8 (t=1.00) at source node 4; stencil {3,4} = {1,0}      -> 0
    const std::vector<std::uint8_t> expected = {0, 0, 0, 0, 0, 1, 0, 0, 0};
    EXPECT_EQ(moved.ineq_active, expected);

    // The re-derived working set agrees with the inherited vector exactly.
    EXPECT_EQ(moved.qp_working_set.active_ineq(), std::vector<Index>{5});

    // EDGE CASE 1 -- A WIDE ACTIVE RUN: only its two END nodes are junction-
    // adjacent, everything strictly inside inherits. Source 0,1,1,1,1,0 over
    // 6 nodes; destination is the same mesh (an identity map), which is the
    // cleanest way to read the rule's treatment of shared nodes.
    Mesh six;
    six.nodes = uniform_nodes(6);
    six.weights = trapezoid_weights(six.nodes);
    WarmStart wide;
    wide.valid = true;
    wide.x = Vec::Zero(6);
    wide.ineq_active = {0, 1, 1, 1, 1, 0};
    wide.qp_working_set = WorkingSet(6, 6);
    const std::vector<std::uint8_t> wide_expected = {0, 0, 1, 1, 0, 0};
    EXPECT_EQ(transfer.transfer(wide, six, six).ineq_active, wide_expected)
        << "an identity map must still surrender the two junction-adjacent nodes";

    // EDGE CASE 2 -- A SINGLE-NODE ACTIVE ISLAND has no interior at all: the
    // node itself is junction-adjacent on BOTH sides, and so is every
    // destination node in either neighbouring cell. The island is therefore
    // erased entirely and the destination QP re-decides it, which is the
    // conservative reading the rule commits to.
    WarmStart island;
    island.valid = true;
    island.x = Vec::Zero(5);
    island.ineq_active = {0, 0, 1, 0, 0};
    island.qp_working_set = WorkingSet(5, 5);
    const WarmStart island_moved = transfer.transfer(island, from, to);
    for (std::uint8_t flag : island_moved.ineq_active) {
        EXPECT_EQ(flag, 0) << "a single-node island has no strict interior to inherit";
    }

    // EDGE CASE 3 -- DESTINATION COARSER THAN SOURCE. Inheritance is a
    // POINT-SAMPLING operation, so an active run survives only where a
    // destination node actually lands strictly inside it. Source: 9 nodes with
    // 3..6 active; destination: the 5-node mesh (every other source node).
    WarmStart fine_ws;
    fine_ws.valid = true;
    fine_ws.x = Vec::Zero(9);
    fine_ws.ineq_active = {0, 0, 0, 1, 1, 1, 1, 0, 0};
    fine_ws.qp_working_set = WorkingSet(9, 9);
    //   dest 0 (t=0.00)  at src 0;  stencil {0,1}     = {0,0}     -> 0
    //   dest 1 (t=0.25)  at src 2;  stencil {1,2,3}   = {0,0,1}   -> 0
    //   dest 2 (t=0.50)  at src 4;  stencil {3,4,5}   = {1,1,1}   -> 1
    //   dest 3 (t=0.75)  at src 6;  stencil {5,6,7}   = {1,1,0}   -> 0
    //   dest 4 (t=1.00)  at src 8;  stencil {7,8}     = {0,0}     -> 0
    const std::vector<std::uint8_t> coarsened = {0, 0, 1, 0, 0};
    EXPECT_EQ(transfer.transfer(fine_ws, to, from).ineq_active, coarsened);

    // EDGE CASE 4 -- NO JUNCTION AT ALL: an all-active source transfers
    // wholesale, endpoints included, because no stencil anywhere disagrees.
    WarmStart all_on;
    all_on.valid = true;
    all_on.x = Vec::Zero(5);
    all_on.ineq_active = {1, 1, 1, 1, 1};
    all_on.qp_working_set = WorkingSet(5, 5);
    const WarmStart all_moved = transfer.transfer(all_on, from, to);
    for (std::uint8_t flag : all_moved.ineq_active) {
        EXPECT_EQ(flag, 1);
    }

    // BOUND ACTIVITY follows the identical rule, including the fact that a
    // stencil mixing -1 and +1 is a disagreement and yields FREE. Source
    // bounds: lower, lower, free, upper, upper.
    WarmStart bounds;
    bounds.valid = true;
    bounds.x = Vec::Zero(5);
    bounds.bound_active = {-1, -1, 0, +1, +1};
    bounds.qp_working_set = WorkingSet(5, 5);
    const WarmStart bounds_moved = transfer.transfer(bounds, from, to);
    //   k=0 at s0: {s0,s1} = {-1,-1} -> -1
    //   k=1 in (s0,s1): {-1,-1}      -> -1
    //   k=2 at s1: {s0,s1,s2} = {-1,-1,0} -> 0
    //   k=3..5 all straddle the free node or the sign flip -> 0
    //   k=6 at s3: {s2,s3,s4} = {0,+1,+1} -> 0
    //   k=7 in (s3,s4): {+1,+1}      -> +1
    //   k=8 at s4: {s3,s4} = {+1,+1} -> +1
    const std::vector<std::int8_t> bounds_expected = {-1, -1, 0, 0, 0, 0, 0, +1, +1};
    EXPECT_EQ(bounds_moved.bound_active, bounds_expected);
    EXPECT_EQ(bounds_moved.qp_working_set.bound_state()[0], BoundState::kAtLower);
    EXPECT_EQ(bounds_moved.qp_working_set.bound_state()[4], BoundState::kFree);
    EXPECT_EQ(bounds_moved.qp_working_set.bound_state()[8], BoundState::kAtUpper);
}

// ============================================================ (c)
//
// A SOLVE ON THE FINE MESH STARTED FROM THE TRANSFERRED POINT COSTS FEWER
// MAJORS THAN THE COLD ONE.
//
// THE TRANSFERRED POINT ENTERS AS x0, NOT AS A `warm` OBJECT. Through Phase 5
// that was forced by the transfer's own semantics rather than being a
// shortcut: the output carries structure_hash == 0 (see mesh_transfer.h),
// which sqp_driver.h's warm-start ingest treated exactly like a mismatch and
// resolved to kCold, so the multipliers and the activity guess -- the
// transfer's main product -- could not ride into the driver at all.
//
// PHASE-6 TASK 5 MADE THE OTHER HALF REACHABLE, and this test DELIBERATELY
// STAYS AS IT IS: it remains the x0-only measurement, which is now the CONTROL
// ARM for (c') immediately below rather than the whole story. Keeping it
// unchanged is what lets (c') attribute its difference to the duals and the
// activity hint alone -- same meshes, same models, same options, one argument
// of difference.
TEST(MeshTransfer, WarmSolveOnFineMeshBeatsCold) {
    const Mesh coarse = uniform_mesh(17);
    const Mesh fine = uniform_mesh(33);

    const MeshTransfer transfer;
    const WarmStart moved = transfer.transfer(cold_solve(coarse), coarse, fine);

    F6PathBoundQuadrature model(fine.nodes, fine.weights, kBound);
    SqpOptions opts;
    SqpDriver driver(opts);

    const SqpSolution cold = driver.solve(model, model.start_point());
    const SqpSolution warm = driver.solve(model, moved.x);

    ASSERT_EQ(cold.status, SqpStatus::kOptimal);
    ASSERT_EQ(warm.status, SqpStatus::kOptimal);
    // Same answer, reached for less: the two solves agree on x to well inside
    // the convergence tolerance, so the major counts below compare like with
    // like.
    EXPECT_LT((cold.x - warm.x).cwiseAbs().maxCoeff(), 1e-6);

    // PINNED BOTH SIDES (measured; see this file's own report). A change to
    // either number is a behaviour change in the driver or in the transfer,
    // not a tuning detail.
    EXPECT_EQ(cold.counters.major_iters, kColdMajors);
    EXPECT_EQ(warm.counters.major_iters, kWarmMajors);
    EXPECT_LT(warm.counters.major_iters, cold.counters.major_iters);
}

// =========================================================== (c')
//
// **PHASE-6 TASK 5: THE TRANSFERRED DUALS RIDE, AND IT IS WORTH COUNTED
// WORK.** This is the measurement mesh_transfer.h's PHASE-6 INGEST GAP note
// promised and could not make: the whole product of sections 1 and 3 -- the
// unscaled costates and the inherited activity -- fed into a real solve
// through the ordinary 3-argument API, with the benefit read off counters
// rather than argued.
//
// THE THREE ARMS, all on the SAME 33-node fine mesh and the same model:
//   COLD      driver.solve(model, model.start_point())    -- no transfer at all
//   x0-ONLY   driver.solve(model, moved.x)                -- (c)'s own arm, the
//                                                            primal half alone
//   SEEDED    driver.solve(model, moved.x, moved)         -- the whole object
//
// MEASURED (MKL Pardiso, clang++ Release, MKL_NUM_THREADS=1; counters are
// deterministic and are asserted, wall time is not measured here at all):
//
//   arm        level     majors   QP minors   factorizations
//   cold       kCold     3        25          3
//   x0-only    kCold     1        21          1
//   seeded     kSeeded   1        6           1
//
// **THE BENEFIT IS IN MINORS, NOT MAJORS, AND SAYING SO PRECISELY IS THE
// POINT.** The transferred x is already good enough that ONE major suffices
// either way, so the duals buy no major here -- what they buy is that major
// being CHEAP: 21 minor iterations down to 6, a 3.5x cut, because the
// inherited activity hint seeds the working set the QP would otherwise have to
// walk to one ratio test at a time, and the inherited costates mean it starts
// from prices rather than from zero. That is the same mechanism the Phase-5
// head-to-head named as the one that matters ("the mechanism is minor COUNT,
// not per-minor cost", docs/notes/2026-08-01-psiopt-first-comparison.md), and
// it is the half the ingest gap was throwing away.
//
// A ZERO-MAJOR SEEDED EXIT IS NOT WHAT THIS FIXTURE PRODUCES, and the pin
// records that honestly rather than being written to the hoped-for number: the
// transferred point is second-order accurate in the interior (test (a')) but
// only FIRST-order across the two activity junctions, so its Lagrangian
// gradient at the fine mesh is outside kkt_tol and a real major is owed. A
// fixture whose transfer landed inside tolerance would certify for free --
// tests/test_b1_gate.cpp's seeded zero-major guard covers that shape.
//
// THE ANSWER IS CHECKED INDEPENDENTLY, not taken on the certificate's word:
// the seeded arm's x is compared against the COLD arm's converged answer.
TEST(MeshTransfer, SeededTransferRidesTheDualsAndCostsFewerMinors) {
    const Mesh coarse = uniform_mesh(17);
    const Mesh fine = uniform_mesh(33);

    const MeshTransfer transfer;
    const WarmStart moved = transfer.transfer(cold_solve(coarse), coarse, fine);
    ASSERT_EQ(moved.structure_hash, 0u) << "a transfer's hash is UNKNOWN, and kSeeded is fine with";
    ASSERT_GT(moved.lambda_i.maxCoeff(), 0.0) << "and it carries live costates to ride in on";

    F6PathBoundQuadrature model(fine.nodes, fine.weights, kBound);
    SqpOptions opts;
    SqpDriver driver(opts);

    const SqpSolution cold = driver.solve(model, model.start_point());
    const SqpSolution x_only = driver.solve(model, moved.x);
    const SqpSolution seeded = driver.solve(model, moved.x, moved);

    ASSERT_EQ(cold.status, SqpStatus::kOptimal);
    ASSERT_EQ(x_only.status, SqpStatus::kOptimal);
    ASSERT_EQ(seeded.status, SqpStatus::kOptimal);

    EXPECT_EQ(cold.counters.start_level_used, StartLevel::kCold);
    EXPECT_EQ(x_only.counters.start_level_used, StartLevel::kCold)
        << "the 2-arg overload is cold by construction -- the control arm";
    EXPECT_EQ(seeded.counters.start_level_used, StartLevel::kSeeded)
        << "THE PIN: a transferred object now reaches the ingest";
    EXPECT_EQ(seeded.counters.n_seeded, 1);
    EXPECT_EQ(seeded.counters.seeded_clamped, 0)
        << "a transfer of a driver-produced object carries no negative price";

    // PINNED, ALL THREE ARMS (measured; MKL-observed, and see this file's
    // ACCELERATE STANDING RULE -- trajectory counts are the one class of pin in
    // this file that can legitimately re-measure on another backend. The
    // INEQUALITY beside them is what must hold on any backend.)
    EXPECT_EQ(cold.counters.major_iters, kColdMajors);
    EXPECT_EQ(cold.counters.qp_minor_iters, 25);
    EXPECT_EQ(x_only.counters.major_iters, kWarmMajors);
    EXPECT_EQ(x_only.counters.qp_minor_iters, 21);
    EXPECT_EQ(seeded.counters.major_iters, kWarmMajors)
        << "the duals buy no MAJOR on this fixture -- see the note above";
    EXPECT_EQ(seeded.counters.qp_minor_iters, 6);

    // THE COUNTED BENEFIT, as an inequality that survives a re-measurement.
    EXPECT_LT(seeded.counters.qp_minor_iters, x_only.counters.qp_minor_iters / 2)
        << "THE PIN: the inherited activity and costates cut the single major's minor count by "
           "more than half against the primal half alone (measured 21 -> 6)";

    // AND IT IS THE RIGHT ANSWER -- checked against the cold arm's own
    // converged point, independently of the certificate the seeded arm issued.
    EXPECT_LT((seeded.x - cold.x).cwiseAbs().maxCoeff(), 1e-6);
    EXPECT_NEAR(seeded.f, cold.f, 1e-8);
}

// ============================================================ (d)
//
// COARSENING: FINE -> A NON-NESTED, NON-UNIFORM COARSE MESH.
//
// The direction reverses -- now the interpolation SUBSAMPLES rather than
// refines -- and the destination weights VARY BY A FACTOR OF ~5.5 across the
// mesh, so the unscale/rescale correction is spatially varying rather than a
// single global ratio. Raw copying cannot be right anywhere except by
// accident. The destination clusters nodes near the two activity junctions,
// which is what a real mesh-refinement pass produces.
TEST(MeshTransfer, CoarseningTransferPreservesCostates) {
    const Mesh fine = uniform_mesh(33);

    const double left = F6PathBoundQuadrature::junction_left(kBound);
    const double right = F6PathBoundQuadrature::junction_right(kBound);
    Mesh coarse;
    coarse.nodes = (Vec(13) << 0.0, 0.09, 0.17, left - 0.02, left + 0.03, 0.32, 0.5, 0.68,
                    right - 0.03, right + 0.02, 0.83, 0.91, 1.0)
                       .finished();
    coarse.weights = trapezoid_weights(coarse.nodes);
    ASSERT_GT(coarse.weights.maxCoeff() / coarse.weights.minCoeff(), 5.0);

    const WarmStart fine_ws = cold_solve(fine);
    const WarmStart coarse_ws = cold_solve(coarse);

    const MeshTransfer transfer;
    const WarmStart moved = transfer.transfer(fine_ws, fine, coarse);

    ASSERT_EQ(moved.lambda_i.size(), coarse.nodes.size());

    const double transferred_err = rel_error(moved.lambda_i, coarse_ws.lambda_i);
    const double raw_err = rel_error(raw_copy(fine_ws.lambda_i, fine, coarse), coarse_ws.lambda_i);

    // The coarsening direction is MORE accurate than the refining one, not
    // less: every destination node's value is interpolated from data that is
    // finer than it, so the only error is the fine mesh's own resolution of
    // nu near the junctions. Measured 1.3e-3.
    EXPECT_LT(transferred_err, 5e-3) << "transferred = " << transferred_err;
    EXPECT_GT(raw_err, 0.5) << "raw = " << raw_err;
    // Measured factor ~640; pinned at 50 for the same reason (a) pins 8.
    EXPECT_GT(raw_err / transferred_err, 50.0)
        << "transferred = " << transferred_err << ", raw = " << raw_err;
}

// ============================================================ (e) T6
//
// Every validation failure is a CALLER ERROR and throws with the offending
// value in the message (project rule T6: never a diagnostic the exception
// does not carry).
TEST(MeshTransfer, RejectsMalformedInput) {
    const MeshTransfer transfer;
    const Mesh good = uniform_mesh(5);

    WarmStart ws;
    ws.valid = true;
    ws.x = Vec::Zero(5);
    ws.lambda_i = Vec::Zero(5);
    ws.qp_working_set = WorkingSet(5, 5);

    auto throws_with = [](auto &&fn, const char *needle) {
        try {
            fn();
        } catch (const std::invalid_argument &e) {
            EXPECT_NE(std::string(e.what()).find(needle), std::string::npos)
                << "message was: " << e.what();
            return true;
        }
        return false;
    };

    // Non-increasing nodes, with both offending values named.
    Mesh bad_nodes = good;
    bad_nodes.nodes(2) = bad_nodes.nodes(1);
    EXPECT_TRUE(throws_with([&] { transfer.transfer(ws, bad_nodes, good); }, "strictly increasing"))
        << "equal adjacent nodes must be rejected";
    EXPECT_TRUE(
        throws_with([&] { transfer.transfer(ws, good, bad_nodes); }, "strictly increasing"));

    // Non-positive weight, with the index and the value named.
    Mesh bad_w = good;
    bad_w.weights(3) = 0.0;
    EXPECT_TRUE(throws_with([&] { transfer.transfer(ws, bad_w, good); }, "weights[3]"));
    bad_w.weights(3) = -1.5;
    EXPECT_TRUE(throws_with([&] { transfer.transfer(ws, bad_w, good); }, "-1.5"));

    // weights/nodes length mismatch.
    Mesh bad_len = good;
    bad_len.weights = Vec::Ones(4);
    EXPECT_TRUE(throws_with([&] { transfer.transfer(ws, bad_len, good); }, "expected 5"));

    // A one-node mesh has no interval to interpolate on.
    Mesh tiny;
    tiny.nodes = Vec::Constant(1, 0.0);
    tiny.weights = Vec::Constant(1, 1.0);
    EXPECT_TRUE(throws_with([&] { transfer.transfer(ws, tiny, good); }, ">= 2"));

    // A non-finite node.
    Mesh nan_nodes = good;
    nan_nodes.nodes(2) = std::numeric_limits<double>::quiet_NaN();
    EXPECT_TRUE(throws_with([&] { transfer.transfer(ws, nan_nodes, good); }, "finite"));

    // WarmStart vectors must be sized one-per-node (or empty).
    WarmStart bad_x = ws;
    bad_x.x = Vec::Zero(4);
    EXPECT_TRUE(throws_with([&] { transfer.transfer(bad_x, good, good); }, "WarmStart::x"));

    WarmStart bad_lam = ws;
    bad_lam.lambda_i = Vec::Zero(3);
    EXPECT_TRUE(throws_with([&] { transfer.transfer(bad_lam, good, good); }, "lambda_i"));

    WarmStart bad_act = ws;
    bad_act.ineq_active = {0, 1, 0};
    EXPECT_TRUE(throws_with([&] { transfer.transfer(bad_act, good, good); }, "ineq_active"));

    WarmStart bad_bound = ws;
    bad_bound.bound_active = {0, 1, 0, 0, 0, 0};
    EXPECT_TRUE(throws_with([&] { transfer.transfer(bad_bound, good, good); }, "bound_active"));

    WarmStart bad_wsz = ws;
    bad_wsz.qp_working_set = WorkingSet(4, 4);
    EXPECT_TRUE(throws_with([&] { transfer.transfer(bad_wsz, good, good); }, "qp_working_set"));

    // A COLD (default-constructed) WarmStart carries nothing that may be fed
    // forward -- warm_start.h's own contract -- so transferring one is a
    // caller error rather than a silent no-op.
    WarmStart cold;
    EXPECT_TRUE(throws_with([&] { transfer.transfer(cold, good, good); }, "valid"));

    // An empty vector is ABSENT, not malformed: F6 has me == 0, so a
    // WarmStart from it carries an empty lambda_e, and that must survive a
    // transfer as an empty lambda_e rather than tripping a size check.
    const WarmStart moved = transfer.transfer(ws, good, good);
    EXPECT_EQ(moved.lambda_e.size(), 0);
    EXPECT_EQ(moved.z.size(), 0);
    EXPECT_TRUE(moved.ineq_active.empty());
    EXPECT_TRUE(moved.bound_active.empty());
    EXPECT_EQ(moved.qp_working_set.mi(), 5); // lambda_i was present

    // ALL THREE INEQUALITY CARRIERS ARE CONSULTED when sizing the destination
    // working set (mesh_transfer.h's note beside `has_ineq`): a source whose
    // ONLY record of having inequality rows is its own qp_working_set.mi()
    // must still produce a destination working set with mi() == n, not 0.
    // Today's driver never emits that shape -- a solve with mi > 0 always
    // populates lambda_i -- so this is the consistency guarantee, pinned here
    // because nothing else could reach it.
    WarmStart ws_only;
    ws_only.valid = true;
    ws_only.x = Vec::Zero(5);
    ws_only.qp_working_set = WorkingSet(5, 5); // mi() == 5, both vectors empty
    ASSERT_EQ(ws_only.lambda_i.size(), 0);
    ASSERT_TRUE(ws_only.ineq_active.empty());
    EXPECT_EQ(transfer.transfer(ws_only, good, good).qp_working_set.mi(), 5);
}

} // namespace
