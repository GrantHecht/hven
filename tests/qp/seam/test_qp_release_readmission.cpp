// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_release_readmission.cpp -- M8 PF-1, then M8 QPC, on the walk seam's target
// (docs/testing.md): THE RELEASE RE-ADMISSION (walk_engine.h, Degeneracy). A row released on its
// dual_mu footprint is re-admitted by the shift refresh on the post-drop pass; from then on every
// EQP solve of the walk converges its mu-free refinement (eqp_solve.h, THE CONVERGED REFINEMENT).
//
// The fixture is the census cell f7_n1000_path_corrupted's QP1 at m8 bdd797e3 (FORMAT.txt), which
// ran to its 20000-minor cap in-cell. From minor 311 it cycles with period four: row 905 released
// on a multiplier of -13.84 (the exact face's is -2.85), a full step of 0.06 along the row, the row
// re-admitted 2.7e-8 off its bound, a step back -- the objective repeating to 1e-15 at the capped
// endpoint `x_final`. The face is full rank but its smallest singular value is ~1e-6, below what
// dual_mu = 1e-8 resolves in one step (measured contraction 0.74 per refinement step, P-1a D-1).
// Until QPC the walk answered with a dual_mu rung (1e-8 -> 1e-10 at minor 312, 4758 minors, 192
// perturbed pivots in the next 87 passes: below the static-pivot floor). Under QPC the walk keeps
// dual_mu = 1e-8 and reaches the same optimum, q = -3.2319e-3 (the dense unregularized EQP of its
// final working set agrees to 1e-17, P-1a deciders), on both algebras.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/qp/walk/walk_engine.h>

#include "qp/support/qp_dump.h"
#include "support/route_reference.h"

using namespace hven;
using namespace hven::detail;
using hven::test_support::QpDump;
using Obs = hven::detail::testing::QpWalkObserver;
using Record = hven::detail::testing::QpWalkMinorRecord;

namespace {

constexpr const char *kFixture = "PF1_f7_n1000_path_corrupted_qp1.txt";
constexpr Index kMinors = 400;
// The optimum: the walk's certified objective at the rung (both algebras) and the dense
// unregularized EQP of the final working set, which agree to 9e-18 (P-1a deciders, fix1.txt).
constexpr double kPf1Optimum = -3.231904984786688e-03;

QpDump load_fixture() {
    return hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) + "/" + kFixture);
}

} // namespace

TEST(QpReleaseReadmission, F7PathCorruptedQp1SolvesAtItsOwnRegularization) {
    const QpDump d = load_fixture();
    // The premise: in-cell, this subproblem ran to its minor cap on the independent walk.
    ASSERT_NE(d.result.find("status=1 minors=20000"), std::string::npos) << d.result;
    ASSERT_TRUE(d.opts.independent_working_set);
    ASSERT_EQ(d.x_final.size(), d.qp.n());
    for (const auto alg : {QpWorkingSetAlgebra::kRefactorize, QpWorkingSetAlgebra::kSchurBorder}) {
        SCOPED_TRACE(static_cast<int>(alg));
        QpOptions o = d.opts;
        o.working_set_algebra = alg;
        Obs::reset();
        Obs::active = true;
        const QpSolution s = hven::test_support::solve_qp_dump(d, o);
        const std::vector<Record> records = Obs::records;
        const bool capture_complete = !Obs::truncated && !Obs::allocation_failed;
        Obs::reset();
        const double f = hven::test_support::qp_dump_objective(d, s.x);
        std::printf("[ pf1 ] %s status=%d minors=%ld facts=%ld border_refine=%ld eqp_refine=%ld "
                    "dual_regularization_decreases=%ld f=%.16g\n",
                    alg == QpWorkingSetAlgebra::kSchurBorder ? "border" : "refactorize",
                    static_cast<int>(s.status), static_cast<long>(s.counters.minor_iters),
                    static_cast<long>(s.counters.factorizations),
                    static_cast<long>(s.counters.border_refine_steps),
                    static_cast<long>(s.counters.eqp_refine_steps),
                    static_cast<long>(s.counters.dual_regularization_decreases), f);
        ASSERT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_LE(std::abs(f - kPf1Optimum), 1e-12 * std::abs(kPf1Optimum)) << f;
        // No rung: dual_mu on every pass record is the dump's own.
        EXPECT_EQ(s.counters.dual_regularization_decreases, 0);
        EXPECT_EQ(s.counters.converged_refinement_arms, 1);
        EXPECT_EQ(s.counters.certificate_gate_refusals, 0);
        if (capture_complete) {
            for (const Record &r : records) {
                ASSERT_EQ(r.dual_mu, d.opts.dual_regularization) << "minor " << r.minor;
            }
        }
        // Inside the box and the trust region about x0 = clamp(seed_x, lower, upper).
        ASSERT_TRUE(d.has_seed);
        const double tr = d.overrides.trust_region_radius;
        for (Index i = 0; i < d.qp.n(); ++i) {
            const double x0 = std::clamp(d.seed.x(i), d.qp.lower(i), d.qp.upper(i));
            ASSERT_GE(s.x(i), std::max(d.qp.lower(i), x0 - tr) - 1e-12) << i;
            ASSERT_LE(s.x(i), std::min(d.qp.upper(i), x0 + tr) + 1e-12) << i;
        }
    }
}

// THE CONVERGED REFINEMENT IS PER SOLVE: an engine that armed it on PF1 (400 minors; the first
// re-admission is at minor 312) solves the next QP exactly as a fresh engine does.
TEST(QpReleaseReadmission, TheConvergedRefinementIsPerSolve) {
    const QpDump pf1 = load_fixture();
    const QpDump qp7 = hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) +
                                                        "/QC8_DALLASM_qp7.txt");
    QpOptions o = pf1.opts;
    o.working_set_algebra = QpWorkingSetAlgebra::kRefactorize;
    o.max_iterations = kMinors;
    QpOptions o7 = qp7.opts;
    o7.working_set_algebra = QpWorkingSetAlgebra::kRefactorize;
    ASSERT_EQ(o7.max_iterations, qp7.opts.max_iterations);
    // One engine runs both; its options are PF1's, so qp7 runs under them on both sides.
    const QpEngine shared(o, pf1.threads, hven::linear::DensePolicy::kNever);
    const QpSolution first = shared.solve(pf1.qp, pf1.seed, pf1.overrides);
    ASSERT_EQ(first.counters.converged_refinement_arms, 1);
    const QpSolution second = qp7.has_seed ? shared.solve(qp7.qp, qp7.seed, qp7.overrides)
                                           : shared.solve(qp7.qp, qp7.overrides);
    const QpEngine fresh(o, pf1.threads, hven::linear::DensePolicy::kNever);
    const QpSolution alone = qp7.has_seed ? fresh.solve(qp7.qp, qp7.seed, qp7.overrides)
                                          : fresh.solve(qp7.qp, qp7.overrides);
    EXPECT_EQ(second.counters.converged_refinement_arms, alone.counters.converged_refinement_arms);
    EXPECT_EQ(static_cast<int>(second.status), static_cast<int>(alone.status));
    EXPECT_EQ(second.counters.minor_iters, alone.counters.minor_iters);
    EXPECT_EQ(second.counters.eqp_refine_steps, alone.counters.eqp_refine_steps);
    EXPECT_EQ(second.x, alone.x);
    EXPECT_EQ(second.lambda_e, alone.lambda_e);
}

// The rule belongs to the independent walk: the option-off walk never takes a rung.
TEST(QpReleaseReadmission, TheOptionOffWalkKeepsItsRegularization) {
    const QpDump d = load_fixture();
    QpOptions o = d.opts;
    o.independent_working_set = false;
    o.max_iterations = kMinors;
    const QpSolution s = hven::test_support::solve_qp_dump(d, o);
    std::printf("[ pf1 ] option-off status=%d minors=%ld dual_regularization_decreases=%ld\n",
                static_cast<int>(s.status), static_cast<long>(s.counters.minor_iters),
                static_cast<long>(s.counters.dual_regularization_decreases));
    EXPECT_EQ(s.counters.dual_regularization_decreases, 0);
    EXPECT_EQ(s.counters.converged_refinement_arms, 0);
}
