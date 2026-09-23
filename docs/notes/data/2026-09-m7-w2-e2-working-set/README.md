# M7 W2 E2 — an independent working set with a multiplier-aware exchange (Task B, steps 1–2)

Design: `.superpowers/m7-w2-e2-design.md` (settler-cleared R1–R9; rulings S1–S14 in the brief).
The only paper cited is Gill & Wong 2015 (Results 3.3–3.5, §6.2). Step 1 is `1b6aa8e1`; this
note covers step 2. No new factorization kind; no Eigen sparse factorization.

## What step 2 builds

- **The dependency test at the add site.** Each ratio-test blocker is tested on the factor the
  pass already holds (`K_mu [u; v] = [a; 0]`, rho <= 1e-6, or the integer screen
  `me + |W| + 1 > n`).
- **The exchange** (Result 3.4(b)). A dependent blocker r is swapped for the member
  `j = argmin m_k+ / v_k` over the admissible members. The step is taken, j is removed and r
  added. The multipliers become `m - sigma v`, with `sigma = m_j+ / v_j` on r. Counters:
  `ws_exchanges`, `ws_exchanges_bound`, and `ws_drops` for the release, which keeps the net
  identity.
- **No admissible member.** (N1) A Farkas certificate is built and VERIFIED explicitly: the
  implied lower bound, box included, must clear the blocker's limit by the structural margin. It
  is stored in `QpSolution::farkas` and the solve ends `kInfeasible`. (N2) Otherwise the blocker
  is passed over, if its predicted violation stays within `feas_tol * row_scale`. (N3) Otherwise
  the solve ends `kNumericalError` at the current iterate, which is never a step past the
  blocker.
- **The zero-motion re-entry rule.** It works through point memory, drop exemption and the cap
  `2(n + mi)`.
- **The S3 crutch is removed.** The Debug assert "over-count ⇒ the detector found it" is back.

## Deviations from the cleared design (for the settler)

1. **The border retreat (S11, accepted).** It fires at most once per solve. When N3 or a
   rejected certificate would end a border-stack pass, the pass is redone on the bound-eliminated
   path. It is counted in `dep_border_retreats` and traced as `kEliminatedRetreat`. See E2-F1.
2. **The entered-at-point lock applies only to a step that does not move.** The design locks
   members that entered at the current point, to break exchange ping-pong at a fixed point.
   Implementer 4's build applied the lock even when the step to the blocker moves, but that move
   clears the point memory before the exchange mutates. The lock now holds only when
   `alpha*|p|inf <= step_tol`. Without this, `QpEngine.DropRuleRunsBeforeInfeasibilityIsDeclared`, HS10
   and 28 of the 53 failing fixtures ended in N3.
3. **An undecidable blocker is deferred, not terminated.** The case is a screen-negative test
   with no usable factor view or a stalled refinement. It carries no coefficients, so no
   exchange can be picked. Treating it as dependent sent `indefinite_equality_qp` (S2's named
   correctness fixture) to N3. It is now admitted, and `recheck_pending` makes the next pass's
   detector test the grown set on its own factor, as for a shift add. Counter:
   `dep_deferred_adds`.
4. **A shifted row is exchangeable as a last resort.** HS22's QPs have both rows shifted in a
   square W, and the blocker's only positive coefficient sits on a shifted row. That is N3 by the
   design, a regression from `kOptimal`. A shifted row is now admissible only when no other
   member is. With `v_j > 0` its value on the new face is `d_j − g/v_j <= d_j`, so the face
   satisfies it. It is marked shift-refused so the homotopy does not re-admit it. Counter:
   `ws_exchanges_shifted`.
5. **Certificate verification tolerates the dependency residual on unbounded coordinates.**
   A `box_part` entry of at most `kDepTol·‖c‖` on a coordinate unbounded on the needed side is
   dropped, and its norm is kept in `QpFarkasCertificate::residual`. Without this, every
   certificate on a free-variable QP failed (the implied bound was −inf), for example the
   objective-inflated contradiction fixtures.

### Settler rulings on the deviations (S15–S20, 2026-09-23 12:10)

- **S15** (deviation 2) accepted. The header comment at the lock (`ExchangePick`,
  `qp_engine.h`) now says the lock applies only on a zero-motion step and why.
- **S16** (deviation 5) accepted as a TOLERANCED certificate. `verify_farkas` and
  `QpFarkasCertificate` now say that the kInfeasible verdict means "infeasible within
  `kDepTol·‖c‖` on the dropped coordinates", and they name `QpFarkasCertificate::residual`. The
  certificate is copied into `QpSolution::farkas` only on kInfeasible.
- **S17** (deviation 3) accepted. A deferred add that the next pass's detector finds dependent is
  removed and counted in `dep_recheck_removals`. If the detector does not resolve an over-count,
  it lands in `dep_untested_growth` instead. `dep_deferred_adds` is never decremented. This is
  documented at the counter and at the admission site.
- **S18** (deviation 4) accepted. Shifted rows rank LAST, and `select_exchange` Debug-asserts
  that a shifted row is never chosen while an unshifted admissible member exists.
- **S19** accepted. An exchange's release counts in `ws_drops`, so the net identity holds.
- **S20** accepted. The declared movers below are re-pinned with attribution at each pin.

## Declared counter re-pins (S20)

All values are MKL Pardiso, clang++, and were observed in the configuration named. Every
Accelerate arm keeps its pre-E2 observed value and is marked UNOBSERVED under E2. The cause is
the same in every row: the add-site exchange swaps a dependent blocker for a member instead of
admitting it and paying for a later removal, and the zero-motion re-entry rule ends
release/re-entry churn at a fixed point. Both shorten the QP walks. No status or answer
assertion moved.

| Fixture | Counter | Old → new |
|---|---|---|
| `HsBattery.CrashBasisIsANullResult…` | minors off / on | 861 / 858 → 804 / 801 |
| | factorizations off / on | 320 / 323 → 267 / 270 |
| `HsSweepCorpus.Corpus` | SOC extra minors / factorizations | 23 / 4 → 22 / 3 |
| | total minors | 3725 → 3371 |
| `ScaleF7.WideWindowWalkIsChurnNotDegeneracy` N=30 | minors, adds, drops, adds_bound / drops_bound / distinct_bound | 295, 95, 97, 36 → 280, 96, 98, 44 |
| … N=50 | minors, adds, drops, adds_bound / drops_bound, distinct_bound | 635, 207, 210, 69, 69 → 526, 181, 184, 70, 68 |
| `SqpDriverSsnMode.RestorationStaysOnTheWalkUnderKSsn` | factorizations | 234 → 218 (ssn_iters 131 unchanged) |
| `Continuation.ProbeBudgetBoundsAFailingProposal` | arm A step-1 factorizations | 303 → 200 |
| | arm B step-1 factorizations | 14 → 11 |
| `WarmStartBattery.Corpus` F2far | minors / border_refine, cold and cold@pred | 158 / 159 → 148 / 149 |
| | minors / border_refine, warm and hot | 45 / 45 → 43 / 43 |
| | minors / border_refine, warm+pred | 42 / 42 → 40 / 40 |

Release observed the same value as Debug for every re-pinned counter, so no pin is config-split.

## Suites (this tree, MKL, clang++)

- **Full suites before the re-pins**, at Implementer 6's tree, run by `e2_suites.sh` under the box
  lock at `ctest -j4`:
  - Debug: 2784/2790 passed. All six failures are the counter pins above.
  - Release: 2781/2790 passed. The failures are the same six pins, which read the same values
    as in Debug, plus the three Release-only premises listed under S7/S13 above. No status or
    answer assertion failed in either configuration.
- **After the re-pins, targeted re-runs in both configurations**, after a rebuild that
  force-touched `qp_engine.cpp` and the edited headers. The set was the six re-pinned fixtures,
  `QpE2WorkingSet.*` (6), `QpEngineIndefinite.*` (34, including `EngineLandsOnALocalMinimizer`
  and the `indefinite_equality_qp` fixtures), the S7 on-fixture, and the three Release-only
  premise fixtures. Results:
  - Release: 49/49 at `-j4`, `Continuation.ProbeBudgetBoundsAFailingProposal` 1/1 alone, and
    the three premise fixtures 3/3 alone.
  - Debug: 49/49 at `-j4` and `Continuation.ProbeBudgetBoundsAFailingProposal` 1/1 alone.
  - The full suites were not re-run after the re-pins. The edits since then touch only these
    fixtures, comments, and one Debug assert.

## Finding E2-F1 (S11)

The border/Schur path loses accuracy when K0 is near-singular on the pinned directions. The
Schur complement carries O(1) rounding and the border EQP step leaves its face. Measured:
p = (0, 0, −9.3e-8) off-face on the period-2 QP, and row1·p = 1.6e-3 on the period-4 QP. The
dependency coefficients are also inaccurate: rho = 1.4e-3 and 0.34 on a square independent W
where the exact value is 0. The per-cell `dep_border_retreats` count in the TRAIN re-take
measures whether this bites on real cells.

## Fixtures (step 2)

`tests/qp/test_qp_anti_cycling_arm.cpp`, `QpE2WorkingSet.*`:
- the period-2 and period-4 QPs with the option ON, in both algebras: `kOptimal` at the
  brute-force optimum, `minors*5 < 200`, no cap and no termination; border retreats = 1,
  refactorize = 0;
- the exchange (a bound for a bound, on a moving step), ending at (1, 1);
- the verified certificate on a constructed infeasible QP with no admissible member, re-derived
  in the test from the problem data;
- the in-box termination (N3 at dual_mu = 1e-2): the iterate is inside the box with the rows
  satisfied;
- the border retreat: exactly one retreat record, then `kOptimal`.

S7's circle-line ON fixture:
`SqpDriverRestorationSeed.TheIndependentWorkingSetReachesTheSameCertifiedAnswer`.

## What each `independent_working_set = false` fixture now proves (S7, S13)

Each keeps the pre-E2 walk because its PREMISE belongs to that walk:
- `QpAntiCyclingArm` (`capped()`) and `QpDegeneracyArm` (`cycling_opts()`): the old walk cycles,
  and the retired arms act on that recurrence;
- `SqpDriverRestorationSeed.TheSeedIsNotGatedOnTheQpMode`, `.TheExhaustedLadderSiteSeedsFromItsOwnStep`,
  `.TheGuardAndThePhaseRunOnTheCallerScale` and the step-1 seed and proximity fixtures: a taken
  seed candidate on a knife-edge model (the ipm mode takes none with the option on);
- `SqpDriverRestoration.AlgebraModeDivergenceIsANonUniqueSubproblemOptimum`: the two algebras'
  different points on the face (with E2 both land on x0 + x1 = 2.25);
- `SqpDriverCertifiedFallback.T7TheT3ARouteIsREACHABLEAtTheDriverAndCostsOneExtraLadder`:
  rung B's walk certifying `kInfeasible` on the box-blocked relaxation;
- `SqpDriverQpFailure.RepeatedQpMaxIterStillPropagates`: "more than 3 minors at every radius";
- `QpEngineIndefinite.T7Fix2A…` (step 1): runs both settings;
- Release-only premises, found by the Release full suite and set off under the coordinator's
  ruling (the class Implementer 6 handled under S7/S13). Each still passes in Debug with the
  option on:
  - `SqpDriverRestorationSeed.TheTakenCandidateIsRecordedOnTheRequestingRow` and
    `SqpDriverRestorationProximity.TheSeedGuardFiresAndTheStartObjectiveIsHPlusP`. The premise
    is "one request, and it took a candidate", and `seeded_rows` reads 0 with the option on.
  - `SqpDriverElastic.EarlyExitOnADegenerateRelaxationStillCertifiesCorrectly`. The assertion
    that moved is the trajectory claim `on.major_iters < off.major_iters`, which reads 48 vs 44
    with the option on. Its status and certificate assertions (kInfeasible, certified, the
    nearest point to 1e-6) passed with the option on. The option-on certified answer on the
    circle-line model stays covered by
    `SqpDriverRestorationSeed.TheIndependentWorkingSetReachesTheSameCertifiedAnswer`.

Declared movers where the new answer is the honest one:
- `T6bTheBandsHIGHRungs`: a false `kOptimal` becomes `kNumericalError`;
- `T6bTheStiffWALKS`: a false `kInfeasible` on a consistent QP becomes `kNumericalError`;
- `T7TheFeasibleCensus`: `shut_slack_residue` goes from 11 to 6.

## Process notes

- **S9 PROCESS BREACH.** Implementer 3 ran `timeout 590 flock … setsid ctest`. The timeout
  killed the lock holder, and the ctest ran its last minutes UNLOCKED. The protocol is flock
  with NO timeout and NO fallback, never wrapped.
- **Budget overruns disclosed.** The implementers ran over the 200k retirement line: 233k and
  259k (implementers 2 and 3), then 250k (implementer 4). Implementer 5 handed off early at
  about 150k with the Debug suite red: 53/2783 failures, all traced here to deviations 2–5 above.
- **Implementers 5 and 6 overran the 180k hand-off line** (the settler's record in the brief,
  after S20; the "about 150k" above was Implementer 5's own estimate). Implementer 6 fixed the engine
  (deviations 2–5 and the ws_drops accounting), then handed off at the budget with six counter
  pins still red. Implementer 7 made the re-pins, applied S15–S20 and the Debug assert, and
  verified both configurations.
