// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <string>
#include <vector>

// This header does not use fmt any more, but the include below is kept
// deliberately: dropping it changes the preprocessed input of every TU that
// includes this header, which is nearly the whole SQP tree, and that is
// exactly the kind of include-order perturbation the PCH measurement table in
// src/CMakeLists.txt records as moving emission order. Removing it is a fine
// separate change with its own P-SYM run.
#include <fmt/core.h>

#include <hven/core/solver_counters.h>
#include <hven/core/solver_status.h>
#include <hven/core/start_level.h>

namespace hven::solvers {

/// A single QP-engine solve event: label, warm-start flag, status, and
/// performance counters.
struct SolveRecord {
    /// @brief Caller-chosen label for the solve.
    std::string label;
    /// @brief Whether the solve was warm-started.
    bool warm;
    /// @brief The QP subproblem solver's outcome verdict.
    QpStatus status;
    /// @brief Performance counters for the solve.
    QpCounters counters;
};

/// One record per WHOLE SQP DRIVER SOLVE -- the aggregate counters a caller
/// wants when comparing driver-level runs, exactly as SolveRecord above is
/// one record per QP-engine solve. Emitted by SqpDriver::attach_ledger (see
/// sqp_driver.h), which also forwards the same Ledger to the driver's own
/// internal QpEngine, so a Ledger attached to a driver ends up holding BOTH
/// kinds of record: one SqpSolveRecord per solve() call, and the QP-level
/// SolveRecord entries (one per subproblem/SOC/elastic re-solve) the engine
/// emits as it always has. The two never collide -- they live in separate
/// vectors -- so attaching to a driver cannot silently break the QP-level
/// mechanism.
///
/// The flat fields below are DELIBERATE, NAMED DUPLICATES of a subset of
/// `counters`' fields, added so benchmarks can read a flat `rec.ssn_iters`
/// rather than navigate the nested structs for the handful of counters a
/// comparison actually needs. Each name matches its source field in
/// SqpCounters / SsnCounters (solver_counters.h) exactly --
/// `soc_steps` here IS `counters.soc_steps`, not a rename. They can never
/// drift from `counters`: SqpDriver::record_solve populates both from the
/// same `out.counters` in the same statement list, so `counters` remains the
/// single source of truth and these are a read-only-shaped copy of part of
/// it, never an independent measurement.
struct SqpSolveRecord {
    /// @brief Caller-chosen label for the solve.
    std::string label;
    /// @brief The SQP driver's outcome verdict.
    SolveStatus status;
    /// Aggregate counters for the whole solve (single source of truth for
    /// every value below).
    SqpCounters counters;
    /// Duplicate of SqpCounters::start_level_used: the RESOLVED level
    /// (kCold/kSeeded/kWarm/kHot) this solve actually used -- what was
    /// observed to happen, never what a caller offered. The column a
    /// cold-vs-warm benchmark groups rows by.
    StartLevel start_level_used = StartLevel::kCold;
    /// @brief Duplicate of SqpCounters::full_step_majors.
    Index full_step_majors = 0;
    /// @brief Duplicate of SqpCounters::watchdog_restores.
    Index watchdog_restores = 0;
    /// @brief Duplicate of SqpCounters::soc_steps (which IS the attempts count).
    Index soc_steps = 0;
    /// Duplicate of SqpCounters::soc_applied (see soc_steps' note in
    /// solver_counters.h for the three-way outcome breakdown this is one
    /// third of).
    Index soc_applied = 0;
    /// @brief Duplicate of SqpCounters::border_refine_steps.
    Index border_refine_steps = 0;
    /// @brief Duplicate of SqpCounters::eqp_refine_steps.
    Index eqp_refine_steps = 0;

    /// 1 iff this solve's FIRST subproblem actually skipped its own
    /// factorization by reusing an offered hot-start K0, else 0 -- exactly
    /// `(start_level_used == StartLevel::kHot) ? 1 : 0`. That re-derives
    /// nothing new: solver_counters.h states `start_level_used`'s kHot
    /// reading is set ONLY when QpCounters::k0_reused read true on that
    /// first subproblem (the engine's observed-reuse signal, NOT an
    /// inference from `qp_factorizations == 0` -- see that field's note for
    /// why the two are not interchangeable). Unlike the fields above, this
    /// names no SqpCounters field (none exists) and is NOT the same quantity
    /// as `counters.factorizations`, which counts factorizations PAID, the
    /// opposite sense.
    ///
    /// ALWAYS 0 ON A COLD OR WARM (non-kHot) SOLVE, and that 0 is a CORRECT
    /// READING, not a missing measurement: a cold solve pays its
    /// factorization by construction, and a kWarm resolution -- whether no
    /// hot handle was offered or one was offered and the engine's own reuse
    /// gate (qp_engine.h's conditions (a)-(e)) refused it -- always paid a
    /// real refactorization on its first subproblem. Bounded by 1 today
    /// because a hot-start reuse is only ever OFFERED for a solve's FIRST
    /// subproblem (warm_start.h's `hot` handle is consumed once, at ingest);
    /// every later major builds its subproblem fresh from a new
    /// linearization and was never a reuse candidate.
    Index factorizations_saved = 0;

    /// THE SSN COLUMNS -- flat, named duplicates of three of
    /// `counters.ssn`'s six fields, on the same argument as the duplicates
    /// above: a benchmark comparing `qp_mode = kWalk` against
    /// `qp_mode = kSsn` groups rows by these three (`ssn_iters` is the work,
    /// `ssn_bulk_flips` the identification story, `ssn_escapes` the routing
    /// story). `ssn_backtracks`, `ssn_prox_updates` and `ssn_uncertain_peak`
    /// are safeguard diagnostics rather than comparison columns and stay
    /// reachable through `counters.ssn`, which carries all six.
    ///
    /// ALL THREE ARE IDENTICALLY 0 ON EVERY SOLVE RUN AT THE SHIPPED DEFAULT
    /// (`SqpOptions::qp_mode == QpMode::kWalk`), because no SSN subproblem
    /// is solved there -- adding them moves no existing ledger row.
    Index ssn_iters = 0;
    /// @brief See ssn_iters' note.
    Index ssn_bulk_flips = 0;
    /// @brief See ssn_iters' note.
    Index ssn_escapes = 0;

    /// Wall-clock time this ONE public solve() call spent inside solve_impl
    /// (sqp_driver.h times std::chrono::steady_clock around that call alone,
    /// never around model construction or CSV/ledger bookkeeping).
    ///
    /// INFORMATIONAL ONLY, EXACTLY LIKE PEAK RSS ELSEWHERE IN THIS PROJECT:
    /// NEVER ASSERTED ON A SPECIFIC VALUE BY ANY TEST, and no counter or
    /// regression contract may depend on it. It is machine-, load-, backend-
    /// and build-type-dependent (Debug's Eigen bounds checks alone move a
    /// solve's wall time by an order of magnitude against Release), so a
    /// single run's value is LEADING-DIGITS-ONLY evidence: read it as
    /// "roughly this many seconds", never as a bit-for-bit reproducible
    /// number the way `counters` is. Its test asserts only that it is
    /// populated (> 0 on a solve that did any work), never a magnitude.
    double wall_seconds = 0.0;
};

/// One record per WHOLE INTERIOR-POINT SOLVE -- the interior-point engine's
/// counterpart to SqpSolveRecord above, written by
/// InteriorPointSolver::attach_ledger (see drivers/interior_point_solver.h),
/// one per public solve() call that RETURNS. A call that leaves by an
/// exception writes nothing, which is the honest record of one.
///
/// UNLIKE SqpSolveRecord THIS CARRIES NO `counters` OBJECT, because the
/// interior-point engine has none: its measurements live on IpmResult, a
/// value the caller already holds. The nine fields below are the ones a
/// cross-run comparison groups or sums by, and each names exactly what it
/// was read from -- see the per-field notes.
///
/// THE ENGINE'S NESTED FEASIBILITY RESTORATION WRITES NO RECORD OF ITS OWN.
/// It runs inside this solve rather than as a separate solver object, and
/// its iterations are already inside `iterations` -- exactly as the SQP's
/// restoration sub-driver receives attach_trace but no attach_ledger.
struct IpmSolveRecord {
    /// @brief Caller-chosen label prefix plus this solver's own solve counter,
    ///        formatted "{prefix}_{n}" -- the same shape SqpSolveRecord uses.
    std::string label;
    /// @brief The solve's outcome verdict (IpmResult::status).
    SolveStatus status;
    /// @brief IpmResult::iterations -- summed over the phases that ran.
    Index iterations = 0;
    /// The number of entries of IpmResult::phases whose `ran` is true. A
    /// conditional phase the sequence skipped is counted by `phases` on the
    /// trace's begin line and NOT here: this is what happened.
    Index phases_run = 0;
    /// @brief IpmResult::total_time, in SECONDS. Informational, like
    ///        `wall_seconds` below; never asserted on a value.
    double total_time = 0.0;
    /// KKT factorizations paid by THIS CALL -- the difference between
    /// `KktFactorization::lifetime_factorize_count()` after the call and before
    /// it, NOT that total itself. On a solver reused for a second solve the
    /// total would charge this record for the previous call's work.
    ///
    /// The accumulator is differenced rather than the linear engine's own
    /// `Counters::factorize_count` (M6 W5 T8.7 fix1) because that counter is
    /// per ENGINE INSTANCE and starts again at zero when the analysis is
    /// re-laid -- so on a solver handed a second, DIFFERENT program the
    /// difference of two such readings is negative. This field is never
    /// negative.
    Index factorizations = 0;
    /// @brief IpmResult::kkt_analyses_this_call -- symbolic analyses paid by
    ///        THIS call (the engine's own per-call counter).
    Index analyses = 0;
    /// @brief IpmResult::soc_steps_taken. 0 when SOC is off.
    Index soc_steps_taken = 0;
    /// @brief IpmResult::watchdog_activations. 0 when the watchdog is off.
    Index watchdog_activations = 0;
    /// Wall-clock seconds this ONE public solve() call took, measured by the
    /// entry's own clock (IpmResult::wall_seconds).
    ///
    /// INFORMATIONAL ONLY, on exactly SqpSolveRecord::wall_seconds' terms:
    /// never asserted on a value by any test, and no counter or regression
    /// contract may depend on it.
    double wall_seconds = 0.0;
};

/// Instrumentation ledger for cold-vs-warm solve tracking (QP-level, via
/// SolveRecord), whole-SQP-solve tracking (SqpSolveRecord) and
/// whole-interior-point-solve tracking (IpmSolveRecord). One Ledger instance
/// can hold all three kinds of record at once; they live in separate vectors
/// and never collide.
class Ledger {
  public:
    Ledger() = default;

    /// @brief Records a single QP-engine solve event.
    void record(SolveRecord r) { records_.push_back(std::move(r)); }

    /// @brief Records a single whole-driver solve event.
    void record(SqpSolveRecord r) { sqp_records_.push_back(std::move(r)); }

    /// @brief Records a single whole-interior-point-solve event.
    void record(IpmSolveRecord r) { ipm_records_.push_back(std::move(r)); }

    /// @brief All recorded QP-engine solve events.
    const std::vector<SolveRecord> &records() const { return records_; }

    /// @brief All recorded whole-driver solve events.
    const std::vector<SqpSolveRecord> &sqp_records() const { return sqp_records_; }

    /// @brief All recorded whole-interior-point-solve events.
    const std::vector<IpmSolveRecord> &ipm_records() const { return ipm_records_; }

    // The three reporting functions below are defined in src/core/ledger.cpp:
    // they run once per report -- a tally and two fmt-formatted tables -- so
    // nothing about them wants inlining. record() and the accessors above
    // deliberately STAY inline: one push_back per solve, and out-lining them
    // would put a call on the recording path in exchange for no measurable
    // build time.

    /// The start-level histogram over every SqpSolveRecord this ledger holds
    /// (QP-level SolveRecords have no level and are not counted). Reads
    /// `start_level_used`, which is the RESOLVED level -- what was observed
    /// to happen, never what a caller offered (solver_counters.h).
    StartLevelHistogram level_histogram() const;

    /// @brief Formatted table: label, warm?, iters, factorizations, schur updates.
    std::string summary_table() const;

    /// Formatted table: label, status, Level, major/QP-minor/factorization
    /// counters -- the SQP-level analogue of summary_table(); the Level
    /// column (start_level_used) is the column a cold-vs-warm comparison
    /// groups by.
    std::string sqp_summary_table() const;

    /// Formatted table: label, status, iterations, phases run, factorizations,
    /// analyses -- the interior-point analogue of sqp_summary_table(). NO
    /// TIMING COLUMN, deliberately: both time fields on IpmSolveRecord are
    /// informational (CLAUDE.md section 7) and a table is where an
    /// informational number is most likely to be quoted as a measurement.
    std::string ipm_summary_table() const;

  private:
    // The -> string helpers this class's tables use are
    // core/solver_status.h's to_string(SolveStatus) and core/start_level.h's
    // to_string(StartLevel); both arrive through the includes at the top of
    // this file, and every one of those is a core/ header.
    //
    // THIS HEADER'S INCLUDE CLOSURE IS core/ HEADERS PLUS <string>, <vector>,
    // fmt AND EIGEN, AND NOTHING ELSE. DO NOT ADD AN INCLUDE OF drivers/,
    // qp/, model/, kkt/, interior/, linear/, globalization/, warmstart/ OR
    // detail/ TO THIS OR ANY OTHER core/ HEADER: core/ is the bottom tier of
    // the project's layer map and depends on nothing above it. ENFORCED, not
    // merely asked for: tests/core/test_core_layering.cpp scans every header
    // in include/hven/core/ and fails the suite on any such include, naming
    // the file and the line.

    std::vector<SolveRecord> records_;
    std::vector<SqpSolveRecord> sqp_records_;
    std::vector<IpmSolveRecord> ipm_records_;
};

} // namespace hven::solvers
