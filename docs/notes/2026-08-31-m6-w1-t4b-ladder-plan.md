# M6 W1 — T4b: the inertia ladder, re-planned (DRAFT for owner + tycho-sqp review)

Status: v3 APPROVED BY OWNER (Grant, direct, 2026-08-31 — "approved"); Q-O2
ruled option A per this plan. Executes as T4b after T6. Settler-authored; v2 folds the tycho-sqp lane's
review (GO with five amendments; `.superpowers/w1-t4b-plan-tycho-review.md`)
and Codex's design review (SOUND WITH CHANGES, five required changes, all
folded; `.superpowers/w1-t4b-plan-codex.md`). Open owner question Q-O2 (ledger). Nothing here is implemented.
Ownership note for the record (tycho-sqp): the monotone-per-solve floor came
from their W1 review ("make the ladder monotone — no flapping"); correct for
SSN's ladder, whose sigma anchors matrix AND residual at the current iterate,
wrong for a design with a lagging prox centre. IC memory is the right answer
to flapping; the floor is deleted as planned.

## 0. The problem in one paragraph

No Hock–Schittkowski indefinite-Hessian row converges under the IPQP tier at
W1 head (T5 report §S3): each burns its full budget at an exact fixed point of
a shifted problem with `alpha = 1` and `|dx| = 1e-17`. Acceptance cell A11's
HS half is pinned NOT MET. The convex F7 corpus never arms the ladder and is
untouched. The cause is not one bug but a design that lets two different
regularizations be confused with each other, plus a monotone rule the cited
algorithm does not have.

## 1. What actually happens (four mechanisms, one more than §S3 found)

The tier carries TWO primal regularizations that the spec says are different
things and the code treats as one number:

- `rho_sched` — §3.2's PROXIMAL term. Part of the subproblem being solved:
  `min Q(x) + (rho_sched/2)||x - zeta||²`. Enters the KKT diagonal AND the
  right-hand side as `rho_sched (x - zeta)`, anchored at the last prox centre
  `zeta`; decreases only when the gate sees the regularized residual contract.
- `rho_dem` — §2.2's inertia-demanded MODIFICATION (Ipopt's `delta_w`). NOT
  part of the subproblem: "applied on a CURRENT-ITERATE anchor", i.e. its
  proximal term is `(rho_dem/2)||x - x_k||²`, whose gradient at `x_k` is ZERO.
  It should enter the Newton matrix diagonal only.

Today `rho = max(rho_sched, rho_floor)` is one variable used everywhere
(`ipqp_engine.cpp:1797`, `build_rhs` :1471 `rho * (x - zeta)`,
`write_diagonals`). Mechanisms, in the order they bite on
`indefinite_equality_qp` (eig(H) = {1, −1, −2}):

1. **Gate before ladder, same iteration.** `rho_sched` decays 8 → 0.8 before
   the first inertia read; 8 was already sufficient (H + 8I ≻ 0).
2. **Rung geometry.** First rung `max(rho × 100, 1e-4)` from the DECAYED value
   → 80; two decades per rung cannot land between 0.8 and 80; needed ≈ 2.
   Overshoot 8–40× on the three rows.
3. **Monotone-per-solve floor** (§2.2 item 3, §3.2 bullet 3 — an hven addition;
   Wächter–Biegler Algorithm IC, which §2.2 cites by name, restarts each new
   trial at 1/3 of the last shift) makes 80 permanent.
4. **Two regularized problems (NEW).** The step is the Newton step of
   `Q + 40||x - zeta||²` (RHS uses 80), so the iterate converges to THAT
   problem's KKT point. The §3.2 gate measures progress on the subproblem it
   defines (schedule value 0.8) — at the 80-problem's fixed point that residual
   is a nonzero constant (`≈ 79.2·|x - zeta|` on stationarity), so it never
   contracts, the gate never advances, `zeta` never moves, `rho_sched` never
   falls, and the point is frozen for 56 iterations with full steps. This is
   the mechanism that makes the freeze EXACT rather than slow, and it survives
   any choice of rung constants as long as the RHS and the gate disagree about
   which subproblem is being solved. CONFIRMED BY INSPECTION: the gate's
   regularized residual is built with `rho_sched * (x - zeta)`
   (`ipqp_engine.cpp:1697`) while the step's RHS is built with the total
   `rho * (x - zeta)` (`:1488`). T4b step 0 still traces the gate's residual
   at the frozen point to record the constant it sits at.

## 2. Design (what T4b builds)

### 2.1 Separate the two regularizations — one definition of "the subproblem"

- The proximal subproblem is `(rho_sched, delta_sched, zeta, lambda_est)` and
  NOTHING else: the RHS proximal terms, the gate's regularized residual, and
  the prox-centre update all use `rho_sched` / `delta_sched`.
- `rho_dem` enters the Newton matrix primal diagonal ONLY, as a UNIFORM shift
  in the Ruiz-SCALED system the inertia is read on: unscaled diagonal entry
  `i` becomes `src[i] + rho_sched + rho_dem / d_i^2 + sigma[i]` (so that after
  `D K D` the shift is `rho_dem` in every coordinate; with Ruiz off,
  `d_i = 1`). Additive, not `max`: the proximal Hessian is part of the
  subproblem; the modification is added to it. `ipqp_reg_max` caps
  `rho_dem` (the scaled uniform value), not the total. When `rho_sched`
  already makes the reduced inertia right, `rho_dem = 0` and the code path
  MUST execute today's exact expression `(src[i] + rho_sched + sigma[i]) *
  dsq[i]` unchanged — an explicit `rho_dem == 0` branch, never a `+ 0.0`
  folded into a re-associated sum — which is what makes the convex corpus
  bit-identical under this plan (gate 4 below; Codex).
- THE PROPERTY 2.1 BUYS (tycho-sqp, scoped by Codex): on the reduced primal
  system with multipliers and slacks held fixed, the full step
  `d = -M^-1 grad phi` (`M = H + (rho_s + rho_d) I + Sigma > 0`, `phi` the
  schedule's barrier-proximal subproblem) satisfies
  `phi(x + d) - phi(x) = -1/2 grad phi' M^-1 grad phi - 1/2 rho_d ||d||^2 < 0`
  — a DESCENT step on the schedule's subproblem for every `rho_d >= 0`, with
  fixed points at stationary points of `phi`, never of a shifted problem.
  This is MOTIVATION, not a theorem about the executed step: the actual
  direction is the coupled predictor-corrector KKT solve (equality,
  slack, `-delta I` blocks) followed by fraction-to-boundary scaling. The
  executable statement is gate 9's invariant on the actual system, and the
  design consequence that survives the caveat is the one that matters: a
  fixed point of the executed map is a point where the RHS the step is built
  from is zero, and 2.1 makes that RHS the schedule's own residual, so the
  gate sees it. The gate
  then sees the residual reach zero, advances, `zeta <- x`, `rho_sched`
  decays, and the outer proximal-point loop proceeds toward a stationary point
  of the caller's QP. The §2.2 item 4 final read (at `rho_sched = reg_floor`,
  `rho_dem = 0`, one factorization — plan §7 note (i)) decides the certificate
  exactly as today. Closed form of today's freeze, for the record: at the
  frozen point `grad Q + 80 (x - zeta) + bounds = 0`, so the gate's residual
  `grad Q + 0.8 (x - zeta) + bounds = -79.2 (x - zeta) != 0` — a constant that
  cannot contract because `x` does not move.
- THE GAP 2.1 OPENS (tycho-sqp, correctness amendment 1). The same algebra
  gives `grad phi(x + d) = rho_d M^-1 grad phi(x)`: per eigen-direction of `M`
  the factor is `rho_d / (lambda_i + rho_s + rho_d)` — `< 1` where
  `H + rho_s I + Sigma` has positive curvature, `> 1` wherever
  `lambda_i + rho_s < 0`. While the ladder is armed the inner iteration is a
  descent method on `phi` whose RESIDUAL IS NOT MONOTONE: it grows along the
  subproblem's negative-curvature directions until `Sigma` (a bound
  approaching) or the equality rows make them positive. The §3.2 gate
  measures residual contraction, so on a nonconvex row it is SILENT (no
  advance, `zeta` and `rho_s` pinned) for the whole negative-curvature walk to
  the bound face, resuming only when the reduced curvature at the iterate is
  positive and `rho_d -> 0` — at which point one Newton step lands the
  residual at machine precision (why IC's "try zero first" is essential, not
  cosmetic). Not a freeze — `x` moves, `phi` decreases, `mu` falls, `alpha` is
  fraction-to-boundary-limited — but a LONG walk (small |lambda_min|, distant
  bound) sits inside §6.2's residual clause with the gate never advancing.
  §6.2 needs ALL THREE conjuncts, and conjunct (iii) (min alpha < 1e-2 on
  every window step) should not hold on a full-step walk — so the gap is
  expected to be benign, but that is a prediction, not evidence. REQUIRED:
  (b) the long-walk fixture in §3 (gate 10) FIRST; (a) ONLY IF that fixture
  shows §6.2 firing: while `rho_d > 0`, an accepted step that decreased the
  schedule's barrier-proximal merit counts as inner progress for the stall
  window (window reset) WITHOUT moving `zeta` or decaying `rho_s` — the
  schedule still advances only on residual contraction, so convex cells
  (`rho_d = 0`) are untouched and gate 4 holds. Either way the new counter
  `ipqp_iters_ladder_armed_no_advance` records the walk.

### 2.2 Replace the monotone floor with Algorithm IC's memory (as Ipopt implements it)

Per iteration, before the first factorization:

```
trial:
  if rho_dem_last == 0 or fewer than kLadderSkipAfter (= 3) CONSECUTIVE
     preceding iterations needed rho_dem > 0:
                       trial = 0                    (IC-1: try the unmodified system)
  else:                trial = max(reg_floor, rho_dem_last / kLadderDown)   kLadderDown = 3
on wrong inertia at trial 0:
  if rho_dem_last == 0: rho_dem = kLadderInit                                 1e-4
  else:                 rho_dem = max(reg_floor, rho_dem_last / kLadderDown)
on wrong inertia at rho_dem > 0:
  if rho_dem_last == 0: rho_dem *= kLadderUpFirst   (the WHOLE first climb)   100
  else:                 rho_dem *= kLadderUp                                   8
  cap: rho_dem > ipqp_reg_max -> exhausted -> escape (classification per note (h))
on success:             rho_dem_last = rho_dem (updated only on a successful MODIFIED
                        factorization; a success at trial 0 leaves it unchanged, as IC does)
```

- This is Wächter–Biegler 2006 Algorithm IC's rules and constants
  (`delta_w^0 = 1e-4`, `kappa_w^- = 1/3`, `kappa_w^+ = 8`,
  `bar kappa_w^+ = 100`) with two DECLARED ADAPTATIONS, so it is NOT called
  "verbatim" (Codex): (i) the paper's `delta_w^min = 1e-20` / `delta_w^max =
  1e40` are hven's `ipqp_reg_floor = 1e-10` / `ipqp_reg_max = 1e6` (the
  spec's own bounds); (ii) the unmodified trial (IC-1) is skipped only after
  three consecutive iterations needed a modification — the deviation Ipopt's
  implementation itself documents (paper p. 10) — never from the first
  modification on. Ipopt's four constants are 20 years of CUTEst; do not tune
  them on three HS rows (tycho-sqp). Note for the spec fold: §2.2's claim
  that the in-tree NLP driver "already implements" IC is too strong — its
  defaults are 1e-5 / 8 / 0.333 with its own cycling heuristic
  (`interior_point_solver.h:419-425`, `.cpp:2537-2552`); the spec sentence is
  corrected to "IC-derived (1e-5, ×8, ÷3, cycling guard)" so no reader infers
  a parity that is not there. The QP tier uses the PAPER's constants, not the
  driver's (tycho-sqp): the shift is scale-relative (the driver's acts on the
  Lagrangian Hessian at its own scaling, the tier's on the Ruiz-scaled QP
  matrix — same numbers would not be same behaviour); the cycling guard
  exists because an NLP's Hessian changes every iteration and the memory can
  go stale, whereas the QP's H is FIXED for the solve; and source parity is
  about contracts, not shared tuning constants. REGISTERED (M7, one line):
  measure paper constants vs the driver's on the same instrumented fixtures
  rather than settling it by fiat.
- The "monotone per solve" sentence and §3.2's "the monotone floor overrides
  the decrease" bullet are DELETED; the `rho_floor` variable goes with them.
- EVIDENCE FAILURE (unavailable inertia; T5 note (n)) under IC memory —
  defined, since `rho_floor` no longer carries it (Codex): the iteration
  runs at `rho_dem = max(trial, kIpqpEvidenceFailureRhoFloor)`, that value IS
  recorded as `rho_dem_last` (so later trials descend /3 from it and are
  floored again while evidence stays unavailable — a constant floor on an
  always-unavailable backend, as today), the iteration is counted as
  armed, and the whole-solve downgrade is armed as today. Evidence failure
  never raises the floor above `max(trial, floor)`.
- Scalar ILLUSTRATION only (Codex recomputed; the real threshold on an HS
  row is set by the assembled Schur complement with `Sigma`, finite
  `delta_sched`, equality rows and Ruiz — gate 7): threshold for the
  ADDITIONAL shift is `θ = |λmin| - rho_sched = 2 - 0.8 = 1.2`. it 1: trial 0
  wrong → 1e-4 → 1e-2 → 1 → 100 right (4 rungs, once per solve; W-B's reason —
  no scale information on the first climb — stands). Then, with trial 0
  retried until three consecutive iterations needed a modification: 33.3,
  11.1, 3.70, 1.23 (right — total 2.03 > 2), 0.41 wrong → 3.29, 1.10 wrong →
  8.78, 2.93, 0.98 wrong → 7.80 … Accepted values cycle inside `(θ, 8θ]` =
  `(1.2, 9.6]` with a reclimb every ~2 iterations ≈ 0.5 extra
  factorizations/iteration. Ipopt's own behaviour, accepted by Ipopt. Registered refinement, NOT built
  in T4b: a "sticky trial" (if last iteration's /3 trial failed, start at
  `rho_dem_last` instead) — decide on T9's measured factorization counts.
- Per-QP-solve memory: `rho_dem_last` starts at 0 on every cold `solve()`.
  Carrying it is T7's call; tycho-sqp's vote, adopted as the recommendation:
  (i) trust-region shrink-retries WITHIN a major (same H, same pattern,
  smaller box) carry `rho_d_last` VERBATIM as the first trial (not /3) —
  resetting re-climbs 3–4 rungs per retry for nothing, and the design already
  calls TR retries "warm restarts, never escapes"; (ii) across majors carry it
  as the IC seed (trial = `rho_d_last / 3`, exactly Ipopt's cross-iteration
  use, `H_{k+1} ≈ H_k` near convergence) as an engine-owned tagged extension of
  the warm-start currency under the DeclarationKey stamp, DISCARDED on any key
  change (mesh transfer, continuation over n) — the M5 lesson: carry only what
  describes the same declared problem. Two hard constraints either way: it is
  a trial seed, never a floor; the final read is always `rho_d = 0`. Cost on
  convex sequences: zero (the carried value is 0).
- Evidence failure (T5, note (n)): `rho_dem = max(trial,
  kIpqpEvidenceFailureRhoFloor)` on unavailable evidence; downgrade as today.
- Dual shift `delta` (perturbed-pivot branch): unchanged.
- WHERE THE SHIFT ACTS (tycho-sqp): `write_diagonals` writes the UNSCALED
  matrix and the Ruiz scaling `D K D` is applied after, so a uniform `rho_d`
  in unscaled space is the NON-uniform shift `rho_d d_i^2` in the scaled
  system the inertia is read on — a coordinate with tiny `d_i` gets almost
  nothing and the ladder must climb far to cover it, over-shifting every
  other coordinate. Ipopt adds `delta_w` AFTER its scaling. T4b applies the
  modification in SCALED space — the single definition is in 2.1 (`rho_d /
  d_i^2` on the unscaled diagonal; cap on the scaled value). IC's constants
  are scale-free only under this choice. Convex cells: `rho_d = 0`, gate 4
  holds.
  (`rho_sched` stays where it is — it is part of the caller's subproblem, not
  a numerical device.)
- Additive has a second reason (tycho-sqp): with `max`, whenever
  `rho_s > trial` the trial is a no-op and IC's memory would record a
  `rho_d_last` that was never applied — the /3 rule would descend from a
  fiction. Additive keeps "`rho_d = 0` means unmodified" exact. Expected and
  counted, not a bug: every scheduled decrease of `rho_s` by Δ on a row whose
  curvature sits at the boundary demands `rho_d += Δ`, so reclimbs correlate
  with `reg_decreases` on nonconvex rows. Registered refinement (with the
  sticky trial, not built): trial = `rho_d_last + Δ` after a decrease.

### 2.3 Gate order: unchanged, and why

Leaving the gated decrease before the factorization is correct once 2.1 and
2.2 hold: the decrease is a statement about the proximal subproblem; the IC
trial absorbs the consequence at a bounded cost (one climb, then the /3 band).
Moving the read before the decrease would couple the two mechanisms again.

### 2.4 Counters (§7 amendment)

- `ipqp_rho_flaps` is RENAMED `ipqp_ladder_reclimbs` and counts iterations
  whose IC trial (`rho_dem_last/3`) was rejected and had to re-escalate — the
  cost signal §2.2 item 3 wanted "flaps" to be. No consumer exists (the field
  is M6-new, not in any CSV schema until T9), so the rename is free now and
  would not be in W5.
- The T4 identity `prox_center_updates == reg_decreases + rho_flaps` loses its
  class (a) (no monotone refusal exists) and becomes
  `prox_center_updates == reg_decreases + class_c` — re-pinned.
- `ipqp_rho_demanded_max/_last`, `ipqp_iters_at_elevated_rho` (`rho_dem > 0`),
  `ipqp_inertia_retries`, `ipqp_reg_increases`: semantics unchanged.
- `IpqpResult` gains `rho_mod` (final `rho_dem`) beside `rho`; T8's `ipqp.reg`
  event carries both.
- NEW counter `ipqp_iters_ladder_armed_no_advance` (iterations with
  `rho_d > 0` and no gate advance) — the direct instrument for the
  negative-curvature-walk gap; T9 reads it.
- No new `IpqpOptions` in W1: the four IC constants are `detail::k*` with the
  paper's values; exposing them is an M7 tuning item (registered).

### 2.5 Spec text touched (plan §7 note (p), folded at W1 close)

§2.2 item 2 (IC rule, additive, diagonal-only modification); §2.2 item 3
(DELETED, replaced by the IC memory rule + reclimb counter); §3.2 bullet 3
(DELETED); §7 counter row (rename); §3.1 Newton system (`rho_sched + rho_dem`
on the primal diagonal, `rho_sched (x - zeta)` on the RHS).

## 3. Close gate for T4b (all required)

1. **A11 HS pin flips 0 → 3**: all three HS rows converge, reach the §2.2
   item 4 read, and report an honest certificate (stands, or kIndefinite on a
   genuine saddle); no row escapes on budget. Exact iteration and
   factorization counts pinned per row (mutation non-vacuity as always).
2. Every T4/T5 fixture re-derived under the new semantics: saddle
   `diag(2,−1)` and `H = [−1]` still downgrade; cap-1 budget fixture; the
   stall fixture; the identity fixtures; seam pins.
3. Default-mode (`kWalk`) both-arm 27-cell replay: 0 diffs (trivially — the
   engine is not on that path; still run, per the plan's rule).
4. **kIpm-arm 27-cell run, BEFORE vs AFTER T4b, bit-identical on all 36
   non-wall columns** (T6 adds the corpus flag; this is the real inertness
   proof: convex cells never arm the ladder, so 2.1 + 2.2 must change nothing
   there — any diff is a bug in the separation). PLUS (Codex): a direct
   byte comparison of the assembled KKT matrix values and of the predictor
   and corrector RHS vectors, before vs after, on two convex fixtures with
   Ruiz ON and OFF, at `rho_dem == 0` — the corpus columns are the
   consequence, these are the cause.
5. Informational: factorizations/iteration and reclimbs on the three HS rows
   and the parametric saddle family, before/after — the numbers T9 will
   measure against.
6. Seam byte-identity re-check; clang-format; Debug + Release; Claude + Codex
   review as every task.
7. (tycho-sqp A, scoped by Codex) SETTLED-BAND pin: on a deliberately
   equality-only, no-active-bound indefinite fixture the analytic reduced
   Hessian (H projected onto null(A)) gives the threshold `θ = |λmin_red| -
   rho_sched`; pin the SETTLED `rho_d` inside `(θ, 8θ]` — "flips 0 → 3" is
   compatible with settling at 40. On the three HS rows (bounds active,
   `Sigma`, finite `delta_sched`) the analytic band is NOT valid; pin the
   observed threshold from the assembled Schur complement informationally
   and assert only `rho_d_settled <= 8 × the smallest sufficient value seen`.
   Note
   the target `(n+mi, me+mi, 0)` ⇔ `G + A'A/δ_sched > 0`, which converges to
   "G PD on null(A)" only as `δ_sched → 0`; early in a solve the gate demands
   `rho_d` for curvature the equality rows eliminate later — over-shift that
   shrinks with the schedule (reclimbs trend DOWN over a solve). Expected.
8. (tycho-sqp B) WEAK-ACTIVE-INDEFINITE fixture (the R5 precondition): an
   indefinite direction whose binding bound is WEAKLY active (`x - l ≈ √μ`,
   `z ≈ √μ`, `Sigma ≈ 1` — neither masking nor exposing). At the final read
   this is where "`H + Sigma` at the point" is ambiguous and the certificate
   must DOWNGRADE rather than stand; A11 asserts it.
9. (tycho-sqp C, restated on the actual system per Codex) EXECUTABLE
   INVARIANT for mechanism 4: at any accepted step whose full primal-dual
   increment is negligible (`||(dx, ds, dy, dz)||inf < 1e-12`) the §3.2
   regularized KKT residual (all blocks) is `<=` the stopping tolerance — the
   RHS the step was built from IS that residual under 2.1; a mutation
   re-introducing the total `rho` in `build_rhs` must fail it. Also pin that the final read runs at `rho_d = 0`,
   `rho_s = reg_floor` by construction (a mutation seeding it from
   `rho_d_last` must fail).
10. (tycho-sqp 1b, sharpened on second look) LONG NEGATIVE-CURVATURE WALK
   fixture — ADVERSARIAL by construction or it licenses nothing: the
   additional-shift threshold small (`θ = |λmin_red| − rho_sched ≈ 0.05–0.1`),
   the binding bound ≈ 100 from the start, so the armed walk is ≥ 10
   iterations with ZERO gate advances; pinned to converge INSIDE the budget
   (the residual risk on a long walk is the budget, not §6.2, whose
   conjunct (iii) cannot hold on full steps), with the iteration count pinned
   against the cap, `ipqp_iters_ladder_armed_no_advance` exact, and — only if
   §6.2 fires on it — the φ-decrease window reset of 2.1(a). Without it the three HS rows
   (walks of 2–4 iterations) cannot distinguish "the gap is benign" from
   "the gap is untested".

## 4. Alternatives considered and rejected

- Keep monotone, shrink the first rung: the overshoot shrinks, mechanism 4 is
  untouched — the freeze persists at whatever floor is recorded.
- Fix 2.1 alone (rho consistency), keep monotone: unfreezes, but the shift
  stays at 80 for the solve: Newton becomes gradient descent with step
  ∝ 1/80 — the crawl §S3 correctly said was NOT happening today would then
  happen.
- Skip the scheduled decrease while the ladder is armed: AS A STANDALONE
  patch it hides mechanism 1 and leaves 2–4. In the separated design (Codex)
  it becomes a cost refinement — each scheduled decrease by Δ forces
  `rho_d += Δ` on a boundary-curvature row — equivalent to the registered
  "trial = rho_d_last + Δ after a decrease"; measured via
  `ipqp_ladder_reclimbs` vs `ipqp_reg_decreases` at T9, decided there.
- Estimate λmin (Lanczos on H) and set the shift directly: precise, new
  machinery, M7 candidate; not for a W1 fix.
- Trust-region on Δ instead of a shift: a different algorithm than the spec's.

## 5. Sequencing, size, risk

- T6 (in flight, routing) → **T4b** → T7 → T8 → T9 → T10. T4b does not
  depend on T6's code but must not share the tree with it (§8).
- Size M: one TU (`ipqp_engine.cpp` loop + ladder lambda), header constants
  and `IpqpResult`, `solver_counters.h` rename, fixtures, spec note. Opus
  implementer; Claude + Codex reviews.
- Risk 1: IC's /3 band costs ≈ 0.5 factorizations/iteration on nonconvex
  rows — measured at gate 5; the sticky-trial refinement is the lever if T9
  says so. Risk 2: additive vs `max` moves the effective shift when both are
  large — only when `rho_dem > 0`, i.e. never on convex cells (gate 4 catches
  any leak). Risk 3: a stationary point of a nonconvex QP with bounds may be a
  local minimizer on a bound face — the final read reads `H + Sigma` at the
  point, which is the correct certificate for a local minimizer; A11 asserts
  honesty of the certificate, not global optimality.

## 6. Review record

- tycho-sqp lane (2026-08-31): GO with five amendments, all folded above —
  descent theorem + residual-non-monotonicity gap (2.1, gate 10, the φ-reset,
  the new counter); IC verbatim with x100 through the first climb (2.2);
  scaled-space placement of `rho_d` (2.2); warm-restart carry split by what
  changed (2.2); close-gate 7–9. Concurs on gate order, additive, counters
  rename, all five rejected alternatives; Lanczos λmin is the M7 candidate.
  Second look (same day): no objection to Codex's two scopings; the
  long-walk fixture must be adversarial (gate 10 sharpened); paper constants
  over the NLP driver's, spec sentence corrected, M7 measurement registered.
  "Take v3 to Grant."
- Codex (2026-08-31): SOUND WITH CHANGES — five required, all folded:
  "verbatim" withdrawn (IC-1 tries zero every iteration; 1e-20/1e40 bounds)
  → declared adaptations; trajectory and band recomputed from the actual
  threshold θ; unscaled-additive vs scaled-space contradiction resolved
  (single definition in 2.1, cap on the scaled value); the descent identity
  scoped as motivation, gate 9 restated on the full KKT increment;
  evidence-failure memory defined. Also: gate 4 gains direct matrix/RHS byte
  checks; gate 7's analytic band restricted to an equality-only fixture;
  "skip decrease while armed" re-evaluated as a T9 refinement. Mechanism 4
  derivation confirmed (`R* = 0.99 S0` at the frozen point; the gate's second
  clause needs `R* <= 1e-7`, which it is not).
