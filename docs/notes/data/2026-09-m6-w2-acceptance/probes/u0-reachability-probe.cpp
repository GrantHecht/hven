/*
 * M6 W2 T8 -- the U0 REACHABILITY probe. A SCRATCH INSTRUMENT: it re-runs the 27 U0 replay
 * cells in all three arms and reads the counters the 76-column corpus schema DOES NOT carry --
 * elastic activations, the fallback route, restoration, the restoration seed row and
 * verdict_refine_steps -- so "no U0 cell reaches T4's seed or T6b/T7's verdict-site refinement"
 * is a MEASUREMENT rather than an inference. Per-taxonomy setup is copied from
 * bench/corpus_cells.h's own `run_cell_engine` (:1615), with the SqpSolution kept instead of
 * reduced to a CorpusRow.
 */
#include <cstdio>
#include <string>
#include <vector>

#include "corpus_cells.h"

using namespace hven;
using namespace hven::solvers;
namespace corpus = hven::solvers::corpus;
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

SqpSolution solve_cell(const CorpusCell &cell, const corpus::detail::EngineConfig &cfg,
                       Sink &sink) {
    corpus::F7CollocationChain model = corpus::detail::make_model(cell);
    const SqpOptions opts = corpus::detail::options_for_cell(cell, cfg);
    SqpDriver driver(opts);
    driver.attach_trace(&sink);
    switch (cell.start) {
    case StartTaxonomy::kNeutralCold: {
        model.set_parameters(Vec::Constant(1, cell.p));
        return corpus::detail::budgeted_solve(driver, model, model.start_point());
    }
    case StartTaxonomy::kPhysicsInformed: {
        model.set_parameters(Vec::Constant(1, cell.p));
        return corpus::detail::budgeted_solve(driver, model,
                                              corpus::detail::physics_informed_start(model, cell.p));
    }
    case StartTaxonomy::kCorrupted: {
        model.set_parameters(Vec::Constant(1, cell.p0));
        const SqpSolution seed = corpus::detail::budgeted_solve(driver, model, model.start_point());
        const WarmStart corrupted = corpus::detail::corrupt_warm_start(seed.warm_start);
        model.set_parameters(Vec::Constant(1, cell.p));
        return corpus::detail::budgeted_solve(driver, model, corrupted.x, corrupted);
    }
    case StartTaxonomy::kActivityOnly: {
        model.set_parameters(Vec::Constant(1, cell.p));
        const double mu = corpus::detail::crossover_mu_for_n(cell.n_nodes);
        const auto it = corpus::detail::f7_ip_iterate(
            model, cell.p, mu, corpus::detail::physics_informed_start(model, cell.p));
        const WarmStart crossover =
            from_interior_point(it.x, it.lambda_e, it.lambda_i, it.slack_i, it.z_lower, it.z_upper,
                                model.lower(), model.upper(), IpCrossoverOptions{});
        return corpus::detail::budgeted_solve(driver, model, crossover.x, crossover);
    }
    case StartTaxonomy::kFullWarm: {
        model.set_parameters(Vec::Constant(1, cell.p0));
        const SqpSolution seed = corpus::detail::budgeted_solve(driver, model, model.start_point());
        model.set_parameters(Vec::Constant(1, cell.p));
        return corpus::detail::budgeted_solve(driver, model, seed.warm_start.x, seed.warm_start);
    }
    }
    throw std::invalid_argument("unrecognised StartTaxonomy");
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: u0_reach_probe <comma-separated cell ids>\n");
        return 2;
    }
    std::vector<std::string> ids;
    {
        std::string s(argv[1]), cur;
        for (const char c : s) {
            if (c == ',') {
                if (!cur.empty()) ids.push_back(cur);
                cur.clear();
            } else if (c != ' ' && c != '\n') {
                cur.push_back(c);
            }
        }
        if (!cur.empty()) ids.push_back(cur);
    }
    std::printf("arm,cell_id,status,major_iters,rejected_steps,elastic_activations,"
                "elastic_from_ipqp_escape,ipqp_fallback_rung_b,ipqp_suspicion_disproved,"
                "elastic_rho0_ceiling_hits,elastic_floor_retries,restoration_iters,"
                "restoration_seed_rows,verdict_refine_steps,border_refine_steps,eqp_refine_steps,"
                "fallback_events,ipqp_escapes,soc_steps\n");
    int failures = 0;
    for (const char *arm : {"walk", "ssn", "ipm"}) {
        corpus::detail::EngineConfig cfg;
        cfg.qp_mode = std::string(arm) == "walk"
                          ? QpMode::kWalk
                          : (std::string(arm) == "ssn" ? QpMode::kSsn : QpMode::kIpm);
        for (const std::string &id : ids) {
            const CorpusCell *found = nullptr;
            for (const CorpusCell &c : corpus::all_cells()) {
                if (c.id == id) {
                    found = &c;
                    break;
                }
            }
            if (found == nullptr) {
                std::fprintf(stderr, "unknown cell id '%s'\n", id.c_str());
                ++failures;
                continue;
            }
            Sink sink;
            const SqpSolution sol = solve_cell(*found, cfg, sink);
            Index seeded = 0;
            for (const SqpIterate &h : sol.history) {
                seeded += h.restoration_seed_used ? 1 : 0;
            }
            std::printf("%s,%s,%s,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,"
                        "%lld,%lld,%lld\n",
                        arm, id.c_str(), to_string(sol.status), (long long)sol.counters.major_iters,
                        (long long)sol.counters.rejected_steps,
                        (long long)sol.counters.elastic_activations,
                        (long long)sol.counters.elastic_from_ipqp_escape,
                        (long long)sol.counters.ipqp_fallback_rung_b,
                        (long long)sol.counters.ipqp_suspicion_disproved,
                        (long long)sol.counters.elastic_rho0_ceiling_hits,
                        (long long)sol.counters.elastic_floor_retries,
                        (long long)sol.counters.restoration_iters, (long long)seeded,
                        (long long)sol.counters.verdict_refine_steps,
                        (long long)sol.counters.border_refine_steps,
                        (long long)sol.counters.eqp_refine_steps, (long long)sink.fb.size(),
                        (long long)sol.counters.ipqp.ipqp_escapes,
                        (long long)sol.counters.soc_steps);
            std::fflush(stdout);
        }
    }
    return failures == 0 ? 0 : 1;
}
