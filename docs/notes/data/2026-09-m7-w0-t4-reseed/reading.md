# M7 W0 T4 — the reading and the disposition

## 1. The measurement

One cell, both toggle states, four `max_iter` budgets. `SqpOptions::ipqp_retire_after`
at its shipped 3 throughout. Counters, from `raw/both-states-3reps.log`:

| max_iter | state | status | majors | tier escapes | disprovals | tier retired at | entries cold/base/full | qp minors | factorizations |
|---|---|---|---|---|---|---|---|---|---|
| 1 | off | max_iter | 1 | 1 | 1 | — | 1/0/0 | 8 | 15 |
| 1 | on  | max_iter | 1 | 1 | 1 | — | 1/0/0 | 8 | 15 |
| 2 | off | max_iter | 2 | 2 | 2 | — | 2/0/0 | 14 | 21 |
| 2 | on  | max_iter | 2 | 1 | 1 | — | 1/1/0 | 8 | 22 |
| 3 | off | max_iter | 3 | 3 | 3 | **major 3** | 3/0/0 | 16 | 27 |
| 3 | on  | **optimal** | 3 | 1 | 1 | — | 1/1/1 | 8 | 27 |
| 6 | off | max_iter | 6 | 3 | 3 | **major 3** | 3/0/0 | 22 | 27 |
| 6 | on  | **optimal** | 3 | 1 | 1 | — | 1/1/1 | 8 | 27 |

Read the `max_iter = 6` pair, which is the only one where both arms are given
enough budget to finish:

* **OFF** — the tier suspects the subproblem at major 1; rung A disproves it; the
  carry was already dropped at the escape, so major 2's entry is COLD and the tier
  suspects the same shape again; and again at major 3, where three consecutive
  escapes retire it for the rest of the solve. The walk then finishes the solve to
  `f = -49.98` but never reaches the KKT tolerance: **status `max_iter` at 6 majors**.
* **ON** — major 1 is identical (nothing is staged yet). Its disproval stages rung
  A's certified duals; major 2's entry is **BASE-warm** and does NOT escape, which
  in turn leaves the engine's own cross-major carry alive, so major 3's entry is
  **FULL-warm**. One escape in the whole solve, the tally never reaches K, and the
  solve **converges — `optimal` at 3 majors**, 8 QP minor iterations against 22, at
  the same 27 factorizations and the same objective.

The mechanism is exactly the one §0 gap 5 named: the disproved entry's certified
multipliers are the state the escape threw away, and handing them back stops the
detector re-firing on the same shape.

## 2. What the tier was suspecting, and why it is a false positive

The escape's own evidence block (read off the `ipqp.escape` trace line and pinned
in `SqpDriverDisprovedEntry.TheVehicleEscapesInfeasibleSuspectOnAConsistentSubproblem`):
dual growth **1e6** across the stall window against the `kSsnDualGrowthFactor = 1e4`
threshold, with the primal residual above the feasibility target and IMPROVEMENT
NEGATIVE (−0.094: it got worse). Both of §6.3's conjuncts hold, so the escape is the
detector working as specified.

It is nonetheless a false positive, and the fallback is what proves it: rung A's
elastic copy CLOSED its slacks, so its answer is the unrelaxed subproblem's — which
is what `ipqp_suspicion_disproved` counts and what makes the certified solution
available to reseed with.

## 3. The disproved-entry charge (Q-S3 (a)), now pinned

The M6 ledger ruled that the K = 3 tally is charged on a disproval as well, "because
a disproved suspicion is evidence that the tier's DETECTOR is unsuited to this
problem", and T8 registered that the charge had no pin. It has three now, all on this
cell:

* one disproval at `ipqp_retire_after = 1` → `ipqp_tier_retired_after == 1`;
* the same one disproval at the shipped `3` → `ipqp_tier_retired_after == 0`;
* **three consecutive disprovals at the shipped 3** → `ipqp_tier_retired_after == 3`,
  with `relaxed + exhausted + rung_b == 0`, so nothing but disprovals fed the tally.

The ruling is now a fact about the code rather than a decision in a plan.

## 4. The disposition: the default ships OFF

**`ipqp_reseed_after_disproval = false`.** Not because the measurement shows nothing
— it shows a clear win — but because of what the measurement is ON.

The M7 exit criterion asks for a win on the suite's TRAIN split, and this win is on a
CONSTRUCTED cell, built for this task precisely because nothing in this repository
disproves at all (`raw/no-cell-disproves.txt`: 1200 F7 sweep cells, 33 + 33 + 81
acceptance rows, all zero; the U0 27-cell replay never reaches a tier escape in any
arm; HS reads zero and is TRAIN-embargoed until W1 H2). The T4 brief anticipated
exactly this case and fixed the rule in advance: "if there are none, build the
fixture in (b) FIRST and measure on it, and the default ships OFF with the finding
recorded." This is that finding, recorded.

It is the same disposition M7 W0 T3 reached one task earlier and for the same
reason, and the two are related: T3's `kTier` elastic rung is what turned this
false-positive shape up in the first place.

## 5. What would move it

1. **W2's TRAIN re-read.** The first population with a genuine disproval on it
   settles this; the toggle is one field and the A/B is the table in §1.
2. **The detector itself.** The honest reading of §2 is that the reseed is a
   *mitigation* for a §6.3 false positive, not a fix for it. If W2 or M8 narrows
   the detector on this shape, the reseed's win shrinks with it — and that is the
   better repair. Registered, not taken here.

## 6. Scope, stated plainly

* `SqpOptions::ipqp_reseed_after_disproval` is read at ONE site: route 5b of the kIpm
  arm, on the `kDisproved` verdict alone. Exhaustion and a relaxed answer keep
  today's cold path, and the walk route never reaches the site at all.
* OFF is byte-for-byte today's behaviour on every path — pinned by
  `SqpDriverDisprovedEntry.TheReseedIsInertWhereNothingIsDisproved` on a cell that
  disproves nothing, and by the U0 27-cell three-arm replay (0 differences).
* No counter was added and no CSV schema moved. The reseeded entry is visible
  through columns that already exist: `ipqp_restart_repairs`, and the
  `ipqp.restart` trace line's `grade`.

## 7. The instruction counts (INFORMATIONAL)

`perf/both-states-perf.log`, one process per toggle state over the same four-budget
sweep, plus the process-startup baseline so a reader can subtract the harness:

| cell | instructions | minus baseline |
|---|---|---|
| `baseline_no_test` (a filter matching no test) | 13 707 567 | — |
| `reseed_off` | 38 651 316 | 24 943 749 |
| `reseed_on` | 31 437 901 | 17 730 334 |

−29 % on the ON arm. It is the same story the counters tell and it is reported for
that reason only; nothing above rests on it. No wall-clock number was taken at all
— the measurement CSV has no wall column.

## 8. Everything that was verified, in one place

| gate | result |
|---|---|
| Release `hven_sqp_tests` / `hven_drivers_tests` | **1251 / 1251** and **134 / 134** |
| Debug `hven_sqp_tests` (the >600 s `Continuation.ProbeBudgetBoundsAFailingProposal` excluded) / `hven_drivers_tests` | **1250 / 1250** and **134 / 134** |
| that one Debug test, ALONE and detached | `raw/gates/debug-continuation-alone.log` |
| U0 27-cell three-arm replay vs the M6 close-gate CSVs, under the DEFAULT | **27 cells / 75 columns / 0 differences**, all three arms |
| install smoke | PASSED, **19 standalone-include TUs** |
| export contract | `== export contract OK for platform=linux, align=32` |
| P-SYM, same path, both configs | `raw/psym-same-path/` |
| clang-format | 36 violations at HEAD **and** at the base `0be12ec3` — **0 new, 0 removed** |
