# M8 AP-5 G3 -- DECLARATION (best-so-far and kStopReturnBest; committed before any leg; never edited afterwards)

## Spec and change
Design page docs/notes/2026-11-m8-ap5-interface-design.md, row G3: `IterationSnapshot`, `snapshot()`, `SolveResult::best_iterate`,
`CallbackAction::kStopReturnBest`, best-tracker arming (section 3.3, Q4, Q14). Behaviour change: none. The tracker copies and
never steers; the new result field and the new action are the only visible additions, and a solve nobody arms and nobody stops
is bit-for-bit what it was. Declared as an event anyway, so the identity is measured rather than argued.

## Code head and base
Code head 8e8f2e65240c597b0ec9b8c998d6e61cd784013e (one code commit) on origin/m8 f1d8a8d19 (G1 and G2 landed). Arm A = origin/m8
f1d8a8d19. Source/include/tests/bench/examples/cmake/scripts at the chain's guard are identical to the code head.

## What landed, and where it differs from the page (recorded)
D-a  `IterationSnapshot` mirrors the event AS LANDED (G1/G2): `phase`, `depth`, `radius`, `mu` are the optionals the event still
     carries (G5 replaces them with the `ipm`/`sqp` info blocks and widens both types together). `violation`, `z_lower`,
     `z_upper` are not on the snapshot yet: the event has no such views until G5, and an always-empty public field was judged
     worse than adding the three with their payload bits. `g` IS on the snapshot (the page's section 3.3 fills it in
     best_iterate); `snapshot(event)` leaves it empty (an event carries no row values).
D-b  Row-space `lambda` and `g` of best_iterate are mapped by Problem from private engine row blocks
     (`SolveResult::EngineRows::best_*`), through the same function as the primary fields (`Problem::map_rows`).
D-c  The interior-point best is built once per phase at alg_impl's exit from the tracked pair, by the event's own declared-space
     arithmetic (`IpmSolverImpl::declared_point`, extracted from fire_iteration_event); the last phase that ran wins, as the
     tracker does; cleared when the solve ends kOptimal or kAcceptable. It carries the event scalars of its own iterate
     (step_norm, mu, elapsed, iteration) stored at tracking time. The step norm is therefore also taken when the tracker is
     armed without a callback (one inf-norm per iteration, armed solves only).
D-d  SQP: the tracker is captured eagerly where it improves, in the caller's units and variables (the engine->caller map the
     event uses, now one lambda shared by both: `caller_point`); `step_norm` (the step taken FROM the point) is filled when
     that row is pushed. A restoration sub-solve never tracks. Arming reads `return_best_on_max_iterations`, an installed
     callback, or a token that can fire.
D-e  kStopReturnBest on the SQP is honoured at all four callback-stop exits (post-push poll, ordinary terminal exit, and both
     restoration-return exits, the latter clearing returned_pre_restoration_point); a token stop never returns the best.
     The last non-continue action returned wins on both engines.
D-f  The empty-tracker fallback (kStop's point, returned_best_iterate false) is a defence: no fixture reaches it (the first
     finite iterate of a caller's solve is always recorded; a non-finite start ends before a stop is read). Not tested, stated.
     Observed, unchanged: the interior-point tracker's "recorded" test treats a criterion value of exactly 1e10 as unrecorded.
D-g  dep/eigen, dep/fmt in this worktree are plain copies of the main checkout's submodule trees (submodule update refused).

## Tests (P1 count)
ctest as it counts them: "100% tests passed, 0 tests failed out of 3703", Release and Debug (G2 r3: 3685). +18 verified from the
diff over origin/m8 (`git diff origin/m8 -- tests | grep '^+TEST'` plus the new file): 13 TESTs in tests/drivers/test_best_iterate.cpp,
4 in test_stop_recovery.cpp (StopRecovery.G3*), 1 in test_callback.cpp (Callback.SqpStopReturnBestInsideRestoration...). No test is
modified or removed. The page's T7 and T9 are these (T9 armed-and-unfired through restoration on both IPM modes, the SQP half
beside the existing SQP restoration fixtures); T12's snapshot half is here, its payload half is G5's.

## Pre-declaration runs (NOT evidence, disclosed)
- Dev Release (build-g3/rel) and Debug (build-g3/dbg) builds, every target, of a tree whose only later change is clang-format
  whitespace on the new test file; ctest 100% of 3703 in both. Then both rebuilt at the code head by legs/build.sh.
- Targeted gtest runs while writing the tests (BestIterate.*, StopRecovery.G3*, Callback.SqpStopReturnBest*) all pass; an
  exploratory empty-tracker test (a NaN-at-start problem) was written, showed the fixture does not reach the path, and was removed.
- legs/mutate.py's string edits were dry-run against copies of the four source files (all twenty-two apply); no mutant was built.
- Arm A built by legs/prebuild.sh (git archive f1d8a8d19 + dep copies); arm B frozen (cp -p) from build-g3/rel/bench.
- No ctest leg, replay, interior, runtime, install-smoke, TSan or mutation leg has run.

## Binaries and trees (git-ignored build-g3/)
Arm A  build-g3/armAbin/hven_sqp_corpus  (origin/m8 f1d8a8d19)
       sha256 77c3e021d694160a61d4b8810c5fe3bbe6ea489e20188c94116da7bf7787bae5
Arm B  build-g3/armBbin/hven_sqp_corpus  frozen (cp -p) from build-g3/rel/bench/hven_sqp_corpus at the code head
       sha256 35f6e048fdc73f3531acd9978496589304d6d0ee86d29878e6270bc073a7f524
Trees  build-g3/rel, build-g3/dbg, build-g3/tsan, build-g3/smoke-* (legs), build-g3/mut-src + build-g3/mut (mutation scratch).
Instruments  legs/ and runtime/ in this directory: G2 r3's, re-pointed to build-g3 and this directory. replay.sh compares arm B
with the COMMITTED G2 r3 rows (../2026-11-m8-ap5-g2/replay-r3/r3-<mode>-B3.csv); chain.sh adds the install smoke;
runtime/run_attached.sh and attach_delta.py are new; tsan.sh's filter adds BestIterate.*; mutations.sh/mutate.py are G3's.
Run order, each under `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty first, wall-clock legs SERIAL and alone (never
co-run): runtime/run_all.sh <armB> <armA>, runtime/run_attached.sh <armB> <armA>, then legs/chain.sh 8e8f2e65240c597b0ec9b8c998d6e61cd784013e <armB>.
Evidence is written to runtime/, suites/, replay/, interior/, install-smoke/, tsan/, mutation/ here; raw per-run dumps under
the ignored .scratch/ with a MANIFEST (sha256, size, path) committed; summaries, CSVs, verdicts and shas are committed.

## Proofs and pass criteria
P0  arm A / arm B sha256 above (checked by runtime/run_all.sh, run_attached.sh, legs/replay.sh, legs/interior.sh).
P1  ctest Release (build-g3/rel) and Debug (build-g3/dbg), -j4 under setsid: "100% tests passed, 0 tests failed out of 3703".
P2a runtime gate, unattached (runtime/run_all.sh): interior, ipm, ssn, walk, serial, alone; THROTTLE == 0 (else VOID);
    counter identity per mode; filtered B/A in [0.995, 1.005] (instructions:u, perf, pinned to one core).
P2b the attached-callback legs (runtime/run_attached.sh; the M7-4 residual re-measured): the interior leg with the read-only counting
    callback in BOTH arms under IterationPayload::kAll and under kScalars; THROTTLE == 0; counter identity; filtered B/A in
    [0.995, 1.005] for each. attach_delta.py reports each arm's cost of attaching beside the unattached leg (informational; the
    criterion is each leg's B/A). Arm B's tracker is armed in these legs, arm A has none.
P3  replay 27 cells x (walk, ssn, ipm): arm B vs the COMMITTED G2 r3 arm B rows, scripts/compare_replay.py EXACT, "differences: 0" x3;
    AND the ARMED leg: arm B armed (counting callback, best tracker armed, never-requested token) vs arm B unarmed, EXACT, 0 x3.
P4  interior, 43 cells: unmoved (arm A vs arm B) EXACT 0; promoted (arm B vs bench/baselines/2026-11-m8-ap5-g1-ipm-leg/
    interior_baseline.csv) EXACT 0; armed (arm B with HVEN_LEG_COUNT_CALLBACK=1 and HVEN_LEG_UNFIRED_TOKEN=1 vs arm B) EXACT 0.
P5  install smoke, default and clang arms, each with scripts/check_export_contract.sh: pass (TU count reported). Re-run because a
    public header (solve_result.h) changed; no TU, file or export-surface was added.
P8  ThreadSanitizer (legs/tsan.sh, build-g3/tsan, hven_drivers_tests, filter 'StopToken*:StopRecovery.*:Callback*:BestIterate.*', i.e.
    every stop, callback and best-iterate test in the driver binary): test binary exit 0 and 0 "WARNING: ThreadSanitizer" lines.
T7/T9 the G3 tests above, inside P1 in both builds, and re-checked by the mutation table.
M   mutation table (legs/mutations.sh, Release scratch copy, hven_drivers_tests + hven_interior_tests run in full; mutation r also
    runs hven_sqp_tests ColumnScalingDriver.*): the control (unmutated) fails nothing; each of the twenty-two a..v is applied alone.
    MUST be killed (at least one failing test): a b c d e f g h i l m n o p q r v. AT LEAST ONE of t, u must be killed (the SQP
    restoration test reaches one of the two restoration-return exits). j, k, s, and whichever of t/u is not reached may survive and
    are reported either way. Mutations: a ipm arming ignores callback and token; b ipm no stop-path tracking; c ipm no
    kStopReturnBest substitution; d ipm action never sets stop_return_best; e ipm best kept on an optimal exit; f ipm best step_norm
    zeroed; g ipm best ce/ci rows not published; h ipm best-return in restoration keeps the restoration-flagged exit; i ipm ordinary
    tracking only under the option; j ipm step norm only under a callback; k ipm best iteration index zeroed; l sqp tracker arming
    ignores callback and token; m sqp post-push poll ignores kStopReturnBest; n sqp best step_norm never filled; o sqp best kept on
    an optimal exit; p sqp restoration forwarder drops the best request; q sqp best exit leaves returned_pre_restoration_point;
    r sqp caller_point drops the column map; s sqp kFinishCurrent ignores kStopReturnBest; t/u the two sqp restoration-return exits
    ignore it; v Problem does not map best_iterate's row blocks.
ON FAIL  stop, commit the evidence, report; a successor -rN keeps these criteria unchanged.
