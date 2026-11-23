# I19-1 -- RESULTS (hven issue #19: docs and report-only telemetry)

Code commit `f13ae09fe` (docs commit `80e8784d4`) on branch `i19-1`, one step above `origin/m8` `fd1e318b5`.
Linux x86-64, AMD Ryzen 7 5800X3D, clang 22.1.8, MKL (system oneAPI), SAFER_FAST, `MKL_NUM_THREADS=1`,
2026-11-23. Every build, ctest and replay ran under `box-lane` (shared lane, never `--exclusive`, `ctest -j4`).
Apple/Accelerate: the macOS CI lane (run 45238799403) is the only observation; nothing local. Nothing here is a timing claim (wall-clock columns are informational).

## What changed (behaviour-neutral by construction)

Reporting only. `SqpIterationRow` gains eleven trailing keys (`cap_binding`, `pred_df`, `f_trial`, `h_trial`,
`soc_f`, `soc_h`, `h_lin`, `qp_minor_iterations_total`, `qp_factorizations_total`, `factorizations_delta`,
`t_s`); `ElasticLadderRow` gains `rho_0`, `minor_iterations`, `factorizations`; `SqpIterationInfo` gains
`tr_binding`, `cap_binding`. `SqpMajorRow::tr_binding` is renamed `region_limited` and stays the merged flag
every decision reads; the exported `tr_binding` is the radius only (declared pre-tag meaning change, freeze-table
row in `docs/trace-schema-v1.md`). No decision reads a new field.

## Gates

| gate | reading | verdict |
|---|---|---|
| ctest Release | 100% of 4100 tests passed, 0 failed (`suites/ctest-release-summary.txt`) | MET |
| ctest Debug | 100% of 4100 tests passed, 0 failed (`suites/ctest-debug-summary.txt`) | MET |
| counter replay vs `origin/m8` base, x3 | walk / ssn / ipm, 27 cells x 75 columns, three head runs each compared byte-exact (`scripts/compare_replay.py`, no residual gate) to the base run: 9 comparisons, 0 differences in every one (`replay/comparison-k*-*.txt`) | MET |
| install smoke + export contract | default (GCC) and clang front ends: smoke PASSED, export contract rc 0 (`install-smoke/`) | MET |
| unit test per new field | `tests/sqp/test_iteration_telemetry.cpp`, 9 tests (below) | MET |
| mutation must-kills | M1-M7 all killed (`mutations/`) | MET |
| CI (`workflow_dispatch` on the branch, run 45238799403 at `f13ae09fe`) | linux-clang-release, macos-clang-release, windows-clang-release, linux-clang-coverage: all four success | MET |

Replay protocol (declared): one base run per mode (`hven_sqp_corpus` built from `origin/m8` in a separate
checkout, sha256 `cd4633e9...`), then three head runs per mode (head binary sha256 `b9d24094...`), all serial, one
process at a time, `MKL_NUM_THREADS=1`; the cell list is line 3 of
`docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt` (the walk-cap round's list). The replay corpus runs
with callback and trace off (the shipped shape); the callback and trace paths of the new fields are covered by the
unit suites. `legs/replay.sh` is the script; the base and head CSV provenance headers are in `replay/*.csv`.

## The new tests (what each pins)

- `PredDfAndTheTrialPointAgreeWithAnIndependentRecomputation`: on a quadratic objective with a linear equality,
  `pred_df` equals `-(g.p + p'Hp/2)` recomputed from the model and the step `p = x_{k+1} - x_k` read off the
  callback's iterates, `f_trial` and `h_trial` equal the model's `f` and `|c|` at `x_{k+1}`, and (the model being
  exact) `pred_df == f_k - f_{k+1}`; `h_lin` equals `h_trial` because the linearization is exact.
- `HLinIsTheLinearizedViolationOfANonlinearRowAtTheStep`: on a circle constraint with a small radius,
  `h_lin == |c + J p|` recomputed from the model, and exceeds 1 on row 0 (the radius leaves the linearization open).
- `TheSocPointIsReportedOnTheRescuedRowAndOnlyThere`: on the Maratos rescue, `soc_f`/`soc_h` equal the model's `f`
  and `|c|` at the iterate the rescue moved to; `soc_*` are `null` on every row without a SOC re-solve.
- `ACapPinIsCapBindingAndNotTrBinding`, `TheRadiusIsTrBindingAndNotCapBinding`,
  `NeitherFlagIsSetWhenNothingLimitsTheStep`: the split, on the row and on `SqpIterationInfo`; and that a
  cap-bound accepted step still doubles the radius (growth reads the merged flag, the decision is unchanged).
- `RowWorkTotalsReconcileWithTheSolvesCounters`: the per-row totals sum to `counters.qp_minor_iters` and
  `counters.factorizations`; `factorizations_delta` sums to `counters.factorizations`.
- `ALadderRowsTotalsDominateItsLastRungAndItsOwnShare`: on an exhausted ladder with a restoration phase, the
  ladder's own totals are at least the row's last-rung figures (strictly more over seven rungs), the row total is
  at least the ladder's, and the row totals leave out the restoration phase's folded work while the deltas see it.
- `TheRowClockIsMonotoneAndInsideTheSolve`: `t_s` is finite, non-decreasing, and not past `wall_seconds`.

## Mutation must-kills (`legs/mutate.sh`, diffs and logs in `mutations/`)

Each is a one-line edit to `src/sqp/sqp_solver.cpp`, built into the Release tree and run against
`IterationTelemetry.*:CallerStepLimit*`; the source is restored from a saved copy afterwards.

| mutant | edit | killed by |
|---|---|---|
| M1 | exported `tr_binding` reads the merged flag | `ACapPinIsCapBindingAndNotTrBinding`, `CallerStepLimitSolve.ALimitPinIsReportedFreeAndTheStepRegionLimited` |
| M2 | `cap_binding` never set | the same two |
| M3 | radius growth reads the radius-only flag | `ACapPinIsCapBindingAndNotTrBinding` (the cap-bound step no longer doubles the radius) |
| M4 | `qp_factorizations_total` is the cumulative counter, not the delta | `RowWorkTotalsReconcile...`, `ALadderRowsTotals...` |
| M5 | the row-work base never advances | the same two |
| M6 | ladder `factorizations` is the last rung's, not the sum | `ALadderRowsTotals...` |
| M7 | a restoration phase's folded factorizations are not excluded from the row totals | `ALadderRowsTotals...` |

## Not done / registered

- The telemetry is not exercised by the replay corpus (callback and trace off there); the unit suites cover it.
- `h_lin` is `null` under an equality-basis reduction (the QP's rows are then not the model's); a reduced-QP
  `h_lin` would need the lift's row map and was left out.
- `t_s` of a depth-1 row counts from the nested solve's own entry (documented in the schema); a solve-wide
  clock for restoration rows would need the parent's entry stamp passed to the sub-driver.
- The ladder block's totals (`minor_iterations`, `factorizations`) are the surviving ladder's, plus a certified
  fallback's first ladder when it retried at the floor; a walk-route ladder has no retry.
