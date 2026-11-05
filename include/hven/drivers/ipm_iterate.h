// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

#pragma once

namespace hven {

/// @brief Per-iteration record: one instance per solver iteration, carrying
/// the residuals, barrier quantities, line-search outcome, and diagnostic
/// signals that iteration produced.
struct IpmIterate {

    int iter = 0;

    double mu = 0;
    double prim_obj = 0;
    double barr_obj = 0;
    double kkt_inf = 0;
    double barr_inf = 0;
    double econ_inf = 0;
    double icon_inf = 0;

    double pen_par1 = 0.0;
    double pen_par2 = 0.0;

    int ls_iters = 0;
    double alpha_p = 1.0;
    double alpha_d = 1.0;
    double alpha_t = 1.0;

    // THE IPM'S PER-ITERATE CURVATURE-CORRECTION RECORD: `h_pert`, `h_facs`,
    // `h_pert_cum` and the two proximal shifts below are what the classic inertia
    // ladder and the proximal mode did to THIS iteration's KKT diagonal -- the IPM's
    // counterpart of the IPQP ladder counters in core/solver_counters.h.
    // CURVATURE SOURCE: exact model Hessian (M9 label; the quantity this reads assumes H is
    // the model's second derivative -- a first-order mode re-derives or re-labels it).
    double h_pert = 0;
    /// Inertia-ladder steps this iteration's factorization took; 0 when none
    /// were needed. SENTINEL -1: the Newton direction came back non-finite, so
    /// no ladder ran and no count exists (alg_impl's !GoodStep branch). Not a
    /// step count of -1.
    int h_facs = 0;

    /// Running total of every inertia-perturbation delta applied to the KKT
    /// diagonal during this iteration's factorization — the CUMULATIVE value,
    /// where h_pert above is only the LAST delta.
    double h_pert_cum = 0;

    // Proximal primal-dual regularization shifts applied this iteration
    // (written only under inertia_mode_ == proximal_regularization; sentinel
    // -1 on the classic path). Both >= 0 when active; negative means "mode
    // off". Not printed (the iteration table formats an explicit field list).
    double prox_reg_primal = -1.0; ///< Persistent primal base shift rho_k on the Hessian diagonal.
    double prox_reg_dual =
        -1.0; ///< Barrier-scaled dual shift delta_c on the constraint-row diagonals (0 when
              ///< suppressed inside a nested l1 restoration phase).

    int p_pivots = 0;
    /// Was p_pivots an OBSERVATION, or the projection's substitute?
    ///
    /// KktFactorization::ppivs() projects an absent backend count to the
    /// integer 0 (`.value_or(0)`, kkt_factorization.cpp), and Apple Accelerate
    /// reports no perturbed-pivot count at all -- so a 0 there is a substituted
    /// value, not a measurement. fill_iter_info sets this
    /// from the factorization's own optional beside the projected int; it stays
    /// false on a record that was never factorized (the converge-check exit's).
    /// The projected int is unchanged and every existing consumer reads it as
    /// before.
    bool p_pivots_observed = false;
    double max_e_mult = 0;
    double max_i_mult = 0;
    double merit_val = 0.0;

    // Line-search acceptance outcome, recorded by the merit line search at
    // the point it decides accept vs. reject. Write-only diagnostics except
    // for the recovery-dispatch gate's read of accepted; not printed (the
    // iteration table formats an explicit field list).
    //
    // NORM-CONVENTION CALLOUT (theta_at_first_rejection): the convention
    // DIFFERS by acceptance path -- the classic variants store the SQUARED L2
    // norm of the full constraint block; the generic path stores the L1 norm.
    // Never compare across strategies; compare only against another reading
    // taken under the SAME acceptance path (the live consumer's caller,
    // SocRecovery::on_step_rejected, selects the norm via
    // drives_classic_path(), so the two readings soc_should_trigger()
    // compares always match). -1.0 means UNAVAILABLE (no rejection recorded,
    // or the LANG variant ran); a real infeasibility is always >= 0, so treat
    // any negative value as "skip theta-based logic", not as a feasible
    // reading.
    bool accepted = false; ///< Did the merit test accept a step this line search (false if every
                           ///< backtrack was rejected).
    int first_rejection_iter =
        -1; ///< Backtracking index of the first rejected trial (-1 if accepted with no rejection).
    double theta_at_first_rejection = -1.0; ///< Constraint infeasibility at that first rejected
                                            ///< trial; see the norm-convention callout above.

    /// Trial-point evaluations that threw during this iteration's acceptance
    /// attempts (line-search rungs, SOC/extended-backtrack trials,
    /// soft-feasibility trial); 0 on the no-exception path. Not printed. Each
    /// completed record is also handed once per iteration to a registered
    /// late callback; no Python surface exposes it.
    int eval_exceptions = 0;
};

} // namespace hven
