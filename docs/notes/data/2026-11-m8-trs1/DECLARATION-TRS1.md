# DECLARATION-TRS1 -- the trace prints the quantities the tolerances test; the console legend is removed

Written before any gate leg; criteria are never changed after data. A failed or void leg is answered by a `-r2` file. Branch `trs1` from m8 `0cecdc6d0`.
Declared trace/golden event: the printed `stat`/`feas`/`comp` change meaning on every scaled solve; the human goldens, the v1 JSON goldens and the trace-schema freeze table are re-derived. The v1 freeze has not been tagged (M8 close), so this is a pre-tag correction (trace-schema-v1.md section 6, same ground as the TR-D rider).

## 1. Audit (base `0cecdc6d0`; line numbers are the base's)

### 1.1 What each termination test compares

| engine | test | quantity compared | units / scale | abs or rel | norm | where |
|---|---|---|---|---|---|---|
| IPM | optimality | `kkt_inf` < `common.optimality_tol` | the solve's problem: objective multiplied by `solve_obj_scale_` (`objective_scale`, auto-scaling), no row scaling; z-form dual infeasibility incl. bound multipliers | absolute | inf | `ipm_solver.cpp:1929` (`converge_check`); measure `fill_residual_info` `:1307` |
| IPM | feasibility, equality | `econ_inf` < `common.feasibility_tol` | same (rows unscaled) | absolute | inf (nan-sticky) | `ipm_solver.cpp:1930`; `:1337` |
| IPM | feasibility, inequality | `icon_inf` < `common.feasibility_tol` | same | absolute | inf | `ipm_solver.cpp:1931`; `:1318` |
| IPM | complementarity | `barr_inf` < `effective_complementarity_tol` = `termination.complementarity_tol` else `optimality_tol` | engine scale (carries the objective scale), slack and bound pairs, restoration elastics folded in while nested | absolute | max of products | `ipm_solver.cpp:1932`, `ipm_options.h:250`; `:1333` |
| IPM | acceptable tier | the same four against `acceptable_*_tol` | same | absolute | same | `ipm_solver.cpp:103-106` |
| IPM | divergence | the same four against `divergence_*_tol` (and non-finite) | same | absolute | same | `ipm_solver.cpp:1944-1959` |
| SQP | optimality | `SqpKkt::stationarity` <= `common.optimality_tol` | the engine's scaled problem: objective factor, row factors, column factors (when active) | absolute | inf of the reduced/projected grad L | `sqp_solver.cpp:8157` (`mj.converged`); measure `evaluate_kkt` |
| SQP | feasibility | `SqpKkt::feasibility` <= `common.feasibility_tol` | engine units (a row shrunk by its factor passes at `feas_tol / s_j`) | absolute | inf of `max(\|cE\|, max(cI)+, bound violation)` | `sqp_solver.cpp:8158` |
| SQP | complementarity | `SqpKkt::complementarity` <= `common.optimality_tol` | engine units | absolute | `max_j \|lambda_i(j) cI_j\|` | `sqp_solver.cpp:8159` |
| SQP | caller-unit feasibility guard (scaled solves) | caller-unit `feasibility` <= `common.feasibility_tol`; a refusal keeps iterating | caller units | absolute | same | `sqp_solver.cpp:8170-8175` (`mj.caller_row`, `scaled_convergence_refusals`) |
| SQP | caller-unit stationarity ceiling (column-scaled solves only) | `caller_unit_stationarity` <= `kCallerStationarityCeiling` | caller units | absolute | inf | `sqp_solver.cpp:8180-8187` |

No test is relative; both engines compare absolute values against the tolerance.

### 1.2 What each output printed at the base

| output | IPM | SQP |
|---|---|---|
| console `stat` / JSON `iter.stationarity` | `kkt_inf / solve_obj_scale_` (caller objective units; NOT the tested value under objective scaling) `ipm_solver.cpp:2857` | `stationarity / obj` when row scaling is active (caller units; NOT tested) `sqp_solver.cpp:7689-7699` |
| console `feas` / `iter.feasibility` | `max(econ_inf, icon_inf)` (the tested value) `:2858` | caller-unit feasibility when row scaling is active (the guard's value, NOT the primary test) |
| console `comp` / `iter.complementarity` | `barr_inf / solve_obj_scale_` (NOT tested) `:2859` | `complementarity / obj` when scaled (NOT tested) |
| `ipm.phase.exit.stationarity/complementarity` | `kkt_inf` / `barr_inf`, engine scale (tested) | n/a |
| `solve.end.convergence` | engine measure of the returned row (tested units): `ipm_solver.cpp:5414-5420` | `kkt` of the returned point, engine units `sqp_solver.cpp:281-284` |
| `solve.end.diagnostics`, console footer `diagnostics` | the DECLARED set in caller units (`SolveResult`) | same |
| console header `tolerances` | `optimality_tol`, `feasibility_tol` (solve.begin) | same |

Unscaled solves: every printed value equals the tested value in both engines (the rows already were). Under scaling the IPM printed stat/comp and the SQP printed stat/feas/comp differed from the tested value; this is the defect (a stationarity of 1.36e-11 printed while the test, at the 73626x objective factor, read 1e-6).

## 2. The change

- `IterationRecord` gains `tested_stationarity`, `tested_feasibility`, `tested_complementarity`: the three values exactly as the test compares them (IPM: `kkt_inf`, `max(econ_inf, icon_inf)`, `barr_inf`; SQP: `SqpKkt::{stationarity, feasibility, complementarity}`, recorded at measure time in `SqpMajorRow`, not mapped on export). The unprefixed fields keep their caller-unit values, so `history` and the callback (`IterationEvent`) are unchanged.
- Console columns `stat feas comp` print the tested values; headers become `stat_scl feas_scl comp_scl`. The legend block, `Format::legend` and the member `legend_done_` are removed. The column reference moves to `docs/trace-schema-v1.md` section 8.1.
- JSON `iter` core keys `stationarity`, `feasibility`, `complementarity` print the tested values; new trailing core keys after `factorizations`: `stationarity_caller`, `feasibility_caller`, `complementarity_caller` (the former values). Freeze-table row added (pre-tag meaning change + trailing keys).
- `ipm.phase.exit`, `solve.end` and the footer are unchanged.

## 3. Legs and criteria

All under `box-lane --`, `MKL_NUM_THREADS=1`; wall-clock not asserted. Flags as `legs/build.sh` (clang, SAFER_FAST, bench + examples ON); scripts in `legs/`.

| leg | instrument | criterion |
|---|---|---|
| L1 | `legs/ctest.sh` Release and Debug | 100% passed; total test count recorded (m8 PF-7 gate: 3957, +new tests) |
| L2 | `legs/replay.sh` on the trs1 binary, compared to the pinned base replay `docs/notes/data/2026-11-m8-pf7/gate/replay/B-*.csv` (the m8 code of `0cecdc6d0`; PF-7's tree equals it in every `src/include` file save `get_core_count.cpp`, which no solver counter reads) with `scripts/compare_replay.py` | 27 cells x walk, ssn, ipm; every non-wall column exactly equal ("differences: 0") in each of the three: counters, statuses AND residuals (the change is print-only) |
| L3 | golden diff review | every changed human golden and v1 golden line is one of: header text, legend removal, or a tested-vs-caller value change on a SCALED run; recorded in `gate/golden-diff.md`. Unscaled goldens change in header/legend only |
| L4 | `legs/install_smoke.sh` | smoke + export contract rc 0 on both front ends |
| L5 | mutations M1-M6 (section 4) | each killed by the named test; control passes |
| CI | `gh workflow run ci.yml --ref trs1`, one watch | all four lanes green |

## 4. New tests and mutations

New tests (the "printed == tested" assertions, on a SCALED solve of each engine): over the whole history of a solve that ends Optimal, the final row's three `tested_*` values satisfy the engine's tolerance test (IPM strictly, SQP non-strictly), NO earlier non-restoration row does (the solve continued because the tested value said so), and the caller values differ from the tested ones by the objective factor (a printed `_caller` stationarity far below `optimality_tol` while the tested one is not). A JSON/console test asserts the printed `stat_scl` and the JSON `stationarity` equal `tested_stationarity` (not the caller field) with distinct values.

| id | mutation | killed by |
|---|---|---|
| M1 | IPM `tested_stationarity = kkt_inf / scale` | IPM printed == tested test |
| M2 | IPM `tested_complementarity = barr_inf / scale` | same |
| M3 | SQP `tested_stationarity` taken from the caller-mapped value | SQP printed == tested test |
| M4 | SQP `tested_feasibility` taken from `mj.caller_row.feasibility` | same |
| M5 | JSON `stationarity` written from the caller field | JSON key test + goldens |
| M6 | console `stat` column from the caller field | console test + goldens |
