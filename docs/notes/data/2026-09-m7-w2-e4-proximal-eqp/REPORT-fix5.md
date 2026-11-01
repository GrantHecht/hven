<!-- M7 W2 E4 (the proximal EQP) -- the implementer report for fix round 5 (the ACOPR118
     diagnosis), copied VERBATIM from .superpowers/m7-w2-e4-fix5-report.md. Nothing below
     this comment is edited: the bytes after this preamble are that file byte for byte (cmp-checked). -->

# M7 W2 E4 — FIX ROUND 5: the ACOPR118 regression, diagnosed

Implementer report, 2026-09-27. Branch `e4` @ `84fb520e` (sources == m7 `e212a308` == the
RETAKE9 pin `ff02789b`; `git diff 84fb520e ff02789b` touches `docs/` only — OBSERVED).
Work area `/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9`, scratch
`.scratch/e4-fix5/`. **NO COMMIT LANDS THIS ROUND.** The worktree is clean at `84fb520e`;
hven's main tree and hven-bench were never written.

## 0. The one-paragraph answer

**E4 did not cause the ACOPR118 regression, and there is no E4 defect to fix.** The cell dies
at majors 17–20 on four successive walk-owned sub-QPs that each return `kNumericalError` after
2–3 minor iterations. Those four QPs were dumped and **replayed on the PRE-E4 engine
(pin `85dc01ac`, RETAKE8's): identical failure, identical minors (3/2/3/3), identical
factorizations (5/5/5/5)** — so the failure is pre-existing and E4's only contribution was to
move the walk's trajectory onto the iterate where these QPs arise. The single `infeasible`
QP the ledger counts (`harness_qp_ledger_infeasible = 1`) is **not** that failure: it is the
**second-order-correction re-solve at major 0**, its Farkas certificate is **TRUE** (HiGHS
phase-1 minimum l1 violation **16.501**, Clarabel **PrimalInfeasible**), and a failed SOC
correction correctly leaves the original `kReject` verdict standing — which is exactly why
`elastic_activations = 0` and `restoration_iters = 0`. Two **pre-existing** defects are
registered below with their numbers: an E2-lane one in the QP walk (§4) and a driver-side one
in the QP-failure routing (§5). E4 is exonerated on this cell; on the SOC QP it is a strict
improvement (it produces a verified certificate where the pre-E4 engine reported only
`kNumericalError`).

## 1. The regression reproduces, and the instrumented build is the pin

**OBSERVED.** `ACOPR118` re-run alone at the pin, `hven_bench_cell --lane hven-sqp --problem
ACOPR118 --options default --budget-iters 100 --assert-reference`, cwd `hven-bench`,
`MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`, `taskset -c 2`, under `flock /tmp/box-build.lock` with
no timeout (`PGREP_NO_UNCOVERED_CELL` / `IN_LOCK_PGREP_EMPTY` / `FOREGROUND_START
2026-09-27T20:18:57-04:00` / `FOREGROUND_END 20:20:18 rc=0`):

- **all 115 side-file counters identical, 0 differing**; all 9 results fields identical
  (`status numerical_error`, `objective 129660.5787807497`, `kkt_stationarity
  125.63036678886341`, `kkt_primal_eq 0.028299340877029344`, `kkt_complementarity
  4.6718101338902809`, `iterations 21`, `factorizations 11629`).
- transcripts: `.scratch/e4-fix5/logs/repro.log`, `logs/repro-cmp.txt`,
  `repro/ACOPR118-pin.out`.

**The diagnostic pipeline is the pin, byte for byte.** `.scratch/e4-fix5/src` is
`git archive 84fb520e` plus the two submodules; built Release/clang++/tests OFF/bench OFF/LTO
OFF (`w2_retake9_build.sh`'s flags) and installed, then a **copy** of hven-bench's tracked
tree builds `hven_bench_cell` against it. **OBSERVED: `libhven.a` sha256
`57c458e0…2b1393` and `hven_bench_cell` sha256 `78d65a55…aa43` are byte-identical to
hven-bench's own pin build.** So the baseline needed no separate cell run, and every diagnostic
effect is attributable to the patch.

The diagnostic patch (`.scratch/e4-fix5/patch_diag.py`, `patch_diag2.py`; `HVEN_F5_TRACE`,
`HVEN_F5_QPDUMP`, `HVEN_F5_DUMPALL`, `HVEN_F5_UNGATE`) is **scratch only and is never
committed** — it reads `getenv` in library code, which this repository does not allow (the same
reason branch `e3` was not mergeable). **OBSERVED: the instrumented run is bit-identical to the
pin run — 0 of 115 counters differ, all results equal** — so the trace is faithful. There is no
in-tree writer for the E2-D4 `D4QP 1` dump format at this pin (only the loader,
`tests/qp/support/qp_dump.h`), so the writer had to be re-created, as the E2-D4 round did.

## 2. What actually happens: the 21 majors

**OBSERVED**, `.scratch/e4-fix5/trace/ACOPR118-diag.err` (160 lines). Per-major, `h` is
`violation_l1`, `verdict` 0=acceptF 1=acceptH 2=reject:

| major | QP status | verdict | h | f | tr | E4 counters that moved |
|---|---|---|---|---|---|---|
| 0 | optimal | reject | 18.745 | 131321.99 | 1 | refusals 1, susp_esc 1, polish 289 |
| 1–3 | optimal | accept | 18.7→60.9 | →129099.03 | 0.5–1 | polish 162–229 |
| 4 | optimal | reject | 7.065 | 129584.10 | 1 | refusals 2, polish 266 |
| 5 | optimal | reject | 7.065 | 129584.10 | 0.5 | refusals 2, polish 260 |
| 6 | optimal | accept | 7.065 | 129584.10 | 0.25 | refusals 1, polish 271 |
| 7–10 | optimal | accept | 6.88→7.36 | →129660.6844 | 0.25 | polish 279–402 |
| **11** | optimal | accept | 7.593 | 129660.6839 | 0.5 | **terminal polish 1 solve, 1 REJECTED; verdict-refine 6 steps** |
| 12 | optimal | reject | 6.207 | 129793.06 | 0.5 | polish 358 |
| 13 | optimal | accept | 6.207 | 129793.06 | 0.25 | polish 345 |
| 14 | **numerical_error** | reject | 5.714 | 129621.30 | 0.5 | polish 237 |
| 15–16 | optimal | accept | 5.71→3.820 | →129662.08 | 0.25 | polish 444–523 |
| **17** | **numerical_error** | reject | **0.4995** | **129660.5787807497** | 0.25 | polish 13 |
| **18** | **numerical_error** | reject | 0.4995 | 129660.5787807497 | 0.125 | polish 14 |
| **19** | **numerical_error** | reject | 0.4995 | 129660.5787807497 | 0.0625 | polish 12 |
| **20** | **numerical_error** | reject | 0.4995 | 129660.5787807497 | 0.03125 | polish 11 → `kFinishQpFailure` |

The accepted step at major 16 lands on the point `f = 129660.5787807497`, `violation_l1 =
0.4995017264`. Majors 17–20 then solve the SAME subproblem at that point with the trust region
shrinking `0.25 → 0.125 → 0.0625 → 0.03125`, each returning `kNumericalError` in 2–3 minors and
5 factorizations, each counted `routed_failure 1/3`, `2/3`, `3/3`, and major 20 takes
`MajorOutcome::kFinishQpFailure` → `map_status(kNumericalError)` → NLP `numerical_error`.
**OBSERVED** (`F5ROUTE`, `F5FINISH`).

**None of E4's mechanisms fires in the four killing QPs**: `eqp_residual_refusals 0`,
`eqp_row_drift_refusals 0`, `terminal_polish_solves 0`, `verdict_refine_steps 0`,
`suspect_escalations 0` in each (`F5SOLVE end=24..27`). The only E4 column that moves there is
`eqp_polish_steps` (11–14 out of 5199 for the cell).

## 3. Question 1 — the `infeasible` QP: a TRUE certificate, in the SOC re-solve

**OBSERVED.** The one solve in the whole cell that returns `QpStatus::kInfeasible` is
**solve 1 = the second-order-correction re-solve at major 0** (`F5SOC … SOC_QP_FAILED
status=2 minors=446 facts=958 cert=1`). Its certificate is the dependency add-exit Farkas
certificate (`F5ADDEXIT solve=1 it=445 add_exit=1 … kInfeasible(add-exit CERTIFICATE)`),
i.e. `dep_infeasible_certificates` — which was already **1 at RETAKE8**
(compare file: `dep_infeasible_certificates 1->1`), so it is pre-existing, not an E4 event.

**The certificate is TRUE.** Dump `.scratch/e4-fix5/dumps/ACOPR118_e4/qp0.txt`
(D4QP v1, n=344 me=236 mi=608, tr=1), verified independently in a scratchpad venv
(numpy 2.5.3 / scipy 1.18.1 / Clarabel; never vendored):

- **HiGHS phase-1 LP** (min l1 violation of `Ae x = be`, `Ai x <= bi`, box, trust region
  included): status 0, **minimum l1 violation = 16.50110172408627 > 0**.
- **Clarabel**: **`PrimalInfeasible`**.
- transcript `.scratch/e4-fix5/cert/qp0.oracle.txt`, json `qp0.oracle.json`.

Both oracles agree with hven: the TR-restricted SOC subproblem at major 0 is genuinely
infeasible. This is ruling E4-29's case exactly.

**Why neither elastic nor restoration engaged, and why that is correct.** The driver's only
consumer of `QpStatus::kInfeasible` is `src/drivers/sqp_solver.cpp:6464`, and it is reached
only by a MAJOR's own answer. A **SOC** re-solve is not that: a non-`kOptimal` SOC falls to
`++st.out.counters.soc_qp_infeasible` and **the original `kReject` verdict stands** (OBSERVED
at the site, and in the trace `ACTION=original_verdict_stands(no elastic,no restoration)`).
A SOC is an optional correction to an already-rejected trial; there is nothing for elastic or
restoration to answer. So `elastic_activations = 0` / `restoration_iters = 0` beside
`harness_qp_ledger_infeasible = 1` is **not** an anomaly — it is the SOC path, and the two
columns cannot both be nonzero on it. **This resolves the apparent contradiction in the
RETAKE9 row.**

**E4 IMPROVED this solve.** Replayed on the pre-E4 engine (`85dc01ac`) the same QP returns
`status=3` (`kNumericalError`, `blk=1`) with **no certificate**; at the E4 pin it returns
`status=2` with `cert=1`. E4 turned an unexplained numerical failure into a verified
infeasibility certificate. (Note: `qp0` is the one dump whose replay does not reproduce the
in-cell minor count — 355 replayed vs 446 in-cell, the in-cell solve having `exch=401` against
the replay's 318 — the hot-K0/warm-seed effect the D4 format's own note records. `qp1`–`qp8`
reproduce status, minors AND factorizations exactly, so the replay is a faithful instrument for
the failing set.)

## 4. THE ROOT CAUSE — pre-existing, in the E2 independent-working-set lane. REGISTERED.

The four killing QPs are `.scratch/e4-fix5/dumps/ACOPR118_e4/qp5.txt` … `qp8.txt`
(n=344, me=236, mi=608; tr 0.25 / 0.125 / 0.0625 / 0.03125; warm seed present).

**(a) The QP is FEASIBLE and the walk's kWrong verdict is ARITHMETICALLY TRUE.**
Oracle on `qp5` (`.scratch/e4-fix5/cert/qp5.oracle.txt`):

- HiGHS phase-1: status 0, **minimum l1 violation = 0.0** — the QP is feasible.
- `H` eigenvalues: min **−1.4419e+02**, max 6.4676e+05, **2 negative**, 54 zero.
- `Z'HZ` on `null(Ae)` (dim 108): min **−7.2663e-02**.
- `Z'HZ` on **the walk's own terminal working set** (9 rows + 30 pins, me+|W| = 275 = rank,
  null-space dim **69**): min eigenvalue **−6.8122e-02**, next **+6.3571e+00**, max 3.8108e+04.
- The walk's returned point is feasible for the QP: eq 3.89e-16, ineq 6.07e-18, box 0.

So the face the walk stops on carries **exactly one** direction of negative curvature, and the
`kWrong` inertia verdict is a correct reading, not a false one.

**(b) The same QP SOLVES with `independent_working_set = false`.** Same dump, same warm seed,
same border algebra, only that one field flipped (`.scratch/e4-fix5/logs/replay-sens.log`):

| arm | status | minors | facts | f | stationarity | rows | eq resid |
|---|---|---|---|---|---|---|---|
| E2 **on**, border | **kNumericalError** | 3 | 5 | 0.1054162515 | 1.00e+00 | 5.41e-14 n/a | 1.87e-16 |
| E2 **on**, refactorize | **kNumericalError** | 3 | 77 | 0.105416246754 | 1.00e+00 | — | 3.01e-14 |
| E2 **off**, border | **kOptimal** | 682 | 36 | **−0.154376254009** | **1.77e-14** | 3.75e-14 | 5.41e-14 |
| E2 **off**, refactorize | **kOptimal** | 539 | 689 | −0.154376247307 | 2.32e-14 | 2.78e-17 | 8.10e-15 |

The E2-off answer is a clean KKT point of the same QP at a **lower** objective
(−0.1544 against the walk's +0.1054), complementarity 4.87e-15. **The QP is answerable and the
E2-on path does not answer it.**

**(c) The exact divergence, minor by minor** (`.scratch/e4-fix5/logs` for the two traces):

- **E2 ON**: minor 0 at 9 rows / 29 pins. The blocker is **pin 291 at its lower bound**,
  `alpha = 5.405382e-01`, a MOVING step. The dependency test returns
  **`kUndecidable`** with **`rho` = the struct's NaN default** (`solves=0`, `screen=0`,
  `stall_admit=0` — the test computed nothing), which `DepTest::dependent()` counts as
  dependent, so the candidate takes the **deferred-add** path
  (`F5DEFER … ADMITTED_AS_DEFERRED_ADD (undecidable)`, `dep_deferred_adds`). Minor 1: 9 rows /
  30 pins with `recheck_pending=1` — the pass is spent on the recheck and **nothing else is
  added**. Minor 2: the walk reaches the terminal disjunct on that 9-row / 30-pin face,
  `worst = 0`, `runaway = 0`, verdict `kWrong`; `reread_wrong_inertia` does not overturn it
  (`dep_inertia_rereads = 2`, `flips = 0`); the post-probe restart is gated off
  (`probe_drop_made = 0`) → **`kNumericalError` at minor 3**.
- **E2 OFF**: minor 0 at 9 rows / 29 pins; minor 1 at **10 rows** / 30 pins — a row AND a pin,
  no deferral — then 31, 32, 33, 34, 35, 36 pins by minor 7, monotone, to 682 minors and
  verdict `kOk`.

**(d) The one obvious remedy is REFUTED, measured.** The post-probe restart at the `kWrong`
dead end is gated on `probe_drop_made`, which is false here, so the temporary-vertex repair —
the very mechanism the walk spends at `iter == 0` on a `kWrong` start face — is never offered.
Removing that gate (`HVEN_F5_UNGATE`) was measured: `repair_temporary_vertex` **returns false**,
and the results on all four dumps are **byte-identical** to the gated run (`qp5` status 3 minors
3 facts 5 f 0.1054162515 either way; the call did run — `eqp_polish` 13→18 and `border_refine`
73→142). So that gate is not the fix.

**(e) PRE-EXISTING: the pre-E4 engine fails identically.** `qp5`–`qp8` replayed against the
RETAKE8 pin `85dc01ac` (`.scratch/e4-fix5/logs/replay-cmp.log`):

| dump | E4 `ff02789b` | pre-E4 `85dc01ac` |
|---|---|---|
| qp5 | status 3, minors 3, facts 5 | **status 3, minors 3, facts 5** |
| qp6 | status 3, minors 2, facts 5 | **status 3, minors 2, facts 5** |
| qp7 | status 3, minors 3, facts 5 | **status 3, minors 3, facts 5** |
| qp8 | status 3, minors 3, facts 5 | **status 3, minors 3, facts 5** |

(also identical in class on `qp1`–`qp4`: status 3 at both pins, minors 2184 / 2942 / 488 /
270–274.) **E4 did not cause this failure. E4 changed the trajectory that reaches it.** The E4
polish reaches every QP from major 0 (`eqp_polish_steps` 289 on the first one), so there is no
single "divergence major" later than 0 to name.

> **REGISTERED ITEM R1 (E2 lane, pre-existing, out of E4's scope).** With
> `independent_working_set = true`, an **undecidable dependency verdict on a MOVING blocker**
> (`rho` never computed) takes the deferred-add path and costs the next pass to its recheck; if
> the resulting face is indefinite the walk reaches the terminal disjunct, reads `kWrong`, and
> has **no recovery at all** — the re-read does not overturn it and the post-probe restart is
> gated on `probe_drop_made`, which a 2-minor walk from a warm seed never sets. Measured on
> ACOPR118's major-17 subproblem: 3 minors to `kNumericalError` where the same QP solves in 682
> minors with the field off, at a lower objective and stationarity 1.8e-14. Ungating the restart
> is refuted (repair returns false). Cell-level note: the `independent-working-set-off` LABEL's
> whole ACOPR118 cell is `dnf_wall` at **both** pins, so E2-off is a **localization, not a
> remedy**. Candidate directions, unmeasured: ride the single negative-curvature direction at
> the `kWrong` dead end; or give the undecidable deferred add a rho rather than the default NaN.

## 5. Question 2 — the 8 rejected steps, and what the returned point is

**OBSERVED**, per-major `rejected_steps` deltas: majors **0, 4, 5, 12** (+1 each) and
**14, 17, 18, 19** (+1 each) = **8**.

- Majors 0, 4, 5, 12 are **ordinary globalization rejections at a `kOptimal` QP**. The trial
  point raised the violation sharply each time (`SOC_ENTER h_old → h_new`: 18.745 → 233.714,
  7.065 → 187.393, 7.065 → 43.786, 6.207 → 44.031), the SOC re-solve was attempted and itself
  failed (status 2 at major 0, status 3 at majors 4, 5, 12), so the original `kReject` stood.
  Nothing pathological: the linearization is poor at those iterates.
- Majors 14, 17, 18, 19 are **routed QP failures** — the QP returned `kNumericalError`, so no
  step existed to judge. Major 14's successor QP succeeded, which reset the run; 17/18/19 are
  the consecutive 1/3, 2/3, 3/3 that exhaust `qp_failure_budget`.

**The returned point is INFEASIBLE, not a better feasible point.** `f = 129660.5787807497`
against the RETAKE8 optimum `129660.6833784326` — lower by 0.1046 — but with
`violation_l1 = 0.4995017264` and `kkt_primal_eq = 0.028299340877029344` (RETAKE8:
1.939e-09). It sits just outside the feasible set, which is exactly how a smaller objective is
bought. **And the multipliers are the wrong set for it**: the `kFinishQpFailure` exit returns
`st.lambda_e` / `st.lambda_i` — the last *successful* subproblem's prices, i.e. major 16's,
priced at major 16's iterate — against the point major 16's accepted step moved to. That is what
`kkt_stationarity 125.63` and `kkt_complementarity 4.67` are measuring. Both halves of the row
are therefore consistent: the point is infeasible **and** the reported prices belong to a
different iterate.

> **REGISTERED ITEM R2 (driver side, pre-existing, out of E4's scope).** The driver's response
> to a walk-owned `kNumericalError` carrying `dep_blocker_terminations = 0`,
> `zero_motion_cap_hits = 0`, `dep_certificate_rejections = 0` is the **trust-region shrink
> ladder alone** — `e2_termination_to_elastic` (`sqp_solver.cpp:6458`) excludes this cause, so
> the elastic tier is never opened even though `h = 0.4995 > feas_tol` would admit it. A shrink
> cannot change the curvature of a face, so the four attempts at 0.25 / 0.125 / 0.0625 / 0.03125
> return the identical verdict in 2–3 minors, `qp_failure_budget = 3` is exhausted, and the NLP
> reports `numerical_error` at an infeasible point with stale prices. Two sub-items:
> (i) whether the L3 widening should admit an **unrepairable `kWrong` inertia verdict** as a
> cause that the shrink ladder cannot fix (it is the same argument L3 already makes for E2
> terminations: a smaller box is a subset); (ii) `map_status` sends a `kMaxIter` QP to
> `numerical_error` as well, so the NLP status cannot distinguish "the QP was capped" from "the
> QP broke" — the discriminating columns (`qp_numerical_exits`, `qp_failures_routed`,
> `qp_failures_in_a_row_peak`) are **not** exported to the harness lane today, which is why this
> round needed a source patch to see it at all.

## 6. Question 3 — YORKNET and STEENBRG at the 300 s wall

**OBSERVED, the 2×2** (`results.csv` at both pins, both labels; `budget_wall_s` 300 throughout;
wall is informational per CLAUDE.md §7 and is never asserted here):

| cell | label | RETAKE8 | RETAKE9 |
|---|---|---|---|
| YORKNET | default | `numerical_error`, 1 major, 57326 facts, wall 92.902 | **`dnf_wall`**, wall 300.103 |
| YORKNET | E2-off | `numerical_error`, 1 major, 37530 facts, wall 36.060 | `numerical_error`, **9** majors, 53256 facts, wall 204.153 |
| STEENBRG | default | `numerical_error`, 8 majors, 22418 facts, wall 135.422 | **`dnf_wall`**, wall 300.103 |
| STEENBRG | E2-off | `numerical_error`, 5 majors, 34353 facts, wall 120.686 | `numerical_error`, **6** majors, 34715 facts, wall 104.106 |

**A `dnf_wall` row carries NO counters at all.** RETAKE9's default counters file merged **13 of
23** legs against RETAKE8's 15; the two missing are exactly these cells, and every asserted
`results.csv` column on them is the literal `null`. So nothing about the two timed-out processes
is measured — not majors, not minors, not factorizations. **Everything below is INFERRED** from
the E2-off arm (the same pin change on the same problem, with the working-set machinery off) and
from neighbouring cells.

**YORKNET — INFERRED: the cost is per unit of QP work, NOT more minors per QP.** The E2-off twin
moves the *other* way on minors-per-major (37269 → 5618, −85%) while majors go 1 → 9; total
minors rise only +35.7% and factorizations +41.9%, yet wall rises 5.66× — i.e. **ms per minor
0.968 → 4.038, a 4.17× inflation**. Sufficiency: YORKNET-default at RETAKE8 spent 92.902 s on
52549 minors = 1.768 ms/minor; at 4.17× that is 7.37 ms/minor, so 300 s buys ≈40,700 minors —
**fewer than RETAKE8 already did**. The per-unit inflation alone is sufficient to explain the
timeout with *less* QP work than before; no minors-per-QP growth is needed. What appeared
alongside it in that arm: `eqp_polish_steps` 1920, `eqp_refine_iters` 105,
`terminal_polish_solves` 14, `border_refine_steps` 140 → 614, `symbolic_analyses` 3 → 15 — and
`harness_factorizations_total` 53270 is only 14 above `factorizations` 53256, so the polish is
**not** buying numeric factorizations; it is arithmetic on factors already paid for.

**STEENBRG — INFERRED: not attributable on the evidence available.** Its E2-off twin gives no
support for an E4 slowdown at all: wall **fell** 13.7% at essentially constant work (+2.3%
minors, +1.1% factorizations) while carrying 10184 polish steps and 815 refine iters. So whatever
cost the default arm lives in the interaction with the independent-working-set machinery, which
is on only there. STEENBRG-default was already at 45.1% of the ceiling at RETAKE8, so a 2.22×
wall multiplier reaches it, and **both** mechanisms occur at that magnitude on siblings in the
same arm (ms/minor: STEENBRB 3.27×, STEENBRD 1.80×; majors: STEENBRC 4 → 43, STEENBRD 5 → 11).
Either is individually sufficient. **I decline to attribute this cell**; the honest reading is
that the E2-off arm rules out neither because it ran a different, cheaper-per-minor QP path.

**Suite context (OBSERVED, the 13 finishers):** the E4 polish reaches every row
(`eqp_polish_steps` nonzero on 13 of 13; density ≈ 0.64–1.01 steps per minor on the STEENBR
family), and `border_refine_steps` moves in **both** directions RT8 → RT9 (ACOPP118 455 → 18899,
DALLASM 8146 → 1580, STEENBRB 842 → 454). Three separable mechanisms are visible among the
finishers, not one: per-minor cost inflation (STEENBRB 3.27×, NET3 1.96×, STEENBRD 1.80×), a
majors explosion (STEENBRC 4 → 43, STEENBRD 5 → 11, ACOPR118 15 → 21), and a minors-per-QP
explosion (ACOPP118 87 → 3994 minors/major, 12.1 s → 246.9 s — itself a near-miss of the same
ceiling). **REGISTERED as R3: the E4 cost profile on the STEENBR/network family needs a round of
its own; two cells left the suite to the wall and a third came within 53 s of it.**

## 7. Question 4 — the two conversions

**DALLASM (`numerical_error` → `optimal`).** The cell did not improve along the same trajectory;
it took a different and far shorter one. `major_iters` 5 → 26 with `steps_accepted` 0 → **23**
and `rejected_steps` 3 → 2, while total QP work collapsed: `qp_minor_iters` 8673 → 1956 (−77.5%,
so minors/major 1735 → 75), `factorizations` 878 → 643, `border_refine_steps` 8146 → 1580
(−80.6%), wall 23.016 → 3.912 s. The whole E2 dependency apparatus quiesced by two orders of
magnitude (`dep_test_solves` 145133 → 6174, `dep_tests` 31733 → 2202, `dep_passovers` 23420 →
755, `ws_exchanges` 8029 → 653, `dep_structural_first` 11937 → 278, `dep_grey_rereads` 15496 →
290). `objective` 61972694.25 → **−48198.188**, `kkt_stationarity` 89474.29 → 4.97e-09,
`kkt_complementarity` 3.196e+07 → 5.48e-12. E4 columns: `eqp_polish_steps` 2554 (1.31 per minor),
`eqp_refine_iters` 0, all `terminal_polish_*` 0, `harness_factorizations_total` 643 = `factorizations`.
**The corpus prediction is half borne out and half invisible at cell level.** The "cap → opt"
half IS borne out: `qp_maxiter_exits` **4 → 0** (`qp_cap_retries` 0 at both), which is the
`DALLASM_f4 qp2/qp3 convert (cap → opt)` claim in the cell's own currency. The "certified
kInfeasible routes to elastic/restoration" half leaves **no trace at either pin**:
`harness_qp_ledger_infeasible` 0 → 0, `dep_infeasible_certificates` 0 → 0,
`dep_certificate_rejections` 0 → 0, `sqp_infeasibility_certified` 0 → 0 — and the elastic /
restoration event it would become did not newly appear either: `elastic_activations` **1 at both
pins**, `restoration_iters` **2 at both pins**, `dep_border_retreats` 1 → 1. Stated plainly: the
certificate half is **neither confirmed nor refuted by this cell**, because those `DALLASM_def`
qp1/qp2 faces are corpus dumps of subproblems the TRAIN walk did not visit in either run. The
compare file grades it `MIXED` on the same zeros.

**ACOPP57 (`numerical_error` → `optimal`).** Its RETAKE8 row is the failure signature ACOPR118
now partly wears, and **all eight columns of it collapse at RETAKE9**: `rejected_steps` 8 → 0,
`verdict_refine_steps` 2 → 0, `suspect_escalations` 1 → 0, `harness_qp_ledger_infeasible` 1 → 0,
`dep_infeasible_certificates` 1 → 0, `ride_sign_vetoes` 6 → 0 (with
`ride_declines_fallthrough_blocked` 6 → 0), `sqp_kkt_residual` 167.187 → **1.002e-07**,
`sqp_feasibility` 6.029e-04 → **7.464e-11**. Alongside: `major_iters` 13 → 5 with
`steps_accepted` 4 → 5 (every major accepted), `factorizations` 1211 → **98** (−91.9%) while
`qp_minor_iters` 564 → 649 (+15.1%) — more minor iterations inside far fewer factorizations,
the arithmetic shape of "stopped refactorizing to escape a bad QP"; `elastic_activations` 1 → 0;
`harness_qp_ledger_solves` 20 → 5 (exactly one QP per major); `symbolic_analyses` 3 → 1;
`dep_pin_skips` 456 → 0. E4 columns are small and in the polish class: `eqp_polish_steps` 89,
`eqp_refine_iters` 0, `eqp_residual_refusals` 0, `terminal_polish_solves` 1 / `adopted` 3 /
`rejected` 0. In one line: the certified-infeasible QP and the six sign vetoes disappeared, and
with them the whole reject / verdict-refine / elastic recovery loop — 8 rejected majors and 1113
of 1211 factorizations were pure recovery cost, and the run now walks 5 accepted majors straight
to a 1e-07 residual, 45.7× faster.

**On the "ACOPR118 now has ACOPP57's old signature" reading:** it is **related, not identical**.
Four columns match (`rejected_steps` 8, `harness_qp_ledger_infeasible` 1,
`dep_infeasible_certificates` 1, `suspect_escalations` 1); four diverge —
`verdict_refine_steps` **6** not 2, `sqp_kkt_residual` **125.630** not 167.187,
`sqp_feasibility` **0.02830** not 6.03e-04 (47× worse), and `ride_sign_vetoes` **0** not 6. The
diagnosis above supersedes the analogy: ACOPP57's RETAKE8 failure was a certificate-and-sign-veto
recovery loop; ACOPR118's RETAKE9 failure is an indefinite face with no recovery (§4).

## 8. A counter note the round should NOT chase

`terminal_polish_adopted + terminal_polish_rejected > terminal_polish_solves` on five rows
(ACOPP57 1/3/0, ACOPR57 2/2/1, SMBANK 2/2/1, STEENBRC 7/7/5, YORKNET-E2-off 14/10/6). **This is
the documented contract, not a defect.** `solver_counters.h` states it at the field: stage 1 of
the terminal polish adopts the pass's own EQP answer **for free**, with no solve, so
"`terminal_polish_adopted` exceeds `terminal_polish_solves` on any solve whose terminal face the
free candidate closed, and the factorizations spent are `terminal_polish_solves` alone."
`terminal_polish_solves − terminal_polish_rejected` is the **stage-2** adoption count, and the
three columns are not a partition by design. ACOPR118's own 1 / 1 / 0 reads correctly: stage 1
declined, stage 2 was entered once and rejected, and the walk's point stood (OBSERVED,
`F5POLISH solve=15 ENTER before_stat=5.359179e-04 … STAGE2_REJECTED cand_stat=2.854750e-01`
— the candidate's stationarity was 533× worse, so the adoption test refused it correctly).

## 9. Disposition

**NO FIX LANDS. No commit is made.** The worktree is clean at `84fb520e`; hven's main tree and
hven-bench were not written. The deliverable is this report plus the evidence under
`.scratch/e4-fix5/`. Both defects found are **pre-existing** (measured identical at the pre-E4
pin `85dc01ac`) and lie outside E4's own mechanisms — R1 in the E2 independent-working-set lane
of the QP walk (§4), R2 in the driver's QP-failure routing (§5) — so by this round's own rule
they are registered with their numbers rather than fixed here. R3 (§6) is the E4 cost profile on
the STEENBR/network family, which cost two cells to the wall.

**What a settler should decide.** (a) Whether R1 or R2 gets a round, and in which lane — R2 is
the cheaper of the two and would have converted this cell's *status* (an elastic tier opened at
`h = 0.4995` where the shrink ladder provably cannot help), while R1 is the one that would make
the sub-QP actually solvable. (b) Whether ACOPR118's move off `optimal` is acceptable as a
trajectory consequence of an E4 that also converted DALLASM and ACOPP57 and turned an
unexplained SOC failure into a verified certificate — the TRAIN optimal count is the number to
weigh, not this cell alone. (c) R3, which is the larger cost question.

**Evidence index** (all under
`/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/.scratch/e4-fix5/`):

| path | what |
|---|---|
| `STATE.md` | the running log (a usage-limit death resumes from it) |
| `leg.sh`, `repro/cell.sh` | the box-lock leg wrapper and the cell invocation |
| `logs/repro.log`, `logs/repro-cmp.txt`, `repro/ACOPR118-pin.out` | the reproduction, 0 of 115 counters differing |
| `patch_diag.py`, `patch_diag2.py` | the scratch diagnostic patch (NEVER committed) |
| `src/`, `build-rel/`, `bench/`, `bench-build/` | the scratch tree, byte-identical to the pin before the patch |
| `trace/ACOPR118-diag.err` | the 160-line instrumented trace (bit-identical run) |
| `dumps/ACOPR118_e4/qp0..qp8.txt` | the 9 D4QP v1 dumps; qp0 = the SOC infeasible, qp5–qp8 = the four killing QPs |
| `cert/qp0.oracle.txt`, `qp0.oracle.json` | HiGHS phase-1 16.501 + Clarabel PrimalInfeasible |
| `cert/qp5.oracle.txt`, `qp5.oracle.json` | feasible; `Z'HZ` on the walk's face min eig −6.8122e-02 |
| `logs/replay-cmp.log` | the 9 dumps on both pins — the E4 exoneration |
| `logs/replay-sens.log` | the four E2 × algebra arms on qp5 |
| `replay/`, `replay-build-*` | the replay tool, built against both pins and the instrumented install |

SIGNOFF: M7-W2-E4-FIX5-REPORT
