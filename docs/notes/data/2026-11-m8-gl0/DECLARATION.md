# M8 GL-0 (globalization diagnostics) -- DECLARATION (committed before any leg; never edited afterwards -- a failure is DECLARATION-r2.md)

Spec: docs/notes/2026-11-m8-brief.md, section TU, "globalization track (GL)", GL-0. Schema: docs/trace-schema-v1.md section 6 (freeze table). Base = arm A = origin/m8 `3edf5ede4` (RS-1a..1e landed). Arm B = the gl0 code head `fcd93dd44` (the commit preceding this declaration and the leg-scripts/evidence commits; its sha and the Release `hven_sqp_corpus` sha256 are stamped in VERDICTS.txt and checked fail-closed by the chain). Instrument: the TR-D legs (docs/notes/data/2026-11-m8-tr-d/), copied and re-pointed (paths, tags, mutation list, the RS-moved source paths); thresholds unchanged.

## What changes
- `SqpCounters` gains eight fields, appended after `payload_ignored` (so `solve.end.counters` and `dump_counters` gain eight trailing keys before the `ssn` object): `reject_funnel_ceiling`, `reject_f_armijo`, `reject_h_funnel`, `reject_qp_failed`, `reject_other`, `accepted_f_type`, `accepted_h_type`, `rejection_run_histogram` (`std::array<Index, 5>`, one table entry). Identities on every solve: `rejected_steps == sum of the five reject_*`; `steps_accepted == accepted_f_type + accepted_h_type`; `sum(rejection_run_histogram) == rejected_steps`. Not folded from a restoration sub-solve (as `rejected_steps`/`steps_accepted`).
- `detail::GlobalizationStrategy::last_reject_cause()` (new virtual, default `kUnclassified`); `FunnelStrategy::judge` records the cause of each `kReject` (one enum store per judgement); the driver reads it right after the first judgement of a trial and counts it at the three sites that already increment `rejected_steps`.
- `SqpIterationRow::switching_ratio` (`std::optional<double>`): `pred_df / (kFunnelDelta * h_old^2)` stored on the driver's row when the QP solved to `kOptimal`; the trace `sqp` block gains the trailing key `switching_ratio`. DECISION: an iter-row field, not a per-solve summary, because docs/trace-schema-v1.md section 6 admits "a new trailing key in a record or block" within v1 (declared in the freeze table, golden lines re-derived). The row is built unconditionally (history), so the field is one stored double; the division is done once per major.
- (c) SOC attempts and rescues are the EXISTING `soc_steps` and `soc_applied` (not duplicated); f-type/h-type accepted counts and the run histogram are new.
- Docs: docs/trace-schema-v1.md (sqp block, freeze row), docs/trace-schema-v0.md (counter rows, appended note), docs/migration-0.2.md section GL-0, CATCH-UP.txt.
- No existing counter, status, point, option, existing trace key or console byte changes.

## EXPECTED MOVERS (everything else must not move)
1. Source: the eight counters, the row field, the strategy virtual.
2. Declared golden/pin re-derivations (the arity pins move by design): `kAggregateArity<SqpIterationRow>` 23 -> 24 (src/trace/trace_writer.cpp and the mirrored pin in tests/sqp/test_trace_writer.cpp); `kSqpCountersFieldCount` 95 -> 103; the SQP golden lines in `tests/drivers/golden/trace_v1/sqp_complete.jsonl` (iter lines gain `switching_ratio`, `solve.end.counters` gains eight keys) and any test-embedded SQP iter/solve.end golden string; the distinct-values counters fixtures. The IPM goldens (`ipm_complete.jsonl`, `ipm_aborted.jsonl`) and the human-trace goldens do NOT move.
3. NOT MOVING, asserted: every counter value that existed before, statuses, stop reasons, points, multipliers, residual columns. Replay (27 cells x walk/ssn/ipm) A vs B and the interior leg A vs B: `differences: 0` over every existing column (the bench CSV selects its columns by name and is not edited, so the replay compares exactly the pre-GL-0 columns; the new counters are proven by the unit/scenario tests and by the identities, not by replay columns); armed legs B-armed vs B: 0.
4. Instruction counts (runtime leg): one enum store per judgement, one counter increment per rejection/acceptance, one double division per major; expected within the band. Pass criterion is the band, not a prediction; a reading outside is a FAIL and is not re-banded.

## Proofs and pass criteria (every run under `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty first and as its own command, setsid -w, MKL_NUM_THREADS=1, ctest -j4)
S    Scenario tests (in P1): `FunnelRejectCause.*` (each branch of `judge()` names its cause; every other verdict clears it; the default virtual reports `kUnclassified`); `SqpGl0.*` on fixtures whose trajectories are derived in the test file: an Armijo failure (FlatQuartic), a run of ceiling rejections ending in the restoration request (SecondOrderInfeasible), an h-type funnel-decrease failure (the x^2-1 fixture with the Newton step landing at h = 99.5 against beta*tau = 99), a routed QP failure (TwinSaddle), an h-type acceptance, a caller's strategy without a cause (counted `reject_other`, with a run of six pinning the 5+ bucket), the ratio against the hand-derived value and bit-for-bit against the first judged context, `+inf` at h = 0, null on a row with no solved subproblem, and the three identities over a 30-solve HS battery; the trace writer's per-key iter-line pin and the counters key-list pins cover the wire.
P1   ctest Release and Debug, every target, at the code head: 100% passed.
P3   Replay x3: 27 cells x walk/ssn/ipm, arm A vs arm B, scripts/compare_replay.py EXACT: differences 0; armed (callback count + unfired token) B-armed vs B: 0.
P4   Interior: 43 cells: unmoved (A vs B) / promoted (B vs bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv) / armed: 0 x3.
P5   Install smoke (default and clang) + scripts/check_export_contract.sh, rc 0.
RT   Off-path runtime, cost contract section 4 proofs with NO SINK ATTACHED (the corpus binary attaches none): the A/A calibration FIRST (arm A as both arms, solo, serial, taskset core 2, perf instructions:u): the band [0.995, 1.005] is CONFIRMED iff every mode's A/A filtered ratio lies in [0.999, 1.001], else revised to 5x the worst A/A deviation (declared in VERDICTS). THEN B/A filtered per mode (interior, ipm, ssn, walk) within the band, 0 throttled, counter identity holds. Aggregate |delta| and per-cell maxima reported.
M    MUTATIONS (Release `git archive` copy of the code head; hven_drivers_tests and the SqpGl0/FunnelRejectCause/JsonLinesTraceSink/SqpConsole/Counters filter of hven_sqp_tests; control fails nothing; each id alone). MUST BE KILLED:
     c1  a ceiling rejection counted as an Armijo failure
     c2  an Armijo failure counted as an h-type funnel failure
     c3  an h-type funnel failure counted as a ceiling violation
     c4  a routed QP failure counted as "other"
     c5  the strategy swaps its Armijo and h-type causes
     c6  the strategy reports no cause for a ceiling rejection
     f1  an h-type acceptance counted as f-type
     r1  the switching ratio is unwired (never stored)
     r2  the ratio divides by the trial's h instead of the iterate's
     r3  the ratio drops the delta factor
     r4  the trace writer drops the key
     r5  the record builder drops the ratio
     h1  the histogram is off by one
     h2  the histogram's last bucket saturates one early
     h3  the routed-failure site never advances the run
     REPORTED, not must-kill: x1 (the cause is read after the second-order correction's judgement); a survivor is disclosed, not hidden.
     Verdict: legs/mutation_verdict.py (kills judged by failing test names; a build failure is an instrument fault).

## Not run, disclosed
No TSan leg. hven-bench not edited (its columns for the new counters are listed in CATCH-UP.txt). Windows/Accelerate UNOBSERVED. Pre-declaration runs (not evidence): the Debug build and the new tests' first runs, all on the pre-commit tree.
