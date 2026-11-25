# M8 I17 r2 -- gate results (DECLARATION-I17-r2.md; chain `orchestrate-r2.out`, outputs under `gate-r2/`)

Chain run 2026-11-20 02:24-03:32 on the declared code commit `0e5eaa8aeda30fdab5f378eb22ca8c85839d5df4` (leg HEAD `15a04dc61`, base `afc9744bf`), every leg passed, `ALL_LEGS_PASSED`; no successor round was needed.

| leg | result |
|---|---|
| P0 | code paths == code commit, clean; base worktree == `afc9744bf`; four sha256 match; three build trees up to date |
| P1 | Release and Debug: "100% tests passed, 0 tests failed out of 3954" (`gate-r2/suites/`) |
| P2 | replay, 27 cells x 75 columns, walk / ssn / ipm, A vs B: 0 differences in each (`gate-r2/replay/cmp-*.txt`) |
| P3 | TRAIN 476 rows x 2 arms: `I17_TRAIN_PASS`; 40 movers, all LEDGER class (declared `movers-train-r2.txt`), 0 OTHER, 0 missing or capped (`gate-r2/train/train_check.txt`) |
| P4 | Always-forced TRAIN (119 hven-sqp cells): `I17_ALWAYS_PASS`; 17 ledger-only movers (declared), Auto unmoved at every cap, smallest unmoved Always cap 2^10 = the adopted default (`gate-r2/always/always_gate.txt`) |
| P5 | mutations: control clean, 26 of 26 killed (r1's 17 re-anchored + I1a I1b I2 I4a I4b I5a I5b I5c I6; `gate-r2/mutation/*.killers.txt`; per-mutation diffs regenerate with `legs/mutate_i17_r2.py <id>`) |
| P6 | install smoke and export contract rc 0, default and clang++ (`gate-r2/install-smoke/`) |

The TRAIN and Always cell directories are packed as `gate-r2/cells-*.tar.xz`; the mutation build logs and diffs are not kept (as in r1).

## The restoration limit (ruling 4)

`CallerStepLimitSolve.RestorationStepsStayWithinTheLimitInCallerUnits` (an infeasible row, column scales 1e4 : 1, r = 0.05): with the limit applied in restoration the largest
caller-unit excess over the limit along the whole path, restoration's rows included, is <= 0 under kOff and kAlways. Without it (mutation I4a) the excess is 0.035 (kOff) and 6.35
(kAlways); applied in engine units without the parent's column factors (I4b), 3.15 (kAlways) (`dev-r2/restoration-limit-excess.txt`; both mutations killed in P5). The synthetic
issue-17 model with r = 0.05 under kAlways converges in 32 majors and never enters restoration.

CI: see the hand-back record (run on the pushed head).
