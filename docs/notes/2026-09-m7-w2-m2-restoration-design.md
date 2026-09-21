# M7 W2 M2 — the restoration resume rule: the design page

**Status: a design page for the settler to rule on. Nothing here is implemented.**
**Revision 2 (fix round 1, 2026-09-18)**: the review of this page's first draft
(`Fable`, verdict *code + artifact SHIP, page REVISE*) found three load-bearing
arguments — §2.1's acceptability argument, §3.4's certificate correction and
§3.1/§3.5's weight precedent — that do not survive the code they cite, plus a
set of numeric slips. Each is re-argued below **against the code, read first**,
and every retraction is marked as one. §11 now carries **four settled rulings**
(questions 4–7) and **three corrected analyses with one recommendation each**
(questions 1–3), which are what the settler rules on.

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

**This is not what decides shape (ii)** — §2.1's first draft thought it was, and
review F2 retired that argument: hven's funnel never reads `f` on the arm a
restoration iterate reaches. What the climb does establish is the SIZE of the
damage the unconditional resume accepts, and it is the argument for shape (i).

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
`tr_max = 1e10` by default, so nothing in the sub-solve's own configuration was
going to stop the doubling.

**Correction (review F4): what ended the ladder was `h`, not the box.** Row 15 of
`ladder/STEENBRC.txt` is a REJECTION — the radius halves 8 192 → 4 096 at
unchanged `f_event`, `f_main`, `h_main` and `dx_inf` — and row 16 is the accepted
step that collapses `h` to 4.8902304088678294e-12, inside `feas_tol`, which is
what ends the phase. The separate and also-true fact is that row 16's
`dx_inf = 9 999.9000000000015` **is** the variable box: the excursion ran into the
box, but the box never capped the radius.

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
  0 → 0.075494336118951622 (`tables/BRITGAS.md`). **Corrected count (review F4):
  three after, two before, seven rows.** `ladder/BRITGAS.txt` carries seven
  depth-1 rows and the radius column reads them — **two rejections before** the
  improving step (0.125 → 0.0625 → 0.03125 at unchanged `f_event` and
  `dx_inf = 0`), the improving step at iteration 3 (the radius doubles back to
  0.0625, `f_event` falls 115.03999999999999 → 110.0603170765435), and **three
  rejections after** it at a halving radius (0.03125 → 0.015625 → 0.0078125) that
  change nothing. The first draft said "four more"; it is three. The excursion is
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
   correction, not a numeric disagreement.

   **And it is a CROSS-ENGINE correction, which is why it needs its own argument**
   (review F4). M1 moved STEENBRB too: the read's row is 3 majors / 11 277 minors
   / **16 582** factorizations at the read's pin
   (`docs/notes/2026-09-m7-w2-sqp-cutest-read.md` lines 169 and 479, reproduced at
   full precision in
   `docs/notes/data/2026-09-m7-w2-m4-start-point/cells-off-r1.csv:16`), while this
   ladder's engine costs **25 973** (`ladder/STEENBRB.txt`, identical to
   `docs/notes/data/2026-09-m7-w2-m1-qp-cap/cells-budget-r1.csv:16`). A naming
   correction taken off one engine and applied to a table printed from another is
   valid only if the thing being renamed is the same object in both — and here it
   is, for a stated reason: **`steps_accepted = 0` in both engines, so the phase
   is entered at the start point `x0` in both** (`x_k_source=start_point`), and
   the first restoration iterate reproduces the read's `4 547.15` to the read's
   own printed digits. On that basis, and only on it, no value in the read moves.
2. **M1 moved STEENBRC's cost, and the two engines must not be mixed.** At the
   read's pin the cell costs **32 137** factorizations (`control.txt`, identical
   to H3's `run-A` at full precision, 16/16 columns); at the M1-shipped default it
   costs **42 946** (`ladder/STEENBRC.txt`). The control reproduces the read's
   published table exactly — 24 cells compared, 0 differing — so this is M1's
   declared effect on the cell and not a reproduction failure.

---

## 2. Ruling on the three shapes, from the numbers

The settler's ranking is (i) proximity term > (ii) acceptability-based return >
(iii) reporting, and the page may overturn it with evidence. **The ladder
confirms the ranking. Fix round 1 changed the REASON for (ii)'s place in it**:
(ii) is not weak because there is nothing for it to return — that argument read
`f` where the code does not — but because, spelled as the settler spelled it, it
returns at the phase's first iterate on every cell measured, including the two
the TRAIN bar protects. §2.1 is the derivation and §2.1a re-specifies the arm.

### 2.1 (ii), re-argued from `FunnelStrategy::judge`: the funnel is f-blind, so a judged return fires at the phase's FIRST iterate on every cell in this set

**This section is a retraction and a re-argument (review F2).** The first draft
rejected every STEENBRC iterate because "every depth-1 iterate raises `f`". That
is a MERIT-FUNCTION argument, and hven's funnel is not a merit function. The code
is the authority, and it was not read closely enough the first time.

**What `judge` actually does** (`src/globalization/sqp/funnel.cpp`, in order):

1. the non-finite guard — anything NaN or infinite is `kReject`;
2. **Eq. (8)**, necessary for any acceptance: `h_new <= width_`, else `kReject`
   (or `kRestore` on the five-conjunct restoration signature);
3. **Eq. (10)**, the switching condition: the trial is **f-type** iff
   `pred_df >= kFunnelDelta * h_old * h_old` with `kFunnelDelta = 0.999`;
4. on the f-type arm only, **Eq. (11)**'s Armijo test reads `f_old − f_new`;
5. otherwise the trial is **h-type**, and **Eq. (12)** accepts it iff
   `h_new <= kFunnelBeta * width_` with `kFunnelBeta = 0.99`.

**`ctx.f_old` and `ctx.f_new` are never read on the h-type arm.** That is not an
oversight in the transcription — it is KLV Algorithm 2, and the class note says
so. So the question "would the strategy accept this restoration iterate as a
trial step?" is, for any restoration iterate, a question about `h` alone.

**A restoration iterate is always h-type.** It carries no sub-QP model of the
MAIN problem's objective, so whatever a judged-return arm supplies for `pred_df`,
it is nowhere near Eq. (10)'s threshold: `0.999 · h_old²` is 1.7455e9 on
STEENBRC and 606.7 on ZAMB2-8, while the objective actually MOVES by +81.93 and
−0.2256 across those steps — three to seven orders below the threshold a
predicted decrease would have to clear.

**And the width is wide, because nothing has narrowed it.** `reset(h0)` sets
`width_ = max(kFunnelTauBar, kFunnelKappaBar · h0) = max(100, 1.25·h0)`
(`include/hven/detail/globalization/sqp/globalization.h`: `kFunnelTauBar = 100`,
`kFunnelKappaBar = 1.25`), and it moves only on an accepted h-type step or on
`resume_from_restoration`. **On all six restoring cells of this artifact
`steps_accepted = 0` at the moment of the phase** (`ladder/*.txt`; ZAMB2-8's
eight acceptances are all AFTER its restoration), so the width is still
`1.25 · h_entry` — and the phase's own entry iterate is inside it by
construction, with **23.75 % of headroom above `h_entry`** (Eq. (12) refuses only
above `0.99 × 1.25 = 1.2375` times it) before the funnel bites at all.

**Worked, on every cell that restores** — `h0` and the iterates from `ladder/*.txt`:

| cell | `h0` (= `h` at entry) | `width_` = max(100, 1.25·h0) | `0.99·width_` | first restoration iterate's `h` | Eq. (12) |
|---|---:|---:|---:|---:|---|
| STEENBRC | 41 800 | 52 250 | 51 727.5 | 41 786 (iter 1) | **`kAcceptH`** |
| STEENBRG | 41 800 | 52 250 | 51 727.5 | 41 786 (iter 1) | **`kAcceptH`** |
| STEENBRB | 19 800 | 24 750 | 24 502.5 | 19 776 (iter 1) | **`kAcceptH`** |
| YORKNET | 48 491.383947016489 | 60 614.23 | 60 008.09 | 48 491.383947016489 (iter 0, `dx_inf = 0`) | **`kAcceptH`** |
| BRITGAS | 115.03999999999979 | 143.80 | 142.36 | 115.03999999999979 (iter 0, `dx_inf = 0`) | **`kAcceptH`** |
| ZAMB2-8 | 24.643618798749976 | **100** (the floor binds) | 99 | **26.8646180503947** (iter 1, *above* `h0`) | **`kAcceptH`** |

**So shape (ii), spelled as the settler spelled it — "resume at the first
restoration iterate the strategy would accept as a trial step from the requesting
iterate" — is not a guard. It is "return at once", on all six.** Two consequences
are load-bearing and both contradict the first draft:

* **It MOVES ZAMB2-8**, the cell §6's TRAIN bar names as unmoved. The judged
  return fires at iteration 1 and hands back `h = 26.8646180503947` — *above* the
  entry's 24.643618798749976 — at `f = −0.20751874500187104`, in place of today's
  return at `h = 1.209356876794021e-11`, `f = −0.069042214516419051`
  (`ladder/ZAMB2-8.txt`). The cell that converges today is the cell (ii) disturbs.
* **It also moves BRITGAS**, which the first draft called inert on the ground
  that the phase exits on the terminal arm. That was wrong for the same reason:
  (ii) replaces the resume TEST, so it can resume where today's `h_r <= feas_tol`
  falls through to the terminal arm. BRITGAS's iterate 0 is already acceptable.

**And the other reading of (ii) — judge only the phase's FINAL point — changes
nothing on the cells that motivated M2.** STEENBRC's returned point has
`h = 4.8902304088678294e-12 ≤ 51 727.5`, so Eq. (12) accepts `f =
1.8345379627133412e+14` exactly as the unconditional rule does today. `f` is not
consulted, so there is nothing for it to catch.

**What survives.** (ii) cannot make STEENBRC converge. Returning at iterate 1
hands the main loop a point 0.13429130195248773 from the one it asked from, with
`h` 41 800 → 41 786 and `f` 81.93 higher; the funnel is then re-based on 41 786
and the loop is where it started.

**What is retracted, in words:** "there is nothing for it to return"; "shape (ii)
therefore has nothing to return on the cells that need help"; "the resume is
refused and the cell falls to the terminal arm"; "it is TRAIN-safe where it
matters"; "passes any sane acceptability test"; and §5's entire (ii) column,
rewritten below.

**The general fact, which is the thing to register.** A funnel is **f-blind by
design** on the only arm a restoration iterate can reach. A "judged" return
therefore **cannot be spelled as `st.strategy->judge(...)`** — it fires
immediately or not at all, and never on `f`. Any arm that is to bound the
objective at the return must carry **its own explicit test**, which is precisely
what A0 §2.1's third arm `kObjectiveBounded` is. §4 and question 3 take that up:
once the f-blindness is admitted, (ii) and `kObjectiveBounded` are the same shape
under two names, and the page keeps one arm rather than two.

### 2.1a Re-specifying (ii), honestly

The arm is worth keeping — a restoration that returns an arbitrarily worse point
is a real failure mode and a future cell's ladder may contain a good iterate —
but it must be specified as the test it needs, not as a call into the strategy:

> `kJudged`: resume at the first restoration iterate satisfying **both**
> `h <= feas_tol` (today's test, unchanged) **and** an explicit objective bound at
> the return. The bound is the arm's own — the strategy supplies nothing usable —
> and its constant is the unresolved part.

That spelling keeps ZAMB2-8 (its returned point satisfies both halves) and
BRITGAS (its phase never reaches `h <= feas_tol`, so the arm never fires and the
terminal exit stands) exactly where they are today, and it is the only spelling
of (ii) on which the first draft's TRAIN-safety claim is true. **The objection the
read raised against `kObjectiveBounded` — that the bound is a tuning constant —
applies to this spelling in full and is the real objection**, not the one the
first draft made. It is named and unbuilt (§4), and question 3 asks the settler
to confirm that.

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

Shapes (ii) and (iii) both *select among the iterates the phase generates*, and
§2.1 shows what selection can and cannot buy: the only iterate on STEENBRC's
ladder that is both inside `feas_tol` and selectable is the one the driver takes
today, at `f = 1.8345379627133412e+14`. Selection can refuse it; it cannot
replace it. **Only (i) changes which iterates are generated**, and it does so by
supplying precisely the
information the feasibility problem is missing — that `x` should stay near `x_k`
— which is what `dx_inf` 0.13 → 9 999.9 against a radius 0.5 → 8 192 measures.

---

## 3. The proximity term, spelled exactly

### 3.1 The precedent, as the IPM lane carries it — and WHICH half of it applies

**The IPM lane carries the proximity term twice, in two different roles, and the
first draft cited the wrong one (review F3).** Both are stated here, because the
difference is the whole of §3.5's calibration question.

**The half that gives the FORM** is the proximal mode-switch,
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

**But it gives no SCALE, because it is a SUBSTITUTION.** At the site
(`src/drivers/ipm_solver.cpp`, the proximal mode-switch) the term replaces the
objective outright — `val = this->restoration_->proximal_objective(...)`, the
true objective's value and gradient discarded, "uniform proximal-objective
substitution, no constraint-row modification". **There is no violation term for
`ζ` to be weighed against**, so nothing in this half of the precedent says how
heavy the proximity term should be *relative to* an l1 feasibility objective.

**The half that gives the SCALE is the nested l1 mode**,
`include/hven/detail/globalization/l1_restoration.h` — and it is the right
analogue, because the SQP wrapper IS an l1 elastic feasibility problem
(`restoration.h`'s own formulation). Its objective, from the class header:

```
    rho*sum(n+p) + (eta(mu)/2)*|D_R (x - x_R)|^2

    rho = kRestoPenaltyParameter = 1.0e3       (Ipopt "resto_penalty_parameter")
    eta = kRestoProximityWeight * sqrt(mu)     (= 1.0 * sqrt(mu), recomputed LIVE
                                                here, unlike the switch's frozen zeta)
    D_R = diag(1 / max(1, |x_R_i|))            (the same d_i as above)
```

**So the precedent's weight is RELATIVE, and it is `√μ / 1e3` per unit of
violation.** The violation term is carried at 1e3 and the proximity term at `√μ`:
the reference's answer is a **weak regulariser**, three orders below the
violation term before `√μ` is counted at all. That single ratio is what §3.5's
candidate set has to be anchored on, and what the first draft's candidates missed
by three decades.

### 3.2 The SQP transcription

The SQP restoration phase's centre is the **requesting iterate** `x_k` — the same
point S5's entry half is taken at, and already in hand at
`enter_restoration`'s `resto_ev`/`st.ev`.

**One open item, named here rather than discovered in implementation (review
F4): `x_k` is not always the point the wrapper is BUILT at.** The phase's seed
guard may start it from an offered candidate — `const RestorationModel
feasibility(bridge.model(), *x_start, resto_ev)` in `enter_restoration`, where
`x_start` is `cand.x` whenever the candidate's `h` is below the entry's. When
that guard fires, `x_start != x_k`, and a term centred on `x_k` is non-zero at
the wrapper's own start point — which breaks
`RestorationModel.IsTheSmoothL1FeasibilityProblem`'s `eval_f(y0) == h(x_entry)`
(§7). Centring on `*x_start` instead keeps the pin and costs the excursion
measure its anchor. **This page recommends centring on `*x_start`** — the point
the wrapper is constructed at, which is also the point its σ scaling and start
slacks are read from, so the whole wrapper stays centred on one point — and
registers the decision as item 8 of §11. The two coincide whenever the seed guard
does not fire, which is every cell in this artifact
(`mj.row.restoration_seed_used` is the observable).

The term is the IPM lane's, verbatim in form:

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
`restoration_restart_radius()`, no second tuning surface. **The settler has ruled
on the alternative** (§11, ruling 5): an explicit excursion cap on the sub-solve
is REGISTERED as a fallback and left UNBUILT, for the case where §3.5's
calibration leg finds no admissible `ζ`. The reason this page does not recommend
it as the primary stands: a cap is a hard constant where the term is a price, and
it carries the same calibration problem without the term's precedent.

### 3.4 What it costs the infeasibility certificate — the load-bearing consequence

**This is the one place shape (i) is not a free transcription of the IPM lane's
term, and it must be ruled on rather than discovered during implementation.**
*(Rewritten in fix round 1, review F1: the correction the first draft proposed
was a tautology.)*

Today's certified-infeasibility exit rests on the zero x-block, twice:

1. In the driver, the bound price is taken from the restoration problem and not
   from `kkt_r` — `st.resto.kkt.z = feasibility.original_x(rs.z)` — with the
   reason stated at the site: *"The sub-solve's own z is the price of the SAME
   box in the feasibility problem, whose objective has no x-block, so its x-part
   is precisely the normal-cone component of `Je^T λ_e + Ji^T λ_i`."*
2. In the suite, `check_certificate` (`tests/sqp/test_sqp_restoration.cpp`)
   re-derives `stationarity = ‖Jeᵀλ_e + Jiᵀλ_i − z‖∞` from the model, with *"NO
   grad f term — that absence is what makes this a certificate of INFEASIBILITY
   rather than of optimality"*.

#### The correction the first draft proposed is VACUOUS, and the reason matters

The wrapper's x-block stationarity at `ζ > 0` is `∇P + Jᵀλ − z_sub = 0` (the
driver's sign convention is `∇L = ∇f + Jᵀλ − z`). Subtracting `∇P` from the price
— `z_cert = z_sub − ζD²(x − x_k)` — therefore gives

```
    Jᵀλ − z_cert  =  Jᵀλ − z_sub + ∇P  =  0     identically, at every
                                                wrapper-stationary point
```

so the "explicit residual re-check" built on it is satisfied **exactly when
`rs.status == kOptimal` is**. It adds nothing to the arm it was supposed to
guard, and what it certifies is stationarity of `h + P` — the very thing the
first draft's own next sentence says must not be certified. `z_cert` is also not
a normal-cone element of the box: it is non-zero on coordinates strictly inside
their bounds wherever `x ≠ x_k`. The claim that it "restores precisely today's
quantity" was true of the **arithmetic** and false of the **meaning**.

#### The exact rule is the OPPOSITE substitution: leave `z_sub` alone

`z_sub` is still the genuine price of the SAME box in the sub-problem —
sign-correct and complementary whatever that sub-problem's objective is — so it
is still a valid normal-cone element and today's substitution at the site stands
**unchanged**. What moves is the TEST:

> at `ζ > 0`, `infeasibility_certified` is set **only if**
> `‖Jeᵀλ_e + Jiᵀλ_i − z_sub‖∞ ≤ kkt_tol` — the UNCORRECTED residual, re-checked
> explicitly — and never on `rs.status == kOptimal` alone.

Three properties follow, and they are why this is the rule to take:

* **It is the right quantity.** At a wrapper-stationary point that residual is
  exactly `‖∇P‖∞ = ζ‖D²(x_r − x_k)‖∞` — the gap between "stationary for `h + P`"
  and "stationary for `h`". The test asks whether the proximity term was doing
  any work at the returned point and certifies only when it was not.
* **It is `O(n)` and needs no Jacobian product**: `∇P` is closed-form from data
  already in hand, so the test can be written either as the residual or as the
  cheaper `ζ‖D²(x_r − x_k)‖∞ ≤ kkt_tol`.
* **`check_certificate` needs NO change.** It already computes `‖Jᵀλ − z‖∞` with
  the uncorrected `z`; at `ζ = 0` it is today's test byte for byte. The first
  draft had it changing on both halves; it changes on neither.

#### The consequence is a real loss, and the option that keeps the capability

`‖∇P‖∞` is small only when `x_r ≈ x_k`. At any `ζ` on §3.5's scale the certified
exit therefore becomes **practically unavailable on every phase that moves** —
which is every phase shape (i) exists to change. That is not a footnote; it is
the price of the term, and the settler should weigh it as one.

**The option that keeps M6's capability, which was not on the first draft: a
`ζ = 0` continuation on the TERMINAL arm only.** When the phase exits
not-feasible-enough at `ζ > 0`, re-solve the wrapper once from `x_r` with the
term switched off, and read the certificate off that solve. It costs one extra
feasibility solve on the exit path that is already the expensive one, it touches
nothing on the resume arm, and it is strictly separable — the first task can land
the term at a declared OFF plus the residual test and leave this unbuilt.
Question 1 in §11.

### 3.5 What this artifact cannot settle: ζ's value

*(Rewritten in fix round 1, review F3. The precedent the first draft cited was
the wrong half of the IPM lane, and the candidate set inherited the error.)*

#### What survives from the first draft

The dimensional argument. The wrapper's objective is in violation units and
`‖D(x − x_k)‖²` is a squared *relative* displacement, so `ζ` carries violation
units, and the one violation quantity in hand at entry is `h_k` — which is also
exactly the wrapper's own objective at its start point and exactly S5's
`restoration_h_in`. **The ladder confirms that identity rather than assuming
it**: the wrapper's objective on each phase's first depth-1 row is
`f_event = 41 800` on STEENBRC and `24.643618798749969` on ZAMB2-8
(`ladder/STEENBRC.txt`, `ladder/ZAMB2-8.txt`), against `h` of 41 800 and
24.643618798749976 at the same points.

Also surviving: "smallest member that bites", and "return to the settler if none
qualifies rather than introduce a constant quietly".

#### What is dropped, and why

* **`√(h_k)` is dropped.** It contradicts the units argument in the same
  paragraph that makes it.
* **`h_k/‖D(x_box)‖²` is dropped.** `x_box` is defined nowhere — not in this
  engine (`grep` finds it only on this page) and not on the page itself — and any
  reading of it that admits an infinite bound gives `ζ = 0`, which is OFF. It is
  dropped rather than repaired.
* **`ζ = h_k` is demoted from "the natural candidate" to the heaviest member of
  the set**, for §3.1's corrected precedent: it puts the proximity weight at
  violation scale against an l1 term at unit scale (`RestorationModel`'s header:
  the σ scaling rides the objective coefficient AND the column, so "the objective
  is still the actual violation"), i.e. **three orders heavier than the nested l1
  mode's `√μ : 1e3`**. The knife edge the first draft found on ZAMB2-8 is the
  symptom of that, not a property of the shape.

#### The corrected candidate set

Ordered smallest-first; every member keyed to the requesting iterate, every one
built from constants **already in the tree** — `kRestoProximityWeight = 1.0` and
`kRestoPenaltyParameter = ρ = 1.0e3` — so the set still introduces no new
constant:

| # | member | reading |
|---|---|---|
| 1 | `ζ = h_k / (n·ρ)` | the precedent's relative weight, spread over the coordinates |
| 2 | **`ζ = h_k / ρ`** | **the precedent, transcribed**: the nested l1 mode's `η : ρ` ratio, at violation scale |
| 3 | `ζ = h_k / n` | one coordinate's share of the entry violation |
| 4 | `ζ = h_k` | the first draft's candidate: the phase pays its whole entry violation for one unit of relative displacement |

On all seven cells of this artifact `n ≤ 540 < ρ` (`ladder/*.txt`: 102, 138, 312,
450, 468, 540, 540), so the listed order IS the numeric order. If a future cell
has `n > ρ` the order is re-derived from the values, never assumed from the list.

#### The measurement

One added probe column, `prox_rel = ‖D(x − x_k)‖²` with `D` built from the
requesting iterate, per depth-1 row — a diagnostic in the probe's own wrapper, no
engine change, the same seven cells, seven legs, one per lock hold. **It is
needed because the ladder does not carry it**: the probe measured
`dx_inf = ‖x − x_k‖_∞`, and the two differ by the per-coordinate magnitudes of
`x_k`, which this artifact does not carry — `prox_rel` is a SUM over coordinates
(so it can exceed `dx_inf²` by up to `n`) scaled by `d_i ≤ 1` (so it can fall
below it without bound). Neither direction is bounded from what is in hand, which
is exactly why the column exists.

**One measurement-meaning caveat the leg's README must carry** (review F4): at
`ζ > 0` the sub-solve's own funnel judges `h + P`, so the probe's depth-1
`f_event` column — the wrapper's objective — no longer equals the wrapper's
violation. The identity quoted above (`f_event == h` at the first depth-1 row) is
a **`ζ = 0` fact** and must be labelled as one wherever the leg reprints it.

#### The decision rule, pre-registered before the leg runs

On A4's own precedent (`hven-bench/docs/w2-a4-decision-rule.md`, landed before
any TRAIN run), and stated with the three things the first draft left
under-specified:

* **the rule**: take the **smallest** member of the ordered set above that
  (a) makes STEENBRB's, STEENBRC's and STEENBRG's measured final step
  inadmissible, while (b) leaving ZAMB2-8's returned point and (c) BRITGAS's
  0.031-excursion step admissible — each **by a factor of at least 10**. If no
  member satisfies all three with that margin, **the shape is reported as not
  calibratable without a new constant and returns to the settler** rather than
  one being introduced quietly.
* **the margin is part of the rule, and is pre-registered for a reason.** The
  screen's free quantity (`prox_rel` against `dx_inf`) is unbounded in both
  directions, so a member that clears a clause by a factor of 2 has not cleared
  it. One order of magnitude is the smallest margin that is a finding rather than
  a coincidence; it is a decision-rule constant, not an algorithm constant, and
  it ships nowhere.
* **incremental vs cumulative, resolved — (a) is INCREMENTAL, (b) and (c) are
  CUMULATIVE, because they ask different questions.**
  * (a) asks whether the sub-QP would still TAKE the runaway step, which is a
    per-STEP question: the step `x_{t−1} → x_t` is inadmissible when
    `(ζ/2)·[prox_rel(x_t) − prox_rel(x_{t−1})]` exceeds the violation it buys,
    `h(x_{t−1}) − h(x_t)`. The choice is material: on STEENBRC's last step the
    incremental and cumulative readings differ by a factor of
    `1 − (6 060.0144008426114 / 9 999.9000000000015)² ≈ 0.633`
    (`ladder/STEENBRC.txt`, rows 14 and 16).
  * (b) and (c) ask whether the PHASE survives the price, which is a per-PHASE
    question: the returned point is admissible when `(ζ/2)·prox_rel(x_r)` —
    cumulative, from `x_k` — is below the phase's whole gain `h_k − h_r`. An
    incremental reading of (b) would be vacuous: ZAMB2-8's final depth-1 row has
    `step_norm = 0` (`ladder/ZAMB2-8.txt`), so every `ζ` would pass it.
  * The leg prints **both** readings on every row regardless; the rule reads the
    one named here.
* **it is a TRAIN-fitted SCREEN on the `ζ = 0` ladder, not a prediction.** With
  the term ON, the sub-QP's minimiser moves from iteration 1, so the trajectory
  the screen is evaluated on is **not** the trajectory the fix produces. The
  screen says which weights cannot be right; it does not say which one is. The
  discrete choice among four members is a fit to the TRAIN cells; HOLDOUT is the
  check and the fix's own TRAIN re-run (§8, item 8) is what decides.

#### The rule, run on what is in hand — a worked preview, not the leg's answer

Substituting `prox_rel ≈ dx_inf²` (the `d = 1`, one-dominant-coordinate reading —
an ILLUSTRATION, with the unbounded error above), the ratio
`(ζ/2)·prox_rel / gain` comes out as follows, every input from `ladder/*.txt`
(`restoration_h_in`, `restoration_h_out`, the final depth-1 `dx_inf`, `n`).
**> 1 means the phase is refused:**

**The preview evaluates all three clauses CUMULATIVELY**, including (a), because
the ladder's final-row `dx_inf` is a cumulative quantity and the incremental
split needs the `prox_rel` column the leg adds. That is a deviation from the rule
as pre-registered above, declared here rather than smuggled: on the three runaway
cells it is a mild one, since their final step IS most of the excursion (STEENBRC
6 060.0 → 9 999.9, STEENBRB 696.9 → 1 353.2), so the incremental reading moves
each row by a factor between 0.6 and 0.8, not by an order. The leg applies the
rule as written.

| member | STEENBRB | STEENBRC | STEENBRG | ZAMB2-8 | BRITGAS |
|---|---:|---:|---:|---:|---:|
| 1 · `h_k/(n·ρ)` | **1.96** | 92.6 | 92.6 | 5.21e-4 | 2.53e-8 |
| 2 · `h_k/ρ` | **916** | **5.0e4** | **5.0e4** | **0.0718** | **1.14e-5** |
| 3 · `h_k/n` | 1 956 | 9.26e4 | 9.26e4 | 0.521 | 2.53e-5 |
| 4 · `h_k` | 9.16e5 | 5.0e7 | 5.0e7 | **71.8** | 0.0114 |

Read against the rule: **member 4 fails (b)** — it refuses ZAMB2-8 by a factor of
72, which is the first draft's knife edge landing on the wrong side once the
excursion is taken at `d = 1`. **Member 3 clears (b) by only a factor of 1.9**
and fails the margin. **Member 1 clears (a) on STEENBRB by only a factor of 1.96**
and fails the margin there. **Member 2 — the precedent — clears every clause by
between one order (ZAMB2-8, ×13.9) and nearly five (BRITGAS, ×8.8e4), with ×916
on the tightest runaway cell** — which is the first independent thing on this
page to point at the same weight the reference implementation already ships.

That convergence is the reason to run the leg, not a substitute for running it:
the table's single free assumption is the one the leg's `prox_rel` column
replaces, and the trajectory it is computed on is the `ζ = 0` one.

#### Why the separation is robust even though `ζ` is not

The one scale-free thing the ladder does establish is that **the penalty is
QUADRATIC in the excursion while the gain is not.** STEENBRC's phase buys 41 800
of violation at a cumulative `dx_inf` of 9 999.9000000000015; ZAMB2-8's buys
24.643618798749973 at 11.986825691351491 — **834 times the displacement for 1 696
times the gain**, so the `ζ` at which STEENBRC's phase becomes inadmissible is
`834²/1 696 ≈ 4.1e2` times smaller than the `ζ` at which ZAMB2-8's does. (The
first draft quoted `834²/147 ≈ 4.7e3` for this; that figure mixed STEENBRC's
*last step's* gain with ZAMB2-8's *whole phase's*, which is exactly the
incremental-vs-cumulative confusion resolved above. The honest phase-level
separation is 4.1e2, and it is still two and a half orders.) BRITGAS sits far
outside either end, with a whole excursion of 0.03125000000000089 against a gain
of 4.932593526637135 — inert under every member of the set.

---

## 4. The toggle, its exact spelling, and its OFF proof

```cpp
/// How a restoration phase decides to hand its point back to the main loop.
enum class RestorationResumeRule {
    kUnconditional,   ///< today: resume iff h_r <= feas_tol, whatever f_r is
    kJudged,          ///< shape (ii), as re-specified in 2.1a: resume at the first
                      ///< restoration iterate satisfying BOTH h <= feas_tol AND this
                      ///< arm's OWN objective bound at the return. NOT FunnelStrategy::
                      ///< judge() -- the funnel is f-blind on the only arm a restoration
                      ///< iterate reaches, so judge() would fire at iterate 1 on every
                      ///< cell measured (2.1). The bound's constant is unresolved and
                      ///< this arm is NOT BUILT by the first task.
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
  `QpMode::kQpModeCount`'s precedent.
* **A0's third arm `kObjectiveBounded` is KEPT, NAMED AND UNBUILT** — the first
  draft dropped it, and review F5 retired half of that argument. The half that
  stands is the read's: the bound is a **tuning constant**, and that is the real
  and sufficient objection to building it now. The half that falls is this page's
  second reason ("it approximates a test with nothing to bite on"), which was
  §2.1's error: with the funnel f-blind at the return, `kObjectiveBounded` is the
  **only listed shape that consults `f` there at all**. §2.1a shows the further
  consequence — once (ii) is spelled honestly it *is* `kObjectiveBounded`, so the
  two are one shape under two names and the page keeps **one** arm, not two.
  Whether the shipped spelling is a third enumerator or the `kJudged` arm's own
  test is the implementing task's call and is not decided here; neither is built
  until (i) is measured.
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

**The (ii) column is rewritten (review F2).** It now reports what
`FunnelStrategy::judge` actually does — §2.1's table is the derivation — and the
first draft's column, which read `f` where the code does not, is retracted in
full. "(ii) as spelled" below means the settler's own spelling, judged by the
strategy; §2.1a's re-specified arm is a different object and is discussed there.

| cell | under (i) `ζ > 0` | under (ii) **as spelled** (`judge()`) | under (iii) reporting |
|---|---|---|---|
| **STEENBRC** | the runaway is priced: the last step buys 3 617 of violation for an excursion of 9 999.9, which a `ζ` calibrated to leave ZAMB2-8 admissible refuses by two and a half orders (§3.5). **The trajectory changes**, so the outcome is an empirical question, not a prediction | **fires at restoration iteration 1** (`h` 41 786 ≤ 0.99·52 250 = 51 727.5), returning a point 0.134 from `x_k` at `f` +81.93 and `h` 41 786. The funnel is re-based on 41 786 and the loop is where it started — **(ii) cannot make this cell converge** | reports the requesting iterate — `f` 4 528.31 at `h` 41 800 — instead of 1.83e14 at 4.9e-12. Neither is an answer |
| **STEENBRB** | as STEENBRC (`dx_inf` 1 353.18, `f` → 4.09e12) | fires at iteration 1 (`h` 19 776 ≤ 0.99·24 750 = 24 502.5); otherwise as STEENBRC | as STEENBRC |
| **STEENBRG** | as STEENBRC, to eleven digits | as STEENBRC, to eleven digits | as STEENBRC |
| **ZAMB2-8** | **at risk, and the bar** — §3.5's calibration exists to decide exactly this. Its returned point is at `dx_inf` 11.99 having bought 24.64 of violation *and* 0.087 of objective; the corrected candidate set leaves it admissible by more than an order at member 2 | **MOVED — and this is the page's second finding.** `width_` = max(100, 1.25·24.6436) = **100** (the floor binds), so iterate 1's `h` = 26.8646 ≤ 99 is `kAcceptH` and the arm returns `h` 26.86 (*above* the entry's 24.64) at `f` −0.2075, in place of today's `h` ≈ 1.2e-11 at `f` −0.0690. **The TRAIN bar's "unmoved" is broken by (ii) as spelled** | unmoved: the returned point *is* the best iterate |
| **YORKNET** | **inert**: `dx_inf = 0` at all 18 iterations, so `P ≡ 0` and `∇P ≡ 0` along the whole phase. The added `ζD²` diagonal makes a zero x-step *more* attractive, not less | **fires at iteration 0 and returns the requesting iterate itself** (`h` 48 491.383947016489 ≤ 0.99·60 614.23), because `dx_inf = 0` makes the candidate and the centre the same point. **No returned value differs from today's**; whether that ends the phase after one iteration or cycles the main loop back into it is not predictable from the ladder | inert: every iterate reports the same `f` and the same four residuals |
| **BRITGAS** | **very likely inert** — the whole excursion is 0.031 against STEENBRC's 9 999.9, and the penalty is quadratic in it, so a weight that bounds the latter is many orders from touching the former (§3.5's preview: 2.5e-8 to 1.1e-2 across the set). Measured by §3.5's leg rather than assumed | **MOVED, and the first draft was wrong here too.** It said BRITGAS is inert because the phase exits on the terminal arm — but (ii) replaces the resume TEST, so it can resume where `h_r <= feas_tol` falls through. Iterate 0 is already acceptable (`h` 115.04 ≤ 0.99·143.80 = 142.36), so (ii) resumes at the entry point instead of exiting at `h` 110.107, `f` 0.0755 | would report `f = 0` at `h = 115.04` instead of 0.0755 at 110.107 |
| **READING6** | **provably inert**: `restoration_entries = 0`, the phase never runs, no wrapper is constructed | **provably inert, and the only cell in the set that is** — same reason | provably inert, same reason |

Three of those rows are the page's substantive findings.

**(ii) as the settler spelled it is not a guard — it is "return at once", on all
six restoring cells.** That is §2.1's table read down the column, and it is why
the arm must be re-specified (§2.1a) or dropped rather than shipped as written.
The cause is structural, not a property of these cells: the funnel's initial
width is `1.25 ×` the START point's `h`, `steps_accepted = 0` on every one of the
six at the moment of the phase, and the h-type arm accepts anything up to
`0.99 × 1.25 = 1.2375` times it — **23.75 % above the entry violation** — so the
phase's own entry iterate is inside its own funnel by construction.

**YORKNET is not M2's to fix, and the ladder proves it.** `dx_inf = 0` at every
one of eighteen iterations, with the sub-solve's radius halving 1 → 7.63e-06 and
`h_in == h_out` to the last digit, says the phase generates no motion at all —
so a term that prices motion changes nothing, and a rule that judges motion has
nothing to judge (it judges the entry point, and returns it unchanged). The read
said it first from the other side: *"Where STEENBRC's restoration went somewhere
bad, YORKNET's went nowhere at all — and the two failures are opposite, which is
why one fix will not serve both."* **What would shorten YORKNET's ladder is a
fourth shape that is on nobody's list: a phase-level stall exit** — end the phase
when `h` has not moved in `k` iterations. That is registered (ruling 4 in §11)
and belongs with M5/M3, which own the price: 18 iterations that moved nothing
cost **≈ 2 900 factorizations each** (`ladder/YORKNET.txt`: 52 180 factorizations
over 18 restoration iterations, with 50 241 minors and 19 elastic activations).
*(Corrected from the first draft's "~2 200", review F4: 2 200 is the read's own
per-iteration slope from its budget ladder,
`docs/notes/2026-09-m7-w2-sqp-cutest-read.md:718`, not this ladder's quotient.)*

**BRITGAS is a new cell and a new question.** It exists in this set only because
M1 shipped budget 3, its phase neither runs away nor helps, and no shape on the
list improves it. It belongs to M1's own follow-up rather than to M2 (ruling 6).

---

## 6. The TRAIN bar

The bar the brief and A0 agree on, restated with what this page now knows — and
with the settler's **two rulings on it already written in** (questions 4 and 6,
settled before this revision; §11 records them as rulings, not questions):

| item | bar | this page's reading |
|---|---|---|
| STEENBRB | → `optimal` at **9 075.855**, where the IPM, IPOPT and SNOPT lanes all land | **not predictable from the ladder, and must be measured.** No iterate on STEENBRB's ladder is both inside `feas_tol` and sane, so reaching the bar requires the phase to generate *different* iterates — which only (i) does. It is the bar, not a forecast |
| ZAMB2-8 | **unmoved** (`optimal` at `f = −0.15293592733713887`) | the binding constraint on `ζ`'s calibration (§3.5), and — new in this revision — **the bar that (ii) as spelled breaks** (§2.1, §5). Under (i) at member 2 of the corrected candidate set it is admissible by more than an order |
| READING6 | **unmoved** (`optimal` at −144.65970057279054) | provably unmoved: `restoration_entries = 0`. READING6 is M1's converted control, so a shape that moves it is a shape that broke what M1 fixed |
| YORKNET | **RULED (4): "unmoved, registered to M5"** — the bar as originally written ("ladder shortened; its `kInfeasible` still uncertified until M5") cannot be met by this task | `dx_inf = 0` at all 18 restoration iterations is conclusive: no M2 shape shortens it (§5). The **phase-level stall exit** is registered as a candidate owned by M5/M3 |
| STEENBRC, STEENBRG | not named in the bar | they move with STEENBRB or the fix is not the fix |
| BRITGAS | **RULED (6): added to the bar as *unmoved*** | on READING6's logic — it is M1's newest restoration cell and a shape that changes it is changing something M1 shipped. The cell itself belongs to **M1's follow-up**, not to M2 |

---

## 7. Pins at risk, and the declared re-derivation plan

| pin | at risk under | plan |
|---|---|---|
| the three replay CSVs `head-{walk,ssn,ipm}.csv` (`docs/notes/data/2026-09-m6-close-gate/replay/`, `scripts/compare_replay.py`, the 27 cells at `docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt`) | any shape, if a default moves; **nothing**, at the declared OFF | **checked, not assumed.** M1's precedent (a re-derivation budgeted and not needed, because the 27 replay cells never route a subproblem failure) does **not** transfer for free: M2's shapes change what happens at a *restoration*. S5 makes the check a one-column question it was not before this task — run the 27 cells and read `restoration_entries`; a set that never restores is unmoved by construction, and a set that does gets a declared re-derivation |
| `tests/sqp/test_sqp_restoration.cpp`'s certified-exit fixtures | (i), via §3.4 | **`check_certificate` itself is NOT at risk and does not move** (review F1): the corrected rule leaves `z_sub` uncorrected, so the helper's `‖Jᵀλ − z‖∞` is today's quantity at `ζ = 0` and the meaningful one at `ζ > 0`. What moves is the DRIVER's gate on `infeasibility_certified`, pinned by §8's tests 3 |
| **`RestorationModel.IsTheSmoothL1FeasibilityProblem`** (`tests/sqp/test_sqp_restoration.cpp`: `eval_f(y0) == h(x_entry)`, exactly, on both constraint types) | **(i), unconditionally at `ζ > 0`** — the wrapper's objective at its own start point gains `P(x_start)` | it holds **iff the centre is the wrapper's construction point**. Centred on `*x_start` (§3.2's recommendation) `P(x_start) = 0` and the pin is **unmoved**; centred on `x_k` it breaks whenever the seed guard fires. §11 item 8 is the decision; the pin is the reason it is a decision and not a detail |
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
3. **The certificate — items (a) and (b) are INVERTED from the first draft**
   (review F1), because the rule is inverted. On `InfeasibleCircleLineModel` at
   `ζ > 0`:
   (a) with `z_sub` left **uncorrected**, `check_certificate`'s residual equals
   `ζ‖D²(x_r − x_k)‖∞` to floating-point — the quantity §3.4's gate tests,
   asserted against the closed form rather than against today's value;
   (b) **the mutant that must die is the one that SUBTRACTS `∇P`**: with
   `z = z_sub − ζD²(x_r − x_k)` the residual collapses to ~0 at every
   wrapper-stationary point, so a test that accepts that value is testing
   nothing. Assert the subtracted form is NOT ~0 at a point with `x_r ≠ x_k`;
   (c) `infeasibility_certified` is refused when the **uncorrected** residual
   exceeds `kkt_tol` even though the sub-solve returned `kOptimal` (§3.4's rule),
   and is still set at `ζ = 0` on the same fixture — the OFF half of the gate;
   (d) `check_certificate` is **unchanged**; a diff of the helper against its
   parent is part of the review, not a test.
4. **The runaway, priced** — `RunawayValleyModel` (already in the suite) at
   `ζ = 0` and `ζ > 0`, asserting S5's `restoration_f_out` bounded at the second.
   This is the unit-scale analogue of STEENBRC and is the fixture the fix is
   test-driven from.
5. **The resume rule's own arm**, if and when it is built — under `kJudged` as
   re-specified in §2.1a, a fixture where the phase returns a point inside
   `feas_tol` that the arm's OWN objective bound refuses, pinning
   `restoration_resume_refused` and the exit that follows. The fixture must also
   pin the negative that §2.1 exposed: **`FunnelStrategy::judge` on the same
   candidate ACCEPTS it** (h-type, `h_new ≤ 0.99·width_`), so the arm is
   demonstrably not the strategy's verdict.
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
| the certificate gate in `enter_restoration` (§3.4: the uncorrected residual, re-checked; the substitution site itself unchanged) | ~20 lines and a long comment at the `switch (rs.status)` arm |
| the optional `ζ = 0` continuation on the terminal arm (§3.4), only if question 1 takes it | one extra sub-solve on the terminal path, ~40 lines, separable from the rest |
| the two `SqpOptions` fields, `validate()`, `dump_options()`, the three drift pins | ~40 lines, mechanical |
| `kJudged`'s arm as re-specified (§2.1a) and `restoration_resume_refused` — **not built by the first task** | ~50 lines plus one declared counter re-derivation (five pins, as `9a08a715` did) |
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

**Adopt shape (i), the proximity term, as M2's fix; keep ONE judged-return arm —
`kJudged` as re-specified in §2.1a, which is A0's `kObjectiveBounded` under
another name — NAMED and UNBUILT; leave shape (iii) registered and unbuilt.**

The ladder settles the ranking, and fix round 1 changed the reason for part of
it. On STEENBRC the objective climbs from the *first* restoration iteration
(4 528.31 → 4 610.23 for 41 800 → 41 786) and never turns, while the sub-solve's
radius doubles fourteen times, 0.5 → 8 192, and `‖x − x_k‖_∞` runs 0.13 →
9 999.9 into the box — because the feasibility objective has no x-block and
nothing in the problem the sub-solve is given prices distance from the requesting
iterate. That missing price is exactly the term the IPM lane already carries, so
(i) is a transcription rather than an invention, and **it is the only shape that
changes which iterates the phase generates.**

What changed in this revision: **(ii) is not "toothless", it is indiscriminate.**
`FunnelStrategy::judge` is f-blind on the only arm a restoration iterate can
reach, and every restoring cell in this artifact enters its phase with
`steps_accepted = 0` and therefore inside its own funnel — so a judged return
spelled on the strategy fires at the phase's FIRST iterate on all six, moving
ZAMB2-8 and BRITGAS, which the bar and the first draft both called unmoved. The
arm survives only if it carries its own objective bound, at which point it is
`kObjectiveBounded`, whose real objection is the read's: the bound is a tuning
constant. One arm, named, unbuilt, until (i) is measured.

Three things must be ruled on before the term is written.

1. **The certificate.** At `ζ > 0` the wrapper's `kOptimal` certifies
   stationarity of `h + P`, not of `h`. The fix is **not** to correct `z` — that
   substitution is algebraically vacuous — but to leave `z_sub` alone and gate
   `infeasibility_certified` on the uncorrected residual
   `‖Jᵀλ − z_sub‖∞ ≤ kkt_tol`, which at a wrapper-stationary point IS
   `ζ‖D²(x_r − x_k)‖∞`. `check_certificate` does not move. The cost is that the
   certified exit becomes practically unavailable on any phase that moves, and
   the option that keeps it is a `ζ = 0` continuation on the terminal arm.
2. **`ζ`'s value cannot be settled from this artifact** — the probe measured
   `‖x − x_k‖_∞`, not the scaled norm the weight multiplies — so M2's second task
   opens with a calibration leg whose candidate set, admissibility convention,
   margin and decision rule are pre-registered before it runs (§3.5, on A4's
   precedent). The set is now anchored on the right half of the IPM precedent:
   the nested l1 mode weighs its proximity term `√μ` against an l1 term at
   `1e3`, and `ζ = h_k/ρ` is that ratio transcribed.
3. **What `kJudged` means, and whether A0's third arm stays named.** §2.1a's
   re-specification and §4's "named and unbuilt" are the page's answer; the
   settler confirms or keeps two arms.

And one thing is registered rather than asked (§11 item 8): **the centre when the
phase is seeded**, `x_k` or `*x_start`. The page recommends `*x_start`, which
keeps `RestorationModel.IsTheSmoothL1FeasibilityProblem` unmoved.

Land the term at a declared OFF first — it moves no pin outside the options
triple — and make the default move a separate declared event with its own
re-derivations.

---

## 11. What the settler rules on

**Four rulings are already settled** (questions 4–7 of the first draft) and are
recorded here as rulings, not questions. **Three are open** (questions 1–3) and
are presented as corrected analysis with **one recommendation each** — this page
does not decide them. Item 8 is registered for the implementing task.

### Open: question 1 — the certificate at `ζ > 0`

**Corrected analysis.** The first draft offered a choice between "correct `z` and
re-check" and "declare the certificate unavailable at `ζ > 0`". The first option
does not exist: `z_cert = z_sub − ∇P` makes `Jᵀλ − z_cert ≡ 0` at every
wrapper-stationary point, so the re-check is satisfied exactly when
`rs.status == kOptimal` is and certifies `h + P`-stationarity (§3.4). The real
choice is between (a) keeping `z_sub` and gating on the **uncorrected** residual
`‖Jᵀλ − z_sub‖∞ ≤ kkt_tol` = `ζ‖D²(x_r − x_k)‖∞` — `O(n)`, `check_certificate`
unchanged, today's test at `ζ = 0` — and (b) declaring the certificate
unavailable whenever `ζ > 0`. Both have the same practical effect on a phase that
moves, because the residual is small only when `x_r ≈ x_k`; (a) is strictly more
informative and costs ~20 lines. Keeping the capability at `ζ > 0` needs a third,
separable piece: a `ζ = 0` continuation from `x_r` on the terminal arm only.

**Recommendation.** Take (a) — keep `z_sub` uncorrected and gate on that
residual, `check_certificate` untouched — and register the `ζ = 0` terminal-arm
continuation as a **separate, optional** follow-on, not part of the first task.

### Open: question 2 — `ζ`'s calibration

**Corrected analysis.** The rule's direction was right and its content was not.
The precedent is the nested l1 mode (`l1_restoration.h`), not the proximal switch
— the switch substitutes the objective and sets no relative scale — and the
nested mode's weight is `√μ` against an l1 term at `kRestoPenaltyParameter = 1e3`,
i.e. a **weak regulariser three orders below the violation term**. The first
draft's four candidates all sat at violation scale, which is where ZAMB2-8's
knife edge came from. `√(h_k)` contradicted the page's own units argument;
`h_k/‖D(x_box)‖²` named an undefined `x_box` and reduces to OFF on any infinite
bound. §3.5 now carries: a four-member set ordered smallest-first
`{h_k/(n·ρ), h_k/ρ, h_k/n, h_k}`, all built from constants already in the tree;
an explicit admissibility convention (incremental for the runaway clause,
cumulative for the two protective clauses, with the artifact's own numbers for
why); a factor-of-10 margin requirement; the "TRAIN-fitted SCREEN, not a
prediction" label; and the `f_event`-changes-meaning caveat for the leg's README.
Run on the ladder's `dx_inf` as a preview, the rule selects **member 2,
`ζ = h_k/ρ` — the precedent**: member 4 fails clause (b) outright (it refuses
ZAMB2-8 by ×72), and members 1 and 3 clear their tightest clause by only ×1.96
and ×1.9, inside the margin. That is a preview and not the leg's answer.

**Recommendation.** Adopt §3.5's corrected rule as written and **commit it before
the leg runs**; keep "return to the settler if no member qualifies" rather than
introducing a constant.

### Open: question 3 — A0's third arm, and what `kJudged` means

**Corrected analysis.** The first draft dropped `kObjectiveBounded` on two
grounds; review F5 retired one of them. With `FunnelStrategy::judge` f-blind on
the h-type arm, `kObjectiveBounded` is **the only listed shape that consults `f`
at the return at all**, so "it approximates a test with nothing to bite on" was
this page's own §2.1 error, not a finding. The surviving objection is the read's
and it is sufficient: the bound is a tuning constant. The further consequence is
that (ii) and `kObjectiveBounded` are **one shape**: any honest spelling of
"resume only at an acceptable iterate" has to carry its own objective test,
because the strategy will not supply one.

**Recommendation.** Keep it **named and unbuilt**, folded into a single
judged-return arm (§2.1a's `kJudged`), and say in the header comment that it is
not `judge()`. Revisit only after (i) is measured.

### Ruled: 4 — YORKNET's bar, and the fourth shape

**RULING.** YORKNET's TRAIN bar becomes **"unmoved, registered to M5"**, and a
**phase-level stall exit** — end the phase when `h` has not moved in `k`
iterations — is registered as a candidate **owned by M5/M3**, not M2. The
evidence is conclusive: `dx_inf = 0` at all eighteen restoration iterations
(`ladder/YORKNET.txt`), `h_in == h_out` and `f_in == f_out` to the last digit,
the radius halving monotonically 1 → 7.63e-06. No M2 shape shortens it, and the
price it is paying is ≈ 2 900 factorizations per restoration iteration.

### Ruled: 5 — the trust-region alternative

**RULING.** An explicit excursion cap on the sub-solve (`ropts.tr_max`, or a
`‖x − x_k‖ ≤ Δ_resto` constraint) is **REGISTERED as a fallback, UNBUILT** — for
the case where the calibration leg finds no admissible `ζ`. §3.3's reason stands:
the price is the primary, and a cap is a hard constant carrying the same
calibration problem as the weight without the weight's precedent.

### Ruled: 6 — BRITGAS's membership

**RULING.** BRITGAS **joins the TRAIN bar as UNMOVED**, on READING6's logic, and
**the cell itself belongs to M1's follow-up**, not to M2. Note for the
implementing task: it is unmoved under (i) by a wide margin (§3.5's preview), but
**not** under (ii) as the first draft spelled it (§5) — which is one more reason
the arm is re-specified rather than shipped as written.

### Ruled: 7 — the two-task split

**RULING.** The split is **CONFIRMED**: the term lands at a **declared OFF**
first, and the default move is a **separate declared event**. The default-move
task's re-derivation list is fixed here:

* the **27-cell replay** read via `restoration_entries` — **necessary, not
  sufficient**: a set that never restores is unmoved by construction, but a set
  that does needs the full three-arm re-derivation;
* the **four `SqpDriverRestorationExcursion` fixtures'** `h_out <= feas_tol`
  assertions;
* **`HsSweepCorpus.Corpus`**;
* the **options triple** — `kSqpOptionsFields` **30**, `kSqpDumpLines` **61**,
  `sizeof(SqpOptions)` **400**, read live from
  `tests/drivers/test_options_dump.cpp` today and re-read off the build then;
* the **bench digests** (`options_digest` on every retained row);
* **`RestorationModel.IsTheSmoothL1FeasibilityProblem`**.

### Registered: 8 — the centre when the phase is seeded

Not a settler question; recorded so the implementing task does not decide it
silently. `enter_restoration` builds the wrapper at `*x_start`, which the seed
guard may set to an offered candidate rather than to `x_k`. A term centred on
`x_k` is then non-zero at the wrapper's own start point and breaks
`RestorationModel.IsTheSmoothL1FeasibilityProblem`. **The page recommends
centring on `*x_start`** — the same point the σ scaling and the start slacks are
read from — and pins the decision with a fixture that exercises a fired seed
guard at `ζ > 0`. The two coincide on every cell in this artifact.

---

## Outcome (M2b) — appended 2026-09-20, after the term landed and the ζ calibration ran

**Append-only.** Nothing above this line is altered. This section is the page's
own record of what happened when the design it argues for was built and
measured, written against the artifact
`docs/notes/data/2026-09-m7-w2-m2b-proximity/` (35 legs, 2026-09-18), the code
commit `577719a0`, the settler's ruling `cb8b88c7`, and the review that ordered
fix round 1 (`.superpowers/m7-w2-m2b-review-sol.md`). A reader who arrives at
any line above should read that line together with the correction here.

### 0. The status line

**§0's "Nothing here is implemented" is superseded.** Shape (i) — the proximity
term — IS implemented, on branch `m7`:

* `SqpOptions::restoration_proximity`, a public `double` defaulting to `0.0`
  (OFF), `577719a0`;
* the term in `RestorationModel`'s objective, gradient and Hessian, branched out
  entirely at `ζ = 0`;
* the certificate gate of §3.4, in its exact form (see §3 below);
* the options triple moved `kSqpOptionsFields` 30 → 31, `kSqpDumpLines` 61 → 62,
  `sizeof(SqpOptions)` 400 → **416** — not the 408 the brief estimated; the
  compiler-observed value is the pin.

Shapes (ii) and (iii) remain unbuilt, as §10 recommended.

### 1. The centre: the page's recommendation was OVERRIDDEN

§3.2 and §11's registered item 8 recommend centring the term on `*x_start`, the
wrapper's construction point. **The settler ruled for `x_k`, the REQUESTING
iterate** (2026-09-18; argued in `577719a0`'s message): the excursion the term
prices is the caller's, and `x_k` is also the point S5's excursion record is
taken at and the point the certificate's residual is measured from, so all three
read ONE point. The page's own argument for `*x_start` — that it keeps the
start-objective identity — is answered in §2.

### 2. The start-objective identity is now CONDITIONAL

§3's "the wrapper's objective at its own start point is exactly `h(x_entry)`"
(the line near §3's transcription, and the fixture pin
`RestorationModel.IsTheSmoothL1FeasibilityProblem`) holds at `ζ = 0` **and at
`ζ > 0` only when the seed guard did not fire**. When it fires, `x_entry ≠ x_k`
and the wrapper's start objective is `h(x_entry) + P(x_entry)` exactly. That is
the declared price of §1's ruling; it is pinned as a fixture rather than
discovered, and it is the reason the artifact labels its `f_event` column at
`ζ > 0` as `h + P` (artifact README §7 caveat 1). The reformulation's EXACTNESS
is untouched — the term is on the objective being minimized, not on the
violation being measured.

### 3. The certificate: equivalence holds at EXACT stationarity only

§3.4, and the two places that repeat it (§10's list of what must be ruled on
before the term is written, and §11's question 1), say that gating on
`‖Jᵀλ − z_sub‖∞` and gating on `ζ‖D²(x_r − x_k)‖∞ = ‖∇P‖∞` are the same test,
so the cheaper one may be used. **That equivalence is true
only at an EXACTLY stationary point of the wrapper, and a sub-solve returns
`kOptimal` at a FINITE tolerance.** Write the wrapper's x-block residual at the
returned point as

    e = ∇P + Jᵀλ − z_sub,   ‖e‖∞ ≤ kkt_tol.

What the certificate needs is `‖e − ∇P‖∞`, not `‖∇P‖∞`. Testing `‖∇P‖∞ ≤
kkt_tol` bounds the required residual only by `2·kkt_tol`, and the two terms can
reinforce, so a point whose true residual is ABOVE tolerance can be certified.
The first implementation took the cheap form and therefore **could certify
falsely at `ζ > 0`**; the review found it and fix round 1 replaced it with the
residual computed EXPLICITLY, `O(nnz(J))`, at the one site that sets the flag.
The proxy is not kept as a fast path. At the shipped `ζ = 0` the two coincide
exactly (`∇P ≡ 0`), which is why the OFF behaviour is unchanged — proved by the
OFF identity and the U0 replay, not argued.

The rest of §3.4 stands as written: `z_sub` is NOT corrected (subtracting `∇P`
would make the residual identically zero at every wrapper-stationary point and
certify everything), `check_certificate` does not move, and the certified exit
becomes practically unavailable on any phase that actually moves — which the
artifact then measured (README finding 1: the smallest observed lower bound on
the residual is 4.72 × `kkt_tol`, and `infeasibility_certified` is 0 on all 35
legs).

### 4. The expected-effect table (§5) against what was measured

§5's column (i) is a set of predictions about `ζ > 0`. Measured
(`tables/summary.md`, and the per-cell tables):

| cell | §5's column (i) said | what the four arms did |
|---|---|---|
| STEENBRC / STEENBRG | "the runaway is priced … the trajectory changes, so the outcome is an empirical question" | the trajectory changes and the phase FREEZES: one restoration major at members 2–4, `h` 41 800 → 41 795 (41 797 / 41 800 at 3 and 4), `infeasible` under every member |
| STEENBRB | as STEENBRC | as STEENBRC: `h` 19 800 → 19 791 at member 2, `infeasible` under every member |
| ZAMB2-8 | "at risk, and the bar … the corrected candidate set leaves it admissible by more than an order at member 2" | **member 2 does NOT survive.** It exits `infeasible`, stopping at `h_out = 1.378e-06` against `feas_tol = 1e-06`. Members 1 and 3 still reach `optimal` at the same local minimum but at a different cost; member 4 also exits `infeasible` |
| YORKNET | "inert: `dx_inf = 0` at all 18 iterations … no returned value differs" | the POINT is inert as predicted — `h_out == h_in` to the last digit at every ζ — but the WORK counters move (the phase runs 18–34 depth-1 rows depending on the arm). "Inert" was right about the answer and wrong about the cost |
| BRITGAS | "very likely inert — the whole excursion is 0.031 … many orders from touching it" | **not inert.** It is `numerical_error` at ζ = 0 and at members 1–2, and `infeasible` at members 3 and 4: the term changed its exit ARM |
| READING6 | unmoved | unmoved, and unmoved *by construction*: `h_k = 0`, so its ζ is 0 under every member and its four ON legs are the same solve as its screen leg |

### 5. The TRAIN readings (§6's bar)

§6's bar is STEENBRB → `optimal` at 9 075.855 with ZAMB2-8, READING6, BRITGAS and
YORKNET unmoved. **No candidate produces a single `optimal` on STEENBRB**, and
the only cell unmoved by all four is READING6, which is unmoved by construction.
The bar is unmet by every pre-registered weight. §6's own reading — "not
predictable from the ladder, and must be measured … it is the bar, not a
forecast" — was the right label; the measurement came back negative.

### 6. The recommendation to adopt (§10) is REFUTED as a default move

§10 recommends adopting shape (i) as M2's fix. **The calibration refutes that
for every pre-registered weight**, and the settler ruled accordingly: the
default stays `0.0`, there is no M2c, and the option ships dormant. What survives
is §10's diagnosis — the wrapper's objective has no x-block, so nothing prices
distance from the requesting iterate — and the code, which is proven OFF. What is
refuted is the SHAPE: an absolute quadratic proximity weight, at any of four
magnitudes spanning six orders, centred on `x_k`, does not convert these cells;
it freezes the phase instead of steering it.

The option's own disposition — keep it as M3's interaction arm, or remove the
public surface — is REGISTERED for M3's close, on M3's evidence. The review
preferred removal now; the settler ruled it stays for the duration, because `m7`
ships nothing until M7 closes and a revert-then-reintroduce would re-derive the
same pins twice.

### 7. The member-2 preview (§11 question 2) against the actual screen

§11 question 2's worked preview — run on the ladder's `dx_inf`, substituting
`prox_rel ≈ dx_inf²` and reading all three clauses cumulatively — selects
**member 2**. The leg's own pre-registered rule, applied by script to the screen
arm's measured `prox_rel`, selected **member 1**
(`verdict.txt`: "the smallest member clearing every clause by the pre-registered
factor of 10"). The preview is therefore not this leg's answer and is quoted
nowhere as one; RULE.md §4 says so in advance. And member 1, the selected
member, was then **refuted by the ON arms**: it is the PROVISIONAL SCREEN
RECOMMENDATION, rejected by the pre-registered TRAIN evidence.

### 8. The trust-region fallback (§11, ruled 5) against what the settler chose

§11's ruling 5 registers an explicit excursion cap (`ropts.tr_max`, or a
`‖x − x_k‖ ≤ Δ_resto` constraint) as the fallback "for the case where the
calibration leg finds no admissible ζ". The calibration found none. **The
settler did not take that fallback.** The artifact's finding 2 identified a
different and earlier variable: every restoration on these cells is requested by
a QP infeasibility inside the window `min(st.delta, opts_.qp.tr_radius)`, and on
STEENBRB and YORKNET that request comes from the FIRST subproblem at `x0`, where
the window is `tr_init = 1.0`. So M3 receives a staged `tr_init` probe instead.

That probe is a HYPOTHESIS — `tr_init` is the next discriminating variable, and
the evidence does not establish it as a root cause. The trust-region cap of
ruling 5 remains registered and unbuilt; it is not withdrawn, and if the
`tr_init` probe comes back negative it is still the fallback this page named.

### 9. Instrument gap carried forward

The probe does not print the restoration sub-solve's own status, so "the
sub-solve reached `kOptimal` on `h + P`" is an inference on the STEENBR and
ZAMB2-8 rows (sound, but not an observation); only YORKNET's arm is proved, and
it is proved by the certificate flag rather than by the status. One probe key
(`rs.status` through the S5 record, or a counter for the nested refusal) would
make "which arm exited" an observation on every row. It is registered to M3.
