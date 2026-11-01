# M7 W2 A2 — a complementarity-derived `mu_0`: the design page and its pre-registered protocol

Date: 2026-10-23. Author: the hven W2 A2 lane. Status: a DESIGN PAGE, **committed before any
measurement**. Branch `w2-a2` off `m7` `f424c8db`; every `file:line` below was located at that
commit. `f424c8db` carries the same `include/`, `src/`, `bench/` and `tests/` as RETAKE18's pin
`6a2a58c7` (`git diff --stat 6a2a58c7 f424c8db -- include src bench tests` is empty).

Authority: the M7 brief `docs/notes/2026-09-m7-brief.md` §1 W2 A2 — "complementarity-derived
`mu_0` (Mehrotra/PIQP style — papers only) after the starting heuristic, the measured fixed
default as fallback; per-family defaults (HS 1e-3, else 1e-2, T10's sweep) become a measured
TRAIN choice, not a table" — and §0's settings contract; the A0 per-toggle contract
(`docs/notes/2026-09-m7-w2-a0-signals-design.md` §2); the A4 decision rule
(`hven-bench/docs/w2-a4-decision-rule.md`, `scripts/w2_a4_decide.py`, unchanged since `cbd26ed`);
the carried item A1-R1 (`docs/notes/2026-10-m7-w2-a1-stopping-mu-design.md` §1.2 and §2,
"Second mechanism"; settler ruling C125 Q3: A1-R1 → A2, confirmed).

## 0. The answer in one paragraph

`mu_0` lives in two engines and the record's `mu_0` — T10's sweep, T10b's adopted 1e-2, "HS
1e-3, else 1e-2" — is the **IPQP tier's** `IpqpOptions::ipqp_init_mu`. Its cold start places
every complementary pair at exactly `mu_0` (`z = mu_0 / distance`), so the start's measured
complementarity is the setting by construction: nothing about the problem enters it. A2 adds
**one field**, `IpqpOptions::ipqp_mu0_rule` {`kFixed` = today, `kComplementarity`}: after the
tier's own primal start heuristic, the multipliers are ESTIMATED by one affine-scaling solve of
the tier's own system at the unit-scaled point (Nocedal–Wright §16.6's estimate; PIQP §IV.A's
`W = I` system), shifted positive by Mehrotra's rule as PIQP states it, and `mu_0` is their
MEASURED complementarity `sᵀz / m` (PIQP Algorithm 1 step 5). The measured fixed default
`ipqp_init_mu` stays the ceiling — the same clamp the warm restart already applies — and is the
FALLBACK whenever the derived value is out of `[ipqp_min_mu, ipqp_init_mu]` or the estimate
cannot be taken. **A1-R1 is a μ-UPDATE defect, not a μ_0 defect**, but its literature remedy is
anchored at `mu_0`, so A2 carries it as a **second, separately toggled field**,
`IpqpOptions::ipqp_mu_residual_floor` (bool): the corrector's target is held at or above
`mu_s · r_k / max(1, r_s)` — the infeasible-IPM neighbourhood's infeasibility clause (Wright
1997 ch. 6; Pougkakiotis–Gondzio 2021's IP-PMM neighbourhood). Both land OFF (provisional); §6
pre-registers what decides each, and TRAIN under the A4 rule verbatim — against each toggle's
own OFF state, the `sqp-ipm` label — decides any default. The NLP IPM driver's `init_mu` is out
of A2's scope and registered (§7).

## 1. Where `mu_0` is, located

### 1.1 The IPQP tier (the record's `mu_0`; A2's subject)

| what | where |
|---|---|
| the field: `double ipqp_init_mu = 1e-2` — "the cold starting barrier parameter, and the ceiling of the warm-restart clamp" | `include/hven/drivers/sqp_solver_types.h:136-142` |
| the floor `ipqp_min_mu = 1e-12` | `sqp_solver_types.h:144-149` |
| `kappa_mu` of the warm clamp, `ipqp_mu_adopt_factor = 1.0` | `sqp_solver_types.h:236-243` |
| the tier is reached only at `qp_mode == kIpm`; the shipped `qp_mode` is `kWalk` | `sqp_solver_types.h:572` |
| validation (`> 0`, finite; `min <= init`) | `src/drivers/sqp_options.cpp:183-193` |
| the dump line `ipqp.ipqp_init_mu` | `src/drivers/sqp_options.cpp:640` |
| `mu0` declared from the setting | `src/qp/ipqp_engine.cpp:856-857` |
| **the starting heuristic**: `x = box.centre`, pushed into the box by Ipopt's `bound_push`/`bound_frac` rule (`push_into_box`, `:862-884`); `s = max(bi - Ai x, kIpqpSlackInit)`; `yi = mu_0 / s`; `zl = mu_0/(x-l)`, `zu = mu_0/(u-x)`; `ye = 0`; `zeta = x`; `lam_est = (ye, yi)` | `cold_start`, `src/qp/ipqp_engine.cpp:933-960` |
| the warm restart's §5.3 clamp `mu_0 = clamp(max(mu_meas, kappa * mu_payload), min, init)` — already a MEASURED complementarity | `clamp_mu0`, `:967-977` |
| dispatch: warm seed → §5.2 repair or degrade; no seed → `cold_start()` | `:1181-1198` |
| the measured complementarity `mu_meas := avg pair product`, re-taken every iteration | `residuals()`, `:1378` |
| the warm-kill's exactly-once cold restart (re-runs `cold_start()`) | `:2138-2150` |
| Mehrotra's oracle `sigma = (mu_aff / mu)^3` and the target clamp `mu_t = clamp(sigma * mu_meas, ipqp_min_mu, ipqp_init_mu)` | `:2330-2351` |
| the proximal schedule's gated decrease (the regularized residual must contract) | `:2190-2266` |

### 1.2 The NLP interior-point driver (not A2's subject; §7)

`IpmOptions::init_mu = 0.001` (`include/hven/drivers/ipm_solver_types.h:254`), validated
`ipm_options.cpp:270-279`, dumped `:679`. Its starting heuristic: `push_initial_point_interior`
(`src/drivers/ipm_solver.cpp:491`) projects `x` inside the bounds and seeds the bound multipliers
at `min(kBoundMultInitCap, mu0 / distance)`; `init_impl` (`:4418-4470`) sets the slacks to
`max(-c_i(x), bound_push)`, the inequality multipliers to `mu / slack` (`:4454`) and the equality
multipliers by one least-squares KKT solve. The same circular placement, one decade lower.

### 1.3 The record

* **T10** (M6 ledger `docs/notes/2026-08-m6-ledger.md:1105-1160`): the Q4 sweep of
  `ipqp_init_mu` over {1e-3, 1e-2, 1e-1, 1} on 37 cells (29 E1 + `hs10/24/33` under `SqpSolver`
  at kIpm + three indefinite fixtures + two `path_warm` corpus cells): 1e-2 wins outright (1052
  iterations / 1191 factorizations / 37 of 37); **"HS alone prefers 1e-3 (158 vs 214) —
  per-family mu is an M7 adaptivity question"**.
* **T10b** (`:1183-1240`): 1e-2 ADOPTED as a declared break; A4 at 1e-2 is 28/29 converge, RED on
  `e1_f7_n20000_af30_m1e-6` alone. The owner ruling recorded there registers "a
  complementarity-derived mu_0 (fixed 1e-2, Ruiz-scaled, as fallback)" as an M7 adaptivity lever.
  (The tier's pairs are on the CALLER's scale — Ruiz scales the KKT system only, in `assemble` and
  in `solve_system` at `:1931-1939` — so "Ruiz-scaled" describes the matrix the value is used against, not `mu`.)
* **W1 close** ("mu=1e-2 adopted"): `:1245-1270`.
* **A0** proposed `IpmOptions::mu0_rule`. That placement is SUPERSEDED here, on A1's precedent
  (C125 Q1, `IpqpOptions` placement): every number the brief cites is the tier's.
* **A4's sweep** (`hven-bench/docs/w2-a4-sweep.md` §3–§4, pin `3904a477`): `sqp-ipm` solves 22/22
  on `hs` at 10.63 factorizations per major and 2/23 on `cutest`; never re-taken since.

## 2. The mechanism A2 addresses

At `kFixed` the start is CENTRED AT THE SETTING: every pair product equals `ipqp_init_mu`, so
`mu_meas = ipqp_init_mu` exactly (the comment at `:945-947` says so). The multipliers carry no
information about the problem's prices, and the start's barrier level carries none about its
scale. T10 measured the cost of a single number serving every problem: HS wanted a decade less.
A complementarity-derived `mu_0` reverses the dependency — estimate the multipliers first, then
READ the barrier level off the start the way every later iteration reads it.

## 3. The rule `kComplementarity` (papers only)

Sources, each read as a paper or textbook: Mehrotra (1992), *On the implementation of a primal-dual
interior point method*, SIAM J. Optim. 2(4) — the starting-point shift; Nocedal & Wright (2006),
*Numerical Optimization* 2nd ed., §14.2 (Mehrotra's LP start) and §16.6 (the QP start: an
affine-scaling step from the user's point, then the multipliers taken from it); Schwan, Jiang,
Kuhn & Jones (2023), *PIQP: A Proximal Interior-Point Quadratic Programming Solver*, IEEE CDC,
arXiv:2304.00290 — §IV.A (the system (12) with `W = I` and dual block `-(1 + delta_0) I`, the
`1.5 · min` positivity shift, the `0.5 · sᵀz / Σ` complementarity shift) and Algorithm 1 step 5
(`mu_k = s_kᵀ z_k / m`); Gertz & Wright (2003), *Object-oriented software for quadratic
programming* (OOQP), ACM TOMS 29 — the same shift for QP. No solver source was read.

At a solve-ENTRY cold start with at least one complementary pair, under `kComplementarity`:

1. **The primal is the tier's own heuristic**, unchanged: `x̄`, `s̄` exactly as `cold_start`
   builds them (`:933-941`). This is the brief's "after the starting heuristic". Write `d̄_j` for
   pair `j`'s primal distance: `s̄_j` on a row, `x̄_i - l_i` / `u_i - x̄_i` on a bound side.
2. **The unit-scaled point.** Every pair's multiplier is set EQUAL to its distance, `z̄_j = d̄_j`
   (so every barrier ratio `z/d` is 1: the matrix is PIQP's (12) system, `W = I`, dual block
   `-(1 + delta_0) I`), `ye = 0`, the proximal centre `zeta = x̄` and the multiplier estimates
   `lam_est = 0` (PIQP's (12) is stated at estimates 0).
3. **One affine-scaling step** (complementarity target 0, no corrector) of the tier's OWN system
   at that point, through the tier's own `assemble` / inertia ladder / `build_rhs` /
   `solve_system` / `recover` at `(rho, delta) = (ipqp_rho_init, ipqp_delta_init)`. This is
   Nocedal–Wright §16.6's estimate, taken from the unit-scaled point rather than from a
   `mu`-placed one so that no `mu` value enters it. The estimate is `ẑ_j = z̄_j + Δz_j` for every
   pair and `ŷ_e = Δy_e`. COST: one numeric factorization (plus any inertia-ladder rungs), on the
   pattern the first iteration would have analysed anyway — PIQP §IV.A notes the same reuse. It is
   counted in `ipqp_factorizations` like any other.
4. **Mehrotra's positivity shift, as PIQP states it, on the multipliers only**:
   `Δ1 = max(0, -1.5 · min_j ẑ_j)`, `z = ẑ + Δ1`; then
   `Δ2 = 0.5 · (d̄ᵀ z) / Σ_j d̄_j`, `z = z + Δ2`. PIQP shifts the slacks too
   (`Δs0 = Δs̃0 + 0.5 · …`); the primal here is the heuristic's, already strictly interior, so only
   the multiplier half applies — a DECLARED adaptation, stated once.
5. **`mu_derived = d̄ᵀ z / m`**, `m` the pair count — PIQP Algorithm 1 step 5's measure, which is
   the tier's own `mu_meas` (`:1378`).
6. **Acceptance and the fallback.** If `mu_derived` is finite and
   `ipqp_min_mu <= mu_derived <= ipqp_init_mu`, the solve starts at `(x̄, s̄, ŷ_e, z)` with
   `lam_est = (ŷ_e, z_rows)` and `zeta = x̄`; `ipqp_mu0_derived = 1`. **Otherwise the start is
   `cold_start()` exactly** — the measured fixed default — and `ipqp_mu0_fallbacks = 1`. The same
   fallback is taken when the estimate cannot be had: the factorization budget refuses the call,
   the ladder ends on any reading other than `kOk`, or the step is non-finite.
   **Why `ipqp_init_mu` is the ceiling and not a lever:** the corrector's target is clamped to it
   (`:2350`), so a larger `mu_0` would be cut on the first corrector; T10 measured every ceiling
   above 1e-2 worse in aggregate; and the warm restart already reads its `mu_0` as
   `clamp(measured, ipqp_min_mu, ipqp_init_mu)` (`:967-977`) — the cold rule is that clamp's
   cold-start twin, with the out-of-band case falling back rather than clamping, because a
   clamped `mu` paired with an uncentred point is the one combination the corrector's target
   clamp would mis-serve.
7. **The ladder's memory is left as OFF would find it**: `rho_dem_last` and `rho_dem_max` are
   restored after step 3, so step 3 changes no later ladder trial. Its counters
   (`ipqp_inertia_retries`, `ipqp_reg_increases`, any `ipqp.reg` trace event) are real work and
   stay counted.

**Scope, declared.** The rule runs at the solve-entry cold start only. A WARM start is untouched —
§5.3's clamp already derives its `mu_0` from the measured complementarity of the repaired seed. The
WARM-KILL's cold restart (`:2143`) stays `cold_start()`: the rule's extra factorization would be
charged to an attempt whose own budget was just re-based, and the kill's "exactly once, to the
cold point" contract is kept literal. A QP with no pairs has no barrier and reads neither counter.

## 4. A1-R1, and the rule `ipqp_mu_residual_floor`

**The finding** (A1 §1.2, trace `docs/notes/data/2026-10-m7-a1/out/e1-red-s100-c60.txt`): on
`e1_f7_n20000_af30_m1e-6` μ falls 6.7e-6 → 2.4e-12 over iterations 12–19 while the primal
residual sits FLAT at 2.24e-3 with `alpha_p` 0.6–0.98, and the proximal weight `rho` sits at
8e-3 the whole time (the gate advanced at 11 and next at 19). The inner, regularized subproblem
is solved to the μ floor before the outer proximal step moves; when it does, the pairs are at
1e-12 for a problem that has changed, the steps collapse (`alpha_d` 0.04) and μ climbs back. The
same cycle at 41–44 and 56–57. With the cap at 200 the cell converges.

**Disposition: a μ-UPDATE defect, carried by A2 as its own toggle.** No `mu_0` rule fixes it — it
happens 19 iterations after the start. But the literature's remedy is a μ floor PROPORTIONAL TO
THE INFEASIBILITY and ANCHORED AT `mu_0`: Wright (1997), *Primal-Dual Interior-Point Methods*,
ch. 6 — the infeasible path-following neighbourhood `N_-inf(gamma, beta)` requires
`||r_k|| / mu_k <= beta ||r_0|| / mu_0` with `beta >= 1`; Kojima, Megiddo & Mizuno (1993), *A
primal-dual infeasible-interior-point algorithm for linear programming*, Math. Prog. 61; and the
IP-PMM neighbourhood of Pougkakiotis & Gondzio (2021), *An interior point-proximal method of
multipliers for convex quadratic programming*, Comput. Optim. Appl. 78 — the method PIQP's §III
cites as its basis ([24]). Read as a lower bound on μ it is `mu_k >= mu_0 · ||r_k|| / (beta ||r_0||)`.
It is the update half of the same question A2 asks of the start — what barrier level does THIS
iterate justify — and its scale is `mu_0`, so it belongs with A2 rather than to a new item.

**The rule** (`ipqp_mu_residual_floor = true`). Per attempt, at its first iterate, record
`mu_s = mu_meas` and `r_s = max(stationarity, primal_eq, primal_iq)` — the UNREGULARIZED relative
residuals `residuals()` computes and the stall window already reads (`:2088`). At every iteration
`floor_k = mu_s · r_k / max(1, r_s)` and the corrector target becomes
`mu_t = clamp(max(sigma · mu_meas, floor_k), ipqp_min_mu, ipqp_init_mu)`. `beta = max(1, r_s)`
satisfies `beta >= 1` and needs no tuned constant: on an infeasible start the clause is Wright's
with `beta = 1`, on a nearly feasible one the floor is `mu_s · r_k`, which keeps it below the
complementarity target at any stop (`r_k <= feas_target` there). A warm-kill re-takes
`(mu_s, r_s)` at the cold restart. `ipqp_mu_floor_binds` counts the iterations where
`floor_k > sigma · mu_meas` and `floor_k > ipqp_min_mu`.

**What is NOT built**, registered as **A2-R1**: PIQP's Algorithm 2 couples the proximal
decrease to the complementarity reduction ratio `r` (`delta_{k+1} = (1 - r) delta_k` on progress),
which attacks the same mechanism from the regularization side. hven's gate (`:2190-2266`)
decreases by a fixed factor on residual contraction. It moves the proximal schedule every kIpm
trajectory runs on; it is a separate design question.

## 5. The two fields, against A0's per-toggle contract

| clause | `ipqp_mu0_rule` | `ipqp_mu_residual_floor` |
|---|---|---|
| 1. one named field | `IpqpOptions::ipqp_mu0_rule`, enum `IpqpMu0Rule {kFixed, kComplementarity, kIpqpMu0RuleCount}`, declared last in `IpqpOptions` | `IpqpOptions::ipqp_mu_residual_floor`, `bool`, after it |
| 2. validation | negated acceptance form: `!(v == kFixed || v == kComplementarity)` throws `std::invalid_argument` naming the field and the value; the count sentinel is refused by name (`QpMode::kQpModeCount`'s and `IpqpMarginStop`'s precedent) | a `bool` refuses nothing: no predicate, a declaration-position comment saying why, an `@throws`-list entry, a test pinning both states accepted and dumped |
| 3. OFF | `kFixed` ⇒ `cold_start()` and nothing else; the three legs: U0 27×75 ×3 exact vs `docs/notes/data/2026-10-m7-scw-gates/gates/replay/`, corpus + golden lines (the golden lines move only by the DECLARED additive trailing keys below), P-SYM classified in full | `false` ⇒ `mu_t` is today's expression; same three legs |
| 4. fired counters | `ipqp_mu0_derived`, `ipqp_mu0_fallbacks` — trailing `IpqpCounters` fields, summed fold, X-table entries; both 0 at `kFixed` | `ipqp_mu_floor_binds`, same treatment; 0 at `false` |
| 5. dump | `ipqp.ipqp_mu0_rule = kFixed`, `ipqp.ipqp_mu_residual_floor = false`, last two `ipqp.` lines; `kIpqpOptionsFields` and `kSqpDumpLines` +2; the size pins move as the compiler lays the struct out (measured, not predicted) | — |
| 6. default | provisional OFF; TRAIN (§6.4) decides; a move is its own declared event | same |
| 7. labelling | no curvature reading: unlabelled | unlabelled |

`docs/counters.md` gains the three counters. The two trace golden lines
(`GoldenLineSqpSolveEndWithEveryCounterDistinct`, `…AbsenceSentinelsAsNull`) are re-derived ONCE
by the three declared additive trailing keys (`docs/trace-schema-v0.md` §8). The harness side
(every retained `hven-sqp` row's `options_digest`) moves when a pin carrying these fields is
taken; that is a declared harness options event, not a gate here.

## 6. The measurement protocol — PRE-REGISTERED

Nothing below is edited once a measured row exists. A criterion found ill-posed is reported as
ill-posed and the round STOPS; it is not amended. Arms: **OFF** (both fields at their defaults),
**M0** (`ipqp_mu0_rule = kComplementarity`), **MF** (`ipqp_mu_residual_floor = true`); **M0+MF**
is run on the fixture instruments as a DESCRIPTIVE arm only (no clause reads it). Box terms:
every leg its own `flock /tmp/box-build.lock` hold, `pgrep -a hven_bench_cell` first, one solver
at a time, `MKL_NUM_THREADS=1`; counters only, wall informational; Accelerate UNOBSERVED.

### 6.1 Gates (the OFF proof and the suites)

* **G1** U0: the three-arm 27-cell replay at the head with both fields OFF against
  `docs/notes/data/2026-10-m7-scw-gates/gates/replay/head-{walk,ssn,ipm}.csv`, exact: **0 / 0 / 0**.
  Any difference is declared, not explained away.
* **G2** E1 OFF identity: `hven_sqp_ipqp_e1_arm --gate` at OFF against
  `docs/notes/data/2026-08-m6-w1-acceptance/a4-gate-mu1e-2.csv` on that file's 66 columns, 0
  differences outside `wall_s`.
* **G3** suites: Release and Debug in full, the long test alone; P-SYM base `f424c8db` vs head,
  both configs, classified object by object (differing set = includers/readers of the changed
  headers and the engine TU; anything else is the finding).
* **G4** fire counters: all three are 0 on every OFF row of every instrument; under M0,
  `ipqp_mu0_derived + ipqp_mu0_fallbacks` equals the number of solve-entry cold starts with pairs
  (E1: 1 per cell); under MF, `ipqp_mu0_*` are 0.
* **G5** mutants: each new rule's load-bearing lines mutated (the shift factors, the band test,
  the fallback, the ladder-memory restore, the floor's max, its `max(1, r_s)`, the per-attempt
  re-take, each counter) — every mutant killed by the new tests.

### 6.2 The E1 population (29 cells, the tier direct; OFF measured in the same build)

| clause | statement | M0 predicted | MF predicted |
|---|---|---|---|
| **E1-a** no new RED | the cells failing "optimal and `ipqp_iters < 40`" are a subset of `{e1_f7_n20000_af30_m1e-6}` | PASS | PASS |
| **E1-b** cost | Σ over the 29 cells of tier + polish factorizations (`e2e_factorizations`) `<=` OFF's | FAIL (the +29 estimate factorizations are not recouped on a population 1e-2 was measured best for) | PASS |
| **E1-c** recovery | end-to-end exact recovery count (`e2e_rule_a_missed == 0 && e2e_rule_a_false_positive == 0`) `>=` OFF's, AND tier-contract count (`misclassified == 0`) `>=` OFF's | PASS | PASS |
| **E1-d** A1-R1 | `e1_f7_n20000_af30_m1e-6` returns `optimal` inside the shipped cap (60) | FAIL (the rule acts at iteration 0) | PASS |

### 6.3 T10's other families (the mu-sweep population minus E1)

`hs10/24/33` under `SqpSolver` at kIpm (`max_iter 60`), the three indefinite tier fixtures, the
two `path_warm` corpus cells — `--mu-sweep`'s families, at each arm:

| clause | statement | M0 | MF |
|---|---|---|---|
| **T-a** no loss | no cell that is `optimal` (SQP) / `kOptimal` (tier) at OFF is otherwise under the arm | PASS | PASS |
| **T-b** HS cost | Σ `ipqp_factorizations` over the three HS cells `<=` OFF's | PASS (T10: HS wanted a lower `mu_0`; the rule can only lower it) | PASS (tie expected) |

### 6.4 TRAIN — the A4 rule verbatim, per family, against each toggle's OWN OFF state

The rule of `hven-bench/docs/w2-a4-decision-rule.md` §2 and its script, unchanged, with ONE
reading stated here before any row: **`D` is the label `sqp-ipm`** (`qp_mode = kIpm`, everything
else the lane's), because that is the OFF state of these toggles (A0 clause 6, "a measured win
over the toggle's own OFF state"); the lane's `default` never reaches the tier
(`qp_mode = kWalk`), so a comparison against it would score the engine, not the rule. Each label
is `sqp-ipm` plus ONE field:

| label | field | families |
|---|---|---|
| `sqp-ipm` (D) | — | `hs`, `cutest` |
| `sqp-ipm-mu0c` | `ipqp.ipqp_mu0_rule = kComplementarity` | `hs`, `cutest` |
| `sqp-ipm-mufloor` | `ipqp.ipqp_mu_residual_floor = true` | `hs`, `cutest` |
| `sqp-ipm-mu1e-3` | `ipqp.ipqp_init_mu = 1e-3` — the table's HS entry, measured rather than tabled | `hs` only (the table's "else 1e-2" IS `D`) |

Every label runs TWICE on TRAIN only, serially, through the harness's runner
(`runners/run_serial.py`, the set's own wall ceiling: `hs` 600 s, `cutest` 300 s; the lane's
300-iteration budget), from a scratch clone of `hven-bench` at `40bb726` whose lane gains exactly
these labels, against an install of this branch's head; the determinism gate is §4's `N / N / 0`.
**HOLDOUT is never run.** Two controls run once on `cutest`: `default` and `sqp-qn`, which must
reproduce RETAKE18's chain rows (`runs/2026-10-m7-w2-retake18/run-A-{default,sqp-qn}/results.csv`)
cell for cell on `outcome`, and on `iterations`, `factorizations` and `objective` wherever both
rows carry them — the fields are unread under `kWalk`. (`options_digest` moves by the two new
dump lines and is not compared; a cell within a few seconds of the 300 s ceiling that flips
`dnf_wall` either way is reported as a wall event, not a difference.)

Per family the script reports WIN / TIE / lose / no-decision. Predictions, stated so the reading
can be scored: `sqp-ipm-mu0c` — `hs` WIN (cost), `cutest` no better than TIE; `sqp-ipm-mufloor` —
TIE on `hs` (no HS QP is expected to collapse), `cutest` TIE; `sqp-ipm-mu1e-3` — `hs` WIN.

**What the reading decides.** A field's default is RECOMMENDED ON iff its label WINS on at least
one family and loses on none (TIE, WIN or no-decision elsewhere). The default is NOT moved in this
round in either case: a move re-derives the kIpm replay arm and every kIpm pin, and is the
settler's declared event. `sqp-ipm-mu1e-3` decides nothing about a shipped default — hven ships one
`ipqp_init_mu` — it answers the brief's "per-family defaults … a measured TRAIN choice": a WIN
confirms 1e-3 for `hs` as configuration guidance; anything else retires the table entry.

## 7. Out of scope, registered

* **A2-R1** — PIQP Algorithm 2's complementarity-coupled proximal decrease (§4).
* **A2-R2** — the NLP IPM driver's `init_mu = 1e-3` (§1.2) carries the same circular placement.
  Its governor (`barrier_governor = monitored`, `ipm_solver_types.h:226`) re-reads the barrier
  from the iterate after the first step, so `mu_0`'s leverage there is the first iterates; T10
  never measured it. A complementarity-derived start for it is a separate measured item with its
  own TRAIN read on the `hven-ipm` lane.
* **A2-R3** — the warm-kill's cold restart under `kComplementarity` (§3 scope).

## 8. What is here

This page. No code lands with it; the implementation, its tests, the instruments' flags and the
measurements follow on `w2-a2`, data in `docs/notes/data/2026-10-m7-a2/`.
