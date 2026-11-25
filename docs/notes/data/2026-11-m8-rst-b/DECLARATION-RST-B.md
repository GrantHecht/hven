# DECLARATION-RST-B -- SQP restoration and retry-ladder fixes, and a per-solve restoration bound (DRAFT)

**UNFROZEN DRAFT.** Committed on branch `rst-b` for review only; not frozen, no leg has run. It is committed, with every leg script it names (`legs/`), before any leg and never
edited afterwards; a failed or void leg chain gets a `-r2` file and this one is kept. Criteria are never
changed after data. Precedent: `../2026-11-m8-rf1/DECLARATION-RF-1c.md`. Design and diagnosis:
`.superpowers/rst-diag-2026-11-24.md`, sections (a)-(d), accepted as designed by ruling C514.

**Pin (R): m8 `99712ec32`** (the census pin; it differs from the I19-2a evidence head `96789b9be` only by TU-3
docs: `git diff --stat 96789b9be 99712ec32 -- src include bench tests` is empty, verified). I19-2a shipped nothing, so R is
that head with no default changed. The branch `rst-b` is rebased onto it: C' `46761076f`, P `5375bfbf5`, naming fix
`70d67680f`, rider `27384f25a`, entry-rule `795504e49` (hashes of the rebased branch, re-read from `git log` at the
freeze). If m8 moves before the freeze the rebase is redone and R is restated in the frozen file.

## 1. The change (two commits of behaviour on R)

**C' -- reporting, path-neutral by construction** (commit `46761076f`):

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

**P -- path-moving** (commit `5375bfbf5` + naming fix `70d67680f` + rider `27384f25a` + entry-rule `795504e49`):

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
  same counters, B2's reading) like any unproductive entry. **The entry rule too (settler ruling C543):** the rule's
  "a step accepted since the resume" reads the ledger's kept-accept count (`kept_accepts()`), which the same
  retraction decrements; a request after an interval whose accepts were all rolled back is refused (`stalled` /
  `restoration_no_progress`, below the cap). The public `steps_accepted` counter is NOT decremented and still counts
  those steps. Path: only a solve that both rolls back and has
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

**HARNESS (pinned).** The ensemble harness is hven-bench branch `ens-reader` at **`27c02687d9389ed43f3dbef27b4cbe389ca2b5d0`** (final `ens`, settler C558; lanes tree equal to the `eca19b8` the lanes were built at; settler C552 named `37e24bb`, which `eca19b8` descends from, and C554/C556 added `set=` and the lane overrides; the settler restates the final pin;  the freeze records the harness sha and STOPS on mismatch). The census rows R's ensemble consists of
were produced by `ens` `6e42a30`; the `lanes/` tree is identical at `e4c1a1d`, `6e42a30` and `37e24bb`
(`git rev-parse <c>:lanes` = `386d43a5d7d591ff869bcd1f1c9b77a6159c036f`). Rule: its `docs/ensemble-rule.md` (section
"Binary arms and the declared-binaries file"); mechanism `tu/ens.py`, `tu/ens_members.py`; tests `tests/test_ens.py`.
Commands used by the round (the store `S` and paths are the frozen file's):

- baseline ensemble and arm members: `python3 tu/ens.py arms <arm> --m 5 --kind x0` lists the member arm names
  (`<arm>@x1`..`@x5`); each runs through `tu/i19_run.py arm <name> --store S --binary <lane binary> ...`; the census
  runner scripts `scripts/census_arm.sh <arm> <corpora> <phase>`, `scripts/census_chain.sh <logdir> <step>...`
  (steps `arm:ARM:CORPORA:PHASE` and `pilots:ARM,...`) and `scripts/census_pilots.py run ...` (PILOT-62, several member
  arms in one lane acquisition, one pinned core each) are the reference runners;
- `python3 tu/ens.py freeze --store S --arm H0 --corpora TRAIN-238,CAP-238-1,CAP-238-01 --m 5 --kind x0 --out FREEZE.json`
  (classes, `p_base`, floors, provenance, digests; committed before the first arm row);
- `python3 tu/ens.py freeze --store S --arm H0 --corpora NLP-46 --out FREEZE-NLP46.json` (its own freeze);
- `python3 tu/ens.py read --store S --base H0 --arm <arm> --freeze FREEZE.json` (the decision read, per arm);
- `python3 tu/ens.py power --store S --arm H0 --freeze FREEZE.json --event "restoration_entries>=1"` (the power gate).

`H0` is the harness's name for the baseline arm; here it is R. RST-B's arms are binary arms: each is DECLARED in the
declared-binaries file (section 5a), `ens.py read` takes `--binaries FILE`, and the certificate leg runs each arm with its
own `bin=`. The arms' option overrides (section 2's table: `max_unproductive_restoration_entries`,
`restoration_max_total_factorizations`, `qp_retry_regularization_first`, `max_consecutive_qp_failures`) are not levers of
the I19 `options=` table; the harness (`eca19b8`) takes them as `set=key:value,...` on the `declare` line, folded into the arm's
label so the loader's full-label check covers them, and its lane carries them requires-gated (section 5a has the lines).

**R's ensemble IS the baseline census (settler ruling C547).** R's ensemble rows are the rows of the m8-head baseline
census, not a fresh run: hven-bench `ens` `6e42a30` (the census harness; every census row carries
`# harness.sha: 6e42a30...`), hven pin `99712ec324716e6d08ee0545a43d595ca15eec3f`, lane binary
`3155a0834e9e9e7748fe51b818acafc5fd3ff502bf86e1f58ef2bb165f995bc0` (recorded in the census `BINARIES.txt` and in each
row's `# lane.binary.sha256`), store `/home/ghecht/Projects/hven-bench-ens/.scratch/census-m8/store`. The freeze verifies
that identity (provenance equal across every baseline member; the arm builds against the same pin) and STOPS otherwise.
The census binary is the m8 head WITHOUT C': its side files lack the C' counters (`restoration_unproductive_entries`,
`restoration_no_progress_requests`, `restoration_factorizations`), which exist only on the arms; comparisons read them as
absent-in-R and use `restoration_entries`, `restoration_iters` and the existing columns for R.

**1. Members and the decision ensemble.** The DECISION ensemble is the `x0` kind: member 0 (unperturbed) plus x1..x5, six
runs, for the baseline R AND for every arm, **paired by seed** (member `@xj` of an arm and of R receive the identical
perturbation). NLP-46 is its OWN ensemble: member 0 plus x1, x2 (three runs), its own freeze, floors (3 baseline pairs)
and votes (3 of 3), never pooled with TRAIN/CAP. Seeds are the harness's (`tu/ens_members.py`, splitmix64 on problem
name and seed).

**2. Perturbations** (applied by the harness in `hven_bench_cell`; the binary under test is untouched):
- *Decision (`x0` kind):* x0_j += 2^-48 * c_j * max(1, |x0_j|), c_j in [-1, 1) seeded, clipped into the variable box;
  only coordinates inside their box move.
- *Secondary robustness REPORT (`tr` kind, members t1..t5):* `trust_region_initial` = base + sign * k * ulp(base),
  k = ceil(seed/2), sign + for odd seeds and - for even; a perturbed value equal to the base is refused. The `tr`
  ensemble is run only for R and the arm(s) the owner names, is read and reported as a separate ensemble (its class
  counts and net signs), and is **not a decision input**: no bar of this round reads it.

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
(`tu/ens.py freeze`). Computed from R's ensemble alone, frozen with the classes.

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
(`restoration_entries >= 1` in member 0, in any member and in all members, as `tu/ens.py power` prints it, per family) is stated in the
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
| RB2 | the entry rule still counts a rolled-back accept (kept-accept count not decremented) | `RstbEntrySeam.ARolledBackAcceptIsNotAnAcceptedStepForTheEntryRule`, `RstbEntryLedger.KeptAcceptsCountOnlyAcceptsThatWereNotRolledBack` |
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

## 5a. BINARIES plan and the declared-binaries file

One install per binary, built on the lane, `HVEN_FP_MODE=SAFER_FAST`, Release, no LTO, clang. The declared-binaries file
`gate-RST-B/BINARIES.txt` (format: ensemble-rule.md at `eca19b8`; only its `declare` lines are read) is written, with every
sha filled from the builds, before any arm row, and re-checked at the end.

| name | commit | used by |
|---|---|---|
| R | `99712ec32` (m8 pin = the census pin) | R rows = the census rows |
| C' | `46761076fbbdbc616a015ff4570b082443a454ab` | N0, C' rows |
| P | `795504e49c82208d3c97039fbf68b0bda32a4689` (the code head of `rst-b`; later commits are docs only; re-read at the freeze) | arms F, F2, F2', F+B1, F+B1+B2 (overrides per section 2) |

The plan, with the builds made (C' and P installs and lanes built by `scripts/census_build.sh` at the harness head
`eca19b8` under `box-lane`, on disk in the `rst-b` worktree's `.scratch/`; the `set=` syntax is the harness's, ensemble-rule.md
at `eca19b8`; nothing below is a result):

    declare H0   prefix=/home/ghecht/Projects/hven/.claude/worktrees/census/.scratch/install 99712ec324716e6d08ee0545a43d595ca15eec3f 3155a0834e9e9e7748fe51b818acafc5fd3ff502bf86e1f58ef2bb165f995bc0
    declare C'   options=H0 prefix=/home/ghecht/Projects/hven/.claude/worktrees/rst-b/.scratch/inst-C/install base=99712ec324716e6d08ee0545a43d595ca15eec3f bin=/home/ghecht/Projects/hven/.claude/worktrees/rst-b/.scratch/lane-C/lanes/hven/hven_bench_cell lane_src=4319ab1512e2de6dbc385eb964be182f0af4eb70 46761076fbbdbc616a015ff4570b082443a454ab 054aac010942a2e7648ebc0b48b590330d64cb2b12e7d655f0ad0fd145e88c7d
    declare F        options=H0 set=advanced.max_unproductive_restoration_entries:1000000000,advanced.restoration_max_total_factorizations:1000000000000 prefix=/home/ghecht/Projects/hven/.claude/worktrees/rst-b/.scratch/inst-P/install base=99712ec324716e6d08ee0545a43d595ca15eec3f bin=/home/ghecht/Projects/hven/.claude/worktrees/rst-b/.scratch/lane-P/lanes/hven/hven_bench_cell lane_src=4319ab1512e2de6dbc385eb964be182f0af4eb70 795504e49c82208d3c97039fbf68b0bda32a4689 fb25fbf53f2c6d5189452b221ee17114ff1389cd20a2b1e226ff764331a02321
    declare F2       options=H0 set=advanced.max_unproductive_restoration_entries:1000000000,advanced.restoration_max_total_factorizations:1000000000000,advanced.qp_retry_regularization_first:true prefix=/home/ghecht/Projects/hven/.claude/worktrees/rst-b/.scratch/inst-P/install base=99712ec324716e6d08ee0545a43d595ca15eec3f bin=/home/ghecht/Projects/hven/.claude/worktrees/rst-b/.scratch/lane-P/lanes/hven/hven_bench_cell lane_src=4319ab1512e2de6dbc385eb964be182f0af4eb70 795504e49c82208d3c97039fbf68b0bda32a4689 fb25fbf53f2c6d5189452b221ee17114ff1389cd20a2b1e226ff764331a02321
    declare F2'      options=H0 set=advanced.max_unproductive_restoration_entries:1000000000,advanced.restoration_max_total_factorizations:1000000000000,advanced.max_consecutive_qp_failures:4 prefix=/home/ghecht/Projects/hven/.claude/worktrees/rst-b/.scratch/inst-P/install base=99712ec324716e6d08ee0545a43d595ca15eec3f bin=/home/ghecht/Projects/hven/.claude/worktrees/rst-b/.scratch/lane-P/lanes/hven/hven_bench_cell lane_src=4319ab1512e2de6dbc385eb964be182f0af4eb70 795504e49c82208d3c97039fbf68b0bda32a4689 fb25fbf53f2c6d5189452b221ee17114ff1389cd20a2b1e226ff764331a02321
    declare F+B1     options=H0 set=advanced.restoration_max_total_factorizations:1000000000000 prefix=/home/ghecht/Projects/hven/.claude/worktrees/rst-b/.scratch/inst-P/install base=99712ec324716e6d08ee0545a43d595ca15eec3f bin=/home/ghecht/Projects/hven/.claude/worktrees/rst-b/.scratch/lane-P/lanes/hven/hven_bench_cell lane_src=4319ab1512e2de6dbc385eb964be182f0af4eb70 795504e49c82208d3c97039fbf68b0bda32a4689 fb25fbf53f2c6d5189452b221ee17114ff1389cd20a2b1e226ff764331a02321
    declare F+B1+B2  options=H0 prefix=/home/ghecht/Projects/hven/.claude/worktrees/rst-b/.scratch/inst-P/install base=99712ec324716e6d08ee0545a43d595ca15eec3f bin=/home/ghecht/Projects/hven/.claude/worktrees/rst-b/.scratch/lane-P/lanes/hven/hven_bench_cell lane_src=4319ab1512e2de6dbc385eb964be182f0af4eb70 795504e49c82208d3c97039fbf68b0bda32a4689 fb25fbf53f2c6d5189452b221ee17114ff1389cd20a2b1e226ff764331a02321

The census install is the census lane's own and carries no `lane_src` (the baseline is exempt: its lane is the clean `e4c1a1d`
build). Both new lanes were stamped `lane_src` `4319ab1512e2de6dbc385eb964be182f0af4eb70`. The five P arms share one build, one install and one lane binary.
Install `HVEN_PIN.txt` pins were read after the builds and equal the declared pins.

`lane_src=` is required because the lane binary links `libhven` statically, so each arm's binary sha256 differs from
the census lane's (`3155a083...`); here the lane SOURCE also differs from the census lane's: the harness commit
`eca19b8` added the RST-B option overrides to the lane (C556; `lanes/` tree `4319ab15...`, the census lane's was
`386d43a5...`), requires-gated so a binary without them refuses an override. The harness requires `lane_src` for any arm
whose binary differs from the baseline's and compares it. The five P arms share one build, one
lane binary and one install; they differ only in the option overrides (`set=`, per section 2's table). The reader checks `HVEN_PIN` equality for both prefixes, ancestry of each
declared pin from R (`99712ec32` is an ancestor of both), and prints the `src/include` diff-stat. Sequence: build C' and P
installs and their lanes (`scripts/i19_build_install.sh`, `scripts/i19_build_lane.sh` stamp `<binary>.lane-src`), fill the
shas, write the file, then freeze.

The rider changes nothing on a path without a non-finite rollback after a restoration resume; the frozen file
records which of T/P/U/N rows (if any) differ between the P commit before the rider (`5375bfbf5`+naming) and after it,
by a replay of the F+B1+B2 arm on the two binaries (leg N0r: 0 differences expected, any difference listed).

## 6. Box time (one lane, estimates)

Builds: 3 installs + harness rebuilds, about 1.5 h. C (Release + Debug ctest): about 1.5 h. M: 15 rebuild+run
cycles, about 2 h. T: 238 rows x 7 arms co-run two arms at a time on 4 cores, about 6-8 h. P: 62 cells x 7 arms,
about 4 h (the 900 s kill dominates). U: 48 x 7, about 4 h at the 250k-factorization budget. N: 46 x 7, about
2 h. N0 replay, S, CI: about 1.5 h. PE: six members for R and every arm (about 6x the single-pass legs; 3 on NLP-46 exclusive) -- the legs above are the single-pass figures and the ensemble multiplies the TRAIN/CAP-class legs, so the total is **re-estimated from the m8-head baseline census** (running from `ens` `e4c1a1d`; its per-leg box time is the instrument) when that census lands, and the frozen file states the figure and the census it came from (not yet available at this draft); the single-pass total is **about 22-25 lane-hours**, plus solo re-runs of candidate cells.

## 7. Not run

HOLDOUT (never). Wall-clock. The QP walk census. hven-ipm rows (no code path). B1b (envelope filter on entry
points) and the iteration-share B2 variant (registered). D4 (kTier elastic rung exhausts at rung 0; infeasible
verdict at h <= feas_tol routed to elastic) is registered, not designed here. The E2-route F1 extension is
registered.
