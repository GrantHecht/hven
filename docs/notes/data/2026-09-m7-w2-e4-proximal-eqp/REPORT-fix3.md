<!-- M7 W2 E4 (the proximal EQP) -- the implementer report for fix round 3, copied VERBATIM from
     .superpowers/m7-w2-e4-fix3-report.md. Nothing below this comment is edited: the
     bytes after this preamble are that file byte for byte (cmp-checked). -->

# M7 W2 E4 — FIX ROUND 3 report (the contract text, one guard, the prose corrections)

Implementer report, 2026-09-27. Brief: `.superpowers/m7-w2-e4-brief.md` §5 (rulings E4-27,
E4-28, E4-29). Review acted on: `.superpowers/m7-w2-e4-review-fable.md` items (2)–(5), (11)
and verdict items 1–8. Worktree `/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9`,
branch `e4`. Base `799b527d` (= m7 `25e11280`, sources identical). Round dir `.scratch/e4/`
(`STATE.md` current, logs under `.scratch/e4/logs3/`, corpus rows under `.scratch/e4/corpus/k3-*`).

**DELIVERABLE: ONE NEW COMMIT `5ed7bf16` on top of `799b527d`.** 7 files, no attribution
trailer of any kind (`git log --format=%B -1 | grep -ciE "co-authored|claude|anthropic|opus|generated with"`
= 0), `git rev-list --count 799b527d..HEAD` = 1, `git status --porcelain` empty, staged with
explicit pathspecs. The settler cherry-picks it onto m7's `25e11280`.

Labels: **OBSERVED** = read in the tree, in a raw log, or in a diff I ran. **INFERRED** = my
reasoning over those.

---

## 0. Headline

The round is text plus one one-line guard, and it moved nothing measurable. **OBSERVED:** the
51-dump corpus on both algebras with E2 on is identical to fix round 2's rows on EVERY column
(`rowcmp.py k1 k3`: `TOTAL ROWS WITH ANY COLUMN CHANGE: 0`, 51 rows per algebra on each side);
the T7 feasible census, `HsBattery`, `HsSweepCorpus` and `B1Gate` are green (38/38); U0's whole
comparison block is BYTE-IDENTICAL to fix round 2's (260 lines each); Release is 0 failed of
2870 and Debug 0 failed of 2869 with the four declared skips, both identical to fix round 2
test by test; the > 600 s test alone passes at 619.78 s. **No mover anywhere, so nothing to
declare with a cause.**

E4-4 as amended (E4-27) is now stated ONCE, tersely, in `qp_engine.h` §5, and the five other
contract sites the review named point at it. The STEP-currency runaway guard is out of the
contract text at both sites, with one clause recording that it was built, measured and removed
so a later reader does not re-add it. `polish_terminal_point`'s declaration no longer
contradicts ruling E4-17.

---

## 1. Item 1 — the exhausted-ladder text aligned to E4-27 (six review sites + two I found)

The rule as it now reads, in `include/hven/detail/qp/qp_engine.h` §5 (the one full statement;
every other site points here):

> THE RULE, E4-4 AS AMENDED (ruling E4-27; the other sites point here): no step is taken
> toward a refused answer WHILE THE LADDER HAS RUNGS; an EXHAUSTED ladder is ADVISORY for BOTH
> refusals — the walk steps and this section's TERMINAL SITE decides, because exhaustion says
> the residual is IRREDUCIBLE (what an inconsistent row set looks like) rather than
> untrustworthy, and the certificate is a claim about the point the walk stops at and never
> about the refused pass — and only a NON-FINITE answer is TERMINAL (kNumericalError at the
> finite iterate the walk stands on). MEASURED both ways: the terminal form cost an
> inconsistent pair its certified kInfeasible, and classifying AT the refused pass produced 16
> false census certificates. `eqp_residual_refusals` and `eqp_row_drift_refusals` carry the
> reason.

Sites changed (each verified by `grep` before and after; OBSERVED):

| site | was | now |
|---|---|---|
| `include/hven/detail/qp/qp_engine.h` §5 (the refusal paragraph, was ~:344) | "an exhausted ladder reports kNumericalError at the iterate the walk stands on. Never a step toward an answer that does not solve its own system." | the rule above, and the paragraph now names BOTH refusals as riders of the ladder |
| `include/hven/detail/qp/qp_engine.h` `eqp_answer_drifts_off_met_rows` decl (was :1712–1713) | "AND IT IS ADVISORY: an EXHAUSTED ladder takes the step anyway, unlike the residual refusal, which ends the solve." | "AND AN EXHAUSTED LADDER IS ADVISORY, exactly as it is for the residual refusal — section 5 states the rule." (its ACOPR57/YORKNET measurement kept verbatim) |
| `include/hven/core/solver_counters.h` `QpCounters::eqp_residual_refusals` | "an eliminated-path refusal ends the solve kNumericalError at the iterate the walk stands on" | "rides the suspect-stall ladder — no step toward the answer WHILE THE LADDER HAS RUNGS, and an EXHAUSTED ladder is ADVISORY … only a NON-FINITE answer being terminal … So a solve can read many refusals and still kOptimal" |
| `include/hven/core/solver_counters.h` `QpCounters::eqp_row_drift_refusals` | "ADVISORY, NOT TERMINAL … An EXHAUSTED ladder ends the solve for a residual refusal" | "ADVISORY ONCE THE LADDER IS EXHAUSTED — as the residual refusal is, on the same rule … This is its OWN field because the CAUSE differs, not the response" |
| `include/hven/detail/qp/qp_walk_trace.h` `eqp_residual_refused` | "a refusal that reaches the walk ends the solve `kNumericalError`, so the column is how a trace names that exit" | "rides the suspect-stall ladder, and an EXHAUSTED ladder is ADVISORY … so the column names a pass whose answer the check would not stand behind, not an exit" |
| `docs/trace-schema-v0.md` `eqp_row_drift_refusals` row | "is ADVISORY where that one is terminal — an exhausted ladder takes the step anyway" | "an EXHAUSTED ladder is ADVISORY for this refusal and for that one alike … only a non-finite answer being terminal" |

**Two more the grep found, both fixed (OBSERVED):**

- `docs/trace-schema-v0.md`'s `eqp_residual_refusals` row said "the pass took a retreat instead
  of stepping" with no qualifier; it now names the two retreats and says the exhausted ladder
  is advisory, as for the drift counter. (Not on the review's list, same defect class.)
- `src/qp/qp_engine.cpp`'s UPPER in-loop block (the one before the ladder) asserted the E4-4
  LETTER — "E4-4's rule is that no step may be taken toward such an answer" and "Only an
  EXHAUSTED ladder reports kNumericalError, at the iterate the walk stands on" — contradicting
  the lower block twenty lines below it, which was already right. It now states the
  with-rungs half and points down and at §5.

**Grep evidence (OBSERVED, post-commit):** `grep -rn "ends the solve" include src docs/trace-schema-v0.md`
returns exactly two hits, both about the SQP driver's consecutive-failure budget
(`include/hven/drivers/sqp_solver_types.h:725`, `src/drivers/sqp_solver.cpp:6610`) and neither
about a refusal. No site in `include/`, `src/` or the schema now claims a refusal is terminal.

---

## 2. Item 2 — the STEP-currency runaway guard out of the contract text

**OBSERVED, the code first:** `unbounded_artifact_scale` has exactly two consumers in the
library — `QpEngine::is_runaway` (`src/qp/qp_engine.cpp:2865`, the iterate's currency) and the
terminal classification's comment at `:1528`. There is no step-currency guard. The review's
reading is confirmed.

Both text sites are corrected:

- `qp_engine.h` §5: the "two currencies" paragraph is now one currency, the ITERATE's, with
  its proximal-centring reason unchanged, plus one clause: a second guard in the STEP's
  currency **was built, MEASURED and REMOVED** — at the scale the three runaway fixtures need,
  it fired on 10 HS cells that are not runaways and cost HS10 and HS27 — "so the iterate and
  the ridge scale are the only currencies the guard is asked in (eqp_solve.h says the same, and
  says why ||p|| cannot carry it)". Keeping that clause is deliberate: the alternative is a
  future reader re-deriving the removed guard from the step's arithmetic.
- `include/hven/qp/qp_types.h` `QpOptions::primal_delta` (was ~:105): "asked in both
  currencies — the iterate's and the step's" → "asked on the ITERATE and on the ridge scale —
  NOT on the step, where a guard was built, measured and removed"; the closing clause now says
  what replaces ‖p‖ (the walk's own KKT-point test and that guard) instead of claiming a
  two-currency guard buys the bound.

`eqp_solve.h:74–76`'s wording, which the review called the correct one, is untouched.

---

## 3. Item 3 — the polish charge sentence (E4-17)

`qp_engine.h`'s `polish_terminal_point` declaration said "One local factorization (charged to
`factorizations`, as refine_terminal_mu_free's is)". It now reads: "One local factorization,
charged to `terminal_polish_solves` and NOT to `factorizations` (ruling E4-17;
solver_counters.h states the contract at both fields, and the asymmetry with
refine_terminal_mu_free, whose factorization IS in `factorizations`)". That is what the field
notes, `docs/trace-schema-v0.md` and the code already said; the declaration was fix1's stale
draft. **OBSERVED:** the code charges `++counters.terminal_polish_solves` and folds only the
EQP-level fields back from its `local_counters`, unchanged by this round.

---

## 4. Item 4 — THE ONE CODE CHANGE: the terminal polish's adoption requires the walk's own KKT-point test

### 4a. The form shipped, and why this form

```cpp
    const double slack = detail::kTerminalPolishWorseFactor;
    const bool feasible = after.rows <= std::max(opts.feas_tol, before.rows);
    const bool kkt_point = after.stationarity <= opts.opt_tol;          // NEW
    const bool no_worse = ...
    const bool strictly_better = ...
    if (!after.all_finite() || !feasible || !kkt_point || !no_worse || !strictly_better) {
        return false;
    }
```

I shipped `after.stationarity <= opts.opt_tol` rather than a second call to
`free_block_is_stationary`, and the reason is that they are the SAME TEST here (OBSERVED):
`detail::terminal_kkt` sets `out.stationarity = stat / std::max(stat_scale, 1e-300)` from the
same `free_block_stationarity(qp, x, ws, λe, λi, stat_scale)` call whose two outputs
`free_block_is_stationary(residual, scale, opts)` compares as `residual <= opt_tol * scale`. So
the relative comparison is the walk's test on the candidate, to within the division's rounding,
with **no second stationarity computed** (the candidate's residual is already in `after`) and
with the acceptance chain's NaN convention preserved — a NaN `after.stationarity` makes
`kkt_point` false and `!kkt_point` rejects, exactly as the other clauses do.

The comment at the site states the rule and the one-line reason: the rounding margin is for ulp
ties and may not be spent as headroom above the tolerance; without the test a walk that
certified at 0.9·opt_tol could export a candidate at up to 3.6·opt_tol because the candidate
improved complementarity by the margin's factor instead.

### 4b. One consequence, stated rather than discovered later (INFERRED, unobserved in the battery)

The polish's `else` branch is reached from BOTH disjuncts of the termination test
(`zero_motion || stationary_on_ws`, `src/qp/qp_engine.cpp:1160`). On the `stationary_on_ws`
disjunct the walk has just applied `free_block_is_stationary` at this x with these multipliers,
so `before.stationarity <= opt_tol` holds and the new test can only bind on a candidate that is
WORSE than the incumbent's certified level. On the `zero_motion` disjunct the stationarity gate
is consulted only when the inertia verdict is `kSuspect` (`:1336–1344`), so an incumbent above
opt_tol is reachable in principle; there the new test refuses an adoption that would have
IMPROVED the point. That refusal is the conservative side — `x` and every multiplier are left
exactly as the walk had them, the exit is unchanged, and `terminal_polish_rejected` counts it —
but it is a real narrowing, and it is why I measured the whole battery rather than the corpus
alone. **It did not occur on any measured cell** (§5). The stricter-but-safer alternative
`after.stationarity <= std::max(opts.opt_tol, before.stationarity)` (the shape the row half
already uses) would close the same gap without that narrowing; I did not ship it because E4-28
item 4 ruled the plain form and the measurement gives no reason to deviate. **Registered** in
§8 in case a later round finds a cell where it matters.

### 4c. Coverage (INFERRED)

No fixture in the tree exercises the new branch, because no measured cell reaches it: a μ-free
face re-solve lands at ~1e-14 relative, decades inside opt_tol. The guard is a contract
closure with no witness. A purpose-built witness would need a face whose μ-free re-solve is
LESS stationary than the incumbent by more than 1× and less than 4× while improving
complementarity 4× — constructible, but it is a fixture-design task, not a text round.
**Registered** (§8).

---

## 5. The measured battery

Every leg alone on the box, each taking `/tmp/box-build.lock` for itself through
`.scratch/e4/leg.sh` (flock, NO timeout, never under `timeout`, in-lock `pgrep -a -x
hven_bench_cell`, `PGREP_EMPTY`/`FOREGROUND_START`/`FOREGROUND_END` markers, `set -o pipefail`),
`MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`, one solver at a time, `taskset -c 2` for the single-process
legs, `ctest -j4` under `setsid`, the whole chain detached (`.scratch/e4/fix3.sh`, log
`logs3/fix3.log`). A separate prior `pgrep -a -x hven_bench_cell` was empty before the launch,
and no foreign process was signalled at any point. **Exit codes are not the result; the numbers
below are read out of the logs.**

**The battery is AT the committed sources (OBSERVED):** after the commit, `cmake --build` in
both scratch trees reports `ninja: no work to do.`, and the last source edit preceded both
builds (Debug 09:01:38–09:03:09, Release 09:03:09–09:05:26).

| leg | evidence | result |
|---|---|---|
| corpus, 51 dumps × both algebras, **E2 ON** | `corpus/k3-{border,refactorize}-{old36,new15}.txt`; `python3 -B tools/rowcmp.py k1 k3` | **`TOTAL ROWS WITH ANY COLUMN CHANGE: 0`**; border k1 51 rows / k3 51 rows, refactorize 51 / 51, no missing file on either side. Every column — status, minors, facts, f, eq, in, box, stat, sgn, comp and all 24 counter columns including `refuse`, `residiters`, `drift` — identical to fix round 2 |
| T7 feasible census | `logs3/k3-key.log` | `QpEngineStructuralViolation.T7TheFeasibleCensusHalfOverTheFULLDualMuSet` **OK** (25 ms) |
| HS + B1Gate | `logs3/k3-key.log` | **38 tests from 4 suites ran, 38 PASSED, 0 failed** (`HsBattery.*`, `HsSweepCorpus.*`, `B1Gate.*`, the census) |
| U0 — 27 F7 cells × 3 engines × 75 columns vs the M6-close pin, residual gate 1e-5 | `logs3/k3-u0.log`, `u0/comparison.txt` | walk **17** differences (within gate 71), ssn **13** (68), ipm **13** (72) = **43**, and the ENTIRE comparison block (260 lines, every per-cell line) is **byte-identical** to fix round 2's `logs2/i51-u0.log` — `diff` returns 0 |
| Release suite | `logs3/ctest-release-k3.log` | **100% passed, 0 failed out of 2870**, 113.89 s, 1 skip + 2 disabled |
| Debug suite (the long test excluded by name) | `logs3/ctest-debug-k3.log` | **100% passed, 0 failed out of 2869**, 634.99 s, the four declared skips (`QpE2WorkingSet.AGreyPinOnYorknet…`, `QpE4ProximalEqp.TheYorknetRestoration…`, `QpNc2FlatFace.…Qp2Border`, `FailByDesignControl.…`) |
| the > 600 s test ALONE, detached | `logs3/ctest-debug-k3-long.log` | `Continuation.ProbeBudgetBoundsAFailingProposal` **Passed 619.78 s** (1/1) |
| both suites vs fix round 2, test by test | name+outcome rows sorted and diffed | Release **2870 = 2870, diff empty**; Debug **2869 = 2869, diff empty** |
| clang-format | `--dry-run --Werror` on all 6 touched `.cpp`/`.h` (clang-format 22.1.8) | CLEAN, rc=0 each |

**Movers: NONE.** INFERRED: the guard binds on no cell in the corpus, the census, the HS
battery, the B-1 gate, U0 or either suite, which is what a μ-free face re-solve landing at
~1e-14 predicts — the gap the review found was real in the contract and empty in the
measurement.

---

## 6. Item 5 — the prose corrections

**(a) "fixtures unmodified" holds for the two `QpEngineIndefinite` cells only.**
`QpE2WorkingSet.TheLockedVertexUnlocksOnTheAcopr57PrimalQps` WAS modified in fix round 2, and
the review is right to say so. **OBSERVED** in `git diff c864b8a6 799b527d --
tests/sqp/test_qp_e2_working_set.cpp`: the cross-arm pin `EXPECT_LE(|f - f_off|, 1e-7·|f_off|)`
is REMOVED and replaced by a per-arm ORACLE pin — a new `on_rel_tol` field, 1e-9 on
`ACOPR57_def-qp0` and 1e-7 on qp1, asserted against Clarabel's `f_star` for the OFF arm and for
each of the two algebras of the ON arm, with the old declared `rel_tol` (1e-4 / 1e-5) kept as
the looser of the two bounds. The fixture's own comment carries the attribution and the four
measured margins (qp0 on 2.2e-12 / off 2.2e-12; qp1 on 1.0e-9 / off 7.2e-10 relative). **This
is a STRONGER claim than the pin it replaces** — two to four decades tighter, and measured
against the oracle rather than against a control lane that this round declared MOVING — but it
IS a modification, and fix round 2's commit message and report should have said "the two
`QpEngineIndefinite` status regressions were fixed at the mechanism with their fixtures
unmodified", not "fixtures unmodified" unqualified. The record is corrected here; the landed
commit is not rewritten.

**(b) the three Debug skips' Release timings.** Cited from `logs2/ctest-release-final2.log` —
the run at fix round 2's COMMITTED tree, which is the run the shipped artifact should be quoted
from (OBSERVED, grep of that log):

| test | Release time at 799b527d's tree |
|---|---|
| `QpE2WorkingSet.AGreyPinOnYorknetThatIsIndependentStaysIndependent` | **22.18 s** |
| `QpE4ProximalEqp.TheYorknetRestorationQpIsDriftFreeAndOptimal` | **33.13 s** |
| `QpNc2FlatFace.TheSteenbrbRestorationLpQp2NeverAscendsTheSlopeBorder` | **9.55 s** |

(The review quoted `ctest-release-final.log`'s 22.05 / 32.90 / 9.60, from the pre-commit run of
the same sources. Both runs exist and agree to the second decimal; `final2` is the one at the
committed tree. The skip comments in the fixtures themselves quote their own Debug costs and
were not touched.)

---

## 7. Item 6 — evidence hygiene on the cap search (no re-run)

Per the settler's ruling, the cap search's per-cell rows are accepted as `STATE.md`'s
transcript plus the `cap*-key.log` raw half; **nothing was re-run** (each non-shipped cap is a
scratch rebuild of the library, and the shipped cap's rows are in `corpus/k1-*` already).
What exists, verified (OBSERVED):

- the per-cell rows — `ACOPR57_def/qp4` opt/1683 at 1e-6, num/2564 at 1e-5 and 1e-4;
  `STEENBRB_def/qp2` num/547, opt/554, opt/498; `A0_YORKNET/qp1` opt at all three; the five
  F3n50 and five F3n1000 minor counts — are `capsearch.sh`'s stdout as transcribed into
  `.scratch/e4/STATE.md:227–233`. No raw file carries them.
- the RAW half is `.scratch/e4/logs2/cap1e-6-key.log` (`[ OK ] HsSweepCorpus.Corpus (1289 ms)`),
  `cap1e-5-key.log` (`[ FAILED ] HsSweepCorpus.Corpus (1283 ms)`) and `cap1e-4-key.log`
  (`[ FAILED ] HsSweepCorpus.Corpus (1289 ms)`), plus `cap1e-{6,5,4}-wsb.log`. That is the
  decision the cap was chosen on — 1e-6 is the only value of the three that keeps
  `HsSweepCorpus` — recorded in a raw log.
- the shipped cap's own rows for those cells are in `corpus/k1-*` and, at this head, in
  `corpus/k3-*`, which the `rowcmp` above shows identical.

The registered follow-up (a conditioning-scaled cap, which is the principled form) is
unchanged and is not this round's.

---

## 8. Item 7 — the two stale narrations, and the settlement prose trimmed

**(a) `include/hven/detail/kkt/bordered_eqp.h`, `bordered_regularization`.** The present-tense
"the four certified `ACOPP57_def` cells the border path loses under E4 (kOptimal in 8 minors ->
kMaxIter at 1795 …)" is now past tense and says the loss was E4's FIRST round's and was
recovered: "…LOST in E4's first round … and fix round 1's unregularized polish recovered them
(kOptimal in 2 minors at 9.7e-15, parity with the eliminated path)". The clause's actual point
— that the δ-free form is not what lost them — is unchanged.

**(b) `include/hven/detail/qp/qp_walk_trace.h`, `eqp_residual_refused`** — covered by item 1.

**(c) settlement prose trimmed from `src/qp/qp_engine.cpp`** (header-comment discipline; the
narrative lives in the reports):

- the ladder's lower block: the retraction narrative ("MEASURED, and this is why the
  classification this replaced is gone … A FALSE CERTIFICATE is not a trade any fix may make,
  so the classification is withdrawn…") and the per-cell drift measurement paragraph are
  replaced by the RULE, its one-line reason (exhaustion means irreducible; classifying at the
  refused pass claims something about an unfinished iterate; on an elastic relaxation a
  mid-walk iterate violates rows it has not reached), and a pointer to
  `docs/notes/data/2026-09-m7-w2-e4-proximal-eqp/REPORT-fix2.md`. Net −13 lines.
- `adopt_polished_candidate`: the 16-vs-0 census narrative is reduced to "feasibility is the
  one thing the polish may never trade, so `after.rows` is compared exactly on both halves
  (the constant's note carries the measured cost of trading it)" — the measurement itself is
  intact at `kTerminalPolishWorseFactor`'s own note in the header, which is where a calibration
  argument belongs.
- `polish_terminal_point`: "REGISTERED for the settler as an accounting choice, with this as
  the argument" → "(ruling E4-17; solver_counters.h and docs/trace-schema-v0.md state the
  contract)". The ruling is settled; the sentence was asking for it.

**What I did NOT trim, and why (INFERRED, flagged for the settler).** The phrases the ruling
quoted — "An intermediate version…" and "the second retraction of this round" — are not in
`src/qp/qp_engine.cpp`. The first is in a TEST file
(`tests/sqp/test_qp_ws_independence.cpp`, in the E2 fixture's in-fixture attribution, wrapped
across a line break, which is why a plain grep for it misses), and E4-13/E4-19 MANDATE an
in-fixture attribution for exactly those re-premised fixtures, so trimming it would delete
required text. A normalized (comment-joined) search of `qp_engine.cpp`, `qp_engine.h` and
`eqp_solve.h` for `intermediate|retraction|withdrawn|REGISTERED for` returns nothing after this
round. The repo-wide header-comment cleanup pass stays registered.

---

## 9. E4-29 — the ledger corrections and registrations to carry

Carried verbatim from the ruling for the ledger, with my own additions marked NEW:

- **E4-18's premise was a misreading of a set NAME.** `DALLASM_def` qp0–qp3 were already
  classified TR-infeasible by the E2-D4 report (`REPORT.md:14`, `:88`; min-ℓ1 violation 0.448 /
  2.37 / 6.02 / 9.07), which fix2's `rawfeas.py` reproduces exactly. The two certificates are
  HONEST CONVERSIONS, and they are **CROSS-ALGEBRA ASYMMETRIC** (each certifies on one algebra
  and reads `num` on the other) — registered. At the driver a TR-restricted infeasibility is an
  ordinary elastic/restoration event, not an NLP verdict.
- **the E2-off lane is no longer an identity control for the QP engine** (25/51 rows move; 3
  certified cells lost). `independent_working_set` is a PUBLIC option, so a consumer turning E2
  off reaches a worse walk on a dependent set, because the proximal pair's safety rests on
  independence → a docs note on the option, and an M7-close question.
- `suspect_escalations` now conflates three causes (stall, residual, drift); the two refusal
  counters keep them readable.
- the conditioning-scaled polish cap (the principled form of `kEqpPolishStepCap`).
- the M4 anti-cycling arm has **no cycling subject left in the tree** — a finding.
- `SqpDriverRestorationProximity`'s Debug margin 1.9% (will close first).
- the `QpDegeneracyArm` diagnostics are now trivially satisfied (loops iterate zero times —
  honest, weak).
- `TheWatchdogRestoreCarries…` no longer tests the watchdog carry (coverage gap (a)).
- the re-take 9 schema event = SEVEN trailing `SqpCounters` keys + the accounting contract
  ("total factorizations" = `factorizations + terminal_polish_solves`).
- **NEW (this round, §4b):** `adopt_polished_candidate`'s new KKT-point test can refuse an
  IMPROVING candidate at a `zero_motion` exit whose incumbent sits above opt_tol (reachable
  only there, since that disjunct consults the stationarity gate only on a `kSuspect` verdict).
  Unobserved in the battery. If a cell ever shows it, the remedy is the row half's own shape:
  `after.stationarity <= std::max(opts.opt_tol, before.stationarity)`.
- **NEW (this round, §4c):** the new guard has no fixture witness — no measured cell reaches
  the branch. A purpose-built witness (a face whose μ-free re-solve is 1–4× less stationary
  while improving complementarity 4×) is a fixture-design task.
- **NEW (this round, §6a):** fix round 2's "fixtures unmodified" claim is corrected to the two
  `QpEngineIndefinite` cells only; the landed commit is not rewritten.

---

## 10. What I could not determine

- **Whether the new guard ever binds in production.** The battery says it does not bind on any
  cell in the corpus, the census, HS, B1Gate, U0 or either suite. That is an absence of
  evidence over ~2870 tests and 102 corpus rows, not a proof that no problem reaches it; §4b
  and §4c name the reachable path and the missing witness.
- **Whether the removed STEP-currency guard should return in some other scale.** I only
  recorded, from the E4 report's own measurement, that the scale the three runaway fixtures
  need costs 10 false firings and HS10/HS27. I did not re-measure it and did not search for a
  scale that separates the two populations; the text now says the guard is not there and why,
  which is the honest state.
- **Whether `docs/notes/data/2026-09-m7-w2-e4-proximal-eqp/REPORT-fix2.md` is the final path**
  of the citation I added to `qp_engine.cpp`. That directory exists in the MAIN tree as
  untracked work-in-progress (README.md, REPORT.md, REPORT-fix1.md, REPORT-fix2.md, evidence/,
  wrappers-gates/, timestamped 09:02–09:07 today, OBSERVED by `ls`), so the pointer resolves
  once the settler commits the task dir. If the file is renamed at close, that comment needs
  the new name.

---

## 11. Files changed (all paths absolute-from-repo-root), commit `5ed7bf16`

```
 docs/trace-schema-v0.md                |   4 +-    (both E4 refusal rows)
 include/hven/core/solver_counters.h    |  26 +++--- (the two refusal counters' notes)
 include/hven/detail/kkt/bordered_eqp.h |  13 +++--  (bordered_regularization, past tense)
 include/hven/detail/qp/qp_engine.h     |  71 +++--- (section 5's rule + guard, the drift
                                                     declaration, polish_terminal_point's
                                                     charge sentence + its adoption paragraph)
 include/hven/detail/qp/qp_walk_trace.h |   7 ++-   (eqp_residual_refused)
 include/hven/qp/qp_types.h             |  14 +++-- (primal_delta's guard clause)
 src/qp/qp_engine.cpp                   | 100 ++--- (the two ladder blocks, the KKT-point test
                                                     + its comment, the two prose trims)
 7 files changed, 127 insertions(+), 108 deletions(-)
```

`git status --porcelain` empty at the commit; both scratch builds report `ninja: no work to do`.

SIGNOFF: M7-W2-E4-FIX3-REPORT
