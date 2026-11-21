# DECLARATION-PF-2b -- per-component backend solve counters (step 1), then the PF-2 re-measure

Written and committed before any gate or measurement leg; never edited afterwards (a failed or void leg is answered by a
`-r1` file, this one kept). Criteria are never changed after data. Brief: the PF-2b task (RESULTS-PF-12's open item 1: the
bundle that owns the PF-2 per-minor excess -- E2 detector/screen, E4 in-EQP polish, BPS bordered refinement -- is split by
nothing; there are no separate toggles and dwarf call graphs fail on this box).

## 1. What is built (measurement only; no behaviour change by construction)

- `hven::linear::SolveTally` + `SolveContextScope` (`include/hven/linear/symmetric_factor.h`, `src/linear/analysis_tally.cpp`;
  no new source file, so the `src/CMakeLists.txt` count pin is untouched). The adapters record every completed backend
  solve (MKL and Accelerate `run_solve` both overloads, `solve_partial`, the dense arm's `run_dense_solve` both overloads)
  as one call, its RHS columns and the refinement steps the backend reports (Pardiso `iparm[6]`; dense and Accelerate: 0)
  into every open tally on the thread, under the context label open at that moment. With no tally open `record` loops over
  nothing. Four contexts partition every solve: `kOrdinary` (no scope open), `kDetector` (every solve issued inside
  `ws_independence.cpp`: dependency tests, entry detector, certificate coefficients, face probes, rank repair),
  `kPolish` (`polish_eqp_core`, `polish_bordered_unregularized`), `kBorderRefine` (`refine_bordered_solve_iterated`: the
  mandatory first step and the sweeps after it, including the verdict-site replay's call). `kOrdinary` therefore holds the
  EQP's first solves and EQP-site residual loops, the eliminated path's model refinement, the verdict replays' first solve,
  and the SSN/IPQP/restoration engines.
- Twelve counters appended to `SqpCounters` after `backend_symbolic_repeats` (`backend_solves_*`, `backend_solve_rhs_*`,
  `backend_solve_refine_*`, each over `ordinary`, `detector`, `polish`, `border_refine`), taken by a `SolveTally` opened
  next to PF-4's `AnalysisTally` over `solve_impl_body` (so the restoration sub-solve is inside; its own tally nests).
  **Placement deviates from the brief's "QpCounters"**, on the PF-4 precedent (`backend_symbolic_analyses` is
  SqpCounters-only and the brief points at it): the tally is a boundary observation over a whole solve, summing twelve
  QpCounters through the ~13 aggregation sites in `sqp_solver.cpp` would be a large unverifiable diff, and a QP solved
  directly (not through `SqpSolver`) has no tally. The schema plumbing is the rest of PF-4's: the `HVEN_SQP_COUNTERS_FIELDS`
  X-macro (which generates the trace keys and `dump_counters`), arity pins, goldens, trace-schema and migration docs.
- A declared SCHEMA EVENT: twelve keys on the `sqp.solve.end` counters object (both v1 golden lines and the two
  counters-object goldens re-derived once, the nested tables' distinct values up by twelve), `kSqpCountersFieldCount`
  108 -> 120, `sizeof(SqpCounters)` 1384 -> 1480, and twelve `side.*` lines on every hven-sqp lane row.
- Bench: `CorpusRow::solve_tally` and a `.tally` SIDECAR written by `--internal-run-one` when `HVEN_BENCH_SOLVE_TALLY` is set
  (one header line and one line of twelve integers). The 76-column corpus CSV is NOT changed: no new column, so every
  frozen baseline and replay row keeps its schema.
- Refinement steps inside a call (Pardiso's `iparm[6]`, 2 per solve at the defaults on these cells) are the backend's own
  sweeps (the `mvout` matvec of RESULTS-PF-12); a "solve" below is a backend call, and the instruction ratio, not the call
  count, is the cost.

Code commits (on `origin/m8` `0ee37c2ca`): `a1bce3866` (the counters, tests, docs, bench sidecar) and `ae2e6729f` (the
fixtures: partial solve, per-algebra buckets, refinement steps). Code paths `src include tests bench examples bindings
CMakeLists.txt cmake scripts` of the leg HEAD must equal `ae2e6729f` (checked by the leg runner).

## 2. Arms and binaries (frozen at hand-off to the first leg; shas appended in `BINARIES.txt` before it)

- A = `0ee37c2ca` (`git archive`, tree `.scratch/armA`); B = `ae2e6729f` (worktree, `.scratch/build-pf2b/rel`, Debug in `dbg`).
  clang 22.1.8, `-DHVEN_FP_MODE=SAFER_FAST -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON`, Linux x86-64, AMD Ryzen 7 5800X3D,
  MKL 2026.2, `MKL_NUM_THREADS=1`. Apple/Accelerate and Windows: CI only; the Accelerate arm's hooks record 0 refinement
  steps by construction and are compile-checked on the macOS lane (UNOBSERVED here).
- TRAIN lanes: private hven-bench clone at `e03cbad` pinned to A and to B (`legs/lane_build.sh`, `NEVER=1`), as PF-4.
- Measurement binaries (section 4): U0 = M6 `19f7674f6` (`.scratch/build-u0`, sha256 32a67ecc... as RESULTS-PF-12), and
  `measB` = `git archive ae2e6729f` + `scripts/pf12-lever.patch` (the env levers `HVEN_PF12_QP_FAILURE_BUDGET`,
  `HVEN_PF12_IWS`; scratch-only, never committed), built by RESULTS-PF-12's `build.sh`.

## 3. Gate for the counters (legs in `legs/`, evidence in `gate/`)

| leg | instrument | criterion |
|---|---|---|
| P1 | `ctest.sh` rel and dbg (box-lane, `ctest -j4`) | each log reads "100% tests passed, 0 tests failed out of N" (N = 4042, the Release dev run; Debug must read the same N) |
| P2 | `replay.sh` x3 (27 cells x walk/ssn/ipm) of A and of B, co-run (A and B each one process pinned to its own physical core of the lane, `MKL_NUM_THREADS=1`; counters, statuses and residuals only) | every B run vs A run: `scripts/compare_replay.py` exact, **0 differences in all three modes, all three repetitions** (no new column exists in the CSV, so there is no excepted column) |
| P3 | `train_enum.sh` x2 (238 x D/K, stat mode, CAP 300 s, co-run: A and B on two pinned physical cores of one lane, `MKL_NUM_THREADS=1`) + `train_check.py` | every B row equals its A row on every key except the twelve new `side.backend_solve*` keys (residual-class keys byte-equal or inside the calibrated gate, as PF-4's `compare()`), no row missing or capped: `SUMMARY ... movers vs arm A 0`; the instruction totals B/A are reported (`instr_compare.py`), the tally adds a few instructions per solve; asserted `TOTAL hven-sqp B/A <= 1.005` |
| P4 | `mutations.sh` | control clean; every MUST mutation KILLED (`MUTATIONS_AS_DECLARED`); tree restored |
| P5 | `install_smoke.sh` (default and clang front ends) | smoke and export contract rc 0 |
| CI | one dispatch of the four-lane workflow at the pushed head | green on all four lanes |

Mutations (`legs/mutate.py`): M1 solves never counted, M2 columns never counted, M3 refinement steps never counted, M4
only the innermost tally counts, M5 a scope never restores; S1 detector scope removed (all six sites), S2 eliminated polish
unlabelled, S3 bordered polish unlabelled, S4 bordered refinement unlabelled; A1 MKL multi-RHS solve unrecorded, A2 MKL
single solve unrecorded, A3 MKL partial solve unrecorded, A4 dense multi-RHS unrecorded, A5 dense single unrecorded;
C1..C12 each of the twelve counters' assignment into the result removed. Killers: `SolveTally.*` (linear) and
`SqpBackendSolveTally.*` (sqp).

## 4. The PF-2 re-measure (RESULTS-PF-12's instruments and cells, re-taken)

Same cells (tier A: `f7_n1000_path_corrupted`, `f7_n1000_path_neutral`, `f7_n800_path_neutral`,
`f7_n825_path_neutral_control`, `f7_n750_path_neutral_control`; tier B context: `f7_n800_path_corrupted`,
`f7_n2000_path_activity`, `f7_n2000_path_physics`), same instrument (`perf stat -e instructions:u,branches:u,cycles:u,
branch-misses:u,L1-icache-load-misses:u`, one process per cell `--internal-run-one --engine walk`, untimed warm-up, three
rounds, median), same co-run terms (four workers pinned to four distinct physical cores of one box lane, SMT siblings
idle, `MKL_NUM_THREADS=OMP_NUM_THREADS=1`; counters and instructions only, nothing quotable as timing). Differences from
RESULTS-PF-12, all declared: the new arm is `measB` (the post-PF-4 + I18 tree + tally), run with
`HVEN_PF12_QP_FAILURE_BUDGET=1` like the U0 arm; a third arm `off` = the same binary with `HVEN_PF12_IWS=0`
(`independent_working_set` off: the U0-equivalent work, which has no detector, no certificate, no accuracy stop and
issues every solve in the ordinary bucket -- the nearest reproduction of the U0 solve mix the tally can take; the U0
binary itself has no tally); arms rotate order across rounds. The tally sidecars are written by `new` and `off`.
Deliverables: per-minor instruction ratio vs U0 per cell (new, off); solve calls, RHS columns and refinement steps per
component per minor, `off` vs `new`; the excess carried by each component. Determinism STOP: per arm and cell the three
rounds must agree on status/factorizations/qp_minors and on all twelve tally values, else the cell is not an instruction
measurement of a fixed computation. U0 rows must equal the U0 baseline row on those columns (C179). The `tally` covers
the designated solve only (a corrupted-start cell's setup hop is outside it; its instructions are inside the numerator,
as in RESULTS-PF-12).

## 5. Not run

HOLDOUT (never). The census. Any wall-clock claim. Remedies (the settler decides; section 4's table is the input to the
recommendation). `f7_n10000_path_physics`. Accelerate and Windows readings of the counters.
