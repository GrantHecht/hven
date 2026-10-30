<!-- M7 W2 E4 (the proximal EQP) -- the implementer report for round 0 (the proximal EQP), copied VERBATIM from
     .superpowers/m7-w2-e4-report.md. Nothing below this comment is edited: the three
     bytes after this preamble are that file byte for byte (cmp-checked). -->

# M7 W2 E4 — the proximal EQP, with the walk's tests in its currency, and the EQP-site residual check

Implementer hand-back. Worktree `agent-a69f46e11796560b9`, branch `e4` on `c864b8a6`
(sources `0fa59cdc` = NC2 + fix1). ONE squashed commit, **`c46485d8`**, parent `c864b8a6`,
tree clean, no attribution trailer. Builds `.scratch/nc2-build-{debug,release}` (clang,
`HVEN_FP_MODE=SAFER_FAST`, incremental). Evidence `.scratch/e4/`; `.scratch/e3/`,
`.scratch/nc2/`, `.scratch/nc2-fix1/` read-only (read only to COPY tools and to compare).
Every solver leg under `flock /tmp/box-build.lock` with the `PGREP_EMPTY` /
`FOREGROUND_START` / `FOREGROUND_END` markers, `set -o pipefail`,
`MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2`, one solver at a time.

SIGNOFF: M7-W2-E4-REPORT

## 0. THE ANSWER, IN ONE PARAGRAPH

All four clauses are built and measured, and three of them do exactly what E3 predicted they
would. **On the 51-dump corpus the pair converts THIRTEEN cells to `kOptimal` on the
eliminated path and TWELVE on the border path, loses NO certified cell on the eliminated
path, produces NO NaN anywhere, and holds the T7 feasible census at 120/120.** The
conversions are the SNOPT-set carriers: `A0_YORKNET` qp1–qp6 **all six**, `Cs_STEENBRB`
qp1/qp3/qp5/qp6 at the Clarabel optimum, `STEENBRB_def/qp2` — one of E3's two NaN cells —
`kMaxIter` → `kOptimal` at 392 minors with a finite, better objective, plus
`Hj_STEENBRD/qp4`, `YORKNET_def/qp1` and `ACOPR57_def/qp3`. The equality drift falls with
them (`A0_YORKNET/qp3`: `eq` 2.50e−01 → 8.86e−14, `eq_drift_violations` 7502 → 12, `stat`
2.14e−02 → 3.79e−10). E4-2's termination test restores `ACOPP57_def` qp0–qp3 to their base
2 minors on the eliminated path; the three runaway fixtures pass UNMODIFIED; E4-4's check,
routed to the engine's own suspect-stall ladder, is what converts the NaN cell. **And it
does not ship yet, for three measured reasons**: `ACOPP57_def` qp0–qp3 are lost on the
BORDER path only (`kOptimal` in 8 minors → `kMaxIter` at 1795, because that face's bordered
answer is 1.25e−07-stationary against the eliminated path's 1.02e−14 and the proximal step
amplifies the gap by 1/δ); **83 of 2863 Release tests fail**, of which about 25 are fixtures
that PIN the cycling this change removes and six `B1Gate` cells plus `HsSweepCorpus.Corpus`
re-open the **B-1 complementarity family** (0.177 = μ·λ² at a warm-certified point); and
U0's walk terminal `kkt_stationarity` moves from ~1e−15 to ~1e−11 on the F7 bound family,
which is the new test stopping at the engine's own `opt_tol` instead of grinding past it.
The commit is a candidate with those three items named; the fixture re-derivation and the
B-1 re-check are the next round's.

## 1. THE SYSTEM AS LANDED (E4-1)

`eqp_solve.h`'s header now states it in full (three new sections: THE PROXIMAL CENTRE,
the rewritten Iterative refinement, THE EQP-SITE RESIDUAL CHECK). The statement:

    min  (1/2) x'Hx + g'x + (delta/2)||x_F - x_k,F||^2
    s.t. Ae x = be,  Aw x = bw,  x_P pinned

    free-block row:  (H_FF + delta I) x_F + Ae_F' le + Aw_F' lw = -g_F - hess_shift + delta*x_k,F
    in the step:     (H_FF + delta I) p_F + Ae_F' le + Aw_F' lw = -(g + H x_k)_F - hess_shift
    refinement:      ONE step against the PROXIMAL system (only the DUAL mu backed out)

K is bit-identical to the unshifted assembly; only the free block's rhs moves. This is
E3's mode 2, re-implemented with **no env toggle and no thread-local**: `x_k` is a
REQUIRED parameter of `solve_eqp` and `solve_bordered_eqp`, threaded explicitly through
`eqp_candidate`, `eliminated_candidate`, `border_candidate`, `border_solve_or_fall_back`,
`probe_inertia`, `probe_zero_multiplier_drops`, `reread_wrong_inertia` and
`build_bordered_system`, so every signature on the path says which point the answer is a
step from. The centre is always the pass's CURRENT iterate (`refine_on_face` passes the
caller's own `face.x`; the probe/inertia-read sites pass the same iterate although only the
factorization's inertia is read there). References: Friedlander & Orban 2012 and
Gill & Wong 2015 for the proximal/primal-dual regularized form; Gill, Murray, Saunders &
Wright 1991 was NOT needed — the working-set KKT test this round uses is the engine's own
`free_block_stationarity`/`free_block_is_stationary` pair, unchanged. Papers only.

**THE BORDER TWIN SOLVES THE SAME SYSTEM, justified per variable** (E4-1's explicit ask).
The proximal term goes on EVERY variable row of the bordered rhs, and the reason is
different for the two kinds of row:
- a FREE variable's row IS the eliminated path's free stationarity row, and gets the same
  `+delta*x_k,i`;
- a PINNED variable's row is one the eliminated path does not have — it substitutes `x_i`
  out — while K0, assembled bound-free, carries `+delta` on it anyway. What the elimination
  IMPLIES for that row is that the delta term is absent, and `+delta*x_k,i` supplies exactly
  that: the pin border holds `x_i` at its bound, which IS `x_k,i`, so the two cancel to
  `O(mu*delta*|y_i|) ~ 1e-16`.
  MASKED instead (the term on free rows only) that cancellation does not happen, the
  leftover `delta*x_i` perturbs `x` through the pin (`x_i = bound + mu*y_i`), and it is
  enough to **flip a weakly-active bound on `QpEngineBorder.BorderModeMatchesRefactorizeMode`**
  — same `x` to 1e−8, different `bound_state`. MEASURED, and the reason the landed form is
  unmasked. (That fixture still fails for a different reason; §3f.)

**`bordered_regularization` no longer reports the primal delta**, on both of its consumers:
the refinement TARGET (which is the proximal system now, on both algebras) and the iterated
loop's footprint stopping rule. `bordered_regularized_residual` is factored out as the
`rhs - A*sol` primitive the residual check reads and `bordered_residual` is expressed in
terms of it — no behaviour change in that refactor. `ws_independence.cpp`'s
`DepSystem::residual` had to zero `primal_delta` in a local copy of `opts` to get "the
factored system with mu only backed out"; that is now what the function returns, and the
hack is gone (same object, checked).

`unrefined_out` is kept as the seam column (E3-11) and now means "before the model's mu-free
step", which is byte-identically the raw solve whenever the residual check spends nothing.

## 2. CLAUSE BY CLAUSE, WITH ITS FIXTURES AND VALUES

### E4-2 — termination in the EQP's own currency. **DELIVERED on the eliminated path.**

The test, in `qp_engine.h` section 5 and in the loop:

    zero_motion        ||p||inf <= step_tol                 (kept, as the observation it was)
    stationary_on_ws   free_block_stationarity <= opt_tol*scale  AND  worst_structural_violation == 0

`stat_free_inf`/`stat_scale` are now computed in PRODUCTION, once, and the seam reports the
same two values rather than recomputing them. **I use the stationarity residual, not
`delta*||p||`**, and the reason is that it is the quantity the classification below already
decides `kOptimal` on: one number, one threshold, two branches, so the loop cannot recognise
a point its own classifier would refuse (and vice versa). `delta*||p||` is the same number
to within `(H_FF + delta I)` on a flat face — the header says so — but it is not the same
number on a curved one.

NOTHING MOVES on the second disjunct: the snap `x = eqp.x` is taken for zero motion only.
A stationary point IS the answer, and stepping to the proximal minimiser from it would move
`x` by up to `opt_tol*scale/delta` with no ratio test having been run.

| fixture (E4-2) | base | E3's arm | E4 |
|---|---|---|---|
| `ACOPP57_def/qp0` refactorize | opt, 2 minors | cap, 1795 | **opt, 2 minors** |
| `ACOPP57_def/qp1..qp3` refactorize | opt, 2 | cap, 1795 | **opt, 2** |
| `ACOPP57_def/qp0..qp3` border | opt, 8 | cap, 1795 | **cap, 1795 — NOT recovered (§3f)** |
| `A0_YORKNET/qp3` (the 300-full-step tail) | cap, 7680 | cap (non-terminating) | **opt, 725 minors** |

### E4-3 — the runaway guard in the step's currency. **DELIVERED as the iterate's currency on a moving pass; the ridge-scale step guard was built, measured and REMOVED.**

What ships: `is_runaway(qp, x, ws, eff_opts)` — the same predicate, unchanged — asked at the
head of the ordinary path on every pass that is about to move, plus a NON-FINITE-answer
refusal at the EQP site. Why the iterate's currency has to be asked there is in
`qp_engine.h` section 5: under the absolute centring the artifact was self-limiting (the
answer at the ridge IS the ridge's fixed point, so the next pass's `p` vanished and the
terminal classification read the runaway off the iterate); under the proximal centring the
same 1e+08-sized answer is proposed again from the point it just reached, so the walk burns
its cap instead of reporting the artifact.

**The three named fixtures pass UNMODIFIED**, Debug and Release
(`QpEngine.UnboundedProblemIsReportedAsNumericalErrorNotOptimal`,
`RunawayWithActiveRowClearsItsMultipliers`, `EffectivelyInfiniteBoundsStillDetectTheRunaway`).
Placement is what they decide: they assert BOTH `kNumericalError` AND `|x(0)| > 1e7`, so the
guard must fire on the pass that would take a SECOND ridge step, not the first.

**AND THE STEP-CURRENCY FORM THE BRIEF NAMES DOES NOT SURVIVE ITS OWN BATTERY.**
`||p||inf > 0.1/delta * max(1, ||x||inf)` with the step-does-not-move conjunct fires **at
iter 0, with `alpha = 0`, on an ORDINARY DEGENERATE FIRST STEP**: measured on the HS battery,
10 firings, e.g. `x_inf=0 p_inf=8e+09 alpha=0 dx=0 kind=1 scale=1e+07` and
`p_inf=7.82e+11 alpha=-0`. A blocked zero-length step is a normal, well-handled event (the
engine has a whole zero-motion machinery and a `2(n+mi)` cap for it), and turning it into
`kNumericalError` cost `HS10` and `HS27` their `kOptimal` exits in both algebra modes. It
was removed rather than tuned. What it was written for — E3's two NaN cells, `p_inf` reaching
6.8e+97 while `x` never moves — is answered twice over without it: E4-4 refuses those cells'
first solve, and the non-finite-answer refusal means **no NaN can reach `f`** (measured:
`STEENBRB_def/qp2` and `/qp3` both finite, and qp2 now `kOptimal`).

### E4-4 — the EQP-site residual check. **DELIVERED, and it is what converts the NaN cell.**

Per EQP solve, on both algebras, before the model's mu-free step (order is load-bearing:
afterwards the row residual IS the model's own `mu*lambda`):

    r = rhs - K*y                                  (one mat-vec)
    ratio = ||r_rows||inf / max( 1e-1*(1 + ||rhs_rows||inf),  mu*||lambda||inf )
    while ratio > 1 and iters < 3:  y += K^-1 r    (kept only while the ratio strictly falls)
    refused iff !(ratio <= 1)                      (a NaN refuses)

Two of the three constants are measured, not chosen:
- **the CONSTRAINT ROWS ONLY**, which is E4-4's own wording and which I first disobeyed. The
  same check applied to the free block as well refuses the FIRST solve of every restoration
  cell in the corpus (`Cs_STEENBRB/qp1` at 1.95× its free-block scale, `A0_YORKNET/qp3` at
  1.011×) on faces whose ROW residual is 1.3e−08 and 6.8e−07 and which the walk then solves
  to the Clarabel optimum. A flat face's free block IS `delta*I`, so its stationarity rows
  are the ill-conditioned half by construction and their residual is not evidence about the
  solve.
- **the scale carries `mu*||lambda||`.** Without it, `HS10` and `HS27` are refused on
  3-variable over-determined faces whose row residual is 3.1e−05 and 7.8e−03 against a
  `mu*|lambda|` of order 1 (Pardiso perturbs a pivot there and the answer is still usable),
  and two `kOptimal` exits become `kNumericalError`.
- `1e-1` sits between the two measured populations: usable answers at 1.1e−13 … 7.8e−03,
  failures at 2.6e+02, 7.1e+02, 9.3e+10, 1.3e+14 (E3 §1c, §3e).

**THE ROUTING IS THE ENGINE'S OWN LADDER, and that is the round's single most valuable
correction.** A refusal first retreats the border path to the bound-eliminated form (the
existing fall-back), and a refusal that still reaches the walk takes 4b's SUSPECT-STALL
ESCALATION — one decade of `primal_delta` per rung, the border state discarded, at most
`kMaxSuspectEscalations = 3` rungs — and only an EXHAUSTED ladder reports `kNumericalError`
at the iterate the walk stands on. With the refusal ENDING the solve instead, the corpus
reads: `A0_YORKNET` qp2–qp6 `cap -> num` at 1–121 minors, `Cs_STEENBRB/qp2` and
`R1_Cs_STEENBRB_qp3` `opt -> num` (two certified cells lost), `STEENBRB_def/qp2` `num` at 1
minor. With the ladder: those same cells read `A0_YORKNET` qp2–qp6 **`cap -> opt`**,
`Cs_STEENBRB/qp2` and `R1_Cs_STEENBRB_qp3` **unchanged and still `opt`**, `STEENBRB_def/qp2`
**`cap -> opt`**. `A0_YORKNET/qp3` spends 3 refusals and 5 refinement solves on its way to
`kOptimal`.

E4-4's fixtures, the two NaN cells:

| cell | base | E3's arm | E4 |
|---|---|---|---|
| `STEENBRB_def/qp2` | cap 4680, `f = -990591855583` | **`f = -nan`**, num 559 | **opt, 392 minors, `f = -990729858499`, `stat` 1.65e−05 → 5.15e−10** |
| `STEENBRB_def/qp3` | num 1185, `eq = 8.11e+02` | **`f = -nan`**, num 2963 | num, `f` finite, `eq` 8.11e+02 → 5.42e−09 (same status, honest exit) |

The state dump E3-11 asked for at those passes was not taken as a separate artifact: the
in-engine ratio and refusal columns (`eqp_row_residual`, `eqp_residual_ratio`,
`eqp_residual_refused`, `eqp_residual_iters` on the walk record) answer the same question
per pass and are now permanent seam columns. What they say for qp2 is that the first solve's
row residual clears the refusal target and the ladder's first rung is enough — i.e. **the
failure is the factor, not the 1/delta amplification**, because one decade of delta fixes it.

### E4-5 — what does not change

The drop rule, `ride_sign`, the noise mark, the zero-length-ride rule, the E2 exchange
machinery, IPARM: untouched. **No option was added**, and the change reaches EVERY QP solve
— E2 on, E2 off, IPM-side and SQP-side alike — because `assemble_kkt_core` applies the primal
delta on every EQP solve regardless of `independent_working_set`. That is DECLARED, and §3's
E2-OFF corpus, the census, U0 and the suites are the measurement.

Two counters are added (`QpCounters::eqp_refine_iters`, `eqp_residual_refusals`) and three
walk-trace columns. `QpCounters` carries no aggregate-arity assertion and its fields are
enumerated nowhere else (checked: `reentry_exemptions` appears only in `solver_counters.h`
and `qp_engine.cpp`), so nothing else had to move. They are NOT aggregated into
`SqpCounters` — registered (§7).

## 3. THE BATTERY

### 3a. The 51-dump corpus, E2 ON, both algebras, all columns

`rowcmp.py fix1 e4` (`.scratch/e4/logs/rowcmp-e2on-v4.txt`); base = NC2 fix1's own rows,
copied byte-for-byte from `.scratch/nc2-fix1/corpus/`. **51 of 51 rows move on each
algebra**; 21 change status on border, 19 on refactorize.

**THE WINS (both algebras unless noted).**

| cell | base | E4 | reading |
|---|---|---|---|
| `A0_YORKNET/qp1` | num 777 | **opt** | |
| `A0_YORKNET/qp2` | cap 7680 | **opt** | |
| `A0_YORKNET/qp3` | cap 7680, `eq` 2.50e−01, `stat` 2.14e−02, `eqviol` 7502 | **opt 725, `eq` 8.86e−14, `stat` 3.79e−10, `eqviol` 12, `f` −10552.28 → −10933.567** | the drift cell, converted |
| `A0_YORKNET/qp4` | cap 7680, `eq` 5.17e+00 | **opt** | |
| `A0_YORKNET/qp5` | cap 7680 | **opt** | |
| `A0_YORKNET/qp6` | cap 7680, `eq` 7.81e−03 | **opt** | all six of the restoration family |
| `Cs_STEENBRB/qp1` | cap 6600, `f` −1378.22272466 | **opt 232, `f` −1378.22320088** | the Clarabel optimum; `stat` 3.93e−07 → 1.08e−16 |
| `Cs_STEENBRB/qp3, qp5, qp6` | cap 6600 | **opt** | the cycling family |
| `STEENBRB_def/qp2` | cap 4680 | **opt 392** | E3's NaN cell |
| `Hj_STEENBRD/qp4` | num 283, `stat` 5.76e−01 | **opt 295, `stat` 3.85e−17** | |
| `YORKNET_def/qp1` | num 596, `eq` 3.59e+03 | **opt** | base's point was not feasible |
| `ACOPR57_def/qp3` | cap 4360 | **opt 2** | |

**THE LOSSES.**

| cell | base → E4 | mechanism |
|---|---|---|
| `ACOPP57_def/qp0..qp3`, **BORDER ONLY** | opt 8 → cap 1795 | that face's bordered answer is 1.25e−07-stationary (the eliminated path's is 1.02e−14); the proximal step turns that into `p ~ 12.5` and the walk cannot satisfy a test written at `opt_tol`. `refuse = 0`, so NOT the residual check. §3f. |
| `ACOPR57_def/qp6` | cap 7100 → num 41 | an honest early failure replacing a cap; no certified point either way |
| `DALLASM_def/qp1,qp2,qp3`, `DALLASM_f4/qp2,qp3` | num↔cap both ways | `eq ~ 1.5` in BOTH arms on every one of them: churn on a family that never reaches feasibility, as at base |

**NO CERTIFIED (`kOptimal`) CELL IS LOST ON THE ELIMINATED PATH.** On the border path the
four `ACOPP57_def` cells are.

### 3b. The 51-dump corpus, E2 OFF — the option-off path moves too, and the drift gets worse there

`rowcmp.py fix1off e4off --alg border` (`logs/rowcmp-e2off.txt`). **51 of 51 rows move**;
8 change status.

| direction | cells |
|---|---|
| WON `cap -> opt` | `Cs_STEENBRB` qp3, qp4, qp5; `A0_YORKNET/qp1`; `Hj_STEENBRD/qp4` |
| **LOST `opt -> cap`** | **`A0_YORKNET/qp6`**, **`Cs_STEENBRB/qp2`** |
| LOST `cap -> num` | `Hj_STEENBRD/qp1` |
| **DRIFT MADE WORSE** | `A0_YORKNET` qp3 `eq` 4.54e−14 → **5.00e−01**; qp4 2.32e−02 → **2.50e−01**; qp5 1.74e−14 → **1.25e−01**; qp6 4.55e−15 → **6.25e−02** |

That is E3 §3d's finding reproduced exactly: on the E2-ON path the change REMOVES the
drift, on the E2-OFF path it CREATES it on four cells and costs two certified cells. The
option-off walk is not a different system — it is the same EQP without the independence
machinery that repairs a dependent working set — so this is evidence about that machinery,
not about the centring.

### 3c. The T7 feasible census — **120/120 HOLDS**

`QpEngineStructuralViolation.T7TheFeasibleCensusHalfOverTheFULLDualMuSet` PASSES in the
Release suite and in the final targeted run. E4-6's STOP criterion is met.
Its sibling `T7ACertifiedInfeasibleExitRETURNSTheRefinedPointInBothAlgebras` fails on one
pinned count (`latched_border.factorizations` 3 → 4; the test's own note says "a regression
to a fresh factor per entry reads 4"): every other assertion in it passes, including
`verdict_refine_steps == 9` on both twins and the two `x0`/residual pins, so the structural
claim holds and the COUNT moved. Attribution is one extra minor on the latched route
(§3e's cleanup pass); it is a counter re-pin, NOT made in this round.

### 3d. U0, 27 cells × 75 columns × 3 engines, against the M6-close pin (gate 1e−5)

`logs/u0.log`, `u0/comparison.txt`.

| engine | differences | within gate | the classes |
|---|---|---|---|
| walk | **51** | 37 | (i) `kkt_stationarity`/`kkt_residual` DEGRADE 4–5 decades on the F7 bound family (1.02e−15 → 1.00e−11; 9.19e−14 → 9.20e−10; 4.20e−14 → 4.20e−10); (ii) `qp_minors` 2 → 3 on five cells; (iii) `kkt_primal` IMPROVES by 2 decades on several (1.94e−13 → 2.44e−16) |
| ssn | **2** | — | both `kkt_primal`, strictly better (1.94e−13 → 2.52e−16) |
| ipm | **43** | 42 | same character as the walk's, one decade smaller |

**Class (i) is the new termination test, and it is the cost of it.** The walk now stops as
soon as the free-block stationarity residual is within `opt_tol*scale`, where before the
`||p|| <= step_tol` test kept it stepping until the answer stopped moving at all — which on
these cells was ~1e−15. Every degraded value is still far beneath `opt_tol*scale`; nothing
changes status, verdict or major count. Class (ii) is the O(δ) cleanup pass (§3e). Class
(iii) is the proximal fixed point being genuinely better on the rows.

E3's mode 2 had 37 walk differences with residuals strictly BETTER; the difference is E4-2's
test, which E3's arm did not have. That trade — terminate correctly at `opt_tol`, or grind to
1e−15 and cap on `ACOPP57` — is the settler's to rule on.

### 3e. The cleanup pass, measured (the mechanism behind `qp_minors` +1 and two fixtures)

`.scratch/e4`'s per-minor probe on `simple_box_qp`, both algebras
(the record the scratch harness printed; the harness itself is removed from the tree):

| minor | refactorize | border |
|---|---|---|
| 0 | add, `p_inf` 2, `alpha` 0.333 | add, same |
| 1 | moveOnly, `p_inf` 0.333, `alpha` 1 | moveOnly, same |
| 2 | **moveOnly, `p_inf` 3.33e−09, `alpha` 1** | **add(bound), `p_inf` 3.33e−09** |
| exit | opt, `x0 = 1.96e−17`, `bound_state = (free, free)` | opt, `x0 = 0`, `bound_state = (atLower, free)` |

`p_inf = 3.33e-09` at minor 2 is `delta*|x_k - x*|` exactly: the proximal model's own bias,
which the walk then closes in ONE more pass (the error falls from `delta*0.33` to
`delta^2*0.33 = 2e-17`). So the proximal walk's fixed point IS the exact KKT point — a `p = 0`
answer says the unregularized stationarity holds at `x_k` — and it costs one extra pass per
face whenever `delta*||p|| > opt_tol*scale`. On that pass the landing ratio to the bound
`x0 >= 0` ties at 1 within `kEngineStepTieTol = 1e-9` on the border path and not on the
eliminated one, so the two algebras end at the same point with different `bound_state`.

### 3f. `ACOPP57_def` on the border path — what it is, and what it is NOT

`refuse = 0` on those rows, so the residual check is not involved. Measured: the base's
BORDER row for `ACOPP57_def/qp0` reads `stat = 1.25e-07` at its `kOptimal` exit, against
`1.02e-14` on refactorize — i.e. the base certified a point whose stationarity residual is
two decades ABOVE `opt_tol` (its own trustworthy-range caveat (ii)). Under E4 the proximal
step from that point is `p ~ (1/delta)*1.25e-07 = 12.5`, the stationarity test refuses to
call it converged, and the walk spends 1795 minors with **0 adds and 0 drops** — the
zero-motion cap counts releases and re-entries, so nothing bounds a stall that adds and
drops nothing.

I tested one hypothesis for it and it was WRONG: that the border refinement's footprint
stopping rule, which leaves `||delta*sol||inf ~ 1e-07` behind, is the cause. Both forms of
the footprint (delta kept, delta-free) give the SAME 51 rows on both algebras. The
delta-free form is kept because it is what the model says. So the remaining reading is the
plain one — the border path's own accuracy on that face — and the two candidate answers,
neither taken in this round, are (a) an accuracy fix on the bordered solve for that face and
(b) a stall rule for an ordinary pass that changes nothing at all.

### 3g. The suites — Release, 2863 tests, **83 failures**

`logs/ctest-rel-e4.log`, `ctest -j4` under `setsid`, `MKL_NUM_THREADS=1`, inside the lock.
Per-test first failure messages: `logs/failure-messages.txt`. By suite:

| count | suite | what the messages say |
|---|---|---|
| 6 | `QpAntiCyclingArm` | **"`off.solution.status` Which is: 0, expected `kMaxIter`"** — the period-2 and period-4 cycles ARE GONE. Every one of these six fixtures asserts that the unarmed walk cycles. |
| 10 | `QpDegeneracyArm` | 4 of the same ("really does cycle"), 6 diagnostics that now have nothing to observe (`drops > 0`, `total_diag > 0`, `loud_refused > 0` all read 0 — the arms are asked about drops the walk no longer makes) |
| 8 | `QpE2WorkingSet` | 4 `dep_border_retreats` 1 → 0 (the retreat is not needed any more), `AMuFreeInertiaReReadCertifiesTheSingularPsdFaceOfAcopp57` (the border `ACOPP57` loss, §3f), 4 minor-count pins on the STEENBRB restoration QPs |
| 3 | `QpNc2FlatFace` | fix1's own fixtures: qp3 `status` 0 vs `expected` (it now CONVERGES where the fixture pins a cap), and `ANoiseLevelDropIsUndoneAndCountedNotRidden` (`ride_sign_vetoes` 1 → 0 — no veto because no ascending ride) |
| **6** | **`B1Gate`** | **the serious class**: the WARM arm's `f` off the analytic path by 4.28e−02 (`ReviewSphereAgreesWithItsAnalyticPathAtEveryStepSize`), `worst_rel` 6.7e−02 against 1e−6 where "pre-repair the WARM arm read 0.293", a bit-identity pin, and the watchdog-restore premise no longer firing |
| 1 | `HsSweepCorpus.Corpus` | same family: `complementarity` 0.177 = `mu*lambda^2` at a warm-certified `HS10`/`HS33` point, with `primal = 0` and `stationarity ~ 1e-9`, i.e. a multiplier of ~1.3e+03 on a row slack by `mu*lambda` |
| 5 | `QpEngineIndefinite` | ride/no-blocker and local-minimiser fixtures on synthetic indefinite QPs |
| 2 | `QpEngineBorder` | the degenerate-vertex tie of §3e |
| 2 | `HsBattery` | counter pins: `border_refine_steps` 10 → 6, `minors_off` 802 → 824 |
| ~20 | driver-side (`SqpDriverRestorationSeed` 6, `SqpDriverCertifiedFallback` 4, `SqpDriverElastic` 3, `SqpEqualityBasis` 3, …) | not individually attributed in this round |
| 5 | one-offs (`Continuation` +1 minor, `CorpusTask6bPhaseB` a derivation bound, `IpqpDispatch` ×2, `PredictorRatioTest`, `ScaleF7`, `F7ColdScaleSmoke`, `QpEngineTr`, `QpEngine.T6b…`) | counts and residual pins |

**Reading, stated plainly.** About 25 of the 83 are fixtures that pin a pathology this
change REMOVES, and they are evidence FOR it, not against it — but each needs a
re-derivation, which is a declared event and not a silent re-pin. The `B1Gate` +
`HsSweepCorpus` class (7 tests) is a genuine quality regression in the WARM-start
multiplier path, of exactly the family B-1 was repaired for, and it is the one item I would
put ahead of everything else in the next round. The Debug suite was NOT run at the final
head (targeted Debug runs were: the runaway fixtures, `EqpSolve`, the census, the HS battery,
`QpEngineBorder`, `QpNc2FlatFace`); Release is the full measurement here, and the Debug/Release
failure sets agreed on 46 of 50/52 under E3's arm.

### 3h. HS pins

`HsBattery.RefactorizeMode`, `BorderMode`, `AggregateCostIsRecordedAndBounded`,
`EveryElasticActivationRunsTheRhoLadderToItsCeiling`, `ModeStatusDivergenceIsEmpty`,
`NoBatteryProblemEverCertifiesInfeasibility`, `BorderModeFalseInfeasible`,
`FeasibleStartsEndFeasibleDespiteTheLooseFunnelWidth`, `SocIsRareAndUnprofitable…`,
`AdaptiveMuNeverSharesAMajorWith…`, `TableCoversEveryShippedProblem` — **all PASS**. Two fail:
`RefinementStepCountersOnBorderRepro` (`border_refine_steps` 10 → 6) and
`CrashBasisIsANullResultOnTheBatteryWithTwoBoundSeededExceptions` (`minors_off` 802 → 824,
`minors_on` 799 → 821) — both counter pins, both NOT moved in this round, plus
`HsSweepCorpus.Corpus`, which is a correctness fixture and is reported as a regression rather
than re-pinned.

## 4. THE CALIBRATION HISTORY — five measured corrections to the round's own design

Recorded because each one is a fact about the engine, and because a later round should not
re-derive them:

1. **The ridge-scale STEP guard is not implementable as stated** (§E4-3): it fires on an
   ordinary degenerate first step. 10 firings on the HS battery, 2 `kOptimal` cells lost.
2. **The residual check's tolerance must carry `mu*|lambda|`**: at `1e-6*(1+||rhs_rows||)`
   it refuses `HS10` (3.1e−05) and `HS27` (7.8e−03), both usable.
3. **The residual check must read the ROWS ONLY**: with the free block included it refuses
   the first solve of every restoration cell (ratios 1.95, 1.011).
4. **A refusal must route to the suspect ladder, not to an exit**: ending the solve costs
   five `A0_YORKNET` conversions and two certified `Cs_STEENBRB`/`R1` cells; the ladder
   converts them instead.
5. **The border proximal term must be UNMASKED**: masking leaves a `delta*x_i` the
   eliminated path does not have and flips a weakly-active bound.

And one hypothesis MEASURED FALSE: the border refinement's footprint is not what loses
`ACOPP57_def` (§3f).

## 5. FIXTURES (E4-7)

**Not added.** E4-7's list is conditional on the clauses passing their own fixtures, and
three of the seven items are open (`ACOPP57` on the border path, the two NaN cells' state
dump as a separate artifact, `R2_Cs_STEENBRB_qp1` as a tree dump), while the suites are at
83 failures — adding fixtures to a tree whose existing ones need a declared re-derivation
would put this round's new pins underneath that event rather than on top of it. What each
item would have asserted, with the value it would have asserted:

| E4-7 item | status | what it would pin |
|---|---|---|
| the ordinary-step DESCENT INVARIANT on a flat face | measurable, not built | E3 §3f's tolerance form: assert the model decrease with the `1/delta`-amplified RANGE component accounted for, i.e. restrict to passes with `active_res <= delta*tol` and assert `g'p <= kEngineDenomTol*||g||*||p||` there |
| `R2_Cs_STEENBRB_qp1` at the Clarabel oracle | **available now**: `f = -1378.22320088` at 232 minors, both algebras | the dump is `docs/notes/data/2026-09-m7-w2-r2-composite-landing/dumps/Cs_STEENBRB/qp1.txt` |
| `A0_YORKNET_qp3` drift-free `kOptimal` | **available now**: opt at 725 minors, `eq = 8.86e-14`, `eqviol = 12` (not 0) | note `eqviol` is 12, not 0 — the invariant fires 12 times before the rows are met for good |
| the `ACOPP57_def` termination cells | **half available**: refactorize 2 minors; border still capped | |
| the two NaN cells | **available now**: qp2 `kOptimal` 392 minors, qp3 `kNumericalError` with finite `f` and `eq` 8.11e+02 → 5.42e−09 | plus `eqp_residual_refusals >= 1` on qp3 |
| the three runaway fixtures unmodified | **PASS** | unchanged in the tree |
| every Debug test < 600 s | NOT verified at the final head | two `QpNc2FlatFace` Debug cells took 285–293 s at an intermediate head; they are inside the bound but the margin shrank |

The two `EqpSolve` fixtures that the system change broke were **fixed, not re-pinned**:
`MatchesOracleOnItsOptimalWorkingSet`, `MatchesOracleOnEqualityOnlyProblem` and
`FixedVariableWithEqualityRefinementImprovesAccuracy` now pass `oracle.x` as the proximal
centre with their tolerances UNCHANGED (1e−10, 1e−10, 1e−9), because the proximal EQP's
fixed point is the face's exact KKT point for any `delta` — which makes those tests a
statement about the assembly, the sign convention and the refinement rather than about the
model's `O(delta*||x* - x_k||)` bias. The eight other direct `solve_eqp`/`probe_inertia`
call sites in tests pass `Vec::Zero(n)` (they discard the answer or build a factor).

## 6. THE RE-TAKE 9 PREDICTION, PER CELL

**The on-row identity premise is available for NO cell.** 51 of 51 corpus rows move on the
E2-ON path and 51 of 51 on the E2-OFF path, both algebras; there is no cell whose walk this
change leaves alone, and the standing rule (a re-take's expected-identical set needs a
per-cell measurement, never a carried list) applies with nothing carried.

| TRAIN family | prediction, and what supports it |
|---|---|
| the 5 optimal cells | **status must hold** — and the corpus cannot say so for them: none of the five is in the 51-dump corpus. They need their own measurement, and a status move is a STOP. |
| `STEENBRB` / `Cs_STEENBRB` | **conversion expected.** Four of six `Cs_STEENBRB` cells convert to the Clarabel optimum here and `STEENBRB_def/qp2` converts from a cap. This is the family the round was for. |
| `YORKNET` / `A0_YORKNET` | **conversion expected, E2 ON.** All six restoration QPs convert, with the drift gone. On the E2-OFF path the same family LOSES `qp6` and gains the drift, so a TRAIN cell that routes through an option-off solve may go the other way. |
| `STEENBRC`, `STEENBRD`, `STEENBRE`, `STEENBRG` | `Hj_STEENBRD` qp4 converts (`num -> opt`) and qp1 is lost on the E2-OFF path; the corpus has no `STEENBRC/E/G` rows, so **no prediction**. |
| `ACOPP57`-carrying cells | **regression expected on the border path** (the default algebra): four certified corpus cells go `opt -> cap` there. |

## 7. REGISTERED ITEMS

1. **THE B-1 FAMILY RE-OPENS** (the one item to schedule first). Six `B1Gate` cells and
   `HsSweepCorpus.Corpus` fail with the B-1 signature: `complementarity = 0.177 = mu*lambda^2`
   at a warm-certified point with `primal = 0` and `stationarity ~ 1e-9`, and a warm arm's
   `f` 4.3e−02 off the analytic path where the cold arm is exact. The reading available from
   the numbers is that the proximal walk lands on a working set whose multipliers are
   ~1e+03-sized (a nearly-dependent member), and the regularized row identity
   `A x - mu*lambda = b` then leaves that row slack by `mu*lambda`, which the NLP-level check
   multiplies by `lambda` again. Not diagnosed further.
2. **`ACOPP57_def` on the border path** (§3f): the bordered answer on that face is
   1.25e−07-stationary against the eliminated path's 1.02e−14, and nothing bounds a stall
   that makes 0 adds and 0 drops. Two candidate answers named there.
3. **~25 fixtures pin a pathology this change removes** (`QpAntiCyclingArm` ×6,
   `QpDegeneracyArm` ×4 + 6 diagnostics, `QpE2WorkingSet` ×4, `QpNc2FlatFace` ×3). Their
   re-derivation is a declared event, not a re-pin pass.
4. **The new counters are not aggregated into `SqpCounters`.** `eqp_refine_iters` and
   `eqp_residual_refusals` are QP-level only, so a driver-level reader cannot see a refusal;
   the SQP aggregation is ~15 call sites plus an arity assert and was out of scope here.
5. **U0's terminal `kkt_stationarity` is now `opt_tol`-sized, not machine-sized** (§3d). If a
   consumer depends on the walk grinding past its own tolerance, that is a contract question
   to settle explicitly.
6. **The `O(delta)` cleanup pass** (§3e) is intrinsic to the proximal model at
   `delta = 1e-8, opt_tol = 1e-9`: one extra pass per face whenever `delta*||p|| > opt_tol*scale`.
   It is what `qp_minors` 2 → 3 is, and it is where the degenerate-vertex tie is met.
7. **`kEngineStepTieTol` decides a weakly-active bound on that pass** and the two algebras
   answer differently (§3e). A tie rule stated on the ratio cannot be algebra-independent at
   1e−17; if cross-algebra `bound_state` equality is a contract, it needs a different rule.

## 8. WHAT I COULD NOT DETERMINE

- **Whether the B-1 class is repairable inside the EQP layer or needs the driver's warm
  ingest.** The failing points satisfy stationarity and feasibility; only the
  multiplier/slack product is wrong, which is a statement about WHICH working set the walk
  lands on, not about the solve.
- **Whether `ACOPP57_def`'s bordered answer can be made as accurate as the eliminated one on
  that face.** I measured that the footprint rule is not the cause and did not go further.
- **The driver-side failures' attribution** (~20 tests: `SqpDriverRestorationSeed` ×6,
  `SqpDriverCertifiedFallback` ×4, `SqpDriverElastic` ×3, `SqpEqualityBasis` ×3 and others).
  They were counted, not read.
- **The Debug suite at the final head**, and the per-test Debug wall-clock margin.
- **Whether `A0_YORKNET/qp3`'s 12 remaining `eq_drift_violations`** (from 7502) are the
  ladder's rungs or the ordinary early passes.

## 9. PROCESS, AND WHAT IS ON THE BRANCH

- Worktree `/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9`, branch
  `e4` on `c864b8a6`. The main tree was never touched; its only new file is this report,
  under the git-ignored `.superpowers/`.
- Every solver leg through `.scratch/e4/leg.sh`: `flock /tmp/box-build.lock` with NO timeout
  and never under `timeout`, an in-lock `pgrep -a -x hven_bench_cell` emitting `PGREP_EMPTY`,
  then `FOREGROUND_START` / `FOREGROUND_END <rc>`; `set -o pipefail` throughout;
  `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2` on every solver cell; ONE solver at a
  time; `ctest -j4` under `setsid`. Legs longer than 600 s ran detached with a marker poll,
  never signalled. No process this round did not start was ever signalled.
- **The diagnostic scaffolding is GONE from the tree**: the `E4_OFF`/`E4_DIAG` attribution
  patch (`.scratch/e4/tools/patch_diag.py`, applied and reverted), the temporary
  `test_hs_sweeps.cpp` probe (reverted), and the scratch per-minor harness
  `tests/qp/test_e4_scratch.cpp` with its `tests/CMakeLists.txt` line (removed). `git status`
  is clean at `c46485d8` and both configurations build.
- clang-format run on all 14 changed files. IPARM surface untouched. `notices/` untouched.
  SNOPT firewall: nothing read. No correctness fixture re-pinned; two `EqpSolve` fixtures
  FIXED at unchanged tolerances (§5).
- **THE COMMIT**: `c46485d8`, one commit, message `qp(m7): W2 E4 -- …`, NO attribution
  trailer, explicit pathspecs. Files: `include/hven/core/solver_counters.h`,
  `include/hven/detail/kkt/bordered_eqp.h`, `include/hven/detail/qp/eqp_solve.h`,
  `include/hven/detail/qp/qp_engine.h`, `include/hven/detail/qp/qp_walk_trace.h`,
  `include/hven/qp/qp_types.h`, `src/qp/qp_engine.cpp`, `src/qp/ws_independence.cpp`, and
  six test files' call sites.

## 10. EVIDENCE INDEX (`.scratch/e4/`)

| what | where |
|---|---|
| the corpus, both algebras, E2 on and off | `corpus/e4-*.txt`, `corpus/e4off-*.txt` (base rows `corpus/fix1*-*.txt`, copied) |
| the comparisons | `logs/rowcmp-e2on-v4.txt` (landed), `logs/rowcmp-e2on-v3.txt` (ladder, delta footprint), `logs/rowcmp-e2on.txt` (refusal-as-exit), `logs/rowcmp-e2off.txt` |
| the Release suite | `logs/ctest-rel-e4.log`, `logs/failure-messages.txt` |
| U0 | `logs/u0.log`, `u0/comparison.txt`, `u0/head-{walk,ssn,ipm}.csv` |
| the targeted Debug runs | `logs/t1-*.log`, `logs/t2-*.log` |
| the attribution sweeps (E4_OFF) | `logs/diag1-m*.log` … `logs/diag12-m*.log`, `tools/patch_diag.py` |
| the per-minor probes | `logs/diag11.log` (simple_box_qp, both algebras), `logs/probe4` / `logs/diag6.log` (refusal margins) |
| the final build + key fixtures | `logs/final-build-{debug,release}.log`, `logs/final-quick.log` |
| the runners | `leg.sh`, `build.sh`, `corpus.sh`, `build_tools.sh`, `g4.sh`, `suite1.sh`, `corp2.sh`, `final-build.sh`, `t1.sh`, `t2.sh`, `diag*.sh`, `probe*.sh` |
| the replay tool (copied, `-DNC2_COUNTERS=1`, plus a scratch `refuse=`/`residiters=` column) | `tools/replay/`, `tools-build-replay/` |
| running state | `STATE.md` |
