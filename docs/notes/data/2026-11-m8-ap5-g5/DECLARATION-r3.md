# M8 AP-5 G5 -- DECLARATION-r3 (review riders, successor to r2; committed before any r3 leg; never edited afterwards -- a failure is r4)

## Relation to r1 and r2
r1 (DECLARATION.md) and r2 (DECLARATION-r2.md, evidence 36d46ec7c) stand as history. r2 failed ONE criterion: mutation x (sqp depth-1 z_lower not column-mapped, rider 3) SURVIVED,
because the column-scaling fixtures' depth-1 prices sit on the upper bound only. Every other r2 leg passed (P1 3757 x2, P3 0 x3 + ARMED 0 x3, P4 0/0/0, P5, P8 0 reports, h killed).
r3 changes: (1) the test gains a floor-blocked fixture (ScaledFloorBlockedModel; the premise asserts a nonzero depth-1 z_lower price on a scaled column) -- commit 56604db3b;
(2) the branch is rebased onto origin/m8 774cebeeb (the kOptimal 1x fix: QP counter optimal_row_gray_exits, polish/terminal changes in src/qp/qp_engine.cpp), which is arm A;
(3) every leg is re-run from scratch (no carried leg): the criteria, the mutation list and the instruments (legs-r3 = legs-r2 re-pointed) are otherwise r2's.
Runtime gates P2a/P2b: NOT re-run, same justification as stated below (the riders still add nothing executed on the unarmed path; P3/P4 arm A vs arm B EXACT stand guard); r1's gates stand.

## Change over r1 (the riders, unchanged in r3; settlement prose in docs/migration-0.2.md section G5 and CATCH-UP.txt)
1. SQP event rows composed on the measured flag (`MajorState::event_rows_measured`, set only under an installed callback with kConstraints at a measured point) instead of
   on block sizes: a problem whose rows are all free (me = mi = 0, m > 0) now gets m NaN `g` / zero `violation` on SQP events like IPM events, SolveResult::g and best_iterate.
2. Bench levers: `--callback count` / HVEN_LEG_CALLBACK_PAYLOAD=all (the widened kAll) are stamped `count-g5` / `all-g5` (header, console, ATTACHED-CALLBACK line); stale comment fixed.
3. Test: ColumnScalingDriver.RestorationEventsCarryTheCallersUnitsInTheCallersVariables also asserts z_lower / z_upper at depth 1.
4. IPM fixing-row sign split through `detail::split_bound_multiplier` (NaN kept) instead of the ternary; test CallbackPayload.TheFixedVariableMakeConstraintArmSplitsItsPriceIntoTheBoundSides.
5. Tests: SQP block non-vacuity; CallbackPayload.IpmBlockCarriesBacktracksAndInertiaCorrections (Rosenbrock from (-1.2, 1) backtracks; DuplicateRows runs the inertia ladder; the
   proximal mode's regularization_dual against the trace row's prox_reg_dual). The SQP block field-by-field comparison against history[k] already existed (Callback.SqpEventRowsEqualHistoryInCallerUnits).
6. Docs/comments: stale best_iterate / IterationEvent::depth/phase wording, violation NaN propagation, compose cost under kConstraints, reflowed comments, SQP best_iterate.lambda sign-repair line.
7. `hven::kDefaultIterationPayload` public (the detail alias removed); default argument on both engines.
8. `SqpIterationInfo::dual_regularization` renamed `regularization_dual` (the IPM block's name); header states IPM regularizations are at the engine's objective scale, mu at the caller's.
Behaviour change on any trajectory, counter or status: NONE (callback / test / doc / bench-tag only).

## Code head and arms
Code head 56604db3b14372c8fa729d56830c9e4912cb702c (the r3 test commit; the rebased feature commit d4c73489a and rider commit 1fda1b8e7 lie beneath it) on origin/m8 774cebeeb9ec23898d21fcdf44ea3861f8947605.
Arm A = origin/m8 774cebeeb (git archive build, build-g5/armA, copy build-g5/armAbin/hven_sqp_corpus) sha256 5cd3ec2ee55ece12a9a193819eb77442d3b24fb488c60094d6798e52f6bd0026
Arm B = build-g5/rel/bench/hven_sqp_corpus at the code head (restamped through GIT_DIR/GIT_WORK_TREE, ceiling kept), frozen cp -p to build-g5/armBbin sha256 d5c70102348d88f69731e32d2e8854f07b202c6dfdb47d3bc4062948f8abbb2a

## Runtime gates (P2a, P2b): NOT re-run -- justification
The riders add no executed instruction to a solve with no callback attached: `event_rows_measured` is written only inside `if (iteration_callback_)` and read only in
fire_iteration_event (which returns first when no callback is installed); the fixing-row split and `split_bound_multiplier` sit in the callback-armed declared_point path; the
default-mask constant moved namespace (constexpr); the rename, docs, tests and bench tags are not runtime. The one unarmed-path difference is one bool member in the per-major
MajorState struct. r1's gates (P2a interior 1.000000 / ipm 0.999998 / ssn 1.000000 / walk 0.999999; P2b within 1e-5) stand. Counter/status identity of the unarmed path is still
asserted by P3 and P4 below (arm A vs arm B, EXACT). If P3/P4 move, r3 fails and the runtime gates are owed.

## Tests (P1 count)
"100% tests passed, 0 tests failed out of 3762", Release and Debug (r1: 3753). New TESTs: CallbackPayload.AnAllFreeRowsProblemKeepsItsRowViewsOnBothEnginesAndTheBestIterate,
CallbackPayload.IpmBlockCarriesBacktracksAndInertiaCorrections, CallbackPayload.TheFixedVariableMakeConstraintArmSplitsItsPriceIntoTheBoundSides (+3 over origin/m8 774cebeeb; 3762 - 3 = the base). r3 adds no TEST (the floor fixture is a third case inside an existing TEST).

## Expected movers
NONE: no replay column, no interior column, no counter, no status.

## Proofs and pass criteria
P0  arm A / arm B sha256 above.
P1  ctest Release (build-g5/rel) and Debug (build-g5/dbg), -j4 under setsid: 100% passed, 3762 tests.
P3  replay 27 cells x (walk, ssn, ipm): arm A (RUN) vs arm B, scripts/compare_replay.py EXACT, "differences: 0" x3; the ARMED leg (--callback count + HVEN_LEG_UNFIRED_TOKEN=1) vs arm B, 0 x3.
P4  interior, 43 cells: unmoved (arm A vs arm B) 0; promoted (vs bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv) 0; armed 0.
P5  install smoke, default and clang arms, each with scripts/check_export_contract.sh: pass ("export contract OK" lines extracted into install-smoke-r3/export-contract-summary.txt; r1's grep matched nothing -- fixed in legs-r3/chain.sh).
P8  ThreadSanitizer (legs-r3/tsan.sh): hven_drivers_tests 'StopToken*:StopRecovery.*:Callback*:BestIterate.*' (matches CallbackPayload.* so the new tests run) and hven_sqp_seam_tests 'StopPollSeam.*': exit 0, 0 reports.
M   mutation table (legs-r3/mutations.sh + legs-r3/mutate.py; Release scratch copy; seam + drivers + interior test binaries in full, hven_sqp_tests filtered 'F7ColdScaleSmoke.TheBestIterates*:ColumnScaling*'): the control
    fails nothing; each id applied alone. The r1 table a..v is re-run (o's text follows the split_bound_multiplier refactor; semantics unchanged) and h is now MUST-KILL.
    New: w sqp event rows gated on block sizes again (rider 1); x / y sqp depth-1 z_lower / z_upper not column-mapped (rider 3); z ipm fixing-row sides swapped (rider 4);
    A sqp block regularization_dual not copied; B qp_minor_iterations not copied; D ipm inertia_corrections not copied; E ipm proximal regularization_dual not copied (rider 5).
    C sqp active_set_size drops the bound sides: REPORTED, not must-kill.
    MUST be killed: a b c d e f g h i j k l m n o p q r s t u v w x y z A B D E (x now by the floor fixture). C may be killed or survive (r2: killed).

## Order (each under `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty first, setsid -w, foreground, MKL_NUM_THREADS=1)
1. legs-r3/chain.sh 56604db3b14372c8fa729d56830c9e4912cb702c d5c70102348d88f69731e32d2e8854f07b202c6dfdb47d3bc4062948f8abbb2a 5cd3ec2ee55ece12a9a193819eb77442d3b24fb488c60094d6798e52f6bd0026  (ctest rel, dbg -> replay -> interior -> install smoke -> TSan -> mutations; stops at a failure)
Evidence: suites-r3/, replay-r3/, interior-r3/, install-smoke-r3/, tsan-r3/, mutation-r3/ here; raw dumps under the ignored .scratch/ with MANIFEST-r3.txt (sha256, size, path) committed; VERDICTS-r3.txt.
ON FAIL  stop, commit the evidence, report; a successor r3 keeps these criteria.

## hven-bench (outside this repository; not an hven proof)
Branch g5-catchup (9ae379d (unchanged by r3), local, not pushed): lanes/chain/hven_bench_chain.cpp reads `e.ipm->mu`. Built whole (`ninja`, all lane targets) against an install prefix of this branch at the code head: rc 0.

## Pre-declaration runs (NOT evidence, disclosed)
- r2's full run and its failure (evidence 36d46ec7c).
- Dev Release and Debug builds and full ctest at the code head: 100% of 3762 in both (.scratch/full-ctest-*.log).
- The floor fixture alone: hven_sqp_tests 'ColumnScalingDriver.Restoration*' 1/1 passed on the pre-rebase head (premise satisfied: a nonzero z_lower depth-1 price on a scaled column).
- rel and dbg re-configured with GIT_DIR/GIT_WORK_TREE (ceiling kept) and rebuilt: stamp 56604db3b143; arm B frozen from rel; arm A built by legs-r3/prebuild.sh; TSan targets pre-built.
- legs-r3/mutate.py dry-applied to a copy of the new head (all 31 ids apply). No leg has run.
