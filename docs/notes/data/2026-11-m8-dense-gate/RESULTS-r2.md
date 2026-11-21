# M8 dense gate -- RESULTS-r2 (against DECLARATION-r2.md, committed `46ccc64de` before the first leg; written after the legs; criteria unchanged)

Code head `c41c759fa` (branch `m8-dense-gate` rebased onto `origin/m8` `286fb5398`, the walk cap). Corpus binary
`db46a7a1...`, lane binary `c34d90d8...` (pin `c41c759fa`). Linux x86-64, Ryzen 7 5800X3D, clang 22.1.8, MKL 2026.2,
Release unless stated, `MKL_NUM_THREADS=1`. Apple/Accelerate and Windows: UNOBSERVED (nothing pushed).

`r2/legs-out/orchestrate.out`: one chain, 2026-11-14 09:32 -> 16:08 (EST); the census (`r2/legs-out/census/`) 10:03 ->
16:08 under the runner's declared co-run tiering. r1 (`DECLARATION.md`, `RESULTS.md`) stays frozen.

| leg | result | evidence |
|---|---|---|
| F1 | MET: Release and Debug "100% tests passed, 0 tests failed out of 3671" | `r2/legs-out/suites/` |
| I1 | MET: 27 cells x walk / ssn / ipm, three times, 0 differences in all 9 (75 columns) against the walk cap's ADDENDUM-r1 rows | `r2/legs-out/replay/` |
| I3 | MET: I3-1 0 differences (43 cells x 30 columns) vs ap5-g1 r3 arm B; I3-2 0 differences vs `bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv` | `r2/legs-out/interior/` |
| F3 | MET: install smoke (IpmSolver and SqpSolver converged) and export contract rc 0, default (GCC) and clang++ | `r2/legs-out/install-smoke/` |
| I4 | MET (`I4_PASS`): I4K 238/238 kNever rows equal the walk cap's leg rows; I4D 41 movers, all declared, none undeclared, none declared-but-unmoved; I4R 238/238 default rows equal the dev run's | `r2/legs-out/identity/tally.txt` |
| I2 | MET (`CENSUS_AS_DECLARED`): 57 cells, 42 exact, PF-1's 2 declared movers as declared, 13 deadline-bound | `r2/legs-out/census/` |

## The movers

The same 41 TRAIN cells as r1, with the same deltas against the new base (`r2/train-movers.txt`): 33 IPM cells move only
`side.dn3b_kkt_dense_flagged_count` (scale-only flags cleared; the IPM does not consume the flag); 8 SQP cells (ALLINITC,
BT7, BURKEHAN, HS88, HS90, HS92, MESH, SPANHYD) move dense counters only -- shifted-solve refusals 41 -> 0, scale-only
flags and their shifted refactors cleared -- plus SPANHYD/MESH eqp refinement counts and ALLINITC's kkt_stationarity in
the residual gate. No status, outcome or iteration count moves; adoption tally (dense vs sparse outcome per cell)
unchanged, dense better 3 / same 235 / worse 0, before and after.

C341's own changes (F1 equilibrated unshifted metric, F4 SSN shifted-step merit test, F7 estimator, the test seam) move
nothing on the replay (ssn mode included), the interior leg, the census or TRAIN beyond r1's set.

## Notes

- I3's references moved to ap5-g1's (declared, section 2 of the declaration): against r1's references the base itself
  reads 2 `stop_reason` differences (ap5-g1's declared change), measured before the declaration.
- The census runner listed 2 cells for serial confirmation (f7_n1000/n2000_path_corrupted): those are PF-1's two
  declared movers, each matching its declared row exactly, so nothing needed confirming.
- Wall-clock is not quoted.
