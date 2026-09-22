# M7 W2 E4 — the QP certificate reads the returned iterate's box

Branch `m7`. BASE `2a5b11f7`. Fix `45e114d4`, fixtures `99334900`, gate
declaration: the commit that adds this file. Brief:
`.superpowers/m7-w2-e4-certificate-brief.md`. Defect: Codex M5 review
Critical 1 / M5 README C7.

## What changed

`QpEngine::run`'s would-be-kOptimal branch (`src/qp/qp_engine.cpp`, after the
suspect-stall gate) now reads the returned x against every simple bound of the
engine's own box (`qp`, so the TR window counts). Each bound is treated as the
row e_i under the row check's own test, `violation_is_structural`, with
row_scale = max(1, |bound|, ‖x‖∞) and the bound's multiplier z(i). That is the
tolerance class `worst_structural_violation` applies to Ae/Ai rows:
`kInfeasibilityMarginFactor * max(feas_tol * scale, min(dual_mu * |z|,
kInfeasibilityAbsorbTol * scale))`, plus the residue that regularization owns.
It was picked so that a bound and a row with the same residue get the same
verdict. A structural violation exits **kInfeasible**, the status the engine
already uses for a primal-infeasible terminal point, and adds no new status.
The violation is recorded in the existing kInfeasible shape: the returned x
is the final iterate and carries it, and the multipliers are cleared. No
counter field was added. Such a field would move every includer of
`solver_counters.h`.

## Fixtures (both configurations, `hven_qp_walk_seam_tests`, 55/55 Debug and Release before commit)

| fixture | what |
|---|---|
| `ArmB1CanLeaveTheIterateOutsideItsBoxAndTheCertificateRefusesIt` | (i) M5's C7 state: box violation still 5.59e-03, the walk is unchanged, status is now kInfeasible and the multipliers are cleared |
| `TheCertificatesBoundHalfPassesASolvedQpWithActiveBounds` | (ii) the optimum lies on both sides of the box. Both ws_algebra paths, dual_mu 1e-8 and 1e-4: kOptimal at the analytic optimum with both bounds pinned. Measured box violation: 0 |
| every other fixture | (iii) G1/G3 below |

**A FINDING THE FIX EXPOSED (test-only arm).** M5's (8a), "b1 reaches the
problem's own optimum" on M4's period-2 fixture, stops with FREE x(2) at
9.33e-08 below its lower bound. That is 100× feas_tol. It passed only because
the KKT check ran at 1e-6. It is now kInfeasible. The test is renamed
`ArmB1EndsTheCyclingWalkNearTheOptimumButOutsideItsBox`, and the per-thread
fixture that replays this walk now expects kInfeasible. M5's b1 "conversion"
count on this fixture was therefore a false certificate as well.

**PRE-EXISTING, NOT E4.** `ScaleF7Slow.F7ProposalFailureEconomics`, which is
excluded from CTest registration, fails with identical numbers (off.majors
21 vs pinned 17, minors 4460 vs 2460) on the BASE engine source and on the fix.
It is registered as a stale pin and is not part of any gate here.

## Gates — DECLARED (before the chain ran) vs MEASURED

Declaration: `gates/psym-derivation-e4.txt`. Runner:
`wrappers/gates-e4.sh`. Chain: `wrappers/chain_gates_e4.sh`, detached with
`setsid`, and its pid is recorded in `.superpowers/m7-w2-e4-STATE.md`.

| gate | DECLARED | MEASURED |
|---|---|---|
| G1 Debug suite | 2733 tests, 0 failed | pending |
| G2 long test alone | 1, 0 failed | pending |
| G3 Release suite | 2734 tests, 0 failed | pending |
| G4 U0 replay | 27 cells × 75 columns × 3 engines, **0 differences**. Any moved cell is a DECLARED FINDING and STOPS the round | pending |
| G5 P-SYM | DIFFERING 1 = class A 1 (`CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o`) + class B 0 in BOTH; compare rc 1; negative control ≥ 1 | pending |
| G5b sections | NON-DEBUG-MOVER set == {`qp_engine.cpp.o`} in both; DEBUG_ONLY 0 / 0; REFUSED 0 | pending |
| G6 | smoke PASSED, export contract OK (align 32), clang-format 0, diff --check 0, dirty 0 | pending |
| fold proofs | 46 / 0 failed | pending |

## Registered, not this task's

The four-lane TRAIN table's hven-sqp column is NOT re-taken here. A re-take
would be a harness re-pin event. HOLDOUT is never touched.
