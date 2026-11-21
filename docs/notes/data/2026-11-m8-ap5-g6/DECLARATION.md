# M8 AP-5 G6 -- DECLARATION (IpmCounters and the regrouped IpmResult; committed before any leg; never edited afterwards -- a failure is DECLARATION-r2.md)

## Change (design docs/notes/2026-11-m8-ap5-interface-design.md 2.1, 2.2, 4, 5 row G6)
Code head 0423a7562 (`feat(m8-ap5-g6)`) on origin/m8 7c70be63a (source-identical to its code head 56604db3b). Names move; no value changes.
- `IpmCounters` (core/solver_counters.h) with `HVEN_IPM_COUNTERS_FIELDS` beside the SQP table; 12 fields: factorizations, symbolic_analyses,
  restoration_entries, restoration_iters, soc_steps, nonfinite_step_rollbacks, polish_ignored, payload_ignored, watchdog_activations,
  recovery_depth_histogram, dense_solve_residual_refusals, dense_factorizations. `IpmResult::counters`, `IpmSolveRecord::counters`.
- The nine `IpmResult::last_*` fields move onto `IpmPhaseReport` (funnel_width, filter_size, filter_resets, monotone_switches, monotone_iterations,
  restoration_entries, restoration_iters, proximal_regularization_primal/dual) plus `kkt_factor_status` (`IpmKktFactorStatus`, replaces the Eigen
  `last_kkt_info`); `last_ran_phase(r)`. The append_diagnostics hooks take the phase report.
- Trace: the `ipm.phase.exit` key `last_kkt_info` -> `kkt_factor_status`. `ipm.solve.end` carries no counter key and is unchanged (the task text
  names that line; the only trace key of this group that exists is the phase.exit one).
- Table of every rename: docs/migration-0.2.md section G6. Consumer list: CATCH-UP.txt.
- NOT in this commit (stated, design 2.1): `IpmCounters::start_level_used` and `::inertia_corrections` -- new values, not renames; design section 5
  puts `start_level_used` (R7) with G7. M7-19 -- see "M7-19" below.
- One meaning change, multi-phase only: `restoration_entries`/`restoration_iters` are the sum over the phases that ran (was the last phase, optional).

## Count before the first leg (design 2.1): corpus cells that are multi-phase AND enter restoration
`legs/count_multiphase_restoration.py` on the promoted interior baseline (43 rows; bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv):
rows=43 multi_phase=2 with_restoration=2 multi_phase_and_restoration=0  (multiphase-restoration-count.txt). The two multi-phase rows are the
`solve_optimize` variant with restoration off; the two restoration rows (`stalled`, `resto_infeasible`) are single-phase. EXPECTED ZERO: confirmed.
The chain re-counts on arm B's own interior CSV (interior/multiphase-restoration-count-B.txt); a nonzero count fails the leg.

## Arms and binaries
Arm A = origin/m8 7c70be63a, `git archive` build (build-g6/armA, plain, no GIT_DIR), corpus binary build-g6/armAbin/hven_sqp_corpus
sha256 c9ee5a8283e3a09caf6f767f06c4294d579a7d2e36a93ba35db95e5765cf8010.
Arm B = build-g6/rel/bench/hven_sqp_corpus at the code head (configured with GIT_DIR/GIT_WORK_TREE, stamp 0423a75624cb), frozen cp -p to
build-g6/armBbin, sha256 7b1f8681a412ab9248019876f3054c60fcd50722099b805704f275ab3dfe729c.
Trees: build-g6/rel (Release), build-g6/dbg (Debug), both all targets, clang++, SAFER_FAST. Instruments: legs/ (this directory), the G5 legs-r3 pattern
re-pointed; googletest from the main checkout's already-fetched source (FETCHCONTENT_SOURCE_DIR_GOOGLETEST; network clone is blocked by the GIT_DIR env).

## Column map (P3)
IDENTITY. The corpus CSVs name their columns by the bench row (`analyses`, `soc_steps`, `watchdog_activations`, `factorizations`, ...), not by the result
fields; bench/ipm_corpus_leg.cpp reads the renamed fields into the same columns, so the 76-column SQP schema and the 31-column interior schema keep every
name. Old -> new column name: none changes. A column-reading miswire (analyses, soc_steps, watchdog_activations) shows as a replay difference.

## Runtime gate (P2a/P2b): NOT run -- justification
No per-iteration or per-factorization instruction is added or removed. The executed deltas are: per PHASE, three optional/enum stores into the phase report and
two optional resets (O(1) per phase, not per iteration); per CALL, a four-line sum over the phases and one IpmCounters copy into the ledger record when a ledger is
attached; the counter increments are the same increments through `result_.counters.` instead of `result_.`; `CheckInfo` writes a member instead of a result field.
The SQP engine is untouched. Counter and status identity of the whole solve path is asserted by P3 and P4 (arm A vs arm B, EXACT). If P3/P4 move, the gate is owed.

## Trace goldens (declared re-derivation)
Re-derived: the test literals carrying the key (tests/sqp/test_trace_writer.cpp, tests/interior/test_ipm_trace.cpp) and the schema table row. Proof:
legs/goldens_rename_check.py 7c70be63a <code head> pairs every removed/added quoted-key line, applies the map {last_kkt_info -> kkt_factor_status} to the removed
line and demands byte equality with the added line: pairs 4, failures 0 expected (pre-checked, not evidence). Every other line of every golden is untouched, and the
suites (P1) pass on the new goldens.

## Ledger record pinned
tests/drivers/test_ipm_counters.cpp `IpmCounters.TheLedgerRecordCarriesTheResultsCounters` (record.counters equals the result's, field by field through the X-macro, and the scalar
fields are read from it) plus the existing IpmLedger tests in tests/interior/test_ipm_trace.cpp; both inside P1.

## Proofs and pass criteria (each leg under `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` first as its own command, setsid -w, foreground, MKL_NUM_THREADS=1, ctest -j4)
P0  arm A / arm B sha256 above (checked by replay.sh, interior.sh).
P1  ctest Release and Debug: 100% passed, 3769 tests (3762 at origin/m8 + the seven new G6 TESTs in test_ipm_counters.cpp).
P3  replay 27 cells x (walk, ssn, ipm): arm A vs arm B scripts/compare_replay.py EXACT, "differences: 0" x3 (identity column map); ARMED leg (--callback count +
    HVEN_LEG_UNFIRED_TOKEN=1) vs arm B, 0 x3.
P4  interior (43 cells): unmoved arm A vs arm B 0; promoted (vs the G1 baseline) 0; armed 0; multi-phase-with-restoration count 0.
P5  install smoke, default and clang arms, each with scripts/check_export_contract.sh: pass.
G   goldens_rename_check.py: failures 0.
M   mutations (legs/mutations.sh + legs/mutate.py; Release scratch copy of the code head; hven_sqp_seam_tests, hven_drivers_tests, hven_interior_tests each run with
    --gtest_fail_fast; the control fails nothing; each id applied alone). MUST BE KILLED -- by a failing test, or by a BUILD_FAIL for m1 and m2 ONLY (the sizeof/offset
    static_asserts are the pin; any other BUILD_FAIL is an instrument fault, not a kill):
      m1 X-macro entry dense_factorizations deleted        m2 X-macro entries out of declaration order
      c1 counters.factorizations not set (solve paths)     c2 symbolic_analyses not set
      c3 soc_steps and watchdog_activations crossed        c4 watchdog_activations dropped
      c5 recovery_depth_histogram dropped                  c6 nonfinite_step_rollbacks dropped
      c7 polish_ignored dropped                            c8 payload_ignored dropped
      c9 dense_solve_residual_refusals dropped             c10 dense_factorizations dropped
      c11 restoration_entries sum dropped                  c12 restoration_iters sum dropped
      p1 funnel_width, p2 filter_size, p3 filter_resets, p4 monotone_switches, p5 monotone_iterations not written to the phase report
      p6 restoration_entries, p7 restoration_iters (phase) not written    p8/p9 proximal primal/dual not copied    p10 kkt_factor_status not copied
      p11/p12/p13 the acceptance / governor / restoration append_diagnostics call not made
      p14 last_ran_phase ignores `ran`                     p15 IpmSolveRecord::counters not set
      p16 ipm.phase.exit key still spelled last_kkt_info
    REPORTED, not must-kill: c1b (counters.factorizations not set on the interrupted/exception hand-back path -- no test reads it) and c13 (restoration_entries
    takes the last phase instead of the sum -- no fixture has restoration entries in a non-last phase; the corpus count above is zero).

## M7-19
Specified (docs/notes/2026-11-m8-tr1-trace-spec.md 5.3): emit the `ipm.iter` row and its callback event on the non-finite evaluation abort door
(ipm_solver.cpp, abort at the non-finite evaluation, counted in `iterations`); TR-B owns the walk test, which lands red against the pre-fix tree. It changes trace lines and
callback events on that door, so it needs its OWN declaration (own expected movers, own goldens). It is NOT in this declaration or commit; it is reported to the settler.

## Order
1. `legs/chain.sh 0423a7562 <armB sha> <armA sha>` (ctest rel, dbg -> replay -> interior (+count) -> install smoke -> goldens check -> mutations; stops at a failure).
Evidence: suites/, replay/, interior/, install-smoke/, mutation/, goldens-rename-check.txt here; raw dumps under the ignored .scratch/ with MANIFEST.txt (sha256, size, path) committed; VERDICTS.txt.
ON FAIL: stop, commit the evidence, report; successor DECLARATION-r2.md.

## hven-bench (outside this repository; not an hven proof)
Branch g6-catchup (local, not pushed): lanes/hven/hven_bench_cell.cpp and lanes/chain/hven_bench_chain.cpp read the five renamed IPM fields. Built against an install prefix of the code head: see VERDICTS.txt.

## Pre-declaration runs (NOT evidence, disclosed)
- Dev Release and Debug builds; the full ctest in the Release tree at the code head: 100% of 3769 (.scratch/pre-ctest-rel.log); the Debug tree built, not run.
- legs/mutate.py dry-applied to copies of the code head: all ids apply (one by one, count checked). legs/goldens_rename_check.py dry-run (pairs 4, failures 0).
- Arm A prebuilt (prebuild.sh), arm B restamped and frozen. No replay, interior, install, or mutation leg has run.
- A stray `git stash` / `git stash pop` in the worktree (tracked changes only, restored whole) happened before the code commit.
