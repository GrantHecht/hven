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


## Fix round 3 — the E2-D4 rules (R-A, R-C′; R-B and R-D deferred; S26–S30)

The E2-D4 read-out (`../2026-09-m7-w2-e2-d4-feasible-qps/REPORT.md`) ranked why the walk fails
feasible QPs and proposed three rules; the settler ruled them (brief S26–S30). Two land; two are
deferred to their own rounds with the evidence below as their briefs.

| Rule | What landed | Where | Fixtures |
|---|---|---|---|
| R-A — an exact structural dependency decides FIRST (E2-D4 class F1) | at every growth site's candidate test (`QpEngine::test_candidate`: the add site, the repair pins, the ride add) a numerical "independent" is replaced by `detail::structural_dependency` when one exists (a candidate row on pinned columns only, or a pin completing a member row); counter `dep_structural_first` | `src/qp/qp_engine.cpp:3105-3115`, `include/hven/detail/qp/ws_independence.h:47-54`, `include/hven/detail/qp/qp_engine.h:368-373`, `include/hven/core/solver_counters.h:280-283` | `QpE2WorkingSet.AStructuralDependencyDecidesFirstOnTheSteenbrbElasticQp` (STEENBRB's elastic QP, n 492: in the cell pin 407 was admitted at ρ 5.7e10 although it completes equality row 63 on pinned columns; the set leaked and the solve ended in zero-motion exhaustion; now kOptimal with the KKT conditions recomputed from the QP, both algebras; the `dep_structural_first ≥ 1` check is NDEBUG-only — Debug's arithmetic never makes that admission on this QP, accepted in S27) |
| R-C′ — the μ-free inertia re-read, confirmed at `dual_mu` (E2-D4 class B; S26) | a kWrong inertia verdict at the start repair or the terminal check is re-read on a bound-eliminated factor of the current working set with `dual_mu = 0` (a local `KktFactor`; the pass's factor and border stack untouched): kOk overturns the kWrong; kSuspect overturns it ONLY when a second eliminated read at the pass's own `dual_mu` reads kOk; anything else leaves the kWrong standing. Counters `dep_inertia_rereads`, `dep_inertia_reread_flips` | `src/qp/qp_engine.cpp:859-861`, `:1023-1025`, `:2923-2958` (`reread_wrong_inertia`), `include/hven/detail/qp/qp_engine.h:1394-1414`, `include/hven/core/solver_counters.h:284-290` | `QpE2WorkingSet.AMuFreeInertiaReReadCertifiesTheSingularPsdFaceOfAcopp57` (Z′HZ PSD-singular, λ_min +3.0e-12: kOptimal in ≤ 12 minors, f within 1e-2 relative of Clarabel's f*); `QpE2WorkingSet.AMuFreeSuspectReadDoesNotCertifyASaddle` (the correctness guard: a 4-variable saddle whose μ-free read is kSuspect; R-C as first ruled returned kOptimal AT the saddle, f = 0 vs −½; R-C′ reaches the minimum, f = −½, 0 flips) |
| S27 — the S3 Debug over-count assert downgraded | `assert(!over \|\| det.found)` removed: a stalled entry detector on an over-counted set is a legitimate, counted outcome (`dep_untested_growth`), not an invariant — it fired on the ACOPR57 dumps at the pre-rule walk | `src/qp/qp_engine.cpp:808-818` | the ACOPR57 fixture below runs in Debug (it was GTEST_SKIPped there) |
| R-B — the locked-vertex unlock (E2-D4 class A) | **DEFERRED** (S28): removed from the tree entirely — no code path, no counter | — | `QpE2WorkingSet.TheLockedVertexOnTheAcopr57PrimalQpsStillEndsN3` asserts the CURRENT status (kNumericalError, `dep_blocker_terminations` 1, both algebras; the E2-OFF walk at Clarabel's f*) and names the deferred rule |
| R-D — a stalled dependency test is undecidable whatever its ρ (S30) | **DEFERRED**: evaluated on the full instrument set, not landed (below) | — | — |

Dumps: `tests/qp/fixtures/e2-d4/` (STEENBRB_def-qp0, ACOPR57_def-qp0/qp1, ACOPP57_def-qp0 — byte-copies of
the E2-D4 corpus files — and `FORMAT.txt`), read through `tests/qp/support/qp_dump.h` (loader, replay,
KKT residuals against the effective box) by `hven_qp_walk_seam_tests` only
(`HVEN_QP_E2D4_FIXTURES`, `tests/CMakeLists.txt:414`). The period-2/4 anti-cycling fixtures also
assert ≤ 5 minors now.

### R-B: the trace, and why it is deferred (S28 (1)–(2)) — the deferred round's brief

R-B as first built (unlock, where the rule would otherwise END the solve, the members locked only by
having entered at this zero-motion point; min-ratio, lowest-index ties) converted ACOPR57 qp0/qp1 and
READING6 qp0/qp1 (the latter to a correct exact-certificate kInfeasible; HiGHS phase-1 12.04 / 1.73),
but on the T7 FULL dual-μ census (`QpEngineStructuralViolation.T7TheFeasibleCensusHalfOverTheFULLDualMuSet`)
`shut_slack_residue` went 6 → 12. The trace (a scratch census driver = `w2_walk_elastic` cell by cell,
temporary env-gated prints, removed):

- **Which cells.** The unlock fires on 21 census cells, all kSchurBorder, all of which ended N3
  (kNumericalError, blocker termination) at the vertex x = (10, 10, 0) with the elastic equality
  violated by 15. Under R-B, 15 end kOptimal at the right answer (box_blocked(10) ×5, stiff(1e2) ×8,
  stiff(4e5) μ = 1e-6 ×2) and 6 end kInfeasible with the slack shut: stiff(4e5), ρ 1e7/1e8,
  μ 1e-3/1e-4/1e-5 — the kSchurBorder twins of the six residues the census already discloses on
  kRefactorize, landing on the IDENTICAL points (x = 0.0124533 / 0.120464 / 0.9 per coordinate,
  worst 4.975 / 4.759 / 3.2).
- **The traced QP** (stiff(4e5), ρ 1e7, μ 1e-5, border): n 3 (x0, x1, slack s ∈ [0, 1]),
  H = diag(8e5, 8e5, 0), g = (0, 0, 5e7), x0 + x1 + 5s = 5, box [−10, 10]². FEASIBLE: HiGHS phase-1
  minimum l1 violation 0.0; the optimum by its KKT conditions is (2.5, 2.5, 0) (λ = −2e6, s's reduced
  cost 4e7 > 0). The kInfeasible is WRONG on the QP the walk was given — the slack is a live column,
  not a shut-slack variant.
- **The walk (observed).** Minors 1–3 reach (10, 10, 0) with s released; minors 4–5 re-block on
  s at zero motion, dependent, no admissible member, the N1 certificate attempted from {x0@U, x1@U}
  and NOT verified (g = 3, cond1 false), pass-over not within ⇒ the unlock releases x0 for s. The
  next step MOVES (α = 1, |dx| 11.05), a drop, another full step (|dx| 9.1), then a KKT point with
  nothing to drop, classified by the terminal check's `worst_structural_violation` = 3.2 > 0 ⇒
  kInfeasible at (0.9, 0.9, 0). **No Farkas certificate is involved** (`dep_infeasible_certificates`
  0, `dep_certificate_toleranced` 0).
- **Why.** (0.9, 0.9, 0) is the `dual_mu`-REGULARIZED stationary point of the face {s = 0}: the
  regularized EQP relaxes the equality by a μ·λ term and λ is O(c) ≈ 1e6, so the residual is O(1)
  (inferred from the three μ: x = 5/(2 + a·μ·8e5) with a = 0.499 / 0.494 / 0.444). The refactorize
  algebra reaches the same point in ONE step with no unlock and misreads it the same way — that is
  the census's disclosed residue. R-B does not create the misreading; it releases the border walk
  from the vertex where it used to stop (N3) and the walk then reaches it.
- **S28's candidate causes, each checked:** the QP infeasible — no (phase-1 0); a leak — no evidence;
  the toleranced branch mislabelled — no (no certificate at all); a zero-motion exchange leaving a set
  whose certificate verifies a perturbed problem — no (nothing verified; motion follows at once).
- **R-B′ (motion before any verdict) cannot help here:** all 21 census unlocks are followed by a full
  step (α = 1.0, |dx| ≈ 10–11). R-B′ differs from R-B only when the step after an unlock is again
  zero-motion, which never happens on the census, so it would read 12 as well. By S28's rule R-B is
  DEFERRED and fix round 3 lands R-A + R-C′ + S27.
- **The deferred round's premise:** the terminal classification must not read a μ·λ equality residual
  as a structural violation (a μ-free re-solve of the face before a kInfeasible verdict — the
  refactorize residue needs it too). With that, the six census cells should end kOptimal on both
  algebras and R-B's conversions (15 census cells, ACOPR57 qp0/qp1, READING6 qp0/qp1) could stand.

### R-D: evaluated, deferred (S30)

R-D (a stalled refinement is kUndecidable whatever its ρ; `dep_stall_admits` renamed
`dep_stall_undecidable`) was built and run through the round's instruments on the R-B-free tree:

- U0 (27 cells × 3 engines, offline): 0 status / objective / counter moves; 6 residual-class
  last-digit differences on f7_n800/f7_n1000_path_warm, inside the 1e-5 gate.
- Census: 6 → 12 — the same six stiff(4e5) border cells, by the same route (the border walk no longer
  stops at the vertex and reaches the μ-point).
- Suites: Release 13 / Debug 10 failures, including STATUS / ANSWER movers on non-E2 fixtures
  (`T6bTheStiffWALKSOwnPathologyAboveTheShippedDualMuIsUNCHANGED` border kNumericalError → kOptimal;
  `WarmStart.FullStepModeInheritsElasticAndRestoration`: no restoration and f off by 0.25;
  `BothAlgebras/WsIndependence.TheUncertifiedFallbackStillEndsAtAFirstOrderPoint/{0,1}`
  kOptimal → kNumericalError) and counter movers (`HsSweepCorpus.Corpus`,
  `ScaleF7.WideWindowWalkIsChurnNotDegeneracy`, `F7ColdScaleSmoke.SchurCapExhaustion…`, the long
  `Continuation.ProbeBudgetBoundsAFailingProposal` (Release), `T7TheVerdictRefineCost…` 15 → 14, HS
  battery minors 804/801 → 809/806).
- Corpus: READING6 qp0/qp1 stay kNumericalError; SAWPATH qp1, STEENBRB qp2, ACOPR57 qp6/qp7 go
  kMaxIter → kNumericalError; STEENBRB qp0 stays kOptimal (658 → 750 minors).

A status move on a non-E2 fixture is a STOP by the round's own rule, so R-D is DEFERRED; the tree
carries S23's contract and `dep_stall_admits` unchanged. Its brief: the 1e10 calibration (an exactly
independent candidate, ρ = 1/√2, whose refinement stalls because the 1e10 row puts the residual's
rounding floor above the unscaled target) is the case a ρ-free rule must still admit, and the census's
μ-point misreading must be fixed first (it absorbs every rule that releases the border walk).

### The 32-dump corpus on the final tree

Replayed on a fresh engine with each dump's own options (`qp_dump.h`), build/ Release, MKL, one thread
(`.scratch/m7w2e2fix3s2/r4-corpus-*.txt`). Columns: status / minors / f. The E2-ON @ a47753a8 column is
the fix-3 predecessor's replay of the same dumps (`corpus-table-rcprime.md`).

| dump | E2-OFF | E2-ON @ a47753a8 | FINAL fix3 (R-A + R-C′, as dumped) | FINAL, refactorize | R-A / R-C′ fired (dep_structural_first / reread flips) |
|---|---|---|---|---|---|
| ACOPP57_def/qp0 | max_iter / 1795 / 0.2426443804 | num_error / 2 / 0.2426448308 | optimal / 8 / 0.2426448308 | optimal / 2 | 0 / 4 |
| ACOPP57_def/qp1 | max_iter / 1795 / 0.2426444386 | num_error / 2 / 0.2426448308 | optimal / 8 / 0.2426448308 | optimal / 2 | 0 / 4 |
| ACOPP57_def/qp2 | max_iter / 1795 / 0.2426443645 | num_error / 2 / 0.2426448308 | optimal / 8 / 0.2426448308 | optimal / 2 | 0 / 4 |
| ACOPP57_def/qp3 | max_iter / 1795 / 0.2426444699 | num_error / 2 / 0.2426448308 | optimal / 8 / 0.2426448308 | optimal / 2 | 0 / 4 |
| ACOPR57_def/qp0 | optimal / 139 / 432.660937 | num_error / 51 / 0 | num_error / 51 / 0 | num_error / 50 | 0 / 0 |
| ACOPR57_def/qp1 | optimal / 154 / 501.8045708 | num_error / 90 / 634.4316793 | num_error / 90 / 634.4316793 | num_error / 89 | 0 / 0 |
| ACOPR57_def/qp2 | optimal / 145 / 649.1195857 | num_error / 47 / 0 | num_error / 47 / 0 | num_error / 46 | 0 / 0 |
| ACOPR57_def/qp3 | optimal / 3 / 1352734602 | max_iter / 4360 / 1352734500 | max_iter / 4360 / 1352734500 | max_iter / 4360 | 0 / 0 |
| ACOPR57_def/qp4 | max_iter / 7100 / 3.251954471 | num_error / 27 / -158.1983118 | num_error / 26 / -154.6021059 | num_error / 26 | 1 / 0 |
| ACOPR57_def/qp5 | max_iter / 7100 / 7.503736734e-09 | max_iter / 7100 / 4.480503331e-09 | max_iter / 7100 / 4.480503331e-09 | max_iter / 7100 | 0 / 0 |
| ACOPR57_def/qp6 | max_iter / 7100 / 0.6722031597 | max_iter / 7100 / 2.400261206e-09 | max_iter / 7100 / 2.400261206e-09 | max_iter / 7100 | 0 / 0 |
| ACOPR57_def/qp7 | max_iter / 7100 / 1.450693373 | max_iter / 7100 / 2.433230104e-09 | max_iter / 7100 / 2.433230104e-09 | max_iter / 7100 | 0 / 0 |
| ACOPR57_def/qp8 | max_iter / 7100 / 1.449219097 | num_error / 35 / 0.01922035013 | num_error / 35 / 0.01922035013 | num_error / 35 | 0 / 0 |
| DALLASM_def/qp0 | max_iter / 1960 / -860194.4492 | num_error / 64 / -905709.8251 | num_error / 347 / -912390.249 | num_error / 184 | 189 / 0 |
| DALLASM_def/qp1 | max_iter / 1960 / -361507.8627 | num_error / 64 / -453972.7467 | num_error / 344 / -457366.5514 | num_error / 126 | 189 / 0 |
| DALLASM_def/qp2 | max_iter / 1960 / -158541.6458 | num_error / 64 / -227271.7672 | num_error / 345 / -228974.9739 | num_error / 158 | 189 / 0 |
| DALLASM_def/qp3 | max_iter / 1960 / -48503.51863 | num_error / 64 / -113705.4969 | num_error / 339 / -114572.819 | num_error / 158 | 189 / 0 |
| DALLASM_f4/qp0 | max_iter / 1960 / -860194.4492 | num_error / 64 / -905709.8251 | num_error / 347 / -912390.249 | num_error / 184 | 189 / 0 |
| DALLASM_f4/qp1 | optimal / 161 / -923778.9323 | num_error / 194 / -918557.805 | num_error / 225 / -921798.7058 | num_error / 225 | 34 / 0 |
| DALLASM_f4/qp2 | max_iter / 1960 / -92643.83509 | num_error / 64 / -90976.08501 | num_error / 302 / -91668.66278 | num_error / 159 | 189 / 0 |
| DALLASM_f4/qp3 | max_iter / 1960 / -92643.83509 | num_error / 64 / -90976.08501 | num_error / 302 / -91668.66278 | num_error / 159 | 189 / 0 |
| READING6_def/qp0 | max_iter / 770 / -55.14512101 | num_error / 74 / -47.91505353 | num_error / 74 / -47.91505353 | num_error / 74 | 0 / 0 |
| READING6_def/qp1 | infeasible / 69 / -101.5217059 | num_error / 40 / -70.42753866 | num_error / 40 / -70.42753866 | num_error / 40 | 0 / 0 |
| SAWPATH_def/qp0 | optimal / 45 / -388.8091309 | num_error / 6 / -592.3480969 | num_error / 6 / -592.3480969 | num_error / 6 | 0 / 0 |
| SAWPATH_def/qp1 | max_iter / 20350 / 1.963399474e+10 | max_iter / 20350 / 157.603186 | max_iter / 20350 / 157.603186 | max_iter / 20350 | 0 / 0 |
| SAWPATH_def/qp2 | optimal / 182 / 252.7867399 | num_error / 6 / -77.58585163 | num_error / 6 / -77.58585163 | num_error / 6 | 0 / 0 |
| STEENBRB_def/qp0 | optimal / 744 / 1974445.191 | num_error / 908 / 1974601.347 | optimal / 658 / 1974445.191 | optimal / 749 | 1 / 0 |
| STEENBRB_def/qp1 | max_iter / 4680 / -9.903939761e+11 | max_iter / 4680 / -9.906415816e+11 | max_iter / 4680 / -9.910097509e+11 | max_iter / 4680 | 101 / 0 |
| STEENBRB_def/qp2 | max_iter / 4680 / -9.904038665e+11 | max_iter / 4680 / -9.905077531e+11 | max_iter / 4680 / -9.904010213e+11 | max_iter / 4680 | 18 / 0 |
| STEENBRB_def/qp3 | max_iter / 4680 / -9.287942432e+11 | num_error / 1222 / nan | num_error / 264 / -9.284241079e+11 | num_error / 264 | 26 / 0 |
| YORKNET_def/qp0 | optimal / 1030 / 1964037.129 | num_error / 242 / -188.6 | num_error / 242 / -188.6 | num_error / 242 | 0 / 1 |
| YORKNET_def/qp1 | optimal / 971 / 664247.8015 | num_error / 594 / 21565.81267 | num_error / 594 / 21565.81267 | num_error / 594 | 0 / 0 |

Status counts: E2-OFF {'optimal': 10, 'max_iter': 21, 'infeasible': 1, 'num_error': 0} ; FINAL as dumped {'optimal': 5, 'max_iter': 7, 'infeasible': 0, 'num_error': 20} ; FINAL refactorize {'optimal': 5, 'max_iter': 7, 'infeasible': 0, 'num_error': 20}

Headline: E2-ON @ a47753a8 solved 0 of the 32; the final tree solves 5 (ACOPP57 qp0–qp3 by R-C′,
STEENBRB qp0 by R-A), identically on both algebras; E2-OFF solves 10. No dump moved to a worse status
against E2-ON @ a47753a8.

### Registered gaps

- **ACOPR57 qp4** (R-C′, S26): a genuine strict local minimum (Z′HZ min 0.73) whose μ-free read is
  kSuspect and whose `dual_mu` read is kWrong; R-C′ does not certify it and the walk ends N3 (26
  minors). R-C as first ruled reached kOptimal there (400 minors, f −18.395). The honest fix is the
  reduced Hessian's inertia on the face computed directly when the face is small — a later round. The
  fixture as it would read (not in the suite; its dump is `../2026-09-m7-w2-e2-d4-feasible-qps/dumps/ACOPR57_def/qp4.txt`):

```cpp
TEST(QpE2WorkingSet, AMuFreeInertiaReReadSolvesTheAcopr57RestorationQp) {
    const QpDump d = e2d4_dump("ACOPR57_def-qp4.txt");
    EXPECT_NE(d.result.find("status=3 minors=27 "), std::string::npos);
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        const QpSolution s = e2d4_solve(d, alg);
        expect_e2d4_kkt(d, s);
        EXPECT_GE(s.counters.dep_inertia_reread_flips, 1);
        EXPECT_EQ(s.counters.dep_blocker_terminations, 0);
    }
}
```
- R-B (the μ-point terminal misreading first) and R-D (above) — deferred rounds.
- R-C′ cost (S29): a second read when the face is unchanged since the last re-read could be skipped —
  a later cost round.

### Re-pins (declared, with attribution)

| Fixture | Pin | Was → now | Cause |
|---|---|---|---|
| `HsBattery.CrashBasisIsANullResultOnTheBatteryWithTwoBoundSeededExceptions` (MKL arm) | `facts_off` / `facts_on` | 267 / 270 → 288 / 289 | R-C′: each re-read of a kWrong verdict costs one or two local eliminated factorizations, counted in `factorizations` whether or not it overturns (S29: cost, not correctness). Minors and answers unchanged. The Accelerate arm keeps its pre-E2 pins (UNOBSERVED) |
| `QpEngineIndefinite.T7Fix2AFailedStartRepairDoesNotRefineThroughTheProbedFactorization` (independent arm, both algebras) | `warm.counters.factorizations` | 1 → 3 | R-C′: the kWrong start verdict is re-read μ-free (kSuspect) and again at `dual_mu` (not kOk): two factorizations, no flip — now asserted (`dep_inertia_rereads` 1, `dep_inertia_reread_flips` 0). Status kInfeasible unchanged |

No status or answer assertion outside the new fixtures moved. The T6b stiff / HIGH-rung pathology
fixtures and the census are untouched (they move only under R-B, which is not landed).

### Offline G4 (before the gates)

U0 27 cells × 3 engines on the final tree (build/ Release): against the M6 close pin under the 1e-5
residual gate, walk 0 differences (7 within the gate, the known f7 warm residual digits), ssn 0,
ipm 0; against the fix-2 head's own CSVs (code-identical to a47753a8 on these cells) EXACT: 0
differences on all 75 columns in all three engines. No U0 status, objective or counter moved.

### Suites (this tree, MKL, clang++, `ctest -j4` under `setsid`, the box lock per leg)

Release 2831/2831; Debug 2830/2830 without the long test, the long test 1/1 alone (detached).
By name against fix round 2's gate transcripts (2826/2827): four ADDED instances
(`QpE2WorkingSet.{AStructuralDependencyDecidesFirstOnTheSteenbrbElasticQp,
TheLockedVertexOnTheAcopr57PrimalQpsStillEndsN3, AMuFreeInertiaReReadCertifiesTheSingularPsdFaceOfAcopp57,
AMuFreeSuspectReadDoesNotCertifyASaddle}`), none removed.

### Schema note (the harness follows at the next re-take)

NEW `QpCounters`: `dep_structural_first` (R-A), `dep_inertia_rereads`, `dep_inertia_reread_flips`
(R-C′). `factorizations` now also counts R-C′'s local re-read factorizations. No counter renamed or
removed (`dep_unlocks` and `dep_stall_undecidable` were never landed).


## Fix round 4 — the terminal point is refined μ-free before any kInfeasible-by-violation verdict

### The defect (the R-B trace, `.scratch/m7w2e2fix3s2/rb-trace/`; settler's ruling after fix round 3)

At a terminal KKT point of the walk (nothing left to drop), the structural classification
(`worst_structural_violation`, E4's companion rule on the rows) read the **dual_mu-regularized
stationary point of the face** as a structural violation. On a working face the regularized EQP
satisfies `A x − μ·λ = b`, so each working row's residual IS the footprint μ·λ; with λ ≈ 1e6
(a stiff row, λ* = −2e6) that is O(1): on the census QP stiff(4e5) (x0 + x1 + 5 s = 5 in a
[−10, 10]² box, s ∈ [0, 1], H = diag(8e5, 8e5, 0)) the terminal point is (0.9, 0.9, 0) at
μ = 1e-5 (residual 3.2; 0.0125 / 0.120 at μ = 1e-3 / 1e-4, residual 4.975 / 4.759), and the
walk returned **kInfeasible** 15 units from the optimum (2.5, 2.5, 0) of a FEASIBLE QP (HiGHS
phase-1 minimum violation 0). No certificate was involved. The refactorize algebra reached that
point in one step (the census's six "shut-slack residues"); the border algebra reached it only
after R-B's unlock (fix round 3, deferred), which is why R-B turned the census count 6 → 12. The
existing verdict-site face refinement (`refine_*_face_for_verdict`) cannot remove it: it refines
toward the REGULARIZED system's own solution and adopts only a closed face.

The same misreading is on the **plain** QP too (observed, `.scratch/m7w2e2fix4/probe-base.txt`
at ef3b5225): `w2_stiff_consistent_qp(4e5)` read kInfeasible at μ ∈ {1e-2, 1e-3, 1e-4, 1e-5} on
BOTH algebras (x = 0.00125 / 0.0125 / 0.120 / 0.9 per coordinate).

### The rule (landed)

| What | Where |
|---|---|
| At a terminal point whose classification would read kInfeasible (`worst > 0`), with `independent_working_set` on and `dual_mu > 0`, and **the cheap entry test** `mu_footprint_open` true — some working-face row (an equality or an active inequality) has `dual_mu·|λ_j| > feas_tol·row_scale_j` (row_scale as `worst_structural_violation`'s) — the face's EQP is **re-solved with dual_mu = 0** on a local bound-eliminated `KktFactor` (`eliminated_candidate` at μ = 0, R-C′'s path; the pass's factor and border stack untouched: ONE factorization, charged to `factorizations`), including `solve_eqp`'s mandatory unregularized step, then at most **k = `kTerminalMuRefineSteps` = 2** further iterative-refinement steps against the fully unregularized residual (primal_delta removed too), each kept only while that residual strictly falls | `src/qp/qp_engine.cpp:1018-1044` (call site), `:2987-3005` (`mu_footprint_open`), `:3007-3120` (`refine_terminal_mu_free`); `include/hven/detail/qp/qp_engine.h:509-515` (k), `:1368-1396` (declarations, the contract), `:373-376` (Degeneracy block) |
| **Adoption**: the refined point is adopted only if it satisfies EVERY row of the QP (equalities `|r_j| ≤ feas_tol·row_scale_j`, inequalities `r_j ≤ feas_tol·row_scale_j`, all rows, not only the face) and EVERY bound (`≤ feas_tol·max(1, |bound|, ‖x‖∞)` outside, E4's box read), with all values finite. Adopted ⇒ x and the multipliers are replaced (λ_e, λ_w from the μ-free solve; z re-priced by `price`) | same |
| After adoption the point meets the **existing** gates again, unchanged: `refresh_shifts`, the drop test on the refined multipliers (a member they release ⇒ the walk continues from the refined, feasible point), the re-read structural classification (now 0), the runaway test, the inertia verdict (+ R-C′), the zero-motion exhaustion test, the zero-multiplier probe, the suspect-stall gate, and **E4's certificate bound half** — so kOptimal is reached only through the same gates as any other kOptimal | same |
| Not adopted (the μ-free point leaves the box, violates a row, or the μ = 0 factorization throws) ⇒ nothing changes: the existing terminal classification stands (kInfeasible by the structural violation) | same |
| Counters `terminal_mu_refines` (entries), `terminal_mu_refine_rescues` (adoptions) | `include/hven/core/solver_counters.h:290-297` |

**No-op where the rows are already satisfied, by construction:** the rule is entered only at
`worst > 0` — a would-be kInfeasible — so a terminal point that already satisfies the rows (the
kOptimal path) never reaches it, and it cannot move any status other than a kInfeasible read off a
violation. It cannot produce a false kOptimal on an infeasible QP: adoption needs a point that
satisfies every row and bound within feas_tol·row_scale, which an infeasible QP does not have.

**Deviation from the brief, stated:** the brief's fallback reads "kInfeasible ONLY with a
verified certificate per S25′; else kNumericalError inside the box". The terminal classification's
kInfeasible exit has never carried a certificate (certificates are produced at blockers, N1); the
brief also says "the existing path", so the existing exit is kept unchanged. Tightening it to
certificate-or-kNumericalError would move statuses on INFEASIBLE fixtures (e.g. stiff(4e5) in a
[−2, 2]² box at μ ≥ 1e-5 reads kInfeasible at the μ-point, with no certificate, where at μ ≤ 1e-6
the walk reaches the corner and its exact N1 certificate) — registered below for the settler.

### Fixtures (both configurations)

- `QpEngineStructuralViolation.E2Fix4TheTerminalMuPointIsRefinedBeforeAnInfeasibleVerdict`
  (`tests/sqp/test_sqp_solver.cpp:11157`): the census cell (elastic stiff(4e5), ρ 1e7, μ 1e-5) on
  kRefactorize — at ef3b5225 kInfeasible at (0.9, 0.9, 0) — is kOptimal at (2.5, 2.5, 0) (1e-9),
  slack shut, `terminal_mu_refines` 1 / `terminal_mu_refine_rescues` 1, the KKT residual
  RECOMPUTED from the returned (x, λ, z): row ≤ 5·feas_tol, stationarity ≤ 1e-9 relative,
  λ_e = −2e6, z(s) > 0. The border twin still ends N3 (kNumericalError, `dep_blocker_terminations`
  1, no refinement entered): it reaches the μ-point only through R-B (deferred). The plain stiff(4e5)
  QP on BOTH algebras at μ ∈ {1e-2, 1e-3, 1e-4, 1e-5} (ef3b5225: kInfeasible at the μ-point) is
  kOptimal at (2.5, 2.5) with one refinement and one rescue; at μ ∈ {1e-6, 1e-8} the refinement
  is never entered.
- `QpEngineStructuralViolation.E2Fix4ARealTerminalViolationStillEndsInfeasible` (`:11215`), the
  control: the same stiff row in a [−2, 2]² box is INFEASIBLE (exact Farkas certificate y = 1:
  max over the box of x0 + x1 = 4 < 5, asserted). At μ = 1e-5 the terminal point is the same
  μ-point, the refinement runs (1) and is NOT adopted (0: the μ-free point (2.5, 2.5) leaves the
  box), and the verdict stays kInfeasible with a violation > 1; at μ = 1e-8 the walk ends at its
  own verified EXACT certificate (`farkas.residual ≤ exact_tol`) with the refinement never entered.
  Both algebras.

### The census (`T7TheFeasibleCensusHalfOverTheFULLDualMuSet`) — declared honest mover

`shut_slack_residue` **6 → 0** (re-pinned, attributed in the fixture): the six were stiff(4e5)'s
kRefactorize cells at ρ 1e7/1e8, μ 1e-3/1e-4/1e-5, each now kOptimal at (2.5, 2.5, 0) with one
refinement and one rescue; no other census cell changed (census tool,
`.scratch/m7w2e2fix4/f4-census.txt` vs `.scratch/m7w2e2fix3s2/r4-census.txt`: kOptimal 93 → 99,
kNumericalError 21 → 21, kInfeasible 6 → 0). The T6b stiff / HIGH-rung fixtures
(`T6bTheStiffWALKSOwnPathologyAboveTheShippedDualMuIsUNCHANGED`,
`T6bTheBandsHIGHRungsRemainAPREEXISTINGResidue`) are UNCHANGED: both end N3 at a zero-motion
vertex on the border path and never reach a terminal point; they improve only with R-B.

### The 32-dump corpus (`.scratch/m7w2e2fix4/f4-corpus-*.txt`, `corpus-table-fix4.md`)

Replayed as fix round 3 did (each dump's own options, fresh engine, Release, MKL, one thread). The
fix-4 lines are **byte-identical** to the ef3b5225 lines (`.scratch/m7w2e2fix3s2/r4-corpus-*.txt`)
in all four columns (as dumped, border, refactorize, E2-OFF) apart from the two new counter
columns — the refinement is entered on none of the 32 dumps (`tref = 0` everywhere). Headline:
E2-OFF 10 kOptimal; E2-ON @ ef3b5225 5; E2-ON + fix4 5 (ACOPP57 qp0–qp3, STEENBRB qp0), both
algebras. The corpus's failures are not this class.

| dump | OFF | ON@ef3b5225 border | ON@ef3b5225 refac | OFF+fix4 | ON+fix4 border | ON+fix4 refac | fix4 tref/tresc (border, refac) |
|---|---|---|---|---|---|---|---|
| ACOPP57_def/qp0 | kMaxIter 1795m | kOptimal 8m | kOptimal 2m | kMaxIter 1795m | kOptimal 8m | kOptimal 2m | 0/0, 0/0 |
| ACOPP57_def/qp1 | kMaxIter 1795m | kOptimal 8m | kOptimal 2m | kMaxIter 1795m | kOptimal 8m | kOptimal 2m | 0/0, 0/0 |
| ACOPP57_def/qp2 | kMaxIter 1795m | kOptimal 8m | kOptimal 2m | kMaxIter 1795m | kOptimal 8m | kOptimal 2m | 0/0, 0/0 |
| ACOPP57_def/qp3 | kMaxIter 1795m | kOptimal 8m | kOptimal 2m | kMaxIter 1795m | kOptimal 8m | kOptimal 2m | 0/0, 0/0 |
| ACOPR57_def/qp0 | kOptimal 139m | kNumericalError 51m | kNumericalError 50m | kOptimal 139m | kNumericalError 51m | kNumericalError 50m | 0/0, 0/0 |
| ACOPR57_def/qp1 | kOptimal 154m | kNumericalError 90m | kNumericalError 89m | kOptimal 154m | kNumericalError 90m | kNumericalError 89m | 0/0, 0/0 |
| ACOPR57_def/qp2 | kOptimal 145m | kNumericalError 47m | kNumericalError 46m | kOptimal 145m | kNumericalError 47m | kNumericalError 46m | 0/0, 0/0 |
| ACOPR57_def/qp3 | kOptimal 3m | kMaxIter 4360m | kMaxIter 4360m | kOptimal 3m | kMaxIter 4360m | kMaxIter 4360m | 0/0, 0/0 |
| ACOPR57_def/qp4 | kMaxIter 7100m | kNumericalError 26m | kNumericalError 26m | kMaxIter 7100m | kNumericalError 26m | kNumericalError 26m | 0/0, 0/0 |
| ACOPR57_def/qp5 | kMaxIter 7100m | kMaxIter 7100m | kMaxIter 7100m | kMaxIter 7100m | kMaxIter 7100m | kMaxIter 7100m | 0/0, 0/0 |
| ACOPR57_def/qp6 | kMaxIter 7100m | kMaxIter 7100m | kMaxIter 7100m | kMaxIter 7100m | kMaxIter 7100m | kMaxIter 7100m | 0/0, 0/0 |
| ACOPR57_def/qp7 | kMaxIter 7100m | kMaxIter 7100m | kMaxIter 7100m | kMaxIter 7100m | kMaxIter 7100m | kMaxIter 7100m | 0/0, 0/0 |
| ACOPR57_def/qp8 | kMaxIter 7100m | kNumericalError 35m | kNumericalError 35m | kMaxIter 7100m | kNumericalError 35m | kNumericalError 35m | 0/0, 0/0 |
| DALLASM_def/qp0 | kMaxIter 1960m | kNumericalError 347m | kNumericalError 184m | kMaxIter 1960m | kNumericalError 347m | kNumericalError 184m | 0/0, 0/0 |
| DALLASM_def/qp1 | kMaxIter 1960m | kNumericalError 344m | kNumericalError 126m | kMaxIter 1960m | kNumericalError 344m | kNumericalError 126m | 0/0, 0/0 |
| DALLASM_def/qp2 | kMaxIter 1960m | kNumericalError 345m | kNumericalError 158m | kMaxIter 1960m | kNumericalError 345m | kNumericalError 158m | 0/0, 0/0 |
| DALLASM_def/qp3 | kMaxIter 1960m | kNumericalError 339m | kNumericalError 158m | kMaxIter 1960m | kNumericalError 339m | kNumericalError 158m | 0/0, 0/0 |
| DALLASM_f4/qp0 | kMaxIter 1960m | kNumericalError 347m | kNumericalError 184m | kMaxIter 1960m | kNumericalError 347m | kNumericalError 184m | 0/0, 0/0 |
| DALLASM_f4/qp1 | kOptimal 161m | kNumericalError 225m | kNumericalError 225m | kOptimal 161m | kNumericalError 225m | kNumericalError 225m | 0/0, 0/0 |
| DALLASM_f4/qp2 | kMaxIter 1960m | kNumericalError 302m | kNumericalError 159m | kMaxIter 1960m | kNumericalError 302m | kNumericalError 159m | 0/0, 0/0 |
| DALLASM_f4/qp3 | kMaxIter 1960m | kNumericalError 302m | kNumericalError 159m | kMaxIter 1960m | kNumericalError 302m | kNumericalError 159m | 0/0, 0/0 |
| READING6_def/qp0 | kMaxIter 770m | kNumericalError 74m | kNumericalError 74m | kMaxIter 770m | kNumericalError 74m | kNumericalError 74m | 0/0, 0/0 |
| READING6_def/qp1 | kInfeasible 69m | kNumericalError 40m | kNumericalError 40m | kInfeasible 69m | kNumericalError 40m | kNumericalError 40m | 0/0, 0/0 |
| SAWPATH_def/qp0 | kOptimal 45m | kNumericalError 6m | kNumericalError 6m | kOptimal 45m | kNumericalError 6m | kNumericalError 6m | 0/0, 0/0 |
| SAWPATH_def/qp1 | kMaxIter 20350m | kMaxIter 20350m | kMaxIter 20350m | kMaxIter 20350m | kMaxIter 20350m | kMaxIter 20350m | 0/0, 0/0 |
| SAWPATH_def/qp2 | kOptimal 182m | kNumericalError 6m | kNumericalError 6m | kOptimal 182m | kNumericalError 6m | kNumericalError 6m | 0/0, 0/0 |
| STEENBRB_def/qp0 | kOptimal 744m | kOptimal 658m | kOptimal 749m | kOptimal 744m | kOptimal 658m | kOptimal 749m | 0/0, 0/0 |
| STEENBRB_def/qp1 | kMaxIter 4680m | kMaxIter 4680m | kMaxIter 4680m | kMaxIter 4680m | kMaxIter 4680m | kMaxIter 4680m | 0/0, 0/0 |
| STEENBRB_def/qp2 | kMaxIter 4680m | kMaxIter 4680m | kMaxIter 4680m | kMaxIter 4680m | kMaxIter 4680m | kMaxIter 4680m | 0/0, 0/0 |
| STEENBRB_def/qp3 | kMaxIter 4680m | kNumericalError 264m | kNumericalError 264m | kMaxIter 4680m | kNumericalError 264m | kNumericalError 264m | 0/0, 0/0 |
| YORKNET_def/qp0 | kOptimal 1030m | kNumericalError 242m | kNumericalError 242m | kOptimal 1030m | kNumericalError 242m | kNumericalError 242m | 0/0, 0/0 |
| YORKNET_def/qp1 | kOptimal 971m | kNumericalError 594m | kNumericalError 594m | kOptimal 971m | kNumericalError 594m | kNumericalError 594m | 0/0, 0/0 |
| **kOptimal** | 10 | 5 | 5 | 10 | 5 | 5 | |

### R-B with fix4 present — READ-ONLY, for the next brief (not landed, not in the tree)

R-B as saved in fix round 3's STATE sections (the narrowed placement: unlock only at `kTerminate`,
zero motion, off the border stack; Bland ties) applied on top of this tree in a scratch copy
(`.scratch/m7w2e2fix4/src-rb`, library only): ACOPR57 qp0 / qp1 **kOptimal** on both algebras
(183 / 182 minors border, 182 / 181 refactorize; f 432.6609425 / 501.8045781 against E2-OFF's
432.660937 / 501.8045708), qp2–qp8 unchanged (num_error / max_iter), fix4 entered on none of them
(`.scratch/m7w2e2fix4/rb-acopr57-*.txt`). The T7 census under R-B + fix4: **120/120 kOptimal,
shut 0, open 0** (12 rescues: the six refactorize cells and their six border twins R-B now reaches;
`rb-census.txt`) — against 12 shut at fix round 3's R-B measurement. The suites were NOT run on
that variant.

### Re-pins (declared, with attribution)

| Fixture | Pin | Was → now | Cause |
|---|---|---|---|
| `QpEngineStructuralViolation.T7TheFeasibleCensusHalfOverTheFULLDualMuSet` | `shut_slack_residue` | 6 → 0 | fix4: the six refactorize residues are the μ·λ misreading; refined, each is the optimum (an IMPROVEMENT to the verified answer) |
| `QpEngineIndefinite.T7Fix2AFailedStartRepairDoesNotRefineThroughTheProbedFactorization` (independent arm, both algebras) | `warm.counters.factorizations` | 3 → 4 | fix4: the terminal x0 = 0.5 reads kInfeasible with the footprint open; one μ-free re-solve (one local factorization), not adopted (the rows e_0 = 0 and e_0 = 1 are inconsistent). Now asserted: `terminal_mu_refines` 1, `terminal_mu_refine_rescues` 0. Status and x unchanged |

No status or answer assertion outside the two new fixtures and the census moved.

### Offline G4 (before the gates)

U0 27 cells × 3 engines on the fix-4 tree (scratch Release build of the same sources): against the
M6 close pin under the 1e-5 residual gate walk 0 differences (7 within the gate, the known f7 warm
residual digits), ssn 0, ipm 0; against the ef3b5225 head CSVs (`.scratch/m7w2e2fix3s2/g4-final/`)
EXACT: 0 differences on all 75 columns in all three engines (`.scratch/m7w2e2fix4/g4/comparison.txt`).
No U0 status, objective or counter moved.

### Suites (MKL, clang++, `ctest -j4` under `setsid`, the box lock per leg)

Release 2833/2833; Debug 2832/2832 without the long test, the long test 1/1 alone (detached, 689 s;
informational) — the main tree's `build/` and `build-debug/` at the committed sources
(`.scratch/m7w2e2fix4/m1-suite-{rel,dbg}.log`, `m1-suite-dbg-long.log`). By name against fix round
3's gate transcripts (`gates-e2-fix3/suite-{debug,release}.txt`, 2830/2831): two ADDED instances
(`QpEngineStructuralViolation.{E2Fix4TheTerminalMuPointIsRefinedBeforeAnInfeasibleVerdict,
E2Fix4ARealTerminalViolationStillEndsInfeasible}`), none removed. The HS corpus (`HsBattery.*`,
`HsSweepCorpus.*`, the HS driver cells) is inside both suites and no HS pin moved (HsBattery's `facts_off` / `facts_on`
pins, which would count each entry's factorization, are unchanged at 288 / 289).

### Registered

- The terminal classification's kInfeasible exit carries no certificate (pre-existing; above). When
  the μ-free point satisfies the rows but leaves the box (the control), a ratio-tested step toward
  it would reach the corner and the N1 certificate the μ ≤ 1e-6 walk already finds — a candidate
  for a later round, not taken here ("never a blind step").
- The refined-multiplier drop branch (a member the μ-free multipliers release ⇒ the walk continues)
  is exercised by no fixture: no observed cell takes it.
- Cost: each entry is one factorization; on an inconsistent face (the T7Fix2 re-pin) it is spent
  and not adopted — the entry test cannot tell an inconsistent row (residual AT the footprint) from
  a consistent one before the solve.
- Accelerate: the μ = 0 local factorization of an exactly singular face (dependent rows) is
  UNOBSERVED there; MKL perturbs and reports success, and a `std::runtime_error` is caught as
  "no refinement".

### Schema note (the harness follows at the next re-take)

NEW `QpCounters`: `terminal_mu_refines`, `terminal_mu_refine_rescues`. `factorizations` now also
counts the terminal μ-free re-solve's local factorization. No counter renamed or removed; not
aggregated into `SqpCounters` (as R-C′'s counters are not).
