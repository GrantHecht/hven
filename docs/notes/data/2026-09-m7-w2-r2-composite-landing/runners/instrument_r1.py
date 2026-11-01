#!/usr/bin/env python3
"""M7 W2 R1 TEMPORARY instrumentation + arm toggles (scratch only; production code is untouched).

Applied to a `git archive c1f73065` tree (argv[1]) and a copy of the hven-bench lane (argv[2]).
Everything is env-gated; with no HVEN_R1* variable set the library computes what the pin computes.

Prints (stderr, HVEN_R1=1):
  R1START / R1QP (first attempt) / R1QPR (cap retry) / R1MAJ / R1RESTO_REQ / R1RESTO_CFG /
  R1OMEGA (Arm C halvings) / R1RESTO_END / R1RESUME / R1END
Arm toggles:
  HVEN_R1_PROX=<zeta>     Arm P: restoration_proximity for the wrapper and the certificate gate
  HVEN_R1_TRCAP=<c>       Arm T: the sub-solve's tr_max = c * ropts.tr_init (= c * max(Delta_entry, restart))
  HVEN_R1_OMEGA=<k>       Arm C: the wrapper minimizes omega*f + l1 violation, omega = k * max(sigma)/||grad f(x_start)||_inf,
                          halved on every restoration major whose l1 violation does not decrease (and is > feas_tol)
  HVEN_R1_OMEGA_NOHESS=1  Arm C variant: omega*f enters the value and the gradient only (no f curvature in the sub-QP)
  HVEN_R1_OMEGA_STOPFEAS=1 Arm C variant: the sub-solve stops (kStop) at its first iterate with l1 violation <= feas_tol
  HVEN_R1_POSTRESTO_E2OFF=1 probe: after a restoration, the main loop's walk QPs run on an engine with qp.independent_working_set=false
Harness copy:
  HVEN_R1_TRACE=<file>    attach a JsonLinesTraceSink writing <file> to the top-level solver (the sub-solve's rows arrive with depth 1)
"""
import sys

tree, bench = sys.argv[1], sys.argv[2]


def patch(path, pairs):
    s = open(path).read()
    for old, new in pairs:
        n = s.count(old)
        if n != 1:
            sys.exit(f"{path}: anchor count {n} != 1: {old[:100]!r}")
        s = s.replace(old, new)
    open(path, "w").write(s)
    print("patched", path)


# ---------------------------------------------------------------- restoration.h
patch(tree + "/include/hven/detail/globalization/sqp/restoration.h", [
    ('''    double zeta_ = 0.0;
    Vec centre_, d_;
};''',
     '''    double zeta_ = 0.0;
    Vec centre_, d_;

  public:
    // ===== M7 W2 R1 SCRATCH ONLY: Arm C's composite weight (0 = OFF) =====
    void r1_set_omega(double w) const { r1_omega_ = w; }
    double r1_omega() const { return r1_omega_; }
    double r1_violation(const Vec &y) const;

  private:
    mutable double r1_omega_ = 0.0;
};'''),
])

# ---------------------------------------------------------------- soc_elastic_restoration.cpp
patch(tree + "/src/globalization/sqp/soc_elastic_restoration.cpp", [
    ('#include <hven/drivers/sqp_solver.h>\n', '#include <hven/drivers/sqp_solver.h>\n#include <cstdlib>\n'),
    ('''        s += 0.5 * zeta_ * p;
    }
    return s;
}''',
     '''        s += 0.5 * zeta_ * p;
    }
    if (r1_omega_ > 0.0) { // M7 W2 R1 scratch: Arm C
        s += r1_omega_ * model_.eval_f(original_x(y));
    }
    return s;
}

double RestorationModel::r1_violation(const Vec &y) const {
    double s = 0.0;
    for (Index k = 0; k < me_; ++k) {
        s += sigma_e_(k) * (y(nx_ + k) + y(nx_ + me_ + k));
    }
    for (Index j = 0; j < mi_; ++j) {
        s += sigma_i_(j) * y(nx_ + 2 * me_ + j);
    }
    return s;
}'''),
    ('''            g(i) = zeta_ * d_(i) * d_(i) * (y(i) - centre_(i));
        }
    }
    return g;''',
     '''            g(i) = zeta_ * d_(i) * d_(i) * (y(i) - centre_(i));
        }
    }
    if (r1_omega_ > 0.0) { // M7 W2 R1 scratch: Arm C
        g.head(nx_) += r1_omega_ * model_.eval_grad(original_x(y));
    }
    return g;'''),
    ('''    model_.eval_hess_in_place(original_x(y), 0.0, lambda_e, lambda_i, inner);''',
     '''    static const bool r1_nohess = std::getenv("HVEN_R1_OMEGA_NOHESS") != nullptr;
    model_.eval_hess_in_place(original_x(y), (r1_omega_ > 0.0 && !r1_nohess) ? obj_scale * r1_omega_ : 0.0,
                              lambda_e, lambda_i, inner); // M7 W2 R1 scratch: Arm C'''),
])

# ---------------------------------------------------------------- sqp_solver.cpp
patch(tree + "/src/drivers/sqp_solver.cpp", [
    ('#include <hven/drivers/sqp_solver.h>\n',
     '''#include <hven/drivers/sqp_solver.h>
#include <cstdio>
#include <cstdlib>
// ===== M7 W2 R1 TEMPORARY DIAGNOSTIC INSTRUMENTATION + ARM TOGGLES (scratch only) =====
namespace hven::solvers::r1 {
inline bool on() { static const bool b = std::getenv("HVEN_R1") != nullptr; return b; }
inline double env(const char *k, double dflt) { const char *v = std::getenv(k); return v ? std::atof(v) : dflt; }
inline long qp_seq = 0;       // top-level QP solves, in order
inline long qp_after_resume = -1; // index of top-level QP solves since the restoration resume (-1 = none yet)
} // namespace hven::solvers::r1
'''),
    # start report
    ('''    if (opts_.project_start_point) {
        const Vec &lo = seam.lower();''',
     '''    if (r1::on()) {
        const Vec &lo3 = seam.lower(); const Vec &up3 = seam.upper(); long c3 = 0; double m3 = 0.0;
        for (Index i = 0; i < n; ++i) { const double v = std::max(lo3[i] - x0[i], x0[i] - up3[i]); if (v > 0) { ++c3; m3 = std::max(m3, v); } }
        std::fprintf(stderr, "R1START depth=%d n=%ld out_of_box=%ld max_box_viol=%.10g project=%d tr_init=%.6g tr_max=%.6g\\n", is_restoration_sub_driver_ ? 1 : 0, (long)n, c3, m3, (int)opts_.project_start_point, opts_.tr_init, opts_.tr_max);
    }
    if (opts_.project_start_point) {
        const Vec &lo = seam.lower();'''),
    # solve end
    ('''    SqpResult out = solve_impl_body(seam, bridge, *x0_used, warm, budget, std::move(strategy));
''',
     '''    SqpResult out = solve_impl_body(seam, bridge, *x0_used, warm, budget, std::move(strategy));
    if (r1::on()) std::fprintf(stderr, "R1END depth=%d status=%d majors=%ld resto_iters=%ld certified=%d obj=%.12g feas=%.6g stat=%.6g\\n", is_restoration_sub_driver_ ? 1 : 0, (int)out.status, (long)out.counters.major_iters, (long)out.counters.restoration_iters, (int)out.infeasibility_certified, out.f, out.sqp_feasibility, out.sqp_stationarity);
'''),
    # QP first attempt (before the cap retry)
    ('''    if (opts_.qp_cap_retry && mj.walk_owns_this_qp && mj.qs.status == QpStatus::kMaxIter &&
        qp_failure_is_retryable(st.qp, mj.qs, opts_.feas_tol)) {''',
     '''    if (r1::on()) {
        long seq = -1, after = -1;
        if (!is_restoration_sub_driver_) { seq = r1::qp_seq++; if (r1::qp_after_resume >= 0) after = r1::qp_after_resume++; }
        std::fprintf(stderr, "R1QP depth=%d major=%ld seq=%ld post_resume_idx=%ld delta=%.6g status=%d minors=%ld facts=%ld h=%.10g f=%.12g gradf_inf=%.6g blk_term=%ld zmcap=%ld cert=%ld cert_rej=%ld n=%ld me=%ld mi=%ld walk=%d resto_used=%d\\n",
                     is_restoration_sub_driver_ ? 1 : 0, (long)iter, seq, after, st.delta, (int)mj.qs.status,
                     (long)mj.qs.counters.minor_iters, (long)mj.qs.counters.factorizations,
                     constraint_violation_l1(st.ev), st.ev.f, st.ev.grad.size() ? st.ev.grad.lpNorm<Eigen::Infinity>() : -1.0,
                     (long)mj.qs.counters.dep_blocker_terminations, (long)mj.qs.counters.zero_motion_cap_hits,
                     (long)mj.qs.counters.dep_infeasible_certificates, (long)mj.qs.counters.dep_certificate_rejections,
                     (long)st.qp.n(), (long)st.qp.me(), (long)st.qp.mi(), (int)mj.walk_owns_this_qp, (int)st.resto.used);
    }
    if (opts_.qp_cap_retry && mj.walk_owns_this_qp && mj.qs.status == QpStatus::kMaxIter &&
        qp_failure_is_retryable(st.qp, mj.qs, opts_.feas_tol)) {'''),
    ('''        mj.qs = std::move(retried);
    }''',
     '''        mj.qs = std::move(retried);
        if (r1::on()) std::fprintf(stderr, "R1QPR depth=%d major=%ld retry_cap=%ld status=%d minors=%ld facts=%ld\\n", is_restoration_sub_driver_ ? 1 : 0, (long)iter, (long)(2 * cap), (int)mj.qs.status, (long)mj.qs.counters.minor_iters, (long)mj.qs.counters.factorizations);
    }'''),
    # restoration request
    ('''    if (!allow_restoration_ || st.resto.used) {
        // The nested case''',
     '''    if (r1::on()) std::fprintf(stderr, "R1RESTO_REQ depth=%d major=%ld allow=%d used=%d h=%.10g f=%.12g delta=%.6g cand=%d\\n", is_restoration_sub_driver_ ? 1 : 0, (long)iter, (int)allow_restoration_, (int)st.resto.used, constraint_violation_l1(st.ev), st.ev.f, st.delta, (int)(cand.x != nullptr));
    if (!allow_restoration_ || st.resto.used) {
        // The nested case'''),
    # wrapper: proximity toggle
    ('''    const RestorationModel feasibility(resto_base, *x_start, resto_ev, st.x,
                                       opts_.restoration_proximity);''',
     '''    const double r1_prox = r1::env("HVEN_R1_PROX", opts_.restoration_proximity);
    const RestorationModel feasibility(resto_base, *x_start, resto_ev, st.x, r1_prox);
    // Arm C: omega = k * max(sigma) / ||grad f(x_start)||_inf, at the wrapper's construction point.
    const double r1_omega_k = r1::env("HVEN_R1_OMEGA", 0.0);
    double r1_omega0 = 0.0, r1_gradf_inf = 0.0, r1_sigma_max = 1.0;
    if (feasibility.slack_scale_e().size() > 0) r1_sigma_max = std::max(r1_sigma_max, feasibility.slack_scale_e().maxCoeff());
    if (feasibility.slack_scale_i().size() > 0) r1_sigma_max = std::max(r1_sigma_max, feasibility.slack_scale_i().maxCoeff());
    if (r1_omega_k > 0.0) {
        const Vec xs0 = feasibility.original_x(feasibility.start_point());
        r1_gradf_inf = resto_base.eval_grad(xs0).lpNorm<Eigen::Infinity>();
        r1_omega0 = r1_gradf_inf > 0.0 ? r1_omega_k * r1_sigma_max / r1_gradf_inf : 0.0;
        feasibility.r1_set_omega(r1_omega0);
    }'''),
    # tr cap
    ('''    ropts.tr_max = std::max(opts_.tr_max, ropts.tr_init);
''',
     '''    ropts.tr_max = std::max(opts_.tr_max, ropts.tr_init);
    if (const double c = r1::env("HVEN_R1_TRCAP", 0.0); c > 0.0) {
        ropts.tr_max = c * ropts.tr_init; // Arm T: the entry-scale cap
    }
    if (r1::on()) std::fprintf(stderr, "R1RESTO_CFG prox=%.6g trcap=%.6g sub_tr_init=%.6g sub_tr_max=%.6g omega_k=%.6g omega0=%.10g gradf_inf=%.10g sigma_max=%.10g sub_max_iter=%ld start_clamps=%ld seed_used=%d\\n",
                               r1_prox, r1::env("HVEN_R1_TRCAP", 0.0), ropts.tr_init, ropts.tr_max, r1_omega_k, r1_omega0, r1_gradf_inf, r1_sigma_max, (long)(st.eff_max_iter - spent), (long)feasibility.start_clamps(), (int)mj.row.restoration_seed_used);
'''),
    # Arm C: halving callback (the bench installs no callback; refuse to combine rather than guess)
    ('''    struct ActiveSubGuard {
        SqpSolver *parent;''',
     '''    double r1_h_prev = r1_omega0 > 0.0 ? feasibility.r1_violation(feasibility.start_point()) : 0.0;
    if (r1_omega0 > 0.0) {
        sub.set_iteration_callback(
            [this, &st, &feasibility, &r1_h_prev](const IterationEvent &e) {
                if (e.x.size() == feasibility.n()) {
                    const double h = feasibility.r1_violation(e.x);
                    const bool halve = !(h < r1_h_prev) && h > opts_.feas_tol;
                    if (halve) feasibility.r1_set_omega(0.5 * feasibility.r1_omega());
                    if (r1::on()) std::fprintf(stderr, "R1OMEGA iter=%ld h_prev=%.10g h=%.10g halve=%d omega=%.10g step=%.6g radius=%.6g\\n", (long)e.iteration, r1_h_prev, h, (int)halve, feasibility.r1_omega(), e.step_norm, e.radius.value_or(-1.0));
                    r1_h_prev = h;
                    // Arm C variant: return to the main loop at the first feasible restoration iterate
                    static const bool stopfeas = std::getenv("HVEN_R1_OMEGA_STOPFEAS") != nullptr;
                    if (stopfeas && h <= opts_.feas_tol) {
                        if (r1::on()) std::fprintf(stderr, "R1OMEGA_STOPFEAS iter=%ld h=%.10g\\n", (long)e.iteration, h);
                        return CallbackAction::kStop;
                    }
                }
                if (!iteration_callback_) return CallbackAction::kContinue;
                // the caller's callback, forwarded exactly as the shipped forwarder does
                IterationEvent nested = e;
                nested.depth = e.depth.value_or(0) + 1;
                nested.elapsed_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - this->entry_time_).count();
                const CallbackAction action = this->invoke_iteration_callback(nested);
                if (action == CallbackAction::kStop) st.interrupted = true;
                return action;
            },
            IterationPayload::kVectors);
    }
    struct ActiveSubGuard {
        SqpSolver *parent;'''),
    # restoration end
    ('''    const double h_r = constraint_violation_l1(ev_r);
''',
     '''    const double h_r = constraint_violation_l1(ev_r);
    if (r1::on()) {
        std::fprintf(stderr, "R1RESTO_END sub_status=%d sub_majors=%ld sub_minors=%ld sub_facts=%ld sub_elastic=%ld sub_maxiter_exits=%ld h_in=%.10g h_r=%.10g box_r=%.6g f_in=%.12g f_r=%.12g dx_inf=%.10g dx_2=%.10g gradf_r_inf=%.6g omega_end=%.10g sub_obj=%.12g sub_stat=%.6g sub_feas=%.6g\\n",
                     (int)rs.status, (long)rs.counters.major_iters, (long)rs.counters.qp_minor_iters, (long)rs.counters.factorizations,
                     (long)rs.counters.elastic_activations, (long)rs.counters.qp_maxiter_exits,
                     excursion_h_in, h_r, box_violation_l1(x_r, seam.lower(), seam.upper()), excursion_f_in, ev_r.f,
                     (x_r - st.x).lpNorm<Eigen::Infinity>(), (x_r - st.x).norm(), ev_r.grad.size() ? ev_r.grad.lpNorm<Eigen::Infinity>() : -1.0,
                     feasibility.r1_omega(), rs.f, rs.sqp_stationarity, rs.sqp_feasibility);
    }
'''),
    ('''        st.have_seed = false;
        return RestorationOutcome::kResumed;''',
     '''        st.have_seed = false;
        if (r1::on()) { r1::qp_after_resume = 0; std::fprintf(stderr, "R1RESUME delta=%.6g f=%.12g h=%.10g\\n", st.delta, st.ev.f, h_r); }
        return RestorationOutcome::kResumed;'''),
    # certificate gate: at Arm C the explicit residual is computed (the objective is not h alone)
    ('''            feasibility_subsolve_certifies(opts_.restoration_proximity, opts_.kkt_tol, rs_lambda_e,''',
     '''            feasibility_subsolve_certifies(r1_omega0 > 0.0 ? std::max(1.0, r1_prox) : r1_prox, opts_.kkt_tol, rs_lambda_e,'''),
    # post-restoration probe: the main loop's walk QPs after a restoration run on an E2-off engine
    ('''    qs = offer_hot   ? engine_->solve(qp, seed, overrides, warm.hot)
         : have_seed ? engine_->solve(qp, seed, overrides)
         : use_crash ? engine_->solve(qp, crash_seed, overrides)
                     : engine_->solve(qp, overrides);''',
     '''    static const bool r1_post_e2off = std::getenv("HVEN_R1_POSTRESTO_E2OFF") != nullptr;
    if (r1_post_e2off && st.resto.used && !is_restoration_sub_driver_) {
        static thread_local std::unique_ptr<QpEngine> r1_e2off;
        if (!r1_e2off) {
            QpOptions q = opts_.qp;
            q.independent_working_set = false;
            r1_e2off = std::make_unique<QpEngine>(q, opts_.common.threads);
        }
        qs = have_seed ? r1_e2off->solve(qp, seed, overrides) : r1_e2off->solve(qp, overrides);
        if (r1::on()) std::fprintf(stderr, "R1POSTE2OFF status=%d minors=%ld\\n", (int)qs.status, (long)qs.counters.minor_iters);
    } else
    qs = offer_hot   ? engine_->solve(qp, seed, overrides, warm.hot)
         : have_seed ? engine_->solve(qp, seed, overrides)
         : use_crash ? engine_->solve(qp, crash_seed, overrides)
                     : engine_->solve(qp, overrides);'''),
    # every major
    ('''        const MajorOutcome outcome = run_major(st, mj, seam, bridge, warm, budget, iter);
''',
     '''        const MajorOutcome outcome = run_major(st, mj, seam, bridge, warm, budget, iter);
        if (r1::on()) {
            std::fprintf(stderr, "R1MAJ depth=%d major=%ld outcome=%d qp_status=%d verdict=%d elastic=%d viol=%.10g f=%.12g tr=%.6g step=%.6g minors=%ld h_now=%.10g f_now=%.12g\\n",
                         is_restoration_sub_driver_ ? 1 : 0, (long)iter, (int)outcome, (int)mj.row.qp_status, (int)mj.row.verdict,
                         (int)mj.row.elastic_applied, mj.row.violation_l1, mj.row.f, mj.row.tr_radius, mj.row.step_norm,
                         (long)mj.row.qp_minor_iters, constraint_violation_l1(st.ev), st.ev.f);
        }
'''),
])

# ---------------------------------------------------------------- harness copy: optional console trace on stderr
patch(bench + "/lanes/hven/hven_bench_cell.cpp", [
    ('#include "hven/drivers/sqp_solver.h"\n',
     '#include "hven/drivers/sqp_solver.h"\n#include "hven/drivers/trace_writer.h"\n#include <cstdlib>\n'),
    ('''    hven::solvers::SqpSolver solver(options);
    // Observation only (M7 W2 E2-RETAKE schema event)''',
     '''    hven::solvers::SqpSolver solver(options);
    // M7 W2 R1 scratch only: HVEN_R1_TRACE=<file> attaches a JSON-lines trace sink (every depth).
    static std::ofstream r1_trace_file;
    static std::unique_ptr<hven::solvers::JsonLinesTraceSink> r1_sink;
    if (const char *tp = std::getenv("HVEN_R1_TRACE"); tp != nullptr) {
        r1_trace_file.open(tp, std::ios::trunc);
        r1_sink = std::make_unique<hven::solvers::JsonLinesTraceSink>(r1_trace_file);
        solver.attach_trace(r1_sink.get());
    }
    // M7 W2 R1 scratch only: HVEN_R1_WALL=<s> ends the solve from inside (kStop at the first
    // history row past <s> seconds) -- the harness's own 300 s ceiling, without a signal.
    if (const char *w = std::getenv("HVEN_R1_WALL"); w != nullptr) {
        const double wall = std::atof(w);
        solver.set_iteration_callback([wall](const hven::solvers::IterationEvent &e) {
            if (e.elapsed_seconds > wall) {
                std::fprintf(stderr, "R1WALLSTOP elapsed=%.3f depth=%ld iter=%ld\\n", e.elapsed_seconds, (long)e.depth.value_or(0), (long)e.iteration);
                return hven::solvers::CallbackAction::kStop;
            }
            return hven::solvers::CallbackAction::kContinue;
        });
    }
    // Observation only (M7 W2 E2-RETAKE schema event)'''),
])
