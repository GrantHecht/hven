#!/usr/bin/env python3
"""M7 W2 R2 TEMPORARY instrumentation (scratch only), applied AFTER instrument_r1.py.

HVEN_R1=1 also prints:
  R2JUDGE  every major's final verdict at every depth: f_old/f_new/h_old/h_new/pred/ared/ratio,
           switching (f-type), funnel width, radius, tr_binding, rejections, soc, and -- inside a
           composite restoration -- the decomposition of pred/ared into the omega*f and the
           sigma-weighted slack (violation) parts.
Variant toggles (Q2), env-gated, only inside a composite restoration (omega > 0):
  HVEN_R2_HJUDGE=1   the sub-solve's funnel judges the violation part v alone (f_old/f_new/pred_df
                     replaced by v_old/v_new/pred_v) -- acceptance on h alone while omega > 0
  HVEN_R2_FRESH=1    the omega halving re-evaluates the sub-iterate (ev.f, ev.grad) at the new omega
                     (see R2 report: the halving otherwise leaves st.ev stale)
"""
import sys
tree = sys.argv[1]

def patch(path, pairs):
    s = open(path).read()
    for old, new in pairs:
        n = s.count(old)
        if n != 1:
            sys.exit(f"{path}: anchor count {n} != 1: {old[:100]!r}")
        s = s.replace(old, new)
    open(path, "w").write(s)
    print("patched", path)

patch(tree + "/src/drivers/sqp_solver.cpp", [
    ('''inline long qp_after_resume = -1; // index of top-level QP solves since the restoration resume (-1 = none yet)
} // namespace hven::solvers::r1''',
     '''inline long qp_after_resume = -1; // index of top-level QP solves since the restoration resume (-1 = none yet)
// R2: the composite decomposition hook, set by the parent around a composite restoration sub-solve.
struct R2Parts { bool ok = false; double omega = 0, v_old = 0, v_new = 0, F_old = 0, F_new = 0, gfd = 0; };
inline std::function<R2Parts(const Vec &, const Vec &)> decomp;
inline bool hjudge() { static const bool b = std::getenv("HVEN_R2_HJUDGE") != nullptr; return b; }
} // namespace hven::solvers::r1'''),
    ('#include <hven/drivers/sqp_solver.h>\n#include <cstdio>\n',
     '#include <hven/drivers/sqp_solver.h>\n#include <functional>\n#include <cstdio>\n'),
    # the judge: the variant substitution + the print
    ('''    StepVerdict verdict = zero_step ? StepVerdict::kAcceptF : st.strategy->judge(mj.ctx);
''',
     '''    r1::R2Parts r2p;
    if (is_restoration_sub_driver_ && r1::decomp) r2p = r1::decomp(st.x, mj.qs.x);
    const StepContext r2_ctx_orig = mj.ctx;
    if (r2p.ok && r1::hjudge() && r2p.omega > 0.0) {
        // Q2 variant: the funnel judges the violation part alone (the pred of the linear slack term is exact)
        mj.ctx.f_old = r2p.v_old;
        mj.ctx.f_new = r2p.v_new;
        mj.ctx.pred_df = r2p.v_old - r2p.v_new;
    }
    StepVerdict verdict = zero_step ? StepVerdict::kAcceptF : st.strategy->judge(mj.ctx);
    bool r2_vguard = false;
    if (r2p.ok && r1::hjudge() && r2p.omega > 0.0 && !zero_step && !(r2p.v_old - r2p.v_new > 0.0) &&
        (verdict == StepVerdict::kAcceptF || verdict == StepVerdict::kAcceptH)) {
        verdict = StepVerdict::kReject; // Q2 variant: a step that does not reduce v is never accepted
        r2_vguard = true;
    }
    if (r1::on()) {
        const double r2_ared = mj.ctx.f_old - mj.ctx.f_new;
        const auto *r2_fun = dynamic_cast<const FunnelStrategy *>(st.strategy.get());
        std::fprintf(stderr, "R2JUDGE depth=%d major=%ld delta=%.6g tr_bind=%d step=%.6g zero=%d rej_at=%ld f_old=%.12g f_new=%.12g h_old=%.6g h_new=%.6g pred=%.6g ared=%.6g ratio=%.6g ftype=%d width=%.6g verdict0=%d orig_pred=%.6g orig_ared=%.6g",
                     is_restoration_sub_driver_ ? 1 : 0, (long)iter, st.delta, (int)mj.row.tr_binding, mj.row.step_norm, (int)zero_step,
                     (long)st.rejections_at_iterate, mj.ctx.f_old, mj.ctx.f_new, mj.ctx.h_old, mj.ctx.h_new, mj.ctx.pred_df, r2_ared,
                     mj.ctx.pred_df != 0.0 ? r2_ared / mj.ctx.pred_df : 0.0, (int)(mj.ctx.pred_df >= kFunnelDelta * mj.ctx.h_old * mj.ctx.h_old),
                     r2_fun ? r2_fun->width() : -1.0, (int)verdict, r2_ctx_orig.pred_df, r2_ctx_orig.f_old - r2_ctx_orig.f_new);
        if (r2_vguard) std::fprintf(stderr, " VGUARD");
        if (r2p.ok) {
            const double pf = -r2p.omega * r2p.gfd, af = r2p.omega * (r2p.F_old - r2p.F_new), pv = r2p.v_old - r2p.v_new;
            std::fprintf(stderr, " | omega=%.6g v_old=%.10g v_new=%.10g F_old=%.10g F_new=%.10g pred_f=%.6g ared_f=%.6g pred_v=ared_v=%.6g quad=%.6g stale=%.6g",
                         r2p.omega, r2p.v_old, r2p.v_new, r2p.F_old, r2p.F_new, pf, af, pv, r2_ctx_orig.pred_df - pf - pv,
                         r2_ctx_orig.f_old - (r2p.v_old + r2p.omega * r2p.F_old));
        }
        std::fprintf(stderr, "\\n");
    }
'''),
    # final verdict print (after SOC)
    ('''    mj.row.verdict = verdict;
    mj.row.soc_applied = mj.soc_applied;
''',
     '''    mj.row.verdict = verdict;
    mj.row.soc_applied = mj.soc_applied;
    if (r1::on()) std::fprintf(stderr, "R2VERDICT depth=%d major=%ld verdict=%d soc=%d\\n", is_restoration_sub_driver_ ? 1 : 0, (long)iter, (int)verdict, (int)mj.soc_applied);
'''),
    # install the hook around the sub-solve (same place as the R1 omega callback)
    ('''    double r1_h_prev = r1_omega0 > 0.0 ? feasibility.r1_violation(feasibility.start_point()) : 0.0;
''',
     '''    double r1_h_prev = r1_omega0 > 0.0 ? feasibility.r1_violation(feasibility.start_point()) : 0.0;
    struct R2HookGuard { ~R2HookGuard() { r1::decomp = nullptr; } } r2_hook_guard;
    if (r1_omega0 > 0.0) {
        r1::decomp = [&feasibility, &resto_base](const Vec &y, const Vec &d) {
            r1::R2Parts o;
            if (y.size() != feasibility.n() || d.size() != y.size()) return o;
            const Vec yn = y + d;
            const Vec xo = feasibility.original_x(y), xn = feasibility.original_x(yn);
            o.omega = feasibility.r1_omega();
            o.v_old = feasibility.r1_violation(y);
            o.v_new = feasibility.r1_violation(yn);
            o.F_old = resto_base.eval_f(xo);
            o.F_new = resto_base.eval_f(xn);
            o.gfd = resto_base.eval_grad(xo).dot(xn - xo);
            o.ok = true;
            return o;
        };
    }
'''),
    ('''            const double actual_df = st.ev.f - mj.ctx.f_new;
''',
     '''            // R2 Q2 variant: under HVEN_R2_HJUDGE inside a composite restoration the growth test reads the judged
            // (violation-only) pair; otherwise ctx.f_old == st.ev.f and this is the shipped arithmetic.
            const double actual_df = (is_restoration_sub_driver_ && r1::hjudge() && r1::decomp) ? mj.ctx.f_old - mj.ctx.f_new
                                                                                                : st.ev.f - mj.ctx.f_new;
'''),
])
