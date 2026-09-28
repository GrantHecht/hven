#!/usr/bin/env python3
"""E4 FIX5 diagnostic instrumentation -- scratch only, env-gated, never committed.
Applies to .scratch/e4-fix5/src (a copy of hven 84fb520e == ff02789b sources).
Idempotent: refuses if the marker is already present."""
import sys, os
S = "/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/.scratch/e4-fix5/src"
QE = S + "/src/qp/qp_engine.cpp"
SQ = S + "/src/drivers/sqp_solver.cpp"

def rd(p): return open(p).read()
def wr(p, t): open(p, "w").write(t)

def sub1(t, anchor, new, where):
    n = t.count(anchor)
    if n != 1:
        raise SystemExit("ANCHOR %s occurs %d times (need 1):\n%r" % (where, n, anchor[:200]))
    return t.replace(anchor, new)

# ---------------------------------------------------------------- qp_engine.cpp
t = rd(QE)
if "HVEN_F5" in t:
    raise SystemExit("qp_engine.cpp already patched")

HDR = r'''#include <hven/detail/qp/qp_engine.h>
// ===== M7 W2 E4 FIX5 TEMPORARY DIAGNOSTIC INSTRUMENTATION (scratch only) =====
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
namespace hven::solvers::f5 {
inline bool trace() { static const bool b = std::getenv("HVEN_F5_TRACE") != nullptr; return b; }
inline const char *dump_dir() { static const char *d = std::getenv("HVEN_F5_QPDUMP"); return d; }
inline bool dump_all() { static const bool b = std::getenv("HVEN_F5_DUMPALL") != nullptr; return b; }
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
} // namespace hven::solvers::f5
'''
t = sub1(t, "#include <hven/detail/qp/qp_engine.h>\n", HDR, "qe-header")

# P1: solve begin
A = """    std::unique_ptr<detail::KktFactor> diag_kkt;
    EliminatedFace diag_face;
#endif
    for (Index iter = 0; !crossed_bounds && iter < eff_max_iter; ++iter) {"""
B = """    std::unique_ptr<detail::KktFactor> diag_kkt;
    EliminatedFace diag_face;
#endif
    const long f5_solve = f5::solve_seq++;
    if (f5::trace())
        std::fprintf(stderr, "F5SOLVE begin=%ld n=%ld me=%ld mi=%ld warm=%d seed=%d tr=%.6g delta=%.3g mu=%.3g maxit=%ld algebra=%d indep=%d\\n",
                     f5_solve, (long)n, (long)me, (long)mi, (int)warm, (int)(seed != nullptr),
                     eff_opts.tr_radius, eff_opts.primal_delta, eff_opts.dual_mu, (long)eff_max_iter,
                     (int)opts_.ws_algebra, (int)eff_opts.independent_working_set);
    for (Index iter = 0; !crossed_bounds && iter < eff_max_iter; ++iter) {"""
t = sub1(t, A, B, "qe-P1-solve-begin")

# P2: refine_on_face refusal
A = """    if (eqp.residual_refused) {
        // THE EQP-SITE RESIDUAL CHECK (E4-4): the face's own solve does not solve
        // its own system, so there is no refined point to offer the caller.
        ++out.counters.eqp_residual_refusals;
        return false;
    }"""
B = """    if (eqp.residual_refused) {
        // THE EQP-SITE RESIDUAL CHECK (E4-4): the face's own solve does not solve
        // its own system, so there is no refined point to offer the caller.
        if (f5::trace())
            std::fprintf(stderr, "F5REF site=refine_on_face solve=%ld resid=%.6e ratio=%.6e iters=%ld polish=%ld ACTION=return_false\\n",
                         f5::solve_seq - 1, eqp.row_residual, eqp.residual_ratio,
                         (long)eqp.residual_refine_iters, (long)eqp.polish_steps);
        ++out.counters.eqp_residual_refusals;
        return false;
    }"""
t = sub1(t, A, B, "qe-P2-refine-on-face")

# P3: eliminated_candidate refusal
A = """    if (res.residual_refused) {
        ++counters.eqp_residual_refusals;
    }"""
B = """    if (res.residual_refused) {
        if (f5::trace())
            std::fprintf(stderr, "F5REF site=eliminated solve=%ld resid=%.6e ratio=%.6e iters=%ld polish=%ld ACTION=returned_to_walk\\n",
                         f5::solve_seq - 1, res.row_residual, res.residual_ratio,
                         (long)res.residual_refine_iters, (long)res.polish_steps);
        ++counters.eqp_residual_refusals;
    }"""
t = sub1(t, A, B, "qe-P3-eliminated")

# P4: bordered refusal
A = """        if (res.residual_refused) {"""
B = """        if (res.residual_refused) {
            if (f5::trace())
                std::fprintf(stderr, "F5REF site=border solve=%ld resid=%.6e ratio=%.6e iters=%ld polish=%ld ACTION=retreat_to_eliminated\\n",
                             f5::solve_seq - 1, res.row_residual, res.residual_ratio,
                             (long)res.residual_refine_iters, (long)res.polish_steps);"""
t = sub1(t, A, B, "qe-P4-border")

# P5: the refusal/drift/escalation site
A = """        if (eqp.residual_refused || eqp_not_finite || eqp_drifted) {
            if (drift_only) {
                ++counters.eqp_row_drift_refusals;
            }
            if (counters.suspect_escalations < detail::kMaxSuspectEscalations) {"""
B = """        if (eqp.residual_refused || eqp_not_finite || eqp_drifted) {
            if (f5::trace())
                std::fprintf(stderr, "F5ESC solve=%ld it=%ld refused=%d nonfinite=%d drifted=%d drift_only=%d esc_now=%ld max=%ld delta=%.3g resid=%.6e ratio=%.6e BRANCH=%s\\n",
                             f5::solve_seq - 1, (long)iter, (int)eqp.residual_refused,
                             (int)eqp_not_finite, (int)eqp_drifted, (int)drift_only,
                             (long)counters.suspect_escalations, (long)detail::kMaxSuspectEscalations,
                             eff_opts.primal_delta, eqp.row_residual, eqp.residual_ratio,
                             counters.suspect_escalations < detail::kMaxSuspectEscalations
                                 ? "escalate"
                                 : (eqp_not_finite ? "terminal_nonfinite" : "ADVISORY_STEP_TOWARD_REFUSED"));
            if (drift_only) {
                ++counters.eqp_row_drift_refusals;
            }
            if (counters.suspect_escalations < detail::kMaxSuspectEscalations) {"""
t = sub1(t, A, B, "qe-P5-escalation")

# P6: terminal classification
A = """                if (worst > 0.0) {
                    status = QpStatus::kInfeasible;
                } else if (is_runaway(qp, x, ws, eff_opts)) {"""
B = """                if (f5::trace())
                    std::fprintf(stderr, "F5TERM solve=%ld it=%ld worst=%.6e refined=%d vrefine_steps=%ld verdict=%d rows=%zu pins=%ld runaway=%d STATUS=%s\\n",
                                 f5::solve_seq - 1, (long)iter, worst, (int)refined,
                                 (long)counters.verdict_refine_steps, (int)verdict,
                                 ws.active_ineq().size(), (long)(n - ws.num_free()),
                                 (int)(worst > 0.0 ? 0 : (int)is_runaway(qp, x, ws, eff_opts)),
                                 worst > 0.0 ? "kInfeasible(terminal)" : "continue_below");
                if (worst > 0.0) {
                    status = QpStatus::kInfeasible;
                } else if (is_runaway(qp, x, ws, eff_opts)) {"""
t = sub1(t, A, B, "qe-P6-terminal")

# P7: AddExit
A = """                if (add_exit == AddExit::kInfeasible) {
                    ++counters.dep_infeasible_certificates;
                    status = QpStatus::kInfeasible;"""
B = """                if (f5::trace())
                    std::fprintf(stderr, "F5ADDEXIT solve=%ld it=%ld add_exit=%d rows=%zu pins=%ld STATUS=%s\\n",
                                 f5::solve_seq - 1, (long)iter, (int)add_exit,
                                 ws.active_ineq().size(), (long)(n - ws.num_free()),
                                 add_exit == AddExit::kInfeasible ? "kInfeasible(add-exit CERTIFICATE)"
                                 : add_exit == AddExit::kToleranced ? "kNumericalError(toleranced)"
                                                                    : "kNumericalError(blocker/rejected)");
                if (add_exit == AddExit::kInfeasible) {
                    ++counters.dep_infeasible_certificates;
                    status = QpStatus::kInfeasible;"""
t = sub1(t, A, B, "qe-P7-addexit")

# P8: terminal polish stages
A = """    if (adopt_polished_candidate(qp, ws, pass_answer, before, opts, x, lambda_e, lambda_i, z)) {
        ++counters.terminal_polish_adopted;
        return true;
    }"""
B = """    if (f5::trace())
        std::fprintf(stderr, "F5POLISH solve=%ld ENTER before_stat=%.6e before_compl=%.6e before_feas=%.6e pass_polish=%ld pass_refused=%d\\n",
                     f5::solve_seq - 1, before.stationarity, before.complementarity,
                     before.feasibility, (long)pass_answer.polish_steps,
                     (int)pass_answer.residual_refused);
    if (adopt_polished_candidate(qp, ws, pass_answer, before, opts, x, lambda_e, lambda_i, z)) {
        ++counters.terminal_polish_adopted;
        if (f5::trace()) std::fprintf(stderr, "F5POLISH solve=%ld STAGE1_ADOPTED\\n", f5::solve_seq - 1);
        return true;
    }"""
t = sub1(t, A, B, "qe-P8-polish-stage1")

A = """    if (!adopt_polished_candidate(qp, ws, r, before, opts, x, lambda_e, lambda_i, z)) {
        ++counters.terminal_polish_rejected;
        return false;
    }
    ++counters.terminal_polish_adopted;
    return true;
}"""
B = """    if (!adopt_polished_candidate(qp, ws, r, before, opts, x, lambda_e, lambda_i, z)) {
        ++counters.terminal_polish_rejected;
        if (f5::trace()) {
            const detail::TerminalKkt cand = detail::terminal_kkt(qp, ws, r.x, r.lambda_e, lambda_i, z);
            std::fprintf(stderr, "F5POLISH solve=%ld STAGE2_REJECTED cand_stat=%.6e cand_compl=%.6e cand_feas=%.6e r_polish=%ld r_refused=%d\\n",
                         f5::solve_seq - 1, cand.stationarity, cand.complementarity,
                         cand.feasibility, (long)r.polish_steps, (int)r.residual_refused);
        }
        return false;
    }
    ++counters.terminal_polish_adopted;
    if (f5::trace()) std::fprintf(stderr, "F5POLISH solve=%ld STAGE2_ADOPTED\\n", f5::solve_seq - 1);
    return true;
}"""
t = sub1(t, A, B, "qe-P8b-polish-stage2")

# P9: run end + the dump (kInfeasible INCLUDED, which the D4 writer did not dump)
A = """    // Emit a record to the ledger if attached."""
B = r'''    if (f5::trace())
        std::fprintf(stderr, "F5SOLVE end=%ld status=%d minors=%ld facts=%ld eqp_refine_iters=%ld eqp_refusals=%ld eqp_polish=%ld drift_refusals=%ld tp_solves=%ld tp_rej=%ld tp_adopt=%ld susp_esc=%ld vrefine=%ld border_refine=%ld cert=%ld cert_rej=%ld blk_term=%ld\n",
                     f5_solve, (int)status, (long)counters.minor_iters, (long)counters.factorizations,
                     (long)counters.eqp_refine_iters, (long)counters.eqp_residual_refusals,
                     (long)counters.eqp_polish_steps, (long)counters.eqp_row_drift_refusals,
                     (long)counters.terminal_polish_solves, (long)counters.terminal_polish_rejected,
                     (long)counters.terminal_polish_adopted, (long)counters.suspect_escalations,
                     (long)counters.verdict_refine_steps, (long)counters.border_refine_steps,
                     (long)counters.dep_infeasible_certificates,
                     (long)counters.dep_certificate_rejections,
                     (long)counters.dep_blocker_terminations);
    if (const char *dd = f5::dump_dir();
        dd != nullptr && (f5::dump_all() || status == QpStatus::kNumericalError ||
                          status == QpStatus::kMaxIter || status == QpStatus::kInfeasible)) {
        const long seq = f5::dump_seq++;
        std::ofstream o(std::string(dd) + "/qp" + std::to_string(seq) + ".txt");
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
        std::snprintf(b, sizeof b, "result status=%d minors=%ld facts=%ld blk_term=%ld cert=%ld cert_rej=%ld zmcap=%ld zmexh=%ld retreats=%ld exch=%ld deferred=%ld undec=%ld maxiter=%ld\n",
                      (int)status, (long)counters.minor_iters, (long)counters.factorizations,
                      (long)counters.dep_blocker_terminations,
                      (long)counters.dep_infeasible_certificates,
                      (long)counters.dep_certificate_rejections,
                      (long)counters.zero_motion_cap_hits, (long)counters.zero_motion_exhausted,
                      (long)counters.dep_border_retreats, (long)counters.ws_exchanges,
                      (long)counters.dep_deferred_adds, (long)counters.dep_undecidable,
                      (long)eff_max_iter);
        o << b;
        f5::dsp(o, "H", qp_in.H);
        f5::dvec(o, "g", qp_in.g);
        f5::dsp(o, "Ae", qp_in.Ae);
        f5::dvec(o, "be", qp_in.be);
        f5::dsp(o, "Ai", qp_in.Ai);
        f5::dvec(o, "bi", qp_in.bi);
        f5::dvec(o, "lower", qp_in.lower);
        f5::dvec(o, "upper", qp_in.upper);
        if (seed != nullptr) {
            f5::dvec(o, "seed_x", seed->x);
            std::vector<long> sb, sa;
            for (BoundState bs : seed->bound_state) sb.push_back((long)bs);
            for (bool a : seed->ineq_active) sa.push_back(a ? 1 : 0);
            f5::divec(o, "seed_bound_state", sb);
            f5::divec(o, "seed_ineq_active", sa);
        }
        f5::dvec(o, "x_final", out.x);
        std::vector<long> fb, fa;
        for (BoundState bs : ws.bound_state()) fb.push_back((long)bs);
        for (Index r : ws.active_ineq()) fa.push_back((long)r);
        f5::divec(o, "final_bound_state", fb);
        f5::divec(o, "final_active_ineq", fa);
        std::fprintf(stderr, "F5DUMP seq=%ld solve=%ld status=%d n=%ld me=%ld mi=%ld warm=%d tr=%.6g minors=%ld facts=%ld cert=%ld\n",
                     seq, f5_solve, (int)status, (long)n, (long)me, (long)mi, (int)warm,
                     eff_opts.tr_radius, (long)counters.minor_iters, (long)counters.factorizations,
                     (long)counters.dep_infeasible_certificates);
    }

    // Emit a record to the ledger if attached.'''
t = sub1(t, A, B, "qe-P9-run-end")
wr(QE, t)
print("qp_engine.cpp: 9 probes applied")
