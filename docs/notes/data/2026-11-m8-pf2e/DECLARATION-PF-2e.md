# DECLARATION-PF-2e -- the contraction probe on the QP independence detector (re-read and test refinement), then TRAIN and the PF-2 re-measure

Written and committed before any gate or measurement leg; never edited afterwards (a failed or void leg is answered by a `-r1`
file, this one kept). Criteria are never changed after data. Base: pf2d ship head `84a6e3373` (code `21ae792ce`, L1). Code under test: `334085b67` (rule) + `13d4c541d` (tests) on branch `pf2e`;
only builds, dev unit-test runs and the scratch diagnostic preceded this file. Evidence
base for the rule: a scratch event log (`diag/`, patch `diag/diag-pf2e.patch`, never committed to the library) over the qp and
qp-seam suites, the 27-cell replay (walk) and the eight f7 cells of PF-2b/2d; findings in section 1.

## 1. What separates "refinement/re-read needed" from "decided" (diagnosis; measured on the scratch log)

- rho alone does not: certified flips occur at test rho up to 9.6e-4 (k = 0 re-reads; 420 flips vs 16 037 non-flips with
  overlapping rho), and the first-solve rho1 of tests whose verdict the refinement changes reaches 5.4 (BRITGAS qp0).
- Neither do the zero-solve features tried (k = 0 re-reads): the cancellation level max_k |v_k| ||a_k|| / ||a|| (flips 0.99 ..
  2.4e3, non-flips 0.04 .. 570), ||v||_1, the mu-free residual of the converged solve. A dependent candidate needs no
  cancellation (a = a_k reads 1), so none of these is a mechanism.
- The REFINEMENT'S CONTRACTION does. A rho that is regularization error contracts geometrically under the mu-free
  refinement; a rho that is distance stays put. ACOPR57's restoration QP (the L2 counter-example): every one of its 68 flips
  contracts by q = rho_{j+1}/rho_j = 0.001 .. 0.08 per step (rho 4.5e-4 -> 5e-10 in three steps); its 4 non-flips read q >= 1.1.
  Ping-pong (the L3 counter-example): rho1 0.064 -> 1.1e-3 -> 4.6e-5 -> 1.2e-6 (q 0.017 .. 0.04), stalled, UNDECIDABLE.
  BRITGAS qp0 (L3's UNOBSERVED mechanism, now observed): a test with rho1 0.0137 contracts to 1.53e-6 in one step (q 1.1e-4),
  CONVERGES, and the grey re-read certifies it dependent (rho' 1.7e-12); L3 decided it independent at the first solve, so the
  candidate entered the working set and the walk moved (the re-read that would have flipped it never ran). Over all logs, at
  every probe point where the rule below could cut, the largest q of an outcome-changing refinement is 0.105 (flips) and 0.083
  (tests); stagnant reads are q ~ 1.

## 2. What is built (branch `pf2e`; `ws_independence.{h,cpp}`, `walk_engine.cpp` test_candidate)

- **L1b (bit-identical by construction).** A converged grey test keeps its FINAL iterate (solve + the k refinement steps that
  reached it: `DepTest::has_resume`, `resume_y/res/res_norm/steps/rho_prev`; it replaces L1's first-solve copy). The re-read
  resumes there with the remaining kDepRefineSteps - k steps: the refinement is deterministic, so the old re-read's first k steps
  ARE the test's. `solves` stays logical (the reused solve and steps counted), so L1b alone moves no QpCounters value.
- **The contraction probe (kDepStagnation = 0.5).** After a kept refinement step with q = rho_new / rho_prev >= 0.5 (STAGNANT),
  with `left` steps of the budget remaining, the refinement stops when rho_new * 0.5^left cannot reach the reading that would
  change the verdict:
  - grey re-read (`grey_reread`, called by test_candidate): stop when rho_new * 0.5^left > kDepTol; the "independent" stands.
    When the test itself took k >= 1 steps its last step is the probe (no re-read solve at all);
  - non-screen test (`dependency_test`): stop when rho_new * 0.5^left >= kDepUndecidable; the test stands as a stall admission
    (independent, `stall_admit`). Screen tests and `certificate_coefficients` (the Farkas multipliers) are untouched.
  An EVIDENCE rule, not a certificate: it assumes a stagnant refinement does not turn into a fast one.
- Expected effects, declared: `dep_test_solves` falls (probe-cut steps are not counted: they did not run); `dep_stall_admits` rises
  where a cut test would have converged under full refinement (scratch log: f7 tier A 2 .. 147 per cell, seam suite 2 131 tests);
  the backend detector tally falls. No other QpCounters value, no verdict, no status and no path is expected to move (scratch
  simulation `diag/sim-stag0.5.txt`: 16 184 of 16 803 grey re-reads cut with 0 of 495 flips lost; 14 274 test cuts with 0 verdict changes;
  the ACOPR57, ping-pong and BRITGAS fixtures are inside the seam suite and also logged alone). Pin moves in tests, declared:
  `AStalledRefinementAboveTheUndecidableBandIsAdmittedAndFlagged` (solves 4 -> 2, probe_cut) and
  `AReReadResumedFromTheTestsFirstSolveEqualsAFreshReRead` (resumed backend solves fresh - 1 -> fresh - t.solves).
- New tests (`WsIndependenceProbe.*` in hven_qp_tests, `QpGreyReread.TheReReadStopsOnAStagnantProbeStep`): a stagnant k = 0 re-read
  stops after one step; a stagnant k = 1 grey needs no re-read solve; a contracting grey re-read runs to its end bit for bit the full
  re-read and certifies; a test stagnant in the grey band is not cut; a screen test is not cut; end to end through test_candidate.

## 3. Arms and binaries (shas appended to `BINARIES.txt` before the first leg; frozen)

- **L** = pf2d ship head: pf2d's frozen lane binary `pf2d/.scratch/bin/L/hven_bench_cell` (sha256 `33183386...`, pf2d BINARIES
  "lane L"), measurement binary `pf2d/.scratch/build-measL/bench/hven_sqp_corpus` (`430e71e0...`, "measL"), both re-checked.
- **E** = the pf2e code head (sha recorded in `BINARIES.txt`): lane binary by `legs/lane_build.sh` (private hven-bench clone at
  `e03cbad`, `NEVER=1`, as PF-2d), measurement binary `measE` = `git archive E` + `pf12-lever.patch` (scratch-only), Release.
- clang 22.1.8, `-DHVEN_FP_MODE=SAFER_FAST -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON`, Linux x86-64, AMD Ryzen 7 5800X3D,
  MKL 2026.2, `MKL_NUM_THREADS=1`. Apple/Accelerate and Windows: CI only.

## 4. Gate and evidence (legs in `legs/`, evidence in `gate/` and `measure/`)

| leg | instrument | criterion |
|---|---|---|
| P1 | `ctest.sh` rel and dbg at E (one box-lane lane, `ctest -j4`) | each log "100% tests passed, 0 tests failed out of N"; N equal rel/dbg, recorded in BINARIES.txt |
| P2 | `train_enum.sh` (238 x D/K, stat mode, CAP 300 s) of L and E co-run on two pinned physical cores of one lane, `MKL_NUM_THREADS=1` + `path_quality.py E L` | **0 status movers (asserted)**, 0 missing/capped (asserted). Reported per row: every path mover (any key outside `dep_test_solves`, `dep_stall_admits`, `dep_grey_rereads`, `side.backend_solve*`) with its objective-quality read (same / better / worse local solution, or no solution either side); counter-only movers counted; sqp instructions E/L over all rows and over path-stable rows. The same read of pf2d's F and G arms against L is reported beside it |
| P3 | PF-2 re-measure: RESULTS-PF-2d's instrument (`run_worker.sh`, perf stat, `--internal-run-one --engine walk`, `HVEN_PF12_QP_FAILURE_BUDGET=1`, tally sidecar), same 8 cells, three rounds, four workers on four pinned physical cores of one lane; arms **newL (control re-take)** and **newE** | determinism STOP: three rounds identical on status/factorizations/qp_minors/tally per arm and cell; CONTROL: newL equals PF-2d's newL rows on status, factorizations, qp_minors and all twelve tally values, and its instructions within 0.1 %, else the leg is void (successor re-takes every arm). Then the table carries PF-2d's u0, prev, newF, newG medians (`../2026-11-m8-pf2d/measure/raw`, spread 0.000 % there) beside newE and newL: newE/U0 per tier-A cell against the 1.15 bar, reported, not asserted |
| P4 | `mutations.sh` (`legs/mutate.py`) | control clean; every MUST mutation killed by a named test; tree restored |
| CI | one dispatch of the four-lane workflow at the pushed `pf2e` head (`gh run watch --interval 120`) | green on all four lanes (reported; a red lane is investigated, not waived) |

MUST mutations: P1 kDepStagnation 0.5 -> 0.01 (cuts contracting refinements); P2 0.5 -> 2.0 (never cuts a stagnant step); P3 the
re-read's reach kDepTol -> kDepUndecidable; P4 the test's reach kDepUndecidable -> kDepTol; P5 the zero-solve pre-check (test's own
last step) removed; P6 the resume ignores the test's steps (resume_steps forced 0 with the final iterate kept); P7 test_candidate
calls certificate_coefficients (no probe) instead of grey_reread; P8 the test probe also on screen tests. Killers: `WsIndependence*`,
`QpGreyReread.*` (hven_qp_tests) and `QpE2WorkingSet.AGreyReReadSolvesTheAcopr57RestorationQp` (hven_qp_seam_tests, Release). A
mutation equivalent by construction is recorded as such with the argument.

## 5. Not run

HOLDOUT (never). The replay (counters of the 27 cells are covered by the scratch log; not a gate leg here). The census. Any
wall-clock claim. Accelerate and Windows readings (CI only). `f7_n10000_path_physics`. Any polish-side lever.
