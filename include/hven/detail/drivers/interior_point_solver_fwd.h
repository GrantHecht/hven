// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

#pragma once

// hven::ConvergenceFlags -- the interior-point engine's own five-value verdict
// enum, with its <=> severity ordering -- was REMOVED in M6 W5 T8.4. Both
// engines now report hven::solvers::SolveStatus (drivers/solve_status.h), and
// the interior-point engine uses it internally as well, so there is no mapping
// step left to get out of step with the verdict it maps. The name table:
//
//   CONVERGED    -> SolveStatus::kOptimal
//   ACCEPTABLE   -> SolveStatus::kAcceptable
//   NOTCONVERGED -> SolveStatus::kMaxIter, or kStalled at the stall and
//                   locally-infeasible-restoration exits -- the split
//                   resolve_ipm_phase_status() makes from the stop reason
//   DIVERGING    -> SolveStatus::kDiverging
//   SINGULAR_KKT -> SolveStatus::kNumericalError
//
// The severity ordering it carried is severity(SolveStatus), which orders all
// nine and keeps the old five in their old relative order.

namespace hven::solvers {
class InteriorPointSolver;

// Step-acceptance strategy selector (IpmOptions::acceptance_strategy).
// Declared here rather than nested in InteriorPointSolver so both
// IpmOptions and the acceptance components can name it
// without a circular include.
//   classic_merit — the fused classic backtracking merit line search
//                   (ClassicMeritAcceptance); the bit-identical default.
//   merit         — the modernized merit family driven through the GENERIC
//                   AcceptanceStrategy path (ModernMeritAcceptance).
//   funnel        — the scalar-funnel-width strategy on the shared
//                   Wächter–Biegler switching skeleton (FunnelAcceptance).
//   filter        — the (θ, φ)-pair filter strategy on the shared
//                   Wächter–Biegler switching skeleton (FilterAcceptance).
enum class AcceptanceStrategies { classic_merit = 0, merit = 1, funnel = 2, filter = 3 };

// Penalty-parameter rule for the modernized merit family
// (IpmOptions::merit_penalty_rule; read only when
// acceptance_strategy_ == merit).
//   wmno     — Waltz, Morales, Nocedal & Orban, Math. Program. 107 (2006),
//              §3.1: single penalty ν updated from the directional-derivative
//              condition (Eqs 3.5, 3.6).
//   flexible — Curtis & Nocedal, IMA J. Numer. Anal. 28(4) (2008): a penalty
//              INTERVAL [π_l, π_u]; a step is accepted if it reduces the merit
//              for at least one π in the interval (Eqs 2.1, 3.9, 3.10).
enum class MeritPenaltyRules { wmno = 0, flexible = 1 };

// Barrier-parameter governor selector (IpmOptions::barrier_governor),
// declared here for the same no-circular-include reason as the selectors above.
//   classic_adaptive — the classic PROBE/LOQO free-mode barrier update
//                      (ClassicAdaptiveGovernor); the bit-identical default.
//   monitored        — the free<->monotone monitored governor
//                      (MonitoredBarrierGovernor): a KKT-error monitor hands
//                      off to a Fiacco-McCormick monotone mode when free-mode
//                      progress stalls, then re-enters free mode once
//                      progress resumes. Composes a ClassicAdaptiveGovernor
//                      as its free-mode delegate, so any acceptance strategy
//                      may pair with it.
enum class BarrierGovernors { classic_adaptive = 0, monitored = 1 };

// Feasibility-restoration mode selector (IpmOptions::restoration_mode),
// declared here for the same no-circular-include reason as the selectors above.
//   off             — no feasibility restoration (default). A ladder-exhausted
//                     rejection is taken as-is; no RestorationStrategy is
//                     constructed, so every restoration branch in the solver
//                     is provably dead.
//   proximal_switch — the proximal feasibility mode-switch
//                     (ProximalSwitchRestoration): on a ladder-exhausted
//                     rejection, keep the same barrier algorithm running but
//                     swap the true objective for a proximal term pulling the
//                     primals back toward the entry point, until infeasibility
//                     is sufficiently reduced. Composes with every acceptance
//                     strategy and barrier governor.
//   l1_nested       — the nested l1 elastic feasibility restoration
//                     (NestedL1Restoration): on a ladder-exhausted rejection,
//                     solve the l1 elastic reformulation (Ipopt-lineage
//                     restoration NLP) as a CONDENSED in-place phase that
//                     reuses the outer barrier algorithm's KKT system, rather
//                     than switching the outer objective the way
//                     proximal_switch does. Every shipped acceptance strategy
//                     implements the restoration exit test, so l1_nested
//                     composes with every acceptance_strategy and
//                     barrier_governor exactly like proximal_switch (no
//                     matrix restrictions).
enum class RestorationModes { off = 0, proximal_switch = 1, l1_nested = 2 };

// KKT inertia-correction / regularization mode selector
// (IpmOptions::inertia_mode), declared here for the same
// no-circular-include reason as the selectors above.
//   classic                 — the on-demand inertia ladder inline in
//                             InteriorPointSolver::factor_impl (the bit-identical default):
//                             each iteration first attempts an unperturbed
//                             factorization and only shifts the Hessian diagonal
//                             (by increasing amounts) when the factorization
//                             reports wrong inertia. No constraint-block shift.
//   proximal_regularization — proximal primal-dual regularization: a small
//                             persistent, decaying primal base shift ρ_k on the
//                             Hessian diagonal plus an always-on barrier-scaled
//                             dual shift −δ_c on the constraint-row diagonals are
//                             baked into the base matrix each iteration, and the
//                             same ladder escalates on top when the base attempt
//                             has wrong inertia or is singular. The dual shift is
//                             suppressed while a nested l1 restoration phase is
//                             active (the elastic pivots own those slots).
enum class InertiaModes { classic = 0, proximal_regularization = 1 };

} // namespace hven::solvers
