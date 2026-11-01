#!/usr/bin/env python3
"""E3 SCRATCH MEASUREMENT PATCH (one-shot, revertible by git checkout).
Adds HVEN_TESTING-only walk-seam columns for E3-1:
  (a) the ORDINARY step's slope g'(eqp.x - x) and the centring term
      (delta/2)(||x_F||^2 - ||eqp.x_F||^2), for the REFINED and the UNREFINED answer;
  (b) the EQP solve's own residuals before and after its one refinement step, split into
      the free block (rho) and the constraint block, in BOTH the regularized and the
      unregularized system -- so the contraction and the factor's own solve accuracy are
      separable.
Files: include/hven/detail/qp/qp_walk_trace.h, include/hven/detail/qp/eqp_solve.h,
       src/qp/qp_engine.cpp.  Nothing outside HVEN_TESTING is touched."""
import sys

W = '/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/'


def patch(path, subs):
    s = open(W + path).read()
    for old, new in subs:
        assert s.count(old) == 1, (path, s.count(old), old[:90])
        s = s.replace(old, new)
    open(W + path, 'w').write(s)
    print('patched', path)


# ---------------------------------------------------------------- qp_walk_trace.h
patch('include/hven/detail/qp/qp_walk_trace.h', [
    # 1. record columns
    ("""    Index add_b1_leak_bound_exceeded = -1;
};""",
     """    Index add_b1_leak_bound_exceeded = -1;

    // ---- M7 W2 E3 SCRATCH: the flat-face centring ---------------------------
    // Every column here is a dot product or a norm of vectors the pass already
    // holds, computed into this record and read by nothing.
    /// `(Hx + g)'(eqp.x - x)` -- the ORDINARY step's slope, on EVERY pass (the
    /// existing `ride_slope` is the ride site's own and is absent elsewhere).
    double e3_ord_gp = kAbsent;
    double e3_ord_php = kAbsent;
    /// `(delta/2)(||x_F||^2 - ||eqp.x_F||^2)`: what the absolute centring at 0
    /// permits the ordinary step to ascend by.
    double e3_ord_centre = kAbsent;
    /// The same three for the PRE-refinement (regularized) answer.
    double e3_ord_gp_unref = kAbsent;
    double e3_ord_php_unref = kAbsent;
    double e3_ord_centre_unref = kAbsent;
    double e3_unref_dx = kAbsent;
    /// `||x_F||^2` and `||eqp.x_F||^2` themselves.
    double e3_x_free_sq = kAbsent;
    double e3_eqp_free_sq = kAbsent;
    // ---- the EQP solve's own residuals (ELIMINATED path only) --------------
    double e3_res_reg_unref = kAbsent;
    double e3_res_reg_ref = kAbsent;
    double e3_res_unreg_unref = kAbsent;
    double e3_res_unreg_ref = kAbsent;
    double e3_rho_unref = kAbsent;
    double e3_rho_ref = kAbsent;
    double e3_row_unref = kAbsent;
    double e3_row_ref = kAbsent;
    double e3_rhs_inf = kAbsent;
    double e3_lam_inf = kAbsent;
    Index e3_dim = -1;
    Index e3_n_free = -1;
};"""),
    # 2. scratch slots on the evidence struct + the unrefined point
    ("""    Index border_dim = -1;
};""",
     """    Index border_dim = -1;

    // ---- M7 W2 E3 SCRATCH: the EQP solve's own residuals -------------------
    double e3_res_reg_unref = std::numeric_limits<double>::quiet_NaN();
    double e3_res_reg_ref = std::numeric_limits<double>::quiet_NaN();
    double e3_res_unreg_unref = std::numeric_limits<double>::quiet_NaN();
    double e3_res_unreg_ref = std::numeric_limits<double>::quiet_NaN();
    double e3_rho_unref = std::numeric_limits<double>::quiet_NaN();
    double e3_rho_ref = std::numeric_limits<double>::quiet_NaN();
    double e3_row_unref = std::numeric_limits<double>::quiet_NaN();
    double e3_row_ref = std::numeric_limits<double>::quiet_NaN();
    double e3_rhs_inf = std::numeric_limits<double>::quiet_NaN();
    double e3_lam_inf = std::numeric_limits<double>::quiet_NaN();
    Index e3_dim = -1;
    Index e3_n_free = -1;
};"""),
    # 3. the unrefined point, as its own thread_local (a Vec, not a scalar column)
    ("""    static inline thread_local QpWalkEqpEvidence eqp{};""",
     """    static inline thread_local QpWalkEqpEvidence eqp{};

    /// M7 W2 E3 SCRATCH: the PRE-refinement EQP point of the pass's own call.
    /// Empty when the call did not produce one (border path, empty system).
    static inline thread_local Vec e3_unrefined_x{};"""),
    # 4. snapshot
    ("""        rec_.border_dim = e.border_dim;
    }""",
     """        rec_.border_dim = e.border_dim;
        rec_.e3_res_reg_unref = e.e3_res_reg_unref;
        rec_.e3_res_reg_ref = e.e3_res_reg_ref;
        rec_.e3_res_unreg_unref = e.e3_res_unreg_unref;
        rec_.e3_res_unreg_ref = e.e3_res_unreg_ref;
        rec_.e3_rho_unref = e.e3_rho_unref;
        rec_.e3_rho_ref = e.e3_rho_ref;
        rec_.e3_row_unref = e.e3_row_unref;
        rec_.e3_row_ref = e.e3_row_ref;
        rec_.e3_rhs_inf = e.e3_rhs_inf;
        rec_.e3_lam_inf = e.e3_lam_inf;
        rec_.e3_dim = e.e3_dim;
        rec_.e3_n_free = e.e3_n_free;
    }"""),
    # 5. reset the E3 point with the scratch
    ("""inline void qp_walk_reset_eqp() {
    if (QpWalkObserver::capturing) {
        QpWalkObserver::eqp = QpWalkEqpEvidence{};
    }
}""",
     """inline void qp_walk_reset_eqp() {
    if (QpWalkObserver::capturing) {
        QpWalkObserver::eqp = QpWalkEqpEvidence{};
        QpWalkObserver::e3_unrefined_x.resize(0);
    }
}"""),
])

# ---------------------------------------------------------------- eqp_solve.h
patch('include/hven/detail/qp/eqp_solve.h', [
    ("""#include <hven/detail/qp/working_set.h>
#include <hven/qp/qp_types.h>""",
     """#include <hven/detail/qp/working_set.h>
#include <hven/qp/qp_types.h>
#ifdef HVEN_TESTING
#include <hven/detail/qp/qp_walk_trace.h>
#endif"""),
    ("""    y = y + detail::solve_vec(kkt, residual_of(y));

    EqpResult out = scatter(y);""",
     """#ifdef HVEN_TESTING
    const Vec e3_y0 = y;
#endif
    y = y + detail::solve_vec(kkt, residual_of(y));
#ifdef HVEN_TESTING
    // M7 W2 E3 SCRATCH, record-only: how well the factor solved its OWN
    // (regularized) system, and what the one unregularized refinement step
    // actually contracted. No solve, no factorization; two mat-vecs.
    {
        const auto Kv = [&](const Vec &yy) {
            return Vec(asm_.K.template selfadjointView<Eigen::Upper>() * yy);
        };
        const Vec reg_u = Vec(rhs - Kv(e3_y0));
        const Vec reg_r = Vec(rhs - Kv(y));
        const Vec unr_u = residual_of(e3_y0);
        const Vec unr_r = residual_of(y);
        const Index nc = me + n_working;
        const double lam_inf =
            nc > 0 ? y.segment(n_free, nc).lpNorm<Eigen::Infinity>() : 0.0;
        detail::testing::qp_walk_note_eqp([&](detail::testing::QpWalkEqpEvidence &s) {
            s.e3_res_reg_unref = reg_u.lpNorm<Eigen::Infinity>();
            s.e3_res_reg_ref = reg_r.lpNorm<Eigen::Infinity>();
            s.e3_res_unreg_unref = unr_u.lpNorm<Eigen::Infinity>();
            s.e3_res_unreg_ref = unr_r.lpNorm<Eigen::Infinity>();
            s.e3_rho_unref = n_free > 0 ? unr_u.head(n_free).lpNorm<Eigen::Infinity>() : 0.0;
            s.e3_rho_ref = n_free > 0 ? unr_r.head(n_free).lpNorm<Eigen::Infinity>() : 0.0;
            s.e3_row_unref = nc > 0 ? unr_u.segment(n_free, nc).lpNorm<Eigen::Infinity>() : 0.0;
            s.e3_row_ref = nc > 0 ? unr_r.segment(n_free, nc).lpNorm<Eigen::Infinity>() : 0.0;
            s.e3_rhs_inf = rhs.lpNorm<Eigen::Infinity>();
            s.e3_lam_inf = lam_inf;
            s.e3_dim = dim;
            s.e3_n_free = n_free;
        });
        if (detail::testing::QpWalkObserver::capturing) {
            detail::testing::QpWalkObserver::e3_unrefined_x = scatter(e3_y0).x;
        }
    }
#endif

    EqpResult out = scatter(y);"""),
])

# ---------------------------------------------------------------- qp_engine.cpp
patch('src/qp/qp_engine.cpp', [
    ("""            detail::testing::QpWalkMinorRecord &r = walk_obs.record();
            r.p_sq = p.squaredNorm();""",
     """            detail::testing::QpWalkMinorRecord &r = walk_obs.record();
            r.p_sq = p.squaredNorm();

            // ---- M7 W2 E3 SCRATCH: the ORDINARY step's slope and the
            // centring term, for the refined and the unrefined answer.
            {
                const Vec grad = Vec(qp.H.selfadjointView<Eigen::Upper>() * x) + qp.g;
                r.e3_ord_gp = grad.dot(p);
                r.e3_ord_php = p.dot(Vec(qp.H.selfadjointView<Eigen::Upper>() * p));
                double xfsq = 0.0;
                double efsq = 0.0;
                for (Index i = 0; i < qp.n(); ++i) {
                    if (ws.bound_state()[static_cast<std::size_t>(i)] ==
                        BoundState::kFree) {
                        xfsq += x(i) * x(i);
                        efsq += eqp.x(i) * eqp.x(i);
                    }
                }
                r.e3_x_free_sq = xfsq;
                r.e3_eqp_free_sq = efsq;
                r.e3_ord_centre = 0.5 * eff_opts.primal_delta * (xfsq - efsq);
                const Vec &ux = detail::testing::QpWalkObserver::e3_unrefined_x;
                if (ux.size() == qp.n()) {
                    const Vec pu = ux - x;
                    r.e3_ord_gp_unref = grad.dot(pu);
                    r.e3_ord_php_unref =
                        pu.dot(Vec(qp.H.selfadjointView<Eigen::Upper>() * pu));
                    double ufsq = 0.0;
                    for (Index i = 0; i < qp.n(); ++i) {
                        if (ws.bound_state()[static_cast<std::size_t>(i)] ==
                            BoundState::kFree) {
                            ufsq += ux(i) * ux(i);
                        }
                    }
                    r.e3_ord_centre_unref =
                        0.5 * eff_opts.primal_delta * (xfsq - ufsq);
                    r.e3_unref_dx = (ux - eqp.x).lpNorm<Eigen::Infinity>();
                }
            }"""),
])
print('E3_MEASURE_PATCH_OK')
