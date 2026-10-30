#include <cstdlib>
#include "hven/core/compiler.h"
// WX-R1 diagnostic 2 (scratch): HS10 warm chain dp=0.25, dump the false point.
#include <cstdio>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>
#include "support/hs_sweeps.h"
#include "support/nlp_kkt_check.h"
using namespace hven; using namespace hven::solvers;
using test_support::make_hs_sweep;
HVEN_SUPPRESS_DEPRECATED_BEGIN
int main(int argc, char **argv) {
    const double dp = argc > 1 ? std::atof(argv[1]) : 0.25;
    const double pstop = argc > 2 ? std::atof(argv[2]) : 2.25;
    auto model = make_hs_sweep(10);
    SqpResult prev; bool have_prev = false;
    for (double p = 0.0; p <= pstop + 1e-12; p += dp) {
        model->set_parameters(Vec::Constant(1, p));
        SqpOptions o; o.kkt_tol = 1e-6; o.feas_tol = 1e-6; o.max_iter = 60; o.adaptive_mu = false;
        o.common.start_level = have_prev ? StartLevel::kWarm : StartLevel::kCold;
        o.warm_full_step = false; o.enable_soc = true; o.qp_failure_step = false;
        SqpSolver driver(o);
        SqpResult sol = have_prev ? driver.solve(*model, prev.x, prev.warm_start) : driver.solve(*model, model->start_point());
        auto k = test_support::self_check_kkt(*model, sol, 1e-6);
        Vec ci = model->eval_ci(sol.x);
        std::printf("p=%.4g status=%d lvl=%d majors=%d f=%.10g x=(%.8g,%.8g) li=%.6g ci=%.6g comp=%.3g tp=%d/%d/%d\n", p, (int)sol.status,
            (int)sol.counters.start_level_used, (int)sol.counters.major_iters, sol.f, sol.x(0), sol.x(1), sol.lambda_i(0), ci(0),
            k.complementarity, (int)sol.counters.terminal_polish_solves, (int)sol.counters.terminal_polish_adopted, (int)sol.counters.terminal_polish_rejected);
        for (const auto &r : sol.history)
            std::printf("   k=%d f=%.10g stat=%.3g feas=%.3g comp=%.3g tr=%.3g step=%.3g qp=%d qpst=%d minors=%d fact=%d tr_bind=%d verdict=%d\n",
                (int)r.trial, r.f, r.stationarity, r.feasibility, r.complementarity, r.tr_radius, r.step_norm, (int)r.qp_solved,
                (int)r.qp_status, (int)r.qp_minor_iters, (int)r.qp_factorizations, (int)r.tr_binding, (int)r.verdict);
        prev = sol; have_prev = sol.status == SolveStatus::kOptimal;
        if (!have_prev) break;
    }
}
