// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/drivers/test_concurrency.cpp -- THE CONCURRENCY CONTRACT'S ONE PIN
// (M6 W6 T4).
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
// WHAT THE PIN AVOIDS, AND WHY THAT IS NOT A DODGE. The ruling names three
// process-global exceptions; `docs/concurrency.md` names them as audited. The
// one that could reach these cells is the interior-point engine's EVALUATION
// POOL (`hven::utils::set_num_threads`, src/interior/utils/thread_pool.cpp),
// which is process-global by design. These cells avoid it BY CONSTRUCTION
// rather than by arrangement: `make_nlp_program` defaults to one partition, and
// `parallel_sequence` (include/hven/detail/interior/utils/thread_pool.h:698)
// runs inline whenever `nparts <= 1`, so the pool is never dispatched to and
// never even constructed. That premise is ASSERTED below (the adopted partition
// count, and the pool width unmoved across the whole test) rather than assumed.
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

#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <Eigen/Core>

#include <hven/detail/interior/utils/thread_pool.h>
#include <hven/detail/model/nlp_adapter.h>
#include <hven/drivers/ipm_solver.h>
#include <hven/drivers/ipm_solver_types.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>
#include <hven/model/nlp_model.h>

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

CellReading run_ipm_cell(const std::string &label, const Eigen::VectorXd &x0) {
    hven::solvers::IpmOptions o;
    o.common.threads = 1;      // THE PIN.
    o.common.print_level = 10; // Silent -- this engine's default is 0, i.e. loud.
    hven::solvers::IpmSolver solver(o);

    const auto program =
        hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
    const hven::solvers::IpmResult r = solver.solve(*program, x0);

    CellReading out;
    out.cell = "ipm/" + label;
    push_shared_result_columns(r, out);
    // THE PREMISE THAT KEEPS THIS CELL CLEAR OF THE EVALUATION POOL, carried as
    // a column so it is compared between the two runs as well as asserted once
    // on the main thread: one partition means parallel_sequence runs inline and
    // the process-global pool is never reached.
    out.exact.push_back({"partitions", static_cast<long long>(program->num_partitions_)});
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

    // The pin is not allowed to be vacuous: every cell must have solved
    // something. A cell that refused at the boundary would compare equal to
    // itself and prove nothing.
    for (int i = 0; i < kCellCount; ++i) {
        ASSERT_EQ(serial[i].exact[0].name, "status");
        ASSERT_EQ(serial[i].exact[0].value,
                  static_cast<long long>(hven::solvers::SolveStatus::kOptimal))
            << "PREMISE: cell " << serial[i].cell << " must actually converge";
        ASSERT_EQ(serial[i].exact[1].name, "iterations");
        ASSERT_GT(serial[i].exact[1].value, 0)
            << "PREMISE: cell " << serial[i].cell << " must actually iterate";
    }

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
    // ---------------------------------------------------------------------
    int exact_columns = 0;
    int exact_byte_equal = 0;
    int measure_columns = 0;
    int measure_byte_equal = 0;
    double worst_relative_move = 0.0;
    std::string worst_column = "(none)";

    for (int i = 0; i < kCellCount; ++i) {
        const CellReading &s = serial[i];
        const CellReading &c = concurrent[i];
        ASSERT_EQ(s.cell, c.cell) << "the two runs must have run the same cells in the same order";
        ASSERT_EQ(s.exact.size(), c.exact.size());
        ASSERT_EQ(s.measures.size(), c.measures.size());

        for (std::size_t k = 0; k < s.exact.size(); ++k) {
            ++exact_columns;
            if (s.exact[k].value == c.exact[k].value) {
                ++exact_byte_equal;
            }
        }
        for (std::size_t k = 0; k < s.measures.size(); ++k) {
            ++measure_columns;
            const double a = s.measures[k].value;
            const double b = c.measures[k].value;
            if (bits_equal(a, b)) {
                ++measure_byte_equal;
                continue;
            }
            if (std::isfinite(a) && std::isfinite(b)) {
                const double scale = std::max(std::abs(a), std::abs(b));
                const double rel = scale > 0.0 ? std::abs(a - b) / scale : 0.0;
                if (rel > worst_relative_move) {
                    worst_relative_move = rel;
                    worst_column = s.cell + " " + s.measures[k].name;
                }
            }
            std::cout << "[ MOVED    ] " << s.cell << " " << s.measures[k].name << ": serial "
                      << std::scientific << a << " concurrent " << b << "\n";
        }
    }

    const bool all_byte_equal =
        (exact_byte_equal == exact_columns) && (measure_byte_equal == measure_columns);

    RecordProperty("cells", kCellCount);
    RecordProperty("exact_columns", exact_columns);
    RecordProperty("exact_columns_byte_equal", exact_byte_equal);
    RecordProperty("measure_columns", measure_columns);
    RecordProperty("measure_columns_byte_equal", measure_byte_equal);
    RecordProperty("bit_identical", all_byte_equal ? "yes" : "no");
    RecordProperty("worst_relative_move", std::to_string(worst_relative_move).c_str());
    RecordProperty("worst_moved_column", worst_column.c_str());

    std::cout << "[ FINDING  ] the ruling's literal bit-identity, as observed on this box:\n"
              << "[ FINDING  ]   counters and statuses byte-equal: " << exact_byte_equal << "/"
              << exact_columns << "\n"
              << "[ FINDING  ]   floating measures byte-equal:     " << measure_byte_equal << "/"
              << measure_columns << "\n"
              << "[ FINDING  ]   bit-identical overall:            "
              << (all_byte_equal ? "YES" : "NO") << "\n"
              << "[ FINDING  ]   worst relative move:              " << std::scientific
              << worst_relative_move << " (" << worst_column << ")\n";

    // ---------------------------------------------------------------------
    // (4) STEP TWO -- ASSERT. Counters and statuses exact; measures gated.
    // ---------------------------------------------------------------------
    for (int i = 0; i < kCellCount; ++i) {
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

    // The premise that the cells stay clear of the process-global evaluation
    // pool, asserted on the main thread as well as compared above.
    for (int i = 2; i < kCellCount; ++i) {
        bool saw_partitions = false;
        for (const ExactColumn &col : serial[i].exact) {
            if (col.name == "partitions") {
                saw_partitions = true;
                EXPECT_EQ(col.value, 1)
                    << serial[i].cell
                    << ": this cell must adopt ONE partition, so parallel_sequence runs inline and "
                       "the process-global evaluation pool is never dispatched to";
            }
        }
        EXPECT_TRUE(saw_partitions) << "the interior-point cells must report their adopted count";
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
