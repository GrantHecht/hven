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

---

# r2 -- review round (Fable review findings M1-M2, m1-m7, n1-n5)

Code and docs commit `b791f164d` on `i19-1`, on top of `45b293077` (r1 text above is kept as written). No `src/` line
changed in this round: `src/sqp/sqp_solver.cpp` is byte-identical to r1, the changes are tests, header comments
(constraint only, no task history), the schema, the guide and the mutation script. Linux x86-64, same toolchain and
box protocol as r1 (every build, ctest and replay under `box-lane`, shared lane, `ctest -j4`, `MKL_NUM_THREADS=1`).

## Findings and what was done

| finding | disposition |
|---|---|
| M1 scaled-solve coverage | Three tests: `TrialDiagnosticsAreInCallerUnitsWhenRowAndObjectiveScalingAreActive` (circle, equality and inequality, `row_scaling` on, objective factor < 0.1 and row factor < 0.5 asserted), `PredDfIsInCallerUnitsWhenRowAndObjectiveScalingAreActive` (exact quadratic, `pred_df` against the model's decrease and against `f_k - f_{k+1}`), each asserting `f_trial`, `h_trial`, `h_lin` (and `pred_df`) against the model's own functions at the step read off the callback's iterates AND against the next row's exported `f` / `violation_l1`. Mutants M8 (`pred_df` drops `/sc.obj`), M9 (`f_trial` drops `/sc.obj`), M10 (equality row factor division dropped), M11 (inequality row factor division dropped) are all killed (`mutations-r2/`). |
| M2 guide state carry | Section 5 now says the carry is declined on a solve with ANY scaling layer (row/objective or column); with the column layer the stored scaled-space state is dropped and the radius restarts at `trust_region_initial`. |
| m1 `ladder.rho_0` on a floor retry | Documentation option taken (not a code change): `rho_0` is the surviving ladder's first penalty, so after a certified fallback's floor retry it is the floor, not the first attempt's value; the work totals do include the first ladder. Stated in `iteration_record.h` and the schema. |
| m2 `switching_ratio` scale | Schema says it is computed in engine scale and does not reproduce from the caller-unit `pred_df` / `violation_l1` on a scaled solve. |
| m3 ladder test premises | `ASSERT_GT(restoration_iters, 0)` and `ASSERT_GT(multi_rung, 0)` added, so the M6/M7 kills cannot pass silently. |
| m4 further tests | `TrialDiagnosticsOnAnInequalityRowAreTheModelsOwn` (unscaled; `h_lin`, `h_trial` on inequality rows); `HLinIsNullUnderAnEqualityBasisReduction` (three rows, two dependent: the basis drops one, `h_lin` null, `h_trial` present); `SocPointIsReportedOnARowWhereTheCorrectionWasTriedAndLost` (asserts at least one such row exists); the ladder test now also asserts the minor-iteration analogue (`minors < counters.qp_minor_iters` across a restoration phase; kill M13, the minor base not advancing past the phase); `ARestorationPhasesRowsCarryTheWorkTheParentLeavesOut` (a sink collects every row of every depth: depth-1 rows exist, `t_s` finite and inside the solve, and the totals summed over all depths equal the solve's `qp_minor_iters` and `factorizations`). |
| m5 writer / callback / contract tests | `set_sqp_signature` is now twelve bools: `cap_binding` is bit 12, true on lines 2 and 3, so `tr_binding` and `cap_binding` each hold true on a line where the other is false (line 0 / 1: tr only; line 3: cap only) and a key swap in the writer changes bytes; the "eleven bools" comment is updated. Hand-built row 0 gives `pred_df`, `f_trial`, `h_trial`, `h_lin`, the two totals, `factorizations_delta` and `t_s` non-default values, row 3 gives `soc_f` / `soc_h`; the four inline goldens carry them. `expect_same_info` (callback) and the trace/contract info check compare `tr_binding` and `cap_binding`. No golden file on disk needed re-deriving. |
| m6 zero-step / judge reads | Mutants added (M12 zero-step read -> `radius_binding`, M14 main judge read, M15 SOC judge read); all three SURVIVE and that is the recorded outcome, argued below, not a gap papered over. |
| m7 census separator | Schema binding-flags paragraph: the active-side census (`active_lower_sides`, `active_upper_sides`, `active_set_size`) counts real bound activity and excludes both kinds of pin, so `tr_binding || cap_binding` is what separates a pinned variable from a bound-active one. |
| n1 header history | Task codenames and "pre-I19-1 meaning" prose removed from `iteration_record.h`, `sqp_major_row.h`, `sqp_solver_impl.h`; the constraint is stated, history lives in the schema freeze table. (`sqp_solver.cpp` comments were not in scope and keep their I19-1 tags.) |
| n2 | `SqpIterationInfo`: "the four fields below" is now "the fields below ... (the two binding flags read false)". The "four" was already stale at r1's base (five fields followed `qp_solved`). |
| n3 | Schema: an unevaluable trial has a NaN objective, written as the string `"nan"` (never `null`), so `f_trial` / `soc_f` read `"nan"` and `h_trial` / `soc_h` reflect the unevaluable bundle (`"inf"` or `"nan"`). |
| n4 | Schema row-work paragraph: an exhausted ladder's row figures are the final rung's counts (`report.qp_minor_iters`). |
| n5 | Long lines wrapped (guide, section 5 `hs` bullet, schema rule 3, the schema's switching/main-status lines). |

### m6: why the zero-step and judge reads are not killed

- Judge reads (M14 main, M15 SOC): `StepContext::tr_active` has no reader. `funnel.h` documents it as carried for the
  driver's radius update and "the acceptance test never reads it, by design", and a grep over `src/` and `include/`
  finds only assignments. Flipping the flag changes nothing observable: these are equivalent mutants.
- Zero-step read (M12): the zero-step short circuit reads the merged flag, so the mutant (radius flag only) differs
  from the original only on a step that is within `qp_no_motion_tol` (`qp.feasibility_tol * max(1, |p|)`, 1e-9
  by default) while a `caller_step_limit` pin is active. A fixture sweep (walk, SSN and IPQP; limits 1e-10 to 1e-7;
  starts (0,4) and (0,1e-3)) found no such state: a cap small enough to put the step inside the tolerance is not
  reported as a pin by the QP engines (below about 1e-7 the capped box makes the linearization infeasible and the row
  goes through the elastic ladder to a restoration verdict identically under both reads), and a cap large enough to
  be reported as a pin gives a step above the no-motion tolerance. The mutant is therefore equivalent over every state
  reached; no unit test is claimed for it and none was committed.

## Gates (r2, head `b791f164d` for the build gates)

| gate | reading | verdict |
|---|---|---|
| ctest Release | 100% of 4106 tests passed, 0 failed (`suites-r2/ctest-release-summary.txt`) | MET |
| ctest Debug | 100% of 4106 tests passed, 0 failed (`suites-r2/ctest-debug-summary.txt`) | MET |
| counter replay vs `origin/m8` base, x3 | same protocol as r1; 9 comparisons, 0 differences (`replay-r2/`). The head corpus binary's sha256 is `f013f5c3...` (r1: `b9d24094...`): `src/` is unchanged but the header edits move the compiled object, so the binary was re-measured rather than reused | MET |
| mutation must-kills | M1-M11 and M13 killed, with the r1 kills re-run on the r2 tests (`mutations-r2/`); M12, M14, M15 survive as argued above | MET (M12/M14/M15 argued) |
| install smoke + export contract | default and clang front ends: PASSED, export contract rc 0 (`install-smoke-r2/`) | MET |
| CI | see the commit that follows this one for the run id | recorded below |

Reader scan (read-only, `hven-bench` main and its tu1 / tu2 working trees, and hven `examples/`, `bindings/`,
`bench/`, `scripts/`): no live reader of the trace key `tr_binding` assumes the old merged meaning. The harness's trace
validator (`schema/trace_v1.py`) ignores unknown keys by its reader contract and does not read `tr_binding`. The only
code that reads the field is the frozen M7 cost-census probe (`runs/2026-09-m7-w2-c1-sqp-cost-census/probe/c1_probe.cpp`,
the C++ field `h.tr_binding`, compiled against the hven of its day) and its summariser
(`wrappers/summarise.py`, `tr_binding_majors` from the probe's CSV); both are frozen run artefacts of a pinned census and
keep the merged meaning they were taken under. Stored traces (`runs/.../traces`) and `schema/fixtures/trace_v1/sqp_complete.jsonl`
carry the old key meaning and the old arity (a copy of hven's golden, which changed in r1); nothing in hven touches them.
`examples/` and `bindings/` do not mention `tr_binding`.
