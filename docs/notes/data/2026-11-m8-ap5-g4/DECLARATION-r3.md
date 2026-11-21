# M8 AP-5 G4 -- DECLARATION-r3 (successor to DECLARATION-r2.md; committed before any r3 leg; never edited afterwards)

## Why a successor
r2 (DECLARATION-r2.md, VERDICTS.txt, evidence d92c1a5c6 after the rebase) met every criterion on code head 1cc9001a3 over origin/m8
f1d8a8d19. Since then the code moved twice: (1) the review riders (phase 1, now ad2fec63d code+docs and bb4c67111 tests); (2) the
branch was rebased onto origin/m8 10ec3df92, which carries G3 (best-so-far tracking, SolveResult::best_iterate,
CallbackAction::kStopReturnBest), and the G3 merge commit 73a97fb4d was added. Every leg re-runs on the new head against the new base.
A failed r3 leg is not repaired in place: a successor DECLARATION-r4.md follows, with these criteria unchanged.

## Code head and base
Code head 73a97fb4dff30af7f8f70ac4debeda99643654c0 over origin/m8 10ec3df92 (G3 r4's evidence commit; its code is G3's head 062fadba2).
Arm A = origin/m8 10ec3df92. Rebased commits (old -> new): 11a984f45 -> eaf0822fd (feature), 4033f0038/658e1c27f/425a1ad40/1174a113d/
d92c1a5c6 (r1/r2 declarations, evidence, the T4 fire-point test), b6542cfdd -> ad2fec63d, fab2a8e97 -> bb4c67111; the pre-rebase head is
kept on the local branch g4-pre-rebase-fab2a8e97. The only textual conflict was docs/migration-0.2.md (the G3 and G4 sections, both kept).

## The merge (what 73a97fb4d and the rebase resolved)
M-1  IPM: G3's stop-path best scoring and its kStopReturnBest substitution sit INSIDE `honour_stop`, the one stop exit the iteration-top
     poll and the ladder poll share (the rebase placed G3's hunk in the lambda body; verified by reading). A ladder stop therefore
     scores the iteration's start point. Not twice: the ladder exit re-pushes the iteration's own event row (Citer) and calls
     honour_stop once; the end-of-iteration tracking that scores the same point (src/drivers/ipm_solver.cpp, after fill_iter_info,
     with XSL not yet moved) is below the ladder and never reached on that path. No kStopReturnBest can be pending at a ladder poll:
     the callback ran before the iteration-top poll, which takes any latch.
M-2  SQP: the abandon (`abandon_stopped_qp`) and the exit-test stop both leave through `stop_exit` to MajorOutcome::kFinishCurrent,
     whose arm applies G3's `stop_returns_best` (st.interrupted && st.stop_return_best && !mj.token_exit && snapshot). The abandon is
     the token's (no callback runs between the exit test and an abandon point, so st.interrupted is false there) and is labelled
     before the abandoned row's event fires; a kStopReturnBest answered on that row is a no-op, exactly as on the exit test's
     token-stopped row. Token stops never substitute. Inside the restoration sub-solve the depth-1 answer latches the parent, and
     G3's depth-1 rule returns the parent's best through the sub-solve's same stop exit, as on its exit test's row.
M-3  Re-derived, not rebased: expect_stopped_at_major (the capped twin) now asserts returned_best_iterate false on both sides,
     best_iterate present on the stopped solve, and best_iterate equal field for field (elapsed_seconds excepted); the token-stop
     sentence of SolveBudget::stop (solve_result.h) says the current iterate is returned, never the best; docs/migration-0.2.md's G4
     section gains a best-tracker paragraph and the restoration clause of its SQP row.
M-4  New tests (tests/sqp/test_stop_poll_seams.cpp): StopPollSeam.T3ALadderStopScoresItsPointIntoTheBestTracker (classic and proximal,
     return_best off and on, first and last ladder poll: best_iterate is the top-of-iteration twin's and is the stopped-at iterate);
     StopPollSeam.T4AnAbandonAnsweredWithKStopReturnBestIsTheExitTestTokenStop (each QP mode: the abandon with kStopReturnBest answered on
     its row vs the solve whose token, requested by major k-1's commit, stops it at major k's exit test with the same callback: same
     rows, point, warm start, message and best; kStopRequested; no substitution);
     StopPollSeam.T4ARestorationAbandonAnsweredWithKStopReturnBestIsTheExitTestStop (the same at depth 1: identical results, kCallbackStop,
     the parent's best returned). The "kStopReturnBest requested by the callback in that major" case is reachable only on the abandoned
     row's own event (no event fires inside a major before it); a callback latch BEFORE an abandon is not reachable at depth 0.

## Expected movers
None. The unarmed path is unchanged by the riders and the merge (no poll, no snapshot, no check without a token: the riders' new checks
are `stop_flag_set(qp_stop_)` null tests, the merge is tests/docs only). P2 and IC compare against arm A = G3; P3 against G3 r4's rows;
ARMED legs expect 0 differences (G3 already showed an armed tracker moves no counter).

## Deviations from the page (carried from DECLARATION.md, plus)
D-a .. D-f as DECLARATION.md. D-g: GIT_CEILING_DIRECTORIES is the worktree root on every command; the bench's configure-time git stamp
(bench/CMakeLists.txt, `git describe` run in bench/) cannot find the repository under that ceiling, so the rel and dbg trees were
re-configured with GIT_DIR/GIT_WORK_TREE set to this worktree's git directory (ceiling kept): arm B's stamp is 73a97fb4dff3. Arm A is a
`git archive` export (stamp "unknown"); its sha256 equals G3 r4's arm B4 byte for byte (8c5302..., same code, same stamp).
D-h (recorded, not changed): at depth 0 a kStop/kStopReturnBest answered on a token-stopped terminal row is a no-op (the token labelled the
exit first); at depth 1 the same answer on the sub-solve's terminal row labels the parent's exit the callback's (G2/G3's parent-latch
rule). G4 only routes the abandon through the same exits; the asymmetry predates it.

## Tests (P1 count)
"100% tests passed, 0 tests failed out of 3747", Release and Debug. `ctest -N` lists 3749 (the 2 Disabled EqpRefinementAb entries are
not counted; the Skipped FailByDesignControl entry is). 3747 = 3729 (origin/m8 10ec3df92's count, derived: no ctest is run on arm A) + 18
StopPollSeam cases
(`git diff 10ec3df92 HEAD -- tests | grep -c '^+TEST'` = 18, '^-TEST' = 0): r1's 6, phase 1's 9, the merge's 3.

## Binaries and trees (git-ignored build-g4/)
Arm A  build-g4/armAbin/hven_sqp_corpus  (legs-r3/prebuild.sh 10ec3df92)  sha256 8c530207b47180f7baf6ad53109bed8ea252a4d3b1348a37923603ef2fb2bb78
Arm B  build-g4/armBbin/hven_sqp_corpus  frozen (cp -p) from build-g4/rel/bench/hven_sqp_corpus at 73a97fb4d (stamp 73a97fb4dff3)
       sha256 663b1138968b6b82811b730dc49d278e868b5dad56fd8ccf7b8700bcbc199a77
Trees  build-g4/rel, build-g4/dbg (every target at 73a97fb4d), build-g4/tsan, build-g4/mut-src + build-g4/mut (WIPED by mutations.sh before
       the control build: `git archive` stamps files with the commit time, so stale objects could otherwise survive), build-g4/smoke-*.
Raw dumps under the ignored .scratch/g4r3-{runtime,ic,suites,install-smoke,tsan,mutation} plus the replay/interior run text kept in the
evidence directories; MANIFEST-r3.txt (manifest-r3.py: sha256, size, path) committed with the evidence. Raw dumps are not committed.

## Instruments (declared before data)
legs-r3/, runtime-r3/, ic-r3/ are copies of legs/, runtime/, ic/ with output directories suffixed -r3 and raw directories .scratch/g4r3-*.
Changes beyond the re-pointing: legs-r3/replay.sh's reference rows are ../2026-11-m8-ap5-g3/replay-r4/g3-<mode>-B.csv (the base's own
committed rows; G3 r4 matched G2 r3 with 0 differences); legs-r3/tsan.sh also builds hven_sqp_seam_tests, adds BestIterate.* to the drivers
filter and runs the seam binary with StopPollSeam.*; legs-r3/mutations.sh wipes build-g4/mut first and runs ids a..y, z, Z with
legs-r3/mutate.py (the r2 strings with g/h/n/o/r updated to the rider code, plus s..y, z, Z).

## Order (each under `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty first, setsid, foreground, MKL_NUM_THREADS=1)
1. runtime-r3/run_all.sh 663b1138968b6b82811b730dc49d278e868b5dad56fd8ccf7b8700bcbc199a77 8c530207b47180f7baf6ad53109bed8ea252a4d3b1348a37923603ef2fb2bb78   -- FIRST, serial, solo
2. ic-r3/run_ic.sh 663b1138968b6b82811b730dc49d278e868b5dad56fd8ccf7b8700bcbc199a77 8c530207b47180f7baf6ad53109bed8ea252a4d3b1348a37923603ef2fb2bb78       -- serial, solo
3. legs-r3/chain.sh 73a97fb4dff30af7f8f70ac4debeda99643654c0 663b1138968b6b82811b730dc49d278e868b5dad56fd8ccf7b8700bcbc199a77   -- ctest x2, replay, interior, install smoke, TSan, mutations

## Proofs and pass criteria (r2's, carried; P1, P3's reference, P8's filter and M's list updated as stated)
P0  arm A / arm B sha256 above (checked by run_all.sh, run_ic.sh, replay.sh, interior.sh).
P1  ctest Release (build-g4/rel) and Debug (build-g4/dbg), -j4 under setsid: "100% tests passed, 0 tests failed out of 3747".
P2  runtime gate (runtime-r3/run_all.sh): interior, ipm, ssn, walk; per mode COUNTER IDENTITY HOLDS, THROTTLE 0 (else VOID), filtered B/A
    in [0.995, 1.005]; VERDICT PASS.
IC  pass A instruction counts (ic-r3/run_ic.sh): walk, ssn, ipm, interior; three rounds, arms alternated per cell, per-cell median, corpus =
    sum of medians. PASS per mode: counter check 0 differences; corpus instructions:u B/A in [0.999, 1.001]; every cell in [0.995, 1.005];
    IC_VERDICT PASS. Cycles informational.
P3  replay 27 cells x (walk, ssn, ipm): arm B vs G3 r4's committed rows, compare_replay.py EXACT, "differences: 0" x3; AND ARMED (counting
    callback + never-requested token): arm B armed vs arm B, EXACT, "differences: 0" x3.
P4  interior 43 cells: unmoved (arm A vs B) EXACT 0; promoted (B vs bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv) EXACT 0;
    armed (B armed vs B) EXACT 0.
P5  install smoke, default and clang arms, each with scripts/check_export_contract.sh: pass (TU count reported).
P6  T3/T4 (StopPollSeam.*, 18 cases incl. the merge's three) inside P1 in both builds.
P8  ThreadSanitizer (legs-r3/tsan.sh): hven_drivers_tests 'StopToken*:StopRecovery.*:Callback*:BestIterate.*' (91 cases) AND
    hven_sqp_seam_tests 'StopPollSeam.*' (18 cases): both exit 0, 0 "WARNING: ThreadSanitizer" lines.
M   mutation table (legs-r3/mutations.sh; hven_sqp_seam_tests, hven_drivers_tests, hven_interior_tests in full): the control fails nothing;
    MUST BE KILLED: a c d e f g h i j k l m n o q r s t u v w x y z Z. MAY SURVIVE (reported either way): b, p.
ON FAIL  stop, commit the evidence, report; a successor -r4 keeps these criteria unchanged.

Mutations (legs-r3/mutate.py). r2's a..r (a ipm ladder not handed the flag; b ladder stop without re-pushing the row; c ladder stop
labelled a callback stop; d/e/f walk/SSN/IPQP poll ticks but the loop continues; g no abandon after the main subproblem; h none after the
elastic ladder; i stopped row not restored; j abandoned major still counted; k SOC re-solve left armed; l/m lazy SSN/IPQP engine not armed;
n/o SSN/IPQP route runs on after a stop; p certified fallback runs on (no fixture stops inside rung A: accepted survivor); q the flag not
cleared at solve exit -- now must-kill, through SqpSolverAccess::qp_stop_disarmed; r the abandon does not label the token). Phase-1 riders:
s a stopped SSN subproblem's factorizations not charged; t the same for IPQP; u no stop check after the subproblem build; v the SSN warm
grade's internal post-solve check off; w the walk not re-armed after the SOC re-solve; x the abandon keeps the abandoned kernel's row mode;
y the warm-grade routing lambda's post-grade check off. G3 merge points: z the best scoring removed from honour_stop (the ladder exit's
included); Z the shared stop exit's substitution test ignores the token label (kStopReturnBest taken on a token-labelled abandon).

## Pre-declaration runs (NOT evidence, disclosed)
- Release seam binary (dev, ceiling-stamped "unknown"): StopPollSeam.* 15/15 after the rebase, then 18/18 with the merge's tests.
- Dev mutation spot check (scratch copy of the working tree, seam + drivers binaries, filters StopPollSeam.* and BestIterate.*:Stop*:Callback*):
  z killed (StopPollSeam.T3ALadderStopScoresItsPointIntoTheBestTracker and two BestIterate cases), Z killed
  (StopPollSeam.T4AnAbandonAnsweredWithKStopReturnBestIsTheExitTestTokenStop only). Phase 1's spot check (q s t u v w x y killed, p survives)
  was on the pre-rebase head. legs-r3/mutate.py dry-applied to copies of the five source files at 73a97fb4d: all 27 ids apply exactly once.
- build-g4/rel and build-g4/dbg built (every target) at 73a97fb4d; dev ctest of rel: "100% tests passed, 0 tests failed out of 3747" (before
  the re-stamp, which rebuilt 24 objects/links, all in bench/ and tests/golden_rig/, the
  git-stamped targets; no library or test-suite object). Both trees then re-configured for the stamp (D-g) and rebuilt.
- Arm A built (legs-r3/prebuild.sh 10ec3df92). No runtime, instruction-count, replay, interior, smoke, TSan or mutation leg has run on r3.
