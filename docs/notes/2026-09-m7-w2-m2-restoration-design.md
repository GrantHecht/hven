# M7 W2 M2 — the restoration resume rule: the design page

**Status: a design page for the settler to rule on. Nothing here is implemented.**
What M2 has landed in code is the S5 excursion record (`9a08a715`, counters only,
replay 27/75/0 on all three engines) and the probe that produced the ladder. The
fix is M2's second task and is dispatched after the ruling.

**Every number on this page comes from
`docs/notes/data/2026-09-m7-w2-m2-restoration/`** (the artifact), whose
`README.md` states the instrument, the two pins and the citation rules, and whose
`PROVENANCE.txt` carries the stamp. Two of those rules are repeated here because
this page would be wrong without them: the ladder's caller-model `f` and `h`
columns are the **probe's own recomputation** at `x.head(n)`, not engine output —
the engine's own numbers are the S5 record and the counters block — and **the
ladder is the M1-shipped engine while the control is the read's**, so every
number below says which.

---

## 1. The mechanism, restated in the ladder's numbers

The A-SQP read established the shape of the failure: when the restoration
sub-solve returns a point whose violation is inside `feas_tol`, the driver takes
it unconditionally —

```cpp
    if (h_r <= opts_.feas_tol) {
        // RESUME. The order here is the header note's order.
        st.x = x_r;
        st.ev = std::move(ev_r);
```

(`src/drivers/sqp_solver.cpp`, the RESUME arm) — with no merit test, no funnel
test and no comparison of `ev_r.f` against `st.ev.f`, because the feasibility
sub-problem has no objective block of the main problem's. The funnel is then
`resume_from_restoration(h_r)`'d — **re-based on the new point rather than
consulted about it** (`src/globalization/sqp/funnel.cpp`: `width_ = (1 − κ)·h_r +
κ·width_`).

The ladder says three further things the read could not.

### 1.1 The objective climbs from the FIRST restoration iteration, not at the end

The read's reading was "the violation falls only at the end". That is right about
the collapse and **incomplete about the trade**. On STEENBRC
(`tables/STEENBRC.md`), the very first restoration iteration trades

| | `f` (caller model) | `h` (caller model) | `dx_inf` | sub-solve radius |
|---|---:|---:|---:|---:|
| requesting iterate | 4 528.3061353626763 | 41 800 | 0 | — |
| restoration iter 1 | **4 610.2320318680231** | **41 786** | 0.13429130195248773 | 1 |

— **81.93 units of objective for fourteen of violation** — and every
iteration after it is worse in the same direction. The violation then falls
gradually to 3 617 by iteration 14 while `f` reaches 7.0384577428087656e+13, and
**collapses to 4.89e-12 in the single last step**, where `f` goes to
1.8345379627133412e+14 and `dx_inf` reaches 9 999.9 — the box.

STEENBRG is the same trajectory to eleven digits (`tables/STEENBRG.md`: iteration
1 at `f = 4 609.9158041020064`, `h = 41 786`, the same `dx_inf`), and STEENBRB is
the same shape over eleven iterations (`tables/STEENBRB.md`: `f` 2 858.79 →
4.0908606864423218e+12, `h` 19 800 → 1.63e-12, `dx_inf` 0 → 1 353.18).

**This is what decides shape (ii)** — see §2.1.

### 1.2 The mechanism is a runaway sub-solve trust region, and the reason is structural

On STEENBRC the depth-1 trust radius **doubles every iteration, 0.5 → 8 192** —
fourteen consecutive doublings, i.e. fourteen consecutive accepted steps — and
`‖x − x_k‖_∞` tracks it: 0.13 → 4.63 → 145.8 → 1 357.6 → 6 060.0 → **9 999.9**
(`tables/STEENBRC.md`). The sub-solve is not misbehaving: a trust-region method
whose model predicts well *should* double its radius. The problem is the model it
is given.

`RestorationModel` (`include/hven/detail/globalization/sqp/restoration.h`) is

```
    min   Σ_i σE_i (sp_i + sm_i) + Σ_j σI_j si_j
    s.t.  cE(x) + σE.*(sp − sm) = 0,  cI(x) − σI.*si ≤ 0,  l ≤ x ≤ u, sp,sm,si ≥ 0
```

and its own header states the consequence in one line: *"CONSTANT gradient: the
objective is LINEAR with no x-block at all. That zero x-block is what makes a
stationary point of this problem a stationary point of h rather than of some
trade-off against f."* **Nothing in the problem the sub-solve is given penalises
distance from the requesting iterate**, and nothing bounds the excursion either:
the sub-solve's ceiling is `ropts.tr_max = max(opts_.tr_max, ropts.tr_init)` with
`tr_max = 1e10` by default, so the only thing that stopped STEENBRC's radius at
8 192 was the variable box.

That is the argument for shape (i) written in the ladder's own numbers: the
missing term is exactly the one the IPM lane already carries.

### 1.3 The other four cells, and what each one is for

| cell | what the phase does | `entries` | `dx_inf` reached | outcome |
|---|---|---:|---:|---|
| ZAMB2-8 | **lowers the objective and converges** | 1 | 11.99 | `optimal` |
| YORKNET | **moves nothing at all**, 18 iterations | 1 | **0 at every iteration** | `infeasible` |
| BRITGAS | one improving step, then stalls | 1 | 0.031 | `numerical_error` |
| READING6 | **never restores** | **0** | — | `optimal` |

* **ZAMB2-8 is the counter-example any refusing shape fails** (`tables/ZAMB2-8.md`).
  Its restoration moves the caller's objective **down** — S5 reads `f` 0.018068535257036302
  → **−0.069042214516419051** while `h` goes 24.643618798749973 → 1.209356876794021e-11 —
  and the solve then converges to `optimal` at `f = −0.15293592733713887`
  (`ladder/ZAMB2-8.txt`). Its ladder is also qualitatively different from
  STEENBRC's: the radius does *not* run away (0.5 → 8, with two shrinks at
  iterations 5 and 7), and the caller's objective stays within 0.226 of its
  entry value throughout rather than climbing. A rule that refuses this phase refuses one that helped.
* **YORKNET's phase is measured going nowhere** (`tables/YORKNET.md`). `h_in ==
  h_out == 48 491.383947016526` and `f_in == f_out == 110 670.29950761795` to the
  last digit, and **`dx_inf` is 0 at every one of its eighteen restoration
  iterations** while the sub-solve's radius halves monotonically 1 → 7.63e-06.
  The read inferred this from unchanged residuals; it is now a column. It is
  M5's cell, and §5 says why no M2 shape shortens it.
* **BRITGAS is in this set only because M1 shipped `qp_failure_budget = 3`.** At
  the read's pin it never reached restoration; at the shipped default it does,
  for seven iterations, moving `h` 115.03999999999996 → 110.10740647336283 and `f`
  0 → 0.075494336118951622, with one improving step at iteration 3 and four more
  at a halving radius that change nothing (`tables/BRITGAS.md`). The excursion is
  **0.03125** — five orders of magnitude smaller than STEENBRC's. The phase
  neither ran away nor helped, and the cell ends `numerical_error`.
* **READING6 reports `restoration_entries = 0`** (`tables/summary.md`). At the
  M1-shipped default it never restores and lands `optimal` at
  −144.65970057279054. It is a control for "nothing moved", **not** for
  "restoration behaved", and must not be described as a restoration cell.

### 1.4 Two corrections to the read, both from the ladder

1. **STEENBRB has no major before its restoration phase.** `ladder/STEENBRB.txt`
   records `x_k_source=start_point (the phase was entered in major 0)`, and the
   requesting iterate is therefore the start point at `f = 2 858.7861353626763`,
   `h = 19 800`. The read's "at the last major before the phase, `f = 4 547.15`"
   row is in fact the phase's **first restoration iterate** — the ladder carries
   exactly `4 547.1515977177878` at depth 1, iteration 1. This is a naming
   correction, not a numeric disagreement; no value in the read moves.
2. **M1 moved STEENBRC's cost, and the two engines must not be mixed.** At the
   read's pin the cell costs **32 137** factorizations (`control.txt`, identical
   to H3's `run-A` at full precision, 16/16 columns); at the M1-shipped default it
   costs **42 946** (`ladder/STEENBRC.txt`). The control reproduces the read's
   published table exactly — 24 cells compared, 0 differing — so this is M1's
   declared effect on the cell and not a reproduction failure.

---

## 2. Ruling on the three shapes, from the numbers

The settler's ranking is (i) proximity term > (ii) acceptability-based return >
(iii) reporting, and the page may overturn it with evidence. **The ladder confirms
it, and sharpens the reason.**

### 2.1 (ii) alone cannot bite on STEENBRC — there is nothing for it to return

An acceptability-based return resumes at *the first restoration iterate the
strategy would accept as a trial step from the requesting iterate*. Read
STEENBRC's ladder as a list of candidates for that test (`tables/STEENBRC.md`):

* every depth-1 iterate **raises** `f` above the requesting iterate's 4 528.31 —
  from the first one (4 610.23) to the last (1.8345e+14);
* only the **last** one has `h` inside `feas_tol` (4.89e-12; the second-best is
  3 617);
* so no iterate on the ladder both reduces the violation to admissibility and
  offers the objective anything, and a funnel judging `(f, h)` pairs would accept
  none of them as a step from `x_k`.

**Shape (ii) therefore has nothing to return on the cells that need help.** What
it does instead is refuse the resume it makes today, which leaves the phase
falling through to its terminal arm — i.e. it converts M2's failure into shape
(iii)'s reporting question, which the read already judged: *"neither point is an
answer, and a benchmark that scored one over the other would be scoring the
reporting rule."*

This is not an argument that (ii) is wrong. It is TRAIN-safe where it matters —
on ZAMB2-8 the restored point has `f` 0.0181 → −0.0690 at `h` ≈ 1.2e-11, which
passes any sane acceptability test, so ZAMB2-8 stays — and it is a real guard
against a future cell whose ladder *does* contain a good iterate. **It is
toothless on the three cells that motivated M2**, and that is the finding.

### 2.2 (iii) is reporting, and stays registered

Best-iterate reporting on the QP-failure exit changes what is printed, not what
happens: it would have STEENBRB report `f = 4 547` at a violation of ~2e3 instead
of 4.09e12 at 2e-13. The read's own verdict stands — neither point is an answer.
The ladder adds one fact in its favour and one against: in favour, S5 now makes
the excursion *visible* without changing the return (`restoration_f_in`/`f_out`
are on every solve's counters); against, on STEENBRB the "best" iterate by any
funnel ordering is the requesting iterate itself, which is the point the solve was
already stuck at. **Registered, not M2's fix** — as the settler ruled.

### 2.3 (i) is the missing term, and the ladder is the argument for it

Shapes (ii) and (iii) both *select among the iterates the phase generates*.
STEENBRC's ladder contains no acceptable iterate to select. **Only (i) changes
which iterates are generated**, and it does so by supplying precisely the
information the feasibility problem is missing — that `x` should stay near `x_k`
— which is what `dx_inf` 0.13 → 9 999.9 against a radius 0.5 → 8 192 measures.

---

## 3. The proximity term, spelled exactly

### 3.1 The precedent, as the IPM lane carries it

`include/hven/detail/globalization/proximal_restoration.h`,
`ProximalSwitchRestoration`:

```
    ζ  = kRestoProximityWeight * sqrt(mu),  frozen ONCE at entry
    d_i = min(1, 1/|x_R_i|)
    P(x)      = (ζ/2) Σ_i d_i² (x_i − x_R_i)²
    dP/dx_i   = ζ d_i² (x_i − x_R_i)
    proximal_diagonal()  = ζ d_i²      (the primal-diagonal Hessian block of P)
```

with `inline constexpr double kRestoProximityWeight = 1.0;` — *"the reference
implementation's `resto_proximity_weight` shipped default 1.0"*. `x_R` is the
primal snapshot at the moment restoration was entered, `ζ` is frozen at entry and
not re-derived while restoration remains active, and `d` is per-coordinate: a
coordinate with `|x_R_i| < 1` is unscaled, one with `|x_R_i| > 1` shrinks in
proportion to the centre's magnitude so the proximal curvature stays bounded near
large centres, and `|x_R_i| == 0` lands in the first branch without a division.

### 3.2 The SQP transcription

The SQP restoration phase's centre is the **requesting iterate** `x_k` — the same
point S5's entry half is taken at, and already in hand at
`enter_restoration`'s `resto_ev`/`st.ev`. The term is the IPM lane's, verbatim in
form:

```
    D      = diag(d_i),  d_i = min(1, 1/|x_{k,i}|)          (i over the ORIGINAL n only)
    P(x)   = ½ ζ ‖D (x − x_k)‖²
    ∇P(x)  = ζ D² (x − x_k)                                  (x-block; zero on every slack)
    ∇²P    = ζ D²                                            (a diagonal, x-block only)
```

and the feasibility wrapper's objective becomes

```
    min   Σ_i σE_i (sp_i + sm_i) + Σ_j σI_j si_j  +  ½ ζ ‖D(x − x_k)‖²
```

over the same augmented variables `(x, sp, sm, si)`, with the same constraints,
the same σ scaling and the same box. `D` and `ζ` are computed **once**, at
construction, from data already in hand — no model query, `O(n)`.

**Where it enters the sub-solve's QP.** `RestorationModel::eval_grad` and
`::eval_hess` are what `build_subproblem` reads, so the subproblem's objective
`gᵀd + ½dᵀHd` gains `∇P` in `g` (x-block) and `ζ D²` on `H`'s x-block diagonal.
Two consequences must be stated where the code states their opposites today:

* `eval_grad`'s contract note — *"CONSTANT gradient: the objective is LINEAR with
  no x-block at all"* — is no longer true at `ζ > 0` and must be rewritten with
  the new term and with what it costs the stationarity claim (§3.4).
* `eval_hess` currently **ignores its `obj_scale` argument by name**, and the
  reason it gives is exact: *"hess = obj_scale*hess(f) + Σ lambda*hess(c), and
  hess(f) is IDENTICALLY ZERO for a linear objective."* With `P` present
  `hess(f) = ζD²` is not zero, the parameter must be named again, and the
  wrapper's stated *"no new nonzeros"* property goes with it: the augmented
  Hessian gains up to `n` diagonal entries. Those are cheap to add (the
  implementation already builds `H` from triplets, and `setFromTriplets` sums
  duplicates) but they change the sub-solve's **KKT sparsity pattern**, hence its
  symbolic analysis — a work effect that must be measured, not assumed away.

### 3.3 How it interacts with the sub-solve's trust region

The term acts **inside the QP**, not only at the acceptance test. A positive
diagonal on `H`'s x-block means the sub-QP's own minimiser stops running to the
trust-region boundary once the penalty outweighs the linear gain — which is
exactly the fourteen-consecutive-doubling pattern the ladder measures. The trust
region is therefore left alone: no new cap on `ropts.tr_max`, no change to
`restoration_restart_radius()`, no second tuning surface. §11's question 5 asks
whether the settler wants the alternative (an explicit excursion cap on the
sub-solve) registered as a fallback; this page does not recommend it, because a
cap is a hard constant where the term is a price.

### 3.4 What it costs the infeasibility certificate — the load-bearing consequence

**This is the one place shape (i) is not a free transcription of the IPM lane's
term, and it must be ruled on rather than discovered during implementation.**

Today's certified-infeasibility exit rests on the zero x-block, twice:

1. In the driver, the bound price is taken from the restoration problem and not
   from `kkt_r`, with the reason stated at the site: *"The sub-solve's own z is
   the price of the SAME box in the feasibility problem, whose objective has no
   x-block, so its x-part is precisely the normal-cone component of
   `Je^T λ_e + Ji^T λ_i`."* At `ζ > 0` the sub-solve's `z` carries `∇P(x) = ζD²(x
   − x_k)` as well, so the substituted `st.resto.kkt.z` is **polluted by exactly
   that vector**.
2. In the suite, `check_certificate` (`tests/sqp/test_sqp_restoration.cpp`)
   re-derives `stationarity = ‖Jeᵀλ_e + Jiᵀλ_i − z‖∞` from the model, with *"NO
   grad f term — that absence is what makes this a certificate of INFEASIBILITY
   rather than of optimality"*. The same pollution lands in that residual.

The correction is exact, closed-form and `O(n)`: `∇P` is known without a model
query, so `z_certificate = z_subsolve − ζD²(x − x_k)` restores precisely today's
quantity. But a second, subtler point does **not** go away with it: at `ζ > 0` the
sub-solve's `kOptimal` certifies stationarity of `h + P`, **not** of `h`, and the
driver's certified arm reads that status directly. So the rule must be:

> at `ζ > 0`, `infeasibility_certified` is set **only if** the corrected residual
> `‖Jeᵀλ_e + Jiᵀλ_i − z_corrected‖∞ ≤ kkt_tol` — an explicit, cheap re-check —
> and never on `rs.status == kOptimal` alone.

That makes the certificate at `ζ > 0` **at least as strong** as today's, at the
cost of one `O(n)` test. The alternative — declaring the certificate unavailable
whenever `ζ > 0` — is cheaper to implement and strictly weaker, and it would
retire a capability M6 built. Question 1 in §11.

### 3.5 What this artifact cannot settle: ζ's value

The IPM lane's `ζ = kRestoProximityWeight · √μ` keys the weight to the barrier's
own measure of how far from converged the solve is. **The SQP lane has no μ**, so
the transcription needs an analogue, and the settler's ruling constrains the
choice: the weight the IPM lane already carries, keyed to the requesting iterate,
**no new free constant**. Dimensionally, the feasibility objective is in
violation units and `‖D(x − x_k)‖²` is a squared *relative* displacement, so `ζ`
must carry violation units, and the one violation quantity in hand at entry is
`h_k` — which is also exactly the feasibility objective's own value at the start
point, and exactly S5's `restoration_h_in`. **The ladder confirms that identity
rather than assuming it**: the wrapper's own objective on each phase's first
depth-1 row is `f_event = 41 800` on STEENBRC and `24.643618798749969` on ZAMB2-8
(`tables/STEENBRC.md`, `tables/ZAMB2-8.md`), against `h` of 41 800 and
24.643618798749976 at the same points. That gives the natural candidate

```
    ζ = kRestoProximityWeight · h_k          (= 1.0 · restoration_h_in)
```

whose reading is legible: *the phase will not pay more than its entire entry
violation to move the iterate by one unit of its own magnitude.*

**The ladder cannot confirm that this value is right, and saying so is the
honest reading of the artifact.** The probe carries `dx_inf = ‖x − x_k‖_∞`, not
`‖D(x − x_k)‖²`, and the two differ by exactly the per-coordinate magnitudes of
`x_k`, which this artifact does not carry. Worked at the two extremes on ZAMB2-8
— the cell the TRAIN bar protects — the candidate lands on a knife edge: the
phase's total available gain is `h_k = 24.64` and its returned point sits at
`dx_inf = 11.99`; if ZAMB2-8's `x_k` coordinates are `O(1)` then `d = 1`, the
relative excursion is `≳ 144`, the penalty is `≳ 1 774` and **the phase that
helped would be blocked**; if they are `O(12)` the relative excursion is `≈ 1`,
the penalty is `≈ 12`, and it is allowed.

The same uncertainty is far less material at the two ends of the range, and the
reason is that the penalty is QUADRATIC in the excursion while the gain is not.
STEENBRC's fatal last step buys 3 617 of violation at `dx_inf = 9 999.9`, against
ZAMB2-8's 24.64 at 11.99: **834 times the displacement for 147 times the gain**,
so a weight calibrated to leave ZAMB2-8's step admissible refuses STEENBRC's by
roughly `834²/147 ≈ 4.7e3` — modulo the per-coordinate scaling `D`, which differs
between the two cells and which the calibration leg is there to measure. BRITGAS
sits at the other end, with a whole excursion of 0.031.

**Therefore M2's second task opens with a calibration leg, and its decision rule
is pre-registered before the leg runs**, on the precedent of A4's own rule
(`hven-bench/docs/w2-a4-decision-rule.md`, landed before any TRAIN run):

* **the measurement**: one added probe column, `prox_rel = ‖D(x − x_k)‖²` with `D`
  built from the requesting iterate, per depth-1 row — a diagnostic in the
  probe's own wrapper, no engine change, the same seven cells, seven legs, one
  per lock hold;
* **the candidate set, fixed in advance**: `ζ ∈ {h_k, h_k/n, √(h_k), h_k/‖D(x_box)‖²}`
  — every member keyed to the requesting iterate, every one reusing
  `kRestoProximityWeight = 1.0` as its coefficient, no new constant;
* **the rule**: choose the **smallest** member that (a) makes STEENBRB, STEENBRC
  and STEENBRG's measured final step inadmissible — `(ζ/2)·prox_rel` at that step
  exceeds the violation it buys — while (b) leaving ZAMB2-8's returned point
  admissible by the same arithmetic, and (c) leaving BRITGAS's 0.031-excursion
  step admissible. If no member satisfies all three, **the shape is reported as
  not calibratable without a new constant and returns to the settler** rather
  than one being introduced quietly.

---

## 4. The toggle, its exact spelling, and its OFF proof

```cpp
/// How a restoration phase decides to hand its point back to the main loop.
enum class RestorationResumeRule {
    kUnconditional,   ///< today: resume iff h_r <= feas_tol, whatever f_r is
    kJudged,          ///< shape (ii): resume at the first restoration iterate the
                      ///< strategy would accept as a trial step from the requesting iterate
    kRestorationResumeRuleCount,  ///< NOT A RULE — the enumerator count
};

struct SqpOptions {
    // ...
    RestorationResumeRule restoration_resume_rule = RestorationResumeRule::kUnconditional;
    /// The proximity weight's coefficient on the feasibility objective's
    /// 1/2 zeta ||D (x - x_k)||^2 term. 0 == OFF: no term, no gradient, no
    /// diagonal, and the wrapper is the one that ships today.
    double restoration_proximity_weight = 0.0;
};
```

* **The enum's arms follow A0 §2.1's M2 row** (`kUnconditional` = today, `kJudged`),
  and the count sentinel is refused by name in `validate()` on
  `QpMode::kQpModeCount`'s precedent. A0's third arm `kObjectiveBounded` is
  **dropped** on this page's evidence: it is a cap on the objective increase, i.e.
  precisely the tuning constant the read objected to, and §2.1 shows the
  acceptability test it would approximate has nothing to bite on.
* **The proximity weight is a field of its own, not an arm of the enum**, because
  the two are orthogonal: the term changes what the phase *generates*, the rule
  changes what it *returns*. The settler's ruling permits the second field only
  if shape (i) survives the page; it does (§2.3), and `0.0` is its OFF state.
* **OFF is provable by the A0 contract, at source, on both fields.**
  `restoration_resume_rule == kUnconditional` leaves `if (h_r <= opts_.feas_tol)`
  the only test on the resume path — byte-identical. `restoration_proximity_weight
  == 0.0` makes `ζ = 0`, so `P ≡ 0`, `∇P ≡ 0` and `∇²P ≡ 0`: `eval_f`, `eval_grad`
  and `eval_hess` return exactly today's values and **today's sparsity pattern**,
  provided the implementation *branches* on `ζ == 0` rather than adding zeros —
  a zero triplet would still enter the pattern and move the symbolic analysis.
  The OFF proof is therefore a code shape, not only an arithmetic identity, and
  §8's test 2 pins it.
* **Validation**: `!(w >= 0.0)` refuses a negative or NaN weight (the `!(>=)` form
  catches NaN, as the funnel's own guard does), and the count sentinel is refused
  by name.
* **Fired counters** (A0 clause 4): S5 is already in the tree and is the ON
  reading. A `restoration_resume_refused` count is owed under `kJudged` — the
  arm's only observable — and is **not** owed by the weight, whose effect is
  visible in S5's four absolutes.

**The options-side cost is fixed and knowable, and is read off the build rather
than predicted**: `+2` fields, `+2` `dump_options()` lines in declaration
position, and the three drift pins in `tests/drivers/test_options_dump.cpp` move
together — `kSqpOptionsFields` (**30** today), `kSqpDumpLines` (**61** today) and
`sizeof(SqpOptions)` (**400** today; M1 moved it 384 → 400, sixteen bytes for one
`Index`, because of an alignment hole — so the new value is whatever the compiler
says and is never arithmetic on this page). Harness-side, every retained row's
`options_digest` moves: a declared re-derivation on the bench lane, not a gate
here.

---

## 5. Expected effect, per cell, argued from the numbers

| cell | under (i) `ζ > 0` | under (ii) `kJudged` | under (iii) reporting |
|---|---|---|---|
| **STEENBRC** | the runaway is priced: the last step buys 3 617 of violation for an excursion of 9 999.9, which a `ζ` calibrated to leave ZAMB2-8 admissible refuses by some three orders of magnitude (§3.5). **The trajectory changes**, so the outcome is an empirical question, not a prediction | **no effect available**: no iterate on the ladder is acceptable (§2.1); the resume is refused and the cell falls to the terminal arm | reports the requesting iterate — `f` 4 528.31 at `h` 41 800 — instead of 1.83e14 at 4.9e-12. Neither is an answer |
| **STEENBRB** | as STEENBRC (`dx_inf` 1 353.18, `f` → 4.09e12) | as STEENBRC | as STEENBRC |
| **STEENBRG** | as STEENBRC, to eleven digits | as STEENBRC | as STEENBRC |
| **ZAMB2-8** | **at risk, and the bar** — §3.5's calibration exists to decide exactly this. Its returned point is at `dx_inf` 11.99 having bought 24.64 of violation *and* 0.087 of objective | **unmoved**: the restored point lowers `f` (0.0181 → −0.0690) at `h` ≈ 1.2e-11 and passes any acceptability test | unmoved: the returned point *is* the best iterate |
| **YORKNET** | **inert**: `dx_inf = 0` at all 18 iterations, so `P ≡ 0` and `∇P ≡ 0` along the whole phase. The added `ζD²` diagonal makes a zero x-step *more* attractive, not less | **inert**: no iterate differs from the requesting one, so none is an improvement and none is acceptable | inert: every iterate reports the same `f` and the same four residuals |
| **BRITGAS** | **very likely inert** — the whole excursion is 0.031 against STEENBRC's 9 999.9, and the penalty is quadratic in it, so a weight that bounds the latter is many orders of magnitude from touching the former. Measured by §3.5's leg rather than assumed | inert: the phase exits on the **terminal** arm (`h_out` 110.107 > `feas_tol`), which the resume rule does not govern | would report `f = 0` at `h = 115.04` instead of 0.0755 at 110.107 |
| **READING6** | **provably inert**: `restoration_entries = 0`, the phase never runs, no wrapper is constructed | provably inert, same reason | provably inert, same reason |

Two of those rows are the page's substantive findings.

**YORKNET is not M2's to fix, and the ladder proves it.** `dx_inf = 0` at every
one of eighteen iterations, with the sub-solve's radius halving 1 → 7.63e-06 and
`h_in == h_out` to the last digit, says the phase generates no motion at all —
so a term that prices motion changes nothing, and a rule that judges motion has
nothing to judge. The read said it first from the other side: *"Where STEENBRC's
restoration went somewhere bad, YORKNET's went nowhere at all — and the two
failures are opposite, which is why one fix will not serve both."* **What would
shorten YORKNET's ladder is a fourth shape that is on nobody's list: a
phase-level stall exit** — end the phase when `h` has not moved in `k`
iterations. That is worth registering (question 4), and it belongs with M5/M3,
which own the price: 18 iterations that moved nothing cost ~2 200 factorizations
each (`ladder/YORKNET.txt`: 52 180 factorizations, 50 241 minors, 19 elastic
activations).

**BRITGAS is a new cell and a new question.** It exists in this set only because
M1 shipped budget 3, its phase neither runs away nor helps, and no shape on the
list improves it. It belongs to M1's own follow-up rather than to M2.

---

## 6. The TRAIN bar

The bar the brief and A0 agree on, restated with what this page now knows:

| item | bar | this page's reading |
|---|---|---|
| STEENBRB | → `optimal` at **9 075.855**, where the IPM, IPOPT and SNOPT lanes all land | **not predictable from the ladder, and must be measured.** No iterate on STEENBRB's ladder is both admissible and sane, so reaching the bar requires the phase to generate *different* iterates — which only (i) does. It is the bar, not a forecast |
| ZAMB2-8 | **unmoved** (`optimal` at `f = −0.15293592733713887`) | the binding constraint on `ζ`'s calibration (§3.5) |
| READING6 | **unmoved** (`optimal` at −144.65970057279054) | provably unmoved: `restoration_entries = 0`. READING6 is M1's converted control, so a shape that moves it is a shape that broke what M1 fixed |
| YORKNET | ladder shortened; its `kInfeasible` still uncertified until M5 | **proposed amendment**: no M2 shape shortens it (§5), and the bar as written cannot be met by this task. It should read *"YORKNET unmoved, and its ladder registered to M5"* |
| STEENBRC, STEENBRG | not named in the bar | they move with STEENBRB or the fix is not the fix |
| BRITGAS | not named in the bar | should be added as *unmoved*, on READING6's logic: it is M1's newest restoration cell and a shape that changes it is changing something M1 shipped |

---

## 7. Pins at risk, and the declared re-derivation plan

| pin | at risk under | plan |
|---|---|---|
| the three replay CSVs `head-{walk,ssn,ipm}.csv` (`docs/notes/data/2026-09-m6-close-gate/replay/`, `scripts/compare_replay.py`, the 27 cells at `docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt`) | any shape, if a default moves; **nothing**, at the declared OFF | **checked, not assumed.** M1's precedent (a re-derivation budgeted and not needed, because the 27 replay cells never route a subproblem failure) does **not** transfer for free: M2's shapes change what happens at a *restoration*. S5 makes the check a one-column question it was not before this task — run the 27 cells and read `restoration_entries`; a set that never restores is unmoved by construction, and a set that does gets a declared re-derivation |
| `tests/sqp/test_sqp_restoration.cpp`, **including `check_certificate`'s own re-derivation** | (i), via §3.4's `z` pollution | the correction is exact and closed-form; the fixture pins both halves (§8, tests 3) |
| the four `SqpDriverRestorationExcursion` fixtures' `h_out <= feas_tol` assertions | **(ii) directly** — they are statements about the RESUME RULE; **(i) indirectly** — the models' trajectories move at `ζ > 0`, so the `EXPECT_DOUBLE_EQ`s against history rows and `log.resumes.size() == 1` move with them | at the declared OFF they are unmoved and are the OFF proof. Under an ON default they are re-derived **once**, in the commit that moves the default, with the new values stated |
| `HsSweepCorpus.Corpus` | any default move | M1's own precedent for how to declare such a move |
| the counter pins — `kSqpCountersFieldCount` (**49**), `sizeof(SqpCounters)` (**848**), the `44*8` → `49*8` mask arithmetic in `tests/drivers/test_threads.cpp`, both golden lines (**106** counters), `docs/counters.md`, `docs/trace-schema-v0.md` §5/§8 | **only if `restoration_resume_refused` lands**, i.e. only under `kJudged` | one declared re-derivation, exactly as `9a08a715` did it for the five S5 fields |
| the options pins — `kSqpOptionsFields` **30**, `kSqpDumpLines` **61**, `sizeof(SqpOptions)` **400** | both fields, unconditionally | they move together in one commit, with the size read off the build |
| the harness's `options_digest` on every retained bench row | both fields | a declared re-derivation on the bench lane; **not a gate here** |

---

## 8. The test plan

1. **The term, in isolation** — `P`, `∇P` and the diagonal against hand-computed
   values, with `d_i` exercised at `|x_{k,i}| = 0`, `< 1` and `> 1` (the IPM
   lane's three branches), and at `ζ = 0`.
2. **The OFF proof in code, not only in arithmetic** — at `ζ = 0`, `eval_f`,
   `eval_grad` and `eval_hess` byte-identical to today's **and the Hessian's
   nonzero pattern unchanged** (`H.nonZeros()` and the inner index array
   compared), which is what pins the branch-rather-than-add shape §4 requires.
3. **The certificate, both halves** — on `InfeasibleCircleLineModel` at `ζ > 0`:
   (a) the corrected `z` restores `check_certificate`'s residual to today's
   value; (b) the **uncorrected** one does not — the mutant that would ship a
   polluted certificate dies here; (c) `infeasibility_certified` is refused when
   the corrected residual exceeds `kkt_tol` even though the sub-solve returned
   `kOptimal` (§3.4's rule).
4. **The runaway, priced** — `RunawayValleyModel` (already in the suite) at
   `ζ = 0` and `ζ > 0`, asserting S5's `restoration_f_out` bounded at the second.
   This is the unit-scale analogue of STEENBRC and is the fixture the fix is
   test-driven from.
5. **The resume rule's own arm** — under `kJudged`, a fixture where the phase
   returns a feasible point the strategy refuses, pinning
   `restoration_resume_refused` and the exit that follows.
6. **The four excursion fixtures re-run at the declared OFF**: unmoved, verbatim.
7. **The gates of this task, repeated**: Debug + Release suites, the U0 27-cell
   three-arm replay (27/75/0 at OFF), P-SYM classified in both configs against the
   fix's own parent with the task's own header list, install smoke, clang-format,
   `git diff --check`.
8. **TRAIN**: the seven cells of this artifact re-run on the same probe, the
   ladder re-taken, and §6's bar read off it — with the calibration leg (§3.5)
   *first* and its decision rule pre-registered before it runs.

---

## 9. The cost estimate

| piece | size |
|---|---|
| the term in `RestorationModel` (header contract text, ctor, `eval_f`/`eval_grad`/`eval_hess`, the `ζ == 0` branch) | ~80 lines of code, ~60 of contract text, one header + one TU |
| the certificate correction and its gate in `enter_restoration` | ~30 lines and a long comment at the substitution site |
| the two `SqpOptions` fields, `validate()`, `dump_options()`, the three drift pins | ~40 lines, mechanical |
| `kJudged`'s arm and `restoration_resume_refused` | ~50 lines plus one declared counter re-derivation (five pins, as `9a08a715` did) |
| tests 1–6 | ~300 lines in `tests/sqp/test_sqp_restoration.cpp` and the wrapper's own suite |
| the calibration leg (§3.5) | one probe column, seven legs; hours, not days, and no engine change |
| the gates | this task's six segments, plus the replay check |
| **shape** | **two tasks**: calibration + the term at a declared OFF (one), then the default move with its declared re-derivations (the second, only if TRAIN clears the bar) |

The split matters: **landing the term OFF costs no pin outside the options
triple**, so it can be reviewed and merged on its own evidence, and the
default move — the part that moves the replay, the corpus and the bench's
digests — is a separate, separately-declared event.

---

## 10. Recommendation

**Adopt shape (i), the proximity term, as M2's fix; keep shape (ii) as a
non-default arm of the same toggle; leave shape (iii) registered and unbuilt.**
The ladder settles the ranking rather than merely supporting it: on STEENBRC the
objective climbs from the *first* restoration iteration (4 528.31 → 4 610.23 for
41 800 → 41 786) and never turns, so there is no iterate for an
acceptability-based return to select — shape (ii) is TRAIN-safe but toothless
where M2 hurts — while the sub-solve's radius doubles fourteen times, 0.5 → 8 192,
and `‖x − x_k‖_∞` runs 0.13 → 9 999.9 into the box, because the feasibility
objective has no x-block and nothing in the problem the sub-solve is given prices
distance from the requesting iterate. That missing price is exactly the term the
IPM lane already carries, so (i) is a transcription rather than an invention, and
it is the only shape that changes which iterates the phase generates. Two things
must be ruled on before it is written: the **certificate** loses its zero-x-block
argument at `ζ > 0` and needs the exact `O(n)` correction plus an explicit
residual re-check (§3.4), and **`ζ`'s value cannot be settled from this artifact**
— the probe measured `‖x − x_k‖_∞`, not the scaled norm the weight multiplies, and
on ZAMB2-8, the cell the TRAIN bar protects, the natural candidate `ζ = h_k` sits
on a knife edge between "inert" and "blocks the phase that helped" — so M2's
second task opens with a calibration leg whose candidate set and decision rule are
pre-registered before it runs, on A4's own precedent. Land the term at a declared
OFF first (it moves no pin outside the options triple), and make the default move
a separate declared event with its own re-derivations.

---

## 11. Open questions for the settler

1. **The certificate at `ζ > 0`.** Take the exact `O(n)` correction
   `z_corrected = z_subsolve − ζD²(x − x_k)` **plus** the explicit residual
   re-check before `infeasibility_certified` is set (§3.4) — stronger, ~30 lines
   — or declare the certificate unavailable whenever `ζ > 0`, which is cheaper
   and retires a capability M6 built? This page recommends the former.
2. **`ζ`'s calibration.** Is the pre-registered rule of §3.5 — the candidate set
   `{h_k, h_k/n, √(h_k), h_k/‖D(x_box)‖²}`, the smallest member that bounds
   STEENBRB/C/G while leaving ZAMB2-8 and BRITGAS admissible, and a **return to
   the settler** if none qualifies rather than a new constant — the rule the
   settler wants, and is it to be committed before the leg runs?
3. **The dropped enum arm.** A0 §2.1 names `kObjectiveBounded` as M2's third arm.
   This page drops it (§4) as the tuning constant the read objected to and as an
   approximation of a test with nothing to bite on. Confirm, or keep it named.
4. **YORKNET's bar, and a fourth shape.** The ladder shows `dx_inf = 0` at all 18
   restoration iterations, so no M2 shape shortens YORKNET. Amend the TRAIN bar
   to "unmoved, registered to M5" (§6), and register the **phase-level stall
   exit** — end the phase when `h` has not moved in `k` iterations — as a
   candidate owned by M5/M3 rather than M2?
5. **The trust-region alternative.** §3.3 declines an explicit excursion cap on
   the sub-solve (`ropts.tr_max`, or a `‖x − x_k‖ ≤ Δ_resto` constraint) in favour
   of the price. Register the cap as a fallback for the case where the
   calibration leg finds no admissible `ζ`, or leave it out entirely?
6. **BRITGAS's membership.** It is in this cell set only because M1 shipped
   budget 3, its phase neither runs away nor helps, and no shape on the list
   improves it (§5). Add it to the bar as *unmoved* and hand the cell itself to
   M1's follow-up?
7. **The two-task split.** §9 proposes landing the term at a declared OFF as one
   task and the default move as a second, separately-declared event. Confirm the
   split, and confirm that the replay check — run the 27 cells and read
   `restoration_entries`, S5's new one-column answer — is the evidence the
   default-move task must carry.
