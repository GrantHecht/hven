# M7 W2 NC2 — the flat face

Brief: `.superpowers/m7-w2-nc2-brief.md` (M1–M25: M1–M9 the round's terms, M10–M15 the settler's
acceptance rulings, M16–M20 the review acceptance and fix round 1's items, M21–M25 the fix round's
own measurement, rulings and declared skip). Implementer reports: `REPORT.md` (NC2) and
`REPORT-fix1.md` (fix round 1), verbatim copies of `.superpowers/m7-w2-nc2-report.md` and
`.superpowers/m7-w2-nc2-fix1-report.md`. The review that opened the fix round is `REVIEW-fable.md`. Base `828dc917` (the NC1
ledger docs record; its sources are NC1's gated sources `5732a1f3`, checked: no file under
`include/`, `src/` or `tests/` differs). Origin: the R2 diagnosis
(`../2026-09-m7-w2-r2-composite-landing/REPORT.md` — "recommendation: QP engine first (the flat
face)").

When the QP walk drops a constraint onto a face with NO curvature — an LP face, where the
Rayleigh quotient of the EQP step is exactly 0 against a finite `curvature_tol` — the direction is
a ray, and the engine used to decide the ride's sign from the measured slope `gᵀp`. On these faces
that measurement is dominated by the point's own stationarity residual ρ (`gᵀp = z_s·a_sᵀp + ρᵀp`
exactly, every retained row being orthogonal to `p`), and at the multiplier magnitudes the
restoration LPs reach (1e-7…1e-8) the residual term is the bigger one: all 2175 observed vetoes
were sign DISAGREEMENTS between the measured and the predicted slope, which sent NC1's feasibility
half to undo the drop and the solve to zero-motion exhaustion or the pass cap. NC2 takes the
sign from the drop's own multiplier instead — the KKT-exact predicted slope `s = z_s·(leaving rate
along p)` — and rides the ray to its blocker with NC1's blocker rules; a drop whose *predicted*
slope is itself at noise level is UNDONE and counted. The equality drift (M3) ships only as its
checked, counted invariant with NO re-projection (`eq_drift_violations`); the saddle exit (M4) did
not ship and is registered as its own round. The rule is stated in
`include/hven/detail/qp/qp_engine.h` §4c; two counters are appended to `QpCounters`
(`ride_predicted_signs`, `eq_drift_violations`).

## Fix rounds

| Round | Commit | What landed | Fixtures | Declared movers | Gates |
|---|---|---|---|---|---|
| NC2 | `71f54c50` (cherry-pick of `ce3963b0`) | the flat-face sign rule (predicted slope from the drop's multiplier; noise-level predicted slope ⇒ undo + count), `src/qp/qp_engine.cpp`, header §4c; the equality drift as a checked, counted invariant with no re-projection; QpCounters: 2 new fields (`ride_predicted_signs`, `eq_drift_violations`) | `QpNc2FlatFace.*`, 8 instances: `AWrongSignedDropOnAnLpFaceRidesTheRayToItsBlocker`, `ANoiseLevelDropIsUndoneAndCountedNotRidden` (synthetic), and `TheSteenbrbRestorationLpQp{2,3,4}TakesTheMultipliersSign{Refactorize,Border}` — **renamed by fix 1** to `…NeverAscendsTheSlope{Refactorize,Border}`, which is what the tree and this declaration carry — (the three R2 dumps × both algebras, `tests/qp/fixtures/e2-d4/R2_Cs_STEENBRB_qp{2,3,4}.txt`; they pin the MECHANISM — `ride_predicted_signs ≥ 1`, `zero_motion_exhausted == 0`, KKT + oracle when optimal — not the conversion, which is configuration-dependent: M15) | corpus kOptimal 12/51 → **15/51** (`Cs_STEENBRB/qp{2,3,4}` convert to the Clarabel optimum); `Cs_STEENBRB/qp1` num 219 → cap 6600; `Cs_STEENBRB/qp5` undos 23 → 0, `qp6` undos 816 → 0 (both stay capped); `ACOPR57_def/qp6` 9th-digit f on a capped cell; census 120/120; U0 EXACT (0 differences on walk, ssn and ipm); HS unmoved; **0 re-pins** | this declaration, jointly with fix 1 |
| NC2 fix round 1 | `0fa59cdc` (cherry-pick of `7b36a01e`) | **the rule reversed by measurement**: ρ is the PRIMAL regularization's own term (‖ρ‖∞ ≈ 0.28–0.47·δ‖x‖∞, the μ footprint 8 decades below), so the MEASURED `gᵀp` — on an LP the exact directional derivative — decides the ride's sign whenever it resolves, and NC2's predicted slope survives only as its floor (M2's literal floor, **0 firings on 51 dumps**). A drop whose multiplier sits below `max(dot-product floor, \|gᵀp − s\|)` is a NOISE drop, undone and marked `kPtNoise` on that test (`ride_drop_is_noise`, no extra solve). **A zero-length ride is a zero-motion event** (no `pt_clear` unless `dx_inf > step_tol`; the blocker `pt_admit`ted). Header §4c made terse (M19). M21(a)'s regularization-free re-price REFUSED on cost and registered to the EQP-side round (M23) | the same 8 `QpNc2FlatFace.*` instances, six renamed `…NeverAscendsTheSlope{Refactorize,Border}`, now pinning the **descent invariant** (`sign·gᵀp ≤ kEngineDenomTol·‖g‖·‖p‖`, `ascending_rides == 0`) and the absence of zero-motion exhaustion rather than NC2's status claim. **One declared `GTEST_SKIP`** — Cs qp2 border in Debug, 745 s against M6's 600 s budget (M25); Release asserts both algebras and Debug refactorize covers the same walk | corpus **12/51 → 14/51** (NC2 was 15/51): Cs qp2/qp4 num → opt from the corrected noise test, Cs qp1 num → cap (residuals 3–7 decades better), Cs qp3/qp5/qp6 and `ACOPR57_def/qp6` **back to base exactly**; from the zero-length-ride rule `A0_YORKNET/qp2` strictly better (eq 2.5e-3 → 5.7e-14, stat 2e-2 → 1e-16), `Hj_STEENBRD/qp9` +2 minors, `STEENBRB_def/qp1` refactorize cap → num, `STEENBRB_def/qp2` stationarity 2 decades worse on a capped elastic cell (accepted, registered) — all M24. Census 120/120; U0 0 differences on all three engines; option-off byte-identical to base on all 51; HS unmoved; **0 re-pins** | this declaration |

**One declaration covers both commits** (M16; precedent E2 fix 1 + fix 1b under `3e2bed07`): the
gates run once, on the final range `828dc917..0fa59cdc`. Every declared value is derived from that
range and from a fresh P-SYM rehearsal on the final pair — nothing is carried from NC1, and nothing
from the NC2-only range this declaration's first draft measured before the fix round landed (that
draft's numbers were replaced, not extended; the fix round renamed six of the eight test instances,
which is exactly why). The declaration is `wrappers-gates/psym-derivation-nc2.txt`, its pre-flight
transcript is `wrappers-gates/preflight.txt`, and both were committed before the chain was
detached.

Registered alongside (M12–M15, M20, M23–M25): the EQP-side round is now the design centre of the
registered work — M21(a)'s regularization-free re-price is already what `solve_eqp_eliminated` does
and removes at most 56% of ρ on a flat face, because the regularized factor is an O(1)-error
preconditioner there; at δ = 1e-9 ρ → 1e-20 and 0 rides ascend, a different walk and a declared
confounder. Also registered: the 833× border/refactorize cost on Cs qp2's add-heavy walk at
`schur_cap` 128 (the reason for M25's skip), and `STEENBRB_def/qp2`'s two-decade stationarity loss
on a capped elastic cell. Successor note on the NC2 report §7 (M25): an eighth base → NC2 mover
existed, `A0_YORKNET/qp2` in counters only (exch 354 → 68, undo 3016 → 0), invisible to the
status/minors/f comparator. Carried from NC2: the equality drift's cause is an EQP-side item (a regularized
EQP answer missing its own rows by 1/δ-sized amounts — 7.1e2 on YORKNET at pass 177 — the E2-D2
multiplier-leak class), and the walk-side re-projection must not be retried before it; the saddle
exit becomes NC3, starting from a redesign of the four driver fixtures that have a QP
`kNumericalError` as their premise; the RE-TAKE 8 prior stays 0 TRAIN conversions with the
per-cell on-row proof in M20(c); the lane side-file goes 106 → 108 columns for the two appended
counters.
