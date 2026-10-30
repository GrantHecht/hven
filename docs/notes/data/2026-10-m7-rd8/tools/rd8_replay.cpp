// RD8 scratch replay (NOT FOR MERGE): replay one D4QP dump on a fresh QpEngine, the walk observer
// armed when built with HVEN_TESTING, and print a census + optional per-pass CSV.
// usage: rd8_replay DUMP [--maxiter N] [--cap C] [--refactorize] [--csv FILE] [--every K]
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

#include "support/qp_dump.h"
#ifdef HVEN_TESTING
#include <hven/detail/qp/qp_walk_trace.h>
#endif

using namespace hven;

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage\n");
        return 2;
    }
    const std::string path = argv[1];
    long maxiter = -1, cap = -1;
    bool refac = false;
    std::string csv;
    for (int a = 2; a < argc; ++a) {
        std::string s = argv[a];
        if (s == "--maxiter") maxiter = std::atol(argv[++a]);
        else if (s == "--cap") cap = std::atol(argv[++a]);
        else if (s == "--refactorize") refac = true;
        else if (s == "--csv") csv = argv[++a];
    }
    test_support::QpDump d = test_support::load_qp_dump(path);
    solvers::QpOptions o = d.opts;
    if (maxiter > 0) o.max_iter = maxiter;
    if (cap > 0) o.schur_cap = cap;
    if (refac) o.ws_algebra = solvers::WorkingSetLinearAlgebra::kRefactorize;
#ifdef HVEN_TESTING
    namespace T = hven::solvers::detail::testing;
    T::QpWalkObserver::reset();
    T::QpWalkObserver::active = true;
    T::QpWalkObserver::max_records = 2000000;
#endif
    const auto t0 = std::chrono::steady_clock::now();
    const solvers::QpSolution s = test_support::solve_qp_dump(d, o);
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const auto &c = s.counters;
    long pins = 0;
    for (auto b : s.bound_state)
        if (b != solvers::BoundState::kFree) ++pins;
    std::printf("RD8 %s n=%ld me=%ld mi=%ld cap=%ld algebra=%s status=%d minors=%ld facts=%ld schur=%ld "
                "analyses=%ld adds=%ld drops=%ld degen=%ld wall=%.3f q=%.12e pins=%ld xinf=%.4e\n",
                path.c_str(), (long)d.qp.n(), (long)d.qp.me(), (long)d.qp.mi(), (long)o.schur_cap,
                refac ? "refac" : "border", (int)s.status, (long)c.minor_iters, (long)c.factorizations,
                (long)c.schur_updates, (long)c.symbolic_analyses, (long)c.ws_adds, (long)c.ws_drops,
                (long)c.degenerate_steps, wall, test_support::qp_dump_objective(d, s.x), pins,
                s.x.lpNorm<Eigen::Infinity>());
    std::printf("CNT repair_pins=%ld repair_budget=%ld repair_restarts=%ld latch_cap=%ld latch_cond=%ld "
                "releases=%ld holds=%ld standstill=%ld stag_drops=%ld stag_stops=%ld zm_ex=%ld zm_cap=%ld "
                "ride_vetoes=%ld ascent=%ld saddle=%ld saddle_miss=%ld rank_rep=%ld dep_tests=%ld\n",
                (long)c.dep_repair_screen_pins, (long)c.dep_repair_budget_stops, (long)c.dep_repair_restarts,
                (long)c.border_latches_cap, (long)c.border_latches_conditioning, (long)c.border_latch_releases,
                (long)c.border_latch_holds, (long)c.standstill_retreats, (long)c.stagnation_drops,
                (long)c.stagnation_stops, (long)c.zero_motion_exhausted, (long)c.zero_motion_cap_hits,
                (long)c.ride_sign_vetoes, (long)c.ascent_rides, (long)c.saddle_exits, (long)c.saddle_exit_misses,
                (long)c.rank_repairs, (long)c.dep_tests);
#ifdef HVEN_TESTING
    const auto &R = T::QpWalkObserver::records;
    std::map<std::string, long> path_n;
    std::map<int, long> act_n;
    for (const auto &r : R) {
        std::string k = r.eqp_path == T::QpWalkEqpPath::kBorderSchur  ? "border"
                        : r.eqp_path == T::QpWalkEqpPath::kEliminated ? "elim"
                        : r.eqp_path == T::QpWalkEqpPath::kEmptySystem ? "empty"
                                                                       : "none";
        if (r.border_attempted) k += r.border_latched ? "+latched" : "+attempted";
        if (r.k0_rebuilt) k += "+k0rebuilt";
        k += "|pf=" + std::to_string(std::min<long>(r.pass_factorizations, 3));
        ++path_n[k];
        ++act_n[static_cast<int>(r.action)];
    }
    for (const auto &[k, v] : path_n) std::printf("PATH %-50s %ld\n", k.c_str(), v);
    for (const auto &[k, v] : act_n) std::printf("ACT %d %ld\n", k, v);
    if (!csv.empty()) {
        FILE *f = std::fopen(csv.c_str(), "w");
        std::fprintf(f, "minor,action,block,bidx,path,latched,k0rb,bdim,pf,su,ws_in,ws_out,alpha,deg,ride,"
                        "rel_idx,curv,ride_out,iv,ineg,ineg_exp,obj,xinf,sig,asc,slope,php,inface\n");
        for (const auto &r : R)
            std::fprintf(f, "%ld,%d,%d,%ld,%d,%d,%d,%ld,%ld,%ld,%ld,%ld,%.3e,%d,%d,%ld,%.3e,%d,%d,%ld,%ld,%.12e,%.6e,%016llx,%d,%.4e,%.4e,%d\n",
                         (long)r.minor, (int)r.action, (int)r.block, (long)r.block_index, (int)r.eqp_path,
                         r.border_latched, r.k0_rebuilt, (long)r.border_dim, (long)r.pass_factorizations,
                         (long)r.pass_schur_updates, (long)r.ws_size_in, (long)r.ws_size_out, r.alpha,
                         (int)r.degenerate, (int)r.ride, (long)r.released_index, r.curvature,
                         (int)r.ride_outcome, r.inertia_verdict, (long)r.inertia_neg,
                         (long)r.inertia_expected_neg, r.obj_in, r.x_inf, (unsigned long long)r.ws_sig_in,
                         (int)r.ascent_ride, r.step_slope, r.step_php, r.step_in_face);
        std::fclose(f);
    }
#endif
    return 0;
}
