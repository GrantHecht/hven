# CP-1: the accuracy-matching rule

Date: 2026-11-24. Status: DRAFT for owner review (not yet a declaration; nothing here has been
measured). Rule id: `cp1-v1`. Branch `cp1`, from `origin/m8`.

Authority and inputs of record:

- M8 brief, `docs/notes/2026-11-m8-brief.md`: §0 `:17-20` (the rule is defined once in M8 and M8's
  comparison set is chosen so that M9b's like-for-like set is a subset), §2 CP-1 `:430-434`,
  CP-3 `:440-451`, §3 exit criterion 11 `:485`, HB-1 `:328-331`, HB-4 `:338-341`, Q2 `:502-512`.
- Roadmap, `docs/notes/2026-08-26-roadmap-m6-m8-draft.md`: the published-comparison bullet
  `:259-276` (IPOPT anchors the cross-machine ratio, claims against Knitro are relative to it) and
  the 2026-08-31 amendment, `:363-370` ("natural tolerance is the bar, matched accuracy is
  context"; the calibration grid's absent cell is filled in M8; M9b's set is a subset of M8's),
  with M9b at `:375`.
- Harness referee, hven-bench `docs/lanes.md` ("the referee: what `kkt_*` means"): the four
  `kkt_*` columns are one quantity, computed by hven's own `compute_declared_diagnostics_from_grad_lag`
  at each lane's returned point. That is what makes a per-cell attained-residual comparable across
  the four lanes at all.

## 1. What the rule is, in one paragraph

Every solver runs at its own natural tolerance, and that tolerance is the bar: a cell is solved
if the lane's own status says so, nothing re-grades it. The comparison then reports, beside every
cell, how accurate the answer actually was: the attained KKT residual (the referee's, the same
quantity for every lane) and the objective gap against a reference objective defined per cell
below. These are context columns. They are never used to move a cell between solved and unsolved,
never enter a shifted geometric mean, a profile or a success rate, and never select which cells
count.

## 2. Definitions

### 2.1 The bar

- **Success** is the harness's `outcome == optimal` (hven-bench `schema/columns.py`
  `SUCCESS_OUTCOMES`), which buckets each lane's own success statuses (`kOptimal`/`kAcceptable`
  for hven, `Solve_Succeeded`/`Solved_To_Acceptable_Level` for IPOPT, `Optimal` for SNOPT).
  `infeasible`, `dnf_budget`, `dnf_wall` and `error` are failures.
- **No tolerance re-grade.** The summaries run with `tol = None` (HB-1's optional tolerance
  gate is NOT used for the headline numbers). A second, labelled reading with a stated `tol` may
  be shown as a sensitivity row, never in place of the headline.
- **Natural tolerance is read, not typed.** Each lane's tolerance options (hven's `tol` family,
  IPOPT `tol`/`acceptable_*`, SNOPT's optimality and feasibility tolerances) are tabulated in the
  artifact from the committed options dumps (`sets/options-dumps/`, whose sha256 is the row's
  `options_digest`). The artifact states, per lane, what "converged" means to that solver; the
  reader sees that they are different quantities (SNOPT's are relative, hven's and the referee's
  absolute; hven-bench `docs/lanes.md`, "what this means for a reader").
- **Acceptable-level statuses are disclosed, not split off.** The headline success count follows
  the bucketing above; the artifact adds, per lane, the count of successes whose verbatim
  `status` is an acceptable-level status, so a reader can see how much of a success rate rests
  on the looser bar.

### 2.2 Context column 1: attained KKT residual

Per (cell): `kkt_max = max(kkt_stationarity, kkt_primal_eq, kkt_primal_ineq, kkt_complementarity)`
as the referee reports it, plus `kkt_scaled = kkt_max / max(1, |objective|)`. Both are shown.
A null residual stays null and is printed as `UNOBSERVED-NULL` (absent evidence is not zero,
CLAUDE.md §6). A row that did not return a point (no residuals) has no context columns and says so.

### 2.3 Context column 2: objective gap against the reference

`gap = (f - f_ref) / max(1, |f_ref|)`, signed. Positive means worse than the reference (minimization).
Printed for every cell that has an objective and a reference; never printed as zero when either
is absent.

### 2.4 The reference objective, per cell

The reference is defined per problem (not per lane), from evidence in the frozen store only, by the
first tier that applies. Every cell prints which tier supplied its reference (`ref_tier`).

- **R1: the pinned published optimum.** If the set pin carries a numeric `f_star` for the problem
  (hs-27 does, from Hock-Schittkowski/Schittkowski), that value.
- **R2: the certified-pool minimum.** Otherwise, the minimum objective over the problem's rows,
  across all four lanes in the store, that are (a) `outcome == optimal` and (b) certified: all
  four referee residuals `<= eps_ref * max(1, |f|)` with `eps_ref = 1e-6`.
- **R3: the feasible-pool minimum.** Otherwise, the minimum objective over `outcome == optimal`
  rows with both primal residuals `<= eps_ref * max(1, |f|)`. Flagged weak: stationarity was not
  certified.
- **R4: none.** Gap is `UNOBSERVED`; the cell still prints its residual context.

Notes that are part of the definition:

- The reference may be supplied by the very lane it grades. That is intended: the minimum over a
  certified pool is ">= 0 gap for the pool's own best row, by construction", and the gap column is
  then read as "how far from the best answer anyone at this pin certified". It is not a claim that
  the reference is the global optimum.
- **Multi-basin flag.** If the certified pool's relative spread (max - min over `max(1, |min|)`) exceeds
  `1e-5`, the cell is printed `multi_basin = 1`: lanes certified different stationary points, so a
  positive gap may be a different local solution, not inaccuracy. Counted, never re-graded.
- **Beaten-reference flag.** If a certified row beats an R1 `f_star` by more than `eps_ref`
  relative, `ref_beaten = 1` (the pin's `f_star` is then suspect, or the row is at a different
  point); the reference stays R1 and the case is listed for the owner.
- The reference is frozen with the store: it is a function of the store's bytes and nothing else, so
  re-deriving the artifact re-derives it exactly. A later pin's rows never silently change an earlier
  reference (M9b reuses the frozen one, section 4).
- The tier counts are an artifact output. Illustration, from READING COMMITTED TRAIN ROWS ONLY (the
  M7 H-REPIN hven rows with the H4 IPOPT/SNOPT rows; no run was made for this note): with the
  absolute-residual form of eps_ref, 15 of 45 TRAIN problems had no certified row (the residuals
  are absolute, not scaled); with the `max(1, |f|)`-scaled form above, 5 of 45 remain. That is why
  the gate is scaled. The five fall to R3 or R4 and the artifact says so.

### 2.5 The calibration grid's absent cell

The roadmap amendment (`:363-370`) and brief CP-1 (`:433-434`) ask CP-1 to fill "the calibration
grid's absent cell (an external solver on a non-F7 problem)". The grid is read here as
(lane x problem family), the cells where a cross-solver comparison is calibrated on a common
referee; the cell with no entry is an external lane (IPOPT, SNOPT) on a problem outside the F7
family. (The repository text available to this draft names the cell but does not define the grid;
see open question 1.) CP-1 fills it as follows.

- A cell is **filled** iff, for that (external lane, non-F7 problem): the store contains a row from
  a serial run at the pinned harness and options (`run.mode: serial`, `mkl.num_threads: 1`) with the
  referee residuals non-null, so that section 2.2 and 2.3 can be printed. The pinned HS-27 and
  CUTEst-NLP-35 TRAIN/HOLDOUT cells are exactly those non-F7 problems, and CP-2/CP-3 run both
  external lanes on all of them; the filled grid is therefore the artifact's context table itself.
- A cell that cannot be filled (lane unavailable, no returned point, residuals null) is recorded
  `UNOBSERVED` with the reason. It is never zero-filled, interpolated, or borrowed from F7.
- The calibration claim made from the grid is bounded: it licenses reading the context columns of
  external lanes beside hven's, on the referee's definition of residual. It does not turn an
  external lane's own tolerance into hven's. The rule is "report, do not convert".
- The IPOPT anchor for published Knitro numbers (roadmap `:264-270`; brief CP-3 `:443-445`) is separate from
  this grid. At the HB-4 retrieval (2026-11-13, Mittelmann AMPL-NLP) the overlap with the pinned
  sets is 0 problems, below the brief's Q2 floor of 10, so no Knitro claim is made.

## 3. What the rule forbids

- Re-grading any cell on the context columns; excluding a cell because its residual is loose; using
  the gap to rank.
- Shifting a lane's options to "match accuracy" (tightening IPOPT's `tol` toward hven's, or the
  reverse). Natural tolerance means defaults, or the declared label for the lane, as pinned.
- Mixing the referee's residual with a lane's self-reported one in one column.
- Silently dropping a context value that is absent. Absent is printed.

## 4. Inheritance by M9b

M9b's like-for-like first-derivative comparison (roadmap `:375`; brief CP-3 scope `:449-450`) inherits
the rule by reference, as `cp1-v1`:

1. Same bar: each lane at its own natural tolerance, `tol = None` headline.
2. Same context columns (2.2, 2.3) with the same definitions, printed beside the first-order lanes.
3. **The reference is the M8 frozen reference.** M8's set is chosen so that M9b's set is a subset,
   so every M9b cell has an M8 `f_ref` and `ref_tier` already. M9b does not recompute references
   from its own rows; a first-order or quasi-Newton lane that reaches a lower objective than `f_ref`
   shows a negative gap, and that is the evidence, not a reason to move the reference. (Objective
   at a KKT point does not depend on whether the Hessian was exact, so exact-Hessian rows are valid
   reference sources for a first-derivative comparison.)
4. M9b adds columns, never redefines one. Changing a definition above, or `eps_ref`, or the
   multi-basin threshold, is a declared event with a new rule id (`cp1-v2`), and M8's artifact stays
   readable under `cp1-v1`.
5. M9b needs its own fresh holdout (brief Q3); the reference rule is independent of that.

## 5. What the artifact generator does with it

hven-bench's CP-3 generator (branch `cp3`) takes the four-lane store and prints, per cell and per
lane, `outcome`, `kkt_max`, `kkt_scaled`, `ref_tier`, `f_ref`, `gap`, `multi_basin`, `ref_beaten`,
and a header block carrying `cp1-v1`, `eps_ref`, the multi-basin threshold and the tier counts.
Its self-tests pin sections 2.2-2.4 with hand-computed fixtures.

## 6. Open questions for the owner

1. **Define the calibration grid.** Section 2.5 reads it as (lane x problem family) with F7 the only
   family filled so far. If the grid is something else (for example cross-hardware ratios), the fill
   rule needs rewriting.
2. **eps_ref = 1e-6, scaled by `max(1, |f|)`.** Right strictness for the reference pool, given that
   SNOPT's own optimality is relative and its referee-form residuals are large on several TRAIN rows?
3. **R2 takes the pool minimum.** Accept that a lane which is the best certified row has gap 0 by
   construction, or prefer a median/consensus reference (more robust to one lane landing in a lower
   basin, but less interpretable)?
4. **Acceptable-level statuses.** Keep them inside the headline success count with disclosure
   (as drafted), or split them out?
5. **R3 (feasible but uncertified) as a weak reference.** Keep, or go straight to `UNOBSERVED`?
