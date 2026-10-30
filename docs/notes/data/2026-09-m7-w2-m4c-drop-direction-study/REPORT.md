# M7 W2 M4c — TRACE STUDY: why does the drop decision contradict the direction?

**READ-ONLY, POST-HOC, nothing ships.** No solver ran for this study, no leg was
re-run, no process was signalled, nothing was built, and no file under `src/`,
`include/`, `tests/`, `bench/` or `docs/` was touched. Every number below is read
out of flat per-minor traces M4 already took and M4b already pinned. The
question this answers is **not pre-registered anywhere** — the settler asked it
after M4b's chain had run — and no verdict in
`docs/notes/data/2026-09-m7-w2-m4b-expand-on-the-ten/verdict.txt` rests on it.

---

## 0. Provenance, and the tree this was read from

| | |
|---|---|
| repository state | branch `m7`, HEAD **`6af1d43e`** |
| source cited | **`git show 6af1d43e:…`**, not the working tree — see the note below |
| traces read | `.scratch/m7w2m4/arm/traces/off-<CELL>.csv`, the ten OFF walks M4 took and M4b pinned (`…/m4b-expand-on-the-ten/logs/pin-off-traces.txt`, 10/10 OK) |
| derived scripts | `.scratch/m7w2m4c/m4c_tails.py`, `.scratch/m7w2m4c/m4c_table.py` |
| derived tables | `.scratch/m7w2m4c/out/tails.json`, `.scratch/m7w2m4c/out/tables.md` |

**A file changed under me — twice — and it is declared.** At the time of
reading, `src/qp/qp_engine.cpp`, `include/hven/detail/qp/qp_anti_cycling_arm.h`
and `tests/qp/test_qp_anti_cycling_arm.cpp` carried **uncommitted
modifications** from the implementer working on this branch concurrently, so
every `file:line` citation below is taken from **`6af1d43e`** — the committed
state at read time — via `git show`, never from the working tree. **Before this
report was finished that implementer committed `0292bfd5`** ("W2 M4 FIX ROUND 1
— EXPAND resets AFTER its Kth iteration…"), which is now `HEAD` and which
touches `src/qp/qp_engine.cpp` only (62 insertions, 27 deletions). Every hunk of
it is inside `#ifdef HVEN_TESTING` arm code; **no line this report cites changed
in content**, and `include/hven/detail/qp/qp_engine.h` and
`include/hven/detail/qp/qp_walk_trace.h` are byte-identical across the two
commits, so every citation to those two files holds at either.

Line numbers in `qp_engine.cpp` DID shift. The mapping, for a reader at
`0292bfd5`: `718→720`, `726→728`, `750→752`, `765→767`, `777→779`, `778→780`,
`786→788`, `789→791`, `955→957`, `972→974`, `1020→1022`, `1029→1031`,
`1057→1059`, `1066→1068`, `1070→1072`, `1088` (unchanged), `1843→1842`,
`2282→2281`, `2301→2300`, `2320`/`2321`/`2331`/`2333`/`2337` (all −1:
`2319`/`2320`/`2330`/`2332`/`2336`), `2352→2351`, `2360→2359`, `2423→2428`,
`2456→2461`, `2477→2482`, `2509→2514`, `2561→2566`, `2872→2907`, `2925→2960`,
`2927→2962`, `2928→2963`, `2962→2997`.

The OFF traces predate all of it: they were produced by the pinned M4 probe
binary, a separate translation of the M4 tree.

**Configuration of the traces.** `tr_init=1` (hven's own default), `warm=1`,
`m3c_pass2=1`, `opt_tol = 1e-9`, `primal_delta = dual_mu = 1e-8` on every
record. These are the **default-options failures** — the ten cells `hven-sqp`
fails at defaults. *(This is NOT M3b's configuration: M3b's fix-round traces are
`tr_init=inf`, `warm=0`, one cold first QP, and its YORKNET QP even has a
different dimension — `n=312, me=256` there against `n=900, me=256` here. M3b's
per-cell verdicts are statements about that object, not this one.)*

---

## 1. Column semantics, grounded

Every claim below rests on these, and only these. The record's declaration is
`include/hven/detail/qp/qp_walk_trace.h`; the CSV column order is the probe's,
`docs/notes/data/2026-09-m7-w2-m4-anticycling-arm/wrappers/m4_arm_probe.cpp:514`.

| column | what it is, in the engine's own terms |
|---|---|
| `decision_multiplier` | **the dropped constraint's Dantzig VIOLATION** — `pick->violation` at `qp_engine.cpp:2423`. For a bound at LOWER it is `-z(i)` (`:2331`), at UPPER it is `+z(i)` (`:2333`); a candidate enters only if that is `> opts_.opt_tol` (`:2337`). So on a bound drop **`decision_multiplier == |z_dropped|` exactly**, and its SIGN is fixed by the side: `z < 0` at a lower bound, `z > 0` at an upper one. `decision_tol` is `opts_.opt_tol` (`1e-9`, `include/hven/qp/qp_types.h:115`). |
| `best_multiplier`, `drop_candidates`, `drop_tied`, `candidates` | the whole candidate set the rule chose from: the largest violation, the count, how many sat inside `kEngineDropTieTol` of it, and up to 8 `block:index:side:violation` entries largest-first (`qp_engine.cpp:2431`–`:2456`). |
| `z` itself | produced by `price()` at the **EQP solution**, not at `x`: `r = H·eqp.x + g + Aeᵀλe + Aiᵀλi`, `z(i) = r(i)` for pinned `i` (`qp_engine.cpp:2282`, `:2301`). |
| `p_released` | `p(dropped.idx)` for a bound (`qp_engine.cpp:777`), where `p = eqp.x − x` (`:718`). |
| `released_directional` | `dropped_directional(qp, dropped, p)` (`:778`, defined `:2477`): `+p(i)` if released from UPPER, `−p(i)` if from LOWER. **Positive means `p` points INWARD — back through the bound just released.** This is the sign-convention-free statement of the contradiction. |
| `p_inf`, `alpha`, `dx_inf`, `raw_ratio`, `ratio_tied` | `‖p‖∞` before the step; the step taken (`ratio_test`'s return, capped at 1 on the ordinary path); `alpha·‖p‖∞`; the UNCAPPED minimum ratio and its exact multiplicity (`qp_engine.cpp:1020`, `:1066`, `:2962`). |
| `step_tol` | `feas_tol · max(1, ‖x‖∞)` (`qp_engine.cpp:726`) — the SAME threshold the KKT-point test (`:786`) and the degenerate-step count (`:1029`) use. |
| `ws_size_in/out` | rows plus pinned variables (`qp_walk_trace.h:283`). **The equality rows are always in the KKT and are NOT counted here**, hence the derived `me + ws_size` below. |
| `inertia_*` | on the `eliminated` path, the reduced K's own backend triple against `(n_free, me + n_w)`. On the `border_schur` path it is **K0's** triple against an expectation the Schur complement's Haynsworth contribution has been **backed out of** (`qp_engine.cpp:1843`–`:1893`) — M3b's correction C11. `inertia_zero` is DERIVED under MKL (`inertia_zero_derived = 1` on every record here), so it is **never** an observation that there are no zero eigenvalues. |
| `active_res_inf` | `max` over the equality rows of `|Ae·x − be|`, over the WORKING inequality rows of `|Ai·x − bi|`, and over the PINNED variables of `|x_i − bound_i|`, at the iterate the pass STARTED from (`qp_engine.cpp:750`–`:765`). How far the iterate sits off the constraints its own EQP holds at equality. |
| `stat_free_inf`, `stat_scale` | `detail::free_block_stationarity` (`include/hven/detail/qp/qp_engine.h:526`); `stat_scale = max(1, ‖Hx‖∞, ‖g‖∞, ‖Aeᵀλe‖∞, ‖Aiᵀλi‖∞)` (`:530`–`:537`) — **the scale of the very terms `z` is a cancellation of**, which is what makes `|z| / (ε·stat_scale)` the right relative reading of a multiplier. |
| `curvature`, `curvature_tol`, `p_hp`, `p_sq`, `curvature_fired`, `ride_outcome` | §4c's gate `pᵀHp/pᵀp ≤ kCurvatureTolFactor·hessian_scale` (`qp_engine.cpp:955`, `:972`) and the granular refusal `ride_sign`/`ride_negative_curvature` recorded (`:2509`, `:2561`). |

### 1.1 Why the α of the re-add is exactly `0` — and why no tolerance touches it

On the add path the engine sets `x(block_idx)` to the bound **exactly**
(`qp_engine.cpp:1088`). So at the next ratio test `lower(i) − x(i)` is exactly
`0`, and the lower-bound branch computes `std::min(0.0, 0.0) / p(i) = -0.0` for
any `p(i) < -kEngineDenomTol` (`:2927`–`:2928`); the upper branch gives `+0.0`
symmetrically (`:2925`). **Eligibility is the only threshold in sight, and it is
`|p_i| > 1e-12`** (`kEngineDenomTol`, `include/hven/detail/qp/qp_engine.h:417`).
Nothing here is a tolerance call; widening `opt_tol` or `feas_tol` does not move
it. *(OBSERVED in the traces: α is exactly `0` on **100 051 of 100 051** core
units — §2 — and M3b reached the same conclusion for its two cells.)*

---

## 2. The object: the tails, the units, and the declared sample

**Tails.** For each of the ten OFF flat traces, split by `solve`, then apply
M4/M4b's own tail construction — `readouts_m4.maximal_periodic_tail` (D1: the
maximal exactly-periodic suffix on the F₀ field set, bit-exact, period searched
to 64) with `readouts_driver_m4b.predicate`'s `RECURRENT = T1∧T2∧T3`
(`|T| ≥ 0.10 N`, `|T| ≥ 8p`). The reader `classify_cause_fix1` (`F0`, `bitkey`,
`load_trace`) is imported **unmodified**; the tail construction is transcribed
(the module pulls in machinery this study does not use). **This reproduces M4b's
count exactly: 52 RECURRENT OFF solves across the ten cells** — the same 52 the
committed `tables/readouts-m4b.md` marks `REC` on its `off` rows.

**Units.** Inside each tail, a **unit** is an adjacent pair (drop pass `D`,
post-drop pass `A`): `D.action ∈ {drop_dantzig, drop_zero_probe}` and
`A.post_drop == 1`. The engine guarantees `A` is `D`'s immediate successor —
`last_drop` is consumed on the very next iteration and cleared (`:721`–`:722`).
**140 770 units** in total.

**THE CORE SUBSET — the object the brief asks about.** A unit is *core* when
`A` re-admits **exactly** the identity `D` released (`block:index:side` equal)
through the **ordinary ratio test** (`A.drop_route == none`). **100 051 core
units.** On every single one of them, without exception:

* `alpha` is **exactly `0`** — 100 051 / 100 051;
* `released_directional > 0` — the direction points **back through the released
  bound** — 100 051 / 100 051;
* the drop was a **BOUND** drop by the **Dantzig** route — 100 051 / 100 051
  (never the zero-multiplier probe);
* 67 975 lower-bound releases, 32 076 upper-bound releases.

*(All OBSERVED.)* This is M4b §4.1's finding, re-derived independently and
extended from "every cell" to "every unit of every recurrent solve".

**The declared sampling rule.** Within a tail the F₀ fields repeat bit-exactly
with the tail's period, so a representative unit is faithful *to the extent that
the classification's inputs are constant over the tail* — which is checked, not
assumed. **Every unit is classified**, and the per-solve rows in
`.scratch/m7w2m4c/out/tables.md` §A carry the **full distribution** of the
classification over all units of that tail alongside the printed representative
(the LAST **core** unit of the tail, falling back to the last unit where a tail
has no core unit). A `homogeneous` flag reports whether the 28 declared fields
are constant over the whole tail.

---

## 3. The contradiction, stated exactly — and why the premise fails

**The contradiction (OBSERVED).** The drop rule releases bound `i` because its
multiplier violates its sign condition: at a lower bound `z_i < −opt_tol`, i.e.
the reduced gradient says *increasing* `x_i` decreases the objective, so *leave
the bound upward*. The direction computed after the drop then has
`released_directional > 0` — it moves `x_i` **further into** the bound — so the
bound blocks at ratio exactly `0`, re-enters the working set, and the state
returns.

**The brief's premise — "for a convex QP with a full-rank working set those two
cannot disagree" — is sound, and BOTH of its clauses fail in this engine. The
engine's own contract text says so, in advance of any trace:**

> *"At a KKT point of the PRE-drop working set the two provably agree; they CAN
> disagree on a **DEGENERATE working set, where the multipliers split among
> dependent rows** and `lambda_c < 0` stops implying that moving off `c`
> improves anything."*
> — `include/hven/detail/qp/qp_engine.h:1507`–`:1511`

> *"**Linearly dependent working sets do not break the loop**: the delta/mu
> regularization keeps the KKT matrix factorizable, and the dependent rows split
> the multiplier between them. No anti-cycling rule is implemented; a degenerate
> stall is bounded by `max_iter`."*
> — `include/hven/detail/qp/qp_engine.h:335`–`:338`

**There is no linear-independence test anywhere on the add path.** `ratio_test`
returns a blocker and the loop admits it unconditionally (`qp_engine.cpp:1020`,
`:1057`–`:1070`); the only rank screen in the file is inside `refine_on_face`
and applies to that function alone (`qp_engine.h:947`–`:949`). *(OBSERVED from
the source; the absence is confirmed by grep over `qp_engine.cpp`,
`qp_engine.h` and `working_set.h` — no rank, independence or dependency test
exists on the walk's add or drop path.)*

**The decidable consequence.** The active constraint matrix at a pass is
`[Ae ; Ai_W ; e_i for each pinned i]`, with `me + ws_size` rows in `ℝⁿ`
(equality rows are always in the KKT; each pinned bound removes one degree of
freedom exactly as a row does). **`me + ws_size > n` is therefore an integer
PROOF that the active set is linearly dependent** — no factorization, no
tolerance, no arithmetic beyond addition. It is *sufficient, not necessary*: a
set with `me + ws_size ≤ n` can be dependent too, and the records cannot say
(§6).

---

## 4. The classification

**The arms** (each is a comparison of columns the record carries; all inputs are
printed per solve in `.scratch/m7w2m4c/out/tables.md` §A):

| arm | holds when |
|---|---|
| `d_no_direction` | `‖p‖∞ ≤ step_tol` on the post-drop pass — the drop bought no direction at all |
| `d_abs` / `d_rel` | `|p_released| ≤ 1e-12` (`kEngineDenomTol`) / `|p_released|/‖p‖∞ ≤ 1e-12` |
| `a_abs` / `a_rel` | `|λ|/opt_tol < 10` / `|λ| < ε·stat_scale` (the multiplier is below the rounding level of the gradient terms it cancels) |
| `b_dim_A` / `b_dim_D` | `me + ws_size > n` **after** / **before** the drop — the dependency proof |
| `b_perturbed` | Pardiso reported perturbed pivots on the pass |
| `c_neg_excess` | `inertia_neg > inertia_expected_neg` — **the only predicate of class (c)** |
| `c_verdict_wrong` **`[fix1]`** | the engine's own verdict is `kWrong`. **A RECORDED DIAGNOSTIC, NOT A CLASS PREDICATE.** It was advertised as one in the first issue of this report and never caused a label; it is withdrawn from the advertised predicates rather than promoted, because it fires on units whose inertia excess is *negative* (ACOPR57, ACOPP57) — the adverse verdict §6 item 4 declines to interpret — and promoting it would relabel ACOPP57's 1 777 residual units. Its key keeps the `c_` spelling it was pinned with in `tails.json`; that prefix is a frozen artifact name, not a claim of class membership. |
| `e_off_face` | `active_res_inf > 10 · step_tol` — the iterate is off the face its own EQP holds at equality |
| `e_square` | `me + ws_size == n` exactly — no remaining degrees of freedom at all |

**`[fix1]` Class (e) has no predicate.** Neither `e_off_face` nor `e_square` is
required for the label: (e) is returned **unconditionally** when no earlier arm
holds. The two arms above are descriptive readings printed for the units that
land there. Its name is **"residual / unclassified after (a)–(d), often off the
retained face"** — it is **not** a direction mechanism. Every core unit's
direction crosses the released bound (`released_directional > 0`, 100 051 /
100 051, §2); it is `α = 0` that leaves the variable in place (§1.1).

**The precedence, declared: `d → a → b_dim → c → b_perturbed → e`.** It is
**POST-HOC** — the arms and their order were chosen after the distributions were
seen. `b_dim` sits above `c` because it is an integer proof while `c` is the
engine's own gate reading, which M3b C11 says is an *algebraic* verdict about the
bordered system rather than a fresh factorization of the post-drop reduced
Hessian. Arms overlap heavily, so precedence decides the label; the overlap is
printed rather than hidden (§4.2).

### 4.1 PER-CELL CLASSIFICATION — the core subset

*(OBSERVED. One row per cell; `core units` is every drop→immediate-same-identity-
re-add pair in every recurrent OFF tail of that cell. `med` = median over the
cell's core units.)*

| cell | n / me | rec. solves | core units | **CLASS** | class distribution | med `me+\|W\|−n` (after drop) | med `\|λ\|` in ulps of `stat_scale` | med `active_res/step_tol` | inertia on the post-drop pass |
|---|---|---|---|---|---|---|---|---|---|
| **ACOPP118** | 344 / 236 | 4 | 9 212 | **b** | b 100 % | **+25** | 4.4e+15 | 8.4e+04 | as expected, verdict `kOk` |
| **ACOPR57** | 128 / 114 | 6 | 4 225 | **b** | b 100 % | **+20** | 4.1e+15 | 2.2e+08 | `kWrong` on 3 of 6 solves, excess NEGATIVE (fewer negatives than expected) |
| **DALLASM** | 196 / 151 | 5 | 3 778 | **b** | b 100 % | **+50** | 6.6e+13 | 12.3 | as expected, `kOk` |
| **STEENBRB** | 468 / 108 | 4 | 4 520 | **b** | b 100 % | **+88** | 4.4e+10 | 2.5e+07 | as expected, `kOk` |
| **STEENBRC** | 540 / 126 | 6 | 10 276 | **b** | b 100 % | **+90** | 9.7e+08 | 5.0e+07 | as expected, `kOk` |
| **STEENBRG** | 540 / 126 | 6 | 5 216 | **b** | b 100 % | **+101** | 8.0e+08 | 3.8e+07 | as expected, `kOk` |
| **SAWPATH** | 1935 / 578 | 5 | 32 878 | **b** (89.9 %) | b 29 551 / e 3 327 | **+10** | 4.5e+15 | 1.0e+09 | `+1` excess on solve 7; as expected on 8/9/10 |
| **ACOPP57** | 128 / 114 | 4 | 3 540 | **b / e, split** | e 1 777 (50.2 %) / b 1 763 (49.8 %) | **0** (range −3…+7) | 4.5e+10 | 9.1e+04 | `kWrong` on all four solves, excess **negative** (−3…−9) |
| **BRITGAS** | 1170 / 360 | 10 | 23 868 | **e (64.7 %) / c (35.3 %)** | e 15 434 / c 8 434 | **−6** (never positive) | 3.0e+15 | 2.5e-06 | `+1`/`+2` excess on solves 4, 6, 8, 10 (`kWrong` on 4/8/10, `kSuspect` on 6); as expected elsewhere |
| **YORKNET** | 900 / 256 | 2 | 2 538 | **a** | a 100 % | **−377** | **8.3e-04** | 8.2 | as expected, `kSuspect`, 1 perturbed pivot |

**Totals over the 100 051 core units: (a) 2 538 = 2.5 %, (b) 68 541 = 68.5 %,
(c) 8 434 = 8.4 %, (d) 0, (e) 20 538 = 20.5 %.**
Over all 140 770 units (core plus the non-core drop pairs): (a) 2 538 = 1.8 %,
(b) 78 448 = 55.7 %, (c) 14 916 = 10.6 %, (d) 19 906 = 14.1 %, (e) 24 962 =
17.7 %.

### 4.2 Reading each class

**(b) — 68.5 % of core units, the verdict on 7 of 10 cells. OBSERVED, and it is
a proof, not an inference.** On these units the equality rows plus the working
rows plus the pinned bounds outnumber the variables *after* the drop: `+25` on
ACOPP118, `+50` on DALLASM, `+88` on STEENBRB, `+90` on STEENBRC, `+101` on
STEENBRG, `+10` on SAWPATH, up to `+106` on STEENBRC solve 27. The active set is
linearly dependent by counting alone, so **the multipliers are not unique**, and
`z_i < 0` on one of infinitely many multiplier vectors carries no information
about whether moving off bound `i` improves anything — which is exactly what
`qp_engine.h:1509` says in advance. The Dantzig rule nevertheless picks the
largest violation with no regard for uniqueness (`qp_engine.cpp:2352`–`:2362`),
and the violations it picks are enormous: median `|λ|` of 6.6e+13 to 4.5e+15 ulps
of the gradient scale. **These are not tolerance calls — they are well-determined
numbers computed for an ill-posed question.**

**(e) — RESIDUAL / UNCLASSIFIED after (a)–(d), often off the retained face —
20.5 %, the verdict on ACOPP57 (marginally) and BRITGAS. `[fix1]`** *(The first
issue of this report named this class "the direction leaves the released
variable in place". That name is withdrawn: it is a residual class, not a
direction mechanism. All 100 051 core directions cross the released bound; what
leaves the variable in place is `α = 0`, §1.1.)* The
dependency proof does not fire (`me+|W| ≤ n`), the inertia is as expected, `|λ|`
is far above any rounding level and `|p_released|` far above `1e-12`. What the
records do show on 82.5 % of all core units is that **the iterate sits off the
very constraints its own EQP holds at equality**: median `active_res_inf /
step_tol` of 9.1e+04 on ACOPP57, 1.0e+09 on SAWPATH, 2.5e+07–5.0e+07 on the
STEENBR* family. On ACOPP57 that residual is within a factor ~2 of the dual
regularization's own floor `μ·‖λ‖ ≈ 1e-8 · 4.3e+03 ≈ 4.3e-05`
(`res/(μ·stat_scale)` = 2.07–2.25 on all four solves); **across the core units as
a whole the ratio's median is 2.0e-04, so the μ-floor is an upper bound that is
usually far from tight and is NOT a general explanation** (reported, not
claimed). ACOPP57 is additionally the one cell sitting *exactly* at the square
point: `me + |W| = n = 128` on solve 3 (888 units, `e_square`), where the null
space of the active set is `{0}` and the "EQP direction" can only be a
regularized feasibility correction — it has no descent content to agree or
disagree with a multiplier about.

**(c) — THE ENGINE'S NEGATIVE-INERTIA-EXCESS SIGNAL — 8.4 % of core units, ALL
of them BRITGAS (solves 4, 6, 8, 10). `[fix1]`** *(Named "indefinite reduced
Hessian" in the first issue of this report. Withdrawn: the post-drop reduced
Hessian's inertia was never recorded — §6 item 2 says so explicitly — so the
class names the signal that WAS recorded, and nothing beyond it.)*
`inertia_neg` exceeds `inertia_expected_neg` by `+1` or `+2` with the engine's
verdict `kWrong`. *(SAWPATH solve 7 carries a `+1` excess too, but its active
set is over-determined by `+5`, so precedence classifies it (b); the overlap is
printed in `tables.md` §A and §B rather than hidden.)* On BRITGAS solve 4 the post-drop direction is `‖p‖∞ = 2.5e+10`
with `active_res_inf = 3.1e-15` — a genuinely unbounded-looking ray inside a
working set the inertia gate flags with a negative-inertia excess `[fix1]`, and §4c's curvature gate does
not fire on it (`curvature_fired = 0`), so the ride is never offered the
direction. **Subject to C11**: on the `border_schur` path this excess is K0's
triple against a Haynsworth-adjusted expectation, not a fresh factorization of
the post-drop reduced Hessian, so it is the engine's own indefiniteness signal
read exactly as the engine reads it — evidence toward (c), not a measurement of
the reduced Hessian on the released subspace.

**(a) — 2.5 %, YORKNET alone. `[fix1]` The READING is unambiguous; "false drop"
is a HYPOTHESIS pending arm (a)'s intervention in M5, not an established fact.**
What the records show is a drop triggered by a noise-level cancellation residue
under an absolute test; that the absolute test is what *causes* the drop is the
inference §7 flags and the M5 experiment settles. `|λ| = 2.9802322387695312e-08`
— which is exactly `2⁻²⁵` — against `stat_scale = 1.6e+11`. That is **8.3e-04
ulps of the gradient scale**: three orders of magnitude *below* one rounding
unit of the numbers it is the cancellation of. `drop_tied = 8` and the recorded
candidate list carries **eight bounds at the identical bit value**
`2.9802322387695312e-08` — cancellation residue, not prices. The drop clears the
engine's bar only because `drop_worst` tests `viol > opts_.opt_tol` **absolutely**
(`qp_engine.cpp:2321`, `:2337`), while the very same file's
`free_block_stationarity` tests the same class of residual **relatively** and
says why in its own comment: *"THE THRESHOLD IS SCALED, unlike this file's other
(absolute) `opt_tol` tests: this one reads a GRADIENT RESIDUAL, which scales with
the objective, so an absolute test would refuse every suspect exit on a
large-objective problem"* (`include/hven/detail/qp/qp_engine.h:516`–`:519`).
**YORKNET's tail is that documented asymmetry, firing.** *(OBSERVED; the reading
that the asymmetry causes the drop is INFERRED — see §7.)*

**(d) — 0 core units, 19 906 non-core units (14.1 % of all).** No core re-add is
a signed-zero or weakly-active artefact: a re-add requires the ratio test to have
found `|p_released| > 1e-12` (`qp_engine.cpp:2927`), so the arm is falsifiable
and falsified on the core subset. It does fire on 14.1 % of **all** units, and
what it names there is a different and interesting thing: passes where the drop
bought **no direction at all** (`‖p‖∞ ≤ step_tol`, so the loop snaps and
immediately drops again). STEENBRB solve 13 is the clean instance — releasing
`bound:84:upper`, whose violation is `7.46e+08`, produces `‖p‖∞ = 1.7e-13`.
**A constraint whose removal does not enlarge the feasible subspace is a
dependent constraint**; this is (b) seen from the other side, and it is
concentrated on exactly the cells (b) dominates.

---

## 5. Per-solve detail

`.scratch/m7w2m4c/out/tables.md` §A carries all 52 recurrent OFF solves with, per
solve: `N`, the winning period, `|T|`, `|T|/N`, the unit count, the full
classification distribution over the units, the dropped identity, whether the
post-drop pass re-adds it, `me+|W| − n`, `|λ|/opt_tol`, `|λ|/(ε·stat_scale)`, the
candidate and tie counts, `p[released]` with its sign, `|p_r|/‖p‖∞`, `‖p‖∞`, `α`
as printed, the ratio multiplicity, the observed and expected inertia pair, the
excess, the verdict, whether the curvature gate fired, the granular ride outcome,
`active_res/step_tol` and `res/(μ·scale)`. §B is the per-cell rollup, §C the
totals.

Two shapes appear among the 52: **period-2** tails (drop ↔ add, 36 solves), where
every unit is core; and **period-4/6/8/10/11/15/51** tails (16 solves) in which
§4c's negative-curvature ride *does* step, giving drop → ride_stepped → add →
drop cycles. The ride steps and the cycle survives it: on STEENBRB solve 13 the
ride's own step is `alpha = 0` with a blocker at ratio zero, so it re-pins a bound
and moves nothing. *(OBSERVED.)*

---

## 6. What the records CANNOT decide

1. **The RANK of the active constraint matrix is never measured.** `me+|W| > n`
   is *sufficient* for dependency and nothing more. On BRITGAS (`−6` median),
   YORKNET (`−377`) and ACOPP57 solves 3–4 the set may be dependent and the
   records cannot say. **(b) is therefore under-counted, never over-counted.**
2. **The post-drop inertia of the reduced working set is deliberately NOT
   recorded.** `qp_walk_trace.h:513`–`:522` says so explicitly: the candidates'
   predicted post-drop inertia and rank evidence "would each need an extra
   factorization, so they are deliberately NOT recorded and are marked absent in
   the rule instead." So **(c) can never be settled on the released subspace
   specifically** from these traces.
3. **`inertia_zero` is DERIVED on every record here** (`inertia_zero_derived = 1`,
   MKL forcing `dim − pos − neg`). A derived `0` is arithmetic, not an
   observation, so **rank deficiency cannot be read off the zero count**.
4. **On the `border_schur` path the inertia comparison is K0's triple against a
   Haynsworth-adjusted expectation** (C11, `qp_engine.cpp:1843`–`:1893`), not a
   per-working-set factorization. ACOPP57 and ACOPR57 show a *negative* excess
   (fewer negatives than expected) with verdict `kWrong` — an adverse verdict
   that RULE-G's `neg_excess` arm does not name and that this study does not
   interpret.
5. **No condition number, smallest singular value, or second multiplier
   estimate** is recorded anywhere, so "the multipliers are not unique" is
   established by the *dimension count only* and never by a direct measurement of
   non-uniqueness.
6. **Only up to 8 drop candidates are stored** (`kMaxRecordedCandidates`,
   `qp_walk_trace.h:518`) while the counts run to 316 (SAWPATH solve 9), so the
   full candidate set is not recoverable for the large cells.
7. **Causation is not established anywhere in this report.** Every classification
   is a statement about what co-occurs with the contradiction on the traced
   passes. That a dependent working set *causes* the sign contradiction — rather
   than merely accompanying it — requires an intervention, not a trace. §8 is
   that intervention, and it has not been run.
8. **Nothing here generalizes past these ten cells at these options**, and
   nothing here is a statement about any configuration other than `tr_init = 1`,
   defaults, `MKL` backend.

---

## 7. The published remedy that matches the mechanism — papers only

*(Read as algorithmic references only. No solver source of any kind was read or
consulted, inside or outside this repository, for this section.)*

**For (b), the dominant mechanism — a working set that is not linearly
independent.** The classical remedy is not a step-length rule at all: it is to
**maintain the working set as a linearly INDEPENDENT subset of the active set**,
which is how the working set is *defined* in Gill, Murray & Wright, *Practical
Optimization* (Academic Press, 1981), Chapter 5 on linearly constrained
optimization.

**`[fix1]` The three citations, corrected — the first issue of this section
overstated two of them.** Gill & Wong, "Methods for convex and general quadratic
programming", *Mathematical Programming Computation* **7** (2015) 71–112, is the
one that states the remedy in the form this mechanism needs: the working set is
formulated as linearly INDEPENDENT, dependency is decided by a **KKT-based
dependency test**, and a dependent blocker is handled by a **multiplier-aware
exchange** — *not* by "drop the largest dependency coefficient", which no cited
paper prescribes. Nocedal & Wright, *Numerical Optimization*, 2nd ed. (Springer,
2006), §16.5 is about the **exact** method: with an independent starting working
set and an exact null-space direction, a genuine blocking constraint is
*automatically* independent — that is what Exercise 16.15 asks the reader to
prove. It prescribes **nothing** for the regularized case, where a blocker can
exist only because `aᵀp` leaked off zero, and the earlier reading of it as "admits
a blocking constraint only when doing so keeps the gradients independent" is
withdrawn. Fletcher — "Degeneracy in the presence of roundoff errors", *Linear
Algebra and its Applications* **106** (1988) 149–183, and "Resolving degeneracy in
quadratic programming", *Annals of Operations Research* **46/47** (1993) — supports
a **dedicated degeneracy procedure** over the tied/dependent constraints, rather
than a perturbation of the step.

hven does the opposite by
design (`qp_engine.h:335`–`:338`), leaning on `primal_delta`/`dual_mu` to keep the
dependent KKT factorizable — which keeps the *linear algebra* alive and destroys
the *multiplier's meaning*, exactly as `qp_engine.h:1509` predicts. Gill, Murray, Saunders & Wright's EXPAND
(*Mathematical Programming* **45** (1989) 437–474) is explicitly *not* the match
here — and M4b proved it empirically: EXPAND's whole content is a limited
acceptance of infeasibility and a working feasibility tolerance that expands over
a sequence, i.e. a **step-length** rule, and M4b §4.3 showed hven erases its
displacement one pass later on the KKT-snap branch (`qp_engine.cpp:786`–`:789`).
Hall & McKinnon (*Mathematical Programming* **100** (2004) 133–150) independently
document conditions under which EXPAND fails to prevent cycling.

**For (a), YORKNET's mechanism — a multiplier at rounding level.** The published
remedy is a **scaled (relative) optimality tolerance on the pricing test**:
declare `λ_i` non-optimal only when `|λ_i| > ε_opt · s` with `s` the scale of the
terms the multiplier is formed from, rather than against an absolute constant.
This is standard in Gill, Murray & Wright (1981) §8's treatment of termination
and dual tolerances, and GMSW (1989) §2 makes the same point about the role of
tolerances at a degenerate vertex. *(`[fix1]` The earlier appeal to Nocedal &
Wright §16.5 here is dropped with the correction above: §16.5 is about the exact
method and prescribes no tolerance policy.)* **hven already implements exactly this form one function away** —
`free_block_is_stationary(residual, scale, opts)` tests `residual <= opt_tol *
scale` (`qp_engine.h:572`) — so the remedy for (a) is an internal-consistency fix,
not an import.

**For (c), BRITGAS's minority mechanism** — the engine's negative-inertia-excess
signal `[fix1]`, which is evidence toward, and not a measurement of, an indefinite
post-drop reduced Hessian (§6 item 2) — the
literature answer is an **inertia-controlling** factorization that refuses to form
a working set whose reduced Hessian has the wrong inertia in the first place
(Gill, Murray, Saunders & Wright's inertia-controlling framework for indefinite
QP), not a degeneracy rule. That is a different piece of work from (a) and (b) and
should not be folded into them.

---

## 8. The cheapest decisive experiment — `HVEN_TESTING`-only, not proposed for
production

**Run (a) first: it is five lines, one cell, and decisive.** Add an
`HVEN_TESTING`-gated arm to `drop_worst` that replaces its two absolute tests
(`qp_engine.cpp:2321`, `:2337`) with the scaled form the same header already
uses — `viol > opt_tol · max(1, ‖Hx‖∞, ‖g‖∞, ‖Aeᵀλe‖∞, ‖Aiᵀλi‖∞)`, the scale
`free_block_stationarity` already computes — and run it OFF-vs-ON on **YORKNET
alone** under M4b's `RULE.md` §6 read-outs, reading one number: the count of core
drop→same-identity-re-add units in the recurrent tails. The prediction is sharp
and falsifiable: YORKNET's `|λ| = 2⁻²⁵` against `stat_scale = 1.6e+11` fails the
scaled test by twelve orders of magnitude — and the eight bit-identical tied
candidates say the same thing eight times, so the result cannot be a
single-constraint accident.

**`[fix1]` The prediction is split into an OFFLINE and a DYNAMIC claim** (M5
amendment A6; the first issue of this section conflated them). *OFFLINE*, replayed
over the traces already in hand with no solver at all: **all 2 538 recorded core
drops fail the scaled test**. That is computable now and is a pre-registered check
of the read-out. *DYNAMICALLY*, the altered run **cannot** promise a core count of
zero, because the trajectory diverges after the very first refused drop and the
solve may reach a different recurrent tail. Refutation therefore means the **same
candidate identity is still dropped** under the arm, or the **same matched
recurrence** (tail signature set) persists — a different tail surviving is *not* a
refutation and is reported as "diagnosis untested by this trajectory". Cost: one
arm, one cell, one pair of legs.

**Then run (b): the same shape, one order harder.** An `HVEN_TESTING` arm that
refuses to admit a blocking constraint which is linearly dependent on the current
working set — the classical independent-working-set rule — with the *diagnostic*
half recording, per tail drop, the post-drop inertia and rank evidence
`qp_walk_trace.h:513`–`:522` deliberately omits (one extra factorization per drop
pass, affordable on a traced run and on a traced run only). Read out the
same core-unit count on the seven cells (b) is the verdict for. **This is the
intervention §6 item 7 says is missing**, and it is the only thing that can turn
68.5 % of co-occurrence into a cause.

**`[fix1]` Its design was amended before it was built**
(`.superpowers/m7-w2-m5-armb-amendments.md` A1–A7), because the shape named in the
first issue of this section — "one triangular solve against the retained factor"
per candidate — cannot test what it claims to. That factor's `−μI` block is
*precisely* what makes dependent rows factorizable, so a solve against it need not
reveal the rank of the unregularized active-row matrix (A1): the rank oracle is
instead a rank-revealing QR of the **unregularized** rows `R = [Ae; A_W; eᵢ]` at a
**pre-declared** `dependency_tol`, with the regularized factor kept only for the
inertia diagnostic. The invariant is **rank**, `rank(R) = me + |W| ≤ n` after every
mutation — the row count is necessary and not sufficient, so the integer screen
survives only as a free pre-check (A2). One shared `admit_if_independent` predicate
covers **every** membership-mutating site, not just blocking constraints — ordinary
add, ride add, seed ingestion, shift additions, zero-probe restoration, repair pins
— with a dependent seed repaired at entry and a dependent MANDATORY row (equality,
fixed bound) refused with a diagnostic rather than pretended away (A3). **No
exchange is attempted in M5**: the intervention is a pass-over of dependent
candidates in ratio order, and Gill & Wong's multiplier-aware exchange is
registered as the production design item — the "largest dependency coefficient"
rule of the first issue is withdrawn as theorem-free (A4). The leak is measured
with normalized `aᵀp`, the retained rows' own `|A_W p|`, the dependency
coefficients, and a `μ` sweep, because a single `aᵀp` observation does not
establish an order (A5). Arm (a)'s prediction is split offline/dynamic as above
(A6), and the neutrality report adds feasibility and stationarity against each
leg's own tolerances, with no "production-ready" wording anywhere (A7). It is named here and **not proposed**:
whether such a rule is sound, safe or implementable is established by nothing in
this report, and nothing in this report licenses a line under `src/` or
`include/`.

---

## 9. What this study does NOT show

* **Nothing ships.** Not a default change, not a production switch, not a line
  under `src/` or `include/`, not in this study and not as a consequence of it.
* **It does not reclassify M4's or M4b's results**, whose verdicts stand
  untouched; the question answered here is not pre-registered in either rule.
* **It does not establish causation** (§6 item 7), and the precedence that turns
  overlapping arms into one label per unit is POST-HOC and declared as such.
* **It does not contradict M3b**, and it does not confirm it either: M3b's two
  cells were traced at `tr_init=inf`, cold, first QP, and YORKNET's QP there has
  a different dimension. Its `RIDE_VETOED` / `NUMERICAL_DIRECTION` verdicts are
  about that object.
* **It says nothing about any cell, option set, backend or platform other than
  the ten OFF traces named in §0.**
* **Wall-clock is not quoted anywhere** and no timing claim is made (CLAUDE.md
  §7).

SIGNOFF: M7-W2-M4C-TRACESTUDY-REPORT
