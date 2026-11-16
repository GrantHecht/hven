# sparse-equil r2 results (code commit 35ab82989939; DECLARATION.md + DECLARATION-r2.md)

- P1 (Release, Debug: 3782/3782) and P2 (replay x3, 0 differences, walk/ssn/ipm): MET (r1, `r1/`), carried.
- P3 TRAIN: **FAILED the declared criterion** (`train/train_check.txt`, `r2-orchestrate.out`). 476 rows per arm, none missing or capped.
  Rows with equilibrated refactors > 0: 54 (all `hven-sqp`; every IPM row identical). Movers vs arm A: 53, ALL among the 54 perturbed
  rows (0 unperturbed movers): better 2, neutral 49, **worse 2**:
  - WORSE `sd-hven-sqp-CRESC100.D` optimal (45 it) -> max_iter (291 it, budget), 21 refactors;
  - WORSE `sd-hven-sqp-CRESC4.K` optimal (26 it) -> numerical_error (factorizations 190 -> 637), 30 refactors;
  - BETTER `sd-hven-sqp-ALLINITC.K` numerical_error -> optimal (78 -> 28 it); `sd-hven-sqp-CRESC100.K` infeasible -> optimal.
  Largest refactor counts: SPANHYD 1487/1476, OPTPRLOC 438, MSS1 248.
- P4, P5: not run (chain stopped at P3, as declared). The dev mutation run (not a leg): control clean, M1..M6 all killed.
- CI on the code commit (run 43881820381): linux-clang-release, linux-clang-coverage, **macos-clang-release (Accelerate)**: success;
  windows-clang-release: failure -- `QpRdvRankRepair.TheSeamRecordsARepairWithItsDropIdentity` (rank_repairs 0, an off-reference route);
  fixed test-only in the next commit, not yet re-dispatched.
