> Copied verbatim from `.superpowers/` (source: the round's own report/review) on 2026-10-01; nothing below this preamble is edited.

# M7 W2 E2 — FIX ROUND 8: THE BOUNDED REPAIR (with fix 7 re-verified)

Continuation implementer report, 2026-09-30 (the fix-8 implementer died on the Opus 5 weekly
limit; this round continued from its WIP snapshot `6586164f`). Worktree
`/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9`, branch **`e2fix8`**:

| commit | what | parent |
|---|---|---|
| **`86bd0962`** | fix 7 = RULE A only (unchanged, verified, NOT amended) | `84fb520e` |
| **`35986d1f`** | fix 8 = rule B bounded + F7-1 + fixtures (the WIP squashed in) | `86bd0962` |

No attribution trailer on either. Explicit pathspecs only. hven's main tree and hven-bench were
never written (hven-bench's pin binary was RUN read-only, outputs into this round's scratch).
Scratch: `.scratch/e2fix7/` (STEP 1; `STATE.md` current) and `.scratch/e2fix8/` (fix 8;
`STATE.md` current). IPARM untouched. No SNOPT material read. Accelerate values UNOBSERVED.

## 0. The answer

- **Fix 7 (`86bd0962`) is verified as landed**, rule A alone: corpus moves exactly the 3 honesty
  rows per algebra, HS stays 3340, U0 block byte-identical, suites green. Not amended.
- **Fix 8 is rule B with a budget of `max(8, ceil(n/2))` pins.** The WIP's `n/3`, derived from
  replayed dumps alone, was **refuted in-cell**: on ACOPR118 and ACOPP118 the first, cold
  subproblem needs a **118-pin** repair (n = 344, 0.343 n), and `n/3` = 115 cut it in both cells
  (budget stop → different trajectory; on ACOPP118 that cost **43 666 factorizations / ≈979 s**
  against **1 833 / ≈48 s** unbounded). With `n/2` both cells are **field-for-field identical
  (115/115 side-file counters) to the unbounded repair**, i.e. nothing measured is cut.
- **F7-1** spends the pass only when the failed repair took away a factor view the pass held —
  not on every failed repair (the WIP's version moved `T7Fix2…` +1 factorization for a repair
  that never probed).
- **ACOPR118 does not convert**: numerical_error, 15 majors, f 129660.68777485074 (RT8
  129660.6833784326), stationarity 0.045, `qp_maxiter_exits` 4. **The cap is not the problem**:
  the four capped QPs replay exactly and stay capped at **4×** their cap with **no new
  factorization** (a stalled walk), and `qp_cap_retry` is off under `default`.
- **Wall attribution:** the OFF twins are byte-identical to RETAKE9's control (YORKNET, STEENBRG:
  115/115 counters); STEENBRG/YORKNET `default` cost the same on the RETAKE9 **pin binary**
  (STEENBRG 359 s pin vs ≈360 s fix 8; YORKNET > 600 s on both). **Their wall is E4's (R3), not
  rule B's.** ACOPP118 `default` gets CHEAPER under fix 8 (RT9 247 s → ≈48 s).

## 1. STEP 1 — fix 7 `86bd0962` verified (rule A only)

Done by a detached checkout of `86bd0962` IN THIS WORKTREE (the WIP was committed, nothing at
risk), incremental `nc2-build-{release,debug}`, tag `V7`. Leg under `/tmp/box-build.lock`:
PGREP_EMPTY / FOREGROUND_START 18:21:01 / FOREGROUND_END 19:04:50 rc=0
(`.scratch/e2fix7/logs/battery_V7.out`). All OBSERVED:

| check | result |
|---|---|
| census T7 + HsBattery + HsSweepCorpus + B1Gate | 39/39; `HsSweepCorpus.Corpus` OK at its **3340** pin (86bd0962 carries 3340, not 3342 — no defect) |
| corpus 51 dumps × 2 algebras vs k3 (`rowcmp.py k3 V7`) | **6 rows = exactly** Hj_STEENBRD/qp4 (sfirst 0→1, undec 1→0), STEENBRB_def/qp0 and qp1 (undec 1→0, deferred 1→0), each algebra; nothing else, no status/minors/f/residual |
| E2-off corpus | `V7off` vs `k1off` and vs `fAoff`: 0 rows |
| U0 | walk 17 / ssn 13 / ipm 13 = 43; comparison block byte-identical to `.scratch/e4/u0` |
| Release `ctest -j4` (setsid) | 2876 / 0, 114.45 s |
| Debug `ctest -j4`, long test excluded | 2875 / 0, 689.77 s |
| `Continuation.ProbeBudgetBoundsAFailingProposal` alone | 1 / 0, 623.24 s |
| clang-format | 0 replacements on all 8 touched files |

## 2. What the WIP contained, and what "fully-gated tree" meant

The WIP (`git diff 86bd0962 6586164f`): rule B; a pin budget `max(8, ceil(n/3))` plus a "probe
budget" of 2× that; F7-1 as an unconditional `continue` on every failed start repair;
`dep_repair_budget_stops`, `dep_repair_restarts`, `kStartRepairRefused`; the ACOPR118 qp5/qp6
fixtures; re-pins of `AFullyPinnedRow…` (minors 1→2) and HS (3340→3342); the RPX measurement
(838 repairs). **"Fully gated" = both the budget and F7-1 gated on `independent_working_set`**,
so the option-off control arm is byte-identical (its earlier ungated variant moved 3 OFF corpus
rows). Its last battery (G, 09-28 03:33) had **one failure**:
`QpEngineIndefinite.T7Fix2AFailedStartRepairDoesNotRefineThroughTheProbedFactorization`,
refactorize, factorizations 4 → 5 (F7-1 spending a pass on a repair that pinned nothing).

What this round changed on top of it:
1. **F7-1 narrowed** (that failure gone, no re-pin): spend the pass iff
   `dep_factor_view` was usable BEFORE the repair and is not AFTER it.
2. **The "probe budget" removed** — it was dead code: the pin loop probes once per pin and the
   release pass at most once per pin, so probes ≤ 2 × pins always; it can never bind before the
   pin budget. The probe bound is now stated as that construction.
3. **The divisor re-derived 3 → 2** from the in-cell measurement (§3).
4. **A false header claim removed** ("qp_engine.cpp may not gain a line … two include lines moved
   STEENBRB_def/qp1 by 614 factorizations"): the WIP itself adds ~80 lines to that TU with the
   OFF corpus unmoved, so the claim as written is contradicted by its own tree; the integer
   ceiling stays (it needs no comment).
5. **New fixture `QpE2Fix8Budget`** pinning the budget's declared cost (§4).
6. Comment corrections: "a budget stop is the safe direction" was false (§4).

## 3. The bound — derivation, with the distribution

**(a) Replayed from dumps** (WIP's RPX measurement, `.scratch/e2fix8/rpx/`, `rpx_stats.py`;
scratch getenv patch never committed): the 51-dump corpus × 2 algebras + the whole HS sweep +
ACOPR118 qp5–qp8 × 2 algebras = **838 repairs**; rule B on: **832 succeed**, 6 fail.

| successful repairs | p50 | p90 | p95 | p99 | max |
|---|---|---|---|---|---|
| pins tried | 1 | 1 | 2 | 134 | 161 (n = 824) |
| pins kept | 1 | 1 | 2 | 81 | 81 |
| tried / n | 0.333 | 0.5 | 0.667 | 0.8 | 0.8 (n = 5, 4 pins) |

Largest fraction at n ≥ 24: 75/344 = 0.218 (ACOPR118 dumps), 161/824 = 0.195. Rule B changes
exactly 14 of the 838: the 8 ACOPR118 dump repairs (base FAIL at 69/70 tried with 245 screen
skips → OK at 74/75, 5 screen pins) and 6 HS15 repairs at n = 2 (base FAIL 0 tried → OK 1 tried).
**No corpus dump has a screen-positive repair candidate at all.**

**(b) In-cell, the lane's `default` cells** (this round; scratch `patch_capdump.py` on a COPY of
the sources, never the worktree; diag run proven representative: 0 of 115+ result/counter
fields differ from the clean run). With the repair UNBOUNDED (`HVEN_F8_PINDIV=0`, scratch):

| cell | repairs | all succeed | pins tried (successes) |
|---|---|---|---|
| ACOPR118 | 16 | yes | **118**, 99, 96, 77×5, 74×5, 69, 68, 16 |
| ACOPP118 | 4 | yes | **118**, 55, 53, 45 |

Most in-cell repairs are screen-positive on EVERY candidate (77/77, 96/96, 99/99): this is rule
B's whole territory. Largest: **118/344 = 0.343** — each cell's first, cold subproblem.

**The bound: `pin budget = max(kRepairPinFloor = 8, ceil(n / kRepairPinDivisor = 2))`**
(`detail::repair_pin_budget`, `qp_engine.h`). 172 at n = 344: **1.46× the largest measured
success**; at n ≤ 16 the floor governs and clears the largest small-n repair (4 pins at n = 5)
by 2×, n = 2's one pin by 8×. Probe factorizations ≤ 2 × the pin budget by construction.
`dep_repair_budget_stops` = 0 on every measured repair (corpus `rbs` column 0 on all 102 rows;
HS pin unchanged at 3342; both cells identical to unbounded).

**Why not n/3 (the WIP's).** OBSERVED with the `n/3` build: on both cells the 118-pin solve-0
repair stops at 115 (`RPX FAIL … tried=115 budget=115 stopped=1`) — the bound cut a measured
success and moved both trajectories:

| cell | n/3 (WIP) | n/2 = unbounded (fix 8) |
|---|---|---|
| ACOPR118 | numerical_error 15 maj, 32 067 minors, 13 542 facts | numerical_error 15 maj, 32 929 minors, 15 489 facts (same f, same KKT) |
| ACOPP118 | numerical_error 8 maj, f 129657.9559, stat 6.73, **43 666 facts, ≈979 s** | numerical_error 5 maj, f 128439.187, stat 4441, **1 833 facts, ≈48 s** |

The n/2 rows were re-run on the CLEAN lane build of the final tree and are identical in every
field to the unbounded diag runs (`.scratch/e2fix8/repro2/`).

## 4. The rules as landed (header text, in `include/hven/detail/qp/qp_engine.h`)

**(i) Rule B, bounded** — the Degeneracy contract's `E2 FIX8, THE BOUNDED REPAIR` paragraph and
the `--- The temporary-vertex repair's BUDGET ---` block. A screen-positive repair candidate is
decided by the EXACT structural dependency alone and otherwise TRIED
(`dep_repair_screen_pins`); screen-negative candidates keep the numerical test unchanged; the
loop's bound is the pin budget, not the screen. The block states the two populations above, the
1.46× / 2× / 8× headroom, that n/3 cut the in-cell 118-pin repairs, and:

> A repair that stops on it is counted QpCounters::dep_repair_budget_stops and FAILS, with every
> consequence a failed start repair has -- on a truly indefinite face the walk has no way off it
> and ends kNumericalError, which is what the screen produced whenever it aborted a repair. THE
> COST WHEN IT BINDS is therefore real, and pinned (QpE2Fix8Budget …): a fraction of n cannot see
> the INERTIA DEFICIT, so a QP needing more pins than the budget -- H = -I on n = 12 needs all
> twelve, its budget is 8 -- is cut where the unbounded repair would have succeeded. None of the
> measured repairs is; a deficit-aware budget is registered. THE BUDGET IS independent_working_set
> ONLY … That its repair is unbounded is registered.

**(ii) F7-1** — the Degeneracy paragraph's (ii) and the run-loop comment at the start-repair site:
when the repair took away a `dep_factor_view` the pass HELD (usable before, unusable after), the
pass is SPENT (`dep_repair_restarts`, walk-seam action `kStartRepairRefused`), and the next pass
re-establishes the factor by the ordinary route; a repair that never probed, or a pass with no
view to lose (border path with no Schur complement yet), runs on as before. Gated on
`independent_working_set`.

Counters (`QpCounters`, `solver_counters.h`): `dep_repair_screen_pins`,
`dep_repair_budget_stops`, `dep_repair_restarts` (plus fix 7's `dep_unrun_tests`).

## 5. Fixtures and values

`tests/qp/test_qp_unrun_dependency_test.cpp` (walk-seam target), both configurations OBSERVED
(`.scratch/e2fix8/logs/fixtures-H2-{release,debug}.log`):

| test | Release | Debug |
|---|---|---|
| `QpE2Fix7Acopr118.Major17…Refactorize` (qp5) | kOptimal 367 min / 519 facts, f −0.154376247286, stat 3.81e-14 | 367/519, f −0.154376252011, 4.33e-14, 2.9 s |
| `…Major17…Border` (qp5) | 397/21, f −0.154376253941, 2.47e-14 | 396/21, f −0.154376253948, 3.52e-14, 70.3 s |
| `…Major18…Refactorize` (qp6) | 353/503, f +0.0395282055522, 1.70e-14 | 353/503, f +0.0395282102954, 3.60e-14, 2.8 s |
| `…Major18…Border` (qp6) | 416/18, f +0.0395282103146, 2.10e-14 | 416/18, f +0.0395282036387, 5.13e-14, 72.1 s |

All four: `screen_pins` 5, `budget_stops` 0, `restarts` 0, `deferred` 0, `undecidable` 0,
objective within 1e-6 rel of the option-off reference, full KKT battery. Debug worst 72.1 s
(< 600 s per case).

| `QpE2Fix7Unrun.AMovingBlockerWithNoFactorIsAnOrdinaryAdd` (both algebras) | now witnesses F7-1: `restarts` 1, `unrun` 0, pass 0 `kStartRepairRefused`, pass 1 the moving add of row 0, exact optimum f = −23/8, 6 minors. Rule A's contract stays asserted at unit level (`WsIndependenceUnrun.*`). |
|---|---|
| `QpE2Fix8Budget.ARepairThatNeedsMorePinsThanItsBudgetStopsAndTheWalkFails` (both algebras, new) | H = −I, n = 12, box [−1,1]: ON `budget_stops` 1, `restarts` 1, `kStartRepairRefused`, **kNumericalError** at 3 minors; OFF `kStartRepair`, kOptimal at a vertex (2 minors) |

## 6. The battery — every mover declared and attributed

Final tree (`35986d1f` == `5a89c0e9` tree), battery H2, one leg 21:16:59 → 22:05:31 rc=0
(`.scratch/e2fix8/logs/battery_H2.out`); the n/3 tree's battery H was identical in every row
below except the constant.

| item | result | attribution |
|---|---|---|
| census T7, HsBattery 13/13, B1Gate 23/23 | 39/39 | — |
| `HsSweepCorpus.Corpus` | **3340 → 3342**, all HS15 cold (1067 → 1069 per KD/SOC cell) | **rule B** (n = 2: the screen fired on the first candidate, the repair pinned nothing; now one pin repairs). Accelerate pin 3717 UNOBSERVED, not adjusted |
| corpus vs fix 7 (`rowcmp V7 H2`) | 3 rows/algebra: Hj_STEENBRD/qp4 +1 minor +1 fact, sfirst 1→0; STEENBRB_def/qp0 +1 minor (+1 fact), +1 stall_admit; STEENBRB_def/qp1 +1 minor (+1 fact refactorize), +1 stall_admit | **F7-1** (`rrs`=1 on exactly these 6 rows); no status/f/residual column; rule B fires on no dump (`rsp` 0), `unrun` 0, `rbs` 0 on all 102 rows |
| corpus vs base k3 | the union of fix 7's 3 rows and the above | rule A + F7-1 |
| E2-off corpus | 0 rows vs fix 7 / k1off | — (all fix-8 code gated) |
| `QpE2WorkingSet.AFullyPinnedRowDecidesAnUndecidablePinExactly` border | minors 1 → 2, `dep_repair_restarts` 1 | **F7-1** (counter route; every correctness statement unchanged) |
| U0 | 17/13/13 = 43, block byte-identical; CSV fields vs fix 7 differ only in `wall_s` | — |
| Release `ctest -j4` | **2882 / 0**, 114.98 s (+6 by name: 4 `QpE2Fix7Acopr118.*`, 2 `QpE2Fix8Budget/*`) | |
| Debug `ctest -j4`, long excluded | **2881 / 0**, 678.24 s | |
| long test alone | 1 / 0, 607.38 s | |
| clang-format / `git show --check` | 0 replacements on all 7 source files / clean | |

## 7. The ACOPR118 cell

Lane build of the final tree (`.scratch/e2fix8/cellbuild.sh`: `git archive` HEAD + submodules,
pin flags, prefix NAMED `install-ff02789b46a7` because hven-bench refuses any other —
`HVEN_PIN.txt` says so; hven-bench `4097f5c` archived). `libhven.a` sha256 `934390c0…`,
`hven_bench_cell` `a8f45479…`. Run alone under the lock, `MKL_NUM_THREADS=1`, `taskset -c 2`
(`repro2/ACOPR118-default.out`, leg 22:05:57 → 22:09:15):

| | RT8 | RT9 (E4 pin) | **fix 8** |
|---|---|---|---|
| status | optimal | numerical_error | **numerical_error** |
| majors | 15 | 21 | **15** |
| objective | 129660.6833784326 | 129660.5787807497 | **129660.68777485074** |
| stationarity / primal_eq | 3.2e-10 / 1.9e-9 | 125.63 / 0.0283 | **0.04498 / 0.02115** |
| qp_minor_iters / factorizations | 17 301 / 11 136 | 21 515 / 11 629 | **32 929 / 15 489** |
| qp_maxiter_exits / qp_cap_retries | 0 / 0 | 0 / 0 | **4 / 0** |
| dep_pin_skips | 3084 | 4955 | **163** |
| wall (informational) | 63.9 s | 81.1 s | ≈198 s |

Identical in all fields to the 06cd4b31 (fix 7 A+B unbounded) cell: F7-1 never fires in-cell
(all 16 repairs succeed) and the budget never binds.

## 8. The cap question — answered by measurement, nothing tuned

- **Does the cap-retry engine engage?** No: `qp_cap_retries` 0, because `SqpOptions::qp_cap_retry`
  defaults to false and the `default` label dumps `qp_cap_retry = false`. By construction, not by
  a decision on these QPs.
- **The cap** is `max(500, 5·(n + mi + #bounded))`: 5300 for ACOPR118, 4710 for ACOPP118.
- **Are the capped QPs honestly "needs more minors"? NO.** The capped QPs were dumped in-cell
  (D4QP v1) and replayed (`.scratch/e2fix8/capreplay.sh`, `logs/capreplay.txt`, each replay
  ≤ 600 s). Replay at the dump's own cap reproduces the in-cell exit EXACTLY (minors and
  factorizations: 5300/69, 5300/26, 5300/1104, 5300/16; ACOPP118 4710/178, /7, /3, /7). At **4×
  the cap** every one is STILL kMaxIter, and on 7 of the 8 the factorization count does **not
  move at all** over the extra 3× minors (69, 26, 16, 178, 7, 3, 7 unchanged; qp5 1104 → 4530 at f 77.9023 → 77.9016) while
  `stall_admits` climbs (qp3 839 → 4742) and f moves in the 4th–7th digit: the walk is cycling
  through stall-admitted zero-factorization passes. The iterate at exit is off its equality rows
  (eq 2.8e-2 … 1.4). **With `independent_working_set` OFF the same eight QPs also cap** (and the two
  ACOPR118 `kNumericalError` QPs, solves 6 and 8, are kMaxIter OFF), so these QPs are hard for
  both arms. **A larger cap would buy nothing; a doubled-cap retry would not rescue them**
  (INFERRED: 4× ⊃ 2×).

## 9. Wall-cost attribution (R3 vs rule B)

| cell | RT8 | RT9 (E4 pin) | pin binary, uncapped (this round) | fix 8 |
|---|---|---|---|---|
| STEENBRG default | 135 s error | dnf_wall | max_iter 86 maj, **≈359 s** | max_iter 86 maj, same f/stat, +37 minors, **≈360 s** |
| YORKNET default | 93 s error | dnf_wall | **> 600 s** (capped) | **> 600 s** (capped, both n/3 and n/2) |
| ACOPP118 default | 12 s | 247 s error | — | **≈48 s** error, 5 maj |
| YORKNET OFF | — | 204 s, error | — | **115/115 counters byte-identical**, ≈208 s |
| STEENBRG OFF | — | 104 s, error | — | **115/115 counters byte-identical**, ≈106 s |
| ACOPP118 OFF | — | dnf_wall | — | numerical_error 8 maj, ≈309 s |

The pin binary is hven-bench's `build/lanes/hven/hven_bench_cell` (sha256 `78d65a55…`, the
pin build e4-fix5 verified byte-identical), run read-only with outputs in this round's scratch.
**The STEENBRG/YORKNET wall is the E4 pin's (R3), measured on the pin itself, and rule B adds ≈0
to it** (STEENBRG +37 minors / +35 factorizations). The OFF path is untouched by construction and
measured identical. ACOPP118's OFF twin takes ≈309 s — just over the 300 s ceiling, which is
why RT9 recorded it dnf_wall.

## 10. RE-TAKE 10 prediction, per cell

MEASURED rows are this round's single serial runs; wall is informational.

| cell | RT9 | prediction | basis |
|---|---|---|---|
| ACOPR118 | numerical_error | **numerical_error**, 15 maj, f 129660.68777, ≈198 s | MEASURED |
| ACOPP118 | numerical_error 247 s | **numerical_error**, 5 maj, ≈48 s | MEASURED |
| STEENBRG | dnf_wall | **dnf_wall** (≈360 s) | MEASURED (uncapped) |
| YORKNET | dnf_wall | **dnf_wall** (> 600 s) | MEASURED |
| DALLASM, ACOPP57, SMBANK, ZAMB2-8 (optimal) | optimal | **unchanged** | INFERRED: RT9 `dep_pin_skips` 0 ⇒ the screen never skipped a repair pin there, so rule B has nothing to change; the budget only binds above 0.5 n (no measured repair is above 0.343 n); F7-1 fires only on a failed repair that probed — not observable from lane columns |
| ACOPR57, READING6 (optimal) | optimal | **at risk, unpredicted** | nonzero RT9 `dep_pin_skips` (1144, 23, a superset); their corpus dumps show no screen-positive repair |
| STEENBRC, NET3, STEENBRD, STEENBRE, STEENBRB | numerical_error | reachable, unpredicted | nonzero RT9 skips; STEENBRB/STEENBRD corpus dumps: F7-1 +1 minor only |
| ACOPR118 OFF / YORKNET OFF / STEENBRG OFF | as RT9 | **identical** | MEASURED (OFF path) |
| ACOPP118 OFF | dnf_wall | dnf_wall (≈309 s) | MEASURED |

TRAIN optimal count: no conversion is predicted; the expected count is RT9's 6 if ACOPR57 and
READING6 hold.

## 11. Registered items

- **F8-1 (the stalled capped QPs).** ACOPR118 qp3–qp6 and ACOPP118 qp0–qp3 (in-cell dumps at
  `.scratch/e2fix8/diag/capdump/*-pd0/`) cycle through stall-admitted zero-factorization passes
  to any cap (4× measured), in both arms. The `stall_admits` growth names the mechanism to look at
  (the stall class R-D left alone, S34). These are the cell's real blocker now.
- **F8-2 (a deficit-aware budget).** A fraction of n cannot see the inertia deficit; the
  `QpE2Fix8Budget` fixture pins the cost (base screen-bounded walk would pin all 12 there and
  succeed — INFERRED from the screen arithmetic, me + |W| + 11 + 1 = 12 ≯ 12; not run).
- **F8-3 (the option-off repair is unbounded)** — deliberately, it is the control arm.
- **F8-4 (lane columns).** `dep_unrun_tests`, `dep_repair_screen_pins`, `dep_repair_budget_stops`,
  `dep_repair_restarts` are not exported by hven-bench (S41's schema event should include all
  four).
- Still open from fix 7: F7-2(ii) (`map_status` folds kMaxIter into numerical_error), F7-5.

## 12. What I could not determine

- YORKNET `default`'s outcome or wall beyond 600 s, on either binary (capped at 600 s to keep the
  cell bounded).
- Whether fix 8 moves any TRAIN cell other than the four run; only RE-TAKE 10 answers that.
- Whether F7-1 fires on any TRAIN cell (no lane column; it fired on 0 of ACOPR118's 16 in-cell
  repairs and on 3 corpus dumps).
- Any Accelerate value (HS 3717 pin UNOBSERVED, not adjusted).
- Wall numbers are single serial runs, informational only (CLAUDE.md §7).

## 13. Evidence index (under the worktree)

| path | what |
|---|---|
| `.scratch/e2fix7/logs/battery_V7.out`, `u0-V7/`, `corpus/V7*` | STEP 1 |
| `.scratch/e2fix8/logs/battery_H2.out` (final), `battery_H.out` (n/3) | battery |
| `.scratch/e2fix8/rpx/`, `rpx_stats.py` | population (a) |
| `.scratch/e2fix8/diag/repro/*-pd0.*`, `*-pdship.*`, `patch_capdump.py` | population (b), budget stop |
| `.scratch/e2fix8/repro/` (n/3 tree), `repro2/` (final), `repro-pin/` (RT9 pin binary) | cells |
| `.scratch/e2fix8/logs/capreplay.txt`, `diag/capdump/` | the cap question |
| `.scratch/e2fix8/logs/wallcap.log` | the three 600 s kills (own pids, logged: YORKNET default on the n/3, n/2 and pin binaries) |

SIGNOFF: M7-W2-E2-FIX8-REPORT
