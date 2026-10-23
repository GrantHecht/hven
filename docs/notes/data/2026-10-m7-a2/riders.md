# A2 rider round (review `m7-a2-review-fable.md`, ruling C130)

One commit on top of `e626e6ea`. Riders taken: MEDIUM-1, MEDIUM-2 (PAGE-LITERAL), LOW-5, NIT-6, NIT-7, NIT-8.
LOW-3 and LOW-4 are registered, not changed here.

| rider | change |
|---|---|
| MEDIUM-1 | `tests/sqp/test_ipqp_mu0_rule.cpp`, `AFallbackLeavesTheLadderMemoryAsOffFindsIt`: fifth fixture `scalar_box_qp(-50, 1)`, box `[-0.01, 0.01]`, `g *= 1e4` (fallbacks 1, off `ipqp_rho_demanded_max` 0, on == off). README G5 row corrected: the restore is load-bearing, M7 now killed. |
| MEDIUM-2 | `src/qp/ipqp_engine.cpp` (estimate acceptance): accepted only when the reading is `InertiaRead::kOk`; `kUnreadable` falls back to kFixed. `evidence_failed` is not restored (never-disarm contract); the save/restore lines are removed. Test through the inertia seam: `IpqpSeamTest.AnUnreadableMu0EstimateFallsBackAndTheEvidenceFlagStaysArmed` (`tests/sqp/test_ipqp_seams.cpp`). |
| LOW-5 | `IpqpMu0Rule.TheEstimateIsChargedToTheFactorizationBudget`: at `ipqp_max_factorizations = 1` under kComplementarity the code gives `ipqp_factorizations == 1`, `escape_reason == kBudget`, `ipqp_iters == 0` (the reviewer's prediction). Header line added to `ipqp_mu0_rule`. |
| NIT-6 | the two A2 constants in `ipqp_engine.h` moved after `kIpqpRepairSlackEps`. |
| NIT-7 | `ipqp_mu0_rule` comment in `sqp_solver_types.h` shortened to the terse form with the design-page pointer. |
| NIT-8 | `logs/fam-*.log` removed (`git rm`); the family instrument writes only the CSV (README says so). |

## Mutants (`out/riders/mutants.txt`; each applied alone, rebuilt in Release, targeted test run, reverted)

| mutant | test | result |
|---|---|---|
| M7 ladder high-water mark not restored | `IpqpMu0Rule*` | KILLED (`AFallbackLeavesTheLadderMemoryAsOffFindsIt`) |
| M12a estimate accepts `kUnreadable` | `hven_ipqp_seam_tests` `*Mu0*` | KILLED |
| M12b `evidence_failed` restored after the estimate | `hven_ipqp_seam_tests` `*Mu0*` | KILLED |

A first M12a attempt SURVIVED because the seam fixture (`convex_qp`) falls back anyway (O(1) prices exceed the band ceiling), so accepting
the unreadable reading was invisible; the seam test was changed to a scalar box (`g = 1e-3`) that DERIVES on a clean run, which
kills M12a. The surviving first attempt is a record of the test fixing, not a result.

## Re-measurement at the riders tree (`out/riders/chain.txt`)

* Suites: Release **3222/3222**, Debug **3221/3221** (long test excluded) + the long test alone **1/1** (previous 3220/3219: +2 = the budget pin and the seam test).
* G1 U0 exact vs `docs/notes/data/2026-10-m7-scw-gates/` (walk / ssn / ipm): **0 / 0 / 0** differences (27 cells x 75 columns each).
* G2 E1 OFF: **29 x 65, 0 differences** vs `a4-gate-mu1e-2.csv` (`wall_s` excluded; the three new counters are 0 on every row), and 0 differences vs the committed `out/meas/e1-off.csv` (68 columns).
* Every leg ran with `PGREP_EMPTY`, under `flock /tmp/box-build.lock`, `MKL_NUM_THREADS=1`.

## Why TRAIN is not re-run

The riders change ON-path behaviour only on a `kUnreadable` estimate reading (MKL never produces one outside the test seam), plus tests,
comments and docs. The OFF path is untouched (E1 OFF identity and U0 above). The TRAIN measurements and decisions stand.
