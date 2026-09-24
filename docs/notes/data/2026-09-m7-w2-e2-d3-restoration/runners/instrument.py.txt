#!/usr/bin/env python3
"""E2-D3 TEMPORARY instrumentation (scratch only). Env-gated: HVEN_D3=1 prints; HVEN_D3_DUMP=dir dumps
restoration entry/exit points; HVEN_D3_F4=1 routes E2 terminations (kNumericalError with
dep_blocker_terminations/zero_motion_cap_hits/dep_certificate_rejections > 0) to the elastic tier
(E2-D1's F4 experiment, verbatim condition)."""
import sys
p = sys.argv[1] + "/src/drivers/sqp_solver.cpp"
s = open(p).read()
def sub(old, new, count=1):
    global s
    n = s.count(old)
    if n != count:
        sys.exit(f"anchor count {n} != {count}: {old[:80]!r}")
    s = s.replace(old, new)

sub('#include <hven/drivers/sqp_solver.h>\n',
'''#include <hven/drivers/sqp_solver.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
// ===== E2-D3 TEMPORARY DIAGNOSTIC INSTRUMENTATION (scratch only) =====
namespace hven::solvers::d3 {
inline bool on() { static const bool b = std::getenv("HVEN_D3") != nullptr; return b; }
inline bool f4() { static const bool b = std::getenv("HVEN_D3_F4") != nullptr; return b; }
inline const char *dump_dir() { return std::getenv("HVEN_D3_DUMP"); }
inline int dump_seq = 0;
inline void dump_vec(std::ofstream &o, const char *name, const Eigen::VectorXd &v) {
    o << name << " " << v.size() << "\\n";
    for (Eigen::Index i = 0; i < v.size(); ++i) { char b[64]; std::snprintf(b, sizeof b, "%.17g\\n", v(i)); o << b; }
}
template <class M> inline void dump_sp(std::ofstream &o, const char *name, const M &A) {
    o << name << " " << A.rows() << " " << A.cols() << " " << A.nonZeros() << "\\n";
    for (int k = 0; k < A.outerSize(); ++k)
        for (typename M::InnerIterator it(A, k); it; ++it) { char b[96]; std::snprintf(b, sizeof b, "%ld %ld %.17g\\n", (long)it.row(), (long)it.col(), it.value()); o << b; }
}
} // namespace hven::solvers::d3
''')

# 1. per-rung line in the elastic ladder
sub('''        if (qs_e.status != QpStatus::kOptimal) {
            break;
        }
        // MATERIALLY NONZERO IS MEASURED ON THE VIOLATION''',
'''        if (d3::on()) {
            const Vec v3 = qs_e.status == QpStatus::kOptimal ? elastic.slack_violations(qs_e.x) : Vec();
            std::fprintf(stderr, "D3RUNG rho=%.6g status=%d minors=%ld facts=%ld slack_l1=%.10g slack_max=%.6g viol_l1_in=%.10g blk_term=%ld zmcap=%ld cert=%ld cert_rej=%ld\\n",
                         rho, (int)qs_e.status, (long)qs_e.counters.minor_iters, (long)qs_e.counters.factorizations,
                         v3.size() ? v3.lpNorm<1>() : -1.0, v3.size() ? v3.maxCoeff() : -1.0, elastic.violation_l1,
                         (long)qs_e.counters.dep_blocker_terminations, (long)qs_e.counters.zero_motion_cap_hits,
                         (long)qs_e.counters.dep_infeasible_certificates, (long)qs_e.counters.dep_certificate_rejections);
        }
        if (qs_e.status != QpStatus::kOptimal) {
            break;
        }
        // MATERIALLY NONZERO IS MEASURED ON THE VIOLATION''')

sub('''    const bool usable = qs_e.status == QpStatus::kOptimal && (closed || reduced || promises_f);
''', '''    const bool usable = qs_e.status == QpStatus::kOptimal && (closed || reduced || promises_f);
    if (d3::on()) {
        std::fprintf(stderr, "D3LADDER rho0=%.6g final_rho=%.6g status=%d slack_l1=%.10g viol_l1_in=%.10g closed=%d reduced=%d promises_f=%d pred_dec=%.6g usable=%d step_inf=%.6g window=%.6g\\n",
                     rho_0, rho, (int)qs_e.status, slack_l1, elastic.violation_l1, (int)closed, (int)reduced, (int)promises_f,
                     qs_e.status == QpStatus::kOptimal ? predicted_decrease(qp, p_elastic) : 0.0, (int)usable,
                     p_elastic.size() ? p_elastic.lpNorm<Eigen::Infinity>() : 0.0, window);
    }
''')

# 2. the QP answer at the elastic-tier gate, and the F4 experiment
sub('''    bool rho0_ceiling_hit = mj.fallback_report && mj.fallback_report->rho0_ceiling_hit;
    if (mj.qs.status == QpStatus::kInfeasible) {''',
'''    bool rho0_ceiling_hit = mj.fallback_report && mj.fallback_report->rho0_ceiling_hit;
    const bool d3_route_elastic =
        d3::f4() && mj.walk_owns_this_qp && mj.qs.status == QpStatus::kNumericalError &&
        (mj.qs.counters.dep_blocker_terminations > 0 || mj.qs.counters.zero_motion_cap_hits > 0 ||
         mj.qs.counters.dep_certificate_rejections > 0);
    if (d3::on()) {
        std::fprintf(stderr, "D3QP depth=%d major=%ld delta=%.6g status=%d minors=%ld facts=%ld h=%.10g f=%.10g blk_term=%ld zmcap=%ld cert=%ld cert_rej=%ld n=%ld me=%ld mi=%ld f4route=%d resto_used=%d\\n",
                     is_restoration_sub_driver_ ? 1 : 0, (long)iter, st.delta, (int)mj.qs.status,
                     (long)mj.qs.counters.minor_iters, (long)mj.qs.counters.factorizations,
                     constraint_violation_l1(st.ev), st.ev.f,
                     (long)mj.qs.counters.dep_blocker_terminations, (long)mj.qs.counters.zero_motion_cap_hits,
                     (long)mj.qs.counters.dep_infeasible_certificates, (long)mj.qs.counters.dep_certificate_rejections,
                     (long)st.qp.n(), (long)st.qp.me(), (long)st.qp.mi(), (int)d3_route_elastic, (int)st.resto.used);
    }
    if (mj.qs.status == QpStatus::kInfeasible || d3_route_elastic) {''')

# 3. the routed-failure path
sub('''            if (shrink_hits_floor(st.delta)) {
                // PUSHED AFTER THE CALL (R9), as at every request site.''',
'''            if (d3::on()) std::fprintf(stderr, "D3ROUTE depth=%d major=%ld routed failure %ld/%ld delta=%.6g floor=%d\\n", is_restoration_sub_driver_ ? 1 : 0, (long)iter, (long)st.qp_failures_in_a_row, (long)opts_.qp_failure_budget, st.delta, (int)shrink_hits_floor(st.delta));
            if (shrink_hits_floor(st.delta)) {
                // PUSHED AFTER THE CALL (R9), as at every request site.''')
sub('''        push_history(mj.row);
        // No restoration was consulted on this path -- x is still''',
'''        if (d3::on()) std::fprintf(stderr, "D3ROUTE depth=%d major=%ld kFinishQpFailure status=%d failures_in_a_row=%ld retryable=%d\\n", is_restoration_sub_driver_ ? 1 : 0, (long)iter, (int)mj.qs.status, (long)st.qp_failures_in_a_row, (int)qp_failure_is_retryable(st.qp, mj.qs, opts_.feas_tol));
        push_history(mj.row);
        // No restoration was consulted on this path -- x is still''')

# 4. enter_restoration
sub('''    if (!allow_restoration_ || st.resto.used) {
        // The nested case''',
'''    if (d3::on()) std::fprintf(stderr, "D3RESTO_REQ depth=%d major=%ld allow=%d used=%d h=%.10g cand=%d\\n", is_restoration_sub_driver_ ? 1 : 0, (long)iter, (int)allow_restoration_, (int)st.resto.used, constraint_violation_l1(st.ev), (int)(cand.x != nullptr));
    if (!allow_restoration_ || st.resto.used) {
        // The nested case''')
sub('''    const double h_r = constraint_violation_l1(ev_r);
''', '''    const double h_r = constraint_violation_l1(ev_r);
    if (d3::on()) {
        std::fprintf(stderr, "D3RESTO_END sub_status=%d sub_majors=%ld sub_elastic=%ld h_in=%.10g h_r=%.10g f_r=%.10g |x_r-x|inf=%.6g seed_used=%d sub_obj=%.10g sub_kkt_stat=%.6g sub_feas=%.6g\\n",
                     (int)rs.status, (long)rs.counters.major_iters, (long)rs.counters.elastic_activations,
                     excursion_h_in, h_r, ev_r.f, (x_r - st.x).lpNorm<Eigen::Infinity>(), (int)mj.row.restoration_seed_used,
                     rs.f, rs.sqp_stationarity, rs.sqp_feasibility);
    }
    if (const char *dd = d3::dump_dir(); dd != nullptr && x_r.allFinite()) {
        const NlpModel &m3 = bridge.model();
        for (int which = 0; which < 2; ++which) {
            const Vec &xx = which == 0 ? st.x : x_r;
            std::ofstream o(std::string(dd) + "/resto" + std::to_string(d3::dump_seq) + (which == 0 ? "_entry" : "_exit") + ".txt");
            o << "n " << m3.n() << " me " << m3.me() << " mi " << m3.mi() << "\\n";
            d3::dump_vec(o, "x", xx);
            d3::dump_vec(o, "lower", m3.lower());
            d3::dump_vec(o, "upper", m3.upper());
            d3::dump_vec(o, "ce", m3.eval_ce(xx));
            d3::dump_vec(o, "ci", m3.eval_ci(xx));
            d3::dump_sp(o, "Je", m3.eval_jac_e(xx));
            d3::dump_sp(o, "Ji", m3.eval_jac_i(xx));
            d3::dump_vec(o, "lambda_e", rs_lambda_e);
            d3::dump_vec(o, "lambda_i", rs.lambda_i);
            if (which == 1) d3::dump_vec(o, "z", feasibility.original_x(rs.z));
        }
        ++d3::dump_seq;
    }
''')

# 5. every major's outcome
sub('''        const MajorOutcome outcome = run_major(st, mj, seam, bridge, warm, budget, iter);
''', '''        const MajorOutcome outcome = run_major(st, mj, seam, bridge, warm, budget, iter);
        if (d3::on()) {
            std::fprintf(stderr, "D3MAJ depth=%d major=%ld outcome=%d qp_status=%d verdict=%d elastic=%d viol=%.10g f=%.10g tr=%.6g step=%.6g minors=%ld h_now=%.10g\\n",
                         is_restoration_sub_driver_ ? 1 : 0, (long)iter, (int)outcome, (int)mj.row.qp_status, (int)mj.row.verdict,
                         (int)mj.row.elastic_applied, mj.row.violation_l1, mj.row.f, mj.row.tr_radius, mj.row.step_norm,
                         (long)mj.row.qp_minor_iters, constraint_violation_l1(st.ev));
        }
''')

# 6. solve end
sub('''    SqpResult out = solve_impl_body(seam, bridge, *x0_used, warm, budget, std::move(strategy));
''', '''    SqpResult out = solve_impl_body(seam, bridge, *x0_used, warm, budget, std::move(strategy));
    if (d3::on()) std::fprintf(stderr, "D3END depth=%d status=%d majors=%ld resto_iters=%ld certified=%d obj=%.10g feas=%.6g stat=%.6g\\n", is_restoration_sub_driver_ ? 1 : 0, (int)out.status, (long)out.counters.major_iters, (long)out.counters.restoration_iters, (int)out.infeasibility_certified, out.f, out.sqp_feasibility, out.sqp_stationarity);
''')
open(p, "w").write(s)
print("instrumented", p)

# 7. out-of-box start report + HVEN_D3_PROJECT forcing the existing project_start_point lever (top-level solve only)
s = open(p).read()
old = '''    if (opts_.project_start_point) {
        const Vec &lo = seam.lower();'''
assert s.count(old) == 1
s = s.replace(old, '''    if (d3::on()) {
        const Vec &lo3 = seam.lower(); const Vec &up3 = seam.upper(); long c3 = 0; double m3 = 0.0;
        for (Index i = 0; i < n; ++i) { const double v = std::max(lo3[i] - x0[i], x0[i] - up3[i]); if (v > 0) { ++c3; m3 = std::max(m3, v); } }
        std::fprintf(stderr, "D3START depth=%d n=%ld out_of_box=%ld max_box_viol=%.10g project=%d\\n", is_restoration_sub_driver_ ? 1 : 0, (long)n, c3, m3, (int)(opts_.project_start_point || (std::getenv("HVEN_D3_PROJECT") && !is_restoration_sub_driver_)));
    }
    if (opts_.project_start_point || (std::getenv("HVEN_D3_PROJECT") != nullptr && !is_restoration_sub_driver_)) {
        const Vec &lo = seam.lower();''')
open(p, "w").write(s)
print("step 7 applied")

# 8. experiment toggles (scratch only):
#   HVEN_D3_ELASTIC_E2OFF  the elastic ladder's rungs run on an engine with qp.independent_working_set=false
#   HVEN_D3_RESTO_E2OFF    the restoration sub-driver runs with qp.independent_working_set=false
#   HVEN_D3_PROX=<zeta>    the restoration proximity weight (both the wrapper and the certificate gate)
#   HVEN_D3_RESTO_CLAMP    the restoration wrapper's start/centre x is clamped into the box
s = open(p).read()
old = '''                               : run_elastic_ladder(*engine_, st.qp, elastic_seed_source, window,'''
assert s.count(old) == 1
s = s.replace(old, '''                               : run_elastic_ladder(std::getenv("HVEN_D3_ELASTIC_E2OFF") ? *[&] { static thread_local std::unique_ptr<QpEngine> e3; QpOptions q3 = opts_.qp; q3.independent_working_set = false; e3 = std::make_unique<QpEngine>(q3, opts_.common.threads); return e3.get(); }() : *engine_, st.qp, elastic_seed_source, window,''')
old = '''    ropts.equality_basis = false;
'''
assert s.count(old) == 1
s = s.replace(old, '''    ropts.equality_basis = false;
    if (std::getenv("HVEN_D3_RESTO_E2OFF")) ropts.qp.independent_working_set = false;
''')
old = '''    const RestorationModel feasibility(resto_base, *x_start, resto_ev, st.x,
                                       opts_.restoration_proximity);'''
assert s.count(old) == 1
s = s.replace(old, '''    const double d3_prox = std::getenv("HVEN_D3_PROX") ? std::atof(std::getenv("HVEN_D3_PROX")) : opts_.restoration_proximity;
    Vec d3_xs = *x_start, d3_xc = st.x;
    if (std::getenv("HVEN_D3_RESTO_CLAMP")) {
        d3_xs = d3_xs.cwiseMax(resto_base.lower()).cwiseMin(resto_base.upper());
        d3_xc = d3_xc.cwiseMax(resto_base.lower()).cwiseMin(resto_base.upper());
        if (d3::on()) std::fprintf(stderr, "D3RESTO_CLAMP moved=%.6g\\n", (d3_xs - *x_start).lpNorm<Eigen::Infinity>());
    }
    const RestorationModel feasibility(resto_base, d3_xs, resto_ev, d3_xc, d3_prox);''')
old = '''            feasibility_subsolve_certifies(opts_.restoration_proximity, opts_.kkt_tol, rs_lambda_e,'''
assert s.count(old) == 1
s = s.replace(old, '''            feasibility_subsolve_certifies(d3_prox, opts_.kkt_tol, rs_lambda_e,''')
open(p, "w").write(s)
print("step 8 applied")
