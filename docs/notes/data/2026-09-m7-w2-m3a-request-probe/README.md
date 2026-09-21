# M7 W2 M3a — the restoration REQUEST probe: is the initial trust radius what asks for restoration?

**Stage 1, the whole of it. Stages 2 and 3 were not run.**

Pre-registered rule: [`RULE.md`](RULE.md), committed at `dbf55675` **before any
leg ran**. Provenance: [`PROVENANCE.txt`](PROVENANCE.txt). Machine-written
verdict: [`verdict.txt`](verdict.txt). Raw legs: [`rows/stage1/`](rows/stage1).

---

## The result, in plain language

**The hypothesis is NOT SUPPORTED.** Raising the initial trust radius does make
the major-0 restoration request go away — on both cells, at every radius from
`1e2` up — but **on neither cell does that buy a better answer**, and on YORKNET
it makes the answer worse. Under `RULE.md` §5.2 a radius only counts as showing
the request was a window artefact if the request vanishes *and* the terminal
status holds or improves *and* the returned point is feasible. **No radius
satisfies all three on both cells**, so §5.5's failure path applies: this task
reports the table below and stops.

One radius does qualify on one cell. **STEENBRB at `tr_init = 1e3` satisfies
WINDOW_ARTEFACT as the rule is written**: the request is gone, the status holds
at `numerical_error` (which is STEENBRB's control status, and the bottom of the
rule's ordering, so "holds" is all it can do), and the returned point is feasible
to `3.4e-13`. It is a real cell of the table and is recorded as such. It is **not**
a qualifying radius for the task, because the rule requires both cells and
YORKNET gives none — and it is worth being plain about what it is: a solve that
still ends in `numerical_error`, at `f = 7.58e6`, having found *a* feasible point
rather than *the* answer.

On **YORKNET** nothing improves at any radius. At `tr_init` 10 and `1e2` the
phase still runs, just from a later major; at `1e3` and above no phase runs at
all and the terminal status *drops* from `infeasible` to `numerical_error` — a
status regression, which the rule calls HARM. The iterate never moves in **any**
YORKNET leg: `f` is `110670.29950761795` and the constraint violation is
`4099.28` at the start point and at the returned point, in all six legs.

**No default was changed by this task**, under any outcome — not `tr_init`, not
`restoration_proximity`, not `qp.tr_radius`, not `feas_tol`. That was the rule
before the legs ran and it is the rule after them.

The control-identity gate — the one thing that could have invalidated everything
below — **passed clean**: 38 shared keys and the full excursion ladder
byte-identical to M2b's committed screen rows on both cells, with `0` near-ulp
tolerances used ([`identity-stage1.txt`](identity-stage1.txt)).

---

## The verdict table

Machine-written from the rows; the full version with its predicate definitions is
[`verdict.txt`](verdict.txt).

| cell | tr_init | req0 | first QP (plain) | rest. entries | rest. sub-status | terminal status | max(h_e,h_i) | f | majors | minors | factorizations | fact/control | A1 | A2 | A3 | WINDOW_ARTEFACT | HARM |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| STEENBRB | 1 (control) | yes | `infeasible (inferred)` | 1 | optimal | numerical_error | 2.27374e-13 | 4.090860686e+12 | 5 | 20637 | 25973 | 1.0000 | — | — | — | — | — |
| STEENBRB | 10 | yes | `infeasible (inferred)` | 1 | optimal | numerical_error | 6.82121e-13 | 3.45990574e+12 | 5 | 20545 | 23421 | 0.9017 | NO | yes | yes | no | no |
| STEENBRB | 1e2 | no | `infeasible (inferred)` | 0 | (no phase) | numerical_error | 1599.6 | 604199.9831 | 5 | 20404 | 20346 | 0.7834 | yes | yes | NO | no | no |
| STEENBRB | 1e3 | no | `max_iter` | 0 | (no phase) | numerical_error | 3.41061e-13 | 7577124.911 | 6 | 20368 | 15618 | 0.6013 | yes | yes | yes | **YES** | no |
| STEENBRB | 1e4 | no | `max_iter` | 0 | (no phase) | numerical_error | 2000 | 2858.786135 | 4 | 18720 | 16 | 0.0006 | yes | yes | NO | no | no |
| STEENBRB | +inf | no | `max_iter` | 0 | (no phase) | numerical_error | 2000 | 2858.786135 | 4 | 18720 | 16 | 0.0006 | yes | yes | NO | no | no |
| YORKNET | 1 (control) | yes | `infeasible (inferred)` | 1 | infeasible | infeasible (cert=0) | 4099.28 | 110670.2995 | 1 | 50241 | 52180 | 1.0000 | — | — | — | — | — |
| YORKNET | 10 | no | `max_iter` | 1 | infeasible | infeasible (cert=0) | 4099.28 | 110670.2995 | 4 | 59033 | 61028 | 1.1696 | yes | yes | NO | no | **HARM (H2)** |
| YORKNET | 1e2 | no | `max_iter` | 1 | infeasible | infeasible (cert=0) | 4099.28 | 110670.2995 | 4 | 23342 | 28513 | 0.5464 | yes | yes | NO | no | no |
| YORKNET | 1e3 | no | `max_iter` | 0 | (no phase) | numerical_error | 4099.28 | 110670.2995 | 4 | 10240 | 20470 | 0.3923 | yes | NO | NO | no | **HARM (H1)** |
| YORKNET | 1e4 | no | `max_iter` | 0 | (no phase) | numerical_error | 4099.28 | 110670.2995 | 4 | 10240 | 20470 | 0.3923 | yes | NO | NO | no | **HARM (H1)** |
| YORKNET | +inf | no | `max_iter` | 0 | (no phase) | numerical_error | 4099.28 | 110670.2995 | 4 | 10240 | 20466 | 0.3922 | yes | NO | NO | no | **HARM (H1)** |

`req0` is `RULE.md` §2.2's predicate: a phase ran **and** it was entered in major
0. YORKNET at 10 and `1e2` shows `restoration entries = 1` with `req0 = no`
because the phase was entered from a later major (`x_k_source = depth0_event`) —
see [`tables/stage1-restoration-record.txt`](tables/stage1-restoration-record.txt).

`first QP (plain)` is `RULE.md` §2.1's derived key. `(inferred)` is part of the
printed value: on a major the elastic tier answered, the driver overwrites the
history row's `qp_*` fields with the elastic solve's, so the plain solve's own
status is not on the public surface. The inference — elastic tier ran ⇒ the plain
QP returned `kInfeasible` — holds because at `QpMode::kWalk` the elastic tier is
the driver's only consumer of `QpStatus::kInfeasible`, and its precondition
(`elastic_from_ipqp_escape == 0`) is **checked and printed on every leg**, not
assumed.

Every leg: `deadline_fired = 0`, `rc = 0`. **No leg was TRUNCATED and none was
CENSORED**, so no verdict in the table is UNDETERMINED.

---

## What this does and does not show

**It shows**, as counters and statuses:

- The major-0 restoration request on these two cells **is** tied to the l∞ window.
  At `tr_init ≥ 1e3` it is gone on both cells and no restoration phase runs at
  all. On STEENBRB the *parent* QP's own status changes with the window too —
  from `kInfeasible` (inferred, at `tr_init` 1, 10 and `1e2`) to `kMaxIter` at
  `1e3` and above.
- Removing the window **does not** give either cell a better answer. STEENBRB
  ends `numerical_error` at every radius; YORKNET's status *falls* from
  `infeasible` to `numerical_error` at `1e3` and above.
- The counters move a long way even where the answer does not: STEENBRB's
  factorizations run 25973 → 16 across the ladder; YORKNET's 52180 → 20466.
- `restoration_sub_status` (M3a task A's new field) closes M2b finding 1's
  inference gap on these legs: on STEENBRB the feasibility sub-solve itself
  returns **`optimal`** and drives `h` from 19800 to ~1e-12 — and the solve then
  dies anyway, at `f ≈ 4.09e12`. On YORKNET the sub-solve itself returns
  **`infeasible`** and returns the point it was handed unchanged (`h_in == h_out`,
  `f_in == f_out`). Those are two different failures that M2b could not tell apart.

**It does not show**:

- Anything about the other five M2b ladder cells (STEENBRC, STEENBRG, ZAMB2-8,
  BRITGAS, READING6). Stage 2 was the leg that would have, and it did not run.
- Anything about the proximity term. ζ = 0 on all 12 legs; stage 3 did not run.
- A *cause*. The rule tested one hypothesis at six radii on two cells and the
  hypothesis failed. What actually breaks these two solves is the next question,
  not this one's answer — see the lead below.
- Any timing claim. Wall-clock appears in `logs/chain1.log` marked
  `(INFORMATIONAL)` and supports nothing here.

**A caveat on the ordering, stated plainly.** `RULE.md` §5.1 puts
`numerical_error` at rank 0, the floor. STEENBRB's control *is* `numerical_error`,
so on STEENBRB clause (A2) — "the status improves or holds" — can never fail; it
is satisfied by every leg including the ones that clearly made things worse
(`1e4` and `+inf`, which return the start point untouched). (A3) is what does the
discriminating on that cell. This is the rule as pre-registered and it was applied
as written; it is recorded here because a reader of the STEENBRB `1e3` row should
know that "A2 = yes" carries less information on a cell whose control is already
at the floor than it does on YORKNET, whose control sits one rank above it.

---

## The lead these rows open: the first QP subproblem does not solve

With the window removed, the solve does not die in restoration — **there is no
restoration** — it dies on QP subproblem failures. The counters are in
[`tables/stage1-qp-counters.txt`](tables/stage1-qp-counters.txt). Observed and
inferred are labelled throughout.

**OBSERVED (public counters, printed verbatim by the probe):**

- At `tr_init ≥ 1e3` on both cells the first QP's plain status is **`max_iter`**
  with `elastic_applied = 0` — no inference involved, this is
  `history[0].qp_status` itself.
- `qp_failures_in_a_row_peak = 3` — the shipped budget, exactly reached — on
  **eleven of the twelve legs**. The one exception is the YORKNET control, whose
  peak is 0: at `tr_init = 1` YORKNET never routes a QP failure at all, it enters
  restoration in major 0 and stops after one major.
- `qp_maxiter_exits = 4` on every STEENBRB leg and on YORKNET at `tr_init ≥ 1e3`
  (YORKNET is 3 at 10 and `1e2`, and 0 at the control). `qp_failures_routed` is 3
  wherever the peak is 3, except STEENBRB at `1e3`, which routes 4 across two
  runs.
- `steps_accepted = 0` on both cells at `1e4` and `+inf` and on YORKNET at every
  radius: the iterate **never moves**. The depth-0 ladder rows in
  `rows/stage1/*.txt` show it directly — STEENBRB at `+inf` holds
  `f_main = 2858.7861353626763`, `h_main = 19800` (an l1 violation; the
  inf-norm equality residual at the same point is the table's `2000`) across all
  four rows, and YORKNET at `+inf` holds `f_main = 110670.29950761795`,
  `h_main = 48491.383947016489` across all four.
- `qp_minor_iters` is exactly `4 × 4680 = 18720` on STEENBRB at `1e4`/`+inf` and
  exactly `4 × 2560 = 10240` on YORKNET at `1e3`/`1e4`/`+inf`, with
  `qp_maxiter_exits == major_iters` and no restoration and no elastic minors
  mixed in. Every major in those legs spent its entire minor budget.
- `elastic_activations` collapses with the radius: STEENBRB 1, 1, 1, 0, 0, 0 and
  YORKNET 19, 20, 2, 0, 0, 0 down the ladder. Above `1e2` the elastic tier is
  never reached on either cell, because the parent QP returns `kMaxIter` rather
  than `kInfeasible` and `kInfeasible` is the tier's only trigger.
- The factorization/minor ratio splits sharply by cell. STEENBRB at `+inf`:
  18720 minors on **16** factorizations. YORKNET at `+inf`: 10240 minors on
  **20466** factorizations — more than two per minor. `symbolic_analyses` is 1 in
  both.

**INFERRED (from the public headers, not from a counter):**

- The per-solve minor cap in force is the shipped size-derived default:
  `QpOptions::max_iter = 0` is the sentinel, and
  `detail::effective_qp_max_iter` resolves it to
  `max(kQpMaxIterFloor, kQpMaxIterCoeff × (n + mi + #bounded))` with
  `kQpMaxIterFloor = 500`, `kQpMaxIterCoeff = 5`
  (`include/hven/detail/qp/qp_engine.h:385-409`,
  `include/hven/qp/qp_types.h:117-138`). The observed per-major quotients above
  give **4680 on STEENBRB** (⇒ base 936 = n 468 + mi 0 + 468 bounded) and
  **2560 on YORKNET** (⇒ base 512 = n 312 + mi 0 + 200 bounded). `#bounded`
  counts the *caller's real* bounds, explicitly **before** the trust-region
  window (the code says so, and says why), so the cap is the **same at every
  radius** — it does not move with `tr_init`. The quotients are observed; the
  decomposition into `n + mi + #bounded` is the inference, and `#bounded` is not
  printed by the probe.
- The solve ends where it does because the **consecutive-failure budget** runs
  out: `SqpOptions::qp_failure_budget = 3` by default
  (`include/hven/drivers/sqp_solver_types.h:717-767`), and
  `qp_failures_in_a_row_peak = 3` on every one of those legs. Three capped
  subproblems in a row at one iterate, then the fourth ends the solve. The
  driver reports a capped subproblem and a broken one as the same
  `SolveStatus::kNumericalError` (the header says this in as many words), which
  is why these legs read `numerical_error` while nothing arithmetic actually
  failed — `qp_numerical_exits = 0` on STEENBRB at `1e4`/`+inf` and on YORKNET at
  `1e3`/`1e4`/`+inf`. The one leg where a subproblem did fail arithmetically is
  STEENBRB at `1e3`: `qp_numerical_exits = 1` there, alongside 4 cap exits.
- **So the shape of the failure at `tr_init ≥ 1e3` is: subproblem runs out of
  minors → routed as a failure → radius shrunk → same thing again → budget
  exhausted at three → solve ends.** The four depth-0 ladder rows, with the
  radius halving down each one and the iterate never moving, are that sequence.
  This is an inference from the counters plus the two headers; no counter says
  "budget exhausted" in those words.

**What this lead is not.** It is not a claim that raising the cap would solve
these cells — `qp_cap_retry`'s own header is explicit that "raising the cap never
converts a wrong answer into a right one, it only stops refusing subproblems that
were going to finish". A capped subproblem at a huge radius may simply be a
subproblem with no useful answer in it. The experiment below is designed to tell
those two apart, and nothing in stage 1 decides it.

---

## Proposed next discriminating experiment (for the settler to rule on)

`RULE.md` licenses no further legs — §5.5 says this task stops — so none were
run. These are **proposals**, in cost order.

### E1 (cheapest, 2 legs): does the first subproblem finish if it is given the minors?

hven already ships the exact knob: `SqpOptions::qp_cap_retry` (default `false`)
re-solves a `kMaxIter` subproblem **once, at twice the cap**, warm from the capped
solve's own working set, and `SqpCounters::qp_cap_retries` counts the attempts
(`sqp_solver_types.h:770-820`). Turning it on at `tr_init = +inf` asks precisely
the discriminating question: *is the subproblem one that was going to finish?*

Requires one probe change: an `--qp-cap-retry on|off` argument setting that one
bool, nothing else. Then, under the identical terms (alone, under
`/tmp/box-build.lock`, `taskset -c 2`, `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`,
300 s deadline inside `timeout 900`):

```
./m3a_probe --problem STEENBRB --tr-init inf --qp-cap-retry on --deadline 300
./m3a_probe --problem YORKNET  --tr-init inf --qp-cap-retry on --deadline 300
```

Read-out: `qp_cap_retries`, `qp_maxiter_exits`, `steps_accepted`, terminal status.
If the retry converges and the major proceeds, the cap is the binding constraint
at that radius. If both retries hit the doubled cap too, it is not, and the
subproblem at an unbounded radius is genuinely unanswerable by the walk. Either
answer is worth having and neither costs more than two legs.

*Note for whoever writes the rule for it:* `qp_cap_retry` is an option the M1
reading already measured on its own arm; this would be a second reading of it at
a radius M1 never ran. It is a measurement, not a default proposal.

### E2 (2 legs, an alternative to E1's mechanism): an explicit absolute cap

`QpOptions::max_iter > 0` is an explicit, absolute cap that "wins outright, at any
size". A single leg per cell at, say, `10 ×` the derived cap separates "needs more
minors" from "will never finish" without the retry's working-set seeding in the
way. Also needs one probe argument (`--qp-max-iter N`).

```
./m3a_probe --problem STEENBRB --tr-init inf --qp-max-iter 46800 --deadline 300
./m3a_probe --problem YORKNET  --tr-init inf --qp-max-iter 25600 --deadline 300
```

E1 and E2 answer nearly the same question; E1 is the one the engine already
ships a counter for, E2 is the blunter instrument. **Running both is not worth
four legs — pick one.**

### E3 (the intermediate radius, 2 legs): where the first QP's status flips

On STEENBRB the plain first QP is `kInfeasible` at `1e2` and `kMaxIter` at `1e3`;
on YORKNET it flips between `1` and `10`. Two legs at an intermediate radius each
would locate the flip. **Lowest value of the three** — it refines a boundary in a
hypothesis the rule has already recorded as not supported, and RULE.md §5.5
explicitly forbids re-testing this hypothesis at other radii, so it would have to
be opened as a *different* question (about the QP's status transition, not about
the restoration request) under its own rule.

### Not proposed

Stage 2 as written (the five remaining cells at a qualifying radius). There is no
qualifying radius, and running it anyway would be exactly the post-hoc widening
`RULE.md` §5.5 forbids.

---

## Addendum E — the E legs: does the first QP finish if it is given the minors?

**The result, in plain language: no, it does not — and the rule does not let us
say so in its own word.** Given ten times the minor budget stage 1 refused it,
the first QP subproblem on both cells still came back `max_iter`, still moved the
iterate not at all, and the solve still ended the same way. But both of those
legs ran into the 300 s solver deadline, and `RULE-E.md` §5 says a TRUNCATED leg
yields **no verdict** — so `REFUTED`, the label those two legs would otherwise
have carried, is withheld. The two legs that *did* finish (the `qp_cap_retry`
arm, at twice the cap) are simply **not SUPPORTED**: same `max_iter`, same
`steps_accepted = 0`.

So the recorded outcome is: **H-QP is not supported on these cells at these caps,
and it is not refuted either.** The addendum stops there — no larger cap, no
other cells, no wider budget, no relaxed predicate (`RULE-E.md` §5's failure
path). **No default is changed**, and a SUPPORTED verdict would not have licensed
one either.

This is a *different* hypothesis from stage 1's. The radius hypothesis stays
CLOSED by `RULE.md` §5.5, and [`verdict.txt`](verdict.txt) and "The verdict
table" above stand exactly as committed at `6bc75ee9`.

### What ran

Five legs, one at a time, each its own hold of `/tmp/box-build.lock`, pinned to
one physical core at `MKL_NUM_THREADS=1`, all at `--tr-init inf` with ζ = 0 and
exactly one option off its default:

| leg | argument | outcome |
|---|---|---|
| `STEENBRB@identity` | (both new options at their defaults) | the §3 gate — **IDENTICAL** to the committed `rows/stage1/STEENBRB@trinf.txt` |
| `STEENBRB@E1` | `--qp-cap-retry on` | complete, not SUPPORTED |
| `YORKNET@E1` | `--qp-cap-retry on` | complete, not SUPPORTED |
| `STEENBRB@E2` | `--qp-max-iter 46800` | TRUNCATED — counters reported, no verdict |
| `YORKNET@E2` | `--qp-max-iter 25600` | TRUNCATED — counters reported, no verdict |

The identity leg is the gate on the *rebuilt* probe: it gained two arguments and
five printed keys, and a leg with neither argument had to reproduce stage 1
before any E leg was allowed to run. It did — 50 shared keys and the whole
excursion ladder byte-identical, `NEAR_ULP_TOLERATED=0`, the only new keys being
the five `RULE-E.md` §3 excluded by name ([`identity-e.txt`](identity-e.txt), and
the fold re-checks it independently in [`verdict-E.txt`](verdict-E.txt)).

Machine-written read-outs: [`verdict-E.txt`](verdict-E.txt),
[`tables/e-first-qp.txt`](tables/e-first-qp.txt),
[`tables/e-solve-counters.txt`](tables/e-solve-counters.txt). Raw rows in
[`rows/e/`](rows/e), transcript in [`logs/chainE.log`](logs/chainE.log). All five
stderr files were empty and are not committed.

### The observation the verdict does not carry

**OBSERVED**, from the two TRUNCATED legs' own `history[0]` fields — which were
**complete before the deadline fired**, and the row says so rather than us
assuming it (`first_qp_solved = 1`; `first_qp_minor_iters` equal to the cap
*exactly*, i.e. the subproblem ran its whole budget and returned; a later major
present in `history`):

- STEENBRB@E2: first QP `max_iter` after **46 800** minors — 10 × its own derived
  per-major cap of 4680 — on **5** factorizations.
- YORKNET@E2: first QP `max_iter` after **25 600** minors — 10 × 2560 — on
  **51 186** factorizations.

and from the two legs that are not truncated at all:

- STEENBRB@E1 and YORKNET@E1: first QP `max_iter` at **2 ×** the cap (9360 and
  5120), with `qp_cap_retries = 4` and `qp_maxiter_exits = 4` on both. By
  `RULE-E.md` §4's accounting — cap exits are counted at the routing site, so a
  retry that *rescued* a subproblem would not appear there — those two equal,
  non-zero counters read as: every capped subproblem was retried at twice the
  budget and hit the doubled cap too. **No retry rescued anything.**

`steps_accepted = 0` on all four legs: the iterate never moves, on either cell,
at any of these caps.

Wall-clock is informational and supports nothing here: it is in
`logs/chainE.log` marked `(INFORMATIONAL)` and is quoted as a measurement
nowhere. (The E2 legs' processes outlived the 300 s solver deadline, which the
probe enforces in its iteration callback and which therefore fires only when a
callback is reached.)

### The new lead: why does an active-set walk spend 46 800 minors and never converge?

This is the question the E legs hand on. Everything below is from the **public
headers** and from the harness's own cost census; observed and inferred are
labelled, and no source outside those was read.

**DOCUMENTED (the header says it in as many words).** `qp_engine.h`'s own
"Degeneracy" section, `include/hven/detail/qp/qp_engine.h:333-338`:

> Linearly dependent working sets do not break the loop: the delta/mu
> regularization keeps the KKT matrix factorizable, and the dependent rows split
> the multiplier between them. **No anti-cycling rule is implemented; a
> degenerate stall is bounded by `max_iter`.**

That is the shipped contract, and it is a complete explanation of the *shape* we
see — a walk that runs to its cap — without being evidence that this particular
walk is stalling rather than working.

**DOCUMENTED, on the nonconvex side.** The walk does *not* assume a convex QP,
and it is not silently mis-handling an indefinite Hessian: `qp_engine.h:8-16`
states "`H` need not be positive semidefinite", §4b (`:129-176`) is an **inertia
gate** that refuses to certify `kOptimal` at a second-order-inconsistent point
(reporting `kNumericalError` instead) and escalates `primal_delta` a decade at a
time on a suspect reading, and §4c (`:177-208`) is a **negative-curvature ride**
to the nearest blocking constraint, with an engine-internal `kUnbounded` outcome
when nothing blocks. There is no `QpStatus::kUnbounded`
(`include/hven/core/solver_status.h:20-26` has exactly `kOptimal`, `kMaxIter`,
`kInfeasible`, `kNumericalError`); an unbounded-looking answer is folded into
`kNumericalError` by the `unbounded_artifact_scale()` guard
(`qp_engine.h:210-226`, `:424-435`). **None of that fires here**: the first QP
returns `kMaxIter`, not `kNumericalError`, on every E leg.

**WHAT THE SURFACE CANNOT SHOW, and this is the finding of the lead.** The
counters that would discriminate *cycling / degenerate stalling* from *a long but
genuine walk* exist — `degenerate_steps`, `degenerate_run_max`, `drop_ties`,
`ratio_ties`, `ws_adds`, `ws_drops`, `schur_updates`
(`include/hven/core/solver_counters.h:111-200`, `:33`) — but every one of them
lives on **`QpCounters` only**. `solver_counters.h:988-993` aggregates just
`qp_minor_iters` and `factorizations` onto `SqpCounters` (plus
`eqp_refine_steps`, `border_refine_steps`, `verdict_refine_steps`,
`suspect_escalations`, `symbolic_analyses`, re-declared at `:1128-1164`), and
`SqpIterate` carries only `qp_minor_iters` / `qp_factorizations` per row
(`include/hven/drivers/sqp_solver_types.h:1061-1063`). **From `SqpResult` alone —
which is all this probe can see — "many genuine EQP re-solves" and "cycling
through ties and zero-length pivots" are indistinguishable.** That is a
measurement gap, not a null result.

**The 5-vs-51 186 factorization contrast, OBSERVED.** At the same cap and the
same QP status, STEENBRB@E2 spends 5 factorizations over 46 800 minors while
YORKNET@E2 spends 51 186 over 25 600 — 2.0 per minor. The rows carry a second
column that splits the two the same way: `border_refine_steps` is **843 016** on
STEENBRB@E2 and **9** on every YORKNET leg. So on STEENBRB the Schur border stack
is carrying the walk, and on YORKNET it is essentially never used.

**INFERRED**, using the harness census's vocabulary
(`/home/ghecht/Projects/hven-bench/docs/w2-c1-sqp-cost-census.md`, read-only;
its chain B covers STEENBRB but **never ran YORKNET**, never measured YORKNET's
pin peak, and never varies `tr_radius` at all — so it does not contain these
numbers and cannot be cited as explaining them). The census's regimes are: pin
peak above `schur_cap = 128` ⇒ bordering "unavailable at any price" and
≈1 factorization per minor; pin peak far under ⇒ ≪1; and one intermediate,
conditioning-guard-driven regime at ≈1.78 (READING6, fixed by relaxing
`schur_cond_max`). Against that:

- STEENBRB's census pin peak is **337**, far above the cap, and its census
  default arm refactorizes ≈1.00 per minor — yet **at `tr_init = inf` the same
  cell spends 5 factorizations on 46 800 minors**. The natural inference is that
  the trust-region window's own bound rows are a large part of what pins
  STEENBRB past `schur_cap`: remove the window and the pin count falls under the
  cap, so bordering becomes available and the walk runs almost refactorization-
  free. **This is an inference.** `pinned` is not printed by any counter this
  probe can reach, and the census itself lists "the depth at each refactorization
  trigger" and "which of the four triggers fires" among its never-counted open
  questions.
- YORKNET at ≈2.0 factorizations per minor with `border_refine_steps = 9` matches
  **none** of the census's three regimes (it is above even READING6's 1.78), and
  YORKNET's pin peak has never been measured. Candidates, all uninstrumented from
  here: the pin-count latch, the depth test `dim() > schur_cap`, the conditioning
  guard `schur_cond_max`, or `nearly_singular()`. **Not guessed at further.**

### The single cheapest next discriminating measurement (the settler rules; nothing was run)

**Two legs, no new solver behaviour, no hven source change** — a three-line
addition to this directory's own probe so it prints the three reachable
`SqpCounters` fields it does not currently print (`suspect_escalations`,
`eqp_refine_steps`, `verdict_refine_steps`), then the two `@trinf` controls
re-run under the same terms:

```
# after adding the three kvi() lines to wrappers/m3a_probe.cpp and rebuilding
# it against the SAME install (the chain refuses a library-sha mismatch):
flock /tmp/box-build.lock env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 \
  taskset -c 2 timeout 900 \
  /home/ghecht/Projects/hven/.scratch/m7w2m3a/probebuild/m3a_probe \
  --problem STEENBRB --tr-init inf --deadline 300
flock /tmp/box-build.lock env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 \
  taskset -c 2 timeout 900 \
  /home/ghecht/Projects/hven/.scratch/m7w2m3a/probebuild/m3a_probe \
  --problem YORKNET  --tr-init inf --deadline 300
```

(run from `/home/ghecht/Projects/hven-bench`, which stays read-only; each leg its
own lock hold, `pgrep` guard first, a fresh pre-registered rule before it runs.)

What it discriminates: `suspect_escalations > 0` says the inertia gate's
suspect-stall ladder is firing inside these subproblems — the walk is repeatedly
re-regularizing a KKT system whose inertia it cannot read, which is a mechanism
for the minors; `suspect_escalations == 0` rules that mechanism out and leaves
the documented `qp_engine.h:333-338` gap — a degenerate stall bounded only by
`max_iter` — as the standing candidate. It is cheap because both legs are the
already-measured `@trinf` configuration (stage 1 ran them in 31.6 s and 97.3 s of
informational wall) and neither can truncate.

**What it will not settle**, and should be said in the same breath: the
decisive counters (`degenerate_steps`, `degenerate_run_max`, `drop_ties`,
`ratio_ties`, `ws_adds`/`ws_drops`, `schur_updates`, and the pin depth) are not
reachable from `SqpResult` at all. Making them reachable — aggregating them onto
`SqpCounters`, or mirroring them onto the `SqpIterate` row — is **engine work for
the settler to rank**, not a measurement, and this addendum proposes no such
change.

---

## Addendum F — the F legs: what was the walk DOING for those minors?

Addendum E left one number standing: the first QP subproblem consumed 4680 and
2560 minor iterations at the shipped cap on STEENBRB and YORKNET, ten times that
at the raised cap, and came back `max_iter` every time. Addendum F asks what
those minors were spent on.

**The rule was committed first**, at `229d63de`, before any leg including the
identity leg: `RULE-F.md`. Its hypothesis:

> **H-DEG.** The first QP's tens of thousands of minor iterations on STEENBRB
> and YORKNET at `tr_init = inf` are a DEGENERATE STALL of the active-set walk,
> not a long genuine walk.

hven documents both the mechanism and the absence of a remedy: "Linearly
dependent working sets do not break the loop... **No anti-cycling rule is
implemented; a degenerate stall is bounded by max_iter**"
(`include/hven/detail/qp/qp_engine.h`, the `--- Degeneracy ---` note).

### The instrument, and the correction to what addendum E said about it

Addendum E's closing sections recorded that `QpCounters::schur_updates` and the
whole degeneracy set are "not reachable from `SqpResult`". **That is true and
stays true** — `SqpCounters` genuinely does not carry those fields, and nothing
in `verdict-E.txt` is wrong.

What it missed is a SECOND public surface. `hven::solvers::Ledger`
(`include/hven/core/ledger.h`) is a public, read-only instrument;
`SqpSolver::attach_ledger` is a public method that also forwards the same Ledger
to the driver's own `QpEngine`; and `Ledger::records()` hands back one
`SolveRecord` **per QP-engine solve**, each carrying a whole `QpCounters`. The
harness's own M7 W2 C1 cost census already read the degeneracy set this way.

**Its scope, pre-registered** (`RULE-F.md` §1.2): `attach_ledger` attaches to the
driver's MAIN WALK ENGINE and to nothing else. The restoration phase's
sub-driver receives `attach_trace` but no `attach_ledger` (ledger.h says so), and
the four lazily-built engines are never handed it. So a record count is a LOWER
BOUND on the solve's QP solves — which matters on the `tr_init = 1` legs, where a
restoration phase runs.

**All twenty `QpCounters` fields are printed on every record**, in declaration
order. Two quantities the ruling named do not exist on that struct and are
recorded ABSENT rather than substituted for: there is **no negative-curvature
field** (the ride's own blocking constraint is folded into `ws_adds`, by that
field's own doc) and **no inertia field** (`IpqpCounters`' is a different struct
describing a tier that does not run here). `suspect_escalations` is the one
escalation counter and is printed like the rest.

### The two gates, and the one that mattered

The probe gained one argument, `--ledger on|off`, **default off** = addendum E's
behaviour exactly (no `Ledger` is constructed and `attach_ledger` is never
called). Two different things had to be proved:

- **IDENTITY** — `--ledger off` reproduces the committed
  `rows/stage1/STEENBRB@trinf.txt` on all 50 shared keys and the whole ladder,
  `NEAR_ULP_TOLERATED=0`. The new argument is inert at its default.
- **NEUTRALITY** — each `--ledger on` leg equals **its own** committed stage-1
  twin on every shared key (50 on the `@trinf` arms, 52 on the `@tr1` arms),
  ladder included, 0 differing. **No solver counter moved with the ledger
  attached.** The first measured leg's check gated the other three, so the STOP
  would have been a stop rather than a note appended afterwards.

### The verdict: MIXED on both cells

`RULE-F.md` §4 fixed, before any leg: DEGENERATE_STALL iff
`degenerate_steps / minor_iters >= 0.5` **and** `degenerate_run_max >= 1000`;
GENUINE_WALK iff the ratio `< 0.1`; otherwise MIXED.

| cell | first QP at `tr_init = inf` | minors | degenerate | ratio | run max | verdict |
|---|---|---:|---:|---:|---:|---|
| STEENBRB | `f_qp_0` `max_iter` | 4680 | 2319 | 0.495513 | 2310 | **MIXED** |
| YORKNET | `f_qp_0` `max_iter` | 2560 | 1257 | 0.491016 | 1191 | **MIXED** |

Both ratios sit just **below** the 0.5 bar while both run maxima sit far
**above** the 1000 bar, so DEGENERATE_STALL's conjunction is false and
GENUINE_WALK's 0.1 is nowhere near met. The rule's third row governs. Nothing is
relabelled to reach a cleaner word, and **no verdict licenses a default change**
— none was made.

### Outside the verdict

`verdict-F.txt` carries a clearly separated section that licenses no verdict and
no default change. In brief:

**The rule's 0.5 bar was set on a denominator that cannot reach it**, a design
error the settler declared on reading the rows. Checked against the headers
rather than assumed: `minor_iters` increments once per pass of the walk loop
(`src/qp/qp_engine.cpp:613`, fixed as a relied-on contract by `qp_engine.h`'s
COUNTER SEMANTICS note), while `degenerate_steps` counts only passes that ADDED a
blocking constraint, and `degenerate_run_max` is not broken by a drop iteration
at all. In a pure add/drop alternation half the passes are drops, so the ratio
cannot exceed about 0.5 by construction. From the rows:
`ws_adds + ws_drops` = 4667 vs 4680 minors on STEENBRB, 2519 vs 2560 on YORKNET.

**The descriptive numbers the rule pre-registered as non-verdict read-outs.**
`degenerate_steps / ws_adds` is 0.9893 and 0.9767. The net working-set change is
+21 and +55 over thousands of admissions. One unbroken degenerate run covers 2310
of 2319 and 1191 of 1257 degenerate steps. The loudest number is admission
multiplicity: **23 distinct variables admitted 2344 times** on STEENBRB, 95
admitted 1287 times on YORKNET. Machine-checked, not asserted:
`ws_adds == ws_adds_bound` on all 22 records and every leg prints `mi = 0`, so
every admission is a bound pin.

**The later majors repeat it.** YORKNET's records 1–3 are BYTE-IDENTICAL to one
another on all nineteen `QpCounters` fields but `symbolic_analyses` — three
consecutive majors performing the same stall, with **two** distinct variables
admitted 1279 times each.

**The contrast with `tr_init = 1`**, same cells, same binary, same terms: the
first QP ends `infeasible` after 827 / 397 minors with a longest degenerate run
of 3 / 48, against `max_iter` after 4680 / 2560 with runs of 2310 / 1191.

**What would confirm cycling proper** — the same working SET revisited — and why
admission multiplicity does not: it is equally consistent with a long degenerate
crawl over many sets drawn from the same few variables. Checked, not assumed:
`QpEngine` has no trace sink at all, and its only instrument is the per-SOLVE
Ledger record this addendum read, so the finest public QP granularity is the
subproblem and never the minor. Confirming it needs a registered instrument — an
`HVEN_TESTING`-gated per-minor working-set signature on CLAUDE.md §8's test-seam
convention — or an offline replay of one subproblem. Either is its own task with
its own pre-registered rule.

**What this ranks for the settler, deciding nothing.** An **anti-cycling rule for
the active-set walk** is the lead candidate, from the OPEN literature only
(Bland's, or a perturbation / EXPAND-style scheme) and **never from SNOPT
source**. Start/crash quality stays live on the `tr_init = 1` contrast. Cap policy
ranks last: addendum E already showed a 10× cap changes nothing, and the stall is
not a budget shortage.

## Files

```
RULE.md                             the pre-registered rule (dbf55675, before any leg)
PROVENANCE.txt                      toolchain, hardware, date, commit, sha256s, terms
verdict.txt                         RULE.md section 5 applied -- machine-written
identity-stage1.txt                 the control-identity gate (RULE.md section 4)
rows/stage1/<CELL>@<tag>.txt        the 12 raw legs, unedited
                                    (all 12 stderr files were empty and are not committed)
logs/chain1.log                     the chain transcript, lock markers and wall (informational)
tables/stage1-qp-counters.txt       the QP-subproblem counters -- machine-written
tables/stage1-restoration-record.txt the S5 record incl. restoration_sub_status -- machine-written
wrappers/m3a_probe.cpp              the instrument
wrappers/probe-CMakeLists.txt       the install-only build (the harness CMake is never entered)
wrappers/chain1.sh                  the stage-1 chain
wrappers/identity_m3a.py            the control-identity checker
wrappers/summarise_m3a.py           the fold that writes tables/, verdict.txt and verdict-E.txt
RULE-E.md                           addendum E's pre-registered rule (ade36329, before any E leg)
verdict-E.txt                       RULE-E.md section 5 applied -- machine-written
identity-e.txt                      the rebuilt probe's identity gate (RULE-E.md section 3)
rows/e/<CELL>@<tag>.txt             the 5 raw E legs, unedited
                                    (all 5 stderr files were empty and are not committed)
logs/chainE.log                     the E chain transcript, lock markers and wall (informational)
tables/e-first-qp.txt               the first QP subproblem, leg by leg -- machine-written
tables/e-solve-counters.txt         the whole-solve counters, leg by leg -- machine-written
wrappers/identity_e.py              the addendum-E identity checker
wrappers/chainE.sh                  the E chain
RULE-F.md                           addendum F's pre-registered rule (229d63de, before any F leg)
verdict-F.txt                       RULE-F.md section 4 applied, plus the outside-the-verdict
                                    section -- machine-written
identity-f.txt                      the identity gate (RULE-F.md section 3.1)
neutral-first-f.txt                 the gating leg's neutrality check (section 3.2)
neutrality-f.txt                    all four measured legs' neutrality check
rows/f/<TAG>.txt                    the 5 raw F legs, unedited, each with its LEDGER block
                                    (all 5 stderr files were empty and are not committed)
logs/chainF.log                     the F chain transcript, lock markers and wall (informational)
tables/f-first-qp.txt               record 0 of every measured leg, all 20 QpCounters fields
tables/f-records.txt                EVERY ledger record of every measured leg
tables/f-neutrality.txt             the two checks, leg by leg -- machine-written
wrappers/identity_f.py              the addendum-F identity AND neutrality checker
wrappers/chainF.sh                  the F chain
gates/                              task A's six-gate chain (committed at 3caf8d50)
```
