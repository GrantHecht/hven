// M7 W2 E1 D7 (3): the offline equality-basis check over the U0 replay cells.
// For every cell and engine it mirrors corpus::detail::run_cell_engine's
// taxonomy (the same setup hop, the same designated start) and reports, per
// solve: (a) the basis computed at the solve's start point with no solver
// (verdict, dropped, ran_qr); (b) the solve's own SqpEqualityBasisReport,
// which covers every in-solve split (x0, trigger-(b) recomputes, the warm
// probe); (c) the basis re-computed at the solve's final iterate (S5).
// Byte-identical QPs are predicted iff every row reports dropped 0 in (a),
// dropped_peak 0 in (b), and dropped 0 in (c).
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include "../../../bench/corpus_cells.h"
#include <hven/detail/drivers/equality_basis.h>

using namespace hven;
using namespace hven::solvers;
using namespace hven::solvers::corpus;
namespace cd = hven::solvers::corpus::detail;
namespace sd = hven::solvers::detail;
using sd::EqualityBasis;

namespace {

const char *verdict_name(sd::EqualityBasisVerdict v) {
    switch (v) {
    case sd::EqualityBasisVerdict::kNotComputed: return "not_computed";
    case sd::EqualityBasisVerdict::kFullRank: return "full_rank";
    case sd::EqualityBasisVerdict::kReduced: return "reduced";
    case sd::EqualityBasisVerdict::kUndecidable: return "undecidable";
    }
    return "?";
}

int g_bad = 0;

void report(const std::string &cell, const std::string &engine, const char *hop,
            const F7CollocationChain &model, const Vec &x_start, const SqpResult &sol) {
    EqualityBasis at_start;
    at_start.compute(model.eval_jac_e(x_start), model.lower(), model.upper());
    EqualityBasis at_final;
    at_final.compute(model.eval_jac_e(sol.x), model.lower(), model.upper());
    const SqpEqualityBasisReport &r = sol.equality_basis;
    const bool ok = !at_start.reduces() && !at_final.reduces() && r.dropped_peak == 0 &&
                    r.dropped_rows.empty() && !r.switched_off && r.inconsistent_row == -1;
    if (!ok) {
        ++g_bad;
    }
    std::printf("%s,%s,%s,%ld,%ld,%s,%zu,%d,%.3e,%d,%d,%ld,%ld,%ld,%d,%s,%zu,%d,%s,%s\n",
                cell.c_str(), engine.c_str(), hop, static_cast<long>(model.n()),
                static_cast<long>(model.me()), verdict_name(at_start.verdict()),
                at_start.dropped().size(), at_start.ran_qr() ? 1 : 0,
                at_start.min_retained_pivot(), r.computed ? 1 : 0, r.undecidable ? 1 : 0,
                static_cast<long>(r.dropped_peak), static_cast<long>(r.recomputes),
                static_cast<long>(r.qr_factorizations), r.switched_off ? 1 : 0,
                verdict_name(at_final.verdict()), at_final.dropped().size(),
                at_final.ran_qr() ? 1 : 0, to_string(sol.status), ok ? "OK" : "MOVES");
    std::fflush(stdout);
}

void run(const CorpusCell &cell, const std::string &engine, QpMode mode) {
    cd::EngineConfig cfg;
    cfg.qp_mode = mode;
    F7CollocationChain model = cd::make_model(cell);
    const SqpOptions opts = cd::options_for_cell(cell, cfg);
    SqpSolver driver(opts);
    const std::string id = cell.id;
    switch (cell.start) {
    case StartTaxonomy::kNeutralCold: {
        model.set_parameters(Vec::Constant(1, cell.p));
        const Vec x0 = model.start_point();
        report(id, engine, "target", model, x0, cd::budgeted_solve(driver, model, x0));
        return;
    }
    case StartTaxonomy::kPhysicsInformed: {
        model.set_parameters(Vec::Constant(1, cell.p));
        const Vec x0 = cd::physics_informed_start(model, cell.p);
        report(id, engine, "target", model, x0, cd::budgeted_solve(driver, model, x0));
        return;
    }
    case StartTaxonomy::kCorrupted: {
        model.set_parameters(Vec::Constant(1, cell.p0));
        const Vec xs = model.start_point();
        const SqpResult seed = cd::budgeted_solve(driver, model, xs);
        report(id, engine, "setup", model, xs, seed);
        const SqpWarmStart corrupted = cd::corrupt_warm_start(seed.warm_start);
        model.set_parameters(Vec::Constant(1, cell.p));
        report(id, engine, "target", model, corrupted.x,
               cd::budgeted_solve(driver, model, corrupted.x, corrupted));
        return;
    }
    case StartTaxonomy::kActivityOnly: {
        model.set_parameters(Vec::Constant(1, cell.p));
        const double mu = cd::crossover_mu_for_n(cell.n_nodes);
        const cd::IpIterate it = cd::f7_ip_iterate(
            model, cell.p, mu, cd::physics_informed_start(model, cell.p));
        const SqpWarmStart crossover =
            from_interior_point(it.x, it.lambda_e, it.lambda_i, it.slack_i, it.z_lower,
                                it.z_upper, model.lower(), model.upper(), IpCrossoverOptions{});
        report(id, engine, "target", model, crossover.x,
               cd::budgeted_solve(driver, model, crossover.x, crossover));
        return;
    }
    case StartTaxonomy::kFullWarm: {
        model.set_parameters(Vec::Constant(1, cell.p0));
        const Vec xs = model.start_point();
        const SqpResult seed = cd::budgeted_solve(driver, model, xs);
        report(id, engine, "setup", model, xs, seed);
        model.set_parameters(Vec::Constant(1, cell.p));
        report(id, engine, "target", model, seed.warm_start.x,
               cd::budgeted_solve(driver, model, seed.warm_start.x, seed.warm_start));
        return;
    }
    }
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: basis_check <comma-separated cells>\n");
        return 2;
    }
    std::vector<std::string> cells;
    std::stringstream ss(argv[1]);
    for (std::string c; std::getline(ss, c, ',');) {
        if (!c.empty()) {
            cells.push_back(c);
        }
    }
    std::printf("cell,engine,hop,n,me,start_verdict,start_dropped,start_ran_qr,"
                "start_min_pivot,solve_computed,solve_undecidable,solve_dropped_peak,"
                "solve_recomputes,solve_qr_factorizations,solve_switched_off,final_verdict,"
                "final_dropped,final_ran_qr,status,prediction\n");
    for (const std::string &c : cells) {
        const CorpusCell *cell = find_cell(c.c_str());
        if (cell == nullptr) {
            std::fprintf(stderr, "unknown cell %s\n", c.c_str());
            return 2;
        }
        run(*cell, "walk", QpMode::kWalk);
        run(*cell, "ssn", QpMode::kSsn);
        run(*cell, "ipm", QpMode::kIpm);
    }
    std::printf("# cells %zu rows_moving %d\n", cells.size(), g_bad);
    return g_bad == 0 ? 0 : 1;
}
