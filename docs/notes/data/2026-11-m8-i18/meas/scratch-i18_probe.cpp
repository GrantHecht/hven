// SCRATCH (issue #18 diagnosis; not for merge).
#include <cstdio>
#include <cstdlib>
#include <string>
#include <gtest/gtest.h>
#include <hven/detail/qp/walk/walk_engine.h>
#include "qp/support/qp_dump.h"
using namespace hven;
using namespace hven::detail;
namespace T = hven::detail::testing;
TEST(I18Probe, Trace) {
    const char *path = std::getenv("I18_DUMP");
    if (!path) GTEST_SKIP();
    auto d = test_support::load_qp_dump(path);
    d.opts.hessian_known_convex = d.result.find("hkc=1") != std::string::npos;
    if (const char *e = std::getenv("I18_FEAS")) d.opts.feasibility_tol = std::atof(e);
    if (const char *e = std::getenv("I18_OPT")) d.opts.optimality_tol = std::atof(e);
    if (const char *e = std::getenv("I18_HKC")) d.opts.hessian_known_convex = std::atoi(e) != 0;
    if (std::getenv("I18_COLD")) d.has_seed = false;
    if (std::getenv("I18_REFAC")) d.opts.working_set_algebra = QpWorkingSetAlgebra::kRefactorize;
    if (const char *e = std::getenv("I18_THREADS")) d.threads = std::atoi(e);
    T::QpWalkObserver::reset();
    T::QpWalkObserver::active = true;
    const auto s = test_support::solve_qp_dump(d, d.opts);
    T::QpWalkObserver::active = false;
    const auto k = test_support::qp_dump_kkt(d, s);
    const auto &c = s.counters;
    std::printf("RES status=%d minors=%ld facts=%ld f=%.12e eq=%.3e ineq=%.3e box=%.3e stat_abs=%.3e sign=%.3e compl=%.3e\n",
                (int)s.status, (long)c.minor_iters, (long)c.factorizations, test_support::qp_dump_objective(d, s.x),
                k.eq, k.ineq, k.box, k.stationarity * k.scale, k.sign, k.complementarity);
    std::printf("CNT blkterm=%ld tests=%ld dep=%ld undec=%ld deferred=%ld recheck_rm=%ld passovers=%ld implied=%ld retreats=%ld "
                "known_dep_ref=%ld refused_x=%ld rank_rep=%ld rank_miss=%ld unlocks=%ld struct_exact=%ld zm_ex=%ld zm_cap=%ld "
                "cert_rej=%ld cert_tol=%ld exch=%ld adds=%ld drops=%ld degen=%ld entry_rm=%ld stall_admits=%ld\n",
                (long)c.dep_blocker_terminations, (long)c.dep_tests, (long)c.dep_dependent, (long)c.dep_undecidable,
                (long)c.dep_deferred_adds, (long)c.dep_recheck_removals, (long)c.dep_passovers, (long)c.dep_implied_passovers,
                (long)c.dep_border_retreats, (long)c.dep_known_dependent_refusals, (long)c.dep_refused_exchanges,
                (long)c.rank_repairs, (long)c.rank_repair_misses, (long)c.dep_unlocks, (long)c.dep_structural_exact,
                (long)c.zero_motion_exhausted, (long)c.zero_motion_cap_hits, (long)c.dep_certificate_rejections,
                (long)c.dep_certificate_toleranced, (long)c.ws_exchanges, (long)c.ws_adds, (long)c.ws_drops,
                (long)c.degenerate_steps, (long)c.entry_dependency_removals, (long)c.dep_stall_admits);
    for (const auto &r : T::QpWalkObserver::records) {
        std::printf("M %3ld act=%2d blk=%d/%ld/%d route=%d gap=%.3e alpha=%.6e p=%.3e dx=%.3e stol=%.3e deg=%d ride=%d raw=%.6e tied=%ld "
                    "dmult=%.3e dtol=%.3e best=%.3e ncand=%ld ws=%ld->%ld xinf=%.6e obj=%.10e path=%d bdim=%ld pf=%ld k0rb=%d latched=%d "
                    "iv=%d statf=%.3e rowres=%.3e actres=%.3e a.p=%.3e leak=%.3e deprows=%ld screen=%d depres=%.3e depv=%d xidx=%ld xcoef=%.3e "
                    "rel=%d/%ld postdrop=%d shift=%ld dscale=%.3e dstol=%.3e drefused=%ld dmaxref=%.3e\n",
                    (long)r.minor, (int)r.action, (int)r.block, (long)r.block_index, (int)r.block_side, (int)r.drop_route, r.bound_gap,
                    r.alpha, r.p_inf, r.dx_inf, r.step_tol, (int)r.degenerate, (int)r.ride, r.raw_ratio, (long)r.ratio_tied,
                    r.decision_multiplier, r.decision_tol, r.best_multiplier, (long)r.drop_candidates, (long)r.ws_size_in,
                    (long)r.ws_size_out, r.x_inf, r.obj_in, (int)r.eqp_path, (long)r.border_dim, (long)r.pass_factorizations,
                    (int)r.k0_rebuilt, (int)r.border_latched, r.inertia_verdict, r.stat_free_inf, r.eqp_row_residual,
                    r.active_res_inf, r.add_a_dot_p, r.add_leak_bound, (long)r.add_dep_rows, r.add_dep_screen, r.add_dep_residual,
                    r.add_dep_verdict, (long)r.add_exchanged_index, r.add_exchange_coefficient, (int)r.released_block,
                    (long)r.released_index, (int)r.post_drop, (long)r.shift_adds, r.drop_scale, r.drop_scaled_tol,
                    (long)r.drop_refused_by_scale, r.drop_max_refused_ratio);
        for (Index q = 0; q < r.candidates_recorded; ++q)
            std::printf("    cand blk=%d idx=%ld side=%d viol=%.6e\n", (int)r.candidates[q].block, (long)r.candidates[q].index,
                        (int)r.candidates[q].side, r.candidates[q].violation);
    }
    std::printf("X");
    for (Index i = 0; i < s.x.size(); ++i) std::printf(" %.6e", s.x(i));
    std::printf("\nBS");
    for (auto b : s.bound_state) std::printf(" %d", (int)b);
    std::printf("\nTR");
    for (bool b : s.tr_active) std::printf(" %d", (int)b);
    std::printf("\nIA");
    for (bool b : s.ineq_active) std::printf(" %d", (int)b);
    std::printf("\n");
}
