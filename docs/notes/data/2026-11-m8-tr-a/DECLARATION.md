# M8 TR-A -- DECLARATION (trace v1: machine schema, writer, human trace; committed before any leg; never edited afterwards -- a failure is DECLARATION-r2.md)

## Change
Branch tr-a off origin/m8 `fa97d6bf0`. Spec: docs/notes/2026-11-m8-tr1-trace-spec.md section 6 row TR-A (owner-approved, section 8 answers
as recommended). Schema: docs/trace-schema-v1.md. Migration lines: docs/migration-0.2.md section TR-A. Reader-move list: READERS.md.
- A (writer). `JsonLinesTraceSink` writes schema v1 only; the v0 writer is retired. Four shared records (`solve.begin`, `iter`, `message`,
  `solve.end`) over `IterationRecord` / `IpmIterationRow` / `SqpIterationRow`; envelope `{v:1, ev, seq, sid, depth, eng}`.
- B (human trace). `ConsoleTraceSink` re-cut: header, legend, fixed-width lines, footer, verbosity ladder with `Verbosity::kDetail`,
  `Format{verbosity, color, legend, timing}`. `IpmOptions::wide_console`, the IPM banner/table and `format_iteration_table` (+ five pieces) retired.
- C (engines, observation only). The IPM per-iterate record gains five unconditional fields (`step_norm`, `soc`, `watchdog`, `rolled_back`,
  `factorizations`); `emit_message` takes a builder; `solve.end` is emitted by the public handles after the result is finished (the SQP
  sub-driver emits its own depth-1 end). No trajectory code moves.

## Declared movers (the ONE console-bytes and trace-bytes event)
- Trace bytes: every golden line re-derived (all records; the QP-tier events change only in the envelope).
- Console bytes: every console golden re-derived; the 159/119-column IPM tables and the SQP table go.
- Options dump: `IpmOptions::wide_console` removed (`kIpmOptionsFields` 17 -> 16, `kIpmDumpLines` 68 -> 67, `sizeof(IpmOptions)` re-observed).
- `Verbosity` gains `kDetail` (validation range, `enum_name`).
- NOTHING ELSE MOVES: corpus counters/statuses/residual columns (replay, interior, armed) byte-exact against arm A; the off path is gated below.

## Arms and binaries
Arm A = origin/m8 `fa97d6bf0` (`git archive` build, legs/prebuild.sh). Arm B = the code head built Release in .scratch/build-tra/rel, frozen to
.scratch/build-tra/armBbin by runtime/run_all.sh. Both `hven_sqp_corpus`; sha256 recorded in VERDICTS.txt and MANIFEST.txt. SAFER_FAST, clang++.

## Proofs and pass criteria (each under `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty first as its own command, setsid -w, MKL_NUM_THREADS=1, ctest -j4)
P1  ctest Release and Debug at the code head: 100% passed.
P2  RUNTIME / INSTRUCTIONS OFF (REQUIRED: the five IPM fields add per-iteration work; the SQP/IPM emit sites add pointer tests). runtime/ = the
    AP-5 G7 instrument (perf instructions:u at -c 400000, taskset core 2, phase-aligned, A B / B A / A B rounds, THROTTLE events void the leg),
    SOLO and SERIAL, FIRST: counter identity holds, 0 throttled processes, FILTERED B/A within [0.995, 1.005] in each of interior, ipm, ssn, walk.
    The aggregate-|delta| <= 0.1% / per-cell <= 0.5% gate of spec section 4 proof 4 is read from the same tables and reported; the band above
    is the verdict. A/A calibration leg: NOT RUN in this group (the instrument's spread is established by G7's accepted runs under the same
    band); reported as open, not asserted.
P3  replay 27 cells x (walk, ssn, ipm): arm A vs arm B, scripts/compare_replay.py EXACT, "differences: 0" x3; ARMED (--callback count +
    HVEN_LEG_UNFIRED_TOKEN=1) vs arm B unarmed, 0 x3. The in-process proof 3 (JSON sink + kDetail console, and callback at kAll on top,
    same counters/statuses/points as nothing attached) is `TraceV1Contract.ObservationChangesNoCounterStatusOrPoint` inside P1.
P4  interior: unmoved (A vs B) 0; promoted (vs bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv) 0; armed 0.
P5  install smoke, default and clang front ends, each followed by scripts/check_export_contract.sh: pass x2.
P6  structural proof 1: `TraceOffPath.*` (source scan with planted-site falsification) inside P1.
P7  contract tests inside P1: V1-V4 pins, identities 1/2/3/5 on the four-problem battery on both engines (`TraceV1Contract.*`), a golden
    line per record and block with hand-filled structs, human-trace goldens rendered from real solves, one golden per message template.
M   mutations (legs/mutations.sh + legs/mutate.py; a Release `git archive` copy of the code head; hven_drivers_tests, hven_interior_tests in full
    and hven_sqp_tests filtered to JsonLinesTraceSink.*:SqpConsole.*, each --gtest_fail_fast; the control fails nothing; each id alone).
    MUST BE KILLED by a failing test:
      w1 record field dropped from the writer           w2 wrong key (stationarity -> stationary)
      w3 sid never advances                             w4 solver_initialized milliseconds leak onto the wire
      c1 ladder rung leak (rows print at kSummary)      c2 colour on under kNever
      c3 kDetail line prints at kIterations             o1 off path: message builder runs before the sink test
      o2 off path: SQP row built outside the guard      v2 IPM step norm not written
      v3 IPM soc flag never set                         v4 IPM factorizations stamp dropped at the main site
      t1 SQP row without a step still reports one       t2 IPM row alpha is the maximum, not the effective length
    Verdict: legs/mutation_verdict.py.

## Order
1. runtime/run_all.sh <armB sha256> <armA sha256>   (solo, serial)
2. legs/chain.sh <code head> <armB sha256> <armA sha256>: ctest rel, dbg -> replay -> interior -> install smoke -> mutations -> verdict; STOPS at the first
   failed leg.
Evidence: runtime/<mode>/analysis.txt, suites/, replay/, interior/, install-smoke/, mutation/, VERDICTS.txt; raw dumps under the ignored
.scratch/tra-* with MANIFEST.txt (sha256, size, path). ON FAIL: stop, commit the evidence, report; DECLARATION-r2.md.

## Not run, disclosed
No TSan leg (nothing here touches threads or the stop machinery). hven-bench is not edited (TR-E); its break list is in the report.

## Pre-declaration runs (NOT evidence, disclosed)
Dev Release builds of the library and test targets; test migration runs of the touched suites; the contract/off-path tests were developed against
the dev tree. No runtime, replay, interior, install-smoke or mutation leg has run.
