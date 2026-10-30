# M7 W2 M3a — ADDENDUM E — THE QP-CAP EXPERIMENTS: THE PRE-REGISTERED DECISION RULE

Committed BEFORE any E leg ran, including the identity leg. Nothing below is
revisable by what the legs say; a rule that turned out to be the wrong question
is reported as the wrong question and re-registered in a named successor round,
never edited here.

Settler ruling, 2026-09-21, after stage 1 closed: run E1 **and** E2 (4 legs) as a
**NEW hypothesis under this addendum**.

---

## 0. What this addendum is, and what it is NOT

**It is NOT a re-test of the radius hypothesis.** That hypothesis — "the major-0
restoration request is an artefact of the l∞ window" — was tested by `RULE.md`
stage 1 and is **CLOSED by `RULE.md` §5.5**: no radius qualified on both cells,
and §5.5 forbids re-testing it at other radii, widening it to other cells, or
rescuing it by relaxing (A1)–(A3). Nothing here reopens it. `verdict.txt` and
the `README.md` "The verdict table" section stand exactly as committed at
`6bc75ee9`, including the settler's accepted correction that `RULE.md` §5.2's
clause (A2) reads "improves **or holds**" and that STEENBRB at `tr_init = 1e3`
therefore satisfies `WINDOW_ARTEFACT` on its own.

**It is a different question**, opened by what stage 1's counters showed once
the window was gone: with no restoration phase running at all, the solve dies on
capped QP subproblems. This addendum asks whether those subproblems were going to
finish.

### The hypothesis, registered as a HYPOTHESIS

> **H-QP.** With the trust window removed, the first QP subproblem on STEENBRB
> and YORKNET is SOLVABLE by the shipped active-set walk given more minor
> iterations; the solve dies on the iteration cap, not on a broken subproblem.

It is a hypothesis, not a diagnosis. `QpOptions::max_iter`'s own header is
explicit that "raising the cap never converts a wrong answer into a right one, it
only stops refusing subproblems that were going to finish" — so a REFUTED verdict
here is as informative as a SUPPORTED one, and neither is a finding about why
these two cells fail.

### No default is changed by this addendum under any outcome

Not `qp_cap_retry`, not `QpOptions::max_iter`, not `tr_init`, not
`restoration_proximity`, not `qp.tr_radius`, not `feas_tol`. Both options
exercised here ship OFF/at-the-sentinel and stay there.

**A SUPPORTED verdict licenses NO default change.** It ranks engine work for the
settler — crash start, warm active set, cap policy — and nothing else. That
sentence is the rule, not a caveat on it.

---

## 1. The four legs, fixed here

Every leg runs this lane's `default` label — hven's own defaults plus
`common.threads = 1` and `common.print_level = 10`, exactly as
`lanes/hven/hven_bench_cell.cpp` sets them — with `--tr-init inf` and ζ = 0, and
with exactly ONE option moved off its default per arm.

| leg | cell | arm | probe arguments beyond `--deadline 300` |
|---|---|---|---|
| `STEENBRB@E1` | STEENBRB | E1 | `--problem STEENBRB --tr-init inf --qp-cap-retry on` |
| `YORKNET@E1` | YORKNET | E1 | `--problem YORKNET --tr-init inf --qp-cap-retry on` |
| `STEENBRB@E2` | STEENBRB | E2 | `--problem STEENBRB --tr-init inf --qp-max-iter 46800` |
| `YORKNET@E2` | YORKNET | E2 | `--problem YORKNET --tr-init inf --qp-max-iter 25600` |

Run in that order, each its own lock hold.

### E1 — `SqpOptions::qp_cap_retry = true`

Re-solves a subproblem the WALK answered with `QpStatus::kMaxIter`, **once, at
twice the cap that solve resolved**, warm from the capped solve's own WORKING SET
(not its point — the engine centres the trust window on the seed's primal, so
every seeding site zeroes `seed.x`). Default `false`; off means no retry is
attempted and no second engine is constructed.
(`include/hven/drivers/sqp_solver_types.h:770-820`.)

### E2 — `QpOptions::max_iter` set to an explicit absolute value

`max_iter > 0` is an EXPLICIT, ABSOLUTE cap that "wins outright, at any size"
(`include/hven/qp/qp_types.h:117-138`).

**Two scope facts, stated before the legs so neither is a surprise in the
read-out:**

1. E2's cap applies to **every** QP solve in the solve, not only the first — the
   elastic ladder's rungs, the SOC re-solve and the restoration sub-solve's own
   subproblems included, wherever they run. E1 is narrower: it fires only on a
   walk subproblem that returned `kMaxIter` and whose iterate was usable.
2. Stage 1 showed no restoration phase and no elastic activation at
   `tr_init = inf` on either cell. If an E2 leg produces one, that is a change E2
   caused and the read-out reports it as such.

### The two numbers, and what they are

`46800` and `25600` are **10× the per-major minor quotient OBSERVED in stage 1**
at `tr_init = inf` on each cell:

- STEENBRB: `qp_minor_iters = 18720` over `major_iters = 4` with
  `qp_maxiter_exits = 4`, `restoration_iters = 0`, `elastic_activations = 0`
  ⇒ every major spent exactly `4680`. 10× = **46800**.
- YORKNET: `10240 / 4` under the same three conditions ⇒ **2560**. 10× = **25600**.

Those quotients are OBSERVED (`tables/stage1-qp-counters.txt`, column
`minors/maxiter-exit`). The further reading that `4680 = 5 × (468 + 0 + 468)` and
`2560 = 5 × (312 + 0 + 200)` under
`detail::effective_qp_max_iter`'s `max(500, 5·(n + mi + #bounded))` is an
INFERENCE — `#bounded` is not printed by the probe — and **this rule does not
depend on it**. What E2 sets is the two literal integers above, recorded here
before the legs; if the inference about `#bounded` is wrong, the legs still ran at
exactly 46800 and 25600 and the rule below still reads.

---

## 2. The controls

**The control for each cell is that cell's own committed stage-1 `@trinf` row**,
`rows/stage1/<CELL>@trinf.txt`, landed at `b663b834`. They are not re-run: they
are the same binary's own output at `tr_init = inf` with both E options at their
shipped defaults, taken under the identical terms, and §3's identity leg is the
proof that the rebuilt probe still produces them.

A control is therefore never CENSORED or TRUNCATED here — both rows exist,
`deadline_fired=0`, rc 0 — so no leg is UNDETERMINED on a missing-control ground.

---

## 3. The identity requirement on the REBUILT probe

The probe gains two arguments (`--qp-cap-retry on|off`, `--qp-max-iter N`) and
five printed keys. A leg with neither argument is therefore stage 1's own solve,
and **the rebuilt binary must prove it before any E leg runs.**

**The identity leg: `STEENBRB --tr-init inf --deadline 300`, both new options at
their defaults, compared against the committed `rows/stage1/STEENBRB@trinf.txt`.**

Compared: every key present in BOTH blocks — the shared-key set — and every
ladder column present in both headers, row for row, as STRINGS (the probe prints
every float at `%.17g`, so string equality is value equality).

Excluded, **by name and only by name** — this addendum's new keys:

```
qp_cap_retry             the E1 argument, printed as a key
qp_max_iter              the E2 argument, printed as a key
qp_cap_retries           SqpCounters::qp_cap_retries
first_qp_minor_iters     SqpIterate::qp_minor_iters on history[0]
first_qp_factorizations  SqpIterate::qp_factorizations on history[0]
```

Time-valued fields are dropped BY NAME, the same total list `RULE.md` §4 fixed:
`wall_seconds`, `elapsed`, `elapsed_seconds`. None is printed by either probe
today; the list is stated in full so a future key of that kind is excluded by a
committed rule rather than by a later judgement call.

**The bar is byte equality**, with `RULE.md` §4's near-ulp fallback carried over
verbatim and unchanged: counters, statuses, integer- and enum-valued keys and
`x_k_source` are byte equality with no tolerance ever; residual-class floats and
columns accept `|a - b| <= 1e-12 * max(1, |a|, |b|)` as cross-process noise, and
**every such acceptance is printed with both values and the relative gap**. Stage
1 needed none (`NEAR_ULP_TOLERATED=0`) and none is expected here — this is the
same binary's arm on the same machine.

**An identity failure ABORTS the chain.** No E leg runs, and the failure is
reported as the finding: it would mean the two new arguments are not inert at
their defaults, and no E row taken with that binary could be believed.

---

## 4. The read-outs

Reported for all four E legs and for both controls, from the probe's own block:

- `first_qp_status`, `first_qp_elastic_applied`, `first_qp_plain_status`
  (`RULE.md` §2.1's derived key, its `(inferred)` marker and its
  `elastic_from_ipqp_escape == 0` precondition unchanged), `first_qp_verdict`,
  `first_qp_tr_radius`, `first_qp_tr_binding`
- **`first_qp_minor_iters`** and **`first_qp_factorizations`** — `SqpIterate`'s
  own per-row fields on `history[0]`, meaningful iff `qp_solved`. These are what
  answer "how many minors did the first QP actually need".
- `qp_cap_retries`, `qp_maxiter_exits`, `qp_numerical_exits`,
  `qp_failures_routed`, `qp_failures_in_a_row_peak`
- `steps_accepted`, `rejected_steps`, `major_iters`, `history_rows`,
  `qp_minor_iters`
- `factorizations`, `symbolic_analyses`, `border_refine_steps` — the
  Schur/border-stack counters this probe can reach. **`QpCounters::schur_updates`
  is NOT on `SqpCounters` and is therefore not reachable from `SqpResult`; it is
  recorded here as absent rather than substituted for.**
- `elastic_activations`, `restoration_entries`, `restoration_sub_status` (never
  printed without `restoration_entries` beside it)
- terminal `status`, `infeasibility_certified`,
  `max(feasibility_e, feasibility_i)`, `f`

**The counter accounting, stated before the legs** so no E1 row is misread:
`qp_maxiter_exits` counts capped exits **at the routing site**, so a capped
attempt that the retry RESCUED is not in it at all — it is in `qp_cap_retries`.
A retry that hit the doubled cap too is in BOTH. So
`qp_cap_retries == 1, qp_maxiter_exits == 2` reads as "one subproblem hit the
cap, was given twice the budget, and hit that too"
(`include/hven/core/solver_counters.h:1435-1535`).

---

## 5. The decision rule

Evaluated **per cell**, per arm.

- **SUPPORTED(cell, arm)** iff BOTH: the first QP ends **`optimal`**
  (`first_qp_plain_status == optimal`, which at `elastic_applied == 0` is
  `first_qp_status` itself) **AND** `steps_accepted >= 1`.
- **REFUTED(cell)** iff the first QP still ends **`max_iter`** on that cell's
  **E2** leg — the 10×-cap arm. Ten times the minors it was refused is the
  budget this rule calls enough; a subproblem that still will not finish there is
  not one the cap was holding back.
- **E1 fails while E2 succeeds** ⇒ **"SOLVABLE, BUT 2× IS NOT ENOUGH"** on that
  cell. The read-out then reports **the minors the first QP actually needed**:
  `first_qp_minor_iters` on the E2 leg, against the cell's own derived cap
  (4680 / 2560) and against 2× it.
- Any other combination is reported as exactly what it is — the table is the
  finding — and is not forced into one of the three labels above.

**TRUNCATED** (`deadline_fired = 1`, the 300 s solver deadline) and **CENSORED**
(the 900 s outer stop, rc 124, or any other non-zero rc) carry `RULE.md` §6's
meaning without change: a TRUNCATED leg's counters ARE reported and it yields
**no verdict**; a CENSORED leg produces **NO ROW**, never filled, never
interpolated, never re-run at a different budget, and yields no verdict. `E2`'s
raised cap makes truncation more likely than in stage 1 — at `tr_init = inf` the
stage-1 legs ran 31.6 s (STEENBRB) and 97.3 s (YORKNET), informational, and a 10×
cap can take a multiple of that. **That is a reported outcome, not a reason to
widen the budget mid-task.**

### The failure path

If every leg is REFUTED, or if the legs produce no verdict at all, **the addendum
reports that, with the full table and the counters behind it, and stops.** H-QP
is then recorded as not supported on these cells at these caps. It is not
re-tested at a larger cap, not widened to other cells, and not rescued by
relaxing the SUPPORTED predicate. A rule relaxed after seeing the data is not a
rule.

---

## 6. Budgets and measurement conditions

Identical to `RULE.md` §6, restated so this addendum is self-contained:

**Per leg: a 300 s SOLVER deadline inside a 900 s OUTER stop.** The solver
deadline is enforced by the probe in its iteration callback and prints
`deadline_seconds` and `deadline_fired` on every leg; the outer stop is
`timeout 900` around the process, for a solve that spends its whole budget inside
one subproblem and emits no event the deadline could fire on.

**One leg per process, serial, ALONE** under `flock /tmp/box-build.lock` with no
timeout and no fallback, released per leg; `MKL_NUM_THREADS=1`,
`OMP_NUM_THREADS=1`; pinned to one physical core with `taskset -c 2`; a `pgrep`
check for a foreign cell binary as a separate prior command, with a `PGREP_EMPTY`
marker inside the lock. These are the serial terms of CLAUDE.md §7, not the
co-run terms: **nothing here co-runs.**

**Wall-clock is INFORMATIONAL everywhere** and is quoted as a measurement
nowhere. Counters and statuses are the currency.

**HOLDOUT is never run.** Both cells are TRAIN.

---

## 7. What gets committed, and in what order

1. This file — **before any leg, including the identity leg**. Done at this
   commit.
2. The probe's two new arguments and five new keys, the identity checker for §3,
   and the chain wrapper.
3. The raw leg output, one file per leg, under `rows/e/`, unedited.
4. A machine-written fold: `verdict-E.txt` and the E tables — **no number in the
   artifact transcribed by hand**.
5. An "E legs" section appended to `README.md` and to
   `.superpowers/m7-w2-m3a-report.md`, the report's `SIGNOFF` line staying last.

Provenance: the E legs run on a REBUILT probe against the SAME hven install as
stage 1 (`libhven.a` sha256
`c79f404fd377f7ecdefc8ab016a71d65a8cba026a498b4f1f65ce303a72d5cd3`). The new
probe's own sha256 differs from stage 1's by construction and is recorded in
`PROVENANCE.txt` alongside it, with the identity leg as the proof that the
difference is inert at the defaults. Nothing built here carries
`HVEN_BENCH_PIN_SHA` — the harness CMake is never entered — so **no row from this
probe is ever cited as a harness row**, and `/home/ghecht/Projects/hven-bench` is
read-only to this addendum as it was to stage 1.
