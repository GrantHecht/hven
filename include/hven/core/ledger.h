#pragma once

#include <string>
#include <vector>

#include <fmt/core.h>

#include <hven/core/solver_counters.h>
#include <hven/core/solver_status.h>
#include <hven/core/start_level.h>

namespace hven::solvers {

// A single solve event: label, warm start flag, status, and performance counters.
struct SolveRecord {
    std::string label;
    bool warm;
    QpStatus status;
    QpCounters counters;
};

// TASK 12. One record per WHOLE SQP DRIVER SOLVE -- the aggregate counters a
// caller wants when comparing driver-level runs, exactly as SolveRecord above
// is one record per QP-engine solve. Emitted by SqpDriver::attach_ledger (see
// sqp_driver.h), which also forwards the same Ledger to the driver's own
// internal QpEngine, so a Ledger attached to a driver ends up holding BOTH
// kinds of record: one SqpSolveRecord per solve() call, and the QP-level
// SolveRecord entries (one per subproblem/SOC/elastic re-solve) the engine
// emits as it always has. The two never collide -- they live in separate
// vectors below -- so attaching to a driver cannot silently break the
// QP-level mechanism that predates this struct.
// PHASE-4 TASK 7. `counters` above already carries every one of these
// values -- SqpDriver::record_solve (sqp_driver.h) copies `out.counters`
// there wholesale, so `.counters.start_level_used` and friends were always
// reachable (test_sqp_driver.cpp's SuspectEscalationsAggregateToTheDriver
// note said as much before this task existed). The seven fields below are
// DELIBERATE, NAMED DUPLICATES of a subset of `counters`' fields, added so
// the Phase-6 cold-vs-warm benchmark this ledger exists for can read a flat
// `rec.start_level_used` rather than navigating the nested struct for the
// handful of counters that comparison actually needs. They can never drift
// from `counters`: SqpDriver::record_solve populates both from the same
// `out.counters` in the same statement list, so there is exactly one source
// of truth (`counters`) and these are a read-only-shaped copy of part of it,
// never an independent measurement.
//
// EACH NAME MATCHES ITS SOURCE FIELD IN SqpCounters (solver_counters.h) EXACTLY --
// deliberately, closing a naming question the Phase-4 warm-start plan's own
// Task-1 carry raised for `soc_steps` (the plan's draft called it
// `soc_attempts`; Phase 4 already ruled there is no separate "attempts"
// concept, `soc_steps` IS the attempts count -- see that field's own note).
// Reusing the source name here rather than coining a second one for the same
// quantity avoids exactly the two-names-one-concept trap that carry warns
// against, so `soc_steps` below is `counters.soc_steps`, not a rename of it.
//
//   - start_level_used: SqpCounters::start_level_used -- the RESOLVED level
//     (kCold/kWarm/kHot) this solve actually used, which is the whole point
//     of a "cold-vs-warm" ledger: it is the column a benchmark groups rows
//     by.
//   - full_step_majors, watchdog_restores: SqpCounters' own fields, Task 5's
//     full-step-first rule's own diagnostics.
//   - soc_steps, soc_applied: SqpCounters' own fields (Task 1/7 of this
//     plan's own numbering -- see SqpCounters::soc_steps' note for the
//     three-way outcome breakdown soc_applied is one third of).
//   - border_refine_steps, eqp_refine_steps: SqpCounters' own fields (Task
//     0's EQP refinement A/B).
//
// factorizations_saved IS NOT ONE OF THESE COPIES -- it names no
// SqpCounters field because none exists for it, and it is NOT the same
// quantity as `counters.factorizations` (which counts factorizations PAID,
// the opposite sense). ITS PRECISE DEFINITION: 1 iff this solve's FIRST
// subproblem actually skipped its own factorization by reusing an offered
// hot-start K0, else 0 -- i.e. exactly
//     (start_level_used == StartLevel::kHot) ? 1 : 0.
// This is sound, not a guess, because solver_counters.h's own note on
// `start_level_used` already states its kHot reading is set ONLY when
// QpCounters::k0_reused read true on that first subproblem (the engine's own
// observed-reuse signal, not an inference from `qp_factorizations == 0` --
// see that field's note in solver_counters.h for why the two are not interchangeable).
// So this field re-derives nothing new; it is start_level_used's own
// evidence, restated as a count for a benchmark that wants "how many
// factorizations did warm-starting save" rather than "which level resolved".
//
// IT IS ALWAYS 0 ON A COLD OR WARM (non-kHot) SOLVE, and that 0 is a
// CORRECT READING, not a missing measurement: a cold solve pays its
// factorization by construction, and a kWarm resolution -- whether because
// no hot handle was offered or because one was offered and the engine's own
// reuse gate (qp_engine.h's conditions (a)-(e)) refused it -- always paid a
// real refactorization on its first subproblem, so there is nothing to
// count as saved. It is bounded by 1 today because a hot-start reuse is
// only ever OFFERED for a solve's FIRST subproblem (warm_start.h's `hot`
// handle is consumed once, at ingest); every later major in the same solve
// builds its subproblem fresh from a new linearization and was never a
// reuse candidate to begin with.
struct SqpSolveRecord {
    std::string label;
    SqpStatus status;
    SqpCounters counters;
    StartLevel start_level_used = StartLevel::kCold;
    Index full_step_majors = 0;
    Index watchdog_restores = 0;
    Index soc_steps = 0;
    Index soc_applied = 0;
    Index border_refine_steps = 0;
    Index eqp_refine_steps = 0;
    Index factorizations_saved = 0;

    // PHASE-7 TASK 5. THE SSN COLUMNS -- flat, named duplicates of
    // `counters.ssn`'s own fields, added on exactly the argument the seven
    // duplicates above are added on: a benchmark comparing `qp_mode = kWalk`
    // against `qp_mode = kSsn` groups its rows by these three, and should read
    // `rec.ssn_iters` rather than navigate two levels of nested struct for it.
    // They can never drift: SqpDriver::record_solve populates them from the
    // same `out.counters` in the same statement list, so `counters` remains
    // the single source of truth and these are a read-only-shaped copy of part
    // of it.
    //
    // EACH NAME MATCHES ITS SOURCE FIELD IN SsnCounters (solver_counters.h)
    // EXACTLY, the same rule the block above states for SqpCounters.
    //
    // THREE OF SIX, NOT ALL SIX, AND THAT IS A CHOICE. `ssn_iters` is the
    // work, `ssn_bulk_flips` is the identification story the kernel exists to
    // buy, and `ssn_escapes` is the routing story this task ships -- those are
    // the three a mode comparison groups by. `ssn_backtracks`,
    // `ssn_prox_updates` and `ssn_uncertain_peak` are safeguard diagnostics
    // rather than comparison columns and stay reachable through
    // `counters.ssn`, which carries all six.
    //
    // ALL THREE ARE IDENTICALLY 0 ON EVERY SOLVE RUN AT THE SHIPPED DEFAULT
    // (`SqpOptions::qp_mode == QpMode::kWalk`), because no SSN subproblem is
    // solved there -- so adding them cannot move a single existing ledger row.
    Index ssn_iters = 0;
    Index ssn_bulk_flips = 0;
    Index ssn_escapes = 0;

    // PHASE-5 TASK 2. Wall-clock time this ONE public solve() call spent
    // inside solve_impl (sqp_driver.h times std::chrono::steady_clock around
    // that call alone, never around model construction or CSV/ledger
    // bookkeeping) -- the measurement the Phase-5 scale/benchmark work reads.
    //
    // INFORMATIONAL ONLY, EXACTLY LIKE PEAK RSS ELSEWHERE IN THIS PROJECT
    // (tests/sqp/support/scale_problems.h's own note on its peak_rss_mib(), the
    // same standing rule): THIS FIELD IS NEVER ASSERTED ON A SPECIFIC VALUE
    // BY ANY TEST, and no counter or regression contract may depend on it.
    // It is machine-, load-, backend- and build-type-dependent (Debug's
    // Eigen bounds checks alone move a solve's wall time by an order of
    // magnitude against Release -- see CLAUDE.md's BUILD AND TEST section),
    // so a single run's value is LEADING-DIGITS-ONLY evidence: read it as
    // "roughly this many seconds", never as a bit-for-bit reproducible
    // number the way `counters` above is. test_ledger.cpp's own test on this
    // field asserts only that it is populated (> 0 on a solve that did any
    // work), never a magnitude.
    double wall_seconds = 0.0;
};

// Instrumentation ledger for cold-vs-warm solve tracking (QP-level, Phase 1)
// and, from Task 12, whole-driver-solve tracking (SQP-level). One Ledger
// instance can hold both kinds of record at once; see SqpSolveRecord above.
class Ledger {
  public:
    Ledger() = default;

    // Record a single QP-engine solve event.
    void record(SolveRecord r) { records_.push_back(std::move(r)); }

    // Record a single whole-driver solve event.
    void record(SqpSolveRecord r) { sqp_records_.push_back(std::move(r)); }

    // Access all recorded QP-engine solve events.
    const std::vector<SolveRecord> &records() const { return records_; }

    // Access all recorded whole-driver solve events.
    const std::vector<SqpSolveRecord> &sqp_records() const { return sqp_records_; }

    // PHASE-6 TASK 5. The start-level histogram over every SqpSolveRecord this
    // ledger holds (QP-level SolveRecords have no level and are not counted).
    // Reads `start_level_used`, which is the RESOLVED level -- what was
    // observed to happen, never what a caller offered (solver_counters.h).
    StartLevelHistogram level_histogram() const {
        StartLevelHistogram h;
        for (const SqpSolveRecord &rec : sqp_records_) {
            switch (rec.start_level_used) {
            case StartLevel::kCold:
                ++h.cold;
                break;
            case StartLevel::kSeeded:
                ++h.seeded;
                break;
            case StartLevel::kWarm:
                ++h.warm;
                break;
            case StartLevel::kHot:
                ++h.hot;
                break;
            }
        }
        return h;
    }

    // Formatted table: label, warm?, iters, factorizations, schur updates.
    std::string summary_table() const {
        if (records_.empty()) {
            return "";
        }

        // Build the table with header
        std::string result;
        const std::string header = fmt::format("{:<30} {:<10} {:<8} {:<16} {:<14}", "Label",
                                               "Warm?", "Iters", "Factorizations", "Schur Updates");
        result += header + "\n";
        result += std::string(header.size(), '-');
        result += "\n";

        for (const auto &rec : records_) {
            result += fmt::format("{:<30} {:<10} {:<8} {:<16} {:<14}\n", rec.label,
                                  rec.warm ? "yes" : "no", rec.counters.minor_iters,
                                  rec.counters.factorizations, rec.counters.schur_updates);
        }

        return result;
    }

    // Formatted table: label, status, major/QP-minor/factorization counters --
    // the SQP-level analogue of summary_table() above. PHASE-4 TASK 7 adds
    // the Level column (SqpSolveRecord::start_level_used) -- the "row
    // extension" the cold-vs-warm ledger instrumentation exists to add,
    // since it is exactly the column a cold-vs-warm comparison groups by.
    std::string sqp_summary_table() const {
        if (sqp_records_.empty()) {
            return "";
        }

        std::string result;
        const std::string header =
            fmt::format("{:<30} {:<16} {:<6} {:<8} {:<12} {:<14}", "Label", "Status", "Level",
                        "Majors", "QP Minors", "Factorizations");
        result += header + "\n";
        result += std::string(header.size(), '-');
        result += "\n";

        for (const auto &rec : sqp_records_) {
            result += fmt::format("{:<30} {:<16} {:<6} {:<8} {:<12} {:<14}\n", rec.label,
                                  to_string(rec.status), to_string(rec.start_level_used),
                                  rec.counters.major_iters, rec.counters.qp_minor_iters,
                                  rec.counters.factorizations);
        }

        return result;
    }

  private:
    // The two -> string helpers this class's tables use are
    // core/solver_status.h's to_string(SqpStatus) and core/start_level.h's
    // to_string(StartLevel). Both arrive through the includes at the top of
    // this file, and every one of those is a core/ header.
    //
    // THIS NOTE USED TO SAY THE OPPOSITE, and correcting it is not cosmetic.
    // Through M3 phase-C S2 it read "this header already depends on
    // sqp_types.h for the enum itself, so sharing it here costs nothing
    // extra" -- true when it was written, FALSE NOW, and precisely the
    // sentence a maintainer could act on to reopen the layering inversion S2
    // closed. S2 moved SqpStatus/QpStatus, SqpCounters/SsnCounters/QpCounters,
    // StartLevel and StartLevelHistogram out of drivers/sqp_types.h,
    // qp/qp_types.h and detail/warmstart/warm_start.h into core/ exactly so
    // that this header -- which records a whole-driver solve -- stops reaching
    // UP into drivers/ for the types it records. Its include closure is now
    // core/ headers plus <string>, <vector>, fmt and Eigen, and nothing else.
    //
    // SO: DO NOT ADD AN INCLUDE OF drivers/, qp/, model/, kkt/, interior/,
    // linear/, globalization/, warmstart/ OR detail/ TO THIS OR ANY OTHER
    // core/ HEADER. core/ is the bottom tier of CLAUDE.md section 2's map and
    // depends on nothing above it. That is ENFORCED rather than merely asked
    // for: tests/core/test_core_layering.cpp scans every header in
    // include/hven/core/ and fails the suite on any such include, naming the
    // file and the line.

    std::vector<SolveRecord> records_;
    std::vector<SqpSolveRecord> sqp_records_;
};

} // namespace hven::solvers
