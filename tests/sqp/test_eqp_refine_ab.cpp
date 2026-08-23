// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_eqp_refine_ab.cpp — the EQP-refinement A/B harness (Phase-4
// Task 0, discharging the Phase-3 carry in
// docs/notes/2026-07-29-phase-3-close-carries.md).
//
// THE QUESTION IT EXISTED TO ANSWER, AND WHAT REMAINS. Task 11b iterated the
// BORDERED path's refinement and deliberately did not apply the symmetric fix
// to solve_eqp, because a symmetric fix (written then WITHOUT the footprint
// stopping rule) broke both SqpDriverAdaptiveMu tests by removing the fixed-mu
// accuracy ceiling that Task 10's adaptive-mu schedule exists to break
// through. The carry's prior question was therefore: is adaptive-mu a REAL
// mechanism, or is it compensation for under-refinement? Measured across the
// full cross product, the answer was that the symmetric flag is INERT and
// adaptive-mu is the only live lever -- confirmed independently on Apple
// Accelerate by the port audit, which is what carried R-1 to its ruling.
//
// QpOptions::eqp_refine has since been DELETED (Task 3b; see the DISPOSITION
// section of docs/notes/2026-07-29-eqp-refinement-ab.md), so its lever is gone
// from the cross product and the harness measures what is still variable:
//
//     {adaptive_mu off, on} x {kSchurBorder, kRefactorize}   = 4 cells
//
// x 27 Hock-Schittkowski problems, recording per problem: status, f error
// against the same target the battery asserts, majors, minors,
// factorizations, and both refinement-step counters. eqp_refine_steps stays in
// the table as the standing invariant it became -- identically 0, and a
// nonzero reading would mean solve_eqp grew a step nobody asked for.
//
// HOW TO RUN IT (it is DISABLED_ because it is a measurement, not an
// assertion -- it must not vote on whether the branch is green):
//
//     build/tests/hven_sqp_tests --gtest_also_run_disabled_tests \
//         --gtest_filter='EqpRefinementAb.DISABLED_FullBattery'
//
// It prints one line per (cell, problem) and one aggregate line per cell to
// stdout, deterministically -- the same binary on the same machine reproduces
// the table in docs/notes/2026-07-29-eqp-refinement-ab.md byte for byte.
//
// EqpRefinementAb.EveryCellSolvesTheBorderReproProblem (enabled, cheap) keeps
// the harness compiled and its cell enumeration exercised on every run, so
// the measurement tool cannot rot between the sessions that use it.

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/detail/qp/eqp_solve.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>
#include <hven/model/nlp_model.h>

#include "support/hs_problems.h"
#include "support/scale_problems.h"

using namespace hven::solvers;
using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using hven::solvers::test_support::hs_numbers;
using hven::solvers::test_support::HsProblem;
using hven::solvers::test_support::make_hs;

namespace {

// One A/B cell: the three levers, and a stable short name used in the note.
struct Cell {
    bool adaptive_mu;
    WorkingSetLinearAlgebra algebra;

    std::string name() const {
        return fmt::format("mu={} alg={}", adaptive_mu ? "on " : "off",
                           algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border"
                                                                            : "refactorize");
    }
};

// All four, in a fixed order the note's table follows.
std::vector<Cell> cells() {
    std::vector<Cell> out;
    for (bool mu : {false, true}) {
        for (WorkingSetLinearAlgebra alg :
             {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
            out.push_back(Cell{mu, alg});
        }
    }
    return out;
}

// The battery's per-problem budget and f target, in the SAME form
// test_hs_battery.cpp uses -- duplicated here rather than shared because that
// file's table is an ASSERTION table (budgets with headroom, excusal
// contract) and this one is a MEASUREMENT table: the harness must be able to
// report a cell that regresses, which means it must not inherit expectations
// tuned to the default cell.
//
// f_target is NaN for a problem measured against its cited f*; the three
// values below are the battery's excused targets (HS15/HS25/HS33 -- local
// solution, noise floor, and local corner respectively), used here so that an
// excused problem's f error reads as ~0 in the DEFAULT cell and any cell that
// moves it shows up as a difference rather than as a constant offset.
struct Row {
    int number;
    Index max_iter;
    double f_target;
};

const std::vector<Row> &battery() {
    static const std::vector<Row> kRows = [] {
        std::vector<Row> rows;
        for (int n : hs_numbers()) {
            Index max_iter = 60;
            double target = std::numeric_limits<double>::quiet_NaN();
            if (n == 38) {
                max_iter = 120;
            } else if (n == 15) {
                target = 360.37976717049241;
            } else if (n == 25) {
                target = 32.834999999663594;
            } else if (n == 33) {
                target = -3.9999999999737859;
            }
            rows.push_back(Row{n, max_iter, target});
        }
        return rows;
    }();
    return kRows;
}

const char *status_name(SqpStatus s) {
    switch (s) {
    case SqpStatus::kOptimal:
        return "kOptimal";
    case SqpStatus::kMaxIter:
        return "kMaxIter";
    case SqpStatus::kInfeasible:
        return "kInfeasible";
    case SqpStatus::kNumericalError:
        return "kNumericalError";
    case SqpStatus::kBudgetExhausted:
        return "kBudgetExhausted";
    }
    return "?";
}

// What one problem produced in one cell.
struct Outcome {
    SqpStatus status = SqpStatus::kOptimal;
    double f_err = 0.0; // |f - target| / max(1, |target|), the battery's own measure
    Index majors = 0, minors = 0, factorizations = 0;
    Index eqp_refine_steps = 0, border_refine_steps = 0;
};

// A tolerance REGIME. The battery's own tolerances (1e-6) are the shipped
// defaults, but SqpDriverAdaptiveMu's fixture comment already records why a
// lever A/B run there can be blind: at 1e-6 a solve certifies before either
// the mu schedule or a refinement footprint has anything to say. So every
// cell is measured twice -- once at the shipped tolerances (does the lever
// change what users see today?) and once at 1e-10 (does the lever have a
// mechanism at all?).
struct Regime {
    const char *name;
    double tol;
};

Outcome run_one(const Row &r, const Cell &c, const Regime &g) {
    const HsProblem p = make_hs(r.number);
    SqpOptions opts;
    opts.max_iter = r.max_iter;
    opts.kkt_tol = g.tol;
    opts.feas_tol = g.tol;
    opts.adaptive_mu = c.adaptive_mu;
    opts.qp.ws_algebra = c.algebra;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model);

    const double target = std::isnan(r.f_target) ? p.f_star : r.f_target;
    Outcome o;
    o.status = sol.status;
    o.f_err = std::abs(sol.f - target) / std::max(1.0, std::abs(target));
    o.majors = sol.counters.major_iters;
    o.minors = sol.counters.qp_minor_iters;
    o.factorizations = sol.counters.factorizations;
    o.eqp_refine_steps = sol.counters.eqp_refine_steps;
    o.border_refine_steps = sol.counters.border_refine_steps;
    return o;
}

// THE MEASUREMENT, one tolerance regime at a time. Long-running by battery
// standards (4 x 27 solves per regime) and deliberately assertion-free apart
// from the two invariants at the end, which are statements about the HARNESS
// rather than about the solver.
void sweep(const Regime &g) {
    const std::vector<Cell> cs = cells();
    const std::vector<Row> &rows = battery();

    fmt::print("\n=== EQP refinement A/B: {} cells x {} problems, regime {} (kkt_tol = feas_tol "
               "= {:g}) ===\n",
               cs.size(), rows.size(), g.name, g.tol);
    fmt::print("{:<44} {:>4} {:>12} {:>6} {:>7} {:>6} {:>8} {:>8}\n", "cell", "hs", "f_err", "maj",
               "minor", "fact", "eqp_ref", "bord_ref");

    // Per-cell aggregates, in the order cells() lists them.
    struct Agg {
        int optimal = 0;
        double worst_f_err = 0.0;
        int worst_at = 0;
        Index majors = 0, minors = 0, factorizations = 0;
        Index eqp_refine_steps = 0, border_refine_steps = 0;
        std::vector<std::string> non_optimal;
    };
    std::vector<Agg> aggs(cs.size());
    // Statuses of cell 0 (the shipped defaults are mu=on/border, which is NOT
    // cell 0 -- cell 0 is mu=off/border -- so the divergence column below is
    // against the SHIPPED cell, found by name).
    std::vector<std::vector<SqpStatus>> statuses(cs.size());

    for (std::size_t ci = 0; ci < cs.size(); ++ci) {
        const Cell &c = cs[ci];
        for (const Row &r : rows) {
            const Outcome o = run_one(r, c, g);
            statuses[ci].push_back(o.status);
            Agg &a = aggs[ci];
            if (o.status == SqpStatus::kOptimal) {
                ++a.optimal;
            } else {
                a.non_optimal.push_back(fmt::format("HS{}:{}", r.number, status_name(o.status)));
            }
            if (o.f_err > a.worst_f_err) {
                a.worst_f_err = o.f_err;
                a.worst_at = r.number;
            }
            a.majors += o.majors;
            a.minors += o.minors;
            a.factorizations += o.factorizations;
            a.eqp_refine_steps += o.eqp_refine_steps;
            a.border_refine_steps += o.border_refine_steps;

            fmt::print("{:<44} {:>4} {:>12.3e} {:>6} {:>7} {:>6} {:>8} {:>8}{}\n", c.name(),
                       r.number, o.f_err, o.majors, o.minors, o.factorizations, o.eqp_refine_steps,
                       o.border_refine_steps,
                       o.status == SqpStatus::kOptimal
                           ? ""
                           : fmt::format("  <-- {}", status_name(o.status)));
        }
    }

    // The shipped configuration, for the divergence column: adaptive_mu on,
    // border mode.
    std::size_t shipped = 0;
    for (std::size_t ci = 0; ci < cs.size(); ++ci) {
        if (cs[ci].adaptive_mu && cs[ci].algebra == WorkingSetLinearAlgebra::kSchurBorder) {
            shipped = ci;
        }
    }

    fmt::print("\n=== per-cell aggregate, regime {} (27 problems each) ===\n", g.name);
    fmt::print("{:<44} {:>4} {:>12} {:>5} {:>4} {:>6} {:>6} {:>6} {:>8} {:>8}\n", "cell", "opt",
               "worstF", "@HS", "div", "maj", "minor", "fact", "eqp_ref", "bord_ref");
    for (std::size_t ci = 0; ci < cs.size(); ++ci) {
        const Agg &a = aggs[ci];
        int diverged = 0;
        for (std::size_t k = 0; k < rows.size(); ++k) {
            if (statuses[ci][k] != statuses[shipped][k]) {
                ++diverged;
            }
        }
        fmt::print("{:<44} {:>4} {:>12.3e} {:>5} {:>4} {:>6} {:>6} {:>6} {:>8} {:>8} {}\n",
                   cs[ci].name(), a.optimal, a.worst_f_err, a.worst_at, diverged, a.majors,
                   a.minors, a.factorizations, a.eqp_refine_steps, a.border_refine_steps,
                   fmt::join(a.non_optimal, ","));
    }
    fmt::print("(div = problems whose STATUS differs from the shipped cell '{}')\n",
               cs[shipped].name());

    // Harness invariants, not solver claims: every cell ran every problem, and
    // no cell can report an EXTRA solve_eqp refinement step -- that path has no
    // iterated loop at all now (solver_counters.h's QpCounters note).
    for (std::size_t ci = 0; ci < cs.size(); ++ci) {
        ASSERT_EQ(statuses[ci].size(), rows.size());
        EXPECT_EQ(aggs[ci].eqp_refine_steps, 0) << "cell " << cs[ci].name();
    }
}

// THE ILL-SCALED FIXTURE, ported verbatim (modulo naming) from
// tests/test_sqp_driver.cpp's ScaledRowModel -- the fixture the two
// SqpDriverAdaptiveMu tests are built on, and the ONLY place in this project
// where the fixed-mu accuracy ceiling has ever been demonstrated:
//
//     min 1/2||x||^2  s.t.  a*x0 + a*x1 = 2a,   x0 >= lo,  x1 free.
//
// It is itself a QP, so every linearization is exact and the solve isolates
// the ENGINE's regularization footprint from any nonlinear effect. Scaling
// the row by `a` divides the equality multiplier by `a`, so the engine's
// dual_mu * |lambda_e| footprint on that row is amplified by 1/a: at
// a = 1e-3 that is a 1000x amplification, which is what makes the ceiling
// visible in x.
//
// IT IS IN THE A/B BECAUSE THE BATTERY CANNOT SEE THE QUESTION. Measured
// below, all 27 HS problems return byte-identical results in every cell at
// both tolerance regimes -- the lever moves nothing there. The prior
// question ("is adaptive-mu a real mechanism or compensation for
// under-refinement?") is only decidable on a fixture where the ceiling
// exists, and this is that fixture.
// The "no bound" sentinel this project's models use (nlp_model.h treats
// magnitudes at or beyond 1e20 as absent), spelled out here exactly as
// tests/test_sqp_restoration.cpp does.
constexpr double kInfBound = 1e20;

class ScaledRowModel : public NlpModel {
  public:
    ScaledRowModel(double a, double lo, Vec x0) : a_(a), lo_(lo), x0_(x0) {}
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override { return 0.5 * x.squaredNorm(); }
    Vec eval_grad(const Vec &x) const override { return x; }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << a_ * x(0) + a_ * x(1) - 2.0 * a_;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale;
        h.insert(1, 1) = obj_scale;
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = a_;
        j.insert(0, 1) = a_;
        j.makeCompressed();
        return j;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    const Vec &lower() const override {
        lower_ = Vec::Constant(2, -kInfBound);
        lower_(0) = lo_;
        return lower_;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kInfBound);
        return u;
    }
    Vec start_point() const override { return x0_; }

  private:
    double a_, lo_;
    Vec x0_;
    mutable Vec lower_;
};

// =====================================================================
// R-4: THE SECOND, INDEPENDENT CEILING FIXTURE (Phase-5 Task 4).
//
// docs/notes/2026-07-29-eqp-refinement-ab.md's R-4 records that the WHOLE
// quantitative answer above rests on ONE 2-variable fixture (ScaledRowModel),
// because it was the only thing in this project that exhibited the fixed-mu
// accuracy ceiling at all, and it asks for a second one with a DIFFERENT
// active-set shape, a DIFFERENT source of multiplier inflation and more than
// two variables before any refinement default is re-ruled. This is it.
//
// THE CONSTRUCTION: F7, THE COLLOCATION FAMILY, WITH ITS OBJECTIVE INFLATED.
// tests/sqp/support/scale_problems.h's F7CollocationChain wrapped so that
//
//     f_S(x) = S * f(x),      constraints, bounds and start point UNTOUCHED.
//
// The optimizer of f_S is x*(p) exactly as for f -- scaling an objective moves
// no minimizer -- and the KKT multipliers scale with it: (lambda_e, lambda_i,
// z) -> S*(lambda_e, lambda_i, z). The Lagrangian is likewise S*(f + sum
// lambda_orig * c), so hess L_S(x, s, le, li) = model.eval_hess(x, s*S, le,
// li) EXACTLY, with le/li already the inflated multipliers the driver carries
// -- which is what the forwarding below does, and why no term has to be
// re-derived here.
//
// HOW IT IS INDEPENDENT OF ScaledRowModel, item by item against R-4's ask:
//
//   * SOURCE OF MULTIPLIER INFLATION. ScaledRowModel scales a CONSTRAINT ROW
//     by a, which inflates lambda_e by 1/a AND ill-conditions the Jacobian.
//     Here the Jacobian and the constraint rows are byte-identical to plain
//     F7 -- Je is the real trapezoidal block-banded matrix, Ji the real path
//     rows -- and the inflation comes entirely from the OBJECTIVE side. So the
//     ceiling cannot be an artifact of a badly scaled constraint block. (The
//     engine's own QP-level battery makes the same distinction with
//     objective_inflated_lambda_qp in test_qp_engine_border.cpp; this is its
//     driver-level, many-variable counterpart.)
//   * ACTIVE-SET SHAPE. ScaledRowModel has one equality row and one active
//     BOUND at the optimum, and nothing else. F7 in its wide window has
//     ns*N equality rows, a Theta(N) window of active general INEQUALITY
//     rows, and NO active bound at all (the box on the controls is strictly
//     inactive at x* by construction). The two shapes have no feature in
//     common.
//   * SIZE. 2 variables versus n = N*(ns + nc); the sweep below runs n = 60.
//   * NONLINEARITY. ScaledRowModel is a QP (linear constraint, constant
//     Hessian). F7's path rows are quadratic, so the active set has to be
//     IDENTIFIED rather than declared, and hess L genuinely depends on
//     lambda_i.
class ObjectiveInflatedF7 : public NlpModel {
  public:
    ObjectiveInflatedF7(Index nodes, double p, double scale)
        : inner_(nodes, 3, 2, p, 1.0), scale_(scale), p_(p) {}

    Index n() const override { return inner_.n(); }
    Index me() const override { return inner_.me(); }
    Index mi() const override { return inner_.mi(); }

    double eval_f(const Vec &x) const override { return scale_ * inner_.eval_f(x); }
    Vec eval_grad(const Vec &x) const override { return scale_ * inner_.eval_grad(x); }
    Vec eval_ce(const Vec &x) const override { return inner_.eval_ce(x); }
    Vec eval_ci(const Vec &x) const override { return inner_.eval_ci(x); }

    // hess L_S = S*obj_scale*hess f + sum (S*lambda) hess c, and the caller
    // already hands us the inflated multipliers, so the ONLY change is on
    // obj_scale. See the block comment above for the derivation.
    SpMatRM eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                      const Vec &lambda_i) const override {
        return inner_.eval_hess(x, obj_scale * scale_, lambda_e, lambda_i);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        return inner_.eval_jac_e(x);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        return inner_.eval_jac_i(x);
    }
    const Vec &lower() const override { return inner_.lower(); }
    const Vec &upper() const override { return inner_.upper(); }
    Vec start_point() const override { return inner_.start_point(); }

    // The family's own analytic optimum, unchanged by the inflation.
    Vec x_star() const { return inner_.x_star(p_); }
    Index active_rows() const {
        const test_support::AnalyticActiveSet a = inner_.active_set(p_);
        return static_cast<Index>(std::count(a.ineq_active.begin(), a.ineq_active.end(), 1));
    }

  private:
    test_support::F7CollocationChain inner_;
    double scale_;
    double p_;
};

// The second ceiling fixture across the same four cells scaled_row_sweep uses,
// reporting the same quantity (relative error in x against the analytically
// known optimum) so the two fixtures' tables are read side by side.
void f7_ceiling_sweep(Index nodes, double p, double scale, double tol) {
    fmt::print("\n=== second ceiling fixture (ObjectiveInflatedF7 nodes={} p={:g} scale={:g}, "
               "kkt_tol = feas_tol = {:g}) ===\n",
               nodes, p, scale, tol);
    fmt::print("{:<44} {:>14} {:>12} {:>5} {:>6} {:>8} {:>8}\n", "cell", "status", "rel_x_err",
               "maj", "minor", "eqp_ref", "bord_ref");
    for (const Cell &c : cells()) {
        ObjectiveInflatedF7 model(nodes, p, scale);
        const Vec x_star = model.x_star();
        SqpOptions opts;
        opts.kkt_tol = tol;
        opts.feas_tol = tol;
        opts.max_iter = 60;
        opts.qp.max_iter = 5000; // the wide window needs it (scale-study note S5)
        opts.adaptive_mu = c.adaptive_mu;
        opts.qp.ws_algebra = c.algebra;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model, model.start_point());
        const double err = (sol.x - x_star).norm() / x_star.norm();
        fmt::print("{:<44} {:>14} {:>12.4e} {:>5} {:>6} {:>8} {:>8}\n", c.name(),
                   status_name(sol.status), err, sol.counters.major_iters,
                   sol.counters.qp_minor_iters, sol.counters.eqp_refine_steps,
                   sol.counters.border_refine_steps);
    }
}

// The ill-scaled fixture across all eight cells. Reports the quantity
// AdaptiveMuRecoversTailAccuracy asserts on -- relative error in x against
// the exactly-known optimum (lo, 2 - lo) -- alongside the status and both
// refinement counters, so a cell that recovers accuracy shows WHICH lever
// paid for it.
void scaled_row_sweep(double a, double lo, double tol) {
    Vec x0 = Vec::Zero(2);
    Vec x_star(2);
    x_star << lo, 2.0 - lo;

    fmt::print("\n=== ill-scaled fixture (ScaledRowModel a={:g}, lo={:g}, kkt_tol = feas_tol = "
               "{:g}) ===\n",
               a, lo, tol);
    fmt::print("{:<44} {:>14} {:>12} {:>5} {:>6} {:>8} {:>8}\n", "cell", "status", "rel_x_err",
               "maj", "minor", "eqp_ref", "bord_ref");
    for (const Cell &c : cells()) {
        ScaledRowModel model(a, lo, x0);
        SqpOptions opts;
        opts.kkt_tol = tol;
        opts.feas_tol = tol;
        opts.adaptive_mu = c.adaptive_mu;
        opts.qp.ws_algebra = c.algebra;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model, x0);
        const double err = (sol.x - x_star).norm() / x_star.norm();
        fmt::print("{:<44} {:>14} {:>12.4e} {:>5} {:>6} {:>8} {:>8}\n", c.name(),
                   status_name(sol.status), err, sol.counters.major_iters,
                   sol.counters.qp_minor_iters, sol.counters.eqp_refine_steps,
                   sol.counters.border_refine_steps);
    }
}

} // namespace

// THE FOOTPRINT-RULE PROBE, at the QP level rather than the solver level --
// and, since the deletion of QpOptions::eqp_refine, the STANDING DERIVATION of
// why the eliminated path takes one refinement step and not a loop.
//
// The rule the bordered path runs on (bordered_eqp.h) refines only while the
// unregularized residual EXCEEDS the regularization footprint
// ||diag(reg)*y||inf. The measured claim about solve_eqp is that its mandatory
// step already lands BELOW that footprint, so an iterated loop there would
// have nothing to do -- which is why the flag that added one was measured
// inert on both shipped backends and removed. This probe prints residual and
// footprint side by side over a spread of deliberately nasty systems
// (ill-scaled rows walking the multiplier up as 1/a^2, rank-deficient and
// near-singular Hessians, and Hessians scaled up to 1e14) so that claim is
// re-derivable rather than quoted, and so a future backend that broke it would
// be caught by running this rather than by reasoning about it.
//
// The step-count and stopped-by columns went with the loop; `margin` replaces
// them and carries the same information in the only form still meaningful:
// residual/footprint, which must stay below 1 for the single step to be
// enough. See docs/notes/2026-07-29-eqp-refinement-ab.md's DISPOSITION.
//
//     build/tests/hven_sqp_tests --gtest_also_run_disabled_tests \
//         --gtest_filter='EqpRefinementAb.DISABLED_FootprintRuleProbe'
TEST(EqpRefinementAb, DISABLED_FootprintRuleProbe) {
    fmt::print("\n=== footprint-rule probe (solve_eqp, one mandatory step) ===\n");
    fmt::print("{:<30} {:>12} {:>12} {:>12}  {}\n", "system", "residual", "footprint", "margin",
               "verdict");

    const auto probe = [](const std::string &name, const QpProblem &qp, const WorkingSet &ws) {
        QpOptions on;
        detail::KktFactor kkt;
        const EqpResult r = solve_eqp(qp, ws, kkt, on);

        Vec stat = qp.H.selfadjointView<Eigen::Upper>() * r.x + qp.g;
        if (qp.me() > 0) {
            stat += Eigen::MatrixXd(qp.Ae).transpose() * r.lambda_e;
        }
        double residual = stat.lpNorm<Eigen::Infinity>();
        if (qp.me() > 0) {
            residual = std::max(residual, (qp.Ae * r.x - qp.be).lpNorm<Eigen::Infinity>());
        }
        const double footprint =
            std::max(on.primal_delta * r.x.lpNorm<Eigen::Infinity>(),
                     qp.me() > 0 ? on.dual_mu * r.lambda_e.lpNorm<Eigen::Infinity>() : 0.0);
        // WOULD AN ITERATED LOOP HAVE HAD ANYTHING TO DO? The bordered rule's
        // two exits, evaluated against this path's single-step answer: the
        // footprint test (residual above ||diag(reg)*y||inf) and the relative
        // floor beneath it, which scales with ||rhs||inf and so is itself
        // enormous on a Hessian scaled to 1e14. floor_abs approximates the
        // rule's ||rhs||inf by ||g||inf, exact for these all-free fixtures up
        // to the constraint rhs. A row reading "would refine" is the finding
        // this probe exists to surface -- none has ever appeared on either
        // shipped backend.
        const double floor_abs =
            detail::kBorderRefineRelFloor * std::max(1.0, qp.g.lpNorm<Eigen::Infinity>());
        const char *verdict = "would refine";
        if (residual <= footprint) {
            verdict = "footprint";
        } else if (residual <= floor_abs) {
            verdict = "rel-floor";
        }
        const double margin = residual / std::max(footprint, floor_abs);
        fmt::print("{:<30} {:>12.4g} {:>12.4g} {:>12.4g}  {}\n", name, residual, footprint, margin,
                   verdict);
    };

    // Upper-triangle storage helper (QpProblem::validate rejects anything
    // below the diagonal).
    const auto upper = [](double h00, double h01, double h11) {
        Eigen::MatrixXd hd(2, 2);
        hd << h00, h01, 0.0, h11;
        return SpMatRM(hd.sparseView());
    };
    const auto unbounded_box = [](QpProblem &qp) {
        qp.Ai.resize(0, 2);
        qp.bi = Vec(0);
        qp.lower = Vec::Constant(2, -1e20);
        qp.upper = Vec::Constant(2, 1e20);
    };

    // (1) the ill-scaled ladder: min 1/2||x||^2 s.t. a(x0 + x1) = -1, whose
    // multiplier grows as 1/(2a^2) and drags the footprint up with it.
    for (double a : {1e-1, 1e-2, 1e-3, 1e-4, 1e-5, 1e-6}) {
        QpProblem qp;
        qp.H = upper(1, 0, 1);
        qp.g = Vec::Zero(2);
        Eigen::MatrixXd aed(1, 2);
        aed << a, a;
        qp.Ae = aed.sparseView();
        qp.be = Vec::Constant(1, -1.0);
        unbounded_box(qp);
        probe(fmt::format("ladder a={:g}", a), qp, WorkingSet(2, 0));
    }

    // (2) rank-deficient and near-singular Hessians -- the eliminated-path
    // analogue of the exactly-singular seed K0 that made the BORDERED path
    // need extra steps in the first place (bordered_eqp.h's LOAD-BEARING
    // BORDERS note).
    for (double eps : {0.0, 1e-14, 1e-10}) {
        QpProblem qp;
        qp.H = upper(1, -1, 1 + eps);
        qp.g = (Vec(2) << -1, 1).finished();
        qp.Ae.resize(0, 2);
        qp.be = Vec(0);
        unbounded_box(qp);
        probe(fmt::format("near-singular H eps={:g}", eps), qp, WorkingSet(2, 0));
    }

    // (3) THE HS26 SEED SYSTEM ITSELF, all variables free -- the very system
    // whose BORDERED solve needs six extra steps
    // (HsBattery.RefinementStepCountersOnBorderRepro). Solved through the
    // ELIMINATED path it has no cancellation to recover from, which is the
    // whole structural asymmetry between the two paths.
    {
        QpProblem qp;
        SpMatRM h(3, 3);
        const std::vector<Eigen::Triplet<double>> t = {
            {0, 0, 2.0}, {0, 1, -2.0}, {1, 1, 2.0}, {2, 2, 0.0}};
        h.setFromTriplets(t.begin(), t.end());
        qp.H = h;
        qp.g = (Vec(3) << -9.2, 9.2, 0.0).finished();
        Eigen::SparseMatrix<double, Eigen::RowMajor> ae(1, 3);
        const std::vector<Eigen::Triplet<double>> at = {{0, 0, 5.0}, {0, 1, -10.4}, {0, 2, 32.0}};
        ae.setFromTriplets(at.begin(), at.end());
        qp.Ae = ae;
        qp.be = Vec::Zero(1);
        qp.Ai = Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 3);
        qp.bi = Vec(0);
        qp.lower = Vec::Constant(3, -1e20);
        qp.upper = Vec::Constant(3, 1e20);
        probe("HS26 seed system (all free)", qp, WorkingSet(3, 0));
    }

    // (4) badly scaled Hessians against a badly scaled row.
    for (double s : {1e6, 1e10, 1e14}) {
        QpProblem qp;
        qp.H = upper(1, 0, s);
        qp.g = (Vec(2) << -1, -s).finished();
        Eigen::MatrixXd aed(1, 2);
        aed << 1e-6, 1;
        qp.Ae = aed.sparseView();
        qp.be = Vec::Constant(1, 1.0);
        unbounded_box(qp);
        probe(fmt::format("scaled H s={:g}", s), qp, WorkingSet(2, 0));
    }
}

TEST(EqpRefinementAb, DISABLED_FullBattery) {
    sweep(Regime{"shipped", 1e-6});
    sweep(Regime{"tight", 1e-10});
    scaled_row_sweep(1e-3, 1.5, 1e-10);
    // R-4's second, independent ceiling fixture (Phase-5 Task 4). Same four
    // cells, same reported quantity, a fixture with nothing in common with
    // ScaledRowModel but the ceiling itself -- see ObjectiveInflatedF7.
    for (const double scale : {1.0, 1e3, 1e4, 1e6}) {
        f7_ceiling_sweep(/*nodes=*/12, /*p=*/0.85, scale, 1e-10);
    }
}

// The harness's own smoke test, and the one part of this file that votes on
// the build: every cell must still SOLVE. HS26 is the problem the whole carry
// came from (its first subproblem is what Task 11b's iterated bordered
// refinement fixed), so a lever combination that breaks it breaks the thing
// this file exists to measure.
// R-4 DISCHARGED: THE SECOND, INDEPENDENT CEILING FIXTURE (Phase-5 Task 4).
//
// docs/notes/2026-07-29-eqp-refinement-ab.md closes with R-4: every
// quantitative claim in that note rests on ONE 2-variable fixture
// (ScaledRowModel), because it was the only thing in this project that
// exhibited the fixed-mu accuracy ceiling at all, and "a ruling that CHANGES a
// default should wait for" a second, independent one. ObjectiveInflatedF7
// above is that fixture -- see its block comment for the four ways in which it
// is independent -- and THIS test is the verification that the note's ruling
// (keep adaptive_mu on; keep the shipped footprint stopping rule; the deleted
// eqp_refine flag stays deleted) holds on it.
//
// THE SIGNATURE IT REPRODUCES, at nodes = 12 (n = 60), p = 0.85 (F7's wide
// window: 10 of the 12 path rows active at the optimum, no bound active),
// objective scale 1e4, kkt_tol = feas_tol = 1e-10. MEASURED, clang/MKL, this
// machine, and identical in Release and Debug:
//
//     cell                     status      rel. x error   maj  minor  eqp_ref
//     mu=off / border          kMaxIter      4.4062e-06    60     64        0
//     mu=off / refactorize     kMaxIter      4.4062e-06    60     64        0
//     mu=on  / border          kOptimal      5.9218e-12     7     12        0
//     mu=on  / refactorize     kOptimal      5.9218e-12     7     12        0
//
// -- the SAME shape ScaledRowModel(a = 1e-3, lo = 1.5) gives (kMaxIter at
// 3.1000e-05 versus kOptimal at 3.1622e-11): adaptive_mu is the only lever
// that clears the ceiling, it clears it in BOTH algebra modes, and the two
// modes are byte-identical to each other in every cell. The note's conclusions
// 1 and 4 are therefore no longer single-fixture claims.
//
// WHY THE OBJECTIVE SCALE IS 1e4 AND NOT MORE. The ceiling is dual_mu *
// |lambda| in constraint units, so it grows with the scale: measured over
// scale = 1, 1e3, 1e4, 1e6 the mu=off error runs 5.9e-14 (kOptimal -- no
// ceiling yet), 5.7e-08, 4.4e-06, 1.2e-02. At 1e6 the regularized KKT system
// can no longer represent the multipliers at all and BOTH mu settings report
// kInfeasible at 1.2e-02 -- an honest failure, agreed on by both algebra
// modes, but no longer a MU experiment, so it is outside the pinned range.
// DISABLED_FullBattery prints all four scales; this test pins the one that
// isolates the lever.
//
// eqp_refine_steps IS ASSERTED ZERO HERE TOO, which is the standing invariant
// solver_counters.h describes: solve_eqp takes its one mandatory refinement step and no
// more, and a nonzero reading would mean it grew a second one.
TEST(EqpRefinementAb, SecondCeilingFixtureReproducesTheAdaptiveMuRuling) {
    constexpr Index kNodes = 12;
    constexpr double kP = 0.85;
    constexpr double kScale = 1e4;
    constexpr double kTol = 1e-10;

    for (const Cell &c : cells()) {
        SCOPED_TRACE(c.name());
        ObjectiveInflatedF7 model(kNodes, kP, kScale);
        // The fixture is only a WIDE-WINDOW, no-active-bound fixture if the
        // family says so -- re-derived, not assumed, so a change to F7's
        // geometry cannot silently turn this into a different experiment.
        ASSERT_EQ(model.n(), 60);
        ASSERT_GT(model.active_rows(), 0) << "p must sit in F7's wide window";

        SqpOptions opts;
        opts.kkt_tol = kTol;
        opts.feas_tol = kTol;
        opts.max_iter = 60;
        opts.qp.max_iter = 5000;
        opts.adaptive_mu = c.adaptive_mu;
        opts.qp.ws_algebra = c.algebra;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model, model.start_point());
        const Vec x_star = model.x_star();
        const double err = (sol.x - x_star).norm() / x_star.norm();

        EXPECT_EQ(sol.counters.eqp_refine_steps, 0)
            << "solve_eqp must still take exactly its one mandatory step";
        if (c.adaptive_mu) {
            EXPECT_EQ(sol.status, SqpStatus::kOptimal);
            EXPECT_LT(err, 1e-10) << "observed 5.9218e-12";
        } else {
            EXPECT_EQ(sol.status, SqpStatus::kMaxIter)
                << "the fixed-mu ceiling must still block certification";
            EXPECT_GT(err, 1e-8) << "observed 4.4062e-06 -- the ceiling itself";
        }
        RecordProperty(fmt::format("rel_x_err[{}]", c.name()), fmt::format("{:.4e}", err));
    }
}

TEST(EqpRefinementAb, EveryCellSolvesTheBorderReproProblem) {
    const std::vector<Cell> cs = cells();
    ASSERT_EQ(cs.size(), 4u) << "the A/B is a 2x2 cross product";
    for (const Cell &c : cs) {
        SCOPED_TRACE(c.name());
        const Outcome o = run_one(Row{26, 60, std::numeric_limits<double>::quiet_NaN()}, c,
                                  Regime{"shipped", 1e-6});
        EXPECT_EQ(o.status, SqpStatus::kOptimal);
        EXPECT_LT(o.f_err, 1e-6);
        EXPECT_EQ(o.eqp_refine_steps, 0);
        if (c.algebra == WorkingSetLinearAlgebra::kRefactorize) {
            EXPECT_EQ(o.border_refine_steps, 0) << "refactorize mode never borders";
        } else {
            EXPECT_GT(o.border_refine_steps, 0) << "border mode refines every bordered solve";
        }
    }
}
