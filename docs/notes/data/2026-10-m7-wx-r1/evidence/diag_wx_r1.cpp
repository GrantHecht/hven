// WX-R1 diagnostic (scratch, never committed): hunt false kOptimal on MKL.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <fmt/format.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>
#include "support/hs_sweeps.h"
#include "support/nlp_kkt_check.h"

using namespace hven;
using namespace hven::solvers;
using test_support::hs_sweep_specs;
using test_support::make_hs_sweep;

static SqpOptions opts_for(StartLevel level, QpMode mode, bool full_step, bool soc, bool adaptive) {
    SqpOptions o;
    o.kkt_tol = 1e-6; o.feas_tol = 1e-6; o.max_iter = 60;
    o.adaptive_mu = adaptive;
    o.common.start_level = level;
    o.warm_full_step = full_step;
    o.enable_soc = soc;
    o.qp_failure_step = false;
    o.qp_mode = mode;
    return o;
}

int main(int argc, char **argv) {
    const bool verbose = argc > 1 && std::string(argv[1]) == "-v";
    int false_opt = 0, n_opt = 0;
    const QpMode modes[] = {QpMode::kWalk, QpMode::kSsn, QpMode::kIpm};
    const double dps_scale[] = {1.0, 0.5, 0.25, 2.0, 0.3, 0.7};
    for (const auto &spec0 : hs_sweep_specs()) {
      for (QpMode mode : modes) for (int fs = 0; fs < 2; ++fs) for (int ad = 0; ad < 2; ++ad)
      for (double sc : dps_scale) {
        auto spec = spec0; spec.dp = spec0.dp * sc;
        auto model = make_hs_sweep(spec.number);
        SqpResult prev; bool have_prev = false;
        for (double p = spec.p0; p <= spec.p1 + 1e-12; p += spec.dp) {
            model->set_parameters(Vec::Constant(1, p));
            const StartLevel level = have_prev ? StartLevel::kWarm : StartLevel::kCold;
            SqpSolver driver(opts_for(level, mode, fs, true, ad));
            SqpResult sol;
            try {
                sol = have_prev ? driver.solve(*model, prev.x, prev.warm_start)
                                : driver.solve(*model, model->start_point());
            } catch (const std::exception &e) { std::printf("EXC %s\n", e.what()); break; }
            if (sol.status == SolveStatus::kOptimal) {
                ++n_opt;
                auto k = test_support::self_check_kkt(*model, sol, 1e-6);
                const bool bad = k.complementarity > 1e-6 || k.stationarity > 1e-5 || k.primal > 1e-5;
                if (bad || verbose) {
                    std::printf("%s %s mode=%d fs=%d ad=%d dp=%.3g p=%.3g lvl=%d majors=%d f=%.10g stat=%.3g prim=%.3g comp=%.3g sqp_comp=%.3g tp_solves=%d tp_adopt=%d tp_rej=%d\n",
                        bad ? "FALSEOPT" : "ok", spec.name, (int)mode, fs, ad, spec.dp, p,
                        (int)sol.counters.start_level_used, (int)sol.counters.major_iters, sol.f,
                        k.stationarity, k.primal, k.complementarity, sol.sqp_complementarity,
                        (int)sol.counters.terminal_polish_solves, (int)sol.counters.terminal_polish_adopted,
                        (int)sol.counters.terminal_polish_rejected);
                }
                if (bad) ++false_opt;
            }
            prev = sol; have_prev = sol.status == SolveStatus::kOptimal;
            if (!have_prev) break;
        }
      }
    }
    std::printf("TOTAL optimal=%d false=%d\n", n_opt, false_opt);
    return 0;
}
