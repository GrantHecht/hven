// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

#pragma once

namespace hven::solvers {

/// @brief Per-iteration record: one instance per solver iteration, carrying
/// the residuals, barrier quantities, line-search outcome, and diagnostic
/// signals that iteration produced.
struct IterateInfo {

    int iter_ = 0;

    double mu_ = 0;
    double prim_obj_ = 0;
    double barr_obj_ = 0;
    double kkt_inf_ = 0;
    double barr_inf_ = 0;
    double econ_inf_ = 0;
    double icon_inf_ = 0;

    double pen_par1_ = 0.0;
    double pen_par2_ = 0.0;

    int ls_iters_ = 0;
    double alpha_p_ = 1.0;
    double alpha_d_ = 1.0;
    double alpha_t_ = 1.0;

    double h_pert_ = 0;
    int h_facs_ = 0;

    /// Running total of every inertia-perturbation delta applied to the KKT
    /// diagonal during this iteration's factorization — the CUMULATIVE value,
    /// where h_pert_ above is only the LAST delta.
    double h_pert_cum_ = 0;

    /// Proximal primal-dual regularization shifts applied this iteration
    /// (written only under inertia_mode_ == proximal_regularization; sentinel
    /// -1 on the classic path). prox_reg_primal_: persistent primal base shift
    /// rho_k on the Hessian diagonal; prox_reg_dual_: barrier-scaled dual shift
    /// delta_c on the constraint-row diagonals (0 when suppressed inside a
    /// nested l1 restoration phase). Both >= 0 when active; negative means
    /// "mode off". Not printed — console output stays byte-identical.
    double prox_reg_primal_ = -1.0;
    double prox_reg_dual_ = -1.0;

    int p_pivots_ = 0;
    double max_e_mult_ = 0;
    double max_i_mult_ = 0;
    double merit_val_ = 0.0;

    /// Line-search acceptance outcome, recorded by the merit line search at
    /// the point it decides accept vs. reject. Write-only diagnostics except
    /// for the recovery-dispatch gate's read of accepted_; never printed
    /// (console output stays byte-identical).
    ///  - accepted_: did the merit test accept a step this line search (false
    ///    if every backtrack was rejected).
    ///  - first_rejection_iter_: backtracking index of the first rejected
    ///    trial (-1 if the step was accepted with no rejection).
    ///  - theta_at_first_rejection_: constraint infeasibility at that first
    ///    rejected trial. NORM-CONVENTION CALLOUT: the convention DIFFERS by
    ///    acceptance path — the classic variants store the SQUARED L2 norm of
    ///    the full constraint block; the generic path stores the L1 norm.
    ///    Never compare across strategies; compare only against another
    ///    reading taken under the SAME acceptance path (the live consumer,
    ///    soc_should_trigger(), selects the norm via
    ///    drives_classic_path() so its two readings always match). -1.0 means
    ///    UNAVAILABLE (no rejection recorded, or the LANG variant ran); a real
    ///    infeasibility is always >= 0, so treat any negative value as "skip
    ///    theta-based logic", not as a feasible reading.
    bool accepted_ = false;
    int first_rejection_iter_ = -1;
    double theta_at_first_rejection_ = -1.0;

    /// Trial-point evaluations that threw during this iteration's acceptance
    /// attempts (line-search rungs, SOC/extended-backtrack trials,
    /// soft-feasibility trial); 0 on the no-exception path. Not printed. Each
    /// completed record is also handed once per iteration to a registered
    /// late callback; no Python surface exposes it.
    int eval_exceptions_ = 0;
};

} // namespace hven::solvers
