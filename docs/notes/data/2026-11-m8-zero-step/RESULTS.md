# M8 zero-step -- RESULTS (against DECLARATION.md, committed `47e684d83` before the first leg; written after the legs; criteria unchanged)

Code head `237edda08` over arm A `10ec3df92`. One chain, `legs-out/orchestrate.out`: 2026-11-15 11:09 -> 11:40 (EST), `ALL_LEGS_PASSED`.
Linux x86-64, Ryzen 7 5800X3D, clang 22.1.8, MKL 2026.2, `MKL_NUM_THREADS=1`. No wall-clock quoted.

| leg | result | evidence |
|---|---|---|
| F1 | MET: Release and Debug "100% tests passed, 0 tests failed out of 3730" (arm A's count + the one new seam test) | `legs-out/suites/` |
| I1 | MET: 27 replay cells x walk / ssn / ipm, 0 differences in 75 columns each; interior 0 differences (43 cells x 30 columns); arm A vs arm B byte-exact | `legs-out/replay/` |
| I4 | MET (`I4_PASS`): 476 rows per arm present and uncapped; **0 movers**; 0 WORSE; every row equals the dev run's (I4R 0 differing rows); HS battery (44 hs cells = 22 problems x 2 engines, x 2 labels = 88 rows) 0 movers | `legs-out/tally.txt` |
| M | MET (`MUTATION_KILLED`): the control passes `SqpLadderSeam.ZeroStep*`; with the driver threshold reverted to `kZeroStepScale * max(1, |x|)` the same test fails in all six policy x mode configurations (status kNumericalError, uncertified, in each) | `legs-out/mutation/` (`mutation.diff`, `control.txt`, `mutant.txt`) |

## The movers

None. No TRAIN row, replay cell or interior cell moves between arm A and arm B: on Linux/MKL the restoration sub-solve returns exactly 0
where the step is zero, so the threshold change cannot be seen outside the seam. The seam test is where the change shows
(`dev/seam-without-fix.out` fails 6/6 configurations; `dev/seam-with-fix.out` passes 28/28 `SqpLadderSeam.*`).

## The threshold, and why

`qp_no_motion_tol(qp.feasibility_tol, x_inf) = feasibility_tol * max(1, x_inf)` is the walk's own `step_tol` (its only no-motion test,
value unchanged) moved into qp_problem.h and read by the driver's `zero_step` as well, with `x_inf` the step's own norm because the walk's
iterate is the step in the driver's QPs (so under the tolerance it is `qp.feasibility_tol`, 1e-9 default, absolute). SSN and IPQP have no
no-motion test; their answers are accurate to the same `qp.feasibility_tol`, so one definition is honest for all three modes. The old
fixed 1e-12 scale was 3 decades tighter than the walk's own noise floor; 1e-9 is 3 decades below the default kkt_tol (1e-6), so an unjudged
accepted zero step still cannot move a residual the driver tests by more than ||A|| * 1e-9. Rationale prose: docs/migration-0.2.md.

## Mac verification

See the section appended below (run ids and the twins' readings).

## Notes

- `origin/m8` moved after the cut; arm A is the cut base `10ec3df92`, so A/B isolates this change. The branch merges onto the moved m8 without
  touching `qp_engine.cpp` outside the one `step_tol` line (the parallel polish task's functions are untouched).
- The seam's failing shape was found by scanning perturbations (declaration section 2): only a net-zero pair of slack-column moves is judged
  and rejected by the funnel; a single-component or x-only perturbation is accepted, which is why the seam moves two slack columns.
- Raw per-run dumps are in the git-ignored `build-zs/`; `MANIFEST.txt` lists them (sha256, size, path).

## Mac verification (appended after the Mac run; criteria in declaration section 6)

`gh workflow run ci --ref zero-step` at code head `237edda08` (branch pushed as `zero-step`, never `m8`): run **43732948088**, conclusion
success on all four jobs (macos-clang-release, windows-clang-release, linux-clang-release, linux-clang-coverage). The macOS job: "100%
tests passed out of 3621"; `SqpDriverCertifiedFallback.T7F1TheSameInconsistentLinearizationInAllThreeModesDenseArm` and
`...T3TheTierSolvedRungReachesTheSameVerdictOnTheSameLinearizationDenseArm` Passed with their verdict assertions pinned (no
record-not-pin arm), and `SqpLadderSeam.ZeroStepANoiseStep...` Passed on Accelerate too. One run only (the later commits on the branch add
evidence files only, no source). The GitHub API rate limit (shared) was hit once by a 3 s watch loop; later polls were sparse.
