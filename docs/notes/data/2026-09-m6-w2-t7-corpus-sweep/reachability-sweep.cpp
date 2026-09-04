// M6 W2 T7 -- the corpus reachability sweep's own source, committed so the negative result in
// reachability-sweep.csv can be re-derived. It is the sweep's SECOND instrument: the full
// fallback/elastic census read off SqpCounters rather than off the corpus's 76-column row.
//
// BUILD against a Release build tree's libhven.a with that tree's own flags plus
// -I<repo>/bench -I<repo>/tests/sqp. RUN as `<bin> ipm|walk|ssn [big]`: it writes the column
// header and then one CSV row per cell, exactly the bytes below `arm,n_nodes,...` in
// reachability-sweep.csv (whose `#` block is the operator's, recording the run, not the program).
//
// It calls bench/corpus_cells.h's OWN generators, starts, options and budgets. What the sweep
// chooses is the (N, p) grid and the three cell fields those imply: ConstraintFamily
// (kPathInterface above p = 0.5, kBoundArc at or below -- where the window regime splits),
// p0 = p - 0.05 for the taxonomies that hop, and `degenerate = false`.
#include <cstdio>
#include <string>
#include <vector>

#include "corpus_cells.h"

using namespace hven;
using namespace hven::solvers;
namespace corpus = hven::solvers::corpus;
using corpus::BenchFamily;
using corpus::ConstraintFamily;
using corpus::CorpusCell;
using corpus::StartTaxonomy;

namespace {
class Sink : public IpqpTraceSink {
  public:
    std::vector<SqpFallbackVerdictTraceEvent> fb;
    void on_ipqp_iter(const IpqpTraceIterEvent &) override {}
    void on_ipqp_reg(const IpqpTraceRegEvent &) override {}
    void on_ipqp_restart(const IpqpTraceRestartEvent &) override {}
    void on_ipqp_route(const IpqpTraceRouteEvent &) override {}
    void on_ipqp_certify(const IpqpTraceCertifyEvent &) override {}
    void on_ipqp_escape(const IpqpTraceEscapeEvent &) override {}
    void on_qp_mode(const QpModeTraceEvent &) override {}
    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &e) override { fb.push_back(e); }
};
const char *tax(StartTaxonomy t) {
    switch (t) {
    case StartTaxonomy::kNeutralCold:
        return "neutral";
    case StartTaxonomy::kPhysicsInformed:
        return "physics";
    case StartTaxonomy::kCorrupted:
        return "corrupted";
    case StartTaxonomy::kActivityOnly:
        return "activity";
    case StartTaxonomy::kFullWarm:
        return "warm";
    }
    return "?";
}
Index count(const std::vector<SqpFallbackVerdictTraceEvent> &fb, SqpFallbackVerdict v) {
    Index k = 0;
    for (const auto &e : fb) {
        k += e.verdict == v ? 1 : 0;
    }
    return k;
}
} // namespace

int main(int argc, char **argv) {
    const std::string arm = argc > 1 ? argv[1] : "ipm";
    corpus::detail::EngineConfig cfg;
    cfg.qp_mode = arm == "walk" ? QpMode::kWalk : (arm == "ssn" ? QpMode::kSsn : QpMode::kIpm);

    // (a) walk kInfeasible reaching the tier, (b) SSN kInfeasibleSuspect, (c) rung A, (d) rung B,
    // (e) restoration seeded from the elastic point -- the five the brief's census names.
    const char *const arm_name =
        cfg.qp_mode == QpMode::kWalk ? "walk" : (cfg.qp_mode == QpMode::kSsn ? "ssn" : "ipm");
    std::printf("arm,n_nodes,p,taxonomy,status,ipqp_escapes,ipqp_escape_infeasible_suspect,"
                "ssn_escape_infeasible_suspect,elastic_activations,elastic_from_ipqp_escape,"
                "fallback_rung_b,suspicion_disproved,restoration_iters,restoration_seed_rows\n");
    std::vector<Index> ns{10, 20, 40, 100, 200};
    // THE ARMS ARE NOT THE SAME GRID, by the sweep-budget rule and not by a coverage choice: a
    // COLD walk at N = 400, p >= 0.85 has no wall budget here and ran ~25 minutes on one cell, so
    // the walk-driven arms stop at 200 and only kIpm -- the arm rung A needs -- keeps 400.
    if (cfg.qp_mode == QpMode::kIpm) {
        ns.push_back(400);
    }
    std::vector<double> ps{0.02, 0.05, 0.20, 0.30, 0.45, 0.5001, 0.55,  0.68,
                           0.75, 0.85, 0.90, 0.95, 0.99, 0.999,  0.9999};
    if (argc > 2 && std::string(argv[2]) == "big") {
        ns = {800, 1000, 2000};
        ps = {0.45, 0.68, 0.85, 0.99};
    }
    const StartTaxonomy taxes[5] = {StartTaxonomy::kNeutralCold, StartTaxonomy::kPhysicsInformed,
                                    StartTaxonomy::kCorrupted, StartTaxonomy::kActivityOnly,
                                    StartTaxonomy::kFullWarm};
    for (const Index n : ns) {
        for (const double p : ps) {
            for (const StartTaxonomy t : taxes) {
                const double p0 = p > 0.05 ? p - 0.05 : p;
                const CorpusCell cell{
                    "sweep",
                    BenchFamily::kF7,
                    n,
                    p0,
                    p,
                    0,
                    t,
                    p > 0.5 ? ConstraintFamily::kPathInterface : ConstraintFamily::kBoundArc,
                    false};
                try {
                    corpus::F7CollocationChain model = corpus::detail::make_model(cell);
                    const SqpOptions opts = corpus::detail::options_for_cell(cell, cfg);
                    model.set_parameters(Vec::Constant(1, cell.p));
                    SqpDriver driver(opts);
                    Sink sink;
                    driver.attach_trace(&sink);
                    SqpSolution sol;
                    // The two COLD taxonomies run here directly so SqpCounters is readable; the
                    // three hand-off taxonomies go through the corpus runner below.
                    if (t == StartTaxonomy::kNeutralCold) {
                        sol = corpus::detail::budgeted_solve(driver, model, model.start_point());
                    } else if (t == StartTaxonomy::kPhysicsInformed) {
                        sol = corpus::detail::budgeted_solve(
                            driver, model, corpus::detail::physics_informed_start(model, cell.p));
                    } else {
                        const corpus::CorpusRow row = corpus::detail::run_cell_engine(cell, cfg);
                        std::printf("%s,%lld,%.4g,%s,%s,%lld,%lld,%lld,-,-,-,-,-,-\n", arm_name,
                                    (long long)n, p, tax(t), to_string(row.status),
                                    (long long)row.ipqp.ipqp_escapes,
                                    (long long)row.ipqp.ipqp_escape_infeasible_suspect,
                                    (long long)row.ssn.ssn_escape_infeasible_suspect);
                        continue;
                    }
                    Index seeded = 0;
                    for (const SqpIterate &h : sol.history) {
                        seeded += h.restoration_seed_used ? 1 : 0;
                    }
                    std::printf("%s,%lld,%.4g,%s,%s,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,"
                                "%lld\n",
                                arm_name, (long long)n, p, tax(t), to_string(sol.status),
                                (long long)sol.counters.ipqp.ipqp_escapes,
                                (long long)sol.counters.ipqp.ipqp_escape_infeasible_suspect,
                                (long long)sol.counters.ssn.ssn_escape_infeasible_suspect,
                                (long long)sol.counters.elastic_activations,
                                (long long)sol.counters.elastic_from_ipqp_escape,
                                (long long)count(sink.fb, SqpFallbackVerdict::kRungB),
                                (long long)count(sink.fb, SqpFallbackVerdict::kDisproved),
                                (long long)sol.counters.restoration_iters, (long long)seeded);
                } catch (const std::exception &e) {
                    std::printf("%s,%lld,%.4g,%s,THROW,%s\n", arm_name, (long long)n, p, tax(t),
                                e.what());
                }
            }
        }
    }
    return 0;
}
