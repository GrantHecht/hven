# M8 TR-A -- DECLARATION-r3 (successor to DECLARATION.md and DECLARATION-r2.md, which stay frozen; committed before any r3 leg; never edited afterwards -- a failure is DECLARATION-r4.md)

## Why r3
The independent review of the r2 head (3bc8e5b3d) raised four must-fix and ten should-fix riders; all are now code, tests or docs at the **code head db4f8e60f** (this declaration commits the leg scripts that follow it; src/, include/, tests/, bench/ do not move after db4f8e60f).
The r2 legs are superseded wholesale: the chain below re-runs every leg on the new head (r2 skipped runtime/replay/interior and never ran Debug or the A/A calibration).

## What moved since r2 (library behaviour)
- R1 `TraceSink::on_solve_abort() noexcept` (non-emitting unwind, default no-op) called by both engines' handles on every exceptional exit and by the SQP restoration sub-driver's own guard; `JsonLinesTraceSink::reset_nesting` removed (superseded); Json/Console/FanOut implement it.
- R2 the IPM non-finite-evaluation exit no longer adds the gradient block to the retained right-hand side twice (and does no O(n) work for it with no sink).
- R3 `max_multiplier_eq/_ineq` are divided by the objective scale. R4 `barrier_objective` is written only when a step leaves the point.
- R5 the console prints the four QP-detail lines at every depth at kDetail. R11 direct-path census carries the split-form row split. R12 `ipm.phase.exit` keys `stationarity`/`complementarity`.
- Trace bytes that move versus r2: the `ipm.phase.exit` key names; `iter` lines' `max_multiplier_*` at non-unit objective scale; no-step IPM `iter` lines lose a nonzero `barrier_objective`; the depth-1 QP-detail console lines (sqp_elastic_detail golden). Everything else (replay/interior/armed counters, statuses, points) is byte-exact against arm A -- the proofs below assert it.
- R10 finding (no behaviour change): the IPM row's `mu` is the barrier parameter the step was formed at (post governor update); row 0 reads `barrier_min` (1e-12) on HS071 because the adaptive governor's first update clamps there. Event 0 and `solve.begin` carry the starting 1e-3; the row-vs-next-event identity is pinned. No golden moves.

## Arms and binaries
Arm A = origin/m8 `fa97d6bf0` (unmoved on `git fetch origin m8`; binary reused from run 1, sha256 8ad4cc11eb5ea78b9988bdd7fa5cbb01e4f4993ed7151c187420adef97b3e477, checked fail-closed). Arm B = the code head built Release (.scratch/build-tra/rel), `hven_sqp_corpus` sha256 f09806ab9ad0feeb6b4128f1276f08324fe3b955ce8cf476d7a2b00ddab5c9f3, frozen to armBbin by runtime/run_all.sh. SAFER_FAST, clang++.

## Proofs and pass criteria (each under flock /tmp/box-build.lock, `pgrep -a hven_bench_cell` empty first as its own command, setsid -w, MKL_NUM_THREADS=1, ctest -j4)
P1  ctest Release AND Debug (.scratch/build-tra/rel and /dbg, every target) at the code head: 100% passed.
P2  A/A CALIBRATION FIRST (runtime/run_aa.sh, arm A's binary as both arms, solo, serial, same cells/rounds as the gate): spread criterion fixed here BEFORE the data -- the frozen band [0.995, 1.005] is CONFIRMED iff every mode's A/A filtered ratio lies within [0.999, 1.001] (the instrument's spread an order below the band, as the AP-5 G7 runs showed); otherwise the band is REVISED to 5x the worst observed A/A deviation, declared in VERDICTS.txt, and B/A is judged against the revised band. THEN runtime/run_all.sh: counter identity, 0 throttled, FILTERED B/A in each of interior, ipm, ssn, walk within the (confirmed or revised) band; aggregate |delta| <= 0.1% and per-cell <= 0.5% (spec section 4 proof 4) read from the same tables and reported, the band is the verdict.
P3  replay 27 cells x (walk, ssn, ipm), arm A vs arm B EXACT "differences: 0" x3; ARMED (callback count + unfired token) vs arm B unarmed 0 x3.
P4  interior unmoved / promoted / armed 0.
P5  install smoke default and clang, each with scripts/check_export_contract.sh.
P6  `TraceOffPath.*` source scan including the planted cases and the new single-accumulation scan (inside P1).
P7  contract tests inside P1: V1-V4 (V4 now exact: each stepped row's factorizations grows by 1 + inertia_corrections), identities 1 (SQP: iterations + 1, row 0 is the start point) / 2 / 3 / 5 on the HS battery, both engines, observation-changes-nothing, the golden set (35 human-trace files + the IPM restoration and SQP elastic fixtures, each asserted to contain R / E tier rows), per-record/per-message goldens, TraceUnwind (both sinks, both engines, depth 1), RiderNonFiniteAbort, row multiplier scale, no-step barrier objective, shared census, row-mu identity.
M   mutations (legs/mutations.sh + mutate.py, Release `git archive` copy of the code head; hven_drivers_tests and hven_interior_tests in full, hven_sqp_tests filtered to JsonLinesTraceSink.*:SqpConsole.*, --gtest_fail_fast; control fails nothing; each id alone). MUST BE KILLED:
      the r2 table: w1 w2 w3 w4 c1 c2 c3 o1 o2 v2 v3 v4 t1 t2  (v4 now killed by the exact pin)
      new: p1a (IPM handle drops the unwind) p1b (SQP handle drops it) p1c (sub-driver drops its level) p2 (PGX accumulated twice -- the review's head, i.e. the regression test FAILS on that head)
           p3 (multiplier maxima in engine units) p4 (no-step row carries a nonzero barrier objective) p5 (QP-detail lines suppressed at depth 1)
           p11a/p11b (census from the seam, not the problem, on either engine)
    Verdict: legs/mutation_verdict.py.

## Order
1. runtime/run_aa.sh <armA sha>   2. runtime/run_all.sh <armB sha> <armA sha>   (both solo, serial, FIRST)
3. legs/chain.sh <code head> <armB sha> <armA sha>: ctest rel, dbg -> replay -> interior -> install smoke -> mutations -> verdict; STOPS at the first failed leg.
Evidence: runtime/aa/, runtime/<mode>/, suites/, replay/, interior/, install-smoke/, mutation/, VERDICTS.txt, MANIFEST.txt; raw dumps under the ignored .scratch/tra-*.

## Not run, disclosed
No TSan leg (the unwind adds no thread interaction). hven-bench is not edited (TR-E). Windows/Accelerate: UNOBSERVED.

## Pre-declaration runs (NOT evidence, disclosed)
Dev Release builds and full dev ctest (green) at the code head; the P1 Debug build only (no Debug test run); the probe programs for R10. No runtime, A/A, replay, interior, install-smoke or mutation leg has run.
