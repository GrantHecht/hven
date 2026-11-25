# M8 walk cap -- RESULTS (against DECLARATION.md, commit 2701abba9, committed before the first leg)

Written after the legs; criteria unchanged. Code commit `b98a19841` (one commit on `origin/m8` `030bae675`); corpus
binary `99ed4f2b...`, lane binary `09d8dbd0...` (hven-bench-dn1 branch `m8-walk-cap-legs` `c0bc947`, never pushed).
Linux x86-64, AMD Ryzen 7 5800X3D, clang 22.1.8, MKL 2026.2, SAFER_FAST, `MKL_NUM_THREADS=1`. Apple/Accelerate and
Windows: UNOBSERVED (nothing pushed). One orchestration, `orchestrate.out`, 2026-11-13 18:19 -> 2026-11-14 00:56 (EST), worst=0 (ALL_LEGS_PASSED).

## The legs

| leg | criterion | reading | verdict |
|---|---|---|---|
| F1 | "100% tests passed, 0 tests failed out of 3635", Release and Debug | Release and Debug each 100% of 3635 (`suites/`) | MET |
| I1 | 3 runs x walk / ssn / ipm: I1-B vs the base reference, no declared mover; I1-D vs the dev rows, 0 differences | 18 comparisons, 27 cells x 75 columns each, 0 differences in every one (`replay/`) | MET |
| I3 | I3-A default 27-cell interior run vs DN-3 r5's, I3-B baseline invocation vs the DN-3 flip baseline, 0 differences | I3-A 43 cells x 30 columns 0 differences; I3-B 43 x 30, 0 (`interior/`) | MET |
| F3 | install smoke and export contract rc 0, GCC and clang++ | both front ends: smoke PASSED (36 standalone-include TUs), export contract rc 0 (`install-smoke/`) | MET |
| I4 | I4_PASS: every row equals its dev row; the rows differing from DN-3 r5 are exactly the 21 declared | 476 rows, 21 movers (better 0, worse 0, neutral 21), every row equal to its dev row, 0 missing/capped, 0 in-gate residual differences: I4_PASS (`train/train_check.txt`) | MET |
| I2 | CENSUS_WALKCAP_PASS | 57 cells: the 44 non-deadline cells all exact against their reference (the baseline, or PF-1's declared row for f7_n1000/n2000_path_corrupted, both reproduced); the 13 deadline-bound cells byte-equal to the baseline; 0 walk-cap movers: CENSUS_WALKCAP_PASS (`census/census_check.txt`) | MET |

## Every mover, with its direction

Test suite (re-pinned in commit `b98a19841`, DECLARATION section 3):
- BETTER: `SqpEqualityBasis.F9ZeroStepAtAnInconsistencyHandsOffToTheLadder`,
  `SqpEqualityBasisSeam.T6DisabledHandOffRunsF9ToMaxIter` (kInfeasible on the default arm again; the DN-3 flip's
  kNever pin reverted, kNever kept as a second arm); `SqpDriverElasticSeed.T6bTheBandsHIGHRungsReachTheOptimum`
  (false kOptimal at (10, 10) -> kOptimal at (2.5, 2.5), row residual 8.9e-16); the two
  `QpEngine.*ObjectiveInflated*` tests (option-off a = 1e-3: kNumericalError -> kInfeasible, verified certificate).
- NEUTRAL: `HsSweepCorpus.Corpus` total minors Release 3347 -> 3355, Debug 3338 -> 3346 (HS15 cold, +8 minors in
  each KD/SOC cell; statuses, majors, factorizations unchanged).
- TIGHTENED (correct direction, not a regression): `QpDegeneracyArm.CertificateRefusesABoundViolationOutside
  TheStructuralMargin` -- at dual_mu 1e-4 a box leak between 1e-5 and 5e-5 is refused (kNumericalError) where it was
  certified kOptimal.

Replay: none. Interior: none.

TRAIN (21 rows, all hven-sqp small-dense, all NEUTRAL: status, iterations and objective byte-equal on both sides):
ALLINITC K (numerical_error; eq-drift count 91 -> 92); HS64 D/K (optimal; +1 QP minor); HS83 D/K (optimal; +1
factorization, +1 minor); HS88 D/K (infeasible; +5 minors); HS89 D/K (optimal; +2 minors); HS90 D/K (optimal; +4
minors); HS91 D/K (optimal; +1 minor); HS92 D/K (infeasible; +2 minors); LAKES D/K (numerical_error; eq-drift 6 -> 7);
LEWISPOL D/K (infeasible; eq-drift 6 -> 0); SPANHYD D/K (numerical_error both before and after; factorizations
47890 -> 45324 and 47872 -> 40039, QP minors down). A common thread on the optimal rows: fewer terminal polishes
adopted, at the cost of one to five QP minors per solve -- consistent with a formerly excused residual now being
refined before the verdict (not separately attributed). No previously solved cell is worse.

Census: none. Every one of the 57 rows equals its reference row (PF-1's two declared movers reproduce exactly);
the directional criterion had nothing to judge.

## Registered (DECLARATION section 7, unchanged)

- `QpFarkasCertificate` is not exported on the walk's terminal row-certificate path.
- Layers 2 and 3 are unreachable without the seam once the cap holds (defence in depth, each pinned by a seam fixture).
