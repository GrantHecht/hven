# M8 TR-D rider (naming) -- DECLARATION-r2 (addendum to DECLARATION.md; committed before any run of this rider)

Change (code commit f065ef1ff on tr-d 00e199d28): `SqpIterationRow::dual_regularization` -> `regularization_dual`; the sqp iter-row JSON key `dual_regularization` -> `regularization_dual` (the ipm block's spelling, `"regularization_dual"`). Reason: settler ruling from the AP-5 G5 review, one name for the quantity in both engines.

## Scope rationale: why runtime is NOT re-run
A rename is trajectory-free: no arithmetic, branch, layout, counter or option changes (same-length key, same member type and position). The TR-D runtime verdict (RT, instruction-count band) therefore carries over unchanged and is not re-run; the A/A calibration and B/A ratios in VERDICTS.txt stand. Mutations (M) likewise not re-run: no history/trace logic moved. This is a declared omission, not a pass.

## Left alone (different quantity)
`QpOptions::dual_regularization`, `QpSolveOverrides::dual_regularization`, `SolveOverrides`, `adaptive_dual_regularization`, `QpCounters::dual_regularization_decreases`, `IpqpOptions::dual_regularization_initial`, `detail::dual_regularization(mu)` (IPM free function), `qp.dual_regularization` option-dump key, option dumps and frozen evidence under docs/notes (historical), the G5 migration row naming the old G5 development name.

## EXPECTED MOVERS
1. Source/key: the C++ field, the trace key; test/golden strings carrying the key; schema doc, migration table, CATCH-UP.
2. Re-derived goldens: ONLY the sqp iter-row golden literals and key lists in tests/sqp/test_trace_writer.cpp. Proof G: every changed string-literal run (adjacent literals concatenated) at 00e199d28 vs f065ef1ff, with old->new map `dual_regularization`->`regularization_dual` applied, equals the new run byte for byte; the schema-doc table lines pair likewise.
3. Nothing else moves: counters, statuses, points, replay columns.

## Proofs and pass criteria (flock /tmp/box-build.lock, `pgrep -a hven_bench_cell` empty first, setsid -w, MKL_NUM_THREADS=1, ctest -j4)
P1  ctest Release and Debug at f065ef1ff: 100% passed.
P3  Replay 27 cells x walk/ssn/ipm, arm A (origin/m8 70b5d42be, existing binary) vs arm B (rebuilt Release corpus at f065ef1ff): differences 0 x3; armed B vs B: 0 x3. Reuses TR-D legs/replay.sh unchanged.
P5  Install smoke (default and clang) + export contract, rc 0.
G   Golden rename proof as above, 0 failures.
hven-bench: grep shows no reader of the field (reported in VERDICTS-r2).
Not run: RT, M, interior (no trajectory path); Windows/Accelerate UNOBSERVED.
