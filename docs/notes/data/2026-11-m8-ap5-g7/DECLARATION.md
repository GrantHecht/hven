# M8 AP-5 G7 -- DECLARATION (remaining interface symmetry; one stop rule in restoration; committed before any leg; never edited afterwards -- a failure is DECLARATION-r2.md)

## Change
Code head 3e609fdecbaf39438ef956c22a39d2fffdba419d (`feat(m8-ap5-g7)`) on origin/m8 1dacda16f. Rename table: docs/migration-0.2.md section G7.
Consumer list: CATCH-UP.txt.
- A (design row G7). Q7: `IpmConvergence {kkt, barrier}` -> `{stationarity, complementarity}`, the four `termination.*_kkt_tol` /
  `*_barrier_tol` -> `*_stationarity_tol` / `*_complementarity_tol`, and `IpmConvergence::feasibility = max(equality, inequality)` (NaN if
  either is). The norm check the design asks for: the two engines' feasibility norms do NOT agree -- the IPM `inequality` is the
  slack-form residual and the interior iterate never violates a bound, the SQP's is max(|cE|, max(cI)+, bound violation) -- so the
  field is added and the difference documented (header + migration note). R4 `SqpTiming`/`SqpResult::timing.engine_seconds`. E4
  `worker_options(IpmOptions)`/`worker_options(SqpOptions)`. E3 tier-thread introspection -> `detail::SqpSolverAccess`. R7
  `SqpCounters::payload_ignored` (appended last) and `IpmCounters::start_level_used`. E2 SQP `last_result().z` on the throw path:
  `measure_kkt` over the iterate's own evaluation, no model call, empty when the evaluation lags the iterate (`SolveState::ev_lags_x`,
  two stores per accepted major and one per restoration exit).
- B (from G6). `IpmCounters::inertia_corrections` (the ladder count each iteration factorization returns plus the dense guard's
  re-run count, summed where returned) and `IpmCounters::start_level_used` (kWarm when the payload's point is applied at entry;
  kSeeded when a multipliers-only seed is applied at its phase; else kCold). Real values, pinned by tests below.
- C (stop semantics). See "The C rule".

## The C rule (one rule across engines and depths) -- proposed and implemented
RULE 1 (C346 R5). A stop that lands inside feasibility restoration returns the point restoration was entered from, on both engines,
whoever asked: a requested `SolveBudget::stop` OR a callback `kStop` on a `recovering` event (IPM in-place proximal and nested l1;
SQP depth-1 rows under kIncludeRestoration). `returned_pre_restoration_point` true, the message suffix, the entry point's warm start.
`kStopReturnBest` with a recorded best returns the best (G3, unchanged); with none, the entry point.
Why: (a) the restoration iterate is not a point of the caller's problem with usable duals -- IPM recovering rows report `f` and the
diagnostics NaN, the nested arm zeroes the equality multipliers at entry and carries elastic pairs, and SQP depth-1 rows are in the
sub-problem's space; the entry point is the one with a meaningful objective and multipliers. (b) G2's token rule already returns it
on both engines and the SQP callback stop already did, so the IPM callback case was the only exception. (c) Design Q13's "the event
must not be a lie" is kept by the event and the result together: `recovering` tells the callback what it is shown, the result says
`returned_pre_restoration_point`, and a consumer who wants the restoration iterate takes `snapshot(event)` before returning kStop; no
information is lost. No concrete reason to keep the shown point was found: nothing reads it, and that exit's warm start was already
the conservative restoration capture. Mechanics: the IPM entry stash now also arms when a callback is installed (pure copy at entry);
the stash exit applies to any stop that did not substitute the best. THIS REVISES THE APPROVED Q13 ANSWER FOR THE CALLBACK CASE --
flagged for the owner (open item, VERDICTS).
RULE 2 (D-h). A stop the token already decided is not relabelled by a callback's answer on the row the token stopped at, at any
depth. Depth 0 already did this (the token labels the exit before that row's event fires). Depth 1 now matches: the parent reads the
restoration sub-solve's own stop reason, and a `kStopRequested` sub-solve gives the parent the token's label (`kStopRequested`), the
entry point, and no best substitution (the exits test `token_exit` first). Why: the answer arrives after the decision; letting it
relabel made the outcome depend on depth.

## Declared movers
- Corpus: NONE. No corpus cell stops or throws, and the C/E2 changes are on the stop and throw paths only; B, R7 add counter writes
  that no corpus column reads (the IPM `payload_ignored` column and the renamed convergence reads keep their values). P3 and P4 are
  byte-exact through an IDENTITY column map (the corpus CSVs name columns by the bench row: `kkt_inf`, `barr_inf`, ...; no column
  name changes). ARMED: the IPM callback now arms the entry stash (a copy); 0 differences expected.
- Stop-path tests re-derived: `StopRecovery.T5BACallbackStopOnARecoveringEventReturnsTheShownPoint` -> `...ReturnsTheRestorationEntryPoint`
  (callback only, and callback + unfired token); `StopPollSeam.T4ARestorationAbandonAnsweredWithKStopReturnBestIsTheExitTestStop`'s
  last three assertions (kStopRequested, no best, entry point; the twin equalities unchanged).
- Throw path: `SqpSolver::last_result().z` (was empty). T11 below.
- Trace: `sqp.solve.end`'s counters object gains one trailing key `payload_ignored`; the two golden lines re-derived (leg G). The
  ipm trace keys are unchanged.
- Options dump: four IPM lines renamed. Drift pin re-derived (declared): `kIpmOptionsFields` 17, `sizeof(IpmOptions)` 480,
  `kIpmDumpLines` 68 and the SQP constants are UNCHANGED by a rename (confirmed by P1); a new pin names the four lines
  (`OptionsDump.TheTerminationTolerancesPrintUnderTheStationarityAndComplementarityNames`).
- Size pins: `sizeof(SqpCounters)` 1240 -> 1248 (tests/drivers/test_callback.cpp, test_threads.cpp), `kSqpCountersFieldCount` 94 -> 95,
  `kIpmCountersFieldCount` 12 -> 14 (`sizeof(IpmCounters)` = (14 + 4) * 8, `start_level_used` padded to one Index).

## Arms and binaries
Arm A = origin/m8 1dacda16f, `git archive` build (legs/prebuild.sh; build-g7/armA, stamp "unknown"), build-g7/armAbin/hven_sqp_corpus
sha256 4ac810154cc0767369d16933621ce178674e34c6b47e11956578b3d4fc12f6fc.
Arm B = build-g7/rel/bench/hven_sqp_corpus at the code head (configured with GIT_DIR/GIT_WORK_TREE, stamp 3e609fdecbaf), frozen cp -p to
build-g7/armBbin by runtime/run_all.sh, sha256 c1984bd697602a9f5d11515c1eb23e05bf93e4ab902470a9adc35f411eec8f84.
Trees build-g7/rel (Release), build-g7/dbg (Debug), all targets, clang++, SAFER_FAST. GIT_CEILING_DIRECTORIES = the worktree root on
every command. googletest from the main checkout's fetched source.

## Proofs and pass criteria (each under `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty first as its own command, setsid -w, foreground, MKL_NUM_THREADS=1, ctest -j4)
P2a runtime gate (REQUIRED: B and E2 add executed per-iteration code -- two `+=` per IPM iteration, two bool stores per accepted SQP
    major). runtime/ = G4 r4's instrument re-pointed, SOLO and SERIAL, FIRST: counter identity holds, 0 throttled processes, FILTERED
    B/A within [0.995, 1.005] in each of interior, ipm, ssn, walk (`VERDICT PASS`).
P1  ctest Release and Debug at the code head: 100% passed, 3777 tests (3771 at origin/m8 + six new TESTs:
    WarmProtocol.SqpColdCeilingCountsTheIgnoredWarmStart, IpmConvergence.FeasibilityIsTheMaxOfTheEqualityAndInequalityResiduals,
    DenseArmEngines.InertiaCorrectionsCountTheResidualGuardsReRuns, InterruptKinds.T11AThrowAfterAStopRequestKeepsTheExceptionContractAndHandsBackZ,
    OptionsDump.TheTerminationTolerancesPrintUnderTheStationarityAndComplementarityNames, Options.WorkerOptionsPreparesEitherEngineTheSameWay;
    T5BA renamed in place). `ctest -N` lists 3779 (the 2 Disabled entries are not counted).
P3  replay 27 cells x (walk, ssn, ipm): arm A vs arm B, scripts/compare_replay.py EXACT, "differences: 0" x3; ARMED (--callback count +
    HVEN_LEG_UNFIRED_TOKEN=1) vs arm B unarmed, 0 x3.
P4  interior: unmoved (arm A vs arm B) 0; promoted (vs bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv) 0; armed 0.
P5  install smoke, default and clang front ends, each followed by scripts/check_export_contract.sh: pass x2.
G   legs/goldens_check.py 1dacda16f <code head>: the two sqp.solve.end literals equal origin/m8's under the declared transform
    (key inserted; nested values +1 on the distinct line), the rest of tests/sqp/test_trace_writer.cpp unchanged: pairs 2, failures 0.
T11 (design) inside P1: the callback requests a stop and throws; the exception propagates, `last_result()` is kInterrupted/kException
    and carries `z` (size n, finite) on both engines, on the SQP equal bit for bit to the throwing event's `z`.
P8  TSan (legs/tsan.sh, own tree build-g7/tsan): drivers filter StopToken*:StopRecovery.*:Callback*:BestIterate.*:InterruptKinds.*,
    seam StopPollSeam.*; both exit 0 and 0 ThreadSanitizer reports.
M   mutations (legs/mutations.sh + legs/mutate.py; a Release `git archive` copy of the code head; hven_sqp_seam_tests,
    hven_drivers_tests, hven_interior_tests in full and hven_sqp_tests filtered to SqpDriverContract.*:JsonLinesTraceSink.*:
    SqpCountersFieldTables.*, each --gtest_fail_fast; the control fails nothing; each id alone). MUST BE KILLED by a failing test
    (m1, m2 by a BUILD_FAIL ONLY -- the sizeof/arity static_asserts are the pin; any other BUILD_FAIL is an instrument fault):
      q1  IpmConvergence::feasibility not written            q2  dump prints termination.acceptable_kkt_tol
      r4  SqpTiming::engine_seconds not written              e4  worker_options(SqpOptions) keeps the threads
      e3  SqpSolverAccess::ssn_tier_threads reads the IPQP tier
      r7s SqpCounters::payload_ignored never set             r7i IpmCounters::start_level_used never kWarm
      r7j IpmCounters::start_level_used never kSeeded        b1  inertia_corrections: the main ladder count not added
      e2  SQP last_result z not filled (T11)
      c1  RULE 1: IPM callback stop in restoration takes no stash (G2's condition)
      c1b RULE 1: IPM stash armed by the token only (G2's arming; the callback-only case)
      c2  RULE 2: a depth-1 answer relabels a token-stopped sub-solve (G4's behaviour)
      m1  IpmCounters X entry inertia_corrections deleted    m2  SqpCounters X entry payload_ignored deleted
    REPORTED, may survive: b2 (the dense guard's re-run count not added -- the guard fixture may re-run without extra ladder steps)
    and e2b (the ev_lags_x guard ignored -- no test reaches a throw inside the commit's derivative refresh).
    Verdict: legs/mutation_verdict.py.

## Order
1. runtime/run_all.sh c1984bd697602a9f5d11515c1eb23e05bf93e4ab902470a9adc35f411eec8f84 4ac810154cc0767369d16933621ce178674e34c6b47e11956578b3d4fc12f6fc   (solo, serial)
2. legs/chain.sh 3e609fdecbaf39438ef956c22a39d2fffdba419d c1984bd6... 4ac81015... (full shas): ctest rel, dbg -> replay -> interior ->
   install smoke -> goldens -> TSan -> mutations -> mutation verdict; STOPS at the first failed leg.
Evidence: runtime/<mode>/analysis.txt, suites/, replay/, interior/, install-smoke/, goldens-check.txt, tsan/, mutation/, VERDICTS.txt; raw dumps
under the ignored .scratch/g7-* with MANIFEST.txt (sha256, size, path) committed. ON FAIL: stop, commit the evidence, report; DECLARATION-r2.md.

## hven-bench (outside this repository; not an hven proof)
Branch g7-catchup (local, off main 195feaf, not pushed), commit d7a55b9: lanes/hven/hven_bench_cell.cpp reads the renamed convergence
fields. Compile proof against the code head's install prefix (P5's): reported in VERDICTS.txt.

## Pre-declaration runs (NOT evidence, disclosed)
- Dev Release builds; the full Release ctest on a pre-commit tree (3775 then: 3 expected trace failures, re-derived since); the Release
  and Debug trees rebuilt at the code head; the affected suites (356 tests, -R filter) in both trees, which caught a wrong expected
  string in the new dump pin (fixed before the code commit); the dump/worker tests re-run at the head in both trees.
- legs/mutate.py dry-applied to copies (every id matches once); legs/goldens_check.py dry-run against the working tree (pairs 2,
  failures 0); bash -n / py_compile on every instrument. Arm A prebuilt; arm B's sha read from the rel tree.
- No runtime, replay, interior, install, TSan or mutation leg has run.
