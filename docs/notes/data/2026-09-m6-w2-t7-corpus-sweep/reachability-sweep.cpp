// M6 W2 T7 -- the corpus reachability sweep's own source, committed so the
// negative result in reachability-sweep.csv can be re-derived. Build it against
// a Release build tree's libhven.a with that tree's own flags plus
// -I<repo>/bench -I<repo>/tests/sqp, and run it as `<bin> ipm|walk|ssn`.
// It calls bench/corpus_cells.h's OWN generators, starts and options; only the
// (N, p) grid is the sweep's.
// W2 T7 Part 1: the systematic corpus sweep, second instrument -- the FULL fallback/elastic
// census on the corpus's own generators, read off SqpCounters rather than off the 76-column row.
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
    case StartTaxonomy::kNeutralCold: return "neutral";
    case StartTaxonomy::kPhysicsInformed: return "physics";
    case StartTaxonomy::kCorrupted: return "corrupted";
    case StartTaxonomy::kActivityOnly: return "activity";
    case StartTaxonomy::kFullWarm: return "warm";
    }
    return "?";
}
Index count(const std::vector<SqpFallbackVerdictTraceEvent> &fb, SqpFallbackVerdict v) {
    Index k = 0;
    for (const auto &e : fb) { k += e.verdict == v ? 1 : 0; }
    return k;
}
} // namespace

int main(int argc, char **argv) {
    const std::string arm = argc > 1 ? argv[1] : "ipm";
    corpus::detail::EngineConfig cfg;
    cfg.qp_mode = arm == "walk" ? QpMode::kWalk : (arm == "ssn" ? QpMode::kSsn : QpMode::kIpm);

    // (a) walk kInfeasible reaching the tier, (b) SSN kInfeasibleSuspect, (c) rung A, (d) rung B,
    // (e) restoration seeded from the elastic point -- the five the brief's census names.
    std::printf("%-6s %-8s %-9s %-10s %-5s %-5s %-5s %-5s %-5s %-5s %-5s %-5s %-5s\n", "N", "p",
                "taxonomy", "status", "esc", "susp", "ssnS", "act", "aFrE", "rngB", "disp", "rest",
                "seed");
    std::vector<Index> ns{10, 20, 40, 100, 200};
    std::vector<double> ps{0.02, 0.05, 0.20, 0.30, 0.45, 0.5001, 0.55,
                           0.68, 0.75, 0.85, 0.90,  0.95,  0.99, 0.999, 0.9999};
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
                const CorpusCell cell{"sweep", BenchFamily::kF7, n, p0, p, 0, t,
                                      p > 0.5 ? ConstraintFamily::kPathInterface
                                              : ConstraintFamily::kBoundArc,
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
                        std::printf("%-6lld %-8.4g %-9s %-10s %-5lld %-5lld %-5lld %-5s %-5s %-5s "
                                    "%-5s %-5s %-5s\n",
                                    (long long)n, p, tax(t), to_string(row.status),
                                    (long long)row.ipqp.ipqp_escapes,
                                    (long long)row.ipqp.ipqp_escape_infeasible_suspect,
                                    (long long)row.ssn.ssn_escape_infeasible_suspect,
                                    "-", "-", "-", "-", "-", "-");
                        continue;
                    }
                    Index seeded = 0;
                    for (const SqpIterate &h : sol.history) {
                        seeded += h.restoration_seed_used ? 1 : 0;
                    }
                    std::printf("%-6lld %-8.4g %-9s %-10s %-5lld %-5lld %-5lld %-5lld %-5lld "
                                "%-5lld %-5lld %-5lld %-5lld\n",
                                (long long)n, p, tax(t), to_string(sol.status),
                                (long long)sol.counters.ipqp.ipqp_escapes,
                                (long long)sol.counters.ipqp.ipqp_escape_infeasible_suspect,
                                (long long)sol.counters.ssn.ssn_escape_infeasible_suspect,
                                (long long)sol.counters.elastic_activations,
                                (long long)sol.counters.elastic_from_ipqp_escape,
                                (long long)count(sink.fb, SqpFallbackVerdict::kRungB),
                                (long long)count(sink.fb, SqpFallbackVerdict::kDisproved),
                                (long long)sol.counters.restoration_iters, (long long)seeded);
                } catch (const std::exception &e) {
                    std::printf("%-6lld %-8.4g %-9s THROW %s\n", (long long)n, p, tax(t), e.what());
                }
            }
        }
    }
    return 0;
}
