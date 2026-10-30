# M7 W2 A7 — the `ws_algebra` selection rule: the design page and its pre-registered protocol

Date: 2026-10-23. Author: the hven W2 A7 lane. Status: a DESIGN PAGE, **committed before any
measurement**. Branch `w2-a7` off `m7` `f424c8db`; every `file:line` below was located at that
commit, which carries the same `include/`, `src/`, `bench/` and `tests/` as RETAKE18's pin
`6a2a58c7`.

Authority: the M7 brief `docs/notes/2026-09-m7-brief.md` §1 W2 A7 — "hven carries the field and
the per-candidate dispatch keyed on provenance; the rule that CHOOSES is a heuristic here — one
toggle, measured on TRAIN against the fixed choice"; the M6 brief's rider 1 and its M6-close
disposition (`docs/notes/2026-08-m6-brief.md`, the 2026-09-14 amendment under "Riders"); the A0
per-toggle contract (`docs/notes/2026-09-m7-w2-a0-signals-design.md` §2, and its §2.1 A7 row);
the A4 decision rule (`hven-bench/docs/w2-a4-decision-rule.md`, `scripts/w2_a4_decide.py`,
unchanged since `cbd26ed`).

## 0. The answer in one paragraph

The walk QP engine solves its working-set systems in one of two algebras, fixed per engine
instance: `kSchurBorder` (the shipped default — one full-variable K0 factorized from the seed
working set, every later change a GMSW border over it) or `kRefactorize` (a fresh factorization of
the bound-eliminated KKT after every change). The border algebra exists to make a working-set
change cheaper than a factorization; when it does not — the pin-count latch, the conditioning
guard, K0 rebuilds and elimination fallbacks — the committed record shows it paying MORE
factorizations than minors (RETAKE18 `default`: solve-end factorizations above
minors + majors + SOCs on 11 of the 18 finished cutest TRAIN cells, up to 1.57 per minor). A7
adds **one field**, `SqpOptions::ws_algebra_rule` {`kFixed` = today, `kMeasuredCost`}: under
`kMeasuredCost` the driver keeps, per solve, the border algebra's measured factorization count
against the refactorize algebra's nominal count for the same subproblems (one per working-set
change plus one), and the first time the border algebra has cost more, it switches the rest of
that solve's walk to a `kRefactorize` engine. The rule only ever moves border → refactorize; it
is inert when the configured algebra is already `kRefactorize`. It lands OFF (provisional); TRAIN
under the A4 rule verbatim, against the fixed choice, decides any default.

## 1. What exists, located

| what | where |
|---|---|
| the algebra enum `WorkingSetLinearAlgebra {kRefactorize, kSchurBorder}` | `include/hven/qp/qp_types.h:86-89` |
| **the field** `QpOptions::ws_algebra = kSchurBorder` — the FIXED CHOICE shipped today | `qp_types.h:153` |
| the default pinned by test | `tests/sqp/test_qp_engine_border.cpp:1038` |
| the field in the hot-handle options fingerprint (all eleven fields) | `qp_types.h:197-215`; `src/qp/qp_engine.cpp:97` |
| the per-engine dispatch: `eqp_candidate` → `border_candidate` or `eliminated_candidate` on `opts_.ws_algebra`, an ENGINE MEMBER no local options copy overrides (`qp_engine.cpp:1803-1806` says so) | `src/qp/qp_engine.cpp:3580-3590`; declared `include/hven/detail/qp/qp_engine.h:2390-2409` |
| the other algebra reads: hot-handle adoption (border only), the K0 reuse gate, the standstill retreat, the reuse-cache commit | `qp_engine.cpp:376`, `:558`, `:1226`, `:2921` |
| **the per-candidate dispatch keyed on provenance** (M6 W2 T7 and its fix round 1): the verdict-site face refinement picks its twin by `eqp.refine_steps > 0` (bordered) vs `== 0` (eliminated), never by `ws_algebra` | contract `qp_engine.h:563-575`; code `qp_engine.cpp:1364-1374` |
| the walk engine a solve is built with: `engine_qp_options(o)` = `o.qp` (plus `hessian_known_convex` under `kLbfgsDamped`) | `src/drivers/sqp_solver.cpp:1592-1598` |
| `engine_` built at construction and rebuilt by `set_options` | `sqp_solver.cpp:1999-2006`, `:1927-1970` |
| THE WALK INVOCATION: the one successor every walk-owned major reaches | `solve_with_walk`, `sqp_solver.cpp:5189-5218` (the solve at `:5199-5206`) |
| the major's QP counters folded into `SqpCounters` | `sqp_solver.cpp:7362-7363` |
| the cap-retry engine (its own copy of the options, `max_iter` raised) | `sqp_solver.cpp:8571-8590` |
| the restoration sub-driver: `ropts = opts_`, a fresh `SqpSolver` per phase | `sqp_solver.cpp:5621` |
| the dump line `qp.ws_algebra` | `src/drivers/sqp_options.cpp:623` |
| `QpCounters::schur_updates` exists and is deliberately NOT aggregated (A0's S4, unfolded) | `include/hven/core/solver_counters.h:47`, `:1371` |

**What the provenance dispatch buys this rule.** Each engine instance runs one algebra, and the
verdict layer never asks which: a border engine that falls back to elimination refines on the
eliminated twin because the CANDIDATE says so. So choosing the algebra per subproblem — which is
what this rule does, by choosing the engine — cannot desynchronize the refinement from the system
that produced the candidate. That is the safety property the M6 close named; the rule needs no
change to it.

**A0's placeholder name is superseded.** A0 §2.1 wrote the enum as `{kFixed, kByProvenance}`.
Provenance is the key of the existing per-candidate twin dispatch, not of a choice of algebra; the
choice here is keyed on measured cost, and the enumerator is named for what it does. The field's
placement (`SqpOptions`, no `QpOptions` field, no reuse-key move) is A0's.

**Not moot.** The record (no measurement of this round): RETAKE18's committed `default` side rows
(`hven-bench/runs/2026-10-m7-w2-retake18/run-A-default/cutest/counters-hven-sqp.csv`) put
solve-end `factorizations` above `qp_minor_iters + major_iters + soc_steps` on ACOPP118, ACOPR118,
NET3, READING6, STEENBRB/C/D/E/G and YORKNET, and below it on ACOPP57, ACOPR57, BIGBANK, DALLASM,
SAWPATH, SMBANK, ZAMB2-8; `sqp-qn`: above on STEENBRB/C/D/E/G, below on the other nine finished
cells. The two algebras therefore differ in cost on the TRAIN population, in both directions. The
M7 W2 C1 census (`hven-bench/docs/w2-c1-sqp-cost-census.md` §2-§3, an older pin) names the
mechanisms: the pin-count latch, the depth trigger and the conditioning guard.

## 2. The rule `kMeasuredCost` (papers only)

Sources, read as papers: Gill, Murray, Saunders & Wright (1990), *A Schur-complement method for
sparse quadratic programming*, in Cox & Hammarling (eds.), Reliable Numerical Computation, OUP —
the method hven's border algebra is: K0's factors are held fixed and each working-set change is
represented in a dense Schur complement C, so a change costs an update of C instead of a sparse
factorization, and K0 is refactorized when C grows or degrades; Gould (1991), *An algorithm for
large-scale quadratic programming*, IMA J. Numer. Anal. 11 — the same representation, with
refactorization forced by C's dimension and conditioning; Bartlett & Biegler (2006), *QPSchur: a
dual, active-set, Schur-complement method for large-scale and structured convex quadratic
programming*, Optim. Eng. 7 — a Schur-complement solver built for warm-started sequences of QPs,
whose premise is that a warm start leaves FEW changes to represent; Gill & Wong (2015), *Methods
for convex and general quadratic programming*, Math. Prog. Comp. 7 — the block-LU / Schur
complement updates of an active-set QP method alongside refactorization. Common to all four: the
Schur-complement representation is an AMORTIZATION — it pays when the changes it absorbs per K0
factorization outnumber what it costs to keep C, and the method itself bounds C and refactorizes.
None of the four states a rule for choosing BETWEEN the two algebras per subproblem sequence; the
rule below is hven's own cost model, stated in the A4 rule's currency (`factorizations`), and it is
labelled as such. John & Yildirim (2008, Comput. Optim. Appl. 41) and Benson & Shanno (2007,
Comput. Optim. Appl. 38) were read for the warm-start half of the question: both concern warm
starts of INTERIOR-POINT methods (the IPQP tier, `qp_mode = kIpm`), which never reads `ws_algebra`;
they bear on this rule only through its one warm-start effect (below, "what the switch gives up")
and are not load-bearing. No solver source was read.

**The witness.** Under `kRefactorize` a walk subproblem factorizes once at its seed working set and
once after every working-set change (`qp_engine.h:61-62`); each minor iteration makes at most one
change, so its nominal count is at most `minor_iters + 1`. Under `kSchurBorder` the measured count
is `QpCounters::factorizations` (the active-set search's own cost, ruling E4-17 — the summand of
`SqpCounters::factorizations` at `sqp_solver.cpp:7363`, which is the A4 rule's column). For one
solve keep

    F = Σ factorizations,   C = Σ (minor_iters + 1)

over the walk subproblems `solve_with_walk` has run on the configured border engine.

**The rule** (`ws_algebra_rule = kMeasuredCost`, configured `qp.ws_algebra == kSchurBorder`):

1. At each `solve_with_walk` entry, before the subproblem is solved: if the solve has not switched
   and `F > C`, SWITCH — for the rest of this solve, every use of the walk engine (the major's
   subproblem, its SOC re-solve, the elastic ladder's walk rungs, face refinements, the
   certified-feasibility fallback, the warm-start export) is a `kRefactorize` engine built from
   the same options with `ws_algebra` alone changed. `ws_algebra_switches` += 1.
2. After the subproblem, if the solve has not switched, `F += factorizations`,
   `C += minor_iters + 1` from that subproblem's counters. After a switch,
   `ws_algebra_switched_qps` += 1 per walk subproblem instead.
3. The switch is STICKY within the solve: once on refactorize the border algebra's cost is no
   longer observed, so there is no evidence to switch back on; a re-probe would need a tuned
   period and is registered (§7), not built.
4. Every solve starts on the configured algebra: the next `solve` entry, and `set_options`,
   restore the configured engine. The restoration sub-driver copies the whole options value
   (`ropts = opts_`), so a restoration phase runs the rule on its own subproblems with its own
   `F`, `C`; its two counters are FOLDED into the parent's.
5. `qp.ws_algebra == kRefactorize` configured: the rule is inert (nothing to switch to); both
   counters stay 0.

**Why parameter-free, and its known bias.** `F > C` compares the border algebra's measured cost
with the refactorize algebra's nominal UPPER bound on the same subproblems, cumulatively, with no
threshold: a single cheap K0 reuse or a long cheap border run keeps it far from firing, and a
border that is merely break-even never fires. The bias is the other way: factorizations both
algebras pay (inertia re-reads, dependency tests, saddle directions, the suspect ladder) are in
`F` and not in `C`, so on a subproblem dominated by them the rule can fire where refactorize would
have paid them too. That is a stated property of the witness; TRAIN measures what it costs.

**What the switch gives up.** `kRefactorize` never adopts a hot handle (`qp_engine.cpp:376`), so
after a switch the next major's K0 reuse is gone and the warm-start export carries no handle; and
the eliminated system is factorized per change. The witness has already measured that the border
algebra was not saving factorizations, so the K0 reuse it gives up is inside `F`.

**What the switch does NOT touch.** The cap-retry engine (`sqp_solver.cpp:8571`) keeps the
configured algebra — a rare path with its own options copy, registered (§7). The SSN kernel and
the IPQP tier never read `ws_algebra`. No `QpOptions` field moves and no reuse key moves: the twin
engine's options differ in `ws_algebra` only, which the fingerprint already covers, so a handle is
never adopted across algebras.

## 3. The field, against A0's per-toggle contract

| clause | `ws_algebra_rule` |
|---|---|
| 1. one named field | `SqpOptions::ws_algebra_rule`, enum `SqpWsAlgebraRule {kFixed, kMeasuredCost, kSqpWsAlgebraRuleCount}`, declared after `restoration_max_entries`, before `common` (which stays last) |
| 2. validation | the count sentinel refused by name, on `kSqpRestorationCurvatureCount`'s precedent (`src/drivers/sqp_options.cpp` RST block); `std::invalid_argument` naming the field |
| 3. OFF | `kFixed` ⇒ no accumulation, no twin engine, no swap — the code path is today's. The legs: U0 27×75 ×3 exact against `docs/notes/data/2026-10-m7-scw-gates/gates/replay/`; corpus + golden lines (moved only by the declared additive trailing keys below); suites |
| 4. fired counters | `SqpCounters::ws_algebra_switches` (switches taken; ≤ 1 per solve instance, folded from restoration) and `ws_algebra_switched_qps` (walk subproblems solved on the switched engine) — appended last, X-table entries, both 0 at `kFixed` and at a configured `kRefactorize` |
| 5. dump | `ws_algebra_rule = kFixed` in declaration position; `kSqpOptionsFields` 45 → 46, `kSqpDumpLines` 78 → 79; size pins as the compiler lays the structs out (measured, not predicted) |
| 6. default | provisional OFF; TRAIN (§4.4) decides; a move is its own declared event |
| 7. labelling | an algebra selector reads no curvature: unlabelled |

`docs/counters.md` gains the two counters; `docs/trace-schema-v0.md` §8 gains the two declared
additive trailing keys of `sqp.solve.end`, and the two golden lines are re-derived once.

## 4. The measurement protocol — PRE-REGISTERED

Nothing below is edited once a measured row exists. A criterion found ill-posed is reported as
ill-posed and the round STOPS; it is not amended. Arms: **OFF** (`kFixed`) and **ON**
(`kMeasuredCost`), everything else at the shipped defaults unless a clause says otherwise. Box
terms: every leg its own `flock /tmp/box-build.lock` hold, `pgrep -a hven_bench_cell` first in its
own command, one solver at a time, `MKL_NUM_THREADS=1`; counters only, wall informational;
Accelerate UNOBSERVED.

### 4.1 Gates

* **G1** U0 OFF: the three-arm 27-cell replay at the head with the rule OFF against
  `docs/notes/data/2026-10-m7-scw-gates/gates/replay/head-{walk,ssn,ipm}.csv`, exact
  (`scripts/compare_replay.py`, no gate): **0 / 0 / 0**. Any difference is declared, not
  explained away.
* **G2** suites: Release and Debug in full, the long test alone, all green.
* **G3** OFF counters: both counters 0 on every OFF test and fixture solve.
* **G4** mutants: each load-bearing line of the rule mutated — the strict `>`, the `+ 1`, the
  accumulation gate (before vs after the switch), the per-solve reset, the `set_options` reset, the
  inert-on-`kRefactorize` test, the twin's algebra, each counter increment, the restoration fold —
  every mutant killed by the new tests or reported as equivalent with the reason.

### 4.2 Fixtures (new tests; predicted PASS on every clause)

| clause | statement |
|---|---|
| **F-1** fires | a solve whose border algebra churns (the conditioning guard forced: `qp.schur_cond_max` small) switches exactly once, ends with the same status as OFF, at a point with `‖x_ON − x_OFF‖∞ <= 1e-6 · max(1, ‖x_OFF‖∞)`, with `factorizations` strictly below OFF's |
| **F-2** inert until it fires | on a solve where the rule never fires, ON is byte-identical to OFF in `x`, status and every counter but the two new ones (both 0) |
| **F-3** inert on refactorize | `qp.ws_algebra = kRefactorize` + ON: both counters 0, result identical to the same solve OFF |
| **F-4** per-solve | a second solve on the same solver after a switched one starts on the configured algebra (its own switch is counted afresh, and a never-firing problem after a switched solve is identical to a fresh solver's) |
| **F-5** validation and dump | the sentinel throws naming the field; both enumerators accepted and dumped |

### 4.3 U0 (the replay population, both arms)

| clause | statement | predicted |
|---|---|---|
| **U-1** ON identity | the replay with the rule ON is exact against the same record on every engine (`0 / 0 / 0`) and fires on no cell | PASS — the record's walk cells pay 1-2 factorizations against 2-12 minors (`F <= C` throughout), and the ssn / ipm arms run no walk subproblem (`qp_minors = 0`) |

U0 therefore does not exercise the rule; it is its OFF and inert-identity instrument. That is a
finding about the population, stated before the run.

### 4.4 TRAIN — the A4 rule verbatim, per family, against the fixed choice

The rule of `hven-bench/docs/w2-a4-decision-rule.md` §2 and its script, unchanged. Labels (each
one field from its base; the base is the toggle's OFF state, A0 clause 6):

| label | base `D` | field | families |
|---|---|---|---|
| `default` | — | (RETAKE18's label) | `hs`, `cutest` |
| `sqp-wsrule` | `default` | `ws_algebra_rule = kMeasuredCost` | `hs`, `cutest` |
| `sqp-qn` | — | (RETAKE18's label) | `hs`, `cutest` |
| `sqp-qn-wsrule` | `sqp-qn` | `ws_algebra_rule = kMeasuredCost` | `hs`, `cutest` |

Every label runs TWICE on TRAIN only, serially, ONE CELL PER LEG, through the harness's runner
(`runners/run_serial.py` via `scripts/w2_a4_sweep.sh`: the set's own wall ceiling, `hs` 600 s,
`cutest` 300 s; the lane's 300-iteration budget), from a scratch clone of `hven-bench` at `40bb726`
whose lane gains exactly the two labels, against an install of this branch's measured head. The
scored rows are run A; the A4 determinism gate (`schema/compare.py`, run A vs run B, `N / N / 0`)
holds for every label and family. The `sqp-qn` comparison is scored by the same script with the
`sqp-qn` rows in the `default` slot (the A2 precedent). **HOLDOUT is never run.**

**One observation hook, scratch-clone only.** The harness rows carry no field naming whether the
rule fired (the lane's side columns are an explicit list). The clone's lane appends, when the
environment variable `A7_FIRE_LOG` names a file, one line per hven-sqp cell
(`problem label ws_algebra_switches ws_algebra_switched_qps`); no row, column or dump moves.

| clause | statement | predicted |
|---|---|---|
| **T-0** controls | `default` and `sqp-qn` run A on `cutest` reproduce RETAKE18's chain rows (`runs/2026-10-m7-w2-retake18/run-A-{default,sqp-qn}/results.csv`) cell for cell on `outcome`, and on `iterations`, `factorizations`, `objective` wherever both rows carry them (`options_digest` moves by the new dump line and is not compared; a cell within seconds of the 300 s ceiling that flips `dnf_wall` is a wall event, not a difference) | PASS |
| **T-1** determinism | every label, both families: `N / N / 0` | PASS |
| **T-2** inert until it fires | every ON cell whose fire log reads `ws_algebra_switches = 0` is identical to its base's run A row on every compared `results.csv` column (`wall_s` excluded) | PASS |
| **T-3** `sqp-wsrule` vs `default`, `hs` | the A4 verdict | TIE — no HS subproblem is expected to make the border algebra cost more than one factorization per change |
| **T-4** `sqp-wsrule` vs `default`, `cutest` | the A4 verdict | WIN, low confidence: the median factorizations-per-major over the commonly solved cells is predicted lower (the fired cells are the expensive half of the default's distribution); the outcome clause is the risk — switching the algebra changes arithmetic and trajectories on the STEENBR family, NET3 and ACOPP118, which `default` solves today |
| **T-5** `sqp-qn-wsrule` vs `sqp-qn`, `hs` | the A4 verdict | TIE |
| **T-6** `sqp-qn-wsrule` vs `sqp-qn`, `cutest` | the A4 verdict | TIE — the record's firing cells under `sqp-qn` (STEENBR*) are not solved by `sqp-qn`, so the commonly solved set's median is predicted unmoved; a STEENBR conversion would make it a WIN |

The predicted FIRE set (scored, not a criterion), from the record's solve-end reading: `default`
`cutest` ⊇ {ACOPP118, ACOPR118, NET3, READING6, STEENBRB, STEENBRC, STEENBRD, STEENBRE, STEENBRG,
YORKNET}; `sqp-qn` `cutest` ⊇ {STEENBRB, STEENBRC, STEENBRD, STEENBRE, STEENBRG}; `hs` none. A
cell the record reads below the line can still fire on an early transient; the fire log says which.

**What the reading decides.** The rule's default is RECOMMENDED ON iff `sqp-wsrule` WINS on at least
one family, and NO comparison of T-3…T-6 is a `lose`. Otherwise the shipped default stays `kFixed`.
The default is NOT moved in this round in either case: a move re-derives every walk pin and the
replay's walk arm where it fires, and is the settler's declared event.

## 5. What could make a criterion ill-posed, named now

* **T-2** reads the fire log; a cell that ends `dnf_wall` has no row and is excluded from T-2 (it
  is reported). If the hook cannot be reached for a cell, that cell is reported unscored, not
  inferred.
* **F-1**'s fixture forces the churn through a non-shipped `schur_cond_max`; it proves the mechanism,
  not the value of the rule — TRAIN does that.

## 6. Scope, declared

The rule chooses between the two algebras of the walk engine only. It does not move
`schur_cap`, `schur_cond_max` or the pin-count latch (M3's subject), and it does not fold
`schur_updates` (A0's S4, which rides M3).

## 7. Out of scope, registered

* **A7-R1** — a re-probe of the border algebra after a switch (a period, which is a tuned constant)
  — only if TRAIN shows a switched cell whose later subproblems would have bordered cheaply.
* **A7-R2** — the cap-retry engine under a switched solve keeps the configured algebra (§2).
* **A7-R3** — a witness that subtracts the factorizations both algebras pay (§2, "its known bias")
  needs per-source factorization counters that do not exist.

## 8. What is here

This page. No code lands with it; the implementation, its tests, the corpus flag and the
measurements follow on `w2-a7`, data in `docs/notes/data/2026-10-m7-a7/`.
