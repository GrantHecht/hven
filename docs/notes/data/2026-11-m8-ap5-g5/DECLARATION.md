# M8 AP-5 G5 -- DECLARATION (the widened payload; committed before any leg; never edited afterwards)

## Spec and change
Design page docs/notes/2026-11-m8-ap5-interface-design.md, row G5 (section 3.1, Q9, section 7 F7): `IterationPayload::kConstraints`
(`g`, `violation`) and `kBoundMultipliers` (`z_lower`, `z_upper`); `kAll` widened; `set_iteration_callback`'s default mask
`kVectors | kDiagnostics` (`detail::kDefaultIterationPayload`, also the mask in force with no callback, after a clear and after an
empty callable); `IpmIterationInfo` / `SqpIterationInfo` as `IterationEvent::ipm` / `::sqp`; the four top-level optionals
(`phase`, `mu`, `depth`, `radius`) removed. Folded riders: the snapshot widened with the event (`violation`, `z_lower`, `z_upper`,
`ipm`, `sqp`; `snapshot()` now copies `g`); R2 (step_norm), R3 (measured, API proposal only), R5 (best_* engine blocks folded).
Settlement prose: docs/migration-0.2.md, section G5. Behaviour change: NONE on any trajectory, counter or status (callback-only;
the unattached path is untouched beyond the per-phase reset of a 64-byte member and, under an installed callback or an armed
tracker, eight scalar copies per committed step).

## Code head and base
Code head efe57fb24f725177d5e6496297962e5709d548df (one code commit) on origin/m8 737f73df6 (G4 landed). Arm A = origin/m8 737f73df6.

## Design choices and deviations (recorded)
D-a  R2, step_norm: the page decides it (section 3.1, "the extras keep the per-engine timing of step_norm ... KEEP, documented:
     C5"): interior-point = the step that REACHED the point, SQP = the step taken FROM it (the event fires once its subproblem is
     solved, so the event, `history` and `sqp.major` stay one row). Kept; the engine blocks follow the same timing. On each engine
     the event, `snapshot(event)` and `best_iterate` carry the same quantity (pinned: BestIterate.*EqualsItsEventsSnapshot compare
     step_norm and the engine blocks field by field). Cross-engine alignment would need the page's C5 reversed (owner call).
D-b  R5: `SolveResult::EngineRows::best_{lambda_e,lambda_i,ce,ci}` are removed, with Problem::report's best_iterate branch. The
     engines compose best_iterate's `lambda`, `g`, `violation` (and `z_lower`/`z_upper`) with the event's own row map
     (detail::compose_event_lambda / compose_event_rows, through the Problem the public handle installs for the solve). An
     internal route with no Problem now gets the concatenation (as its events do) where it got empty vectors.
D-c  R3: not implemented. kStopReturnBest may be returned under any mask (kScalars included), so the tracker must hold the full
     point whenever a callback is installed; skipping the copies needs an API (an opt-out), which is an owner/settler call
     (proposed in the report). The SQP attached cost is measured (P2b, modes ipm and ssn) at kScalars, the default mask and kAll.
D-d  z_lower / z_upper: interior-point = the engine's per-side multipliers, divided by the objective scale and scattered as `z`
     (z_lower - z_upper equals z to rounding; an eliminated or fixing-row coordinate takes the sign split of its z); SQP = the
     sign split of z (exact). Empty on interior-point `recovering` events, with `g` and `violation` (page section 3.1).
D-e  `regularization_dual`: proximal mode prox_reg_dual (>= 0), classic mode the ladder's dual shift when it engaged (dc_applied),
     else 0; both regularizations are engine-scale KKT values.
D-f  IterationEvent / IterationSnapshot member order follows the page (iteration, recovering, f, three diagnostics, step_norm,
     elapsed_seconds, seven views, ipm, sqp).
D-g  Bench levers (instrument only): `--callback count-vd` (--internal-run-one only; refused on the corpus route) and
     HVEN_LEG_CALLBACK_PAYLOAD=vectors-diagnostics: the counting callback under kVectors|kDiagnostics, so the attached-default leg
     pairs arm B's default mask with arm A's kAll (the same payload).
D-h  Arm A's binary is G4's frozen arm B (sha below), built at 73a97fb4d, whose src/include/bench/tests/cmake/CMakeLists.txt/
     examples/scripts are byte-identical to 737f73df6 (`git diff 73a97fb4d 737f73df6 -- ...` empty); copied cp -p, not rebuilt.
D-i  dep/eigen, dep/fmt in this worktree are plain copies of the main checkout's submodule trees (their .git files removed).
D-j  hven-bench (private harness) lanes/chain/hven_bench_chain.cpp reads `e.mu`: it no longer compiles against this head (one-line
     fix `e.ipm ? e.ipm->mu`); not edited, reported. No leg uses hven-bench.

## Tests (P1 count)
"100% tests passed, 0 tests failed out of 3753", Release and Debug (origin/m8 G4 r4: 3747). New TESTs over origin/m8: CallbackPayload.AllDeliversTheRowsAndTheBoundSidesInTheRowSpaceOnBothEngines,
CallbackPayload.EachNewBitDeliversExactlyItsFields, CallbackPayload.SnapshotOwnsEveryViewAfterTheSolve,
CallbackPayload.IpmBlockDescribesTheStepThatReachedThePoint, Callback.SqpRestorationEventsCarryTheSubProblemsRowsAndADepthOneBlock,
StopRecovery.T12IpmRecoveringEventsCarryNoRowsAndNoBoundSides (+6). Renamed (re-derived for Q9):
CallbackPayload.TheOneArgumentOverloadStillDeliversEverything -> ...TheOneArgumentOverloadDeliversTheVectorsAndTheDiagnostics.
Modified for the API (field moves, the default-mask expectations, best/event comparisons widened to the engine blocks and the G5
views, two BestIterate snapshot tests attach at kAll): listed by `git diff origin/m8 -- tests`. T12 = the six new tests + the renamed one.

## Expected movers
NONE: no replay column, no interior column, no counter, no status. The ARMED legs (--callback count / HVEN_LEG_COUNT_CALLBACK,
whose kAll now prepares the G5 views, + a never-requested token) are byte-identical to the unarmed legs.

## Binaries and trees (git-ignored build-g5/)
Arm A  build-g5/armAbin/hven_sqp_corpus (G4's arm B, see D-h) sha256 663b1138968b6b82811b730dc49d278e868b5dad56fd8ccf7b8700bcbc199a77
Arm B  build-g5/armBbin/hven_sqp_corpus, frozen (cp -p) from build-g5/rel/bench/hven_sqp_corpus at the code head (stamp configured
       through GIT_DIR/GIT_WORK_TREE, ceiling kept) sha256 bbc3f9470262cfdfd7abe622c116b444fc9888b300342461e5b082b306e7b32e. build-g5/rel is not rebuilt before the legs end.

## Proofs and pass criteria
P0  arm A / arm B sha256 above (checked by runtime/run_all.sh, run_attached.sh, legs/replay.sh, legs/interior.sh).
P1  ctest Release (build-g5/rel) and Debug (build-g5/dbg), -j4 under setsid: "100% tests passed, 0 tests failed out of 3753".
P2a unattached runtime gate (runtime/run_all.sh): interior, ipm, ssn, walk, serial, solo; THROTTLE == 0 (else VOID); counter
    identity per mode; filtered B/A in [0.995, 1.005] (instructions:u, perf, one pinned core).
P2b attached-callback legs (runtime/run_attached.sh; the M7-4 residual re-measured), modes interior (the interior leg), ipm and ssn
    (the SQP engine; walk excluded: the same driver code, its QP-dominated leg is the longest, 33 min a leg), serial, solo:
    attached-scalars (both arms kScalars) and attached-default (arm A kAll == G4's kVectors|kDiagnostics, arm B kVectors|
    kDiagnostics): THROTTLE == 0, counter identity, filtered B/A in [0.995, 1.005] -- CRITERIA. attached-all (both arms kAll; arm
    B's prepares the G5 views): THROTTLE == 0 and counter identity are criteria; its B/A is REPORTED (the new kAll's cost), not
    banded. attach_delta.py reports per mode each arm's attach cost and arm B's kAll/default ratio.
P3  replay 27 cells x (walk, ssn, ipm): arm B vs the COMMITTED G4 r4 rows (../2026-11-m8-ap5-g4/replay-r4/g4-<mode>-B.csv),
    scripts/compare_replay.py EXACT, "differences: 0" x3; AND the ARMED leg (--callback count + HVEN_LEG_UNFIRED_TOKEN=1) vs arm B
    unarmed, EXACT, 0 x3.
P4  interior, 43 cells: unmoved (arm A vs arm B) 0; promoted (vs bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv) 0;
    armed (HVEN_LEG_COUNT_CALLBACK=1 + HVEN_LEG_UNFIRED_TOKEN=1 vs arm B) 0.
P5  install smoke, default and clang arms, each with scripts/check_export_contract.sh: pass (TU count reported; one public header
    changed, no TU added).
P8  ThreadSanitizer (legs/tsan.sh, build-g5/tsan): hven_drivers_tests 'StopToken*:StopRecovery.*:Callback*:BestIterate.*' and
    hven_sqp_seam_tests 'StopPollSeam.*': both exit 0, 0 "WARNING: ThreadSanitizer" lines.
M   mutation table (legs/mutations.sh + legs/mutate.py; Release scratch copy; hven_sqp_seam_tests, hven_drivers_tests,
    hven_interior_tests in full, hven_sqp_tests filtered 'F7ColdScaleSmoke.TheBestIterates*:ColumnScaling*'): the control fails
    nothing; each of a..v applied alone. MUST be killed: a b c d e f g i j k l m n o p q r s t u v. h MAY survive (HS071's
    interior-point line searches may take no backtrack), reported either way.
    a ipm kConstraints never delivered; b ipm bound sides delivered on recovering rows; c ipm sides swapped; d ipm reached-step
    facts not reset at a phase start; e ipm alpha_primal not the stepped record's; f ipm best step facts not stored; g ipm best
    rows not composed (R5); h ipm line_search_trials not copied; i sqp kConstraints never delivered; j sqp z delivered without
    kVectors; k sqp depth-1 block not stamped 1; l sqp best block not completed at the push; m sqp violation_l1 from the
    feasibility column; n sqp best sides not split; o split_bound_multipliers swapped; p row violation positive part dropped;
    q equality row without abs; r free row NaN; s snapshot() drops g; t snapshot() drops ipm; u default mask kAll (Q9 reverted);
    v sqp best prices not sign-repaired before the row map (R5).

## Order (each under `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty first, setsid -w, foreground, MKL_NUM_THREADS=1;
## the driver .scratch/g5-legs.sh stops at the first failed leg)
1. runtime/run_all.sh bbc3f9470262cfdfd7abe622c116b444fc9888b300342461e5b082b306e7b32e 663b1138968b6b82811b730dc49d278e868b5dad56fd8ccf7b8700bcbc199a77      -- serial, solo
2. runtime/run_attached.sh bbc3f9470262cfdfd7abe622c116b444fc9888b300342461e5b082b306e7b32e 663b1138968b6b82811b730dc49d278e868b5dad56fd8ccf7b8700bcbc199a77 -- serial, solo
3. legs/chain.sh efe57fb24f725177d5e6496297962e5709d548df bbc3f9470262cfdfd7abe622c116b444fc9888b300342461e5b082b306e7b32e   (ctest rel, dbg -> replay -> interior -> install smoke -> TSan -> mutations; stops at a failure)
Evidence: runtime/, suites/, replay/, interior/, install-smoke/, tsan/, mutation/ here; raw dumps under the ignored .scratch/ with
MANIFEST.txt (sha256, size, path) committed; VERDICTS.txt extracted.
ON FAIL  stop, commit the evidence, report; a successor -r2 keeps these criteria unchanged.

## Pre-declaration runs (NOT evidence, disclosed)
- Dev Release and Debug builds (build-g5/rel, build-g5/dbg), every target, and full ctest of each at the tree committed as the code
  head: 100% of 3753 in both (.scratch/full-ctest-*.log). An earlier Release ctest of the tree before the six T12 tests: 3747/3747.
- Targeted gtest runs while writing the tests (Callback*, BestIterate*, StopRecovery*): one fixture premise fixed (x2's bound moved
  from 2.5, inactive at the solution, to 2.2), one lambda's ASSERT replaced by EXPECT (compile), then 81/81.
- rel and dbg re-configured with GIT_DIR/GIT_WORK_TREE (ceiling kept) and rebuilt: stamp efe57fb24f72; arm B frozen from rel.
- A lever check (one cell each, not timed): arm B --callback count-vd (ipm), arm A --callback count (ssn), arm B interior with
  HVEN_LEG_CALLBACK_PAYLOAD=vectors-diagnostics: all rc 0 and the observer reached.
- legs/mutate.py dry-applied to copies of the six files (all 22 apply); no mutant built.
- `bash -n` on every legs/ and runtime/ shell script. No ctest leg, runtime, attached, replay, interior, install-smoke, TSan or
  mutation leg has run.
