#!/usr/bin/env python3
"""PF-1 scratch diagnostic: env-gated D4QP dump + per-solve summary in QpEngine::run (scratch tree only)."""
import sys, re
p = sys.argv[1] + "/src/qp/qp_engine.cpp"
s = open(p).read()

helpers = r'''
#include <cstdlib>
#include <fstream>
// ===== PF-1 TEMPORARY DIAGNOSTIC INSTRUMENTATION (scratch only) =====
namespace hven::pf1 {
inline const char *dump_dir() { static const char *d = std::getenv("HVEN_PF1_QPDUMP"); return d; }
inline bool summary() { static const bool b = std::getenv("HVEN_PF1_SUMMARY") != nullptr; return b; }
inline long solve_seq = 0;
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
} // namespace hven::pf1
'''
# insert helpers after the last #include line at file top
idx = [m.end() for m in re.finditer(r'^#include .*\n', s, re.M)][-1]
s = s[:idx] + helpers + s[idx:]

anchor = "    // Emit a record to the ledger if attached.\n    if (ledger_ != nullptr) {"
assert s.count(anchor) == 1
dump = r'''    {
        const long pf1_solve = pf1::solve_seq++;
        const auto &c = counters;
        if (pf1::summary()) {
            std::fprintf(stderr,
                "PF1SOLVE solve=%ld status=%d n=%ld me=%ld mi=%ld warm=%d seed=%d hot=%d minors=%ld facts=%ld schur=%ld "
                "adds=%ld drops=%ld adds_b=%ld drops_b=%ld degen=%ld degen_run_max=%ld dep_tests=%ld dep_test_solves=%ld "
                "dep_dependent=%ld dep_undec=%ld entry_rm=%ld recheck_rm=%ld uncert_trims=%ld uncert_fallbacks=%ld "
                "untested_growth=%ld ws_count_viol=%ld exch=%ld deferred=%ld passovers=%ld blk_term=%ld retreats=%ld "
                "standstill=%ld zm_exh=%ld zm_cap=%ld reentry_ex=%ld stag_drops=%ld stag_stops=%ld suspect=%ld "
                "resid_ref=%ld drift_ref=%ld polish=%ld latch_cap=%ld latch_cond=%ld rank_rep=%ld known_dep_ref=%ld "
                "stall_admits=%ld unrun=%ld repair_pins=%ld implied_pass=%ld refused_exch=%ld shift_adds=%ld "
                "ride_exch=%ld saddle=%ld ascent=%ld k0_reused=%d hkc=%d\n",
                pf1_solve, (int)status, (long)n, (long)me, (long)mi, (int)warm, seed != nullptr ? 1 : 0,
                hot != nullptr ? 1 : 0, (long)c.minor_iters, (long)c.factorizations, (long)c.schur_updates,
                (long)c.ws_adds, (long)c.ws_drops, (long)c.ws_adds_bound, (long)c.ws_drops_bound,
                (long)c.degenerate_steps, (long)c.degenerate_run_max, (long)c.dep_tests, (long)c.dep_test_solves,
                (long)c.dep_dependent, (long)c.dep_undecidable, (long)c.entry_dependency_removals,
                (long)c.dep_recheck_removals, (long)c.dep_uncertified_trims, (long)c.dep_uncertified_fallbacks,
                (long)c.dep_untested_growth, (long)c.ws_count_violations, (long)c.ws_exchanges,
                (long)c.dep_deferred_adds, (long)c.dep_passovers, (long)c.dep_blocker_terminations,
                (long)c.dep_border_retreats, (long)c.standstill_retreats, (long)c.zero_motion_exhausted,
                (long)c.zero_motion_cap_hits, (long)c.reentry_exemptions, (long)c.stagnation_drops,
                (long)c.stagnation_stops, (long)c.suspect_escalations, (long)c.eqp_residual_refusals,
                (long)c.eqp_row_drift_refusals, (long)c.eqp_polish_steps, (long)c.border_latches_cap,
                (long)c.border_latches_conditioning, (long)c.rank_repairs, (long)c.dep_known_dependent_refusals,
                (long)c.dep_stall_admits, (long)c.dep_unrun_tests, (long)c.dep_repair_screen_pins,
                (long)c.dep_implied_passovers, (long)c.dep_refused_exchanges, (long)c.shift_adds,
                (long)c.ride_exchanges, (long)c.saddle_exits, (long)c.ascent_rides, (int)c.k0_reused,
                (int)opts_.hessian_known_convex);
        }
        if (const char *dd = pf1::dump_dir(); dd != nullptr) {
            std::ofstream o(std::string(dd) + "/qp" + std::to_string(pf1_solve) + ".txt");
            o << "D4QP 1\n";
            o << "dims " << n << " " << me << " " << mi << "\n";
            char b[512];
            std::snprintf(b, sizeof b, "opts %.17g %.17g %.17g %.17g %ld %ld %.17g %d %.17g %d %d\n",
                          opts_.primal_regularization, opts_.dual_regularization, opts_.feasibility_tol,
                          opts_.optimality_tol, (long)opts_.max_iterations.value_or(0),
                          (long)opts_.max_schur_border_dimension, opts_.schur_condition_max,
                          opts_.working_set_algebra == QpWorkingSetAlgebra::kRefactorize ? 0 : 1,
                          opts_.trust_region_radius, (int)opts_.independent_working_set, threads_);
            o << b;
            std::snprintf(b, sizeof b, "overrides %.17g %.17g %.17g\n", overrides.trust_region_radius,
                          overrides.primal_regularization, overrides.dual_regularization);
            o << b;
            o << "warm " << (int)warm << " seed " << (seed != nullptr ? 1 : 0) << " hot " << (hot != nullptr ? 1 : 0)
              << " k0_reused " << (int)counters.k0_reused << "\n";
            std::snprintf(b, sizeof b, "result status=%d minors=%ld facts=%ld hkc=%d\n", (int)status,
                          (long)counters.minor_iters, (long)counters.factorizations, (int)opts_.hessian_known_convex);
            o << b;
            pf1::dsp(o, "H", qp_in.H);
            pf1::dvec(o, "g", qp_in.g);
            pf1::dsp(o, "Ae", qp_in.Ae);
            pf1::dvec(o, "be", qp_in.be);
            pf1::dsp(o, "Ai", qp_in.Ai);
            pf1::dvec(o, "bi", qp_in.bi);
            pf1::dvec(o, "lower", qp_in.lower);
            pf1::dvec(o, "upper", qp_in.upper);
            if (seed != nullptr) {
                pf1::dvec(o, "seed_x", seed->x);
                std::vector<long> sb, sa;
                for (BoundState bs : seed->bound_state) sb.push_back((long)bs);
                for (bool a : seed->ineq_active) sa.push_back(a ? 1 : 0);
                pf1::divec(o, "seed_bound_state", sb);
                pf1::divec(o, "seed_ineq_active", sa);
            }
            pf1::dvec(o, "x_final", out.x);
            std::vector<long> fb, fa;
            for (BoundState bs : ws.bound_state()) fb.push_back((long)bs);
            for (Index r : ws.active_ineq()) fa.push_back((long)r);
            pf1::divec(o, "final_bound_state", fb);
            pf1::divec(o, "final_active_ineq", fa);
        }
    }
'''
s = s.replace(anchor, dump + anchor)
open(p, "w").write(s)
print("patched", p)
