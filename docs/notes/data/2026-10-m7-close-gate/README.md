# M7 CLOSE GATE -- record (pin `d4b8e84b`; declaration `0de4ae7f`, successor declaration r2 `b9ccfaa6`)

**VERDICT: NOT GREEN -- one leg FAILED its declared criterion: the 57-cell census (20 undeclared movers, all in the path-window cells).** Every other leg passed.
Per the gate discipline the chain stopped at the failure and nothing was patched; the census follow-up below is characterization, not a re-criterion.
Read `DECLARATION.md` (r1) and `DECLARATION-r2.md` first. Everything is MKL on Linux (AMD Ryzen 7 5800X3D, clang 22.1.8, MKL 2026.2.0-881); Apple/Accelerate and Windows: **UNOBSERVED**.
Produced 2026-10-30. The code tree under test is the pin `d4b8e84b` (the declaration commits are docs-only on it; the pre-flight asserted it before the chain and again before r2).

| # | leg | result |
|---|---|---|
| 1 | Release + Debug preset builds, from empty | PASS -- both configures printed `-> REFERENCE (route pins asserted)` exactly once, align 32; builds rc 0, 19 compiler warnings each |
| 2 | Release suite | PASS -- **3232 / 3232**, 0 failed; started names = the declared 3234 (multiset), skipped 1, disabled 2, inventory 3234 |
| 2 | Debug suite (long test excluded) | PASS -- **3231 / 3231**, 0 failed; started 3233, skipped 7, disabled 2, inventory 3234 (incl. the long test) |
| 2 | Debug long test alone | PASS -- 1 / 1 (`Continuation.ProbeBudgetBoundsAFailingProposal`) |
| 3 | install smoke + export contract | PASS -- smoke PASSED, both solvers converged through one generic consumer, 19 standalone-include TUs; export `OK ... align=32` (align read from the smoke's own configure log) |
| 4 | golden rig, three seams | PASS (as declared) -- configure printed both seam pin lines (tycho consumed tree `bfb7d30c`, tycho_sqp `phase-7-close` = `4faa1df1`); scoped **1 failed of 84** and whole **1 failed of 3267** (inventory 3269 = Release 3234 + the M6 delta 35), the failure being exactly `Arms/InteriorPointTrace.P5_InertiaBeforeFactorizationIsAnExplicitState/sqp-old@mkl`; the 3 `FailByDesignControl` tests pass |
| 5 | 27-cell replay, 3 engines | PASS -- gated (1e-5) vs the WX2 record: **0 / 0 / 0** differences; EXACT vs the WX2 record: **0 / 0 / 0** (27 cells x 75 columns each); gated vs the M6 pin: the measured difference set EQUALS the declared 43-set (walk 17 / ssn 13 / ipm 13) by coordinate |
| 6 | suite v1 end-to-end | PASS -- hs 88 rows (22 TRAIN x 4 lanes), cutest 92 rows (23 x 4); schema v1 with split / `hessian_provenance` / stamp (`hven.pin.sha` = the pin, harness `451b51b`); no HOLDOUT cell run. hs run rc 1 = five `REFERENCE FAILURE` rows with all rows written (hs025 x hven-ipm, hven-sqp, snopt; hs033 x hven-sqp, snopt); cutest rc 0. See `suitev1/` and the table below |
| 7 | 57-cell census | **FAIL** -- see below |

## Run history (declared, not hidden)
* r1 chain (`0de4ae7f`): legs 1-5 + suite v1 pin build + harness build GREEN, then **STOPPED at `sv1-guards`**: `schema/set_pin.py --verify` and two unit tests failed because hven-bench's committed `sets/hs-27.json` names the H2 pin `3904a477` (never regenerated across sixteen re-pins) -- independent of hven's code. Logs: `suitev1/guard-*.log`, `chain.txt`.
* r2 (`b9ccfaa6`, successor declaration): the guards leg replaced by assertions that the set pin differs in EXACTLY `hven_pin_sha` and the unit-test failing set is EXACTLY the two stale-pin tests (`suitev1/r2-*`); suite v1 and the census then ran. The frozen r1 files were not edited.

## Suite v1 (leg 6): per-lane TRAIN outcomes (hven-bench `runs`-style CSVs in `suitev1/run-{hs,cutest}/results.csv`)
| set | hven-ipm | hven-sqp | ipopt | snopt |
|---|---|---|---|---|
| hs (22, ceiling 600 s) | 22 optimal | 22 optimal | 22 optimal | 22 optimal |
| cutest (23, ceiling 300 s) | 17 optimal (14 status `optimal`, 3 `acceptable`), 4 dnf_budget (ACOPR57, MSS2, NET3, TAX53322), 2 dnf_wall (MSS3, TAX213322) | 16 optimal, 6 dnf_wall, 1 error | 15 optimal, 3 dnf_budget, 3 dnf_wall, 2 error | 15 optimal, 4 dnf_budget, 1 dnf_wall, 3 error |

hven-ipm cutest `acceptable` rows: ACOPR118 (977 iterations), SAWPATH (70), STEENBRC (208). hven-ipm iteration budget: the lane default `max_iters = 1000`. REFERENCE FAILUREs are recorded by the runner (the evidence of a wrong answer is evidence); they are rows, not missing rows.

## Census (leg 7)
Baseline of record `bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv` (the M6 close census reproduced it 57/57/0, re-proved by the pre-flight). Run: parallel-tiered v1 as the M6 close ran it (T1 44 / T2 3 / T3 10, 6/solo/5-wide, `MKL_NUM_THREADS=1`, one process per pinned physical core), corpus binary stamp `0de4ae7f19ed` (clean), 57 rows, 0 crashed cells.
* Runner's own byte-exact comparison: `57 baseline cells, 57 fresh cells, 48 mismatches`.
* The declared verdict comparator (`--residual-gate 1e-5`): **26 mismatching cells (22 further entries within gate)**. 6 of the 26 are declared-mover cells (the five `f7_n{1000,2000,5000,10000,20000}_bound_neutral` + `f7_n20000_bound_corrupted`); **20 are undeclared movers**, all path-window cells. (The assertion script also listed `f7_n1000_bound_neutral` `kkt_residual` as a 21st: that is a script artifact -- the residual moved 6.3e-14 -> 2.3e-16, inside the 1e-13 absolute floor, so it is within-gate and the cell's only real difference is the declared `qp_minors` 2 -> 3; the verdict does not depend on it.)
* The 20 undeclared movers (`chain.txt` CHECK_FAIL lines): n800 path {neutral, physics, corrupted}; n1000 path {neutral, physics, corrupted, activity}; n2000 path {neutral, physics, corrupted, activity, warm}; n5000 path {neutral, physics, corrupted, activity, warm}; n10000 path physics; the two controls n750 / n825 path neutral. Columns moved are mainly `factorizations`, `qp_minors`, `qp_fact_per_qp` (e.g. n1000_path_corrupted 2976 -> 29828 factorizations; n2000_path_neutral and _corrupted `NumericalError` -> `MaxIter`; n800_path_neutral 29222 -> 9637), plus `status` on five cells: n5000_path_neutral `NumericalError` -> `dnf_budget` (T2, solo, 3600 s), n5000_path_warm `NumericalError` -> `dnf_setup` (T2, solo), n5000_path_corrupted `dnf_budget` -> `dnf_setup` (T3), **n10000_path_physics `Optimal` -> `dnf_budget` (T2, solo; baseline Optimal at 8798 factorizations in 2801 s)**.
* No census-wide declared-mover coordinate is missing: all 10 declared coordinates appeared as predicted. The path-window cells are not in the 27-cell replay (which holds the bound cells and two `path_warm` cells), which is why no earlier gate saw them move.
* Follow-up (`followup-census-confirm/`, written AFTER the data, a section-7 solo confirmation, not a criterion): the 16 T1 (6-wide co-run) undeclared movers were re-run ALONE (`taskset -c 0`, `MKL_NUM_THREADS=1`, box lock): **16 / 16 reproduced byte-identical** on the 13 asserted columns (`census/confirm/`). The three T2 movers already ran solo in the census. NOT re-run: the wall-dependent T3 mover `f7_n5000_path_corrupted` (`dnf_budget` -> `dnf_setup`, 3600 s phases) -- owed a solo run if the settler wants it called a regression. The wall-bound cells (`dnf_*`, status movers on T2) are outcome-wall-dependent and are candidates for the settler's reading, not conclusions.
* Mechanism and the responsible commit are NOT established by this gate: bisecting 20 path cells across M7's walk-engine changes was out of scope. Candidates the settler may weigh: the M7 W2 SQP/QP-tier changes after the M6 pin (E1/E2/E4 working-set and polish changes, restoration, scaling); the replay's declared 43-set is the same lineage's only recorded movement.

## Layout
`DECLARATION.md`, `DECLARATION-r2.md`; `wrappers-close/` (env, run_leg, legs, preflight with falsifiers, chain, close_check, suitev1, check_suitev1) and `wrappers-close-r2/` -- the frozen declarations; `derivation/` (declared sets, derived from git + the WX2 record); `followup-census-confirm/`; `assemble.sh`; `gates/` (the record: `chain.txt`, `logs/`, `suites/`, `rig/`, `replay/`, `suitev1/`, `census/`). Wall-clock everywhere is informational.
