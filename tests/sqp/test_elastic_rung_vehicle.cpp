// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_elastic_rung_vehicle.cpp -- M7 W0 T3 step 2a: the constructed cell that
// FIRES the elastic rung at F7 scale, and the WALK's measured cost on rung A.
//
// The vehicle and why it had to be built are in support/elastic_rung_vehicle.h.
// The pin here is the one the step-2a brief asks for: under `qp_mode = kIpm`,
// `elastic_from_ipqp_escape >= 1` at every size -- i.e. the cell really does
// reach the certified fallback's rung A, which no natural F7 population does.
//
// THE COST READING is the second half, and it is COUNTERS (CLAUDE.md section 7):
// the rung count, the minor iterations each rung spent, and each rung's outcome,
// read off the `qp.mode` trace event's `kElasticRung` site -- one line per rung,
// emitted where the rung has run (src/drivers/sqp_solver.cpp's ladder). Wall is
// printed and NEVER asserted. Factorizations are attributed through the Ledger,
// whose QP-level `SolveRecord` stream is one record per engine solve -- the
// attribution is PROVEN rather than assumed by checking that the correlated
// record's `minor_iters` equals the trace event's `iters` at every index, and
// the reading is reported as unattributed if that check fails.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/core/ledger.h>
#include <hven/core/solver_status.h>
#include <hven/drivers/sqp_solver.h>

#include "support/elastic_rung_vehicle.h"

namespace hven::solvers {
namespace {

using hven::solvers::test_support::F7InconsistentChain;

// The sizes step 2a measures. RELEASE SWEEPS ALL FOUR; DEBUG SWEEPS THE TWO
// SMALL ONES, and that split is declared rather than quiet: the cost this cell
// exists to measure is O(n) walk minors each costing O(n) work, so N = 800 is
// 30 s in Release and would be minutes in Debug -- a second
// `Continuation.ProbeBudgetBoundsAFailingProposal` is not worth a pin that the
// two small sizes already carry (the firing MECHANISM is size-independent; what
// grows with N is only its price, which is a Release measurement anyway).
#if defined(NDEBUG)
constexpr Index kSizes[] = {100, 200, 400, 800};
#else
constexpr Index kSizes[] = {100, 200};
#endif

/// The sizes THIS PROCESS measures. The default is `kSizes` whole; a
/// comma-separated `HVEN_M7T3_SIZES` narrows it. The affordance exists for one
/// reason and is documented so it is not mistaken for a knob: `perf stat`
/// counts a PROCESS, so isolating one size's instruction count means running
/// one size per process. It cannot widen the set beyond what a caller asks for
/// and it changes no assertion -- the PIN above always sweeps `kSizes` whole.
std::vector<Index> sizes_for_this_process() {
    const char *env = std::getenv("HVEN_M7T3_SIZES");
    if (env == nullptr || *env == '\0') {
        return std::vector<Index>(std::begin(kSizes), std::end(kSizes));
    }
    std::vector<Index> out;
    const std::string spec(env);
    std::size_t at = 0;
    while (at <= spec.size()) {
        const std::size_t comma = spec.find(',', at);
        const std::string tok = spec.substr(at, comma == std::string::npos ? comma : comma - at);
        if (!tok.empty()) {
            out.push_back(static_cast<Index>(std::stoll(tok)));
        }
        if (comma == std::string::npos) {
            break;
        }
        at = comma + 1;
    }
    if (out.empty()) {
        throw std::invalid_argument(
            fmt::format("HVEN_M7T3_SIZES is '{}', which names no size", spec));
    }
    return out;
}

// ONE MAJOR, and that is a measurement decision as much as a budget one. The
// cell is INFEASIBLE by construction, so an unbounded run spends its whole
// budget on majors that each re-enter the fallback and then on restoration --
// 40 majors at N = 100 does not finish in 540 s, which prices the SOLVE and not
// the RUNG. At one major each size runs exactly one tier escape and one primary
// fallback entry, from the same start point: the like-for-like comparison the
// "does rung A's cost grow with N?" question needs. It also satisfies CLAUDE.md
// section 8's "no unbounded cells" outright.
//
// WHAT "ONE MAJOR" DOES NOT BUY, corrected 2026-09-16 (fix round 1, sol's
// Minor 5): it is NOT one ladder per cell on both arms. The WALK arm is one
// ladder -- `elastic_activations = 1`, six rungs, five of them escalations. The
// TIER arm is not: three of its five rungs DECLINE, so the driver re-enters, and
// the counters read `elastic_activations = 3`, 5 rungs, 2 escalations,
// `elastic_floor_retries = 1`, `ipqp_fallback_rung_b = 1` and
// `elastic_from_ipqp_escape = 2` at every size. That asymmetry is a finding, not
// an artefact of the budget, and the printed row below carries every one of
// those columns so a reader counts rather than assumes.
constexpr int kMaxIter = 1;

SqpOptions vehicle_options(ElasticRungEngine rung = ElasticRungEngine::kWalk) {
    SqpOptions o;
    o.qp_mode = QpMode::kIpm;
    o.elastic_rung_engine = rung;
    o.max_iter = kMaxIter;
    o.common.threads = 1;
    // The fixture's inconsistency is a dependent row pair; the equality basis removes it.
    o.equality_basis = false;
    return o;
}

const char *engine_name(ElasticRungEngine e) {
    return e == ElasticRungEngine::kWalk ? "walk" : "tier";
}

/// One `qp.mode` line, kept whole: the ladder emits one per rung.
struct ModeLine {
    IpqpTraceQpMode mode = IpqpTraceQpMode::kIpqp;
    IpqpTraceOutcome outcome = IpqpTraceOutcome::kOptimal;
    Index iters = 0;
    QpModeSite site = QpModeSite::kDispatch;
};

class RungSink : public TraceSink {
  public:
    std::vector<ModeLine> modes;
    std::vector<SqpFallbackVerdictTraceEvent> fallbacks;

    void on_ipqp_iter(const IpqpTraceIterEvent &) override {}
    void on_ipqp_reg(const IpqpTraceRegEvent &) override {}
    void on_ipqp_restart(const IpqpTraceRestartEvent &) override {}
    void on_ipqp_route(const IpqpTraceRouteEvent &) override {}
    void on_ipqp_certify(const IpqpTraceCertifyEvent &) override {}
    void on_ipqp_escape(const IpqpTraceEscapeEvent &) override {}
    void on_qp_mode(const QpModeTraceEvent &e) override {
        modes.push_back(ModeLine{e.mode, e.outcome, e.iters, e.site});
    }
    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &e) override {
        fallbacks.push_back(e);
    }
};

struct RungReading {
    Index rungs = 0;            ///< `qp.mode` lines at the kElasticRung site.
    Index rung_minors = 0;      ///< their `iters`, summed -- the ladder's own minors.
    Index rung_max_minors = 0;  ///< the most expensive single rung.
    Index rung_not_optimal = 0; ///< rungs whose outcome was not kOptimal (a declined rung).
    Index tier_rungs = 0;       ///< of `rungs`, how many the interior-point kernel ran.
    Index rung_facts = -1;      ///< ladder factorizations, or -1 when unattributed.
    /// `SqpCounters::factorizations` less the primary tier's and every walk-engine
    /// solve's -- the ELASTIC TIER's own, and 0 on the walk arm by construction.
    Index residual_facts = 0;
    bool facts_attributed = false;
};

/// THE TWO ATTRIBUTIONS, one per engine, both self-checking.
///
/// WALK RUNGS: the ledger's QP-level `SolveRecord`s are one per WALK-ENGINE solve, while the
/// `qp.mode` stream also carries lines for solves that are not the walk's. Aligning the
/// walk-mode SUBSEQUENCE against the ledger gives a 1:1 correspondence -- CHECKED per index
/// (each record's `minor_iters` must equal its line's `iters`), never assumed.
///
/// TIER RUNGS: the elastic engine is the driver's own and emits no ledger record, so its
/// factorizations are read by DIFFERENCE --
///
///     residual = SqpCounters::factorizations
///              - IpqpCounters::ipqp_factorizations   (the PRIMARY tier's)
///              - every ledger record's               (every walk-engine solve's)
///
/// -- which leaves exactly the elastic tier's. The identity is VALIDATED on the walk arm,
/// where the elastic tier never runs and the residual must therefore be 0; a non-zero residual
/// there means some factorization reaches `SqpCounters` by a path this subtraction does not
/// know about, and the tier arm's number is reported unattributed rather than reported wrong.
RungReading read_rungs(const RungSink &sink, const Ledger &ledger, const SqpCounters &counters) {
    RungReading r;
    const std::vector<SolveRecord> &recs = ledger.records();
    std::vector<std::size_t> walk;
    walk.reserve(sink.modes.size());
    for (std::size_t k = 0; k < sink.modes.size(); ++k) {
        if (sink.modes[k].mode == IpqpTraceQpMode::kWalk) {
            walk.push_back(k);
        }
    }
    bool consistent = recs.size() == walk.size();
    if (consistent) {
        for (std::size_t j = 0; j < walk.size(); ++j) {
            if (recs[j].counters.minor_iters != sink.modes[walk[j]].iters) {
                consistent = false;
                break;
            }
        }
    }
    Index ledger_facts = 0;
    for (const SolveRecord &rec : recs) {
        ledger_facts += rec.counters.factorizations;
    }
    // Every rung, whichever kernel ran it: the SITE is what makes a line a rung.
    Index walk_rung_facts = 0;
    for (std::size_t k = 0; k < sink.modes.size(); ++k) {
        const ModeLine &m = sink.modes[k];
        if (m.site != QpModeSite::kElasticRung) {
            continue;
        }
        ++r.rungs;
        r.rung_minors += m.iters;
        r.rung_max_minors = std::max(r.rung_max_minors, m.iters);
        r.rung_not_optimal += m.outcome == IpqpTraceOutcome::kOptimal ? 0 : 1;
        r.tier_rungs += m.mode == IpqpTraceQpMode::kIpqp ? 1 : 0;
    }
    if (consistent) {
        for (std::size_t j = 0; j < walk.size(); ++j) {
            if (sink.modes[walk[j]].site == QpModeSite::kElasticRung) {
                walk_rung_facts += recs[j].counters.factorizations;
            }
        }
    }
    r.residual_facts = counters.factorizations - counters.ipqp.ipqp_factorizations - ledger_facts;
    r.facts_attributed = consistent && r.residual_facts >= 0;
    r.rung_facts = r.facts_attributed ? walk_rung_facts + r.residual_facts : -1;
    return r;
}

} // namespace

// =====================================================================
// THE SHAPE PIN. The vehicle is F7 plus exactly two rows, and the added
// pair really is contradictory -- asserted on the model rather than
// inferred from the solve, so a broken cell is diagnosed here and not
// as a mysterious counter above.
// =====================================================================
TEST(M7T3ElasticRungVehicle, TheAddedPairIsContradictoryAtEveryPoint) {
    F7InconsistentChain model(100);
    EXPECT_EQ(model.me(), model.base().me() + 2);
    EXPECT_EQ(model.n(), model.base().n());
    EXPECT_EQ(model.mi(), model.base().mi());

    const Vec x = model.start_point();
    const Vec ce = model.eval_ce(x);
    const Index r0 = model.base().me();
    EXPECT_DOUBLE_EQ(ce(r0 + 1) - ce(r0), 2.0 * model.scale() * model.offset())
        << "the two residuals differ by 2 s b at every x -- that gap IS the inconsistency";

    const Eigen::SparseMatrix<double, Eigen::RowMajor> je = model.eval_jac_e(x);
    ASSERT_EQ(je.rows(), model.me());
    Vec row0 = Vec::Zero(model.n());
    Vec row1 = Vec::Zero(model.n());
    for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(je, r0); it; ++it) {
        row0(it.col()) = it.value();
    }
    for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(je, r0 + 1); it; ++it) {
        row1(it.col()) = it.value();
    }
    EXPECT_EQ(row0, row1) << "IDENTICAL Jacobian rows with DIFFERENT residuals: no step p solves "
                             "both, at any iterate, so the linearization is inconsistent by "
                             "construction rather than by luck of the starting point";
    EXPECT_GT(row0.cwiseAbs().sum(), 0.0) << "and the rows are not empty";

    // The offset guard is a real boundary check, not decoration.
    EXPECT_THROW(F7InconsistentChain(100, 1.0e2, 0.0), std::invalid_argument);
    EXPECT_THROW(F7InconsistentChain(100, 1.0e2, 1.0e3), std::invalid_argument);
    EXPECT_THROW(F7InconsistentChain(100, 0.0), std::invalid_argument);
}

// =====================================================================
// HAZARD 2's PIN (M7 W0 T3 design section viii): THE TIER RUNG IS IMMUNE TO
// `QpOptions::tr_radius`.
//
// THE HAZARD. The walk ignores a trust-region override at this call site -- the
// window is already folded into the elastic box, and the ladder passes a DEFAULT
// `SolveOverrides` precisely so the slack columns are not capped a second time.
// The tier does not ignore it: `ipqp_effective_tr_radius` resolves the `+inf`
// sentinel to the engine's OWN `opts.tr_radius`, and `make_ipqp_box` then windows
// every one of the `n + ns` columns around `clamp(0, l, u)` -- slacks included.
// The relaxation's feasible-by-construction witness has scaled slack at most 1,
// so a window below that cuts the witness off and the rung solves a DIFFERENT,
// possibly infeasible problem.
//
// THE FIX is that the driver builds its elastic tier engine from a `QpOptions`
// copy carrying `tr_radius = +inf`, whatever the caller set.
//
// THE PIN. `opts.qp.tr_radius` is set to the driver's own `tr_init`, which leaves
// the ELASTIC BOX identical -- the driver folds `min(delta, qp.tr_radius)` into it
// and `delta` starts at `tr_init`, so the min is unchanged at the first major --
// while an UNFIXED tier engine would additionally clamp every slack at `tr_init`.
// Identical ladder counters therefore mean the radius never reached the rung.
// Without the fix this test fails: the tier sees a windowed elastic QP.
// =====================================================================
TEST(M7T3ElasticRungVehicle, TheTierRungIsImmuneToTheDriversTrustRegionRadius) {
    auto run = [](double qp_tr_radius) {
        F7InconsistentChain model(100);
        SqpOptions o = vehicle_options(ElasticRungEngine::kTier);
        o.qp.tr_radius = qp_tr_radius;
        SqpSolver driver(o);
        RungSink sink;
        Ledger ledger;
        driver.attach_trace(&sink);
        driver.attach_ledger(&ledger, "m7t3hz2");
        const SqpResult sol = driver.solve(model);
        return std::make_pair(read_rungs(sink, ledger, sol.counters), sol.counters);
    };
    const SqpOptions probe;
    const auto unwindowed = run(std::numeric_limits<double>::infinity());
    const auto windowed = run(probe.tr_init);

    ASSERT_GE(unwindowed.first.rungs, 1) << "fixture premise: the ladder ran";
    ASSERT_EQ(unwindowed.first.tier_rungs, unwindowed.first.rungs)
        << "fixture premise: the tier ran every rung";

    EXPECT_EQ(windowed.first.rungs, unwindowed.first.rungs)
        << "THE PIN: the driver's own trust-region radius must not reach the elastic rung. A "
           "tier engine built from the caller's QpOptions verbatim would window all n + ns "
           "columns at tr_init and change this climb";
    EXPECT_EQ(windowed.first.rung_minors, unwindowed.first.rung_minors);
    EXPECT_EQ(windowed.first.rung_facts, unwindowed.first.rung_facts);
    EXPECT_EQ(windowed.first.rung_not_optimal, unwindowed.first.rung_not_optimal);
    EXPECT_EQ(windowed.second.elastic_escalations, unwindowed.second.elastic_escalations);
    EXPECT_EQ(windowed.second.ipqp_fallback_rung_b, unwindowed.second.ipqp_fallback_rung_b);
}

// =====================================================================
// THE PIN AND THE MEASUREMENT, IN ONE TEST (step 2a items 1-3).
//
// ONE TEST RATHER THAN TWO, deliberately: the pin and the cost reading
// need the SAME four solves, and the large sizes are the expensive part
// of this suite -- running them twice would double that cost for a
// second copy of the same numbers.
//
// THE PIN is `elastic_from_ipqp_escape >= 1` at every size: the cell
// must really reach the certified fallback's rung A, which no natural F7
// population does, and a later change to the tier's section 6.3 detector
// could silently take that away.
//
// THE MEASUREMENT is printed, not asserted: it is what step 2a's reading
// ("does the walk struggle on a large elastic QP?") rests on, and a
// pinned cost here would be a pin on the walk's active-set iteration
// count at four sizes, which is not a contract.
// =====================================================================
TEST(M7T3ElasticRungVehicle, TheConstructedCellFiresTheElasticRungAndPricesBothEngines) {
    std::fputs("# M7 W0 T3 -- rung A's cost, BOTH engines (counters asserted, wall "
               "informational)\n",
               stdout);
    std::fputs("engine,N,n,me,mi,status,certified,majors,entries,rungs,rung_minors,"
               "rung_max_minors,rung_dnf,rung_facts,facts_attributed,solve_minors,solve_facts,"
               "escalations,ceiling_hits,floor_retries,rung_b,disproved,tier_iters,tier_facts,"
               "analyses,wall_s\n",
               stdout);
    // `HVEN_M7T3_ENGINE` narrows the sweep to one toggle state, for the same and only reason
    // `HVEN_M7T3_SIZES` narrows the sizes: `perf stat` counts a PROCESS, so isolating one
    // cell's instruction count means running one cell per process. Neither can widen the
    // sweep and neither changes an assertion.
    const char *only = std::getenv("HVEN_M7T3_ENGINE");
    for (ElasticRungEngine rung : {ElasticRungEngine::kWalk, ElasticRungEngine::kTier}) {
        if (only != nullptr && *only != '\0' && std::string(only) != engine_name(rung)) {
            continue;
        }
        for (Index nodes : sizes_for_this_process()) {
            SCOPED_TRACE(fmt::format("{} N = {}", engine_name(rung), nodes));
            F7InconsistentChain model(nodes);
            SqpSolver driver(vehicle_options(rung));
            RungSink sink;
            Ledger ledger;
            driver.attach_trace(&sink);
            driver.attach_ledger(&ledger, "m7t3");

            const auto t0 = std::chrono::steady_clock::now();
            const SqpResult sol = driver.solve(model);
            const double wall =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

            const RungReading r = read_rungs(sink, ledger, sol.counters);

            // -- THE PIN --
            EXPECT_GE(sol.counters.elastic_from_ipqp_escape, 1)
                << "THE VEHICLE'S REASON FOR EXISTING: the contradictory equality pair must make "
                   "the "
                   "LINEARIZED subproblem inconsistent, the tier must suspect it (section 6.3), "
                   "and "
                   "the major must route to the certified fallback whose rung A is the ladder. "
                   "Zero "
                   "here means the cell no longer fires and every number below is about nothing";
            EXPECT_GT(sol.counters.ipqp.ipqp_escape_infeasible_suspect, 0)
                << "and it is the INFEASIBLE-SUSPECT escape that got there, not another escape "
                   "class";
            EXPECT_GE(sol.counters.elastic_activations, sol.counters.elastic_from_ipqp_escape)
                << "every fallback-route activation is an activation";
            ASSERT_GE(r.rungs, 1) << "fixture premise: the ladder emitted its own qp.mode lines";
            EXPECT_EQ(r.tier_rungs, rung == ElasticRungEngine::kTier ? r.rungs : 0)
                << "the toggle decides WHICH kernel runs every rung, and the qp.mode line names it";
            if (rung == ElasticRungEngine::kWalk) {
                EXPECT_EQ(r.residual_facts, 0)
                    << "THE ATTRIBUTION IDENTITY, validated where its answer is known: with no "
                       "elastic tier running, every factorization in SqpCounters is either the "
                       "primary tier's or a ledgered walk solve's. A non-zero residual here means "
                       "the tier arm's attribution is measuring something else too";
            }
            EXPECT_EQ(r.rungs, sol.counters.elastic_activations + sol.counters.elastic_escalations)
                << "one qp.mode line per rung, which is what makes the printed cost the LADDER's";

            std::fputs(
                fmt::format(
                    "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},"
                    "{:.6f}\n",
                    engine_name(rung), nodes, model.n(), model.me(), model.mi(),
                    to_string(sol.status), sol.infeasibility_certified ? 1 : 0,
                    sol.counters.major_iters, sol.counters.elastic_from_ipqp_escape, r.rungs,
                    r.rung_minors, r.rung_max_minors, r.rung_not_optimal, r.rung_facts,
                    r.facts_attributed ? 1 : 0, sol.counters.qp_minor_iters,
                    sol.counters.factorizations, sol.counters.elastic_escalations,
                    sol.counters.elastic_rho0_ceiling_hits, sol.counters.elastic_floor_retries,
                    sol.counters.ipqp_fallback_rung_b, sol.counters.ipqp_suspicion_disproved,
                    sol.counters.ipqp.ipqp_iters, sol.counters.ipqp.ipqp_factorizations,
                    sol.counters.symbolic_analyses, wall)
                    .c_str(),
                stdout);
            std::fflush(stdout);
        }
    }
}

} // namespace hven::solvers
