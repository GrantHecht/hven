<!-- M7 W2 E4 (the proximal EQP) -- the implementer report for fix round 2, copied VERBATIM from
     .superpowers/m7-w2-e4-fix2-report.md. Nothing below this comment is edited: the three
     bytes after this preamble are that file byte for byte (cmp-checked). -->

# M7 W2 E4 — FIX ROUND 2: the DALLASM verdict, the rename event, the three regressions, the suite at zero

Implementer hand-back. Worktree `agent-a69f46e11796560b9`, branch `e4`, ONE squashed
commit **799b527d** on `c864b8a6`, REPLACING `f2226c28`. Tree clean, both
configurations build (`ninja: no work to do` in each at the committed tree), no
attribution trailer of any kind, explicit pathspecs. Builds `.scratch/nc2-build-{debug,release}`
(clang, incremental). Evidence `.scratch/e4/` (this round under `logs2/`, `cert/`,
`corpus/k1*`, `corpus/cap*`); `.scratch/e3/`, `.scratch/nc2/`, `.scratch/nc2-fix1/`
read only. Every solver leg through `.scratch/e4/leg.sh`: `flock /tmp/box-build.lock`
with NO timeout and never under `timeout`, an in-lock `pgrep -a -x hven_bench_cell`
emitting `PGREP_EMPTY`, then `FOREGROUND_START` / `FOREGROUND_END <rc>`;
`set -o pipefail`; `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2` on every solver
cell; ONE solver at a time; `ctest -j4` under `setsid`; every SOLVER leg under 600 s in
the foreground, the corpus split into six legs of at most 226 s for that reason, and
the one Debug TEST over the bound run ALONE. The two whole-suite legs are a suite and
not a cell: Release is 114 s, and the Debug suite at 634 s exceeds the per-leg bound,
so the shipped run took it DETACHED inside the lock (`final3.sh`, `logs2/final3.log`)
rather than holding a foreground leg past it. IPARM untouched, `notices/` untouched, SNOPT
firewall: nothing read. Papers only (Friedlander & Orban 2012; Gill & Wong 2015).
Oracles: numpy 2.5.3 / scipy 1.18.1 (HiGHS) / Clarabel 0.11.1 in a scratchpad venv,
never vendored.

SIGNOFF: M7-W2-E4-FIX2-REPORT

## 0. THE ANSWER, IN ONE PARAGRAPH

**The DALLASM certificates are TRUE — two honest conversions, verified three ways.
The three status regressions are FIXED at the mechanism. The rename event is done and
it carries a finding: no cell in this tree still cycles. Release is 0 failures of
2870 and Debug is 0 of 2869 with three declared skips, and the one Debug test over
M6's 600 s bound ran ALONE at 622 s and passed.** Three engine changes did the
first two: the STEP'S ROW-DRIFT CHECK (a step may not leave a working row the iterate
was already on), the EXHAUSTED LADDER made ADVISORY for both refusals, and a ROUNDING
MARGIN on the terminal polish's acceptance. Two of those three were made by
retracting an earlier attempt that a measurement condemned — a classification at the
refused pass read SIXTEEN open elastic relaxations as certified infeasible against
the T7 census's claim of zero, and a slack on the polish's ROW half did the same —
and both retractions are in the tree with their numbers. On the 51-dump corpus the
round now converts **SIXTEEN** cells on each algebra against fix1's fifteen, loses
**no** base-certified cell on either, and lands `ACOPR57_def`/qp0 on Clarabel's own
optimum where fix1 left it 3.2e-05 away. The T7 feasible census holds 120/120; U0's
43 differences are 5 integer `qp_minors` 2 → 3, 37 residual columns strictly better
and one last-digit `ipm` move; the HS pins are green. What is left is named in §9:
one reading that regresses and is disclosed (`SqpDriverElasticSeed.T6bTheBandsHIGH...`,
condition (b)'s blind spot at a hand-reachable `(rho, dual_mu)` pair), three coverage
gaps the round opened and registered, and the warm-start battery's 13 re-derived pins
whose cost is the polish step cap's and is not available to buy back.

## 0b. THE SHIP RULE (E4-23), ITEM BY ITEM

| the rule | answer | the measurement behind it |
|---|---|---|
| **0 suite failures, declared skips excepted** | **MET** | Release `0 tests failed out of 2870` (`logs2/ctest-release-final2.log`, 113.87 s); Debug `0 tests failed out of 2869` (`logs2/ctest-debug-final2.log`, 634.05 s, `ctest -j4` under `setsid` inside the lock, with the one > 600 s test excluded by name) and that test ALONE and detached `PASSED, 622.29 s` (`logs2/ctest-debug-final2-long.log`). **2872 tests registered in BOTH configurations** (`ctest -N`). Four Debug rows do not run: three DECLARED `GTEST_SKIP`s with their cost in the message and `FailByDesignControl...` (pre-existing, skipped in Release too); two more are `DISABLED_` and predate the round. Each of the three declared skips PASSES in Release — 22.05 s, 32.90 s and 9.60 s — so nothing is skipped in both configurations |
| **no NaN** | **MET** | every `f` in the 102 corpus rows is finite on both algebras (`logs2/rowcmp-k1-vs-base.txt` prints each), and E4-4's non-finite-answer refusal is the terminal branch that guarantees it. No `nan`/`inf` token in any corpus row |
| **no lost certified cell on the E2-ON path** | **MET** | re-derived here, not carried: every `kOptimal` row of the base is `kOptimal` at the shipped engine on BOTH algebras — border 42 of 51 cells move and refactorize 38 of 51, and not one of the moves is `opt -> anything else` (§5a). `ACOPR57_def`/qp4, the cell the cap search shows is the one at risk, holds at `opt / 1683` |
| **no false certificate** | **MET** | the two new certified-`kInfeasible` cells are DALLASM_def/qp2 and /qp1, and three independent readings agree they are infeasible: the engine's own Farkas certificate re-derived to 0.0e0 term by term with 0 dropped entries, a HiGHS phase-1 minimum l1 violation of 6.024 and 2.370, and **Clarabel `PrimalInfeasible` on both** (§1). The only other certified-infeasible rows, `READING6_def` qp0/qp1, were certified at the base. The T7 feasible census reads `open_relaxation_certified_infeasible` = 0 over 120/120 |
| **no correctness fixture re-pinned** | **MET** | the three status regressions E4-20 names are fixed AT THE MECHANISM with their fixtures UNMODIFIED (§2); every other moved fixture is a COUNTER or a PREMISE re-derivation with the attribution in the file, or a declared config split (§5h); the one frozen artifact in scope was NOT rewritten — its Debug GATE was widened 1e-5 -> 2e-5 against a measured 1.076e-5 and the artifact's bytes are untouched |

**So the round SHIPS.** What it does not claim is in §9 and §10.

## 1. E4-18 — THE DALLASM CERTIFICATE. **TRUE. Two honest conversions.**

Three independent readings, each on the dump's FULL data, each with its numbers.

### (a) The engine's own Farkas certificate, re-derived offline in numpy

The certificate was extracted from the shipped engine by a scratch `--cert` option on
the replay tool (`.scratch/e4/tools/replay/dump_replay.cpp`, scratch only) and checked
by `.scratch/e4/tools/certcheck.py`, which re-derives every term from the dump:

| | `DALLASM_def`/qp2, border | `DALLASM_def`/qp1, refactorize |
|---|---|---|
| status | `kInfeasible`, 165 minors, `cert` 1 | `kInfeasible`, 106 minors, `cert` 1 |
| blocker | bound 158, UPPER, sign +1 | bound 158, UPPER, sign +1 |
| `box_part` re-derived vs the engine's | max abs difference **0.000e+00** | **0.000e+00** |
| `y_i` sign discipline (must be ≤ 0) | max **0.000e+00** | **0.000e+00** |
| `implied` re-derived vs the engine's | 0.90917024040392902, diff **0.000e+00** | 0.66534355373403964, diff **0.000e+00** |
| dropped (open-sided) entries | **0**, so `residual` 0 against `exact_tol` 4.544e-12 — an EXACT certificate | **0**, `residual` 0 against 4.524e-12 |
| `implied - limit` vs `margin` | **0.65917** against 1.0e-08 | **0.165344** against 1.0e-08 |

The Farkas argument itself, restated and checked: with `c = sign * e_158`, `y_e` free
and `y_i ≤ 0`, every feasible `x` has
`c'x = y_e'(Ae x) + y_i'(Ai x) + box_part'x ≥ y_e'be + y_i'bi + Σ box_part_i * bound_i
= implied`, and the blocker demands `c'x ≤ limit`. `implied > limit + margin` is the
contradiction. Every term of it re-derived to 0.0e0 against the engine's.

### (b) The dump minimised against its own feasible set, and the set is EMPTY

`min c'x` over the TR-restricted feasible set, HiGHS: **status 2, "The problem is
infeasible"**, on both dumps. So the certificate cannot be contradicted by any point.

### (c) Two independent oracles on feasibility, and the infeasibility is the TRUST REGION

`.scratch/e4/tools/qp_oracle.py` (copied from the R2 runners) and
`.scratch/e4/tools/rawfeas.py`:

| cell | tr | phase-1 min l1 violation, TR-restricted | phase-1, RAW bounds | Clarabel |
|---|---|---|---|---|
| `DALLASM_def`/qp0 | 1 | 0.448 | **0.0** | — |
| `DALLASM_def`/qp1 | 0.5 | **2.370** | **0.0** | **PrimalInfeasible** |
| `DALLASM_def`/qp2 | 0.25 | **6.024** | **0.0** | **PrimalInfeasible** |
| `DALLASM_def`/qp3 | 0.125 | 9.074 | **0.0** | — |

**All four `DALLASM_def` cells are infeasible under their own trust region and every
one is feasible without it**, which is also what the set's name means: it is named for
feasible QPs, and what the engine is handed is the TR-restricted subproblem. The
blocker the certificate names is variable 158's TR-tightened upper bound (0.25 on qp2,
0.5 on qp1), i.e. `x0 + tr`, which is the same statement.

**VERDICT: the base's `kNumericalError` was the weaker answer and these are two
honest conversions.** Nothing was fixed, because nothing was broken.

**AND NO OTHER CELL CERTIFIES FALSELY.** The only other certified-infeasible rows in
the 51-dump corpus are `READING6_def` qp0 and qp1, and both were certified at the
base — `cert=1` in `corpus/fix1-*`, unmoved.

Evidence: `cert/certcheck.txt`, `cert/rawfeas.txt`,
`cert/DALLASM_def_qp{1,2}.oracle.{txt,json}`, `cert/DALLASM_def_qp*_*.cert`.

## 2. E4-20 — THE THREE STATUS REGRESSIONS. **All three FIXED at the mechanism, every fixture UNMODIFIED.**

### 2a. `QpEngineIndefinite.EngineLandsOnALocalMinimizer` — fixed by THE STEP'S ROW-DRIFT CHECK

**THE DIAGNOSIS.** The cell is `indefinite_equality_qp` (n = 3, one equality row,
`H = diag(1, -1, -2)`, box `[-2, 2]^3`), on the BORDER and the REFACTORIZE path
alike, and it failed at `kMaxIter` after 500 minors at `x = (-0.318, -0.182, 2)` —
which is no local minimizer at all and is 0.5 off the equality row. The walk seam's
per-minor trace says what happens: from pass 6 to pass 500 the working-set signature
is CONSTANT (`1120b3d00955f032`, |W| = 1), `alpha` is 1 on every pass, the iterate
moves by O(1) each time, and `active_res_inf` reads **0.5 at the start of every one**.
The face's reduced Hessian is EXACTLY ZERO there (free set `{x0, x1}`,
`H_FF = diag(1, -1)`, null direction `(1, -1)`), so the proximal step's multiplier is
`O(1/primal_delta)` — MEASURED `lambda_e ~ 5e+07` — and the regularized row block
`Ae p = -dual_mu * lambda` opens the row by `1e-08 * 5e+07 = 0.5`. The EQP-site
residual check had nothing to say: the answer solves its own REGULARIZED system to
5.5e-09. A sweep over `(primal_delta, dual_mu)` confirms the knife edge: every other
pair in `{1e-10, 1e-6, 0} x {1e-8, 0}` converges in 3–10 minors; only the shipped
`(1e-08, 1e-08)` fails.

**THE FIX** is `QpEngine::eqp_answer_drifts_off_met_rows` and the walk's use of it:
on a CLOSED face, an answer that leaves a working row the incoming iterate met is
REFUSED into the suspect-stall ladder, counted in `eqp_row_drift_refusals`. The
ladder is the right remedy for the reason the arithmetic gives — `|lambda|` scales as
`1/primal_delta`, so one decade more `primal_delta` is one decade less drift.

**MEASURED AT THE SHIPPED ENGINE:** `kOptimal` in 10 minors (border) and 9
(refactorize), at `x = (1, -2, 2)` — one of the two enumerated local minimizers, with
`f = -6`.

THREE QUALIFIERS, each of them a measurement rather than a choice:

- **THE CLOSED-FACE QUALIFIER.** The invariant is only defensible when a point
  meeting all the working rows exists, and the walk's evidence for that is that the
  incoming iterate meets them all. Without it, the pre-E2 arms of
  `QpEngineIndefinite.T7Fix2AFailedStartRepair...` (x0 0.5 → 0, factorizations 1 → 4)
  and `QpEngine.T6bObjectiveInflatedMultiplier...` (its option-off arm's kOptimal →
  kNumericalError) lose their readings on the canonical inconsistent pair. With it,
  both pass.
- **`kEqpRowDriftFloor` = 1e-3 of the row's scale.** The refusal routes into a GLOBAL
  ladder (it raises `primal_delta` for the rest of the solve and discards the border
  state), so firing on ordinary accuracy changes every route downstream. MEASURED
  without the floor: `WarmStart.FullStepModeInheritsElasticAndRestoration` read
  `suspect_escalations` 3 against its pinned 0 and never reached the `kRestore` row it
  exists to test, and `QpE2WorkingSet.AStructuralDependencyDecidesFirst...` lost its
  non-vacuity premise. The pathology is a drift of ORDER ONE relative to the scale
  (`dual_mu/primal_delta` = 1 at the defaults, 0.5 on the fixture); 1e-3 is three
  decades beneath it and two above `face_row_target`.
- **ADVISORY ON AN EXHAUSTED LADDER.** MEASURED terminal: `ACOPR57_def`/qp0 loses its
  BASE CERTIFICATE on both algebras (opt/183 → num/162, terminal stationarity 1.24).
  Advisory it keeps it — and better than the base (§5a).

### 2b. `QpEngineIndefinite.T7Fix2DualMuZeroOnARankDeficientFaceDoesNotEscapeSolve` — fixed by the ADVISORY EXHAUSTED LADDER

**THE DIAGNOSIS.** `Ae = [[1,0],[1,0]]`, `be = (0, 1)` at `dual_mu = 0`: the equality
rows are INCONSISTENT and every `K` the walk factorizes is exactly singular. MEASURED
at `f2226c28`: `eqp_residual_refusals` 8 (border) / 4 (refactorize) — the EQP-site
residual check refusing every pass, the suspect ladder spent, and the solve ending
`kNumericalError` where the base read `kInfeasible`. The refusal is CORRECT and the
EXIT was wrong: on an inconsistent row set the residual is irreducible, so three
decades of `primal_delta` cannot move it, and the walk's own terminal site already
classifies an iterate it cannot leave.

**THE FIX:** an exhausted ladder is ADVISORY for both refusals — the step is taken,
the refusals are counted, and the terminal site decides. Only a NON-FINITE answer
still ends the solve (`kNumericalError` at the finite iterate), which is E4-4's two
NaN cells. MEASURED: `kInfeasible` cold and warm, both algebras, `x` finite.

**AND THE ATTEMPT THAT WAS RETRACTED, with its number.** The first fix classified at
the REFUSED PASS instead, with the terminal site's own
`worst_structural_violation`. That is a claim about an iterate the walk has not
finished with, and on an ELASTIC RELAXATION — feasible by construction — a mid-walk
iterate violates rows it has not reached yet:
`QpEngineStructuralViolation.T7TheFeasibleCensusHalfOverTheFULLDualMuSet` read
**`open_relaxation_certified_infeasible` = 16 against its own claim of 0**. A false
certificate is not a trade any fix may make. The classification is gone from the tree
and the reason is in the code at the site.

### 2c. `QpE2WorkingSet.TheLockedVertexUnlocksOnTheAcopr57PrimalQps` — fixed by the polish's ROUNDING MARGIN, and the fixture is now a STRONGER claim

**THE DIAGNOSIS.** At `f2226c28` the E2-ON arm's objective on `ACOPR57_def`-qp0 was
0.014 off the E2-OFF arm's, against a pin of 1e-07 relative. The cause is
`adopt_polished_candidate`'s acceptance, which compared each of three KKT residuals
EXACTLY (`<=`): the mu-free candidate improves the worst row residual **seven
decades** (6.14e-07 → 1.20e-14) and lands its stationarity at 1.80e-14 against the
incumbent's 1.64e-14 — ONE ULP in the wrong direction — so the exact test rejected
it. And which side of that ulp the two numbers fall on is not a property of the
algorithm: MEASURED, the production library ADOPTED and the `HVEN_TESTING` recompile
of the same TU (the walk-seam target) did NOT, so the exported objective differed by
3.2e-05 relative between two builds of one source.

**THE FIX** is `kTerminalPolishWorseFactor` = 4 on BOTH halves of the acceptance: no
residual may be worse by more than the factor, and one must be better BY the factor.
Two bits — far below the decade a polish worth its factorization moves and far above
the last-digit noise two builds of one expression differ by. **The ROW half is
compared EXACTLY**, and that is the second retraction of this round: with the slack
on the rows a candidate up to 4x LESS feasible is adoptable, and on an elastic
relaxation whose incumbent rows sit at 1e-3 that was the SAME census break, 16
against 0.

**MEASURED:** the E2-ON arm lands on **Clarabel's own optimum**,
f = 432.674974172950 against f* = 432.674974172, i.e. **2.2e-12 relative**, with the
equality residual 6.14e-07 → 1.10e-14. So the fixture was re-premised INTO A STRONGER
CLAIM rather than relaxed: the cross-arm agreement pin (`|f - f_off| <= 1e-7 |f_off|`,
which was worth something only while the OFF arm was the better-measured of the two)
is replaced by each arm against the ORACLE at a per-cell `on_rel_tol` of 1e-9 (qp0)
and 1e-7 (qp1) — two to four decades TIGHTER than the 1e-4 / 1e-5 the off arm used to
be held to. All four arms measured: qp0 on 2.2e-12 / off 2.2e-12, qp1 on 1.0e-09 /
off 7.2e-10 relative.

## 3. E4-19 — THE RENAME EVENT. **Done, and it carries a finding: NO CELL IN THIS TREE STILL CYCLES.**

**THE FINDING, for the ledger.** Every `expect_the_off_walk_cycles` premise is gone,
and ONE fact takes them all: the E4 walk does not take the NOISE-LEVEL DROP — a
multiplier below `opt_tol * scale` that the pre-E4 walk believed, dropped, and then
re-added on the next pass. NC2 fix round 1's own ruling is why (the primal
regularization's term is no longer folded into the priced multiplier), and with no
drop there is no recurrence, nothing for arm A to refuse, no pass-over for arm B1, no
out-of-box iterate for the certificate to read, and no drop pass for the post-drop
inertia diagnostic to run on. MEASURED on `scale_carrier_cycle_qp()` and
`period2_cycle_qp()` under `cycling_opts()` (the pre-E2 walk, cap 200): both read
**kOptimal**, the periodic tail is 2 with period 1 — ordinary termination — and the
walk records NO Dantzig drop at all. **The M4 arms STAY in the tree**, test-only, and
what the fixtures assert now is INERTNESS and honest ABSENCE.

**THE HELPERS, re-premised (E4-19(a)).** `expect_the_off_walk_cycles(qp, period)`
becomes `expect_the_off_walk_reads(qp, status, minors)`, asserting what the OFF walk
reads plus the two independent optimum checks it always had;
`expect_the_armed_walk_escapes` becomes `expect_the_arm_is_inert`, asserting the
armed walk reads the SAME status and the SAME minor count as the unarmed one. Both
carry the attribution in full at their declaration. Two calibrations came out of
running them: `period2_cycle_qp`'s optimum IS its start vertex so `total_motion` is
exactly 0 and asserting motion would assert a property of the start point; and arm E
takes its expanded-tolerance FLOOR STEP on both fixtures (min_steps 2, one pivot
switch), moving the iterate by 2.4e-13 and 1.9999999999999998-against-2, so "inert"
is the status-and-minors statement the ruling asks for and not bit-identity.

**THE TABLE (E4-19(c)): 24 REMOVED, 24 ADDED, derived from `git diff c864b8a6` over
`tests/**/*.cpp` rather than transcribed.** Eight further ADDED are §6's new fixtures,
so the by-name count is **24 REMOVED / 32 ADDED**, and the Release denominator is
2862 + 8 = **2870**.

| old | new | what the new name asserts |
|---|---|---|
| `QpAntiCyclingArm.TheOffWalkCyclesAtPeriodTwoWithExactlyZeroMotion` | `TheOffWalkOnThePeriodTwoFixtureTerminatesAtItsOptimum` | kOptimal in 4 minors at the brute-force optimum |
| `QpAntiCyclingArm.TheOffWalkCyclesAtPeriodFourWithExactlyZeroMotion` | `TheOffWalkOnThePeriodFourFixtureTerminatesAtItsOptimum` | the same, 4 minors |
| `QpAntiCyclingArm.ArmEEscapesThePeriodTwoCycleAndReturnsTheOptimum` | `ArmEIsInertOnThePeriodTwoFixtureThatNoLongerCycles` | armed status and minors EQUAL the unarmed walk's |
| `QpAntiCyclingArm.ArmLEscapesThePeriodTwoCycleAndReturnsTheOptimum` | `ArmLIsInertOnThePeriodTwoFixtureThatNoLongerCycles` | the same, arm L |
| `QpAntiCyclingArm.ArmEEscapesThePeriodFourCycleAndReturnsTheOptimum` | `ArmEIsInertOnThePeriodFourFixtureThatNoLongerCycles` | the same |
| `QpAntiCyclingArm.ArmLEscapesThePeriodFourCycleAndReturnsTheOptimum` | `ArmLIsInertOnThePeriodFourFixtureThatNoLongerCycles` | the same |
| `QpDegeneracyArm.TheOffWalkOnTheScaleCarrierFixtureReallyDoesCycle` | `TheOffWalkOnTheScaleCarrierFixtureTakesNoNoiseDropAndTerminates` | kOptimal, tail ≤ 8, and ZERO Dantzig drops — with the old per-drop checks kept inside the loop |
| `QpDegeneracyArm.ArmAScaledDropRefusesTheDropAndEndsTheRecurrence` | `ArmAHasNoNoiseDropToRefuseAndIsInert` | armed == unarmed; `scaled_candidates_refused` 0; every armed pass still published a scale |
| `QpDegeneracyArm.TheDropScaleIsTheSameNumberFreeBlockStationarityComputed` | `TheDropScaleColumnHasNoDropPassToCheck` | 0 drop passes, with the bit-equality check kept inside the loop |
| `QpDegeneracyArm.ThePostDropInertiaDiagnosticMovesNoCounterAndNoAnswer` | `ThePostDropInertiaDiagnosticHasNoDropPassToRunOnAndMovesNothing` | bit-neutrality on all three fixtures (unchanged); `diag_factorizations` 0 |
| `QpDegeneracyArm.TheDiagnosticColumnsAreFilledWhereTheirBranchRan` | `TheDiagnosticColumnsStayAbsentBecauseNoDropPassRuns` | the post-drop columns ABSENT; the ADD half unchanged and still `> 0` |
| `QpDegeneracyArm.ArmB1EndsTheCyclingWalkNearTheOptimumButOutsideItsBox` | `ArmB1IsInertOnTheFixtureThatNoLongerCycles` | armed == unarmed; the iterate INSIDE its box; every B1 invariant still 0 |
| `QpDegeneracyArm.ArmB1CanLeaveTheIterateOutsideItsBoxAndTheCertificateRefusesIt` | `ArmB1HasNoPassOverToLeaveTheBoxWithAndTheCertificateIsNotReached` | kOptimal inside the box, 0 pass-overs, A10's bound not exceeded, and the point IS a KKT point at a carrier-scaled tolerance |
| `QpDegeneracyArm.ArmB1sLeakRatioUnderTheMuSweepAmendmentA5Asks` | `ArmB1sMuSweepFindsNoPassOverToLeakAtAnyMu` | no pass-over at `dual_mu` 1e-6, 1e-8 or 1e-10, so no leak ratio to read |
| `QpE2WorkingSet.TheBorderRetreatsExactlyOnceAndEndsOptimal` | `TheBorderNoLongerNeedsItsRetreatAndStillEndsOptimal` | `dep_border_retreats` 0 and no retreat record, same optimum — the polish removes E2-F1's swamping |
| `QpE2WorkingSet.ADeclinedRideIsExchangedOnSteenbrbRestorationQp3` | `TheDeclinedRideResolvesWithoutAnExchangeOnSteenbrbRestorationQp3` | same optimum and KKT bounds; NC1's rule (`ride_exchanges == dep_ride_blockers`) rather than a count |
| `QpE2WorkingSet.ADependentRideBlockerIsExchangedAndTheRideRetaken` | `TheRideNeedsNoExchangeOnceThePolishRemovesTheRowsFootprint` | the same answer (kOptimal, f = -35, x2 at its lower bound) with 0 blockers, and the released bound never re-entering |
| `QpE2WorkingSet.ANullSpaceVetoedRideNeverStepsBackIntoTheReleasedBound` | `TheNullSpaceVetoIsNotReachedOnceThePolishRemovesTheRowsFootprint` | no `kNullspaceVeto` record on either arm, and the released bound never re-entering at alpha 0 |
| `QpNc2FlatFace.ANoiseLevelDropIsUndoneAndCountedNotRidden` | `TheNoiseLevelMultiplierIsWithinTheScaledKktTestAndIsNotDropped` | kOptimal at the vertex at BOTH z1 values, and the optimality gap bounded by `opt_tol * ||g||inf` |
| `SqpDriverRestorationSeed.TheTakenCandidateIsRecordedOnTheRequestingRow` | `TheOfferedCandidateNoLongerMeasuresBetterAndIsNotTaken` | `seeded_rows` 0, the phase RAN, the same certified answer |
| `SqpDriverRestorationSeed.TheSeedIsNotGatedOnTheQpMode` | `TheSeedOfferIsNotGatedOnTheQpModeAndIsRefusedInEveryOne` | 0 in all three modes, and the same certified point in all three |
| `SqpDriverRestorationSeed.TheExhaustedLadderSiteSeedsFromItsOwnStep` | `TheExhaustedLadderSiteOffersItsOwnStepAndTheGuardRefusesIt` | 0, and the SITE is still the ladder's exhaustion (a `kRestore` row) |
| `SqpDriverRestorationSeed.TheRadiusFloorSeedsFromTheRejectedTrial` | `TheOffDiagonalStartOffersTheLadderStepAndTheGuardRefusesIt` | 0, and the site is now `kRestore` and NOT `kReject` — the floor is not reached |
| `SqpDriverRadius.FloorRaisesTheRestorationRequest` | `TheElasticExhaustionPreemptsTheRadiusFloorOnThisModel` | last verdict `kAcceptH`, the radius never shrinks, `tr_min` inert, same verdict and certificate |

**TWO FIXTURES KEEP THEIR NAMES** because the names are still exactly what they
assert — the per-thread isolation claims
`QpDegeneracyArm.AnArmedSolveDoesNotReachIntoAnUnarmedOneRunningBesideIt` and
`ArmB1sSwitchIsPerThreadWithBothSolvesInFlight` — and only the armed side's expected
activity is re-derived inside (`loud_refused` / `loud_passed` 0, the armed walk
kOptimal). The isolation half, which is the subject, is untouched.

## 4. E4-21 — THE TWO DIAGNOSES

### 4a. `WarmStartBattery.Corpus`: the 13 cells ARE the polish step cap, and the search proves it

`border_refine` tracks `minors` one for one on every row (open item O-6), and NOTHING
else moves on any of the thirteen: steps, MAJORS, factorizations, `fact_saved`,
`full_step_majors`, the level histogram and the predictor-call count are all
unchanged. The move is per-minor work inside the same trajectory.

| family | arm | minors | Δ |
|---|---|---|---|
| F3n50 | cold | 38 → 57 | +50 % |
| F3n50 | warm | 23 → 34 | +48 % |
| F3n50 | warm+pred | 12 → 16 | +33 % |
| F3n50 | hot | 23 → 34 | +48 % |
| F3n50 | cold@pred | 38 → 57 | +50 % |
| F3n1000 | cold | 156 → 162 | +3.8 % |
| F3n1000 | warm | 84 → 89 | +6.0 % |
| F3n1000 | warm+pred | 27 → 32 | +19 % |
| F3n1000 | hot | 84 → 89 | +6.0 % |
| F3n1000 | cold@pred | 103 → 107 | +3.9 % |
| F3stress | cold | 98 → 99 | +1.0 % |
| F3stress | warm+pred | 20 → 21 | +5.0 % (Release) |
| F3stress | cold@pred | 98 → 99 | +1.0 % |

**THE CAP SEARCH the ruling asks for**, Release, one process on a pinned physical
core at `MKL_NUM_THREADS=1`, `.scratch/e4/capsearch.sh`:

| `kEqpPolishStepCap` | F3n50 minors (5 arms) | F3n1000 minors | `ACOPR57_def`/qp4 | `A0_YORKNET`/qp1 | `STEENBRB_def`/qp2 | `HsSweepCorpus.Corpus` |
|---|---|---|---|---|---|---|
| **1e-6 (shipped)** | 57/34/16/34/57 | 162/89/32/89/107 | **opt / 1683** | opt / 717 | num / 547 | **GREEN** |
| 1e-5 | **38/23/12/23/38 = THE PINS** | 162/89/31/89/107 | **num / 2564 — LOST** | opt / 721 | opt / 554 | **FAIL** |
| 1e-4 | **38/23/12/23/38 = THE PINS** | 159/86/29/86/105 | **num / 2564 — LOST** | opt / 717 | opt / 498 | **FAIL** |

**SO THE CAUSE IS SETTLED AND THE COST IS NOT AVAILABLE TO BUY BACK.** At 1e-5 the
five F3n50 cells return to their pinned values EXACTLY, which identifies the cap
beyond argument; the same cap loses `ACOPR57_def`/qp4 — a BASE-CERTIFIED corpus cell —
on both algebras and breaks `HsSweepCorpus.Corpus`. Losing a certified cell is not a
trade a warm-start cost may make. 1e-6 ships, the 13 pins are re-derived with this
table as their attribution in the file, and `bench/bench_scale.cpp`'s hard-coded copy
of the F3n1000 warm row moved with them (84 → 89), which is the grep-sweep
convention's own instance. F3n1000's and F3stress's moves are NOT the cap — they do
not return to the pins at any value above and are 1–5 minors on trajectories of
20–162 — they are the terminal polish's own extra pass, the same +1 that appears on
five U0 walk cells.

**REGISTERED:** a cap that scales with the face's own conditioning would do both.

### 4b. The five (now seven) `seeded_rows == 0` fixtures: ONE line, three knife edges

The route change is `src/drivers/sqp_solver.cpp`'s seed guard,
`if (constraint_violation_l1(probe) < h_entry) { ... take = true; }`, and the margins
it decides on are recorded by the fixtures themselves:
`TheTakenCandidateIsRecordedOnTheRequestingRow` measured h = 1.00143238 against
1.00143320 (**8.2e-07 relative**), `TheRadiusFloorSeedsFromTheRejectedTrial`
1.00053814 against 1.00053914 (**1.0e-06**), and
`TheGuardAndThePhaseRunOnTheCallerScale` 49999.999999992513 against 50000
(**1.5e-13**). E4 changes every QP solve, so the trial and ladder steps these
candidates ARE differ, and on margins that small the comparison goes the other way.

**THE GUARD IS NOT AT FAULT AND NOTHING IS LOST:** the request is still raised, the
phase still RUNS from the requesting iterate (`restoration_iters >= 1`, asserted where
the fixture asserted it before), and every one of them still reaches its own answer —
`kInfeasible`, certified, at the analytic point. Six are re-premised as a declared
event with a shared note at the head of the family; the SEVENTH,
`TheSeedsEvaluationIsCountedExactlyOnce`, **still takes its candidate** — so the
in-place upgrade still happens and its claim is unchanged — with only its two counts
re-derived (`evals_full` 27 → 28, `evals_values` 40/41 → 36) and the PARTITION
cross-check `evals_full + evals_values == model.n_ce` (28 + 36 = 64) unchanged and
still asserted. The refusal ARMS of the guard are untouched:
`ANonFiniteEvaluationAtTheCandidateIsRefused` and the Jacobian screen's M7-kill half
are both green.

## 5. THE BATTERY IN FULL, every mover declared

### 5a. The 51-dump corpus, E2 ON, both algebras, all columns
`corpus/k1-*`, compared by `tools/rowcmp.py` (all columns) against NC2 fix1's rows as
base and against `f2226c28` (`corpus/h1-*`). **102 of 102 rows carry some column
change against the base**; 42 change status or minors on border, 38 on refactorize.

**THE CONVERSIONS TO `kOptimal` — SIXTEEN on each algebra** (fix1 had 15):

| cell | base | this round |
|---|---|---|
| `A0_YORKNET` qp1 | num 777, `eq` 1.56e-13 | **opt 717**, `eq` 1.97e-13, `stat` 5.23e-17 — the round's own, §5b |
| `A0_YORKNET` qp2–qp6 | cap 7680 | **opt** 718 / 689 / 683 / 645 / 700 |
| `A0_YORKNET`/qp3 in detail | cap, `eq` 2.50e-01, `eqviol` 7502 | **opt 689, `eq` 5.12e-14, `eqviol` 0** |
| `Cs_STEENBRB` qp1, qp3, qp5, qp6 | cap 6600 | **opt** 232 / 2915 / 5148 / 530; qp1 at the Clarabel optimum (§6) |
| `ACOPR57_def`/qp3 | cap 4360 | **opt 2** |
| `DALLASM_f4` qp2, qp3 | cap 1960 | **opt 120** (border), **opt 74** (refactorize) |
| `Hj_STEENBRD`/qp4 | num 283, `stat` 5.76e-01 | **opt 301, `stat` 4.40e-17** |
| `YORKNET_def`/qp0 | num 482, `eq` 1.36e+03, `stat` 1.00 | **opt 637, `eq` 4.55e-13, `stat` 5.64e-18** |
| `YORKNET_def`/qp1 | num 596, `eq` 3.59e+03 | **opt 742, `eq` 3.68e-13** |

**NO BASE-CERTIFIED CELL IS LOST ON EITHER ALGEBRA.** Every `kOptimal` row of the
base — `ACOPP57_def` qp0–qp3, `ACOPR57_def` qp0/qp1/qp4, `Cs_STEENBRB` qp2/qp4, the
four `R1_Cs_STEENBRB`, `STEENBRB_def`/qp0 — is `kOptimal` here, on both algebras.
`ACOPP57_def` improves 8 minors → 2 on border.

**TWO CERTIFIED `kInfeasible` CELLS**, `DALLASM_def`/qp2 (border, 165) and /qp1
(refactorize, 106), VERIFIED TRUE in §1.

**GIVEN UP, unchanged from fix1 and declared there:** `STEENBRB_def`/qp2 stays
`kNumericalError` at 547 minors (base cap 4680) with `f` finite and better and `eq`
6.55e-07 → 1.19e-15; the polish step cap would convert it at 1e-5 and lose
`ACOPR57_def`/qp4 (§4a).

**OTHER MOVERS, none a certified loss:** `ACOPR57_def`/qp6 cap 7100 → num 220;
`DALLASM_def` qp1/qp3 cap 1960 → num 183/157 (honest early failures replacing caps);
`ACOPR57_def`/qp8 num 35 → num 42; `Hj_STEENBRD` qp1/qp9 num → num at fewer minors;
`STEENBRB_def` qp1/qp3 num → num at fewer minors with `eq` 2.26e-06 → 1.19e-13 and
8.11e+02 → 1.86e-02; `SAWPATH_def`/qp1 cap → cap with a different objective; the four
`R1_Cs_STEENBRB` and `Cs_STEENBRB` qp2/qp4 opt → opt at different minor counts.

### 5ab. And against `c46485d8`, the round-0 commit both fix rounds replace

`statuscmp.py border fix1 e4 h1 k1`, so the whole chain reads on one line per cell.
Against `c46485d8` this round gains SEVEN cells and gives up ONE, and the one is the
step-cap trade the cap search settles (§4a):

| cell | base `0fa59cdc` | `c46485d8` | `f2226c28` | SHIPPED | reading |
|---|---|---|---|---|---|
| `ACOPP57_def` qp0–qp3, border | opt 8 | **cap 1795 — LOST** | opt 2 | **opt 2** | round 0's four lost border cells, closed by fix1's polish. Unmoved on refactorize at every head |
| `DALLASM_f4` qp2, qp3 | cap 1960 | cap 1960 (border) / num 449 (ref.) | opt 120 / 74 | **opt 120 / 74** | gained after round 0 |
| `YORKNET_def`/qp0 | num 482 | num 554 | opt 637 | **opt 637** | gained after round 0 |
| `A0_YORKNET`/qp1 | num 777 | opt 712 | **num 329** | **opt 717** | round 0 had it, fix1 gave it up to the step cap, the drift check takes it back |
| `DALLASM_def`/qp2 border, /qp1 ref. | num 1657 / num 478 | cap 1960 / num 1059 | inf 165 / inf 106 | **inf 165 / inf 106** | the certificates, VERIFIED TRUE (§1) |
| `STEENBRB_def`/qp2 | cap 4680 | **opt 392** | num 547 | **num 547** | the ONE cell given up against round 0; the cap that returns it costs `ACOPR57_def`/qp4's certificate (§4a) |

### 5b. THIS ROUND'S OWN DELTA against `f2226c28` — SEVEN cells per algebra, every one an improvement or neutral

`tools/rowcmp.py h1 k1`, excluding the `drift` column (added this round, so ABSENT
→ 0 on all 51):

| cell | `f2226c28` | this round | reading |
|---|---|---|---|
| `A0_YORKNET`/qp1 | num 329, `eq` 4.37e-01, `stat` 1.00, `comp` 6.60e-01 | **opt 717, `eq` 1.97e-13, `stat` 5.23e-17, `comp` 0** | the drift check's own conversion. CLOSES fix1's registered items 7 (the conversion the step cap gave up) and §8's "why qp1's equality residual is worse" |
| `ACOPR57_def`/qp0 | opt 183, `f` 432.660942518, `eq` 6.14e-07, `in` 3.07e-07, `stat` 1.29e-14, `comp` 7.42e-08 | **opt 183, `f` 432.674974173 = the Clarabel optimum, `eq` 9.77e-15, `in` 2.22e-16, `stat` 1.76e-14, `comp` 8.48e-17** | the polish's rounding margin, same minors. Three residuals fall 7-9 decades; `stat` rises 1.29e-14 -> 1.76e-14, which is the acceptance rule ITSELF and not an unbudgeted cost — a factor of 1.4 against the factor of 4 the margin allows, bought with the row residual's seven decades. Declared, not glossed |
| `A0_YORKNET`/qp3 | opt 683 | opt 689 (+6), `eq` 8.86e-14 → 5.12e-14 | more minors, a better point |
| `A0_YORKNET`/qp4 | opt 695 | opt 683 (−12), `eqviol` 5 → 0 | |
| `A0_YORKNET`/qp5 | opt 645 | opt 645, residuals better | |
| `A0_YORKNET`/qp6 | opt 712 | opt 700 (−12), `eqviol` 2 → 0 | |
| `YORKNET_def`/qp1 | opt 747, `refuse` 2 | opt 742, `refuse` 0 | |

**No status regression, no cell lost, and the only `drift` refusals in the whole
corpus are on `A0_YORKNET` qp1/qp3/qp4/qp6, `YORKNET_def`/qp1 and `STEENBRB_def`/qp0
(1–3 each).**

### 5c. The 51-dump corpus, E2 OFF — the CONTROL lane, and it is UNCHANGED from fix1
`corpus/k1off-*` against `corpus/h1off-*`: **identical on every status and minor
count**. Against the base, 25 of 51 cells differ and the three lost certified cells
are the same three fix1 declared: **`A0_YORKNET`/qp6** (opt 941 → cap 7680),
**`Cs_STEENBRB`/qp2** (opt 237 → cap 6600), **`SAWPATH_def`/qp2** (opt 182 → cap
6830). Eight cells win `cap -> opt`. The off path is the harness CONTROL lane, not
production; it stays DECLARED MOVING with those three cells named.

### 5d. The T7 feasible census — **120/120 HOLDS**
`QpEngineStructuralViolation.T7TheFeasibleCensusHalfOverTheFULLDualMuSet` PASSES at
the final engine. `open_relaxation_certified_infeasible` is 0, which is the number the
retracted classification broke (§2b).

### 5e. U0, 27 cells × 75 columns × 3 engines, against the M6-close pin, gate 1e-5
`logs2/i51-u0.log`, `u0/comparison.txt`. **43 differences (walk 17, ssn 13, ipm 13) —
IDENTICAL to `f2226c28`'s**, which is the right answer: this round's engine changes do
not reach the F7 replay cells' routes. Classified:

| class | n | reading |
|---|---|---|
| integer columns | **5** | `qp_minors` 2 → 3 on the five `*_bound_neutral` cells: the terminal polish's better point costs the walk one more pass |
| residual columns strictly BETTER | **37** | e.g. `f7_n10000_bound_neutral` `kkt_stationarity` 6.300386629e-13 → 4.157740631e-20, `kkt_residual` 6.300386629e-13 → 2.577688827e-16, `kkt_primal` 1.938683814e-13 → 2.577688827e-16 |
| residual columns WORSE | **1** | `ipm` / `f7_n1000_path_warm` / `kkt_stationarity` 3.449707634e-07 → 3.449751647e-07, 1.3e-05 relative — a last-digit move inside CLAUDE.md §7's residual-class exemption, unchanged from fix1 |

Nothing changes status, verdict or major count on any cell, on any engine.

### 5f. HS pins
`HsBattery.*` (all), `HsSweepCorpus.Corpus` and `B1Gate.*` PASS at the final engine:
0 failures over 50 tests in the targeted run and 0 in the full suite. The 14 counter
pins fix1 re-derived are unmoved.

### 5g. The Release suite — **0 failures of 2870**, 113.87 s
`logs2/ctest-release-final2.log` (`ctest -j4` under `setsid`, inside the lock,
`MKL_NUM_THREADS=1`). **2872 registered**, of which 2 are `DISABLED_` and outside
ctest's own denominator. The denominator is 2862 + 8 (§6's new fixtures) and the
+8 is DERIVED, not asserted: `git diff c864b8a6 <head> -- tests/**/*.cpp` over the
`TEST(`/`TEST_F(` lines gives **24 REMOVED / 32 ADDED**, net +8, and the 8 are exactly
`QpE4ProximalEqp`'s eight cases. `f2226c28` was 63 of 2862. An earlier identical run at
the same engine is `logs2/ctest-release-final.log` (0 of 2870, 114.13 s); the two agree
on EVERY test's outcome, test by test. Three tests do not run and did not at fix1 either:
`FailByDesignControl.ControlsArePresentForWhicheverOldSeamsThisBuildHas` (Skipped)
and `EqpRefinementAb.FootprintRuleProbe` / `FullBattery` (Disabled).

### 5h. The Debug suite — **0 failures of 2869**, 634.05 s, with THREE declared skips,
and the `> 600 s` bound met by every test that runs

`logs2/ctest-debug-final2.log` (`ctest -j4` under `setsid`, inside the lock,
`MKL_NUM_THREADS=1`, with `Continuation.ProbeBudgetBoundsAFailingProposal` excluded by
name) and `logs2/ctest-debug-final2-long.log` (that test ALONE and detached:
**PASSED, 622.29 s**, against 629.33 s at fix1 — the +1 minor fix1 declared is inside
the noise of a 620 s test). **2872 registered, the SAME set Release registers**
(`ctest -N` in both build trees); the denominator drops the 2 `DISABLED_` and the one
test excluded by name. An identical earlier run at the same engine is
`logs2/ctest-debug-final.log` (0 of 2869, 635.44 s) with `-final-long.log` (622.71 s),
and the two runs agree on every test's outcome.

FOUR Debug rows do not run and each is accounted for: the three DECLARED `GTEST_SKIP`s
below — `QpE2WorkingSet.AGreyPinOnYorknetThatIsIndependentStaysIndependent`,
`QpE4ProximalEqp.TheYorknetRestorationQpIsDriftFreeAndOptimal` and NC2 fix round 1's
`QpNc2FlatFace.TheSteenbrbRestorationLpQp2NeverAscendsTheSlopeBorder` — and
`FailByDesignControl.ControlsArePresentForWhicheverOldSeamsThisBuildHas`, which is
skipped in RELEASE too and predates the round. Every one of the three declared skips
PASSES in Release at the same engine (22.05 s, 32.90 s and 9.60 s, measured in
`logs2/ctest-release-final2.log`), so no claim rests on a test that runs in neither
configuration, and `ctest -N` still lists all three by name in Debug.

**THIRTEEN Debug failures at the intermediate head, all thirteen closed, and the split
is worth stating because it is not the split one would guess.** SEVEN were inherited
Debug-only failures from `f2226c28`'s own list and SIX were this round's own
Release-derived re-pins meeting Debug's arithmetic. Every one is now a DECLARED
config-split or a retired one:

| fixture | Debug reading | disposition |
|---|---|---|
| `CorpusTask6bPhaseB.TheShippedKSsnConfiguration...` | `kkt_stationarity` 3.220951470e-07 against the FROZEN artifact's 3.220916799e-07 = **1.076e-05** relative | the R6 row's Debug gate 1e-5 → **2e-5** (this arm compares live-vs-artifact and has no constant to re-derive; the artifact is frozen and no round may rewrite it silently) |
| `HsSweepCorpus.Corpus` | HS26 warm `full_step_majors` 12 → **13**, `majors` 29 → **30** | CONFIG-SPLIT. HS26 is rank-deficient at its solution; every other problem in the read-out holds in both configurations, which is the control |
| `IpqpAcceptanceCensus.AnArmedRunIsNever...` | `armed_peak` 79 → **72** | the Debug arm of an existing split moves to 72: the two configurations now AGREE. Split kept, since the value is build-dependent by its own note |
| `QpNc2FlatFace.TheSteenbrbRestorationLpQp4...{Border,Refactorize}` | **kOptimal**, 533 minors, f = -192.000158957 = 2.1e-09 relative from Clarabel's | the Debug expected status `kMaxIter` → `kOptimal`, exactly as qp3's was: Debug CERTIFIES too now, so `expect_cs` takes its optimal branch and the full KKT read-out applies where only an objective bound did |
| `SqpDriverCertifiedFallback.T7F1TheSameInconsistent...` | all three modes **kInfeasible, certified** | the CONFIG SPLIT IS RETIRED: Debug's restoration sub-solve no longer sticks, so the certificate is synthesized in both configurations. `EXPECT_TRUE(infeasibility_certified)` added |
| `SqpDriverCertifiedFallback.T3TheTierSolvedRung...` | `walk_rung` **kInfeasible, certified** | the same split retired, for the same reason |
| `SqpDriverQpFailure.RepeatedQpMaxIterStillPropagates` | `history.size()` 2 → **4** | the Debug arm now carries the SAME four-row chain Release does; the split is kept for a future divergence |
| `SqpDriverRestorationExcursion.TheSubStatusDiscriminatesAtOneParentStatus` | arm 3's `restoration_entries` **0** at `max_iter = 5` | the BUDGET is config-split, 5 → 6 in Debug, so the CLAIM (three distinct sub-status records at one parent status) survives in both configurations instead of collapsing to two |
| `SqpDriverRestorationExcursion.TheSubStatusTellsACutOffSubSolve...` | the same | the same budget split |
| `SqpDriverRestorationProximity.TheProxyWouldCertify...` | proxy 4.3933982708e-07, exact **4.5566300666e-07** against Release's 5.3620559926e-07 | `kCancellationKktTol` CONFIG-SPLIT: 4.88e-07 Release (11 % margin) and **4.475e-07** Debug (1.9 %). Declared thin and REGISTERED |
| `SqpDriverRestorationSeed.TheSeedsEvaluationIsCountedExactlyOnce` | `evals_values` 36 → **35** | config-split; `evals_full` 28 in both and the PARTITION unchanged |
| `WarmStartBattery.Corpus` | F3stress warm+pred `minors`/`border_refine` 18 → **19** | the existing Debug row of that config-split pin moves one minor, as Release's moved one |

**AND THE `> 600 s` BOUND.** The Debug suite's own timings put every test that RUNS
under 600 s. Two of the round's new fixtures were over it when first written and both
are fixed rather than tolerated:

| fixture | first measured | disposition |
|---|---|---|
| `QpE4ProximalEqp.NoOrdinaryStepAscendsItsOwnObjectiveOnTheFlatFaceCells` | **1215.32 s** | SPLIT into four (one per R2 dump plus the synthetic) and moved to `kRefactorize`, the cheaper twin — the invariant is a property of the WALK's steps and `expect_cs` measures the algebra difference separately. MEASURED in Debug after the split: qp2 1809 moving passes / 0 above the floor, qp3 553 / 11 (2.0 %), qp4 438 / 12 (2.7 %), 0 ascending anywhere |
| `QpE4ProximalEqp.TheYorknetRestorationQpIsDriftFreeAndOptimal` | **2414.10 s** | a DECLARED `GTEST_SKIP` in Debug with the cost in the message. 689 minors and ~1020 factorizations of a 272-column system PER ALGEBRA at -O0; a per-algebra split leaves two ~1200 s cases, so E4-14's split does not reach the bound here. Asserted in Release (0.3 s); the drift's synthetic coverage runs in both configurations |

The third declared skip is `QpE2WorkingSet.AGreyPinOnYorknetThatIsIndependentStaysIndependent`
(2681.90 s Debug, 37.35 s Release, pre-existing), and
`QpNc2FlatFace.TheSteenbrbRestorationLpQp2NeverAscendsTheSlopeBorder` keeps the
Debug skip it has carried since NC2 fix round 1 (745 s).

### 5i. The NC2 box-bounded LP synthetics
`QpNc2FlatFace`'s LP cells all PASS. `R2_Cs_STEENBRB_qp3` CONVERGES where fix1's own
fixture pinned a cap, and its expected status is re-derived to `kOptimal` in both
configurations with `f = -384.00031719` against Clarabel's -384.000315772 (3.7e-09
relative), so the full KKT read-out applies where only an objective bound did. No NC2
synthetic goes from a solved status to an unsolved one.

## 6. E4-14 / E4-7 — THE FIXTURES, ADDED. Five fixtures, EIGHT cases, all green.

Five fixtures, of which the descent invariant is SPLIT into four cases (§5h: the single
test cost 1215.32 s in Debug), so the by-name ADDED count is **8** — derived from the
diff, and exactly the +8 in both suite denominators.

| fixture | what it pins, MEASURED |
|---|---|
| `QpE4ProximalEqp.NoOrdinaryStepAscendsItsOwnObjectiveOnTheFlatFaceCells` | the ORDINARY STEP'S DESCENT INVARIANT. `kAscentTol` = 1e-6 relative (derived from the proximal penalty's own `O(primal_delta)` term with two decades of margin, and three to four decades beneath the 4.3e-03 E3 measured on the pre-NC2 arm) as the HARD bound, plus a SHAPE bound — at most 5 % of moving passes may rise past a 1e-9 floor. MEASURED: `R2_Cs_STEENBRB_qp2` 0 of 1790 moving passes above the floor, qp3 9 of 2828 (0.3 %, worst rise 1.857e-08), qp4 6 of 304 (2.0 %, worst 2.767e-08), and 0 above `kAscentTol` anywhere. Decidable at all only because the trace gained `obj_in` (below) |
| `QpE4ProximalEqp.TheSteenbrbRestorationLpQp1ReachesTheClarabelOptimum` | `kOptimal` in 232 minors with `f = -1378.22320088448` against the R2 evidence's own Clarabel optimum -1378.22319848 — **1.7e-12 relative**, where the base walk spent its whole 6600-minor cap and its final point was 2.2283e-04 above. Both algebras. The dump is ADDED (`tests/qp/fixtures/e2-d4/R2_Cs_STEENBRB_qp1.txt`, byte-identical to the R2 evidence's) |
| `QpE4ProximalEqp.TheYorknetRestorationQpIsDriftFreeAndOptimal` | `A0_YORKNET`/qp3: `kOptimal`, every equality row met to 1e-12 (measured 5.12e-14) where the base left them at 2.50e-01 with 7502 drift violations, no zero-motion exhaustion, both algebras. Dump ADDED |
| `QpE4ProximalEqp.TheAcopp57PrimaryQpCostsTwoMinorsOnBothAlgebras` | `kOptimal` in **2 minors on BOTH algebras** (base: 8 on border, 2 on refactorize) with the terminal stationarity ≤ 1e-12 |
| `QpE4ProximalEqp.TheSteenbrbRestorationQp2ExitsFiniteAndSaysWhy` | `STEENBRB_def`/qp2: `kNumericalError` at 547 minors — **NOT optimal, and declared as such** — with a FINITE objective, the rows met to 1.19e-15, and `eqp_residual_refusals >= 1`, which is the counter that says WHY. The cap trade that would make it optimal is named in the fixture. Dump ADDED |
| the three runaway fixtures | UNMODIFIED and green in both configurations |

**THE TRACE GAINED ONE FIELD**, `QpWalkMinorRecord::obj_in` (`qp_walk_trace.h`), the
objective at the pass's incoming iterate, set from an `#ifdef HVEN_TESTING` hook: one
mat-vec into storage the loop never reads, which is the allowance that header's IT IS
RECORD-ONLY paragraph states. Without it the descent invariant could only be argued —
the trace carried `alpha`, `p_inf` and `dx_inf` but nothing about the model the step
is supposed to reduce. The production library is untouched by it.

## 7. E4-17 — THE ACCOUNTING CONTRACT, STATED

`solver_counters.h` now carries it at **both** fields: at
`QpCounters::factorizations` (the active-set SEARCH's cost — every budget, the
hot-handle `qp_factorizations == 0` contract and the battery's cost tripwire are
written against it; the total a solve spent is
`factorizations + terminal_polish_solves`) and at `SqpCounters::factorizations` in the
same terms. `terminal_mu_refines`' own note records the ASYMMETRY explicitly — its
factorization IS in `factorizations`, the terminal polish's is not, the two are one
local solve at a terminal point each, and the charging differs for HISTORY rather than
for a reason: it predates the ruling, every pin that reads it was derived with it
there, and moving it would be a silent break of those pins for a cosmetic gain.
`docs/trace-schema-v0.md` states the contract in its own words beside the E4 group's
table.

**`eqp_row_drift_refusals` is a DECLARED ADDITIVE TRAILING KEY.** It is last in
`SqpCounters`, last in `HVEN_SQP_COUNTERS_FIELDS` and last in the trace table, so no
existing key's value or bytes move; in `QpCounters` it reads beside
`eqp_residual_refusals`, where its meaning sits. `kSqpCountersFieldCount` 56 → 57,
`sizeof(SqpCounters)` 904 → 912, the two golden trace lines re-derived ONCE by one key
at the end of the direct block, both size asserts and the schema doc with them.

**AND ONE PIECE OF THAT BOOKKEEPING WAS LEFT BEHIND AND IS FIXED HERE.** The round took
the direct block 56 → 57 and appended its history line in
`tests/drivers/test_callback.cpp` and `tests/drivers/test_threads.cpp`, but NOT in the
two places that state the count in PROSE: `solver_counters.h`'s "the table holds the 56
direct fields and the struct takes 58 initializers" with its own running history, and
`docs/trace-schema-v0.md` §5's "Shape: the 56 direct fields" with its chain. Both now
read **57 / 59** with fix round 2's line appended in each. The mechanical checks were
right throughout (`kAggregateArity<SqpCounters> == kSqpCountersFieldCount + 2` plus the
`offsetof`-monotone check), so this is a comment-and-prose change and nothing else.
**Its neutrality is PROVEN, not presumed:** both configurations were rebuilt from it and
`libhven.a` is **byte-identical** — `sha256` debug
`9ef9dfa66ad5b14a68af86d96ab37cc9fd9fd6af521a4329517aa7aa9abb9902`, release
`7e32f9e463067351f56324159c37183e8dc32eb4cd1923b755f991687142fcee`, unchanged across the
edit — so every corpus, U0, cap-search and census number in this report stands at the
shipped tree without re-measurement. Both suites and the long test were re-run at the
corrected tree regardless, and each agrees with its predecessor on every test's outcome,
test by test.

## 8. THE RE-TAKE 9 PREDICTION, PER CELL

**The on-row identity premise is available for NO cell, and the CONTROL set moves
too.** 102 of 102 corpus rows carry a column change on the E2-ON path and 51 of 51 on
the E2-OFF path; nothing is carried, and the standing rule (a re-take's
expected-identical set needs a per-cell measurement or an on-row proof) applies with
an empty carried list.

| TRAIN family | prediction, and what supports it |
|---|---|
| the 5 optimal cells | **status must hold; a move is a STOP.** None of the five is in the 51-dump corpus, so the corpus cannot speak for them: they need their own measurement. What the corpus DOES support is that no base-certified cell is lost anywhere it can see, on either algebra |
| `STEENBRB` / `Cs_STEENBRB` | **conversion expected.** Four of six `Cs_STEENBRB` cells convert and qp1 lands on the Clarabel optimum to 1.7e-12. `STEENBRB_def`/qp2 does NOT convert (declared, §5a): a TRAIN cell routing through that face may read `kNumericalError` with a finite, better objective and its rows met to 1e-15 |
| `YORKNET` / `A0_YORKNET` | **conversion expected, E2 ON, for ALL SIX `A0_YORKNET` cells and both `YORKNET_def` cells**, with the drift gone (`eqviol` 7502 → 0 on qp3). qp1 is this round's own conversion and its equality residual is 1.97e-13 against the base's 1.56e-13 — the same decade, where fix1 had it at 4.37e-01 |
| `STEENBRC`, `STEENBRE`, `STEENBRG` | **no prediction.** The corpus has no rows for them |
| `STEENBRD` | `Hj_STEENBRD`/qp4 converts (`num -> opt`, `stat` 5.76e-01 → 4.40e-17); qp1 and qp9 stay `kNumericalError` at fewer minors. E2-OFF, qp1 is lost (`cap -> num`) |
| `DALLASM` | **mixed, and the certificates are now VERIFIED.** `DALLASM_f4` qp2/qp3 convert (`cap -> opt`); `DALLASM_def` qp2 (border) and qp1 (refactorize) report a CERTIFIED `kInfeasible` that three oracles agree with (§1). A TRAIN cell whose subproblem is one of those will see a TRUE `kInfeasible` propagate, and the driver's own elastic/restoration route is what decides what that means for the NLP |
| `NET3` | **no prediction.** No `NET3` row in the corpus |
| `ACOPP57`-carrying cells | **improvement expected on the default (border) algebra**: four certified cells go 8 minors → 2 with the terminal stationarity 1.25e-07 → 9.7e-15 |
| `ACOPR57`-carrying cells | **improvement expected**: qp0's objective moves to the oracle's (432.660942518 → 432.674974173) with its equality residual 6.14e-07 → 9.77e-15, and qp3 converts (`cap -> opt 2`) |
| the CONTROL set (E2 OFF) | **DECLARED MOVING, unchanged from fix round 1** — RE-VERIFIED at the shipped engine from the final off rows (`statuscmp.py border fix1off h1off k1off`): 25 of 51 cells move in status or minors, 51 of 51 in some column, and three certified cells are lost — **`A0_YORKNET`/qp6** (opt 941 → cap 7680), **`Cs_STEENBRB`/qp2** (opt 237 → cap 6600), **`SAWPATH_def`/qp2** (opt 182 → cap 6830). This round moved the off path by NOTHING: `k1off` and `h1off` agree on every status and minor count |

## 9. REGISTERED

1. **`SqpDriverElasticSeed.T6bTheBandsHIGHRungsReachTheOptimum` — the one reading in
   this round that regresses, disclosed rather than relabelled.** At
   `(rho = 1e+08, dual_mu = 1e-04)` E4's walk reaches that cell's locked vertex by a
   different route, R-B's unlock does NOT fire (`dep_unlocks` 0 against >= 1), and the
   walk reports `kOptimal` at `x_orig = (10, 10)` with the slack shut and the elastic
   row off by **15** — the same reading the fixture's banner records at `23e884a`,
   i.e. **CONDITION (b)'s BLIND SPOT is observable again**. It is a blind spot and not
   a new defect: `violation_is_structural`'s condition (b) reads 15 against
   `kStructuralResidualFrac * dual_mu * |lambda|` = `1e-04 * 1e+08` = 1e+04 and calls
   it noise. **The driver never places a rung there** — T5's bound is
   `kElasticRhoDualMuSafety / dual_mu` under the `kElasticRhoInit` floor, four decades
   below this rho at the shipped `dual_mu`, and
   `T6bCoversTheBandWhereThePlacementMarginIsUnachievable` measures what the driver
   actually gets (the floor rung, row residual 1.6e-06, closed) and is green. **The
   repair is engine-level: condition (b) needs a cap on what `dual_mu * |lambda|` may
   absorb, the way `kInfeasibilityAbsorbTol` caps `row_tolerance`.** Not this round's.
2. **THREE COVERAGE GAPS THIS ROUND OPENED**, each declared in its fixture:
   (a) the WATCHDOG CARRY of the W1 complementarity gate has no fixture — the recipe
   that made a long post-refusal solve (row `8 * tr` away, TR pinned) now converges in
   ONE major inside `kkt_tol`, because the polish removes the footprint that used to
   keep stationarity above it; the INGEST half of the gate keeps its own two arms and
   both are green. (b) Restoration route (3), the RADIUS FLOOR as a trigger, has no
   fixture: on its model the E4 step is ACCEPTED and the elastic exhaustion raises the
   request on the next major, so the radius never shrinks and no floor value can be
   reached. (c) `IpqpDispatch.ASuccessfulSubproblemResetsTheConsecutiveEscapeTally`'s
   RESET is no longer witnessed: HS15 escapes once, not twice. (d) the EQUALITY-DRIFT
   absence on `A0_YORKNET`/qp3 is not observed under Debug's arithmetic, because that
   fixture is skipped there at 2414 s (§5h).
2b. **FOUR CONFIG SPLITS RETIRED AND FOUR OPENED**, all in §5h's table. The four
   retired ones go the good direction: Debug's restoration sub-solve no longer sticks
   (two `SqpDriverCertifiedFallback` fixtures), Debug certifies `R2_Cs_STEENBRB_qp4`
   like Release, and `IpqpAcceptanceCensus`'s `armed_peak` now agrees across
   configurations. The four opened are HS26's two columns, the excursion budget, the
   cancellation threshold and `evals_values`. One frozen-artifact GATE widened, the R6
   row's Debug one, 1e-5 -> 2e-5 against a measured 1.076e-5.
3. **THE CANCELLATION WINDOW IS DOWN TO 11 % IN RELEASE AND 1.9 % IN DEBUG.**
   `SqpDriverRestorationProximity.TheProxyWouldCertifyWhereTheExactResidualRefuses`
   needs `proxy < kkt_tol < exact residual`; MEASURED now 4.3934000928e-07 and
   5.3620559926e-07 against 4.5482e-07 and 8.1287e-07 before, so `kCancellationKktTol`
   is re-derived to the new midpoint 4.88e-07 and each premise has 11 % of margin where
   it had 28 % and 29 %. The window narrowed because the exact residual IMPROVED.
   **AND DEBUG'S WINDOW IS 3.7 % WIDE** (proxy 4.3933982708e-07, exact
   4.5566300666e-07), so its threshold -- now config-split at 4.475e-07 -- has 1.9 % of
   margin. Both readings are deterministic per build and a 2 % drift fails the test,
   which is what the fixture is for; Debug's will close first, and when it does the
   fixture needs a new POINT rather than a new threshold.
4. **A CAP THAT SCALES WITH THE FACE'S CONDITIONING** would keep both
   `ACOPR57_def`/qp4's certificate and F3n50's pinned cost (§4a). Three fixed values
   were measured; the shape was not searched.
5. **`QpE2WorkingSet.AGreyPinOnYorknetThatIsIndependentStaysIndependent` COSTS
   2681.90 s IN DEBUG** (37.35 s Release) and now carries a DECLARED `GTEST_SKIP` there
   with the cost in the message. The cost is essentially pre-existing (its Release time
   moved 1.4 s across the whole of E4). It wants its dump split, not just the test.
6. **THE TERMINAL POLISH'S FACTORIZATION IS NOT IN `factorizations`** — ruling E4-17,
   now stated at both fields (§7). Unchanged; carried forward for the harness lane's
   schema event at re-take 9.
7. **`kEngineStepTieTol` deciding a weakly-active bound per algebra at 1e-17**
   (fix1's item 8) is still not observable: both `QpEngineBorder` fixtures that caught
   it pass. The question stands; its witness is gone.
8. **THE INTRINSIC O(delta) CLEANUP PASS** (fix1's registered item) is unchanged.
9. **THE WARM BATTERY'S 13 RE-DERIVED PINS** are a real +1.0 % to +50 % cost with a
   measured cause and a cap search behind it (§4a). Accepted here as declared, not as
   repaired.

## 10. WHAT I COULD NOT DETERMINE

- **Whether `ACOPR57_def`/qp0's `HVEN_TESTING`-vs-production divergence has siblings.**
  The one I found was decided by a single ulp inside
  `adopt_polished_candidate` and the rounding margin closes it, but I did not sweep the
  corpus for other quantities whose acceptance is a tie-break. The general shape — a
  decision whose predicate is an exact comparison of two computed residuals — is worth
  a sweep.
- **Why `A0_YORKNET`/qp3 costs 6 more minors while qp4 and qp6 cost 12 fewer** under
  the same change. The residuals improve on all four; the minor counts move both ways
  and I attributed them to the route rather than to a mechanism.
- **Whether `F3n1000`'s and `F3stress`'s 1–5 extra minors are exactly the terminal
  polish's.** The cap search excludes the polish step cap (they do not return to their
  pins at any cap), and the same +1 shape appears on five U0 cells where fix1
  attributed it to the terminal polish — but I did not instrument these cells to see
  the pass.
- **Whether the Debug configuration's own numbers for the 24 renamed fixtures match
  Release's.** The Debug suite's PASS/FAIL is measured (§5h-DEBUG); the individual
  values inside those fixtures are Release-measured, and four of them carry
  `#ifdef NDEBUG` splits from earlier rounds that this round did not revisit.
- **Whether condition (b)'s blind spot (§9.1) is reachable at any `(rho, dual_mu)`
  pair the DRIVER can place.** T5's bound argues not and
  `T6bCoversTheBandWhereThePlacementMarginIsUnachievable` measures the floor rung, but
  I did not sweep the placement rule's whole reachable set.

## 11. EVIDENCE INDEX (`.scratch/e4/`)

| what | where |
|---|---|
| the DALLASM verification | `cert/certcheck.txt`, `cert/rawfeas.txt`, `cert/DALLASM_def_qp{1,2}.oracle.{txt,json}`, `cert/*.cert`; tools `tools/{certcheck,rawfeas,qp_oracle,d4load}.py` |
| the corpus at the final engine, both algebras, E2 on and off | `corpus/k1-*`, `corpus/k1off-*` (base rows `corpus/fix1*`, `f2226c28`'s `corpus/h1*`) |
| the all-column comparisons | `logs2/rowcmp-k1-vs-base.txt`, `logs2/rowcmp-k1-vs-h1.txt`; `tools/rowcmp.py`, `statuscmp.py` |
| the cap search | `capsearch.sh`, `logs2/cap1e-{6,5,4}-{wsb,key}.log` |
| the Release suite, SHIPPED tree | `logs2/ctest-release-final2.log` (0 / 2870, 113.87 s); the identical earlier run `logs2/ctest-release-final.log` (0 / 2870, 114.13 s); the intermediate heads `logs/ctest-release-i50.log` (0 / 2867), `i21`, `i23`, `i24`, `i27`, `i45`, `i47` |
| the Debug suite, SHIPPED tree | `logs2/ctest-debug-final2.log` (0 / 2869, 634.05 s) + `logs2/ctest-debug-final2-long.log` (622.29 s); the identical earlier run `logs2/ctest-debug-final.log` + `-final-long.log` (622.71 s); the intermediate `logs2/ctest-debug-j.log`, `-j-long.log` |
| the byte-identical `libhven.a` across the prose fix (§7) | `logs2/rebuild-{debug,release}.log`; the two `sha256` in `STATE.md`'s continuation entry |
| the runner for the shipped battery | `final3.sh`, `logs2/final3.log` (PGREP_EMPTY / FOREGROUND_START / FOREGROUND_END per leg) |
| the four-head corpus table (§5ab) | `statuscmp.py border fix1 e4 h1 k1`, and the same on `refactorize` |
| U0 | `logs2/i51-u0.log`, `u0/comparison.txt`, `u0/head-{walk,ssn,ipm}.csv` |
| census + HS + B1Gate at the final engine | `logs2/i51-key.log`, `logs2/i25-key.log` |
| the walk-seam binary (90 → 95 tests, all green) | `logs2/i{16,17,20,25,28,49}.log` |
| the diagnoses | `logs2/i6-indef.log` (the flat-face trace), `logs2/i{26,30,34,36,38,41,43}.log` (the per-fixture failure messages) |
| the REMOVED/ADDED name sets, derived from the diff | `git diff c864b8a6 -- 'tests/**/*.cpp' | grep -E '^[-+]TEST\('` |
| the runners | `leg.sh`, `build.sh`, `build_tools.sh`, `corpus.sh`, `corp_fix2.sh`, `capsearch.sh`, `dbgfix2.sh`, `suite2.sh`, `g4.sh`, `i*.sh` |
| running state | `STATE.md` |
| the commit message | `commit-msg-fix2.txt` |
