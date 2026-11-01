#!/usr/bin/env python3
"""M7 W2 C10 SCRATCH ONLY, NEVER COMMITTED. Applied to a COPY of 8cfc12ed sources (.scratch/c10/srcD).
 C10_QPLOG=1     : one C10QP line per QpEngine::run exit on stderr (seq, status, exit SITE = the
                   original source line of the status assignment, dims, warm, tr, minors, facts, counters).
 C10_DUMPDIR=dir : a D4QP v1 dump (e2fix8 format) of every non-optimal solve, up to C10_DUMPMAX (default 400).
usage: patch_qp.py <src/qp/qp_engine.cpp>"""
import sys, re
QE = sys.argv[1]
t = open(QE).read()
if "C10_QPLOG" in t: raise SystemExit("already patched")
def sub1(t, a, b, w):
    n = t.count(a)
    if n != 1: raise SystemExit("ANCHOR %s x%d" % (w, n))
    return t.replace(a, b)
# tag every status assignment with its ORIGINAL line number (comma expression: single statement)
lines = t.split("\n")
pat = re.compile(r"status = (QpStatus::k(NumericalError|Infeasible|MaxIter|Optimal));")
pat2 = re.compile(r"status = (bound_worst > 0\.0 \? QpStatus::kInfeasible : QpStatus::kOptimal);")
ntag = 0
for i, L in enumerate(lines):
    if "QpStatus status = crossed_bounds" in L:
        lines[i] = L + " int c10_site = %d;" % (i + 1); ntag += 1; continue
    m = pat.search(L)
    if m and "out.status" not in L and "rec.status" not in L:
        lines[i] = L.replace(m.group(0), "status = (c10_site = %d, %s);" % (i + 1, m.group(1))); ntag += 1; continue
    m = pat2.search(L)
    if m:
        lines[i] = L.replace(m.group(0), "status = (c10_site = %d, %s);" % (i + 1, m.group(1))); ntag += 1
t = "\n".join(lines)
print("tagged", ntag, file=sys.stderr)
HDR = r'''#include <hven/detail/qp/qp_engine.h>
// ===== M7 W2 C10 TEMPORARY DIAGNOSTIC INSTRUMENTATION (scratch only) =====
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
namespace hven::solvers::c10 {
inline bool qplog() { static const bool b = std::getenv("C10_QPLOG") != nullptr; return b; }
inline const char *dump_dir() { static const char *d = std::getenv("C10_DUMPDIR"); return d; }
inline long dump_max() { static const long v = std::getenv("C10_DUMPMAX") ? std::atol(std::getenv("C10_DUMPMAX")) : 400; return v; }
inline long dump_min() { static const long v = std::getenv("C10_DUMPMIN") ? std::atol(std::getenv("C10_DUMPMIN")) : -1; return v; }
inline long solve_seq = 0;
inline long dump_seq = 0;
inline void dvec(std::ofstream &o, const char *name, const Eigen::VectorXd &v) {
    o << name << " " << v.size() << "\n";
    char b[64];
    for (Eigen::Index i = 0; i < v.size(); ++i) { std::snprintf(b, sizeof b, "%.17g\n", v(i)); o << b; }
}
template <class M> inline void dsp(std::ofstream &o, const char *name, const M &A) {
    o << name << " " << A.rows() << " " << A.cols() << " " << A.nonZeros() << "\n";
    char b[96];
    for (int k = 0; k < A.outerSize(); ++k)
        for (typename M::InnerIterator it(A, k); it; ++it) {
            std::snprintf(b, sizeof b, "%ld %ld %.17g\n", (long)it.row(), (long)it.col(), it.value());
            o << b;
        }
}
inline void divec(std::ofstream &o, const char *name, const std::vector<long> &v) {
    o << name << " " << v.size() << "\n";
    for (long x : v) o << x << "\n";
}
} // namespace hven::solvers::c10
'''
t = sub1(t, "#include <hven/detail/qp/qp_engine.h>\n", HDR, "header")
A = """    for (Index iter = 0; !crossed_bounds && iter < eff_max_iter; ++iter) {"""
ENTRY = r'''    const long c10_solve = c10::solve_seq++;
    const auto c10_t0 = std::chrono::steady_clock::now();
    if (const char *de = std::getenv("C10_DUMPENTRY"); de != nullptr && std::atol(de) == c10_solve &&
                                                       c10::dump_dir() != nullptr) {
        std::ofstream o(std::string(c10::dump_dir()) + "/entry" + std::to_string(c10_solve) + ".txt");
        o << "D4QP 1\n";
        o << "dims " << n << " " << me << " " << mi << "\n";
        char b[512];
        std::snprintf(b, sizeof b, "opts %.17g %.17g %.17g %.17g %ld %ld %.17g %d %.17g %d %d\n",
                      opts_.primal_delta, opts_.dual_mu, opts_.feas_tol, opts_.opt_tol,
                      (long)opts_.max_iter, (long)opts_.schur_cap, opts_.schur_cond_max,
                      (int)opts_.ws_algebra, opts_.tr_radius, (int)opts_.independent_working_set, threads_);
        o << b;
        std::snprintf(b, sizeof b, "overrides %.17g %.17g %.17g\n", overrides.tr_radius,
                      overrides.primal_delta, overrides.dual_mu);
        o << b;
        o << "warm " << (int)warm << " seed " << (seed != nullptr ? 1 : 0) << " hot "
          << (hot != nullptr ? 1 : 0) << " k0_reused " << (int)counters.k0_reused << "\n";
        o << "result entry solve=" << c10_solve << "\n";
        c10::dsp(o, "H", qp_in.H);
        c10::dvec(o, "g", qp_in.g);
        c10::dsp(o, "Ae", qp_in.Ae);
        c10::dvec(o, "be", qp_in.be);
        c10::dsp(o, "Ai", qp_in.Ai);
        c10::dvec(o, "bi", qp_in.bi);
        c10::dvec(o, "lower", qp_in.lower);
        c10::dvec(o, "upper", qp_in.upper);
        if (seed != nullptr) {
            c10::dvec(o, "seed_x", seed->x);
            std::vector<long> sb, sa;
            for (BoundState bs : seed->bound_state) sb.push_back((long)bs);
            for (bool a : seed->ineq_active) sa.push_back(a ? 1 : 0);
            c10::divec(o, "seed_bound_state", sb);
            c10::divec(o, "seed_ineq_active", sa);
        }
        c10::dvec(o, "x_final", x);
        std::vector<long> fb, fa;
        for (BoundState bs : ws.bound_state()) fb.push_back((long)bs);
        for (Index r : ws.active_ineq()) fa.push_back((long)r);
        c10::divec(o, "final_bound_state", fb);
        c10::divec(o, "final_active_ineq", fa);
        std::fprintf(stderr, "C10_DUMPENTRY wrote solve %ld\n", c10_solve);
    }
'''
t = sub1(t, A, ENTRY + A, "solve-begin")
A = """    // Emit a record to the ledger if attached."""
B = r'''    {
        long c10_dump = -1;
        if (const char *dd = c10::dump_dir(); dd != nullptr && (status != QpStatus::kOptimal || (c10::dump_min() >= 0 && counters.minor_iters >= c10::dump_min())) &&
                                               c10::dump_seq < c10::dump_max()) {
            c10_dump = c10::dump_seq++;
            std::ofstream o(std::string(dd) + "/qp" + std::to_string(c10_dump) + ".txt");
            o << "D4QP 1\n";
            o << "dims " << n << " " << me << " " << mi << "\n";
            char b[512];
            std::snprintf(b, sizeof b, "opts %.17g %.17g %.17g %.17g %ld %ld %.17g %d %.17g %d %d\n",
                          opts_.primal_delta, opts_.dual_mu, opts_.feas_tol, opts_.opt_tol,
                          (long)opts_.max_iter, (long)opts_.schur_cap, opts_.schur_cond_max,
                          (int)opts_.ws_algebra, opts_.tr_radius,
                          (int)opts_.independent_working_set, threads_);
            o << b;
            std::snprintf(b, sizeof b, "overrides %.17g %.17g %.17g\n", overrides.tr_radius,
                          overrides.primal_delta, overrides.dual_mu);
            o << b;
            o << "warm " << (int)warm << " seed " << (seed != nullptr ? 1 : 0) << " hot "
              << (hot != nullptr ? 1 : 0) << " k0_reused " << (int)counters.k0_reused << "\n";
            std::snprintf(b, sizeof b, "result status=%d minors=%ld facts=%ld site=%d solve=%ld maxiter=%ld\n",
                          (int)status, (long)counters.minor_iters, (long)counters.factorizations,
                          c10_site, c10_solve, (long)eff_max_iter);
            o << b;
            c10::dsp(o, "H", qp_in.H);
            c10::dvec(o, "g", qp_in.g);
            c10::dsp(o, "Ae", qp_in.Ae);
            c10::dvec(o, "be", qp_in.be);
            c10::dsp(o, "Ai", qp_in.Ai);
            c10::dvec(o, "bi", qp_in.bi);
            c10::dvec(o, "lower", qp_in.lower);
            c10::dvec(o, "upper", qp_in.upper);
            if (seed != nullptr) {
                c10::dvec(o, "seed_x", seed->x);
                std::vector<long> sb, sa;
                for (BoundState bs : seed->bound_state) sb.push_back((long)bs);
                for (bool a : seed->ineq_active) sa.push_back(a ? 1 : 0);
                c10::divec(o, "seed_bound_state", sb);
                c10::divec(o, "seed_ineq_active", sa);
            }
            c10::dvec(o, "x_final", out.x);
            std::vector<long> fb, fa;
            for (BoundState bs : ws.bound_state()) fb.push_back((long)bs);
            for (Index r : ws.active_ineq()) fa.push_back((long)r);
            c10::divec(o, "final_bound_state", fb);
            c10::divec(o, "final_active_ineq", fa);
        }
        if (c10::qplog())
            std::fprintf(stderr,
                         "C10QP solve=%ld status=%d site=%d n=%ld me=%ld mi=%ld warm=%d seed=%d hot=%d tr=%.6g "
                         "minors=%ld facts=%ld cap=%ld zme=%ld zmcap=%ld bret=%ld sstr=%ld stall=%ld refus=%ld "
                         "susp=%ld drift=%ld blkterm=%ld certrej=%ld certtol=%ld ridebl=%ld dump=%ld ws=%ld free=%ld "
                         "wall=%.4f ana=%ld schur=%ld adds=%ld drops=%ld degen=%ld lcap=%ld lcond=%ld lrel=%ld "
                         "stagst=%ld rpins=%ld rbud=%ld rrest=%ld k0re=%d zmex=%ld\n",
                         c10_solve, (int)status, c10_site, (long)n, (long)me, (long)mi, (int)warm,
                         (int)(seed != nullptr), (int)(hot != nullptr), eff_opts.tr_radius,
                         (long)counters.minor_iters, (long)counters.factorizations, (long)eff_max_iter,
                         (long)counters.zero_motion_exhausted, (long)counters.zero_motion_cap_hits,
                         (long)counters.dep_border_retreats, (long)counters.standstill_retreats,
                         (long)counters.dep_stall_admits, (long)counters.eqp_residual_refusals,
                         (long)counters.suspect_escalations, (long)counters.eq_drift_violations,
                         (long)counters.dep_blocker_terminations, (long)counters.dep_certificate_rejections,
                         (long)counters.dep_certificate_toleranced, (long)counters.dep_ride_blockers, c10_dump,
                         (long)ws.active_ineq().size(), (long)ws.num_free(),
                         std::chrono::duration<double>(std::chrono::steady_clock::now() - c10_t0).count(),
                         (long)counters.symbolic_analyses, (long)counters.schur_updates, (long)counters.ws_adds,
                         (long)counters.ws_drops, (long)counters.degenerate_steps, (long)counters.border_latches_cap,
                         (long)counters.border_latches_conditioning, (long)counters.border_latch_releases,
                         (long)counters.stagnation_stops, (long)counters.dep_repair_screen_pins,
                         (long)counters.dep_repair_budget_stops, (long)counters.dep_repair_restarts,
                         (int)counters.k0_reused, (long)counters.zero_motion_exhausted);
    }

    // Emit a record to the ledger if attached.'''
t = sub1(t, A, B, "run-end")
open(QE, "w").write(t)
print("patched", QE)
