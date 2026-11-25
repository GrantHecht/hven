# M8 TR-D / TR-D' (one iteration history for both engines) -- DECLARATION (committed before any leg; never edited afterwards -- a failure is DECLARATION-r2.md)

Spec: docs/notes/2026-11-m8-tr1-trace-spec.md section 2.4, section 4 (C5), section 6 rows TR-D and TR-D', section 8 Q1/Q8/Q14 (owner: full unification). Schema: docs/trace-schema-v1.md. Base = arm A = origin/m8 `70b5d42be`. Arm B = the tr-d code head (the commit preceding the leg-scripts/evidence commit; its sha and the Release `hven_sqp_corpus` sha256 are stamped in VERDICTS.txt and checked fail-closed by the chain). Instrument: the TR-B legs (docs/notes/data/2026-11-m8-tr-b/), copied and re-pointed (paths, tags, mutation list); thresholds unchanged.

## What changes
- TR-D: `IpmResult::history` (`std::vector<IterationRecord>`, `ipm` block set), ALWAYS ON: built at exit of each phase from that phase's `iters` (O(iterations), once, one copy), through the same `make_iteration_record` the trace uses.
- TR-D': `SqpResult::history` becomes `std::vector<IterationRecord>` (`sqp` block set); built unconditionally in `push_history` (the same record the trace emits, then moved into the vector); `SqpIterate` retires from the public headers (the driver's working row survives as `detail::SqpMajorRow`, `hven/detail/drivers/sqp_major_row.h`). `factorizations` is on the SQP row unconditionally (it already was in the trace row; the history now carries it with no sink).
- DEVIATION FROM C2 ("five fields and nothing else"), declared: `IpmIterate` gains TWO more per-record bytes, `recovering` and `objective_bearing`, stamped by `stamp_row` immediately before each of the six `iter` emit sites, whether or not a sink is attached. The trace's `make_iteration_record` used to read the solver's transient evaluation state (`cur_eval_prov_`, `restoration_->is_active()`) at emit time; a history built at exit needs that state per row, so it is stamped on the row and both the trace and the history read it. Two bool stores per iteration; the trace bytes are unchanged by construction (the stamp is taken at the same program point the emit read it) and proved so below.
- Mechanical break: every history reader in tests and bench re-pointed (`h.tr_radius` -> `h.sqp->trust_radius`, ...; table in docs/migration-0.2.md, section TR-D). Test support `tests/common_support/iteration_record_eq.h` (bit-exact record equality). hven-bench: no live reader of `SqpResult::history`/`SqpIterate` (the one reader is the frozen M7 run probe under runs/, not touched); compile of the live lanes against this head reported in VERDICTS.
- docs/trace-schema-v1.md identity 1 stated once: `history.size()` == depth-0 `iter` lines == callback events; stop/abort doors push a row without a step. docs/migration-0.2.md section TR-D; CATCH-UP.txt.
- No counter, status, point, option or trace-byte change.

## EXPECTED MOVERS (everything else must not move)
1. Source: `SqpResult::history` element type (break); `IpmResult::history` (new); `IpmIterate` two bytes; the test/bench readers.
2. NOT MOVING, asserted: trace and console BYTES (every golden unchanged, no re-derivation), counters (all), statuses, stop reasons, points, multipliers, residual columns. Replay (27 cells x walk/ssn/ipm) A vs B and the interior leg A vs B: `differences: 0`; armed legs B-armed vs B: 0. The bench's `kkt_residual` column is re-derived from the record as `max(stationarity, feasibility)`, the engine's own fold, and the replay is its proof.
3. One pinned test assertion changes by design: `SqpDriverQpFailure.QpNumericalErrorShrinksInsteadOfAborting` asserted `history[0].step_norm == 2.0`, the failed subproblem's |p|inf on a routed QP-failure row (old `SqpIterate::step_norm` kept it); the record's step is absent on such a row (spec 2.4: no step leaves a routed QP-failure row), so the assertion becomes "no step". Nothing else asserted a value the record cannot carry. The structural scan `TraceOffPath` gains one allowlist entry (`result_.history.push_back(`, the always-on IPM history, spec C5) and the SQP trace emit gains an explicit sink guard at its call site.
4. Instruction counts (runtime leg): the always-on history costs one record copy per solve plus the per-row stores and the SQP's per-iteration record build; expected slightly above 1.000. Pass criterion below is the band, not a prediction; a reading outside the band is reported as a FAIL and is not re-banded.

## Proofs and pass criteria (every run under `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty first and as its own command, setsid -w, MKL_NUM_THREADS=1, ctest -j4)
H    `history` == the depth-0 `iter` stream, row for row, every member by bits, both engines: tests `IterationHistory.HsBatteryHistoryIsTheDepthZeroIterStreamOnBothEngines` (27 HS problems x 2 engines, premise >= 20 cells each), `IterationHistory.TraceBatteryHistoryIsTheDepthZeroIterStreamOnBothEngines`, and `ExitDoorWalk.*` extended (every exit door of both engines: non-finite abort, callback stop, token stop, iteration limit, last-event stop, infeasible restoration) -- all inside P1; plus `IterationHistory.HistoryExistsWithNoSinkAndIsTheSameWithOne` (always on: no sink, kSilent) and `IterationHistory.SqpRowCarriesTheRunningFactorizationCount`.
P1   ctest Release and Debug, every target, at the code head: 100% passed (this includes the structural off-path scan `TraceOffPath.*`, the trace goldens unchanged, and the migrated history readers).
P3   Replay x3: 27 cells x walk/ssn/ipm, arm A vs arm B, scripts/compare_replay.py EXACT: differences 0 (75 columns); armed (callback count + unfired token) B-armed vs B: 0.
P4   Interior: 43 cells x 30 columns: unmoved (A vs B) / promoted (B vs bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv) / armed: 0 x3.
P5   Install smoke (default and clang) + scripts/check_export_contract.sh, rc 0.
RT   Cost contract section 4 proofs 2/4/5 with NO SINK ATTACHED (the corpus binary attaches none; the armed legs are replay's): the A/A calibration FIRST (arm A as both arms, solo, serial, taskset core 2, perf instructions:u): the band [0.995, 1.005] is CONFIRMED iff every mode's A/A filtered ratio lies in [0.999, 1.001], else revised to 5x the worst A/A deviation (declared in VERDICTS). THEN B/A filtered per mode (interior, ipm, ssn, walk) within the band, 0 throttled, counter identity holds. Aggregate |delta| and per-cell maxima reported.
M    MUTATIONS (Release `git archive` copy of the code head; drivers, interior and sqp-trace suites; control fails nothing; each id alone). MUST BE KILLED:
     d1  a history row is dropped (the IPM drops the last record of every phase)
     d2  a history row is dropped (the SQP stops pushing after two rows)
     d3  an IPM field is miswired in the history row only (complementarity from stationarity)
     d4  an SQP field is miswired in the history row only (trust_radius from violation_l1), after the trace emit
     d5  `factorizations` is not written on the SQP row
     d6i the IPM history is built with tracing on only
     d6s the SQP history rows are real only with a sink attached
     d7  the IPM provenance stamp is omitted (an instrument-completeness mutation: killed by the trace tests)
     Verdict: legs/mutation_verdict.py (kills judged by failing test names; a build failure is an instrument fault).

## Not run, disclosed
No TSan leg. TR-E consumers beyond the minimal hven-bench compile catch-up not touched. Windows/Accelerate UNOBSERVED. Pre-declaration runs (not evidence): the Debug build, a Debug ctest, and the new tests' first runs, all on the pre-commit tree.
