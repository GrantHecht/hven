#!/usr/bin/env python3
import sys
S = "/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/.scratch/e4-fix5/src"
SQ = S + "/src/drivers/sqp_solver.cpp"
def sub1(t, a, b, w):
    n = t.count(a)
    if n != 1: raise SystemExit("ANCHOR %s x%d\n%r" % (w, n, a[:200]))
    return t.replace(a, b)
t = open(SQ).read()
if "HVEN_F5" in t: raise SystemExit("already patched")

HDR = '''#include <hven/drivers/sqp_solver.h>
// ===== M7 W2 E4 FIX5 TEMPORARY DIAGNOSTIC INSTRUMENTATION (scratch only) =====
#include <cstdio>
#include <cstdlib>
namespace hven::solvers::f5d {
inline bool trace() { static const bool b = std::getenv("HVEN_F5_TRACE") != nullptr; return b; }
} // namespace hven::solvers::f5d
'''
t = sub1(t, "#include <hven/drivers/sqp_solver.h>\n", HDR, "sq-header")

# the QP-status routing decision
A = """    if (mj.qs.status == QpStatus::kInfeasible || e2_termination_to_elastic) {"""
B = """    if (f5d::trace())
        std::fprintf(stderr, "F5QP depth=%d major=%ld delta=%.6g qp_status=%d owns=%d minors=%ld facts=%ld h=%.10g box=%.10g f=%.10g cert=%ld blk=%ld zm=%ld certrej=%ld e2route=%d ROUTE=%s\\n",
                     is_restoration_sub_driver_ ? 1 : 0, (long)iter, st.delta, (int)mj.qs.status,
                     (int)mj.walk_owns_this_qp, (long)mj.qs.counters.minor_iters,
                     (long)mj.qs.counters.factorizations, constraint_violation_l1(st.ev),
                     box_violation_l1(st.x, seam.lower(), seam.upper()), st.ev.f,
                     (long)mj.qs.counters.dep_infeasible_certificates,
                     (long)mj.qs.counters.dep_blocker_terminations,
                     (long)mj.qs.counters.zero_motion_cap_hits,
                     (long)mj.qs.counters.dep_certificate_rejections,
                     (int)e2_termination_to_elastic,
                     (mj.qs.status == QpStatus::kInfeasible || e2_termination_to_elastic)
                         ? "ELASTIC_LADDER" : "ordinary");
    if (mj.qs.status == QpStatus::kInfeasible || e2_termination_to_elastic) {"""
t = sub1(t, A, B, "sq-P1-route")

# per-major
A = """        const MajorOutcome outcome = run_major(st, mj, seam, bridge, warm, budget, iter);"""
B = """        const SqpCounters f5_before = st.out.counters;
        const MajorOutcome outcome = run_major(st, mj, seam, bridge, warm, budget, iter);
        if (f5d::trace())
            std::fprintf(stderr, "F5MAJ depth=%d major=%ld outcome=%d qp_status=%d verdict=%d elastic=%d h=%.10g f=%.17g tr=%.6g step=%.6g minors=%ld | d_eqp_it=%ld d_refus=%ld d_polish=%ld d_drift=%ld d_tp_s=%ld d_tp_r=%ld d_tp_a=%ld d_susp=%ld d_vref=%ld d_rej=%ld d_soc=%ld d_elastic=%ld d_resto=%ld h_now=%.10g\\n",
                         is_restoration_sub_driver_ ? 1 : 0, (long)iter, (int)outcome,
                         (int)mj.row.qp_status, (int)mj.row.verdict, (int)mj.row.elastic_applied,
                         mj.row.violation_l1, mj.row.f, mj.row.tr_radius, mj.row.step_norm,
                         (long)mj.row.qp_minor_iters,
                         (long)(st.out.counters.eqp_refine_iters - f5_before.eqp_refine_iters),
                         (long)(st.out.counters.eqp_residual_refusals - f5_before.eqp_residual_refusals),
                         (long)(st.out.counters.eqp_polish_steps - f5_before.eqp_polish_steps),
                         (long)(st.out.counters.eqp_row_drift_refusals - f5_before.eqp_row_drift_refusals),
                         (long)(st.out.counters.terminal_polish_solves - f5_before.terminal_polish_solves),
                         (long)(st.out.counters.terminal_polish_rejected - f5_before.terminal_polish_rejected),
                         (long)(st.out.counters.terminal_polish_adopted - f5_before.terminal_polish_adopted),
                         (long)(st.out.counters.suspect_escalations - f5_before.suspect_escalations),
                         (long)(st.out.counters.verdict_refine_steps - f5_before.verdict_refine_steps),
                         (long)(st.out.counters.rejected_steps - f5_before.rejected_steps),
                         (long)(st.out.counters.soc_steps - f5_before.soc_steps),
                         (long)(st.out.counters.elastic_activations - f5_before.elastic_activations),
                         (long)(st.out.counters.restoration_iters - f5_before.restoration_iters),
                         constraint_violation_l1(st.ev));"""
t = sub1(t, A, B, "sq-P2-major")

# the QP-failure finish
A = """        case MajorOutcome::kFinishQpFailure:"""
B = """        case MajorOutcome::kFinishQpFailure:
            if (f5d::trace())
                std::fprintf(stderr, "F5FINISH depth=%d major=%ld kFinishQpFailure qp_status=%d -> SolveStatus=%d f=%.17g\\n",
                             is_restoration_sub_driver_ ? 1 : 0, (long)iter, (int)mj.qs.status,
                             (int)map_status(mj.qs.status), mj.row.f);"""
t = sub1(t, A, B, "sq-P3-finish")

# the routed-failure decision
A = """            if (shrink_hits_floor(st.delta)) {"""
B = """            if (f5d::trace())
                std::fprintf(stderr, "F5ROUTE depth=%d major=%ld routed_failure %ld/%ld delta=%.6g floor=%d\\n",
                             is_restoration_sub_driver_ ? 1 : 0, (long)iter,
                             (long)st.qp_failures_in_a_row, (long)opts_.qp_failure_budget,
                             st.delta, (int)shrink_hits_floor(st.delta));
            if (shrink_hits_floor(st.delta)) {"""
t = sub1(t, A, B, "sq-P4-routed")

open(SQ, "w").write(t)
print("sqp_solver.cpp: 4 probes applied")
