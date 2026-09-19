# M7 W2 M2b — the ζ calibration's DECISION RULE, pre-registered

**This file is committed BEFORE any leg of the calibration runs.** It carries
the design page's rule verbatim, the candidate values it resolves to on each
cell, the procedure that applies it, and every deviation and extension —
declared here, in advance, rather than discovered in the write-up. Nothing in
this file may be edited once a leg has run: a correction is an APPENDED record
with its reason, on the M2 artifact's own precedent.

The rule's authority is `docs/notes/2026-09-m7-w2-m2-restoration-design.md`
§3.5, ruled by the settler and recorded in the ledger's W2 M2 (design) line
(`a494b7f5`). The precedent for pre-registering it is A4's
(`hven-bench/docs/w2-a4-decision-rule.md`, landed before any TRAIN run).

---

## 1. The candidate set, verbatim from the page's §3.5

Ordered smallest-first; every member keyed to the requesting iterate, every one
built from constants **already in the tree** — `kRestoProximityWeight = 1.0` and
`kRestoPenaltyParameter = ρ = 1.0e3` — so the set introduces no new constant:

| # | member | reading |
|---|---|---|
| 1 | `ζ = h_k / (n·ρ)` | the precedent's relative weight, spread over the coordinates |
| 2 | **`ζ = h_k / ρ`** | **the precedent, transcribed**: the nested l1 mode's `η : ρ` ratio, at violation scale |
| 3 | `ζ = h_k / n` | one coordinate's share of the entry violation |
| 4 | `ζ = h_k` | the phase pays its whole entry violation for one unit of relative displacement |

> On all seven cells of this artifact `n ≤ 540 < ρ` (`ladder/*.txt`: 102, 138,
> 312, 450, 468, 540, 540), so the listed order IS the numeric order. If a future
> cell has `n > ρ` the order is re-derived from the values, never assumed from the
> list.

## 2. The rule, verbatim from the page's §3.5

> * **the rule**: take the **smallest** member of the ordered set above that
>   (a) makes STEENBRB's, STEENBRC's and STEENBRG's measured final step
>   inadmissible, while (b) leaving ZAMB2-8's returned point and (c) BRITGAS's
>   0.031-excursion step admissible — each **by a factor of at least 10**. If no
>   member satisfies all three with that margin, **the shape is reported as not
>   calibratable without a new constant and returns to the settler** rather than
>   one being introduced quietly.
> * **the margin is part of the rule, and is pre-registered for a reason.** The
>   screen's free quantity (`prox_rel` against `dx_inf`) is unbounded in both
>   directions, so a member that clears a clause by a factor of 2 has not cleared
>   it. One order of magnitude is the smallest margin that is a finding rather
>   than a coincidence; it is a decision-rule constant, not an algorithm
>   constant, and it ships nowhere.
> * **incremental vs cumulative, resolved — (a) is INCREMENTAL, (b) and (c) are
>   CUMULATIVE, because they ask different questions.**
>   * (a) asks whether the sub-QP would still TAKE the runaway step, which is a
>     per-STEP question: the step `x_{t−1} → x_t` is inadmissible when
>     `(ζ/2)·[prox_rel(x_t) − prox_rel(x_{t−1})]` exceeds the violation it buys,
>     `h(x_{t−1}) − h(x_t)`.
>   * (b) and (c) ask whether the PHASE survives the price, which is a per-PHASE
>     question: the returned point is admissible when `(ζ/2)·prox_rel(x_r)` —
>     cumulative, from `x_k` — is below the phase's whole gain `h_k − h_r`. An
>     incremental reading of (b) would be vacuous: ZAMB2-8's final depth-1 row has
>     `step_norm = 0` (`ladder/ZAMB2-8.txt`), so every `ζ` would pass it.
>   * The leg prints **both** readings on every row regardless; the rule reads the
>     one named here.
> * **it is a TRAIN-fitted SCREEN on the `ζ = 0` ladder, not a prediction.** With
>   the term ON, the sub-QP's minimiser moves from iteration 1, so the trajectory
>   the screen is evaluated on is **not** the trajectory the fix produces. The
>   screen says which weights cannot be right; it does not say which one is. The
>   discrete choice among four members is a fit to the TRAIN cells; HOLDOUT is the
>   check and the fix's own TRAIN re-run is what decides.

## 3. The numbers the rule resolves to, fixed here

`h_k` is the M2 ladder's committed `restoration_h_in` (the engine's own reading
at the requesting iterate) and `n` is the cell's variable count, both from
`docs/notes/data/2026-09-m7-w2-m2-restoration/ladder/<CELL>.txt`. The shipped
field is an **absolute** weight, so each (cell, candidate) pair is ONE number,
listed here before any leg runs:

| cell | n | h_k | 1 · `h_k/(n·ρ)` | 2 · `h_k/ρ` | 3 · `h_k/n` | 4 · `h_k` |
|---|---:|---:|---:|---:|---:|---:|
| STEENBRB | 468 | 19800 | 0.04230769230769231 | 19.8 | 42.30769230769231 | 19800 |
| STEENBRC | 540 | 41800 | 0.07740740740740741 | 41.8 | 77.4074074074074 | 41800 |
| STEENBRG | 540 | 41800 | 0.07740740740740741 | 41.8 | 77.4074074074074 | 41800 |
| ZAMB2-8 | 138 | 24.643618798749973 | 0.00017857694781702878 | 0.02464361879874997 | 0.1785769478170288 | 24.643618798749973 |
| YORKNET | 312 | 48491.383947016526 | 0.15542110239428375 | 48.49138394701652 | 155.42110239428374 | 48491.383947016526 |
| BRITGAS | 450 | 115.03999999999996 | 0.0002556444444444444 | 0.11503999999999996 | 0.25564444444444434 | 115.03999999999996 |
| READING6 | 102 | 0 | 0 | 0 | 0 | 0 |

**READING6's ζ is 0 under every member, by construction and not by choice**: its
`restoration_entries` is 0, so there is no requesting iterate and no `h_k`. Its
four ON legs are therefore the same solve as its screen leg. It is run anyway,
as M1's converted control, and its role in this leg is to be UNMOVED.

## 4. The measurement, and the arms

* **The screen arm (ζ = 0), seven legs.** The page's §3.5 measurement: the same
  seven cells with the probe's new `prox_rel = ‖D(x − x_k)‖²` column, `D` built
  from `x_k` exactly as the engine builds it. **This is the trajectory the rule
  is evaluated on**, because clauses (a)–(c) are questions about the measured
  `ζ = 0` phase. The M2 ladder cannot supply it: it carries `dx_inf` only, and
  the page states that `prox_rel` is not recoverable from `dx_inf` in either
  direction.
* **The candidate arms (ζ > 0), 28 legs** — seven cells × four members, one leg
  per cell per candidate. These are **evidence, not the rule's input**: what the
  phase actually does at each weight (the S5 record, the outcome, the
  `infeasibility_certified` flag, the ladder's `dx_inf`, `radius` and
  `prox_rel`). They are what a reader checks the screen's verdict against, and
  what M2c's default event will re-read.
* **The OFF arm of the per-cell tables is the M2 ladder** (cited, not re-run)
  for every column M2 already carries.
* Every leg alone, under `flock /tmp/box-build.lock` with no timeout and no
  fallback, `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`, pinned to one physical core,
  one cell per process, with a `pgrep` guard for a foreign cell binary inside the
  lock. Wall is informational (CLAUDE.md §7); a per-leg safety stop is a
  CENSORED finding and never a row.

### 4.1 How the rows are read, fixed here

Stated before the legs run so that no reading is chosen after seeing a number.
The script is `wrappers/summarise_m2b.py` and implements exactly this:

* **The phase's rows are the `depth == 1` rows of the screen leg, in order.**
  The probe's `x_k` is the last `depth == 0` event before the first `depth == 1`
  one, or the start point when the phase is entered in major 0 — the M2 probe's
  own rule, unchanged, with `x_k_source` naming which fired.
* **Clause (a)'s "measured final step" is the LAST PAIR of those rows**:
  `penalty = (ζ/2)·[prox_rel(last) − prox_rel(previous)]`,
  `gain = h_main(previous) − h_main(last)`, `ratio = penalty/gain`, and the
  clause passes when `ratio >= 10`. A `gain <= 0` step is inadmissible at every
  ζ and is recorded as such (margin `inf`) rather than dividing by it.
* **Clauses (b) and (c) read the RETURNED point as the final `depth == 1`
  row** — the page's own reading, which used that row's `dx_inf` — with
  `penalty = (ζ/2)·prox_rel(final)` and `gain = h_k − h_r` taken from the SAME
  leg's S5 record (`restoration_h_in`, `restoration_h_out`). `ratio =
  penalty/gain`, the margin is `1/ratio`, and the clause passes when the margin
  is `>= 10`.
* **`ratio > 1` means the phase is refused**, which is the page's own convention
  in its worked preview, so the two are directly comparable.

## 5. The OFF-IDENTITY CHECK, and its STOP condition

The screen arm runs at `restoration_proximity = 0`, which is the shipped
default, on a library that now contains the term. **Its rows must reproduce the
M2 ladder column for column** — the shared CSV columns
(`depth,event_iteration,f_event,radius,step_norm,f_main,h_main,dx_inf`), the S5
record and the counters block. **A difference is a STOP**: the calibration does
not proceed, and the OFF claim of this task's code half is reported as broken.
(Settler ruling on the interim hand-back, 2026-09-18.) The one column that
cannot be compared is `prox_rel`, which M2 does not carry, and the one key that
must differ is the probe's new `proximity=` line.

## 6. The verdict, and what it is NOT

The script applies §2 to the screen arm and prints, per member, every clause's
ratio and its margin. The verdict **names one member or returns to the settler**.
Whatever it names:

* **NO DEFAULT MOVES IN THIS TASK.** `SqpOptions::restoration_proximity` ships
  at 0.0. The selected member is recorded as a RECOMMENDATION for M2c, the
  default event, with the TRAIN bar the page states: STEENBRB → `optimal` at
  9075.855; ZAMB2-8, READING6, BRITGAS unmoved; YORKNET unmoved.
* **The recommendation is recorded as the FORMULA**, not as a number, because
  every member is a formula in `h_k`. Whether the shipped spelling is an
  absolute weight or a dimensionless coefficient on `h_k/ρ` is **M2c's call and
  is not decided here** (settler ruling on the interim hand-back).
* **The screen is TRAIN-fitted and is not a prediction** (§2's last bullet). The
  candidate arms of §4 are what show the trajectory each weight actually
  produces, and the page's own TRAIN re-run is what decides.

## 7. Declared deviations and extensions, in advance

1. **The seven ζ = 0 screen legs are run** although the brief says the OFF arm is
   the M2 ladder, cited and not re-run. Reason: the rule is stated in `prox_rel`,
   which the M2 ladder does not carry and from which it is not recoverable.
   Settler-ruled on the interim hand-back; the legs double as §5's OFF-identity
   check.
2. **ζ is fixed per (cell, candidate) from the M2 ladder's `h_k`**, because the
   field is an absolute weight and the formula's `h_k` is only known once a phase
   has been entered. A candidate arm whose OWN `restoration_h_in` differs from
   M2's therefore carries a ζ that is no longer exactly its formula's value at
   its own entry: the tables print both `h_k` values and the drift, per cell per
   candidate, and a drifted row is read as what it is rather than as the formula.
3. **The page's worked preview is not this leg's answer** and is quoted nowhere
   as one. It substituted `prox_rel ≈ dx_inf²` and evaluated all three clauses
   cumulatively; this leg has the column and applies the rule as written in §2.
4. The probe is M2's, with two additions and nothing else: `--proximity ζ` and
   the `prox_rel` column (`wrappers/m2b_probe.cpp`'s own header lists them).
5. **At ζ > 0 the depth-1 `f_event` column is the wrapper's `h + P`, not its
   violation** — the `f_event == h` identity is a ζ = 0 fact and is labelled as
   one wherever this artifact reprints it (the page's §3.5 caveat, review F4).
