# Which call site each trace cell exercises

The counter-identity record for the eleven retargeted call sites rests on the
solve traces. Seven of the eleven are unreachable from the default settings the
two original cells use, so five cells were added to reach them. This is the
measured attribution, not an argument: the seven sites were temporarily
instrumented with a stderr probe, a throwaway build was run cell by cell, and
the probes were then removed. The counts are hits per cell.

| site | call | cells that reach it |
|---|---|---|
| 3 | `InteriorPointSolver::eval_kkt_no` (row 7) | `hs071_optno` (10), `infeasible_restoration` (69) |
| 5 | `build_restoration_exit_measures` (row 1) | `infeasible_restoration` (2) |
| 6 | `alg_impl` restoration-exit `obj_val_` (row 1) | `infeasible_restoration` (1) |
| 8 | `ClassicMeritAcceptance::eval_rhs` (row 4) | `hs071_lang` (10) |
| 9 | `ClassicMeritAcceptance::eval_trial_point_occ` (row 2) | `hs071` + `rosenbrock` (33), `maratos_soc` (9) |
| 10 | `modern_eval_trial_point` (row 2) | `hs071_filter` (9), `infeasible_restoration` (73) |
| 11 | `SocRecovery::eval_trial_constraints` (row 2) | `maratos_soc` (3) |

Sites 1, 2, 4 and 7 are on the default optimality path and were already
exercised by the two original cells.

Every cell is configured from public settings — `set_opt_ls_mode`,
`settings().acceptance_strategy_`, `settings().barrier_governor_`,
`settings().max_soc_`, `settings().soe_mode_`,
`settings().restoration_mode_` — so nothing here depends on test-only access.

Two problems were written for the cells that needed a specific solver path
rather than a specific answer: the Maratos example, whose full Newton step
increases the constraint violation so the line search rejects it and the
second-order correction runs; and a pair of conflicting linear inequalities
(`x0 >= 1` and `x0 <= -1`), whose least-infeasible point admits no further
reduction, so feasibility restoration converges there and the solve takes the
locally-infeasible exit.
