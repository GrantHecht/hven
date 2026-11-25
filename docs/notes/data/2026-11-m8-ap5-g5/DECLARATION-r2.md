# M8 AP-5 G5 -- DECLARATION-r2 (review riders; committed before any r2 leg; never edited afterwards -- a failure is r3)

## Relation to r1
r1 (DECLARATION.md, evidence d2240e7db on the pre-rebase branch) stands as the history of the first head. This r2 re-proves the branch AFTER (a) the rebase onto
origin/m8 5ce6dc220 (which added the zero-step work) and (b) the review riders below. r1's runtime gates (P2a unattached, P2b attached) are NOT re-run; the
justification is declared here (section "Runtime gates").

## Change over r1 (the riders; settlement prose in docs/migration-0.2.md section G5 and CATCH-UP.txt)
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
Code head e90182b35263d3c59d31a85ba69f475a4758fdca (the rider commit) on origin/m8 5ce6dc2204251bd0a56a751196d8a05ddc7d83eb. The g5 feature commit beneath it is the rebased efe57fb24 (89938b020).
Arm A = origin/m8 5ce6dc220 (built from `git archive`, build-g5/armA, copy build-g5/armAbin/hven_sqp_corpus) sha256 959a15707aff4786bc42f7be7930a4c3c45896a28571e66e3140244547284999
Arm B = build-g5/rel/bench/hven_sqp_corpus at the code head (restamped through GIT_DIR/GIT_WORK_TREE, ceiling kept), frozen cp -p to build-g5/armBbin sha256 91b3f9cb66c54319b86e17abbe440fd98873255e6309ebca0bdf1c8033710ea7

## Runtime gates (P2a, P2b): NOT re-run -- justification
The riders add no executed instruction to a solve with no callback attached: `event_rows_measured` is written only inside `if (iteration_callback_)` and read only in
fire_iteration_event (which returns first when no callback is installed); the fixing-row split and `split_bound_multiplier` sit in the callback-armed declared_point path; the
default-mask constant moved namespace (constexpr); the rename, docs, tests and bench tags are not runtime. The one unarmed-path difference is one bool member in the per-major
MajorState struct. r1's gates (P2a interior 1.000000 / ipm 0.999998 / ssn 1.000000 / walk 0.999999; P2b within 1e-5) stand. Counter/status identity of the unarmed path is still
asserted by P3 and P4 below (arm A vs arm B, EXACT). If P3/P4 move, r2 fails and the runtime gates are owed.

## Tests (P1 count)
"100% tests passed, 0 tests failed out of 3757", Release and Debug (r1: 3753). New TESTs: CallbackPayload.AnAllFreeRowsProblemKeepsItsRowViewsOnBothEnginesAndTheBestIterate,
CallbackPayload.IpmBlockCarriesBacktracksAndInertiaCorrections, CallbackPayload.TheFixedVariableMakeConstraintArmSplitsItsPriceIntoTheBoundSides (+3 over r1; origin/m8 5ce6dc220 count in r1 base +zero-step tests).

## Expected movers
NONE: no replay column, no interior column, no counter, no status.

## Proofs and pass criteria
P0  arm A / arm B sha256 above.
P1  ctest Release (build-g5/rel) and Debug (build-g5/dbg), -j4 under setsid: 100% passed, 3757 tests.
P3  replay 27 cells x (walk, ssn, ipm): arm A (RUN) vs arm B, scripts/compare_replay.py EXACT, "differences: 0" x3; the ARMED leg (--callback count + HVEN_LEG_UNFIRED_TOKEN=1) vs arm B, 0 x3.
P4  interior, 43 cells: unmoved (arm A vs arm B) 0; promoted (vs bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv) 0; armed 0.
P5  install smoke, default and clang arms, each with scripts/check_export_contract.sh: pass ("export contract OK" lines extracted into install-smoke-r2/export-contract-summary.txt; r1's grep matched nothing -- fixed in legs-r2/chain.sh).
P8  ThreadSanitizer (legs-r2/tsan.sh): hven_drivers_tests 'StopToken*:StopRecovery.*:Callback*:BestIterate.*' (matches CallbackPayload.* so the new tests run) and hven_sqp_seam_tests 'StopPollSeam.*': exit 0, 0 reports.
M   mutation table (legs-r2/mutations.sh + legs-r2/mutate.py; Release scratch copy; seam + drivers + interior test binaries in full, hven_sqp_tests filtered 'F7ColdScaleSmoke.TheBestIterates*:ColumnScaling*'): the control
    fails nothing; each id applied alone. The r1 table a..v is re-run (o's text follows the split_bound_multiplier refactor; semantics unchanged) and h is now MUST-KILL.
    New: w sqp event rows gated on block sizes again (rider 1); x / y sqp depth-1 z_lower / z_upper not column-mapped (rider 3); z ipm fixing-row sides swapped (rider 4);
    A sqp block regularization_dual not copied; B qp_minor_iterations not copied; D ipm inertia_corrections not copied; E ipm proximal regularization_dual not copied (rider 5).
    C sqp active_set_size drops the bound sides: REPORTED, not must-kill.
    MUST be killed: a b c d e f g h i j k l m n o p q r s t u v w x y z A B D E. C may survive.

## Order (each under `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty first, setsid -w, foreground, MKL_NUM_THREADS=1)
1. legs-r2/chain.sh e90182b35263d3c59d31a85ba69f475a4758fdca 91b3f9cb66c54319b86e17abbe440fd98873255e6309ebca0bdf1c8033710ea7 959a15707aff4786bc42f7be7930a4c3c45896a28571e66e3140244547284999  (ctest rel, dbg -> replay -> interior -> install smoke -> TSan -> mutations; stops at a failure)
Evidence: suites-r2/, replay-r2/, interior-r2/, install-smoke-r2/, tsan-r2/, mutation-r2/ here; raw dumps under the ignored .scratch/ with MANIFEST-r2.txt (sha256, size, path) committed; VERDICTS-r2.txt.
ON FAIL  stop, commit the evidence, report; a successor r3 keeps these criteria.

## hven-bench (outside this repository; not an hven proof)
Branch g5-catchup (9ae379d, local, not pushed): lanes/chain/hven_bench_chain.cpp reads `e.ipm->mu`. Built whole (`ninja`, all lane targets) against an install prefix of this branch at the code head: rc 0.

## Pre-declaration runs (NOT evidence, disclosed)
- Dev builds and full ctest, Release and Debug, at the code head: 100% of 3757 in both (.scratch/full-ctest-*.log); the 3757 is the origin/m8 count plus the three new tests.
- Targeted gtest runs while writing the tests: one assertion of mine dropped (delta_c does not imply a primal correction), then Callback*/BestIterate*/StopRecovery* 84/84 and ColumnScaling* 34/34.
- rel and dbg re-configured with GIT_DIR/GIT_WORK_TREE (ceiling kept) and rebuilt: stamp e90182b35263; arm B frozen from rel; arm A built by legs-r2/prebuild.sh; the TSan targets pre-built (no TSan run).
- legs-r2/mutate.py dry-applied to copies (all 31 ids apply); no mutant built or run. No replay, interior, install-smoke, TSan or mutation leg has run.
- hven-bench g5-catchup built whole against an install prefix of the code head (rc 0).
