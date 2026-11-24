# CP-1: the accuracy-matching and comparison rule

Date: 2026-11-24. Status: DRAFT for owner review (a rule, not yet a declaration; nothing here has
been measured). Rule id: `cp1-v2`. Branch `cp1`, from `origin/m8`.

`cp1-v2` replaces `cp1-v1` (commit `ace6fb0e8`, readable in git history; the dry-run artifact of
hven-bench branch `cp3` was produced under it and says so). The changes are the owner rulings of
2026-11-24 (settler ledger C528) applied to the decision memo of the same date. Every change is
listed in section 8 so that no definition moves silently.

Authority and inputs of record:

- M8 brief, `docs/notes/2026-11-m8-brief.md`: §0 `:17-20` (the rule is defined once in M8 and M8's
  comparison set is chosen so that M9b's like-for-like set is a subset), §2 CP-1 `:430-434`,
  CP-3 `:440-451`, §3 exit criterion 11 `:485`, HB-1 `:328-331`, HB-4 `:338-341`, Q2 `:502-512`.
- Roadmap, `docs/notes/2026-08-26-roadmap-m6-m8-draft.md`: the published-comparison bullet
  `:259-276` (IPOPT anchors the cross-machine ratio) and the 2026-08-31 amendment, `:363-370`
  ("natural tolerance is the bar, matched accuracy is context"; the calibration grid's absent cell
  is filled in M8; M9b's set is a subset of M8's), with M9b at `:375`.
- Harness referee, hven-bench `docs/lanes.md` ("the referee: what `kkt_*` means"): the four `kkt_*`
  columns are one quantity, computed by hven's own `compute_declared_diagnostics_from_grad_lag` at
  each lane's returned point, so a per-cell attained residual is comparable across the four lanes.
- TU-1 declaration, hven `origin/m8`, `docs/notes/data/2026-11-m8-tu/DECLARATION-TU-1.md`: the
  family table of section 6 (classes, section 5 below).

## 1. What the rule is, in one paragraph

Every solver runs at its own natural tolerance. A cell is **solved** under two readings, always
printed together: the lane's own status (the headline) and a referee-checked reading that also
requires the returned point to pass a declared residual gate. A standing is MET only if it holds
under both readings. Cross-solver speed is serial wall time around the solve call, identical in every
lane, with harness-counted evaluations beside it; native iteration counts are never ranked across
solvers. The reference objective for the objective-gap context is the pool minimum over rows that
pass the primal gate. Nothing is re-tuned, re-graded or re-defined after a HOLDOUT row is read.

## 2. Definitions

### 2.1 Success, two readings

- **Reading A, own status (the headline).** `outcome == optimal` (hven-bench `schema/columns.py`
  `SUCCESS_OUTCOMES`), which buckets each lane's own success statuses (`kOptimal`/`kAcceptable` for
  hven, `Solve_Succeeded`/`Solved_To_Acceptable_Level` for IPOPT, `Optimal` for SNOPT). `infeasible`,
  `dnf_budget`, `dnf_wall` and `error` are failures. No tolerance re-grade; the summaries run with
  `tol = None`.
- **Reading B, referee (the mandatory row).** A row is solved under B iff it is solved under A and
  the referee gate of section 2.2 passes at its returned point (primal gate and the stationarity gate).
  The objective is not part of B: a different local solution is not a failure (the multi-basin flag
  of section 2.4 counts those).
- **A standing is MET only under both readings.** The artifact prints each standing once per
  reading and the conjunction. A reading that cannot be evaluated (a solved row whose referee scales
  are absent, section 2.2) is `UNOBSERVED`, the standing is then a NAMED GAP, never MET.
- **Acceptable-level statuses.** The artifact additionally prints, per lane, the count of A-successes
  whose verbatim `status` is an acceptable-level status.
- **Natural tolerance is read, not typed.** Each lane's tolerance options are tabulated from the
  committed options dumps (`sets/options-dumps/`, whose sha256 is the row's `options_digest`); the
  reader sees they are different quantities (SNOPT's are relative, hven's and the referee's absolute;
  hven-bench `docs/lanes.md`).

### 2.2 The referee gate (replaces the `max(1, |f|)` scaling of `cp1-v1`)

Dividing a KKT residual by the objective value is not a convention (the objective carries an
arbitrary constant), and `cp1-v1` adopted it after seeing TRAIN rows fail without it. `cp1-v2`
drops it. With `eps_ref = 1e-6`, per row:

- **x_scale** `= max(1, ||x||_inf)` at the returned point.
- **mult_scale** `= max(100, (||lambda||_1 + ||z||_1) / (m + n)) / 100` over the row multipliers
  and bound multipliers at the returned point: the multiplier-based scaling of the IPOPT paper's
  stationarity test (`s_d`, with `s_max = 100`).
- **Primal gate:** `max(kkt_primal_eq, kkt_primal_ineq) <= eps_ref * x_scale`.
- **Stationarity gate:** `kkt_stationarity <= eps_ref * mult_scale` and
  `kkt_complementarity <= eps_ref * mult_scale`.
- **Referee gate** = primal gate and stationarity gate. A row is **stationarity-certified** iff it
  passes the stationarity gate.

`x_scale` and `mult_scale` are measured by the harness from the lane's own returned point (columns
`ref_x_norm_inf` and `ref_mult_scale`, hven-bench schema event of 2026-11-24). A row without them
(a row taken before the event, a row that returned no point) has no referee reading: `UNOBSERVED`,
never zero-filled (CLAUDE.md §6).

### 2.3 Context columns

- `kkt_max = max` of the four referee residuals, `UNOBSERVED-NULL` where null.
- `feas_rel = max(kkt_primal_eq, kkt_primal_ineq) / x_scale`, `stat_rel = max(kkt_stationarity,
  kkt_complementarity) / mult_scale`.
- `gap = (f - f_ref) / max(1, |f_ref|)`, signed. Positive is worse than the reference. The objective
  scale `max(1, |f_ref|)` is only a presentation scale for the gap; it gates nothing.
- Context columns never move a cell between solved and unsolved and never enter a mean, profile or
  rate.

### 2.4 The reference objective: pool minimum

Per problem, from the frozen store only. Every cell prints which tier supplied its reference.

- **R1: the pinned published optimum.** If the set pin carries a numeric `f_star` for the problem
  (hs-27 does), that value.
- **R2: the pool minimum.** Otherwise the minimum objective over the problem's rows, across all
  lanes in the store, that are (a) solved under reading A and (b) pass the primal gate of section
  2.2. A flag `ref_stationarity_certified` says whether the supplying row also passes the stationarity
  gate (ties on the minimum are broken in favour of a certified row).
- **R3 is folded into R2** (owner ruling, memo Q3: R3 was used by 0 of 45 TRAIN problems and a minimum
  needs feasibility, not stationarity). There is one tier.
- **R4: none.** No row passes: the gap is `UNOBSERVED`; the cell still prints its residual context.

Notes that are part of the definition:

- The reference may be supplied by the very lane it grades. The gap then reads "how far from the best
  feasible answer anyone at this pin returned". It is not a claim that the reference is the global
  optimum. A median or consensus reference is declined: two of the four voices are hven's.
- **Multi-basin flag.** If the pool's relative spread (max - min over `max(1, |min|)`) exceeds `1e-5`,
  `multi_basin = 1`: lanes returned different feasible objectives, so a positive gap may be a different
  local solution. Counted, never re-graded.
- **Beaten-reference flag.** A pool row below an R1 `f_star` by more than `eps_ref` relative sets
  `ref_beaten = 1`; the reference stays R1 and the case is listed for the owner.
- The reference is a function of the store's bytes and nothing else; a later pin's rows never change it.
- Signed gap bins are split, never folded: `gap < -1e-6` (beat the reference), `|gap| <= 1e-6`,
  `1e-6 < gap <= 1e-3`, `gap > 1e-3` (section 6, fix 8).

### 2.5 The calibration grid and its absent cell

The accuracy-matching calibration grid is the fixed set of cells on which the four engines' answers
are read at a natural tolerance and at a matched accuracy: four solvers (the SQP engine, the interior-point
engine, SNOPT, one further external solver) by {cold, native-warm} by the F7 problem at two sizes
(`nx = 1e4` and `nx = 1e5`), plus one nonconvex HS row. Each cell carries both readings (natural
tolerance and matched accuracy) or is a documented absent cell. The absent cell is: an external solver
on a problem outside the F7 family (no external solver had been run on an HS or other non-F7 problem
when the grid was defined).

Status of that cell:

- It was **filled at the M7 H4 external run** (`runs/2026-09-m7-h4-external`): IPOPT and SNOPT ran on
  all 45 HS and CUTEst-NLP TRAIN problems. What that run lacked was the matched-accuracy reading on
  those rows. Non-F7 problems have no manufactured optimum, so the matched-accuracy reading is supplied
  by sections 2.2-2.4 (referee gate, pool-minimum reference, gap), which need no manufactured optimum.
- It is **completed by CP-2 and CP-3** on the HOLDOUT rows (all four lanes, the same problems), at which
  point the filled grid is the comparison artifact's context table itself.
- A cell that cannot be filled (lane unavailable, no returned point, referee scales absent) is recorded
  `UNOBSERVED` with the reason, never zero-filled, interpolated or borrowed from F7.
- The grid licenses reading the context columns of external lanes beside hven's on the referee's
  definition of residual. It does not turn an external lane's own tolerance into hven's: "report, do
  not convert".
- Knitro is a **named gap**: at the HB-4 retrieval (2026-11-13, Mittelmann AMPL-NLP) the overlap with the
  pinned sets is 0 problems, below the brief's Q2 floor of 10. No Knitro claim is made, and the roadmap's
  Knitro claim exits as a named gap. Six table problems are exact CUTEst names excluded only by the
  fixed-dimension rule; they could form an untuned validation set in a later milestone.

### 2.6 The currency: serial solve-call wall time

(Owner ruling Q5, 2026-11-24; amends the roadmap's "counters primary" for the cross-solver standing only.
Counters stay the asserted currency for hven's own regressions.)

- **Currency.** `solve_wall_s`: wall time around the solve call, measured identically in every lane (the
  hven lanes, IPOPT, SNOPT), serial, one box, one solve alone on the machine (CLAUDE.md §7). Process
  start, library initialisation and problem load are outside it. Whole-process `wall_s` stays in the rows
  as an informational column and is never the currency.
- **Companion.** Evaluation counts (`eval_f + eval_g + eval_jac + eval_hess`), shown beside every wall
  figure. Native iterations are shown per lane as each solver's own count and are never ranked across
  solvers.
- **Shift.** `0.1 s`, declared before any row is read; `1 s` is the sensitivity row. Mean = shifted
  geometric mean `exp(mean(ln(max(1, t + shift)))) - shift` on the solve-time column.
- **Penalty.** One uniform declared absolute penalty: a failed cell (reading A for the headline, reading B
  for the referee row) is charged **600 s** (the larger of the two set wall ceilings), on every problem and
  every lane. It is not "the maximum budget over configs".
- **Budgets.** The wall ceiling is the only binding budget. Iteration ceilings are not binding in the
  comparison arms: hven-ipm 1000 per phase, hven-sqp 100 (label default: 300), IPOPT 3000 and SNOPT's
  problem-shaped limits. In the comparison arms each is set so that it cannot bind before the wall ceiling,
  and the artifact lists each lane's resolved ceiling beside its options digest; a cell that ends on an
  iteration ceiling anyway is reported by name.

## 3. What the rule forbids

- Re-grading any cell on the context columns; excluding a cell because its residual is loose; using the
  gap to rank.
- Shifting a lane's options to "match accuracy" (tightening IPOPT's `tol` toward hven's, or the reverse).
- Mixing the referee's residual with a lane's self-reported one in one column.
- Silently dropping a context value that is absent. Absent is printed.
- Ranking native iteration counts across solvers; any "best of hven" entry in a profile or a table.
- Re-tuning, re-labelling or re-defaulting on anything a HOLDOUT read shows.

## 4. The band (owner ruling Q4)

The leader for speed is IPOPT only. SNOPT runs without a Hessian and Knitro has zero overlap, so no speed
claim is made against SNOPT (section 6, fix 6). hven's **default lane** (named in the store manifest
before any row is read; one lane per engine) satisfies the band iff, under both readings (section 2.1):

1. **Coverage:** its solved count is at least IPOPT's (on the full set; on the commonly solved subset the
   two counts are equal by construction, so the clause is carried by the full set), and
2. **Speed:** its shifted-mean solve wall time (section 2.6) is at most `K = 2.0` times IPOPT's,

on each of two sets: **(a) the full set** (failures penalised at the uniform penalty), and **(b) the commonly
solved subset** (problems solved by both the hven lane and IPOPT under the reading, no penalty needed).
No accuracy exclusions remove a problem from either set. Coverage is exact (`>=`); the +-2 tie of section 6
fix 5 applies to the paired win/loss report only, never to a standing.

## 5. Classes (owner ruling Q4; the TU-1 family table)

Classes are the label clusters of the TU-1 declaration, section 6 (CUTE classification code plus size,
predicates in hven-bench `tu/clusters.py`), applied to every problem of the comparison set:

| class | predicate |
|---|---|
| K1 network | constraint type N (CLASSF `C = N`) |
| K2 quadratic objective, general constraints | `O = Q` and `C = O` |
| K3 constraint-dominant | `m > n` |
| K4 large | `n >= 500` |
| K5 sum-of-squares objective | `O = S` |

A problem may belong to several classes; a problem in none belongs to the class `other`. The table is fixed
here from the TU-1 declaration and is not re-cut after rows are seen. **Outlier failure:** the hven default
lane's solved count in a class is below every external lane's by **two or more problems** (the classes are
small, so a one-problem difference is noise). The robustness standing is MET iff its overall solved count is
at least IPOPT's and no class is an outlier failure, under both readings.

## 6. Fairness fixes (adopted from the decision memo, 2026-11-24)

1. **One uniform declared penalty**: 600 s absolute (section 2.6). Never the largest budget among lanes.
2. **The wall ceiling is the only binding budget** (section 2.6).
3. **Success rows resolved before publishing.** Every row that is solved under reading A and fails the
   referee gate (the YORKNET and DALLASM external rows in the dry run; HS25 at a non-reference objective) is
   listed in the artifact with its residual and gap, and the external multiplier mapping or the row is
   resolved before the comparison is signed.
4. **Held-out instances.** HOLDOUT-only numbers are printed beside the full-set numbers, labelled "held-out
   instances", with this disclosure: the hash split puts sibling problems on both sides of the split
   (STEENBRF against STEENBR*, TAXR* against TAX*, ACOP* variants, ZAMB2-n, READING7, DALLASL, NET2), and
   the HS models are copied from hven's own test battery, so the HS held-out problems are ones hven was
   developed against. Held-out instances are not independent instances.
5. **Paired wins and losses, with a tie.** Per pair (hven lane, external lane) the artifact reports problems
   where the hven lane's solve time is lower by more than the tie, higher by more than the tie, or tied; and
   solved-only-by-one counts. A difference of two problems or fewer in a count is a tie and is never reported
   as a lead.
6. **Handicaps in the headline.** SNOPT gets no Hessian and no linear/nonlinear split (hven-bench
   `docs/lanes.md` calls every SNOPT reading a lower bound): no speed claim is made against SNOPT. IPOPT runs
   on the one linear solver its local build has. Both are printed in the headline of the artifact.
7. **Pairwise profiles; no best-of-hven.** A profile compares exactly two configs. The artifact writes one
   per (hven lane, external lane) pair and none with two hven lanes in it.
8. **Signed gap bins** (section 2.4).
9. **Generator gates are hard in FINAL mode.** A dirty harness, `allow_mixed_pins`, and a CP-2 declaration
   that is not committed at its stated sha (checked with `git cat-file`, not a text match) each refuse.
10. **Knitro is a named gap** (section 2.5).

## 7. The holdout consequence (owner ruling Q6)

If the CP-2 read says a tuned default does not generalize (the pre-registered reading of the CP-2
declaration): the **tuned defaults ship anyway, the verdict is printed in the comparison artifact, no
generalization claim is made, and nothing is re-tuned.** If it says generalizes, the artifact says so
under the declaration's scope (whole configuration, one verdict per hven lane).

## 8. Changes from `cp1-v1`

| item | v1 | v2 |
|---|---|---|
| success | own status only; acceptable disclosed | two readings; MET only under both |
| residual scaling | `max(1, abs(f))` | feasibility vs `max(1, ||x||)`; multiplier-scaled stationarity |
| reference tiers | R1, R2 (certified pool), R3 (feasible pool), R4 | R1, R2 (primal-gate pool, stationarity flag), R4; R3 folded |
| calibration grid | guessed as lane by problem family | the four-solver grid, restated; absent cell filled at H4, completed by CP-2/CP-3 |
| speed currency | native iterations | serial solve-call wall time, evaluations companion |
| penalty | max budget over lanes | uniform declared 600 s |
| band, classes | undeclared; classes = problem set | 2.0x IPOPT + coverage, full and commonly solved; TU-1 family table |
| gap bins | signed gaps folded into `<= 1e-6` | split |
| profiles | all lanes in one profile | pairwise only |

## 9. Inheritance by M9b

M9b inherits `cp1-v2` by reference: same two readings, same referee gate, same pool-minimum reference (the
M8 frozen reference, never recomputed from M9b's rows), same currency and penalty. M9b adds columns, never
redefines one; changing a definition above, `eps_ref`, the shift or the penalty is a declared event with a
new rule id (`cp1-v3`), and M8's artifact stays readable under `cp1-v2`. M9b needs its own fresh holdout
(brief Q3).

## 10. What the artifact generator does with it

hven-bench's CP-3 generator (branch `cp3`) implements the above: it prints the headline with the handicap
disclosures, both readings, the shifted wall means at 0.1 s and 1 s, the paired tables, the pairwise
profiles, the class table and outlier test, the held-out section, the context table with split gap bins, and a
header with `cp1-v2`, `eps_ref`, the shift, the penalty, the multi-basin threshold and the tier counts. Its
self-tests pin each branch with hand-computed fixtures.
