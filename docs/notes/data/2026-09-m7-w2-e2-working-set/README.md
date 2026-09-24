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

## Fix round 1 — D1, D2, D3 of the E2-D1 diagnosis (F1–F3)

The E2 TRAIN re-take converted 0 of 10 predicted cells and broke READING6. The diagnosis is
`docs/notes/data/2026-09-m7-w2-e2-d1-diagnosis/REPORT.md`. This round lands its fixes F1, F2 and
F3, with the default `independent_working_set` still ON (settler ruling there): the gates and the
re-take decide it. F4 (driver routing, D4) waits on the E2-F2 restoration diagnosis; F5 (D5) and
F6 are separate tasks. Lines are at the fix commit.

| Defect | Fix | Where | Fixture |
|---|---|---|---|
| D1: N1 tried the certificate only under cond1; a locked positive-coefficient member leaves cond1 false on an infeasible QP, so N3 fired | F1: when no member is admissible, `verify_farkas` is ALWAYS tried; verified ⇒ `kInfeasible`; a cond1 candidate that fails is still `kRejected`; else N2/N3 | `src/qp/qp_engine.cpp:1487` | `QpE2WorkingSet.ALockedPositiveMemberStillGetsTheVerifiedCertificate` |
| D2: on the eliminated path the detector's support listed rows only; a dependency an optional pin creates was attributed to mandatory equalities | F2: the pins' coefficients (`A_Wᵀŷ` at the pinned columns) join the support at the same `kDepCoeffTol` threshold | `src/qp/ws_independence.cpp:365` | `WsIndependence/*.AnOptionalPinInADependentEqualityChainIsRemoved`, `WsIndependenceDetector.TheEliminatedSupportNamesThePinsTheDependencyUses` |
| D3: an exactly dependent pin tests UNDECIDABLE on the border path, S17 admits it, the re-check removes it, it re-blocks at α = 0, to the zero-motion cap | F3(a): an undecidable test on a border-stack pass takes the border retreat (once per solve, `dep_border_retreats`) before S17 may admit it | `src/qp/qp_engine.cpp:1436`, `:1535` | `QpE2WorkingSet.TheUndecidableBorderPingPongRetreatsOnceAndEndsOptimal` |
| | F3(c): an undecidable blocker that completes a member row on pinned columns (or a row on pinned columns only) is decided EXACTLY, coefficients read off the row, and feeds `select_exchange` / `verify_farkas` (`dep_structural_exact`) | `src/qp/qp_engine.cpp:1443`; `detail::structural_dependency`, `src/qp/ws_independence.cpp:403` | `QpE2WorkingSet.AFullyPinnedRowDecidesAnUndecidablePinExactly` |
| | F3(b): a re-check removal of a deferred add marks it known-dependent at that point (`kPtKnownDependent`, cleared when the iterate moves); blocking again at zero motion on an undecided test it is refused (`dep_known_dependent_refusals`), N2 or N3 | `src/qp/qp_engine.cpp:838`, `:1455` | none (see below) |

Order at the add site for a blocker whose test is not a decided dependent: (a) retreat if the pass
is on the border stack; else (c) the structural dependency if one exists; else (b) refuse if known
dependent at this zero-motion point; else defer (S17). (a) runs before (c) deliberately: in a
20 000-solve sweep (below) the reverse order turned one optimal border solve into N3 and changed
nothing else.

**Design amendment, §2.4 (N1).** The design gated the certificate on cond1 (every
sign-constrained member's scaled coefficient ≤ τ·max) and a structural violation. The verified
certificate does not need either: `verify_farkas` puts every positive-coefficient row into the
explicit box part (y_i = min(v, 0)) and checks the implied bound against the box, so a certificate
that verifies is a proof whatever the coefficients' signs. N1 now reads: with no admissible member,
build and verify the certificate; verified ⇒ `kInfeasible`. The design's candidate (cond1 and a
structural violation) that fails verification still ends `kRejected`; anything else goes to N2
and N3. The gap was in the design, not the implementation: a locked member (entered at this
point) with a positive coefficient is not admissible, yet it makes cond1 false.

**Counters.** Two new `QpCounters` fields: `dep_structural_exact` and
`dep_known_dependent_refusals`. The comments on `dep_recheck_removals`,
`dep_infeasible_certificates`, `dep_certificate_rejections`, `dep_deferred_adds` and
`dep_border_retreats` now say what F1–F3 changed.

**What the fixtures prove, and what the base did** (each fixture's QP was also run on a scratch
Debug build of the base `6d123e75`):
- F1: seed (−1, −1, 0) on −x0 + x1 + x2 = −3 with x2 fixed: x0's pin enters and is locked, x1's
  pin blocks at zero motion with a positive coefficient on x0. Base: N3 (`kNumericalError`;
  border: a retreat, then N3). Now: a verified `kInfeasible` in 2 minors in both algebras, the
  certificate re-derived from the data (implied 2 > limit 1).
- F2: the chain x0 − x1 = 0, x1 − x2 = 0, x0 fixed, x2's lower pin seeded. Base refactorize:
  `mandatory_dependent_entries` 1, nothing removed (border: the pin removed, as always). Now:
  the pin is removed in both algebras, `kOptimal` at (0, 0, 0, 1), multipliers O(1).
- F3(a): a 5-variable QP from the sweep whose border pass stalls at ρ = 1.2e-6 on an exactly
  dependent pin. Base border: 9 deferred adds, 9 re-check removals, the zero-motion cap. Now: one
  retreat, one exchange, `kOptimal` at the refactorize path's point.
- F3(c): a READING6-shaped QP (x0 − x2 = 0.5, x0 fixed, x2 ≥ 0) whose first border pass holds no
  usable factor. Base border: deferred, re-checked, retreated, N3 in 6 minors. Now: the exact
  structural dependency and a verified `kInfeasible` in 1 minor.

**Not covered by a fixture: F3(b).** No constructed or sampled QP reached a refusal: after a
re-check pass the factor is usable, so the re-blocking member's test is almost always decided,
and (a) and (c) now take the cases that looped. `dep_known_dependent_refusals` read 0 on every
sweep solve. It stays as the bound on the one path left (a stalled refinement on the eliminated
path, twice at one point).

**The exploratory sweep (scratch only, not a gate).** 24 000 random small QPs (n 3–6, 1–4 rows,
1–3 equalities, ±1 two- and three-term rows, PSD rank-one or indefinite H, optionally a fixed
variable), both algebras, base vs this round, Debug:
| Base (6d123e75) → this round | Solves |
|---|---|
| `kNumericalError` → `kInfeasible`, every one a VERIFIED certificate | 755 |
| `kNumericalError` → `kOptimal` | 14 |
| `kInfeasible` (the walk's end classifier, not a certificate) → `kOptimal` | 20 |
| `kMaxIter` (1/μ multipliers) → `kNumericalError` | 1 |
| `kInfeasible` → `kNumericalError` | 7 |
| `kOptimal` → `kNumericalError` | 8 |
| zero-motion cap hits | 22 → 0 |

Every one of the 34 new `kOptimal` answers passes an independent first-order check (equalities
included) at ≤ 6.5e-9, most at 1e-16: the 20 base `kInfeasible` answers there were FALSE.
The 47 950 solves ran in 24 000 trials × 2 algebras, less the asserting ones below.
The Debug "over-count ⇒ found" assert fires on the same 34 random QPs in base and here
(pre-existing; the swamped-C class R6 already names). The 16 regressions end `kNumericalError`,
never a wrong verdict. The ones traced are F2 path changes: the eliminated path's re-check now
removes the newest member of the true support, a pin, as the border path always did, and on six
of them the refactorize walk now ends in the N3 the base's border walk already reached on the
same QP. A pin removed
by an over-count re-check can re-block at zero motion, dependent, with its positive-coefficient
partner locked and no verifying certificate: that is N3 in both algebras. It is a candidate
finding for the next round, not a defect of F2's attribution.

**Re-pins: none.** No counter, status or answer assertion moved in either configuration; the
E2-pinned fixtures (HsBattery, HsSweepCorpus, ScaleF7, WarmStartBattery, Continuation, the SSN
restoration fixture) read their E2 values. F1–F3 act only on the no-admissible exit and on
undecidable tests, which none of those walks reaches.

**Suites (this tree, MKL, clang++, `ctest -j4` under `setsid`, the box lock per leg).**
- Debug: 2813/2813 without `Continuation.ProbeBudgetBoundsAFailingProposal`, which passed alone
  (1/1).
- Release: 2814/2814.
- By name against E1b fix1's 2807/2808: six new instances, none removed —
  `QpE2WorkingSet.{ALockedPositiveMemberStillGetsTheVerifiedCertificate,
  TheUndecidableBorderPingPongRetreatsOnceAndEndsOptimal,
  AFullyPinnedRowDecidesAnUndecidablePinExactly}`,
  `BothAlgebras/WsIndependence.AnOptionalPinInADependentEqualityChainIsRemoved/{0,1}` and
  `WsIndependenceDetector.TheEliminatedSupportNamesThePinsTheDependencyUses`.
- clang-format clean on every changed file.

Fix round 1 was not gated on its own: its offline G4 found the two f7 walk movers below before any
declaration. The gates for fix rounds 1 and 1b together (range `6d123e75..` the fix1b commit) are
declared in `wrappers-gates-fix1/` (see the derivation file there).

## Fix round 1b — the removal certificate (E2-D2)

The offline G4 at b25ab7ed moved two U0 walk cells, `f7_n800_path_warm` Optimal → NumericalError
(factorizations 2 → 1115, minors 6 → 1627) and `f7_n1000_path_warm` (2 → 9984, 12 → 10496), F2
alone. The trace is `docs/notes/data/2026-09-m7-w2-e2-d2-f7-regression/` (REPORT.md). In short: in
the cell's setup solve a converged re-check null vector ŷ (residual 9.3e-7) on a working set that
already carried seven exact dependencies (inherited from the warm seed; the entry detector stalled
at 1.9e-6, the F6 class) listed the deferred pin P3768 at 2e-4 of the largest coefficient.
Newest-first removed it; ŷ proves it dependent only to ρ ≈ 1.4e-2. It re-blocked at α = 0 with
violation rate 2.75e-3 (the working set's own μλ leak: rows at 1.4e-4, |λ| ≈ 1.8e5), F3(b) refused
it, the pass-over failed, N3. The base (F2 off) removed the newest shifted row R756 (ρ ≈ 1.7e-5)
and reached kOptimal.

| Defect | Fix | Where | Fixture |
|---|---|---|---|
| D2′: F2 made a support member removable on a coefficient above kDepCoeffTol alone; ŷ need not prove that member dependent | F1b: a removable member (a working row, a non-fixed pin) is offered for removal only when ŷ certifies it, ‖A_Wᵀŷ‖ ≤ kDepRemovalTol·\|ŷ_k\|‖a_k‖ (kDepRemovalTol = kDepUndecidable: the candidate test's own independent boundary); with none certified, the best-certified member alone (D2's point stands: a dependency is never attributed to mandatory rows only). Counters `dep_uncertified_trims`, `dep_certified_fallbacks` | `src/qp/ws_independence.cpp` (`detect_dependency`), `include/hven/detail/qp/ws_independence.h` | `WsIndependenceDetector.{APinInTheSupportByASmallCoefficientIsNotOfferedForRemoval, WithNoRemovableMemberCertifiedTheBestCertifiedOneIsOfferedAlone}`, `BothAlgebras/WsIndependence.AnUncertifiedPinIsKeptAndACertifiedRowRemoved/{0,1}` |

On an exact dependency ‖A_Wᵀŷ‖ is at rounding level and every support member is certified: the rule
is a no-op there (the fix1 random sweep: every solve identical in status to b25ab7ed but one, below).

**Candidates rejected** (the trace's table): (A) pins only when the rows-only support has nothing
removable — fixes f7, gives back the 20 false-kInfeasible → kOptimal gains; (B) the rows' threshold
on the rows' own maximum — no effect (the pins did not raise the maximum); (C) prefer a row over a pin
— no effect on n800 (the rows were shift-driven and sort after pins), n1000 ran out of budget; (D)
skip a known-dependent zero-rate re-blocker, clear a stale mark on a nonzero rate — its premise fails
on a leaking working set (rate 2.75e-3 on a rank-dependent pin), the stale branch re-admits it and
the walk cycles at zero motion (unbounded; stopped at 20 min).

**The sweep (scratch, 24 000 trials × 2 algebras) against b25ab7ed:** one status change,
`t4922` family 1 border: verified kInfeasible → kNumericalError (the pin-re-block N3 class fix 1
registered; the same QP's refactorize path already ended N3 at b25ab7ed). The 755 + 14 + 20 gains
are all kept.

**F3(b)** still has no fixture: with the certificate the f7 cells no longer reach it.

**Suites (this tree, MKL, clang++, `ctest -j4` under `setsid`, the box lock per leg).** Debug
2817/2817 without the long test, the long test 1/1; Release 2818/2818. By name against fix 1's
2813/2814: four new instances (the three fixtures above, one parameterised over both algebras),
none removed. No counter, status or answer assertion moved: no re-pins.

**Offline G4 at the fix** (`build/` Release `hven_sqp_corpus`, the U0 replay cells, three engines,
`compare_replay.py --residual-gate 1e-5` against the M6 close replay): walk 0 differences
(7 within the gate, residual-class last digits on the two f7 cells: their M6 values, Optimal,
factorizations 2, minors 6/12), ssn 0, ipm 0.

## Fix round 2 — the Codex full review (S23, S24, S25 + S25′)

The settler's adjudication of `.superpowers/m7-w2-e2-review-sol.md` items 1–3 (S23–S25) and the
amendment S25′ after the first fix-2 implementer's STOP (S25 as ruled turned four truly infeasible
fixtures into kNumericalError: the certificate's multipliers come from the `dual_mu`-regularized
solve and are O(μ) off, so on free coordinates the residual was never exactly 0).

| Ruling | What landed | Where | Fixtures |
|---|---|---|---|
| S23 — the dependency-test contract as landed (replaces R2) | ρ = ‖(a − A_Wᵀv)_free‖/‖a‖, scale-normalized, NO row equilibration (it cannot be applied to the retained factor); converged ρ ≤ kDepTol or the screen ⇒ dependent; a stall with ρ < kDepUndecidable ⇒ undecidable (border retreat / eliminated defer); a stall with ρ ≥ kDepUndecidable ⇒ admitted as independent AND counted (`DepTest::stall_admit`, new counter `dep_stall_admits`); converged kDepTol < ρ < kDepUndecidable ⇒ independent | `include/hven/detail/qp/ws_independence.h:13` (contract block), `src/qp/ws_independence.cpp:273` (the flag), `src/qp/qp_engine.cpp:3072` (the count), `include/hven/core/solver_counters.h:273` | `WsIndependenceCalibration.{RefinedStatisticSeparatesAtARowRatioOf1e10BothAlgebras, AStalledRefinementAboveTheUndecidableBandIsAdmittedAndFlagged}`, `BothAlgebras/WsIndependence.AStalledTestAboveTheUndecidableBandIsAdmittedAndCounted/{0,1}` |
| S24 — the removal certificate is SUFFICIENT | comments corrected: ‖A_Wᵀŷ‖/\|ŷ_k\| is an UPPER BOUND on dist(a_k, span of the others), so the ratio is a sufficient numerical certificate; kDepRemovalTol = 1e-3 is a numerical-rank POLICY, not the candidate classifier's boundary; the no-certified-member fallback is HEURISTIC and kept; `best_certified_fallback` → `uncertified_fallback`, counter `dep_certified_fallbacks` → `dep_uncertified_fallbacks` | `include/hven/detail/qp/ws_independence.h:73-79`, `src/qp/ws_independence.cpp:409`, `:463`, `src/qp/qp_engine.cpp:808`, `include/hven/core/solver_counters.h:269` | `WsIndependenceCalibration.AConvergedRhoBetweenKDepTolAndKDepUndecidableIsIndependent`, `BothAlgebras/WsIndependence.TheUncertifiedFallbackStillEndsAtAFirstOrderPoint/{0,1}` (status AND a recomputed KKT residual) |
| S25 — the toleranced certificate is not a proof | a verified certificate whose residual (dropped box_part entries on coordinates unbounded on the needed side) is above the rounding floor ends kNumericalError at the current iterate (after the border retreat on a border pass), the certificate kept in `QpSolution::farkas`, counted `dep_certificate_toleranced` (`AddExit::kToleranced`) | `src/qp/qp_engine.cpp:1512`, `:1555`, `:1577-1580`, `include/hven/detail/qp/qp_problem.h:100-133`, `include/hven/core/solver_counters.h:237` | `QpE2WorkingSet.ATolerancedCertificateOnAnUnboundedCoordinateEndsNumericalErrorInsideTheBox` (original vs coefficient-perturbed problem checked independently; now also the refined y within the floor of −1) |
| S25′ — refine μ-free, then a rounding floor | `verify_farkas` refines the certificate's multipliers μ-free before verification: `detail::certificate_coefficients` is the dependency test with its ≤ 3 refinement steps run to their end (target 0: until a step stops lowering the μ-free residual); on a single-row dependency the row's EXACT coefficient (c's least-squares fit on it over the free columns) replaces the refined one when it lowers the residual. EXACT iff residual ≤ `exact_tol` = kCertExactKappa·(n + m)·ε·‖y‖₁·max_k‖a_k‖ (κ = 10, m = me + \|W\|, ‖y‖₁ with c's unit coefficient; standard-model derivation in the header). Tests with ρ == 0 (structural, no free column) are exact and not refined. The refinement's solves are added to `dep_test_solves` | `include/hven/detail/qp/qp_engine.h:478-488` (κ, derivation), `src/qp/qp_engine.cpp:3305` (`verify_farkas`), `:3356` (single-row), `:3427` (floor), `src/qp/ws_independence.cpp:175`, `:285`, `include/hven/detail/qp/ws_independence.h:123`, `include/hven/detail/qp/qp_problem.h:112`, `:133` (`exact_tol`) | `QpE2WorkingSet.AnExactlyDependentBlockerOnFreeVariablesIsCertifiedInfeasibleAfterRefinement` (a two-row dependency on free variables: the refinement alone decides) |

**S25′ measured (Debug, temporary stderr probe, removed before the commit; the residual is the
norm of box_part on coordinates unbounded on the needed side):**

| Fixture (QP) | unrefined | refinement only (≤ 3 steps) | as landed (+ single-row exact) | floor `exact_tol` | status |
|---|---|---|---|---|---|
| `objective_inflated_lambda_qp(1e-2, 3e-4)` (ObjectiveInflated…, T6b… both algebras) | 3.5e-11 | 2.5e-18 | 0 | 1.9e-16 | kInfeasible |
| `objective_inflated_lambda_qp(3e-3, 3e-4)` (same two tests) | 1.3e-9 | 4.0e-16 (y_e = 1 − 9.5e-14: the three steps ran out, contraction ~ μ‖H‖/‖a‖² on a 4e-3 row) | 0 (y_e = 1) | 5.7e-17 | kInfeasible |
| SsnEngineLocal walk cross-checks (`contradictory_qp`) | 3.9e-9 | 0 | 0 | 3.4e-14 | kInfeasible |
| the second shape in `InfeasibilityIsReachedByBothRoutesAndOnBothShapes` | 0 | 0 | 0 | 4.3e-13 | kInfeasible |
| the new two-row fixture | 8.2e-9 | 0 | 0 (single-row not applicable) | 8.2e-14 | kInfeasible |
| the toleranced fixture (e = 2⁻²¹ on the free x1) | 4.8e-7 | 4.8e-7 | 4.8e-7 = e | 1.3e-14 | kNumericalError |

The refinement alone does NOT reach the floor on the a = 3e-3 instance (4.0e-16 > 5.7e-17): the
single-row exact coefficient is what returns it to kInfeasible. S25′ names both routes; both landed.

**The four correctness fixtures are back at kInfeasible and were NOT re-pinned:**
`QpEngine.ObjectiveInflatedMultiplierHidesASmallContradiction`,
`QpEngine.T6bObjectiveInflatedMultiplierBehavesTheSameInBothAlgebras`,
`SsnEngineLocal.InfeasibleQpIsSuspectedNotCertified`,
`SsnEngineLocal.InfeasibilityIsReachedByBothRoutesAndOnBothShapes`.

**The 1e10 stall-admit finding (S25′, recorded as expected).** On the 1e10 row-ratio calibration the
independent candidate e₀ is admitted through the stall branch (the 1e10 row puts the residual's
rounding floor above the unscaled target), so it would count `dep_stall_admits`; the verdict and ρ
are exact. Expected under the landed rule and the counter, not a defect; the flag is not pinned (a
backend's rounding decides it). Re-take 3 reads the counter on real cells.

**Suites (this tree, MKL, clang++, `ctest -j4` under `setsid`, the box lock per leg).** Debug
2826/2826 without the long test, the long test 1/1 (alone, detached); Release 2827/2827. By name
against the E1b fix-round-3 gate transcripts (2817/2818): nine new instances — the eight above from
S23/S24/S25 (three `WsIndependenceCalibration`, two parameterised over both algebras, the toleranced
fixture) and the S25′ fixture — none removed. The HS corpus and the options-dump pins are inside the
suites and did not move.

**Re-pins: none.** No counter, status or answer assertion outside the new fixtures moved; the one
edit to a round-2 fixture (the toleranced one: y within the floor of −1 instead of within 1e-6)
tightens its own assertion.

**Schema note (the harness follows at re-take 3).** Counter RENAMED: `dep_certified_fallbacks` →
`dep_uncertified_fallbacks` (same event, the name corrected; the harness column
`harness_qpsum_dep_certified_fallbacks` follows). NEW: `dep_stall_admits` (S23),
`dep_certificate_toleranced` (S25). `dep_test_solves` now also counts the certificate's refinement
solves (S25′). `QpFarkasCertificate` gains `exact_tol`.

**Process.** The first fix-2 implementer's one direct test-binary run outside the box lock (~08:37,
~10 ms, a Debug unit-test filter) is recorded here. Its STOP is recorded in the STATE note.
