# DECLARATION-RST-B -- SQP restoration and retry-ladder defect fixes, and per-solve restoration bounds (DRAFT)

**UNFROZEN DRAFT (C562 rework).** Committed on branch `rst-b` for review only; not frozen, no leg has run. It is frozen
by committing it, with every leg script it names (`legs/`) and the freeze files of section 3a, before any arm row; it is
never edited afterwards; a failed or void leg chain gets a `-r2` file and this one is kept. Criteria are never changed
after data. Precedent: `../2026-11-m8-rf1/DECLARATION-RF-1c.md`. Design and diagnosis: `.superpowers/rst-diag-2026-11-24.md`
(sections (a)-(d), accepted by ruling C514); perturbation diagnosis `.superpowers/pdiv-diagnosis-2026-11-25.md` (C561);
pre-freeze review `.superpowers/rst-b-prefreeze-review-fable.md`; every ruling of C562 is implemented here.

**What this round is and is not.** RST-B fixes three defects on the restoration path (D1 the R4 trigger, D2 the false
floor label, D3 the re-entry progress test counting failure steps) and the rolled-back-accept rider, and it **bounds the
cost of the restoration amplifier** on failing branches (B1, B2). **It does not fix the walk-origin instability.** The
perturbation diagnosis puts the first split of an outcome change in the QP walk (ratio-test ties, the dependency test
near `kDepTol`, EQP directions from near-singular systems); RST-B touches none of those sites. Its B1 never tests the first
restoration entry and B2 does not gate entry, so the 14 restoration-entry first splits are mostly untouched; F1/F2 change
the retry ladder before a request and may move some, in an unknown direction. Where RST-B's budgets fire on a crawl, the
expected outcome change is `iteration_limit` -> `stalled` at lower cost: the solve is still lost, only the cost of the loss
drops. The origin belongs to the walk remedies the diagnosis ranks 1-3 and to QPC/QPD.

**Disclosures: criteria chosen after seeing baseline data (ruling C565).** Two criteria in this declaration were chosen
**after** the baseline-only (R = census) rows had been read, at a time when **no arm row of any kind existed** (no P or C'
member had been run on any corpus):

1. **The cost-of-failure statistic (section 3b).** The pre-review plan was a by-name net. On the census T+CAP rows its
   null floor is 12 names against a B1 benefit-side set of about 9-12 names, so no arm could have met it. The
   summed-factorization band statistic (5 of 6 seeds below R's pair band `[0.8231, 1.2150]` on cells unsolved in both,
   with the by-name net >= 0 as a guard) was chosen after that null was seen. It is accepted provisionally (C565(1)); the
   pre-freeze re-review rules on it explicitly.
2. **The 10-name benefit-side gate (section 9).** The threshold was set after the provisional benefit-side counts (12
   names: 9 on T+CAP, 3 on partial NLP-46) were seen. On the complete census (C576) the count is **14 names** (9 + 5),
   so the gate is met by 4; the NLP-46 part was partial when the threshold was set.

3. **The cost-of-failure concentration guard (section 3b; C577 M3)** and **the STABLE-SOLVED floor from x0 split-halves
   (section 3d; C577 M4, floor 2 names)** were set by the pre-freeze re-review after it had read the R-only census rows
   (SPANHYD's 22-40% share; the split-half null).

All of these were fixed from R's rows alone; no arm outcome was known or knowable when they were set. They are disclosed because
a criterion tuned against the baseline's own distribution is weaker than one fixed in advance, and a reader should weigh
them accordingly. Two further declared limits: **N1 is a proxy** (section 3), and **the PILOT-62 predictions are scored on
status and factorizations only** (section 3e), because those are the only result columns PILOT-62 rows carry.

**Pin (R): m8 `99712ec32`** = the census pin (hven `99712ec324716e6d08ee0545a43d595ca15eec3f`). R's rows are the baseline
census rows (section 3a): hven-bench `ens` `27c02687d9389ed43f3dbef27b4cbe389ca2b5d0`, lanes tree
`4319ab1512e2de6dbc385eb964be182f0af4eb70`, lane binary sha256
`7d2f9f376d6f9f82d554efc2a41a1e52a36769a061d02060c5a765d236c573ca`, store
`/home/ghecht/Projects/hven-bench-ens/.scratch/census-m8/store`. The branch `rst-b` is rebased onto R: C' `46761076f`,
P `5375bfbf5`, naming fix `70d67680f`, rider `27384f25a`, entry-rule `795504e49`, C562 counters `34a3348df`
(re-read from `git log` at the freeze). If m8 moves before the freeze the rebase is redone and R is restated.

## 1. The change

**C' -- reporting, path-neutral by construction** (commit `46761076f`): `SqpCounters::restoration_factorizations`,
`restoration_unproductive_entries`, `restoration_no_progress_requests`; `StopReason::kRestorationNoProgress` (18), verdict
`kStalled`, or `kAcceptable` through RF-1 section 1.2's relabel, never `kInfeasible`, the certificate untouched; the
existing zero-accepted-step refusal relabelled `stalled` / `restoration_no_progress` (trace `s = no_progress`), the entry
cap keeping `numerical_error` / `restoration_refused` and outranking it; trace schema: one freeze-table row.

**P -- path-moving** (commits `5375bfbf5`, `70d67680f`, `27384f25a`, `795504e49`, and the C562 counters commit):

- **F1.** The R4 trigger also reads `zero_motion_exhausted`. The E2 elastic route is unchanged (registered).
- **F2.** `advanced.qp_retry_regularization_first` (bool, default **false**): R4 ahead of R2/R3 after a dependent-blocker or
  zero-motion termination.
- **F3.** The rung-exhausted floor exit requires `shrink_hits_floor(delta)`; above the floor the same exit is labelled
  honestly (`numerical_error` / `subproblem_failure`, or `stalled` / `subproblem_iteration_limit`): label-only.
- **Rider.** A rolled-back non-finite accept is no progress: `RestorationEntryLedger::retract_last_accept`.
- **Entry rule (C543).** "A step accepted since the resume" reads the ledger's kept-accept count; the public
  `steps_accepted` is not decremented.
- **B1.** `advanced.max_unproductive_restoration_entries` (Index >= 0, default **1**): a later request is refused (`stalled` /
  `restoration_no_progress`) once more consecutive re-entries than the tolerance followed intervals with no accepted step
  from a solved subproblem (main QP `kOptimal`, optimal elastic rung, SOC); a failure step does not count; a productive
  interval clears the run; the first entry is never tested.
- **B2.** `advanced.restoration_max_total_factorizations` (`optional<Index>`, default **unset = derived, on**): at each entry
  `max(20000, rho x (factorizations - restoration_factorizations))` (the main loop's own; `restoration_total_budget`), each
  phase run gets `min(what is left, the caller's remaining ceiling)`; a cut is `stalled` / `restoration_work_limit` (the
  reduction resume reads it), the caller's ceiling stays `work_limit`. **rho = 4, fixed** (`kRestorationTotalWorkRatio`);
  no data-driven choice is made in this round (C562 I3).
- **B2's overshoot bound (C576; a declared property, not redesigned here).** The phase sub-solve reads its factorization
  ceiling at each major's exit test, before the next subproblem is built (the `work_exhausted` conjunction, on the
  sub-solve's own count). A phase run therefore spends **at most the budget it was handed plus the factorizations of one
  restoration major**, the one that crosses the line; every later phase run and entry is then handed 0 and stops before
  factorizing (pinned by `RstbBudget.ALaterEntryWithNoBudgetLeftIsCutBeforeItFactorizes`). One major is bounded by the
  QP caps and retry rungs, not by B2, so **20,000 is not a hard cap** and the overshoot can be large: **observed, TU-2 A0 BRITGAS default, arm F2'+B1+B2, member 0
  (smoke, P lane `afa3a040`): 24,417 restoration factorizations against a derived budget of 20,000** (the floor;
  main-loop factorizations 2,679), 22% over, the solve ending `stalled` / `restoration_work_limit` at major 1 with 27,096
  factorizations.
- **Declared B2 behaviour at a feasible entry point (C562 I4).** B2 never refuses a request. A phase entered at a point the
  convergence test already calls feasible converges at its start without factorizing and resumes (and resets the radius)
  whatever is left of the budget, even none; such re-entries are bounded by the entry cap (`max_restoration_entries` = 10)
  and by B1 only. Pinned by `RstbBudgetSeam.AZeroBudgetDoesNotRefuseAFeasibleEntryAndTheCapAndProgressTestBoundIt` and the
  must-kill FR1.
- **C562 counters (appended fields, allowed by C562 Q5).** `restoration_budget_cuts` (phase runs B2 cut where it was the
  smaller bound, resumed or not: B2 cuts are observable, I3); `restoration_no_accept_refusals` (later requests below the cap
  refused because no accepted step since the resume was kept: the entry rule's refusals, I6);
  `restoration_refusal_feasibility` (the convergence test's feasibility measure at the refused request, absent at +inf
  when none: h at every refusal, I9).
- The retry options are copied into restoration sub-solves through `ropts`, so F1, F2 and F2' also change phase paths.

**Attribution rule (C562 I6), from the counters.** A refusal ends the solve, so each row has at most one. Its kind is:
the entry cap iff the stop is `restoration_refused` with `restoration_entries = max_restoration_entries`; the entry rule
iff `restoration_no_accept_refusals = 1` (the progress test may also have found it inadmissible, counted in
`restoration_no_progress_requests`); B1 iff the stop is `restoration_no_progress` (or `acceptable` with that exit in its
message) and `restoration_no_accept_refusals = 0`. A B2 cut is `restoration_budget_cuts >= 1`, its exit
`restoration_work_limit` unless the entry then resumed.

**Behaviour changes declared** (section 5 lists the test re-pins): the HS15 warm chain reaches R4 under F1 (held-off arm
stop objective 1.0038 -> 1.0652, the near-origin tolerance widened to 0.1); F3 moves a subproblem-failure trace line.

## 2. Arms and candidates

One hven install per binary (Release, sequential MKL, no LTO, clang, `-DHVEN_FP_MODE=SAFER_FAST`); the P arms share one
build and differ only in the option overrides the harness passes (`set=`, section 5a). "off" for B1 is
`max_unproductive_restoration_entries = 1000000000`, for B2 `restoration_max_total_factorizations = 1000000000000`.

| arm | binary | overrides | role |
|---|---|---|---|
| R | the pin | none | baseline (= the census) |
| C' | C' commit | none | N0; diagnostic |
| F (F1+F3+rider+entry rule) | P | B1 off, B2 off | **candidate K0** (part (i) alone) |
| F2 | P | B1 off, B2 off, `qp_retry_regularization_first = true` | diagnostic; N4 base |
| F2' | P | B1 off, B2 off, `max_consecutive_qp_failures = 4` | diagnostic; N5 base |
| F+B1 | P | B2 off | diagnostic; N2/N3 |
| F+B1+B2 | P | none (P's defaults) | **candidate K1** |
| F2+B1+B2 | P | `qp_retry_regularization_first = true` | **candidate K2** (C562 B2) |
| F2'+B1+B2 | P | `max_consecutive_qp_failures = 4` | **candidate K3** (C562 B2) |

**The candidates are fully specified defaults**: shipping Kn means the shipped defaults are exactly that arm's settings.
Diagnostic arms never fail the round and never ship.

## 3. Legs and bars

Every step under `~/.local/bin/box-lane`. Evidence in `gate-RST-B/`. **Tier assignment: the census's, verbatim**
(`/home/ghecht/Projects/hven-bench-ens/.scratch/census-m8/CENSUS.md`):

> co-run protocol (CLAUDE.md section 7): every leg one box-lane acquisition; co-run width 4 = one process per pinned
> physical core of the lane, SMT siblings idle, MKL_NUM_THREADS=1; wall-clock informational, never asserted. NLP-46: the
> 10 declared solo problems (20 cells; = the I19-2a tier-assignment.csv exclusive set) run via box-lane --exclusive, the
> rest co-run; a co-run dnf_wall row is superseded and re-run exclusive (harness rule).
> PILOT-62 packer (scripts/census_pilots.py, additive): up to 4 member arms in ONE box-lane acquisition, one pinned
> physical core per arm (BOX_LANE_PHYS element), MKL_NUM_THREADS=1, harness pilot-worker unchanged, 62 cells serial per
> arm, kill 900 s. Each pilot dir has provenance.txt. Every KILL row is re-run ALONE.

The TU-2 cells run on the same terms through `scripts/rstb_cells.py` (one pinned physical core per member arm, up to 4 per
acquisition, kill 900 s, every KILL row re-run alone with `rstb_cells.py alone`). **Every KILL or `dnf_wall` row that
decides a bar is re-run alone** before it is read (the census rule); the columns asserted are counters, statuses and
objectives; wall-clock is never quoted. A deviation observed under a co-run is a candidate until reproduced alone.

| leg | instrument | bar |
|---|---|---|
| C | ctest Release and Debug at the branch head (`legs/ctest.sh`; the P library plus every later test commit) | "100% tests passed" in both; counts reported |
| M | `legs/mutations_rstb.sh` with `legs/mutate_rstb.py`, control first | parity preflight passes (section 4); control clean; every must-kill id killed **by one of its declared killers** (automated, I5); `VERDICT MUTATIONS_AS_DECLARED` |
| PE | the paired perturbation ensembles (section 3a) for every arm over T, CAP, N, P, U | R's ensemble = the census, frozen first; arms read per section 3a |
| N0 | R vs C', **all six x0 members** (`scripts/compare_replay.py`, walk/ssn/ipm) | 0 path movers: byte-exact on every deterministic column but the C' counters, the status/stop_reason/message of a cell whose last event is the relabelled refusal; each relabel listed |
| N1 | F vs C', every member, on the C' cells with `harness_qpsum_zero_motion_exhausted = 0`, `nonfinite_step_rollbacks = 0` and `restoration_entries = 0` (**a proxy**, C565(3); see below) | **0 differences** on every deterministic column but the C562 counters and F3 relabels (`trust_region_floor` -> `subproblem_failure`/`subproblem_iteration_limit`, listed) |
| N2 | F+B1 vs F, every member, on **F+B1's own** cells with `restoration_no_progress_requests = 0` (C577 M1: F runs B1 off, so the counter is identically 0 on F's rows and cannot select) | **0 differences** (no B1 refusal means F's path); the complement is B1's mover list, reported cell by cell |
| N3 | F+B1+B2 vs F+B1, every member, on F+B1's cells with `restoration_factorizations < 20000` | **0 differences** (B2 cannot bind below its floor) |
| N4 | F2+B1+B2 vs F2, every member, on **F2+B1+B2's own** cells with `restoration_no_progress_requests = 0` and `restoration_budget_cuts = 0` (C577 M1) | **0 differences** (no refusal and no cut means F2's path); the complement is the mover list |
| N5 | F2'+B1+B2 vs F2', the same selection on **F2'+B1+B2's own** cells (C577 M1) | **0 differences**; the complement is the mover list |
| N0r | F+B1+B2 on the P binary before the rider (`5375bfbf5`+naming) vs after, member 0 | 0 differences expected; any difference listed |
| T | TRAIN-238 (119 hven-sqp roster cells x {default, sqp-qn}, CAP 300 s), every arm, 6 x0 members | per section 3a |
| CAP | CAP-238-1 and CAP-238-01 (the same 119 problems at the two caps), every arm, 6 x0 members, one freeze with T | per section 3a |
| N | NLP-46 (cutest-nlp-35's 23 TRAIN problems x {default, sqp-qn}), every arm, 3 x0 members, its own freeze | per section 3a |
| P | PILOT-62 (`pilot_cells.tsv`, budget 300 iterations, 900 s kill as safety), every arm, 6 x0 members | read only through `p_cell` (`tu/rstb_read.py pcell --corpus PILOT-62`) |
| U | TU-2 48 (BRITGAS, NET3, SAWPATH, YORKNET x {default, sqp-qn} x A0-A5; `rstb_cells.py list-tu2`; budget 300 iterations, 250000 factorizations), R and every arm, 6 x0 members | read only through `p_cell` (`tu/rstb_read.py pcell --corpus TU2-48 --cells tu2_cells.tsv`) |
| CF | cost of failure (section 3b), every candidate vs R, on T+CAP and on N | per section 3c |
| ST | the one-sided STABLE-SOLVED guard (section 3d), every candidate vs R on T+CAP | per section 3c |
| S | install smoke and export contract | rc 0 |
| CI | `gh workflow run ci.yml --ref rst-b`, `gh run watch <id> --interval 120` | four lanes green; off-reference values recorded as observed, Apple values not observed are UNOBSERVED |

**N1 is a proxy (C565(3)).** The exact neutrality claim is "F's path equals C''s wherever none of F1, F3, the rider or
the entry rule fires". The rows do not record that directly: `harness_qpsum_zero_motion_exhausted` counts only the primary
engine's main-loop QPs (a restoration sub-solve's zero-motion exhaustion, where F1 also acts through `ropts`, is not in it),
and the entry rule has no C' column. The selection therefore excludes every cell with a restoration entry (which removes
the rider's, the entry rule's and the sub-solve F1 sites wholesale) and every cell with a main-loop zero-motion exhaustion
or a rollback. It may exclude cells where F is in fact neutral (conservative), and a difference inside it is a defect to
explain, not a proxy artefact. Cells outside the selection are not claimed neutral by N1.

PILOT-62's declared cap is the 300-iteration budget; the 900 s kill is safety only and a KILL row is re-run alone. The
`tr` ensemble is run for R (the census has it) and for the shipped candidate only; it feeds the stability-guard null (3d)
and the I10 report, never a bar of its own.

## 3a. Paired perturbation ensembles (settler ruling C539; binding)

The perturbation is applied in the HARNESS; the binary under test is untouched.

**HARNESS (pinned).** Rows are produced by hven-bench `ens` at **`27c02687d9389ed43f3dbef27b4cbe389ca2b5d0`** (the census
harness; lanes tree `4319ab15...`, which carries the RST-B option overrides requires-gated, C556). The RST-B readers and the
TU-2 runner are on hven-bench branch **`rstb-harness` at `911d0ebe0eeaf80ed115c983657a82ab6f040b48`**, a descendant of `27c0268` that adds
`tu/rstb_read.py`, `scripts/rstb_cells.py` and `tests/test_rstb_read.py` and touches **no row-producing path**
(`git diff --quiet 27c0268 911d0ebe0eeaf80ed115c983657a82ab6f040b48 -- lanes/ runners/ schema/ tu/ens_members.py` holds, so rows from either read
under `ens.py`'s harness-sha rule). The freeze records both shas and STOPS on mismatch. Rule: `docs/ensemble-rule.md`.
The exception is TU-2: its rows are produced by `scripts/rstb_cells.py` itself (the lane binary and labels are the
harness's; the runner, row format and KILL/alone handling are new). **Smoke (C565(2), 2026-11-25, P lane `afa3a040...`,
member 0):** one ordinary cell (NET3 default, arm F) and one KILL-row cell (BRITGAS default, arm F2'+B1+B2, forced to KILL
with `run --kill-s 10`, then `alone` at 900 s, which replaced it and kept `rows.corun.csv`) each equal a hand-run lane
invocation of the same cell (same label, budgets, pinned core, `MKL_NUM_THREADS=1`) on all 16 row fields (status, outcome,
factorizations, certified, the 12 counters). Every merged label of the 7 P arms over the 48 TU-2 cells (336) is accepted
by the lane's `--dump-options-only`, and the dump shows the RST-B override values applied. Smoke files:
`hven-bench-rstb/.scratch/tu2-smoke/` (not evidence).

**R's ensemble IS the baseline census (C547).** H0 = R. The census binary lacks the C' and C562 counters; comparisons read
them as absent-in-R and use `restoration_entries`, `restoration_iters` and the existing columns for R.

Commands (the store `S` and paths are the frozen file's):

- arms and members: `python3 tu/ens.py arms <arm> --m 5 --kind x0`; each member through `tu/i19_run.py arm <name> --store S
  --binary <lane binary> --binaries gate-RST-B/BINARIES.txt ...` or the census runners `scripts/census_arm.sh`,
  `scripts/census_chain.sh`, `scripts/census_pilots.py run ...` (PILOT-62), `scripts/rstb_cells.py run ...` (TU-2);
- `python3 tu/ens.py freeze --store S --arm H0 --corpora TRAIN-238,CAP-238-1,CAP-238-01 --m 5 --kind x0 --out FREEZE.json`;
- `python3 tu/ens.py freeze --store S --arm H0 --corpora NLP-46 --out FREEZE-NLP46.json` (its own freeze);
- `python3 tu/ens.py read --store S --base H0 --arm <arm> --freeze FREEZE.json --binaries gate-RST-B/BINARIES.txt`;
- `python3 tu/rstb_read.py power|cof|pcell|stability ...` (sections 3b-3d, 9).

**1. Members.** The DECISION ensemble is `x0`: member 0 plus x1..x5 for R and every arm, paired by seed. NLP-46: member 0
plus x1, x2, its own freeze, floors (3 pairs) and votes (3 of 3). PILOT-62 and TU-2: member 0 plus x1..x5, read through
`p_cell` only.

**2. Perturbations.** *x0 kind:* x0_j += 2^-48 c_j max(1, |x0_j|), c_j in [-1, 1) seeded, clipped into the box. *tr kind
(report and null only):* `trust_region_initial` = base +- k ulp(base).

**3. Classes from R's ensemble, frozen before any arm row** (STABLE / COST-UNSTABLE / STATUS-UNSTABLE / NOT-REACHED, the
10% rule). Class counts per corpus and family for R and every arm are reported; a raw class count is **not** a bar (a
truncation that turns a STATUS-UNSTABLE crawl into a consistent early stall reads as an improvement and is not one; I7).

**4. Null.** The 15 pairs of R's members per family and pooled; `floor_f` = the largest absolute value (`ens.py freeze`).

**5. Cost.** As `ens.py read`: `T_j` per seed; family "-" iff `T_j,f < -floor_f` in at least 5 of 6 seeds; pooled "-" the
mirror; one vote per problem name. **A pooled "+" is not required of any candidate.**

**6. Solved, as probabilities.** `p_cell` strict-optimal and optimal+acceptable. A ROBUST LOSS: the base's `p >= 5/6` (3/3 on
NLP-46) and the arm's `p <= 1/2`, i.e. at most 3 of 6, and on NLP-46 0/3 or 1/3 (C577 M5; the reader's `robust_arm_max =
1/2`, `tu/ens.py`). Per family `sum p(arm) >= sum p(base) - 0.5`, read on the T+CAP families and on N (P and U are read
for robust loss through `p_cell`, not for sum p). Robust gains reported symmetrically. "The base" is R unless a bar says
arm F (section 3c, C577 M2).

**7. Tail guard.** On cells R solves in at least 5 of 6 members: the arm's median majors/EV/rejected_steps/
reject_funnel_ceiling must not reach `max(R max + 20, 3 x R max)`.

**8. Same solution.** `|df| <= 1e-6 max(1, |f|)`; otherwise "different solution", in neither net.

0 false infeasibility certificates holds in every member of every arm (V2).

## 3b. Cost of failure (C562 I2)

`tu/rstb_read.py cof --base H0 --arm <Kn>`: per seed j, arm member j against R member j on the cells **unsolved in both**
(ended, neither `optimal` nor `acceptable`; not-reached rows counted apart). The statistic is the **summed factorizations**
the arm spends there against R's, `ratio_j`; beside it the by-name net under the 10% integer rule (one vote per problem
name, corpus copies collapse) and the per-seed geomean. **Null** from R's 15 member pairs on the same cells: the ratio band
`[lo, hi]` with each pair taken in both orders (r and 1/r), and `floor` = the largest |net|.
**POSITIVE** iff `ratio_j < lo` in at least 5 of 6 seeds (3 of 3 on NLP-46) AND the by-name net is >= 0 in as many (a few
large cuts may not hide a by-name loss); **NEGATIVE** iff `ratio_j > hi` in at least 5 of 6 seeds OR net < -floor in as
many; else NEUTRAL. Read on T+CAP (one ensemble) and on N. `infeasible` rows count as unsolved (0.5-1.5% of the summed
factorizations, 11% in member 1 of the census).

**Concentration guard (C577 M3; post-hoc, disclosed).** The summed statistic is concentrated: on the census T+CAP rows
SPANHYD alone carries 22-40% of R's unsolved-in-both factorizations per member, and SPANHYD, LAKES and CRESC50 together
about 55-70%. One name could therefore deliver the whole cut. The reader prints the **per-name contribution table** (each
name's share of the base's summed factorizations on the unsolved-in-both cells, per seed and pooled; published with the
results) and recomputes the reading with the **largest-contributing name removed** (largest = the largest pooled share of
the base's sum), the ratio per seed and the null band both recomputed without it (R-only band without SPANHYD: `[0.8133,
1.2296]`, floor 13; with it `[0.8231, 1.2150]`, floor 12; reader at rstb-harness `911d0eb`, lines `COF-CONTRIB`, `COF-CUT` and
the final `COF arm vs base: VERDICT (full X; without NAME: Y)`). A reading is **POSITIVE** only if it is POSITIVE both with and without that name. POSITIVE with it and not
without reads **"POSITIVE, single name"** and confers no eligibility. NEGATIVE and NEUTRAL are unchanged. This guard
was added after the review had read the R-only concentration (no arm row exists) and is disclosed with the other post-hoc
criteria at the top of this file.

Provisional null on the census T+CAP rows (2026-11-25): band `[0.8231, 1.2150]`, floor 12 names, 33-44 names unsolved in
both per pair. The statistic was chosen after this null was seen and before any arm row exists: the by-name net alone
(floor 12 against a B1 benefit-side set of about 9-12 names) could not be met by any arm, so the claim rests on summed
work, which is where the restoration crawls B1/B2 cut sit; the by-name net guards it. A POSITIVE therefore needs the
candidate to cut at least about 18% of the factorizations spent on failing T+CAP cells in 5 of 6 seeds.

## 3c. The decision rule (pre-registered; C562 B3)

**Part (i)** = the defect fixes: F1 trigger, F3 label, rider, entry rule (arm F, candidate K0). **Part (ii)** = the budgets
B1/B2 and the F2/F2' retry ordering (candidates K1-K3).

**Part (i) bars** (for K0, and also required of every part-(ii) candidate): leg C; leg M; legs N0, N1 and N0r (and for a
part-(ii) candidate its own N2-N5 legs); **no robust loss** (strict and optimal+acceptable) on T+CAP, N, P and U; **V2**
(0 false certificates); **the one-sided STABLE-SOLVED guard PASS** (3d).

**Part (ii) bars**, per candidate Kn (n = 1, 2, 3), in addition to part (i)'s: the sum-p condition per family; the tail
guard; no pooled "-" and no family "-" (T+CAP and N); **cost of failure POSITIVE on T+CAP and not NEGATIVE on N** (3b,
with its concentration guard); the **benefit-side power gate** of section 9 met for B1 (`restoration_entries>=2`). Pooled
"+" is not required.

**Every part-(ii) bar is read twice (C577 M2): against R, and against arm F (K0) as the base.** Part (i)'s effect could
otherwise carry or mask part (ii): K1 could give up solves F gains and still sit within 0.5 of R, and a POSITIVE cost of
failure against R could come entirely from F1/F3. Against F the bars are: no robust loss (base F; strict and
optimal+acceptable, T+CAP, N, P, U), sum p per family at least F's minus 0.5 (T+CAP families and N), and **cost of
failure POSITIVE against F** on T+CAP (not NEGATIVE on N), each with the same definitions as against R
(`tu/rstb_read.py robust --base F --arm <Kn>`, `pcell --base F`, `cof --base F --arm <Kn>`). A candidate is eligible
only if it passes both readings. **K0 vs R cost of failure is reported beside them** (never a bar).

**Result sentence (pre-registered; C577).** For every candidate that passes the robust-loss bars the results state "no
robust loss observed; exposure N names", where N is the harm-side exposure of the candidate's mechanisms read from **arm
F's rows** (B1: names strict-optimal in every F member with `restoration_unproductive_entries >= 2` in some member; the
R-row event of section 9 is a proxy). As section 9 says, harm-side power is far below 20 names for every mechanism:
"no robust loss" over so few exercised names is weak evidence of safety, and the exact legs (N1-N5, whose complements
enumerate every mover cell by cell) carry the safety claim.

**Choice.**
1. A part-(ii) candidate is ELIGIBLE iff it passes both sets of bars.
2. If at least one is eligible: rank the eligible by **robust gains counted by problem name** (strict, union over **T+CAP
   and N only**; P and U run under non-default option variants, so their gains are reported, never ranked). Every
   eligible candidate whose robust-gain count is **at least the best count minus 1** is in the tie (C577 M5; C562's "tie
   margin < 2 names"), and the tie is resolved by the **smallest default change**, in this order: K1 (F+B1+B2), K2
   (F2+B1+B2), K3 (F2'+B1+B2). (The pooled `sum p` key of the earlier draft is deleted: it could never decide, since
   every exact tie is already inside the margin.) Robust gains are expected to be 0-3 names, so in practice the rule picks
   K1 whenever K1 is eligible. The winner ships as declared defaults.
3. If none is eligible: **part (i) ships alone** (K0) iff K0 passes part (i)'s bars. The shipped defaults are then B1 off
   and B2 off: the two options stay as **non-default, argued backstops** (their argument is the per-solve bound, not a
   measured benefit), set off by a one-line defaults commit whose rows must equal arm F's (leg N-ship below).
4. If K0 fails part (i)'s bars too: **nothing ships**; the branch is held.
5. **Losing options are removed before merge**: `qp_retry_regularization_first` unless K2 ships; nothing else is removed
   (B1/B2 stay, on or off by rule 2/3). Each removal or defaults commit is followed by a **neutrality replay (N-ship)**: the
   shipped binary's member-0 rows on T, CAP, N, P and U equal the winning arm's rows on every deterministic column (0
   differences), before merge.

The owner may veto shipping; no reading of the rows may promote a candidate the rule does not.

**B2 vacuity rule (C562 I3).** On the F+B1+B2 rows of every member and corpus, list every cell with
`restoration_budget_cuts >= 1` (`tu/rstb_read.py cuts --arm F+B1+B2`; every listed row reports `restoration_factorizations`
beside `factorizations`, C577; the reader stops if no row carries the column, so a run that never measured cuts cannot
read as zero cuts). If B2 binds on fewer than **3 distinct problem names**, B2 is an **argued backstop**, not a
measured default: it may ship inside K1-K3 (its argument is the bound), but no benefit is credited to it in any report, and
the cost-of-failure claim is attributed to B1 alone. Any cell B2 cuts that F+B1 solves (strict) in the same member is listed.

## 3d. The one-sided STABLE-SOLVED guard (C562 I7)

`tu/rstb_read.py stability --base H0 --arm <Kn>`: the good class is **STABLE-SOLVED** (STABLE and strict-optimal in every
member). Lost = names STABLE-SOLVED under R's x0 ensemble and not under the arm's; gained = the reverse; net = gained - lost
(by problem name). **Name semantics:** a NAME counts as STABLE-SOLVED if ANY one of its cells is (69 of R's 97 names have
all six cells in the class), so losing a name means losing every one of its cells. **Null (C577 M4): R's x0 ensemble split
into halves**, member 0 plus x1..x5 split 3 vs 3 in all 10 distinct ways, each half's STABLE-SOLVED names computed on
its own three members; floor = max over the splits of max(|net|, lost). **Floor = 2 names** on the census T+CAP rows (at
most 1 name lost, largest net 2). It is R-only and an n=3 approximation of the 6-member ensemble, disclosed as such.
**FAIL iff net < -2** (by name). Improvement is a claim, never a bar. **Cell-level net** is reported beside the name
count for the arm and for every split (the splits lose 7-17 and gain 18-27 of about 525 cells). The earlier floor of 8
came from R-x0 vs R-`tr` (97 names, 508 cells, against 105 and 585; x0 -> tr loses 0 names and gains 8; by cell 3 lost,
80 gained). That gap is the strength difference between the two perturbation kinds, not sampling noise, so it is kept
as a report line only. Reported
beside it, unselected: STABLE-SOLVED counts, `sum p(1-p)` over every cell, and on always-solved cells the median and max of
max/min factorizations, for R, R-tr and the arm.

## 3e. Reported endpoints (never bars)

**The 33 confounded cells (C562 I8)** are RST-B's target set, frozen here: NLP-46 BRITGAS, DALLASM and YORKNET x {default,
sqp-qn} (6 cells), and PILOT-62 cells 019-027 (BRITGAS), 028-032 (DALLASM) and 050-062 (YORKNET) (27 cells, 1-based in
`pilot_cells.tsv` order). For each, R and every arm, per member: status, factorizations, and (P arms) the restoration
counters. **Predictions, scored hit or miss** (from the diagnosis; arm named):

| cell | arm | prediction |
|---|---|---|
| PILOT 030 (DALLASM sqp-qn+kTier) | F+B1 | refused by B1 near major 6 (about 1052 factorizations), `stalled` / `restoration_no_progress`, member 0 |
| PILOT 022 (BRITGAS kTier+kAnyAccepted) | F+B1 | refused by B1 near major 18 (about 104k factorizations), member 0 |
| PILOT 025 (BRITGAS kAnyAccepted) and TU-2 A0 BRITGAS default | F+B1 | B1 inert (no B1 refusal; rows equal F's), member 0 |
| TU-2 A0 BRITGAS default and NLP-46 BRITGAS default | K1, K2, K3 (each) | B2 cuts the FIRST restoration entry (`restoration_budget_cuts = 1`, `stalled` / `restoration_work_limit`, `major_iters <= 2`, factorizations about 25k-30k), not solved, in every member (C576). **Seen, not blind, for K3 on TU-2 A0 member 0** (the smoke above); blind for K1 and K2 and for every NLP-46 cell. On NLP-46, R solves this cell in 2 of 3 members (`optimal`, about 50 majors, 2 restoration entries; member 2 `dnf_wall`): a hit is a loss of about 2/3 in `sum p` for the family. It is not a robust loss (R is not 3/3), but it is more than the sum-p tolerance of 0.5 unless gains offset it. The bars decide whether it costs eligibility |
| PILOT 025 | F2 | a long unsolved run (more majors and factorizations than R's, not solved), member 0 |
| NLP-46 BRITGAS default, the members R loses by a restoration crawl | F+B1+B2 | the crawl ends earlier (`stalled`, B1 refusal or B2 cut) at lower factorizations; the solve is not recovered |
| TRAIN MSS1 default | F+B1 | B1 refuses at the third request (entries <= 2 against R's up to 9); still failing; lower factorizations |
| DEMBO7 (TRAIN default, sqp-qn; CAP-238-01 both) and CRESC50 sqp-qn | F+B1+B2 | the members R solves under perturbation stay solved (gains kept; low confidence: B1/B2 can cut a lucky branch) |

Hit = the stated outcome in member 0 (or in the stated members); miss otherwise; scored in the results, never a bar.
**PILOT-62 predictions are scored on status and factorizations only (C565(4)):** PILOT-62 rows carry `status`, `outcome`,
`factorizations` and `certified` and nothing else, so the major index, the refusing mechanism (B1 vs the entry rule) and
the restoration counters are not observable there. For a PILOT cell: "refused ... `stalled`" scores as status `stalled` and
factorizations below R's member-0 value (the stated major and factorization figures are the diagnosis's description, not
part of the score); "B1 inert (rows equal F's)" scores as status and factorizations equal to F's; "a long unsolved run"
scores as not `optimal`/`acceptable` with factorizations above R's. Predictions on NLP-46, TRAIN and TU-2 cells read the
full rows.

**The 26-cell perturbation-divergence endpoint (C562 I10)**, read from the round's own rows (the declared lane binaries;
not the diagnosis's instrumented build or its i19 lane): TRAIN default BT7, CRESC100, DEMBO7, HS101, OPTPRLOC, MISTAKE,
WATER, ACOPP30, HS56, LAKES, MESH, MSS1, NET1; TRAIN sqp-qn ACOPP30, DEMBO7, HS117, CRESC50; CAP-238-01 DEMBO7 (default and
sqp-qn) and CRESC50 sqp-qn; CAP-238-1 HS56 default; NLP-46 ACOPP118, STEENBRD, STEENBRE, BRITGAS (default) and SAWPATH
(sqp-qn). Per cell, for R and every candidate: the pair class of member 0 against each member j (identical / benign / both
fail / status loss / status gain / cost >= 3x / different minimum, the diagnosis's definitions), and per member
`restoration_entries`, `restoration_iters`, `restoration_factorizations`, whether a B1 refusal or a B2 cut fired
(attribution rule, section 1). **Selection caveat:** these cells were chosen because they moved, and R's member 0 is often
the unlucky draw (ACOPP118, BT7, ACOPP30, HS56); any arm will tend to look better on them by regression to the mean. That
is why this is a report beside the unselected measures of 3d and can never gate. The diagnosis's row-level first-divergence
count needs its logging build and is registered for the walk round, not built here.

**Informational owner replay (C562 I8).** R and each candidate on the issue-#19 variants, in the form of
`../2026-11-m8-i19-2a/gate-I19-2a/OWNER-REPLAY-REQUEST.md`; informational, cannot ship or block anything. Making R4
reachable exposes the r -> 2r -> r radius cycle (diagnosis probe, majors 111-135); the replay is where the owner sees it.

**h at every B1 refusal (C562 I9).** `restoration_refusal_feasibility` per refused row; every refusal with a value above
`feasibility_tol` is listed separately (a B1 refusal at an infeasible point: failure steps accepted through the violation
arm reduce h but count as unproductive).

## 4. Mutations and must-kills

All must-kills (control first). A mutation passes only if the run fails AND at least one of ITS declared killers is among
the failed tests (`mutations_rstb.sh`, the declared-killer check; an empty killers file is a failure). Other killers are
allowed as well.

| id | mutation | declared killer(s) |
|---|---|---|
| B1a | a failure step counted as progress | `RstbEntrySeam.FailureStepOnlyCyclesAreRefusedAtTheSecondUnproductiveReEntry` |
| B1b | a productive interval does not clear the run | `RstbEntryLedger.ASolvedStepMakesTheIntervalProductiveAndClearsTheRun`, `RstbEntrySeam.AProductiveIntervalClearsTheRun` |
| B1c | `<=` read as `<` | `RstbEntryLedger.FailureStepsAloneAreUnproductiveAndTheSecondRunIsRefusedAtOne`, `RstbEntrySeam.FailureStepOnlyCyclesAreRefusedAtTheSecondUnproductiveReEntry` |
| B1d | the first entry tested | `RstbEntryLedger.TheFirstRequestIsNeverTested`, `RstbEntrySeam.ToleranceZeroRefusesTheFirstReEntryAndALargeOneNone` |
| B1e | refusal spelled numerical_error / restoration_refused | `RstbEntrySeam.FailureStepOnlyCyclesAreRefusedAtTheSecondUnproductiveReEntry`, `SqpLadderSeam.L1ARefusedRequestAtAFeasiblePointIsNeverInfeasible` |
| B1f | refusal spelled kInfeasible | `RstbEntrySeam.FailureStepOnlyCyclesAreRefusedAtTheSecondUnproductiveReEntry`, `SqpLadderSeam.L1ARefusedRequestAtAFeasiblePointIsNeverInfeasible` |
| B1g | refusal sets the certificate | `RstbEntrySeam.FailureStepOnlyCyclesAreRefusedAtTheSecondUnproductiveReEntry` |
| B1h | an optimal-elastic-rung accept not counted | `RstbEntrySeam.AnElasticRungAcceptIsProgress` |
| RB1 | a rolled-back accept keeps its progress credit (the retraction dropped) | `RstbEntrySeam.ARolledBackAcceptIsNoRestorationProgress`, `RstbEntrySeam.ARolledBackAcceptFeedsTheUnproductiveCountAtTheDefaultTolerance` |
| RB2 | the entry rule still counts a rolled-back accept | `RstbEntrySeam.ARolledBackAcceptIsNotAnAcceptedStepForTheEntryRule`, `RstbEntryLedger.KeptAcceptsCountOnlyAcceptsThatWereNotRolledBack` |
| F1 | R4 trigger ignores zero_motion_exhausted | `Rf1RetryLadderSeam.AZeroMotionExhaustionReachesTheRegularizationRung` |
| F2 | regularization-first ignored | `Rf1RetryLadderSeam.AZeroMotionExhaustionReachesTheRegularizationRung`, `RstbRetryLadder.RegularizationFirstTakesR4AheadOfTheColdAndAlgebraRungs` |
| F3 | floor label without the floor | `Rf1RetryLadderSeam.ANoMotionFailureAboveTheFloorIsTheSubproblemFailure` |
| B2a | B2 cut spelled kWorkLimit | `RstbBudgetSeam.ACumulativeBudgetCutIsTheRestorationWorkLimit` |
| B2b | caller's ceiling spelled kRestorationWorkLimit | `RstbBudgetSeam.TheCallersSmallerCeilingStaysTheWorkLimit`, `Rf1RestorationSeam.TheCallersCeilingInsideThePhaseStaysTheWorkLimit` |
| B2c | the cumulative subtraction across entries dropped | `RstbBudget.TheSecondEntryGetsOnlyWhatTheFirstLeft`, `RstbBudget.ALaterEntryWithNoBudgetLeftIsCutBeforeItFactorizes`, `RstbBudget.OffTheWindowEdgeOnlyTheCumulativeReadingCutsTheSecondEntry` |
| B2d | the derivation base read as the solve's total, not the main loop's | `RstbBudget.TheDerivedBudgetReadsTheMainLoopsFactorizationsOnly` |
| B2r | the reduction resume no longer reads a budget cut | `RstbBudgetSeam.ABudgetCutTakesTheReductionResume` |
| B2z | a later entry with no budget left is handed no ceiling | `RstbBudget.ALaterEntryWithNoBudgetLeftIsCutBeforeItFactorizes` |
| B2s | a stall-window stop counted as a B2 cut (`!phase_stalled` dropped; no label moves, only `restoration_budget_cuts`; C577 M6) | `Rf1RestorationSeam.AStalledPhaseIsTheRestorationWorkLimit`, `Rf1RestorationSeam.TheStallExitTakesTheReductionResume` |
| B2x | the call site passes 0 for the restoration share (the derived base becomes the solve's total) | ARGUED: the share changes the derived budget only when 4 x the main loop's factorizations exceeds the 20,000 floor (more than 5,000 main-loop factorizations) after restoration has already spent; no test-scale fixture reaches that, and pinning it would need a seam inside the P library, which would move the measured binary. The helper's arithmetic is pinned by B2d (`RstbBudget.TheDerivedBudgetReadsTheMainLoopsFactorizationsOnly`); the call site is one reviewed line. Observed surviving the targeted suites (2026-11-25). |
| FR1 | a spent budget forbids the feasible resume | `RstbBudgetSeam.AZeroBudgetDoesNotRefuseAFeasibleEntryAndTheCapAndProgressTestBoundIt` |
| CAP1 | the entry cap no longer outranks the no-progress label | `RstbEntrySeam.TheEntryCapOutranksTheNoProgressLabel`, `RstbEntrySeam.FailureStepOnlyCyclesAreCountedAsUnproductive` |
| RR1 | record_resume keeps the previous interval's solved flag | `RstbEntryLedger.ASolvedStepMakesTheIntervalProductiveAndClearsTheRun` |
| LAB1 | kRestorationNoProgress dropped from the labelable exits | `Rf1AcceptableLabel.OnlyTheFailureExitsAreLabelable`, `RstbEntrySeam.TheRefusalAtAnAcceptablePointIsLabelled` |
| CN1 | `restoration_unproductive_entries` not incremented | `RstbEntrySeam.FailureStepOnlyCyclesAreCountedAsUnproductive` |
| CN2 | `restoration_no_progress_requests` not incremented | `RstbEntrySeam.FailureStepOnlyCyclesAreRefusedAtTheSecondUnproductiveReEntry` |
| CN3 | `restoration_factorizations` not accumulated | `Rf1RestorationSeam.TheCallersCeilingInsideThePhaseStaysTheWorkLimit` |
| CN4 | `restoration_budget_cuts` not incremented | `RstbBudgetSeam.ACumulativeBudgetCutIsTheRestorationWorkLimit` |
| CN5 | `restoration_no_accept_refusals` not incremented | `SqpLadderSeam.ARequestWithNoAcceptedStepSinceTheResumeIsRefusedAtAnyCap`, `RstbEntrySeam.ARolledBackAcceptIsNotAnAcceptedStepForTheEntryRule` |
| CN5b | the entry rule's counter also counts the cap's refusal | `RstbEntrySeam.TheEntryCapOutranksTheNoProgressLabel`, `SqpLadderSeam.ARequestWithNoAcceptedStepSinceTheResumeIsRefusedAtAnyCap` |
| CN6 | `restoration_refusal_feasibility` not recorded | `SqpLadderSeam.ARequestWithNoAcceptedStepSinceTheResumeIsRefusedAtAnyCap`, `RstbEntrySeam.FailureStepOnlyCyclesAreRefusedAtTheSecondUnproductiveReEntry` |

**Argued survivors: B2x only** (C577 should; reason in its row). The runner's `ARGUED` list must equal the table's argued set; a PARITY PREFLIGHT,
before any build, compares this table's ids with `mutate_rstb.py list`, requires a declared killer on every must-kill row,
and writes the declared killers the per-mutation check reads; any difference aborts the leg (`PARITY_FAIL`).

## 5. Declared breaks

- RF-1's `Rf1RetryLadderSeam.ANoMotionFailureAtAFeasiblePointEndsAtTheFloor` re-pinned with `trust_region_min = 0.75`;
  the above-floor case is the new `ANoMotionFailureAboveTheFloorIsTheSubproblemFailure` (F3).
- `SqpLadderSeam.L1A...` and `ARequestWithNoAcceptedStepSinceTheResumeIsRefusedAtAnyCap` read `numerical_error` for the
  zero-accept refusal: now `stalled` / `restoration_no_progress` (C').
- Trace golden `sqp_complete.jsonl` and the counters unit lines: three trailing keys (C'), three more (C562), the last
  absent (`null`) by default; `SqpCounters` absent-capable fields three -> four.
- `HsSweepCorpus.Corpus` and `Hs15WarmKdOffWithTheFailedSubproblemStep` (F1: HS15's warm failing chain reaches R4):
  reference Release 22/40 -> 23/30 full-step/majors, held-off arm 40 -> 30 majors (stop f 1.0038 -> 1.0652, the near-origin
  tolerance widened to 0.1: a worse stop objective on the held-off arm, a behaviour change), step arm 43/149/64/6 ->
  44/153/65/7, corpus minors 3406 -> 3359; Debug 21/39 -> 21/40, 39 -> 40, 3392 -> 3396. Off-reference columns are
  UNOBSERVED under RST-B (CI decides).
- F3 moves a subproblem-failure trace line (the rung-exhausted exit above the floor is the ending warning).

## 5a. BINARIES

One install per binary, built on a lane, `HVEN_FP_MODE=SAFER_FAST`, Release, no LTO, clang; lanes built at the harness
`27c0268` with `scripts/census_build.sh`'s recipe. `gate-RST-B/BINARIES.txt` (format: `docs/ensemble-rule.md`; only its
`declare` lines are read) is filled from the builds before any arm row and re-checked at the end. `lane_src=` is the
`lanes/` tree `4319ab1512e2de6dbc385eb964be182f0af4eb70` for every arm (R's rows and every arm's share one tree). The P arms
share one build, install and lane binary. The C' commit is unchanged by the C562 work, so the C' install and lane stand.

| name | commit | used by |
|---|---|---|
| H0 (= R) | `99712ec32` (lane `7d2f9f37...`, the census) | R rows |
| C' | `46761076fbbdbc616a015ff4570b082443a454ab` | N0, C' rows |
| P | the code head of `rst-b` (`34a3348dfe908185cc68762be0a9a9398a179486`; later commits touch docs and tests only, `git diff --quiet 34a3348df HEAD -- src include` holding, so the installed library and lane are unchanged; re-read at the freeze) | F, F2, F2', F+B1, F+B1+B2, F2+B1+B2, F2'+B1+B2 |

The `declare` lines are in `gate-RST-B/BINARIES.txt`; the reader checks `HVEN_PIN` equality for both prefixes, that
`99712ec32` is an ancestor of each declared pin, and prints the `src/include` diff-stat. A new override key is checked with
the lane's `--dump-options-only` before the freeze (the four RST-B keys are lane clauses at `27c0268`).

## 6. Box time

Builds (P install and lane): about 1 h. C (Release + Debug ctest): about 1.5 h. M: 32 mutation cycles (31 must-kills and the argued B2x), about 4.5 h. The
ensemble legs: 8 arms (C' and seven P arms) x 6 x0 members over T+CAP (714 cells per member), N (46 x 3), P (62) and U (48),
plus R's U members and the shipped candidate's `tr` members. The per-member box time is taken from the census logs
(`census-m8/logs/`) at the freeze and the frozen file states the total and the census figures it came from; it is not
estimated here. Solo re-runs of KILL/`dnf_wall` rows add to it.

## 7. Not run

HOLDOUT (never). Wall-clock. The QP walk census. hven-ipm rows (no code path). B1b (envelope filter on entry points) and
the iteration-share B2 variant (registered). D4 (kTier elastic rung exhausts at rung 0; infeasible verdict at h <=
feas_tol routed to elastic) is registered, not designed here. The E2-route F1 extension is registered. The diagnosis's
row-level first-divergence count (registered for the walk round). Corpus enrichment (the diagnosis's 2-point restoration
probes, CRESC/BRITGAS/DALLASM shapes at other sizes, ready-made AMPL-NLP problems outside HOLDOUT, each a new problem name)
is registered with QPD's STAB-E (C562).

## 8. Instability is not addressed here

See the opening paragraph. The decision rule reads robust losses, the one-sided STABLE-SOLVED guard and cost on failing
cells; no bar credits a candidate for a class-count improvement.

## 9. Power, per mechanism, by problem name (C562 B1) -- counts on the complete census (C576)

One power event per mechanism, counted by distinct problem NAME ((set, problem): corpus copies and bases collapse), with
`tu/rstb_read.py power`:

- **harm-side power** = names baseline-STABLE and strict-optimal in every R member whose rows show the event (member 0 /
  any member / all members): the cells where the mechanism could cost a solve;
- **benefit-side gate** = names R fails (not strict-optimal) in at least 5 of 6 members (3 of 3 on NLP-46) whose rows show
  the event (member 0 / any member): the cells where the mechanism could save cost or a solve.

| mechanism | event (R rows) |
|---|---|
| B1 | `restoration_entries>=2` |
| F1 (and F2/F2') | `harness_qpsum_zero_motion_exhausted>0` (the primary engine's main-loop QPs; a lower bound) |
| B2 | `restoration_factorizations>=20000` (a C' counter: read on C' members, not on R) |
| (the old gate) | `restoration_entries>=1` |

**Use.** Part (i) does not claim benefit: its ensemble bars guard against harm, and the harm-side power is stated beside
them (a "no robust loss" over few exercised names is weak evidence of safety and is reported as such). **Part (ii)'s
benefit-side gate: B1's benefit-side count (any member) over T+CAP plus N must be at least 10 names**, else the cost of
failure claim is exploratory and no part-(ii) candidate is eligible (3c). This threshold was set after the provisional
counts (12 names) were seen; they are baseline-only (no arm row exists), so no arm outcome was known when it was set.

**Counts on the complete census** (C576, re-derived 2026-11-25 with `tu/rstb_read.py power` at rstb-harness `faad9e9`
over store `/home/ghecht/Projects/hven-bench-ens/.scratch/census-m8/store`, arm H0). T+CAP: 6 x0 members, 714 cells,
classes STABLE 535 / COST-UNSTABLE 104 / STATUS-UNSTABLE 75 / NOT-REACHED 0. NLP-46: 3 x0 members, 46 cells (the 10 solo
problems run exclusive), classes STABLE 9 / COST-UNSTABLE 16 / STATUS-UNSTABLE 11 / NOT-REACHED 10. The 10 NOT-REACHED
cells (BIGBANK sqp-qn, MSS2 default, MSS3, READING8, TAX213322, TAX53322 both bases) are exclusive-leg `dnf_wall` rows at
300 s in every member. They count as failing but carry no counters, so no event can be read on them: an undercount, never
an inflation. The provisional figures from partial NLP-46, kept for the record, were B1 3/3, F1 not read, `>=1` 6/6.

| event | corpus | harm-side (any member) | benefit-side (member 0 / any) |
|---|---|---|---|
| `restoration_entries>=2` (B1) | T+CAP | 0 names | 6 / **9 names**: ALLINITC BT12 BT8 CRESC100 CRESC50 HS99EXP MESH MSS1 SPANHYD |
| `restoration_entries>=2` (B1) | NLP-46 | 0 names | 4 / **5 names**: MSS2 NET3 STEENBRC STEENBRG YORKNET |
| `harness_qpsum_zero_motion_exhausted>0` (F1) | T+CAP | 3 names: HS101 HS103 HS108 | 8 / 9: CRESC100 CRESC50 HS61 HS99EXP LAKES MESH MSS1 NET1 SPANHYD |
| `harness_qpsum_zero_motion_exhausted>0` (F1) | NLP-46 | 0 names | 6 / 7: MSS2 NET3 STEENBRC STEENBRD STEENBRE STEENBRG YORKNET |
| `restoration_entries>=1` | T+CAP | 8 names: CRESC4 DNIEPER HS101 HS74 HS75 HS89 HS91 WATER | 16 / 22 |
| `restoration_entries>=1` | NLP-46 | 0 names | 8 / 8 |
| `restoration_factorizations>=20000` (B2) | -- | not computable from R rows | read on C' members |

For comparison, the pre-freeze review's counts on the I19-2a store (H0 and P rows, solved, no stability filter): B1 3 names
on TRAIN/CAP (CRESC100, CRESC4, HS101) and 2 on NLP-46 (BRITGAS, NET3); F1 8 and 7. **Final B1 benefit-side total:
9 + 5 = 14 names (any member; member 0: 6 + 4 = 10). The gate of 10 is MET.** (Provisional figure: 12.) Harm-side power is far below 20 names for every mechanism: the round is
not powered to show safety on exercised cells beyond the robust-loss guard, which is why part (ii) needs a positive
cost-of-failure claim and part (i) ships on tests and exact legs.
