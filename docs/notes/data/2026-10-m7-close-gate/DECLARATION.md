# M7 CLOSE GATE -- the declaration (committed before the first leg; never edited after)

Gate range: code pin `d4b8e84b0283c83e89afa9959544c25577d80519` (branch `m7`, the WX2 gated head plus the WX2 record).
This declaration commit is DOCS-ONLY on top of it (`wrappers-close/preflight.sh` re-asserts that mechanically, and that
`src include tests bench cmake CMakeLists.txt scripts CMakePresets.json dep` are identical to the pin). Worktree
`.claude/worktrees/m7-close-gate`, branch `m7-close-gate`, scratch `.scratch/m7-close/` (git-ignored). Recipe: the M6 close gate
(`docs/notes/data/2026-09-m6-close-gate/`) re-derived, not copied: every path, list, count and baseline below is DERIVED by
`wrappers-close/close_check.py derive` from git and the frozen records (output committed in `derivation/`, re-derived and diffed by the pre-flight).

## Legs, in order (chain.sh; every leg through run_leg.sh = `flock /tmp/box-build.lock`, PGREP_EMPTY inside, FOREGROUND markers, `MKL_NUM_THREADS=1`, ctest `-j4`)

| # | leg | declared pass criterion |
|---|---|---|
| 1 | Release and Debug builds from EMPTY via the `linux-clang-{release,debug}` presets (binaryDir redirected to scratch; tests ON, bench ON in Release) | configure prints `-> REFERENCE (route pins asserted)` exactly once per tree, never OFF-REFERENCE, align 32; build rc 0 |
| 2 | Release suite (whole), Debug suite (long test excluded), the Debug long test alone | 0 failed. Started-name multisets EQUAL the declared ones (derivation: the WX2 record's suites, names normalised for the address-bearing parameter text); git proves the range adds/removes no TEST line (`facts.json`). Declared: Debug 3233 started / 3231 ran, skipped 7, disabled 2, inventory 3234 (incl. the long test); Release 3234 started / 3232 ran, skipped 1, disabled 2, inventory 3234; long test 1/1 |
| 3 | install smoke + export contract (`scripts/check_install_smoke.sh`, `check_export_contract.sh <targets> linux <align read from the smoke's own configure log>`) | smoke PASSED, both solvers through one generic consumer; export `OK ... align=32` |
| 4 | golden rig, three seams (tycho @ consumed-tree pin, tycho_sqp @ phase-7-close) | exactly ONE failure, `Arms/InteriorPointTrace.P5_InertiaBeforeFactorizationIsAnExplicitState/sqp-old@mkl`, in both the rig-scoped run (inventory 84) and the whole run; the 3 `FailByDesignControl` tests pass; whole inventory = Release inventory + 35 (the M6 delta 2628-2593, derived); both seam pin lines printed by the rig's own configure |
| 5 | 27-cell replay, three engines, `taskset -c 2` | the gated (1e-5, absolute floor 1e-13) comparison against the WX2 record has 0 differences (counters/statuses exact, residual-class columns through the calibrated gate); the EXACT comparison against the WX2 record is REPORTED (WX2 asserted 0/0/0); the gated comparison against the M6 pin yields a difference SET equal to the declared 43-set (walk 17 / ssn 13 / ipm 13) by (engine, cell, column) |
| 6 | suite v1 end-to-end (hven-bench @ 451b51b, hven pin = `d4b8e84b`, 4 lanes: hven-ipm, hven-sqp, ipopt, snopt; sets hs (ceiling 600 s) and cutest (ceiling 300 s), TRAIN only, options `default`, iteration budget the lane default) | per set: rows == TRAIN problems (split pin) x 4 lanes, each exactly once; every row split TRAIN, `hessian_provenance` non-empty; provenance block carries schema v1, harness.sha, hven.pin.sha == the pin, split.policy, mkl 1, serial, Apple/Windows UNOBSERVED; runner rc 0, or 1 only as a `REFERENCE FAILURE` with all rows written; no HOLDOUT cell run (the runner refuses `--split HOLDOUT`, this chain never passes `--split`) |
| 7 | 57-cell walk census (`scripts/run_walk_census.sh`, parallel-tiered at its defaults: T1 6-wide pinned, T2 solo, T3 5-wide full budget) | see below |

## Census baseline, and the declared moves since it

Baseline of record: `bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv`. The most recent census run on the tree is the M6 close gate's
(`2026-09-m6-close-gate/census/walk_census.csv`, 57/57/0 against it); the pre-flight proves that equivalence with the byte-exact comparator and proves no later
census record exists (the toolchain re-baseline and the later gate records carry replays and E1 arms, not censuses). The runner's own comparison (byte-exact, 13 asserted
columns) is REPORTED. The VERDICT is the comparator run with `--residual-gate 1e-5` (the residual class `kkt_residual` through the calibrated near-ulp gate; counters and
statuses exact), in which every mismatching cell may differ only in a DECLARED (cell, column) coordinate and every other cell must match. The declared coordinates
(`derivation/expected-census-movers.txt`, 10 coordinates over 6 cells) are the walk-engine entries of the 43-set that name a census cell and one of the 13 asserted columns:

* `qp_minors` 2 -> 3 on `f7_n{1000,2000,5000,10000,20000}_bound_neutral` -- reason: introduced by M7 W2 E4 (the proximal EQP with the unregularized polish inside every EQP solve and the two-stage terminal polish; code on m7 `25e11280`, `0d519a2b`, `5756530e`, `e212a308`); the E4 ledger entry (`docs/notes/2026-09-m7-ledger.md`, W2 E4) and its gates record declared the 43-set (5 `qp_minors` 2 -> 3, 37 residual columns strictly better, 1 worse) and every later gates record (A2, A3, toolchain re-baseline, WX, WX2) reproduced it value for value.
* `kkt_residual` on `f7_n{2000,5000,10000,20000}_bound_neutral` and `f7_n20000_bound_corrupted` -- reason: the same E4 change (E4's ledger: "37 residual columns strictly better") moves the converged residual from ~1e-12/1e-13 to ~1e-16 (strictly better, values in the WX2 record's `replay/comparison.txt`), a larger change than the 1e-13 absolute floor can absorb, so the gated comparison still reports it; declared by coordinate, never by value.

The mechanism is the E4 record's, not re-derived by this gate. A mismatch anywhere else (including a different column on a declared cell) is an UNDECLARED mover: it fails the leg and is reported as a candidate, to be re-run ALONE before it is called a regression (CLAUDE.md section 7). Tiers are assigned from the frozen baseline; wall_s is excluded; the co-run protocol is the runner's, as the M6 close ran it (T1 44 / T2 3 / T3 10 expected in the provenance table).

## Protocol and non-claims

* Wall-clock is informational everywhere; counters, statuses and test counts are the asserted currency. The census T1/T3 tiers co-run under the runner's declared protocol (one process per pinned physical core, `MKL_NUM_THREADS=1`); the replay is solo, pinned `taskset -c 2`; the suite v1 cells run serially, one at a time.
* Apple/Accelerate and Windows values: UNOBSERVED (the box is Linux). Nothing is inferred.
* The chain STOPS at the first failing leg/assertion; a fix is a successor `-r2` declaration.
* Declared deviations from the brief's letter: (a) the in-lock box check matches process NAMES (`pgrep -x`: `hven_*`, ctest, ninja, cmake, ipopt, snopt), not command lines -- an idle shell whose argv merely mentions `ctest` (observed: another lane's waiter loop, pid 587322) is not a process; (b) `/home/ghecht/Projects/hven-bench/.git/info/exclude` gained the line `.claude/` (an untracked `.claude/worktrees` directory made the harness's own dirty-tree trap refuse every run; no tracked file or the harness commit changed); (c) the E1 OFF arm is not part of this gate's legs (it is not in the brief's list; WX2 ran it).
