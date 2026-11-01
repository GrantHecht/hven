# M7 CLOSE GATE -- successor declaration r3 (committed before any r3 leg; never edited after)

Pin `d4b8e84b0283c83e89afa9959544c25577d80519`, unchanged. The r1 (`DECLARATION.md`, `0de4ae7f`) and r2 (`DECLARATION-r2.md`, `b9ccfaa6`) declarations stay frozen and untouched.
Record of the failed r2 chain: `bd7791bb` (legs 1-6 PASS; the 57-cell census FAILED with 20 undeclared movers; `gates/`, `README.md`).
The settler's ruling C179 attributed every one of the 20 movers to deliberate M7 walk-engine commits (diagnosis, solo legs, `MKL_NUM_THREADS=1`); the owner approved closing through r3 (ruling C180).

## What r3 is
A successor declaration: the 57-cell census re-run in full against a declared mover set that now includes the 20 cells, plus the checker fix, two falsifier sets, and one informational solo characterization. Nothing of the compiled tree changes.

### 1. Legs that stand (not re-run)
r1 legs 1-5 (Release+Debug builds, both suites, install smoke + export contract, golden rig, 27-cell replay) and r2 leg 6 (suite v1 end-to-end) stand: the pin, the builds and the tree are identical.
`preflight_r3.sh` asserts it mechanically before any leg: (a) the frozen r1 pre-flight in run mode (HEAD is a docs-only descendant of the pin with `src include tests bench cmake CMakeLists.txt scripts CMakePresets.json dep .gitmodules` identical; tree clean; derivation reproducible; tools; box; seams; baselines; harness pin; `run_leg.sh` proofs); (b) every one of the 14 earlier chain steps logged `STEP_OK` in the record's `gates/chain.txt`; (c) the file/size/mtime manifests of `build-release`, `build-debug`, `build-3seam` and the census corpus binary's sha256 equal the values measured immediately before this declaration (`env.sh`: `DECL_*`); (d) the corpus binary's own stamp is `0de4ae7f19ed`, clean. Each check has a falsifier it must refuse.

### 2. The census, re-run in full
Same baseline of record (`bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv`), same runner (`scripts/run_walk_census.sh`, parallel-tiered v1 at its defaults: T1 6-wide pinned / T2 solo / T3 5-wide full-budget, one process per pinned physical core, `MKL_NUM_THREADS=1`), same corpus binary (the one r1 built; sha256 asserted), same co-run protocol as declared in r1 (`DECLARATION.md`, "Protocol and non-claims"); wall-clock informational. Expected tiering 44 / 3 / 10. Output: `.scratch/m7-close/r3/census` (copied into `gates-r3/` at the end).

### 3. The declared mover set (26 cells)
* The r2 set: the 6 cells and 10 (cell, column) coordinates of r1's `derivation/expected-census-movers.txt` (introduced by E4; unchanged).
* The 20 path-window cells of `expected-census-movers-r3.txt`, each with its responsible commit(s) and reason, citing ruling C179. Attribution, as measured solo by the settler (columns are factorizations / qp_minors):
  * E2 `b5325623` (`independent_working_set` default true + the dependency detector): first mover for `n2000_path_physics`.
  * E2 `67a3fde6`: `n800_path_neutral`, `n750`, `n825`.
  * E2 fix1b `bf37c305`: `n1000_path_corrupted` (2976 -> 28444 facts; QP1 hits the minor cap and survives through the QP failure budget).
  * E4 `25e11280` (proximal EQP): `n1000_path_physics`, `n1000_path_activity`, `n800_path_physics`.
  * BPS `a781c3f2`: `n800_path_corrupted`, `n2000_path_activity`.
  * M1 `3f2f6e35` (`qp_failure_budget` 1 -> 3), INFERRED (these cells take more than 600 s) together with E2/E4: `n2000_path_neutral` and `_corrupted` (NumericalError -> MaxIter), `n5000_path_neutral` (-> dnf_budget), `n5000_path_warm` (-> dnf_setup), `n5000_path_corrupted` (dnf_budget -> dnf_setup).
  * `n10000_path_physics` Optimal -> dnf_budget: per-minor cost up 1.35-1.92x on minor-heavy cells (E2 detector/screen, E4 polish, BPS refinement), and the baseline already used 2759 of 3600 s.
  * Not attributable within 600 s legs, declared by the M1 + E2/E4 inference (C179): `n2000_path_warm`, `n5000_path_activity`, `n5000_path_physics`. `n1000_path_neutral` is not named in the attribution handed to this declaration; it is declared by the same lineage inference, and the file says so ("not individually attributed").
* **The prediction is the r2 measured row** (`gates/census/walk_census.csv`):
  * T1/T2 movers (the 19 non-T3 path cells and the 6 bound cells): every one of the 13 asserted columns must reproduce the r2 row EXACTLY; the residual class (`kkt_residual`) passes the calibrated gate: byte equality, else `|a-b| <= max(1e-5 * max(|a|,|b|), 1e-13)`, non-finite never passing (the comparator's own `residual_columns_agree`, imported). The six r1-declared cells must in addition differ from the baseline ONLY in their r1-declared coordinates.
  * The wall-dependent T3 mover `f7_n5000_path_corrupted` (baseline `dnf_budget`, 3600 s phases, 5-wide): the STATUS CLASS is asserted (`dnf_*`; counters are -1 on every dnf row).
  * Every non-mover (31 cells) must still match the U0 baseline: counters/statuses exact, `kkt_residual` through the gate.
  * Roster 57 / 57, no MISSING/EXTRA; the 20 declared tiers must equal the tiers derived from the baseline (T3 iff baseline `dnf_*`; T2 iff wall >= 1800 s or the physics cell; else T1).
  * A residual value in the T2 `dnf` rows etc. is `-1`; they are compared as strings like every other column.
* A failure of any of these is NOT patched: the chain stops and reports.

### 4. The checker fix (a declared runner change)
`close_check.py check_census_gated` (r1, frozen, imported not edited) read the comparator's text and listed `f7_n1000_bound_neutral kkt_residual` as undeclared although the residual moved 6.295832335e-14 -> 2.291628627e-16, inside the 1e-13 absolute floor (the cell is a MISMATCH only through its declared `qp_minors`). `wrappers-close-r3/close_check_r3.py census-r3` replaces it for the verdict: it reads the three CSVs (baseline, r2, fresh) and every residual comparison goes through the comparator's gate with its floor. Falsifiers (run by the pre-flight, output `gates-r3/selftest-census-r3.txt`): on the REAL r2 compare text the r1 checker still lists the artifact cell (the bug reproduced); the r3 checker accepts the real r2 census as a reproduction; and it refuses a non-mover counter/status/out-of-gate-residual move, a non-listed path cell moving, every mover column deviating from the r2 row (counter, minors, status, residual outside the gate), `n10000_path_physics` back to Optimal, a T3 mover with a terminating status, an r1-declared cell moving outside its declared coordinates, a missing and an extra cell, a removed mover and a wrong tier class; it accepts a non-mover residual inside the floor, a mover residual inside the gate, and a T3 `dnf_budget`.

### 5. Falsifiers for r2's two guard-replacement assertions
`close_check_r3.py falsify-r2-guards` extracts the code of the assertions FROM the frozen `wrappers-close-r2/suitev1_r2.sh` (the python heredoc, run in a sandbox whose `set_pin` module serves the two JSON documents; and the three shell statements of the unittest assertion), then runs them on the real inputs (the committed `sets/hs-27.json` against a copy with `hven_pin_sha` replaced; the real `r2-unittest.log`) -- must accept -- and on mutated inputs, each of which must refuse: a second differing field; a missing problem row; a regenerated pin that is not `d4b8e84b`; a committed pin that is not `3904a477`; no difference at all; a third unittest FAIL; an ERROR; one of the two stale-pin failures missing; both missing; a different test failing in place of one. Output `gates-r3/selftest-r2-guards.txt`, run by the pre-flight.

### 6. The characterization leg (informational; NOT a pass/fail criterion)
`f7_n10000_path_physics` run SOLO, `taskset -c 0`, `MKL_NUM_THREADS=1`, under the box lock with the in-lock process check, with the corpus runner's hidden per-phase wall-budget override `--internal-force-wall-budget-seconds 7200` (setup and solve phases each 7200 s; the runner stamps a `forced_test_budgets` WARNING into the CSV, which marks the row as NOT a committed-budget row and keeps it out of any baseline). Recorded: whether it converges (status), `factorizations`, `qp_minors`, wall. Declared as a CLAUDE.md section 7 solo characterization: not asserted, not compared with a serial timing, not a baseline row. It is the only leg beyond the usual 600 s cell rule, and only at the census's own solo-tier terms (alone on the machine, run after the census completes). It runs only if the census verdict is green.

## Order and stop rule
`chain_r3.sh`: pre-flight (r3 + frozen r1, with all falsifiers) -> `census` -> `census-check` -> `char`. Every leg through the frozen `run_leg.sh` (`flock /tmp/box-build.lock`, in-lock process check -> `PGREP_EMPTY`, `FOREGROUND_START/END`, `MKL_NUM_THREADS=1`, `pipefail`, no `timeout`). The chain stops at the first failing step; a failure is reported, never patched.

## On green: the new census baseline
A record commit plus a NEW directory `bench/baselines/2026-10-m7-close-corpus/` holding the r3 census rows in the U0 baseline's format with a README: the M7 walk census baseline against which M8's census compares. It is a declared event of this gate; the U0 baseline (`2026-08-16-u0-corpus`) is not edited. Its T3 row (`f7_n5000_path_corrupted`, wall-dependent) and the T2 dnf rows are recorded as measured and flagged wall-dependent in its README.

## Non-claims
Apple/Accelerate and Windows values: UNOBSERVED. Wall-clock informational. Mechanism attributions are the settler's (C179); this gate asserts reproduction, not mechanism.
