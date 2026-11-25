# M8 WA-1 r7 -- rider 2: the Fable re-review's rulings 1-6 (DECLARATION, successor of DECLARATION-WA1-r6.md)

Committed before the first r7 leg; never edited afterwards. Every earlier declaration and output is kept. A failed or void r7 leg
is answered by `DECLARATION-WA1-r8.md`.

## 0. Why a successor

The Fable re-review of the r5/r6 range (code `0811f4cf6`) returned SHIP AFTER FIXES (C1', I-a..I-d, minors) with settler
rulings 1-6. This round implements them, on `origin/m8` after RS-R1 (`64e384e2a`; the branch was rebased onto it before any
dev row below was taken; the pre-rebase head is kept as branch `wa-1-pre-rsr1`).

## 1. The change, the code, the binaries

- **Code commit `192d005295913ea8484f445e40c774fa5561c0ce`** on **`origin/m8` `64e384e2a4bf1f80a9dc2d72ee2b0bdf41911ed4`** (arm A, worktree `../wa1-base`). Code
  paths `src include tests bench examples bindings CMakeLists.txt cmake scripts` of the leg HEAD must equal it
  (`legs/orchestrate_r7.sh`). Two code commits over the base's WA-1 history: `5042e815c` (rulings 1, 2, 4, 6) and `192d00529`
  (ruling 5, plus `footprint_rungs_off` removed: at a cap of one it is implied by the cap; a `static_assert` holds the cap at 1).
- **Ruling 1 (C1')**: the kOk regularized step is DROPPED. A kOk face whose penalty merit rises along p is a counted decline;
  `EqpResult::x_regularized` is removed (no other user), with the counter `ascent_footprint_regularized_steps` and the trace value
  `kRegularizedStep`. Engine-level assertion (tests): the pass after a footprint ride lands at or below the ride's start, unless
  that pass is the rule's counted decline (`Tally::follow_rose`, in `expect_portable` and on qp26).
- **Ruling 2 (I-a)**: a footprint ride whose add site passes blockers over (N2) is re-checked against the row budget at the
  point it lands on; outside it the ride is refused -- counters restored (`ascent_footprint_rides`, `ascent_rides`,
  `dep_passovers`), `ascent_footprint_ride_refusals` counted, the trace's `QpWalkFootprint::kRideRefused` -- and the pass is
  solved again without the ride (a rung if one is left, else the counted decline). Test
  `SyntheticRidePassedOverOutOfTheBudgetIsRefused` (bound `x0 <= 1e-10` dependent through the pinned column, blocker at
  `x2 = -4e-5`).
- **Ruling 3 (I-b)**: the revert stays as the stopgap. `solver_counters.h`'s `dual_regularization_decreases` doc is the release
  re-admission's again (footprint rungs no longer use its slots, ruling 5), so a reverted rung never held a shared slot; the
  footprint rung cap is one and a revert spends it. Open item registered (section 6).
- **Ruling 4 (I-c)**: a pass solved again without moving (footprint rung, its revert, a refused ride) restores the drop record
  (`last_drop = dropped` when no new drop was recorded) and `standstill_steps`, `stagnation_idle`, `stagnation_read`, the
  gray-hold carry (`gray_hold_next = gray_hold_prev`) and `gray_return_pending` (`resolve_pass_again`). Every one of these is
  per-pass state the redone pass would have read had the pass not been interrupted; restoring is the faithful "the same pass
  solved again". Invariant (tests, `expect_counted`): the record after such a pass has the same `obj_in` and `post_drop`.
- **Ruling 5 (I-d)**: footprint rungs on their own cap `kMaxFootprintRungs = 1`, not consuming `kMaxDualRegDecreases` (measured,
  section 2; adopted).
- **Ruling 6 (minors)**: helper arguments are structs with designated initializers (`FootprintSlopeArgs`,
  `FootprintBudgetArgs`); the tally excludes kMeritFalls passes; the decline test has a route-independent assertion (no ride:
  the blocker is past the budget at every mu); header/test narrative trimmed (walk_engine.h 4c, test_qp_refused_exchange.cpp,
  test_qp_ascent_guard.cpp, walk_engine.cpp's guard comment); rung, revert and refusal records carry `dx_inf = 0` and the revert
  record `active_res_inf` and `post_drop`.
- The test file moved to `tests/qp/seam/test_qp_ascent_footprint.cpp` (RS-R1 layout; it needs `HVEN_TESTING`).
- Corpus binaries (`build-wa/bin/{A,B}/hven_sqp_corpus`): A `bd3d322803b1d98e90e6b622b39901da19a48991cdf1caf2a9673ee28e8c71c9`,
  B `dac7579832eeb91d8f7f058195e90443a18f57963b5fde791935d12ea5b64403`.
- Lane binaries (`build-wa/bin/{A,B}/hven_bench_cell`; private clone `.scratch/hb`, branches `wa1r7b-A` / `wa1r7b-B`): A
  `ece63b01bde7a7d854a6416ec6e8253b007757cc086952ff0935de93990df641`, B
  `bf29cc72076bc069772e73c71a3402b3a9f0b8ab11b23631fbd6b3aa1e627ade`. The r5/r6 binaries are kept in `build-wa/bin-r5/`.

## 2. Disclosures (none is a leg)

- **Ruling 1 measured** on the r5 code (`r7/c1k-train.txt`, `r7/c1k_sequences.py`, `r7/c1k_summary.py`, records
  `r7/c1k-train-records.txt`; scratch branch `wa1-meas3` 3b381fc6d, env-gated prints, never shipped, diff
  `r7/c1k-scratch-3b381fc6d.diff`; lane `build-wa/bin/R1`, hb branch `wa1r7m-R1`): TRAIN 57 kOk regularized steps in 5 cells,
  34 sequences (consecutive regularized passes merged), 33 ended by a pass: **19 land at or above the no-rule f** of the
  sequence's first step (14 below); of the **21 ending in a row-restoring step, 18 land at or above the no-rule f**. The step
  mostly postpones: dropped. A sticky variant was not measured (the regularized answer is a fixed point of the next pass's
  regularized system, so a sticky step has nothing left to take).
- **What the drop costs** (measured on the shared-budget code, lane C = `5042e815c`'s pre-rebase twin, `r7/r5-cap-variant.txt`):
  CRESC100.K returns to arm A's row (infeasible; r5 had it optimal, the one BETTER mover), HS56.D no longer moves; and the
  pass-42 qp26 fixture (`YRK_YORKNET_qp26_m42`), which r5 certified kOptimal after one kWrong ride and one kOk regularized step,
  now ends **kNumericalError after 906 minors (126 eq drift violations, f -1456.834)**: at minor 19 the kOk face's merit rises
  along p, the rule declines, and that ordinary step raises f by 0.10 and leaves the rows by 8e-6. The kWrong ride (minor 18)
  still fires and falls. The end-to-end qp26 test (`QpPlatformRetake.YorknetRestorationQp26EscalatesToTheMatchedRetake`) and
  qp25's pin are unchanged.
- **Ruling 5 measured** (`r7/r5-cap-variant.txt`; scratch branch `wa1-r7cap` 61643b4b0, lane V): vs the shared-budget code,
  TRAIN qp minors 209313 -> 206415 (-1.38%), factorizations 250134 -> 242329 (-3.12%), no status change, movers MSS1.D and
  MSS1.K only (both NEUTRAL, numerical_error): **MSS1.D 17636 -> 9525 minors (21652 -> 10119 factorizations), MSS1.K 5903 ->
  11116 (7411 -> 11139)**. Adopted.
- **MSS1.D is a cost mover** (ruling 5's disclosure): arm A 9003 minors / 10656 factorizations; r5 15627 / 21568 (22 rungs, 289
  declines, reported NEUTRAL there); r7 9525 / 10119. MSS1.K: A 9521 / 9501, r7 11116 / 11139.
- **Dev on the final range** (`r7/dev/`, raw `.scratch/r7/`): ctest Release and Debug "100% tests passed, 0 tests failed out of
  3925"; replay A vs B 0 differences in walk, ssn, ipm; TRAIN 476 rows, 0 missing/capped, 7 movers (0 BETTER, 7 NEUTRAL, 0 WORSE);
  totals qp minors 209264 -> 206415 (-1.36%), factorizations 249651 -> 242329 (-2.93%). Arm A's TRAIN rows on the new base equal
  r5's arm A rows (RS-R1 moves no TRAIN row); B's equal the pre-rebase lane V rows.
- **Mutation pre-flight** (`.scratch/r7/mut-pre`): control clean; the 18 must-kill mutations each KILLED (N23 there as "no cap", killed by the 900 s timeout with 3 failing tests; re-declared as a cap of two and re-run alone in `.scratch/r7/mut-n23`: KILLED by 5 tests); N20, N21 recorded with no killer (`r7/mut-pre/`). A first pre-flight on the pre-cleanup code found N16 (ignore
  `footprint_rungs_off`) surviving -- equivalent at a cap of one -- and the flag was removed (N16 retired).
- **qp25 in Debug** (`r7/qp25-debug/`, `legs/qp25_debug_revert.sh`): with the revert, qp25 is kOptimal in Debug (80 minors, f = -5.79957813652, both algebras -- the same point as Release); with it removed (N15) qp25 ends kNumericalError (87 minors, at the same f) and fails `QpYrkRefusedExchange.YorknetRestorationQp25{Border,Refactorize}` in Debug. Debug passing depends on the revert.
- CI on the code commit: run 44626643408 dispatched before this file (green on all four lanes when written).

## 3. Expected movers (declared)

- **Tests**: `tests/qp/seam/test_qp_ascent_footprint.cpp` (14 tests: 3 helper, 9 synthetic, 2 qp26): the kOk regularized-step
  test replaced by `SyntheticKOkFaceWhoseMeritRisesDeclines`; `SyntheticRidePassedOverOutOfTheBudgetIsRefused` added; qp26's
  reference assertions now expect the decline (`ascent_footprint_declines >= 1`) and no longer assert kOptimal or zero eq drift
  (KKT checks run only when kOptimal) -- **a declared test mover: the fixture regresses from kOptimal to kNumericalError**
  (section 2). `QpYrkRefusedExchange`'s qp25 pin stays `-5.79957813652` (comment only).
- **Replay**: none (`r7/movers-replay.txt`).
- **TRAIN**: `r7/movers-train.txt` -- HS101.D, HS101.K, MESH.D, MSS1.D, MSS1.K, SPANHYD.D, SPANHYD.K, all NEUTRAL. **Cost bound**:
  TRAIN totals within +3% qp minors and +3% factorizations of arm A; no WORSE mover.

## 4. The legs and their criteria (fixed here, before data), outputs under `r7/`

| leg | criterion |
|---|---|
| P1 | ctest Release and Debug: each log reads "100% tests passed, 0 tests failed out of 3925" |
| P2 | replay 3 x walk/ssn/ipm vs arm A's r7 dev rows (exactly the declared movers: none) and vs B's r7 dev rows (0 differences) |
| P3 | TRAIN 238 x D/K serial: `I4_PASS` against `r7/movers-train.txt` with r7's dev rows as reference; no WORSE mover |
| P4 | `runtime/run_walk.sh` into `r7/runtime/` (walk, serial, solo): THROTTLE 0; counter identity; filtered B/A in [0.995, 1.005] |
| P5 | `legs/mutations_r7.sh`: control clean; N1-N6, N8-N12, N14, N15, N17-N19, N22, N23 each KILLED (MUTATIONS_AS_DECLARED); N20, N21 recorded; tree restored |
| P6 | `legs/install_smoke.sh`: install smoke and export contract rc 0, default (GCC) and clang++ front ends |

Mutations (`legs/mutate_r7.py`): N1-N15 are r5's re-anchored (N7, N13 retired with the regularized step, N16 with the flag).
New: N17 the landing re-check off; N18 the refusal flag ignored; N19 the kOk decline uncounted; N22 the refusal's counter
restore removed; N23 the footprint rung cap doubled -- each must be KILLED. Recorded, not must-kill: N20 (the rung's
`last_drop` restore removed) and N21 (`resolve_pass_again` emptied) -- no seam fixture takes a footprint rung, revert or refusal
on a post-drop pass or with a standstill/stagnation/gray-hold run open (a post-drop pass whose exact Newton step enters the
released constraint's interior descends, so only a footprint-driven ascent reaches the rule there); the invariant in
`expect_counted` would read it if one did.

SPANHYD.K's B row was bimodal across processes in r5 (`r6/spanhyd-solo.txt`); r7's dev row is the solo mode (23 iterations,
20306 minors). A P3 difference on that row alone is answered as r6 answered it: a successor declaration after a solo re-run.

HOLDOUT is never run. The orchestrator stops at the first failed leg.

## 5. Order

```
flock /tmp/box-build.lock setsid -w bash docs/notes/data/2026-11-m8-wa1/legs/orchestrate_r7.sh 192d005295913ea8484f445e40c774fa5561c0ce bd3d322803b1d98e90e6b622b39901da19a48991cdf1caf2a9673ee28e8c71c9 dac7579832eeb91d8f7f058195e90443a18f57963b5fde791935d12ea5b64403 ece63b01bde7a7d854a6416ec6e8253b007757cc086952ff0935de93990df641 bf29cc72076bc069772e73c71a3402b3a9f0b8ab11b23631fbd6b3aa1e627ade 3925 > docs/notes/data/2026-11-m8-wa1/r7/orchestrate.out 2>&1
```

## 6. Open items registered

- kSuspect after any dual_mu rung: one guard for both rung sites (the footprint rung and the release re-admission rung, which
  lowers mu with no guard and can re-reach a kSuspect mu) or a static-pivot pre-check (ruling 3).
- The qp26 m42 fixture's kOk decline (section 2): the footprint's kOk case has no remedy in this rule; the P-1a dual centring
  round is where it is removed at its source.

## 7. CI

Push `wa-1`, `gh workflow run ci.yml --ref wa-1`, all four lanes green; recorded in `RESULTS-r7.md`. Not a leg.
