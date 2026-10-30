// M7 W2 NC2 scratch (after the NC1 scratch tool): replay D4 dumps (tests/qp/support/qp_dump.h
// loader) on a fresh QpEngine. usage: dump_replay [--e2 0|1] [--algebra border|refactorize|dump] DUMP...
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "support/qp_dump.h"
using namespace hven;
using namespace hven::solvers;
#ifndef NC2_COUNTERS
#define NC2_COUNTERS 0
#endif
int main(int argc, char **argv) {
    int e2 = -1;
    std::string alg = "dump";
    std::string certdir;
    std::vector<std::string> files;
    for (int a = 1; a < argc; ++a) {
        std::string s = argv[a];
        if (s == "--e2") e2 = std::atoi(argv[++a]);
        else if (s == "--algebra") alg = argv[++a];
        else if (s == "--cert") certdir = argv[++a];
        else files.push_back(s);
    }
    for (const auto &f : files) {
        const test_support::QpDump d = test_support::load_qp_dump(f);
        QpOptions o = d.opts;
        if (e2 >= 0) o.independent_working_set = e2 != 0;
        if (alg == "border") o.ws_algebra = WorkingSetLinearAlgebra::kSchurBorder;
        if (alg == "refactorize") o.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;
        const QpSolution s = test_support::solve_qp_dump(d, o);
        const auto k = test_support::qp_dump_kkt(d, s);
        const QpCounters &c = s.counters;
        std::printf("%s status=%d minors=%ld facts=%ld f=%.12g eq=%.2e in=%.2e box=%.2e stat=%.2e sgn=%.2e comp=%.2e "
                    "sfirst=%ld unlocks=%ld rereads=%ld flips=%ld exch=%ld zmexh=%ld zmcap=%ld blk=%ld retreats=%ld cert=%ld tol=%ld rej=%ld stall_admits=%ld undec=%ld deferred=%ld tref=%ld tresc=%ld grey=%ld greydep=%ld rideblk=%ld rexch=%ld undo=%ld signv=%ld passov=%ld",
                    f.c_str(), (int)s.status, (long)c.minor_iters, (long)c.factorizations, test_support::qp_dump_objective(d, s.x),
                    k.eq, k.ineq, k.box, k.stationarity, k.sign, k.complementarity,
                    (long)c.dep_structural_first, (long)c.dep_unlocks, (long)c.dep_inertia_rereads, (long)c.dep_inertia_reread_flips,
                    (long)c.ws_exchanges, (long)c.zero_motion_exhausted, (long)c.zero_motion_cap_hits, (long)c.dep_blocker_terminations,
                    (long)c.dep_border_retreats, (long)c.dep_infeasible_certificates, (long)c.dep_certificate_toleranced, (long)c.dep_certificate_rejections,
                    (long)c.dep_stall_admits, (long)c.dep_undecidable, (long)c.dep_deferred_adds, (long)c.terminal_mu_refines, (long)c.terminal_mu_refine_rescues, (long)c.dep_grey_rereads, (long)c.dep_grey_reread_dependent,
                    (long)c.dep_ride_blockers, (long)c.ride_exchanges, (long)c.ride_declines_fallthrough_blocked,
                    (long)c.ride_sign_vetoes, (long)c.dep_passovers);
    std::printf(" refuse=%ld residiters=%ld drift=%ld", (long)c.eqp_residual_refusals, (long)c.eqp_refine_iters, (long)c.eqp_row_drift_refusals);
        if (!certdir.empty()) {
            std::string base = f;
            const auto sl = base.find_last_of('/');
            std::string qpn = base.substr(sl + 1);
            std::string fam = base.substr(0, sl);
            fam = fam.substr(fam.find_last_of('/') + 1);
            const std::string outp = certdir + "/" + fam + "_" + qpn.substr(0, qpn.size() - 4) + "_" + alg + ".cert";
            FILE *fp = std::fopen(outp.c_str(), "w");
            if (fp) {
                const auto &ct = s.farkas;
                std::fprintf(fp, "present %d\nblocker_is_ineq %d\nblocker %ld\nsign %.17g\n",
                             (int)ct.present, (int)ct.blocker_is_ineq, (long)ct.blocker, ct.sign);
                std::fprintf(fp, "implied %.17g\nlimit %.17g\nmargin %.17g\nresidual %.17g\nexact_tol %.17g\n",
                             ct.implied, ct.limit, ct.margin, ct.residual, ct.exact_tol);
                std::fprintf(fp, "status %d\n", (int)s.status);
                const auto vecout = [&](const char *nm, const Vec &v) {
                    std::fprintf(fp, "%s %ld\n", nm, (long)v.size());
                    for (Index i = 0; i < v.size(); ++i) std::fprintf(fp, "%.17g\n", v(i));
                };
                vecout("y_e", ct.y_e);
                vecout("y_i", ct.y_i);
                vecout("box_part", ct.box_part);
                vecout("x", s.x);
                std::fclose(fp);
            }
        }
#if NC2_COUNTERS
        std::printf(" predsign=%ld eqviol=%ld",
                    (long)c.ride_predicted_signs, (long)c.eq_drift_violations);
#endif
        std::printf("\n");
        std::fflush(stdout);
    }
    return 0;
}
