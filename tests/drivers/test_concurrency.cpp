// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/drivers/test_concurrency.cpp -- THE CONCURRENCY CONTRACT'S TWO PINS
// (M6 W6 T4, and the shared-pool fan-out pin added at M7 W0 T1).
//
// The owner's ruling of 2026-09-11 (docs/notes/2026-08-m6-ledger.md:4419-4432,
// the jet ruling) is that hven provides THREAD-SAFE SOLVES and no
// parallel-solve facility, and it registered exactly one pin for W6: N solver
// instances on N threads at `threads = 1` produce results bit-identical to the
// same solves run serially, with no shared mutable state between instances.
// `docs/concurrency.md` is the contract this test is the evidence for, and it
// points at this test by name.
//
// It lives in the shared-driver suite rather than in either engine's, because
// the claim is about BOTH engines and about the shared `common.threads` field:
// two of the four cells run `SqpSolver`, two run `IpmSolver`, each instance on
// its own `std::thread`, each over its own model, and every options value
// carries `common.threads = 1`.
//
// WHY `common.threads` AND NOT AN ENVIRONMENT PIN. CLAUDE.md §7's co-run terms
// ask for MKL_NUM_THREADS=1 per process; the ruling asks for `threads = 1`.
// The option is the stronger of the two and the one a consumer actually has:
// it applies a THREAD-LOCAL MKL override at backend-call scope
// (`MklThreadScope`, include/hven/detail/linear/thread_scope.h) and undoes it
// on every exit, so each worker thread is pinned to one backend thread without
// any instance reaching out to touch the process. No `setenv` appears here,
// deliberately: a test that set the environment would be pinning the process
// rather than exercising the contract.
//
// WHAT THE FIRST PIN AVOIDS, AND WHY THAT IS NOT A DODGE. The ruling names
// three process-global exceptions; `docs/concurrency.md` names them as audited.
// The one that could reach those cells is the interior-point engine's
// EVALUATION POOL (`hven::utils::set_num_threads`,
// src/interior/utils/thread_pool.cpp), which is process-global by design. Those
// cells avoid it BY CONSTRUCTION rather than by arrangement: `make_nlp_program`
// defaults to one partition, and `parallel_sequence`
// (include/hven/detail/interior/utils/thread_pool.h) runs inline whenever
// `nparts <= 1`, so the pool is never dispatched to and never even constructed.
// That premise is ASSERTED below (the adopted partition count, and the pool
// width unmoved across the whole test) rather than assumed.
//
// THE SECOND PIN IS THE SHAPE THE FIRST ONE AVOIDS (M7 W0 T1 --
// `Concurrency.TwoPartitionedProgramsOnTwoThreadsMatchSerial`). The W6 T4
// review found `docs/concurrency.md`'s recipe for N instances each fanning out
// into the ONE process-global pool resting on the pool's own DESIGN STATEMENT
// ("Per-dispatch latches make concurrent dispatches from separate threads
// inherently safe", thread_pool.h) and on nothing measured, and that is the
// shape a consumer's map actually runs. So: two DIFFERENT programs, each built
// through the public `make_nlp_program(problem, num_partitions)` with a count
// above 1 that the layout ACTUALLY ADOPTS (asserted, because the count is
// clamped rather than refused), solved once serially and once with each program
// on its own `std::thread` -- both dispatching into the same pool at the same
// time. The comparison is the first pin's, helper for helper.
//
// WHAT THAT SECOND PIN DOES AND DOES NOT EXERCISE, stated here so nobody reads
// more into it than it measured. `make_nlp_program`'s own doc comment says
// PARTITIONS THROUGH THIS ADAPTER ARE LAYOUT ONLY: all three pieces are
// ThreadingFlags::MainThread, and analyze_partitioning forces every MainThread
// function into the LAST partition, which parallel_sequence runs INLINE on the
// calling thread. So the user's eval_f / eval_jac / eval_hess still run on the
// dispatching thread. What genuinely goes concurrent is the DISPATCH ITSELF --
// two threads enqueueing into one pool's work-stealing queues, each waiting on
// its own per-dispatch latch, with pool workers running the other partitions'
// (empty) bodies and writing into each program's own `vals_scratch_` -- plus
// the `parallel_blocks` passes over the KKT triplet fill, which are sized by
// num_partitions_ too. That is exactly the claim the design statement makes and
// exactly what was unmeasured. GENUINE partitioned EVALUATION over an
// NlpTripletModel needs a thread-safe adapter and is registered for M7; when it
// lands, this test's model is already the right size for it.
//
// THE ASSERTION IS IN TWO STEPS, and the order matters (plan §1 W6.T4, A3).
//
//   STEP ONE -- RECORD. Whether the concurrent readings are BYTE-EQUAL to the
//   serial ones, per column class, is a FINDING either way: the ruling's word
//   is "bit-identical", and what this box does is worth recording whether or
//   not it is what a later box will do. The finding goes to RecordProperty and
//   to the test's own output. Nothing is asserted from it.
//
//   STEP TWO -- ASSERT. Counters and statuses EXACT; the floating-point
//   measures through the 1e-5 relative gate with the 1e-13 absolute floor. That
//   is the rule tests/sqp/test_corpus_cells.cpp:908-944 states and :3558 floors,
//   and the reason it applies here is §7's own: MKL's kernels are
//   address-sensitive, and two runs of the same solve at different addresses --
//   which is exactly what a heap touched by four threads produces -- may differ
//   in a measure's last digits without any counter moving. Counters and
//   statuses are not produced by floating-point arithmetic, so a single-bit
//   move in one of those is a real regression and is refused.

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <Eigen/Core>

#include <hven/detail/interior/utils/thread_pool.h>
#include <hven/drivers/ipm_solver.h>
#include <hven/drivers/ipm_solver_types.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>
#include <hven/model/make_nlp_program.h>
#include <hven/model/nlp_model.h>
#include <hven/model/nlp_triplet_model.h>
#include <hven/model/non_linear_program.h>

#include "sqp/support/hs_problems.h"
#include "support/hs071_problem.h"

namespace {

// The residual-class gate and its absolute floor, carried verbatim from
// tests/sqp/test_corpus_cells.cpp:935 and :3558 rather than re-derived: the
// same measures, the same reason, so the same numbers.
constexpr double kResidualRelativeGate = 1.0e-5;
constexpr double kResidualAbsFloor = 1.0e-13;

// One exact column: a counter, a status, or a size. Aggregate members are bare
// snake_case (CLAUDE.md §4).
struct ExactColumn {
    std::string name;
    long long value = 0;
};

// One floating-point measure column: gated, never asserted byte-equal.
struct MeasureColumn {
    std::string name;
    double value = 0.0;
};

// Everything one cell's solve is read for.
struct CellReading {
    std::string cell;
    std::vector<ExactColumn> exact;
    std::vector<MeasureColumn> measures;
};

// Byte equality on a double, read literally: the bit patterns, not a numeric
// comparison. NaN == NaN is TRUE here and must be, since an unmeasured column
// is NaN in both runs and that agreement is real.
bool bits_equal(double a, double b) {
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

// The gate the measures are asserted through.
bool within_gate(double a, double b) {
    if (bits_equal(a, b)) {
        return true;
    }
    // A non-finite value never passes the numeric path: inf against a finite
    // value would otherwise read as inf <= inf and succeed. Two identical
    // non-finite spellings (NaN against NaN included) already passed above.
    if (!std::isfinite(a) || !std::isfinite(b)) {
        return false;
    }
    const double scale = std::max(std::abs(a), std::abs(b));
    return std::abs(a - b) <= std::max(kResidualRelativeGate * scale, kResidualAbsFloor);
}

// ---------------------------------------------------------------------------
// The four cells. Each runner builds EVERYTHING it uses -- options, model,
// solver -- so that two runners share no object at all. That is the ruling's
// "no shared mutable state between instances", written as construction rather
// than asserted afterwards.
// ---------------------------------------------------------------------------

void push_shared_result_columns(const hven::solvers::SolveResult &r, CellReading &out) {
    out.exact.push_back({"status", static_cast<long long>(r.status)});
    out.exact.push_back({"iterations", static_cast<long long>(r.iterations)});
    out.exact.push_back({"x_size", static_cast<long long>(r.x.size())});
    out.exact.push_back({"ce_size", static_cast<long long>(r.ce.size())});
    out.exact.push_back({"ci_size", static_cast<long long>(r.ci.size())});

    out.measures.push_back({"f", r.f});
    out.measures.push_back({"stationarity", r.stationarity});
    out.measures.push_back({"feasibility_e", r.feasibility_e});
    out.measures.push_back({"feasibility_i", r.feasibility_i});
    out.measures.push_back({"complementarity", r.complementarity});
    for (Eigen::Index i = 0; i < r.x.size(); ++i) {
        out.measures.push_back({"x[" + std::to_string(i) + "]", r.x[i]});
    }
}

CellReading run_sqp_cell(int hs_number) {
    const auto p = hven::solvers::test_support::make_hs(hs_number);

    hven::solvers::SqpOptions o;
    o.common.threads = 1;      // THE PIN: one backend thread per instance.
    o.common.print_level = 10; // Silent: a console table is process-wide output.
    hven::solvers::SqpSolver driver(o);

    const hven::solvers::SqpResult r = driver.solve(*p.model, p.model->start_point());

    CellReading out;
    out.cell = "sqp/hs" + std::to_string(hs_number);
    push_shared_result_columns(r, out);
    out.exact.push_back({"major_iters", static_cast<long long>(r.counters.major_iters)});
    out.exact.push_back({"qp_minor_iters", static_cast<long long>(r.counters.qp_minor_iters)});
    out.exact.push_back({"factorizations", static_cast<long long>(r.counters.factorizations)});
    out.exact.push_back({"steps_accepted", static_cast<long long>(r.counters.steps_accepted)});
    out.exact.push_back({"rejected_steps", static_cast<long long>(r.counters.rejected_steps)});
    out.exact.push_back({"soc_steps", static_cast<long long>(r.counters.soc_steps)});
    out.exact.push_back({"soc_applied", static_cast<long long>(r.counters.soc_applied)});
    out.exact.push_back(
        {"elastic_activations", static_cast<long long>(r.counters.elastic_activations)});
    out.exact.push_back(
        {"restoration_iters", static_cast<long long>(r.counters.restoration_iters)});
    return out;
}

// Everything an interior-point reading carries beyond the shared result core.
// The ADOPTED PARTITION COUNT leads, as a column rather than as a side check, so
// that it is compared between the two runs as well as asserted once on the main
// thread -- the first test needs it to be 1 (the pool is never reached) and the
// second needs it to be >= 2 (the pool is reached from both threads at once), and
// both read it from the same place.
void push_ipm_result_columns(const hven::solvers::NonLinearProgram &program,
                             const hven::solvers::IpmResult &r, CellReading &out) {
    out.exact.push_back({"partitions", static_cast<long long>(program.num_partitions_)});
    out.exact.push_back({"phases", static_cast<long long>(r.phases.size())});
    out.exact.push_back(
        {"kkt_analyses_this_call", static_cast<long long>(r.kkt_analyses_this_call)});
    out.exact.push_back(
        {"factorize_count", static_cast<long long>(r.kkt_factor_counters.factorize_count)});
    out.exact.push_back({"solve_count", static_cast<long long>(r.kkt_factor_counters.solve_count)});
    out.exact.push_back(
        {"analyze_count", static_cast<long long>(r.kkt_factor_counters.analyze_count)});
    out.exact.push_back({"soc_steps_taken", static_cast<long long>(r.soc_steps_taken)});
    out.exact.push_back({"watchdog_activations", static_cast<long long>(r.watchdog_activations)});
    out.exact.push_back({"payload_ignored", static_cast<long long>(r.payload_ignored)});

    out.measures.push_back({"kkt_inf", r.kkt_inf});
    out.measures.push_back({"barr_inf", r.barr_inf});
    out.measures.push_back({"econ_inf", r.econ_inf});
    out.measures.push_back({"icon_inf", r.icon_inf});
}

CellReading run_ipm_cell(const std::string &label, const Eigen::VectorXd &x0) {
    hven::solvers::IpmOptions o;
    o.common.threads = 1;      // THE PIN.
    o.common.print_level = 10; // Silent -- this engine's default is 0, i.e. loud.
    hven::solvers::IpmSolver solver(o);

    // ONE PARTITION, the default: THE PREMISE THAT KEEPS THIS CELL CLEAR OF THE
    // EVALUATION POOL. parallel_sequence runs inline whenever nparts <= 1, so
    // the process-global pool is never dispatched to and never constructed.
    const auto program =
        hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
    const hven::solvers::IpmResult r = solver.solve(*program, x0);

    CellReading out;
    out.cell = "ipm/" + label;
    push_shared_result_columns(r, out);
    push_ipm_result_columns(*program, r, out);
    return out;
}

// A second interior-point START, inside HS071's box [1,5]^4 and distinct from
// the shared fixture's (1,5,5,1). A CELL is a (problem, start) pair, so this is
// a distinct cell off a fixture already in the tree -- which is what the brief
// asks for, and it adds no problem class.
Eigen::VectorXd hs071_second_start() {
    Eigen::VectorXd x0(4);
    x0 << 2.0, 4.0, 4.0, 2.0;
    return x0;
}

constexpr int kCellCount = 4;

CellReading run_cell(int index) {
    switch (index) {
    case 0:
        return run_sqp_cell(7);
    case 1:
        return run_sqp_cell(76);
    case 2:
        return run_ipm_cell("hs071@a", hven_drivers_tests::hs071_start());
    default:
        return run_ipm_cell("hs071@b", hs071_second_start());
    }
}

// ---------------------------------------------------------------------------
// THE TWO STEPS, factored out so both pins in this file run the SAME record and
// the SAME assertion over their own readings rather than two copies that can
// drift apart (M7 W0 T1).
// ---------------------------------------------------------------------------

// What STEP ONE records. Every field is a count of columns or a move between
// them, so this is an aggregate and its members are bare snake_case
// (CLAUDE.md section 4).
struct Finding {
    int exact_columns = 0;
    int exact_byte_equal = 0;
    int measure_columns = 0;
    int measure_byte_equal = 0;
    double worst_relative_move = 0.0;
    std::string worst_column = "(none)";

    bool all_byte_equal() const {
        return (exact_byte_equal == exact_columns) && (measure_byte_equal == measure_columns);
    }
};

// STEP ONE -- RECORD. Whether the concurrent readings are BYTE-EQUAL to the
// serial ones, per column class, is a FINDING either way. Nothing is asserted
// from it; the two ASSERT_EQs below are on the SHAPE of the comparison (same
// cells, same columns), without which the finding would be meaningless.
void record_finding(const std::vector<CellReading> &serial,
                    const std::vector<CellReading> &concurrent, Finding &out) {
    ASSERT_EQ(serial.size(), concurrent.size());
    for (std::size_t i = 0; i < serial.size(); ++i) {
        const CellReading &s = serial[i];
        const CellReading &c = concurrent[i];
        ASSERT_EQ(s.cell, c.cell) << "the two runs must have run the same cells in the same order";
        ASSERT_EQ(s.exact.size(), c.exact.size());
        ASSERT_EQ(s.measures.size(), c.measures.size());

        for (std::size_t k = 0; k < s.exact.size(); ++k) {
            ++out.exact_columns;
            if (s.exact[k].value == c.exact[k].value) {
                ++out.exact_byte_equal;
            }
        }
        for (std::size_t k = 0; k < s.measures.size(); ++k) {
            ++out.measure_columns;
            const double a = s.measures[k].value;
            const double b = c.measures[k].value;
            if (bits_equal(a, b)) {
                ++out.measure_byte_equal;
                continue;
            }
            if (std::isfinite(a) && std::isfinite(b)) {
                const double scale = std::max(std::abs(a), std::abs(b));
                const double rel = scale > 0.0 ? std::abs(a - b) / scale : 0.0;
                if (rel > out.worst_relative_move) {
                    out.worst_relative_move = rel;
                    out.worst_column = s.cell + " " + s.measures[k].name;
                }
            }
            std::cout << "[ MOVED    ] " << s.cell << " " << s.measures[k].name << ": serial "
                      << std::scientific << a << " concurrent " << b << "\n";
        }
    }
}

// The finding, to RecordProperty and to the test's own output. `cells` is the
// cell count the reading covers; `what` names the shape that was run, because
// two pins in one binary write into the same property namespace.
void report_finding(const Finding &f, int cells, const char *what) {
    ::testing::Test::RecordProperty("shape", what);
    ::testing::Test::RecordProperty("cells", cells);
    ::testing::Test::RecordProperty("exact_columns", f.exact_columns);
    ::testing::Test::RecordProperty("exact_columns_byte_equal", f.exact_byte_equal);
    ::testing::Test::RecordProperty("measure_columns", f.measure_columns);
    ::testing::Test::RecordProperty("measure_columns_byte_equal", f.measure_byte_equal);
    ::testing::Test::RecordProperty("bit_identical", f.all_byte_equal() ? "yes" : "no");
    ::testing::Test::RecordProperty("worst_relative_move",
                                    std::to_string(f.worst_relative_move).c_str());
    ::testing::Test::RecordProperty("worst_moved_column", f.worst_column.c_str());

    std::cout << "[ FINDING  ] the ruling's literal bit-identity, as observed on this box (" << what
              << "):\n"
              << "[ FINDING  ]   counters and statuses byte-equal: " << f.exact_byte_equal << "/"
              << f.exact_columns << "\n"
              << "[ FINDING  ]   floating measures byte-equal:     " << f.measure_byte_equal << "/"
              << f.measure_columns << "\n"
              << "[ FINDING  ]   bit-identical overall:            "
              << (f.all_byte_equal() ? "YES" : "NO") << "\n"
              << "[ FINDING  ]   worst relative move:              " << std::scientific
              << f.worst_relative_move << " (" << f.worst_column << ")\n";
}

// STEP TWO -- ASSERT. Counters and statuses EXACT; the floating-point measures
// through the 1e-5 relative gate with the 1e-13 absolute floor.
void assert_readings_match(const std::vector<CellReading> &serial,
                           const std::vector<CellReading> &concurrent) {
    for (std::size_t i = 0; i < serial.size(); ++i) {
        const CellReading &s = serial[i];
        const CellReading &c = concurrent[i];
        for (std::size_t k = 0; k < s.exact.size(); ++k) {
            EXPECT_EQ(s.exact[k].name, c.exact[k].name);
            EXPECT_EQ(s.exact[k].value, c.exact[k].value)
                << s.cell << " " << s.exact[k].name
                << ": a counter or a status moved between the serial and the concurrent run. No "
                   "floating-point arithmetic produces these, so this is a real regression.";
        }
        for (std::size_t k = 0; k < s.measures.size(); ++k) {
            EXPECT_EQ(s.measures[k].name, c.measures[k].name);
            EXPECT_TRUE(within_gate(s.measures[k].value, c.measures[k].value))
                << s.cell << " " << s.measures[k].name << ": " << s.measures[k].value << " -> "
                << c.measures[k].value
                << " is past the 1e-5 relative gate (1e-13 absolute floor). Last-digit drift is "
                   "permitted here and nothing else is.";
        }
    }
}

// Every cell must have solved something: a cell that refused at the boundary
// would compare equal to itself and prove nothing.
void assert_cells_are_not_vacuous(const std::vector<CellReading> &serial) {
    for (const CellReading &s : serial) {
        ASSERT_EQ(s.exact[0].name, "status");
        ASSERT_EQ(s.exact[0].value, static_cast<long long>(hven::solvers::SolveStatus::kOptimal))
            << "PREMISE: cell " << s.cell << " must actually converge";
        ASSERT_EQ(s.exact[1].name, "iterations");
        ASSERT_GT(s.exact[1].value, 0) << "PREMISE: cell " << s.cell << " must actually iterate";
    }
}

// Read one exact column out of a reading by name; -1 if it is not there (which
// every caller below treats as a failure, because it would mean the reading's
// own shape moved).
long long exact_column(const CellReading &r, const std::string &name) {
    for (const ExactColumn &col : r.exact) {
        if (col.name == name) {
            return col.value;
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// THE SHARED-POOL FAN-OUT PIN's model (M7 W0 T1).
//
// A PARAMETRIC, STRICTLY CONVEX QCQP, written here rather than borrowed because
// nothing this suite already carries is BIG ENOUGH. The adopted partition count
// is clamped at num_user_kkt_elems_ / kMinKktElementsPerPartition (1000;
// src/model/non_linear_program.cpp:414-426), and num_user_kkt_elems_ through
// this adapter is the Jacobian nonzeros plus the Lagrangian Hessian's, counted
// once (nlp_adapter.h:372 and :511 -- exactly one piece owns the Hessian). HS071
// contributes 18 and would ADOPT ONE partition however many were asked for,
// which is the shape the first pin already covers. The brief's instruction on
// this point is explicit: do not lower the threshold, build a model that clears
// it. So this family is sized, not tuned.
//
//   min  sum_i 0.5 * w_i * (x_i - t_i)^2
//   s.t. x_{2k} + x_{2k+1} = 2.5                    (K equality rows)
//        x_{2k}^2 + x_{2k+1}^2 <= 4.0               (K inequality rows)
//        0.1 <= x_i <= 10                           (never active at the optimum)
//
// with w_i = 1 + 0.25*(i mod 4), t_{2k} = 2.0 + 0.05*(k mod 3) + shift and
// t_{2k+1} = 0.4. The objective is separable and strictly convex, the equality
// rows are affine, the inequality rows are convex, and the pair targets are
// deliberately lopsided (2.0 against 0.4) so that the ball rows are ACTIVE at
// the solution rather than decorative. n = 2K variables, 4K Jacobian nonzeros,
// 2K Hessian nonzeros (the Hessian is DIAGONAL: the objective is separable and
// each ball row touches only its own pair), so num_user_kkt_elems_ = 6K and the
// adopted count is 6K/1000 capped by the request.
//
// x = 1.25 everywhere is strictly inside every bound, satisfies every equality
// row exactly and leaves every ball row slack (3.125 < 4.0), so the start is
// strictly feasible. eval_jac and eval_hess are polynomials and are therefore
// total at the projected origin transcription evaluates them at (x = 0.1).
// ---------------------------------------------------------------------------

inline constexpr double kChainInf = std::numeric_limits<double>::infinity();
inline constexpr double kChainPairSum = 2.5;
inline constexpr double kChainBallCap = 4.0;

class PartitionedChainProblem final : public hven::solvers::NlpTripletModel {
  public:
    PartitionedChainProblem(int pairs, double target_shift)
        : pairs_(pairs), target_shift_(target_shift) {}

    int num_vars() const override { return 2 * pairs_; }
    int num_cons() const override { return 2 * pairs_; } // K equality + K ball rows
    int num_jac_nonzeros() const override { return 4 * pairs_; }
    int num_hess_nonzeros() const override { return 2 * pairs_; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl.setConstant(0.1);
        xu.setConstant(10.0);
        for (int k = 0; k < pairs_; ++k) {
            gl[k] = kChainPairSum; // gl == gu: an equality row
            gu[k] = kChainPairSum;
            gl[pairs_ + k] = -kChainInf; // one-sided: no internal split
            gu[pairs_ + k] = kChainBallCap;
        }
    }

    void eval_f(hven::ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        double v = 0.0;
        for (int i = 0; i < 2 * pairs_; ++i) {
            const double d = x[i] - target(i);
            v += 0.5 * weight(i) * d * d;
        }
        f = v;
    }

    void eval_grad_f(hven::ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        for (int i = 0; i < 2 * pairs_; ++i) {
            g[i] = weight(i) * (x[i] - target(i));
        }
    }

    void eval_g(hven::ConstEigenRef<Eigen::VectorXd> x,
                Eigen::Ref<Eigen::VectorXd> g) const override {
        for (int k = 0; k < pairs_; ++k) {
            g[k] = x[2 * k] + x[2 * k + 1];
            g[pairs_ + k] = x[2 * k] * x[2 * k] + x[2 * k + 1] * x[2 * k + 1];
        }
    }

    // Slot order: the 2K equality entries first, then the 2K ball entries.
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        for (int k = 0; k < pairs_; ++k) {
            r[2 * k] = k;
            c[2 * k] = 2 * k;
            r[2 * k + 1] = k;
            c[2 * k + 1] = 2 * k + 1;
            r[2 * pairs_ + 2 * k] = pairs_ + k;
            c[2 * pairs_ + 2 * k] = 2 * k;
            r[2 * pairs_ + 2 * k + 1] = pairs_ + k;
            c[2 * pairs_ + 2 * k + 1] = 2 * k + 1;
        }
    }

    // The Lagrangian Hessian is diagonal, so the lower triangle is the diagonal.
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        for (int i = 0; i < 2 * pairs_; ++i) {
            r[i] = i;
            c[i] = i;
        }
    }

    void eval_jac(hven::ConstEigenRef<Eigen::VectorXd> x,
                  Eigen::Ref<Eigen::VectorXd> v) const override {
        for (int k = 0; k < pairs_; ++k) {
            v[2 * k] = 1.0;
            v[2 * k + 1] = 1.0;
            v[2 * pairs_ + 2 * k] = 2.0 * x[2 * k];
            v[2 * pairs_ + 2 * k + 1] = 2.0 * x[2 * k + 1];
        }
    }

    void eval_hess(hven::ConstEigenRef<Eigen::VectorXd> x, double obj_factor,
                   hven::ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        (void)x; // every second derivative here is constant in x
        for (int i = 0; i < 2 * pairs_; ++i) {
            // Variable i belongs to pair i/2, whose ball row is pairs_ + i/2.
            // The equality rows are affine and contribute nothing.
            v[i] = obj_factor * weight(i) + 2.0 * lambda[pairs_ + i / 2];
        }
    }

    std::string name() const override { return "PartitionedChainProblem"; }

  private:
    double weight(int i) const { return 1.0 + 0.25 * static_cast<double>(i % 4); }
    double target(int i) const {
        if (i % 2 == 1) {
            return 0.4;
        }
        return 2.0 + 0.05 * static_cast<double>((i / 2) % 3) + target_shift_;
    }

    int pairs_ = 0;
    double target_shift_ = 0.0;
};

// One fan-out cell: a size, a data shift and a REQUESTED partition count. The
// adopted count is read off the built program and asserted, never assumed.
struct FanOutCell {
    std::string label;
    int pairs = 0;
    double target_shift = 0.0;
    int requested_partitions = 0;
};

const std::vector<FanOutCell> &fanout_cells() {
    // 500 pairs -> 6*500 = 3000 user KKT elements -> a cap of 3, so a request of
    // 2 is ADOPTED as 2. 700 pairs -> 4200 -> a cap of 4, so a request of 3 is
    // ADOPTED as 3. The two cells therefore fan out to DIFFERENT widths into the
    // one pool, which is a sharper reading of "concurrent dispatches from
    // separate threads" than two identical dispatches would be.
    static const std::vector<FanOutCell> cells = {
        {"chain500@p2", 500, 0.0, 2},
        {"chain700@p3", 700, 0.2, 3},
    };
    return cells;
}

CellReading run_fanout_cell(const FanOutCell &cell) {
    hven::solvers::IpmOptions o;
    o.common.threads = 1;      // THE PIN, unchanged: one backend thread per instance.
    o.common.print_level = 10; // Silent: a console table is process-wide output.
    hven::solvers::IpmSolver solver(o);

    // THROUGH THE PUBLIC ENTRY, with a count above 1 that the layout ACTUALLY
    // ADOPTS -- which is what puts every eval_kkt / eval_aug / eval_soe of this
    // solve through parallel_sequence's pool arm rather than its inline one.
    const auto program = hven::solvers::make_nlp_program(
        std::make_shared<PartitionedChainProblem>(cell.pairs, cell.target_shift),
        cell.requested_partitions);

    const Eigen::VectorXd x0 = Eigen::VectorXd::Constant(2 * cell.pairs, 1.25);
    const hven::solvers::IpmResult r = solver.solve(*program, x0);

    CellReading out;
    out.cell = "ipm-fanout/" + cell.label;
    push_shared_result_columns(r, out);
    push_ipm_result_columns(*program, r, out);
    out.exact.push_back(
        {"requested_partitions", static_cast<long long>(cell.requested_partitions)});
    out.exact.push_back({"user_kkt_elems", static_cast<long long>(program->num_user_kkt_elems_)});
    return out;
}

} // namespace

// The ONE pin the jet ruling registered. `docs/concurrency.md` names this test.
TEST(Concurrency, NInstancesOnNThreadsMatchSerial) {
    // The process-global evaluation-pool width, read before anything runs. It
    // is never set by this test and must be unmoved by every solve in it --
    // see this file's header on why these cells stay clear of the pool.
    const int pool_width_before = hven::utils::get_num_threads();

    // ---------------------------------------------------------------------
    // (1) THE SERIAL RUN, FIRST, IN THIS PROCESS. Same cells, same order.
    // ---------------------------------------------------------------------
    std::vector<CellReading> serial;
    serial.reserve(kCellCount);
    for (int i = 0; i < kCellCount; ++i) {
        serial.push_back(run_cell(i));
    }

    // The pin is not allowed to be vacuous. (ASSERT_NO_FATAL_FAILURE because a
    // fatal assertion inside a helper returns from the HELPER; the wrapper is
    // what turns it back into an abort of this test.)
    ASSERT_NO_FATAL_FAILURE(assert_cells_are_not_vacuous(serial));

    // ---------------------------------------------------------------------
    // (2) THE CONCURRENT RUN: N instances on N std::threads, one cell each.
    // ---------------------------------------------------------------------
    std::vector<CellReading> concurrent(kCellCount);
    {
        std::vector<std::thread> workers;
        workers.reserve(kCellCount);
        for (int i = 0; i < kCellCount; ++i) {
            workers.emplace_back([i, &concurrent]() { concurrent[i] = run_cell(i); });
        }
        for (std::thread &w : workers) {
            w.join();
        }
    }

    EXPECT_EQ(hven::utils::get_num_threads(), pool_width_before)
        << "no solve, serial or concurrent, may move the process-global evaluation-pool width";

    // ---------------------------------------------------------------------
    // (3) STEP ONE -- RECORD the byte-equality finding, per column class.
    // (4) STEP TWO -- ASSERT. Counters and statuses exact; measures gated.
    // ---------------------------------------------------------------------
    Finding finding;
    ASSERT_NO_FATAL_FAILURE(record_finding(serial, concurrent, finding));
    report_finding(finding, kCellCount, "N instances on N threads, one partition each");
    assert_readings_match(serial, concurrent);

    // The premise that the cells stay clear of the process-global evaluation
    // pool, asserted on the main thread as well as compared above.
    for (int i = 2; i < kCellCount; ++i) {
        EXPECT_EQ(exact_column(serial[i], "partitions"), 1)
            << serial[i].cell
            << ": this cell must adopt ONE partition, so parallel_sequence runs inline and "
               "the process-global evaluation pool is never dispatched to (-1 means the cell "
               "did not report its adopted count at all)";
    }

    // The four cells must be four DIFFERENT solves, or the pin would be one
    // solve run four times and its breadth would be imaginary. The labels are
    // distinct by construction; the two SQP cells are two different problems,
    // which their objectives say, and the two interior-point cells are one
    // problem from two different starts, which their iteration counts say.
    for (int i = 0; i < kCellCount; ++i) {
        for (int j = i + 1; j < kCellCount; ++j) {
            EXPECT_NE(serial[i].cell, serial[j].cell) << "the four cells must be four cells";
        }
    }
    double f_sqp_a = 0.0;
    double f_sqp_b = 0.0;
    for (const MeasureColumn &m : serial[0].measures) {
        if (m.name == "f") {
            f_sqp_a = m.value;
        }
    }
    for (const MeasureColumn &m : serial[1].measures) {
        if (m.name == "f") {
            f_sqp_b = m.value;
        }
    }
    EXPECT_GT(std::abs(f_sqp_a - f_sqp_b), 1.0)
        << "the two SQP cells must be two different problems, not one twice";
}

// THE SHARED-POOL FAN-OUT PIN (M7 W0 T1; the M7-3 / D23 registration, and the
// W6 T4 review's Important-1). See this file's header for what it adds to the
// pin above and, just as importantly, for what it does NOT reach.
TEST(Concurrency, TwoPartitionedProgramsOnTwoThreadsMatchSerial) {
    // The process-global evaluation-pool width, read before anything runs. This
    // test NEVER sets it -- setting it would pin the process rather than
    // exercise the contract -- and it must be unmoved at the end.
    const int pool_width_before = hven::utils::get_num_threads();
    ::testing::Test::RecordProperty("evaluation_pool_width", pool_width_before);
    std::cout << "[ PREMISE  ] process-global evaluation-pool width: " << pool_width_before << "\n";

    // PREMISE ONE -- THERE IS A POOL TO CONTEND FOR AT ALL. parallel_sequence
    // takes its pool arm only when use_thread_pool() holds, which is the
    // process-global width being above 1 (thread_pool.h, use_thread_pool). On a
    // one-core box the shape this test pins DOES NOT EXIST: every dispatch runs
    // inline and nothing about it is concurrent. That is a SKIP and not a pass,
    // because a green cell there would claim evidence the run never produced.
    if (pool_width_before <= 1) {
        ::testing::Test::RecordProperty("skipped", "evaluation-pool width is 1: no fan-out here");
        GTEST_SKIP() << "the process-global evaluation width is " << pool_width_before
                     << ", so parallel_sequence runs every dispatch inline and there is no "
                        "fan-out to pin on this box";
    }

    // PREMISE TWO -- THE POOL REALLY DISPATCHES OFF-THREAD, OBSERVED rather than
    // argued from the source. parallel_sequence(n) enqueues indices [0, n-1) to
    // the pool and runs index n-1 inline on the caller; that sentence is checked
    // here, once, through the very helper every evaluation below goes through.
    // Without it the rest of this test would rest on a code reading.
    {
        const std::thread::id caller = std::this_thread::get_id();
        std::vector<std::thread::id> ran_on(2);
        hven::utils::parallel_sequence(2, [&ran_on](int i) {
            ran_on[static_cast<std::size_t>(i)] = std::this_thread::get_id();
        });
        ASSERT_NE(ran_on[0], caller)
            << "PREMISE: index 0 of a two-way parallel_sequence must run on a POOL thread";
        ASSERT_EQ(ran_on[1], caller)
            << "PREMISE: the last index must run inline on the calling thread";
    }

    const std::vector<FanOutCell> &cells = fanout_cells();
    const int cell_count = static_cast<int>(cells.size());
    ASSERT_EQ(cell_count, 2) << "this pin is the TWO-program shape";

    // ---------------------------------------------------------------------
    // (1) THE SERIAL RUN, FIRST, IN THIS PROCESS. Both programs solved one
    //     after the other on the calling thread, each fanning into the pool.
    // ---------------------------------------------------------------------
    std::vector<CellReading> serial;
    serial.reserve(cells.size());
    for (const FanOutCell &cell : cells) {
        serial.push_back(run_fanout_cell(cell));
    }

    ASSERT_NO_FATAL_FAILURE(assert_cells_are_not_vacuous(serial));

    // PREMISE THREE -- THE REQUESTED PARTITIONS ARE ACTUALLY ADOPTED. The count
    // is CLAMPED, not refused (make_nlp_program's own doc comment), so a request
    // of 2 on a small model is silently adopted as 1 and this whole test would
    // become the first one again, passing while pinning nothing. The adopted
    // count is read off the built program and asserted; it is not assumed.
    std::string adopted_summary;
    for (int i = 0; i < cell_count; ++i) {
        const long long adopted = exact_column(serial[i], "partitions");
        const long long requested = exact_column(serial[i], "requested_partitions");
        const long long elems = exact_column(serial[i], "user_kkt_elems");
        std::cout << "[ PREMISE  ] " << serial[i].cell << ": requested " << requested
                  << " partitions, ADOPTED " << adopted << ", over " << elems
                  << " user KKT elements\n";
        adopted_summary += (i == 0 ? "" : ",") + serial[i].cell + "=" + std::to_string(adopted) +
                           "/" + std::to_string(requested);
        ASSERT_GE(adopted, 2)
            << serial[i].cell << ": this cell must ADOPT at least two partitions (it requested "
            << requested << " over " << elems
            << " user KKT elements, and the cap is one partition per 1000 of those). At one "
               "partition parallel_sequence runs inline and this test would pin nothing.";
    }
    ::testing::Test::RecordProperty("adopted_partitions", adopted_summary.c_str());

    // ---------------------------------------------------------------------
    // (2) THE CONCURRENT RUN: each program on its own std::thread, both fanning
    //     into the SAME process-global pool at the same time.
    // ---------------------------------------------------------------------
    std::vector<CellReading> concurrent(cells.size());
    {
        std::vector<std::thread> workers;
        workers.reserve(cells.size());
        for (int i = 0; i < cell_count; ++i) {
            workers.emplace_back([i, &cells, &concurrent]() {
                concurrent[static_cast<std::size_t>(i)] =
                    run_fanout_cell(cells[static_cast<std::size_t>(i)]);
            });
        }
        for (std::thread &w : workers) {
            w.join();
        }
    }

    EXPECT_EQ(hven::utils::get_num_threads(), pool_width_before)
        << "no solve, serial or concurrent, may move the process-global evaluation-pool width";

    // ---------------------------------------------------------------------
    // (3) STEP ONE -- RECORD the byte-equality finding, per column class.
    // (4) STEP TWO -- ASSERT. Counters and statuses exact; measures gated.
    // ---------------------------------------------------------------------
    Finding finding;
    ASSERT_NO_FATAL_FAILURE(record_finding(serial, concurrent, finding));
    report_finding(finding, cell_count,
                   "two partitioned programs fanning into one shared pool from two threads");
    assert_readings_match(serial, concurrent);

    // The two cells must be two DIFFERENT solves, for the first test's reason:
    // otherwise the pin is one solve run twice and its breadth is imaginary.
    // They differ in size and in data, which their objectives say.
    EXPECT_NE(serial[0].cell, serial[1].cell) << "the two cells must be two cells";
    double f_a = 0.0;
    double f_b = 0.0;
    for (const MeasureColumn &m : serial[0].measures) {
        if (m.name == "f") {
            f_a = m.value;
        }
    }
    for (const MeasureColumn &m : serial[1].measures) {
        if (m.name == "f") {
            f_b = m.value;
        }
    }
    EXPECT_GT(std::abs(f_a - f_b), 1.0)
        << "the two fan-out cells must be two different problems, not one twice";
}
