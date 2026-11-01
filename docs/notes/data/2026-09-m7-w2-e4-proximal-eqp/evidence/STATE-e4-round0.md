# M7 W2 E4 — running state

Branch `e4` on `c864b8a6` (sources = `0fa59cdc`). Builds `.scratch/nc2-build-{debug,release}`.
Evidence: `.scratch/e4/`. `.scratch/e3/`, `.scratch/nc2*/` are READ-ONLY.

## DONE
- Read: brief, E3 report/brief, the E3 arm diff, the EQP layer, the walk.
- IMPLEMENTED (uncommitted):
  - E4-1 proximal pair: `solve_eqp`/`solve_bordered_eqp` take a required `x_k`
    (proximal centre), rhs free block +delta*x_k (MASKED to free rows on the
    border path), refinement target = proximal (mu only backed out) on BOTH
    paths (`bordered_regularization` no longer reports primal_delta).
  - E4-4 EQP-site residual check in both solves: rows of (rhs - K y) vs
    kEqpResidualRefuseTol*(1+||rhs_rows||), up to kEqpResidualRefineIters=3
    extra solves, then REFUSE -> border retreats to eliminated, eliminated ends
    the solve kNumericalError. Counters eqp_refine_iters / eqp_residual_refusals.
  - E4-2 KKT-point test: zero motion OR (stat_free_inf <= opt_tol*stat_scale AND
    worst_structural_violation == 0); no snap on the second disjunct.
  - E4-3 runaway guards: is_runaway on every MOVING pass + a step-currency guard
    (p_inf > 0.1/delta*max(1,|x|) with a step the ratio test cannot take).
  - Docs: eqp_solve.h system section, qp_engine.h section 5, qp_types.h
    primal_delta, solver_counters.h, qp_walk_trace.h columns.

## NEXT
1. Debug build + targeted fixtures (3 runaway, flat-face, census/HS).
2. Corpus both algebras + E2 off vs `.scratch/nc2-fix1/corpus/` rows.
3. Release suites, census/HS, U0.
4. New fixtures (E4-7), Debug suites.
5. Report + ONE squashed commit on c864b8a6.

## 2026-09-26 22:1x — CALIBRATION ROUND (what the measurements changed)
- E4-3's ridge-scale STEP guard: implemented, MEASURED, REMOVED. It fires at
  iter 0 with alpha = 0 on an ordinary degenerate first step (HS10/HS27:
  p_inf 8e9 / 7.8e11, x_inf 0, kind != none) -> 2 kOptimal cells lost. Replaced
  by a NON-FINITE-ANSWER refusal at the EQP site (never ships a NaN in f).
  The iterate-currency guard (is_runaway on a moving pass) is what the three
  runaway fixtures need and they PASS unmodified (Debug + Release).
- E4-4's tolerance: 1e-6 relative REFUSED HS10/HS27 (row residual 3.1e-05 /
  7.8e-03 against mu*|lambda| ~ 1) -> both cells became kNumericalError. Target
  is now max(1e-1*(1+||rhs_rows||), mu*||lambda||).
- E4-4 on BOTH blocks: refused the first solve of every restoration cell
  (Cs_STEENBRB qp1 ratio 1.95, A0_YORKNET qp3 1.011) -> rows ONLY, per E4-4's
  own wording.
- bordered_regularization split into TARGET (delta-free) + FOOTPRINT (delta kept,
  the stopping rule's calibration against the eliminated path).
- Border proximal rhs: UNMASKED (all n variable rows), since masking leaves a
  delta*x_i on pinned rows the eliminated path does not have.
- OPEN (measured, not yet dispositioned): QpEngineBorder.BorderModeMatches-
  RefactorizeMode + RebuildUnderTinySchurCap (a weakly-active zero-multiplier
  bound tie on simple_box_qp's degenerate vertex, reached on the proximal walk's
  O(delta) cleanup pass); HsSweepCorpus.Corpus (B-1 complementarity 0.177 =
  mu*lambda^2 on HS10/HS33 warm points); HsBattery.CrashBasis (minors 802->846)
  and RefinementStepCountersOnBorderRepro (border_refine_steps 10->6) = counter
  pins; T7ACertifiedInfeasibleExit (factorizations 3->4).
- SCRATCH TO REMOVE BEFORE THE COMMIT: tests/qp/test_e4_scratch.cpp + its line in
  tests/CMakeLists.txt; .scratch/e4/tools/replay's `refuse=`/`residiters=` print.

## 2026-09-26 23:1x — ROUND CLOSED
Commit **c46485d8** on branch `e4`, parent c864b8a6, tree clean, both configs build.
Report: /home/ghecht/Projects/hven/.superpowers/m7-w2-e4-report.md (SIGNOFF: M7-W2-E4-REPORT).
Headline: 13 corpus conversions to kOptimal on the eliminated path (12 on border), NO
certified cell lost there, census 120/120, no NaN. Open: ACOPP57_def x4 lost on BORDER,
83/2863 Release failures (~25 pathology-pinning fixtures + 7 B-1-family), U0 walk terminal
stationarity 1e-15 -> 1e-11. Scratch scaffolding all removed from the tree.
