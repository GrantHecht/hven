<!-- M7 W2 E4 (the proximal EQP) -- the implementer report for fix round 1, copied VERBATIM from
     .superpowers/m7-w2-e4-fix1-report.md. Nothing below this comment is edited: the three
     bytes after this preamble are that file byte for byte (cmp-checked). -->

# M7 W2 E4 — FIX ROUND 1: the terminal cleanup, the border path, the suite, the fixtures

Implementer hand-back. Worktree `agent-a69f46e11796560b9`, branch `e4`, ONE squashed
commit **`f2226c28`** on `c864b8a6`, REPLACING `c46485d8`. Tree clean, both
configurations build, no attribution trailer, explicit pathspecs, 30 files.
Builds `.scratch/nc2-build-{debug,release}` (clang, incremental). Evidence
`.scratch/e4/`; `.scratch/e3/`, `.scratch/nc2/`, `.scratch/nc2-fix1/` read only.
Every solver leg through `.scratch/e4/leg.sh`: `flock /tmp/box-build.lock` with NO
timeout and never under `timeout`, an in-lock `pgrep -a -x hven_bench_cell` emitting
`PGREP_EMPTY`, then `FOREGROUND_START` / `FOREGROUND_END <rc>`; `set -o pipefail`;
`MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2` on every solver cell; ONE solver at
a time; `ctest -j4` under `setsid`. IPARM untouched, `notices/` untouched, SNOPT
firewall: nothing read. Papers only (Friedlander & Orban 2012; Gill & Wong 2015).

SIGNOFF: M7-W2-E4-FIX1-REPORT

## 0. THE ANSWER, IN ONE PARAGRAPH

**Blocker (1), the B-1 family, is CLOSED. Blocker (2), `ACOPP57_def` on the border
path, is CLOSED. Blocker (3), the suite, is REDUCED from 83 failures to 63 in Release (69
in Debug, the two agreeing on 61) and is not closed.** One mechanism did the first two: the proximal answer carries an
`O(delta * ||x* - x_k||)` bias away from the face's exact KKT point, and that bias is a
real error in the answer even though `delta` is a real term of the model. The
**UNREGULARIZED POLISH** takes it back out — at most two refinement steps against the
face's OWN exact system, on the factor the solve already paid for, each kept only while
that residual strictly falls AND the step is small enough to be a polish — and the
**TERMINAL POLISH** then re-solves a certified point mu-free on its final working set.
`HsSweepCorpus.Corpus`'s B-1 complementarity assertion, which read 0.177 under
`c46485d8`, now passes; four of the six `B1Gate` cells pass; `ACOPP57_def` qp0–qp3 read
`kOptimal` in **2 minors on BOTH algebras** with that face's bordered terminal
stationarity 1.25e−07 → 9.69e−15, so the 1795-minor wander has no successor. On the
51-dump corpus the round converts **fifteen** cells on each algebra and loses **no**
base-certified cell on either; the T7 feasible census holds 120/120; U0's 43 differences
over three engines are every one strictly better but for one last-digit `ipm` move, with
the walk's `kkt_stationarity` 6.30e−13 → 4.16e−20 on the F7 bound family. What is left
is named in §5 with numbers: 63 of 2862 Release tests and 69 of 2861 Debug ones (46
inherited from `c46485d8`, 17 this round's own, 37 of its 83 fixed), of which about 25 are
the pathology-pinning fixtures whose declared re-derivation this round did not attempt;
two `DALLASM` cells that now read a CERTIFIED `kInfeasible` where the base read
`kNumericalError`; the warm-start battery's 13 pinned cells 4–50 % more minors; one Debug
test at 2682 s that this round is the first to measure and did not cause; and E4-7's
fixtures, not added.

## 1. E4-11 — THE TERMINAL CLEANUP. **DELIVERED, in two pieces, and the second is the
one the brief did not ask for.**

### 1a. The shared primitive (`eqp_solve.h`, `bordered_eqp.h`)

    polish_eqp_core(asm_, rhs0, opts, kkt, n_free, n_rows, max_steps, y)      // eliminated
    polish_bordered_unregularized(K0, var_count, border_v, border_d, schur,
                                  rhs, x_k, opts, max_steps, sol)            // border

Refinement against the residual of the system with **BOTH** regularizations backed out —
the dual `mu` AND the primal `delta` — i.e. the face's own exact KKT system

    H_FF x_F + Ae_F' le + Aw_F' lw = -g_F - hess_shift,   Ae x = be,  Aw x = bw

computed without ever forming it (`K0 = K - diag(reg)`, `reg = [+primal_delta on the free
rows; -dual_mu on the constraint rows]`), on the ALREADY-FACTORIZED factor, at most
`kEqpPolishSteps = 2` steps, each KEPT only if

  (i) the step's inf-norm is at most `kEqpPolishStepCap * max(1, ||y||inf)`, and
  (ii) the residual's inf-norm STRICTLY FALLS.

`(ii)` alone is the safety argument on a FLAT face: there the exact system has no answer
along the null space, the step does not reduce the null-space component of the residual,
and the proximal answer stands untouched. `(i)` is what separates a POLISH from a
DIFFERENT ANSWER on a nearly dependent face, and it is **measured, not chosen** — the
header carries this table:

| cap | base-certified corpus cells lost | what it recovers |
|---|---|---|
| none | **3** (`ACOPR57_def` qp0, qp4; `Cs_STEENBRB`/qp6) | `ACOPP57_def` qp0–qp3 on the border path |
| 1e−4 | **1** (`ACOPR57_def`/qp4: opt/1736 → num/2564) | + `ACOPR57_def`/qp0, `Cs_STEENBRB`/qp6 back |
| **1e−6** | **0** | + `ACOPR57_def`/qp4 back, + `YORKNET_def`/qp0 converts |

It is the same mechanism read from both ends: the uncapped step that repairs
`ACOPP57_def`'s bordered face is the same KIND of step that derails `ACOPR57_def`'s, and
only the SIZE separates them. 1e−6 is two decades above the bias being removed
(`O(primal_delta)` relative at the defaults) and four beneath a 1/delta-sized move.

The primitive runs **inside `solve_eqp` and `solve_bordered_eqp`**, so it reaches every
EQP answer on both algebras, and `QpCounters::eqp_polish_steps` counts the steps kept —
which is also the measurement of how often the face's exact system has an answer at all
(0 on a flat face). `refine_terminal_mu_free` (E2 fix4) was refactored onto the same
loop, byte-identically. `refine_on_face` needs nothing of its own any more: its promise
of THE FACE'S EXACT ANSWER is what the polish now delivers, and
`QpEngineFaceRefine.AnExactFaceSolveLandsOnTheAnalyticEqpAnswer` and
`ATrustRegionPinIsHonouredAsPartOfTheFaceAndPricesNothing` pass at their UNCHANGED
tolerances (1e−12 / 1e−9) — they failed at `c46485d8` with `out.x(0) = 1.2500001e-09`,
which is exactly the bias.

### 1b. The terminal polish (`qp_engine.cpp`, at the walk's ONE certified exit)

Placed in the `else` branch that decides `kOptimal` — which the 4b ladder's certified
rung reaches too, by falling through the stationarity gate — BEFORE the certificate's
bound half and the certificate seam, so what is gated is what is exported. It moves no
minor count: it runs after every drop, probe and gate has had its say.

TWO STAGES.
- **Stage 1, free:** the pass's own EQP answer, which `solve_eqp` has already polished.
  Admissible here and not in the loop because the acceptance test reads every row and
  every bound of the problem — which is what the ratio test the walk skips protects — and
  because nothing happens afterwards. MEASURED: it adopts rarely (1 of 59 adoptions on
  the HS battery under the wide gate), and it costs nothing.
- **Stage 2, one factorization:** the face re-solved with the dual regularization out of
  the SYSTEM rather than only out of the refinement target. What stage 1 cannot reach is
  a point whose working-row slack IS `dual_mu*|lambda|` because the system that produced
  it had `-dual_mu` on those rows: on a nearly dependent face (`lambda ~ 1e+03..1e+05`
  there) `dual_mu * ||K^{-1}||` is not a contraction, so no refinement of that answer
  against the mu-free system converges, and only a re-solve reaches it. That slack,
  priced by `lambda` again, is the B-1 term.

ACCEPTANCE (`adopt_polished_candidate`), shared by both stages and written as a chain of
`!(good)` tests so a NaN rejects: finite; feasibility KEPT (no row and no bound outside
`feas_tol * row_scale` that the incumbent met, and nothing worse than the incumbent); no
one of the three KKT residuals worse; at least one strictly better. The three are
`detail::terminal_kkt`'s, each RELATIVE to its own scale: free-block stationarity over
its own scale, the worst row/bound violation over that row's scale, and the worst
`|slack| * |multiplier|` product over the working rows and pinned bounds. A rejection
leaves `x` and every multiplier exactly as the walk had them, so the polish is a cost and
never a risk.

THE ENTRY TEST (`polish_footprint_open`) is a COST gate, two disjuncts, both calibrated:
- `complementarity > kTerminalPolishComplFactor * feas_tol`, factor **100** (1e−07 at the
  defaults, one decade beneath the 1e−06 `kkt_tol` both drivers default to). MEASURED on
  the 27-problem HS battery at `kSchurBorder`: at factor 1 the gate fired at nearly every
  certified exit for a 40 % adoption rate and 145 stage-2 solves; at 100, one. Every
  fixture the wide gate fixed is green at 100.
- `stationarity > kTerminalPolishStatFactor * opt_tol`, factor **1e−3**. This is E4-11's
  U0 target and the one quantity the EQP-site polish cannot reach: the KKT-point test
  stops at `opt_tol * scale`, correctly, where the walk it replaced ground four decades
  further. Adding it moved the Release suite's failure set by ZERO and took U0's walk
  differences from 29 to 17, all of them then strictly better.
- A third disjunct was MEASURED AND REMOVED: `mu_footprint_open` is this product's square
  root and fires two decades wider for nothing a consumer sees.

**THE ONE ACCOUNTING DEPARTURE, and it is deliberate.** Stage 2's factorization is
charged to `terminal_polish_solves` and **NOT** to `factorizations`, unlike
`terminal_mu_refines`, whose own note says it is. `factorizations` is the active-set
SEARCH's cost: every budget is written against it, the hot-handle contract is stated as
`qp_factorizations == 0` on a reused K0 (`WarmStart`'s own pins), and the HS battery's
cost tripwire reads it. This solve searches nothing and reuses nothing. MEASURED: with it
charged there, EIGHT `WarmStart` tests fail because a zero-factorization hot reuse reports
one — and re-pinning them to 1 would destroy the pin's ability to see a LOST hot reuse,
which is a correctness fixture re-pinned to hide a break. Nothing is hidden: the count is
one per solve entered, asserted, and aggregated into `SqpCounters`; the total is
`factorizations + terminal_polish_solves`. **REGISTERED for the settler** (§7.1).

### 1c. E4-11's three targets, measured

| target | result |
|---|---|
| the 6 `B1Gate` cells + `HsSweepCorpus.Corpus` GREEN unmodified | **`HsSweepCorpus.Corpus`'s B-1 assertion (line 938) PASSES** — it read `complementarity = 0.177` at `c46485d8` and its ONLY remaining failure was a `total_minors` pin, re-derived here, so the test is GREEN. Of the 6 `B1Gate` cells, **4 are GREEN unmodified** (`ReviewSphereAgreesWithItsAnalyticPathAtEveryStepSize`, `ReviewSphereContinuationSweepTracksTheTruePath`, `SeededIngestCarriesTheB1Repair`, `TheScaledStalePriceIsRefusedAtWarmToo`); 2 still fail, and both for the OPPOSITE reason (§5c) |
| U0 walk terminal `kkt_stationarity` back to base decades, or every move strictly better | **every move strictly better.** 17 walk differences: `kkt_stationarity` 6.30e−13 → 4.16e−20, 1.26e−12 → 1.84e−20, 1.84e−13 → 1.93e−20, 3.15e−13 → 6.11e−20, 1.26e−13 → 1.91e−19; `kkt_residual` and `kkt_primal` better on the same cells; `qp_minors` 2 → 3 on five cells. The 4-decade DEGRADATION `c46485d8` carried on the F7 `bound_corrupted`/`bound_warm` cells (9.19e−14 → 9.20e−10) is GONE |
| the F7 family's 51 walk differences explained one by one | **17, not 51**, and the 34 that went away are the degradations. The 17: 12 residual columns, all strictly better and named above; 5 `qp_minors` 2 → 3, which is the terminal polish's better point costing the walk one more pass to reach |

## 2. E4-12 — THE BORDER PATH ON `ACOPP57_def`. **CLOSED by measurement, and it is
answer (a): parity, reached by the polish's own refinement.**

Neither of the brief's two shapes had to be built as a new mechanism. The diagnosis in
`c46485d8`'s §3f was right and complete — that face's BORDERED answer is
1.25e−07-stationary against the eliminated path's 1.02e−14, and the proximal step turns
that into `p ~ 12.5` — and the polish is precisely a bounded, counted refinement of the
bordered answer against the face's own exact system. It reaches parity:

| cell, BORDER path | base (NC2 fix1) | `c46485d8` | **`f2226c28`** |
|---|---|---|---|
| `ACOPP57_def`/qp0 | opt, 8 minors, `stat` 1.25e−07 | cap, 1795 | **opt, 2 minors, `stat` 9.69e−15, `eq` 4.03e−07 → 1.37e−14** |
| `ACOPP57_def`/qp1–qp3 | opt, 8 | cap, 1795 | **opt, 2** |
| the same four, REFACTORIZE | opt, 2 | opt, 2 | **opt, 2** |

**Both algebras agree on status AND minors on that dump (opt / 2), which they did not
before this round** (8 on border, 2 on refactorize at the base). No 1795-minor wander is
reachable: the answer the stationarity test is asked about is now the face's exact KKT
point. `terminal_attainable_exits` was NOT added — shape (b) was not needed and an
unused counter is a liability. The counted cost is in `eqp_polish_steps` (0 on that dump's
flat faces, 1–2 elsewhere) and `refuse`/`residiters` read 0 there, so the residual check
is still not involved.

## 3. E4-13 — THE SUITE. **PARTIAL: 83 → 63 of 2862, and (e) is DONE.**

The denominator is **2862**, not 2863: `c46485d8`'s reported 2863 counted its own
`E4Scratch.Dump`, which its commit removed. No test is added or removed by this round.

### 3a/3b. The declared fixture event — FOURTEEN counter pins re-derived, each with an
in-fixture attribution naming what it pinned, what it reads now, and why

| fixture | old | new | attribution (in the fixture, in full) |
|---|---|---|---|
| `HsBattery.RefinementStepCountersOnBorderRepro` | `border_refine_steps` 10 | **6** | `bordered_regularization` no longer reports the primal delta, so the iterated loop's footprint rule reads a different threshold; answer, status and `minor_iters` unchanged, and the Task-11b inequality still holds at 6 > 4 |
| `HsBattery.CrashBasis…` | `minors_off/on` 802/799 | **800/797** | the EQP-site polish removes the extra pass per face the bias used to cost; two cells save one minor each; `facts_off/on` UNMOVED (the polish's solve is not in `factorizations`) |
| `ScaleF7.WideWindowWalkIsChurnNotDegeneracy` | 278, 526 | **280, 527** | the terminal polish's better point costs the walk extra passes on this window; every other column of both rows unmoved, the churn reading unchanged |
| `F7ColdScaleSmoke.EmptyWindowColdSolveAtTenThousandVariables` | `qp_minor_iters` 2 | **3** | the terminal polish re-solves the certified point mu-free; `factorizations` unmoved |
| `PredictorRatioTest.InheritsTheTrueActivitySet…` | `qp_minor_iters` 2 | **3** | same; majors and factorizations unmoved |
| `IpqpAcceptanceCensus.AnArmedRunIsNeverMistakenForAStall…` | `armed_peak` 85 | **72** | the polish shortens the walks the peak is taken over; the claim is the comparison, not the peak |
| `QpEngineStructuralViolation.T7ACertifiedInfeasibleExit…` | `latched_border.factorizations` 3 | **4** | one extra eliminated factorization on the latched route from the mu-free inertia re-read the polished point reaches; `verdict_refine_steps == 9` on both twins and the structural claim unchanged |
| `WarmStart.BudgetModeOffMatchesMaxIterExactly` | `f` 0.0024386526357234916 | **0.0024386526444118797** | HS26 is rank-deficient at its solution; the polish takes delta out of the truncated majors' answers. The new MKL value agrees with the Accelerate pin to 2.1e−12, CLOSER than the 8.69e−12 the comment records for that pair |
| `HsSweepCorpus.Corpus` | `total_minors` 3365 | **3340** | the same event that CLOSED this test's B-1 assertion; −25 minors, majors/statuses/KKT columns hold, 22/3340 = 0.66 % |
| `SqpDriverWarmStart.WarmSeedingKeepsLateSubproblemsCheap` | `<= 2` | **`<= 3`** | trial 9's warm-seeded subproblem spends one more pass reaching the better point; "cheap, and cheaper than cold" holds at 3 |
| `WarmStartBattery.CrossoverChain…` | 1e−5 row `cold_minors` 49 | **48** | the polish removes that cold arm's extra pass; **the 1e−3 row is UNMOVED, which is the control** |
| `QpE2WorkingSet.ADeclinedRideIsExchanged…` qp3–qp6 | `off_minors` 596/192/191/184 | **586/197/181/180** | the option-off walk is reached by every EQP solve too (E4-5), so the polish moves it; the objective pin and the `kOptimal` status — what N3 is about — hold at every one |
| `SqpCountersFieldTables.EachTableEnumeratesItsWholeStruct` | 50 | **56** | the six-field E4 group |
| the two `JsonLinesTraceSink` golden lines + `docs/trace-schema-v0.md` | 50 keys | **56 keys** | re-derived ONCE, by six keys, appended after `restoration_sub_status` so no pre-existing key's bytes move; the two size asserts (`test_callback.cpp` 856 → 904, `test_threads.cpp` 50 → 56) with them |

**NO correctness fixture was re-pinned.** Two were FIXED instead of re-pinned
(`QpEngineFaceRefine` ×2, at unchanged tolerances, §1a); two hand-rolled border tests
(`QpEngineBorder.BorderPinEquivalence`, `BorderOps.PinVariableMatchesEliminated`) had
their MIRROR of the engine's refinement updated to include the border polish — the
1e−9 assertion is unchanged, and `border_test_utils.h` says why.

### 3c. `eqp_refine_iters` / `eqp_residual_refusals` / the polish counters into
`SqpCounters`. **DONE — this was E4-13(e).**

Six fields appended to `SqpCounters` in declaration order after `restoration_sub_status`:
`eqp_refine_iters`, `eqp_residual_refusals`, `eqp_polish_steps`,
`terminal_polish_solves`, `terminal_polish_rejected`, `terminal_polish_adopted`. Each
carries its `QpCounters` meaning unchanged and its own note. Folded at **nine** sites: the
five block folds in `sqp_solver.cpp` (elastic rung, restoration sub-solve, the major's own
solve, the cap retry, the SOC re-solve) and the four `refine_on_face` charge sites (the
refused-face helper and the SSN, IPQP and warm-grade arms, where the E4-level fields are
read BEFORE the swap overwrites `refined.counters` and charged after it, exactly as
`refine_facts`/`refine_steps` already were). `HVEN_SQP_COUNTERS_FIELDS`, the arity
`static_assert`, the offset-order `static_assert`, both test-side size assertions, the
field-count test, the two golden trace lines and the schema doc all move with them.
`sizeof(SqpCounters)` 856 → 904 (six `Index`, the first landing in the 4-byte enum's
padding, so no new padding and every pre-existing offset unmoved).

### 3d. The Debug suite at the final head. **RUN, AND IT AGREES WITH RELEASE.**

At the committed head `f2226c28`, `ctest -j4` under `setsid`, inside the lock, with
`Continuation.ProbeBudgetBoundsAFailingProposal` EXCLUDED and then run ALONE afterwards.
`logs/ctest-debug-h1.log`, `logs/ctest-debug-h1-long.log`.

**69 failures out of 2861** (two more tests are `Not Run (Disabled)` and outside the
denominator). The two suites agree on **61**; 8 fail only in Debug and 2 only in Release:

| direction | tests |
|---|---|
| Debug only (8) | `HsSweepCorpus.Corpus` (Debug's arithmetic takes a different, equally optimal walk, so its `total_minors` pin is a Release-only claim — the fixture says so itself), `IpqpAcceptanceCensus.AnArmedRunIsNeverMistakenForAStall…`, `QpNc2FlatFace.TheSteenbrbRestorationLpQp4NeverAscendsTheSlope{Border,Refactorize}`, `SqpDriverQpFailure.RepeatedQpMaxIterStillPropagates`, `SqpDriverCertifiedFallback.T7F1TheSameInconsistentLinearizationInAllThreeModes`, `SqpDriverRestorationExcursion.TheSubStatusDiscriminatesAtOneParentStatus`, `SqpDriverRestorationSeed.TheGuardAndThePhaseRunOnTheCallerScale` |
| Release only (2) | `QpEngineStructuralViolation.T6bTheInequalityBranchClosesWithItsResidueOnTheSafeSide` (an 8.88e−16 residue on the wrong side of a `<= 0` claim — a rounding sign, so backend- and config-sensitive by construction), `SqpDriverCertifiedFallback.T7TheEscapeBranchTernaryCONSUMESAnAttachedReportAtTheDriver` |

**EVERY DEBUG TEST'S WALL-CLOCK, and there are TWO above 600 s, one of them a finding
this round did not cause:**

| test | Debug | Release | reading |
|---|---|---|---|
| `QpE2WorkingSet.AGreyPinOnYorknetThatIsIndependentStaysIndependent` | **2681.90 s** | 37.35 s (35.94 s at `c46485d8`) | **> 600 s and NOT this round's**: its Release time moves 1.4 s, so at the base's 35.94 s and this tree's ~72x Debug factor it was already ~2590 s. It is the slowest test in the Debug suite by a factor of nine and it was never measured before, because no round had run the WHOLE Debug suite. REGISTERED (§7.10) |
| `Continuation.ProbeBudgetBoundsAFailingProposal` | **629.33 s**, run ALONE and detached, PASSED | 12 s | the known one, and it is barely over: 629 s |
| `QpNc2FlatFace.TheSteenbrbRestorationLpQp3NeverAscendsTheSlopeBorder` | 304.98 s | — | `c46485d8` recorded 285–293 s for this pair; the margin is intact |
| `QpNc2FlatFace.TheSteenbrbRestorationLpQp4NeverAscendsTheSlopeBorder` | 294.39 s | — | |
| `Continuation.GrowthSuspensionSkipsTheStepAfterAFailure` | 263.03 s | — | |
| `QpE2WorkingSet.AGreyReReadSolvesTheAcopr57RestorationQp` | 241.93 s | — | |
| `QpE2WorkingSet.ADeclinedRideIsExchangedOnSteenbrbRestorationQp3` | 180.90 s | — | the next slowest; everything else is beneath it |

So E4-14's "every Debug test < 600 s" does NOT hold on this tree, and the violation it
does not hold for is one this round inherited and is the first to see.

## 4. E4-14 — THE FIXTURES OF E4-7. **NOT ADDED**, and the reason is the same as
`c46485d8`'s: adding new pins under an event whose ~25 existing pins still need their
declared re-derivation would put them underneath it rather than on top. What each would
assert, with the value MEASURED at `f2226c28` (border path):

| E4-7 item | value now | note |
|---|---|---|
| the ordinary-step DESCENT INVARIANT on a flat face | measurable, not built | E3 §3f's tolerance form: restrict to passes with `active_res <= delta*tol` and assert `g'p <= kEngineDenomTol*||g||*||p||` there |
| `R2_Cs_STEENBRB_qp1` at the Clarabel oracle | **available: opt, 232 minors, `f = -1378.22320088`, `stat` 3.93e−07 → 1.08e−16, `comp` 8.66e−05 → 0** | the dump to add under `tests/qp/fixtures/e2-d4/` is `…/dumps/Cs_STEENBRB/qp1.txt` |
| `A0_YORKNET_qp3` drift-free `kOptimal` | **available, and BETTER than the brief's read value: opt, 683 minors, `eq` 2.50e−01 → 8.86e−14, `eq_drift_violations` 7502 → 0** (it was 12 at `c46485d8`), `stat` 2.14e−02 → 1.57e−10, `refuse` 3, `residiters` 4 | the "12 is a READ value" caveat is retired: it is 0 |
| `ACOPP57_def` qp0 at 2 minors both algebras | **available: opt/2 on border AND refactorize** (§2) | |
| `STEENBRB_def` qp2 finite and optimal, `eqp_residual_refusals` reported | **HALF: finite, `f = -990729858499` (base `-990591855583`), `eq` 6.55e−07 → 1.19e−15, but `kNumericalError` at 547 minors, NOT optimal.** `c46485d8` had it `kOptimal` at 392; the step cap gives that conversion up (§5a) | |
| the three runaway fixtures unmodified | **PASS**, Debug and Release | unchanged in the tree |
| every Debug test < 600 s | not verified at the final head | §3d, §8 |

## 5. THE BATTERY IN FULL, every mover declared

### 5a. The 51-dump corpus, E2 ON, both algebras, all columns
`.scratch/e4/corpus/h1-*`, compared against NC2 fix1's own rows by
`.scratch/e4/tools/rowcmp.py` (all columns) and `.scratch/e4/statuscmp.py`
(status/minors). **51 of 51 rows move on each algebra**; 42 change status or minors on
border, 38 on refactorize.

**THE CONVERSIONS TO `kOptimal` — FIFTEEN on each algebra** (`c46485d8` had 13 / 12):

| cell | base | `f2226c28` |
|---|---|---|
| `A0_YORKNET` qp2, qp3, qp4, qp5, qp6 | cap 7680 | **opt** 703/676/681/660/698 → 718/683/695/645/712 |
| `A0_YORKNET`/qp3 in detail | cap, `eq` 2.50e−01, `stat` 2.14e−02, `eqviol` 7502 | **opt 683, `eq` 8.86e−14, `stat` 1.57e−10, `eqviol` 0, `f` −10552.28 → −10933.567** |
| `Cs_STEENBRB` qp1, qp3, qp5, qp6 | cap 6600 | **opt** 232 / 2915 / 5148 / 530; qp1 at the Clarabel optimum |
| `ACOPR57_def`/qp3 | cap 4360 | **opt 2** |
| `DALLASM_f4` qp2, qp3 | cap 1960 | **opt 120** (border), **opt 74** (refactorize), `stat` 8.80e−03 → 9.21e−17 |
| `Hj_STEENBRD`/qp4 | num 283, `stat` 5.76e−01 | **opt 301, `stat` 4.40e−17** |
| `YORKNET_def`/qp0 | num 482, `eq` 1.36e+03, `stat` 1.00 | **opt 637, `eq` 4.55e−13, `stat` 5.64e−18** — NEW this round |
| `YORKNET_def`/qp1 | num 596, `eq` 3.59e+03 | **opt 747, `eq` 8.93e−14** |

**NO BASE-CERTIFIED CELL IS LOST ON EITHER ALGEBRA.** Every `kOptimal` row of NC2 fix1
— `ACOPP57_def` qp0–qp3, `ACOPR57_def` qp0/qp1/qp4, `Cs_STEENBRB` qp2/qp4, the four
`R1_Cs_STEENBRB`, `STEENBRB_def`/qp0 — is `kOptimal` at `f2226c28`, on border and on
refactorize. `ACOPP57_def` improves from 8 minors to 2 on border.

**WHAT THIS ROUND GIVES UP relative to `c46485d8`** (both are step-cap decisions, §1a):

| cell | base | `c46485d8` | `f2226c28` | reading |
|---|---|---|---|---|
| `A0_YORKNET`/qp1 | num 777, `eq` 1.56e−13 | opt 712 | **num 329, `eq` 4.37e−01** | E4's conversion lost; vs the BASE it is num → num, so no certified cell is lost — but the equality residual is WORSE than the base's on this one cell |
| `STEENBRB_def`/qp2 | cap 4680 | opt 392 | **num 547** | E4's conversion lost; `f` finite and better than the base's, `eq` 6.55e−07 → 1.19e−15 |

**OTHER MOVERS, none of them a certified loss:** `ACOPR57_def`/qp6 cap 7100 → num 220 and
`DALLASM_def` qp1/qp3 cap 1960 → num 183/157 (honest early failures replacing caps);
`ACOPR57_def`/qp8 num 35 → num 42; `Hj_STEENBRD` qp1/qp9 num → num at fewer minors;
`STEENBRB_def` qp1/qp3 num → num at fewer minors with `eq` 7.83e−09 → 1.19e−13 and
8.11e+02 → better; the four `R1_Cs_STEENBRB` and `Cs_STEENBRB` qp2/qp4 `opt → opt` at
different minor counts.

**AND TWO CELLS NOW READ A CERTIFIED `kInfeasible`** where the base read
`kNumericalError`: `DALLASM_def`/qp2 on border (`inf`, 165 minors, `eq` 1.52 → 1.31,
`cert` 0 → 1) and `DALLASM_def`/qp1 on refactorize (`inf`, 106 minors). The `DALLASM`
family reaches no feasible point in ANY arm — its equality residual is ~1.3–1.5
everywhere, at the base too — so the verdict is not obviously wrong, but these dumps come
from a set named for FEASIBLE QPs and a `kInfeasible` is a CERTIFICATE claim. **This is
the round's most serious open item after the suite and it is REGISTERED (§7.2).**

### 5b. The 51-dump corpus, E2 OFF — the CONTROL lane, and it moves

`h1off` vs `fix1off`, border. **51 of 51 rows move**; 25 change status or minors.
WON `cap -> opt`: `A0_YORKNET` qp1, qp2; `Cs_STEENBRB` qp3, qp4, qp5; `DALLASM_f4` qp2,
qp3; `Hj_STEENBRD`/qp4 — eight. **LOST `opt -> cap`: `A0_YORKNET`/qp6 (opt 941 → cap
7680), `Cs_STEENBRB`/qp2 (opt 237 → cap 6600), `SAWPATH_def`/qp2 (opt 182 → cap 6830)** —
THREE off-path certified cells, named here as E4-15 requires. `Hj_STEENBRD`/qp1 cap →
num. The off path is the harness CONTROL lane, not production; it is DECLARED MOVING.

### 5c. The T7 feasible census — **120/120 HOLDS**
`QpEngineStructuralViolation.T7TheFeasibleCensusHalfOverTheFULLDualMuSet` PASSES at the
final engine, Release and in the targeted Debug run. E4-6's STOP criterion is met.

### 5d. U0, 27 cells × 75 columns × 3 engines, against the M6-close pin, gate 1e−5
`.scratch/e4/logs/g5-u0.log`, `u0/comparison.txt`. **43 differences** (walk 17, ssn 13,
ipm 13) against `c46485d8`'s 96.

| engine | differences | reading |
|---|---|---|
| walk | 17 | 12 residual columns ALL STRICTLY BETTER (`kkt_stationarity` 6.30e−13 → 4.16e−20 and four more like it; `kkt_residual` and `kkt_primal` to ~2.6e−16); 5 `qp_minors` 2 → 3 |
| ssn | 13 | 12 strictly better; `f7_n800_path_warm` `kkt_stationarity` 1.7596082e−09 → 1.7594681e−09, better |
| ipm | 13 | 12 strictly better; `f7_n1000_path_warm` `kkt_stationarity` 3.4497076e−07 → **3.4497516e−07**, WORSE by 1.3e−12 relative — a last-digit move, inside §7's residual-class exemption |

Nothing changes status, verdict or major count on any cell, on any engine.

### 5e. HS pins
`HsBattery.RefactorizeMode`, `BorderMode`, **`AggregateCostIsRecordedAndBounded`**,
`EveryElasticActivationRunsTheRhoLadderToItsCeiling`, `ModeStatusDivergenceIsEmpty`,
`NoBatteryProblemEverCertifiesInfeasibility`, `BorderModeFalseInfeasible`,
`FeasibleStartsEndFeasibleDespiteTheLooseFunnelWidth`, `SocIsRareAndUnprofitable…`,
`AdaptiveMuNeverSharesAMajorWith…`, `TableCoversEveryShippedProblem`, and
**`HsSweepCorpus.Corpus`** — all PASS. The two counter pins in the family were re-derived
(§3a). The cost tripwire is worth calling out: at the wide gate the battery's
`factorizations` read 432 against its bound of 400; at the shipped gate, with the polish's
own solve in its own counter, it reads **288 against a pre-E4 baseline of 320** — BELOW
it, because the EQP-site polish removes the extra pass per face.

### 5f. The Release suite — 2862 tests, **63 failures** (`c46485d8`: 83 of the same 2862)
`.scratch/e4/logs/ctest-release-h2.log`; per-test first messages in
`logs/h1-failure-messages.txt`. **37 of `c46485d8`'s 83 are FIXED, 17 are new, 46 are
inherited.** Every one is attributed:

**FIXED (37)**, including six status regressions `c46485d8` carried:
`SqpEqualityBasis.F8…` (kNumericalError → kOptimal), `F9ZeroStepAtAnInconsistency…`,
`SqpEqualityBasisSeam.T6DisabledHandOff…`, `IpqpDispatch.KIpmAndKWalkAgree…` and
`TheRoutingTableIsExercised…` (both kNumericalError → kOptimal on the HS battery),
`QpE2WorkingSet.AMuFreeInertiaReReadCertifiesTheSingularPsdFaceOfAcopp57` (the border
`ACOPP57` loss); plus four `B1Gate` cells, both `QpEngineFaceRefine`, both
`QpEngineBorder` cross-algebra `bound_state` fixtures, three `QpEngineIndefinite` minor
pins, three `SqpDriverElastic`, `SqpDriverQpFailure`, `Continuation.ProbeBudget…`,
`QpEngineTr`, and the fourteen re-derived pins of §3a.

**THE 63 THAT REMAIN, by class:**

| n | class | attribution |
|---|---|---|
| 6 | `QpAntiCyclingArm` | `off.solution.status` kOptimal, `off.minors` 4 against `kCycleCap` 200: **the period-2 and period-4 recurrences ARE GONE.** Every one of these six asserts, through one shared helper, that the unarmed walk cycles. Their declared re-derivation is a settler-level question (the M4 arms' remaining purpose) and was not taken |
| 10 | `QpDegeneracyArm` | the same helper class: 4 "really does cycle" and 6 diagnostics with nothing left to observe (`drops`, `total_diag`, `loud_refused`, `loud_passed` all 0) |
| 11 | `QpE2WorkingSet` | 3 `dep_border_retreats` 1 → 0 (the retreat is not needed any more); `ADeclinedRide…Qp3` now fails on `dep_ride_blockers`/`ride_exchanges` 0 (NC1's mechanism is inert on that cell); `ADependentRideBlockerIsExchanged…` and `ANullSpaceVetoedRide…` the same; `NoAdmissibleMemberAndNoPassOver…` `x(0)` 0.5 → 0.6; `AGreyReRead…` FIXED; **`TheLockedVertexUnlocksOnTheAcopr57PrimalQps` is a REAL loss: `f` off the option-off arm by 1.4 % on `ACOPR57_def` qp0** |
| 3 | `QpNc2FlatFace` | fix1's own fixtures: qp3 CONVERGES where they pin a cap, and `ANoiseLevelDropIsUndoneAndCountedNotRidden` reads `ride_sign_vetoes` 0 — no veto because no ascending ride |
| 2 | `B1Gate` NEW | **both are the pathology removed, asserted from the wrong side**: `AScaledStalePriceIsRefusedAtIngest` requires its converged point's complementarity `> 1e-6` to argue a scope ruling, and it now reads 3.23e−13; `KSsnMatchesTheWalks…` pins the SSN's at 1.0000e−04 and the walk-relative ratio, and both arms are now at rounding (`ssn` 3.23e−13, walk 0) |
| 2 | `B1Gate` inherited | `EqualityOnlyWarmSolvesAreBitIdenticalAcrossTheRepair` (`x0` differs in the last 4 digits: 3.4794942110708625e−10 vs …750977e−10); `TheWatchdogRestoreCarriesTheComplementarityGate…` (fixture premise: `kWarmFullStepWindow` no longer fires on this solve) |
| 2 | `EqpRefinementAb`, `SqpDriverAdaptiveMu` | **both pin an ACCURACY CEILING the polish removes**: `SecondCeilingFixture…` asserts `err > 1e-8` and reads 9.66e−16; `AdaptiveMuRecoversTailAccuracy` asserts a fixed-mu ceiling of `err > 1e-6` and reads 3.04e−09 |
| 1 | `WarmStartBattery.Corpus` | 13 pinned cells, `minors` and `border_refine` both up: 38 → 57, 23 → 34, 12 → 16, 156 → 162, 84 → 89, 27 → 32, 103 → 107, 98 → 99, 20 → 21. **+4 % to +50 %: the largest cost regression this round carries, and it is not re-pinned** |
| 1 | `QpModeSites.EveryKernelInvocationIsAccountedForBySite` | HS15 in mode `kIpm` no longer reaches the `fallback_rung_b` site: a non-vacuity premise, not an identity |
| 3 | `QpEngineStructuralViolation.T6b…` ×2, `QpEngine.T6bANonFiniteDualMu…` | `verdict_refine_steps` 0 (the verdict-site refinement is not needed), `signed_ineq_resid` 8.88e−16 against `<= 0` (rounding, wrong side), and `kNumericalError` where `kMaxIter` is pinned on two deliberately ill-posed QPs |
| 2 | `QpEngineIndefinite` | `EngineLandsOnALocalMinimizer` reads `kMaxIter` on one synthetic indefinite fixture (a REAL status regression); `T7Fix2DualMuZeroOnARankDeficientFace…` reads `kNumericalError` where `kInfeasible` is pinned, on an inconsistent equality pair at `dual_mu = 0` (inherited from `c46485d8`) |
| ~17 | driver-side (`SqpDriverRestorationSeed` ×5, `SqpDriverCertifiedFallback` ×4, `SqpDriverRestorationProximity` ×2, `SqpDriverElastic` ×2, `SqpDriverRadius`, `SqpDriverRestoration`, `SqpDriverRestorationExcursion`, `SqpDriverDisprovedEntry`, `SqpDriverElasticSeed`, `IpqpDispatch`, `CorpusTask6bPhaseB`, `SqpEqualityBasis.T5Off…`) | the largest single signature is `seeded_rows(sol) == 0` on five `SqpDriverRestorationSeed` fixtures — the restoration seed's candidate is no longer TAKEN on those trajectories, which is a route change, not a counter. `SqpDriverCertifiedFallback.T7TheEscapeBranchTernary…` and `T3TheTierSolvedRung…` are status swaps between `kInfeasible` and `kNumericalError`; `T7TheVerdictRefineCostAtScale…` reads 0 verdict-refine steps against 15; `CorpusTask6bPhaseB` misses its derivation bound by 4 % on one cell's `kkt_stationarity` (3.2209505e−07 observed against 3.2209168e−07 derived); `SqpEqualityBasis.T5Off…` is a byte-identity pin against the pre-E1 base and cannot survive a change to every QP solve |
| 2 | `SqpDriverSsnMode`, `SqpDriverWarmStart` | FIXED — not in the 63 |

Three of the 63 are **REAL DEFECTS** by E4-13(c)'s rule (a status regression is a defect):
`QpEngineIndefinite.EngineLandsOnALocalMinimizer`,
`QpEngineIndefinite.T7Fix2DualMuZeroOnARankDeficientFaceDoesNotEscapeSolve` (inherited),
and `QpE2WorkingSet.TheLockedVertexUnlocksOnTheAcopr57PrimalQps`. None was fixed.

### 5g. The NC2 box-bounded LP synthetics
`QpNc2FlatFace`'s LP cells are among the three failures above, and they fail because they
CONVERGE where they pin a cap. No NC2 synthetic changes from a solved status to an
unsolved one.

## 6. THE RE-TAKE 9 PREDICTION, PER CELL

**The on-row identity premise is available for NO cell, and this time the CONTROL set
moves too.** 51 of 51 corpus rows move on the E2-ON path (both algebras) and 51 of 51 on
the E2-OFF path; nothing is carried, and the standing rule (a re-take's expected-identical
set needs a per-cell measurement or an on-row proof) applies with an empty carried list.

| TRAIN family | prediction, and what supports it |
|---|---|
| the 5 optimal cells | **status must hold; a move is a STOP.** None of the five is in the 51-dump corpus, so the corpus cannot speak for them: they need their own measurement. What the corpus DOES support is that the round loses no base-certified cell anywhere it can see |
| `STEENBRB` / `Cs_STEENBRB` | **conversion expected.** Four of six `Cs_STEENBRB` cells convert, qp1 at the Clarabel optimum. `STEENBRB_def`/qp2 does NOT convert here (it did at `c46485d8`) — a TRAIN cell routing through that face may read `kNumericalError` with a finite, better objective |
| `YORKNET` / `A0_YORKNET` | **conversion expected, E2 ON**, for qp2–qp6 and both `YORKNET_def` cells, with the drift gone (`eqviol` 7502 → 0). `A0_YORKNET`/qp1 stays `kNumericalError` and its equality residual is WORSE than the base's (1.56e−13 → 4.37e−01), so a TRAIN cell landing on that face may report a larger row residual |
| `STEENBRC`, `STEENBRE`, `STEENBRG` | **no prediction.** The corpus has no rows for them |
| `STEENBRD` | `Hj_STEENBRD`/qp4 converts (`num -> opt`, `stat` 5.76e−01 → 4.40e−17); qp1 and qp9 stay `kNumericalError` at fewer minors. E2-OFF, qp1 is lost (`cap -> num`) |
| `DALLASM` | **mixed and the one to watch.** `DALLASM_f4` qp2/qp3 convert (`cap -> opt`); `DALLASM_def` qp2 (border) and qp1 (refactorize) read a CERTIFIED `kInfeasible` where the base read `kNumericalError`. A TRAIN cell whose subproblem is one of those will see a `kInfeasible` propagate |
| `NET3` | **no prediction.** No `NET3` row in the corpus |
| `ACOPP57`-carrying cells | **improvement expected on the default (border) algebra**: four certified cells go from 8 minors to 2 with the terminal stationarity 1.25e−07 → 9.69e−15 |
| the CONTROL set (E2 OFF) | **DECLARED MOVING, for the first time.** 51 of 51 rows move and three certified cells are lost: **`A0_YORKNET`/qp6** (opt 941 → cap 7680), **`Cs_STEENBRB`/qp2** (opt 237 → cap 6600), **`SAWPATH_def`/qp2** (opt 182 → cap 6830) |

## 7. REGISTERED

1. **THE TERMINAL POLISH'S FACTORIZATION IS NOT IN `factorizations`** (§1b). An
   accounting choice with an argument, taken because the alternative re-pins eight
   `WarmStart` hot-reuse fixtures and destroys their ability to see a lost hot reuse. If
   the settler wants it in `factorizations`, the eight pins and the HS battery's cost
   tripwire (288 → 432) move with it and the hot-handle contract needs restating.
2. **TWO `DALLASM` CELLS NOW REPORT A CERTIFIED `kInfeasible`** where the base reported
   `kNumericalError`: `DALLASM_def`/qp2 on border (165 minors, `eq` 1.31, `cert` 1) and
   `DALLASM_def`/qp1 on refactorize (106 minors). The family reaches no feasible point in
   any arm, at the base too, so this is not obviously wrong — but it is a certificate
   claim on a dump from the FEASIBLE-QP set and it needs adjudication. **Schedule first.**
3. **THE ~25 PATHOLOGY-PINNING FIXTURES' DECLARED RE-DERIVATION** (`QpAntiCyclingArm` ×6,
   `QpDegeneracyArm` ×10, `QpE2WorkingSet` ×~8, `QpNc2FlatFace` ×3) was NOT taken. All six
   `QpAntiCyclingArm` cells and four of the `QpDegeneracyArm` ones run through ONE shared
   helper, `expect_the_off_walk_cycles`, whose premise is gone; flipping it also empties
   `expect_the_armed_walk_escapes` (there is nothing to escape) and makes the tests' own
   NAMES wrong, which is a rename event and a question about whether the M4 anti-cycling
   arms still have a subject. That is a settler ruling, not an implementer's.
4. **`WarmStartBattery.Corpus`: 13 pinned cells 4–50 % MORE minors** (38 → 57 worst),
   `border_refine` moving with them one for one. Not re-pinned: a +50 % cost on the
   warm-start battery is a finding, not a pin to move, and it deserves a look before it is
   accepted.
5. **THREE REAL STATUS REGRESSIONS**, none fixed:
   `QpEngineIndefinite.EngineLandsOnALocalMinimizer` (`kMaxIter` on one synthetic
   indefinite fixture), `QpEngineIndefinite.T7Fix2DualMuZero…` (`kNumericalError` where
   `kInfeasible` is pinned, on an inconsistent equality pair at `dual_mu = 0`, inherited
   from `c46485d8`), and `QpE2WorkingSet.TheLockedVertexUnlocks…` (`f` off the option-off
   arm by 1.4 % on `ACOPR57_def` qp0).
6. **FIVE `SqpDriverRestorationSeed` FIXTURES READ `seeded_rows(sol) == 0`**: the
   restoration seed's candidate is no longer TAKEN on those trajectories. One route
   change, five fixtures, and worth one diagnosis rather than five re-pins.
7. **THE TWO E4 CONVERSIONS THE STEP CAP GIVES UP**: `A0_YORKNET`/qp1 (opt 712 → num 329)
   and `STEENBRB_def`/qp2 (opt 392 → num 547). Both are `num` or `cap` at the BASE, so no
   certified cell is lost — but a cap between 1e−6 and 1e−4, or a cap that scales with the
   face's own conditioning, might keep them AND keep `ACOPR57_def`/qp4. Measured, not
   searched: only three cap values were tried.
8. `kEngineStepTieTol` deciding a weakly-active bound per algebra at 1e−17 — the
   cross-algebra `bound_state` contract question `c46485d8` registered — is **no longer
   observable**: both `QpEngineBorder` fixtures that caught it now pass, because the
   polish removes the `O(delta)` cleanup pass on which the tie was met. The question
   stands; its witness is gone.
9. `terminal_attainable_exits` was NOT added (§2): E4-12's shape (b) was not needed.
10. **`QpE2WorkingSet.AGreyPinOnYorknetThatIsIndependentStaysIndependent` TAKES 2682 s IN
    DEBUG** — nine times the next slowest test in the suite, and four times the 600 s bound
    E4-14 states. Its Release time is 37 s and moved 1.4 s this round, so the cost is
    essentially pre-existing; what is new is that it has been MEASURED, because this is the
    first round to run the whole Debug suite. It wants the split-per-(dump, algebra)
    treatment E4-14 describes, or a declared `GTEST_SKIP` with its cost.
11. **The Debug suite costs ~55 minutes of wall-clock at `-j4`** (2861 tests, plus 629 s
    for the one test that must run alone). Cheaper than feared and worth running per round,
    but it must be sequenced: it holds the box lock throughout.

## 8. WHAT I COULD NOT DETERMINE

- **Whether `QpE2WorkingSet.AGreyPinOnYorknetThatIsIndependentStaysIndependent`'s 2682 s
  in Debug is EXACTLY unchanged by this round.** Its Release time moves 35.94 s → 37.35 s
  and the Debug/Release factor on this tree is ~72, which puts the base at ~2590 s, so the
  round adds on the order of 90 s to a test that was already four times over the bound.
  That is an inference from the Release timing, not a base Debug measurement — no round
  before this one ran the whole Debug suite, so there is no base to difference against.
- **Whether `DALLASM_def`'s new `kInfeasible` is a false certificate.** The equality
  residual at the reported point is 1.31 and the family never reaches feasibility in any
  arm, so the verdict is internally consistent; whether the QP is actually infeasible I did
  not establish.
- **Why `A0_YORKNET`/qp1's equality residual is worse than the base's** (1.56e−13 →
  4.37e−01) while every other cell of that family improves. Both arms read
  `kNumericalError`, so no certificate is at stake, but the direction is the wrong one.
- **Whether the ~17 driver-side failures share a cause.** Five share the
  `seeded_rows == 0` signature; the rest were read one by one and attributed, not diagnosed.
- **Whether a cap between 1e−6 and 1e−4 exists that keeps all three cells** (§7.7). Three
  values were measured; the space was not searched.

## 9. EVIDENCE INDEX (`.scratch/e4/`)

| what | where |
|---|---|
| the corpus at the final engine, both algebras, E2 on and off | `corpus/h1-*`, `corpus/h1off-*` (base rows `corpus/fix1*`, `c46485d8`'s `corpus/e4*`) |
| the cap calibration | `corpus/g1-*` (no cap), `corpus/g2-*` (1e−4), `corpus/g3-*` (1e−6) |
| the comparison tools | `tools/rowcmp.py` (all columns, copied), `statuscmp.py` (status/minors, this round) |
| the Release suites | `logs/ctest-release-{f1,g3,g4,g5,h1,h2}.log`; per-test messages `logs/{f1,h1}-failure-messages.txt`, extractor `msgs.py` |
| the Debug suite | `logs/ctest-debug-h1.log` (69/2861, `-j4`, the long test excluded), `logs/ctest-debug-h1-long.log` (that test alone, PASSED, 629.33 s) |
| U0 | `logs/g5-u0.log`, `u0/comparison.txt`, `u0/head-{walk,ssn,ipm}.csv` |
| census + HS + B1Gate at the final engine | `logs/g5-key.log`, `logs/verify1.log` |
| the terminal-polish gate calibration | `logs/h-*.log`, and the scratch battery probe's output quoted in `STATE.md` |
| the runners | `leg.sh`, `build.sh`, `corpus.sh`, `corp3.sh`, `g2.sh`, `g3.sh`, `g4.sh` (U0), `g5.sh`, `both.sh`, `final.sh`, `final2.sh`, `dbgsuite.sh`, `dbgfinal.sh`, `f1.sh`, `harvest.sh`, `verify1.sh`, `msgs.py`, `statuscmp.py` |
| running state | `STATE.md` |
| the commit message | `commit-msg-fix1.txt` |
