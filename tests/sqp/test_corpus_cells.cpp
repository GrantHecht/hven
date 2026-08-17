// tests/test_corpus_cells.cpp — PHASE-7 TASK 1: the replay corpus's own
// correctness gate. Shares bench/corpus_cells.h's implementation with
// bench/bench_corpus.cpp rather than duplicating it (the SNOPT-gate
// precedent tests/CMakeLists.txt already documents).
//
// EVERYTHING SOLVED HERE IS TINY (N = 12 nodes), deliberately: this file is
// registered with ctest (both Release and Debug, every commit), unlike the
// corpus's own baseline sweep (docs/notes/data/2026-08-06-corpus/), whose
// N = 20000 cells run for minutes to hours. The production census
// (all_cells(), 57 cells) is exercised only by STATIC inspection here -- no
// cell from it is ever SOLVED in this file. Two exceptions, both cheap and
// both subprocess tests: the wall-deadline arms drive the real binary on a
// bound-arc cell (sub-second) with a forced budget.
//
// FIX ROUND 1 added the coverage the review found missing on the SCORED
// surface (I5): the gate POPULATIONS, the G3 pairing, the DNF charge and the
// offline --from-csv scoring path all now have direct tests, because all of
// them decide a Task-6 verdict.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include "../../bench/corpus_cells.h"

#ifndef HVEN_SQP_CORPUS_BINARY
#error "HVEN_SQP_CORPUS_BINARY must be defined by bench/CMakeLists.txt (see its own comment)"
#endif
#ifndef HVEN_SQP_CORPUS_BASELINE_CSV
#error "HVEN_SQP_CORPUS_BASELINE_CSV must be defined by tests/CMakeLists.txt"
#endif
#ifndef HVEN_SQP_SSN_BATTERY_CSV
#error "HVEN_SQP_SSN_BATTERY_CSV must be defined by tests/CMakeLists.txt"
#endif
#ifndef HVEN_SQP_WALK_RESWEPT_CSV
#error "HVEN_SQP_WALK_RESWEPT_CSV must be defined by tests/CMakeLists.txt"
#endif

namespace {

using namespace hven::solvers::corpus;

// A tiny, hand-built cell -- NOT from the production census -- for the
// producer/determinism tests below. N = 12, p0 = 0.80/p = 0.85: both inside
// the wide-window (path-interface) regime (p_activation = R/2 = 0.5), so the
// crossover/warm producers have a genuine, nonempty active window to work
// with even at this size.
CorpusCell tiny_cell(StartTaxonomy start, bool use_p0 = true) {
    return CorpusCell{"tiny",
                      BenchFamily::kF7,
                      /*n_nodes=*/12,
                      use_p0 ? 0.80 : 0.85,
                      0.85,
                      /*step_index=*/0,
                      start,
                      ConstraintFamily::kPathInterface,
                      /*degenerate=*/false};
}

// =============================================================================
// STATIC CENSUS INVARIANTS -- no solving anywhere in this section.
// =============================================================================

TEST(CorpusCells, AllIdsAreUnique) {
    std::vector<std::string> ids;
    for (const CorpusCell &c : all_cells()) {
        ids.emplace_back(c.id);
    }
    std::vector<std::string> sorted = ids;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    EXPECT_EQ(sorted.size(), ids.size()) << "every cell id must be unique";
}

TEST(CorpusCells, CensusSizeIsTheDocumentedFiftySeven) {
    // 5 N x 2 windows x 5 taxonomies (50) + N=800 x 5 taxonomies (5) + 2
    // healthy controls (N=750/825, neutral only) = 57. See corpus_cells.h's
    // own census note for the derivation; this pins the count so a silent
    // addition/removal is caught.
    EXPECT_EQ(all_cells().size(), 57u);
}

TEST(CorpusCells, BuildAllCellsIsIdempotentAndDoesNotGrowTheIdPool) {
    // FIX ROUND 1 (M5). The id pool used to be an append log, so every extra
    // build_all_cells() call grew it without bound and handed out a fresh
    // pointer for an id that already existed. It is an INTERNER now: same
    // count, and the same id resolves to the same storage.
    const std::vector<CorpusCell> a = build_all_cells();
    const std::vector<CorpusCell> b = build_all_cells();
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_STREQ(a[i].id, b[i].id);
        EXPECT_EQ(a[i].id, b[i].id) << "the same id must intern to the same storage: " << a[i].id;
    }
    EXPECT_EQ(detail::id_pool().size(), 55u)
        << "50 main-grid ids + 5 N=800 ids; the two controls carry literal ids";
}

TEST(CorpusCells, MainGridHasEveryNWindowTaxonomyCombination) {
    const Index ns[] = {1000, 2000, 5000, 10000, 20000};
    const ConstraintFamily windows[] = {ConstraintFamily::kBoundArc,
                                        ConstraintFamily::kPathInterface};
    const StartTaxonomy taxonomies[] = {StartTaxonomy::kNeutralCold,
                                        StartTaxonomy::kPhysicsInformed, StartTaxonomy::kCorrupted,
                                        StartTaxonomy::kActivityOnly, StartTaxonomy::kFullWarm};
    for (const Index n : ns) {
        for (const ConstraintFamily w : windows) {
            for (const StartTaxonomy t : taxonomies) {
                const int count = static_cast<int>(
                    std::count_if(all_cells().begin(), all_cells().end(), [&](const CorpusCell &c) {
                        return c.n_nodes == n && c.ctag == w && c.start == t &&
                               c.family == BenchFamily::kF7;
                    }));
                EXPECT_EQ(count, 1)
                    << "N=" << n << " window=" << to_string(w) << " taxonomy=" << to_string(t)
                    << " must be present exactly once in the main grid";
            }
        }
    }
}

TEST(CorpusCells, DegenerateCellsAreExactlyTheNamedTenCitizens) {
    // N=2000/path-interface (all 5 taxonomies, the named "wide-window stall")
    // plus N=800/path-interface (all 5 taxonomies, the named "frozen cell") =
    // 10 degenerate cells, and NOTHING ELSE is tagged degenerate.
    int degenerate_count = 0;
    for (const CorpusCell &c : all_cells()) {
        if (c.degenerate) {
            ++degenerate_count;
            EXPECT_EQ(c.ctag, ConstraintFamily::kPathInterface)
                << c.id << ": every degenerate cell in this census is path-interface";
            EXPECT_TRUE(c.n_nodes == 2000 || c.n_nodes == 800)
                << c.id << ": every degenerate cell in this census is N=800 or N=2000, got "
                << c.n_nodes;
        }
    }
    EXPECT_EQ(degenerate_count, 10);
}

TEST(CorpusCells, HealthyControlsFlankTheFrozenCell) {
    const CorpusCell *c750 = find_cell("f7_n750_path_neutral_control");
    const CorpusCell *c825 = find_cell("f7_n825_path_neutral_control");
    ASSERT_NE(c750, nullptr);
    ASSERT_NE(c825, nullptr);
    for (const CorpusCell *c : {c750, c825}) {
        EXPECT_FALSE(c->degenerate) << c->id << ": these are the STUDY'S OWN healthy controls";
        EXPECT_EQ(c->start, StartTaxonomy::kNeutralCold);
        EXPECT_EQ(c->ctag, ConstraintFamily::kPathInterface);
    }
}

TEST(CorpusCells, ActivityOnlyAndFullWarmCellsHaveAProducer) {
    // Every kActivityOnly/kFullWarm cell in the census must be reachable
    // through run_cell's "walk" dispatch -- i.e. detail::run_cell_walk's
    // switch has a real case for each, not a fallthrough to the "unrecognised
    // taxonomy" throw. STATIC half: both taxonomies appear in the main grid
    // at every N/window, plus N=800, so 5*2 + 1 = 11 each.
    int activity_only = 0;
    int full_warm = 0;
    for (const CorpusCell &c : all_cells()) {
        activity_only += (c.start == StartTaxonomy::kActivityOnly) ? 1 : 0;
        full_warm += (c.start == StartTaxonomy::kFullWarm) ? 1 : 0;
    }
    EXPECT_EQ(activity_only, 11);
    EXPECT_EQ(full_warm, 11);

    // BEHAVIOURAL half (fix round 1, M8): a count is not an existence proof.
    // Replay a tiny cell of each of the two taxonomies and require a real row
    // back -- if either producer were missing, run_cell would throw the
    // "unrecognised StartTaxonomy" message instead.
    for (const StartTaxonomy t : {StartTaxonomy::kActivityOnly, StartTaxonomy::kFullWarm}) {
        SCOPED_TRACE(to_string(t));
        EXPECT_NO_THROW({
            const CorpusRow row = run_cell(tiny_cell(t), "walk");
            EXPECT_STREQ(row.cell_id, "tiny");
        });
    }
}

TEST(CorpusCells, NonHoppingTaxonomiesCarryP0EqualToP) {
    // kNeutralCold/kPhysicsInformed/kActivityOnly never read p0 (see
    // corpus_cells.h's taxonomy note); the census convention sets p0 == p for
    // them so the field is never a meaningless sentinel.
    for (const CorpusCell &c : all_cells()) {
        if (c.start == StartTaxonomy::kNeutralCold || c.start == StartTaxonomy::kPhysicsInformed ||
            c.start == StartTaxonomy::kActivityOnly) {
            EXPECT_EQ(c.p0, c.p) << c.id;
        } else {
            EXPECT_NE(c.p0, c.p) << c.id << ": kCorrupted/kFullWarm must hop from a distinct p0";
        }
    }
}

// =============================================================================
// THE ENGINE INTERFACE: run_cell dispatch, determinism, producers.
// =============================================================================

TEST(CorpusCellsRunner, UnknownEngineNameThrows) {
    const CorpusCell cell = tiny_cell(StartTaxonomy::kNeutralCold, /*use_p0=*/false);
    EXPECT_THROW(run_cell(cell, "gauss_newton"), std::invalid_argument);
}

TEST(CorpusCellsRunner, EngineSsnSelectsTheSemismoothKernelAndNothingElse) {
    // PHASE-7 TASK 6. Task 1 shipped this name THROWING (no consumer for
    // SqpOptions::qp_mode existed); this task wired it up, and the whole
    // validity of the SSN-vs-walk comparison rests on the two arms differing
    // in exactly one field. Asserted directly on the option object, not
    // inferred from a counter.
    const CorpusCell cell = tiny_cell(StartTaxonomy::kNeutralCold, /*use_p0=*/false);
    detail::EngineConfig walk_cfg;
    detail::EngineConfig ssn_cfg;
    ssn_cfg.qp_mode = hven::solvers::QpMode::kSsn;
    const hven::solvers::SqpOptions w = detail::options_for_cell(cell, walk_cfg);
    const hven::solvers::SqpOptions s = detail::options_for_cell(cell, ssn_cfg);
    EXPECT_EQ(w.qp_mode, hven::solvers::QpMode::kWalk);
    EXPECT_EQ(s.qp_mode, hven::solvers::QpMode::kSsn);
    EXPECT_EQ(w.kkt_tol, s.kkt_tol);
    EXPECT_EQ(w.feas_tol, s.feas_tol);
    EXPECT_EQ(w.max_iter, s.max_iter);
    EXPECT_EQ(w.qp.max_iter, s.qp.max_iter);
    EXPECT_EQ(w.adaptive_mu, s.adaptive_mu);
    EXPECT_EQ(w.warm_full_step, s.warm_full_step);
    EXPECT_EQ(w.ssn_prox_carry, s.ssn_prox_carry) << "the prox carry is a LEVER, not part of the "
                                                     "engine selection -- it ships off in both";

    // And the engine actually runs: the kernel's own counters move under ssn
    // and are structurally zero under walk.
    const CorpusRow ssn_row = run_cell(cell, "ssn");
    const CorpusRow walk_row = run_cell(cell, "walk");
    EXPECT_GT(ssn_row.ssn.ssn_iters, 0);
    EXPECT_EQ(walk_row.ssn.ssn_iters, 0);
    EXPECT_EQ(walk_row.ssn.ssn_refinements, 0);
    EXPECT_EQ(walk_row.ssn.ssn_refine_factorizations, 0);
    EXPECT_EQ(walk_row.ssn.ssn_refine_neg_duals, 0);
    EXPECT_EQ(walk_row.escapes, 0);
}

TEST(CorpusCellsRunner, SsnEngineIsDeterministicOnEveryTaxonomy) {
    // The walk arm's own determinism test, applied to the second engine, and
    // for the same reason: five producers, five routes, five places a coupling
    // to clock/address/allocator state could hide. The SSN kernel additionally
    // carries an iteration history the walk does not, so its counters are a
    // strictly wider surface to be non-deterministic on.
    for (StartTaxonomy t :
         {StartTaxonomy::kNeutralCold, StartTaxonomy::kPhysicsInformed, StartTaxonomy::kCorrupted,
          StartTaxonomy::kActivityOnly, StartTaxonomy::kFullWarm}) {
        SCOPED_TRACE(to_string(t));
        const CorpusCell cell = tiny_cell(t);
        const CorpusRow a = run_cell(cell, "ssn");
        const CorpusRow b = run_cell(cell, "ssn");
        EXPECT_EQ(a.factorizations, b.factorizations);
        EXPECT_EQ(a.qp_minors, b.qp_minors);
        EXPECT_EQ(a.escapes, b.escapes);
        EXPECT_EQ(a.status, b.status);
        EXPECT_EQ(a.qp_factorizations, b.qp_factorizations);
        EXPECT_EQ(a.kkt_residual, b.kkt_residual);
        EXPECT_EQ(a.ssn.ssn_iters, b.ssn.ssn_iters);
        EXPECT_EQ(a.ssn.ssn_bulk_flips, b.ssn.ssn_bulk_flips);
        EXPECT_EQ(a.ssn.ssn_refinements, b.ssn.ssn_refinements);
        EXPECT_EQ(a.ssn.ssn_refine_refused, b.ssn.ssn_refine_refused);
        EXPECT_EQ(a.ssn.ssn_refine_factorizations, b.ssn.ssn_refine_factorizations);
        EXPECT_EQ(a.ssn.ssn_refine_neg_duals, b.ssn.ssn_refine_neg_duals);
        EXPECT_EQ(a.kkt_complementarity, b.kkt_complementarity);
    }
}

TEST(CorpusCellsRunner, TheRefinementsOwnFactorizationCountIsBoundedByItsAttempts) {
    // TASK 6 INSTRUMENT. `ssn_refine_factorizations` is a COST, not a count of
    // attempts, and the two are not equal: refine_on_face short-circuits an
    // empty face and fails its rank pre-screen BEFORE any factorization, so a
    // refusal can cost 0. The invariants that must hold either way --
    //     refinements <= refine_factorizations <= refinements + refusals
    // -- are what a mutation replacing the field with either endpoint breaks.
    for (StartTaxonomy t :
         {StartTaxonomy::kNeutralCold, StartTaxonomy::kPhysicsInformed, StartTaxonomy::kCorrupted,
          StartTaxonomy::kActivityOnly, StartTaxonomy::kFullWarm}) {
        SCOPED_TRACE(to_string(t));
        const CorpusRow row = run_cell(tiny_cell(t), "ssn");
        EXPECT_GT(row.ssn.ssn_refinements, 0) << "this fixture must actually refine, or the "
                                                 "invariant below is vacuous";
        EXPECT_GE(row.ssn.ssn_refine_factorizations, row.ssn.ssn_refinements)
            << "an ACCEPTED refinement solved the face, so it paid at least one factorization";
        EXPECT_LE(row.ssn.ssn_refine_factorizations,
                  row.ssn.ssn_refinements + row.ssn.ssn_refine_refused)
            << "refine_on_face pays at most one factorization per attempt";
        EXPECT_LE(row.ssn.ssn_refine_factorizations, row.factorizations)
            << "the refinement's cost is folded into the solve's own total, never added beside it";
        // AND THE SIGN COUNTER'S OWN COMPARISON. Every subproblem on this
        // fixture prices most of its inequality rows at EXACTLY 0 (they are off
        // the identified face), so a counter testing `<= 0.0` instead of
        // `< 0.0` would report dozens here. Zero is the measured value, and it
        // is what makes the strictness of the test observable.
        EXPECT_EQ(row.ssn.ssn_refine_neg_duals, 0)
            << "no NEGATIVE price is adopted on this fixture -- the many exactly-zero ones are "
               "not negative, and the counter must not say they are";
    }
}

TEST(CorpusCellsRunner, AnEscapeIsReportedFromTheDriversOwnCounterAndDragsTheWalkInWithIt) {
    // TASK 6. `CorpusRow::escapes` is G4's numerator. Under the walk it is
    // structurally zero (nothing to escape TO), so the field is only observable
    // on an SSN row that actually hands off -- and the census's own escaping
    // rows are N >= 1000 cells that run for seconds to minutes, far too heavy
    // for ctest. THIS is the cheap fixture that exercises it: N = 64 nodes
    // (nx = 320), p = 0.60, kFullWarm, found by a small sweep over
    // (N, p, taxonomy). One subproblem hands off, so `escapes` is 1 AND
    // `qp_minors` is nonzero -- the walk really did re-solve it, which is the
    // half of the hand-off contract a hard-coded escape count would not show.
    const CorpusCell cell{
        "escape_fixture",    BenchFamily::kF7,
        /*n_nodes=*/64,
        /*p0=*/0.55,
        /*p=*/0.60,
        /*step_index=*/0,    StartTaxonomy::kFullWarm, ConstraintFamily::kPathInterface,
        /*degenerate=*/false};
    const CorpusRow ssn = run_cell(cell, "ssn");
    EXPECT_EQ(ssn.escapes, 1) << "one subproblem handed off to the walk";
    EXPECT_GT(ssn.qp_minors, 0) << "and the walk really re-solved it";
    EXPECT_EQ(ssn.status, hven::solvers::SqpStatus::kOptimal);
    // PHASE-7 TASK 6b (docket D6): and the row now carries WHY, not just how
    // many. The census partitions `escapes` exactly -- the property
    // bench_corpus.cpp's reader enforces on every artifact row it scores.
    const auto census_sum = [](const SsnCounters &c) {
        return c.ssn_escape_budget + c.ssn_escape_singular + c.ssn_escape_no_contraction +
               c.ssn_escape_infeasible_suspect + c.ssn_escape_indefinite +
               c.ssn_escape_gate_refused;
    };
    EXPECT_EQ(census_sum(ssn.ssn), ssn.escapes) << "the census must partition the total";
    const CorpusRow walk = run_cell(cell, "walk");
    EXPECT_EQ(walk.escapes, 0) << "the walk has nowhere to escape to, structurally";
    EXPECT_EQ(walk.ssn.ssn_iters, 0);
    EXPECT_EQ(census_sum(walk.ssn), 0) << "and the walk's census is structurally empty";
}

TEST(CorpusCellsRunner, TheKktCheckRecordsTheRowsOwnScaleDenominators) {
    // W3's denominators come off the RETURNED SOLUTION, not from a constant.
    // On this corpus the multipliers happen to be O(1), so a mutation pinning
    // either denominator to 1.0 would be invisible on any real cell here --
    // `record_kkt_check` is therefore driven DIRECTLY on a hand-built solution
    // carrying a large multiplier and a large primal, which is exactly the
    // regime the relative rule exists for.
    hven::solvers::corpus::F7CollocationChain model(12, 3, 2, 0.85, 1.0);
    model.set_parameters(hven::Vec::Constant(1, 0.85));
    hven::solvers::SqpSolution sol;
    sol.x = hven::Vec::Constant(model.n(), 3.0);
    sol.lambda_e = hven::Vec::Zero(model.me());
    sol.lambda_i = hven::Vec::Zero(model.mi());
    sol.z = hven::Vec::Zero(model.n());
    ASSERT_GT(model.mi(), 0);
    sol.lambda_i(0) = 1.0e6;
    CorpusRow row{};
    detail::record_kkt_check(model, sol, detail::kFeasTol, row);
    EXPECT_DOUBLE_EQ(row.dual_scale, 1.0e6) << "the DUAL denominator is max(1, ||lambda||inf, "
                                               "||z||inf) off the returned solution";
    EXPECT_DOUBLE_EQ(row.x_scale, 3.0) << "the PRIMAL denominator is max(1, ||x||inf)";
    EXPECT_EQ(row.neg_ineq_duals, 0);
    sol.lambda_i(1) = -2.0;
    detail::record_kkt_check(model, sol, detail::kFeasTol, row);
    EXPECT_EQ(row.neg_ineq_duals, 1);
    EXPECT_DOUBLE_EQ(row.kkt_dual_sign, 2.0);
}

TEST(CorpusCellsRunner, EveryRowCarriesAModelLevelKktCheckUnderBothEngines) {
    // INSTRUMENT REQUIREMENT 1 (Task-5 re-review NF-2), at its widest: the
    // check is not an ssn-only path, because the WALK column is what calibrates
    // the rule (W4). A negative residual is the "never measured" sentinel and
    // must not appear on a row that finished.
    for (const char *engine : {"walk", "ssn"}) {
        SCOPED_TRACE(engine);
        const CorpusRow row = run_cell(tiny_cell(StartTaxonomy::kNeutralCold), engine);
        ASSERT_EQ(row.status, hven::solvers::SqpStatus::kOptimal);
        EXPECT_GE(row.kkt_stationarity, 0.0);
        EXPECT_GE(row.kkt_primal, 0.0);
        EXPECT_GE(row.kkt_dual_sign, 0.0);
        EXPECT_GE(row.kkt_complementarity, 0.0);
        EXPECT_GE(row.dual_scale, 1.0);
        EXPECT_GE(row.x_scale, 1.0);
        EXPECT_GE(row.neg_ineq_duals, 0);
        EXPECT_EQ(kkt_gate_verdict(row), KktVerdict::kOk)
            << "both kernels reach a genuine KKT point on this fixture";
    }
}

TEST(CorpusCellsRunner, BothKernelsAgreeOnTheANSWEREvenWhereTheyDisagreeOnTheCOST) {
    // The comparison's own sanity condition, and the reason the KKT gate can
    // be calibrated on the walk arm at all: on a fixture both kernels solve,
    // the RESIDUALS coincide while the WORK does not. A mutation that made the
    // ssn arm solve a different problem (a different tolerance, a different
    // start, a different model) shows up here and nowhere else.
    const CorpusCell cell = tiny_cell(StartTaxonomy::kNeutralCold);
    const CorpusRow w = run_cell(cell, "walk");
    const CorpusRow s = run_cell(cell, "ssn");
    EXPECT_EQ(w.status, s.status);
    EXPECT_NEAR(w.kkt_stationarity, s.kkt_stationarity, 1e-12);
    EXPECT_NEAR(w.kkt_primal, s.kkt_primal, 1e-12);
    EXPECT_NEAR(w.kkt_complementarity, s.kkt_complementarity, 1e-12);
    EXPECT_NE(w.factorizations, s.factorizations) << "and the cost genuinely differs";
}

TEST(CorpusCellsRunner, WalkEngineIsDeterministic) {
    // ALL FIVE taxonomies, not just kNeutralCold -- each one's producer takes
    // a different route to a WarmStart (or none at all), and each route is a
    // place an accidental coupling to wall-clock/address/allocator state
    // could sneak in without the others catching it (this is precisely what
    // caught a hand-injected mutation in kCorrupted's displacement during
    // this task's own mutation pass: a NeutralCold-only version of this test
    // did not).
    for (StartTaxonomy t :
         {StartTaxonomy::kNeutralCold, StartTaxonomy::kPhysicsInformed, StartTaxonomy::kCorrupted,
          StartTaxonomy::kActivityOnly, StartTaxonomy::kFullWarm}) {
        SCOPED_TRACE(to_string(t));
        const CorpusCell cell = tiny_cell(t);
        const CorpusRow a = run_cell(cell, "walk");
        const CorpusRow b = run_cell(cell, "walk");
        EXPECT_EQ(a.factorizations, b.factorizations);
        EXPECT_EQ(a.qp_minors, b.qp_minors);
        EXPECT_EQ(a.escapes, b.escapes);
        EXPECT_EQ(a.status, b.status);
        EXPECT_EQ(a.qp_factorizations, b.qp_factorizations)
            << "the per-QP vector the gates score must be deterministic too, not just its sum";
        EXPECT_EQ(a.kkt_residual, b.kkt_residual)
            << "bit-identical, not merely close -- same model, same options, same start, no RNG "
               "anywhere";
    }
}

TEST(CorpusCellsRunner, WalkEngineNeverReportsAnEscape) {
    // The walk engine has nowhere to escape TO (Task 3+ is what makes this
    // field move); every taxonomy must report escapes == 0 under "walk".
    for (StartTaxonomy t :
         {StartTaxonomy::kNeutralCold, StartTaxonomy::kPhysicsInformed, StartTaxonomy::kCorrupted,
          StartTaxonomy::kActivityOnly, StartTaxonomy::kFullWarm}) {
        const CorpusRow row = run_cell(tiny_cell(t), "walk");
        EXPECT_EQ(row.escapes, 0) << to_string(t);
    }
}

TEST(CorpusCellsRunner, PerQpFactorizationsMatchTheIterateHistoryExactly) {
    // FIX ROUND 1 (C3). G1/G2 are pre-registered PER QP, and this vector is
    // where that quantity comes from: one entry per history row with
    // `qp_solved`, carrying that row's own SqpIterate::qp_factorizations.
    // Checked against a solve run directly here, so a mutation that (say)
    // pushed the whole-solve sum, or included stopped-AT-iterate rows, is
    // caught.
    hven::solvers::corpus::F7CollocationChain model(12, 3, 2, 0.85, 1.0);
    model.set_parameters(hven::Vec::Constant(1, 0.85));
    hven::solvers::SqpDriver driver(
        detail::options_for_cell(tiny_cell(StartTaxonomy::kNeutralCold)));
    const auto sol = detail::budgeted_solve(driver, model, model.start_point());

    std::vector<int> expected;
    for (const auto &it : sol.history) {
        if (it.qp_solved) {
            expected.push_back(static_cast<int>(it.qp_factorizations));
        }
    }
    ASSERT_FALSE(expected.empty()) << "this fixture must build at least one subproblem";

    const CorpusRow row =
        run_cell(tiny_cell(StartTaxonomy::kNeutralCold, /*use_p0=*/false), "walk");
    EXPECT_EQ(row.qp_factorizations, expected);
    int sum = 0;
    for (const int f : row.qp_factorizations) {
        sum += f;
    }
    EXPECT_LE(sum, row.factorizations)
        << "the per-QP entries are a subset of the whole-solve total, never more than it";
}

TEST(CorpusCellsRunner, SetupCompleteFiresExactlyOncePerCell) {
    // FIX ROUND 1 (I1). The runner restarts its wall deadline on this
    // callback, so "exactly once, on every taxonomy" is load-bearing: a
    // taxonomy that never fired it would be judged against the SETUP budget
    // for its whole life and DNF as `dnf_setup` no matter how fast its own
    // solve was.
    for (StartTaxonomy t :
         {StartTaxonomy::kNeutralCold, StartTaxonomy::kPhysicsInformed, StartTaxonomy::kCorrupted,
          StartTaxonomy::kActivityOnly, StartTaxonomy::kFullWarm}) {
        SCOPED_TRACE(to_string(t));
        int calls = 0;
        const CorpusRow row = run_cell(tiny_cell(t), "walk", [&] { ++calls; });
        EXPECT_EQ(calls, 1);
        EXPECT_STREQ(row.cell_id, "tiny");
    }
}

TEST(CorpusCellsRunner, ActivityOnlyStartsOffTheOptimumAndCarriesAnExactActivityHint) {
    // FIX ROUND 1 (I2). The first issue of this task built the crossover at
    // x = x*(p) EXACTLY, so every kActivityOnly cell certified optimality at
    // x0 and built no QP at all -- 11 of 57 cells measuring nothing, six of
    // them inside G1/G2's own population. Two properties are pinned here:
    //   (a) the PRIMAL is kPhysicsInformed's own rollout, i.e. genuinely off
    //       the analytic optimum, so the solve does real work;
    //   (b) the ACTIVITY HINT is still EXACT -- from_interior_point infers
    //       activity from the dual/slack pair alone, so displacing the primal
    //       does not degrade it.
    hven::solvers::corpus::F7CollocationChain model(12, 3, 2, 0.85, 1.0);
    model.set_parameters(hven::Vec::Constant(1, 0.85));
    const hven::Vec x_star = model.x_star(0.85);
    const hven::Vec x0 = detail::physics_informed_start(model, 0.85);
    EXPECT_GT((x0 - x_star).cwiseAbs().maxCoeff(), 0.0)
        << "the crossover primal must not BE the answer";

    const detail::IpIterate it =
        detail::f7_ip_iterate(model, 0.85, detail::crossover_mu_for_n(12), x0);
    EXPECT_EQ(it.x, x0) << "the iterate carries the physics-informed primal, not x*";
    const hven::solvers::WarmStart crossover = hven::solvers::from_interior_point(
        it.x, it.lambda_e, it.lambda_i, it.slack_i, it.z_lower, it.z_upper, model.lower(),
        model.upper(), hven::solvers::IpCrossoverOptions{});

    const auto analytic = model.active_set(0.85);
    ASSERT_EQ(crossover.ineq_active.size(), analytic.ineq_active.size());
    for (std::size_t j = 0; j < analytic.ineq_active.size(); ++j) {
        EXPECT_EQ(crossover.ineq_active[j] != 0, analytic.ineq_active[j] != 0)
            << "row " << j << ": the hint must reproduce the family's analytic active set exactly";
    }

    const CorpusRow row =
        run_cell(tiny_cell(StartTaxonomy::kActivityOnly, /*use_p0=*/false), "walk");
    EXPECT_EQ(row.status, hven::solvers::SqpStatus::kOptimal);
    EXPECT_FALSE(row.qp_factorizations.empty())
        << "a crossover cell must build at least one QP -- measuring nothing is the defect this "
           "test exists against";
}

TEST(CorpusCellsRunner, ActivityOnlyResolvesInFarLessWorkThanItsMatchedPhysicsControl) {
    // The taxonomy's whole premise, now that it starts off the optimum: SAME
    // primal as kPhysicsInformed, PLUS an exact activity hint, must cost no
    // more identification than the primal alone. (An equality would also
    // pass; a regression that broke the hint would not.)
    const CorpusRow hinted =
        run_cell(tiny_cell(StartTaxonomy::kActivityOnly, /*use_p0=*/false), "walk");
    const CorpusRow control =
        run_cell(tiny_cell(StartTaxonomy::kPhysicsInformed, /*use_p0=*/false), "walk");
    EXPECT_EQ(hinted.status, hven::solvers::SqpStatus::kOptimal);
    EXPECT_EQ(control.status, hven::solvers::SqpStatus::kOptimal);
    EXPECT_LE(hinted.qp_minors, control.qp_minors)
        << "hinted=" << hinted.qp_minors << " control=" << control.qp_minors;
}

TEST(CorpusCellsRunner, FullWarmProducerBeatsNeutralColdOnMinors) {
    // The two-hop warm chain (cold at p0=0.80, then warm to p=0.85 on the
    // SAME driver) should need materially fewer minors at the target p than
    // a fresh cold solve there -- the ordinary warm-start advantage this
    // whole project measures everywhere else. This is a BEHAVIOURAL check
    // that the producer is wired to the real warm-start machinery (a stubbed
    // "producer" that quietly ran cold would still return kOptimal).
    const CorpusRow warm = run_cell(tiny_cell(StartTaxonomy::kFullWarm), "walk");
    const CorpusRow cold =
        run_cell(tiny_cell(StartTaxonomy::kNeutralCold, /*use_p0=*/false), "walk");
    EXPECT_EQ(warm.status, hven::solvers::SqpStatus::kOptimal);
    EXPECT_EQ(cold.status, hven::solvers::SqpStatus::kOptimal);
    EXPECT_LE(warm.qp_minors, cold.qp_minors)
        << "warm=" << warm.qp_minors << " cold=" << cold.qp_minors;
}

TEST(CorpusCellsRunner, CorruptedProducerStillResolvesWarmAndConverges) {
    // The damaged hand-off must not crash the driver, and (F7 being a
    // well-conditioned convex-in-the-relevant-sense family at this scale)
    // should still recover kOptimal -- the interesting question for a LARGER
    // corpus row is how much EXTRA work it costs, which this test does not
    // need to pin (that is what the baseline CSV is for).
    const CorpusRow row = run_cell(tiny_cell(StartTaxonomy::kCorrupted), "walk");
    EXPECT_EQ(row.status, hven::solvers::SqpStatus::kOptimal);
}

TEST(CorpusCellsRunner, KktResidualSentinelOnEmptyHistory) {
    // A default-constructed SqpSolution has an empty history (no subproblem
    // was ever built) -- the -1.0 sentinel this file's own last_kkt_residual
    // documents, tested directly rather than by hunting for a real F7 cell
    // that happens to converge at major_iters == 0.
    hven::solvers::SqpSolution sol;
    ASSERT_TRUE(sol.history.empty());
    EXPECT_DOUBLE_EQ(hven::solvers::corpus::detail::last_kkt_residual(sol), -1.0);
}

// =============================================================================
// PHASE-7 TASK 2 (PIQP acquisition oracle): first_qp_for_cell, the cell's OWN
// designated hop's first QP subproblem, built but NOT solved.
// =============================================================================

TEST(CorpusCellsRunner, FirstQpForCellMatchesBuildSubproblemOnNeutralCold) {
    // kNeutralCold's designated hop starts at (x0, 0, 0) -- exactly what
    // SqpDriver::solve's own first iteration would build. Compared against a
    // build_subproblem call made directly here, with the model built the same
    // way detail::make_model does, so a mutation that (say) fed the wrong x or
    // nonzero initial multipliers is caught.
    const CorpusCell cell = tiny_cell(StartTaxonomy::kNeutralCold, /*use_p0=*/false);
    hven::solvers::corpus::F7CollocationChain model(cell.n_nodes, 3, 2, cell.p, 1.0);
    model.set_parameters(hven::Vec::Constant(1, cell.p));
    const hven::Vec x0 = model.start_point();
    const hven::solvers::QpProblem expected = hven::solvers::build_subproblem(
        model, x0, hven::Vec::Zero(model.me()), hven::Vec::Zero(model.mi()));

    const hven::solvers::QpProblem actual = detail::first_qp_for_cell(cell);
    EXPECT_EQ(actual.n(), expected.n());
    EXPECT_EQ(actual.me(), expected.me());
    EXPECT_EQ(actual.mi(), expected.mi());
    EXPECT_TRUE(actual.g.isApprox(expected.g, 0.0)) << "g must match bit-for-bit";
    EXPECT_TRUE(actual.be.isApprox(expected.be, 0.0)) << "be must match bit-for-bit";
    EXPECT_TRUE(actual.bi.isApprox(expected.bi, 0.0)) << "bi must match bit-for-bit";
    EXPECT_TRUE(actual.lower.isApprox(expected.lower, 0.0)) << "lower must match bit-for-bit";
    EXPECT_TRUE(actual.upper.isApprox(expected.upper, 0.0)) << "upper must match bit-for-bit";
    EXPECT_EQ(actual.H.nonZeros(), expected.H.nonZeros());
    EXPECT_TRUE(actual.H.isApprox(expected.H, 0.0)) << "H must match bit-for-bit";
}

TEST(CorpusCellsRunner, FirstQpForCellUsesTheWarmHandoffsOwnDualsOnFullWarm) {
    // kFullWarm's designated hop starts at the SETUP solve's own exit
    // (seed.warm_start.x/lambda_e/lambda_i) -- NOT zero multipliers. Run the
    // exact same setup hop here and compare against build_subproblem called
    // on ITS output, so a mutation that (say) dropped the warm duals back to
    // zero, or reused the setup's own p rather than the target's, is caught.
    const CorpusCell cell = tiny_cell(StartTaxonomy::kFullWarm);
    hven::solvers::corpus::F7CollocationChain model(cell.n_nodes, 3, 2, cell.p0, 1.0);
    model.set_parameters(hven::Vec::Constant(1, cell.p0));
    hven::solvers::SqpDriver driver(detail::options_for_cell(cell));
    const auto seed = detail::budgeted_solve(driver, model, model.start_point());
    ASSERT_EQ(seed.status, hven::solvers::SqpStatus::kOptimal);

    model.set_parameters(hven::Vec::Constant(1, cell.p));
    const hven::solvers::QpProblem expected = hven::solvers::build_subproblem(
        model, seed.warm_start.x, seed.warm_start.lambda_e, seed.warm_start.lambda_i);

    const hven::solvers::QpProblem actual = detail::first_qp_for_cell(cell);
    EXPECT_TRUE(actual.g.isApprox(expected.g, 0.0));
    EXPECT_TRUE(actual.be.isApprox(expected.be, 0.0));
    EXPECT_TRUE(actual.bi.isApprox(expected.bi, 0.0));
    EXPECT_TRUE(actual.H.isApprox(expected.H, 0.0));
    // And distinguishing from the cold case above is the whole point of this
    // test: the warm multipliers are not all zero on this fixture.
    EXPECT_GT(seed.warm_start.lambda_e.cwiseAbs().maxCoeff(), 0.0)
        << "fixture must exercise a nonzero warm dual, or this test proves nothing";
}

TEST(CorpusCellsRunner, FirstQpForCellFiresSetupCompleteExactlyOnceOnEveryTaxonomy) {
    // Same seam run_cell_walk's own on_setup_complete fires at (right before
    // the designated hop's first evaluation) -- a caller wrapping this in a
    // wall deadline relies on that.
    for (StartTaxonomy t :
         {StartTaxonomy::kNeutralCold, StartTaxonomy::kPhysicsInformed, StartTaxonomy::kCorrupted,
          StartTaxonomy::kActivityOnly, StartTaxonomy::kFullWarm}) {
        SCOPED_TRACE(to_string(t));
        int calls = 0;
        const hven::solvers::QpProblem qp =
            detail::first_qp_for_cell(tiny_cell(t), [&] { ++calls; });
        EXPECT_EQ(calls, 1);
        EXPECT_GT(qp.n(), 0);
    }
}

TEST(CorpusCellsRunner, FirstQpForCellUnrecognisedTaxonomyThrows) {
    CorpusCell bad = tiny_cell(StartTaxonomy::kNeutralCold);
    bad.start = static_cast<StartTaxonomy>(99);
    EXPECT_THROW(detail::first_qp_for_cell(bad), std::invalid_argument);
}

// =============================================================================
// THE WALL-BUDGET BAND (fix round 1, C2): a pure function, pinned against the
// EVIDENCE it was derived from rather than against itself.
// =============================================================================

// The committed, uncontended, single-threaded reference runtimes the band's
// rule R1 is defined over -- docs/notes/2026-07-30-scale-study-cold.md Sec.
// 4.2's Arm B table, kNeutralCold (the hardest taxonomy at every N here),
// p = 0.85, qp.max_iter = 20000 where capped. Duplicated here ON PURPOSE: if
// someone lowers a tier, this table is what fails, and it names the note the
// number came from.
struct BudgetReference {
    Index n_nodes;
    double committed_uncontended_s;
    const char *provenance;
};

const BudgetReference kBudgetReferences[] = {
    {750, 73.9, "scale-study-cold.md 4.2, n=3750 uncapped kOptimal 9297 minors"},
    {800, 279.0, "scale-study-cold.md 4.2, n=4000 cap 20000 kOptimal 30165 minors (FROZEN CELL)"},
    {1000, 121.0, "scale-study-cold.md 4.2, n=5000 uncapped kOptimal 11043 minors"},
    {2000, 819.0, "scale-study-cold.md 4.2, n=10000 cap 20000 kNumericalError 40004 minors"},
};

TEST(CorpusBudget, EveryReferenceCellGetsAtLeastThreeTimesItsCommittedRuntime) {
    // RULE R1, tested as a rule rather than as a table of numerals. The
    // previous band failed exactly here: 300 s against a 279 s reference is
    // 1.08x, and ordinary scheduling noise closed it, deleting the named
    // frozen cell's reference row from the artifact every later task cites.
    for (const BudgetReference &ref : kBudgetReferences) {
        SCOPED_TRACE(fmt::format("N={} ({})", ref.n_nodes, ref.provenance));
        const double budget = detail::wall_budget_seconds(static_cast<Index>(5 * ref.n_nodes));
        EXPECT_GE(budget, 3.0 * ref.committed_uncontended_s)
            << "N=" << ref.n_nodes << ": budget " << budget << "s gives only "
            << budget / ref.committed_uncontended_s << "x margin over " << ref.provenance;
    }
}

TEST(CorpusBudget, TheLadderIsMonotoneInNx) {
    // RULE R2. The previous band inherited a NON-monotonicity (900 s at
    // nx = 10^4 but 600 s at nx = 10^5) that gave the census's hardest SMALL
    // cell the tightest budget.
    double previous = 0.0;
    for (const Index nx :
         {Index{1}, Index{4000}, Index{8000}, Index{8001}, Index{10000}, Index{10001}, Index{25000},
          Index{50000}, Index{100000}, Index{1000000}}) {
        const double budget = detail::wall_budget_seconds(nx);
        EXPECT_GE(budget, previous) << "nx=" << nx;
        previous = budget;
    }
}

TEST(CorpusBudget, TheLadderMatchesItsDocumentedTiersExactly) {
    using detail::wall_budget_seconds;
    EXPECT_DOUBLE_EQ(wall_budget_seconds(1), 900.0);
    EXPECT_DOUBLE_EQ(wall_budget_seconds(8000), 900.0) << "the floor tier's own boundary";
    EXPECT_DOUBLE_EQ(wall_budget_seconds(8001), 2700.0);
    EXPECT_DOUBLE_EQ(wall_budget_seconds(10000), 2700.0) << "nx = 10^4, N = 2000's tier";
    EXPECT_DOUBLE_EQ(wall_budget_seconds(10001), 3600.0) << "the ceiling begins";
    EXPECT_DOUBLE_EQ(wall_budget_seconds(100000), 3600.0);
}

TEST(CorpusBudget, WallBudgetForCellUsesFiveTimesNodeCountAsNx) {
    using detail::wall_budget_for_cell;
    // tiny_cell is N = 12 -> nx = 60, well inside the floor tier.
    EXPECT_DOUBLE_EQ(wall_budget_for_cell(tiny_cell(StartTaxonomy::kNeutralCold)), 900.0);
    const CorpusCell *n2000 = find_cell("f7_n2000_path_neutral");
    ASSERT_NE(n2000, nullptr);
    EXPECT_DOUBLE_EQ(wall_budget_for_cell(*n2000), 2700.0) << "N=2000 -> nx=10000 exactly";
    const CorpusCell *n5000 = find_cell("f7_n5000_path_neutral");
    ASSERT_NE(n5000, nullptr);
    EXPECT_DOUBLE_EQ(wall_budget_for_cell(*n5000), 3600.0) << "N=5000 -> nx=25000, no reference";
    const CorpusCell *n20000 = find_cell("f7_n20000_path_neutral");
    ASSERT_NE(n20000, nullptr);
    EXPECT_DOUBLE_EQ(wall_budget_for_cell(*n20000), 3600.0) << "N=20000 -> nx=100000";
}

TEST(CorpusBudget, BudgetTableHashPinsTheCommittedTable) {
    // Stamped into every CSV's provenance header, so an artifact produced
    // under a different band is identifiable at a glance. Pinned here so the
    // hash cannot drift silently from the table it fingerprints.
    EXPECT_EQ(budget_table_hash(), 0x357aee91dee27391ULL);
}

// =============================================================================
// THE PER-SOLVE MINOR BUDGET (secondary net; the primary mechanism is the
// runner's wall deadline). See corpus_cells.h's kMinorBudget/budgeted_solve.
// =============================================================================

TEST(CorpusCellsRunner, MinorBudgetConstantIsFiftyThousand) {
    // Pins the PRODUCTION value directly (rather than only the wiring that
    // forwards it): on a tiny fixture a budget of 50000 and a budget of 0
    // (unbounded) are behaviourally indistinguishable, so a solve-based
    // comparison alone cannot catch a mutation to the DEFAULT.
    EXPECT_EQ(hven::solvers::corpus::detail::kMinorBudget, 50000);
}

TEST(CorpusCellsRunner, BudgetedSolveMatchesAnExplicitDriverCallAtTheSameBudget) {
    hven::solvers::corpus::F7CollocationChain model(12, 3, 2, 0.85, 1.0);
    model.set_parameters(hven::Vec::Constant(1, 0.85));
    hven::solvers::SqpOptions opts;
    opts.kkt_tol = 1e-8;
    opts.feas_tol = 1e-8;

    hven::solvers::SqpDriver driver_a(opts);
    const auto direct = driver_a.solve(model, model.start_point(), hven::solvers::WarmStart{},
                                       hven::solvers::corpus::detail::kMinorBudget);
    hven::solvers::SqpDriver driver_b(opts);
    const auto via_helper =
        hven::solvers::corpus::detail::budgeted_solve(driver_b, model, model.start_point());

    EXPECT_EQ(direct.status, via_helper.status);
    EXPECT_EQ(direct.counters.qp_minor_iters, via_helper.counters.qp_minor_iters);
    EXPECT_EQ(direct.counters.factorizations, via_helper.counters.factorizations);
    EXPECT_EQ(direct.counters.probe_budget_stops, via_helper.counters.probe_budget_stops);
}

TEST(CorpusCellsRunner, BudgetedSolveTruncatesIntoADnfRowRatherThanHanging) {
    // A tiny EXPLICIT budget (1 minor) on a fixture that genuinely needs more
    // must stop at SqpStatus::kMaxIter with probe_budget_stops == 1 --
    // sqp_types.h's own documented contract -- rather than run to completion
    // or hang.
    hven::solvers::corpus::F7CollocationChain model(12, 3, 2, 0.85, 1.0);
    model.set_parameters(hven::Vec::Constant(1, 0.85));
    hven::solvers::SqpOptions opts;
    opts.kkt_tol = 1e-8;
    opts.feas_tol = 1e-8;
    opts.max_iter = 10;
    hven::solvers::SqpDriver driver(opts);

    const auto sol = hven::solvers::corpus::detail::budgeted_solve(
        driver, model, model.start_point(), hven::solvers::WarmStart{}, /*budget=*/1);

    EXPECT_EQ(sol.status, hven::solvers::SqpStatus::kMaxIter);
    EXPECT_EQ(sol.counters.probe_budget_stops, 1);
    EXPECT_GE(sol.counters.qp_minor_iters, 1)
        << "the budget was crossed, not skipped -- some real work was still done";
}

// =============================================================================
// THE RUNNER AS A PROCESS: the two-phase wall deadline, the provenance
// header, and the offline scoring path. All SUBPROCESS tests -- the
// orchestration lives in bench_corpus.cpp's main() and is not reachable from
// a gtest linked only against corpus_cells.h. HVEN_SQP_CORPUS_BINARY is
// injected by bench/CMakeLists.txt (see its own comment).
// =============================================================================

namespace runner_test {

std::string temp_path(const std::string &stem) { return ::testing::TempDir() + stem; }

int run_binary(const std::string &args, const std::string &stdout_path = "/dev/null") {
    const std::string cmd =
        fmt::format("{} {} > {} 2>&1", HVEN_SQP_CORPUS_BINARY, args, stdout_path);
    return std::system(cmd.c_str());
}

std::vector<std::string> read_lines(const std::string &path) {
    std::ifstream in(path);
    std::vector<std::string> out;
    std::string line;
    while (std::getline(in, line)) {
        out.push_back(line);
    }
    return out;
}

// Data rows only: provenance comments and the column header dropped.
std::vector<std::string> data_rows(const std::string &path) {
    std::vector<std::string> out;
    for (const std::string &line : read_lines(path)) {
        if (line.empty() || line[0] == '#' || line.rfind("cell_id,", 0) == 0) {
            continue;
        }
        out.push_back(line);
    }
    return out;
}

std::vector<std::string> split_all(const std::string &s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        out.push_back(item);
    }
    return out;
}

std::string slurp(const std::string &path) {
    std::ifstream in(path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace runner_test

TEST(CorpusRunnerProcess, WallDeadlineEmitsADnfBudgetRowWhenTheSOLVEPhaseIsForcedTiny) {
    // THE FIXTURE HAS TO OUTLAST A POLL GAP, and the margin is the whole
    // design of this test. The parent polls every 20 ms (bench_corpus.cpp's
    // run_cell_with_deadline): it notices the setup marker at one poll, arms
    // the 1 ms SOLVE deadline, and can only enforce it at the NEXT poll -- so
    // a child whose SOLVE phase fits inside one poll gap exits first, is
    // reaped, and writes a finished row instead of a dnf_budget one. Nothing
    // is wrong with the enforcement when that happens; the fixture simply
    // failed to still be running when the parent looked.
    //
    // f7_n5000_bound_neutral, this test's original cell, was chosen against a
    // measured ~150-270 ms and lost that race on the macOS CI lane: the Apple
    // runner solves it in 83.7 ms (row emitted at
    // https://github.com/GrantHecht/hven/actions/runs/31824897327, `Optimal`,
    // wall_s 0.083678334) and, with CI scheduler jitter stretching the
    // parent's 20 ms sleep, the child finished inside a single gap. The test
    // passed on 2 of 8 m3 runs there and failed the other 6 -- a race, in the
    // observed direction.
    //
    // THE FIX IS THE FIXTURE, NOT THE ASSERTION. There is no per-platform
    // expectation here and no Apple number to pin: `dnf_budget` is what the
    // runner must emit on every platform. f7_n20000_bound_neutral is the same
    // cell shape at 4x the nodes and measures 0.35 s of SOLVE (Linux, Release,
    // clang, where the same measurement puts n=5000 at 0.091 s -- within ~10 %
    // of the Apple reading above, so a comparable ~4x margin there) while
    // costing this
    // test almost nothing in wall time, because the child is killed 20-40 ms
    // into a solve it never finishes -- only its SETUP is paid in full. Its
    // SETUP budget is left at the band (900 s) so this arm cannot accidentally
    // measure the other phase.
    //
    // If this ever races again, the next move is in-child enforcement (the
    // child checking its own deadline), NOT a platform-conditional assertion.
    const std::string csv = runner_test::temp_path("corpus_dnf_solve.csv");
    ASSERT_EQ(runner_test::run_binary(
                  fmt::format("--engine walk --cells f7_n20000_bound_neutral --csv {} "
                              "--internal-force-solve-budget-seconds 0.001",
                              csv)),
              0);
    const std::vector<std::string> rows = runner_test::data_rows(csv);
    ASSERT_EQ(rows.size(), 1u);
    const std::vector<std::string> cols = runner_test::split_all(rows[0]);
    // 14 Task-1 columns + Task 6's 17 + Phase-7 Task 6b's 6 (the escape-reason
    // census). A SCHEMA-GENERATION pin, moved deliberately with the schema.
    ASSERT_EQ(cols.size(), 37u) << rows[0];
    EXPECT_EQ(cols[0], "f7_n20000_bound_neutral");
    EXPECT_EQ(cols[6], "dnf_budget") << rows[0];
    for (const std::size_t i : {7u, 8u, 9u, 10u}) {
        EXPECT_EQ(cols[i], "-1") << "column " << i << " must be absent-by-design: " << rows[0];
    }
    EXPECT_EQ(cols[11], "") << "no per-QP list survives a kill: " << rows[0];
    EXPECT_DOUBLE_EQ(std::stod(cols[12]), -1.0) << rows[0];
    EXPECT_DOUBLE_EQ(std::stod(cols[13]), 0.001)
        << "wall_s on a DNF row is the ENFORCED DEADLINE, not a measurement: " << rows[0];
    std::remove(csv.c_str());
}

TEST(CorpusRunnerProcess, WallDeadlineEmitsADnfSetupRowWhenTheSETUPPhaseIsForcedTiny) {
    // FIX ROUND 1 (I1). The distinguishing arm: a microscopic SETUP budget
    // with a full-band SOLVE budget must kill the child BEFORE it signals
    // setup-complete, and the row must say `dnf_setup` -- not `dnf_budget`,
    // which would attribute the cost to a hand-off that was never even built.
    // Six of the first baseline's fourteen DNF rows were misattributed this
    // way.
    const std::string csv = runner_test::temp_path("corpus_dnf_setup.csv");
    ASSERT_EQ(
        runner_test::run_binary(fmt::format("--engine walk --cells f7_n5000_bound_warm --csv {} "
                                            "--internal-force-setup-budget-seconds 0.000001",
                                            csv)),
        0);
    const std::vector<std::string> rows = runner_test::data_rows(csv);
    ASSERT_EQ(rows.size(), 1u);
    const std::vector<std::string> cols = runner_test::split_all(rows[0]);
    // 14 Task-1 columns + Task 6's 17 + Phase-7 Task 6b's 6 (the escape-reason
    // census). A SCHEMA-GENERATION pin, moved deliberately with the schema.
    ASSERT_EQ(cols.size(), 37u) << rows[0];
    EXPECT_EQ(cols[6], "dnf_setup") << rows[0];
    std::remove(csv.c_str());
}

TEST(CorpusRunnerProcess, WallDeadlineDoesNotFireAtTheRealBudgetOnAFastCell) {
    // The regression control: WITHOUT forcing anything, the same cell (real
    // budget 3600 s per phase) converges normally through the subprocess
    // path and reports real counters, including a non-empty per-QP list.
    const std::string csv = runner_test::temp_path("corpus_fast.csv");
    ASSERT_EQ(runner_test::run_binary(
                  fmt::format("--engine walk --cells f7_n5000_bound_neutral --csv {}", csv)),
              0);
    const std::vector<std::string> rows = runner_test::data_rows(csv);
    ASSERT_EQ(rows.size(), 1u);
    const std::vector<std::string> cols = runner_test::split_all(rows[0]);
    // 14 Task-1 columns + Task 6's 17 + Phase-7 Task 6b's 6 (the escape-reason
    // census). A SCHEMA-GENERATION pin, moved deliberately with the schema.
    ASSERT_EQ(cols.size(), 37u) << rows[0];
    EXPECT_EQ(cols[6], "Optimal") << rows[0];
    EXPECT_NE(cols[7], "-1") << "a real solve must report a real factorization count: " << rows[0];
    EXPECT_NE(cols[10], "0") << "and at least one QP subproblem: " << rows[0];
    EXPECT_FALSE(cols[11].empty()) << "the per-QP list must be populated: " << rows[0];
    std::remove(csv.c_str());
}

TEST(CorpusRunnerProcess, EveryCsvCarriesAProvenanceHeader) {
    // FIX ROUND 1 (M1). The reviewer's biggest cannot-verify on the first
    // baseline was that the CSV said nothing about which binary or which
    // budget table produced it.
    const std::string csv = runner_test::temp_path("corpus_provenance.csv");
    ASSERT_EQ(runner_test::run_binary(
                  fmt::format("--engine walk --cells f7_n1000_bound_neutral --csv {}", csv)),
              0);
    const std::string text = runner_test::slurp(csv);
    EXPECT_NE(text.find("# hven_sqp_corpus provenance"), std::string::npos) << text;
    EXPECT_NE(text.find("# binary: "), std::string::npos) << text;
    EXPECT_NE(text.find(fmt::format("# budget_table_hash: {:#018x}", budget_table_hash())),
              std::string::npos)
        << text;
    EXPECT_NE(text.find("# invocation: "), std::string::npos) << text;
    EXPECT_NE(text.find("# MKL_NUM_THREADS: "), std::string::npos) << text;
    std::remove(csv.c_str());
}

TEST(CorpusRunnerProcess, AForcedTestBudgetIsStampedIntoTheProvenanceHeader) {
    // FIX ROUND 1 (M9). The hidden levers are parsed in every invocation,
    // including production ones. They cannot be made unreachable without
    // breaking the tests that need them, so instead they are made
    // UNMISSABLE: a warning on stderr and a WARNING line in the artifact.
    const std::string csv = runner_test::temp_path("corpus_forced.csv");
    const std::string log = runner_test::temp_path("corpus_forced.log");
    ASSERT_EQ(
        runner_test::run_binary(fmt::format("--engine walk --cells f7_n1000_bound_neutral --csv {} "
                                            "--internal-force-solve-budget-seconds 0.001",
                                            csv),
                                log),
        0);
    EXPECT_NE(runner_test::slurp(csv).find("# WARNING forced_test_budgets:"), std::string::npos);
    EXPECT_NE(runner_test::slurp(log).find("WARNING"), std::string::npos);
    std::remove(csv.c_str());
    std::remove(log.c_str());
}

TEST(CorpusRunnerProcess, ScoreGatesRunsEndToEndOnRealCells) {
    // I5's live arm: the whole --score-gates path (run, write, filter, pair,
    // score, print) over two cheap bound-arc cells. The verdict itself is
    // pinned on hand-built fixtures below and on the committed baseline; what
    // this arm proves is that the wiring runs on real rows without crashing
    // and prints all four gates.
    const std::string csv = runner_test::temp_path("corpus_score.csv");
    const std::string log = runner_test::temp_path("corpus_score.log");
    ASSERT_EQ(runner_test::run_binary(
                  fmt::format("--engine walk --cells f7_n1000_bound_activity,f7_n1000_bound_warm "
                              "--csv {} --score-gates",
                              csv),
                  log),
              0);
    const std::string text = runner_test::slurp(log);
    EXPECT_NE(text.find("G1 median factorizations per QP"), std::string::npos) << text;
    EXPECT_NE(text.find("G2 p95 factorizations per QP"), std::string::npos) << text;
    EXPECT_NE(text.find("G3 median growth 5000->20000"), std::string::npos) << text;
    EXPECT_NE(text.find("G4 escape rate per QP"), std::string::npos) << text;
    std::remove(csv.c_str());
    std::remove(log.c_str());
}

TEST(CorpusRunnerProcess, FromCsvRoundTripsAnArtifactAndOrdersItByCensus) {
    // FIX ROUND 1 (I6/M1). --from-csv is both the offline re-score path AND
    // the merge step that produces the committed baseline out of a fan-out's
    // one-row files; before this there was no executable path from 57
    // per-cell CSVs to the artifact, and the merge script was not committed.
    const std::string a = runner_test::temp_path("corpus_part_a.csv");
    const std::string b = runner_test::temp_path("corpus_part_b.csv");
    const std::string merged = runner_test::temp_path("corpus_merged.csv");
    // Deliberately produced in REVERSE census order (bound_warm is the fifth
    // N=1000 bound cell, bound_neutral the first).
    ASSERT_EQ(runner_test::run_binary(
                  fmt::format("--engine walk --cells f7_n1000_bound_warm --csv {}", a)),
              0);
    ASSERT_EQ(runner_test::run_binary(
                  fmt::format("--engine walk --cells f7_n1000_bound_neutral --csv {}", b)),
              0);
    ASSERT_EQ(runner_test::run_binary(fmt::format("--from-csv {},{} --csv {}", a, b, merged)), 0);

    const std::vector<std::string> rows = runner_test::data_rows(merged);
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(runner_test::split_all(rows[0])[0], "f7_n1000_bound_neutral") << "census order";
    EXPECT_EQ(runner_test::split_all(rows[1])[0], "f7_n1000_bound_warm");
    // And the counter columns survive the round trip byte-for-byte.
    const std::vector<std::string> original = runner_test::data_rows(a);
    ASSERT_EQ(original.size(), 1u);
    EXPECT_EQ(rows[1], original[0]);

    std::remove(a.c_str());
    std::remove(b.c_str());
    std::remove(merged.c_str());
}

TEST(CorpusRunnerProcess, FromCsvRejectsAnInternallyInconsistentRow) {
    // A row whose qp_subproblems column disagrees with its own per-QP list is
    // an artifact that must never be scored -- G1/G2 read the list and G4
    // reads its length.
    const std::string bad = runner_test::temp_path("corpus_bad.csv");
    {
        std::ofstream out(bad);
        out << "cell_id,family,n_nodes,window,taxonomy,degenerate,status,factorizations,qp_minors,"
               "escapes,qp_subproblems,qp_fact_per_qp,kkt_residual,wall_s\n";
        out << "f7_n1000_path_warm,F7,1000,path,warm,0,Optimal,3,12,0,5,1;2,1.0e-10,0.1\n";
    }
    EXPECT_NE(runner_test::run_binary(fmt::format("--from-csv {} --score-gates", bad)), 0);
    std::remove(bad.c_str());
}

TEST(CorpusRunnerProcess, FromCsvRejectsARowWhoseStoredKktVerdictContradictsItsOwnResiduals) {
    // TASK 6. The artifact may not assert its own innocence: the stored
    // verdict is RE-DERIVED from the stored residuals on read, and a
    // disagreement is a refusal to score, exactly as an inconsistent
    // qp_subproblems column is. Without this, "wrong-answer rows = 0" would be
    // a claim about a text file rather than about a solve.
    const std::string bad = runner_test::temp_path("corpus_bad_verdict.csv");
    {
        std::ofstream out(bad);
        out << "f7_n1000_path_warm,F7,1000,path,warm,0,Optimal,3,12,0,2,1;2,1.0e-10,0.1,"
               "ok,1e-10,1e-10,0.0,1.0,1.0,1.0,0,0,0,0,0,0,0,0,0,0\n";
    }
    EXPECT_NE(runner_test::run_binary(fmt::format("--from-csv {} --score-gates", bad)), 0)
        << "complementarity 1.0 against dual scale 1.0 is not 'ok'";
    std::remove(bad.c_str());

    // ...and the same row, honestly labelled, is accepted and SCORED as the
    // wrong answer it is.
    const std::string honest = runner_test::temp_path("corpus_honest_verdict.csv");
    const std::string log = runner_test::temp_path("corpus_honest_verdict.log");
    {
        std::ofstream out(honest);
        out << "f7_n1000_path_warm,F7,1000,path,warm,0,Optimal,3,12,0,2,1;2,1.0e-10,0.1,"
               "wrong,1e-10,1e-10,0.0,1.0,1.0,1.0,0,0,0,0,0,0,0,0,0,0\n";
    }
    ASSERT_EQ(runner_test::run_binary(fmt::format("--from-csv {} --score-gates", honest), log), 0);
    const std::string text = runner_test::slurp(log);
    EXPECT_NE(text.find("WRONG-ANSWER rows = 1"), std::string::npos) << text;
    EXPECT_NE(text.find("G1 median factorizations per QP = 1000000.000"), std::string::npos)
        << text;
    std::remove(honest.c_str());
    std::remove(log.c_str());
}

// =====================================================================
// PHASE-7 TASK 6b (docket D6) -- THE ESCAPE-REASON CENSUS's CSV SCHEMA.
//
// The census adds six columns (schema 37). The committed artifacts are
// schema 14 (Task 1's walk baseline) and schema 31 (the 2026-08-08 battery
// CSVs), they are PINNED evidence, and they are not regenerated -- so the
// reader has to accept all three generations, and has to say "absent" rather
// than "zero" for a census that was never measured.
// =====================================================================
TEST(CorpusRunnerProcess, FromCsvAcceptsBothSchemasAndCallsAnAbsentCensusAbsent) {
    // A schema-31 row (no census tail), honest in every column it does have.
    const std::string old_schema = runner_test::temp_path("corpus_schema31.csv");
    const std::string merged = runner_test::temp_path("corpus_schema31_merged.csv");
    {
        std::ofstream out(old_schema);
        out << "f7_n1000_path_warm,F7,1000,path,warm,0,Optimal,3,12,0,2,1;2,1.0e-10,0.1,"
               "ok,1e-10,1e-10,0.0,1e-10,1.0,1.0,0,0,0,0,0,0,0,0,0,0\n";
    }
    // It still SCORES -- that is the whole point of the optional tail.
    ASSERT_EQ(runner_test::run_binary(fmt::format("--from-csv {} --csv {}", old_schema, merged)), 0)
        << "a schema-31 artifact must keep reading through this reader";
    const std::vector<std::string> rows = runner_test::data_rows(merged);
    ASSERT_EQ(rows.size(), 1u);
    const std::vector<std::string> col = runner_test::split_all(rows[0]);
    ASSERT_EQ(col.size(), 37u) << "the merge writes the CURRENT schema";
    for (std::size_t i = 31; i < 37; ++i) {
        EXPECT_EQ(col[i], "-1") << "column " << i
                                << ": an unmeasured census is ABSENT, never a measured zero";
    }
    std::remove(old_schema.c_str());
    std::remove(merged.c_str());
}

TEST(CorpusRunnerProcess, FromCsvRejectsACensusThatDoesNotPartitionTheEscapeCount) {
    // Same contract as the qp_subproblems and kkt_verdict checks above: an
    // artifact whose own numbers contradict each other is refused, not scored.
    // Here `escapes` says 2 and the six buckets sum to 1.
    const std::string bad = runner_test::temp_path("corpus_bad_census.csv");
    {
        std::ofstream out(bad);
        out << "f7_n1000_path_warm,F7,1000,path,warm,0,Optimal,3,12,2,2,1;2,1.0e-10,0.1,"
               "ok,1e-10,1e-10,0.0,1e-10,1.0,1.0,0,0,0,0,0,0,0,0,0,0,"
               "1,0,0,0,0,0\n";
    }
    EXPECT_NE(runner_test::run_binary(fmt::format("--from-csv {} --score-gates", bad)), 0)
        << "six buckets summing to 1 against `escapes` = 2 is not scorable";
    std::remove(bad.c_str());

    // ...and the same row with a census that DOES partition is accepted.
    const std::string good = runner_test::temp_path("corpus_good_census.csv");
    const std::string log = runner_test::temp_path("corpus_good_census.log");
    {
        std::ofstream out(good);
        out << "f7_n1000_path_warm,F7,1000,path,warm,0,Optimal,3,12,2,2,1;2,1.0e-10,0.1,"
               "ok,1e-10,1e-10,0.0,1e-10,1.0,1.0,0,0,0,0,0,0,0,0,0,0,"
               "1,0,1,0,0,0\n";
    }
    EXPECT_EQ(runner_test::run_binary(fmt::format("--from-csv {} --score-gates", good), log), 0);
    std::remove(good.c_str());
    std::remove(log.c_str());
}

TEST(CorpusRunnerProcess, TheSsnArmGoesThroughTheSameWallDeadlineAsTheWalkArm) {
    // TASK 6. Task 1 exempted --engine ssn from the deadline machinery because
    // the name threw in-process. Now that it runs, an unbudgeted SSN arm would
    // be both a hang risk AND an unfair comparison against a budgeted walk
    // arm. Forced sub-millisecond solve budget on a real cell: the row must be
    // a dnf_budget, not a completed row and not a hang.
    const std::string csv = runner_test::temp_path("corpus_ssn_dnf.csv");
    ASSERT_EQ(
        runner_test::run_binary(fmt::format("--engine ssn --cells f7_n20000_path_neutral --csv {} "
                                            "--internal-force-solve-budget-seconds 0.001",
                                            csv)),
        0);
    const std::vector<std::string> rows = runner_test::data_rows(csv);
    ASSERT_EQ(rows.size(), 1u);
    const std::vector<std::string> col = runner_test::split_all(rows[0]);
    EXPECT_EQ(col[6], "dnf_budget");
    EXPECT_EQ(col[14], "unchecked") << "nothing was measured past the kill, so nothing is 'ok'";
    std::remove(csv.c_str());
}

TEST(CorpusRunnerProcess, AnEngineThatTHROWSBecomesAScoredRowNotALostCell) {
    // TASK 6, and it is not a hypothetical path: the kSsn arm THREW on four
    // path-interface cells of this census (see the task report's section 2.1a
    // for the mechanism). Task 1 treated an abnormally-exiting child as a hard
    // error that aborted the whole sweep -- correct while the only engine was
    // the walk, which cannot throw from inside a solve, and wrong now.
    //
    // The census's own throwing cells are N >= 2000 and run for minutes, so
    // the PARENT's path is driven here by a hidden test lever that makes the
    // child throw at the setup-complete seam. Three things are asserted: the
    // sweep CONTINUES, the row says `engine_error` (never a `dnf_*`, which
    // would claim a timeout that did not happen), and every counter column is
    // the absent sentinel.
    const std::string csv = runner_test::temp_path("corpus_engine_error.csv");
    const std::string log = runner_test::temp_path("corpus_engine_error.log");
    ASSERT_EQ(runner_test::run_binary(
                  fmt::format("--engine walk --cells f7_n1000_bound_neutral,f7_n1000_bound_warm "
                              "--csv {} --score-gates --internal-force-child-throw",
                              csv),
                  log),
              0)
        << "a throwing engine must not abort the sweep";
    const std::vector<std::string> rows = runner_test::data_rows(csv);
    ASSERT_EQ(rows.size(), 2u) << "BOTH cells produce a row -- the sweep continued";
    for (const std::string &r : rows) {
        const std::vector<std::string> col = runner_test::split_all(r);
        EXPECT_EQ(col[6], "engine_error") << r;
        EXPECT_EQ(col[7], "-1") << "factorizations are ABSENT, not zero: " << r;
        EXPECT_EQ(col[14], "unchecked") << "nothing was measured, so nothing is 'ok': " << r;
    }
    const std::string text = runner_test::slurp(log);
    EXPECT_NE(text.find("ENGINE-ERROR rows = 2"), std::string::npos) << text;
    EXPECT_NE(text.find("FORCED TEST THROW"), std::string::npos)
        << "the reason reaches the console, not just 'the child exited abnormally': " << text;
    // AND IT IS CHARGED THE WORST CASE. These are bound-arc cells, so they are
    // outside G1/G2's population but inside G4's universe: two engine_error
    // rows charge 2 * kDnfChargedSubproblems escapes over the same denominator.
    EXPECT_NE(text.find("G4 escape rate per QP           = 1.0000"), std::string::npos) << text;
    // The provenance header says these rows are fixtures.
    EXPECT_NE(runner_test::slurp(csv).find("# WARNING forced_child_throw"), std::string::npos);
    std::remove(csv.c_str());
    std::remove(log.c_str());
}

TEST(CorpusRunnerProcess, AnEngineErrorRowRoundTripsThroughFromCsvAndIsStillChargedWorstCase) {
    // The OFFLINE half: a committed artifact carrying an engine_error row
    // must re-score to the same charge, because --from-csv is how every later
    // reader (and the CorpusBaseline pins) reaches this verdict.
    const std::string csv = runner_test::temp_path("corpus_engine_error_offline.csv");
    const std::string log = runner_test::temp_path("corpus_engine_error_offline.log");
    {
        std::ofstream out(csv);
        out << "f7_n5000_path_warm,F7,5000,path,warm,0,engine_error,-1,-1,-1,-1,,-1.0,3600.0,"
               "unchecked,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1\n";
    }
    ASSERT_EQ(runner_test::run_binary(fmt::format("--from-csv {} --score-gates", csv), log), 0);
    const std::string text = runner_test::slurp(log);
    EXPECT_NE(text.find("ENGINE-ERROR rows = 1"), std::string::npos) << text;
    EXPECT_NE(text.find("G1 median factorizations per QP = 1000000.000"), std::string::npos)
        << "an engine that threw has not answered, and is charged exactly as a DNF is: " << text;
    EXPECT_NE(text.find("G2 p95 factorizations per QP    = 1000000.000"), std::string::npos)
        << text;
    std::remove(csv.c_str());
    std::remove(log.c_str());
}

TEST(CorpusRunnerProcess, ASignalKilledChildIsAHardErrorAndNeverAMeasurement) {
    // TASK 6. The engine_error path reads EXIT CODE 1 -- main()'s documented T6
    // exit -- and nothing else. A signal is a runner failure, and turning it
    // into a scored row would let a crash masquerade as a measurement.
    //
    // Nothing else in the suite reaches this branch: the wall deadline SIGKILLs
    // its child but returns the DNF outcome before ever inspecting a signal
    // status, so the mutant "treat ANY abnormal exit as engine_error" survived
    // the first mutation pass. This lever is what makes the rule fixturable.
    const std::string csv = runner_test::temp_path("corpus_child_abort.csv");
    EXPECT_NE(
        runner_test::run_binary(fmt::format("--engine walk --cells f7_n1000_bound_neutral --csv {} "
                                            "--internal-force-child-abort",
                                            csv),
                                runner_test::temp_path("corpus_child_abort.log")),
        0)
        << "a signal-killed child must fail the run, not produce an engine_error row";
    EXPECT_TRUE(runner_test::data_rows(csv).empty())
        << "and it must not write a scored row for the cell that crashed";
    std::remove(csv.c_str());
}

TEST(CorpusRunnerProcess, TheProxCarryLeverReachesTheSOLVEAndNotJustTheHeader) {
    // TASK 6. The stamp test above proves the PROVENANCE records the lever; it
    // does not prove the lever reached SqpOptions. The mutant that drops
    // `opts.ssn_prox_carry = cfg.ssn_prox_carry` from options_for_cell survived
    // the first mutation pass for a real reason: the carry is PROVABLY INERT on
    // any tiny fixture -- at N = 12 and N = 40 the proximal ladder never arms
    // (`ssn_prox_updates` is 0), so on and off are behaviourally identical.
    //
    // f7_n800_path_warm is the smallest census cell this task found on which
    // the lever is OBSERVABLE, and it costs 0.40 s end to end (setup hop
    // included) -- which is why this one test departs from the file's N = 12
    // norm, deliberately and with the cost stated.
    const std::string off = runner_test::temp_path("corpus_prox_off.csv");
    const std::string on = runner_test::temp_path("corpus_prox_on.csv");
    ASSERT_EQ(runner_test::run_binary(
                  fmt::format("--engine ssn --cells f7_n800_path_warm --csv {}", off)),
              0);
    ASSERT_EQ(runner_test::run_binary(fmt::format(
                  "--engine ssn --cells f7_n800_path_warm --csv {} --ssn-prox-carry", on)),
              0);
    const std::vector<std::string> a = runner_test::data_rows(off);
    const std::vector<std::string> b = runner_test::data_rows(on);
    ASSERT_EQ(a.size(), 1u);
    ASSERT_EQ(b.size(), 1u);
    const std::vector<std::string> ca = runner_test::split_all(a[0]);
    const std::vector<std::string> cb = runner_test::split_all(b[0]);
    EXPECT_NE(ca[7], cb[7]) << "factorizations must move: off=" << ca[7] << " on=" << cb[7];
    EXPECT_NE(ca[11], cb[11]) << "and so must the per-QP list";
    EXPECT_NE(ca[24], cb[24]) << "and the backtrack count, which is where the carry acts";
    std::remove(off.c_str());
    std::remove(on.c_str());
}

TEST(CorpusRunnerProcess, TheProxCarryLeverIsStampedIntoTheProvenanceHeader) {
    // TASK 6 measurement arm. ssn_prox_carry is a real shipped option that
    // ships OFF; a run that turns it on is an ARM and must never be mistaken
    // for a default-configuration artifact. Same discipline the hidden
    // wall-budget overrides already carry.
    const std::string csv = runner_test::temp_path("corpus_prox_arm.csv");
    ASSERT_EQ(runner_test::run_binary(fmt::format(
                  "--engine ssn --cells f7_n1000_bound_warm --csv {} --ssn-prox-carry", csv)),
              0);
    const std::string text = runner_test::slurp(csv);
    EXPECT_NE(text.find("# lever: ssn_prox_carry=true"), std::string::npos) << text;
    // AND THE CHILD REALLY RAN THE SSN KERNEL. The deadline machinery forks and
    // re-execs, so the engine name has to be FORWARDED to the child; Task 1
    // hard-coded "walk" there (correctly, since that was the only engine). A
    // mutation restoring that literal produces a row with every SSN counter at
    // zero, and this is what sees it.
    const std::vector<std::string> arm_rows = runner_test::data_rows(csv);
    ASSERT_EQ(arm_rows.size(), 1u);
    EXPECT_GT(std::stoi(runner_test::split_all(arm_rows[0])[22]), 0)
        << "ssn_iters must be nonzero: the child was asked for --engine ssn";
    // ...and a default run does NOT carry the line.
    const std::string plain = runner_test::temp_path("corpus_prox_default.csv");
    ASSERT_EQ(runner_test::run_binary(
                  fmt::format("--engine ssn --cells f7_n1000_bound_warm --csv {}", plain)),
              0);
    EXPECT_EQ(runner_test::slurp(plain).find("# lever: ssn_prox_carry"), std::string::npos);
    std::remove(csv.c_str());
    std::remove(plain.c_str());
}

TEST(CorpusRunnerProcess, FromCsvRefusesToBeCombinedWithARunRequest) {
    const std::string csv = runner_test::temp_path("corpus_conflict.csv");
    EXPECT_NE(runner_test::run_binary(
                  fmt::format("--from-csv {} --engine walk --cells f7_n1000_bound_neutral", csv)),
              0);
}

// =============================================================================
// PHASE-7 TASK 2 (PIQP oracle): --dump-qp, driven as a subprocess (the CLI
// wiring lives in bench_corpus.cpp, unreachable from a gtest linked only
// against corpus_cells.h).
// =============================================================================

namespace dump_test {

// A minimal, deliberately independent reader of write_qp_dump's own format
// (bench_corpus.cpp) -- independent so a mutation to the WRITER's section
// ordering or counts is caught by a reader that does not share its bug.
//
// FIX ROUND 1 (review I5). The first issue of this reader collected only
// dims/nnz-counts/lengths, never a single numeric value, index or sign --
// which the review demonstrated leaves a sign flip on `be`, a transposed
// `Ae` index pair, or a precision truncation completely undetected (PIQP
// solves the corrupted QP; `external_residual` scores it against the same
// corrupted data; the residual stays tiny either way). This version stores
// every triplet and every vector entry and the test below compares them
// value-by-value, index-by-index against `first_qp_for_cell`'s own
// `QpProblem` -- the actual in-process object the dump is supposed to be a
// byte-faithful text rendering of.
struct Triplet {
    Index row = 0, col = 0;
    double value = 0.0;
};

struct ParsedDump {
    Index n = 0, me = 0, mi = 0;
    std::vector<Triplet> h, ae, ai;
    std::vector<double> g, be, bi, lower, upper;
    bool ended = false;
};

std::vector<double> read_vec(std::ifstream &in, std::size_t count) {
    std::vector<double> out;
    out.reserve(count);
    std::string line;
    for (std::size_t i = 0; i < count; ++i) {
        std::getline(in, line);
        out.push_back(std::stod(line));
    }
    return out;
}

std::vector<Triplet> read_triplets(std::ifstream &in, std::size_t count) {
    std::vector<Triplet> out;
    out.reserve(count);
    std::string line;
    for (std::size_t i = 0; i < count; ++i) {
        std::getline(in, line);
        std::stringstream ss(line);
        Triplet t;
        ss >> t.row >> t.col >> t.value;
        out.push_back(t);
    }
    return out;
}

ParsedDump parse(const std::string &path) {
    std::ifstream in(path);
    ParsedDump out;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::stringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "PIQP_QP_DUMP" || tag == "cell" || tag == "taxonomy" || tag == "window" ||
            tag == "n_nodes") {
            continue;
        }
        std::size_t count = 0;
        if (tag == "n") {
            ss >> out.n;
        } else if (tag == "me") {
            ss >> out.me;
        } else if (tag == "mi") {
            ss >> out.mi;
        } else if (tag == "H_NNZ") {
            ss >> count;
            out.h = read_triplets(in, count);
        } else if (tag == "G_VEC") {
            ss >> count;
            out.g = read_vec(in, count);
        } else if (tag == "AE_NNZ") {
            ss >> count;
            out.ae = read_triplets(in, count);
        } else if (tag == "BE_VEC") {
            ss >> count;
            out.be = read_vec(in, count);
        } else if (tag == "AI_NNZ") {
            ss >> count;
            out.ai = read_triplets(in, count);
        } else if (tag == "BI_VEC") {
            ss >> count;
            out.bi = read_vec(in, count);
        } else if (tag == "LOWER_VEC") {
            ss >> count;
            out.lower = read_vec(in, count);
        } else if (tag == "UPPER_VEC") {
            ss >> count;
            out.upper = read_vec(in, count);
        } else if (tag == "END") {
            out.ended = true;
        }
    }
    return out;
}

// Canonicalizes a triplet list into a (row, col) -> value map, so a
// comparison against a `QpProblem`'s own sparse matrix does not depend on
// either side's iteration ORDER matching -- only on the SET of (row, col,
// value) triples agreeing, which is the actual claim "the dump is a
// faithful rendering of the QP" makes.
std::map<std::pair<Index, Index>, double> canonicalize(const std::vector<Triplet> &triplets) {
    std::map<std::pair<Index, Index>, double> out;
    for (const Triplet &t : triplets) {
        out[{t.row, t.col}] = t.value;
    }
    return out;
}

// Covers both `QpProblem::H` (SpMatRM = Eigen::SparseMatrix<double, RowMajor>)
// and `QpProblem::Ae`/`Ai` (the same underlying type) with one overload.
std::map<std::pair<Index, Index>, double>
canonicalize(const Eigen::SparseMatrix<double, Eigen::RowMajor> &m) {
    std::map<std::pair<Index, Index>, double> out;
    for (int k = 0; k < m.outerSize(); ++k) {
        for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(m, k); it; ++it) {
            out[{it.row(), it.col()}] = it.value();
        }
    }
    return out;
}

} // namespace dump_test

TEST(CorpusRunnerProcess, DumpQpWritesAParseableTripletFileMatchingTheCellsFirstQp) {
    const std::string out_path = runner_test::temp_path("corpus_dump.qp");
    ASSERT_EQ(runner_test::run_binary(
                  fmt::format("--dump-qp f7_n1000_bound_neutral --dump-qp-out {}", out_path)),
              0);
    const dump_test::ParsedDump parsed = dump_test::parse(out_path);
    EXPECT_TRUE(parsed.ended) << "the file must be terminated by its own END marker";

    const CorpusCell *cell = find_cell("f7_n1000_bound_neutral");
    ASSERT_NE(cell, nullptr);
    const hven::solvers::QpProblem expected = detail::first_qp_for_cell(*cell);

    EXPECT_EQ(parsed.n, expected.n());
    EXPECT_EQ(parsed.me, expected.me());
    EXPECT_EQ(parsed.mi, expected.mi());

    // Vectors: length AND every value, index-for-index.
    ASSERT_EQ(static_cast<Index>(parsed.g.size()), expected.n());
    ASSERT_EQ(static_cast<Index>(parsed.be.size()), expected.me());
    ASSERT_EQ(static_cast<Index>(parsed.bi.size()), expected.mi());
    ASSERT_EQ(static_cast<Index>(parsed.lower.size()), expected.n());
    ASSERT_EQ(static_cast<Index>(parsed.upper.size()), expected.n());
    for (Index i = 0; i < expected.n(); ++i) {
        EXPECT_DOUBLE_EQ(parsed.g[static_cast<std::size_t>(i)], expected.g(i)) << "g[" << i << "]";
        EXPECT_DOUBLE_EQ(parsed.lower[static_cast<std::size_t>(i)], expected.lower(i))
            << "lower[" << i << "]";
        EXPECT_DOUBLE_EQ(parsed.upper[static_cast<std::size_t>(i)], expected.upper(i))
            << "upper[" << i << "]";
    }
    for (Index i = 0; i < expected.me(); ++i) {
        EXPECT_DOUBLE_EQ(parsed.be[static_cast<std::size_t>(i)], expected.be(i))
            << "be[" << i << "]";
    }
    for (Index i = 0; i < expected.mi(); ++i) {
        EXPECT_DOUBLE_EQ(parsed.bi[static_cast<std::size_t>(i)], expected.bi(i))
            << "bi[" << i << "]";
    }

    // Matrices: the (row, col, value) SET, order-independent, sign and index
    // included -- catches a transposed pair or a sign flip the old dims/nnz
    // check could not.
    EXPECT_EQ(dump_test::canonicalize(parsed.h), dump_test::canonicalize(expected.H));
    EXPECT_EQ(dump_test::canonicalize(parsed.ae), dump_test::canonicalize(expected.Ae));
    EXPECT_EQ(dump_test::canonicalize(parsed.ai), dump_test::canonicalize(expected.Ai));
}

TEST(CorpusRunnerProcess, DumpQpRejectsAnUnknownCellId) {
    const std::string out_path = runner_test::temp_path("corpus_dump_bad.qp");
    EXPECT_NE(runner_test::run_binary(
                  fmt::format("--dump-qp not_a_real_cell --dump-qp-out {}", out_path)),
              0);
}

TEST(CorpusRunnerProcess, DumpQpRequiresDumpQpOut) {
    // Not just "nonzero exit" -- a caller who dropped the required-arg CHECK
    // (rather than the flag itself) can still exit nonzero via an unrelated
    // exception (e.g. dereferencing the unset optional), which would pass a
    // bare exit-code assertion for the wrong reason. Pin the CLEAN usage
    // message this project's own T6 convention requires.
    const std::string stdout_path = runner_test::temp_path("corpus_dump_no_out.stdout");
    EXPECT_NE(runner_test::run_binary("--dump-qp f7_n1000_bound_neutral", stdout_path), 0);
    const std::string output = runner_test::slurp(stdout_path);
    EXPECT_NE(output.find("--dump-qp requires --dump-qp-out"), std::string::npos) << output;
}

TEST(CorpusRunnerProcess, DumpQpCannotBeCombinedWithARunRequest) {
    const std::string out_path = runner_test::temp_path("corpus_dump_conflict.qp");
    EXPECT_NE(runner_test::run_binary(fmt::format(
                  "--dump-qp f7_n1000_bound_neutral --dump-qp-out {} --engine walk --cells "
                  "f7_n1000_bound_neutral",
                  out_path)),
              0);
}

// =============================================================================
// THE GATE ARITHMETIC -- hand-built vectors, no cell table in sight.
// =============================================================================

TEST(CorpusGates, MedianAndP95AreNearestRankOnASmallVector) {
    // {2, 4, 6, 8, 10, 12} -- median (nearest-rank, ceil(0.5*6)-1 = index 2)
    // = 6; p95 (ceil(0.95*6)-1 = index 5) = 12.
    const auto v = detail::gate_arithmetic({2, 4, 6, 8, 10, 12}, {}, 0, 0);
    EXPECT_DOUBLE_EQ(v.g1_median, 6.0);
    EXPECT_DOUBLE_EQ(v.g2_p95, 12.0);
}

TEST(CorpusGates, PercentileAtQuantileZeroDoesNotUnderflow) {
    // FIX ROUND 1 (M6). ceil(0 * n) - 1 underflows an unsigned index; the
    // gates only ever pass 0.5/0.95, but a silent wrap in a scored path is
    // not a thing to leave lying around.
    EXPECT_DOUBLE_EQ(detail::percentile({7.0, 1.0, 4.0}, 0.0), 1.0);
    EXPECT_DOUBLE_EQ(detail::percentile({}, 0.0), 0.0);
}

TEST(CorpusGates, G1G2PassAtTheExactThreshold) {
    const auto v = detail::gate_arithmetic({12}, {}, 0, 0);
    EXPECT_TRUE(v.pass[0]) << "median == 12 must PASS (<=12)";
    EXPECT_TRUE(v.pass[1]) << "p95 == 12 <= 25 must PASS";
}

TEST(CorpusGates, G2PassesAtExactlyTwentyFive) {
    // A single value makes median == p95, so the fixture above never
    // exercises G2's OWN threshold at its boundary. Two values separate them:
    // median (nearest-rank index 0 of 2) reads the smaller, p95 (index 1) the
    // larger -- pinned at exactly 25 here.
    const auto v = detail::gate_arithmetic({1, 25}, {}, 0, 0);
    EXPECT_DOUBLE_EQ(v.g2_p95, 25.0);
    EXPECT_TRUE(v.pass[1]) << "p95 == 25 must PASS (<=25)";
}

TEST(CorpusGates, G1FailsOneOverTheThreshold) {
    EXPECT_FALSE(detail::gate_arithmetic({13}, {}, 0, 0).pass[0]);
}

TEST(CorpusGates, G2FailsOneOverTheThreshold) {
    EXPECT_FALSE(detail::gate_arithmetic({26}, {}, 0, 0).pass[1]);
}

TEST(CorpusGates, G3GrowthIsMedianOfPairedDeltasAndPassesAtZeroOrNegative) {
    // NEAREST-RANK median (corpus_cells.h's own convention), not the averaged
    // midpoint: sorted deltas {-6, 0}, ceil(0.5*2)-1 = index 0 -> -6.
    const auto v = detail::gate_arithmetic({}, {0.0, -6.0}, 0, 0);
    EXPECT_DOUBLE_EQ(v.g3_growth, -6.0);
    EXPECT_TRUE(v.pass[2]);
}

TEST(CorpusGates, G3FailsOnPositiveMedianGrowth) {
    const auto v = detail::gate_arithmetic({}, {30.0}, 0, 0);
    EXPECT_DOUBLE_EQ(v.g3_growth, 30.0);
    EXPECT_FALSE(v.pass[2]);
}

TEST(CorpusGates, G3PassesAtExactlyZeroGrowth) {
    // "No median growth" is <= 0 -- pin the boundary so a mutant tightening
    // pass[2] to a strict '< 0.0' is caught here rather than only on the
    // (already negative) shrink fixture above.
    const auto v = detail::gate_arithmetic({}, {0.0}, 0, 0);
    EXPECT_DOUBLE_EQ(v.g3_growth, 0.0);
    EXPECT_TRUE(v.pass[2]) << "zero growth is 'no growth' and must PASS (<=0)";
}

TEST(CorpusGates, G4RateIsANumeratorOverADenominator) {
    const auto v = detail::gate_arithmetic({}, {}, 1.0, 3.0);
    EXPECT_DOUBLE_EQ(v.g4_escape_rate, 1.0 / 3.0);
    EXPECT_FALSE(v.pass[3]);
}

TEST(CorpusGates, G4FailsAtOrAboveTwoPercent) {
    const auto v = detail::gate_arithmetic({}, {}, 1.0, 50.0);
    EXPECT_DOUBLE_EQ(v.g4_escape_rate, 0.02);
    EXPECT_FALSE(v.pass[3]) << "the gate is a STRICT '< 2%'";
}

TEST(CorpusGates, EmptyInputsAreVacuouslyClean) {
    const auto v = detail::gate_arithmetic({}, {}, 0, 0);
    EXPECT_DOUBLE_EQ(v.g1_median, 0.0);
    EXPECT_DOUBLE_EQ(v.g2_p95, 0.0);
    EXPECT_DOUBLE_EQ(v.g3_growth, 0.0);
    EXPECT_DOUBLE_EQ(v.g4_escape_rate, 0.0);
    EXPECT_TRUE(v.pass[0]);
    EXPECT_TRUE(v.pass[1]);
    EXPECT_TRUE(v.pass[2]);
    EXPECT_TRUE(v.pass[3]);
}

// =============================================================================
// THE GATE POPULATIONS AND THE DNF CHARGE (fix round 1, I3/I4/C1/C3/I5). This
// is the half of the verdict that used to live in untested runner glue.
// =============================================================================

namespace gate_population_test {

// Cells are addressed by POINTER in CorpusOutcome, so fixtures keep their own
// storage alive in a deque (stable addresses).
std::deque<CorpusCell> &cell_storage() {
    static std::deque<CorpusCell> cells;
    return cells;
}

const CorpusCell *make_cell(Index n_nodes, ConstraintFamily ctag, StartTaxonomy start,
                            bool degenerate = false) {
    cell_storage().push_back(CorpusCell{"fixture", BenchFamily::kF7, n_nodes, 0.80, 0.85,
                                        /*step_index=*/0, start, ctag, degenerate});
    return &cell_storage().back();
}

CorpusOutcome finished(const CorpusCell *cell, std::vector<int> per_qp, int escapes = 0) {
    CorpusOutcome o;
    o.cell = cell;
    o.row.cell_id = cell->id;
    o.row.status = hven::solvers::SqpStatus::kOptimal;
    o.row.escapes = escapes;
    o.row.qp_factorizations = std::move(per_qp);
    for (const int f : o.row.qp_factorizations) {
        o.row.factorizations += f;
    }
    return o;
}

CorpusOutcome dnf(const CorpusCell *cell, DnfPhase phase = DnfPhase::kSolve) {
    CorpusOutcome o;
    o.cell = cell;
    o.dnf_phase = phase;
    o.dnf_wall_s = 3600.0;
    return o;
}

} // namespace gate_population_test

TEST(CorpusGatePopulation, G1G2ReadPathInterfaceWarmAndActivityCellsOnly) {
    // PRE-REGISTRATION P1, tested as a predicate so the ruling is pinned
    // independently of any verdict arithmetic.
    using namespace gate_population_test;
    EXPECT_TRUE(in_g1_g2_population(
        *make_cell(1000, ConstraintFamily::kPathInterface, StartTaxonomy::kFullWarm)));
    EXPECT_TRUE(in_g1_g2_population(
        *make_cell(1000, ConstraintFamily::kPathInterface, StartTaxonomy::kActivityOnly)));
    EXPECT_FALSE(in_g1_g2_population(
        *make_cell(1000, ConstraintFamily::kPathInterface, StartTaxonomy::kNeutralCold)))
        << "cold rows are not a WARM-MODE statistic";
    EXPECT_FALSE(in_g1_g2_population(
        *make_cell(1000, ConstraintFamily::kPathInterface, StartTaxonomy::kPhysicsInformed)));
    EXPECT_FALSE(in_g1_g2_population(
        *make_cell(1000, ConstraintFamily::kPathInterface, StartTaxonomy::kCorrupted)))
        << "a deliberately damaged hand-off is out of contract, like a degenerate cell";
    EXPECT_FALSE(in_g1_g2_population(
        *make_cell(1000, ConstraintFamily::kBoundArc, StartTaxonomy::kFullWarm)))
        << "bound-arc cells are not path-constraint-heavy";
    EXPECT_FALSE(in_g1_g2_population(*make_cell(2000, ConstraintFamily::kPathInterface,
                                                StartTaxonomy::kFullWarm, /*degenerate=*/true)))
        << "intentionally-degenerate cells are excluded";
}

TEST(CorpusGatePopulation, G1G2PoolPerQpValuesAndIgnoreOutOfPopulationRows) {
    using namespace gate_population_test;
    const std::vector<CorpusOutcome> outcomes{
        finished(make_cell(1000, ConstraintFamily::kPathInterface, StartTaxonomy::kFullWarm),
                 {2, 4}),
        finished(make_cell(1000, ConstraintFamily::kPathInterface, StartTaxonomy::kActivityOnly),
                 {6, 8, 10}),
        // Out of population, and enormous: if either leaked in, the p95 would
        // move to 9999.
        finished(make_cell(1000, ConstraintFamily::kPathInterface, StartTaxonomy::kNeutralCold),
                 {9999}),
        finished(make_cell(1000, ConstraintFamily::kBoundArc, StartTaxonomy::kFullWarm), {9999}),
    };
    const auto v = evaluate_gates(outcomes);
    // Pool = {2,4,6,8,10}: median index ceil(0.5*5)-1 = 2 -> 6; p95 index
    // ceil(0.95*5)-1 = 4 -> 10.
    EXPECT_DOUBLE_EQ(v.g1_median, 6.0);
    EXPECT_DOUBLE_EQ(v.g2_p95, 10.0);
    EXPECT_TRUE(v.pass[0]);
    EXPECT_TRUE(v.pass[1]);
}

TEST(CorpusGatePopulation, ADnfIsScoredAsTheWorstCaseNotSkipped) {
    // PRE-REGISTRATION P3, and the headline of C1: the first issue of this
    // task dropped DNF rows before building any population, which made "do
    // not finish" the cheapest way to pass three of the four gates.
    using namespace gate_population_test;
    const std::vector<CorpusOutcome> outcomes{
        finished(make_cell(1000, ConstraintFamily::kPathInterface, StartTaxonomy::kFullWarm), {1}),
        dnf(make_cell(1000, ConstraintFamily::kPathInterface, StartTaxonomy::kActivityOnly)),
    };
    const auto v = evaluate_gates(outcomes);
    EXPECT_DOUBLE_EQ(v.g1_median, static_cast<double>(kDnfFactorizationSentinel))
        << "one finishing QP against ten charged ones -- the median is the sentinel";
    EXPECT_DOUBLE_EQ(v.g2_p95, static_cast<double>(kDnfFactorizationSentinel));
    EXPECT_FALSE(v.pass[0]);
    EXPECT_FALSE(v.pass[1]);
}

TEST(CorpusGatePopulation, BothDnfPhasesScoreIdentically) {
    // `dnf_setup` vs `dnf_budget` is a REPORTING distinction: the budget was
    // exhausted either way and the cell produced no row.
    using namespace gate_population_test;
    const CorpusCell *cell =
        make_cell(1000, ConstraintFamily::kPathInterface, StartTaxonomy::kFullWarm);
    const auto solve_phase = evaluate_gates({dnf(cell, DnfPhase::kSolve)});
    const auto setup_phase = evaluate_gates({dnf(cell, DnfPhase::kSetup)});
    EXPECT_DOUBLE_EQ(solve_phase.g1_median, setup_phase.g1_median);
    EXPECT_DOUBLE_EQ(solve_phase.g4_escape_rate, setup_phase.g4_escape_rate);
}

TEST(CorpusGatePopulation, ATimingOutEngineNeverOutscoresAFinishingOne) {
    // The controller's own statement of C1, tested comparatively: take one
    // corpus, replace ONE finishing row with a DNF, and no gate may improve.
    using namespace gate_population_test;
    const CorpusCell *warm =
        make_cell(1000, ConstraintFamily::kPathInterface, StartTaxonomy::kFullWarm);
    const CorpusCell *activity =
        make_cell(1000, ConstraintFamily::kPathInterface, StartTaxonomy::kActivityOnly);
    const auto finishing = evaluate_gates({finished(warm, {3, 3}), finished(activity, {40, 40})});
    const auto timing_out = evaluate_gates({finished(warm, {3, 3}), dnf(activity)});

    EXPECT_GE(timing_out.g1_median, finishing.g1_median);
    EXPECT_GE(timing_out.g2_p95, finishing.g2_p95);
    EXPECT_GE(timing_out.g4_escape_rate, finishing.g4_escape_rate);
    for (int g = 0; g < 4; ++g) {
        EXPECT_FALSE(timing_out.pass[g] && !finishing.pass[g])
            << "gate " << (g + 1) << ": timing out must never turn a fail into a pass";
    }
}

TEST(CorpusGatePopulation, G3PairsMatchOnWindowAndTaxonomyAcrossTheTwoMeshes) {
    using namespace gate_population_test;
    const std::vector<CorpusOutcome> outcomes{
        finished(make_cell(5000, ConstraintFamily::kPathInterface, StartTaxonomy::kFullWarm), {10}),
        finished(make_cell(20000, ConstraintFamily::kPathInterface, StartTaxonomy::kFullWarm),
                 {10}),
        // A DIFFERENT taxonomy at N = 20000: must not pair with the warm
        // N = 5000 row above.
        finished(make_cell(20000, ConstraintFamily::kPathInterface, StartTaxonomy::kNeutralCold),
                 {900}),
    };
    const auto v = evaluate_gates(outcomes);
    EXPECT_DOUBLE_EQ(v.g3_growth, 0.0) << "one matched pair, zero growth";
    EXPECT_TRUE(v.pass[2]);
}

TEST(CorpusGatePopulation, G3ExcludesEmptyWindowPairsNoEngineCanFail) {
    // PRE-REGISTRATION P5. A bound-arc pair costs exactly one factorization
    // per QP at every N for every engine (the working set never changes
    // there), so admitting them lets structurally-flat pairs outvote measured
    // ones: on this task's own walk baseline the unrestricted reading reports
    // "no growth PASS" while ALL FIVE path pairs are DNF-at-N=20000.
    using namespace gate_population_test;
    const std::vector<CorpusOutcome> outcomes{
        finished(make_cell(5000, ConstraintFamily::kBoundArc, StartTaxonomy::kNeutralCold), {1}),
        finished(make_cell(20000, ConstraintFamily::kBoundArc, StartTaxonomy::kNeutralCold), {1}),
        finished(make_cell(5000, ConstraintFamily::kPathInterface, StartTaxonomy::kNeutralCold),
                 {5}),
        dnf(make_cell(20000, ConstraintFamily::kPathInterface, StartTaxonomy::kNeutralCold)),
    };
    const auto v = evaluate_gates(outcomes);
    EXPECT_DOUBLE_EQ(v.g3_growth, static_cast<double>(kDnfFactorizationSentinel))
        << "the one path pair is the whole population; the flat bound-arc pair must not dilute it";
    EXPECT_FALSE(v.pass[2]);
    EXPECT_TRUE(in_g3_population(
        *make_cell(5000, ConstraintFamily::kPathInterface, StartTaxonomy::kNeutralCold)))
        << "P5 keeps every path taxonomy, warm-mode or not -- G3's own text does not say "
           "warm-mode";
    EXPECT_FALSE(in_g3_population(*make_cell(2000, ConstraintFamily::kPathInterface,
                                             StartTaxonomy::kNeutralCold, /*degenerate=*/true)));
}

TEST(CorpusGatePopulation, G3TreatsADnfOnEitherSideAsGrowth) {
    // PRE-REGISTRATION P3: a pair that cannot be measured at both sizes is
    // not evidence of "no growth". The first issue dropped such pairs, which
    // left G3 reporting 0.000 PASS having measured nothing but zero-work
    // cells.
    using namespace gate_population_test;
    const CorpusCell *small =
        make_cell(5000, ConstraintFamily::kPathInterface, StartTaxonomy::kFullWarm);
    const CorpusCell *large =
        make_cell(20000, ConstraintFamily::kPathInterface, StartTaxonomy::kFullWarm);
    for (const bool dnf_the_large_side : {true, false}) {
        SCOPED_TRACE(dnf_the_large_side ? "N=20000 DNF" : "N=5000 DNF");
        const std::vector<CorpusOutcome> outcomes{
            dnf_the_large_side ? finished(small, {5}) : dnf(small),
            dnf_the_large_side ? dnf(large) : finished(large, {5}),
        };
        const auto v = evaluate_gates(outcomes);
        EXPECT_DOUBLE_EQ(v.g3_growth, static_cast<double>(kDnfFactorizationSentinel));
        EXPECT_FALSE(v.pass[2]);
    }
}

TEST(CorpusGatePopulation, G4CountsEscapesPerQpSubproblemNotPerRow) {
    // PRE-REGISTRATION P4 / I3. The first issue counted ROWS with any escape
    // over non-degenerate ROWS, whose smallest expressible nonzero rate on
    // the real corpus was 1/36 = 2.78% -- coarser than the 2% threshold it
    // was testing.
    using namespace gate_population_test;
    std::vector<CorpusOutcome> outcomes;
    // 49 clean subproblems spread over rows that build several QPs each,
    // plus one escape: 1/50 = 2% exactly, the strict boundary.
    outcomes.push_back(
        finished(make_cell(1000, ConstraintFamily::kBoundArc, StartTaxonomy::kNeutralCold),
                 std::vector<int>(49, 1)));
    outcomes.push_back(
        finished(make_cell(1000, ConstraintFamily::kBoundArc, StartTaxonomy::kFullWarm), {1},
                 /*escapes=*/1));
    const auto v = evaluate_gates(outcomes);
    EXPECT_DOUBLE_EQ(v.g4_escape_rate, 0.02) << "50 subproblems, one escape";
    EXPECT_FALSE(v.pass[3]);
}

TEST(CorpusGatePopulation, G4ExcludesDegenerateCellsAndChargesADnfAsAllEscaping) {
    using namespace gate_population_test;
    const CorpusOutcome degenerate_escaper =
        finished(make_cell(2000, ConstraintFamily::kPathInterface, StartTaxonomy::kFullWarm,
                           /*degenerate=*/true),
                 {1}, /*escapes=*/1);
    const CorpusOutcome clean =
        finished(make_cell(1000, ConstraintFamily::kBoundArc, StartTaxonomy::kNeutralCold),
                 std::vector<int>(90, 1));
    EXPECT_DOUBLE_EQ(evaluate_gates({clean, degenerate_escaper}).g4_escape_rate, 0.0)
        << "the degenerate cell's escape is outside the gate's own universe";

    const auto with_dnf = evaluate_gates(
        {clean, dnf(make_cell(1000, ConstraintFamily::kBoundArc, StartTaxonomy::kFullWarm))});
    EXPECT_DOUBLE_EQ(with_dnf.g4_escape_rate, 10.0 / 100.0)
        << "a DNF is charged kDnfChargedSubproblems QPs, all escaping";
    EXPECT_FALSE(with_dnf.pass[3]);
}

// =============================================================================
// TASK 6: THE MODEL-LEVEL KKT GATE, AS ARITHMETIC. Hand-built rows, no solve
// anywhere -- the rule W1-W3 states, pinned independently of any cell.
// =============================================================================

namespace kkt_gate_test {

CorpusRow row_with(double stat, double primal, double sign, double comp, double dual_scale = 1.0,
                   double x_scale = 1.0,
                   hven::solvers::SqpStatus status = hven::solvers::SqpStatus::kOptimal) {
    CorpusRow r{};
    r.cell_id = "fixture";
    r.status = status;
    r.kkt_stationarity = stat;
    r.kkt_primal = primal;
    r.kkt_dual_sign = sign;
    r.kkt_complementarity = comp;
    r.dual_scale = dual_scale;
    r.x_scale = x_scale;
    return r;
}

} // namespace kkt_gate_test

TEST(CorpusKktGate, AClaimedOptimumWithACleanQuadrupleIsOk) {
    using namespace kkt_gate_test;
    EXPECT_EQ(kkt_gate_verdict(row_with(1e-10, 1e-10, 0.0, 1e-10)), KktVerdict::kOk);
}

TEST(CorpusKktGate, EachOfTheThreeGatedQuantitiesCanFailOnItsOwn) {
    // W2. THREE gated quantities, and complementarity is the one the whole
    // requirement was written for (Task-5's kSsn certified at 0.631; Phase-5
    // Task 7's warm solve certified kOptimal at a non-complementary point).
    using namespace kkt_gate_test;
    EXPECT_EQ(kkt_gate_verdict(row_with(1e-3, 1e-10, 0.0, 1e-10)), KktVerdict::kWrong)
        << "stationarity";
    EXPECT_EQ(kkt_gate_verdict(row_with(1e-10, 1e-3, 0.0, 1e-10)), KktVerdict::kWrong)
        << "primal feasibility";
    EXPECT_EQ(kkt_gate_verdict(row_with(1e-10, 1e-10, 0.0, 1e-3)), KktVerdict::kWrong)
        << "COMPLEMENTARITY -- the conjunct NF-2 exists for";
}

TEST(CorpusKktGate, DualSignIsMeasuredAndReportedButIsNOTGATED) {
    // W2's deliberate split (re-review NF-1 asked Task 6 to MEASURE dual sign
    // at scale; NF-2 asked it to GATE stationarity/feasibility/complementarity).
    // A row with a large sign violation and three clean conjuncts is `ok` AND
    // flags the counterfactual -- both halves matter: dropping the first turns
    // a telemetry request into an unratified gate, dropping the second hides
    // the finding.
    using namespace kkt_gate_test;
    // WHY THE FIXTURE VALUE IS 1e-3 AND NOT A CORPUS READING. Most of the
    // battery's dual-sign readings (2.3e-07 .. 8.2e-07 on 12 of the 14 rows
    // that carry any) are BELOW the gate's own 1e-6 bound, so they cannot
    // discriminate "dual sign is not gated" from "dual sign is gated at 1e-6"
    // -- a mutant adding the conjunct survives them. Two rows DO exceed 1e-6
    // (f7_n20000_path_activity at 3.5075e-06 and f7_n800_path_physics at
    // 1.0739e-06; corrected in fix round 1, the earlier comment here quoted
    // 8.2e-07 as "the corpus's worst measured reading", which was a
    // partial-sweep figure and is neither the maximum nor the row it named),
    // but a fixture pinned to either of those would be discriminating by a
    // margin of 1.07x -- a hair's breadth from equivalence. The fixture
    // therefore uses a violation three orders ABOVE the bound: under the
    // shipped rule this row is `ok` because dual sign is not a conjunct AT
    // ALL, and no threshold choice can make that reading accidental.
    const CorpusRow gross = row_with(1e-10, 1e-10, 1e-3, 1e-10);
    EXPECT_EQ(kkt_gate_verdict(gross), KktVerdict::kOk)
        << "dual feasibility is TELEMETRY (NF-1), not a gated conjunct (NF-2's three)";
    EXPECT_TRUE(dual_sign_would_fail(gross));
    // ...and a TYPICAL corpus reading (8.2e-07, f7_n750_path_neutral_control)
    // is inside the gate and outside the HS battery's 1e-9, which is exactly
    // what makes the counterfactual worth reporting rather than academic. It
    // is NOT the corpus maximum -- see the fix-round-1 note above, and the
    // row below, which is.
    const CorpusRow measured = row_with(1e-10, 1e-10, 8.2e-7, 1e-10);
    EXPECT_EQ(kkt_gate_verdict(measured), KktVerdict::kOk);
    EXPECT_TRUE(dual_sign_would_fail(measured))
        << "at the HS battery's own 1e-9 this row would be a wrong answer, and Task 6 reports so";
    EXPECT_FALSE(dual_sign_would_fail(row_with(1e-10, 1e-10, 1e-12, 1e-10)));
    // THE CORPUS MAXIMUM, pinned in fix round 1 so it cannot go stale again:
    // f7_n20000_path_activity reads 3.5075e-06 at dual_scale = 1.0, which is
    // ABOVE the gate's own 1e-6 bound -- and the shipped rule still calls the
    // row `ok`, because dual sign is not one of W2's three conjuncts. That is
    // the whole of docket D2: the reading is telemetry, and it crosses the
    // threshold the gate would use if it ever became a conjunct.
    const CorpusRow corpus_max = row_with(1e-10, 1e-10, 3.5075e-6, 1e-10);
    EXPECT_EQ(kkt_gate_verdict(corpus_max), KktVerdict::kOk)
        << "the corpus's WORST dual-sign row is still `ok` -- dual sign is not gated";
    EXPECT_GT(corpus_max.kkt_dual_sign, 1e-6 * std::max(1.0, corpus_max.dual_scale))
        << "...and it is above the gate's own 1e-6, which is the finding D2 rules on";
    EXPECT_TRUE(dual_sign_would_fail(corpus_max));
}

TEST(CorpusKktGate, TheThresholdIsRELATIVEToTheRowsOwnScale) {
    // W3. The relative form is the SAFE rule, not a load-bearing one on this
    // corpus: every gated row of the committed battery reads
    // dual_scale = x_scale = 1.0, so the gate reduces to an absolute 1e-6
    // there and no committed row is decided by the denominator (fix round 1;
    // battery note §8.1). This test exercises the rule's intent with a
    // SYNTHETIC dual_scale = 1e6 row: under a genuinely large multiplier a
    // point feasible to 1e-8 carries |lambda * cI| ~ 1e-2, which the relative
    // form accepts and an absolute form would reject. The two denominators
    // stay DIFFERENT on purpose -- a large multiplier must not buy slack on
    // CONSTRAINT VIOLATION.
    using namespace kkt_gate_test;
    EXPECT_EQ(kkt_gate_verdict(row_with(1e-10, 1e-10, 0.0, 1e-2, /*dual_scale=*/1e6)),
              KktVerdict::kOk);
    EXPECT_EQ(kkt_gate_verdict(row_with(1e-10, 1e-10, 0.0, 1e-2, /*dual_scale=*/1.0)),
              KktVerdict::kWrong);
    EXPECT_EQ(kkt_gate_verdict(row_with(1e-10, 1e-2, 0.0, 1e-10, /*dual_scale=*/1e6)),
              KktVerdict::kWrong)
        << "PRIMAL feasibility is scaled by ||x||, never by ||lambda||";
    EXPECT_EQ(kkt_gate_verdict(row_with(1e-10, 1e-2, 0.0, 1e-10, /*dual_scale=*/1.0,
                                        /*x_scale=*/1e6)),
              KktVerdict::kOk);
}

TEST(CorpusKktGate, OnlyRowsThatCLAIMOptimalAreJudged) {
    // W1. An honest failure exit claims nothing about the point it returns;
    // re-checking it would manufacture wrong answers out of honest errors.
    using namespace kkt_gate_test;
    for (const hven::solvers::SqpStatus st :
         {hven::solvers::SqpStatus::kMaxIter, hven::solvers::SqpStatus::kNumericalError,
          hven::solvers::SqpStatus::kInfeasible, hven::solvers::SqpStatus::kBudgetExhausted}) {
        EXPECT_EQ(kkt_gate_verdict(row_with(1e3, 1e3, 1e3, 1e3, 1.0, 1.0, st)),
                  KktVerdict::kUnchecked);
    }
    // And a row with NO recorded check (a Task-1-era 14-column artifact) is
    // unchecked, never wrong: absence of evidence is not evidence.
    CorpusRow bare{};
    bare.status = hven::solvers::SqpStatus::kOptimal;
    EXPECT_EQ(kkt_gate_verdict(bare), KktVerdict::kUnchecked);
}

TEST(CorpusGatePopulation, AWrongAnswerRowIsChargedTheWorstCaseExactlyAsADnfIs) {
    // W5. The rule is P3's with one word changed, and this is the assertion
    // that a fast wrong answer cannot buy a gate.
    using namespace gate_population_test;
    using namespace kkt_gate_test;
    const CorpusCell *c =
        make_cell(5000, ConstraintFamily::kPathInterface, StartTaxonomy::kFullWarm);
    CorpusOutcome good = finished(c, {1, 1, 1});
    good.row.status = hven::solvers::SqpStatus::kOptimal;
    good.row.kkt_stationarity = 1e-10;
    good.row.kkt_primal = 1e-10;
    good.row.kkt_dual_sign = 0.0;
    good.row.kkt_complementarity = 1e-10;
    good.row.dual_scale = 1.0;
    good.row.x_scale = 1.0;
    EXPECT_FALSE(good.wrong_answer());
    EXPECT_LE(evaluate_gates({good}).g1_median, 12.0);

    CorpusOutcome bad = good;
    bad.row.kkt_complementarity = 1.0; // certified, and not a KKT point
    EXPECT_TRUE(bad.wrong_answer());
    const GateVerdict v = evaluate_gates({bad});
    EXPECT_DOUBLE_EQ(v.g1_median, static_cast<double>(kDnfFactorizationSentinel));
    EXPECT_DOUBLE_EQ(v.g2_p95, static_cast<double>(kDnfFactorizationSentinel));
    EXPECT_FALSE(v.pass[0]);
    EXPECT_FALSE(v.pass[1]);
    // G4 charges it as all-escaping, exactly as a DNF is charged.
    EXPECT_DOUBLE_EQ(v.g4_escape_rate, 1.0);
}

TEST(CorpusGatePopulation, AWrongAnsweringEngineNeverOutscoresACorrectOne) {
    // The comparative form, which is the one that cannot be satisfied by a
    // sentinel that merely happens to be large.
    using namespace gate_population_test;
    const CorpusCell *a =
        make_cell(5000, ConstraintFamily::kPathInterface, StartTaxonomy::kFullWarm);
    const CorpusCell *b =
        make_cell(20000, ConstraintFamily::kPathInterface, StartTaxonomy::kFullWarm);
    auto correct = [](CorpusOutcome o) {
        o.row.kkt_stationarity = 1e-10;
        o.row.kkt_primal = 1e-10;
        o.row.kkt_dual_sign = 0.0;
        o.row.kkt_complementarity = 1e-10;
        o.row.dual_scale = 1.0;
        o.row.x_scale = 1.0;
        return o;
    };
    std::vector<CorpusOutcome> honest{correct(finished(a, {2, 2})), correct(finished(b, {2, 2}))};
    std::vector<CorpusOutcome> cheating = honest;
    cheating[1].row.qp_factorizations = {1};   // faster...
    cheating[1].row.kkt_complementarity = 1.0; // ...and not an answer
    const GateVerdict h = evaluate_gates(honest);
    const GateVerdict c = evaluate_gates(cheating);
    EXPECT_LE(h.g1_median, c.g1_median);
    EXPECT_LE(h.g2_p95, c.g2_p95);
    EXPECT_LE(h.g3_growth, c.g3_growth);
    EXPECT_LE(h.g4_escape_rate, c.g4_escape_rate);
}

TEST(CorpusGatePopulation, G1G2ReportBothKCorruptedReadingsFromTheSameRows) {
    // corpus-design.md section 5.1's REQUIREMENT on this task: P1's kCorrupted
    // exclusion is a post-hoc LOOSENING that moves the bar in the scored
    // engine's favour, so both readings come out of the same rows and Grant
    // ratifies against the numbers.
    using namespace gate_population_test;
    const CorpusCell *warm =
        make_cell(1000, ConstraintFamily::kPathInterface, StartTaxonomy::kFullWarm);
    const CorpusCell *corrupted =
        make_cell(1000, ConstraintFamily::kPathInterface, StartTaxonomy::kCorrupted);
    EXPECT_FALSE(in_g1_g2_population(*corrupted))
        << "the DEFAULT reading is the pre-registered one";
    EXPECT_TRUE(in_g1_g2_population(*corrupted, /*include_corrupted=*/true));
    EXPECT_TRUE(in_g1_g2_population(*warm, /*include_corrupted=*/true))
        << "widening the predicate must not narrow it anywhere else";

    const std::vector<CorpusOutcome> outcomes{finished(warm, {1, 1}),
                                              finished(corrupted, {2974, 1, 1})};
    EXPECT_DOUBLE_EQ(evaluate_gates(outcomes).g1_median, 1.0);
    EXPECT_DOUBLE_EQ(evaluate_gates(outcomes, /*include_corrupted=*/true).g1_median, 1.0);
    EXPECT_DOUBLE_EQ(evaluate_gates(outcomes).g2_p95, 1.0);
    EXPECT_DOUBLE_EQ(evaluate_gates(outcomes, /*include_corrupted=*/true).g2_p95, 2974.0)
        << "the excluded rows are the expensive TAIL of the warm family -- that is exactly why "
           "the exclusion needed disclosing";
}

// =============================================================================
// THE COMMITTED BASELINE, SCORED OFFLINE (fix round 1, I6). This is the
// artifact every later task cites; re-scoring it through the same evaluator
// Task 6 will use pins BOTH the artifact and the evaluator against drift, and
// costs milliseconds because nothing is re-solved.
// =============================================================================

TEST(CorpusBaseline, TheCommittedWalkBaselineScoresToItsDocumentedVerdict) {
    const std::string log = runner_test::temp_path("corpus_baseline_score.log");
    ASSERT_EQ(runner_test::run_binary(
                  fmt::format("--from-csv {} --score-gates", HVEN_SQP_CORPUS_BASELINE_CSV), log),
              0)
        << "the committed baseline must re-score offline without re-running a cell";
    const std::string text = runner_test::slurp(log);
    // Pinned verbatim from docs/notes/2026-08-06-corpus-design.md section 6.5.
    // The walk engine fails all four of its own gates on the repaired
    // instrument (it passed three of four on the first issue's) -- that is the
    // instrument being discriminating, not a claim about SSN.
    //
    // U0 (2026-08-16): the define now points at the U0-re-derived baseline
    // (see tests/sqp/CMakeLists.txt). EVERY figure below was re-verified
    // against the new artifact and is UNCHANGED, because the U0 census moved
    // no counter and no status on any of the 57 cells -- the entire
    // old-vs-new delta is kkt_residual last digits, which no gate reads.
    // The evaluator's documented-verdict constants therefore carry over
    // bit-for-bit ((c)3 of the U0 plan section: recomputed, compared, no
    // G-gate verdict OR figure moved).
    //
    // DECLARED RE-DERIVATION (CLAUDE.md section 7: intentional breaks of a
    // pinned value are declared and re-derived explicitly, never silent). The
    // baseline row f7_n10000_path_physics was amended at gate B per the
    // execution review's structured ruling -- dnf_budget with -1 counter
    // sentinels -> the fresh Optimal row -- both fresh rows byte-identical on
    // all 13 asserted columns; see docs/notes/2026-08-14-m3-gate-b-review-request.md
    // and the gate-A note's review-outcome postscript
    // (docs/notes/2026-08-14-m3-gate-a-review-request.md), landed in a1b42ca.
    // Exactly two of the pinned figures below move with that one cell, and
    // both were re-derived by re-scoring the amended file offline:
    //   DNF rows                11 -> 10   (the amended cell no longer DNFs)
    //   G4 escape rate per QP   0.6667 -> 0.6369
    // Every other figure is untouched: the amended cell is `physics`, so it was
    // never in the G1/G2 warm|activity population, and n = 10000 is outside G3's
    // 5000->20000 pair. The verdict itself is unchanged -- all four gates still
    // fail.
    for (const char *expected :
         {"rows = 57", "DNF rows = 10",
          "G1/G2 population (path-interface, warm|activity, non-degenerate) = 8 cells",
          "G1 median factorizations per QP = 1000000.000  (<=12)  fail",
          "G2 p95 factorizations per QP    = 1000000.000  (<=25)  fail",
          "G3 median growth 5000->20000    = 1000000.000  (<=0)   fail",
          "G4 escape rate per QP           = 0.6369  (<0.02) fail"}) {
        EXPECT_NE(text.find(expected), std::string::npos) << "missing: " << expected << "\n"
                                                          << text;
    }
    std::remove(log.c_str());
}

// =============================================================================
// PHASE-7 TASK 6: THE PHASE VERDICT, PINNED. Same pattern as the walk baseline
// above, extended to the gate battery's own two artifacts. A verdict that lives
// only in a note rots; re-scored here on every ctest invocation, it cannot.
// =============================================================================

TEST(CorpusBaseline, TheCommittedSsnBatteryScoresToItsDocumentedVerdict) {
    const std::string log = runner_test::temp_path("ssn_battery_score.log");
    ASSERT_EQ(runner_test::run_binary(
                  fmt::format("--from-csv {} --score-gates", HVEN_SQP_SSN_BATTERY_CSV), log),
              0)
        << "the committed kSsn battery must re-score offline without re-running a cell";
    const std::string text = runner_test::slurp(log);
    // Pinned verbatim from docs/notes/2026-08-08-ssn-gate-battery.md section 5.
    // ALL FOUR GATES FAIL on the shipped code. Three fail on NON-ANSWERS (four
    // engine_error rows -- the defect that note's section 7 documents -- and
    // four DNFs); G4 fails on its own terms as well as on the charge.
    for (const char *expected : {
             "rows = 57",
             "DNF rows = 8",
             "ENGINE-ERROR rows = 4",
             "KKT-gated rows = 45, WRONG-ANSWER rows = 0",
             "G1/G2 population (path-interface, warm|activity, non-degenerate) = 8 cells",
             "G1 median factorizations per QP = 1000000.000  (<=12)  fail",
             "G2 p95 factorizations per QP    = 1000000.000  (<=25)  fail",
             "G3 median growth 5000->20000    = 1000000.000  (<=0)   fail",
             "G4 escape rate per QP           = 0.6890  (<0.02) fail",
             // THE BOTH-WAYS kCorrupted READING, which Task 1's own section 5.1
             // made a REQUIREMENT of this task rather than an option.
             "--- G1/G2 WITH kCorrupted admitted (population = 12 cells) ---",
             // G4's measured figure, quoted separately from the charged one
             // exactly as the corpus-design note asked once the SSN arm had
             // real escapes to report. It fails the 2% threshold on its own.
             "G4 measured (finishing rows only) = 3 escapes / 54 QP subproblems = 0.0556",
             // NF-1's telemetry, pinned so the finding cannot quietly vanish:
             // the walk reads 0 on every row (see the arm below) and kSsn does
             // not.
             "dual sign: 14 row(s) carry a strictly negative inequality multiplier",
             "tier-3 refinement: 64 accepted / 6 refused, 70 factorization(s) of 770 total (9.09%)",
         }) {
        EXPECT_NE(text.find(expected), std::string::npos) << "missing: " << expected << "\n"
                                                          << text;
    }
    std::remove(log.c_str());
}

// -----------------------------------------------------------------------------
// THE ONE ADJUDICATED EXCEPTION, NAMED (CLAUDE.md section 7: intentional breaks
// of a pinned artifact are declared and re-derived explicitly, never silent).
//
// The committed walk baseline's f7_n10000_path_physics row was amended at gate B
// per the execution review's structured ruling -- dnf_budget with -1 counter
// sentinels -> the fresh Optimal row -- and BOTH fresh reproductions (gate A,
// serial and alone; gate B, run in the census's SOLO tier) are byte-identical on
// all 13 asserted columns, differing only in wall_s, which every comparator here
// excludes by construction. See docs/notes/2026-08-14-m3-gate-b-review-request.md
// and the gate-A note's review-outcome postscript
// (docs/notes/2026-08-14-m3-gate-a-review-request.md); landed in a1b42ca.
//
// The FROZEN artifacts below are evidence and are never edited: they still carry
// the pre-amendment dnf_budget row, correctly. The two cross-comparisons
// therefore carry this single-cell exception rather than a rewritten artifact --
// and it is a NAMED cell whose expected shape is asserted, not a silent skip, so
// the exception cannot quietly widen to cover a real counter regression. All 56
// other cells stay byte-strict on every asserted column.
// -----------------------------------------------------------------------------
namespace {

const std::set<std::string> kGateBAmendedCells = {"f7_n10000_path_physics"};

// The status column of both schemas (id, family, n, ctype, start, degenerate,
// status, ...), which is the entire content of the amendment.
constexpr std::size_t kStatusColumn = 6;

} // namespace

TEST(CorpusBaseline, TheReSweptWalkArmIsCounterIdenticalToTheFrozenPreU0Baseline) {
    // THE PRECONDITION THE WHOLE BATTERY RESTS ON. The walk census re-swept
    // through the binary that carries the Task-6 instrument must reproduce the
    // Task-1 baseline EXACTLY on every counter/status column. wall_s is
    // excluded and only wall_s: it is informational, and on a DNF row it is a
    // statement about the machine rather than about the solve.
    //
    // ...with the single gate-B adjudicated cell above excepted, since the
    // baseline was amended there and this frozen re-sweep was not.
    //
    // CONVERTED TO FROZEN-VS-FROZEN AT U0 (plan §6 U0(b)5, 2026-08-16,
    // renamed from ...ToTheCommittedBaseline): both sides of this comparison
    // are origin-era artifacts derived under the pre-U0 flag regime, and the
    // claim is a historical one about the Task-6 instrument being inert —
    // discharged on the gate-B record and kept assertable here against the
    // FROZEN pre-U0 baseline. The LIVE baseline of record is the U0
    // re-derivation (HVEN_SQP_CORPUS_BASELINE_CSV), which this frozen
    // comparison deliberately does not read: continuity between origin-era
    // artifacts and the live engine ended at the flag unification, by
    // design and by declaration.
    const auto base = runner_test::data_rows(HVEN_SQP_PRE_U0_WALK_BASELINE_CSV);
    const auto resweep = runner_test::data_rows(HVEN_SQP_WALK_RESWEPT_CSV);
    ASSERT_EQ(base.size(), 57u);
    ASSERT_EQ(resweep.size(), 57u);
    std::map<std::string, std::vector<std::string>> by_id;
    for (const std::string &r : resweep) {
        std::vector<std::string> col = runner_test::split_all(r);
        by_id[col[0]] = col;
    }
    int adjudicated = 0;
    for (const std::string &r : base) {
        const std::vector<std::string> b = runner_test::split_all(r);
        const auto it = by_id.find(b[0]);
        ASSERT_NE(it, by_id.end()) << "cell missing from the re-sweep: " << b[0];
        if (kGateBAmendedCells.count(b[0]) == 1) {
            ++adjudicated;
            // The exception is licensed in exactly ONE shape: the amended
            // baseline claims the fresh Optimal row, the frozen re-sweep still
            // claims the pre-amendment budget DNF. Any other disagreement on
            // this cell is a finding, and these two assertions are what report
            // it.
            EXPECT_EQ(b[kStatusColumn], "Optimal")
                << "the committed baseline no longer carries the gate-B amendment";
            EXPECT_EQ(it->second[kStatusColumn], "dnf_budget")
                << "the FROZEN re-sweep was edited -- it is evidence and must not be";
            continue;
        }
        // Columns 0..12: everything Task 1 measured except wall_s (13).
        for (std::size_t k = 0; k < 13; ++k) {
            EXPECT_EQ(b[k], it->second[k])
                << "cell " << b[0] << " column " << k << " moved between the committed baseline "
                << "and the re-sweep -- the instrument is NOT inert at qp_mode = kWalk";
        }
    }
    EXPECT_EQ(adjudicated, 1) << "exactly one cell is excepted; the other 56 are byte-strict";
}

// =====================================================================
// PHASE-7 TASK 6b -- THE POST-D0-REPAIR RE-SWEEP, PINNED ON BOTH ARMS.
//
// The repair (qp_engine.h's clause 6b: a free variable carries no bound
// price) has exactly two empirical claims, and both are diffs between
// committed artifacts rather than prose:
//
//   (1) THE WALK IS UNTOUCHED. The repair edits code the walk executes on
//       every solve, so "byte-identical at kWalk" has to be demonstrated on
//       real cells rather than argued from the diff.
//   (2) THE kSsn BLAST RADIUS IS THE FOUR CRASHING CELLS AND NOTHING ELSE.
//
// Both are asserted here, offline, in microseconds. If either artifact is
// ever regenerated, these fail and whoever regenerated it has to say why.
// =====================================================================

TEST(CorpusTask6bRepair, TheWalkArmIsCounterIdenticalAcrossTheD0Repair) {
    // Claim (1). Compared against BOTH committed walk artifacts -- Task 1's
    // baseline (schema 14) and Task 6's re-sweep (schema 31) -- because they
    // are themselves pinned equal, so agreeing with one and not the other
    // would be a contradiction this test should surface rather than hide.
    //
    // ...pinned equal EXCEPT on the one gate-B adjudicated cell named above,
    // where the baseline was amended and the two frozen artifacts were not.
    // The exception is taken against the BASELINE ref only: this post-repair
    // artifact and the Task-6 re-sweep are both frozen, both still carry the
    // pre-amendment row, and their comparison stays byte-strict on all 57
    // cells, which is what claim (1) actually rests on.
    const auto post = runner_test::data_rows(HVEN_SQP_TASK6B_WALK_CSV);
    ASSERT_EQ(post.size(), 57u);
    std::map<std::string, std::vector<std::string>> by_id;
    for (const std::string &r : post) {
        std::vector<std::string> col = runner_test::split_all(r);
        by_id[col[0]] = col;
    }
    int adjudicated = 0;
    // U0 (2026-08-16): the first ref is the FROZEN pre-U0 baseline, not the
    // live baseline of record — this test diffs three origin-era artifacts
    // against each other (frozen-vs-frozen; see the conversion note on the
    // test above). The D0-repair claim is a historical claim about those
    // artifacts and survives the flag unification untouched.
    for (const char *ref : {HVEN_SQP_PRE_U0_WALK_BASELINE_CSV, HVEN_SQP_WALK_RESWEPT_CSV}) {
        const bool ref_is_amended_baseline =
            std::string(ref) == HVEN_SQP_PRE_U0_WALK_BASELINE_CSV;
        const auto base = runner_test::data_rows(ref);
        ASSERT_EQ(base.size(), 57u) << ref;
        for (const std::string &r : base) {
            const std::vector<std::string> b = runner_test::split_all(r);
            const auto it = by_id.find(b[0]);
            ASSERT_NE(it, by_id.end()) << "cell missing from the re-sweep: " << b[0];
            if (ref_is_amended_baseline && kGateBAmendedCells.count(b[0]) == 1) {
                ++adjudicated;
                EXPECT_EQ(b[kStatusColumn], "Optimal")
                    << "the committed baseline no longer carries the gate-B amendment";
                EXPECT_EQ(it->second[kStatusColumn], "dnf_budget")
                    << "the FROZEN post-repair arm was edited -- it is evidence and must not be";
                continue;
            }
            // Every column `ref`'s OWN row carries, except wall_s (13, a
            // statement about the machine, not about the solve). This widens
            // to columns 14-30 on the schema-31 ref (final branch review,
            // WAVE #5) without hardcoding the bound: `b.size()` is 14 for the
            // baseline (columns 0-12 only) and 31 for the re-sweep.
            for (std::size_t k = 0; k < b.size(); ++k) {
                if (k == 13) {
                    continue;
                }
                EXPECT_EQ(b[k], it->second[k])
                    << ref << ": cell " << b[0] << " column " << k
                    << " moved across the D0 repair -- the repair is NOT inert at qp_mode = kWalk";
            }
        }
    }
    EXPECT_EQ(adjudicated, 1) << "exactly one (ref, cell) pair is excepted -- the baseline's "
                                 "amended row; every other comparison is byte-strict";
}

TEST(CorpusTask6bRepair, TheKSsnArmMovesTheFourCrashingCellsAndNothingElse) {
    // Claim (2). The four cells are named rather than derived, because the
    // point is that the SET is what was predicted -- a fifth cell moving
    // would be a finding, and this test is what would report it.
    const std::set<std::string> d0 = {"f7_n2000_path_neutral", "f7_n5000_path_neutral",
                                      "f7_n5000_path_corrupted", "f7_n5000_path_warm"};
    const auto shipped = runner_test::data_rows(HVEN_SQP_SSN_BATTERY_CSV);
    const auto post = runner_test::data_rows(HVEN_SQP_TASK6B_SSN_CSV);
    ASSERT_EQ(shipped.size(), 57u);
    ASSERT_EQ(post.size(), 57u);
    std::map<std::string, std::vector<std::string>> by_id;
    for (const std::string &r : post) {
        std::vector<std::string> col = runner_test::split_all(r);
        by_id[col[0]] = col;
    }
    int moved_cells = 0;
    for (const std::string &r : shipped) {
        const std::vector<std::string> b = runner_test::split_all(r);
        const auto it = by_id.find(b[0]);
        ASSERT_NE(it, by_id.end()) << b[0];
        bool moved = false;
        // Every SHIPPED column (0..30, minus wall_s): the schema-37 tail is
        // new and has no counterpart to compare against.
        for (std::size_t k = 0; k < 31; ++k) {
            if (k == 13) {
                continue; // wall_s
            }
            if (b[k] != it->second[k]) {
                moved = true;
                EXPECT_TRUE(d0.count(b[0]) == 1)
                    << "cell " << b[0] << " column " << k << " moved (" << b[k] << " -> "
                    << it->second[k] << ") and it is NOT one of the four D0 cells";
            }
        }
        if (moved) {
            ++moved_cells;
        }
    }
    EXPECT_EQ(moved_cells, 4) << "exactly the four D0 cells move";

    // AND THE DIRECTION OF THE MOVE: every one of the four stops throwing.
    for (const std::string &cid : d0) {
        EXPECT_NE(by_id.at(cid)[6], std::string(hven::solvers::corpus::kEngineErrorStatusString))
            << cid << " must no longer be an engine_error";
    }
    // ...and no cell that used to answer stopped answering, nor vice versa
    // beyond those four: the Optimal count is unchanged at 45.
    const auto count_optimal = [](const std::vector<std::string> &rows) {
        int n = 0;
        for (const std::string &r : rows) {
            if (runner_test::split_all(r)[6] == "Optimal") {
                ++n;
            }
        }
        return n;
    };
    EXPECT_EQ(count_optimal(shipped), 45);
    EXPECT_EQ(count_optimal(post), 45) << "the repair converted no cell into an answer";
}

// =====================================================================
// PHASE-7 TASK 6b PHASE B -- THE LIVE BYTE-IDENTITY DETECTOR
// =====================================================================
//
// The three tests above diff COMMITTED ARTIFACTS against each other, which is
// exactly what is wanted for a repair whose evidence is two sweeps. It is NOT
// what is wanted for an OPT-IN LEVER: an artifact diff cannot notice a lever
// leaking into the default path, because both artifacts predate the lever.
//
// This test re-runs two post-repair cells IN PROCESS at the SHIPPED kSsn
// configuration and compares every counter column against the committed
// post-repair row. It is the detector for Phase B's central discipline -- the
// shipped kSsn configuration must be byte-identical on the post-repair
// artifacts with four new option surfaces in the header.
//
// **THE TWO CELLS ARE BOTH `neutral`, AND THAT IS A COST CHOICE STATED AS
// ONE.** A corpus cell's in-process run includes its SETUP HOP, which for the
// `warm`/`corrupted`/`activity` taxonomies means solving a whole preceding
// problem -- minutes, not milliseconds, which is why the census runs them
// under a separate setup deadline in a forked child. `neutral` is the cold
// taxonomy and has no setup solve. What the pair still covers is the two
// window families and both certificate polarities: the bound cell certifies in
// ONE SSN step with one accepted refinement, and the path cell runs four
// subproblems with three refinements and one REFUSAL -- i.e. both branches of
// R5's own hoist. A lever leaking into a taxonomy these two do not exercise
// would be caught by the arms in the Phase-B report rather than here.
//
// Every column is compared except wall_s, which is a statement about the
// machine.
// ======= MKL-OBSERVED. RE-VERIFIED ON ACCELERATE AT M3 GATE B: every =======
// ======= integer column, the status and the per-QP shape reproduce it  =======
// ======= exactly; the five float residual columns do not and are not   =======
// ======= assertable there (register entry M3-3, at the compare below). =======
TEST(CorpusTask6bPhaseB, TheShippedKSsnConfigurationIsUnmovedByTheFourLevers) {
    // Pin the thread count the same way every committed artifact's provenance
    // header does (see bench_corpus.cpp's `# MKL_NUM_THREADS:` line): the five
    // float residual columns below are compared by re-print at `{:.9e}`, and
    // multi-threaded MKL's reduction order is not fixed run-to-run, so a bare
    // multi-threaded `ctest` can flip the last 1-2 digits and fail a test that
    // defends byte-identity, not numerics (final branch review, must-fix #1;
    // results note §10.2/§11.11 item 1). `setenv` with overwrite so the test
    // is correct even if a caller already exported a different value.
    ::setenv("MKL_NUM_THREADS", "1", 1);
    const auto post = runner_test::data_rows(HVEN_SQP_TASK6B_SSN_CSV);
    ASSERT_EQ(post.size(), 57u);
    std::map<std::string, std::vector<std::string>> by_id;
    for (const std::string &r : post) {
        std::vector<std::string> col = runner_test::split_all(r);
        by_id[col[0]] = col;
    }

    // Chosen for coverage per the banner, and for cost: BOTH are under 0.35 s
    // in the committed artifact. (The count in this sentence used to say
    // "four"; it has always been two -- Phase-B review, minor 4.)
    const std::vector<std::string> ids = {"f7_n1000_bound_neutral", "f7_n1000_path_neutral"};
    for (const std::string &id : ids) {
        SCOPED_TRACE(id);
        const auto it = by_id.find(id);
        ASSERT_NE(it, by_id.end());
        const CorpusCell *cell = nullptr;
        for (const CorpusCell &c : all_cells()) {
            if (c.id == id) {
                cell = &c;
            }
        }
        ASSERT_NE(cell, nullptr);

        // The SHIPPED configuration: run_cell's default EngineConfig, which is
        // every lever at its shipped value.
        const CorpusRow row = run_cell(*cell, "ssn");
        const std::vector<std::string> &w = it->second;
        // THE STATUS FIRST (Phase-B review, minor 4). It was omitted from the
        // original comparison set, and it is the one column whose movement is
        // an ANSWER change rather than a cost change -- a lever that leaked
        // into the default path and turned an Optimal cell into a
        // NumericalError one would have passed every assertion below. It costs
        // nothing to compare: the runner already prints it into column 6.
        EXPECT_EQ(std::string(to_string(row.status)), w[6]);
        // The INTEGER columns, by name and by index into the schema-37 header,
        // compared exactly. The float columns (12, 15-20) are compared through
        // their own printed form below, because that is the form the artifact
        // holds and a re-print is what a reader would diff.
        EXPECT_EQ(std::to_string(row.factorizations), w[7]);
        EXPECT_EQ(std::to_string(row.qp_minors), w[8]);
        EXPECT_EQ(std::to_string(row.escapes), w[9]);
        EXPECT_EQ(std::to_string(row.qp_factorizations.size()), w[10]);
        EXPECT_EQ(std::to_string(row.neg_ineq_duals), w[21]);
        EXPECT_EQ(std::to_string(row.ssn.ssn_iters), w[22]);
        EXPECT_EQ(std::to_string(row.ssn.ssn_bulk_flips), w[23]);
        EXPECT_EQ(std::to_string(row.ssn.ssn_backtracks), w[24]);
        EXPECT_EQ(std::to_string(row.ssn.ssn_prox_updates), w[25]);
        EXPECT_EQ(std::to_string(row.ssn.ssn_uncertain_peak), w[26]);
        EXPECT_EQ(std::to_string(row.ssn.ssn_refinements), w[27]);
        EXPECT_EQ(std::to_string(row.ssn.ssn_refine_refused), w[28]);
        EXPECT_EQ(std::to_string(row.ssn.ssn_refine_factorizations), w[29]);
        EXPECT_EQ(std::to_string(row.ssn.ssn_refine_neg_duals), w[30]);
        EXPECT_EQ(std::to_string(row.ssn.ssn_escape_budget), w[31]);
        EXPECT_EQ(std::to_string(row.ssn.ssn_escape_singular), w[32]);
        EXPECT_EQ(std::to_string(row.ssn.ssn_escape_no_contraction), w[33]);
        EXPECT_EQ(std::to_string(row.ssn.ssn_escape_infeasible_suspect), w[34]);
        EXPECT_EQ(std::to_string(row.ssn.ssn_escape_indefinite), w[35]);
        EXPECT_EQ(std::to_string(row.ssn.ssn_escape_gate_refused), w[36]);
        // The per-QP factorization shape, which is the column that catches a
        // lever that moved work between subproblems without moving the total.
        std::string per_qp;
        for (std::size_t q = 0; q < row.qp_factorizations.size(); ++q) {
            per_qp += (q == 0 ? "" : ";") + std::to_string(row.qp_factorizations[q]);
        }
        EXPECT_EQ(per_qp, w[11]);
        // The residuals, to the artifact's own printed precision.
        //
        // M3-3 (docs/notes/2026-08-14-accelerate-divergence-register.md). The
        // committed artifact is MKL-observed, and on MKL the MKL_NUM_THREADS
        // pin above makes a byte-compare of these five columns a legitimate
        // claim. IT IS NOT ONE ON ACCELERATE: hven's Accelerate session stores
        // num_threads without applying it to any backend call, so nothing pins
        // the reduction order on that backend, and the observed values are not
        // even stable run-to-run on the macOS CI lane -- f7_n1000_path_neutral's
        // kkt_residual read 3.053657327e-10, 3.053661768e-10 and
        // 3.053663988e-10 on three separate runs (31817935342, 31824897327,
        // 31812442998), failing the two-run reproduction bar this project
        // requires before any float is committed.
        //
        // So on Accelerate these five are REPORTED, NOT ASSERTED, per
        // docs/testing.md's context-pinned float discipline -- and no
        // Accelerate byte value is committed here, because there is no
        // reproducible one to commit. Every INTEGER column, the status, and the
        // per-QP shape above stay asserted on both backends, which is where
        // this test's actual subject (that four levers move nothing) lives:
        // all of them reproduce the MKL artifact exactly on Apple hardware.
        const std::string residuals = fmt::format(
            "kkt_residual={} stationarity={} primal={} dual_sign={} complementarity={}",
            fmt::format("{:.9e}", row.kkt_residual), fmt::format("{:.9e}", row.kkt_stationarity),
            fmt::format("{:.9e}", row.kkt_primal), fmt::format("{:.9e}", row.kkt_dual_sign),
            fmt::format("{:.9e}", row.kkt_complementarity));
#ifdef USE_ACCELERATE_SPARSE
        ::testing::Test::RecordProperty(fmt::format("task6b_residuals_observed_{}", id), residuals);
        ::testing::Test::RecordProperty(
            fmt::format("task6b_residuals_artifact_{}", id),
            fmt::format("kkt_residual={} stationarity={} primal={} dual_sign={} "
                        "complementarity={}",
                        w[12], w[15], w[16], w[17], w[18]));
#elif defined(NDEBUG)
        // U0 (unified flags, 2026-08-16, declared re-derivation -- see
        // docs/notes/2026-08-16-m3-u0-design.md and the delta report). The
        // committed schema-37 artifact is OLD-REGIME evidence (plain
        // per-config flags) and stays frozen; under COMPILE_FLAGS the live
        // Release residuals moved in their last digits, so the Release arm
        // pins the UNIFIED-FLAGS re-derivation inline (two fresh
        // reproductions, MKL, MKL_NUM_THREADS=1). Every INTEGER column, the
        // status, and the per-QP shape above still assert against the frozen
        // artifact UNCHANGED -- the levers-move-nothing subject of this test
        // survived the flag change with zero counter movement on both cells;
        // only the float encodings forked, exactly as they forked between
        // backends (M3-3). Debug arithmetic did not move, so the #else arm
        // still byte-compares the artifact (config-split per plan §6(c)1).
        struct U0Residuals {
            const char *kkt, *stationarity, *primal, *dual_sign, *complementarity;
        };
        static const std::map<std::string, U0Residuals> kU0ReleaseResiduals = {
            {"f7_n1000_bound_neutral",
             {"6.295832335e-14", "6.295832335e-14", "1.937883159e-14", "0.000000000e+00",
              "0.000000000e+00"}},
            {"f7_n1000_path_neutral",
             {"3.053665099e-10", "1.045096220e-10", "3.053665099e-10", "4.555244335e-07",
              "1.271441541e-13"}},
        };
        (void)residuals;
        const U0Residuals &exp9 = kU0ReleaseResiduals.at(id);
        EXPECT_EQ(fmt::format("{:.9e}", row.kkt_residual), exp9.kkt);
        EXPECT_EQ(fmt::format("{:.9e}", row.kkt_stationarity), exp9.stationarity);
        EXPECT_EQ(fmt::format("{:.9e}", row.kkt_primal), exp9.primal);
        EXPECT_EQ(fmt::format("{:.9e}", row.kkt_dual_sign), exp9.dual_sign);
        EXPECT_EQ(fmt::format("{:.9e}", row.kkt_complementarity), exp9.complementarity);
#else
        (void)residuals;
        EXPECT_EQ(fmt::format("{:.9e}", row.kkt_residual), w[12]);
        EXPECT_EQ(fmt::format("{:.9e}", row.kkt_stationarity), w[15]);
        EXPECT_EQ(fmt::format("{:.9e}", row.kkt_primal), w[16]);
        EXPECT_EQ(fmt::format("{:.9e}", row.kkt_dual_sign), w[17]);
        EXPECT_EQ(fmt::format("{:.9e}", row.kkt_complementarity), w[18]);
#endif
    }
}

TEST(CorpusTask6bRepair, ThePostRepairArtifactsCarryTheCensusAndRescoreCleanly) {
    // The census columns exist, partition `escapes` on every row, and the
    // whole artifact survives the reader's own consistency checks -- which is
    // what --score-gates exercises end to end.
    for (const char *csv : {HVEN_SQP_TASK6B_SSN_CSV, HVEN_SQP_TASK6B_WALK_CSV}) {
        const std::string log = runner_test::temp_path("task6b_rescore.log");
        ASSERT_EQ(runner_test::run_binary(fmt::format("--from-csv {} --score-gates", csv), log), 0)
            << csv;
        std::remove(log.c_str());
        int total_escapes = 0;
        int total_census = 0;
        for (const std::string &r : runner_test::data_rows(csv)) {
            const std::vector<std::string> col = runner_test::split_all(r);
            ASSERT_EQ(col.size(), 37u) << "the post-repair artifacts are schema 37: " << csv;
            if (col[9] == "-1") {
                continue; // a DNF row measured nothing
            }
            int census = 0;
            for (std::size_t k = 31; k < 37; ++k) {
                census += std::stoi(col[k]);
            }
            EXPECT_EQ(census, std::stoi(col[9])) << csv << " cell " << col[0];
            total_escapes += std::stoi(col[9]);
            total_census += census;
        }
        EXPECT_EQ(total_census, total_escapes);
    }

    // THE READING, pinned so the addendum's claim cannot rot: on the kSsn
    // arm the escape mechanism at scale is BUDGET EXHAUSTION, and
    // kNoContraction -- the reason the fixture population made look
    // dominant -- never fires at all. See the battery note's Task-6b
    // addendum section D.
    int budget = 0, singular = 0, no_contraction = 0, suspect = 0, indefinite = 0, refused = 0;
    for (const std::string &r : runner_test::data_rows(HVEN_SQP_TASK6B_SSN_CSV)) {
        const std::vector<std::string> col = runner_test::split_all(r);
        if (col[9] == "-1") {
            continue;
        }
        budget += std::stoi(col[31]);
        singular += std::stoi(col[32]);
        no_contraction += std::stoi(col[33]);
        suspect += std::stoi(col[34]);
        indefinite += std::stoi(col[35]);
        refused += std::stoi(col[36]);
    }
    EXPECT_EQ(budget, 11);
    EXPECT_EQ(suspect, 2);
    EXPECT_EQ(no_contraction, 0) << "kNoContraction never fires on the scale corpus";
    EXPECT_EQ(singular, 0);
    EXPECT_EQ(indefinite, 0);
    EXPECT_EQ(refused, 0) << "the driver's usability gate is structurally unreachable";
}

TEST(CorpusBaseline, TheWalkArmCarriesNoNegativeMultiplierAnywhere) {
    // The baseline NF-1 is read against. The walk's model-level dual_sign is
    // exactly 0.0 on every row, so the kSsn arm's 14 rows and 2 212 negative
    // multipliers are a property of the SSN TIER and not of the problem.
    for (const std::string &r : runner_test::data_rows(HVEN_SQP_WALK_RESWEPT_CSV)) {
        const std::vector<std::string> col = runner_test::split_all(r);
        ASSERT_GE(col.size(), 31u);
        if (col[6] != "Optimal") {
            continue;
        }
        EXPECT_EQ(std::stod(col[17]), 0.0) << "walk dual_sign must be exactly zero: " << col[0];
        EXPECT_EQ(col[21], "0") << "walk neg_ineq_duals must be zero: " << col[0];
    }
}

TEST(CorpusBaseline, TheCommittedWalkBaselineCarriesTheCommittedBudgetTableHash) {
    // The provenance header is what makes "one binary, one budget table"
    // checkable rather than asserted; if the band ever moves, this fails and
    // whoever moved it has to re-sweep rather than re-label.
    const std::string text = runner_test::slurp(HVEN_SQP_CORPUS_BASELINE_CSV);
    EXPECT_NE(text.find(fmt::format("# budget_table_hash: {:#018x}", budget_table_hash())),
              std::string::npos)
        << "the committed baseline was produced under a DIFFERENT budget table";
    EXPECT_EQ(runner_test::data_rows(HVEN_SQP_CORPUS_BASELINE_CSV).size(), 57u);
}

} // namespace
