# M7 W2 A1 — stopping-μ against bound geometry: the design page

Date: 2026-10-23. Author: the W2 A1 design lane. Status: a DESIGN PAGE. **No
production code lands with it.** Branch `w2-a1` off `m7` at `3d27c29f`; every
`file:line` below was re-located at that HEAD. `src/qp/ipqp_engine.cpp` has not
changed since the M6 merge (`0339749f`), so the M6 pins this page reads still
describe the code that runs.

Authority: the M7 brief `docs/notes/2026-09-m7-brief.md` §1 W2 A1 ("the three
motivating cases …; a rule that reads the active-set margin against the tier's
accuracy floor; measured on TRAIN against its OFF state. Its fixture class
survives barrier-default moves (M7-42: constructed from `Sigma = 2 mu_stop /
s^2`)") and §0's settings contract; the A0 page
`docs/notes/2026-09-m7-w2-a0-signals-design.md` §2 (the per-toggle contract,
followed clause by clause in §3.6 below) and its §2.1 A1 row, which this page
revises (§3.1).

Evidence produced in this round: `docs/notes/data/2026-10-m7-a1/` — a read-only
probe (public and bench API only, hven unmodified), its build and run scripts
and every raw transcript. Runs were serialized under `/tmp/box-build.lock`,
`MKL_NUM_THREADS=1`, Release, the uniform flag regime, clang 22.1.8, AMD Ryzen 7
5800X3D. Counters only; no wall-clock figure is quoted. No TRAIN cell and no
HOLDOUT cell was run. Accelerate: UNOBSERVED.

---

## 0. The answer in one paragraph

The IPQP tier (the interior-point QP kernel the SQP engine runs at
`qp_mode = kIpm`) stops when its relative residuals meet `ipqp_converge_slack ×
opt_tol` (1e-7). Nothing in that test reads geometry. A constraint whose margin
— the true slack `d` of a nearly active row or bound, or the true multiplier of a
weakly active one — satisfies `d² < κ·μ_stop` (κ = `ipqp_face_kappa` = 1e-2) is
handed on committed to the wrong side, and the barrier curvature `Σ = μ/d²` that
does it is exactly the gate-8 construction parameter. **The proposed rule** reads,
at the moment the stopping test first passes, the set of committed-active pairs
whose multiplier is still scaling with μ (T4c's exponent test, today used only on
bound sides inside the certificate read, extended to inequality rows); if that
set is non-empty it EXTENDS the solve once to the QP layer's own tolerance
(slack 1, the tier's accuracy floor), and in its second arm then DEMOTES whatever
is still suspect from ACTIVE to UNCERTAIN in the face it hands to tier 3. One
named field, `IpqpOptions::ipqp_margin_stop` (`kOff` / `kExtend` /
`kExtendAndDemote`), OFF byte-identical. Measured with a probe this round: the
extension removes the gate-8 standing-saddle exposure for `Σ ≤ 2.5e4` at one
extra iteration; extension plus a history-free demotion proxy turns the polish
from refused to accepted on 5 of the 6 scattered `m1e-6` E1 cells (0 of 6 today;
the RED cell counted only with its iteration cap lifted to 200), demoting 0 truly
active rows in 14 trials. It does not turn the named RED cell
green at the 60-iteration cap: that cell's first failure is a different
mechanism — μ collapsing to its floor while the primal residual is still 1e-3 —
which this page registers for A2 (§1.2, §7).

---

## 1. The three motivating cases, located and re-measured

The brief names them "the gate-8 Sigma ~ 250 fixture, `e1_f7_n20000_af30_m1e-6`,
the C1 accuracy/stopping coupling". The M6 ledger names the third as "the
tight-margin population" (`docs/notes/2026-08-m6-ledger.md:1199`: "the
tight-margin population is the stopping-mu item's third motivating case"), and
calls the second "C1's second case" (`:1152`). The two lists are the same three
things: C1 is the coupling itself, gate-8 is its constructed instance, the named
cell is its worst E1 instance, and the `m1e-6` margin class is its population.
This page uses the ledger's split.

### 1.1 Case 1 — gate-8 at Σ ≈ 250: a saddle certificate that stands

**Where.** The family `weakly_active_indefinite_qp(s, h)`:
`tests/sqp/test_ipqp_certification.cpp:529-544` — `H = diag(2, h)`, `g = 0`, a
symmetric box `[-s, s]` on `x1`, so `x1 = 0` is an exact barrier KKT point at
every μ with `Σ = 2μ/s²`. The Σ-parametrised constructor `:548-550` and the
measured `gate8_mu_stop()` `:555-559`; the gate `:717-895`; the Σ = 250 member
pinned flag-TRUE `:796-818`; the f-gap pin `:1720-1754`. Engine side: the
certificate read's weak-activity scale `src/qp/ipqp_engine.cpp:1702`, its
disclosure band `:1708`, the band and exponent counts `:2535-2570`.

**What goes wrong.** At `h = -1` the true minimizers are `x1 = ±s`; the tier stops
at the saddle `x1 = 0`. Each bound side carries `z = μ/s`, so for `Σ > 2F²` (F =
`kIpqpWeakActiveFactor` = 10, `include/hven/detail/qp/ipqp_engine.h:109`) the
sides are NOT weak, the read keeps their curvature, `h + Σ > 0`, and the
certificate stands at a saddle. The owner ruled the residue ACCEPTED WITH
DISCLOSURE (`docs/notes/2026-08-m6-ledger.md:1004-1018`): it is μ-bounded
(f-gap = ½|h|s² = μ_stop/Σ ≤ μ_stop), it exists only in a certificate the tier
issues standing alone, and widening the 10× factor is the wrong fix.

**Re-measured** (`a1_probe gate8`, tier-direct, the test's own `tight_opts`):
`mu_stop = 1.25e-9` at the shipped slack, 3 iterations for every member.

| Σ target | s | slack 100 (today): h = −1 | slack 1 (one more decade): h = −1 | h = +1, both slacks |
|---:|---:|---|---|---|
| 0.77–10 | 5.7e-5 … 1.6e-5 | downgraded (weak, dropped) | downgraded | certifies |
| **250** | 3.16e-6 | **stands** (band 2, noise 2, flag TRUE) | **downgraded**, 4 iters, μ 6.25e-12 | certifies |
| 1000 | 1.58e-6 | stands, flagged | downgraded | certifies |
| 25000 | 3.16e-7 | stands, flagged | downgraded | certifies |
| 2.5e5 | 1.0e-7 | stands, flagged | **still stands**, flagged (Σ' = 1250) | certifies |

One extra iteration lowers μ by 200× and moves every member with `Σ ≤ 2.5e4`
into the read's weak band, where the saddle is seen; the minimizer partners still
certify, so the extension is not "downgrade everything". The `Σ = 2.5e5` member
is beyond what the floor can resolve and stays a disclosed exposure. Source:
`data/2026-10-m7-a1/out/gate8-slack{100,1}.txt`.

### 1.2 Case 2 — `e1_f7_n20000_af30_m1e-6`: the named RED cell

**Where.** `bench/ipqp_e1_arm.h:53` (`kNamedRedCell`, "its margin class is the
tier's own accuracy floor, so it reaches the cap"); the committed row
`docs/notes/data/2026-08-m6-w1-acceptance/a4-gate-mu1e-2.csv:37`; asserted RED by
name in `IpqpAcceptanceA4`. `nx = 1e5`, 20 000 inequality rows, 30 % active, and
the E1 construction's tightest margin class: every inactive row's relative slack
is drawn log-uniformly over three decades with the 10th percentile at `1e-6`
(`docs/notes/data/2026-08-m6-e1-acquisition/README.md` "How a cell is built", item 4).

**Re-measured.** The probe reproduces the committed row on every scored column
(status `max_iter`, 60 iterations, 60 factorizations, 11 649 misclassified, 2 351
uncertain, Rule A 10 946 / 178 missed / 5 124 false positives, μ 3.33e-8). The
per-iteration trace (`out/e1-red-s100-c60.txt`) shows what the committed row could
not:

* **μ collapses to its floor while the point is still infeasible.** At iteration
  18 μ = 8.7e-11 and at 19 μ = 2.4e-12 (the floor `ipqp_min_mu` is 1e-12,
  `include/hven/drivers/sqp_solver_types.h:149`) with the primal residual at
  2.2e-3 and the dual at 1.2e-3. Mehrotra's centering parameter is 1.5e-4 there:
  complementarity converged two decades ahead of feasibility.
* **The steps then collapse and μ bounces.** α_p = 0.25, α_d = 0.04 at iteration
  19; σ jumps to ~0.9; μ climbs back to ~3e-10. The same cycle repeats at
  iterations 41-44 and 56-57. Sixty iterations are spent in three such cycles.
* The smallest true inactive slack on the cell is 8.7e-9 (absolute): rows that
  close at the floor at that distance are where the fraction-to-boundary rule cuts
  the step.

With the cap lifted to 200 the cell converges — 85 iterations at slack 100, 114 at
slack 1 — but the polish is still refused at both (the face is over-committed by
5 677 and 712 rows respectively). **So the cell fails twice, for two reasons**: a
μ-update failure (it never reaches the stopping test inside 60 iterations), and
behind it the stopping-μ failure this page is about (when it does stop, the face is
wrong). A1 addresses the second. The first is not a stopping rule and is
registered as A1-R1 for A2 (§7, Q3).

### 1.3 Case 3 — the tight-margin population: the `m1e-6` class

**Where.** The M6 W1 acceptance record
`docs/notes/data/2026-08-m6-w1-acceptance/README.md` §9.4 ("the eight `m1e-6`
cells that reached `refine_on_face` with a usable step and were turned away by its
rank pre-screen …, the tier's committed face being over-large at that margin
class") and the registered §2.3 item there (the ratio rule "over-commits … and
under-commits …; both are the same threshold read at the same accuracy floor").
The driver's hand-off is `src/drivers/sqp_solver.cpp:5066-5108` (route 2, the
tier-3 refinement); the classifier is `src/qp/ipqp_engine.cpp:2762-2810`.

**Re-measured** on the five scattered non-RED `m1e-6` cells plus two controls
(`out/e1-*.txt`, `out/demote-*.txt`). Each reproduces its committed face counts
(e.g. `n4000_af01`: 321 committed, 281 misclassified, 3 543 uncertain).

| cell | slack 100 (today): committed / true active, polish | slack 1: committed / true, polish | iterations 100 → 1 |
|---|---|---|---|
| n4000_af01_m1e-6 | 321 / 40, refused | 99 / 40, refused | 34 → 36 |
| n4000_af10_m1e-6 | 2025 / 400, refused | 605 / 400, refused | 23 → 27 |
| n4000_af30_m1e-6 | 2599 / 1200, refused | 1403 / 1200, accepted with **204 false actives** | 17 → 20 |
| n20000_af01_m1e-6 | 5028 / 200, refused | 1523 / 200, refused | 29 → 32 |
| n20000_af10_m1e-6 | 8809 / 2000, refused | 5546 / 2000, refused | 28 → 29 |
| n20000_af30_m1e-4 (control) | 6018 / 6000, accepted, 17 false actives | 6000 / 6000, accepted, exact | 16 → 17 |
| n20000_af30_m1e-2 (control) | exact | exact | 18 → 20 |

Every error on every `m1e-6` cell is in one direction: **over-commitment** — a
truly inactive row committed active. `under_committed` and "uncertain but truly
active" are 0 on all of them. For the over-committed rows the probe measured
`z · s* / μ` (multiplier × TRUE slack / μ), median 0.4–8.4 across cells: their
multipliers are barrier noise, `z ≈ μ/s*`, not prices. That is the signature the
rule detects (§3.2).

---

## 2. The mechanism

**Centrality and the ratio rule.** Near the central path each pair satisfies
`s·z ≈ μ`. The face classifier (`src/qp/ipqp_engine.cpp:2764-2771`) commits a
pair ACTIVE iff `s < κ z` and `z > μ`, INACTIVE iff `z < κ s`, else UNCERTAIN.
For a truly inactive row sitting at its true slack `d` (so `z ≈ μ/d`), those tests
become:

* committed ACTIVE (wrong) iff `d² < κ·μ`;
* UNCERTAIN iff `κ·μ ≤ d² ≤ μ/κ`;
* INACTIVE (right) iff `d² > μ/κ`.

A truly active row with multiplier `λ` is symmetric with `λ` in place of `d`. The
dimensionless quantity is `μ/d²`, which for a bound side is its barrier curvature
`Σ_side = z/d = μ/d²`. **The gate-8 construction parameter is this ratio**
(`Σ = 2μ_stop/s²` sums the two sides), which is why that family is the right
fixture class (§4).

**The tier's accuracy floor.** The stop sets μ_stop from the RESIDUALS
(`ipqp_residuals_meet_target`, `src/qp/ipqp_engine.cpp:563-571`, called at
`:2060`): complementarity ≤ `ipqp_converge_slack × opt_tol`, i.e. 1e-7 relative
at the defaults (`sqp_solver_types.h:214`, `qp/qp_types.h:126-127`). Spec §2.3
step 1 calls this the tier's contract: "the tier does not chase 1e-10 with
barrier iterations; that is what tier 3 is for"
(`docs/notes/2026-08-m6-w1-ipqp-spec.md:193-196`). So the face is over-commitment-
free only when `μ_stop ≤ d_min²/κ = 100·d_min²`. Measured:

* `m1e-4` af30: `d_min = 1.5e-6`, so `100 d_min² = 2.4e-10`; the shipped stop lands
  at μ = 2.35e-10 — on the boundary — and 18 rows over-commit. One more decade
  (slack 1, μ 2.3e-12, one iteration) is exact.
* `m1e-6`: `d_min` 4.8e-9 … 2.1e-8, so `100 d_min²` is 2e-15 … 4e-14 — **below the
  barrier floor `ipqp_min_mu = 1e-12`**. No μ the tier is allowed to reach resolves
  the face. Extension shrinks the over-committed set 1.6–4.8× (table above) but
  cannot empty it; the polish's rank pre-screen still refuses.
* gate-8: the read's weak band is `d ≤ F√μ` and `z ≤ F√μ`, i.e. `μ ≤ F²d²`; at the
  defaults `F² = 1/κ = 100`, so the read and the classifier share one threshold.
  This is a coincidence of two independently chosen constants, recorded as Q5.

So "stopping-μ against bound geometry" has two regimes, and the rule needs both
answers: **resolvable** (`d² ≥ κ μ_floor`) — stop later; **unresolvable** — do not
pretend: hand the ambiguous pairs on as UNCERTAIN, which spec §2.3 item 2 already
says is never forced either way, and let tier 3's exact face read (and SSN's tie
band behind it) decide them.

**What the stop does NOT see today.** Nothing between `:2051` and `:2062` reads the
pairs. The only reading of "is this multiplier a price or barrier noise" is T4c's
exponent test (`include/hven/detail/qp/ipqp_math.h:206-226`), applied to KEPT bound
sides inside the certificate read only, AFTER the solve has stopped, to set a
disclosure flag. Inequality rows have no such reading at all: the engine retains
the previous accepted iterate's bound duals (`prev_zl`, `prev_zu`,
`src/qp/ipqp_engine.cpp:1299`, snapshot at `:2376-2381`) and not its row
multipliers.

**Second mechanism, not A1's (case 2).** μ reaching the floor while the residuals
are 1e-3 is a μ-UPDATE failure: Mehrotra's adaptive σ (spec §3.3, clamp at
`[ipqp_min_mu, ipqp_init_mu]`) has no safeguard tying μ to the infeasibility.
It interacts with geometry (the collapse happens where many inactive rows sit at
slacks ~1e-8) but no stopping rule can fix it, because the solve never reaches the
stopping test. Registered A1-R1 (§7).

**The NLP-level analogue.** The IPM → SQP crossover classifies activity at the
interior engine's stopping μ by its own rule (`IpCrossoverOptions`,
`include/hven/detail/warmstart/warm_start.h:149-159`). The same `μ/d²` geometry
applies there. It is out of A1's scope and registered as A1-R2.

---

## 3. The rule

### 3.1 Placement, and what changes from A0's proposal

A0's §2.1 A1 row proposed `SqpOptions::active_margin_stopping` (bool) reading the
SQP census `weak_active_peak` / `near_active_peak`, with M7-20 as a precondition.
This page moves the rule INTO the tier, on `IpqpOptions`, for three reasons: the
stopping decision is the tier's (finding (ii) of A0: every W2 heuristic is
engine-internal or between-solve, and this one is engine-internal); the inputs are
the tier's own pairs `(s, z)` and its μ history, not a post-solve census; and the
census is structurally blind to the cases (§3.5). A3b's `ladder_skip_after` is the
precedent for a tier lever on `SqpOptions::ipqp`.

### 3.2 Inputs

All engine-internal, all at the point where the stopping predicate first passes:

1. **μ and μ⁻** — the measured complementarity now and at the previous accepted
   iterate (`mu_meas`, `prev_mu`; `prev_mu` exists at `:1300`).
2. **The committed-active pairs** — inequality rows `(s_j, yi_j)` and bound sides
   `(gap, z)` that the face classifier would commit ACTIVE at this point. The
   classifier lambda at `:2764-2771` becomes a free function so the stop and the
   face read ONE statement of it (the same discipline the stopping predicate
   already follows, `:563-565`).
3. **The exponent verdict per pair** — `ipqp_classify_barrier_noise(z⁻, z, μ⁻, μ)`
   (`ipqp_math.h:206`) verbatim: `e = log(z/z⁻)/log(μ/μ⁻)`, SUSPECT iff `e ≥ 0.5`.
   Rows need `prev_yi` retained beside `prev_zl/prev_zu` at `:2378`.
   **Fallback when history is uninformative** (fewer than two accepted iterates,
   or `μ/μ⁻ > 0.5`, the existing rule at `:2542-2543`): SUSPECT iff
   `z ≤ kIpqpTightBandFactor · kIpqpWeakActiveFactor · √μ` — T4c's band ceiling,
   `ipqp_engine.h:109`, `:114`.
4. **The tier's two accuracy levels** — the contracted target
   `ipqp_converge_slack × opt_tol` and the floor `opt_tol` (slack 1), both already
   options.

The A0 signals A1 reads are therefore `ipqp_read_kept_tight_sides` /
`ipqp_read_barrier_noise_sides` generalised (same classifier, same band) and
`ipqp_face_uncertain` (`include/hven/core/solver_counters.h:1130`, `:1273`,
`:1278`). It reads none of the SQP census fields.

### 3.3 The decision

Let `S` be the SUSPECT committed-active pairs at the first stop candidate.

1. **`S` empty → stop.** Today's behaviour, unchanged.
2. **`S` non-empty and not yet extended → EXTEND.** The convergence target for the
   rest of this attempt becomes slack 1 (`opt_tol` itself). The loop continues
   under the EXISTING caps (`ipqp_hard_iter_cap`, the factorization budget, the
   stall window) — no new budget. **An extension never turns a converged solve
   into an escape:** the iterate at the first stop candidate is snapshotted, and if
   a cap or a stall fires during the extension the snapshot is returned as the
   converged result, exactly as it would have been without the rule.
3. **After the extension, or when the configured slack is already 1 → stop.**
4. **DEMOTE (`kExtendAndDemote` only).** Recompute `S` at the final point; every
   pair in it that the classifier commits ACTIVE is reported UNCERTAIN instead
   (counted in `ipqp_face_uncertain` as every uncertain pair is, and in the new
   demotion counter). Never forced INACTIVE. The certificate read is NOT touched:
   the owner's ruling stands, the disclosure flag still fires where it fires today,
   and a dropped price can only cost a refusal downstream, never a false
   certificate (`refine_on_face` keeps its sign, drift and inertia gates;
   a refusal routes to SSN as today, `sqp_solver.cpp:5109+`).

**Considered and rejected: a projected-resolvability test** (extend only if some
suspect would leave ACTIVE above the floor, `d² ≥ κ μ_floor`). It is the
geometrically tidy form of "margin against the floor", and the probe argues against
it: on the `m1e-6` cells it would skip the extension, yet the extension is what
makes the demotion work there — at slack 100 the band proxy recovers the polish on
2 of 6 cells, after the extension on 5 of 6 (§3.4). The floor comparison survives
as the *explanation* of when the second arm is needed, not as a branch.

### 3.4 What the probe measured of it (not the rule; its two halves)

The probe cannot run the rule — the exponent test needs the engine's history — so
it measures the two halves on the shipped engine: the extension as `slack = 1`, and
the demotion as two post-hoc variants on the returned face, handed to
`QpEngine::refine_on_face` exactly as the driver hands it: **band** (the
history-free fallback of §3.2 item 3, the realisable lower bound of the primary
test) and **oracle** (demote exactly the truly inactive committed rows; an upper
bound no rule can reach).

| cell | none, slack 100 | band, slack 100 | band, slack 1 | oracle |
|---|---|---|---|---|
| n4000_af01_m1e-6 | refused | **accepted**, exact | **accepted**, exact | accepted, exact |
| n4000_af10_m1e-6 | refused | refused (face 408/400) | **accepted**, 1 false active | accepted, exact |
| n4000_af30_m1e-6 | refused | accepted, 28 false actives | **accepted**, exact | accepted, exact |
| n20000_af01_m1e-6 | refused | refused (238/200) | **accepted**, exact | accepted, exact |
| n20000_af10_m1e-6 | refused | refused (2044/2000) | refused (2024/2000) | accepted, exact |
| RED cell, cap 200 | refused (85 it) | refused | **accepted**, 2 false actives (114 it) | accepted, exact |
| n20000_af30_m1e-4 | accepted, 17 false | accepted, exact | (nothing to demote) exact | exact |

Across all 14 band trials (the 7 cells above × 2 slacks) **the band variant
demoted 0 truly active rows**; the over-commit it leaves is the rows whose noise multiplier sits
above the band ceiling (24 on n20000_af10), which is what the exponent test is for.
The extension costs +1 to +4 iterations per cell (table in §1.3); on gate-8, +1.

### 3.5 Why M7-20 does not gate A1 (the precondition read)

M7-20 (register D21) is "`near_active_rows` measured against F7 before any
heuristic reads it (0/770 on HS)". A1 does not read it: the census
(`src/drivers/sqp_solver.cpp:1831-1849`) is a post-solve count on the SQP's
accepted QP solution, of inequality ROWS only, at a magnitude margin
(`kWeakActivityMargin = 1e-6` times the largest slack or price,
`sqp_solver_types.h:319`) — not the tier's pairs, not bound sides, and not at the
moment of the stop. The precondition read was taken anyway, because the settler
asked for it and because it is the evidence that the census could not have served:

* **F7 corpus, bound window** (the U0 replay's population), neutral + physics
  starts, 10 cells × {walk, ipm} (`out/f7-walk.txt`, `out/f7-ipm.txt`): `weak = 0`,
  `near = 0` on every cell and every major, and the tier's own band / noise /
  uncertain counts are 0 on every `ipm` cell. These first QPs are
  near-equality-constrained (as T4c recorded). **The U0 replay cannot exercise A1;
  it is an OFF-identity instrument only.**
* **F7 corpus, path window**, N ≤ 1000 (the 1000 and 800 cells, neutral and
  physics, and the two 750/825 controls; `out/f7-path-n1000-{walk,ipm}.txt`). The
  N = 2000 path cell was stopped by the lane after 14 minutes on the walk
  (`out/f7-path-walk-stopped-at-n2000.txt`), outside a light diagnostic; larger
  path cells were not run. Here the census is NOT empty: `near_active_peak` 8–273
  on both engines, `weak_active_peak` 0–1 on the walk and 0–19 on the tier, on
  1–3 majors per cell. On the tier, `ipqp_face_uncertain` is 10–236 per solve
  while the band and noise counts stay 0 — the rows are ambiguous, and no kept
  bound side is in the disclosure band.
* **CUTEst TRAIN** — already in the record, not re-run: the hven-bench C1 cost
  census (`hven-bench/runs/2026-09-m7-w2-c1-sqp-cost-census/rows/{default,sqp-ipm}/`,
  hven pin `aeebdccc`) prints both peaks for 18 of the 23 TRAIN cells on both
  legs: `weak_active_peak = 0` on all 36 rows; `near_active_peak` is non-zero on
  two cells only (ACOPR118: 15, ACOPR57: 2, identical on both legs); 13 of the 18
  have `mi = 0`, so the census is structurally empty there whatever the geometry.
* **HS**: 0/770 (the register's own reading).

**Reading.** The census is informative on exactly one population measured here —
the F7 path window — and empty on the bound window, on HS and on 16 of the 18
CUTEst TRAIN rows. Where it is informative it still measures the wrong thing for
A1: a post-solve magnitude count of ROWS on the accepted solution, blind to bound
sides (where gate-8 lives) and blind to whether a multiplier is a price or barrier
noise. It does not gate A1. It stays registered for any heuristic that does read
it, now with this distribution as its first F7 measurement. One by-product
matters for §5: **the small path cells are the only corpus cells on which the
tier's face is ambiguous at all** (`ipqp_face_uncertain` 10–236), so they are the
corpus population on which A1 can fire.

### 3.6 The field, against A0's per-toggle contract

| clause | A1 |
|---|---|
| 1. one named field | `IpqpOptions::ipqp_margin_stop`, an enum `IpqpMarginStop { kOff, kExtend, kExtendAndDemote, kIpqpMarginStopCount }`, declared LAST in `IpqpOptions` (after `ipqp_require_final_inertia`, `sqp_solver_types.h:280`). One heuristic with two ON arms, each measured against OFF separately, on the `restoration_resume_rule` enum precedent (A0 §2.1 M2); not two fields, because the demote arm is defined only after an extension (Q2) |
| 2. validation | in `src/drivers/sqp_options.cpp` beside the IPQP block (`:238-254`): the count sentinel refused by name in the negated form, on `QpMode::kQpModeCount`'s precedent; `std::invalid_argument` naming the option and its value. Validated unconditionally at every `qp_mode`, as every `IpqpOptions` field is (`sqp_solver_types.h:107-109`) |
| 3. OFF byte-identical | `kOff`: no `prev_yi` retained, no snapshot, no suspect pass — the branch is the only difference in the stop and face code. Proof: U0 27/75/0 ×3, corpus cells and golden bytes (except clause 4's declared keys), P-SYM classified in full. Inert at the shipped `qp_mode = kWalk` (and `elastic_rung_engine = kWalk`), so the shipped configuration's arithmetic cannot move |
| 4. fired-counter | four trailing `IpqpCounters` fields (after `ipqp_read_barrier_noise_sides`, `solver_counters.h:1278`; X-table after `:1320`; fold at `src/drivers/sqp_solver.cpp:1904`): `ipqp_margin_suspects` (|S| at the first stop candidate), `ipqp_margin_extensions` (solves extended), `ipqp_margin_extension_iters`, `ipqp_margin_demoted`. All 0 under `kOff`. The trace golden lines move by the declared additive trailing keys, re-derived once (A0 clause 4) |
| 5. dump | `ipqp.ipqp_margin_stop = kOff` beside `:649`; `kIpqpOptionsFields` 22 → 23 (`tests/drivers/test_options_dump.cpp:79`), `kSqpDumpLines` 78 → 79 (`:185`), `sizeof(IpqpOptions)` off 152 (`:153`) and `sizeof(SqpOptions)` off 504 (`:118`); `kSqpOptionsFields` (45, `:78`) does not move |
| 6. default from TRAIN | §5. Lands provisionally `kOff` |
| 7. label | it selects WHEN the tier stops and how it reports its face, not how curvature is read; unlabelled. (The certificate read it leaves alone already carries the `CURVATURE SOURCE:` label, `sqp_solver_types.h:277-279`) |

**Code sites** (all `src/qp/ipqp_engine.cpp` unless noted): the classifier free
function lifted from `:2764-2771`; `prev_yi` beside `:2378`, gated; the decision at
the stop `:2059-2062`, where `converged = true; break;` becomes the §3.3 branch;
the demotion at the face loop `:2774-2810`; the snapshot-return beside the escape
switch (`:2646+`). Nothing in the driver changes: route 2 already hands
`ires.ineq_active` / `bound_state` on (`sqp_solver.cpp:5071`).

**Cost.** Under ON: one O(n + mi) pass per stop candidate (at most two per
attempt), one O(mi) copy per accepted iteration, one iterate snapshot per extended
solve. Extension iterations: +1 to +4 measured, each one factorization. Under
`kOff`: nothing.

---

## 4. The fixture class (M7-42): built from `Σ = μ_stop / d²`, not from a half-width

The M6 lesson (`docs/notes/2026-08-m6-ledger.md:1126-1128`, `:1158-1159`): a fixture
built at a fixed half-width stops exercising its mechanism when the barrier default
moves. T10b's repair (`:1186`) built gate-8 from `Σ = 2μ_stop/s²` with μ_stop
MEASURED by a probe solve and the regime ASSERTED before the verdict
(`test_ipqp_certification.cpp:555-559`, `:761`). A1's class generalises that
construction to the one dimensionless ratio the mechanism reads, `ρ = μ_stop/d²`
(§2), on both kinds of pair:

1. **Bound sides** — gate-8 itself, swept in Σ, both curvature signs. Members:
   Σ = 250, 1000, 25000 (resolvable: OFF stands + flagged, `kExtend` downgrades,
   the h = +1 partner certifies under every arm); Σ = 2.5e5 (unresolvable: stands
   under every arm, flag TRUE, `kExtendAndDemote` reports the two sides UNCERTAIN
   and leaves the certificate alone).
2. **Inequality rows** — a new two-variable family: one row truly active with
   multiplier `λ = 1`, one row truly INACTIVE at constructed slack
   `d = √(μ_stop/ρ)`, `H` positive definite, so the true face is known by
   construction (the E1 KKT-inversion recipe at its smallest). μ_stop is measured
   by the family's own probe solve, and the test asserts the measured `ρ` lands
   within 1 % of the target before reading any verdict — the gate-8 discipline.
   Members ρ ∈ {10, 200, 2000} (committed ACTIVE under OFF for ρ > 1/κ = 100) and
   ρ = 1e5 (beyond the floor). Pins: OFF over-commits; `kExtend` resolves ρ ≤ the
   floor ratio; `kExtendAndDemote` hands the near row on UNCERTAIN; the truly
   active row is committed under every arm (the false-demotion partner).
3. **E1 is a POPULATION, not a fixture.** Its margin classes are absolute (1e-2,
   1e-4, 1e-6 relative), so its members move between regimes when μ_stop moves —
   the `m1e-4` af30 cell sits exactly on the boundary today (§2). It stays the
   population read of §5, never a pinned regime.

Exact iteration counts stay MKL-scoped; Accelerate arms record UNOBSERVED, as
every gate-8 pin does today.

---

## 5. The measurement protocol — PRE-REGISTERED before any TRAIN run

**Arms.** OFF = the `sqp-ipm` label (`qp_mode = kIpm`, `ipqp_margin_stop = kOff`).
ON-1 = `sqp-ipm` + `kExtend`; ON-2 = `sqp-ipm` + `kExtendAndDemote`. Each ON arm is
compared with OFF, never with the other. The field is inert at the shipped
`qp_mode = kWalk`; a fourth arm, `default` + `kExtendAndDemote`, runs once as the
inertness check and must be row-identical to `default` on every non-wall column.
These are two-field labels relative to the lane default; the A4 rule's §6
coherent-mode record is the precedent, and the comparison reference is `sqp-ipm`,
not `default`.

**Cells.** TRAIN only (`hven-bench/sets/split-v1.json`): `hs` (22) and `cutest`
(23), on the `hven-sqp` lane. HOLDOUT is not run.

**Decision arithmetic.** `hven-bench/scripts/w2_a4_decide.py`, the A4
pre-registered rule §2 VERBATIM (`docs/w2-a4-decision-rule.md`): per family, ON
WINS over OFF iff `|S(ON)| ≥ |S(OFF)|`, `E(ON) ≤ E(OFF)`, and the median
factorizations per major over the commonly solved non-wall cells is ≤ OFF's, with
at least one of the first and third strict; ties go to OFF; `dnf_wall` counts for
neither side. Two serial runs per arm, `N/N/0` determinism (§4 of that rule).

**The A1 instruments**, reported for every arm, gating as stated:

* **I1 — the §4 fixture class**, Debug and Release (gates landing).
* **I2 — the E1 29-cell arm** (`hven_sqp_ipqp_e1_arm`, shipped budgets): OFF must
  reproduce `a4-gate-mu1e-2.csv` on every scored column; per ON arm: the polish
  acceptance count, END-TO-END Rule-A exact recovery, total tier + polish
  factorizations, and the RED cell's row.
* **I3 — U0 replay**, field at `kOff`: 27/75/0 ×3 against the M6 close CSVs (OFF
  identity). Its `ipm` arm at ON is reported; §3.5 predicts 0 firings.
* **I3b — the small F7 path cells** (N ∈ {750, 800, 825, 1000}, neutral and
  physics, `--engine ipm`): the one corpus population where the tier's face is
  ambiguous (§3.5). Per ON arm: status, factorizations, the four fire counters and
  `ipqp_face_uncertain`, against OFF. Reported, not gating; a status lost here
  blocks a default move.
* **I4 — the standing gate**: both suites, both configs; golden re-derivation
  declared; P-SYM classified in full; install smoke; clang-format.
* **Fire counts**: the four `ipqp_margin_*` counters per TRAIN cell. If they are 0
  on every TRAIN cell of a family, that family's verdict is recorded **NOT
  EXERCISED**, whatever the arithmetic returns.

**Pre-registered success criteria.**

* **C1 — the mechanism (gates landing the field, opt-in).** (a) ON-1 and ON-2:
  every resolvable gate-8 and row-family member resolves, every unresolvable member
  keeps today's certificate and flag, and no false-demotion partner moves. (b)
  ON-2 on I2: the polish is accepted on ≥ 4 of the 5 scattered non-RED `m1e-6`
  cells (0 of 5 under OFF), **no** cell of the 29 is worse than OFF in END-TO-END
  Rule-A missed or false actives, and the 29-cell tier + polish factorization
  total is ≤ 1.10 × OFF's. (c) ON-1 on I2: no cell worse than OFF in END-TO-END
  missed or false actives; factorization total ≤ 1.10 × OFF's. (d) No new RED
  cell at shipped budgets for either arm. The RED cell itself is NOT expected to
  turn green under A1 (§1.2); if it does, that is reported, not credited.
  *These E1 thresholds are informed by this round's probe (§3.4) — E1 is not
  TRAIN, and the probe measured halves of the rule, not the rule.*
* **C2 — the default (TRAIN).** An ON arm becomes the default **for
  `qp_mode = kIpm`** only if it WINS on at least one TRAIN family, is not NOT
  EXERCISED there, and on the other family satisfies clauses 1 and 2 of the A4 rule
  (it may tie on cost). Otherwise the default stays `kOff` and the field ships
  opt-in on C1's evidence. A default move is its own declared event, with the
  replay re-derivation budgeted.
* **The expectation, stated now so it cannot be read back:** at the record's last
  reading `sqp-ipm` solves 22/22 on `hs` and 2/23 on `cutest`
  (`hven-bench/docs/w2-a4-sweep.md` §3-§4), and §3.5 found the tier's face
  unambiguous on every bound-window corpus cell. **The likely TRAIN verdict is TIE or NOT
  EXERCISED on both families, and therefore `kOff`.** That outcome is a valid
  result of this protocol, not a failure of it (A0 clause 6, W3's rule: "a lever
  that cannot show a win stays at the shipped default").

---

## 6. Literature grounding (papers only)

No solver source was read for this page. The references are cited for the ideas
used; none was re-read in this round.

* **El-Bakry, Tapia and Zhang (1994)**, "A study of indicators for identifying zero
  variables in interior-point methods", *SIAM Review* 36(1). The Tapia indicator —
  the ratio of a variable's consecutive iterates, tending to 1 for a variable
  bounded away from zero and to 0 for one tending to zero — is the idea behind the
  exponent test: `e = log(z_k/z_{k-1}) / log(μ_k/μ_{k-1})` is its log form normalised
  by the barrier's own decrease. A1 applies it at the stop, to rows as well as
  bounds.
* **Güler and Ye (1993)**, "Convergence behavior of interior-point algorithms",
  *Math. Programming* 60; **Ye (1992)** on finite termination; **Mehrotra and Ye
  (1993)**, "Finding an interior point in the optimal face of linear programs",
  *Math. Programming* 62. The central path tends to the strictly complementary
  (analytic-centre) solution, and the optimal partition is identifiable once μ is
  small against the problem's strict-complementarity margin squared — the `μ ≤ d²/κ`
  threshold of §2 is the ratio-rule form of that statement. When the margin is
  below what the floor reaches, identification must come from somewhere other than
  μ (here: tier 3's exact face read).
* **Wright (1997)**, *Primal-Dual Interior-Point Methods*, SIAM — the
  identification and finite-termination chapter is the textbook statement of the
  above.
* **Facchinei, Fischer and Kanzow (1998)**, "On the accurate identification of
  active constraints", *SIAM J. Optim.* 9(1): an identification radius of order the
  square root of the KKT residual — the same `√μ` scale the weak-activity factor
  and the band use.
* **Mehrotra (1992)**, "On the implementation of a primal-dual interior point
  method", *SIAM J. Optim.* 2(4): the adaptive centering σ the tier uses; case 2's
  σ ≈ 1e-4 while infeasible is the failure mode an infeasibility-blind σ admits.
* **Gondzio (1996)**, "Multiple centrality corrections in a primal-dual method for
  linear programming", *Comput. Optim. Appl.* 6: correctors that re-centre pairs
  whose products are far from μ — a candidate for A1-R1, not for A1.
* **Wächter and Biegler (2006)**, *Math. Programming* 106 (the IPOPT paper's
  algorithmic content): μ decreases only after the barrier problem is solved to a
  tolerance proportional to μ (the monotone rule), and termination is on scaled
  errors. The contrast with the tier's adaptive μ is the substance of A1-R1.
* **Nocedal, Wächter and Waltz (2009)**, "Adaptive barrier update strategies for
  nonlinear interior methods", *SIAM J. Optim.* 19(4): adaptive μ rules
  (quality-function, LOQO) with a globalisation safeguard that falls back to a
  monotone mode when progress stalls — the safeguard class A1-R1 needs.
* **Byrd, Liu and Nocedal (1997)**, "On the local behavior of an interior point
  method for nonlinear programming": fast local convergence needs strict
  complementarity and a μ decrease coordinated with the residual; the near-
  degenerate margins of §1.3 are where both assumptions thin out.
* **Gould (1985)**, *Math. Programming* 32 — inertia on the identified face, which
  is why tier 3's acceptance is also a certificate and why handing it the
  ambiguous pairs as UNCERTAIN is safe.
* **Oberlin and Wright (2006)**, "Active set identification in nonlinear
  programming", *SIAM J. Optim.* 17(2): identification without strict
  complementarity — background for the weakly active direction, which this round
  did not observe on E1 (under-commitment 0).

---

## 7. Open questions for the settler

* **Q1 — placement.** The rule on `IpqpOptions` (this page) rather than A0's
  proposed `SqpOptions::active_margin_stopping`. Recommended; it is the tier's stop.
* **Q2 — one enum or two fields.** `kExtend` / `kExtendAndDemote` as arms of one
  field (recommended: demotion is defined after an extension, and the arms stay
  separately measured) versus `ipqp_margin_extend` + `ipqp_margin_demote` with a
  cross-field validation.
* **Q3 — A1-R1, case 2's μ collapse**, registered for A2 (complementarity-derived
  μ₀ and a μ safeguard against infeasibility; NWW 2009, Gondzio 1996). The trace in
  `out/e1-red-s100-c60.txt` is the evidence. The RED cell stays named RED until
  that lands; A1 does not claim it.
* **Q4 — A1-R2, the crossover analogue** (`IpCrossoverOptions`) — the same `μ/d²`
  geometry at the NLP IPM's stopping μ; registered, not designed here.
* **Q5 — the `1/κ = F²` coincidence.** At the defaults the classifier's threshold
  (`ipqp_face_kappa = 1e-2`) and the read's weak band (`kIpqpWeakActiveFactor = 10`)
  coincide. A1's fixture class asserts its regime against κ; if κ is ever moved
  (A3), the gate-8 and row families separate and both must be re-swept.
* **Q6 — TRAIN is thin for this lever.** §5 pre-registers the likely TIE / NOT
  EXERCISED. The brief says "measured on TRAIN against its OFF state"; the
  evidence that the rule does anything will come from I1 and I2. Does the settler
  accept C1 as the landing bar with TRAIN as the default bar, as written?
* **Q7 — M7-20** is answered for A1 (§3.5: not gating, and uninformative where
  measured). It stays open in the register for any heuristic that does read the
  census.
* **Q8 — the band fallback's width.** On n20000_af10 the band leaves 24 rows
  over-committed above its ceiling. The primary (exponent) test should catch them;
  if I2 shows it does not, widening the fallback band is a measured follow-up, not
  a default to be guessed now.

---

## 8. What is here

* This page.
* `docs/notes/data/2026-10-m7-a1/` — `README.md` (provenance and measurement
  terms), `probe/a1_probe.cpp` (the read-only instrument, final form), the build
  and run scripts, and `out/` (every transcript the page quotes).
* Two of this lane's own probe processes were stopped by exact pid, both on the
  F7 path window's walk at N = 2000 (the second after 14 minutes); the
  transcripts say so and §3.5 states the population actually measured.
