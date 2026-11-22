# DECLARATION-RF-1-AB2 -- gate A+B2: the retry ladder, the floor exit, the bounded restoration

Written and committed, together with every leg script it names, before any gate leg; never edited afterwards (a failed or
void leg is answered by a `-r1` file, this one kept). Criteria are never changed after data. Design:
`docs/notes/2026-11-m8-rf1-design.md` sections 3-5 and amendment B2-1 (the settler's comparator ruling).

## 1. What is built

Library code: `b2263bfb2` (A), `1bb03fe46` (B2), `7b80afca7` (the B2-1 comparator). Tests: `8c4cff01c` (pin moves,
part 1), `85d98d787` (part 2), and the Debug config splits committed just before this file. The gate's code head is
`7b80afca7` (`git diff 7b80afca7 HEAD -- src include` is empty at this file's commit).

- **A 3.1** the retry ladder: `detail::choose_retry_rung` (R1 shrink when the shrink cuts the failed walk; R2 cold after a
  warm non-empty seed; R3 the other working-set algebra on a dedicated engine; R4 `primal_regularization` x 100 capped at
  1e-4 after a dependent-blocker or zero-motion exit); each rung once per iterate; the replay guard (`RetryFingerprint`,
  counted in `qp_retry_replays`); after the rungs a violated linearization goes to elastic.
- **A 3.2** a stagnation-stopped iterate is projected onto the subproblem box (window included) before the retryable test.
- **A 3.3** a certificate ROW refusal routes to elastic at any h.
- **A 3.4** the trust-region floor exit (`kStalled`, `StopReason::kTrustRegionFloor`) at h <= feas_tol and a last predicted
  decrease <= 100 eps max(1, |f|), at both floor sites.
- **B2 4.1** the phase factorization budget (`advanced.restoration_max_factorizations`, unset = 5000 + F_entry), its
  `kWorkLimit` labelled `kStalled` / `kRestorationWorkLimit`.
- **B2 4.2 as ruled (B2-1)** the sub-solve runs `return_best = kOnAnyNonOptimal` and its best tracker ranks by
  `detail::restoration_best_improves`: lowest PARENT h (the wrapped rows' l1 violation, `RestorationModel::
  original_violation_l1`) among phase-feasible iterates, strictly below the phase start's; nothing captured hands back the
  last point, never the entry; the budget-best exit requires a capture. The shared `best_improves` is unchanged.
- **B2 4.3** an exhausted elastic ladder inside the phase takes a rung first. **4.4** the reduction resume also reads
  `kStalled` and the phase budget's `kWorkLimit`; `restoration_resume_reduction` default 0 -> 0.1. **4.5** the
  `soc_qp_iteration_limits` counter.

## 2. Implementation interpretations (decided at implementation; declared here, not silently)

1. A replay is counted (`qp_retry_replays`) and NOT charged to `qp_failures_in_a_row`; it still takes the next rung.
2. The after-rungs elastic route (`rungs_exhausted_to_elastic`) is decided before the failure-step trial, so the last
   failure at an iterate is not offered as a failure step on that route; entry spelled `kWorkingSetTermination`; not taken
   inside the curvature-switch sub-solve.
3. A routed failure (gate passes) with no rung left, or R1 at the floor, takes the floor rule: the floor exit if its two
   conjuncts hold (pred of the failed QP's iterate), else restoration as before. A failure the gate refuses (budget spent /
   not retryable) at h <= feas_tol ends `kFinishQpFailure` as before.
4. R3 and R4 only for walk-owned failures; R4 skipped when its raised value equals the configured one.
5. The floor exit applies in the restoration sub-driver too (`kStalled` to the parent; the reduction resume reads it).
6. The phase budget spans both runs of a phase (the regularized restart included).
7. Inside the phase, an exhausted elastic ladder with a rung left charges `qp_failures_in_a_row`, counts a routed failure
   and a rejection, row verdict kReject; not on the curvature-switch run.

Further, recorded here rather than numbered as a design interpretation: the new counters fold from the sub-solve (as
`qp_cap_retries`); the seam's forced failures can replay (pinned, `EachRoutedFailureTakesADifferentRung`); the B2-1
comparator's incumbent is per run of a phase (a regularized restart starts its own); the h it ranks by is that of the rows
the phase restores (the retained rows on an equality-basis-reduced solve).

## 3. The comparator ruling (B2-1) and its must-kill

`SqpLadderSeam.TheReductionResumeTakesTheNoClaimPointAndCountsIt`, measured at `7b80afca7` (Release): both arms
h_in = 10, h_out = 1.52447e-05 (was 10.000015 at `1bb03fe46`, <= 5 at C); k = 0.5 resumes (1 reduction resume, 2 majors
against 1). It is the must-kill of mutation M14. The comparator's edges: `RestorationBestOrder.*` (unit).

## 4. Pinned expectations moved (the declared event)

Release ctest at `8c4cff01c`'s parent failed 16 tests; every one is listed with its move. Debug was first read at
`85d98d787` (4 config splits, last block). Each "rescue" of a test whose purpose was a still-failing exit is pinned as its
OWN test; the propagation purpose moves to a fixture that still fails.

| test | move | why |
|---|---|---|
| `OptionsReference.CheckedInFileIsCurrent` | regenerated | new options (`restoration_max_factorizations`, the 0.1 default) |
| `SqpCountersFieldTables.EachTableEnumeratesItsWholeStruct` | 120 -> 128 fields | eight new counters |
| `TraceV1Fixtures.SqpCompleteStream` | fixture regenerated; diff = the eight counter keys | new counters |
| `TraceMessagesSqp.SubproblemFailureLinesMatchTheExitCounters` | budget 1 ends kRestorationFailed naming the sub-solve's failed QP; budget 3 ends at the floor exit on a failed QP | A 3.1/3.4 |
| `SqpLadderSeam.R2AJudgedRejectedStepIsNotChargedToTheBudget` | R1 then R3 at the same radius (a replay), 3 routed, 0 accepted, peak on 0 / off 2, restoration 1 | A 3.1 |
| `SqpLadderSeam.CertOneAPointTheConvergenceTestCallsFeasibleIsNoCertificate` | kAcceptable -> kOptimal via the after-rungs elastic route, claim still withheld. **Config split** (Debug): the elastic steps alone reach x0 = 1.1e-12 without a restoration (46 majors, 23 routed); Release keeps the restoration premise (58 majors, 29 routed, x0 = 2.3e-6) | A 3.1; rounding decides whether a ladder comes back unusable first |
| `SqpLadderSeam.QcOneASlackInsideFeasTolIsNotABoundAtAViolatedRow` | re-premised: trust_region_min 0.75, R1 at the floor enters restoration at the start point | A 3.1/3.4 |
| `SqpLadderSeam.AnExhaustedElasticTierInsideThePhaseTakesTheRegularizedRestart` | kModel arm now kInfeasible, certified at the same point (must-kill of M18) | B2 4.3 |
| `SqpLadderSeam.TheReductionResumeTakesTheNoClaimPointAndCountsIt` | unchanged pins, restored by B2-1 (section 3) | ruling B2-1 |
| `SqpLadderSeam.Rf1ARestorationFailureIsLabelledAtTheTopLevelOnly` | its wide arm writes `restoration_resume_reduction = 0` out, as its off arm does (the best-h hand-back would otherwise resume at the 0.1 default; the arms must differ in the tier only) | ruling B2-1 |
| `SqpSolverQpFailure.RepeatedQpNumericalErrorStillPropagates` | REPLACED. Purpose moved to `Rf1RetryLadderSeam.ARepeatedSubproblemFailureStillPropagates` (seam: every walk QP forced to fail at the feasible line-model point with a forced step promising a decrease, budget 1: one rung, then kNumericalError / kSubproblemFailure, 2 rows, at the start point, both algebras). The TwinSaddle rescue (R3, then budget spent at h = 7.5 -> after-rungs elastic -> kOptimal at the minimizer, f = -1.125, 3 rows) is its own test `SqpSolverQpFailure.TheRungsRescueTheTwinSaddleAtABudgetOfOne` | A 3.1 |
| `Rf1AcceptableLabel.ASubproblemFailureAtAnAcceptablePointIsLabelledAndNothingElseMoves` | moved to the seam fixture above (`Rf1RetryLadderSeam.ASubproblemFailureAtAnAcceptablePoint...`); unchanged expectations | the TwinSaddle no longer fails |
| `SqpSolverQpFailure.TheShippedBudgetRoutesMoreAndThisFixtureThenSolves` | 5 -> 6 rows: R2 (cold) then R3, the third failure requests restoration, resumed, kOptimal at f = -1.125 | A 3.1 |
| `SqpSolverQpFailure.RoutedFailuresCountTowardTheRestorationGate` | re-derived: radii 12, 12, 12, 6, 3, 1.5 (R3 keeps the radius); rows 1 and 4 routed (R3, R1); rejections 4 = kRestoreMinRejections, 2 routed (half, never a majority -- funnel.h argument (II) met with equality); kStalled / kSubproblemIterationLimit; restoration 0 | A 3.1 |
| `Continuation.ProbeBudgetBoundsAFailingProposal` | at the old 500 cap the proposal is RESCUED (5 majors): arms A-C move to a 200 cap, the largest of {500, 300, 200, 100} at which it still fails (measured). Arm A: kNumericalError (restoration entered and failed), 3 majors / 1202 minors / 559 factorizations (**Debug 501**); arm B: abandoned, kWorkLimit, 2 / 202 / 5; arm C as B. The 500-cap rescue is the new arm F (3 steps, kOptimal, the proposal 5 majors) | A 3.1 |
| `HsSweepCorpus.Corpus` | HS15 warm on/on full_step_majors / majors 23/30 -> 22/40 (**Debug 21/39**); total minors Release 3355 -> 3406 (**Debug 3346 -> 3392**); every status and objective check unchanged | the failing chain at p = 1 takes the rungs and a restoration |
| `HsSweepCorpus.Hs15WarmKdOffWithTheFailedSubproblemStep` | see the per-point read below | A 3.1 |

**HS15 warm, read per point** (Release; each returned point re-solved cold from itself, KKT residuals of that re-solve by
`nlp_kkt_check.h`). p = 0, 0.25, 0.5, 0.75: both arms identical (f = 360.38, 246.34, 144.39, 63.959), each a KKT point
(stationarity <= 2e-8, primal <= 1e-16, complementarity <= 6e-14). p = 1, held-off arm: x = (-2.3e-5, -6.1e-3),
**f = 1.0038, NOT a valid local solution** -- the re-solve fails too (kNumericalError) with stationarity 2: the LICQ point
`kTruncatedWarmArms` argues, the same honest failure as at C, 30 -> 40 majors on the way (**Debug: 39 majors, f = 1.00343**;
the stop is pinned to |f - 1| <= 1e-2). p = 1, step arm: x = (0.5, 0.25), **f = 0.25, a valid local solution** -- the
re-solve converges in 0 majors with every residual 0 (x1 at its bound 0.5, both swept rows slack); the arm then reaches
p1 = 1.5 at the same point (0 majors at p = 1.25 and 1.5). Release step arm 43 / 149 / 64 / 6 (**Debug 42 / 142 / 58 / 4**),
failures 0 (was 1). The step arm's new f = 0.25 is a rescue onto the other branch, pinned as read.

Not a pin move, recorded: `ScaleF7Slow.EmptyWindowColdSolveAtOneMillionVariables` (outside ctest by its TEST_FILTER) fails
`qp_minor_iters` 3 vs 2 identically at the C head `c721dc32d` (built and run in `.scratch/wt-C`): pre-existing, not RF-1.

## 5. Arms, binaries, legs and bars

Arms: **C** = `c721dc32d` (gate C's head; its committed TRAIN rows `gate-C/train/C/` are the reference and are not
re-run) and **AB2** = `7b80afca7`. Binaries are frozen in `BINARIES.txt` (appended with this file). The corpus binaries'
embedded `# binary:` stamp is resolved at configure time and is stale on both (`-dirty`, an older describe): their
authority is the sha256 and the commit recorded in `BINARIES.txt`, as at gate C. clang 22.1.8, `-DHVEN_FP_MODE=SAFER_FAST`,
Linux x86-64, AMD Ryzen 7 5800X3D, MKL 2026.2, `MKL_NUM_THREADS=1`. Every step under `~/.local/bin/box-lane` (one 4-core
lane per step, never `--exclusive`); counters only; no wall-clock claim.

| leg | instrument (in `legs/`) | bar / what is reported |
|---|---|---|
| T | `ab2_corun.sh`: `train_enum.sh` (238 roster cells x {D, K}, CAP 300 s, one pinned core) on lane AB2, then `ab2_train.py` vs `gate-C/train/C` | every status mover listed with its objective-quality read (same / better / worse / incomparable). Bar: **no lost solve** (optimal/acceptable -> other) without a settler ruling: each LOST_SOLVE is an open item, not a pass. Reported: optimal and optimal+acceptable counts, factorization and iteration totals per lane, C vs AB2. Missing rows 0. HOLDOUT never |
| P | `ab2_corun.sh`: `pilot_run.sh` C and AB2 (the 62 cells of `pilot_cells.tsv`: every non-solved, non-null row of the 8 pilot problems in the TU-1 store, MSS2 restricted to its sqp-qn labels; iteration budget 300, a 900 s kill reported, one pinned core each; the pilot store `hven-bench-tu1` is read-only and never touched) + `pilot_report.py` | each cell old vs new (status, iterations, factorizations, f, stationarity, the rung / restoration counters). Reported, no bar |
| W | `ab2_census.sh C` and `ab2_census.sh AB2`: the census runner's lane copy `run_walk_census_lane.sh` (only change: cells pinned to the lane's `BOX_LANE_PHYS`; protocol stamp says so), 57 cells, T1 4-wide, T2 solo within the lane, T3 4-wide full budget, tiered from `bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv`; then `census_compare.py` C vs AB2 | every C-vs-AB2 mismatch listed. **DEVIATION from the instrument's declared terms**: T2 is solo in its lane, not on the machine (`--exclusive` is not used by this gate), so a wall-dependent (deadline / dnf) status difference is a CANDIDATE only, never a finding; it is not re-run alone here |
| M | `ab2_chain.sh` -> `mutations_ab2.sh` with `mutate_ab2.py`; control first; each mutation rebuilt and run on the pinning filters (sqp: `Rf1*:RestorationBestOrder.*:QpCap*:QpFailureStep.*:SqpSolverQpFailure.*:SqpSolverRestoration*:RestorationResumeReduction*:Continuation.ProbeBudget*:HsSweepCorpus.*`; seam: `SqpLadderSeam.*:Rf1*`; trace: `TraceMessagesSqp.*`) | control clean; M1-M19 each must be killed; a survivor is REPORTED by name (verdict `MUTATION_SURVIVED_<id>`), never hidden. R1 reported only |
| C | `ab2_chain.sh`: `ctest.sh` rel and dbg at the committed head | "100% tests passed" in both logs |
| S | `ab2_chain.sh`: `install_smoke.sh` | smoke and export contract rc 0 |
| CI | the four-lane workflow at the pushed head of `rf1` (`gh run watch <id> --interval 120`) | green on all four lanes; an Accelerate/Windows route pin that differs is recorded, not guessed |

Mutations (each a must-kill unless marked R): M1 R1's skip rule inverted; M2 R2's inverted; M3 R4's inverted; M4 the replay
guard disabled; M5 the projection on every termination kind; M6 the projection never applied; M7 row refusals back to the
bound rule; M8 the floor exit's `h <= feas_tol` conjunct dropped; M9 its noise conjunct dropped; M10 the after-rungs elastic
route reverted; M11 the phase budget unset; M12 the phase budget labelled `kWorkLimit`; M13 the sub-driver's `return_best`
reverted; M14 the B2-1 comparator reverted to the shared order (must-kill: the reduction-resume fixture); M15 its
phase-feasibility conjunct dropped; M16 its start incumbent dropped (the entry point capturable); M17 the budget-best exit
without a capture; M18 the elastic-exhaustion route inside the phase reverted; M19 the reduction-resume extension reverted.
R1 the comparator made non-strict (reported). Known likely survivors, stated before the data: M15 and M16 (no fixture is
known to separate them), M9 (the noise floor's calibration point is SMBANK, not a unit fixture).

## 6. Not run

HOLDOUT (never). Wall-clock. Accelerate and Windows readings (CI only). A machine-solo re-run of census candidates.
