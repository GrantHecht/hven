> Placed by the settler from the Fable reviewer's hand-back text (subagent writes to `.superpowers/` are refused); the verdict and the blocking item are verbatim, and §§1–7 are condensed to their findings.

# M7 W2 NC3 ("the saddle exit"), Fable review of a522f115..054e4c78

## (1) THE DIRECTION: sound; a miss is the only failure mode
- **Same system as the re-read.** `saddle_direction` (`qp_engine.cpp:5229`) calls `eliminated_candidate` with `o.dual_mu = 0`, token for token the `read(0.0)` lambda of `reread_wrong_inertia` (:3607–3620). `view.eliminated` is set only, so `DepSystem` takes its eliminated branch on both algebras. The direction is algebra-independent by construction; the walk that reaches the dead end is not.
- **Lanczos (`ws_independence.cpp:635–719`) is correct.**
  - The operator is Z(Zᵀ(H+δI)Z)⁻¹Zᵀ, and the start vector lies in the face's null space.
  - Two full reorthogonalisation passes; invariant-subspace break.
  - Every negative Ritz value is walked in order and each one is re-measured on H itself.
  - The refinement loop makes the operator very slightly nonlinear. Lanczos tolerates that as a perturbation, and the verification covers it.
- **Verification is in the walk's currency.** It is `q <= -curvature_tol` (`kCurvatureTolFactor 1e-12 · hessian_scale`, the same threshold 4c tests) and `ride_stays_in_working_set` (the same predicate 4c's sign veto uses). Both are applied to the final ridden `d`; orientation only flips its sign.
- **A false ride is impossible.** Both acceptance tests read H and A_W directly, never the factor.
- **What a miss costs:** one factorization, ≤ 25·(1+kDepRefineSteps) solves, and the `solve_eqp` side-charges (`eqp_refine_*`, `eqp_polish_steps`). It then ends kNumericalError as before. Corpus confirms: ACOPR57_def qp6/qp8 and STEENBRB_def qp1 move by facts +1 only.

## (2) THE SIGN RULE and §3b
- **Flat slope:** `decrease = a·s + ½a²dᵀHd`, the correct second-order model. A tie goes to the sign leaving the released constraint. Sound.
- **Design note (not a defect):** a sign with no blocker scores −∞, which is truthful on an unboxed QP and moot on TR-boxed subproblems.
- **The anti-return rule closes §3b's route.**
- **The residual risk is REAL, and NC3 creates it.** A concrete scenario: a degenerate saddle vertex V with two constraints c and B through V, neither in the minimal W, with cᵀd > 0 and bᵀd < 0, and the post-probe restart spent.
  1. The probe drops c (exempt), and the dead end rides a zero length into B: ACCEPTED.
  2. At V on W∪{B}, the probe drops B; the dead end rides a zero length into c: ACCEPTED.
  3. At V on W∪{c}, the probe skips exempt c, and the walk returns **kOptimal at the saddle V**.
- The anti-return rule remembers only `released`; the exemption set is per-solve. FIX (small): in `orient_saddle_direction`, refuse a zero-length ride whose blocker is ANY probe-exempt member (`probe.ineq_exempt/bound_exempt`) or any member carrying `kPtReleased` in the point memory at this point. Not observed on any cell; a fixture can be built from the row-bounded synthetic by adding a second row through the saddle.

## (3) THE RIDE
- **The extraction is byte-identical;** 4c's ride is preserved.
- **The dependent-blocker route is correct.** It runs on the exit's μ-free eliminated factor, nothing is re-armed, and the standstill arm excludes `ride_blocked`.
- **Bounds:** one search per dead end.
  - Descent on a moving ride is a CHECK that is a theorem above the rounding threshold. It is asserted on the seam in every fixture.
  - A zero-length ride is bounded by point memory and the minor cap. Ping-pong would need an ascent the walk does not make, and the minor cap bounds it anyway.

## (4) Option interplay
- **The off arm is untouched by construction:** corpus E2-off 0/51 and U0 exact.
- **No interaction with fix 9's standstill retreat or with P1's batched `sync_borders`;** the code paths are disjoint.
- **The combined head is unmeasured.** Expected suites are **Release 2904 / Debug 2903**.

## (5) FIXTURES: honest
- The saddle and miss fixtures assert true, non-trivial things, and no objective is pinned.
- The driver premise redesign is legitimate: `probe-base.txt`'s 16 rows show ON = OFF.
- `PostProbeRestartIsOneShotPerSolve` and `QpE2Fix8Budget` ×2 are improvements with honest assertions. No numeric re-pin anywhere.

## (6) EVIDENCE
- Corpus: 5 rows/algebra. Hj_STEENBRD qp1 reaches two certified local minimizers, acceptable because the algebras already differed there at F9.
- Suites 2901/2900, the long test 627.59 s alone, and key tests 43/43 are supported.
- Re-derived by the reviewer: U0 0/0/0 vs fix 9's gated heads, and the TRAIN cellcmp matches the report.
- **Re-take 11 must measure on the combined head:** STEENBRC/E, NET3, BRITGAS, STEENBRB/D, the three new columns, and the 7 optimal cells + ACOPP118 (measured, not carried).

## (7) Hygiene
- The 4c header text is rule-stating.
- Rider: the test-file comment cites "the NC3 report" by name.
- Rider: one clause on the `eqp_polish_steps` side-charge in `solver_counters.h`.
- `grep -ci co-authored` = 0.

## Verdict: FIX-ROUND
1. **(blocking, small)** src/qp/qp_engine.cpp:5298–5307: extend the anti-return rule from `released` to the probe's exemption set and the point memory's `kPtReleased` members, so that a zero-length ride re-entering any of them is a miss. Add the degenerate-vertex fixture (a second row through the synthetic saddle), asserting status ≠ kOptimal at the saddle and kOptimal only at a PD face. Rationale: §2's scenario certifies a saddle (a wrong answer), the mechanism's one hazard class. The cost is a few lines, and no measured mover is expected.
2. **(non-blocking riders)**
   - (a) reuse the re-read's μ = 0 factor for the search;
   - (b) note the `eqp_polish_steps` side-charge in `solver_counters.h`;
   - (c) the gate declaration on the combined head: suites 2904/2903.

**Ledger:** NC3 reviewed FIX-ROUND 1. Direction, verification and ride are sound, and the option-off arm is untouched by construction. U0 0/0/0 at gate 0 and the TRAIN rows were re-derived. One blocking item: the anti-return rule must cover probe-exempt and point-memory-released members, because a degenerate saddle vertex can be re-certified through two zero-length rides. The combined head 054e4c78 is unmeasured; re-take 11 and the gate chain measure it (suites 2904/2903).

SIGNOFF: M7-W2-NC3-REVIEW-FABLE
