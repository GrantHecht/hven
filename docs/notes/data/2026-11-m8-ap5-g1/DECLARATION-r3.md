# M8 AP-5 G1 -- DECLARATION-r3 (successor of DECLARATION.txt and DECLARATION-r2.txt; committed before any leg; never edited afterwards)

## Why
A code review of G1 (code head 08afb7f3d, r1/r2 evidence green) found seven items; the fix commit MOVES THE CODE HEAD,
so r1/r2's evidence no longer describes the head under review. r3 re-runs the FULL declared set on the new head with the
same criteria. r1/r2's declarations and outputs are kept untouched (suites/, replay/, interior/, install-smoke/, tsan/,
tsan-r2/, runtime/ and legs/ are unmodified; r3 writes only *-r3 directories).

## Unchanged (word for word from DECLARATION.txt)
Spec, base 030bae675, arm A (build artifact: 54d8216068f25d69fe5f4bc1e9d082f209071a1c88ae9ecdf4cc9255d4ac9ce6, the same
binary r1 used, copied with cp -p to build-g3/armAbin), toolchain, the change C1-C8 (as amended below), the column map,
D1-D8, and every criterion P0-P8 (P8 in its r2 form: the clang C compiler, a fresh tree). Pre-declaration runs
(NOT evidence, disclosed): the dev Release tree ran the full suite at this code head before this file: 100% of 3650;
the two fresh trees below were built (no test, replay, interior or runtime leg ran) before this file was committed.

## Code head
a9a1c66a2 (fix(m8-ap5-g1): review fixes), on e1d7a269e (evidence head of r2) on 08afb7f3d on origin/m8 030bae675.
Source/include/tests/bench/examples/cmake/scripts at the chain's guard are identical to a9a1c66a2.

## The fix (what moved, beyond DECLARATION.txt's C1-C8)
F1  Both handles set StopReason::kException beside status = kInterrupted before the swallowing try (a failure inside
    the report steps no longer leaves a kept partial reading kNone).
F2  detail::to_stop_reason takes the RESOLVED status: kOptimal/kAcceptable -> kNone whatever the recorded phase stop
    (a stall or restoration verdict beside a converged status; stall at an acceptable iterate resolves kAcceptable).
    Both call sites (handle, per-phase report). Effect on the interior corpus: the `stop_reason` column / trace key of
    a cell that ended kOptimal/kAcceptable with a recorded stall now reads `none`. D2's criterion is unchanged
    (map(arm A) == arm B EXACT on the 27 replay cells; raw differences reported); if a replay cell ended kAcceptable
    beside stage_stalled, that is a P4 failure, not a re-declared criterion. The G1 interior baseline file
    (bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv) is unchanged; its pinned tests pass at this head
    in the dev run above.
F3  SQP post-push stop arm: retract_last_judgement() before make_warm_start; qp_failure_steps_accepted and
    soc_applied bumps of the untaken step are undone. (Stop path only; the default path is unchanged.)
F4  StopSource: moved-from is stateless (request_stop() false, stop_requested() false, token() empty); request_stop()
    returns bool (true iff this call moved the flag).
F5  Tests: T2b (post-push poll pinned; deleting the token half of the poll fails it -- verified locally and restored),
    callback-wins-label, funnel-width-of-the-stop-point (fails without F3's retract -- verified), moved-from source,
    to_stop_reason's converged-status pins.
F6  IPM polls the token beside interrupt_requested_ after a converged (kOptimal/kAcceptable) phase that is not the last:
    remaining phases are skipped without the next init_impl (test fails without it -- verified).
F7  Docs only: G2 restoration caveat, StopReason renumbering table, last_stop_reason mid-callback note, SolveBudget
    literal-type note (migration-0.2.md, concurrency.md, solve_result.h comment).
D9' TESTS: 3645 -> 3650 run, as ctest counts them, Release and Debug (+5 new TESTs: 4 in test_stop_token.cpp ... see
    below; test_solve_status.cpp's existing test was extended, not added to): AMovedFromSource..., T2b..., ACallbackStop
    Wins..., ATokenStoppedSqpSolve...Funnel..., ATokenOnAPhasesConvergingRow....

## Binaries and trees (all under the git-ignored build-g3/; r1's build-g1 is left as it is)
Arm A   build-g3/armAbin/hven_sqp_corpus  sha256 54d8216068f25d69fe5f4bc1e9d082f209071a1c88ae9ecdf4cc9255d4ac9ce6
Arm B   build-g3/armBbin/hven_sqp_corpus (frozen from build-g3/rel/bench/hven_sqp_corpus)
        sha256 d97de0d735ca86f25b356dbff50fade113f83a91a4840e75dbd98588752104f8
Trees   build-g3/rel (fresh Release, every target), build-g3/dbg (fresh Debug, every target), build-g3/tsan-r3 and
        build-g3/smoke-* (created by their legs).
Instruments  legs-r3/ = legs/ with build-g1 -> build-g3 and output directories *-r3 (chain-r3.sh from chain.sh with
        tsan-r3.sh = tsan-r2.sh, the clang C compiler); runtime-r3/ = runtime/ with the same re-pointing. No other
        change to any instrument.

## Proofs and pass criteria (unchanged; each its own script under `flock /tmp/box-build.lock`; `pgrep -a hven_bench_cell`
empty first, as its own command; the runtime gate FIRST and alone, then legs-r3/chain-r3.sh)
P0  arm A / arm B sha256 above (checked by runtime-r3/run_all.sh, legs-r3/replay.sh, legs-r3/interior.sh).
P1  ctest Release (build-g3/rel) and Debug (build-g3/dbg), -j4 under setsid: "100% tests passed, 0 tests failed out of
    3650" in both (suites-r3/). Golden rig = the hven_golden_rig entries inside it.
P2  runtime gate (runtime-r3/run_all.sh <armB sha> <armA sha>): interior, ipm, ssn, walk; 27 cells; rounds 3, core 2,
    perf period 400000; THROTTLE == 0 or VOID; VERDICT PASS (counter identity per mode, interior map(A) vs B; filtered
    B/A in [0.995, 1.005]).
P3  replay 27 cells x (walk, ssn, ipm), arm A vs arm B, compare_replay.py EXACT, 0 differences each (replay-r3/).
P4  interior: unmoved map(A) vs B EXACT 0; promoted EXACT 0; derived EXACT 0 (interior-r3/). Raw differences reported.
P5  install smoke, default and clang arms, each with scripts/check_export_contract.sh: pass; 37 standalone TUs
    (install-smoke-r3/).
P6  the page's tests inside P1 (both builds), now including this fix's new tests.
P8  ThreadSanitizer (legs-r3/tsan-r3.sh; tsan-r3/): build-g3/tsan-r3, -fsanitize=thread, hven_drivers_tests, filter
    StopToken.*:StopTokenSolve.T2*:StopTokenSolve.T10*: exit 0 and 0 "WARNING: ThreadSanitizer" lines.
ON FAIL  stop, commit the evidence, report; any further run is the settler's DECLARATION-r4.
