# M7 W2 M2b — the restoration proximity term's ζ calibration: the screen, the four candidate arms, and the ruling

## THE RULING: THE TERM STAYS OFF. THERE IS NO M2c.

**The settler's ruling on this artifact's result, stated first because everything
below is read against it.**

`SqpOptions::restoration_proximity` ships at `0.0`, as `577719a0` landed it, and
**no default event follows**. The pre-registered screen (RULE.md §2) named a
candidate — member 1, `ζ = h_k/(n·ρ)` — and **the candidate arms contradict the
screen**: under every one of the four weights the term does not price the
excursion, it *freezes* it, and the page's TRAIN bar is unmet by all four.

* **STEENBRB, STEENBRC, STEENBRG exit `infeasible` under EVERY candidate.** The
  violation the phase was asked to remove barely moves: `h` 19 800 → 19 791
  (member 2 on STEENBRB), 41 800 → 41 795 (members 2–4 on C and G), 41 800 →
  40 550 at member 1; `f` stays bounded where the ζ = 0 run blew it to 1.8e14;
  the phase runs 7 restoration majors at member 1 and exactly 1 at members 2, 3
  and 4; `infeasibility_certified` is 0 on all twelve rows. The resume gate
  `h_r <= feas_tol` (`src/drivers/sqp_solver.cpp:4717`) never fires.
* **ZAMB2-8, the cell the bar says must be UNMOVED, moves under all four.** At
  member 1 and member 3 it still reaches `optimal` at the same local minimum
  (finding 3 below), but with a different restoration exit and a different
  cost; at members 2 and 4 it exits `infeasible`, member 2 stopping at
  `h_out = 1.378e-06` against a `feas_tol` of `1e-06`.
* **The TRAIN bar (the design page §5, restated in RULE.md §6) is STEENBRB →
  `optimal` at 9075.855 with ZAMB2-8, READING6, BRITGAS and YORKNET unmoved.**
  No candidate produces a single `optimal` on STEENBRB, and the only cell
  unmoved by all four is READING6, which is unmoved *by construction* (its
  `h_k` is 0, so its ζ is 0 under every member).

**The screen's verdict is recorded here as what it is — a screen result that the
ON rows contradict** — and not as a recommendation. RULE.md §2's own last bullet
pre-registered exactly this possibility: the screen is a TRAIN-fitted read of the
ζ = 0 trajectory, "it says which weights cannot be right; it does not say which
one is", and the fix's own TRAIN re-run decides. The re-run is in `rows/` and it
decides against all four.

**What is NOT withdrawn.** Task A's code stands and is proven OFF
(`off-identity.txt`: all seven cells IDENTICAL to M2's committed ladder, 0
differing keys and 0 differing row-columns). The design page's diagnosis — the
wrapper's objective has no x-block, so nothing prices distance from the
requesting iterate — stands with it. What this artifact refutes is the *shape*:
an absolute quadratic proximity weight, at any of the four pre-registered
magnitudes, centred on `x_k`, does not convert these cells.

---

## 1. What ran

35 legs, 2026-09-18 20:34 → 21:22, every leg ALONE under `/tmp/box-build.lock`,
on a clean tree at `57742341` (task A's code `577719a0` + RULE.md). All 35
returned rc=0; the 900 s per-leg safety stop never fired. The terms and the
binaries' sha256 are in `PROVENANCE.txt`; the transcript is `logs/chainB.log`.

| arm | ζ | legs | what it is |
|---|---|---:|---|
| `screen` | 0 | 7 | the trajectory the RULE is evaluated on, with the new `prox_rel` column; doubles as the OFF-identity check |
| `c1` | `h_k/(n·ρ)` | 7 | candidate arm — evidence, not the rule's input |
| `c2` | `h_k/ρ` | 7 | candidate arm |
| `c3` | `h_k/n` | 7 | candidate arm |
| `c4` | `h_k` | 7 | candidate arm |

The OFF arm of each per-cell table is **M2's committed ladder**
(`docs/notes/data/2026-09-m7-w2-m2-restoration/ladder/`), cited and not re-run.

## 2. The OFF proof

`off-identity.txt`, machine-written. The screen arm runs at the shipped default
on a library that now contains the term; its rows must reproduce M2's ladder
column for column, and a difference is a STOP (RULE.md §5).

```
STEENBRB   keys  37 compared,   0 differing; rows  17 x 8 columns, 0 differing   IDENTICAL
STEENBRC   keys  37 compared,   0 differing; rows  23 x 8 columns, 0 differing   IDENTICAL
STEENBRG   keys  37 compared,   0 differing; rows  23 x 8 columns, 0 differing   IDENTICAL
ZAMB2-8    keys  37 compared,   0 differing; rows  24 x 8 columns, 0 differing   IDENTICAL
YORKNET    keys  37 compared,   0 differing; rows  19 x 8 columns, 0 differing   IDENTICAL
BRITGAS    keys  37 compared,   0 differing; rows  11 x 8 columns, 0 differing   IDENTICAL
READING6   keys  35 compared,   0 differing; rows  14 x 8 columns, 0 differing   IDENTICAL

OFF_IDENTITY_WORST=0
```

Two of those legs also reproduce the harness's own pinned `hven-sqp` row on a
different binary — STEENBRB 5 majors / 25 973 factorizations, ZAMB2-8 11 / 8 803
(`PROVENANCE.txt`, the cross-check).

## 3. The screen's verdict, and the ON rows that contradict it

`verdict.txt` carries the verdict block verbatim, as the summariser printed it.
Its result:

| member | ζ | (a) STEENBRB/C/G refuse ≥ 10× | (b) ZAMB2-8 admits ≥ 10× | (c) BRITGAS admits ≥ 10× | screen |
|---|---|---|---|---|---|
| 1 | `h_k/(n·ρ)` | PASS (22.2 / 1739.7 / 1739.7) | PASS (4358.4) | PASS (161 125.7) | **QUALIFIES** |
| 2 | `h_k/ρ` | PASS | PASS (31.6) | PASS (358.1) | QUALIFIES |
| 3 | `h_k/n` | PASS | **FAIL** (4.36) | PASS | no |
| 4 | `h_k` | PASS | **FAIL** (0.0316) | **FAIL** (0.358) | no |

`VERDICT: member 1, zeta = h_k/(n*rho) -- the smallest member clearing every
clause by the pre-registered factor of 10.`

And the same four weights, measured (`tables/summary.md`):

| cell | screen (ζ=0) | c1 | c2 | c3 | c4 |
|---|---|---|---|---|---|
| STEENBRB | `numerical_error`, h → 1.6e-12 | `infeasible`, h → 17 719.9 | `infeasible`, h → 19 791.2 | `infeasible`, h → 19 795.9 | `infeasible`, h → 19 800.0 |
| STEENBRC | `numerical_error`, h → 4.9e-12 | `infeasible`, h → 40 550.4 | `infeasible`, h → 41 795.4 | `infeasible`, h → 41 797.5 | `infeasible`, h → 41 800.0 |
| STEENBRG | `numerical_error`, h → 4.9e-12 | `infeasible`, h → 40 550.4 | `infeasible`, h → 41 795.4 | `infeasible`, h → 41 797.5 | `infeasible`, h → 41 800.0 |
| ZAMB2-8 | `optimal` | `optimal` (6 558 fac) | **`infeasible`** | `optimal` (6 164 fac) | **`infeasible`** |
| YORKNET | `infeasible`, h unmoved | `infeasible`, h unmoved | `infeasible`, h unmoved | `infeasible`, h unmoved | `infeasible`, h unmoved |
| BRITGAS | `numerical_error` | `numerical_error` | `numerical_error` | `infeasible` | `infeasible` |
| READING6 | `optimal` | `optimal` | `optimal` | `optimal` | `optimal` |

`infeasibility_certified` is **0 on every one of the 35 legs**.

Note the direction of the contradiction. The screen asked whether each weight
would have refused the ζ = 0 runaway step; every member does, and generously.
What the screen could not ask — RULE.md §2 says so in advance — is what the
sub-solve does once its minimiser moves. It stops moving: at members 2, 3 and 4
the STEENBR phase takes ONE restoration major and a step of `dx_inf` 1e-4 to
0.1, and returns a point that is still 19 791 / 41 795 away from feasible.

## 4. Three findings, from the rows and the code

### Finding 1 — the `infeasible` exits come from TWO different arms, and the rows tell them apart

`enter_restoration`'s exit is decided in two steps. First `h_r <= feas_tol`
(`src/drivers/sqp_solver.cpp:4715-4717`) chooses RESUME or EXIT; on these rows
it always chooses EXIT. Then a switch on the sub-solve's own status
(`:4843`) spells the exit: `kOptimal` → `kInfeasible` **plus the certificate
gate** (`:4844-4869`), the sub-solve's own `kInfeasible` → `kInfeasible` with
**no claim** (`:4870-4876`), `kNumericalError` → `kNumericalError` (`:4893`).
The parent's reported status is therefore arm-faithful, and BRITGAS's
`numerical_error` rows (screen, c1, c2) are the third arm, observed directly.

**YORKNET is the sub-solve's own `kInfeasible`, at every ζ including 0 — proved,
not inferred.** Its restoration never moves: `dx_inf = 0` and `prox_rel = 0` on
every one of the 18–34 depth-1 rows, `h_out == h_in` to the last digit, a
proposed step of 131.08 rejected while the radius halves from 1 to 7.6e-06
(`tables/YORKNET.md`). At `dx_inf = 0` the certificate gate at `:4868` reads
`ζ·‖D²(x_r − x_k)‖∞ = 0 <= kkt_tol`, which is TRUE at every ζ — so had the
`kOptimal` arm been taken, `infeasibility_certified` would be 1. It is 0 on all
five YORKNET legs. The arm taken is `:4870`: the feasibility problem itself
raised a restoration request that nothing can service (the nested refusal,
`:4287-4297`), and **no claim about the model is made**.

**The STEENBR and ZAMB2-8 ON exits have the converged shape**: a terminal
depth-1 row with `step_norm = 0` at a radius that has not shrunk (1 at members
2–4, 64 at member 1 on STEENBRB) and an iterate that moved. On that reading the
sub-solve reached `kOptimal` **on `h + P`** while `h_r` stayed above `feas_tol`,
and `infeasibility_certified` is then the gate at `:4868` refusing. The refusal
is not marginal on any of them. With `d_i = min(1, 1/|x_{k,i}|) <= 1`,
`prox_rel = ‖DΔ‖²₂ <= ‖D²Δ‖∞ · n · ‖Δ‖∞`, so the residual the gate reads is at
least `ζ · prox_rel / (n · dx_inf)`:

| arm | cell | ζ | residual ≥ | × `kkt_tol` (1e-6) |
|---|---|---:|---:|---:|
| c1 | STEENBRB | 0.0423 | 0.0935 | 9.4e+04 |
| c1 | STEENBRC / G | 0.0774 | 0.0887 | 8.9e+04 |
| c1 | ZAMB2-8 | 1.786e-04 | 4.72e-06 | 4.7 |
| c2 | STEENBRB | 19.8 | 0.187 | 1.9e+05 |
| c2 | ZAMB2-8 | 0.0246 | 6.51e-04 | 6.5e+02 |
| c3 | ZAMB2-8 | 0.1786 | 4.72e-03 | 4.7e+03 |
| c4 | ZAMB2-8 | 24.64 | 0.638 | 6.4e+05 |
| c4 | BRITGAS | 115.04 | 0.248 | 2.5e+05 |

So at these weights **a certified infeasibility exit is unreachable**: the same
displacement that makes the term bite also puts the residual orders of magnitude
above `kkt_tol`. That is the gate doing exactly what §3.4 designed it to do —
withholding a claim about `h` that only `h + P` supports — and it means the
certificate and the term are, at this shape, mutually exclusive on these cells.

**What the evidence does not support.** The probe does not print the SUB-solve's
own status, so "the sub-solve reached `kOptimal` on `h + P`" is an inference from
the row shape on the STEENBR and ZAMB2-8 rows — sound, but not an observation;
only YORKNET is proved, and it is proved by the certificate flag rather than by
the status. One probe key (`rs.status`, or a counter for the nested refusal)
would settle every row directly, and it is registered as an instrument gap
rather than assumed away.

### Finding 2 — the phase is requested by a QP infeasibility inside a window `tr_init` sets, and on two cells that is the FIRST subproblem at `x0`

Every restoration on these cells is requested at `:5912`, the driver's only
consumer of `QpStatus::kInfeasible`: the subproblem came back infeasible inside
the window `min(st.delta, opts_.qp.tr_radius)` (`:5918`), the elastic ladder's
answer was not usable (`:5929`), the major's verdict is `kRestore` (`:5939`) and
`enter_restoration` is called at `:5960`. `SqpOptions::tr_init` is `1.0`
(`include/hven/drivers/sqp_solver_types.h:388`) and `st.delta` starts there, so
at major 0 that window IS `tr_init`.

* **STEENBRB and YORKNET enter in major 0** (`x_k_source=start_point` in
  `rows/screen/*.txt`): their depth-0 row 0 shows `radius = 1` with
  `step_norm = 0`, and `elastic_activations` is 1 and 19 respectively —
  `elastic_activations` being an exact count of the `kInfeasible` subproblems the
  WALK route met (`include/hven/core/solver_counters.h:1025-1029`). The very
  first QP at `x0`, inside an l∞ box of radius 1, is infeasible.
* **STEENBRC and STEENBRG enter at the second major** (`x_k_source=depth0_event`):
  row 0 takes a step of norm 1.0 at radius 1, and row 1 sits at radius 0.5 with
  `step_norm = 0` — the window had already halved below `tr_init` when the QP
  came back infeasible.
* **`tr_init` also sets what the solve resumes at**: the restart radius is
  `max(tr_min, kRestoreRadiusFactor · tr_init)` (`:6605-6608`), and the ζ = 0 depth-0
  rows after the phase show exactly that — radius 0.1, then 0.05, 0.025, 0.0125
  as every post-restoration trial is rejected at `f = 4.09e12` (STEENBRB) and
  `f = 1.83e14` (STEENBRC, whose restoration returned a point with
  `dx_inf = 9999.9`, at the box).

So one option governs both ends of the failure on these cells: the radius whose
QP infeasibility requests the phase, and the radius the solve is handed back
afterwards. **This is the coupling M3's `tr_init` arm exists to measure**
(`docs/notes/2026-09-m7-w2-sqp-cutest-read.md:812` and `:833-849` — an options
label on the TRAIN 23, no code, no default moved).

**What the evidence does not support.** Nothing here varies `tr_init`: all 35
legs ran at the default 1.0, so "a different `tr_init` would avoid the request"
is a hypothesis this artifact does not test and must not be quoted as a result.
Nor is `tr_init` shown to be the *cause* of the cells' failure — only that the
request is generated inside a window it sets. What the rows DO rule out is any
hope that the proximity term could have prevented the request: the term lives
inside the restoration wrapper, cannot change the parent QP that asks for the
phase, and indeed `restoration_entries` is 1 and the `h_k` drift is 0.0 on every
one of the 28 candidate rows (`tables/summary.md`) — all four weights enter the
same phase at the same point, and differ only in what happens after.

### Finding 3 — ZAMB2-8's member-1 run is the SAME local minimum, reached differently and more cheaply

The `-0.1189` that distinguishes member 1 from the screen arm is
`restoration_f_out` — the S5 record's objective at the RESTORATION exit, the
`f in -> out` column of `tables/summary.md` — and not the point the solve
converges to. Both runs converge, and they converge to the same place:

| lane | status | objective | majors | factorizations |
|---|---|---|---:|---:|
| this probe, screen (ζ = 0) | `optimal` | **-0.15293592733713887** | 11 | 8 803 |
| this probe, c1 (`h_k/(n·ρ)`) | `optimal` | **-0.1529359283870389** | 11 | 6 558 |
| this probe, c3 (`h_k/n`) | `optimal` | -0.15293592838703887 | 11 | 6 164 |
| SNOPT, H4 default | `Optimal` | -0.15293592478395268 | 36 | — |
| IPOPT, H4 default | `Solve_Succeeded` | -0.15293579466163296 | 20 | — |
| hven-ipm, H4 | `optimal` | -0.1529358 | 39 | 40 |

(the external rows: `hven-bench/runs/2026-09-m7-h4-external/four-lane-table.md`
lines 91–94, and `run-A/results.csv`.) All six agree to at least seven
significant figures. **Member 1 does not find a better local minimum — there is
no second minimum in this evidence at all.** What it changes is the restoration:
8 restoration majors instead of 11, an exit at `h = 2.41e-07` instead of
`1.21e-11`, and 6 558 factorizations against 8 803, a 25% cheaper solve to the
same answer.

That is the one genuinely favourable ON row in the artifact, and it is not
enough: the bar for ZAMB2-8 is UNMOVED, and a cell whose restoration exits
elsewhere and whose factorization count moves by a quarter is not unmoved. At
member 2 the same cell exits `infeasible` having stopped at
`h_out = 1.3783867488762098e-06` against `feas_tol = 1e-06`
(`include/hven/drivers/sqp_solver_types.h:375`) — a factor of 1.38 short of the
resume gate — and at member 4 at `h_out = 2.41`.

**A precise reading, recorded for the record.** The ruling above is stated in the
settler's words, which describe member 1 as converging "to a DIFFERENT point
(−0.1189 vs −0.0690)". Those two numbers are the restoration exits, as this
finding shows; the converged objectives are identical to nine figures. The
ruling is unaffected — the bar is missed either way, and missed on ZAMB2-8 by the
counters rather than by the objective — but the artifact records what the rows
say rather than repeating a reading they do not support.

## 5. The open item

**Absolute ζ versus a dimensionless coefficient on `h_k/ρ`.** The shipped field
is an absolute weight, and every candidate is a formula in `h_k` evaluated at
the requesting iterate, so each (cell, candidate) pair is one number fixed in
advance (RULE.md §3, §7 item 2). Whether the shipped spelling should instead be
a coefficient the engine multiplies by `h_k/ρ` at entry was ruled to be M2c's
call. **With no M2c, the question is moot** — it is a question about how to ship
a shape that does not meet the bar. It is recorded here so that a future attempt
at the *shape* knows the question was asked and left open, not answered.

## 6. Layout

```
README.md          this file — the ruling first
RULE.md            the decision rule, committed 57742341 BEFORE any leg ran
off-identity.txt   the OFF proof: the screen arm vs M2's committed ladder
verdict.txt        RULE.md §2 applied to the screen arm, verbatim
PROVENANCE.txt     the stamp, the binaries' sha256, the measurement terms
rows/screen/       the 7 zeta = 0 legs, raw probe output
rows/c1 .. c4/     the 28 candidate legs, raw probe output
tables/<CELL>.md   the cell's five arms as ladders
tables/summary.md  the S5 record and the outcome, per cell per candidate
logs/chainB.log    the run transcript: binaries, per-leg rc, the verdict block
gates/             the gate transcripts at the artifact commit
wrappers/          m2b_probe.cpp, probe-CMakeLists.txt, chainB.sh,
                   summarise_m2b.py, classify_psym.py
```

`wrappers/summarise_m2b.py` regenerates `tables/`, `off-identity.txt` and
`verdict.txt` from `rows/` and M2's ladder by pure file folding — it starts no
solve — and it was re-run at artifact time: all three outputs reproduced
byte-identically.

## 7. Reading caveats, carried from RULE.md §7

1. **At ζ > 0 the depth-1 `f_event` column is the wrapper's `h + P`, not the
   violation.** The `f_event == h` identity is a ζ = 0 fact. The tables label the
   column accordingly; `h_main` is the violation at every ζ.
2. **ζ is fixed per (cell, candidate) from M2's `h_k`**, not recomputed at the
   arm's own entry. The drift column in `tables/summary.md` is 0.0 on every row,
   so no row here is affected — but the column is printed because the rule
   requires a drifted row to be read as what it is.
3. **The screen is a TRAIN-fitted read of the ζ = 0 trajectory, not a
   prediction.** This artifact is the instance that proves why that caveat was
   pre-registered.
4. **READING6's ζ is 0 under every member by construction** (`h_k = 0`, no
   restoration entered): its four ON legs are the same solve as its screen leg,
   and its role is to be unmoved.
5. **HOLDOUT was not run**; every cell here is TRAIN.

---

## 8. Corrections (fix1) — appended 2026-09-20

**Append-only.** Nothing above this line is altered, and `verdict.txt`,
`RULE.md`, `off-identity.txt`, the `rows/`, the `tables/` and the `gates/`
transcripts stay exactly as they were written. These are the corrections the
review of the artifact commit (`.superpowers/m7-w2-m2b-review-sol.md`) found
owed, adjudicated by the settler and recorded here so that a reader of the text
above reads it with them.

### (i) "not a recommendation" bends the nomenclature — the honest wording

§3's sentence "The screen's verdict is recorded here as what it is — a screen
result that the ON rows contradict — and **not as a recommendation**" is not the
pre-registered wording. RULE.md §6 says the selected member is recorded AS a
recommendation, and `verdict.txt` says exactly that, verbatim, and stays
verbatim.

**The correct statement is: member 1 was the PROVISIONAL SCREEN RECOMMENDATION,
and it was rejected by the pre-registered ON/TRAIN evidence.** Nothing numerical
moves with the wording: the screen selected member 1 under the rule as written,
the four ON arms were run because the rule required them, and all four fail the
TRAIN bar. What was wrong was to deny that the screen had recommended anything;
what the screen produced was a recommendation that the next pre-registered step
overturned, which is the rule working rather than the rule being reinterpreted.

### (ii) STEENBRB and YORKNET have NO depth-0 rows — the supporting sentence is false

Finding 2's bullet claims that STEENBRB and YORKNET "show a depth-0 row 0 with
`radius = 1` and `step_norm = 0`". **No such rows exist.** Both cells' first
recorded rows are DEPTH 1, and their step norms are 1 (STEENBRB) and 131.08
(YORKNET). The sentence is withdrawn.

**The conclusion it was offered in support of survives, on the evidence that was
always the real basis for it**: `x_k_source = start_point` in
`rows/screen/STEENBRB.txt` and `rows/screen/YORKNET.txt` — the phase was
requested at the START POINT — together with the code path, which shows that
`st.delta` is initialised from `tr_init = 1.0`, so the window the first
subproblem is solved in IS `tr_init`. Nothing in finding 2's conclusion rests on
the withdrawn sentence.

### (iii) The trust radius is M3's HYPOTHESIS, not an established root cause

Finding 2 and the settler's own summary of it read more strongly than the
evidence supports. **The corrected statement**: `tr_init` is the NEXT
DISCRIMINATING VARIABLE, and M3's probe is a hypothesis test. What the evidence
DOES establish is (a) that the request is raised inside the
`min(st.delta, opts_.qp.tr_radius)` window, (b) that on STEENBRB and YORKNET that
window is `tr_init` because the request comes at major 0, and (c) that the
proximity term cannot affect the parent QP that raises the request — it lives
only inside the restoration wrapper's objective. It does NOT establish that a
different `tr_init` converts these cells; **no leg in this artifact varies
`tr_init`.** "Root cause moves to M3" overstates it; "the discriminating
variable, and the experiment to run, move to M3" is the claim the rows support.

### (iv) `sizeof(SqpOptions)` is 416, against the brief's 408 estimate

The task brief predicted `sizeof(SqpOptions)` 400 → 408. The observed value is
**416**: the 8-aligned `double` cannot fit after the byte at offset 384, lands
at 392, moves the `common` block to 400–412, and the struct rounds up to 416.
The brief's 408 was an estimate; the compiler-observed value, read off
`tests/drivers/test_options_dump.cpp`'s own `static_assert`, is the pin. The
live triple is `kSqpOptionsFields` **31**, `kSqpDumpLines` **62**,
`sizeof(SqpOptions)` **416** — and it is what makes P-SYM's exit 1 expected.

### (v) The ON-path certificate gate was WRONG in the code this artifact was taken on

Finding 1 reasons about the gate at `src/drivers/sqp_solver.cpp:4868`. In the
code at the artifact commit that gate read `‖∇P‖∞ ≤ kkt_tol` — a PROXY for the
residual §3.4 of the design page specifies — and at a finite-tolerance `kOptimal`
the proxy can pass while the real residual `‖Jᵀλ − z_sub‖∞` exceeds `kkt_tol`.
It is fixed in fix round 1: the residual is computed explicitly, and the proxy is
not kept as a fast path. The line numbers this README cites are the artifact
commit's and have moved since.

**Finding 1's conclusions are unaffected, and here is why.** The refusals it
reports are refusals — the flag is 0 — and a proxy that is too PERMISSIVE cannot
manufacture one. The residual lower bounds in its table are bounds on `‖∇P‖∞`,
and the exact residual is at least `‖∇P‖∞ − kkt_tol`; the smallest entry in that
table is 4.72 × `kkt_tol`, so every row stays above tolerance by at least 3.72 ×
`kkt_tol` under the exact rule. The YORKNET argument is likewise unaffected: at
`dx_inf = 0` both the proxy and the exact residual would have passed had the
`kOptimal` arm been taken, and the flag is 0, so the arm taken is the nested
refusal.
