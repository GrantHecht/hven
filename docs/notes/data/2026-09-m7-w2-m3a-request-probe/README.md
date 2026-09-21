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
wrappers/summarise_m3a.py           the fold that writes tables/ and verdict.txt
gates/                              task A's six-gate chain (committed at 3caf8d50)
```
