# M8 TR-B (trace messages) -- DECLARATION (committed before any leg; never edited afterwards -- a failure is DECLARATION-r2.md)

Spec: docs/notes/2026-11-m8-tr1-trace-spec.md section 6 row TR-B, 2.7, 2.8, 4, 5.3. Schema: docs/trace-schema-v1.md (TR-A). Base = arm A = origin/m8 `e43d9f8b5`. Arm B = the tr-b code head (the commit preceding the leg-scripts commit; its sha and the Release `hven_sqp_corpus` sha256 are stamped in VERDICTS.txt and checked fail-closed by the chain).

## What changes (library behaviour)
- IPM: `restoration_entered{a = theta at entry, iter = the requesting row}` at the one entry function; `restoration_exited` on EVERY episode end with `s in {resumed, locally_infeasible, failed, non_finite, interrupted}`, `a` = violation at exit, `b` = the threshold judged against (one per entered). The locally-infeasible door keeps its existing emission and position.
- SQP: the message record from the SQP for the first time: `restoration_entered/exited/refused`, `subproblem_failure`, `elastic_exhausted`, `inconsistent_equalities`, `start_point_clamped`, `scaled_convergence_refused`, `stop_at_iteration{s}` (depth 0 only).
- M7-19: the IPM non-finite evaluation abort door writes its `ipm` `iter` row AND fires its callback event (before: neither). Trajectories untouched: the push, status and result are as before.
- A source scan over the message emit sites of both engines (a message builder is only invoked inside a sink guard), falsified by planted sites. The exit-door walk test (both engines, every exit door, count identity at each).
- No counter, status, point or option changes. No hven-bench edit (breaks listed in the report).

## EXPECTED MOVERS (everything else must not move)
1. Trace and console BYTES: the message lines listed above; the IPM `iter` line and callback event on the non-finite-abort door (M7-19, +1 row and +1 event on exactly the solves that end at that door, `iterations` unchanged because the record was already counted). Human-trace goldens that contain restoration, elastic or stop fixtures are re-derived; every other golden is byte-identical.
2. The disabled test `IpmTrace.DISABLED_EveryRestorationEntryWritesARestorationEnteredMessage` is enabled and passes.
3. NOT MOVING, asserted: counters (all), statuses, stop reasons, points, multipliers, residual columns, `history`. Replay (27 cells x walk/ssn/ipm) A vs B and the interior leg A vs B: `differences: 0`. The armed legs (counting callback + unfired token) B-armed vs B: 0 (the armed leg compares counter/status columns, not the callback count, so M7-19's extra event is invisible to it; it is asserted by the walk test).

## Proofs and pass criteria (every run under `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty first and as its own command, setsid -w, MKL_NUM_THREADS=1, ctest -j4)
RED  The walk test is committed BEFORE the fix and run on the pre-fix tree: the non-finite-abort cases FAIL (IPM iter lines and events == iterations - 1), every other door passes. Evidence walk-red/. Then GREEN after the fix.
P1   ctest Release and Debug, every target, at the code head: 100% passed.
P2   Section 2.7 identities on the HS battery plus the restoration fixtures (test_callback ~668, test_evaluation_failure ~886), both engines (inside P1): entered lines == restoration_entries (IPM; SQP: + exited{non_finite}, see Conflicts), one exited per entered, refused lines == refusals, subproblem_failure lines == qp_maxiter_exits + qp_numerical_exits + nonfinite_subproblems, scaled_convergence_refused lines == the counter, start_point_clamped iff counter > 0, stop_at_iteration <= 1 and == 1 iff stop_reason in {callback_stop, stop_requested}.
P3   The scan (TraceMessageSites.*) plus its planted falsification (planted unguarded builder invocation and planted unguarded construction both fail the scan) (inside P1).
P4   Cost contract section 4 proofs 1-4: (1) structural scan, (2) counters off: replay 27 cells x 3 engines A vs B exact, (3) counters on: sink and callback+sink give the same counters (inside P1, TraceV1Contract.ObservationChangesNoCounterStatusOrPoint extended to restoration fixtures), (4) instruction counts off: A/A CALIBRATION FIRST (arm A as both arms, solo serial); the band [0.995, 1.005] is CONFIRMED iff every mode's A/A filtered ratio lies in [0.999, 1.001], else revised to 5x the worst A/A deviation (declared in VERDICTS). THEN B/A filtered per mode (interior, ipm, ssn, walk) within the band, 0 throttled, counter identity holds. Aggregate |delta| <= 0.1% and per cell <= 0.5% reported.
P5   Replay x3: A vs B exact, differences 0; armed B vs B 0.  Interior: unmoved / promoted / armed 0.
P6   Install smoke (default and clang) + scripts/check_export_contract.sh.
P7   Goldens re-derived where declared to move (list in VERDICTS); no other golden moves.
M    MUTATIONS (Release `git archive` copy of the code head; drivers, interior and sqp trace suites; control fails nothing; each id alone). MUST BE KILLED:
     i1  IPM drops restoration_exited on the resumed path
     i2  IPM restoration_entered not emitted
     i3  IPM stop_at_iteration wrong s (callback/token swapped)
     i4  IPM message builder invoked before the sink test (runs with no sink)
     s1  SQP drops subproblem_failure
     s2  SQP restoration_entered not emitted
     s3  SQP stop_at_iteration wrong s
     s4  SQP message builder invoked before the sink test (runs with no sink)
     s5  SQP drops restoration_exited on the resumed path
     s6  SQP drops scaled_convergence_refused
     a1  M7-19 row missing (abort door emits the event but no iter row)
     a2  M7-19 event missing (abort door emits the row but no callback event)
     g1  SQP unguarded message construction added at a site (the scan must kill it)
     Verdict: legs/mutation_verdict.py (kills judged by failing test names).

## Conflicts between the spec and landed TR-A, and how they are resolved (follow TR-A / the code)
- Spec 2.7's subproblem_failure identity subtracts `nonfinite_box_projections`; the box projection precedes the guard and is NOT in `nonfinite_subproblems` (solver_counters.h), so the asserted identity has no subtraction.
- SQP `counters.restoration_entries` counts completed excursions (written at exit, after the non-finite return), so `restoration_entered` lines == `restoration_entries` + the count of `restoration_exited{s: non_finite}`; the IPM counter counts entries and the identity is literal.
- Schema doc identity 1: SQP `iter` lines == `iterations + 1` (TR-A); the walk test asserts that, and the IPM count identity literally.
- The spec's source line numbers are stale; sites are located by function.

## Not run, disclosed
No TSan leg. hven-bench not edited (TR-E). Windows/Accelerate UNOBSERVED. Pre-declaration runs (not evidence): none.
