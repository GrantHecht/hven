#!/usr/bin/env python3
"""E3 SCRATCH ARM PATCH: wires include/hven/detail/qp/e3_prox_arm.h into the four rhs
derivations and the two refinement targets. Test-only, env-toggled; reverted before any
production commit."""
W = '/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/'


def patch(path, subs):
    s = open(W + path).read()
    for old, new in subs:
        assert s.count(old) == 1, (path, s.count(old), old[:90])
        s = s.replace(old, new)
    open(W + path, 'w').write(s)
    print('patched', path)


# --- 1. eqp_solve.h: the ELIMINATED rhs and its refinement target -------------
patch('include/hven/detail/qp/eqp_solve.h', [
    ("""#include <hven/detail/kkt/kkt_assembly.h>""",
     """#include <hven/detail/kkt/kkt_assembly.h>
#include <hven/detail/qp/e3_prox_arm.h>"""),
    ("""    detail::factorize_checked(kkt, asm_.K);""",
     """    // M7 W2 E3 SCRATCH ARM: PROXIMAL CENTRING. K is untouched; the free
    // block's rhs gains +delta*x_k,F, which is the stationarity row of
    // min (1/2)x'Hx + g'x + (delta/2)||x_F - x_k,F||^2 on this face.
    if (detail::e3::prox_rhs() && detail::e3::centre->size() == n) {
        const Vec &xk = *detail::e3::centre;
        for (Index k = 0; k < n_free; ++k) {
            rhs(k) += opts.primal_delta * xk(asm_.free_of_full[static_cast<std::size_t>(k)]);
        }
    }
    detail::factorize_checked(kkt, asm_.K);"""),
    ("""    Vec reg = Vec::Zero(dim);
    reg.head(n_free).setConstant(opts.primal_delta);
    reg.segment(n_free, me + n_working).setConstant(-opts.dual_mu);""",
     """    Vec reg = Vec::Zero(dim);
    // E3 SCRATCH ARM mode 2/3: the primal delta is part of the MODEL (the
    // proximal term), so refinement backs out only the DUAL regularization.
    reg.head(n_free).setConstant(detail::e3::prox_refine() ? 0.0 : opts.primal_delta);
    reg.segment(n_free, me + n_working).setConstant(-opts.dual_mu);"""),
])

# --- 2. bordered_eqp.h: the BORDER rhs and its refinement target --------------
patch('include/hven/detail/kkt/bordered_eqp.h', [
    ("""#include <hven/qp/qp_types.h>""",
     """#include <hven/detail/qp/e3_prox_arm.h>
#include <hven/qp/qp_types.h>"""),
    ("""    reg.head(var_count).setConstant(opts.primal_delta);""",
     """    // E3 SCRATCH ARM mode 2/3: see eqp_solve.h. A pinned variable's own
    // delta*x_i is cancelled by delta*x_k,i (x_k sits at its pins), so the
    // unmasked form matches the eliminated path's.
    reg.head(var_count).setConstant(detail::e3::prox_refine() ? 0.0 : opts.primal_delta);"""),
    ("""    sys.rhs.head(n) = -qp.g;""",
     """    sys.rhs.head(n) = -qp.g;
    // E3 SCRATCH ARM: the proximal term, on every variable row (a pinned
    // variable's contribution is absorbed by its pin border, which holds
    // x_i == x_k,i).
    if (detail::e3::prox_rhs() && detail::e3::centre->size() == n) {
        sys.rhs.head(n) += opts.primal_delta * (*detail::e3::centre);
    }"""),
])

# --- 3. qp_engine.cpp: the centre guard and the VERDICT-SITE re-derivation ----
patch('src/qp/qp_engine.cpp', [
    ("""    Vec x = start_center(qp_in, seed, ws);""",
     """    Vec x = start_center(qp_in, seed, ws);
    // M7 W2 E3 SCRATCH ARM: this walk's proximal centre is its own iterate.
    const detail::e3::CentreGuard e3_centre_guard(&x);"""),
    ("""    Vec rhs(dim);
    for (Index k = 0; k < n_free; ++k) {
        rhs(k) = -qp.g(assembly.free_of_full[static_cast<std::size_t>(k)]) - assembly.rhs_shift(k);
    }""",
     """    Vec rhs(dim);
    for (Index k = 0; k < n_free; ++k) {
        rhs(k) = -qp.g(assembly.free_of_full[static_cast<std::size_t>(k)]) - assembly.rhs_shift(k);
    }
    // E3 SCRATCH ARM: the SAME rhs solve_eqp solved, proximal term included --
    // this site's own note says the two must stay in step.
    if (detail::e3::prox_rhs() && detail::e3::centre->size() == n) {
        const Vec &xk = *detail::e3::centre;
        for (Index k = 0; k < n_free; ++k) {
            rhs(k) += opts.primal_delta * xk(assembly.free_of_full[static_cast<std::size_t>(k)]);
        }
    }"""),
    ("""    Vec reg = Vec::Zero(dim);
    reg.head(n_free).setConstant(opts.primal_delta);
    reg.segment(n_free, me + n_working).setConstant(-opts.dual_mu);
    const auto residual_of = [&](const Vec &yy) {""",
     """    Vec reg = Vec::Zero(dim);
    reg.head(n_free).setConstant(detail::e3::prox_refine() ? 0.0 : opts.primal_delta);
    reg.segment(n_free, me + n_working).setConstant(-opts.dual_mu);
    const auto residual_of = [&](const Vec &yy) {"""),
])
print('E3_ARM_PATCH_OK')
