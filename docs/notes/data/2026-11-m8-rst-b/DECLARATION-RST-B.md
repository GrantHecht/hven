# DECLARATION-RST-B -- SQP restoration and retry-ladder fixes, and a per-solve restoration bound (DRAFT)

**UNFROZEN DRAFT.** Committed on branch `rst-b` for review only; not frozen, no leg has run. It is committed, with every leg script it names (`legs/`), before any leg and never
edited afterwards; a failed or void leg chain gets a `-r2` file and this one is kept. Criteria are never
changed after data. Precedent: `../2026-11-m8-rf1/DECLARATION-RF-1c.md`. Design and diagnosis:
`.superpowers/rst-diag-2026-11-24.md`, sections (a)-(d), accepted as designed by ruling C514.

**Pin (R): m8 `96789b9be`** (the I19-2a evidence commit; I19-2a shipped nothing, so R is that head with no
default changed). The branch `rst-b` is rebased onto it (done: C' `7134445ff`, P `50393ebe6`, naming fix `3e4b2f87b`,
rider `02c5a6408`; hashes are of the rebased branch and are re-read from `git log` at the freeze). If m8 moves
before the freeze the rebase is redone and the hashes here are re-stated in the frozen file; R is then the new
head.

## 1. The change (two commits of behaviour on R)

**C' -- reporting, path-neutral by construction** (commit `7134445ff`):

- `SqpCounters::restoration_factorizations` (the phases' factorizations), `restoration_unproductive_entries`
  (later requests with no step from a solved subproblem accepted since the previous resume) and
  `restoration_no_progress_requests` (later requests the progress test finds inadmissible at its default
  tolerance of 1; counted, never acted on in C').
- `StopReason::kRestorationNoProgress` (18), verdict `kStalled`, or `kAcceptable` through RF-1 section 1.2's
  relabel; never `kInfeasible`, `infeasibility_certified` untouched.
- The existing zero-accepted-step refusal (formerly `numerical_error` / `restoration_refused`, trace
  `s = entry_rule`) is relabelled `stalled` / `restoration_no_progress`, trace `s = no_progress`. The entry cap
  keeps `numerical_error` / `restoration_refused` / `entry_rule` and outranks it. Status-only: same point,
  same path, same work.
- Trace schema: one freeze-table row (new enum value, new stop reason, three trailing counter keys).

**P -- path-moving** (commit `50393ebe6` + naming fix `3e4b2f87b` + rider `02c5a6408`):

- **F1.** The R4 trigger also reads `zero_motion_exhausted`. The E2 elastic route is unchanged (registered).
- **F2.** `advanced.qp_retry_regularization_first` (bool, default **false**): R4 ahead of R2/R3 after a
  dependent-blocker or zero-motion termination. Off by default so F2 and F2' are both arms.
- **F3.** The rung-exhausted floor exit requires `shrink_hits_floor(delta)`. Above the floor the same exit is
  labelled honestly (`numerical_error` / `subproblem_failure`, or `stalled` / `subproblem_iteration_limit`):
  label-only, no new restoration request.
- **B1.** `advanced.max_unproductive_restoration_entries` (Index >= 0, default **1**), `RestorationEntryLedger`:
  a later request is refused (`stalled` / `restoration_no_progress`) once more consecutive re-entries than the
  tolerance followed intervals with no accepted step from a solved subproblem (main QP `kOptimal`, optimal
  elastic rung, SOC); a failure step does not count; a productive interval clears the run; the first entry is
  never tested.
  **Rider (settler ruling): a rolled-back non-finite accept is no progress.** A step the loop accepts and the next
  major rolls back because its point is non-finite (R2-1) was credited to the ledger at its commit; the rollback
  withdraws the credit (`RestorationEntryLedger::retract_last_accept`), so an interval whose only accepts were
  rolled back is unproductive and feeds `restoration_unproductive_entries` and the B1 refusal (and, through the
  same counters, B2's reading) like any unproductive entry. Path: only a solve that both rolls back and has
  re-entered restoration; every other path is bit-identical to the P commit.
- **B2.** `advanced.restoration_max_total_factorizations` (`optional<Index>`, default **unset = derived, on**):
  at each entry `max(20000, rho x main-loop factorizations)`, `rho = kRestorationTotalWorkRatio` (restoration.h);
  each phase run gets `min(what is left, the caller's remaining ceiling)`; a cut is `stalled` /
  `restoration_work_limit` (the reduction resume reads it), the caller's ceiling stays `work_limit`.

**rho (declared parameter).** Committed value 4. Rule, pre-registered: rho is the smallest of {2, 4, 8} under
which no cell solved (optimal or acceptable) in the C' arm of TRAIN-238 or PILOT-62 would be cut, computed
offline from the C' rows as `restoration_factorizations <= max(20000, rho x (factorizations -
restoration_factorizations))` (the final-count reading of the per-entry rule; a cell within 10% of its bound
is listed). If that rule gives rho != 4, a one-line constant commit (declared break, rides this declaration's
`-r2`) precedes legs T/P/U/N of arm F+B1+B2; the other arms do not read rho.

## 2. Arms

All on one hven install per binary (Release, sequential MKL, no LTO, clang, `-DHVEN_FP_MODE=SAFER_FAST`). An
arm is a binary plus the option overrides the harness passes (the TU-2/i19 override keys; any new key is
listed in `legs/` and probe-checked against `dump_options`).

| arm | binary | overrides |
|---|---|---|
| R | the pin | none |
| C' | C' commit | none |
| F (F1+F3) | P commit | `max_unproductive_restoration_entries = 1000000000`, `restoration_max_total_factorizations = 1000000000000` |
| F2 | P commit | as F, plus `qp_retry_regularization_first = true` |
| F2' | P commit | as F, plus `max_consecutive_qp_failures = 4` |
| F+B1 | P commit | `restoration_max_total_factorizations = 1000000000000` |
| F+B1+B2 | P commit | none (the shipped default) |

## 3. Legs and bars

Every step under `~/.local/bin/box-lane --` on ONE lane (never `--exclusive`; no leg asserts wall-clock).
Evidence in `gate-RST-B/`. Co-run protocol (CLAUDE.md section 7): legs T, P, U, N co-run one process per pinned
PHYSICAL core of the lane (SMT siblings idle), `MKL_NUM_THREADS=1`, two arms at once; the columns asserted are
counters, statuses and objectives; wall-clock is informational and never quoted. A CAPPED cell, a 900 s kill, or
any deviation that decides a bar is a candidate: re-run ALONE under the serial rule before it is called one.

| leg | instrument | bar |
|---|---|---|
| C | ctest Release and Debug at the P commit (`legs/ctest.sh`) | "100% tests passed" in both; counts reported |
| M | `legs/mutations_rstb.sh` with `legs/mutate_rstb.py`, control first | parity preflight passes (section 4); control clean; every must-kill id killed by its declared killer; every argued id (none declared) reports its outcome as ARGUED; `VERDICT MUTATIONS_AS_DECLARED` plus the killer check |
| PE | the paired perturbation ensembles (section 3a): 6 members (3 on NLP-46 exclusive) for R and for every arm, harness-applied, over T, P, U, N | R's ensemble first: classes, floor and power gate frozen from it before any arm row; the arms' tables read per 3a items 5-8; rows in `gate-RST-B/PE/` |
| N0 | replay R vs C' (`scripts/compare_replay.py`, walk/ssn/ipm) | **0 path movers**: byte-exact on every deterministic column but the three new counters, the status/stop_reason of a cell whose last event is the relabelled refusal, and `message`; each such relabel listed with its cell |
| T | TRAIN-238 (119 hven-sqp roster cells x {default, sqp-qn}, CAP 300 s), every arm vs R | **C' vs R: 0 path movers** (statuses move only `numerical_error` -> `stalled`/`acceptable` beside the relabel; majors, factorizations, f identical). **Every P arm: no robust loss** (section 3a item 6) on strict-optimal and on optimal+acceptable; 0 false infeasibility certificates; every status mover listed with its reason (rung, floor label, B1 refusal, B2 cut); factorizations reported per mover |
| P | PILOT-62 (`pilot_cells.tsv`, budget 300 iterations, 900 s kill reported), every arm vs R | as T; BRITGAS 019-027, DALLASM 030, YORKNET 054-061 reported by row with restoration counters |
| U | TU-2 48 (BRITGAS, NET3, SAWPATH, YORKNET x {default, sqp-qn} x A0-A5; `--budget-facts 250000`, `--budget-iters 300`), every arm vs R | as T. Readings, not bars: DALLASM-shape refusal under B1, p025 under F1+F2 |
| N | NLP-46 (cutest-nlp-35's 23 TRAIN problems x {default, sqp-qn}, TU-2 terms) every arm vs R | as T |
| S | install smoke and export contract | rc 0 |
| CI | `gh workflow run ci.yml --ref rst-b`, `gh run watch <id> --interval 120` | four lanes green; off-reference values recorded as observed, Apple values not observed are UNOBSERVED |

**Bars, summarised:** no robust loss (section 3a: strict-optimal, and optimal+acceptable, by solve probability) in any P
arm against R, every sign and net condition stated against the paired-ensemble floor (section 3a);
0 false certificates; every status mover listed with its reason; C' shows 0 path movers. The arm that
ships as default is chosen from these rows by the owner; the readings expected from the diagnosis (DALLASM 030
refused near major 6, p022 near major 18, p025/A0 inert under B1) are readings, not bars.

## 3a. Paired perturbation ensembles (settler ruling C539, replacing C537(5)'s placebo text; binding on every declared round)

Background: in I19-2a a single radius placebo moved 198 live rows and lost three optimal cells on NLP-46 alone, and
failed the "no -1 family sign" condition itself (`../2026-11-m8-i19-2a/gate-I19-2a/RESULTS-I19-2a.md`); the Fable
review of that round (`.superpowers/i19/i19-2a-results-review-fable.md`, B2, B4, I1) showed that a |net| floor from
one placebo understates noise, that a baseline-only ensemble leaves each arm's own draw a coin flip, and that a
radius-only placebo cannot move cells where the first radius never binds. This round is stated to that rule, declared
before any row exists. The perturbation is applied in the HARNESS; **the binary under test is untouched** (no placebo
arm of the library exists, no option is added for it).

**DEPENDENCY.** The ensemble harness (seeded perturbation, member runs, pairing by seed, class and floor computation,
the vote and probability tables) is built separately on hven-bench branch `ens`. This declaration names its scripts
`TODO-HARNESS-ENSEMBLE-RUN`, `TODO-HARNESS-CLASSES`, `TODO-HARNESS-TABLES`; the freeze is conditional on those names
being replaced by real script paths and their shas being recorded in `gate-RST-B/BINARIES.txt`. Nothing here is run
until they exist.

**1. Members.** Six runs per cell (member 0 unperturbed, members 1-5 seeded) for the baseline R AND for every arm,
**paired by seed**: member j of an arm and member j of R receive the identical perturbation. Three members (0, 1, 2)
on NLP-46's exclusive tier. Seeds are fixed in the frozen file (`TODO-HARNESS-ENSEMBLE-RUN` holds the table).

**2. Perturbations** (both applied by the harness at the model boundary; two entry points):
- *Primary:* a seeded relative perturbation of x0 of size about 2^-48 per component, **projected into the bounds**
  (a component at a bound stays on it).
- *Secondary:* `trust_region_initial = 1.0 +- k*1e-15`, k seeded (the existing option; harness override only).
A member uses the primary on every cell; the secondary is the second family of members 3-5 (members 1-2 primary
only, members 3-5 primary and secondary), so a radius-only effect is measured where it can act and not otherwise.
The ensemble reports the two entry points' movements separately as well as pooled.

**3. Cell classes, from the BASELINE ensemble, frozen before any arm row exists.** `gate-RST-B/classes.txt` is
written from R's ensemble alone and its sha recorded before the first arm row is read.
- STABLE: same status in all members, and majors, linear-algebra work and evaluation work all tie under the 10% rule.
- COST-UNSTABLE: same status in all members, work differs.
- STATUS-UNSTABLE: the status differs between members.
The **class counts (STABLE / COST-UNSTABLE / STATUS-UNSTABLE) per corpus and family, for R and for every arm, are a
declared endpoint of this round** (the instability is a solver-quality defect to fix, not only to measure around);
an arm that moves cells from STABLE to a worse class is reported as such. They are reported, not a ship bar, except
as stated in 5 and 6.

**4. Null distribution.** The 15 pairs of baseline members give, per family and pooled, the distribution of net wins
(per currency) and of solved-count differences under no change. `floor_f` = the largest absolute value in it
(`TODO-HARNESS-TABLES`). Computed from R's ensemble alone, frozen with the classes.

**5. Cost.** `T_j` = net wins of arm member j against baseline member j, per seed j.
- A pooled "+" requires `T_j > pooled floor` in at least 5 of 6 seeds, on one currency, with the median geomean at
  most 0.97 (3 members on NLP-46: all 3).
- A family is "-" only if `T_j,f < -floor_f` in at least 5 of 6 seeds; "+" symmetrically.
- **One vote per problem name.** TRAIN-238, CAP-238-1 and CAP-238-01 are the same 119 problems: a problem counts
  once, as the median over the corpora it is in. Counts in the tables are per problem name.

**6. Solved, as probabilities.** `p_cell` = fraction of members strict-optimal (and, separately, optimal+acceptable).
PILOT-62 single-draw counts are uninterpretable (the I19-2a radius placebo moved 60 of 62 pilot cells): PILOT-62 is
read only through `p_cell`, never through a single run.
- A ROBUST LOSS: baseline `p >= 5/6` (3/3 on NLP-46) and arm `p <= 3/6` (0/3 on NLP-46). **None is allowed.**
- Per family: `sum p(arm) >= sum p(R) - 0.5`.
- Robust gains are reported symmetrically, in a table beside the losses (gained cells are never omitted).

**7. Tail guard.** Applies only to cells the baseline solves strict-optimal in at least 5 of 6 members: the arm's
median majors/factorizations must not exceed `max(baseline max + 20, 3 x baseline max)`. A cell the baseline does not
solve cannot be a tail hit. Gained cells are listed beside lost cells.

**8. Same-solution condition.** A cost win or loss on a cell requires `|df| <= 1e-6 * max(1, |f|)` between the arm
and baseline members (median f). Otherwise the cell is scored in its own column, "different solution: better /
worse objective", and enters neither net.

**9. Power gate (BEFORE the freeze).** The number of baseline-STABLE, solved cells that exercise restoration
(`restoration_entries > 0` in R, from the C' counters on the R ensemble, per family and per currency) is stated in the
frozen file. If it is below 20 distinct problem names, the round is declared EXPLORATORY: its bars are read as
reports, no arm is shipped on it, and the corpus is enriched (the 2-point restoration probes from the diagnosis, the
DALLASM/YORKNET/BRITGAS shapes) before a confirmatory round. The expected count is not assumed here; the baseline
ensemble measures it first.

**Other conditions of this round read through the ensemble.** Every "no lost solve" in sections 3 and 4's tables means
the robust-loss rule of 6; every "path mover" for an arm other than C' is reported with its class (STABLE movers are
real; COST/STATUS-UNSTABLE movers need the paired comparison of 5). The C' arm keeps its stricter bar (0 path
movers, byte-exact on member 0) because it changes no path. 0 false infeasibility certificates is not relative to
anything and holds in every member of every arm.

The ensemble rows, `classes.txt`, the floor table, the vote and probability tables are published in `gate-RST-B/PE/`
and `gate-RST-B/floor.txt`. No bar is edited after the baseline ensemble is read.

## 4. Mutations and must-kills

All must-kills (control first). Declared killers (other killers allowed as well):

| id | mutation | declared killer(s) |
|---|---|---|
| B1a | a failure step counted as progress | `RstbEntrySeam.FailureStepOnlyCyclesAreRefusedAtTheSecondUnproductiveReEntry` |
| B1b | a productive interval does not clear the run | `RstbEntryLedger.ASolvedStepMakesTheIntervalProductiveAndClearsTheRun`, `RstbEntrySeam.AProductiveIntervalClearsTheRun` |
| B1c | `<=` read as `<` | `RstbEntryLedger.FailureStepsAloneAreUnproductiveAndTheSecondRunIsRefusedAtOne`, `RstbEntrySeam.FailureStepOnlyCyclesAreRefusedAtTheSecondUnproductiveReEntry` |
| B1d | the first entry tested | `RstbEntryLedger.TheFirstRequestIsNeverTested`, `RstbEntrySeam.ToleranceZeroRefusesTheFirstReEntryAndALargeOneNone` |
| B1e | refusal spelled numerical_error / restoration_refused | `RstbEntrySeam.FailureStepOnlyCyclesAreRefusedAtTheSecondUnproductiveReEntry`, `SqpLadderSeam.L1ARefusedRequestAtAFeasiblePointIsNeverInfeasible` |
| B1f | refusal spelled kInfeasible | as B1e |
| B1g | refusal sets the certificate | `RstbEntrySeam.FailureStepOnlyCyclesAreRefusedAtTheSecondUnproductiveReEntry` |
| B1h | an optimal-elastic-rung accept not counted | `RstbEntrySeam.AnElasticRungAcceptIsProgress` |
| F1 | R4 trigger ignores zero_motion_exhausted | `Rf1RetryLadderSeam.AZeroMotionExhaustionReachesTheRegularizationRung` |
| F2 | regularization-first ignored | `Rf1RetryLadderSeam.AZeroMotionExhaustionReachesTheRegularizationRung`, `RstbRetryLadder.RegularizationFirstTakesR4AheadOfTheColdAndAlgebraRungs` (unit, if F2 is in `choose_retry_rung`) |
| F3 | floor label without the floor | `Rf1RetryLadderSeam.ANoMotionFailureAboveTheFloorIsTheSubproblemFailure` |
| B2a | B2 cut spelled kWorkLimit | `RstbBudgetSeam.ACumulativeBudgetCutIsTheRestorationWorkLimit` |
| RB1 | a rolled-back accept keeps its progress credit (the retraction dropped) | `RstbEntrySeam.ARolledBackAcceptIsNoRestorationProgress`, `RstbEntrySeam.ARolledBackAcceptFeedsTheUnproductiveCountAtTheDefaultTolerance` (the ledger unit pins test `retract_last_accept` itself, not its call, and do not kill this mutation) |
| B2b | caller's ceiling spelled kRestorationWorkLimit | `RstbBudgetSeam.TheCallersSmallerCeilingStaysTheWorkLimit`, `Rf1RestorationSeam.TheCallersCeilingInsideThePhaseStaysTheWorkLimit` |

**Argued survivors: none declared.** Every id above is a must-kill. The runner holds an `ARGUED` list that MUST equal
the declared argued-survivor set; a PARITY PREFLIGHT, run before any build, reads this table (rows whose killer cell
begins `ARGUED`) and the runner's list, and also compares this table's ids against `mutate_rstb.py list`; any
difference aborts the leg (`PARITY_FAIL`) before the control. (I19-2a's runner listed two argued survivors where its
table declared five; its verdict string disagreed with the frozen table. This round cannot.)

The development dry run (on the pre-rebase head, not evidence) is in the implementer's report; the leg is run
again on the pinned head.

## 5. Declared breaks

- RF-1's `Rf1RetryLadderSeam.ANoMotionFailureAtAFeasiblePointEndsAtTheFloor` read `trust_region_floor` at
  Delta = 1 (ten orders above the floor): re-pinned with `trust_region_min = 0.75` so the floor is reached, and the
  above-floor case is the new `ANoMotionFailureAboveTheFloorIsTheSubproblemFailure` (F3).
- `SqpLadderSeam.L1A...` and `ARequestWithNoAcceptedStepSinceTheResumeIsRefusedAtAnyCap` read
  `numerical_error` for the zero-accept refusal: now `stalled` / `restoration_no_progress` (C').
- Trace golden `sqp_complete.jsonl` and the counters unit lines: three trailing keys (C').
- `HsSweepCorpus.Corpus` and `Hs15WarmKdOffWithTheFailedSubproblemStep` (F1: HS15's warm failing chain reaches R4):
  reference Release 22/40 -> 23/30 full-step/majors, held-off arm 40 -> 30 majors (stop f 1.0038 -> 1.0652, the
  near-origin tolerance widened to 0.1), step arm 43/149/64/6 -> 44/153/65/7, corpus minors 3406 -> 3359; Debug
  21/39 -> 21/40, 39 -> 40, 3392 -> 3396. Off-reference columns are UNOBSERVED under RST-B (CI decides).
- F3 also moves a subproblem-failure trace line: the rung-exhausted exit above the floor is written as the ending
  warning (it was a notice beside the floor label).

## 5a. BINARIES plan

One install per binary, built on the lane, `HVEN_FP_MODE=SAFER_FAST`, Release, no LTO; sha256 recorded in
`gate-RST-B/BINARIES.txt` before any leg and re-checked at the end (`legs/binaries.sh`, named before the freeze).

| name | commit | used by |
|---|---|---|
| R | `96789b9be` (m8 pin; re-stated at the freeze) | R rows and R's ensemble (members perturbed by the harness; the binary is the same) |
| C' | `7134445ff` | N0, C' rows |
| P | the rider commit (rebased head of `rst-b` at the freeze) | arms F, F2, F2', F+B1, F+B1+B2 (overrides per section 2) |

The rider changes nothing on a path without a non-finite rollback after a restoration resume; the frozen file
records which of T/P/U/N rows (if any) differ between the P commit before the rider (`50393ebe6`+naming) and after it,
by a replay of the F+B1+B2 arm on the two binaries (leg N0r: 0 differences expected, any difference listed).

## 6. Box time (one lane, estimates)

Builds: 3 installs + harness rebuilds, about 1.5 h. C (Release + Debug ctest): about 1.5 h. M: 14 rebuild+run
cycles, about 2 h. T: 238 rows x 7 arms co-run two arms at a time on 4 cores, about 6-8 h. P: 62 cells x 7 arms,
about 4 h (the 900 s kill dominates). U: 48 x 7, about 4 h at the 250k-factorization budget. N: 46 x 7, about
2 h. N0 replay, S, CI: about 1.5 h. PE: six members for R and every arm (about 6x the single-pass legs; 3 on NLP-46 exclusive) -- the legs above are the single-pass figures and the ensemble multiplies the TRAIN/CAP-class legs, so the total is re-estimated when the harness exists and stated in the frozen file; the single-pass total is **about 22-25 lane-hours**, plus solo re-runs of candidate cells.

## 7. Not run

HOLDOUT (never). Wall-clock. The QP walk census. hven-ipm rows (no code path). B1b (envelope filter on entry
points) and the iteration-share B2 variant (registered). D4 (kTier elastic rung exhausts at rung 0; infeasible
verdict at h <= feas_tol routed to elastic) is registered, not designed here. The E2-route F1 extension is
registered.
