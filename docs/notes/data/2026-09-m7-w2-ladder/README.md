# M7 W2 ladder round — f2, f1 (= E5), F4 with the guard

Brief: `.superpowers/m7-w2-ladder-brief.md` (settler rulings L1–L4). Base `c1f1e704` (E2 closed).
Evidence behind the rulings: E2-D3 (`../2026-09-m7-w2-e2-d3-restoration/REPORT.md`, Q1–Q6) and
E2-D1 (`../2026-09-m7-w2-e2-d1-diagnosis/REPORT.md`, D4 and F4). Proposal E5 is
`../../2026-09-22-sqp-degeneracy-proposal.md`.

What the round does: the ladder's statuses become honest (L1), the start point and the restoration
wrapper stay inside the box (L2), and E2 terminations at an infeasible iterate open the elastic
tier (L3). It predicts no TRAIN conversion (E2-D3 Q6): the engine still has to solve the elastic
and restoration QPs, which are feasible by construction.

## L1 — an unserviceable restoration request is never `kInfeasible` (f2)

**Why.** E2-D3: every false SQP `infeasible` on TRAIN came from one line. A restoration request
that could not be serviced (nested, or past the once-per-solve cap) was spelled `kInfeasible`,
whatever h was. DALLASM was declared infeasible at h = 3.6e-13 and SAWPATH at h = 0.

**What.** Both refusals are spelled `kNumericalError`. The parent's pass-through of a sub-solve
`kInfeasible` is `kNumericalError` too; since L1 no sub-solve exit spells that status.
`kInfeasible` stays with the certified arm (the sub-solve's `kOptimal` at h > feas_tol). Also still
`kInfeasible`: the proximity-refused certificate on that same arm, and E1's
inconsistent-equalities exit. **`kStalled` is not used**, although it exists: the SQP engine
reports it on no exit (`solver_status.h`), and adding one would widen the engine's status
vocabulary, which the brief did not rule on.

| Site | Where |
|---|---|
| the refusal (nested / cap) | `src/drivers/sqp_solver.cpp:4675-4680` |
| the parent's pass-through of a sub-solve `kInfeasible` | `src/drivers/sqp_solver.cpp:5299-5303` |
| the status contract (two shapes of `kInfeasible`) | `include/hven/core/solver_status.h:45-62` |
| `infeasibility_certified`, `restoration_sub_status` notes | `include/hven/drivers/sqp_solver_types.h` (the flag's note), `include/hven/core/solver_counters.h` (`kInfeasible` row) |

## L2 — the start is projected, h reads the box, the wrapper is clamped (f1 = E5)

**Why.** E2-D3 found that YORKNET's CUTEst x0 violates its own box on 40 of 312 components, and
SAWPATH's on 2 of 583. Three things let that through:
- `project_start_point` was off by default.
- The restoration wrapper started at the unclamped point, so its first QP could not reach the box.
- h ignored the box, so SAWPATH resumed as "feasible" at h = 0 while 3.56 outside its box.

**What.**
- `SqpOptions::project_start_point` defaults to **true** (`include/hven/drivers/sqp_solver_types.h:719`,
  note `:672`). This is a declared dump move; see the digest below.
- **h and the box: a separate box term, added only where h decides the ladder.** The term is
  `box_violation_l1(x, l, u)` (`include/hven/drivers/sqp_solver.h:121`,
  `src/drivers/sqp_solver.cpp:552`). It is added in two places:
  - the restoration resume test (`src/drivers/sqp_solver.cpp:5125-5128`);
  - the F4 guard (L3, `:6397-6402`).

  `constraint_violation_l1` is unchanged. **Why not everywhere:**
  - The funnel's iterates are x + p, with p inside the QP's box [l − x, u − x]. Its box term is
    therefore zero up to the rounding of x + (l − x). Adding it would turn that rounding into
    trajectory moves on every in-box solve, which breaks the byte-identical control.
  - The seed guard compares two rows-only measures. Its candidate is clamped into the box by
    construction, and the wrapper now clamps its start anyway.
  - The budget-best (h, f) order reads the row's `violation_l1`, like the funnel.
- **The wrapper clamps its start and its centre**
  (`src/globalization/sqp/soc_elastic_restoration.cpp:261-263` for the centre, `:297-332` for the
  start, `start_clamps()` at `:347`; header `include/hven/detail/globalization/sqp/restoration.h:152`):
  - The start's x-block is `min(max(x_entry, l), u)`, with the rows re-evaluated once at the clamped
    point. The sigmas stay the entry's, and an in-box entry keeps the old arithmetic with no model
    call.
  - If the rows are not finite at the clamped point (a model undefined on its own bound), the start
    stays the entry. Otherwise the sub-solve's non-finite-x0 refusal would throw out of the parent.
  - At ζ > 0 the proximity centre is clamped the same way.

## L3 — E2 terminations open the elastic tier, only at h > feas_tol (F4)

**Why.** E2-D1 D4 found that only `kInfeasible` opens the elastic tier. A `kNumericalError` from an
E2 termination took the shrink ladder (budget 3), and shrinking keeps an infeasible linearization
infeasible. E2-D3 then showed that F4 as specified routes a failure at a feasible iterate into an
elastic QP with no slacks, which is the same QP (DALLASM major 1).

**What** (`src/drivers/sqp_solver.cpp:6390-6403`). A walk subproblem ending `kNumericalError` with
`dep_blocker_terminations`, `zero_motion_cap_hits` or `dep_certificate_rejections` > 0 opens the
elastic tier iff `h(x) + box(x) > feas_tol` at the iterate. At a feasible iterate the shrink ladder
stays. There is no enum break and no new counter. The seam's hook (below) sits just above,
`:6372-6388`.

## Fixtures (both configurations)

| Ruling | Fixture | Asserts |
|---|---|---|
| L1 | `SqpLadderSeam.L1ARefusedRequestAtAFeasiblePointIsNeverInfeasible` (`tests/sqp/test_sqp_ladder_seams.cpp`) | DALLASM's shape: one restoration, resumed inside feas_tol (`restoration_sub_status` kOptimal); every later QP fails through an E2 termination; the F4 guard keeps the shrink ladder (`qp_failures_routed` ≥ 1); the floor raises a second request; the cap refuses it. Status `kNumericalError`, not certified, \|cE(x)\| ≤ feas_tol |
| L1 | `SqpDriverRestoration.SecondRequestIsCappedAndReported`, `SqpDriverRestorationExcursion.ARefusedSecondRequestLeavesTheFirstEntrysRecord` (re-pinned) | the cap on a feasible NLP: `kNumericalError` |
| L2 | `StartPointProjection.TheDefaultProjectsAndSolvesTheNanStartShape` | the default is on; NET3/STEENBRD/STEENBRE's shape (NaN outside the box) solves at defaults, `start_point_clamps` 2 |
| L2 | `StartPointProjection.TheBoxHalfOfHIsTheL1DistanceToTheBox` | `box_violation_l1`: 0 inside and on the bounds, the ℓ1 gap outside, infinite bounds inert, NaN propagates, sizes checked |
| L2 | `RestorationModelClampedStart.AnOutOfBoxEntryStartsClampedWithItsRowsReadThere` | YORKNET's shape at the wrapper: start (0, 5) from entry (−2, 6); slacks from the rows at the CLAMPED point (sm 4, where the entry reads 3); the start is inside the wrapper's box and satisfies its rows; objective = h(clamped); at ζ > 0 the centre is clamped (proximity gradient 0 at the start) |
| L2 | `RestorationModelClampedStart.ANonFiniteClampedPointKeepsTheEntry` | rows NaN at the bound: the start stays the entry (count 0, finite) |
| L2 (control) | `RestorationModelClampedStart.AnInBoxEntryIsTheWrapperThatShipped` | in-box entry: the start equals the pre-round formula byte for byte; on a bound is not outside |
| L2 (control) | `StartPointProjection.OnLeavesAnInBoxStartByteIdenticalToOff` (existing, unchanged) | an in-box x0: ON and OFF byte-identical, every counter |
| L3 | `SqpLadderSeam.L3AnE2TerminationAtAnInfeasibleIterateOpensTheElasticTier` | READING6's shape: QP 0 at h = 2 ends with an E2 termination, the elastic tier opens (row 0 elastic, no routed failure), the optimum is reached |
| L3 | `SqpLadderSeam.L3AtAFeasibleIterateTheShrinkLadderStays` | the DALLASM major-1 guard: at h = 0 the same failure shrinks (routed 1, no elastic activation) |
| L3 | `SqpLadderSeam.L3APlainNumericalErrorKeepsTheShrinkLadder` | no E2 counter, so no elastic |
| L3 × L2 | `SqpLadderSeam.L3TheGuardReadsTheBox` | rows satisfied, 6 outside the box, projection off: the tier opens. A rows-only h would shrink |
| dump | `OptionsDump.SqpDumpsTheStartPointProjectionOn` | the dump line |

**The seam** (the sixth; `docs/testing.md`, "A sixth seam"). The header is
`include/hven/detail/drivers/ladder_testing.h`, `#ifdef HVEN_TESTING` only. It is compiled into the
existing `hven_sqp_seam_tests`, so there is no new target. It replaces a top-level walk QP's answer
with the engine's own N3 exit shape. It stands in for the QPs E2-D3 dumped and the engine does not
solve, which no small model reproduces. It is an injected result: it shows how the driver routes a
failed QP, and it says nothing about the engine.

## Suites (MKL, clang++, `ctest -j4` under `setsid`, the box lock per leg)

Scratch builds of the main tree at the committed sources (`.scratch/m7w2ladder/build-{rel,dbg}`):
- **Release: 2844/2844** (`s3-suite-rel.log`).
- **Debug: 2843/2843** without the long test (`s3-suite-dbg.log`). The long test
  (`Continuation.ProbeBudgetBoundsAFailingProposal`) ran alone, detached: **1/1**, 706 s
  (informational; `s3-suite-dbg-long.log`).
- Against the E2 fix5 gate record (2833 / 2832), **11 tests were ADDED** and none removed:
  `SqpLadderSeam.*` ×5, `RestorationModelClampedStart.*` ×3, `StartPointProjection.*` ×2 and
  `OptionsDump.SqpDumpsTheStartPointProjectionOn`.
- The HS corpus (`HsBattery.*`, `HsSweepCorpus.*`, the HS driver cells) is inside both suites.
  Every HS status and objective is unchanged; the one HS mover is HS45's counters (below).

## Offline U0 G4 (before the gates)

27 cells × 3 engines on a scratch Release build of these sources (`.scratch/m7w2ladder/g4/`):
- **Against `c1f1e704`** (whose code is E2 fix5's `34e100de`; `.scratch/m7w2e2fix5/g4/`): EXACT,
  **0 differences** on all 75 columns in all three engines.
- **Against the M6 close pin** under the 1e-5 residual gate: 0 differences. The walk engine has 7
  within the gate, the known f7 warm residual digits.

No U0 status, objective or counter moved. None of the U0 cells has an out-of-box start or reaches
a refused request or an E2 termination.

## Options dump — a declared move

The dump keeps its 64 lines, and `sizeof(SqpOptions)` is unchanged. Exactly one line moves:
`project_start_point = false` → `true`. The sha256 of the default `SqpOptions{}` dump (a scratch
program linked against each build's install, `.scratch/m7w2ladder/probe/`):

| | sha256 |
|---|---|
| `c1f1e704` | `096910a0584bf3bca4c9d9c66e37b9b14e5be7c55a0004ea6bf669a677c37988` |
| this round | `600fc9a3d865264939d8d9f0e717110935c947b70a10246ee5bee34fb338aa02` |

The harness's `options_digest` hashes its own lane dump, and it moves with this line at the
re-take. That move is declared here.

## Re-pins (declared, with attribution)

| Fixture | Was → now | Cause | Class |
|---|---|---|---|
| `SqpDriverRestoration.SecondRequestIsCappedAndReported` | `kInfeasible` (uncertified) → `kNumericalError` | L1: the cap refusal on a FEASIBLE NLP (the fixture's own words) | improvement |
| `SqpDriverRestorationExcursion.ARefusedSecondRequestLeavesTheFirstEntrysRecord` | same | same | improvement |
| `HsBattery.CrashBasisIsANullResultOnTheBatteryWithTwoBoundSeededExceptions` (MKL arm) | minors 804/801 → 802/799, facts 288/289 → 287/288 | L2: HS45's x0 is outside its box (1 component, gap 1) and is projected; its answer is unchanged | counter |
| `IpqpAcceptanceCensus.AnArmedRunIsNeverMistakenForAStallAcrossTheHsBattery` (MKL) | `rows_with_armed` 6 → 5 | L2: HS45 (`hs45=33` before) no longer arms the ladder | counter |
| `SqpDriverCertifiedFallback.T7TheEscapeBranchTernaryCONSUMESAnAttachedReportAtTheDriver`, `…T7TheAttachedReportsRestorationOfferIsATROUNDINGNotAMargin` | unchanged: `project_start_point = false` set in the fixture | the swept shape starts at (2.5, 2.5), outside its unit box, by design; projection would change the fixture | premise kept |
| `SqpDriverCertifiedFallback.T3TheTierSolvedRungReachesTheSameVerdictOnTheSameLinearization` | tier arm `kInfeasible` → `kNumericalError` (Release and Debug); walk arm `kInfeasible` → `kNumericalError` in **Debug only** | L1: each moved arm was an UNCERTIFIED `kInfeasible` at `c1f1e704`, from a refused request. Release tier: the test's own `tier_certified` 0. Debug: the sub-solve sticks for 34 majors and its nested request is refused (observed with a scratch probe against a `c1f1e704` Debug install). The certified arms keep `kInfeasible` | **L1-mandated, NOT an improvement: see below** |
| `SqpDriverCertifiedFallback.T7F1TheSameInconsistentLinearizationInAllThreeModes` | Debug only: all three modes `kInfeasible` → `kNumericalError`; Release unchanged (certified) | same as T3's Debug arm | **same** |

Both re-pins now assert the invariant `status == (certified ? kInfeasible : kNumericalError)` and
pin each configuration's observed value with `#ifdef NDEBUG`. The Release/Debug split existed
before the round, when both configurations reported `kInfeasible` but only Release was certified.

**T3 and T7F1 are flagged for the settler.** Their model is genuinely infeasible (x0 + x1 = 2 and
x0 + x1 = −2). The moved arms reported `kInfeasible` before the round, the right word, but through
a refused request, not a certificate. L1 as ruled ("NEVER `kInfeasible`; reserved for the certified
arm") makes them `kNumericalError`.

The implementer read L4's STOP clause ("a status change that is not an improvement to a
verified-correct answer") as not triggered, because none of the moved answers was verified. Each
was uncertified, `infeasibility_certified` 0 before and after. The round therefore proceeded,
and the settler may rule otherwise. The certified arms (Release walk arms) are untouched.

No other status or answer assertion moved in either configuration.

## Declared TRAIN movers for re-take 5

These are the five cells whose starts are outside their box (E2-D3), and which the default
projection now moves:
- **NET3, STEENBRD, STEENBRE.** A NaN objective at x0 today. Projection gives them a finite start,
  which the re-take must READ; it is a large finding either way.
- **YORKNET** (40 components). Predicted `infeasible` → `numerical_error` or better (E2-D3 Q6,
  observed under the scratch fixes).
- **SAWPATH** (2 components). Predicted `infeasible` → `numerical_error` or better.

E2-D3 Q6 predicts 0 further conversions. The must-stay-optimal cells are ACOPR57, ACOPR118,
READING6, ZAMB2-8 and SMBANK. Any cell whose SQP run reached a refused request is a declared status
mover `infeasible` → `numerical_error` (L1). Any cell whose walk ended an E2 termination at
h > feas_tol is a declared counter mover (elastic activations; L3).

**What projection does to the three NaN-start cells: UNOBSERVED locally.** It needs the CUTEst
harness on a scratch pin, and it is the re-take's first read.

## Not covered by the brief, stated

- A WARM solve's start (`warm.x`) is still not projected. This was registered at M7 W2 M4 and is
  unchanged.
- The restoration sub-driver inherits `project_start_point = true`. The wrapper's start is now
  inside its box, so this is a no-op, except for a slack above 1e20.
- F4's routed QP is not counted in `qp_numerical_exits`, the same as a `kInfeasible` the tier
  consumes (the counter's comment says so).
- The T3 fixture's tier arm certifies with a scratch probe program but not inside the test
  binary, at identical options. The fixture sits at a numerical knife edge that is sensitive to the
  model's compile flags. This is recorded, not chased.

## Gates (declared before the chain ran)

`wrappers-gates/`: the runner, the chain and the rehearsal, copied from the E2 round's last
gates and re-pointed. `psym-derivation-ladder.txt` holds every declared value, derived from
`c1f1e704..0275d16d` and a fresh P-SYM rehearsal on the final pair:
- **P-SYM:** Release 8 = A 8 + B 0; Debug 7 = A 7 + B 0. `test_hs_battery.cpp.o` is NOISE-ONLY in
  Debug.
- **Suites:** Release 2844, Debug 2843, the long test 1.
- **By name:** 11 ADDED, none REMOVED.
- **Sections:** 4 non-debug movers per configuration (2 class A, plus sqp_options and ipqp_engine
  NOISE-ONLY); DEBUG_ONLY 0 / 7.
- **G4:** 0 differences.

`preflight.txt` records the pre-flight, the by-name checks and the refusal controls.
