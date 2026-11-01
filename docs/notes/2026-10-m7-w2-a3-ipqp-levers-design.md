# M7 W2 A3 — the IPQP levers as adaptivity levers: the design page and its pre-registered protocol

Date: 2026-10-28. Author: the hven W2 A3 lane. Status: a DESIGN PAGE, **committed before any
measurement**. Branch `w2-a3` off `m7` `534bc87f`; every `file:line` below was located at that
commit.

Authority: the M7 brief `docs/notes/2026-09-m7-brief.md` §1 W2 A3 — "the IPQP levers as
adaptivity levers: `ipqp_converge_slack` (slack 1 = +47 iterations over 29 cells; the 1e2
hand-off) and `kIpqpLadderSkipAfter`; the sticky trial stays NOT ADOPTED unless TRAIN reverses
it" — and §0's settings contract; the A0 per-toggle contract
(`docs/notes/2026-09-m7-w2-a0-signals-design.md` §2 and its A3a/A3b rows in §2.1); the A6 triage
rows M7-36, M7-38, M7-39 (`docs/notes/2026-10-m7-w2-a6-register-triage.md`); the A4 decision rule
(`hven-bench/docs/w2-a4-decision-rule.md` and `scripts/w2_a4_decide.py`, unchanged since
`cbd26ed`); settler rulings C129 (A2-F4: **a lost fixture cell vetoes a default move**) and C133
(the toolchain event). Precedent pages: A1, A2, A7 (`docs/notes/2026-10-m7-w2-a{1,2,7}-*.md`).

## 0. The answer in one paragraph

Two of the three named levers are measured; the third is settled by the record and only probed.
**`ipqp_converge_slack` is already a named, validated, dumped option** (A0 A3a: "its current
value IS the OFF state; no new field"), so A3 adds nothing to it and measures two arms against the
shipped 1e2 — slack 1 (the record's +47-iteration arm) and slack 1e3 (one decade looser).
**`kIpqpLadderSkipAfter` becomes `IpqpOptions::ipqp_ladder_skip_after`** (an `Index`, default 3 =
today, a negative value refused) with one fired-counter, `ipqp_ladder_zero_trial_skips`, and is
measured at 1 and at "never" (a value no attempt's iteration count can reach: Algorithm IC's
IC-1 taken literally, every iteration). **The sticky trial stays NOT ADOPTED**: the record's
measurement (T9) froze five of seven ladder fixtures, and C129 makes any fixture loss a veto. One
fact the record does not carry is stated in §3.3 — T9 measured the UNCONDITIONAL form, not the
conditional refinement T4b registered — and a cheap DESCRIPTIVE probe of both forms is
pre-registered there; it decides nothing about a default and builds no field. Every OFF-identity
leg compares against **this lane's own build of `534bc87f` on the post-2026-10-27 toolchain**,
never against a committed CSV (C133). TRAIN runs per family under the A4 rule verbatim, with `D =
sqp-ipm` (the OFF state of every A3 lever); a lever arm is RECOMMENDED only if it WINS on a
family, loses on none, AND loses no fixture cell (§5.4). No default moves in this round.

## 1. Where the levers are, located

| lever | where | today |
|---|---|---|
| `IpqpOptions::ipqp_converge_slack` | `include/hven/drivers/sqp_solver_types.h:213-220` | `double = 1e2` |
| its validation (`>= 1`, finite) | `src/drivers/sqp_options.cpp:249-255` | — |
| its dump line `ipqp.ipqp_converge_slack` | `src/drivers/sqp_options.cpp:671` | — |
| the stopping predicate `ipqp_residuals_meet_target` (opt/feas targets = `opt_tol`/`feas_tol` × slack) | `src/qp/ipqp_engine.cpp:564-573` | `QpOptions::opt_tol = feas_tol = 1e-9` (`include/hven/qp/qp_types.h:126-127`), so the tier stops at 1e-7 relative |
| the loop's two targets and the §5.5 warm-trust threshold | `src/qp/ipqp_engine.cpp:2181-2192` | — |
| `detail::kIpqpLadderSkipAfter` | `include/hven/detail/qp/ipqp_engine.h:89-92` (banner adaptation (ii) `:64-67`) | `constexpr int = 3` |
| the skip rule (IC-1's zero trial skipped once the memory exists and `consec_modified >= 3`) | `ladder_trial`, `src/qp/ipqp_engine.cpp:1712-1734` | — |
| `consec_modified`: declared `:1226`, `++`/reset after the ladder settles `:2420-2426`, reset at the warm-kill cold restart `:2281` | `src/qp/ipqp_engine.cpp` | — |
| `ladder_trial()` call sites: A2's `kComplementarity` estimate `:2050` (memory saved and restored around it) and the iteration's ladder `:2407` | `src/qp/ipqp_engine.cpp` | — |
| the counter doc naming the constant | `include/hven/core/solver_counters.h:939-958` (`ipqp_ladder_reclimbs`) | — |
| the sticky trial | NOT BUILT. Registered T4b (`docs/notes/2026-08-31-m6-w1-t4b-ladder-plan.md:199-202`, the conditional form; `:236-237`, a `+Δ` variant); measured T9 (`.superpowers/w1-t9-report.md:195-210`, `:478-488`); ruled NOT ADOPTED (`docs/notes/2026-08-m6-ledger.md:1090-1092`) | — |
| Algorithm IC's four constants in the tier | `include/hven/detail/qp/ipqp_engine.h:56-87` | the paper's `1e-4 / 100 / 8 / 1/3` |
| the NLP IPM driver's IC constants (M7-39's "driver") | `include/hven/drivers/ipm_solver_types.h:308-312` | `delta_h = 1e-5`, `incr_h = 8`, `decr_h = 0.333333`; no first-climb factor |

The tier is reached only at `qp_mode == kIpm`; the shipped `qp_mode` is `kWalk`. Every lever here
is therefore unread on the shipped configuration, and `D` for TRAIN is the `sqp-ipm` label.

## 2. The record

* **`ipqp_converge_slack`.** M6 W1 T10 (`docs/notes/data/2026-08-m6-w1-acceptance/README.md:184-227`;
  ledger `docs/notes/2026-08-m6-ledger.md:1127-1131`): at `ipqp_init_mu = 1e-2`, slack 1e2 → 1
  moves tier Rule-A recovery 9/27 → 23/27 for 690 → 737 iterations (+47 over 29 cells), 28/29
  converging either way; the one cell failing every configuration is `e1_f7_n20000_af30_m1e-6`.
  Owner ruling 2026-09-01 (ledger `:1238-1240`): slack stays 1e2 (spec §2.3 step 1: tier 3 owns
  the last two decades), scored at the tier contract plus an end-to-end column; registered as an
  M7 adaptivity lever. A1 (`docs/notes/2026-10-m7-w2-a1-stopping-mu-design.md` §1) measured slack 1
  on its probe cells as one more decade of face accuracy that does not rescue the RED cell. **No
  measurement of slack ABOVE 1e2 exists**, and none of slack on any SQP population.
* **`kIpqpLadderSkipAfter`.** Adaptation (ii) of T4b (plan `:139-145`). Never varied. T9's
  recommendation (`.superpowers/w1-t9-report.md:485-488`): "If the cost of the zero-trial is ever
  worth attacking, the lever to try is the skip rule (`kIpqpLadderSkipAfter`), which suppresses the
  zero-trial without touching the descent — untested here, registered."
* **What the skip rule does, read off the code** (stated so the predictions in §5 can be checked).
  Once `consec_modified >= kIpqpLadderSkipAfter` with a memory, every later trial is
  `max(ipqp_reg_floor, rho_dem_last / 3)` with `ipqp_reg_floor = 1e-10 > 0`, so every later
  iteration is again "modified" and the count only grows: **within one attempt the zero trial is
  never offered again once the rule has fired** (the final unmodified read is separate and
  unaffected). The value therefore sets how many modified iterations an attempt tolerates before
  it commits to a positive shift for the rest of the attempt. `1` commits after the first;
  "never" offers IC-1's zero trial every iteration, as the paper's IC does.
* **The sticky trial.** T9's A/B mutated `ladder_trial` to start at `rho_dem_last` instead of
  `rho_dem_last / 3` UNCONDITIONALLY, and five of seven T4b gate fixtures went to a budget exit
  (24/36 → 60/69, 22/31 → 60/66, 50/75 → 60/66, 20/28 → 60/66, 30/50 → 60/66), HS24 116/166 →
  86/100. Ruled NOT ADOPTED. That measurement was taken at `ipqp_init_mu = 0.1`, before T10b's
  move to 1e-2.

## 3. The decisions this page takes

### 3.1 `ipqp_converge_slack` — no code; two measured arms

A0 clause-by-clause: (1) the field exists; (2) validated, refuses `< 1`, NaN and `inf`; (3) OFF =
1e2, today; (4) A0 names the existing `ipqp_escape_*` / `ipqp_iters_*` family at R — a VALUE
lever has no "fired" event, so no new counter is added; (5) dumped; (6) TRAIN decides; (7) no
curvature reading: unlabelled. A3 measures **S1 = slack 1** (the record's arm, the tightest the
band allows) and **S3 = slack 1e3** (one decade looser; never measured). The E1 arm's existing
`--converge-slack` flag is extended from `--sweep` to every mode (`--gate`, `--families`, and the
new `--ladder`), and the scratch harness lane gains one label per arm.

### 3.2 `kIpqpLadderSkipAfter` → `IpqpOptions::ipqp_ladder_skip_after`

| clause | decision |
|---|---|
| 1. one named field | `Index ipqp_ladder_skip_after = 3`, declared LAST in `IpqpOptions` (after `ipqp_mu_residual_floor`). `detail::kIpqpLadderSkipAfter` is removed; its banner line and the two comments naming it point at the field |
| 2. validation | negated form `!(v >= 0)` throws `std::invalid_argument` naming the field and value (on `ipqp_warm_iter_budget`'s precedent, `sqp_options.cpp:256-260`). `0` is legal: the memory trial whenever a memory exists. Any value above the attempt's iteration count is legal and means "never" |
| 3. OFF | 3 ⇒ the arithmetic that runs today; proved by §5.1's G1/G2 legs against the base build and by P-SYM |
| 4. fired-counter | `IpqpCounters::ipqp_ladder_zero_trial_skips` — iterations whose first trial was the memory value because the skip rule licensed it (`rho_dem_last > 0 && consec_modified >= ipqp_ladder_skip_after` at the iteration's own ladder, `:2407`; NOT at A2's estimate site, whose memory is saved and restored). Trailing field, summed fold, X-table entry, `docs/counters.md`; the two `sqp.solve.end` golden lines re-derived once by the DECLARED ADDITIVE TRAILING KEY (`docs/trace-schema-v0.md` §8). Structurally 0 on a convex subproblem and 0 at "never". **Non-zero at the shipped 3** on a nonconvex subproblem — the rule is on today; the counter reads its firing, which is what clause 4 asks |
| 5. dump | `ipqp.ipqp_ladder_skip_after = 3`, last `ipqp.` line; `kIpqpOptionsFields` 24 → 25, `kSqpDumpLines` 80 → 81; the size pins move as the compiler lays the struct out (observed, not predicted) |
| 6. default | 3 stands unless §5.4 recommends otherwise; a move is the settler's declared event |
| 7. labelling | it selects how the inertia ladder takes its curvature reading ⇒ carries the `CURVATURE SOURCE:` label, worded as `ipqp_require_final_inertia`'s |

**Landing rule, pre-registered:** the field lands at 3 iff §5.1's gates pass; TRAIN decides only
its default. A3 measures **K1 = 1** and **KN = 1000000** ("never": above any attempt's iteration
count — the hard cap is 60 — so `consec_modified` cannot reach it).

### 3.3 The sticky trial — settled by the record; a descriptive probe only

**Disposition: NOT ADOPTED stands; no field is built.** Under C129 a default move is vetoed by
any lost fixture cell, and T9 measured five lost. What the record does NOT carry: T9's mutation
was the UNCONDITIONAL form (`rho_dem_last` always), while T4b registered the CONDITIONAL form
("if last iteration's /3 trial failed, start at `rho_dem_last` instead", plan `:200-202`), which
keeps the descent on every iteration whose memory trial was accepted. It was never measured, and
T9's numbers predate `ipqp_init_mu = 1e-2`.

**Probe P (DESCRIPTIVE; no clause, no default, no field):** two one-line `ladder_trial` patches
applied to a SCRATCH build of the measured head — never committed as code; the patch files are
committed with the data — run on `--ladder` and `--families` with every option at OFF:

* **P-U** (T9's form re-taken): trial = `max(ipqp_reg_floor, rho_dem_last)` where the head
  computes `rho_dem_last / kIpqpLadderDown`. Predicted: ≥ 1 ladder fixture goes to a budget exit
  (T9's freeze class reproduces at `init_mu = 1e-2`).
* **P-C** (the registered conditional form): as P-U only on an iteration following one whose
  memory trial was refused (a reclimb). Predicted: no fixture lost; factorizations on the
  nonconvex fixtures within ±10 % of OFF.

Reading: if P-C loses any fixture cell, the record's NOT ADOPTED extends to the conditional form;
if it loses none, it is REGISTERED (A3-R1) for its own field and TRAIN read — A3 does not build it.

### 3.4 M7-39 (paper-vs-driver IC constants) — not folded

A3 moves no IC constant: the skip rule is hven's adaptation (ii), not one of the paper's four, and
the sticky probe changes the descent's application, not its factor. The located fact for the
register: the tier runs the paper's constants (`1e-4 / 100 / 8 / 1/3`, `ipqp_engine.h:56-87`);
the NLP IPM driver runs `delta_h = 1e-5`, `incr_h = 8`, `decr_h = 0.333333` with no first-climb
factor (`ipm_solver_types.h:308-312`). M7-39 stays CARRIED.

### 3.5 What stays a constant

`kIpqpLadderInit`, `kIpqpLadderUpFirst`, `kIpqpLadderUp`, `kIpqpLadderDown` (the paper's),
`kIpqpPivotReroutePrimalMax`, `kIpqpWeakActiveFactor`, `kIpqpTightBandFactor`: no record names
any as a lever and no A3 measurement reads one.

## 4. Toolchain (C133), binding on every leg

The box moved to `intel-oneapi-mkl` 2026.2.0 on 2026-10-27 (`/opt/intel/oneapi/mkl/latest ->
2026.2`); `clang++ --version` reads 22.1.8 (Fedora 22.1.8-4.fc44), the version A2 and A7 record.
Every OFF-identity claim is **head vs this lane's own clean build of `534bc87f`, same session,
same toolchain**. Committed old-toolchain values (`docs/notes/data/2026-10-m7-a2-gates/`,
`docs/notes/data/2026-08-m6-w1-acceptance/a4-gate-mu1e-2.csv`, A2's TRAIN rows) are compared
against the BASE build only, as the descriptive leg T-0: any difference there is a TOOLCHAIN
EFFECT, reported, never a gate and never attributed to A3. Every artifact is stamped with the
compiler and the MKL directory the build resolved (read from its `CMakeCache.txt`).

## 5. The measurement protocol — PRE-REGISTERED

Nothing below is edited once a measured row exists. A criterion found ill-posed is reported as
ill-posed and the round STOPS; it is not amended. Arms: **OFF** (every A3 lever at its default),
**S1**, **S3**, **K1**, **KN** (each OFF plus ONE value). Box terms: every leg its own `flock
/tmp/box-build.lock` hold, `pgrep -a hven_bench_cell` as a separate command first, one solver at a
time, `MKL_NUM_THREADS=1`, no `timeout`; counters only, wall informational; Accelerate UNOBSERVED.

### 5.1 Gates (OFF proof, suites, fire counter, mutants)

* **G1** U0: the three-arm 27-cell replay (`hven_sqp_corpus`, cells
  `docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt`) at the head vs at the base,
  both new-toolchain Release, `scripts/compare_replay.py`: **0 / 0 / 0**.
* **G2** E1 OFF identity: `hven_sqp_ipqp_e1_arm --gate` head vs base, every base column except
  `wall_s`: **0 differences**; the head's one added column (the new counter) is 0 on every row.
  **G2b** the same for `--families`.
* **G3** suites: Release and Debug in full, the long test alone, all green; P-SYM base vs head
  (same build path, both configs) classified object by object — the differing set is the TUs that
  declare, print, validate or read the field or the counter, the includers of a changed header,
  and the moved test TUs; anything else is the finding.
* **G4** fire counter: `ipqp_ladder_zero_trial_skips` is 0 on every E1 row under every arm (convex
  F7), 0 on every row of every instrument under KN, and > 0 on at least one `--ladder` fixture at
  OFF (non-vacuity).
* **G5** mutants: the field read in `ladder_trial` (→ the literal 3; `>=` → `>`), the counter
  increment (dropped; moved to the estimate site), the validation predicate, the dump line, the
  fold — each killed by the new tests, or argued equivalent by name.
* **T-0** (descriptive): base-new-toolchain vs the committed old-toolchain U0 replay
  (`docs/notes/data/2026-10-m7-a2-gates/`) and E1 gate CSV; differences counted and classed.

### 5.2 Fixture clauses — VETO-CLASS (C129), per arm

The instruments: E1 `--gate` (29 cells, the tier direct); `--families` (`hs10/24/33` under
`SqpSolver` at kIpm, `max_iter 60`; the three indefinite row fixtures; the two `path_warm` corpus
cells); `--ladder` (NEW: the seven T4b gate-9 fixtures of
`IpqpLadderTest.AFixedPointOfTheExecutedMapIsAPointTheGateCanSee`, the tier direct at
`tr_radius = inf`). OFF is measured in the same build, same session.

| clause | statement (vs OFF) | S1 | S3 | K1 | KN |
|---|---|---|---|---|---|
| **F-a** no new RED | E1 cells failing "optimal and `ipqp_iters < 40`" ⊆ `{e1_f7_n20000_af30_m1e-6}` | PASS | PASS | PASS | PASS |
| **F-b** recovery | E1 end-to-end exact count `>=` OFF's AND tier-contract count (`misclassified == 0`) `>=` OFF's | PASS (record: more recovery at slack 1) | **FAIL** (a looser stop commits fewer rows correctly) | PASS (identical: convex) | PASS (identical) |
| **F-c** families | no cell `optimal`/`kOptimal` at OFF is otherwise under the arm | PASS | PASS | PASS | PASS |
| **F-d** ladder | every fixture's exit at the arm is certifying (`escape_reason ∈ {kNone, kIndefinite}`), not `kBudget`, and `ipqp_iters < ipqp_hard_iter_cap` — gate 9's own conditions — wherever OFF meets them | PASS (at-risk: `two_negative_eigenvalue_row`, the costliest) | PASS | PASS | PASS |

DESCRIPTIVE beside them (no veto, no decision): Σ E1 `e2e_factorizations`; Σ families
`ipqp_factorizations` over HS; Σ ladder `ipqp_factorizations`; the new counter per ladder row.
Predicted: S1 dearer on all three; S3 cheaper; K1 ≤ OFF on ladder; KN ≥ OFF on ladder; K1/KN
identical to OFF on E1 (the ladder never arms on convex F7).

### 5.3 TRAIN — the A4 rule verbatim, per family, `D = sqp-ipm`

`hven-bench/docs/w2-a4-decision-rule.md` §2 and `scripts/w2_a4_decide.py`, unmodified. `D` is the
label `sqp-ipm` (A2's reading, restated: the lane's `default` never reaches the tier). Each A3
label is `sqp-ipm` plus ONE field, proved by the lane binary's own option dumps (exactly one line
differs):

| label | field | families |
|---|---|---|
| `sqp-ipm` (D) | — | `hs`, `cutest` |
| `sqp-ipm-slack1` | `ipqp.ipqp_converge_slack = 1` | `hs`, `cutest` |
| `sqp-ipm-slack1e3` | `ipqp.ipqp_converge_slack = 1000` | `hs`, `cutest` |
| `sqp-ipm-skip1` | `ipqp.ipqp_ladder_skip_after = 1` | `hs`, `cutest` |
| `sqp-ipm-skipnever` | `ipqp.ipqp_ladder_skip_after = 1000000` | `hs`, `cutest` |

Every label runs TWICE (A, B) on TRAIN only, serially, one cell per leg, through the harness's
runner (`scripts/w2_a4_sweep.sh` → `runners/run_serial.py`, the sets' own ceilings: `hs` 600 s,
`cutest` 300 s; the lane's 300-iteration budget), from a scratch clone of `hven-bench` at `40bb726`
whose pin is this branch's measured head and whose lane gains exactly these labels. Determinism is
§4's `N / N / 0`. **HOLDOUT is never run.**

**Controls, on the new toolchain:** **C-1** (gate) — a second scratch clone at `40bb726`, its pin
moved to the BASE `534bc87f` and nothing else, runs `sqp-ipm` once (run A) on both families; it
must reproduce the head clone's `sqp-ipm` run A cell for cell on `outcome`, and on `iterations`,
`factorizations`, `objective` and `side.major_iters` wherever both rows carry them (a cell within a
few seconds of its wall ceiling flipping `dnf_wall` either way is a wall event, reported, not a
difference). **C-2** (descriptive, T-0's TRAIN leg) — C-1's rows vs A2's committed old-toolchain
`sqp-ipm` run A (`docs/notes/data/2026-10-m7-a2/out/train/tree/`), differences reported as a
toolchain effect. RETAKE18's `default`/`sqp-qn` rows are NOT re-run here: the A3 levers are unread
under `kWalk`, and those labels' new-toolchain re-take belongs to the C133 re-baseline round.

Predictions (the script's verdicts): S1 — `hs` lose (cost), `cutest` lose (cost); S3 — `hs` WIN,
`cutest` WIN or TIE; K1 — TIE on both (few TRAIN subproblems are expected to arm the ladder long
enough for the skip value to matter); KN — TIE or lose (cost) on both.

### 5.4 What decides a recommendation

A lever arm `X` is **RECOMMENDED as the default** iff all three hold:

1. its TRAIN verdict is WIN on at least one family and `lose` on none (TIE or no-decision allowed);
2. it passes every VETO-CLASS fixture clause of §5.2 (C129: a lost fixture cell vetoes);
3. its two TRAIN runs pass determinism `N / N / 0` on both families.

If both arms of one lever qualify, the reading names both with their `m(X)` and recommends neither
(the settler's call). If neither qualifies, the shipped value stands, in those words. **No default
moves in this round** in any case: a move re-derives the kIpm replay arm and every kIpm pin and is
the settler's declared event. Predicted from §5.2-§5.3: **nothing is recommended** — S1 loses on
cost, S3 is vetoed by F-b, K1 and KN tie.

## 6. Out of scope, registered here

* **A3-R1** — the conditional sticky trial (§3.3), if P-C loses no fixture: its own field and TRAIN
  read.
* **A3-R2** — the skip rule's one-way latch (§2): once fired, the zero trial never returns within an
  attempt. A rule that re-offers IC-1 after the shift has descended to `ipqp_reg_floor` is a
  separate design, not a value of this field.
* **A3-R3** — slack as a per-major schedule (loose early, tight near the NLP solution) rather than a
  constant: the brief's "adaptivity" in its strong sense, which no single TRAIN value can measure.

## 7. What is here

This page. No code lands with it; the implementation, its tests, the instruments' flags and the
measurements follow on `w2-a3`, data in `docs/notes/data/2026-10-m7-a3/`.
