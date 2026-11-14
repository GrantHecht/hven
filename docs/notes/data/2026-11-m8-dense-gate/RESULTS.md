# M8 dense gate -- RESULTS (against DECLARATION.md, committed `71e50ea41` before the first leg; written after the legs; criteria unchanged)

Code head `53fc92c93` (on `origin/m8` `030bae675`). Corpus binary `927cfc3d...`, lane binary `e17dc5f3...` (pin
`53fc92c93`). Linux x86-64, Ryzen 7 5800X3D, clang 22.1.8, MKL 2026.2, Release unless stated, `MKL_NUM_THREADS=1`.
Apple/Accelerate and Windows: UNOBSERVED (nothing pushed).

`legs-out/orchestrate.out`: queued behind other lanes' legs on the box lock (flock-waiter, idle) from 18:20 to 00:56,
then ran 2026-11-14 00:56 -> 07:33 (the census legs-out/census/ 01:28 -> 07:33, the runner's declared co-run tiering).

| leg | result | evidence |
|---|---|---|
| F1 | MET: Release and Debug "100% tests passed, 0 tests failed out of 3638" | `legs-out/suites/` |
| I1 | MET: 27 cells x walk / ssn / ipm, three times, 0 differences in all 9 (75 columns) | `legs-out/replay/` |
| I3 | MET: I3-1 0 differences (43 cells x 30 columns), I3-2 0 differences (43 x 30) | `legs-out/interior/` |
| F3 | MET: install smoke and export contract rc 0, default (GCC) and clang++ | `legs-out/install-smoke/` |
| I4 | MET (`I4_PASS`): I4K 238/238 kNever rows equal DN-3 r5's; I4D 41 movers, all declared, none undeclared, none declared-but-unmoved; I4R 238/238 default rows equal the dev run's | `legs-out/identity/tally.txt` |
| I2 | MET (`CENSUS_AS_DECLARED`): 57 cells, 42 exact, PF-1's 2 declared movers as declared, 13 deadline-bound | `legs-out/census/` |

## The TRAIN movers (I4D), direction

- 33 IPM cells: only `side.dn3b_kkt_dense_flagged_count` falls (roster total 982 -> 24). The IPM does not consume the
  flag: no IPM status, iteration, objective or residual moves. The flags were raw-scale artifacts.
- 8 SQP cells, dense counters only, no status / outcome / iteration change:
  - ALLINITC: flags 20 -> 0, shifted refactors 20 -> 0, dense factorizations 72 -> 52, eqp refinement iterations
    39 -> 0, inertia-suspect verdicts 8 -> 0, kkt_stationarity 3.6e-15 -> 7.1e-15 (in the residual gate). Optimal,
    28 iterations, both.
  - HS90: flags 1 -> 0, refactors 1 -> 0, dense factorizations 99 -> 98, eqp refinement 2 -> 0 (the HS90 row is
    singular by scale only).
  - SPANHYD: flags 6 -> 0, shifted refusals 6 -> 0, refactors 6 -> 0, factorizations 47890 -> 47884, eqp refinement
    29 -> 14, eqp residual refusals 16 -> 10 (still numerical_error at 22 iterations, as before, and as kNever's 23).
  - MESH: shifted refusals 4 -> 0, eqp residual refusals 4 -> 2 (still max_iter 273; its 1068 flags are intrinsic and
    stay).
  - HS92 (23 -> 0), BURKEHAN (6 -> 0), BT7 (1 -> 0), HS88 (1 -> 0): shifted refusals only.
- No cell is made worse on any status, outcome or iteration count. Counter movement over the roster (SQP default
  rows): dense factorizations 72797 -> 72764; flagged factorizations 1126 -> 1099; shifted refactors 1126 -> 1099;
  shifted-solve refusals 41 -> 0; unshifted dense-solve refusals (dn3a) 0 -> 0.

## The adoption tally (reported): dense (default) vs sparse (kNever) outcome per TRAIN cell

Outcome = (reference_ok, converged in {optimal, acceptable}).

| | dense better | same | dense worse |
|---|---|---|---|
| before (DN-3 r5 flip rows) | 3 (ipm LEWISPOL, sqp ALLINITC, sqp CRESC100) | 235 | 0 |
| after (this code) | 3 (the same) | 235 | 0 |

The cells whose dense and sparse statuses differ are the same 8 before and after (ipm ALLINITC, BURKEHAN, LEWISPOL,
MESH; sqp ALLINITC, CRESC100, CRESC50, MESH), with identical statuses and iterations. The dense arm had no outcome
losses on TRAIN to shrink: the defect this round fixes lived in the constructed fixtures (the elastic row scale, the
DN-1d refusal fixtures), and on TRAIN it showed only as refused shifted answers (41) and scale-only flags (27 SQP, 958
IPM), all gone or cleared without moving an outcome.

## Notes

- **The T7 certified-fallback mover the diagnosis saw under `DIAG_NOFLAG`** (SqpDriverCertifiedFallback.
  T7TheEscapeBranchTernaryCONSUMESAnAttachedReportAtTheDriver, 2 fallback entries instead of 3) does not occur: the
  test passes unchanged at the default on Release and Debug (3 entries, 2 activations). `DIAG_NOFLAG` removed every
  flag; fix 1 removes only flags the equilibrated matrix does not confirm. The T7 fixture's KKT (an inconsistent
  dependent row pair at scale 1e8, the elastic fixture's shape) is singular whatever its scaling, so it stays flagged
  and shifted, and fix 2's acceptance of its shifted answers leaves the route as it was.
- (3) "retake on the platform when a shifted factorization's solves are refused" stays registered, not implemented:
  after fix 2 no TRAIN cell, replay cell or suite test refuses a shifted answer (41 -> 0 on TRAIN), so no corpus QP
  fails on that path.
- The rebase onto the moved `origin/m8` (`92e7c5d83`, ap5-g1 landed) is a clean merge (`git merge-tree`); the legs
  ran on `030bae675` + this branch.
